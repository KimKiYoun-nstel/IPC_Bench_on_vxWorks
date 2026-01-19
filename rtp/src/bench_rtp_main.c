#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    printf("bench_rtp usage:\n");
    printf("  bench_rtp -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr\n");
    printf("  bench_rtp -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 30 --payload 256 --tag udp_loop\n");
}

int main(int argc, char** argv)
{
    int is_server = 0, is_client = 0;
    const char* transport = "udp";
    const char* bind_ip = "0.0.0.0";
    const char* dst_ip  = "127.0.0.1";
    int port = 41000;
    int mode = BENCH_MODE_RR;
    int rate = 200;
    int dur = 30;
    int payload = 256;
    const char* tag = "rtp";

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-s")) is_server = 1;
        else if (!strcmp(argv[i], "-c")) is_client = 1;
        else if (!strcmp(argv[i], "--transport") && i+1<argc) transport = argv[++i];
        else if (!strcmp(argv[i], "--bind") && i+1<argc) bind_ip = argv[++i];
        else if (!strcmp(argv[i], "--dst") && i+1<argc) dst_ip = argv[++i];
        else if (!strcmp(argv[i], "--port") && i+1<argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mode") && i+1<argc) {
            const char* m = argv[++i];
            mode = (!strcmp(m, "rr")) ? BENCH_MODE_RR : BENCH_MODE_ONEWAY;
        }
        else if (!strcmp(argv[i], "--rate") && i+1<argc) rate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dur") && i+1<argc) dur = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--payload") && i+1<argc) payload = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tag") && i+1<argc) tag = argv[++i];
        else { usage(); return 1; }
    }

    if (is_server == is_client) { usage(); return 1; }

    bench_endpoint_cfg_t ep = {0};
    ep.transport = transport;
    ep.role = is_server ? BENCH_ROLE_SERVER : BENCH_ROLE_CLIENT;
    ep.bind_or_dst = is_server ? bind_ip : dst_ip;
    ep.port = (uint16_t)port;
    ep.timeout_ms = 1000;

    bench_run_cfg_t run = {0};
    run.mode = (bench_mode_t)mode;
    run.rate_hz = rate;
    run.duration_sec = dur;
    run.payload_len = payload;
    run.warmup_sec = 1;

    bench_report_cfg_t rep = {0};
    rep.tag = tag;

    return is_server ? bench_run_server(&ep, &run) : bench_run_client(&ep, &run, &rep);
}
