#include "bench_transport.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct {
    int sock;
    int is_server;
    int timeout_ms;
    int peer_valid;
    struct sockaddr_un peer;
    char bind_path[96];
} local_impl_t;

static int set_timeout_ms(int sock, int to_ms)
{
    if (to_ms <= 0) return 0;
    struct timeval tv;
    tv.tv_sec = to_ms / 1000;
    tv.tv_usec = (to_ms % 1000) * 1000;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
}

static int open_(bench_transport_t* t, const bench_endpoint_cfg_t* cfg)
{
    if (!t || !cfg) return -1;
    local_impl_t* l = (local_impl_t*)t->impl;
    l->sock = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (l->sock < 0) return -1;
    l->is_server = (cfg->role == BENCH_ROLE_SERVER);
    l->timeout_ms = cfg->timeout_ms;
    (void)set_timeout_ms(l->sock, l->timeout_ms);

    memset(&l->peer, 0, sizeof(l->peer));
    l->peer_valid = 0;

    const char* path = cfg->bind_or_dst ? cfg->bind_or_dst : (cfg->name ? cfg->name : NULL);
    if (!path) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (l->is_server) {
        strncpy(l->bind_path, addr.sun_path, sizeof(l->bind_path) - 1);
        unlink(l->bind_path);
        if (bind(l->sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
        return 0;
    }

    if (connect(l->sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    l->peer = addr;
    l->peer_valid = 1;
    return 0;
}

static int send_(bench_transport_t* t, const void* buf, size_t len)
{
    local_impl_t* l = (local_impl_t*)t->impl;
    if (!l || l->sock < 0) return -1;
    if (l->is_server) {
        if (!l->peer_valid) return -1;
        return (int)sendto(l->sock, (const char*)buf, (int)len, 0,
                           (struct sockaddr*)&l->peer, sizeof(l->peer));
    }
    return (int)send(l->sock, (const char*)buf, (int)len, 0);
}

static int recv_(bench_transport_t* t, void* buf, size_t cap, int to_ms)
{
    local_impl_t* l = (local_impl_t*)t->impl;
    if (!l || l->sock < 0) return -1;
    if (to_ms > 0 && to_ms != l->timeout_ms) {
        if (set_timeout_ms(l->sock, to_ms) == 0) l->timeout_ms = to_ms;
    }
    if (l->is_server) {
        socklen_t slen = sizeof(l->peer);
        int n = (int)recvfrom(l->sock, (char*)buf, (int)cap, 0,
                              (struct sockaddr*)&l->peer, &slen);
        if (n >= 0) l->peer_valid = 1;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        return n;
    }
    {
        int n = (int)recv(l->sock, (char*)buf, (int)cap, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        return n;
    }
}

static void close_(bench_transport_t* t)
{
    local_impl_t* l = (local_impl_t*)t->impl;
    if (!l) return;
    if (l->sock >= 0) close(l->sock);
    if (l->is_server && l->bind_path[0] != '\0') unlink(l->bind_path);
    l->sock = -1;
}
static const bench_transport_vtbl_t V = { open_, send_, recv_, close_ };
int bench_transport_local_init(bench_transport_t* t)
{
    t->impl = calloc(1, sizeof(local_impl_t));
    if (!t->impl) return -1;
    ((local_impl_t*)t->impl)->sock = -1;
    t->vtbl = &V;
    return 0;
}
