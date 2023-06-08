#ifndef __APP_IPCAM_WEBSOCKET_H__
#define __APP_IPCAM_WEBSOCKET_H__

#include <stdio.h>
#include <stdint.h>

#include "cvi_venc.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define WRAPPER_IMAGE       (0)
#define WRAPPER_AIFPS       (1)

int app_ipcam_WebSocket_Init();
int app_ipcam_WebSocket_DeInit();
int app_ipcam_WebSocketChn_Set(int chn);
int app_ipcam_WebSocketChn_Get();
int app_ipcam_WebSocket_Stream_Send(void *pData, void *pArgs);
int app_ipcam_WebSocket_AiFps_Send(void);

#ifdef __cplusplus
}
#endif

#endif	/* __CVI_WEBSOCKET_H__ */