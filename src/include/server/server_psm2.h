#ifndef SERVER_PSM_H_
#define SERVER_PSM_H_

#include <psm2.h>    /* required for core PSM2 functions */
#include <psm2_mq.h> /* required for PSM2 MQ functions (send, recv, etc) */
#include <psm2_am.h>
#include <stdlib.h>
#include <string.h>

#include <mpi.h>
#include "pmi.h"
#include "lc/macro.h"
#include "dreg/dreg.h"

#define GET_PROTO(p) (p & 0x00ff)
#define MAKE_PSM_TAG(proto, rank) \
  ((uint64_t)((((uint64_t)proto) << 40) | (((uint64_t)rank) << 24)))

#ifdef LC_SERVER_DEBUG
#define PSM_SAFECALL(x)                                       \
  {                                                           \
    int err = (x);                                            \
    if (err != PSM2_OK && err != PSM2_MQ_NO_COMPLETIONS) {    \
      fprintf(stderr, "err : (%s:%d)\n", __FILE__, __LINE__); \
      MPI_Abort(MPI_COMM_WORLD, err);                         \
    }                                                         \
  }                                                           \
  while (0)                                                   \
    ;

#else
#define PSM_SAFECALL(x) \
  {                     \
    (x);                \
  }
#endif

#define ALIGNMENT (4096)
#define ALIGNEDX(x) \
  (void*)((((uintptr_t)x + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT))
#define MAX_CQ_SIZE (16 * 1024)
#define MAX_POLL 8

typedef struct psm_server {
  psm2_uuid_t uuid;

  // Endpoint + Endpoint ID.
  psm2_ep_t myep;
  psm2_epid_t myepid;

  psm2_epid_t* epid;
  psm2_epaddr_t* epaddr;

  psm2_mq_t mq;
  int psm_recv_am_idx;

  uintptr_t* heap_addr;
  void* heap;
  uint32_t heap_rkey;
  int recv_posted;
  lcrq_t free_mr;
  lch* mv;
  int with_mpi;
} psm_server __attribute__((aligned(64)));

struct psm_mr {
  psm2_mq_req_t req;
  psm_server* server;
  uintptr_t addr;
  size_t size;
  uint32_t rkey;
};

LC_INLINE void psm_init(lch* mv, size_t heap_size, psm_server** s_ptr);
LC_INLINE void psm_post_recv(psm_server* s, lc_packet* p);
LC_INLINE int psm_write_send(psm_server* s, int rank, void* buf, size_t size,
                             lc_packet* ctx, uint32_t proto);
LC_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             lc_packet* ctx, uint32_t proto);

LC_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx);

LC_INLINE void psm_finalize(psm_server* s);
static uint32_t next_key = 0;

#define PSM_RECV ((uint64_t)1 << 61)
#define PSM_SEND ((uint64_t)1 << 62)
#define PSM_RDMA ((uint64_t)1 << 63)

LC_INLINE void prepare_rdma(psm_server* s, struct psm_mr* mr)
{
  PSM_SAFECALL(psm2_mq_irecv(
      s->mq, mr->rkey,                /* message tag */
      (uint64_t)(0x0000000000ffffff), /* message tag mask */
      0,                              /* no flags */
      (void*)mr->addr, mr->size, (void*)(PSM_RDMA | (uintptr_t)mr), &mr->req));
}

LC_INLINE uintptr_t _real_psm_reg(psm_server* s, void* buf, size_t size)
{
  struct psm_mr* mr = (struct psm_mr*)malloc(sizeof(struct psm_mr));
  mr->server = s;
  mr->addr = (uintptr_t)buf;
  mr->size = size;
  mr->rkey = (1 + __sync_fetch_and_add(&next_key, 1)) & 0x00ffffff;
#ifdef LC_SERVER_DEBUG
  assert(next_key != 0);
#endif
  prepare_rdma(s, mr);
  return (uintptr_t)mr;
}

LC_INLINE int _real_psm_free(uintptr_t mem)
{
  struct psm_mr* mr = (struct psm_mr*)mem;
  if (psm2_mq_cancel(&mr->req) == PSM2_OK) {
    psm2_mq_wait(&mr->req, NULL);
    free(mr);
  } else {
    lcrq_enqueue(&(mr->server->free_mr), (void*)mr);
  }
  return 1;
}

