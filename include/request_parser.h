#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

typedef struct {
    char method[8];
    char host[256];
    int port;
    char path[2048];
    char version[16];
    char raw_headers[8192];
} http_request_t;

int request_parser_parse(const char *request_text, http_request_t *request);

#endif
