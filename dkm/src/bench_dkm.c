#include "bench.h"
#include <stdio.h>

/* VxWorks shell/C interpreter entrypoints */
int benchServerStart(const char* transport, const char* bind_or_name, int port, const char* name,
                     int mode, int rate_hz, int duration_sec, int payload_len)
{
    bench_endpoint_cfg_t ep = {0};
    ep.transport = transport;
    ep.role = BENCH_ROLE_SERVER;
    ep.bind_or_dst = bind_or_name;
    ep.port = (uint16_t)port;
    ep.name = name;
    ep.timeout_ms = 1000;

    bench_run_cfg_t run = {0};
    run.mode = (bench_mode_t)mode;
    run.rate_hz = rate_hz;
    run.duration_sec = duration_sec;
    run.payload_len = payload_len;
    run.warmup_sec = 1;

    return bench_run_server(&ep, &run);
}

int benchClientRun(const char* transport, const char* dst_or_name, int port, const char* name,
                   const char* tag,
                   int mode, int rate_hz, int duration_sec, int payload_len)
{
    bench_endpoint_cfg_t ep = {0};
    ep.transport = transport;
    ep.role = BENCH_ROLE_CLIENT;
    ep.bind_or_dst = dst_or_name;
    ep.port = (uint16_t)port;
    ep.name = name;
    ep.timeout_ms = 1000;

    bench_run_cfg_t run = {0};
    run.mode = (bench_mode_t)mode;
    run.rate_hz = rate_hz;
    run.duration_sec = duration_sec;
    run.payload_len = payload_len;
    run.warmup_sec = 1;

    bench_report_cfg_t rep = {0};
    rep.tag = tag;

    return bench_run_client(&ep, &run, &rep);
}
