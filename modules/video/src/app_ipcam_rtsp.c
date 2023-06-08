#include "app_ipcam_rtsp.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_venc.h"

#ifdef AUDIO_SUPPORT
#include "app_ipcam_audio.h"
#endif

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
APP_PARAM_RTSP_T stRtspCtx;
APP_PARAM_RTSP_T *pstRtspCtx = &stRtspCtx;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
APP_PARAM_RTSP_T *app_ipcam_Rtsp_Param_Get(void)
{
    return pstRtspCtx;
}

static int app_ipcam_RtspAttr_Init(void)
{
    /* update vidoe streaming codec type from video attr */
    for (CVI_S32 i = 0; i < pstRtspCtx->session_cnt; i++) {
        APP_VENC_CHN_CFG_S *pstVencChnCfg = app_ipcam_VencChnCfg_Get(pstRtspCtx->VencChn[i]);
        PAYLOAD_TYPE_E enType = pstVencChnCfg->enType;
        APP_RTSP_VCODEC_CHK(enType, pstRtspCtx->SessionAttr[i].video.codec);
#ifdef AUDIO_SUPPORT
        APP_PARAM_AUDIO_CFG_T *pstAudioCfg = app_ipcam_Audio_Param_Get();
        if (pstAudioCfg->bInit) {
            pstRtspCtx->SessionAttr[i].audio.codec = RTSP_AUDIO_PCM_L16;
            pstRtspCtx->SessionAttr[i].audio.sampleRate = pstAudioCfg->astAudioCfg.enSamplerate;
        }
#endif
        APP_PROF_LOG_PRINT(LEVEL_INFO, "VencChn_%d attach to Session_%d with CodecType=%d\n",
                pstRtspCtx->VencChn[i], i, pstRtspCtx->SessionAttr[i].video.codec);
    }

    return CVI_SUCCESS;
}

static void app_ipcam_Rtsp_Connect(const char *ip, CVI_VOID *arg)
{

    // RTSP_SERVICE_CONTEXT_T *ctx = (RTSP_SERVICE_CONTEXT_T*)arg;
    // CVI_RTSP_SERVICE_PARAM_T *p = &ctx->param;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "rtsp client connected: %s\n", ip);
}

static void app_ipcam_Rtsp_Disconnect(const char *ip, CVI_VOID *arg)
{
    // RTSP_SERVICE_CONTEXT_T *ctx = (RTSP_SERVICE_CONTEXT_T*)arg;
    // CVI_RTSP_SERVICE_PARAM_T *p = &ctx->param;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "rtsp client disconnected: %s\n", ip);
}

int app_ipcam_rtsp_Server_Destroy(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (pstRtspCtx->pstServerCtx == NULL) {
        printf("rtsp server has not been create\n");
        return s32Ret;
    }

    CVI_RTSP_Stop(pstRtspCtx->pstServerCtx);

    pthread_mutex_lock(&pstRtspCtx->RsRtspMutex);
    for (CVI_S32 i = 0; i < pstRtspCtx->session_cnt; i++) {

        if (pstRtspCtx->bStart[i]) {
            CVI_RTSP_DestroySession(pstRtspCtx->pstServerCtx, pstRtspCtx->pstSession[i]);
            pstRtspCtx->bStart[i] = CVI_FALSE;
        }
    }
    pthread_mutex_unlock(&pstRtspCtx->RsRtspMutex);

    CVI_RTSP_Destroy(&pstRtspCtx->pstServerCtx);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "rtsp server destroyed\n");
    pthread_mutex_destroy(&pstRtspCtx->RsRtspMutex);

    pstRtspCtx->pstServerCtx = NULL;

    return 0;
}

int app_ipcam_Rtsp_Server_Create(CVI_VOID)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    app_ipcam_RtspAttr_Init();

    CVI_RTSP_CONFIG config = {0};
    config.port = pstRtspCtx->port;

    if (pstRtspCtx->pstServerCtx != NULL) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "rtsp server has been created\n");
        return s32Ret;
    }

    s32Ret = CVI_RTSP_Create(&pstRtspCtx->pstServerCtx, &config);
    if (s32Ret < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "fail to create rtsp\n");
        return s32Ret;
    }

    pthread_mutex_init(&pstRtspCtx->RsRtspMutex,NULL);
    s32Ret = CVI_RTSP_Start(pstRtspCtx->pstServerCtx);
    if (s32Ret < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "fail to rtsp start\n");
        goto error;
    }

    pthread_mutex_lock(&pstRtspCtx->RsRtspMutex);
    for (CVI_S32 i = 0; i < pstRtspCtx->session_cnt; i++) {
        snprintf(pstRtspCtx->SessionAttr[i].name, sizeof(pstRtspCtx->SessionAttr[i].name), "live%d", i);
        pstRtspCtx->SessionAttr[i].reuseFirstSource = 1;
        CVI_RTSP_CreateSession(pstRtspCtx->pstServerCtx, &pstRtspCtx->SessionAttr[i],
            &pstRtspCtx->pstSession[i]);

        pstRtspCtx->bStart[i] = CVI_TRUE;
        APP_PROF_LOG_PRINT(LEVEL_INFO, "======rtsp start [VencChn%d  %s]  ======\n",
            pstRtspCtx->VencChn[i], pstRtspCtx->SessionAttr[i].name);
    }
    // set listener
    pstRtspCtx->listener.onConnect = app_ipcam_Rtsp_Connect;
    pstRtspCtx->listener.argConn = pstRtspCtx->pstServerCtx;
    pstRtspCtx->listener.onDisconnect = app_ipcam_Rtsp_Disconnect;
    CVI_RTSP_SetListener(pstRtspCtx->pstServerCtx, &pstRtspCtx->listener);
    pthread_mutex_unlock(&pstRtspCtx->RsRtspMutex);

    return s32Ret;

error:
    app_ipcam_rtsp_Server_Destroy();
    
    return s32Ret;
}
