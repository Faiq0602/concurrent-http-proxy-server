#ifndef RESPONSE_FORWARDER_H
#define RESPONSE_FORWARDER_H

#include "request_parser.h"
#include "socket_utils.h"

int response_forwarder_forward(socket_handle_t client_socket,
    const http_request_t *request);

#endif
