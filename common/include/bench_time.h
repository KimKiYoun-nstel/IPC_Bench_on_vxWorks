#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t bench_now_ns(void);
uint64_t bench_wall_ns(void);
#ifdef __cplusplus
}
#endif
