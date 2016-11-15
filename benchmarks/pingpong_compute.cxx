#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <unistd.h>
#include <mpi.h>

// #define CHECK_RESULT

#include "mpiv.h"
#include "comm_exp.h"

#include "profiler.h"

#define FIELD_WIDTH 18
#define FLOAT_PRECISION 2

#if 0
#undef total
#define total 20
#undef skip
#define skip 0
#endif

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 20)

int size = 0;

int main(int argc, char** args) {
  MPIV_Init(&argc, &args);
  if (argc > 1) size = atoi(args[1]);
  MPIV_Init_worker(1);
  MPIV_Finalize();
  return 0;
}

#define ARRAY_SIZE 1024*1024*1024
static char trash[ARRAY_SIZE];

void main_task(intptr_t) {
  double times = 0;
  int rank = MPIV.me;
  void* r_buf = (void*)mpiv_malloc((size_t)MAX_MSG_SIZE);
  void* s_buf = (void*)mpiv_malloc((size_t)MAX_MSG_SIZE);
  srand48(1238);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    MPIV_Barrier(MPI_COMM_WORLD);
    times = 0;
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = MPIV_Wtime();
        }

        MPIV_Send(s_buf, 64, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
        MPIV_Recv(r_buf, 64, MPI_CHAR, 1, 1, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
      }
      times = MPIV_Wtime() - times;
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        MPIV_Recv(r_buf, 64, MPI_CHAR, 0, 1, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);

        int loop = lrand48() % (size);
        for (int ii = 0; ii < loop; ii++) {
          trash[lrand48() % ARRAY_SIZE] += ((char*) r_buf)[lrand48() % 64];
        }

        MPIV_Send(s_buf, 64, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
      }
    }
    MPIV_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      double latency = times * 1e6 / (2.0 * total);
      fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH, FLOAT_PRECISION, latency);
      fflush(stdout);
    }

  }

  mpiv_free(r_buf);
  mpiv_free(s_buf);
}