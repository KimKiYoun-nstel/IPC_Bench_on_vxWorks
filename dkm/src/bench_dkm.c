#include "bench.h"
#include "bench_csv.h"
#include "bench_log.h"
#include "bench_tag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <semLib.h>
#include <rtpLib.h>
#include <rtpLibCommon.h>
#include <sysLib.h>
#include <sys/stat.h>

#define BENCH_AUTO_PORT 41000
#define BENCH_AUTO_SERVER_DUR 30
#define BENCH_AUTO_CLIENT_DUR 30
#define BENCH_AUTO_WARMUP 1
#define BENCH_AUTO_TIMEOUT_MS 1000
#define BENCH_AUTO_PRIORITY 100
#define BENCH_AUTO_STACK 0x10000
#define BENCH_AUTO_RTP_OPTS (RTP_LOADED_WAIT)
#define BENCH_AUTO_TASK_OPTS (VX_FP_TASK)
#define BENCH_AUTO_VXE_DEFAULT "/tffs0/IPC_Bench/bench_rtp.vxe"
#define BENCH_AUTO_CSV_DEFAULT "/tffs0/IPC_Bench/bench_results.csv"
#define BENCH_AUTO_DIR_DEFAULT "/tffs0/IPC_Bench"
#define BENCH_AUTO_LOG_DEFAULT "/tffs0/IPC_Bench/bench_auto.log"
#define BENCH_AUTO_WAIT_MARGIN 10
#define BENCH_RUN_DEFAULT_UDP_IP "127.0.0.1"
#define BENCH_RUN_DEFAULT_DUR 30

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

/* Convenience wrappers: RR server does not need rate/payload */
int benchServerStartRR(const char* transport, const char* bind_or_name, int port, const char* name,
                       int duration_sec)
{
    return benchServerStart(transport, bind_or_name, port, name, BENCH_MODE_RR, 0, duration_sec, 0);
}

int benchServerStartOneWay(const char* transport, const char* bind_or_name, int port, const char* name,
                           int rate_hz, int duration_sec, int payload_len)
{
    return benchServerStart(transport, bind_or_name, port, name, BENCH_MODE_ONEWAY,
                            rate_hz, duration_sec, payload_len);
}

typedef struct {
    bench_endpoint_cfg_t ep;
    bench_run_cfg_t run;
    bench_report_cfg_t rep;
    bench_result_t result;
    const char* csv_path;
    SEM_ID done_sem;
    int rc;
} bench_server_task_ctx_t;

typedef struct {
    bench_endpoint_cfg_t ep;
    bench_run_cfg_t run;
    bench_report_cfg_t rep;
    bench_result_t result;
    const char* csv_path;
    SEM_ID done_sem;
    int rc;
} bench_client_task_ctx_t;

static void bench_make_tag(char* out, size_t cap, const char* transport,
                           const char* mode, const char* role, int rate,
                           int payload, const char* ip)
{
    if (!out || cap == 0) return;
    if (ip && ip[0]) {
        snprintf(out, cap, "%s_%s_%s_r%d_p%d_%s", transport, mode, role, rate, payload, ip);
    } else {
        snprintf(out, cap, "%s_%s_%s_r%d_p%d", transport, mode, role, rate, payload);
    }
}

static void bench_delay_ms(int ms)
{
    int ticks = (int)((ms * sysClkRateGet() + 999) / 1000);
    if (ticks <= 0) ticks = 1;
    taskDelay(ticks);
}

static FILE* bench_auto_log_fp = NULL;

static void bench_auto_log_open(const char* path)
{
    if (!path || !path[0]) return;
    bench_auto_log_fp = fopen(path, "a");
    if (!bench_auto_log_fp) {
        printf("[BENCH][AUTO] log open failed path=%s\n", path);
        return;
    }
    setvbuf(bench_auto_log_fp, NULL, _IOLBF, 0);
}

static void bench_auto_log_close(void)
{
    if (bench_auto_log_fp) {
        fclose(bench_auto_log_fp);
        bench_auto_log_fp = NULL;
    }
}

static void bench_auto_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (!bench_auto_log_fp) return;
    va_start(ap, fmt);
    vfprintf(bench_auto_log_fp, fmt, ap);
    va_end(ap);
    fflush(bench_auto_log_fp);
}

static _Vx_ticks_t bench_wait_ticks(int seconds)
{
    int rate = (int)sysClkRateGet();
    if (rate <= 0) rate = 1000;
    if (seconds < 1) seconds = 1;
    return (_Vx_ticks_t)((unsigned)rate * (unsigned)seconds);
}

static const char* bench_default_name(const char* transport)
{
    if (!transport) return "bench";
    if (strcmp(transport, "msgq") == 0) return "bench_msgq";
    if (strcmp(transport, "shmsem") == 0) return "bench_shm";
    return "bench";
}

