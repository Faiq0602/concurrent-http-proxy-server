#include "config.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

static int parse_int_arg(const char *flag, const char *value, int min, int max, int *out_value)
{
    char *end = NULL;
    long parsed;

    if (value == NULL || out_value == NULL) {
        error_report_usage("%s requires a value", flag);
        return -1;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        error_report_usage("invalid value for %s: '%s'", flag, value);
        return -1;
    }

    if (parsed < min || parsed > max) {
        error_report_usage("%s must be between %d and %d", flag, min, max);
        return -1;
    }

    *out_value = (int)parsed;
    return 0;
}

static int parse_size_arg(const char *flag, const char *value, size_t *out_value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (value == NULL || out_value == NULL) {
        error_report_usage("%s requires a value", flag);
        return -1;
    }

    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        error_report_usage("invalid value for %s: '%s'", flag, value);
        return -1;
    }

    if (parsed == 0 || parsed > (unsigned long long)SIZE_MAX) {
        error_report_usage("%s must be between 1 and %llu", flag,
            (unsigned long long)SIZE_MAX);
        return -1;
    }

    *out_value = (size_t)parsed;
    return 0;
}

static int parse_log_level_arg(const char *value, log_level_t *out_level)
{
    if (logger_parse_level(value, out_level) != 0) {
        error_report_usage("invalid value for --log-level: '%s' (expected debug, info, warn, or error)",
            value);
        return -1;
    }

    return 0;
}

static int require_value(int argc, char **argv, int index)
{
    if (index + 1 >= argc) {
        error_report_usage("missing value for %s", argv[index]);
        return -1;
    }

    return 0;
}

void proxy_config_init_defaults(proxy_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->port = PROXY_DEFAULT_PORT;
    config->worker_count = PROXY_DEFAULT_WORKER_COUNT;
    config->cache_size_bytes = PROXY_DEFAULT_CACHE_SIZE_BYTES;
    config->log_level = LOG_INFO;
}

int proxy_config_parse_args(proxy_config_t *config, int argc, char **argv)
{
    int index;

    if (config == NULL || argv == NULL || argc < 1) {
        error_report("configuration parser received invalid arguments");
        return -1;
    }

    proxy_config_init_defaults(config);

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--help") == 0) {
            proxy_config_print_usage(argv[0]);
            return 1;
        }

        if (strcmp(argv[index], "--port") == 0) {
            if (require_value(argc, argv, index) != 0 ||
                parse_int_arg("--port", argv[++index], 1, 65535, &config->port) != 0) {
                return -1;
            }
            continue;
        }

        if (strcmp(argv[index], "--workers") == 0) {
            if (require_value(argc, argv, index) != 0 ||
                parse_int_arg("--workers", argv[++index], 1, 256, &config->worker_count) != 0) {
                return -1;
            }
            continue;
        }

        if (strcmp(argv[index], "--cache-size") == 0) {
            if (require_value(argc, argv, index) != 0 ||
                parse_size_arg("--cache-size", argv[++index], &config->cache_size_bytes) != 0) {
                return -1;
            }
            continue;
        }

        if (strcmp(argv[index], "--log-level") == 0) {
            if (require_value(argc, argv, index) != 0 ||
                parse_log_level_arg(argv[++index], &config->log_level) != 0) {
                return -1;
            }
            continue;
        }

        error_report_usage("unknown argument: %s", argv[index]);
        return -1;
    }

    return 0;
}

void proxy_config_print_usage(const char *program_name)
{
    const char *name = program_name != NULL ? program_name : "proxy_server";

    (void)fprintf(stderr,
        "Usage: %s [--port PORT] [--workers COUNT] [--cache-size BYTES] [--log-level LEVEL]\n"
        "       %s --help\n"
        "\n"
        "Options:\n"
        "  --port PORT          Listening port (default: %d)\n"
        "  --workers COUNT      Worker thread count (default: %d)\n"
        "  --cache-size BYTES   Shared cache capacity in bytes (default: %u)\n"
        "  --log-level LEVEL    Log level: debug, info, warn, error (default: info)\n",
        name,
        name,
        PROXY_DEFAULT_PORT,
        PROXY_DEFAULT_WORKER_COUNT,
        PROXY_DEFAULT_CACHE_SIZE_BYTES);
}

void proxy_config_log(const proxy_config_t *config)
{
    if (config == NULL) {
        return;
    }

    logger_log(LOG_INFO,
        "config: port=%d workers=%d cache_size_bytes=%lu log_level=%s",
        config->port,
        config->worker_count,
        (unsigned long)config->cache_size_bytes,
        logger_level_to_string(config->log_level));
}
