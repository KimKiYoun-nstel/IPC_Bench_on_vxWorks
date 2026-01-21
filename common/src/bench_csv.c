#include "bench_csv.h"
#include "bench_tag.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>


#ifdef _WRS_KERNEL
#include <errnoLib.h>
#define BENCH_ERRNO() errnoGet()
#else
#include <errno.h>
#define BENCH_ERRNO() errno
#endif

static const char* bench_side_string(void)
{
#ifdef _WRS_KERNEL
    return "DKM";
#else
    return "RTP";
#endif
}

static const char* bench_role_string(bench_role_t role)
{
    return (role == BENCH_ROLE_SERVER) ? "server" : "client";
}

static const char* bench_mode_string(bench_mode_t mode)
{
    return (mode == BENCH_MODE_RR) ? "rr" : "oneway";
}

static void bench_csv_write_field(FILE* f, const char* s)
{
    if (!s) s = "";
    int need_quote = (strchr(s, ',') || strchr(s, '"') || strchr(s, '\n'));
    if (!need_quote) {
        fputs(s, f);
        return;
    }
    fputc('"', f);
    for (const char* p = s; *p; ++p) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

static int bench_csv_need_header(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return (st.st_size == 0);
}

static void bench_csv_dir_from_path(const char* path, char* out, size_t cap)
{
    if (!path || !out || cap == 0) return;
    const char* slash = strrchr(path, '/');
    if (!slash) {
        out[0] = '\0';
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len >= cap) len = cap - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static void bench_csv_ensure_dir(const char* path)
{
    char dir[256];
    bench_csv_dir_from_path(path, dir, sizeof(dir));
    if (dir[0] == '\0') return;
    struct stat st;
    if (stat(dir, &st) == 0) return;
    (void)mkdir(dir, 0777);
}

int bench_csv_append(const char* path,
                     const bench_endpoint_cfg_t* ep,
                     const bench_run_cfg_t* run,
                     const bench_report_cfg_t* rep,
                     const bench_result_t* res)
{
    char derived[256];
    if ((!path || !path[0]) && rep && rep->tag) {
        if (bench_tag_build_path(rep->tag, ".csv", derived, sizeof(derived)) == 0) {
            path = derived;
        }
    }
    if (!path || !path[0] || !ep || !run) return -1;

    bench_csv_ensure_dir(path);
    int need_header = bench_csv_need_header(path);
    FILE* f = fopen(path, "a");
    if (!f) {
        printf("[BENCH][CSV] open failed path=%s errno=%d\n", path, BENCH_ERRNO());
        return -1;
    }

    if (need_header) {
        fputs("timestamp,side,role,mode,transport,bind_or_dst,port,name,tag,rate_hz,"
              "duration_sec,payload_len,warmup_sec,timeout_ms,samples,sent,received,loss,"
              "out_of_order,tx_fail,min_ns,p50_ns,p90_ns,p99_ns,p99_9_ns,p99_99_ns,max_ns,"
              "over_50us,over_100us,over_1ms,"
              "cpu_min_pct,cpu_avg_pct,cpu_max_pct\n", f);
    }

    time_t now = time(NULL);
    bench_result_t zero = {0};
    const bench_result_t* r = res ? res : &zero;

    fprintf(f, "%ld,", (long)now);
    bench_csv_write_field(f, bench_side_string());
    fputc(',', f);
    bench_csv_write_field(f, bench_role_string(ep->role)); fputc(',', f);
    bench_csv_write_field(f, bench_mode_string(run->mode)); fputc(',', f);
    bench_csv_write_field(f, ep->transport); fputc(',', f);
    bench_csv_write_field(f, ep->bind_or_dst); fputc(',', f);
    fprintf(f, "%u,", (unsigned)ep->port);
    bench_csv_write_field(f, ep->name); fputc(',', f);
    bench_csv_write_field(f, (rep && rep->tag) ? rep->tag : ""); fputc(',', f);
    fprintf(f, "%d,%d,%d,%d,%d,", run->rate_hz, run->duration_sec, run->payload_len,
            run->warmup_sec, ep->timeout_ms);
    fprintf(f, "%llu,%u,%u,%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,",
            (unsigned long long)r->samples,
            (unsigned)r->sent,
            (unsigned)r->received,
            (unsigned)r->loss,
            (unsigned)r->out_of_order,
            (unsigned)r->tx_fail,
            (unsigned long long)r->min_ns,
            (unsigned long long)r->p50_ns,
            (unsigned long long)r->p90_ns,
            (unsigned long long)r->p99_ns,
            (unsigned long long)r->p999_ns,
            (unsigned long long)r->p9999_ns,
            (unsigned long long)r->max_ns,
            (unsigned long long)r->over_50us,
            (unsigned long long)r->over_100us,
            (unsigned long long)r->over_1ms);
    fprintf(f, "%u.%02u,%u.%02u,%u.%02u\n",
            r->cpu_min_x100 / 100, r->cpu_min_x100 % 100,
            r->cpu_avg_x100 / 100, r->cpu_avg_x100 % 100,
            r->cpu_max_x100 / 100, r->cpu_max_x100 % 100);

    fclose(f);
    return 0;
}
