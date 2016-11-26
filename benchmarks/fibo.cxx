#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <thread>

#include "comm_exp.h"
#include "ult.h"

long* times;
long* fibo;
worker* w;

struct thread_data_t {
  long val;
  long ret;
};

int number;
int nworker;

void ffibo(intptr_t arg) {
  thread_data_t* td = (thread_data_t*)arg;
  if (td->val <= 1) {
    td->ret = td->val;
  } else {
    thread_data_t data[2];
    data[0].val = td->val - 1;
    data[1].val = td->val - 2;
    auto s1 =
        w[(tlself.worker->id() + 1) % nworker].spawn(ffibo, (intptr_t)&data[0]);
    auto s2 =
        w[(tlself.worker->id() + 2) % nworker].spawn(ffibo, (intptr_t)&data[1]);
    s1->join();
    s2->join();
    td->ret = data[0].ret + data[1].ret;
  }
}
worker* random_worker() {
  int p = rand() % nworker;
  // printf("pick %d\n", p);
  return &w[p];
}

void main_task(intptr_t args) {
  worker* w = (worker*)args;
  double t = wtime();
  thread_data_t data = {number, 0};
  for (int tt = 0; tt < TOTAL_LARGE; tt++) {
    ffibo((intptr_t)&data);
  }
  printf("RESULT: %lu %f\n", data.ret,
         (double)1e6 * (wtime() - t) / TOTAL_LARGE);
  w[0].stop_main();
}

int main(int argc, char** args) {
#ifdef USE_ABT
  ABT_init(argc, args);
#endif
  if (argc < 3) {
    printf("Usage: %s <nworker> <number>\n", args[0]);
    return 1;
  }
  number = atoi(args[1]);
  nworker = atoi(args[2]);
  w = ::new worker[nworker];
  for (int i = 1; i < nworker; i++) {
    w[i].start();
  }
  w[0].start_main(main_task, (intptr_t)w);
  for (int i = 1; i < nworker; i++) {
    w[i].stop();
  }
  return 0;
}
