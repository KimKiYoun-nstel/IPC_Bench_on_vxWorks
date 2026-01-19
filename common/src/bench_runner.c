#include "bench.h"
#include "bench_proto.h"
#include "bench_stats.h"
#include "bench_time.h"
#include "bench_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#ifdef _WRS_KERNEL
#include <taskLib.h>
#include <sysLib.h>
#endif

static void bench_sleep_until(uint64_t target_ns)
{
    for (;;) {
        uint64_t now = bench_now_ns();
        if (now >= target_ns) return;
#ifdef _WRS_KERNEL
        uint64_t remain_ns = target_ns - now;
        int ticks = (int)((remain_ns * sysClkRateGet()) / 1000000000ull);
        if (ticks <= 0) ticks = 1;
        taskDelay(ticks);
#else
        struct timespec ts;
        uint64_t remain_ns = target_ns - now;
        ts.tv_sec = (time_t)(remain_ns / 1000000000ull);
        ts.tv_nsec = (long)(remain_ns % 1000000000ull);
        nanosleep(&ts, NULL);
#endif
    }
}

static int run_rr_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run)
{
    bench_transport_t t;
    if (bench_transport_create(&t, ep->transport) != 0) return -1;
    if (!t.vtbl || !t.vtbl->open || t.vtbl->open(&t, ep) != 0) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t msg_cap = sizeof(bench_msg_hdr_t) + (size_t)run->payload_len;
    uint8_t* buf = (uint8_t*)malloc(msg_cap);
    if (!buf) {
        bench_transport_destroy(&t);
        return -1;
    }

    uint64_t end_ns = 0;
    if (run->duration_sec > 0) end_ns = bench_now_ns() + (uint64_t)run->duration_sec * 1000000000ull;

    while (end_ns == 0 || bench_now_ns() < end_ns) {
        int n = t.vtbl->recv(&t, buf, msg_cap, ep->timeout_ms);
        if (n <= 0) continue;
        bench_msg_hdr_t hdr;
        if (bench_parse_msg(buf, (size_t)n, &hdr, NULL) != 0) continue;
        if (hdr.type != BENCH_MSG_REQ) continue;
        ((bench_msg_hdr_t*)buf)->type = BENCH_MSG_RSP;
        t.vtbl->send(&t, buf, (size_t)n);
    }

    free(buf);
    bench_transport_destroy(&t);
    return 0;
}

static int run_rr_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep)
{
    if (run->rate_hz <= 0 || run->duration_sec <= 0) return -1;
    if (run->payload_len < 0 || run->payload_len > 65535) return -1;

    bench_transport_t t;
    if (bench_transport_create(&t, ep->transport) != 0) return -1;
    if (!t.vtbl || !t.vtbl->open || t.vtbl->open(&t, ep) != 0) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t samples_target = (size_t)run->rate_hz * (size_t)run->duration_sec;
    uint64_t* samples = (uint64_t*)malloc(sizeof(uint64_t) * samples_target);
    if (!samples) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t msg_cap = sizeof(bench_msg_hdr_t) + (size_t)run->payload_len;
    uint8_t* send_buf = (uint8_t*)malloc(msg_cap);
    uint8_t* recv_buf = (uint8_t*)malloc(msg_cap);
    uint8_t* payload = (uint8_t*)malloc((size_t)run->payload_len);
    if (!send_buf || !recv_buf || (!payload && run->payload_len > 0)) {
        free(samples);
        free(send_buf);
        free(recv_buf);
        free(payload);
        bench_transport_destroy(&t);
        return -1;
    }

    if (payload && run->payload_len > 0) memset(payload, 0xA5, (size_t)run->payload_len);
    bench_stats_t stats;
    bench_stats_init(&stats, samples, samples_target);

    uint64_t period_ns = (uint64_t)(1000000000ull / (uint64_t)run->rate_hz);
    uint64_t start_ns = bench_now_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t next_send_ns = start_ns;

    uint32_t seq = 0;
    while (stats.count < samples_target) {
        bench_sleep_until(next_send_ns);
        next_send_ns += period_ns;

        uint64_t t0 = bench_now_ns();
        size_t len = bench_build_msg(send_buf, msg_cap, BENCH_MSG_REQ, seq, t0,
                                     payload, (uint16_t)run->payload_len);
        if (len == 0) break;
        if (t.vtbl->send(&t, send_buf, len) < 0) {
            stats.loss++;
            stats.sent++;
            seq++;
            continue;
        }
        stats.sent++;

        int n = t.vtbl->recv(&t, recv_buf, msg_cap, ep->timeout_ms);
        if (n <= 0) {
            stats.loss++;
            seq++;
            continue;
        }

        bench_msg_hdr_t hdr;
        if (bench_parse_msg(recv_buf, (size_t)n, &hdr, NULL) != 0) {
            stats.out_of_order++;
            seq++;
            continue;
        }
        if (hdr.type != BENCH_MSG_RSP || hdr.seq != seq) {
            stats.out_of_order++;
            seq++;
            continue;
        }
        stats.received++;

        uint64_t t1 = bench_now_ns();
        if (t1 >= warmup_end_ns) bench_stats_add_sample(&stats, t1 - t0);
        seq++;
    }

    bench_stats_finalize(&stats);
    bench_percentiles_t p = bench_stats_percentiles(&stats);
    printf("[BENCH][RR] tag=%s samples=%" PRIu64 " sent=%u recv=%u loss=%u ooo=%u\n",
           rep && rep->tag ? rep->tag : "rtp",
           (uint64_t)stats.count, stats.sent, stats.received, stats.loss, stats.out_of_order);
    printf("[BENCH][RR] min=%" PRIu64 "ns p50=%" PRIu64 "ns p90=%" PRIu64 "ns p99=%" PRIu64 "ns max=%" PRIu64 "ns\n",
           p.min_ns, p.p50_ns, p.p90_ns, p.p99_ns, p.max_ns);

    free(samples);
    free(send_buf);
    free(recv_buf);
    free(payload);
    bench_transport_destroy(&t);
    return 0;
}

