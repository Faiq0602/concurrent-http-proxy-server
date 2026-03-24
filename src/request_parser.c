#include "request_parser.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int copy_substring(char *destination, size_t destination_size,
    const char *start, size_t length)
{
    if (destination == NULL || destination_size == 0 || start == NULL) {
        return -1;
    }

    if (length >= destination_size) {
        return -1;
    }

    (void)memcpy(destination, start, length);
    destination[length] = '\0';
    return 0;
}

static const char *find_line_end(const char *text)
{
    const char *cursor = text;

    while (cursor != NULL && *cursor != '\0') {
        if (*cursor == '\r' || *cursor == '\n') {
            return cursor;
        }
        ++cursor;
    }

    return NULL;
}

static const char *skip_line_break(const char *cursor)
{
    if (cursor == NULL) {
        return NULL;
    }

    if (cursor[0] == '\r' && cursor[1] == '\n') {
        return cursor + 2;
    }

    if (cursor[0] == '\r' || cursor[0] == '\n') {
        return cursor + 1;
    }

    return cursor;
}

static int parse_request_line(const char *request_line, size_t line_length,
    http_request_t *request, const char **url_start, size_t *url_length)
{
    const char *first_space;
    const char *second_space;
    size_t method_length;
    size_t version_length;

    first_space = memchr(request_line, ' ', line_length);
    if (first_space == NULL) {
        return -1;
    }

    second_space = memchr(first_space + 1, ' ',
        line_length - (size_t)((first_space + 1) - request_line));
    if (second_space == NULL) {
        return -1;
    }

    method_length = (size_t)(first_space - request_line);
    if (copy_substring(request->method, sizeof(request->method), request_line, method_length) != 0) {
        return -1;
    }

    if (strcmp(request->method, "GET") != 0) {
        return -1;
    }

    *url_start = first_space + 1;
    *url_length = (size_t)(second_space - *url_start);
    if (*url_length == 0) {
        return -1;
    }

    version_length = line_length - (size_t)((second_space + 1) - request_line);
    if (copy_substring(request->version, sizeof(request->version), second_space + 1,
            version_length) != 0) {
        return -1;
    }

    if (strcmp(request->version, "HTTP/1.0") != 0 &&
        strcmp(request->version, "HTTP/1.1") != 0) {
        return -1;
    }

    return 0;
}

static int parse_port_value(const char *text, size_t length, int *port)
{
    char port_buffer[16];
    char *end = NULL;
    long parsed_port;

    if (copy_substring(port_buffer, sizeof(port_buffer), text, length) != 0) {
        return -1;
    }

    parsed_port = strtol(port_buffer, &end, 10);
    if (end == port_buffer || *end != '\0' || parsed_port < 1 || parsed_port > 65535) {
        return -1;
    }

    *port = (int)parsed_port;
    return 0;
}

static int parse_absolute_url(const char *url_start, size_t url_length, http_request_t *request)
{
    const char *scheme = "http://";
    const size_t scheme_length = 7;
    const char *authority_start;
    const char *path_start;
    const char *host_end;
    size_t authority_length;
    size_t host_length;

    if (url_length <= scheme_length || strncmp(url_start, scheme, scheme_length) != 0) {
        return -1;
    }

    authority_start = url_start + scheme_length;
    path_start = memchr(authority_start, '/', url_length - scheme_length);
    if (path_start == NULL) {
        path_start = url_start + url_length;
        if (copy_substring(request->path, sizeof(request->path), "/", 1) != 0) {
            return -1;
        }
    } else if (copy_substring(request->path, sizeof(request->path), path_start,
            (size_t)((url_start + url_length) - path_start)) != 0) {
        return -1;
    }

    authority_length = (size_t)(path_start - authority_start);
    if (authority_length == 0) {
        return -1;
    }

    host_end = memchr(authority_start, ':', authority_length);
    if (host_end == NULL) {
        host_length = authority_length;
        request->port = 80;
    } else {
        host_length = (size_t)(host_end - authority_start);
        if (parse_port_value(host_end + 1,
                authority_length - host_length - 1, &request->port) != 0) {
            return -1;
        }
    }

    if (host_length == 0) {
        return -1;
    }

    if (copy_substring(request->host, sizeof(request->host), authority_start, host_length) != 0) {
        return -1;
    }

    return 0;
}

static int contains_header_terminator(const char *headers)
{
    return strstr(headers, "\r\n\r\n") != NULL || strstr(headers, "\n\n") != NULL;
}

int request_parser_parse(const char *request_text, http_request_t *request)
{
    const char *line_end;
    const char *headers_start;
    const char *url_start = NULL;
    size_t line_length;
    size_t url_length = 0;

    if (request_text == NULL || request == NULL) {
        return -1;
    }

    (void)memset(request, 0, sizeof(*request));

    line_end = find_line_end(request_text);
    if (line_end == NULL) {
        return -1;
    }

    line_length = (size_t)(line_end - request_text);
    if (line_length == 0) {
        return -1;
    }

    if (parse_request_line(request_text, line_length, request, &url_start, &url_length) != 0) {
        return -1;
    }

    if (parse_absolute_url(url_start, url_length, request) != 0) {
        return -1;
    }

    headers_start = skip_line_break(line_end);
    if (headers_start == NULL || !contains_header_terminator(headers_start)) {
        return -1;
    }

    if (copy_substring(request->raw_headers, sizeof(request->raw_headers),
            headers_start, strlen(headers_start)) != 0) {
        return -1;
    }

    return 0;
}
