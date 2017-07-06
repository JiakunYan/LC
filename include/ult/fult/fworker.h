#ifndef F_WORKER_H_
#define F_WORKER_H_

#include "lc/macro.h"
#include <pthread.h>

typedef struct fworker {
  fthread** thread_pool;
  int thread_pool_last;
  volatile int thread_pool_lock;
  pthread_t runner;
  int stop;
  int id;
  fthread* threads;
  fctx ctx;
  struct {
#ifdef USE_L1_MASK
    unsigned long l1_mask[8];
#endif
    unsigned long mask[NMASK * WORDSIZE];
  } __attribute__((aligned(64)));
} fworker __attribute__((aligned(64)));

LC_INLINE fthread* fworker_spawn(fworker*, ffunc f, void* data,
                                 size_t stack_size);
LC_INLINE void fworker_create(fworker**);
LC_INLINE void fworker_work(fworker*, fthread*);
LC_INLINE void fworker_sched_thread(fworker* w, const int tid);
LC_INLINE void fworker_fini_thread(fworker* w, const int tid);

LC_INLINE void* wfunc(void*);

LC_INLINE void fworker_start(fworker* w)
{
  w->stop = 0;
  pthread_create(&w->runner, 0, wfunc, w);
}

LC_INLINE void fworker_stop(fworker* w)
{
  w->stop = 1;
  pthread_join(w->runner, 0);
}

static fthread main_sched;
static fthread* main_thread;

LC_INLINE void fworker_start_main(fworker* w)
{
  w->stop = 0;
  memset(&main_sched, 0, sizeof(struct fthread));
  fthread_create(&main_sched, wfunc, w, MAIN_STACK_SIZE);
  main_thread = fworker_spawn(w, NULL, NULL, MAIN_STACK_SIZE);
  main_thread->ctx.parent = &(main_sched.ctx);
  fthread_yield(main_thread);
}

LC_INLINE void fworker_stop_main(fworker* w)
{
  w->stop = 1;
  main_sched.ctx.parent = &(main_thread->ctx);
  fthread_wait(main_thread);
  free(main_sched.stack);
}

LC_INLINE int fworker_id(fworker* w) { return w->id; }
#endif
