#include "bench.h"
#include "bench_log.h"
#include "bench_proto.h"
#include "bench_stats.h"
#include "bench_time.h"
#include "bench_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <limits.h>

#ifdef _WRS_KERNEL
#include <taskLib.h>
#include <sysLib.h>
#include <vxWorks.h>
#include <vxCpuLib.h>
#include <private/spyLibP.h>
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

typedef struct {
    int enabled;
    int use_spy;
    uint64_t last_wall_ns;
    uint64_t last_cpu_ns;
    uint64_t sum_pct_x100;
    uint32_t samples;
    uint32_t min_pct_x100;
    uint32_t max_pct_x100;
#ifdef _WRS_KERNEL
    uint32_t spy_last_inc;
    uint32_t spy_last_idle_sum;
    uint32_t cpu_count;
#endif
} bench_cpu_stats_t;

#ifdef _WRS_KERNEL
typedef struct spyCpuData
    {
    UINT    kernelIncTicks;
    UINT    interruptIncTicks;
    UINT    idleIncTicks;
#ifdef _WRS_CONFIG_SMP
    UINT    taskIncTicks;
    UINT    kernelTotalTicks;
    UINT    interruptTotalTicks;
    UINT    idleTotalTicks;
    UINT    taskTotalTicks;
#endif
    } SPY_CPU_DATA;

extern SPY_CPU_DATA * spyCpuTbl;
extern UINT spyIncTicks;
#endif

static int bench_cpu_get_ns(uint64_t* out_ns)
{
    struct timespec ts;
#if defined(CLOCK_THREAD_CPUTIME_ID)
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        *out_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        return 0;
    }
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) {
        *out_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        return 0;
    }
#endif
    return -1;
}

static uint32_t bench_cpu_spy_idle_sum(uint32_t cpu_count)
{
    uint32_t sum = 0;
#ifdef _WRS_KERNEL
    if (!spyCpuTbl) return 0;
    for (uint32_t i = 0; i < cpu_count; ++i) {
        sum += spyCpuTbl[i].idleIncTicks;
    }
#endif
    return sum;
}

static void bench_cpu_init(bench_cpu_stats_t* cpu, uint64_t start_wall_ns)
{
    if (!cpu) return;
    memset(cpu, 0, sizeof(*cpu));
    cpu->min_pct_x100 = UINT32_MAX;
#ifdef _WRS_KERNEL
    cpu->cpu_count = (uint32_t)vxCpuConfiguredGet();
    if (cpu->cpu_count == 0) cpu->cpu_count = 1;
    if (spyCpuTbl != NULL) {
        if (_func_spyClkStart != NULL) {
            (void)(*_func_spyClkStart)(0, NULL);
        }
        cpu->enabled = 1;
        cpu->use_spy = 1;
        cpu->last_wall_ns = start_wall_ns;
        cpu->spy_last_inc = (uint32_t)spyIncTicks;
        cpu->spy_last_idle_sum = bench_cpu_spy_idle_sum(cpu->cpu_count);
        return;
    }
#endif
    uint64_t cpu_ns = 0;
    if (bench_cpu_get_ns(&cpu_ns) == 0) {
        cpu->enabled = 1;
        cpu->last_wall_ns = start_wall_ns;
        cpu->last_cpu_ns = cpu_ns;
    }
}

static void bench_cpu_sample(bench_cpu_stats_t* cpu, uint64_t now_wall_ns)
{
    if (!cpu || !cpu->enabled) return;
#ifdef _WRS_KERNEL
    if (cpu->use_spy) {
        uint32_t cur_inc = (uint32_t)spyIncTicks;
        uint32_t cur_idle = bench_cpu_spy_idle_sum(cpu->cpu_count);
        uint32_t delta_inc = cur_inc - cpu->spy_last_inc;
        uint32_t delta_idle = cur_idle - cpu->spy_last_idle_sum;
        if (delta_inc == 0) return;
        uint64_t total = (uint64_t)delta_inc * (uint64_t)cpu->cpu_count;
        if (total == 0) return;
        uint64_t busy = (delta_idle > total) ? 0 : (total - delta_idle);
        uint32_t pct_x100 = (uint32_t)((busy * 10000ull) / total);
        if (pct_x100 > 10000u) pct_x100 = 10000u;
        if (pct_x100 < cpu->min_pct_x100) cpu->min_pct_x100 = pct_x100;
        if (pct_x100 > cpu->max_pct_x100) cpu->max_pct_x100 = pct_x100;
        cpu->sum_pct_x100 += pct_x100;
        cpu->samples++;
        cpu->spy_last_inc = cur_inc;
        cpu->spy_last_idle_sum = cur_idle;
        cpu->last_wall_ns = now_wall_ns;
        return;
    }
#endif
    uint64_t cpu_ns = 0;
    if (bench_cpu_get_ns(&cpu_ns) != 0) {
        cpu->enabled = 0;
        return;
    }
    uint64_t dw = now_wall_ns - cpu->last_wall_ns;
    if (dw == 0) return;
    uint64_t dc = cpu_ns - cpu->last_cpu_ns;
    uint32_t pct_x100 = (uint32_t)((dc * 10000ull) / dw);
    if (pct_x100 > 10000u) pct_x100 = 10000u;
    if (pct_x100 < cpu->min_pct_x100) cpu->min_pct_x100 = pct_x100;
    if (pct_x100 > cpu->max_pct_x100) cpu->max_pct_x100 = pct_x100;
    cpu->sum_pct_x100 += pct_x100;
    cpu->samples++;
    cpu->last_wall_ns = now_wall_ns;
    cpu->last_cpu_ns = cpu_ns;
}