static int run_oneway_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep)
{
    if (run->rate_hz <= 0 || run->duration_sec <= 0) return -1;
    if (run->payload_len < 0 || run->payload_len > 65535) return -1;

    bench_transport_t t;
    if (bench_transport_create(&t, ep->transport) != 0) return -1;
    if (!t.vtbl || !t.vtbl->open || t.vtbl->open(&t, ep) != 0) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t samples_target = (size_t)run->rate_hz * (size_t)run->duration_sec;
    uint64_t* samples = (uint64_t*)malloc(sizeof(uint64_t) * samples_target);
    if (!samples) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t msg_cap = sizeof(bench_msg_hdr_t) + (size_t)run->payload_len;
    uint8_t* recv_buf = (uint8_t*)malloc(msg_cap);
    if (!recv_buf) {
        free(samples);
        bench_transport_destroy(&t);
        return -1;
    }

    bench_stats_t stats;
    bench_stats_init(&stats, samples, samples_target);

    uint64_t period_ns = (uint64_t)(1000000000ull / (uint64_t)run->rate_hz);
    uint64_t start_ns = bench_now_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = start_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t prev_recv_ns = 0;
    uint32_t expect_seq = 0;

    while (bench_now_ns() < end_ns) {
        int n = t.vtbl->recv(&t, recv_buf, msg_cap, ep->timeout_ms);
        if (n <= 0) continue;
        bench_msg_hdr_t hdr;
        if (bench_parse_msg(recv_buf, (size_t)n, &hdr, NULL) != 0) continue;
        if (hdr.type != BENCH_MSG_DATA) continue;

        stats.received++;
        if (hdr.seq != expect_seq) stats.out_of_order++;
        if (hdr.seq > expect_seq) stats.loss += (hdr.seq - expect_seq);
        expect_seq = hdr.seq + 1;

        uint64_t now_ns = bench_now_ns();
        if (prev_recv_ns != 0 && now_ns >= warmup_end_ns) {
            int64_t jitter = (int64_t)(now_ns - prev_recv_ns) - (int64_t)period_ns;
            if (jitter < 0) jitter = -jitter;
            bench_stats_add_sample(&stats, (uint64_t)jitter);
        }
        prev_recv_ns = now_ns;
    }

    bench_stats_finalize(&stats);
    bench_percentiles_t p = bench_stats_percentiles(&stats);
    printf("[BENCH][ONEWAY][RX] tag=%s samples=%" PRIu64 " recv=%u loss=%u ooo=%u\n",
           rep && rep->tag ? rep->tag : "rtp",
           (uint64_t)stats.count, stats.received, stats.loss, stats.out_of_order);
    printf("[BENCH][ONEWAY][RX] jitter_abs min=%" PRIu64 "ns p50=%" PRIu64 "ns p90=%" PRIu64 "ns p99=%" PRIu64 "ns max=%" PRIu64 "ns\n",
           p.min_ns, p.p50_ns, p.p90_ns, p.p99_ns, p.max_ns);

    free(samples);
    free(recv_buf);
    bench_transport_destroy(&t);
    return 0;
}

static int run_oneway_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep)
{
    (void)rep;
    if (run->rate_hz <= 0 || run->duration_sec <= 0) return -1;
    if (run->payload_len < 0 || run->payload_len > 65535) return -1;

    bench_transport_t t;
    if (bench_transport_create(&t, ep->transport) != 0) return -1;
    if (!t.vtbl || !t.vtbl->open || t.vtbl->open(&t, ep) != 0) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t msg_cap = sizeof(bench_msg_hdr_t) + (size_t)run->payload_len;
    uint8_t* send_buf = (uint8_t*)malloc(msg_cap);
    uint8_t* payload = (uint8_t*)malloc((size_t)run->payload_len);
    if (!send_buf || (!payload && run->payload_len > 0)) {
        free(send_buf);
        free(payload);
        bench_transport_destroy(&t);
        return -1;
    }
    if (payload && run->payload_len > 0) memset(payload, 0xA5, (size_t)run->payload_len);

    uint64_t period_ns = (uint64_t)(1000000000ull / (uint64_t)run->rate_hz);
    uint64_t start_ns = bench_now_ns();
    uint64_t end_ns = start_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_send_ns = start_ns;
    uint32_t seq = 0;
    uint32_t sent = 0;

    while (bench_now_ns() < end_ns) {
        bench_sleep_until(next_send_ns);
        next_send_ns += period_ns;

        uint64_t t0 = bench_now_ns();
        size_t len = bench_build_msg(send_buf, msg_cap, BENCH_MSG_DATA, seq, t0,
                                     payload, (uint16_t)run->payload_len);
        if (len == 0) break;
        if (t.vtbl->send(&t, send_buf, len) >= 0) sent++;
        seq++;
    }

    printf("[BENCH][ONEWAY][TX] sent=%u\n", sent);
    free(send_buf);
    free(payload);
    bench_transport_destroy(&t);
    return 0;
}

int bench_run_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run)
{
    if (!ep || !run) return -1;
    if (run->mode == BENCH_MODE_RR) return run_rr_server(ep, run);
    return run_oneway_server(ep, run, NULL);
}

int bench_run_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep)
{
    if (!ep || !run) return -1;
    if (run->mode == BENCH_MODE_RR) return run_rr_client(ep, run, rep);
    return run_oneway_client(ep, run, rep);
}
