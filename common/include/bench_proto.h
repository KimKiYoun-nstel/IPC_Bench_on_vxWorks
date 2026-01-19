#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BENCH_MAGIC 0x42454E43u /* 'BENC' */
#define BENCH_VER   1

typedef enum { BENCH_MSG_DATA=1, BENCH_MSG_REQ=2, BENCH_MSG_RSP=3 } bench_msg_type_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t type;
    uint32_t seq;
    uint64_t t0_ns;
    uint16_t payload_len;
    uint16_t flags;
} bench_msg_hdr_t;
#pragma pack(pop)

size_t bench_build_msg(uint8_t* out, size_t cap, bench_msg_type_t type, uint32_t seq,
                       uint64_t t0_ns, const void* payload, uint16_t payload_len);

int bench_parse_msg(const uint8_t* buf, size_t len, bench_msg_hdr_t* hdr_out, const uint8_t** payload_out);

#ifdef __cplusplus
}
#endif