static void bench_cpu_finalize(const bench_cpu_stats_t* cpu, bench_result_t* out)
{
    if (!out) return;
    if (!cpu || !cpu->enabled || cpu->samples == 0 || cpu->min_pct_x100 == UINT32_MAX) {
        out->cpu_min_x100 = 0;
        out->cpu_avg_x100 = 0;
        out->cpu_max_x100 = 0;
        return;
    }
    out->cpu_min_x100 = cpu->min_pct_x100;
    out->cpu_max_x100 = cpu->max_pct_x100;
    out->cpu_avg_x100 = (uint32_t)(cpu->sum_pct_x100 / cpu->samples);
}

static void bench_log_cpu_summary(const bench_result_t* res)
{
    if (!res) return;
    if (res->cpu_min_x100 == 0 && res->cpu_avg_x100 == 0 && res->cpu_max_x100 == 0) {
        bench_logf("[BENCH][%s][CPU] min=na avg=na max=na\n", BENCH_SIDE);
        return;
    }
    bench_logf("[BENCH][%s][CPU] min=%u.%02u%% avg=%u.%02u%% max=%u.%02u%%\n",
               BENCH_SIDE,
               res->cpu_min_x100 / 100, res->cpu_min_x100 % 100,
               res->cpu_avg_x100 / 100, res->cpu_avg_x100 % 100,
               res->cpu_max_x100 / 100, res->cpu_max_x100 % 100);
}

static void bench_spin_until(uint64_t target_ns)
{
    while (bench_wall_ns() < target_ns) {
    }
}

