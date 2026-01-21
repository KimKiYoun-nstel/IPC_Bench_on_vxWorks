#pragma once
#include "bench.h"

#ifdef __cplusplus
extern "C" {
#endif

int bench_csv_append(const char* path,
                     const bench_endpoint_cfg_t* ep,
                     const bench_run_cfg_t* run,
                     const bench_report_cfg_t* rep,
                     const bench_result_t* res);

#ifdef __cplusplus
}
#endif
