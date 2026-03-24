#include "response_forwarder.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "error.h"
#include "logger.h"

#define FORWARDER_REQUEST_BUFFER_SIZE 12288
#define FORWARDER_RESPONSE_BUFFER_SIZE 8192

static int header_name_equals(const char *line, size_t name_length, const char *expected)
{
    size_t index;

    if (strlen(expected) != name_length) {
        return 0;
    }

    for (index = 0; index < name_length; ++index) {
        char left = line[index];
        char right = expected[index];

        if (left >= 'A' && left <= 'Z') {
            left = (char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (char)(right - 'A' + 'a');
        }

        if (left != right) {
            return 0;
        }
    }

    return 1;
}

static int append_filtered_headers(char *buffer, size_t buffer_size,
    size_t *offset, const char *raw_headers)
{
    const char *line_start = raw_headers;

    while (line_start != NULL && *line_start != '\0') {
        const char *line_end = strstr(line_start, "\r\n");
        const char *colon;
        size_t line_length;
        int should_skip = 0;

        if (line_end == NULL) {
            line_end = line_start + strlen(line_start);
            line_length = (size_t)(line_end - line_start);
        } else {
            line_length = (size_t)(line_end - line_start);
        }

        if (line_length == 0) {
            break;
        }

        colon = memchr(line_start, ':', line_length);
        if (colon != NULL) {
            size_t name_length = (size_t)(colon - line_start);

            if (header_name_equals(line_start, name_length, "host") ||
                header_name_equals(line_start, name_length, "connection") ||
                header_name_equals(line_start, name_length, "proxy-connection")) {
                should_skip = 1;
            }
        }

        if (!should_skip) {
            int written = snprintf(buffer + *offset, buffer_size - *offset, "%.*s\r\n",
                (int)line_length, line_start);
            if (written < 0 || (size_t)written >= buffer_size - *offset) {
                return -1;
            }

            *offset += (size_t)written;
        }

        if (*line_end == '\0') {
            break;
        }

        line_start = line_end + 2;
    }

    return 0;
}

static int build_origin_request(const http_request_t *request,
    char *buffer, size_t buffer_size)
{
    int written;
    size_t offset = 0;

    written = snprintf(buffer, buffer_size,
        "%s %s %s\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n",
        request->method,
        request->path,
        request->version,
        request->host,
        request->port);
    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }

    offset = (size_t)written;
    if (append_filtered_headers(buffer, buffer_size, &offset, request->raw_headers) != 0) {
        return -1;
    }

    written = snprintf(buffer + offset, buffer_size - offset, "\r\n");
    if (written < 0 || (size_t)written >= buffer_size - offset) {
        return -1;
    }

    offset += (size_t)written;
    return (int)offset;
}

int response_forwarder_forward(socket_handle_t client_socket,
    const http_request_t *request, size_t max_capture_size_bytes,
    forwarder_capture_t *capture, forwarder_result_t *result)
{
    socket_handle_t origin_socket = SOCKET_HANDLE_INVALID;
    char outbound_request[FORWARDER_REQUEST_BUFFER_SIZE];
    char response_buffer[FORWARDER_RESPONSE_BUFFER_SIZE];
    unsigned char *capture_buffer = NULL;
    size_t capture_size = 0;
    size_t capture_capacity = 0;
    int capture_enabled = 0;
    int request_size;
    int bytes_read;

    if (request == NULL) {
        error_report("cannot forward a null request");
        return -1;
    }

    if (capture != NULL) {
        capture->data = NULL;
        capture->size_bytes = 0;
        capture_enabled = max_capture_size_bytes > 0;
    }
    if (result != NULL) {
        result->origin_response_bytes = 0;
    }

    origin_socket = socket_utils_connect_to_host(request->host, request->port);
    if (origin_socket == SOCKET_HANDLE_INVALID) {
        error_report("failed to connect to origin %s:%d (code=%d)",
            request->host, request->port, socket_utils_get_last_error());
        return -1;
    }

    if (socket_utils_set_timeout(origin_socket, PROXY_SOCKET_TIMEOUT_MS) != 0) {
        error_report("failed to set origin socket timeout for %s:%d (code=%d)",
            request->host, request->port, socket_utils_get_last_error());
        socket_utils_close(origin_socket);
        return -1;
    }

    logger_log(LOG_INFO, "connected to origin %s:%d", request->host, request->port);

    request_size = build_origin_request(request, outbound_request, sizeof(outbound_request));
    if (request_size < 0) {
        error_report("failed to build outbound request for origin server");
        socket_utils_close(origin_socket);
        return -1;
    }

    if (socket_utils_write_all(origin_socket, outbound_request, (size_t)request_size) != 0) {
        error_report("failed to send request to origin %s:%d (code=%d)",
            request->host, request->port, socket_utils_get_last_error());
        socket_utils_close(origin_socket);
        return -1;
    }

    logger_log(LOG_INFO, "forwarded request to origin %s:%d", request->host, request->port);

    for (;;) {
        bytes_read = socket_utils_read(origin_socket, response_buffer, sizeof(response_buffer));
        if (bytes_read < 0) {
            error_report("failed while reading response from origin %s:%d (code=%d)",
                request->host, request->port, socket_utils_get_last_error());
            socket_utils_close(origin_socket);
            return -1;
        }

        if (bytes_read == 0) {
            break;
        }

        if (result != NULL) {
            result->origin_response_bytes += (size_t)bytes_read;
        }

        if (capture_enabled && capture != NULL) {
            if (capture_size + (size_t)bytes_read > max_capture_size_bytes) {
                free(capture_buffer);
                capture_buffer = NULL;
                capture_size = 0;
                capture_capacity = 0;
                capture_enabled = 0;
            } else if (capture_size + (size_t)bytes_read > capture_capacity) {
                size_t next_capacity = capture_capacity == 0 ? (size_t)bytes_read : capture_capacity * 2;
                unsigned char *next_buffer;

                while (next_capacity < capture_size + (size_t)bytes_read) {
                    next_capacity *= 2;
                }

                next_buffer = (unsigned char *)realloc(capture_buffer, next_capacity);
                if (next_buffer == NULL) {
                    free(capture_buffer);
                    capture_buffer = NULL;
                    capture_size = 0;
                    capture_capacity = 0;
                    capture_enabled = 0;
                } else {
                    capture_buffer = next_buffer;
                    capture_capacity = next_capacity;
                }
            }

            if (capture_enabled) {
                (void)memcpy(capture_buffer + capture_size, response_buffer, (size_t)bytes_read);
                capture_size += (size_t)bytes_read;
            }
        }

        if (socket_utils_write_all(client_socket, response_buffer, (size_t)bytes_read) != 0) {
            error_report("failed while relaying response to client (code=%d)",
                socket_utils_get_last_error());
            socket_utils_close(origin_socket);
            free(capture_buffer);
            return -1;
        }
    }

    socket_utils_close(origin_socket);
    if (capture != NULL && capture_enabled && capture_buffer != NULL) {
        capture->data = capture_buffer;
        capture->size_bytes = capture_size;
    } else {
        free(capture_buffer);
    }
    logger_log(LOG_INFO, "completed response relay for %s:%d", request->host, request->port);
    return 0;
}

void response_forwarder_capture_free(forwarder_capture_t *capture)
{
    if (capture == NULL) {
        return;
    }

    free(capture->data);
    capture->data = NULL;
    capture->size_bytes = 0;
}