static RTP_ID bench_rtp_spawn(const char* vxe_path, int is_server, const char* transport,
                              const char* bind_or_dst, int port, int mode,
                              int rate, int dur, int payload, const char* tag,
                              const char* csv_path, const char* log_path)
{
    char port_s[16];
    char rate_s[16];
    char dur_s[16];
    char payload_s[16];
    const char* mode_s = (mode == BENCH_MODE_RR) ? "rr" : "oneway";
    const char* role_s = is_server ? "-s" : "-c";
    const char* addr_flag = is_server ? "--bind" : "--dst";

    snprintf(port_s, sizeof(port_s), "%d", port);
    snprintf(rate_s, sizeof(rate_s), "%d", rate);
    snprintf(dur_s, sizeof(dur_s), "%d", dur);
    snprintf(payload_s, sizeof(payload_s), "%d", payload);

    const char* argv[32];
    int idx = 0;
    argv[idx++] = vxe_path;
    argv[idx++] = role_s;
    argv[idx++] = "--transport"; argv[idx++] = transport;
    if (bind_or_dst && bind_or_dst[0]) {
        argv[idx++] = addr_flag; argv[idx++] = bind_or_dst;
    }
    argv[idx++] = "--port"; argv[idx++] = port_s;
    argv[idx++] = "--mode"; argv[idx++] = mode_s;
    argv[idx++] = "--rate"; argv[idx++] = rate_s;
    argv[idx++] = "--dur"; argv[idx++] = dur_s;
    argv[idx++] = "--payload"; argv[idx++] = payload_s;
    if (tag && tag[0]) {
        argv[idx++] = "--tag"; argv[idx++] = tag;
    }
    if (csv_path && csv_path[0]) {
        argv[idx++] = "--csv"; argv[idx++] = csv_path;
    }
    if (log_path && log_path[0]) {
        argv[idx++] = "--log"; argv[idx++] = log_path;
    }
    argv[idx] = NULL;

    return rtpSpawn(vxe_path, argv, NULL,
                    BENCH_AUTO_PRIORITY, BENCH_AUTO_STACK,
                    BENCH_AUTO_RTP_OPTS, BENCH_AUTO_TASK_OPTS);
}

static int bench_server_task(_Vx_usr_arg_t arg)
{
    bench_server_task_ctx_t* ctx = (bench_server_task_ctx_t*)arg;
    if (!ctx) return -1;
    ctx->rc = bench_run_server_result(&ctx->ep, &ctx->run, &ctx->rep, &ctx->result);
    if (ctx->csv_path) {
        (void)bench_csv_append(ctx->csv_path, &ctx->ep, &ctx->run, &ctx->rep, &ctx->result);
    }
    if (ctx->done_sem) semGive(ctx->done_sem);
    return 0;
}

static int bench_server_task_manual(_Vx_usr_arg_t arg)
{
    bench_server_task_ctx_t* ctx = (bench_server_task_ctx_t*)arg;
    if (!ctx) return -1;
    ctx->rc = bench_run_server_result(&ctx->ep, &ctx->run, &ctx->rep, &ctx->result);
    if (ctx->done_sem) semGive(ctx->done_sem);
    return 0;
}

static int bench_client_task(_Vx_usr_arg_t arg)
{
    bench_client_task_ctx_t* ctx = (bench_client_task_ctx_t*)arg;
    if (!ctx) return -1;
    ctx->rc = bench_run_client_result(&ctx->ep, &ctx->run, &ctx->rep, &ctx->result);
    if (ctx->csv_path) {
        (void)bench_csv_append(ctx->csv_path, &ctx->ep, &ctx->run, &ctx->rep, &ctx->result);
    }
    if (ctx->done_sem) semGive(ctx->done_sem);
    return 0;
}

