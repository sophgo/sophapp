#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"
#include "app_ipcam_audio.h"
#include "app_ipcam_comm.h"
#include "cvi_audio.h"
#include "cvi_audio_aac_adp.h"
#include "acodec.h"
#include "app_ipcam_gpio.h"
#include "app_ipcam_rtsp.h"
#include "app_ipcam_ll.h"
static APP_PARAM_AI_CRY_CFG_S stCryCfg;
static APP_PARAM_AI_CRY_CFG_S *pstCryCfg = &stCryCfg;
static volatile bool bRunning = CVI_FALSE;
static volatile bool bPause = CVI_FALSE;
static pthread_t Thread_Handle;
static cviai_handle_t Ai_Handle = NULL;
static const char *enumStr[] = {"no_baby_cry", "baby_cry"};
#define SECOND 3
#define  CVI_AUDIO_BLOCK_MODE -1
APP_PARAM_AI_CRY_CFG_S *app_ipcam_Ai_Cry_Param_Get(void)
{
    return pstCryCfg;
}

CVI_VOID app_ipcam_Ai_Cry_ProcStatus_Set(CVI_BOOL flag)
{
    bRunning = flag;
}

CVI_BOOL app_ipcam_Ai_Cry_ProcStatus_Get(void)
{
    return bRunning;
}

CVI_VOID app_ipcam_Ai_Cry_Pause_Set(CVI_BOOL flag)
{
    bPause = flag;
}

CVI_BOOL app_ipcam_Ai_Cry_Pause_Get(void)
{
    return bPause;
}

static CVI_VOID *Thread_Cry_PROC(CVI_VOID *pArgs)
{
    APP_PARAM_AUDIO_CFG_T *pstAudioCfg = app_ipcam_Audio_Param_Get();
    AUDIO_SAMPLE_RATE_E u32SampleRate = pstAudioCfg->astAudioCfg.enSamplerate;
    CVI_U32 u32NumPerFrm = pstAudioCfg->astAudioCfg.u32PtNumPerFrm;

    CVI_CHAR TaskName[64] = {'\0'};
    sprintf(TaskName, "Thread_Cry_Proc");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry start running!\n");
    CVI_S32 s32Ret;
    AUDIO_FRAME_S stFrame;
    AEC_FRAME_S stAecFrm;
    CVI_U32 i;
    CVI_U32 u32Loop = u32SampleRate / u32NumPerFrm * SECOND;  // 3 seconds
    CVI_U32 u32FrameSize = u32NumPerFrm * (CVI_U32)(pstAudioCfg->astAudioCfg.enBitwidth + 1);       // PCM_FORMAT_S16_LE (2bytes)
    CVI_U32 u32BufferSize = u32SampleRate * (CVI_U32)(pstAudioCfg->astAudioCfg.enBitwidth + 1) * SECOND;
    // classify the sound result
    int index = -1;
    CVI_U32 maxval_sound = 0;
    // Set video frame interface
    CVI_U8 buffer[u32BufferSize];  // 3 seconds
    memset(buffer, 0, u32BufferSize);
    VIDEO_FRAME_INFO_S Frame;
    Frame.stVFrame.pu8VirAddr[0] = buffer;  // Global buffer
    Frame.stVFrame.u32Height = 1;
    Frame.stVFrame.u32Width = u32BufferSize;


    while (app_ipcam_Ai_Cry_ProcStatus_Get()) {
        if (app_ipcam_Ai_Cry_Pause_Get()) {
            usleep(1000*1000);
            continue;
        }
        for (i = 0; i < u32Loop; ++i) {
            s32Ret = CVI_AI_GetFrame(0, 0, &stFrame, &stAecFrm, CVI_AUDIO_BLOCK_MODE);  // Get audio frame
            if (s32Ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_CreateHandle has Failed\n");
                continue;
            } else {
                memcpy(buffer + i * u32FrameSize, (CVI_U8 *)stFrame.u64VirAddr[0],
                       u32FrameSize);  // Set the period size date to global buffer
            }
        }
        CVI_U32 *psound = (CVI_U32 *)buffer;
        float meanval = 0;
        for (i = 0; i < u32SampleRate * SECOND; i++) {
            meanval += psound[i];
            if (psound[i] > maxval_sound) {
                maxval_sound = psound[i];
            }
        }
        printf("maxvalsound:%d,meanv:%f\n", maxval_sound, meanval / (u32SampleRate * SECOND));
        s32Ret = CVI_AI_SoundClassification_V2(Ai_Handle, &Frame, &index);  // Detect the audio
        if (s32Ret == CVI_SUCCESS) {
            printf("esc class: %s\n", enumStr[index]);
        }
    }
    s32Ret = CVI_AI_ReleaseFrame(0, 0, &stFrame, &stAecFrm);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_ReleaseFrame Failed!!\n");
    }
    pthread_exit(NULL);

    return NULL;
}

static CVI_S32 app_ipcam_Ai_Cry_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry init ------------------> start \n");
    
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_AI_SUPPORTED_MODEL_E model_id = pstCryCfg->model_id;

    if (Ai_Handle == NULL)
    {
        s32Ret = CVI_AI_CreateHandle(&Ai_Handle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    } 
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_CreateHandle has created\n");
        return s32Ret;
    }
    
    s32Ret = CVI_AI_SetPerfEvalInterval(Ai_Handle, CVI_AI_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, 10); 
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_SetPerfEvalInterval failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_AI_OpenModel(Ai_Handle, model_id, pstCryCfg->model_path);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_SetModelPath failed with %#x! maybe reset model path\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry init ------------------> done \n");

    return CVI_SUCCESS;
}

int app_ipcam_Ai_Cry_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pstCryCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_Cry_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_Cry_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (Thread_Handle)
    {
        pthread_join(Thread_Handle, NULL);
        Thread_Handle = 0;
    }

    s32Ret = CVI_AI_DestroyHandle(Ai_Handle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        Ai_Handle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI Cry Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_Ai_Cry_Start(void){
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pstCryCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry not enable\n");
        return CVI_SUCCESS;
    }

    if (bRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI Cry has started\n");
        return CVI_SUCCESS;
    }

    app_ipcam_Ai_Cry_Proc_Init();

    app_ipcam_Ai_Cry_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&Thread_Handle, NULL, Thread_Cry_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI Cry_pthread_create failed!\n");
        return s32Ret;
    }

    return s32Ret;
}

CVI_S32 app_ipcam_Ai_Cry_StatusGet(void)
{
    return Ai_Handle ? 1 : 0;
}