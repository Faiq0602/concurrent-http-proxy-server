#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

void logger_init(log_level_t level);
log_level_t logger_get_level(void);
const char *logger_level_to_string(log_level_t level);
int logger_parse_level(const char *value, log_level_t *out_level);
void logger_log(log_level_t level, const char *fmt, ...);

#endif
