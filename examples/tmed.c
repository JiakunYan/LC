#include "lc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG lc_max_medium(0)

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  lc_ep ep;
  lc_init(1, &ep);
  int rank = 0;
  lc_get_proc_num(&rank);
  int tag = {99};

  lc_req req;
  double t1;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf;
  void* dst_buf;
  posix_memalign(&src_buf, alignment, MAX_MSG);
  posix_memalign(&dst_buf, alignment, MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        req.sync = 0;
        while (lc_sendm(src_buf, size, 1-rank, tag, ep) != LC_OK)
          lc_progress(0);

        req.sync = 0;
        while (lc_recvm(dst_buf, size, 1-rank, tag, ep, &req) != LC_OK)
          lc_progress(0);
        while (req.sync == 0)
          lc_progress(0);
        if (i == 0) {
          for (int j = 0; j < size; j++)
            assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
        }
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        req.sync = 0;
        while (lc_recvm(dst_buf, size, 1-rank, tag, ep, &req) != LC_OK)
          lc_progress(0);
        while (req.sync == 0)
          lc_progress(0);

        req.sync = 0;
        while (lc_sendm(src_buf, size, 1-rank, tag, ep) != LC_OK)
          lc_progress(0);
      }
    }
  }
  lc_finalize();
}
