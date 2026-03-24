#include "logger.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    log_level_t level;
    const char *name;
} log_level_entry_t;

static const log_level_entry_t LOG_LEVELS[] = {
    { LOG_DEBUG, "DEBUG" },
    { LOG_INFO, "INFO" },
    { LOG_WARN, "WARN" },
    { LOG_ERROR, "ERROR" }
};

static log_level_t current_level = LOG_INFO;

static int format_timestamp(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm local_time;
    struct tm *local_time_ptr;

    now = time(NULL);

    local_time_ptr = localtime(&now);
    if (local_time_ptr == NULL) {
        return -1;
    }
    local_time = *local_time_ptr;

    if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        return -1;
    }

    return 0;
}

static int string_equals_ignore_case(const char *left, const char *right)
{
    unsigned char left_char;
    unsigned char right_char;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        left_char = (unsigned char)*left;
        right_char = (unsigned char)*right;
        if (tolower(left_char) != tolower(right_char)) {
            return 0;
        }

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

void logger_init(log_level_t level)
{
    current_level = level;
}

log_level_t logger_get_level(void)
{
    return current_level;
}

const char *logger_level_to_string(log_level_t level)
{
    size_t index;

    for (index = 0; index < sizeof(LOG_LEVELS) / sizeof(LOG_LEVELS[0]); ++index) {
        if (LOG_LEVELS[index].level == level) {
            return LOG_LEVELS[index].name;
        }
    }

    return "UNKNOWN";
}

int logger_parse_level(const char *value, log_level_t *out_level)
{
    size_t index;

    if (value == NULL || out_level == NULL) {
        return -1;
    }

    for (index = 0; index < sizeof(LOG_LEVELS) / sizeof(LOG_LEVELS[0]); ++index) {
        if (string_equals_ignore_case(value, LOG_LEVELS[index].name)) {
            *out_level = LOG_LEVELS[index].level;
            return 0;
        }
    }

    return -1;
}

void logger_log(log_level_t level, const char *fmt, ...)
{
    va_list args;
    char timestamp[20];

    if (level < current_level) {
        return;
    }

    if (format_timestamp(timestamp, sizeof(timestamp)) != 0) {
        (void)snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    (void)fprintf(stderr, "[%s] %-5s ", timestamp, logger_level_to_string(level));

    va_start(args, fmt);
    (void)vfprintf(stderr, fmt, args);
    va_end(args);

    (void)fputc('\n', stderr);
}
