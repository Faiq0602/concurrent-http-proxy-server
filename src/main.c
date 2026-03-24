#include "config.h"
#include "logger.h"
#include "proxy_server.h"

int main(int argc, char **argv)
{
    proxy_config_t config;
    int parse_result;

    parse_result = proxy_config_parse_args(&config, argc, argv);
    if (parse_result != 0) {
        if (parse_result < 0) {
            proxy_config_print_usage(argv[0]);
        }
        return parse_result < 0 ? 1 : 0;
    }

    logger_init(config.log_level);
    proxy_config_log(&config);
    logger_log(LOG_INFO, "starting proxy server");

    return proxy_server_run(&config) == 0 ? 0 : 1;
}
