/**
 * @file lci.h
 * @author Hoang-Vu Dang (danghvu@gmail.com)
 * @brief Header file for all LCI code.
 */

#ifndef LCI_H_
#define LCI_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCI_API __attribute__((visibility("default")))

/**
 * LCI Status type.
 */
typedef enum LCI_error_t {
  LCI_OK = 0,
  LCI_ERR_RETRY,
  LCI_ERR_FATAL,
} LCI_error_t;

/**
 * LCI Communication type.
 */
typedef enum LCI_comm_t {
  LCI_COMM_1SIDED,
  LCI_COMM_2SIDED,
  LCI_COMM_COLLECTIVE,
} LCI_comm_t;

/**
 * LCI Message type.
 */
typedef enum LCI_msg_t {
  LCI_MSG_IMMEDIATE,
  LCI_MSG_BUFFERED,
  LCI_MSG_DIRECT,
} LCI_msg_t;


/**
 * LCI completion type.
 */
typedef enum LCI_comp_t {
  LCI_COMPLETION_QUEUE = 0,
  LCI_COMPLETION_HANDLER,
  LCI_COMPLETION_ONE2ONEL,
  LCI_COMPLETION_MANY2ONES,
  LCI_COMPLETION_MANY2ONEL,
  LCI_COMPLETION_ANY2ONES,
  LCI_COMPLETION_ANY2ONEL,
  LCI_COMPLETION_ONE2MANYS,
  LCI_COMPLETION_ONE2MANYL,
  LCI_COMPLETION_MANY2MANYS,
  LCI_COMPLETION_MANY2MANYL
} LCI_comp_t;

/**
 * LCI dynamic buffer.
 */
typedef enum LCI_dynamic_t {
  LCI_STATIC = 0,
  LCI_DYNAMIC
} LCI_dynamic_t;

/**
 * Synchronizer object, owned by the runtime.
 */
typedef uint64_t LCI_one2one_t;

typedef union {
  char immediate[64]; // may not want a full cacheline here.
  void* buffer;
} LCI_data_t;

/**
 * Request object, owned by the user, unless returned from runtime (CQ_Dequeue).
 */
typedef struct LCI_request_s {
  /* Status of the communication. */
  union {
    char immediate[8];
    void* buffer;
  };
  LCI_error_t status;
  enum {invalid, immediate, buffered, direct} type;
  int rank;
  int tag;
  size_t size;
  void* usr_ctx;
  void* __reserved__;
} LCI_request_t;

typedef struct LCI_one2onel {
  LCI_one2one_t sync;
  LCI_request_t request;
} LCI_one2onel_t;

/**
 * Endpoint object, owned by the runtime.
 */
struct LCI_endpoint_s;
typedef struct LCI_endpoint_s* LCI_endpoint_t;

/**
 * Property object, owned by the runtime.
 */
struct LCI_PL_s;
typedef struct LCI_PL_s* LCI_PL;

/**
 * Completion queue object, owned by the runtime.
 */
struct LCI_Cq_s;
typedef struct LCI_Cq_s* LCI_Cq;

/**
 * Handler type
 */
typedef void (*LCI_Handler)(LCI_one2one_t* sync, void* usr_context);

/**
 * Allocator type
 */
typedef void* (*LCI_Allocator)(size_t size, void* usr_context);

/**
 * Initialize LCI.
 */
LCI_API
LCI_error_t LCI_initialize(int num_devices);

/**
 * Finalize LCI.
 */
LCI_API
LCI_error_t LCI_finalize();

/**
 * Create an endpoint Property @prop.
 */
LCI_API
LCI_error_t LCI_PL_create(LCI_PL* prop);

/**
 * Set communication style (1sided, 2sided, collective).
 */
LCI_API
LCI_error_t LCI_PL_set_comm_type(LCI_comm_t type, LCI_PL* prop);

/**
 * Set message type (short, medium, long).
 */
LCI_API
LCI_error_t LCI_PL_set_message_type(LCI_msg_t type, LCI_PL* prop);

/**
 * Set synchronization type for completion.
 */
LCI_API
LCI_error_t LCI_PL_set_sync_type(LCI_comp_t ltype, LCI_comp_t rtype, LCI_PL* prop);