static void bench_result_from_stats(const bench_stats_t* stats, const bench_percentiles_t* p,
                                    const bench_tail_counts_t* tail, bench_result_t* out)
{
    if (!out || !stats || !p) return;
    out->samples = (uint64_t)stats->count;
    out->sent = stats->sent;
    out->received = stats->received;
    out->loss = stats->loss;
    out->out_of_order = stats->out_of_order;
    out->tx_fail = stats->tx_fail;
    out->min_ns = p->min_ns;
    out->p50_ns = p->p50_ns;
    out->p90_ns = p->p90_ns;
    out->p99_ns = p->p99_ns;
    out->p999_ns = p->p999_ns;
    out->p9999_ns = p->p9999_ns;
    out->max_ns = p->max_ns;
    if (tail) {
        out->over_50us = tail->over_50us;
        out->over_100us = tail->over_100us;
        out->over_1ms = tail->over_1ms;
    } else {
        out->over_50us = 0;
        out->over_100us = 0;
        out->over_1ms = 0;
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
        uint64_t now = bench_wall_ns();
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

static const char* bench_log_transport(const bench_endpoint_cfg_t* ep)
{
    return (ep && ep->transport) ? ep->transport : "unknown";
}

static const char* bench_log_tag(const bench_report_cfg_t* rep)
{
    return (rep && rep->tag) ? rep->tag : "na";
}

static void bench_log_tail_summary(const char* phase, const bench_endpoint_cfg_t* ep,
                                   const bench_report_cfg_t* rep,
                                   const bench_percentiles_t* p,
                                   const bench_tail_counts_t* tail)
{
    if (!p || !tail) return;
    bench_logf("[BENCH][%s][%s] transport=%s tag=%s p99_9=%" PRIu64 "ns p99_99=%" PRIu64
               "ns over_50us=%" PRIu64 " over_100us=%" PRIu64 " over_1ms=%" PRIu64 "\n",
               BENCH_SIDE, phase, bench_log_transport(ep), bench_log_tag(rep),
               p->p999_ns, p->p9999_ns,
               tail->over_50us, tail->over_100us, tail->over_1ms);
}

static void bench_log_topk(const char* phase, const bench_endpoint_cfg_t* ep,
                           const bench_report_cfg_t* rep, const bench_stats_t* stats)
{
    uint64_t top[10];
    size_t n = bench_stats_topk(stats, top, 10);
    if (n == 0) return;
    bench_logf("[BENCH][%s][%s] top10 transport=%s tag=%s [",
               BENCH_SIDE, phase, bench_log_transport(ep), bench_log_tag(rep));
    for (size_t i = 0; i < n; ++i) {
        bench_logf("%s%" PRIu64 "ns", (i == 0) ? "" : ",", top[i]);
    }
    bench_logf("]\n");
}

static int run_rr_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                         const bench_report_cfg_t* rep, bench_result_t* out)
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

    uint64_t start_ns = bench_wall_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t recv_cnt = 0;
    uint32_t rsp_cnt = 0;
    uint32_t last_recv = 0;
    uint32_t last_rsp = 0;
    uint32_t log_sec = 0;
    bench_cpu_stats_t cpu;
    if (out) memset(out, 0, sizeof(*out));
    bench_cpu_init(&cpu, start_ns);

    while (bench_wall_ns() < end_ns) {
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
            uint64_t now = bench_wall_ns();
            if (bench_log_due(now, &next_log_ns)) {
                bench_cpu_sample(&cpu, now);
                log_sec++;
                uint32_t d_recv = recv_cnt - last_recv;
                uint32_t d_rsp = rsp_cnt - last_rsp;
                bench_logf("[BENCH][%s][RR][SRV] t=%us transport=%s tag=%s recv/s=%u rsp/s=%u total_recv=%u total_rsp=%u\n",
                           BENCH_SIDE, log_sec, bench_log_transport(ep), bench_log_tag(rep),
                           d_recv, d_rsp, recv_cnt, rsp_cnt);
                last_recv = recv_cnt;
                last_rsp = rsp_cnt;
            }
        }
    }

    bench_cpu_finalize(&cpu, out);
    free(buf);
    bench_transport_destroy(&t);
    return 0;
}

