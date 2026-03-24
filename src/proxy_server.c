#include "proxy_server.h"

#include <stddef.h>
#include <string.h>

#include "error.h"
#include "logger.h"
#include "socket_utils.h"

#define PROXY_SERVER_BACKLOG 16
#define PROXY_SERVER_READ_BUFFER_SIZE 8192

static void log_request_line(const char *request_bytes)
{
    const char *line_end;
    size_t line_length;
    char request_line[1024];

    if (request_bytes == NULL || request_bytes[0] == '\0') {
        logger_log(LOG_WARN, "received empty request payload");
        return;
    }

    line_end = strpbrk(request_bytes, "\r\n");
    if (line_end == NULL) {
        line_length = strlen(request_bytes);
    } else {
        line_length = (size_t)(line_end - request_bytes);
    }

    if (line_length >= sizeof(request_line)) {
        line_length = sizeof(request_line) - 1;
    }

    (void)memcpy(request_line, request_bytes, line_length);
    request_line[line_length] = '\0';

    logger_log(LOG_INFO, "request line: %s", request_line);
}

int proxy_server_run_once(const proxy_config_t *config)
{
    socket_handle_t listener_socket = SOCKET_HANDLE_INVALID;
    socket_handle_t client_socket = SOCKET_HANDLE_INVALID;
    char client_host[NI_MAXHOST];
    char client_service[NI_MAXSERV];
    char request_buffer[PROXY_SERVER_READ_BUFFER_SIZE + 1];
    int bytes_read;

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

    logger_log(LOG_INFO, "listening for a single client on port %d", config->port);

    client_socket = socket_utils_accept(listener_socket,
        client_host, sizeof(client_host),
        client_service, sizeof(client_service));
    if (client_socket == SOCKET_HANDLE_INVALID) {
        error_report("failed to accept client connection (code=%d)",
            socket_utils_get_last_error());
        socket_utils_close(listener_socket);
        socket_utils_cleanup();
        return -1;
    }

    logger_log(LOG_INFO, "accepted client %s:%s", client_host, client_service);

    bytes_read = socket_utils_read(client_socket, request_buffer, PROXY_SERVER_READ_BUFFER_SIZE);
    if (bytes_read < 0) {
        error_report("failed to read request bytes from client (code=%d)",
            socket_utils_get_last_error());
        socket_utils_close(client_socket);
        socket_utils_close(listener_socket);
        socket_utils_cleanup();
        return -1;
    }

    request_buffer[bytes_read] = '\0';
    logger_log(LOG_INFO, "read %d request bytes", bytes_read);
    log_request_line(request_buffer);

    socket_utils_close(client_socket);
    socket_utils_close(listener_socket);
    socket_utils_cleanup();

    logger_log(LOG_INFO, "single-client server session completed");
    return 0;
}