static int bench_run_pair(const char* transport, const char* name_or_ip,
                          int mode, int rate, int payload, int dkm_is_client,
                          const char* vxe_path, const char* csv_path)
{
    const char* mode_s = (mode == BENCH_MODE_RR) ? "rr" : "oneway";
    const char* role_s = dkm_is_client ? "cli" : "srv";
    char tag[96];

    bench_make_tag(tag, sizeof(tag), transport, mode_s, role_s, rate, payload,
                   (strcmp(transport, "udp") == 0) ? name_or_ip : NULL);

    {
        char log_path[256];
        if (bench_tag_build_path(tag, ".log", log_path, sizeof(log_path)) == 0) {
            bench_log_set_path(log_path);
        }
    }

    bench_auto_log("[BENCH][AUTO] %s %s dkm_%s rate=%d payload=%d target=%s\n",
                   transport, mode_s, role_s, rate, payload, name_or_ip ? name_or_ip : "-");

    if (dkm_is_client) {
        const char* srv_bind = (strcmp(transport, "udp") == 0) ? "0.0.0.0" : name_or_ip;
        RTP_ID rtp_id = bench_rtp_spawn(vxe_path, 1, transport, srv_bind,
                                        BENCH_AUTO_PORT, mode, rate,
                                        BENCH_AUTO_SERVER_DUR, payload, tag,
                                        csv_path, BENCH_AUTO_LOG_DEFAULT);
        if (rtp_id == RTP_ID_ERROR) {
            bench_auto_log("[BENCH][AUTO] rtpSpawn server failed transport=%s\n", transport);
            return -1;
        }
        bench_delay_ms(1000);

        bench_client_task_ctx_t* cctx = (bench_client_task_ctx_t*)calloc(1, sizeof(*cctx));
        if (!cctx) return -1;
        cctx->done_sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
        if (cctx->done_sem == SEM_ID_NULL) {
            free(cctx);
            return -1;
        }
        cctx->csv_path = csv_path;

        cctx->ep.transport = transport;
        cctx->ep.role = BENCH_ROLE_CLIENT;
        cctx->ep.bind_or_dst = name_or_ip;
        cctx->ep.name = (strcmp(transport, "udp") == 0) ? NULL : name_or_ip;
        cctx->ep.port = BENCH_AUTO_PORT;
        cctx->ep.timeout_ms = BENCH_AUTO_TIMEOUT_MS;

        cctx->run.mode = (bench_mode_t)mode;
        cctx->run.rate_hz = rate;
        cctx->run.duration_sec = BENCH_AUTO_CLIENT_DUR;
        cctx->run.payload_len = payload;
        cctx->run.warmup_sec = BENCH_AUTO_WARMUP;

        cctx->rep.tag = tag;
        cctx->rep.csv_path = csv_path;

        TASK_ID ctid = taskSpawn("tBenchCli", BENCH_AUTO_PRIORITY, 0, BENCH_AUTO_STACK,
                                 (FUNCPTR)bench_client_task,
                                 (_Vx_usr_arg_t)cctx, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (ctid == TASK_ID_ERROR) {
            semDelete(cctx->done_sem);
            free(cctx);
            return -1;
        }
        {
            int wait_sec = BENCH_AUTO_CLIENT_DUR + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
            if (semTake(cctx->done_sem, bench_wait_ticks(wait_sec)) != OK) {
                bench_auto_log("[BENCH][AUTO] client task timeout, delete task\n");
                taskDelete(ctid);
                cctx->rc = -1;
            }
        }
        int cli_rc = cctx->rc;
        bench_result_t cli_result = cctx->result;
        bench_auto_log("[BENCH][AUTO] done rc=%d\n", cli_rc);
        semDelete(cctx->done_sem);
        free(cctx);
        int wait_sec = BENCH_AUTO_SERVER_DUR + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
        STATUS st = rtpWait(rtp_id, bench_wait_ticks(wait_sec), NULL, NULL);
        if (st != OK) {
            bench_auto_log("[BENCH][AUTO] rtpWait timeout, force delete\n");
            (void)rtpDelete(rtp_id, RTP_DEL_FORCE, 0);
        }
        {
            int ok = 0;
            if (cli_rc == 0 && st == OK) {
                if (mode == BENCH_MODE_RR) ok = (cli_result.received > 0);
                else ok = (cli_result.sent > 0);
            }
            bench_auto_log("[BENCH][AUTO] result transport=%s mode=%s role=dkm_cli ok=%d "
                           "sent=%u recv=%u loss=%u tx_fail=%u\n",
                           transport, mode_s, ok,
                           cli_result.sent, cli_result.received,
                           cli_result.loss, cli_result.tx_fail);
        }
        return 0;
    }

    bench_server_task_ctx_t* ctx = (bench_server_task_ctx_t*)calloc(1, sizeof(*ctx));
    if (!ctx) return -1;
    ctx->done_sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
    if (ctx->done_sem == SEM_ID_NULL) {
        free(ctx);
        return -1;
    }
    ctx->csv_path = csv_path;

    ctx->ep.transport = transport;
    ctx->ep.role = BENCH_ROLE_SERVER;
    ctx->ep.bind_or_dst = (strcmp(transport, "udp") == 0) ? "0.0.0.0" : name_or_ip;
    ctx->ep.name = (strcmp(transport, "udp") == 0) ? NULL : name_or_ip;
    ctx->ep.port = BENCH_AUTO_PORT;
    ctx->ep.timeout_ms = BENCH_AUTO_TIMEOUT_MS;

    ctx->run.mode = (bench_mode_t)mode;
    ctx->run.rate_hz = rate;
    ctx->run.duration_sec = BENCH_AUTO_SERVER_DUR;
    ctx->run.payload_len = payload;
    ctx->run.warmup_sec = BENCH_AUTO_WARMUP;

    ctx->rep.tag = tag;

    TASK_ID tid = taskSpawn("tBenchSrv", BENCH_AUTO_PRIORITY, 0, BENCH_AUTO_STACK,
                            (FUNCPTR)bench_server_task,
                            (_Vx_usr_arg_t)ctx, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (tid == TASK_ID_ERROR) {
        bench_auto_log("[BENCH][AUTO] taskSpawn server failed transport=%s\n", transport);
        semDelete(ctx->done_sem);
        free(ctx);
        return -1;
    }

    bench_delay_ms(1000);

    RTP_ID rtp_id = bench_rtp_spawn(vxe_path, 0, transport, name_or_ip,
                                    BENCH_AUTO_PORT, mode, rate,
                                    BENCH_AUTO_CLIENT_DUR, payload, tag,
                                    csv_path, BENCH_AUTO_LOG_DEFAULT);
    if (rtp_id == RTP_ID_ERROR) {
        bench_auto_log("[BENCH][AUTO] rtpSpawn client failed transport=%s\n", transport);
        taskDelete(tid);
        semDelete(ctx->done_sem);
        free(ctx);
        return -1;
    }

    STATUS rtp_st = OK;
    {
        int wait_sec = BENCH_AUTO_CLIENT_DUR + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
        rtp_st = rtpWait(rtp_id, bench_wait_ticks(wait_sec), NULL, NULL);
        if (rtp_st != OK) {
            bench_auto_log("[BENCH][AUTO] rtpWait timeout, force delete\n");
            (void)rtpDelete(rtp_id, RTP_DEL_FORCE, 0);
        }
    }
    {
        int wait_sec = BENCH_AUTO_SERVER_DUR + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
        if (semTake(ctx->done_sem, bench_wait_ticks(wait_sec)) != OK) {
            bench_auto_log("[BENCH][AUTO] server task timeout, delete task\n");
            taskDelete(tid);
        }
    }
    {
        int ok = 0;
        if (ctx->rc == 0 && rtp_st == OK) {
            if (mode == BENCH_MODE_RR) ok = 1;
            else ok = (ctx->result.received > 0);
        }
        bench_auto_log("[BENCH][AUTO] result transport=%s mode=%s role=dkm_srv ok=%d "
                       "recv=%u loss=%u ooo=%u\n",
                       transport, mode_s, ok,
                       ctx->result.received, ctx->result.loss, ctx->result.out_of_order);
    }
    semDelete(ctx->done_sem);
    free(ctx);
    return 0;
}

int benchRunOnceOpt(const char* transport, int mode, int rate_hz, int payload_len,
                    const char* udp_ip, int client_dur);
int benchRunOnceSrvOpt(const char* transport, int mode, int rate_hz, int payload_len,
                       const char* udp_ip, int client_dur);
int benchRunOnceStr(const char* transport, const char* mode, int rate_hz, int payload_len);
int benchRunOnceStrOpt(const char* transport, const char* mode, int rate_hz, int payload_len,
                       const char* udp_ip, int client_dur);
int benchRunOnceRR_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip);
int benchRunOnceRR_Srv_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip);
int benchRunOnceOneWay_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip);

