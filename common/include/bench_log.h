#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void bench_log_set_path(const char* path);
void bench_log_close(void);
void bench_logf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
