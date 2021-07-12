#include "lcii.h"

#define LCII_SYNC_FULL 1
#define LCII_SYNC_EMPTY 0

struct LCII_sync_t {
  volatile int count;
  volatile int confirm;
  int threshold;
  LCII_context_t **ctx;
};

LCI_error_t LCI_sync_create(LCI_device_t device, int threshold,
                            LCI_comp_t* completion)
{
  // we don't need device for this simple synchronizer
  (void) device;
  LCII_sync_t *sync = LCIU_malloc(sizeof(LCII_sync_t));
  sync->threshold = threshold;
  sync->count = 0;
  sync->confirm = 0;
  sync->ctx = LCIU_malloc(sizeof(LCII_context_t*) * sync->threshold);
  *completion = sync;
  return LCI_OK;
}

LCI_error_t LCI_sync_free(LCI_comp_t *completion) {
  LCII_sync_t *sync = *completion;
  LCIU_free(sync->ctx);
  LCIU_free(sync);
  *completion = NULL;
  return LCI_OK;
}

LCI_error_t LCII_sync_signal(LCI_comp_t completion, LCII_context_t* ctx)
{
  LCII_sync_t *sync = completion;
  int pos = 0;
  if (sync->threshold > 1)
    pos = __sync_fetch_and_add(&sync->count, 1);
  LCM_DBG_Assert(pos < sync->threshold, "Receive more signals than expected\n");
  sync->ctx[pos] = ctx;
  if (sync->threshold > 1)
    __sync_fetch_and_add(&sync->confirm, 1);
  else
    sync->confirm = 1;
  return LCI_OK;
}

LCI_error_t LCI_sync_signal(LCI_comp_t completion, LCI_request_t request)
{
  LCII_context_t *ctx = LCIU_malloc(sizeof(LCII_context_t));
  ctx->rank = request.rank;
  ctx->tag = request.tag;
  ctx->data_type = request.type;
  ctx->data = request.data;
  ctx->user_context = request.user_context;

  LCII_sync_t *sync = completion;
  int pos = 0;
  if (sync->threshold > 1)
    pos = __sync_fetch_and_add(&sync->count, 1);
  LCM_DBG_Assert(pos < sync->threshold, "Receive more signals than expected\n");
  sync->ctx[pos] = ctx;
  if (sync->threshold > 1)
    __sync_fetch_and_add(&sync->confirm, 1);
  else
    sync->confirm = 1;
  return LCI_OK;
}

LCI_error_t LCI_sync_wait(LCI_comp_t completion, LCI_request_t request[])
{
  LCII_sync_t *sync = completion;
  while (sync->confirm < sync->threshold) continue;
  if (request)
    for (int i = 0; i < sync->threshold; ++i) {
      request[i] = LCII_ctx2req(sync->ctx[i]);
    }
  else
    for (int i = 0; i < sync->threshold; ++i) {
      LCIU_free(sync->ctx[i]);
    }
  sync->confirm = 0;
  if (sync->threshold > 1)
    sync->count = 0;
  return LCI_OK;
}

LCI_error_t LCI_sync_test(LCI_comp_t completion, LCI_request_t request[])
{
  LCII_sync_t *sync = completion;
  if (sync->confirm < sync->threshold) {
    return LCI_ERR_RETRY;
  } else {
    if (request)
      for (int i = 0; i < sync->threshold; ++i) {
        request[i] = LCII_ctx2req(sync->ctx[i]);
      }
    else
      for (int i = 0; i < sync->threshold; ++i) {
        LCIU_free(sync->ctx[i]);
      }
    sync->confirm = 0;
    if (sync->threshold > 1)
      sync->count = 0;
    return LCI_OK;
  }
}
