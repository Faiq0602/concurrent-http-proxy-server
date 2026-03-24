#include "request_parser.h"

#include <stdio.h>
#include <string.h>

static int assert_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "test failure: %s\n", message);
        return 1;
    }

    return 0;
}

static int test_valid_proxy_request(void)
{
    static const char *request_text =
        "GET http://example.com:8080/index.html?q=1 HTTP/1.1\r\n"
        "Host: example.com:8080\r\n"
        "User-Agent: test\r\n"
        "\r\n";
    http_request_t request;

    if (assert_true(request_parser_parse(request_text, &request) == 0,
            "expected valid proxy request to parse") != 0) {
        return 1;
    }

    if (assert_true(strcmp(request.method, "GET") == 0, "method should be GET") != 0 ||
        assert_true(strcmp(request.host, "example.com") == 0, "host should parse") != 0 ||
        assert_true(request.port == 8080, "port should parse") != 0 ||
        assert_true(strcmp(request.path, "/index.html?q=1") == 0, "path should parse") != 0 ||
        assert_true(strcmp(request.version, "HTTP/1.1") == 0, "version should parse") != 0 ||
        assert_true(strstr(request.raw_headers, "Host: example.com:8080") != NULL,
            "headers should be preserved") != 0) {
        return 1;
    }

    return 0;
}

static int test_default_port(void)
{
    static const char *request_text =
        "GET http://example.com/ HTTP/1.0\r\n"
        "Host: example.com\r\n"
        "\r\n";
    http_request_t request;

    if (assert_true(request_parser_parse(request_text, &request) == 0,
            "expected request without explicit port to parse") != 0) {
        return 1;
    }

    if (assert_true(request.port == 80, "default port should be 80") != 0 ||
        assert_true(strcmp(request.path, "/") == 0, "root path should parse") != 0) {
        return 1;
    }

    return 0;
}

static int test_reject_non_get_method(void)
{
    static const char *request_text =
        "POST http://example.com/submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    http_request_t request;

    return assert_true(request_parser_parse(request_text, &request) != 0,
        "non-GET request should be rejected");
}

static int test_reject_relative_url(void)
{
    static const char *request_text =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    http_request_t request;

    return assert_true(request_parser_parse(request_text, &request) != 0,
        "relative URL should be rejected for proxy parser");
}

static int test_reject_missing_headers_terminator(void)
{
    static const char *request_text =
        "GET http://example.com/ HTTP/1.1\r\n"
        "Host: example.com\r\n";
    http_request_t request;

    return assert_true(request_parser_parse(request_text, &request) != 0,
        "request without header terminator should be rejected");
}

int main(void)
{
    if (test_valid_proxy_request() != 0 ||
        test_default_port() != 0 ||
        test_reject_non_get_method() != 0 ||
        test_reject_relative_url() != 0 ||
        test_reject_missing_headers_terminator() != 0) {
        return 1;
    }

    (void)printf("test_request_parser: all tests passed\n");
    return 0;
}
