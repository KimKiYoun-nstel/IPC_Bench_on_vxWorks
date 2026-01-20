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

#define BENCH_MAX_PAYLOAD 2048
#ifdef _WRS_KERNEL
#define BENCH_SIDE "DKM"
#else
#define BENCH_SIDE "RTP"
#endif

#ifndef BENCH_PACING_HYBRID
#define BENCH_PACING_HYBRID 0
#endif

#ifndef BENCH_PACING_SPIN_NS
#define BENCH_PACING_SPIN_NS 400000u
#endif

#define BENCH_LOG_INTERVAL_NS 1000000000ull

static int bench_log_due(uint64_t now_ns, uint64_t* next_log_ns)
{
    if (now_ns < *next_log_ns) return 0;
    while (now_ns >= *next_log_ns) *next_log_ns += BENCH_LOG_INTERVAL_NS;
    return 1;
}

static void bench_spin_until(uint64_t target_ns)
{
    while (bench_now_ns() < target_ns) {
    }
}

#ifdef _WRS_KERNEL
static int bench_ns_to_ticks(uint64_t ns)
{
    int ticks = (int)((ns * sysClkRateGet()) / 1000000000ull);
    return (ticks <= 0) ? 1 : ticks;
}
#endif

static void bench_sleep_until(uint64_t target_ns)
{
    for (;;) {
        uint64_t now = bench_now_ns();
        if (now >= target_ns) return;
#ifdef _WRS_KERNEL
        uint64_t remain_ns = target_ns - now;
#if BENCH_PACING_HYBRID
        if (remain_ns > BENCH_PACING_SPIN_NS) {
            uint64_t sleep_ns = remain_ns - BENCH_PACING_SPIN_NS;
            taskDelay(bench_ns_to_ticks(sleep_ns));
        } else {
            bench_spin_until(target_ns);
            return;
        }
#else
        taskDelay(bench_ns_to_ticks(remain_ns));
#endif
#else
        struct timespec ts;
        uint64_t remain_ns = target_ns - now;
#if BENCH_PACING_HYBRID
        if (remain_ns > BENCH_PACING_SPIN_NS) {
            uint64_t sleep_ns = remain_ns - BENCH_PACING_SPIN_NS;
            ts.tv_sec = (time_t)(sleep_ns / 1000000000ull);
            ts.tv_nsec = (long)(sleep_ns % 1000000000ull);
            nanosleep(&ts, NULL);
        } else {
            bench_spin_until(target_ns);
            return;
        }
#else
        ts.tv_sec = (time_t)(remain_ns / 1000000000ull);
        ts.tv_nsec = (long)(remain_ns % 1000000000ull);
        nanosleep(&ts, NULL);
#endif
#endif
    }
}

