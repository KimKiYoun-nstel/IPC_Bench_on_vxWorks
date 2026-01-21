#include "bench_time.h"
#include <time.h>

#ifdef _WRS_KERNEL
#include <sysLib.h>
#include <tickLib.h>
#include <timerDev.h>
#endif

uint64_t bench_now_ns(void)
{
    struct timespec ts;
#ifdef _WRS_KERNEL
    {
        static int ts_inited = 0;
        static UINT32 ts32_freq = 0;
        if (!ts_inited) {
            (void)sysTimestampEnable();
            ts32_freq = sysTimestampFreq();
            ts_inited = 1;
        }
        if (ts32_freq != 0) {
            static UINT32 last;
            static uint64_t high;
            UINT32 now = sysTimestamp();
            if (now < last) high += (1ULL << 32);
            last = now;
            return ((high | now) * 1000000000ull) / (uint64_t)ts32_freq;
        }
    }
#endif
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    }
#ifdef _WRS_KERNEL
    {
        uint32_t ticks = tickGet();
        uint32_t rate = (uint32_t)sysClkRateGet();
        if (rate == 0) rate = 1000;
        return (uint64_t)ticks * 1000000000ull / (uint64_t)rate;
    }
#endif
    return 0;
}

uint64_t bench_wall_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    }
#ifdef _WRS_KERNEL
    {
        uint32_t ticks = tickGet();
        uint32_t rate = (uint32_t)sysClkRateGet();
        if (rate == 0) rate = 1000;
        return (uint64_t)ticks * 1000000000ull / (uint64_t)rate;
    }
#endif
    return 0;
}
