#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t* samples_ns;
    size_t capacity;
    size_t count;
    uint32_t sent;
    uint32_t received;
    uint32_t loss;
    uint32_t out_of_order;
} bench_stats_t;

void bench_stats_init(bench_stats_t* s, uint64_t* buf, size_t cap);
void bench_stats_add_sample(bench_stats_t* s, uint64_t ns);
void bench_stats_finalize(bench_stats_t* s);

typedef struct { uint64_t min_ns, p50_ns, p90_ns, p99_ns, max_ns; } bench_percentiles_t;
bench_percentiles_t bench_stats_percentiles(const bench_stats_t* s);

#ifdef __cplusplus
}
#endif
