#include "bench_log.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static FILE* bench_log_fp = NULL;
static char bench_log_path[256];

void bench_log_set_path(const char* path)
{
    if (!path || !path[0]) {
        bench_log_close();
        return;
    }
    if (bench_log_fp && strcmp(bench_log_path, path) == 0) return;
    bench_log_close();
    strncpy(bench_log_path, path, sizeof(bench_log_path) - 1);
    bench_log_path[sizeof(bench_log_path) - 1] = '\0';
    bench_log_fp = fopen(bench_log_path, "a");
    if (bench_log_fp) {
        setvbuf(bench_log_fp, NULL, _IOLBF, 0);
    }
}

void bench_log_close(void)
{
    if (bench_log_fp) {
        fclose(bench_log_fp);
        bench_log_fp = NULL;
    }
    bench_log_path[0] = '\0';
}

void bench_logf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);

    if (!bench_log_fp) return;
    va_start(ap, fmt);
    vfprintf(bench_log_fp, fmt, ap);
    va_end(ap);
    fflush(bench_log_fp);
}
