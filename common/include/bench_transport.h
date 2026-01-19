#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { BENCH_ROLE_SERVER = 0, BENCH_ROLE_CLIENT = 1 } bench_role_t;
typedef enum { BENCH_MODE_ONEWAY = 0, BENCH_MODE_RR = 1 } bench_mode_t;

typedef struct {
    const char* transport;   /* udp|local|msgq|shmsem */
    bench_role_t role;
    const char* bind_or_dst; /* ip/path/name */
    uint16_t port;           /* sockets */
    const char* name;        /* public object name */
    int timeout_ms;
} bench_endpoint_cfg_t;

typedef struct bench_transport bench_transport_t;

typedef struct {
    int  (*open)(bench_transport_t* t, const bench_endpoint_cfg_t* cfg);
    int  (*send)(bench_transport_t* t, const void* buf, size_t len);
    int  (*recv)(bench_transport_t* t, void* buf, size_t cap, int timeout_ms);
    void (*close)(bench_transport_t* t);
} bench_transport_vtbl_t;

struct bench_transport {
    const bench_transport_vtbl_t* vtbl;
    void* impl;
};

int bench_transport_create(bench_transport_t* t, const char* transport_name);
void bench_transport_destroy(bench_transport_t* t);

#ifdef __cplusplus
}
#endif
