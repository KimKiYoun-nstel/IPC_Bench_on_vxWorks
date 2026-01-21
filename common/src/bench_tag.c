#include "bench_tag.h"

#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>

#define BENCH_TAG_DIR "/tffs0/IPC_Bench"

static void bench_tag_sanitize(const char* tag, char* out, size_t cap)
{
    size_t j = 0;
    if (cap == 0) return;
    for (size_t i = 0; tag && tag[i] != '\0' && j + 1 < cap; ++i) {
        unsigned char c = (unsigned char)tag[i];
        if (isalnum(c) || c == '_' || c == '-' || c == '.') {
            out[j++] = (char)c;
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
}

static void bench_tag_ensure_dir(void)
{
    struct stat st;
    if (stat(BENCH_TAG_DIR, &st) == 0) return;
    (void)mkdir(BENCH_TAG_DIR, 0777);
}

int bench_tag_build_path(const char* tag, const char* ext, char* out, size_t cap)
{
    if (!tag || !tag[0] || !out || cap == 0) return -1;
    if (!ext) ext = "";
    bench_tag_ensure_dir();
    char safe[128];
    bench_tag_sanitize(tag, safe, sizeof(safe));
    size_t needed = strlen(BENCH_TAG_DIR) + 1 + strlen(safe) + strlen(ext) + 1;
    if (needed > cap) return -1;
    (void)snprintf(out, cap, "%s/%s%s", BENCH_TAG_DIR, safe, ext);
    return 0;
}
