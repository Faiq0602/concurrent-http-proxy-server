#include "error.h"

#include <stdarg.h>
#include <stdio.h>

static void error_vreport(const char *prefix, const char *fmt, va_list args)
{
    (void)fprintf(stderr, "%s", prefix);
    (void)vfprintf(stderr, fmt, args);
    (void)fputc('\n', stderr);
    (void)fflush(stderr);
}

void error_report(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    error_vreport("error: ", fmt, args);
    va_end(args);
}

void error_report_usage(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    error_vreport("usage error: ", fmt, args);
    va_end(args);
}
