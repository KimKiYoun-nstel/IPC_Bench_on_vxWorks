#include "bench_stats.h"
#include <stdlib.h>

static int cmp_u64(const void* a, const void* b)
{
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    return (ua>ub) - (ua<ub);
}

void bench_stats_init(bench_stats_t* s, uint64_t* buf, size_t cap)
{
    s->samples_ns = buf;
    s->capacity = cap;
    s->count = 0;
    s->sent = s->received = s->loss = s->out_of_order = 0;
}

void bench_stats_add_sample(bench_stats_t* s, uint64_t ns)
{
    if (s->count < s->capacity) s->samples_ns[s->count++] = ns;
}

void bench_stats_finalize(bench_stats_t* s)
{
    if (s->count > 1) qsort(s->samples_ns, s->count, sizeof(uint64_t), cmp_u64);
}

static uint64_t pickp(const uint64_t* a, size_t n, double p)
{
    if (n == 0) return 0;
    size_t idx = (size_t)((double)(n - 1) * p + 0.5);
    if (idx >= n) idx = n - 1;
    return a[idx];
}

bench_percentiles_t bench_stats_percentiles(const bench_stats_t* s)
{
    bench_percentiles_t o = {0,0,0,0,0};
    if (!s || s->count == 0) return o;
    o.min_ns = s->samples_ns[0];
    o.p50_ns = pickp(s->samples_ns, s->count, 0.50);
    o.p90_ns = pickp(s->samples_ns, s->count, 0.90);
    o.p99_ns = pickp(s->samples_ns, s->count, 0.99);
    o.max_ns = s->samples_ns[s->count - 1];
    return o;
}
