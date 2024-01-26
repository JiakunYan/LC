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
    .niters = 1000,
    .nrecvs = 8
};
const int CACHE_LINE_SIZE = 64;
const int PAGE_SIZE = 4096;
int num_proc, rank;

typedef struct {
  LCIS_server_t server;
  LCIS_endpoint_t endpoint;
  LCIS_mr_t mr;
//  void *mr_addr;
//  uint32_t mr_size;
  uint8_t dev_port;
  // Helper fields.
  int* qp2rank;
  int qp2rank_mod;
} Device;
Device device;

//void *send_buf = NULL;
//void *recv_buf = NULL;
int peer_rank;

int send_done_counter = 0, recv_done_counter = 0;

static inline void LCIS_serve_recv_(void* p, int src_rank, size_t length,
                                   uint32_t imm_data)
{
  LCII_PCOUNTER_ADD(net_recv_comp, length);
  LCII_packet_t* packet = (LCII_packet_t*)p;
  LCII_proto_t proto = imm_data;
  // NOTE: this should be RGID because it is received from remote.
  LCI_endpoint_t ep = LCI_ENDPOINTS[PROTO_GET_RGID(proto)];
  LCI_tag_t tag = PROTO_GET_TAG(proto);
  LCI_msg_type_t msg_type = PROTO_GET_TYPE(proto);
  packet->context.src_rank = src_rank;

  switch (msg_type) {
    case LCI_MSG_RDMA_MEDIUM: {
      LCII_context_t* ctx;
      size_t size = sizeof(LCII_context_t);
      size = (size + LCI_CACHE_LINE - 1) & (~(LCI_CACHE_LINE - 1));
      int ret = posix_memalign(&ctx, 64, size);
      LCI_Assert(
          ret == 0, "posix_memalign(%lu, %lu) returned %d (Free memory %lu/%lu)\n",
          64, size, ret, sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE),
          sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE));
//      ctx = LCIU_malloc(sizeof(LCII_context_t));
//      ctx->data.mbuffer.address = packet->data.address;
//      ctx->data.mbuffer.length = length;
//      ctx->data_type = LCI_MEDIUM;
//      ctx->rank = src_rank;
//      ctx->tag = tag;
//      LCII_initilize_comp_attr(ctx->comp_attr);
//      LCII_comp_attr_set_comp_type(ctx->comp_attr, ep->msg_comp_type);
//      ctx->completion = ep->default_comp;
//      ctx->user_context = NULL;
//      lc_ce_dispatch(ctx);
      break;
    }
    default:
      LCI_Assert(false, "Unknown proto!\n");
  }
}


void pollCQ() {
  LCIS_cq_entry_t entry[LCI_CQ_MAX_POLL];
  int count = LCIS_poll_cq(device.endpoint, entry);
  for (int i = 0; i < count; ++i) {
    if (entry[i].opcode == LCII_OP_SEND) {
      ++send_done_counter;
    } else {
      LCIS_serve_recv_((LCII_packet_t*)entry[i].ctx, entry[i].rank,
                      entry[i].length, entry[i].imm_data);
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

  device.server = LCI_UR_DEVICE->server;
  device.endpoint = LCI_UR_DEVICE->endpoint_worker->endpoint;
//  LCIS_server_init(NULL, &device.server);
//  LCIS_endpoint_init(device.server, &device.endpoint, true);

  // Create RDMA memory.
//  device.mr_size = config.msg_size * 2;
//  posix_memalign(&device.mr_addr, PAGE_SIZE, device.mr_size);
//  if (!device.mr_addr) {
//    fprintf(stderr, "Unable to allocate memory\n");
//    exit(EXIT_FAILURE);
//  }
//  device.mr = LCIS_rma_reg(device.server, device.mr_addr, device.mr_size);
  device.mr = LCI_UR_DEVICE->heap.segment->mr;

//  send_buf = (char*) device.mr_addr;
//  recv_buf = (char*) device.mr_addr + config.msg_size;
  peer_rank = (1 - rank) % num_proc;

//  posix_memalign(&send_buf, PAGE_SIZE, config.msg_size);
//  assert_(send_buf);
//  posix_memalign(&recv_buf, PAGE_SIZE, config.msg_size);
//  assert_(recv_buf);
}

void finalize() {
//  LCI_finalize();
}

void post_recv() {
//  LCI_mbuffer_t mbuffer;
//  LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer);
//  LCI_error_t ret = LCIS_post_recv(device.endpoint, mbuffer.address, config.msg_size,
//                                   device.mr, LCII_mbuffer2packet(mbuffer));
//  assert_(ret == LCI_OK);
}

void post_send() {
  LCI_mbuffer_t mbuffer;
  LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer);
  mbuffer.length = config.msg_size;
//  while (LCIS_post_send(device.endpoint, peer_rank, mbuffer.address, config.msg_size,
//                        device.mr, 0, LCII_mbuffer2packet(mbuffer)) != LCI_OK) continue;
//  while (LCIS_post_sends(device.endpoint, peer_rank, mbuffer.address, config.msg_size, 0) != LCI_OK) continue;

  while (LCI_putmna(LCI_UR_ENDPOINT, mbuffer, peer_rank, 0, LCI_DEFAULT_COMP_REMOTE) != LCI_OK) continue;
//  while (LCI_putmac_(LCI_UR_ENDPOINT, mbuffer, peer_rank, 0, LCI_DEFAULT_COMP_REMOTE, NULL, NULL) != LCI_OK) continue;
  ++send_done_counter;
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
