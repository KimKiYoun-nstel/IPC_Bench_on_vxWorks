#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int bench_tag_build_path(const char* tag, const char* ext, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
