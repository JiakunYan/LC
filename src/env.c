#include <limits.h>
#include "lcii.h"

LCI_API int LCI_NUM_PROCESSES;
LCI_API int LCI_RANK;
LCI_API int LCI_MAX_ENDPOINTS;
LCI_API int LCI_MAX_TAG = (1u << 15) - 1;
LCI_API int LCI_MEDIUM_SIZE = LCI_PACKET_SIZE - sizeof(struct packet_context);
LCI_API int LCI_IOVEC_SIZE = LCIU_MIN(
    (LCI_PACKET_SIZE - sizeof(struct packet_context) - sizeof(struct packet_rts)) / sizeof(size_t),
    (LCI_PACKET_SIZE - sizeof(struct packet_context) - sizeof(struct packet_rtr)) / sizeof(struct packet_rtr_iovec_info));
LCI_API int LCI_REGISTERED_SEGMENT_SIZE;
LCI_API int LCI_MAX_REGISTERED_SEGMENT_SIZE = INT_MAX;
LCI_API int LCI_MAX_REGISTERED_SEGMENT_NUMBER = 1;
LCI_API int LCI_DEFAULT_TABLE_LENGTH = 1u << TBL_BIT_SIZE;
LCI_API int LCI_MAX_TABLE_LENGTH = INT32_MAX;
LCI_API int LCI_DEFAULT_QUEUE_LENGTH = LCI_SERVER_MAX_PKTS * 2;
LCI_API int LCI_MAX_QUEUE_LENGTH = LCI_SERVER_MAX_PKTS * 2;
LCI_API int LCI_MAX_SYNC_LENGTH = INT_MAX;
LCI_API int LCI_PACKET_RETURN_THRESHOLD;
LCI_API int LCI_USE_DREG;
LCI_API LCI_device_t LCI_UR_DEVICE;
LCI_API LCI_endpoint_t LCI_UR_ENDPOINT;
LCI_API LCI_comp_t LCI_UR_CQ;

static inline int getenv_or(char* env, int def) {
  char* val = getenv(env);
  if (val != NULL) {
    return atoi(val);
  } else {
    return def;
  }
}

void lc_env_init(int num_proc, int rank)
{
  LCI_MAX_ENDPOINTS = getenv_or("LCI_MAX_ENDPOINTS", 8);
  LCI_NUM_PROCESSES = num_proc;
  LCI_RANK = rank;
  LCI_REGISTERED_SEGMENT_SIZE = getenv_or("LCI_REGISTERED_SEGMENT_SIZE", LCI_DEV_MEM_SIZE);
  LCI_ENDPOINTS = LCIU_calloc(sizeof(LCI_endpoint_t), LCI_MAX_ENDPOINTS);

  LCI_PACKET_RETURN_THRESHOLD = getenv_or("LCI_PACKET_RETURN_THRESHOLD", 1024);
  LCI_USE_DREG = getenv_or("LCI_USE_DREG", 0);
  if (LCI_USE_DREG == 1) {
    LCM_Log(LCM_LOG_WARN, "The registration cache is enabled. The program might "
            "be buggy when the allocation/free is too dynamic\n");
  }
}