static int run_rr_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                         const bench_report_cfg_t* rep, bench_result_t* out, int do_print)
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
    uint64_t start_ns = bench_wall_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_send_ns = start_ns;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t last_sent = 0;
    uint32_t last_recv = 0;
    uint32_t last_loss = 0;
    uint32_t last_ooo = 0;
    uint32_t last_fail = 0;
    uint32_t log_sec = 0;
    bench_cpu_stats_t cpu;
    bench_cpu_init(&cpu, start_ns);

    uint32_t seq = 0;
    while (1) {
        if (bench_wall_ns() >= end_ns) break;
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
        if (bench_wall_ns() >= warmup_end_ns) bench_stats_add_sample(&stats, t1 - t0);
        seq++;

        {
            uint64_t now = bench_wall_ns();
            if (bench_log_due(now, &next_log_ns)) {
                bench_cpu_sample(&cpu, now);
                log_sec++;
                uint32_t d_sent = stats.sent - last_sent;
                uint32_t d_recv = stats.received - last_recv;
                uint32_t d_loss = stats.loss - last_loss;
                uint32_t d_ooo = stats.out_of_order - last_ooo;
                uint32_t d_fail = stats.tx_fail - last_fail;
                bench_logf("[BENCH][%s][RR][CLI] t=%us transport=%s tag=%s sent/s=%u recv/s=%u loss/s=%u ooo/s=%u tx_fail/s=%u\n",
                           BENCH_SIDE, log_sec, bench_log_transport(ep), bench_log_tag(rep),
                           d_sent, d_recv, d_loss, d_ooo, d_fail);
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
    bench_tail_counts_t tail = {0};
    bench_stats_tail_counts(&stats, &tail);
    bench_result_from_stats(&stats, &p, &tail, out);
    bench_cpu_finalize(&cpu, out);
    if (do_print) {
        bench_logf("[BENCH][%s][RR] transport=%s tag=%s rate=%d dur=%d warmup=%d payload=%d timeout_ms=%d\n",
                   BENCH_SIDE, ep->transport ? ep->transport : "unknown",
                   rep && rep->tag ? rep->tag : "rtp",
                   run->rate_hz, run->duration_sec, run->warmup_sec, run->payload_len, ep->timeout_ms);
        bench_logf("[BENCH][%s][RR] samples=%" PRIu64 " sent=%u recv=%u loss=%u ooo=%u tx_fail=%u\n",
                   BENCH_SIDE, (uint64_t)stats.count, stats.sent, stats.received, stats.loss, stats.out_of_order, stats.tx_fail);
        bench_logf("[BENCH][%s][RR] min=%" PRIu64 "ns p50=%" PRIu64 "ns p90=%" PRIu64 "ns p99=%" PRIu64 "ns max=%" PRIu64 "ns\n",
                   BENCH_SIDE, p.min_ns, p.p50_ns, p.p90_ns, p.p99_ns, p.max_ns);
        bench_log_tail_summary("RR", ep, rep, &p, &tail);
        bench_log_topk("RR", ep, rep, &stats);
        bench_log_cpu_summary(out);
    }

    free(samples);
    free(send_buf);
    free(recv_buf);
    free(payload);
    bench_transport_destroy(&t);
    return 0;
}

static int run_oneway_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                             const bench_report_cfg_t* rep, bench_result_t* out, int do_print)
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
    uint64_t start_ns = bench_wall_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t prev_recv_ns = 0;
    uint32_t expect_seq = 0;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t last_recv = 0;
    uint32_t last_loss = 0;
    uint32_t last_ooo = 0;
    uint32_t log_sec = 0;
    bench_cpu_stats_t cpu;
    bench_cpu_init(&cpu, start_ns);

    while (bench_wall_ns() < end_ns) {
        int n = t.vtbl->recv(&t, recv_buf, msg_cap, ep->timeout_ms);
        uint64_t now_wall = bench_wall_ns();
        uint64_t now_ns = bench_now_ns();
        if (n > 0) {
            bench_msg_hdr_t hdr;
            if (bench_parse_msg(recv_buf, (size_t)n, &hdr, NULL) == 0 && hdr.type == BENCH_MSG_DATA) {
                stats.received++;
                if (hdr.seq != expect_seq) stats.out_of_order++;
                if (hdr.seq > expect_seq) stats.loss += (hdr.seq - expect_seq);
                expect_seq = hdr.seq + 1;

                if (prev_recv_ns != 0 && now_wall >= warmup_end_ns) {
                    int64_t jitter = (int64_t)(now_ns - prev_recv_ns) - (int64_t)period_ns;
                    if (jitter < 0) jitter = -jitter;
                    bench_stats_add_sample(&stats, (uint64_t)jitter);
                }
                prev_recv_ns = now_ns;
            }
        }

        if (bench_log_due(now_wall, &next_log_ns)) {
            bench_cpu_sample(&cpu, now_wall);
            log_sec++;
            uint32_t d_recv = stats.received - last_recv;
            uint32_t d_loss = stats.loss - last_loss;
            uint32_t d_ooo = stats.out_of_order - last_ooo;
            bench_logf("[BENCH][%s][ONEWAY][RX] t=%us transport=%s tag=%s recv/s=%u loss/s=%u ooo/s=%u\n",
                       BENCH_SIDE, log_sec, bench_log_transport(ep), bench_log_tag(rep),
                       d_recv, d_loss, d_ooo);
            last_recv = stats.received;
            last_loss = stats.loss;
            last_ooo = stats.out_of_order;
        }
    }

    bench_stats_finalize(&stats);
    bench_percentiles_t p = bench_stats_percentiles(&stats);
    bench_tail_counts_t tail = {0};
    bench_stats_tail_counts(&stats, &tail);
    bench_result_from_stats(&stats, &p, &tail, out);
    bench_cpu_finalize(&cpu, out);
    if (do_print) {
        bench_logf("[BENCH][%s][ONEWAY][RX] transport=%s tag=%s rate=%d dur=%d warmup=%d payload=%d timeout_ms=%d\n",
                   BENCH_SIDE, ep->transport ? ep->transport : "unknown",
                   rep && rep->tag ? rep->tag : "rtp",
                   run->rate_hz, run->duration_sec, run->warmup_sec, run->payload_len, ep->timeout_ms);
        bench_logf("[BENCH][%s][ONEWAY][RX] samples=%" PRIu64 " recv=%u loss=%u ooo=%u\n",
                   BENCH_SIDE, (uint64_t)stats.count, stats.received, stats.loss, stats.out_of_order);
        bench_logf("[BENCH][%s][ONEWAY][RX] jitter_abs min=%" PRIu64 "ns p50=%" PRIu64 "ns p90=%" PRIu64 "ns p99=%" PRIu64 "ns max=%" PRIu64 "ns\n",
                   BENCH_SIDE, p.min_ns, p.p50_ns, p.p90_ns, p.p99_ns, p.max_ns);
        bench_log_tail_summary("ONEWAY][RX", ep, rep, &p, &tail);
        bench_log_topk("ONEWAY][RX", ep, rep, &stats);
        bench_log_cpu_summary(out);
    }

    free(samples);
    free(recv_buf);
    bench_transport_destroy(&t);
    return 0;
}

