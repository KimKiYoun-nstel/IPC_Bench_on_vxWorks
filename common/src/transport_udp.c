#include "bench_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct {
    int sock;
    int is_server;
    int timeout_ms;
    int peer_valid;
    struct sockaddr_in peer;
} udp_impl_t;

#ifdef _WRS_KERNEL
#include <errnoLib.h>
#define BENCH_ERRNO() errnoGet()
#else
#define BENCH_ERRNO() errno
#endif

static int set_timeout_ms(int sock, int to_ms)
{
    if (to_ms <= 0) return 0;
    struct timeval tv;
    tv.tv_sec = to_ms / 1000;
    tv.tv_usec = (to_ms % 1000) * 1000;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
}

static void set_reuse_addr(int sock)
{
    int opt = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#ifdef SO_REUSEPORT
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt, sizeof(opt));
#endif
}

static int open_(bench_transport_t* t, const bench_endpoint_cfg_t* cfg)
{
    if (!t || !cfg) return -1;
    udp_impl_t* u = (udp_impl_t*)t->impl;
    u->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (u->sock < 0) {
        printf("[BENCH][UDP] socket failed errno=%d\n", BENCH_ERRNO());
        return -1;
    }

    u->is_server = (cfg->role == BENCH_ROLE_SERVER);
    u->timeout_ms = cfg->timeout_ms;
    (void)set_timeout_ms(u->sock, u->timeout_ms);

    memset(&u->peer, 0, sizeof(u->peer));
    u->peer_valid = 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->port);

    if (u->is_server) {
        const char* bind_ip = cfg->bind_or_dst ? cfg->bind_or_dst : "0.0.0.0";
        addr.sin_addr.s_addr = inet_addr(bind_ip);
        if (addr.sin_addr.s_addr == INADDR_NONE) addr.sin_addr.s_addr = htonl(INADDR_ANY);
        set_reuse_addr(u->sock);
        if (bind(u->sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("[BENCH][UDP] bind failed ip=%s port=%u errno=%d\n",
                   bind_ip, (unsigned)cfg->port, BENCH_ERRNO());
            return -1;
        }
        return 0;
    }

    addr.sin_addr.s_addr = inet_addr(cfg->bind_or_dst ? cfg->bind_or_dst : "127.0.0.1");
    if (addr.sin_addr.s_addr == INADDR_NONE) return -1;
    if (connect(u->sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[BENCH][UDP] connect failed dst=%s port=%u errno=%d\n",
               cfg->bind_or_dst ? cfg->bind_or_dst : "127.0.0.1",
               (unsigned)cfg->port, BENCH_ERRNO());
        return -1;
    }
    u->peer = addr;
    u->peer_valid = 1;
    return 0;
}

static int send_(bench_transport_t* t, const void* buf, size_t len)
{
    udp_impl_t* u = (udp_impl_t*)t->impl;
    if (!u || u->sock < 0) return -1;
    if (u->is_server) {
        if (!u->peer_valid) return -1;
        return (int)sendto(u->sock, (const char*)buf, (int)len, 0,
                           (struct sockaddr*)&u->peer, sizeof(u->peer));
    }
    return (int)send(u->sock, (const char*)buf, (int)len, 0);
}

static int recv_(bench_transport_t* t, void* buf, size_t cap, int to_ms)
{
    udp_impl_t* u = (udp_impl_t*)t->impl;
    if (!u || u->sock < 0) return -1;
    if (to_ms > 0 && to_ms != u->timeout_ms) {
        if (set_timeout_ms(u->sock, to_ms) == 0) u->timeout_ms = to_ms;
    }
    if (u->is_server) {
        socklen_t slen = sizeof(u->peer);
        int n = (int)recvfrom(u->sock, (char*)buf, (int)cap, 0,
                              (struct sockaddr*)&u->peer, &slen);
        if (n >= 0) u->peer_valid = 1;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        return n;
    }
    {
        int n = (int)recv(u->sock, (char*)buf, (int)cap, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        return n;
    }
}

static void close_(bench_transport_t* t)
{
    udp_impl_t* u = (udp_impl_t*)t->impl;
    if (!u) return;
    if (u->sock >= 0) close(u->sock);
    u->sock = -1;
}
static const bench_transport_vtbl_t V = { open_, send_, recv_, close_ };
int bench_transport_udp_init(bench_transport_t* t)
{
    t->impl = calloc(1, sizeof(udp_impl_t));
    if (!t->impl) return -1;
    ((udp_impl_t*)t->impl)->sock = -1;
    t->vtbl = &V;
    return 0;
}
