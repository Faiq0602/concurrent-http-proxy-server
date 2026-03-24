#ifndef RESPONSE_FORWARDER_H
#define RESPONSE_FORWARDER_H

#include "request_parser.h"
#include "socket_utils.h"

typedef struct {
    unsigned char *data;
    size_t size_bytes;
} forwarder_capture_t;

typedef struct {
    size_t origin_response_bytes;
} forwarder_result_t;

int response_forwarder_forward(socket_handle_t client_socket,
    const http_request_t *request, size_t max_capture_size_bytes,
    forwarder_capture_t *capture, forwarder_result_t *result);
void response_forwarder_capture_free(forwarder_capture_t *capture);

#endif
