#include "bench_transport.h"
#include <stdlib.h>

/* TODO: implement AF_LOCAL/COMP */
typedef struct { int sock; } local_impl_t;
static int open_(bench_transport_t* t, const bench_endpoint_cfg_t* cfg) { (void)t;(void)cfg; return -1; }
static int send_(bench_transport_t* t, const void* buf, size_t len) { (void)t;(void)buf;(void)len; return -1; }
static int recv_(bench_transport_t* t, void* buf, size_t cap, int to_ms) { (void)t;(void)buf;(void)cap;(void)to_ms; return -1; }
static void close_(bench_transport_t* t) { (void)t; }
static const bench_transport_vtbl_t V = { open_, send_, recv_, close_ };
int bench_transport_local_init(bench_transport_t* t)
{
    t->impl = calloc(1, sizeof(local_impl_t));
    t->vtbl = &V;
    return 0;
}
