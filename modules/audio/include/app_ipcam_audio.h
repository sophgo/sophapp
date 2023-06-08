#ifndef __APP_IPCAM_AUDIO_H__
#define __APP_IPCAM_AUDIO_H__

#include "app_ipcam_mq.h"
#include "cvi_type.h"
#include "cvi_comm_aio.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct APP_AUDIO_CFG_T
{
    AUDIO_SAMPLE_RATE_E enSamplerate;
    AUDIO_SAMPLE_RATE_E enReSamplerate;
    AUDIO_BIT_WIDTH_E   enBitwidth;
    AIO_MODE_E          enWorkmode;
    AUDIO_SOUND_MODE_E  enSoundmode;
    CVI_BOOL            Cal_DB_Enable;
    CVI_U32             u32EXFlag;
    CVI_U32             u32FrmNum;
    CVI_U32             u32PtNumPerFrm;
    CVI_U32             u32ChnCnt;
    CVI_U32             u32ClkSel;
    AIO_I2STYPE_E       enI2sType;
    CVI_U32             u32AiDevId;
    CVI_U32             u32AiChn;
    CVI_U32             u32AoDevId;
    CVI_U32             u32AoChn;
    CVI_U32             u32AeDevId;
    CVI_U32             u32AeChn;
    PAYLOAD_TYPE_E      enAencType;
    CVI_U32             u32AdChn;
} APP_AUDIO_CFG_S;

typedef struct APP_AUDIO_VQE_S
{
    CVI_BOOL            bAiAgcEnable;
    AUDIO_AGC_CONFIG_S  mAiAgcCfg;
    CVI_BOOL            bAiAnrEnable;
    AUDIO_ANR_CONFIG_S  mAiAnrCfg;
    CVI_BOOL            bAiAecEnable;
    AI_AEC_CONFIG_S     mAiAecCfg;
    CVI_BOOL            bAoAgcEnable;
    AUDIO_AGC_CONFIG_S  mAoAgcCfg;
    CVI_BOOL            bAoAnrEnable;
    AUDIO_ANR_CONFIG_S  mAoAnrCfg;
} APP_AUDIO_VQE_S;

typedef struct APP_AUDIO_VOL_S
{
    int iDacLVol;
    int iDacRVol;
    int iAdcLVol;
    int iAdcRVol;
} APP_AUDIO_VOL_T;

typedef struct APP_AUDIO_INTERCOM_S
{
    bool bEnable;
    int iPort;
    char cIp[16];
} APP_AUDIO_INTERCOM_T;

typedef struct APP_PARAM_AUDIO_CFG_S
{
    bool bInit;
    APP_AUDIO_INTERCOM_T astAudioIntercom;
    APP_AUDIO_VOL_T astAudioVol;
    APP_AUDIO_CFG_S astAudioCfg;
    APP_AUDIO_VQE_S astAudioVqe;
} APP_PARAM_AUDIO_CFG_T;

typedef struct APP_AUDIO_RUNSTATUS_S
{
    bool bAiInit;
    bool bAoInit;
    bool bAencInit;
    bool bAdecInit;
} APP_AUDIO_RUNSTATUS_T;

typedef enum AUDIO_NEED_STOP_MODULE_T
{
    AUDIO_NEED_STOP_NULL   = 0x00,
    AUDIO_NEED_STOP_AI     = 0x01,
    AUDIO_NEED_STOP_AO     = 0x02,
    AUDIO_NEED_STOP_AENC   = 0x03,
    AUDIO_NEED_STOP_ADEC   = 0x04,
    AUDIO_NEED_STOP_ALL    = 0xFF
} AUDIO_NEED_STOP_MODULE_E;

typedef enum AUDIO_VQE_FUNCTION_T
{
    AUDIO_VQE_FUNCTION_AGC    = 0x00,
    AUDIO_VQE_FUNCTION_ANR    = 0x01,
    AUDIO_VQE_FUNCTION_AEC    = 0x02,
} AUDIO_VQE_FUNCTION_E;

typedef enum AUDIO_AO_PLAY_TYPE_T
{
    AUDIO_AO_PLAY_TYPE_RAW    = 0x00,
    AUDIO_AO_PLAY_TYPE_AENC   = 0x01,
    AUDIO_AO_PLAY_TYPE_MP3    = 0x02,
    AUDIO_AO_PLAY_TYPE_INTERCOM = 0x03,
} AUDIO_AO_PLAY_TYPE_E;

typedef struct APP_AUDIO_RECORD_S
{
    int iStatus;
    char cAencFileName[64];
} APP_AUDIO_RECORD_T;

APP_PARAM_AUDIO_CFG_T *app_ipcam_Audio_Param_Get(void);

int app_ipcam_Audio_Init(void);
int app_ipcam_Audio_UnInit(void);
int app_ipcam_Audio_AoPlay(char *pAudioFile, AUDIO_AO_PLAY_TYPE_E eAoType);
int app_ipcam_CmdTask_AudioAttr_Set(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_CmdTask_Mp3_Play(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_Audio_AudioReset(APP_PARAM_AUDIO_CFG_T *pstAudioCfg);
int app_ipcam_Audio_SetRecordStatus(APP_AUDIO_RECORD_T *pstAudioRecord);
int app_ipcam_Audio_SetPlayStatus(APP_AUDIO_RECORD_T *pstAudioPlay);

#ifdef __cplusplus
}
#endif

#endif
