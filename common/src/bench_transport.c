#include "bench_transport.h"
#include <string.h>
#include <stdlib.h>

int bench_transport_udp_init(bench_transport_t* t);
int bench_transport_local_init(bench_transport_t* t);
int bench_transport_msgq_init(bench_transport_t* t);
int bench_transport_shmsem_init(bench_transport_t* t);

int bench_transport_create(bench_transport_t* t, const char* name)
{
    if (!t || !name) return -1;
    memset(t, 0, sizeof(*t));
    if (!strcmp(name, "udp")) return bench_transport_udp_init(t);
    if (!strcmp(name, "local")) return bench_transport_local_init(t);
    if (!strcmp(name, "msgq")) return bench_transport_msgq_init(t);
    if (!strcmp(name, "shmsem")) return bench_transport_shmsem_init(t);
    return -2;
}

void bench_transport_destroy(bench_transport_t* t)
{
    if (!t) return;
    if (t->vtbl && t->vtbl->close) t->vtbl->close(t);
    if (t->impl) free(t->impl);
    memset(t, 0, sizeof(*t));
}