LC_INLINE uintptr_t psm_rma_reg(psm_server* s, void* buf, size_t size)
{
#ifdef USE_DREG
  return (uintptr_t)dreg_register(s, buf, size);
#else
  return _real_psm_reg(s, buf, size);
#endif
}

LC_INLINE int psm_rma_dereg(uintptr_t mem)
{
#ifdef USE_DREG
  dreg_unregister((dreg_entry*)mem);
  return 1;
#else
  return _real_psm_free(mem);
#endif
}

LC_INLINE uint32_t psm_rma_key(uintptr_t mem)
{
#ifdef USE_DREG
  return ((struct psm_mr*)(((dreg_entry*)mem)->memhandle[0]))->rkey;
#else
  return ((struct psm_mr*)mem)->rkey;
#endif
}

static volatile int psm_start_stop = 0;

static void* psm_startup(void* arg)
{
  psm_server* server = (psm_server*)arg;
  while (!psm_start_stop) psm2_poll(server->myep);
  return 0;
}

#if 0
static lch* __mv;
static volatile lc_packet* __p_r = 0;
static volatile int has_data = 0;
static int psm_recv_am(psm2_am_token_t token, psm2_amarg_t* args, int nargs,
                       void* src, uint32_t len)
{
  if (!__p_r) __p_r = lc_pool_get(__mv->pkpool);
  memcpy((void*)&__p_r->data, src, len);
  has_data = 1;
  return 0;
}
#endif

