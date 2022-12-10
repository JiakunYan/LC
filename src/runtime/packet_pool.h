#ifndef LC_POOL_H_
#define LC_POOL_H_

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_NPOOLS 272
#define MAX_LOCAL_POOL 32  // align to a cache line.

extern int LCII_pool_nkey;
extern int32_t LCII_tls_pool_metadata[MAX_NPOOLS][MAX_LOCAL_POOL];
extern LCIU_spinlock_t init_lock;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  LCIU_spinlock_t lock;  // size 4 align 4
  LCM_dequeue_t dq;      // size 32 align 8
  char padding[24];
} LCII_local_pool_t __attribute__((aligned(LCI_CACHE_LINE)));

typedef struct LCII_pool_t {
  int key;
  int npools;
  LCII_local_pool_t lpools[MAX_NPOOLS] __attribute__((aligned(LCI_CACHE_LINE)));
} LCII_pool_t __attribute__((aligned(LCI_CACHE_LINE)));

void LCII_pool_create(LCII_pool_t** pool);
void LCII_pool_destroy(LCII_pool_t* pool);
int LCII_pool_count(const struct LCII_pool_t* pool);
static inline void LCII_pool_put(LCII_pool_t* pool, void* elm);
static inline void LCII_pool_put_to(LCII_pool_t* pool, void* elm, int32_t pid);
static inline void* LCII_pool_get(LCII_pool_t* pool);
static inline void* LCII_pool_get_nb(LCII_pool_t* pool);

#ifdef __cplusplus
}
#endif

#define POOL_UNINIT ((int32_t)-1)

static inline int32_t lc_pool_get_local(struct LCII_pool_t* pool)
{
  int wid = LCIU_get_thread_id();
  int32_t pid = LCII_tls_pool_metadata[wid][pool->key];
  if (unlikely(pid == POOL_UNINIT)) {
    LCIU_acquire_spinlock(&init_lock);
    pid = LCII_tls_pool_metadata[wid][pool->key];
    if (pid == POOL_UNINIT) {
      pid = pool->npools;
      LCIU_spinlock_init(&pool->lpools[pid].lock);
      LCM_dq_init(&pool->lpools[pid].dq, LCI_SERVER_NUM_PKTS);
      LCII_tls_pool_metadata[wid][pool->key] = pid;
      ++pool->npools;
    }
    LCIU_release_spinlock(&init_lock);
  }
  // assert(pid >= 0 && pid < pool->npools && "POOL ERROR: pid out-of-range");
  return pid;
}

static inline void* lc_pool_get_slow(struct LCII_pool_t* pool, int32_t pid)
{
  void* ret = NULL;
  int32_t steal = LCIU_rand() % (pool->npools);
  size_t target_size = LCM_dq_size(pool->lpools[steal].dq);
  if (steal != pid && target_size > 0) {
    if (LCIU_try_acquire_spinlock(&pool->lpools[steal].lock)) {
      size_t steal_size =
          LCM_dq_steal(&pool->lpools[pid].dq, &pool->lpools[steal].dq);
      if (steal_size > 0) {
        LCM_DBG_Log(LCM_LOG_DEBUG, "packet", "Packet steal %d->%d: %lu\n",
                    steal, pid, steal_size);
        ret = LCM_dq_pop_top(&pool->lpools[pid].dq);
      }
      LCIU_release_spinlock(&pool->lpools[steal].lock);
    }
  }
  LCII_PCOUNTERS_WRAPPER(LCII_pcounters[LCIU_get_thread_id()].packet_stealing +=
                         1);
  return ret;
}

static inline void LCII_pool_put_to(struct LCII_pool_t* pool, void* elm,
                                    int32_t pid)
{
  LCIU_acquire_spinlock(&pool->lpools[pid].lock);
  LCM_dq_push_top(&pool->lpools[pid].dq, elm);
  LCIU_release_spinlock(&pool->lpools[pid].lock);
}

static inline void LCII_pool_put(struct LCII_pool_t* pool, void* elm)
{
  int32_t pid = lc_pool_get_local(pool);
  LCII_pool_put_to(pool, elm, pid);
}

static inline void* LCII_pool_get_nb(struct LCII_pool_t* pool)
{
  int32_t pid = lc_pool_get_local(pool);
  LCIU_acquire_spinlock(&pool->lpools[pid].lock);
  void* elm = LCM_dq_pop_top(&pool->lpools[pid].dq);
  if (elm == NULL) {
    LCM_DBG_Assert(LCM_dq_size(pool->lpools[pid].dq) == 0,
                   "Unexpected pool length! %lu\n",
                   LCM_dq_size(pool->lpools[pid].dq));
    elm = lc_pool_get_slow(pool, pid);
  }
  LCIU_release_spinlock(&pool->lpools[pid].lock);
  return elm;
}

#endif  // LC_POOL_H_
