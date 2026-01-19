#pragma once
#include "bench_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bench_mode_t mode;
    int rate_hz;
    int duration_sec;
    int payload_len;
    int warmup_sec;
} bench_run_cfg_t;

typedef struct {
    const char* csv_path; /* optional */
    const char* tag;      /* label */
} bench_report_cfg_t;

int bench_run_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run);
int bench_run_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep);

#ifdef __cplusplus
}
#endif