static int run_oneway_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                             const bench_report_cfg_t* rep, bench_result_t* out, int do_print)
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
    uint64_t start_ns = bench_wall_ns();
    uint64_t warmup_end_ns = start_ns + (uint64_t)run->warmup_sec * 1000000000ull;
    uint64_t end_ns = warmup_end_ns + (uint64_t)run->duration_sec * 1000000000ull;
    uint64_t next_send_ns = start_ns;
    uint32_t seq = 0;
    uint32_t sent = 0;
    uint32_t tx_fail = 0;
    uint64_t next_log_ns = start_ns + BENCH_LOG_INTERVAL_NS;
    uint32_t last_sent = 0;
    uint32_t last_fail = 0;
    uint32_t log_sec = 0;
    bench_cpu_stats_t cpu;
    bench_cpu_init(&cpu, start_ns);

    while (1) {
        if (bench_wall_ns() >= end_ns) break;
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
            uint64_t now = bench_wall_ns();
            if (bench_log_due(now, &next_log_ns)) {
                bench_cpu_sample(&cpu, now);
                log_sec++;
                uint32_t d_sent = sent - last_sent;
                uint32_t d_fail = tx_fail - last_fail;
                bench_logf("[BENCH][%s][ONEWAY][TX] t=%us transport=%s tag=%s sent/s=%u tx_fail/s=%u\n",
                           BENCH_SIDE, log_sec, bench_log_transport(ep), bench_log_tag(rep),
                           d_sent, d_fail);
                last_sent = sent;
                last_fail = tx_fail;
            }
        }
    }

    if (out) {
        out->samples = 0;
        out->sent = sent;
        out->received = 0;
        out->loss = 0;
        out->out_of_order = 0;
        out->tx_fail = tx_fail;
        out->min_ns = 0;
        out->p50_ns = 0;
        out->p90_ns = 0;
        out->p99_ns = 0;
        out->p999_ns = 0;
        out->p9999_ns = 0;
        out->max_ns = 0;
        out->over_50us = 0;
        out->over_100us = 0;
        out->over_1ms = 0;
        bench_cpu_finalize(&cpu, out);
    }
    if (do_print) {
        bench_logf("[BENCH][%s][ONEWAY][TX] transport=%s rate=%d dur=%d warmup=%d payload=%d timeout_ms=%d sent=%u tx_fail=%u\n",
                   BENCH_SIDE, ep->transport ? ep->transport : "unknown",
                   run->rate_hz, run->duration_sec, run->warmup_sec, run->payload_len, ep->timeout_ms,
                   sent, tx_fail);
        bench_log_cpu_summary(out);
    }
    free(send_buf);
    free(payload);
    bench_transport_destroy(&t);
    return 0;
}

int bench_run_server(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run)
{
    if (!ep || !run) return -1;
    return bench_run_server_result(ep, run, NULL, NULL);
}

int bench_run_client(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run, const bench_report_cfg_t* rep)
{
    if (!ep || !run) return -1;
    return bench_run_client_result(ep, run, rep, NULL);
}

int bench_run_server_result(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                            const bench_report_cfg_t* rep, bench_result_t* out)
{
    if (!ep || !run) return -1;
    if (run->mode == BENCH_MODE_RR) return run_rr_server(ep, run, rep, out);
    return run_oneway_server(ep, run, rep, out, 1);
}

int bench_run_client_result(const bench_endpoint_cfg_t* ep, const bench_run_cfg_t* run,
                            const bench_report_cfg_t* rep, bench_result_t* out)
{
    if (!ep || !run) return -1;
    if (run->mode == BENCH_MODE_RR) return run_rr_client(ep, run, rep, out, 1);
    return run_oneway_client(ep, run, rep, out, 1);
}
