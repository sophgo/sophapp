#ifndef __APP_IPCAM_RECORD_H__
#define __APP_IPCAM_RECORD_H__

#include <cvi_comm_aio.h>
#include <linux/cvi_comm_video.h>
#include "cvi_comm_vb.h"
#include <linux/cvi_comm_venc.h>

int app_ipcam_Record_Recover_Init();
int app_ipcam_Record_Init();
int app_ipcam_Record_UnInit();
int app_ipcam_Record_VideoInput(int enType, VENC_STREAM_S * pstStream);
int app_ipcam_Record_AudioInput(AUDIO_FRAME_S * pstStream);


#endif
