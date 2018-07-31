/**
 * @file lc.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all LCI code.
 *
 */

#ifndef LC_H_
#define LC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include "lc/macro.h"


#define EP_ANY      (0)
#define EP_AR_ALLOC (1<<1)
#define EP_AR_PIGGY (1<<2)
#define EP_AR_EXPL  (1<<3)

#define EP_CE_NONE  ((1<<1) << 4)
#define EP_CE_QUEUE ((1<<2) << 4)
#define EP_CE_SYNC  ((1<<3) << 4)
#define EP_CE_AM    ((1<<4) << 4)

#define EP_TYPE_TAG    (EP_CE_NONE  | EP_AR_EXPL)
#define EP_TYPE_QUEUE  (EP_CE_QUEUE | EP_AR_ALLOC)
#define EP_TYPE_SQUEUE (EP_CE_QUEUE | EP_AR_PIGGY)

enum lc_wr_state {
  LC_REQ_PEND = 0,
  LC_REQ_DONE = 1,
};

enum lc_wr_type {
  WR_PROD = 0,
  WR_CONS = 1,
};

enum lc_data_type {
  DAT_EXPL = 0,
  DAT_ALLOC,
  DAT_PIGGY,
};

typedef enum lc_status {
  LC_OK = 0,
  LC_ERR_RETRY,
  LC_ERR_FATAL,
} lc_status;

typedef uint32_t lc_id;
typedef uint32_t lc_eid;
typedef uint32_t lc_gid;
typedef uint32_t lc_alloc_id;

typedef struct lci_rep* lc_rep;

typedef void* (*lc_alloc_fn)(void*, size_t);
typedef void (*lc_free_fn)(void*, void*);

// MPI tag is only upto 64K, it can be piggy-backed as well.
typedef union {
  struct {
    uint16_t trank;
    uint16_t tval;
  } tag;
  uint32_t val;
} lc_meta;

// These using 1 byte since they need to be limitted,
// so that can be offloaded in the future.
typedef uint8_t lc_queue_id;
typedef uint8_t lc_sync_id; 
typedef uint8_t lc_handl_id; 
typedef uint8_t lc_global_id;

typedef struct lc_data {
  enum lc_data_type type;
  union {
    // explicit data.
    struct {
      void* addr;
      size_t size;
    };
    // allocator.
    struct {
      lc_alloc_id alloc_id;
      void* alloc_ctx;
    };
  };
} lc_data;

typedef struct lc_wr {
  enum lc_wr_type type;
  struct {
    struct lc_data source_data;
    struct lc_data target_data;
    lc_meta meta;
    lc_id source;
    lc_rep target;
  };
} lc_wr;

typedef struct lc_req {
  // This flag is going to be set when the communication is done.
  // It is going to be set by the communication serve most likely
  // so we are going to align it to avoid false sharing.
  volatile int flag;

  // Additional fields here.
  void* buffer;
  void* parent; // reserved field for internal used.
  void* rhandle;
  size_t size;
  int rank;
  lc_meta meta;
} lc_req;

typedef struct lci_ep* lc_ep;
typedef struct lci_dev* lc_dev;

LC_EXPORT
lc_status lc_init();

LC_EXPORT
lc_status lc_dev_open(lc_dev* dev);

LC_EXPORT
lc_status lc_ep_open(lc_dev dev, long cap, lc_ep* ep);

LC_EXPORT
lc_status lc_send_tag(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_recv_tag(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_status lc_send_qalloc(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_id lc_recv_qalloc(lc_ep ep, lc_req* req);

LC_EXPORT
lc_status lc_send_qshort(lc_ep ep, lc_rep rep, void* src, size_t size, lc_meta tag, lc_req* req);

LC_EXPORT
lc_id lc_recv_qshort(lc_ep ep, lc_req** req);

LC_EXPORT
lc_status lc_req_free(lc_ep ep, lc_req* req);

LC_EXPORT
void lc_get_proc_num(int *rank);

LC_EXPORT
void lc_get_num_proc(int *size);

LC_EXPORT
lc_status lc_finalize();

LC_EXPORT
lc_status lc_submit(lc_ep, struct lc_wr* wr, lc_req* req); 

LC_EXPORT
lc_status lc_progress(lc_dev);

LC_EXPORT
lc_status lc_progress_q(lc_dev);

LC_EXPORT
lc_status lc_progress_sq(lc_dev);

LC_EXPORT
lc_status lc_progress_t(lc_dev);

LC_EXPORT
lc_status lc_free(lc_ep, void* buf);

LC_EXPORT
lc_status lc_ep_query(lc_dev dev, int prank, int erank, lc_rep* rep);

LC_EXPORT
lc_status lc_ep_set_alloc(lc_ep ep, lc_alloc_fn alloc, lc_free_fn free, void* ctx);

LC_EXPORT
void lc_pm_barrier();

#ifdef __cplusplus
}
#endif

#endif
