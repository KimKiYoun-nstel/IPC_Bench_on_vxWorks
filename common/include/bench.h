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

typedef struct {
    uint64_t samples;
    uint32_t sent;
    uint32_t received;
    uint32_t loss;
    uint32_t out_of_order;
    uint32_t tx_fail;
    uint64_t min_ns;
    uint64_t p50_ns;
    uint64_t p90_ns;
    uint64_t p99_ns;
    uint64_t p999_ns;
    uint64_t p9999_ns;
    uint64_t max_ns;
    uint64_t over_50us;
    uint64_t over_100us;
    uint64_t over_1ms;
    uint32_t cpu_min_x100;
    uint32_t cpu_avg_x100;
    uint32_t cpu_max_x100;
} bench_result_t;

int bench_run_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run);
int bench_run_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep);
int bench_run_server_result(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                            const bench_report_cfg_t* rep, bench_result_t* out);
int bench_run_client_result(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                            const bench_report_cfg_t* rep, bench_result_t* out);

#ifdef __cplusplus
}
#endif
