#include "bench_transport.h"
#include "bench_proto.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vxWorks.h>
#include <msgQLib.h>
#include <objLib.h>
#include <sysLib.h>
#include <errnoLib.h>

#define BENCH_MSGQ_MAX_MSGS 128
#define BENCH_MSGQ_MAX_PAYLOAD 2048
#define BENCH_MSGQ_MAX_LEN (sizeof(bench_msg_hdr_t) + BENCH_MSGQ_MAX_PAYLOAD)

typedef struct {
    MSG_Q_ID q_req;
    MSG_Q_ID q_rsp;
    int is_server;
    int timeout_ms;
    char name_req[64];
    char name_rsp[64];
} msgq_impl_t;

static _Vx_ticks_t to_ticks(int ms)
{
    if (ms < 0) return WAIT_FOREVER;
    if (ms == 0) return NO_WAIT;
    int ticks = (int)((ms * sysClkRateGet() + 999) / 1000);
    if (ticks <= 0) ticks = 1;
    return (_Vx_ticks_t)ticks;
}

static void build_names(const bench_endpoint_cfg_t* cfg, char* req, size_t req_len,
                        char* rsp, size_t rsp_len)
{
    const char* base = cfg->name ? cfg->name : (cfg->bind_or_dst ? cfg->bind_or_dst : "bench");
    char tmp[48];
    if (base[0] != '/') {
        snprintf(tmp, sizeof(tmp), "/%s", base);
        base = tmp;
    }
    snprintf(req, req_len, "%s_req", base);
    snprintf(rsp, rsp_len, "%s_rsp", base);
}

static int open_(bench_transport_t* t, const bench_endpoint_cfg_t* cfg)
{
    if (!t || !cfg) return -1;
    msgq_impl_t* m = (msgq_impl_t*)t->impl;
    m->is_server = (cfg->role == BENCH_ROLE_SERVER);
    m->timeout_ms = cfg->timeout_ms;
    build_names(cfg, m->name_req, sizeof(m->name_req), m->name_rsp, sizeof(m->name_rsp));

    if (m->is_server) {
        msgQUnlink(m->name_req);
        msgQUnlink(m->name_rsp);
        m->q_req = msgQOpen(m->name_req, BENCH_MSGQ_MAX_MSGS, BENCH_MSGQ_MAX_LEN,
                            MSG_Q_FIFO, OM_CREATE, NULL);
        if (m->q_req == MSG_Q_ID_NULL) return -1;
        m->q_rsp = msgQOpen(m->name_rsp, BENCH_MSGQ_MAX_MSGS, BENCH_MSGQ_MAX_LEN,
                            MSG_Q_FIFO, OM_CREATE, NULL);
        if (m->q_rsp == MSG_Q_ID_NULL) return -1;
        return 0;
    }

    m->q_req = msgQOpen(m->name_req, 0, 0, 0, 0, NULL);
    if (m->q_req == MSG_Q_ID_NULL) return -1;
    m->q_rsp = msgQOpen(m->name_rsp, 0, 0, 0, 0, NULL);
    if (m->q_rsp == MSG_Q_ID_NULL) return -1;
    return 0;
}

static int send_(bench_transport_t* t, const void* buf, size_t len)
{
    msgq_impl_t* m = (msgq_impl_t*)t->impl;
    if (!m) return -1;
    MSG_Q_ID q = m->is_server ? m->q_rsp : m->q_req;
    if (q == MSG_Q_ID_NULL) return -1;
    if (len > BENCH_MSGQ_MAX_LEN) return -1;
    STATUS st = msgQSend(q, (char*)buf, len, to_ticks(m->timeout_ms), MSG_PRI_NORMAL);
    return (st == OK) ? (int)len : -1;
}

static int recv_(bench_transport_t* t, void* buf, size_t cap, int to_ms)
{
    msgq_impl_t* m = (msgq_impl_t*)t->impl;
    if (!m) return -1;
    MSG_Q_ID q = m->is_server ? m->q_req : m->q_rsp;
    if (q == MSG_Q_ID_NULL) return -1;
    if (cap > BENCH_MSGQ_MAX_LEN) cap = BENCH_MSGQ_MAX_LEN;
    ssize_t n = msgQReceive(q, (char*)buf, cap, to_ticks(to_ms));
    if (n == ERROR) {
        if (errnoGet() == S_objLib_OBJ_TIMEOUT) return 0;
        return -1;
    }
    return (int)n;
}

static void close_(bench_transport_t* t)
{
    msgq_impl_t* m = (msgq_impl_t*)t->impl;
    if (!m) return;
    if (m->q_req != MSG_Q_ID_NULL) msgQClose(m->q_req);
    if (m->q_rsp != MSG_Q_ID_NULL) msgQClose(m->q_rsp);
    if (m->is_server) {
        msgQUnlink(m->name_req);
        msgQUnlink(m->name_rsp);
    }
    m->q_req = MSG_Q_ID_NULL;
    m->q_rsp = MSG_Q_ID_NULL;
}
static const bench_transport_vtbl_t V = { open_, send_, recv_, close_ };
int bench_transport_msgq_init(bench_transport_t* t)
{
    t->impl = calloc(1, sizeof(msgq_impl_t));
    if (!t->impl) return -1;
    ((msgq_impl_t*)t->impl)->q_req = MSG_Q_ID_NULL;
    ((msgq_impl_t*)t->impl)->q_rsp = MSG_Q_ID_NULL;
    t->vtbl = &V;
    return 0;
}
