#include "bench_transport.h"
#include "bench_proto.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#define SHMSEM_MAGIC 0x53484D31u /* SHM1 */
#define SHMSEM_MAX_PAYLOAD 2048
#define SHMSEM_DATA_MAX (sizeof(bench_msg_hdr_t) + SHMSEM_MAX_PAYLOAD)
#define SHMSEM_ENTRY_SIZE (4 + SHMSEM_DATA_MAX)
#define SHMSEM_RING_CAP 1024

typedef struct {
    uint32_t magic;
    uint32_t entry_size;
    uint32_t capacity;
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    uint8_t data[1];
} shmsem_ring_t;

typedef struct {
    shmsem_ring_t* rx;
    shmsem_ring_t* tx;
    size_t ring_size;
    sem_t* rx_sem;
    sem_t* tx_sem;
    int is_server;
    char name_rx[64];
    char name_tx[64];
    char sem_rx[64];
    char sem_tx[64];
} shmsem_impl_t;

static void build_names(const bench_endpoint_cfg_t* cfg, char* c2s, size_t c2s_len,
                        char* s2c, size_t s2c_len, char* c2s_sem, size_t c2s_sem_len,
                        char* s2c_sem, size_t s2c_sem_len)
{
    const char* base = cfg->name ? cfg->name : (cfg->bind_or_dst ? cfg->bind_or_dst : "bench");
    char tmp[48];
    if (base[0] != '/') {
        snprintf(tmp, sizeof(tmp), "/%s", base);
        base = tmp;
    }
    snprintf(c2s, c2s_len, "%s_c2s", base);
    snprintf(s2c, s2c_len, "%s_s2c", base);
    snprintf(c2s_sem, c2s_sem_len, "%s_c2s_sem", base);
    snprintf(s2c_sem, s2c_sem_len, "%s_s2c_sem", base);
}

static size_t ring_total_size(void)
{
    return sizeof(shmsem_ring_t) - 1 + (size_t)SHMSEM_RING_CAP * (size_t)SHMSEM_ENTRY_SIZE;
}

static shmsem_ring_t* map_ring(const char* name, int create, size_t* out_size)
{
    int flags = O_RDWR | (create ? O_CREAT : 0);
    int fd = shm_open(name, flags, 0666);
    if (fd < 0) return NULL;
    size_t sz = ring_total_size();
    if (create) {
        if (ftruncate(fd, (off_t)sz) != 0) {
            close(fd);
            return NULL;
        }
    }
    void* p = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return NULL;
    if (out_size) *out_size = sz;
    return (shmsem_ring_t*)p;
}

static void ring_init(shmsem_ring_t* r)
{
    if (!r) return;
    if (r->magic == SHMSEM_MAGIC) return;
    r->magic = SHMSEM_MAGIC;
    r->entry_size = SHMSEM_ENTRY_SIZE;
    r->capacity = SHMSEM_RING_CAP;
    r->write_idx = 0;
    r->read_idx = 0;
    __sync_synchronize();
}

static int ring_send(shmsem_ring_t* r, const void* buf, size_t len)
{
    if (!r || len > SHMSEM_DATA_MAX) return -1;
    uint32_t w = r->write_idx;
    uint32_t rd = r->read_idx;
    uint32_t next = (w + 1) % r->capacity;
    if (next == rd) return -1;
    uint8_t* slot = r->data + (size_t)w * r->entry_size;
    uint32_t n = (uint32_t)len;
    memcpy(slot, &n, sizeof(uint32_t));
    memcpy(slot + sizeof(uint32_t), buf, len);
    __sync_synchronize();
    r->write_idx = next;
    return (int)len;
}

static int ring_recv(shmsem_ring_t* r, void* buf, size_t cap)
{
    if (!r) return -1;
    uint32_t w = r->write_idx;
    uint32_t rd = r->read_idx;
    if (w == rd) return 0;
    uint8_t* slot = r->data + (size_t)rd * r->entry_size;
    uint32_t n = 0;
    memcpy(&n, slot, sizeof(uint32_t));
    if (n > cap) return -1;
    memcpy(buf, slot + sizeof(uint32_t), n);
    __sync_synchronize();
    r->read_idx = (rd + 1) % r->capacity;
    return (int)n;
}

