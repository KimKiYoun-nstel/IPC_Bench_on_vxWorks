#include "bench_proto.h"
#include <string.h>

size_t bench_build_msg(uint8_t* out, size_t cap, bench_msg_type_t type, uint32_t seq,
                       uint64_t t0_ns, const void* payload, uint16_t payload_len)
{
    if (!out || cap < sizeof(bench_msg_hdr_t) + payload_len) return 0;
    bench_msg_hdr_t h;
    h.magic = BENCH_MAGIC;
    h.ver = BENCH_VER;
    h.type = (uint16_t)type;
    h.seq = seq;
    h.t0_ns = t0_ns;
    h.payload_len = payload_len;
    h.flags = 0;
    memcpy(out, &h, sizeof(h));
    if (payload_len && payload) memcpy(out + sizeof(h), payload, payload_len);
    return sizeof(h) + payload_len;
}

int bench_parse_msg(const uint8_t* buf, size_t len, bench_msg_hdr_t* hdr_out, const uint8_t** payload_out)
{
    if (!buf || len < sizeof(bench_msg_hdr_t)) return -1;
    const bench_msg_hdr_t* h = (const bench_msg_hdr_t*)buf;
    if (h->magic != BENCH_MAGIC || h->ver != BENCH_VER) return -2;
    if (len < sizeof(bench_msg_hdr_t) + h->payload_len) return -3;
    if (hdr_out) *hdr_out = *h;
    if (payload_out) *payload_out = buf + sizeof(bench_msg_hdr_t);
    return 0;
}
