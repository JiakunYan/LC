#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mv.h"
#define MV_USE_SERVER_IBV
#include "src/include/mv_priv.h"

// #define USE_L1_MASK
#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

#include "comm_exp.h"

mvh* mv;

int main(int argc, char** args)
{
  size_t heap_size = 1024 * 1024 * 1024;
  mv_open(&argc, &args, heap_size, &mv);
  set_me_to_last();

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int total, skip;

  mv_ctx ctx;
  for (size_t len = 1; len <= 1 << 22; len <<= 1) {
    if (len > 8192) {
      skip = SKIP_LARGE;
      total = TOTAL_LARGE;
    } else {
      skip = SKIP;
      total = TOTAL;
    }
    void* buffer = mv_alloc(len);
    if (rank == 0) {
      memset(buffer, 'A', len);
      double t1;
      for (int i = 0; i < skip + total; i++) {
        if (i == skip) t1 = MPI_Wtime();
        // send
        while (!mv_send_enqueue_init(mv, buffer, len, 1, 0, &ctx))
          mv_progress(mv);
        while (!mv_test(&ctx))
          mv_progress(mv);
        //recv
        while (!mv_recv_dequeue(mv, &ctx))
          mv_progress(mv);
        mv_free(ctx.buffer);
      }
      printf("%d \t %.5f\n", len, (MPI_Wtime() - t1)/total / 2 * 1e6);
    } else {
      for (int i = 0; i < skip + total; i++) {
        // recv.
        while (!mv_recv_dequeue(mv, &ctx))
          mv_progress(mv);
        if (i == 0)
          for (int j = 0; j < len; j++) {
            assert(((char*)ctx.buffer)[j] == 'A');
          }
        mv_free(ctx.buffer);
        // send
        while (!mv_send_enqueue_init(mv, buffer, len, 0, 0, &ctx))
          mv_progress(mv);
        while (!mv_test(&ctx))
          mv_progress(mv);
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    mv_free(buffer);
  }
  mv_close(mv);
  return 0;
}

void main_task(intptr_t arg) { }