static int sem_wait_ms(sem_t* s, int to_ms)
{
    if (to_ms < 0) return sem_wait(s);
    if (to_ms == 0) return sem_trywait(s);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += to_ms / 1000;
    ts.tv_nsec += (long)(to_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return sem_timedwait(s, &ts);
}

static int open_(bench_transport_t* t, const bench_endpoint_cfg_t* cfg)
{
    if (!t || !cfg) return -1;
    shmsem_impl_t* s = (shmsem_impl_t*)t->impl;
    s->is_server = (cfg->role == BENCH_ROLE_SERVER);

    char c2s[64], s2c[64], c2s_sem[64], s2c_sem[64];
    build_names(cfg, c2s, sizeof(c2s), s2c, sizeof(s2c), c2s_sem, sizeof(c2s_sem), s2c_sem, sizeof(s2c_sem));

    int create = s->is_server ? 1 : 0;
    s->ring_size = 0;
    shmsem_ring_t* ring_c2s = map_ring(c2s, create, &s->ring_size);
    shmsem_ring_t* ring_s2c = map_ring(s2c, create, NULL);
    if (!ring_c2s || !ring_s2c) return -1;
    ring_init(ring_c2s);
    ring_init(ring_s2c);

    s->rx = s->is_server ? ring_c2s : ring_s2c;
    s->tx = s->is_server ? ring_s2c : ring_c2s;
    snprintf(s->name_rx, sizeof(s->name_rx), "%s", s->is_server ? c2s : s2c);
    snprintf(s->name_tx, sizeof(s->name_tx), "%s", s->is_server ? s2c : c2s);

    s->rx_sem = sem_open(s->is_server ? c2s_sem : s2c_sem, O_CREAT, 0666, 0);
    s->tx_sem = sem_open(s->is_server ? s2c_sem : c2s_sem, O_CREAT, 0666, 0);
    if (s->rx_sem == SEM_FAILED || s->tx_sem == SEM_FAILED) return -1;
    snprintf(s->sem_rx, sizeof(s->sem_rx), "%s", s->is_server ? c2s_sem : s2c_sem);
    snprintf(s->sem_tx, sizeof(s->sem_tx), "%s", s->is_server ? s2c_sem : c2s_sem);
    return 0;
}

static int send_(bench_transport_t* t, const void* buf, size_t len)
{
    shmsem_impl_t* s = (shmsem_impl_t*)t->impl;
    if (!s || !s->tx || !s->tx_sem) return -1;
    int rc = ring_send(s->tx, buf, len);
    if (rc > 0) sem_post(s->tx_sem);
    return rc;
}

static int recv_(bench_transport_t* t, void* buf, size_t cap, int to_ms)
{
    shmsem_impl_t* s = (shmsem_impl_t*)t->impl;
    if (!s || !s->rx || !s->rx_sem) return -1;
    if (sem_wait_ms(s->rx_sem, to_ms) != 0) {
        if (errno == ETIMEDOUT || errno == EAGAIN) return 0;
        return -1;
    }
    return ring_recv(s->rx, buf, cap);
}

static void close_(bench_transport_t* t)
{
    shmsem_impl_t* s = (shmsem_impl_t*)t->impl;
    if (!s) return;
    if (s->rx) munmap(s->rx, s->ring_size);
    if (s->tx) munmap(s->tx, s->ring_size);
    if (s->rx_sem && s->rx_sem != SEM_FAILED) sem_close(s->rx_sem);
    if (s->tx_sem && s->tx_sem != SEM_FAILED) sem_close(s->tx_sem);
    if (s->is_server) {
        shm_unlink(s->name_rx);
        shm_unlink(s->name_tx);
        sem_unlink(s->sem_rx);
        sem_unlink(s->sem_tx);
    }
    s->rx = NULL;
    s->tx = NULL;
    s->rx_sem = SEM_FAILED;
    s->tx_sem = SEM_FAILED;
}
static const bench_transport_vtbl_t V = { open_, send_, recv_, close_ };
int bench_transport_shmsem_init(bench_transport_t* t)
{
    t->impl = calloc(1, sizeof(shmsem_impl_t));
    if (!t->impl) return -1;
    ((shmsem_impl_t*)t->impl)->rx_sem = SEM_FAILED;
    ((shmsem_impl_t*)t->impl)->tx_sem = SEM_FAILED;
    t->vtbl = &V;
    return 0;
}
