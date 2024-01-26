#include "runtime/lcii.h"
#include <getopt.h>

#define assert_(ret) if (!ret) { fprintf(stderr, "%d:Assert error! %s:%d:%s\n", rank, __FILE__, __LINE__, __FUNCTION__); abort(); }

typedef struct {
  int msg_size;
  int niters;
  int nrecvs;
} Config;

Config config = {
    .msg_size = 8,
    .niters = 10000,
    .nrecvs = 8
};
const int CACHE_LINE_SIZE = 64;
const int PAGE_SIZE = 4096;
int num_proc, rank;

typedef struct {
  LCIS_server_t server;
  LCIS_endpoint_t endpoint;
  LCIS_mr_t mr;
  void *mr_addr;
  uint32_t mr_size;
  uint8_t dev_port;
  // Helper fields.
  int* qp2rank;
  int qp2rank_mod;
} Device;
Device device;

void *send_buf;
void *recv_buf;
int peer_rank;

int send_done_counter = 0, recv_done_counter = 0;

void pollCQ() {
  LCIS_cq_entry_t entry[LCI_CQ_MAX_POLL];
  int count = LCIS_poll_cq(device.endpoint, entry);
  for (int i = 0; i < count; ++i) {
    if (entry[i].opcode == LCII_OP_SEND) {
      ++send_done_counter;
    } else {
      ++recv_done_counter;
    }
  }
}

void init(int argc, char *argv[]) {
  LCI_initialize();
  rank = LCI_RANK;
  num_proc = LCI_NUM_PROCESSES;

  LCT_args_parser_t parser = LCT_args_parser_alloc();
  LCT_args_parser_add(parser, "niters", required_argument, &config.niters);
  LCT_args_parser_add(parser, "nrecvs", required_argument, &config.nrecvs);
  LCT_args_parser_parse(parser, argc, argv);
  if (rank == 0) {
    LCT_args_parser_print(parser, true);
  }
  LCT_args_parser_free(parser);

  LCIS_server_init(NULL, &device.server);
  LCIS_endpoint_init(device.server, &device.endpoint, true);

  // Create RDMA memory.
  device.mr_size = config.msg_size * 2;
  posix_memalign(&device.mr_addr, PAGE_SIZE, device.mr_size);
  if (!device.mr_addr) {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(EXIT_FAILURE);
  }
  device.mr = LCIS_rma_reg(device.server, device.mr_addr, device.mr_size);

  send_buf = (char*) device.mr_addr;
  recv_buf = (char*) device.mr_addr + config.msg_size;
  peer_rank = (1 - rank) % num_proc;
}

void finalize() {
  LCI_finalize();
}

void post_recv() {
  LCI_error_t ret = LCIS_post_recv(device.endpoint, recv_buf, config.msg_size,
                                   device.mr, NULL);
  assert_(ret == LCI_OK);
}

void post_send() {
  while (LCIS_post_send(device.endpoint, peer_rank, send_buf, config.msg_size,
                        device.mr, 0, NULL) != LCI_OK) continue;
}

void wait_one_send() {
  while (!send_done_counter) {
    pollCQ();
  }
  --send_done_counter;
}

void wait_one_recv() {
  while (!recv_done_counter) {
    pollCQ();
  }
  --recv_done_counter;
}

LCI_error_t LCIX_internal_test(int argc, char *argv[])
{
  init(argc, argv);
  for (int i = 0; i < config.nrecvs; ++i) {
    post_recv();
  }

  LCT_time_t start = LCT_now();
  for (int i = 0; i < config.niters; ++i) {
    if (rank == 0) {
      // the sender
      post_recv();
      post_send();
      wait_one_send();
      wait_one_recv();
    } else if (rank == 1) {
      // the receiver
      post_recv();
      wait_one_recv();
      post_send();
      wait_one_send();
    }
  }
  LCT_time_t total = LCT_now() - start;
  if (rank == 0) {
    printf("Total time %lf s\ntime per iter %lf us\n",
           LCT_time_to_s(total), LCT_time_to_us(total / config.niters));
  }
  finalize();

  return LCI_OK;
}