int benchRunOnce(const char* transport, int mode, int rate_hz, int payload_len)
{
    return benchRunOnceOpt(transport, mode, rate_hz, payload_len, NULL, 0);
}

int benchRunOnceRR(const char* transport, int rate_hz, int payload_len)
{
    return benchRunOnceOpt(transport, BENCH_MODE_RR, rate_hz, payload_len, NULL, 0);
}

int benchRunOnceRR_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip)
{
    return benchRunOnceOpt(transport, BENCH_MODE_RR, rate_hz, payload_len, udp_ip, 0);
}

int benchRunOnceRR_Srv(const char* transport, int rate_hz, int payload_len)
{
    return benchRunOnceSrvOpt(transport, BENCH_MODE_RR, rate_hz, payload_len, NULL, 0);
}

int benchRunOnceRR_Srv_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip)
{
    return benchRunOnceSrvOpt(transport, BENCH_MODE_RR, rate_hz, payload_len, udp_ip, 0);
}

int benchRunOnceOneWay_Srv(const char* transport, int rate_hz, int payload_len)
{
    return benchRunOnceSrvOpt(transport, BENCH_MODE_ONEWAY, rate_hz, payload_len, NULL, 0);
}

int benchRunOnceOneWay_Srv_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip)
{
    return benchRunOnceSrvOpt(transport, BENCH_MODE_ONEWAY, rate_hz, payload_len, udp_ip, 0);
}

int benchRunOnceOneWay(const char* transport, int rate_hz, int payload_len)
{
    return benchRunOnceOpt(transport, BENCH_MODE_ONEWAY, rate_hz, payload_len, NULL, 0);
}

