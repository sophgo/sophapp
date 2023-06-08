#ifndef __CVI_MQ_H__
#define __CVI_MQ_H__

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "app_ipcam_os.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef EINTR
#define    EINTR         4    /* Interrupted system call */
#endif

#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(expression) \
   (__extension__                                  \
     ({ long int __result;                             \
        do __result = (long int) (expression);                     \
        while (__result == -1L && errno == EINTR);                 \
        __result; }))
#endif

#define CVI_MQ_SUCCESS             ((int)(0))
#define CVI_MQ_ERR_FAILURE         ((int)(-1001))
#define CVI_MQ_ERR_NOMEM           ((int)(-1002))
#define CVI_MQ_ERR_TIMEOUT         ((int)(-1003))
#define CVI_MQ_ERR_AGAIN           ((int)(-1004))

// Predefined CLIENT ID for inter-process MQ
#define CVI_MQ_CLIENT_ID_INVALID   (0)
#define CVI_MQ_CLIENT_ID_SCS       (1)
#define CVI_MQ_CLIENT_ID_CLI       (50)
#define CVI_MQ_CLIENT_ID_SVC_0     (100)
#define CVI_MQ_CLIENT_ID_SVC_1     (101)
#define CVI_MQ_CLIENT_ID_SVC_2     (102)
#define CVI_MQ_CLIENT_ID_SVC_3     (103)
#define CVI_MQ_CLIENT_ID_APP_0     (200)
#define CVI_MQ_CLIENT_ID_APP_1     (201)
#define CVI_MQ_CLIENT_ID_APP_2     (202)
#define CVI_MQ_CLIENT_ID_APP_3     (203)
#define CVI_MQ_CLIENT_ID_USER_0    (300)
#define CVI_MQ_CLIENT_ID_USER_1    (301)
#define CVI_MQ_CLIENT_ID_USER_2    (302)
#define CVI_MQ_CLIENT_ID_USER_3    (303)
#define CVI_MQ_CLIENT_ID_MAX       (0xffff)

typedef uint32_t CVI_MQ_ID_t;
#define CVI_MQ_ID(client, channel)     (((client) << 16) | (channel))
#define CVI_MQ_ID_GET_CLIENT(id)       (((id) >> 16) & 0xFFFF)
#define CVI_MQ_ID_GET_CHANNEL(id)      ((id) & 0xFFFF)

#define CVI_MQ_MSG_PAYLOAD_LEN     (512)
#define CVI_MQ_QUEUE_SIZE          (32)

typedef struct MSG_ACK_s {
    char  ackmsg[CVI_MQ_MSG_PAYLOAD_LEN];
    int32_t result_len;
    int32_t status;
} __attribute__((__packed__)) MSG_ACK_t;

typedef struct CVI_MQ_MSG_s {
    CVI_MQ_ID_t target_id;
    int32_t arg1;
    int32_t arg2;
    int16_t seq_no;
    int16_t len;
    uint64_t crete_time;
    uint32_t needack;
    int16_t client_id;
    char payload[CVI_MQ_MSG_PAYLOAD_LEN];
} __attribute__((__packed__)) CVI_MQ_MSG_t;

#define CVI_MQ_MSG_HEADER_LEN      (offsetof(struct CVI_MQ_MSG_s, payload))

struct CVI_MQ_ENDPOINT_s;
typedef struct CVI_MQ_ENDPOINT_s CVI_MQ_ENDPOINT_t;
typedef struct CVI_MQ_ENDPOINT_s *CVI_MQ_ENDPOINT_HANDLE_t;

typedef int (*CVI_MQ_RECV_CB_t)(
    CVI_MQ_ENDPOINT_HANDLE_t      ep,
    CVI_MQ_MSG_t                  *msg,
    void                          *ep_arg);

typedef struct {
    const char                    *name;
    CVI_MQ_ID_t                    id;
    CVI_MQ_RECV_CB_t               recv_cb;
    void                          *recv_cb_arg;
} CVI_MQ_ENDPOINT_CONFIG_t;


int app_ipcam_MqEndpoint_Create(
    CVI_MQ_ENDPOINT_CONFIG_t      *config,
    CVI_MQ_ENDPOINT_HANDLE_t      *ep);

int app_ipcam_MqEndpoint_Destroy(
    CVI_MQ_ENDPOINT_HANDLE_t       ep);

int CVI_MQ_Send_RAW(
    CVI_MQ_MSG_t                  *msg);

int CVI_MQ_Send(
    CVI_MQ_ID_t                    target_id,
    int32_t                        arg1,
    int32_t                        arg2,
    int16_t                        seq_no,
    char                          *payload,
    int16_t                        payload_len);

int CVI_MQ_Send_Ack(CVI_MQ_ENDPOINT_HANDLE_t ep,
    MSG_ACK_t   *ack_msg, int16_t client_id);
int CVI_MQ_Send_NeedAck(
    CVI_MQ_ID_t                    target_id,
    int32_t                        arg1,
    int32_t                        arg2,
    int16_t                        seq_no,
    char                          *payload,
    int16_t                        payload_len,
    MSG_ACK_t                      *ack_msg,
    int16_t                         client_id,
    int32_t                         timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
