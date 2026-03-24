#include "proxy_server.h"

#include <signal.h>
#include <stddef.h>
#include <string.h>

#include "common.h"
#include "error.h"
#include "error_response.h"
#include "logger.h"
#include "request_parser.h"
#include "response_forwarder.h"
#include "socket_utils.h"
#include "thread_pool.h"

#define PROXY_SERVER_BACKLOG 16

typedef struct {
    const proxy_config_t *config;
} proxy_server_context_t;

static volatile sig_atomic_t g_shutdown_requested = 0;

static int buffer_has_complete_headers(const char *buffer)
{
    return strstr(buffer, "\r\n\r\n") != NULL || strstr(buffer, "\n\n") != NULL;
}

static int read_request_bytes(socket_handle_t client_socket,
    char *request_buffer, size_t request_buffer_size, int *out_bytes_read)
{
    int total_bytes_read = 0;
    int bytes_read;

    if (request_buffer == NULL || out_bytes_read == NULL || request_buffer_size < 2) {
        return -1;
    }

    request_buffer[0] = '\0';
    while ((size_t)total_bytes_read < request_buffer_size - 1) {
        bytes_read = socket_utils_read(client_socket,
            request_buffer + total_bytes_read,
            request_buffer_size - 1 - (size_t)total_bytes_read);
        if (bytes_read < 0) {
            return -1;
        }

        if (bytes_read == 0) {
            break;
        }

        total_bytes_read += bytes_read;
        request_buffer[total_bytes_read] = '\0';

        if (buffer_has_complete_headers(request_buffer)) {
            *out_bytes_read = total_bytes_read;
            return 0;
        }
    }

    if ((size_t)total_bytes_read >= request_buffer_size - 1) {
        return 1;
    }

    *out_bytes_read = total_bytes_read;
    return 0;
}

static void proxy_server_signal_handler(int signal_number)
{
    (void)signal_number;
    g_shutdown_requested = 1;
}

static void process_client_connection(const client_job_t *job, void *context)
{
    const proxy_server_context_t *server_context = (const proxy_server_context_t *)context;
    socket_handle_t client_socket;
    char request_buffer[PROXY_REQUEST_BUFFER_SIZE];
    http_request_t request;
    int bytes_read;

    if (job == NULL || server_context == NULL || server_context->config == NULL) {
        return;
    }

    client_socket = job->client_socket;
    logger_log(LOG_INFO, "worker handling client %s:%s", job->client_host, job->client_service);

    if (socket_utils_set_timeout(client_socket, PROXY_SOCKET_TIMEOUT_MS) != 0) {
        error_report("failed to set client socket timeout (code=%d)",
            socket_utils_get_last_error());
        socket_utils_close(client_socket);
        return;
    }

    bytes_read = 0;
    switch (read_request_bytes(client_socket, request_buffer, sizeof(request_buffer), &bytes_read)) {
    case -1:
        error_report("failed to read request bytes from client (code=%d)",
            socket_utils_get_last_error());
        (void)error_response_send(client_socket, 408, "Request Timeout",
            "request read failed or timed out\n");
        socket_utils_close(client_socket);
        return;
    case 1:
        logger_log(LOG_WARN, "request exceeded buffer limit");
        (void)error_response_send(client_socket, 413, "Payload Too Large",
            "request headers exceed supported size\n");
        socket_utils_close(client_socket);
        return;
    default:
        break;
    }

    if (bytes_read == 0) {
        logger_log(LOG_WARN, "client closed connection before sending a request");
        (void)error_response_send(client_socket, 400, "Bad Request",
            "empty request received\n");
        socket_utils_close(client_socket);
        return;
    }

    logger_log(LOG_INFO, "read %d request bytes", bytes_read);
    if (request_parser_parse(request_buffer, &request) != 0) {
        logger_log(LOG_WARN, "failed to parse incoming proxy request");
        (void)error_response_send(client_socket, 400, "Bad Request",
            "unsupported or malformed proxy request\n");
        socket_utils_close(client_socket);
        return;
    }

    logger_log(LOG_INFO, "parsed request: method=%s host=%s port=%d path=%s version=%s",
        request.method, request.host, request.port, request.path, request.version);

    if (response_forwarder_forward(client_socket, &request) != 0) {
        (void)error_response_send(client_socket, 502, "Bad Gateway",
            "failed to fetch response from origin server\n");
        socket_utils_close(client_socket);
        return;
    }

    socket_utils_close(client_socket);
    logger_log(LOG_INFO, "completed client %s:%s", job->client_host, job->client_service);
}

int proxy_server_run(const proxy_config_t *config)
{
    socket_handle_t listener_socket = SOCKET_HANDLE_INVALID;
    thread_pool_t *thread_pool = NULL;
    proxy_server_context_t server_context;
    client_job_t job;
    int accept_ready;
    int result = -1;

    g_shutdown_requested = 0;
    if (config == NULL) {
        error_report("server configuration is required");
        return -1;
    }

    if (socket_utils_initialize() != 0) {
        error_report("failed to initialize socket subsystem (code=%d)",
            socket_utils_get_last_error());
        return -1;
    }

    listener_socket = socket_utils_create_listener(config->port, PROXY_SERVER_BACKLOG);
    if (listener_socket == SOCKET_HANDLE_INVALID) {
        error_report("failed to create listening socket on port %d (code=%d)",
            config->port, socket_utils_get_last_error());
        socket_utils_cleanup();
        return -1;
    }

    server_context.config = config;
    thread_pool = thread_pool_create(config->worker_count, PROXY_THREAD_POOL_QUEUE_CAPACITY,
        process_client_connection, &server_context);
    if (thread_pool == NULL) {
        error_report("failed to initialize thread pool with %d workers", config->worker_count);
        socket_utils_close(listener_socket);
        socket_utils_cleanup();
        return -1;
    }

    (void)signal(SIGINT, proxy_server_signal_handler);
#if defined(SIGTERM)
    (void)signal(SIGTERM, proxy_server_signal_handler);
#endif

    logger_log(LOG_INFO, "listening on port %d with %d workers",
        config->port, config->worker_count);

    for (;;) {
        if (g_shutdown_requested) {
            logger_log(LOG_INFO, "shutdown signal received, draining worker queue");
            result = 0;
            break;
        }

        accept_ready = socket_utils_wait_for_readable(listener_socket, PROXY_ACCEPT_POLL_TIMEOUT_MS);
        if (accept_ready < 0) {
            error_report("listener readiness check failed (code=%d)",
                socket_utils_get_last_error());
            break;
        }

        if (accept_ready == 0) {
            continue;
        }

        job.client_socket = socket_utils_accept(listener_socket,
            job.client_host, sizeof(job.client_host),
            job.client_service, sizeof(job.client_service));
        if (job.client_socket == SOCKET_HANDLE_INVALID) {
            error_report("failed to accept client connection (code=%d)",
                socket_utils_get_last_error());
            break;
        }

        logger_log(LOG_INFO, "accepted client %s:%s", job.client_host, job.client_service);
        if (thread_pool_submit(thread_pool, &job) != 0) {
            error_report("failed to enqueue client connection");
            socket_utils_close(job.client_socket);
            break;
        }
    }

    socket_utils_close(listener_socket);
    thread_pool_destroy(thread_pool);
    socket_utils_cleanup();
    logger_log(LOG_INFO, "proxy server shutdown complete");
    return result;
}
