#include "lc.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG lc_max_medium(0)

int main(int argc, char** args) {
  lc_ep ep, ep2;
  lc_req req;
  lc_req* req_ptr;
  int rank;

  lc_init(1, &ep2);
  lc_opt opt = {.dev = 0, .desc = LC_IMM_CQ };
  lc_ep_dup(&opt, ep2, &ep);

  lc_get_proc_num(&rank);

  uintptr_t addr, raddr;
  lc_ep_get_baseaddr(ep, MAX_MSG, &addr);

  lc_sendm(&addr,  sizeof(uintptr_t), 1-rank, 0, ep);
  while (lc_cq_pop(ep, &req_ptr) != LC_OK)
    lc_progress(0);
  memcpy(&raddr, req_ptr->buffer, req_ptr->size);
  lc_cq_reqfree(ep, req_ptr);

  long* sbuf = (long*) addr;
  long* rbuf = (long*) (addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  lc_pm_barrier();

  double t1;

  for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (i == SKIP)
        t1 = wtime();
      if (rank == 0) {
        while (lc_cq_pop(ep, &req_ptr) != LC_OK)
          lc_progress(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep, req_ptr);
        while (lc_putms(sbuf, size, 1-rank, raddr + MAX_MSG, i, ep) != LC_OK)
          lc_progress(0);
      } else {
        while (lc_putms(sbuf, size, 1-rank, raddr + MAX_MSG, i, ep) != LC_OK)
          lc_progress(0);
        while (lc_cq_pop(ep, &req_ptr) != LC_OK)
          lc_progress(0);
        assert(req_ptr->meta == i);
        lc_cq_reqfree(ep, req_ptr);
      }
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  lc_finalize();
}
