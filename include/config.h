#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#include "logger.h"

#define PROXY_DEFAULT_PORT 8080
#define PROXY_DEFAULT_WORKER_COUNT 4
#define PROXY_DEFAULT_CACHE_SIZE_BYTES 1048576U

typedef struct {
    int port;
    int worker_count;
    size_t cache_size_bytes;
    log_level_t log_level;
} proxy_config_t;

void proxy_config_init_defaults(proxy_config_t *config);
int proxy_config_parse_args(proxy_config_t *config, int argc, char **argv);
void proxy_config_print_usage(const char *program_name);
void proxy_config_log(const proxy_config_t *config);

#endif
