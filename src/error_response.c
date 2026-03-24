#include "error_response.h"

#include <stdio.h>
#include <string.h>

#include "socket_utils.h"

int error_response_send(socket_handle_t client_socket, int status_code,
    const char *reason_phrase, const char *message_body)
{
    char response[512];
    const char *body = message_body != NULL ? message_body : "";
    const char *reason = reason_phrase != NULL ? reason_phrase : "Error";
    int written;

    written = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code,
        reason,
        (unsigned int)strlen(body),
        body);
    if (written < 0 || (size_t)written >= sizeof(response)) {
        return -1;
    }

    return socket_utils_write_all(client_socket, response, (size_t)written);
}
