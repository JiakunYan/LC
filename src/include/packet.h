#ifndef PACKET_H_
#define PACKET_H_

#include "config.h"
#include <stdint.h>

#define lc_pk_init(ep_, pid_, proto_, p)  \
  p->context.ep = (ep_);                   \
  p->context.poolid = (pid_);              \
  p->context.ref = 1;                      \
  p->context.proto = (proto_);

#define lc_pk_free(ep_, p_) \
  lc_pool_put((ep_)->pkpool, p_);

#define lc_pk_free_data(ep_, p_) \
  if ((p_)->context.poolid != -1)   \
    lc_pool_put_to((ep_)->pkpool, p_, (p_)->context.poolid); \
  else \
    lc_pool_put((ep_)->pkpool, p_); \

struct __attribute__((packed)) packet_context {
  // Most of the current ctx requires 128-bits (FIXME)
  int64_t hwctx[2];
  // Here is LLCI context.
  LCI_Request req_s;
  LCI_Request* req;
  LCI_Endpoint ep;
  intptr_t rma_mem;
  int16_t proto;
  int8_t poolid;
  int8_t ref;
};

struct __attribute__((packed)) packet_rts {
  void* cb;
  intptr_t ce;
  intptr_t src_addr;
  size_t size;
  intptr_t tgt_addr;
};

struct __attribute__((packed)) packet_rtr {
  // lc_send_cb cb;
  intptr_t ce;
  intptr_t src_addr;
  size_t size;
  intptr_t tgt_addr;
  uint32_t rkey;
  uint32_t comm_id;
};

struct __attribute__((packed)) packet_data {
  union {
    struct packet_rts rts;
    struct packet_rtr rtr;
    char buffer[0];
  };
};

struct __attribute__((packed)) lc_packet {
  struct packet_context context;
  struct packet_data data;
};

#endif
