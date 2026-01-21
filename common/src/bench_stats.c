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
    s->tx_fail = 0;
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
    bench_percentiles_t o = {0,0,0,0,0,0,0};
    if (!s || s->count == 0) return o;
    o.min_ns = s->samples_ns[0];
    o.p50_ns = pickp(s->samples_ns, s->count, 0.50);
    o.p90_ns = pickp(s->samples_ns, s->count, 0.90);
    o.p99_ns = pickp(s->samples_ns, s->count, 0.99);
    o.p999_ns = pickp(s->samples_ns, s->count, 0.999);
    o.p9999_ns = pickp(s->samples_ns, s->count, 0.9999);
    o.max_ns = s->samples_ns[s->count - 1];
    return o;
}

static size_t lower_bound_u64(const uint64_t* a, size_t n, uint64_t v)
{
    size_t lo = 0;
    size_t hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < v) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

void bench_stats_tail_counts(const bench_stats_t* s, bench_tail_counts_t* out)
{
    if (!out) return;
    out->over_50us = 0;
    out->over_100us = 0;
    out->over_1ms = 0;
    if (!s || s->count == 0) return;
    size_t n = s->count;
    size_t idx50 = lower_bound_u64(s->samples_ns, n, 50000ull);
    size_t idx100 = lower_bound_u64(s->samples_ns, n, 100000ull);
    size_t idx1ms = lower_bound_u64(s->samples_ns, n, 1000000ull);
    out->over_50us = (idx50 < n) ? (uint64_t)(n - idx50) : 0;
    out->over_100us = (idx100 < n) ? (uint64_t)(n - idx100) : 0;
    out->over_1ms = (idx1ms < n) ? (uint64_t)(n - idx1ms) : 0;
}

size_t bench_stats_topk(const bench_stats_t* s, uint64_t* out, size_t k)
{
    if (!s || !out || k == 0 || s->count == 0) return 0;
    size_t n = (s->count < k) ? s->count : k;
    for (size_t i = 0; i < n; ++i) {
        out[i] = s->samples_ns[s->count - 1 - i];
    }
    return n;
}