int benchRunOnceOneWay_IP(const char* transport, int rate_hz, int payload_len, const char* udp_ip)
{
    return benchRunOnceOpt(transport, BENCH_MODE_ONEWAY, rate_hz, payload_len, udp_ip, 0);
}

static int bench_parse_mode(const char* mode, int* out)
{
    if (!mode || !out) return -1;
    if (strcmp(mode, "rr") == 0) {
        *out = BENCH_MODE_RR;
        return 0;
    }
    if (strcmp(mode, "oneway") == 0 || strcmp(mode, "ow") == 0) {
        *out = BENCH_MODE_ONEWAY;
        return 0;
    }
    return -1;
}

int benchRunOnceStr(const char* transport, const char* mode, int rate_hz, int payload_len)
{
    return benchRunOnceStrOpt(transport, mode, rate_hz, payload_len, NULL, 0);
}

int benchRunOnceStrOpt(const char* transport, const char* mode, int rate_hz, int payload_len,
                       const char* udp_ip, int client_dur)
{
    int m = 0;
    if (bench_parse_mode(mode, &m) != 0) return -1;
    return benchRunOnceOpt(transport, m, rate_hz, payload_len, udp_ip, client_dur);
}

int benchRunOnceOpt(const char* transport, int mode, int rate_hz, int payload_len,
                    const char* udp_ip, int client_dur)
{
    if (!transport || !transport[0]) return -1;
    if (rate_hz <= 0 || payload_len < 0) return -1;

    const char* target = NULL;
    const char* name = NULL;
    if (strcmp(transport, "udp") == 0) {
        target = (udp_ip && udp_ip[0]) ? udp_ip : BENCH_RUN_DEFAULT_UDP_IP;
    } else {
        name = bench_default_name(transport);
        target = name;
    }

    if (client_dur <= 0) client_dur = BENCH_RUN_DEFAULT_DUR;
    int server_dur = client_dur + 1;

    char tag[96];
    const char* mode_s = (mode == BENCH_MODE_RR) ? "rr" : "oneway";
    bench_make_tag(tag, sizeof(tag), transport, mode_s, "cli", rate_hz, payload_len,
                   (strcmp(transport, "udp") == 0) ? target : NULL);

    {
        char log_path[256];
        if (bench_tag_build_path(tag, ".log", log_path, sizeof(log_path)) == 0) {
            bench_log_set_path(log_path);
        }
    }

    bench_logf("[BENCH][MANUAL] transport=%s mode=%s rate=%d payload=%d dur=%d target=%s\n",
               transport, mode_s, rate_hz, payload_len, client_dur, target);

    RTP_ID rtp_id = bench_rtp_spawn(BENCH_AUTO_VXE_DEFAULT, 1, transport,
                                    (strcmp(transport, "udp") == 0) ? "0.0.0.0" : target,
                                    BENCH_AUTO_PORT, mode, rate_hz, server_dur, payload_len,
                                    tag, NULL, NULL);
    if (rtp_id == RTP_ID_ERROR) {
        bench_logf("[BENCH][MANUAL] rtpSpawn server failed\n");
        return -1;
    }
    bench_delay_ms(1000);

    bench_endpoint_cfg_t ep = {0};
    ep.transport = transport;
    ep.role = BENCH_ROLE_CLIENT;
    ep.bind_or_dst = target;
    ep.name = (strcmp(transport, "udp") == 0) ? NULL : target;
    ep.port = BENCH_AUTO_PORT;
    ep.timeout_ms = BENCH_AUTO_TIMEOUT_MS;

    bench_run_cfg_t run = {0};
    run.mode = (bench_mode_t)mode;
    run.rate_hz = rate_hz;
    run.duration_sec = client_dur;
    run.payload_len = payload_len;
    run.warmup_sec = BENCH_AUTO_WARMUP;

    bench_report_cfg_t rep = {0};
    rep.tag = tag;

    bench_result_t result = {0};
    int rc = bench_run_client_result(&ep, &run, &rep, &result);
    (void)bench_csv_append(NULL, &ep, &run, &rep, &result);

    int wait_sec = server_dur + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
    STATUS st = rtpWait(rtp_id, bench_wait_ticks(wait_sec), NULL, NULL);
    if (st != OK) {
        bench_logf("[BENCH][MANUAL] rtpWait timeout, force delete\n");
        (void)rtpDelete(rtp_id, RTP_DEL_FORCE, 0);
    }
    int ok = 0;
    if (rc == 0 && st == OK) {
        if (mode == BENCH_MODE_RR) ok = (result.received > 0);
        else ok = (result.sent > 0);
    }
    bench_logf("[BENCH][MANUAL] result ok=%d sent=%u recv=%u loss=%u tx_fail=%u\n",
               ok, result.sent, result.received, result.loss, result.tx_fail);
    return ok ? 0 : -1;
}

