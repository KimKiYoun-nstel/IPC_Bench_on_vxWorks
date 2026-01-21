#include "bench.h"
#include "bench_csv.h"
#include "bench_log.h"
#include "bench_tag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    printf("bench_rtp.vxe usage:\n");
    printf("  bench_rtp.vxe -s --transport udp --bind 0.0.0.0 --port 41000 --mode rr\n");
    printf("  bench_rtp.vxe -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 30 --payload 256 --tag udp_loop\n");
    printf("  bench_rtp.vxe -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 30 --payload 256 --csv /tffs0/IPC_Bench/bench_results.csv\n");
    printf("  bench_rtp.vxe -c --transport udp --dst 127.0.0.1 --port 41000 --mode rr --rate 200 --dur 30 --payload 256 --log /tffs0/IPC_Bench/bench_auto.log\n");
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
    const char* csv_path = NULL;
    const char* log_path = NULL;

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
        else if (!strcmp(argv[i], "--csv") && i+1<argc) csv_path = argv[++i];
        else if (!strcmp(argv[i], "--log") && i+1<argc) log_path = argv[++i];
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
    rep.csv_path = csv_path;
    if (!rep.csv_path && rep.tag) {
        char csv_buf[256];
        if (bench_tag_build_path(rep.tag, ".csv", csv_buf, sizeof(csv_buf)) == 0) {
            rep.csv_path = csv_buf;
        }
    }

    if (log_path) {
        bench_log_set_path(log_path);
    } else if (rep.tag) {
        char log_buf[256];
        if (bench_tag_build_path(rep.tag, ".log", log_buf, sizeof(log_buf)) == 0) {
            bench_log_set_path(log_buf);
        }
    }

    bench_result_t result = {0};
    int rc = is_server ? bench_run_server_result(&ep, &run, &rep, &result)
                       : bench_run_client_result(&ep, &run, &rep, &result);
    if (rc == 0 && rep.csv_path) {
        (void)bench_csv_append(rep.csv_path, &ep, &run, &rep, &result);
    }
    return rc;
}
