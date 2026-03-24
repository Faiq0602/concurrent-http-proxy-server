#ifndef ERROR_RESPONSE_H
#define ERROR_RESPONSE_H

#include "socket_utils.h"

int error_response_send(socket_handle_t client_socket, int status_code,
    const char *reason_phrase, const char *message_body);

#endif