static int run_rr_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run)
{
    if (run->duration_sec <= 0) return -1;
    bench_transport_t t;
    if (bench_transport_create(&t, ep->transport) != 0) return -1;
    if (!t.vtbl || !t.vtbl->open || t.vtbl->open(&t, ep) != 0) {
        bench_transport_destroy(&t);
        return -1;
    }

    size_t msg_cap = sizeof(bench_msg_hdr_t) + (size_t)BENCH_MAX_PAYLOAD;
    uint8_t* buf = (uint8_t*)malloc(msg_cap);
    if (!buf) {
        bench_transport_destroy(&t);
        return -1;
    }

    uint64_t start_ns = bench_now_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t recv_cnt = 0;
    uint32_t rsp_cnt = 0;
    uint32_t last_recv = 0;
    uint32_t last_rsp = 0;

    while (bench_now_ns() < end_ns) {
        int n = t.vtbl->recv(&t, buf, msg_cap, ep->timeout_ms);
        if (n > 0) {
            bench_msg_hdr_t hdr;
            if (bench_parse_msg(buf, (size_t)n, &hdr, NULL) == 0 && hdr.type == BENCH_MSG_REQ) {
                recv_cnt++;
                ((bench_msg_hdr_t*)buf)->type = BENCH_MSG_RSP;
                if (t.vtbl->send(&t, buf, (size_t)n) >= 0) rsp_cnt++;
            }
        }

        {
            uint64_t now = bench_now_ns();
            if (bench_log_due(now, &next_log_ns)) {
                uint32_t d_recv = recv_cnt - last_recv;
                uint32_t d_rsp = rsp_cnt - last_rsp;
                uint32_t sec = (uint32_t)((now - start_ns) / 1000000000ull);
                printf("[BENCH][%s][RR][SRV] t=%us recv/s=%u rsp/s=%u total_recv=%u total_rsp=%u\n",
                       BENCH_SIDE, sec, d_recv, d_rsp, recv_cnt, rsp_cnt);
                last_recv = recv_cnt;
                last_rsp = rsp_cnt;
            }
        }
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

    size_t msg_cap = sizeof(bench_msg_hdr_t) + (size_t)BENCH_MAX_PAYLOAD;
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
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_send_ns = start_ns;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t last_sent = 0;
    uint32_t last_recv = 0;
    uint32_t last_loss = 0;
    uint32_t last_ooo = 0;
    uint32_t last_fail = 0;

    uint32_t seq = 0;
    while (1) {
        if (bench_now_ns() >= end_ns) break;
        if (next_send_ns > end_ns) break;
        bench_sleep_until(next_send_ns);
        next_send_ns += period_ns;

        uint64_t t0 = bench_now_ns();
        size_t len = bench_build_msg(send_buf, msg_cap, BENCH_MSG_REQ, seq, t0,
                                     payload, (uint16_t)run->payload_len);
        if (len == 0) break;
        if (t.vtbl->send(&t, send_buf, len) < 0) {
            stats.tx_fail++;
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

        {
            uint64_t now = bench_now_ns();
            if (bench_log_due(now, &next_log_ns)) {
                uint32_t d_sent = stats.sent - last_sent;
                uint32_t d_recv = stats.received - last_recv;
                uint32_t d_loss = stats.loss - last_loss;
                uint32_t d_ooo = stats.out_of_order - last_ooo;
                uint32_t d_fail = stats.tx_fail - last_fail;
                uint32_t sec = (uint32_t)((now - start_ns) / 1000000000ull);
                printf("[BENCH][%s][RR][CLI] t=%us sent/s=%u recv/s=%u loss/s=%u ooo/s=%u tx_fail/s=%u\n",
                       BENCH_SIDE, sec, d_sent, d_recv, d_loss, d_ooo, d_fail);
                last_sent = stats.sent;
                last_recv = stats.received;
                last_loss = stats.loss;
                last_ooo = stats.out_of_order;
                last_fail = stats.tx_fail;
            }
        }
    }

    bench_stats_finalize(&stats);
    bench_percentiles_t p = bench_stats_percentiles(&stats);
    printf("[BENCH][%s][RR] transport=%s tag=%s rate=%d dur=%d warmup=%d payload=%d timeout_ms=%d\n",
           BENCH_SIDE, ep->transport ? ep->transport : "unknown",
           rep && rep->tag ? rep->tag : "rtp",
           run->rate_hz, run->duration_sec, run->warmup_sec, run->payload_len, ep->timeout_ms);
    printf("[BENCH][%s][RR] samples=%" PRIu64 " sent=%u recv=%u loss=%u ooo=%u tx_fail=%u\n",
           BENCH_SIDE, (uint64_t)stats.count, stats.sent, stats.received, stats.loss, stats.out_of_order, stats.tx_fail);
    printf("[BENCH][%s][RR] min=%" PRIu64 "ns p50=%" PRIu64 "ns p90=%" PRIu64 "ns p99=%" PRIu64 "ns max=%" PRIu64 "ns\n",
           BENCH_SIDE, p.min_ns, p.p50_ns, p.p90_ns, p.p99_ns, p.max_ns);

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
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t prev_recv_ns = 0;
    uint32_t expect_seq = 0;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t last_recv = 0;
    uint32_t last_loss = 0;
    uint32_t last_ooo = 0;

    while (bench_now_ns() < end_ns) {
        int n = t.vtbl->recv(&t, recv_buf, msg_cap, ep->timeout_ms);
        uint64_t now_ns = bench_now_ns();
        if (n > 0) {
            bench_msg_hdr_t hdr;
            if (bench_parse_msg(recv_buf, (size_t)n, &hdr, NULL) == 0 && hdr.type == BENCH_MSG_DATA) {
                stats.received++;
                if (hdr.seq != expect_seq) stats.out_of_order++;
                if (hdr.seq > expect_seq) stats.loss += (hdr.seq - expect_seq);
                expect_seq = hdr.seq + 1;

                if (prev_recv_ns != 0 && now_ns >= warmup_end_ns) {
                    int64_t jitter = (int64_t)(now_ns - prev_recv_ns) - (int64_t)period_ns;
                    if (jitter < 0) jitter = -jitter;
                    bench_stats_add_sample(&stats, (uint64_t)jitter);
                }
                prev_recv_ns = now_ns;
            }
        }

        if (bench_log_due(now_ns, &next_log_ns)) {
            uint32_t d_recv = stats.received - last_recv;
            uint32_t d_loss = stats.loss - last_loss;
            uint32_t d_ooo = stats.out_of_order - last_ooo;
            uint32_t sec = (uint32_t)((now_ns - start_ns) / 1000000000ull);
            printf("[BENCH][%s][ONEWAY][RX] t=%us recv/s=%u loss/s=%u ooo/s=%u\n",
                   BENCH_SIDE, sec, d_recv, d_loss, d_ooo);
            last_recv = stats.received;
            last_loss = stats.loss;
            last_ooo = stats.out_of_order;
        }
    }

    bench_stats_finalize(&stats);
    bench_percentiles_t p = bench_stats_percentiles(&stats);
    printf("[BENCH][%s][ONEWAY][RX] transport=%s tag=%s rate=%d dur=%d warmup=%d payload=%d timeout_ms=%d\n",
           BENCH_SIDE, ep->transport ? ep->transport : "unknown",
           rep && rep->tag ? rep->tag : "rtp",
           run->rate_hz, run->duration_sec, run->warmup_sec, run->payload_len, ep->timeout_ms);
    printf("[BENCH][%s][ONEWAY][RX] samples=%" PRIu64 " recv=%u loss=%u ooo=%u\n",
           BENCH_SIDE, (uint64_t)stats.count, stats.received, stats.loss, stats.out_of_order);
    printf("[BENCH][%s][ONEWAY][RX] jitter_abs min=%" PRIu64 "ns p50=%" PRIu64 "ns p90=%" PRIu64 "ns p99=%" PRIu64 "ns max=%" PRIu64 "ns\n",
           BENCH_SIDE, p.min_ns, p.p50_ns, p.p90_ns, p.p99_ns, p.max_ns);

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
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_send_ns = start_ns;
    uint32_t seq = 0;
    uint32_t sent = 0;
    uint32_t tx_fail = 0;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t last_sent = 0;
    uint32_t last_fail = 0;

    while (1) {
        if (bench_now_ns() >= end_ns) break;
        if (next_send_ns > end_ns) break;
        bench_sleep_until(next_send_ns);
        next_send_ns += period_ns;

        uint64_t t0 = bench_now_ns();
        size_t len = bench_build_msg(send_buf, msg_cap, BENCH_MSG_DATA, seq, t0,
                                     payload, (uint16_t)run->payload_len);
        if (len == 0) break;
        if (t.vtbl->send(&t, send_buf, len) >= 0) sent++;
        else tx_fail++;
        seq++;

        {
            uint64_t now = bench_now_ns();
            if (bench_log_due(now, &next_log_ns)) {
                uint32_t d_sent = sent - last_sent;
                uint32_t d_fail = tx_fail - last_fail;
                uint32_t sec = (uint32_t)((now - start_ns) / 1000000000ull);
                printf("[BENCH][%s][ONEWAY][TX] t=%us sent/s=%u tx_fail/s=%u\n",
                       BENCH_SIDE, sec, d_sent, d_fail);
                last_sent = sent;
                last_fail = tx_fail;
            }
        }
    }

    printf("[BENCH][%s][ONEWAY][TX] transport=%s rate=%d dur=%d warmup=%d payload=%d timeout_ms=%d sent=%u tx_fail=%u\n",
           BENCH_SIDE, ep->transport ? ep->transport : "unknown",
           run->rate_hz, run->duration_sec, run->warmup_sec, run->payload_len, ep->timeout_ms,
           sent, tx_fail);
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
