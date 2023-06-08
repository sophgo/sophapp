#ifndef __APP_IPCAM_DUMP_H__
#define __APP_IPCAM_DUMP_H__

#include "cvi_type.h"
#include "linux/cvi_common.h"
#include "linux/cvi_comm_video.h"

#ifdef __cplusplus
extern "C"
{
#endif

int app_ipcam_Vi_Yuv_Dump(void);
int app_ipcam_VpssChn_Yuv_Dump(VENC_CHN VencChn, VIDEO_FRAME_INFO_S *pFrame);

#ifdef __cplusplus
}
#endif

#endif