LC_INLINE void psm_init(lch* mv, size_t heap_size, psm_server** s_ptr)
{
  setenv("I_MPI_FABRICS", "ofa", 1);
  // setenv("PSM2_DEVICES", "shm,hfi", 1);
  setenv("PSM2_SHAREDCONTEXTS", "0", 1);

  psm_server* s = (psm_server*)malloc(sizeof(struct psm_server));

  int rc;
  int ver_major = PSM2_VERNO_MAJOR;
  int ver_minor = PSM2_VERNO_MINOR;

#ifdef USE_DREG
  dreg_init();
#endif

  PSM_SAFECALL(psm2_init(&ver_major, &ver_minor));

  /* Setup the endpoint options struct */
  struct psm2_ep_open_opts option;
  PSM_SAFECALL(psm2_ep_open_opts_get_defaults(&option));
  option.affinity = 0;  // Disable affinity.

  psm2_uuid_generate(s->uuid);

  /* Attempt to open a PSM2 endpoint. This allocates hardware resources. */
  PSM_SAFECALL(psm2_ep_open(s->uuid, &option, &s->myep, &s->myepid));

  /* Exchange ep addr. */
  int with_mpi = s->with_mpi = 0;
  char* lc_mpi = getenv("LC_MPI");
  if (lc_mpi) with_mpi = s->with_mpi = atoi(lc_mpi);

  char key[256];
  char value[256];
  char name[256];

  if (!with_mpi) {
    int spawned;
    PMI_Init(&spawned, &mv->size, &mv->me);
    PMI_KVS_Get_my_name(name, 255);
  } else {
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    if (MPI_THREAD_MULTIPLE != provided) {
      printf("Need MPI_THREAD_MULTIPLE\n");
      exit(EXIT_FAILURE);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &mv->me);
    MPI_Comm_size(MPI_COMM_WORLD, &mv->size);
  }

  int epid_array_mask[mv->size];

  s->epid = (psm2_epid_t*)calloc(mv->size, sizeof(psm2_epid_t));
  s->epaddr = (psm2_epaddr_t*)calloc(mv->size, sizeof(psm2_epaddr_t));

  if (!with_mpi) {
    sprintf(key, "_LC_KEY_%d", mv->me);
    sprintf(value, "%llu", (unsigned long long)s->myepid);
    PMI_KVS_Put(name, key, value);
    PMI_Barrier();
    for (int i = 0; i < mv->size; i++) {
      sprintf(key, "_LC_KEY_%d", i);
      PMI_KVS_Get(name, key, value, 255);
      psm2_epid_t destaddr;
      sscanf(value, "%llu", (unsigned long long*)&destaddr);
      memcpy(&s->epid[i], &destaddr, sizeof(psm2_epid_t));
      epid_array_mask[i] = 1;
    }
  } else {
    for (int i = 0; i < mv->size; i++) {
      psm2_epid_t destaddr;
      MPI_Sendrecv(&s->myepid, sizeof(psm2_epid_t), MPI_BYTE, i, 99,
          &destaddr, sizeof(psm2_epid_t), MPI_BYTE, i, 99,
          MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      memcpy(&s->epid[i], &destaddr, sizeof(psm2_epid_t));
      epid_array_mask[i] = 1;
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  pthread_t startup_thread;
  pthread_create(&startup_thread, NULL, psm_startup, (void*)s);

  psm2_error_t epid_connect_errors[mv->size];
  PSM_SAFECALL(psm2_ep_connect(s->myep, mv->size, s->epid, epid_array_mask,
                               epid_connect_errors, s->epaddr, 0));

  if (!with_mpi)
    PMI_Barrier();
  else
    MPI_Barrier(MPI_COMM_WORLD);

  psm_start_stop = 1;
  pthread_join(startup_thread, NULL);

  /* Setup mq for comm */
  PSM_SAFECALL(psm2_mq_init(s->myep, PSM2_MQ_ORDERMASK_NONE, NULL, 0, &s->mq));

  lcrq_init(&s->free_mr);

#ifdef USE_AM
  psm2_am_handler_fn_t am[1];
  am[0] = psm_recv_am;

  PSM_SAFECALL(psm2_am_register_handlers(s->myep, am, 1, &s->psm_recv_am_idx));
#endif

  s->heap = 0;
  posix_memalign(&s->heap, 4096, heap_size);

  s->recv_posted = 0;
  s->mv = mv;
  *s_ptr = s;
}

LC_INLINE void psm_progress(psm_server* s)
{
  psm2_mq_req_t req;
  psm2_mq_status_t status;
  psm2_error_t err;

#ifdef USE_AM
  // NOTE(danghvu): This ugly hack is to ultilize PSM2 AM layer.
  // This saves some memory copy.
  if (!__p_r) __p_r = lc_pool_get_nb(s->mv->pkpool);
  if (__p_r) psm2_poll(s->myep);
  if (has_data) {
    lc_serve_recv(s->mv, (lc_packet*)__p_r);
    __p_r = 0;
    has_data = 0;
  }
#endif

  err = psm2_mq_ipeek(s->mq, &req, NULL);
  if (err == PSM2_OK) {
    err = psm2_mq_test(&req, &status);  // we need the status
    uintptr_t ctx = (uintptr_t)status.context;
    if (ctx) {
      if (ctx & PSM_RECV) {
        uint64_t proto = (status.msg_tag >> 40);
        lc_packet* p = (lc_packet*)(ctx ^ PSM_RECV);
        p->context.from = (status.msg_tag >> 24) & 0x0000ffff;
        p->context.size = (status.msg_length);
        p->context.tag = proto >> 8;
        lc_serve_recv(s->mv, p, GET_PROTO(proto));
        s->recv_posted--;
      } else if (ctx & PSM_SEND) {
        lc_packet* p = (lc_packet*)(ctx ^ PSM_SEND);
        uint32_t proto = p->context.proto;
        lc_serve_send(s->mv, p, proto);
      } else {  // else if (ctx & PSM_RDMA) { // recv rdma.
        struct psm_mr* mr = (struct psm_mr*)(ctx ^ PSM_RDMA);
        uint32_t imm = (status.msg_tag >> 32);
        prepare_rdma(s, mr);
        if (imm) lc_serve_imm(s->mv, imm);
      }
    }
  }

  if (s->recv_posted < MAX_RECV)
    psm_post_recv(s, lc_pool_get_nb(s->mv->pkpool));

  // Cleanup stuffs when nothing to do, this improves the reg/dereg a bit.
  struct psm_mr* mr = (struct psm_mr*)lcrq_dequeue(&(s->free_mr));
  if (unlikely(mr)) {
    if (psm2_mq_cancel(&mr->req) == PSM2_OK) {
      psm2_mq_wait(&mr->req, NULL);
      free(mr);
    } else {
      lcrq_enqueue(&(s->free_mr), (void*)mr);
    }
  }

#ifdef LC_SERVER_DEBUG
  if (s->recv_posted == 0) printf("WARNING DEADLOCK\n");
#endif
}

LC_INLINE void psm_post_recv(psm_server* s, lc_packet* p)
{
  if (p == NULL) return;

  PSM_SAFECALL(psm2_mq_irecv(
      s->mq, 0,                       /* message tag */
      (uint64_t)(0x0000000000ffffff), /* message tag mask */
      0,                              /* no flags */
      &p->data, POST_MSG_SIZE, (void*)(PSM_RECV | (uintptr_t)&p->context),
      (psm2_mq_req_t*)p));
  s->recv_posted++;
}

LC_INLINE int psm_write_send(psm_server* s, int rank, void* ubuf, size_t size,
                             lc_packet* ctx, uint32_t proto)
{
  int me = s->mv->me;
  uint64_t real_tag = MAKE_PSM_TAG(proto, me);

  if (size <= 1024) {
#ifdef USE_AM
    PSM_SAFECALL(psm2_am_request_short(
        s->epaddr[rank], s->psm_recv_am_idx, NULL, 0, buf, size,
        PSM2_AM_FLAG_NOREPLY | PSM2_AM_FLAG_ASYNC, NULL, 0));
#else
    PSM_SAFECALL(psm2_mq_send(s->mq, s->epaddr[rank], 0, real_tag, ubuf, size));
#endif
    lc_serve_send(s->mv, ctx, GET_PROTO(proto));
    return 1;
  } else {
    if (ubuf != ctx->data.buffer) memcpy(ctx->data.buffer, ubuf, size);
    ctx->context.proto = GET_PROTO(proto);
    PSM_SAFECALL(psm2_mq_isend(s->mq, s->epaddr[rank], 0, /* no flags */
                               real_tag, ctx->data.buffer, size,
                               (void*)(PSM_SEND | (uintptr_t)ctx),
                               (psm2_mq_req_t*)ctx));
    return 0;
  }
}

LC_INLINE void psm_write_rma(psm_server* s, int rank, void* from,
                             uintptr_t addr, uint32_t rkey, size_t size,
                             lc_packet* ctx, uint32_t proto)
{
}

LC_INLINE void psm_write_rma_signal(psm_server* s, int rank, void* buf,
                                    uintptr_t addr, uint32_t rkey, size_t size,
                                    uint32_t sid, lc_packet* ctx)
{
  PSM_SAFECALL(psm2_mq_isend(s->mq, s->epaddr[rank], 0,    /* no flags */
                             ((uint64_t)sid << 32) | rkey, /* tag */
                             buf, size, (void*)(PSM_SEND | (uintptr_t)ctx),
                             (psm2_mq_req_t*)ctx));
}

LC_INLINE void psm_finalize(psm_server* s)
{
  if (s->with_mpi)
    MPI_Finalize();
  else
    PMI_Finalize();
  free(s);
}

LC_INLINE uint32_t psm_heap_rkey(psm_server* s, int node __UNUSED__)
{
  return 0;
}

LC_INLINE void* psm_heap_ptr(psm_server* s) { return s->heap; }
#define lc_server_init psm_init
#define lc_server_send psm_write_send
#define lc_server_rma psm_write_rma
#define lc_server_rma_signal psm_write_rma_signal
#define lc_server_heap_rkey psm_heap_rkey
#define lc_server_heap_ptr psm_heap_ptr
#define lc_server_progress psm_progress
#define lc_server_finalize psm_finalize
#define lc_server_post_recv psm_post_recv

#define lc_server_rma_reg psm_rma_reg
#define lc_server_rma_key psm_rma_key
#define lc_server_rma_dereg psm_rma_dereg
#define _real_server_reg _real_psm_reg
#define _real_server_dereg _real_psm_free

#endif