int benchRunOnceSrvOpt(const char* transport, int mode, int rate_hz, int payload_len,
                       const char* udp_ip, int client_dur)
{
    if (!transport || !transport[0]) return -1;
    if (rate_hz <= 0 || payload_len < 0) return -1;

    const char* target = NULL;
    const char* name = NULL;
    if (strcmp(transport, "udp") == 0) {
        target = (udp_ip && udp_ip[0]) ? udp_ip : BENCH_RUN_DEFAULT_UDP_IP;
    } else {
        name = bench_default_name(transport);
        target = name;
    }

    if (client_dur <= 0) client_dur = BENCH_RUN_DEFAULT_DUR;
    int server_dur = client_dur + 1;

    char tag[96];
    const char* mode_s = (mode == BENCH_MODE_RR) ? "rr" : "oneway";
    bench_make_tag(tag, sizeof(tag), transport, mode_s, "srv", rate_hz, payload_len,
                   (strcmp(transport, "udp") == 0) ? target : NULL);

    {
        char log_path[256];
        if (bench_tag_build_path(tag, ".log", log_path, sizeof(log_path)) == 0) {
            bench_log_set_path(log_path);
        }
    }

    bench_logf("[BENCH][MANUAL] transport=%s mode=%s rate=%d payload=%d dur=%d target=%s role=dkm_srv\n",
               transport, mode_s, rate_hz, payload_len, client_dur, target);

    bench_server_task_ctx_t* sctx = (bench_server_task_ctx_t*)calloc(1, sizeof(*sctx));
    if (!sctx) return -1;
    sctx->done_sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
    if (sctx->done_sem == SEM_ID_NULL) {
        free(sctx);
        return -1;
    }

    sctx->ep.transport = transport;
    sctx->ep.role = BENCH_ROLE_SERVER;
    sctx->ep.bind_or_dst = (strcmp(transport, "udp") == 0) ? "0.0.0.0" : target;
    sctx->ep.name = (strcmp(transport, "udp") == 0) ? NULL : target;
    sctx->ep.port = BENCH_AUTO_PORT;
    sctx->ep.timeout_ms = BENCH_AUTO_TIMEOUT_MS;

    sctx->run.mode = (bench_mode_t)mode;
    sctx->run.rate_hz = rate_hz;
    sctx->run.duration_sec = server_dur;
    sctx->run.payload_len = payload_len;
    sctx->run.warmup_sec = BENCH_AUTO_WARMUP;

    sctx->rep.tag = tag;

    TASK_ID tid = taskSpawn("tBenchSrvMan", BENCH_AUTO_PRIORITY, 0, BENCH_AUTO_STACK,
                            (FUNCPTR)bench_server_task_manual,
                            (_Vx_usr_arg_t)sctx, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (tid == TASK_ID_ERROR) {
        semDelete(sctx->done_sem);
        free(sctx);
        return -1;
    }

    bench_delay_ms(1000);

    RTP_ID rtp_id = bench_rtp_spawn(BENCH_AUTO_VXE_DEFAULT, 0, transport,
                                    target, BENCH_AUTO_PORT, mode,
                                    rate_hz, client_dur, payload_len,
                                    tag, NULL, NULL);
    if (rtp_id == RTP_ID_ERROR) {
        bench_logf("[BENCH][MANUAL] rtpSpawn client failed\n");
        taskDelete(tid);
        semDelete(sctx->done_sem);
        free(sctx);
        return -1;
    }

    STATUS rtp_st = OK;
    {
        int wait_sec = client_dur + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
        rtp_st = rtpWait(rtp_id, bench_wait_ticks(wait_sec), NULL, NULL);
        if (rtp_st != OK) {
            bench_logf("[BENCH][MANUAL] rtpWait timeout, force delete\n");
            (void)rtpDelete(rtp_id, RTP_DEL_FORCE, 0);
        }
    }
    {
        int wait_sec = server_dur + BENCH_AUTO_WARMUP + BENCH_AUTO_WAIT_MARGIN;
        if (semTake(sctx->done_sem, bench_wait_ticks(wait_sec)) != OK) {
            bench_logf("[BENCH][MANUAL] server task timeout, delete task\n");
            taskDelete(tid);
            sctx->rc = -1;
        }
    }

    (void)bench_csv_append(NULL, &sctx->ep, &sctx->run, &sctx->rep, &sctx->result);

    {
        int ok = (sctx->rc == 0 && rtp_st == OK);
        bench_logf("[BENCH][MANUAL] result ok=%d srv_rc=%d rtp=%s\n",
                   ok, sctx->rc, (rtp_st == OK) ? "ok" : "timeout");
        semDelete(sctx->done_sem);
        free(sctx);
        return ok ? 0 : -1;
    }
}

int benchRunOnceRR_Preset(int repeat)
{
    if (repeat <= 0) return -1;
    for (int i = 0; i < repeat; ++i) {
        benchRunOnceRR_IP("udp", 200, 256, "127.0.0.1");
        benchRunOnceRR("shmsem", 200, 256);
        benchRunOnceRR("msgq", 200, 256);

        benchRunOnceRR_IP("udp", 1000, 256, "127.0.0.1");
        benchRunOnceRR("shmsem", 1000, 256);
        benchRunOnceRR("msgq", 1000, 256);

        benchRunOnceRR_IP("udp", 200, 512, "127.0.0.1");
        benchRunOnceRR("shmsem", 200, 512);
        benchRunOnceRR("msgq", 200, 512);

        benchRunOnceRR_IP("udp", 1000, 512, "127.0.0.1");
        benchRunOnceRR("shmsem", 1000, 512);
        benchRunOnceRR("msgq", 1000, 512);

        benchRunOnceRR_IP("udp", 200, 1024, "127.0.0.1");
        benchRunOnceRR("shmsem", 200, 1024);
        benchRunOnceRR("msgq", 200, 1024);

        benchRunOnceRR_IP("udp", 1000, 1024, "127.0.0.1");
        benchRunOnceRR("shmsem", 1000, 1024);
        benchRunOnceRR("msgq", 1000, 1024);
    }
    return 0;
}

int benchRunOnceOneWay_Preset(int repeat)
{
    if (repeat <= 0) return -1;
    for (int i = 0; i < repeat; ++i) {
        benchRunOnceOneWay_IP("udp", 200, 256, "127.0.0.1");
        benchRunOnceOneWay("shmsem", 200, 256);
        benchRunOnceOneWay("msgq", 200, 256);

        benchRunOnceOneWay_IP("udp", 1000, 256, "127.0.0.1");
        benchRunOnceOneWay("shmsem", 1000, 256);
        benchRunOnceOneWay("msgq", 1000, 256);

        benchRunOnceOneWay_IP("udp", 200, 512, "127.0.0.1");
        benchRunOnceOneWay("shmsem", 200, 512);
        benchRunOnceOneWay("msgq", 200, 512);

        benchRunOnceOneWay_IP("udp", 1000, 512, "127.0.0.1");
        benchRunOnceOneWay("shmsem", 1000, 512);
        benchRunOnceOneWay("msgq", 1000, 512);

        benchRunOnceOneWay_IP("udp", 200, 1024, "127.0.0.1");
        benchRunOnceOneWay("shmsem", 200, 1024);
        benchRunOnceOneWay("msgq", 200, 1024);

        benchRunOnceOneWay_IP("udp", 1000, 1024, "127.0.0.1");
        benchRunOnceOneWay("shmsem", 1000, 1024);
        benchRunOnceOneWay("msgq", 1000, 1024);
    }
    return 0;
}

int benchRunOnceOneWay_Preset_RtpClient(int repeat)
{
    if (repeat <= 0) return -1;
    for (int i = 0; i < repeat; ++i) {
        benchRunOnceOneWay_Srv_IP("udp", 200, 256, "127.0.0.1");
        benchRunOnceOneWay_Srv("shmsem", 200, 256);
        benchRunOnceOneWay_Srv("msgq", 200, 256);

        benchRunOnceOneWay_Srv_IP("udp", 1000, 256, "127.0.0.1");
        benchRunOnceOneWay_Srv("shmsem", 1000, 256);
        benchRunOnceOneWay_Srv("msgq", 1000, 256);

        benchRunOnceOneWay_Srv_IP("udp", 200, 512, "127.0.0.1");
        benchRunOnceOneWay_Srv("shmsem", 200, 512);
        benchRunOnceOneWay_Srv("msgq", 200, 512);

        benchRunOnceOneWay_Srv_IP("udp", 1000, 512, "127.0.0.1");
        benchRunOnceOneWay_Srv("shmsem", 1000, 512);
        benchRunOnceOneWay_Srv("msgq", 1000, 512);

        benchRunOnceOneWay_Srv_IP("udp", 200, 1024, "127.0.0.1");
        benchRunOnceOneWay_Srv("shmsem", 200, 1024);
        benchRunOnceOneWay_Srv("msgq", 200, 1024);

        benchRunOnceOneWay_Srv_IP("udp", 1000, 1024, "127.0.0.1");
        benchRunOnceOneWay_Srv("shmsem", 1000, 1024);
        benchRunOnceOneWay_Srv("msgq", 1000, 1024);
    }
    return 0;
}

int benchRunOnceRR_Preset_RtpClient(int repeat)
{
    if (repeat <= 0) return -1;
    for (int i = 0; i < repeat; ++i) {
        benchRunOnceRR_Srv_IP("udp", 200, 256, "127.0.0.1");
        benchRunOnceRR_Srv("shmsem", 200, 256);
        benchRunOnceRR_Srv("msgq", 200, 256);

        benchRunOnceRR_Srv_IP("udp", 1000, 256, "127.0.0.1");
        benchRunOnceRR_Srv("shmsem", 1000, 256);
        benchRunOnceRR_Srv("msgq", 1000, 256);

        benchRunOnceRR_Srv_IP("udp", 200, 512, "127.0.0.1");
        benchRunOnceRR_Srv("shmsem", 200, 512);
        benchRunOnceRR_Srv("msgq", 200, 512);

        benchRunOnceRR_Srv_IP("udp", 1000, 512, "127.0.0.1");
        benchRunOnceRR_Srv("shmsem", 1000, 512);
        benchRunOnceRR_Srv("msgq", 1000, 512);

        benchRunOnceRR_Srv_IP("udp", 200, 1024, "127.0.0.1");
        benchRunOnceRR_Srv("shmsem", 200, 1024);
        benchRunOnceRR_Srv("msgq", 200, 1024);

        benchRunOnceRR_Srv_IP("udp", 1000, 1024, "127.0.0.1");
        benchRunOnceRR_Srv("shmsem", 1000, 1024);
        benchRunOnceRR_Srv("msgq", 1000, 1024);
    }
    return 0;
}

int benchAutoRunAll(const char* vxe_path, const char* csv_path)
{
    const char* vxe = (vxe_path && vxe_path[0]) ? vxe_path : BENCH_AUTO_VXE_DEFAULT;
    (void)csv_path;
    const char* csv = NULL;
    (void)mkdir(BENCH_AUTO_DIR_DEFAULT, 0777);
    bench_auto_log_open(BENCH_AUTO_LOG_DEFAULT);
    bench_log_set_path(BENCH_AUTO_LOG_DEFAULT);

    const int rates[] = {200, 500, 1000};
    const int payloads[] = {256, 512, 1024};
    const char* transports[] = {"udp", "shmsem", "msgq"};
    const char* udp_ips[] = {"127.0.0.1"};
    size_t total = 0;
    size_t current = 0;

    {
        size_t per_combo = (sizeof(udp_ips) / sizeof(udp_ips[0])) * 2u;
        per_combo += 2u; /* shmsem */
        per_combo += 2u; /* msgq */
        total = per_combo * (sizeof(rates) / sizeof(rates[0])) *
                (sizeof(payloads) / sizeof(payloads[0])) * 2u;
    }
    bench_auto_log("[BENCH][AUTO] total_runs=%u\n", (unsigned)total);

    for (size_t r = 0; r < (sizeof(rates) / sizeof(rates[0])); ++r) {
        for (size_t p = 0; p < (sizeof(payloads) / sizeof(payloads[0])); ++p) {
            int rate = rates[r];
            int payload = payloads[p];
            bench_auto_log("[BENCH][AUTO] combo rate=%d payload=%d\n", rate, payload);
            for (size_t t = 0; t < (sizeof(transports) / sizeof(transports[0])); ++t) {
                const char* transport = transports[t];
                for (int mode = BENCH_MODE_RR; mode >= BENCH_MODE_ONEWAY; --mode) {
                    if (strcmp(transport, "udp") == 0) {
                        for (size_t i = 0; i < (sizeof(udp_ips) / sizeof(udp_ips[0])); ++i) {
                            const char* ip = udp_ips[i];
                            bench_auto_log("[BENCH][AUTO] progress=%u/%u\n",
                                           (unsigned)(++current), (unsigned)total);
                            (void)bench_run_pair(transport, ip, mode, rate, payload, 1, vxe, csv);
                            bench_auto_log("[BENCH][AUTO] progress=%u/%u\n",
                                           (unsigned)(++current), (unsigned)total);
                            (void)bench_run_pair(transport, ip, mode, rate, payload, 0, vxe, csv);
                        }
                    } else {
                        const char* name = (strcmp(transport, "msgq") == 0) ? "bench_msgq" : "bench_shm";
                        bench_auto_log("[BENCH][AUTO] progress=%u/%u\n",
                                       (unsigned)(++current), (unsigned)total);
                        (void)bench_run_pair(transport, name, mode, rate, payload, 1, vxe, csv);
                        bench_auto_log("[BENCH][AUTO] progress=%u/%u\n",
                                       (unsigned)(++current), (unsigned)total);
                        (void)bench_run_pair(transport, name, mode, rate, payload, 0, vxe, csv);
                    }
                }
            }
        }
    }
    bench_auto_log("[BENCH][AUTO] all done\n");
    bench_auto_log_close();
    bench_log_close();
    return 0;
}