/**
 * Set handler for AM protocol.
 */
LCI_API
LCI_error_t LCI_PL_set_handler(LCI_Handler handler, LCI_PL* prop);

/**
 * Set allocator for dynamic protocol.
 */
LCI_API
LCI_error_t LCI_PL_set_allocator(LCI_Allocator alloc, LCI_PL* prop);

/**
 * Create an endpoint, collective calls for those involved in the endpoints.
 */
LCI_API
LCI_error_t LCI_endpoint_t_create(int device_id, LCI_PL prop, LCI_endpoint_t* ep);

/**
 * Query the rank of the current process.
 */
LCI_API
int LCI_Rank();

/**
 * Query the number of processes.
 */
LCI_API
int LCI_Size();

/* Two-sided functions. */

/**
 * Send a short message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_Sends(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep);

/**
 * Send a medium message, completed immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_Sendm(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep);

/**
 * Send a long message.
 */
LCI_API
LCI_error_t LCI_Sendl(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, void* sync_context);

/**
 * Receive a short message.
 */
LCI_API
LCI_error_t LCI_Recvs(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, LCI_one2one_t* sync);

/**
 * Receive a medium message.
 */
LCI_API
LCI_error_t LCI_Recvm(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, LCI_one2one_t* sync);

/**
 * Receive a medium message.
 */
LCI_API
LCI_error_t LCI_Recvl(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, LCI_one2one_t* sync);

/* One-sided functions. */

/**
 * Put short message to a remote address @rma_id available at the remote endpoint, offset @offset.
 * Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_Puts(void* src, size_t size, int rank, int rma_id, int offset, LCI_endpoint_t ep);

/**
 * Put medium message to a remote address @rma_id available at the remote endpoint, offset @offset.
 * Complete immediately, or return LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_Putm(void* src, size_t size, int rank, int rma_id, int offset, LCI_endpoint_t ep);

/**
 * Put long message to a remote address @rma_id available at the remote endpoint, offset @offset.
 */
LCI_API
LCI_error_t LCI_Putl(void* src, size_t size, int rank, int rma_id, int offset, LCI_endpoint_t ep, void* context);

/**
 * Put short message to a remote address, piggy-back data to completed request.
 * Complete immediately or LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_Putsd(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep);

/**
 * Put medium message to a remote address, piggy-back data to completed request.
 * Complete immediately or LCI_ERR_RETRY.
 */
LCI_API
LCI_error_t LCI_Putmd(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep);

/**
 * Put long message to a remote address, required a remote allocation.
 */
LCI_API
LCI_error_t LCI_Putld(void* src, size_t size, int rank, int tag, LCI_endpoint_t ep, void* sync_context);

/* Completion methods */

/**
 * Create a completion queue.
 */
LCI_API
LCI_error_t LCI_Cq_create(LCI_Cq* ep);

/**
 * Return first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_Cq_dequeue(LCI_Cq ep, LCI_request_t** req);

/**
 * Return at most @n first completed request in the queue.
 */
LCI_API
LCI_error_t LCI_Cq_mul_dequeue(LCI_Cq ep, int n, int* actual, LCI_request_t** req);

/**
 * Return @n requests to the runtime.
 */
LCI_API
LCI_error_t LCI_request_t_free(int n, LCI_request_t** req);

/**
 * Create a Sync object.
 */
LCI_API
LCI_error_t LCI_one2one_create(LCI_one2one_t* sync);

/**
 * Reset on a Sync object.
 */
LCI_API
LCI_error_t LCI_one2one_set_full(LCI_one2one_t* sync);

/**
 * Wait on a Sync object.
 */
LCI_API
LCI_error_t LCI_one2one_set_empty(LCI_one2one_t* sync);

/**
 * Test a Sync object, return 1 if finished.
 */
LCI_API
int LCI_one2one_wait_full(LCI_one2one_t* sync);

/**
 * Signal a Sync object.
 */
LCI_API
LCI_error_t LCI_one2one_wait_empty(LCI_one2one_t* sync);

/**
 * Polling a specific device @device_id for at least @count time.
 */
LCI_API
LCI_error_t LCI_progress(int device_id, int count);

#ifdef __cplusplus
}
#endif

#endif
