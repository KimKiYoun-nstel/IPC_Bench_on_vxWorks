#include "bench.h"
#include <stdio.h>

/* TODO: implement RR and ONEWAY loops per docs/01_design.md */
int bench_run_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run)
{
    (void)ep; (void)run;
    printf("[BENCH] server skeleton (implement me)\n");
    return 0;
}

int bench_run_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep)
{
    (void)ep; (void)run; (void)rep;
    printf("[BENCH] client skeleton (implement me)\n");
    return 0;
}
