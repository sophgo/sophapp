#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

SMT_MUTEXAUTOLOCK_INIT(MD_Mutex);
static pthread_mutex_t Ai_Md_Mutex = PTHREAD_MUTEX_INITIALIZER;
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_AI_MD_CFG_S stMdCfg;
static APP_PARAM_AI_MD_CFG_S *pstMdCfg = &stMdCfg;

static volatile CVI_U32 threshold = 0;
static CVI_U32 md_fps;
static CVI_U32 md_proc;
static volatile bool bRunning = CVI_FALSE;
static volatile bool bPause = CVI_FALSE;
static pthread_t Thread_Handle;
static cviai_handle_t Ai_Handle = NULL;
static cviai_service_handle_t Ai_Service_Handle = NULL;
static cvai_object_t stObjDraw;

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_AI_MD_CFG_S *app_ipcam_Ai_MD_Param_Get(void)
{
    return pstMdCfg;
}

CVI_VOID app_ipcam_Ai_MD_Thresold_Set(CVI_U32 value)
{
    threshold = value;
}

CVI_U32 app_ipcam_Ai_MD_Thresold_Get(void)
{
    return threshold;
}

CVI_VOID app_ipcam_Ai_MD_ProcStatus_Set(CVI_BOOL flag)
{
    bRunning = flag;
}

CVI_BOOL app_ipcam_Ai_MD_ProcStatus_Get(void)
{
    return bRunning;
}

CVI_VOID app_ipcam_Ai_MD_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&Ai_Md_Mutex);
    bPause = flag;
    pthread_mutex_unlock(&Ai_Md_Mutex);
}

CVI_BOOL app_ipcam_Ai_MD_Pause_Get(void)
{
    return bPause;
}

CVI_U32 app_ipcam_Ai_MD_ProcFps_Get(void)
{
    return md_fps;
}

CVI_S32 app_ipcam_Ai_MD_ProcTime_Get(void)
{
    return md_proc;
}

static void app_ipcam_Ai_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n", 
        pstMdCfg->bEnable, pstMdCfg->VpssGrp, pstMdCfg->VpssChn, pstMdCfg->u32GrpWidth, pstMdCfg->u32GrpHeight);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "threshold=%d miniArea=%d u32BgUpPeriod=%d\n",
        pstMdCfg->threshold, pstMdCfg->miniArea, pstMdCfg->u32BgUpPeriod);

}

int app_ipcam_Ai_MD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame)
{
    CVI_S32 iTime;

    if (app_ipcam_Ai_MD_ProcStatus_Get())
    {
        SMT_MutexAutoLock(MD_Mutex, lock);
        iTime = GetCurTimeInMsec();
        CVI_AI_RescaleMetaCenterObj(pstVencFrame, &stObjDraw);
        CVI_AI_Service_ObjectDrawRect(
                            Ai_Service_Handle, 
                            &stObjDraw, 
                            pstVencFrame, 
                            CVI_FALSE, 
                            pstMdCfg->rect_brush);
        CVI_AI_Free(&stObjDraw);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "MD draw result takes %u ms \n", (GetCurTimeInMsec() - iTime));
    }

    return CVI_SUCCESS;
}


static CVI_S32 app_ipcam_Ai_Proc_Init()
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD init ------------------> start \n");

    if ((Ai_Handle != NULL) || (Ai_Service_Handle != NULL))
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Ai Proc Init but handle is not NULL!\n");
        return CVI_SUCCESS;
    }

    app_ipcam_Ai_Param_dump();

    s32Ret  = CVI_AI_CreateHandle(&Ai_Handle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_CreateHandle failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_AI_Service_CreateHandle(&Ai_Service_Handle, Ai_Handle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_Service_CreateHandle failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD init ------------------> done \n");

    return CVI_SUCCESS;
}

static CVI_VOID *Thread_MD_Proc(CVI_VOID *pArgs)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    CVI_CHAR TaskName[64] = {'\0'};
    sprintf(TaskName, "Thread_MD_Proc");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD start running!\n");

    CVI_U32 md_frame = 0;
    CVI_S32 iTime_start, iTime_proc,iTime_fps;
    float iTime_gop;
    iTime_start = GetCurTimeInMsec();

    CVI_U32 count = 0;
    CVI_U32 u32BgUpPeriod = pstMdCfg->u32BgUpPeriod;

    CVI_U32 miniArea = pstMdCfg->miniArea;
    app_ipcam_Ai_MD_Thresold_Set(pstMdCfg->threshold);
    cvai_object_t obj_meta;
    memset(&obj_meta, 0, sizeof(cvai_object_t));
    
    VIDEO_FRAME_INFO_S stVencFrame;
    memset(&stVencFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    VPSS_GRP VpssGrp = pstMdCfg->VpssGrp;
    VPSS_CHN VpssChn = pstMdCfg->VpssChn;

    while (app_ipcam_Ai_MD_ProcStatus_Get()) {
        pthread_mutex_lock(&Ai_Md_Mutex);
        s32Ret = app_ipcam_Ai_MD_Pause_Get();
        
        if (s32Ret) {
            pthread_mutex_unlock(&Ai_Md_Mutex);
            usleep(1000*1000);
            continue;
        }
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stVencFrame, 3000);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&Ai_Md_Mutex);
            usleep(100*1000);
            continue;
        }
        pthread_mutex_unlock(&Ai_Md_Mutex);
        iTime_proc = GetCurTimeInMsec();
        
        if ((count % u32BgUpPeriod) == 0)
        {
            APP_PROF_LOG_PRINT(LEVEL_TRACE, "update BG interval=%d, threshold=%d, miniArea=%d\n",
            u32BgUpPeriod, threshold, miniArea);
            // Update background. For simplicity, we just set new frame directly.
            if (CVI_AI_Set_MotionDetection_Background(Ai_Handle, &stVencFrame) != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "Cannot update background for motion detection\n");
                CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stVencFrame);
                break;
            }
        }

        // Detect moving objects. All moving objects are store in obj_meta.
        CVI_AI_MotionDetection(Ai_Handle, &stVencFrame, &obj_meta, threshold, miniArea);
        md_proc = GetCurTimeInMsec() - iTime_proc;
        APP_PROF_LOG_PRINT(LEVEL_TRACE, "MD process takes %d\n", md_proc);
        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stVencFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }

        md_frame ++;
        iTime_fps = GetCurTimeInMsec();
        iTime_gop = (float)(iTime_fps - iTime_start)/1000;
        if(iTime_gop >= 1)
        {
            md_fps = md_frame/iTime_gop;
            md_frame = 0;
            iTime_start = iTime_fps;
        }
        
        if (obj_meta.size == 0) {
            count = (count == u32BgUpPeriod) ? (1) : (count+1);
            continue;
        }
        SMT_MutexAutoLock(MD_Mutex, lock);
        if (stObjDraw.info) {
            CVI_AI_Free(&stObjDraw);
        }
        memset(&stObjDraw, 0, sizeof(cvai_object_t));
        memcpy(&stObjDraw, &obj_meta, sizeof obj_meta);
        stObjDraw.info = (cvai_object_info_t *)malloc(obj_meta.size * sizeof(cvai_object_info_t));
        memset(stObjDraw.info, 0, sizeof(cvai_object_info_t) * stObjDraw.size);
        for (uint32_t i = 0; i < obj_meta.size; i++) {
            CVI_AI_CopyObjectInfo(&obj_meta.info[i], &stObjDraw.info[i]);
            stObjDraw.info[i].vehicle_properity = NULL;
            stObjDraw.info[i].pedestrian_properity = NULL;
        }

        CVI_AI_Free(&obj_meta);

        count = (count == u32BgUpPeriod) ? (1) : (count+1);
    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_Ai_MD_ObjDrawInfo_Get(cvai_object_t *pstAiObj)
{
    _NULL_POINTER_CHECK_(pstAiObj, -1);

    SMT_MutexAutoLock(MD_Mutex, lock);

    if (stObjDraw.size == 0) {
        return CVI_SUCCESS;
    } else {
        memcpy(pstAiObj, &stObjDraw, sizeof stObjDraw);
        pstAiObj->info = (cvai_object_info_t *)malloc(stObjDraw.size * sizeof(cvai_object_info_t));
        _NULL_POINTER_CHECK_(pstAiObj->info, -1);
        memset(pstAiObj->info, 0, sizeof(cvai_object_info_t) * stObjDraw.size);
        for (CVI_U32 i = 0; i < stObjDraw.size; i++) {
            CVI_AI_CopyObjectInfo(&stObjDraw.info[i], &pstAiObj->info[i]);
            pstAiObj->info[i].vehicle_properity = NULL;
            pstAiObj->info[i].pedestrian_properity = NULL;
        }

        CVI_AI_Free(&stObjDraw);
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_MD_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pstMdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_MD_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_MD_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (Thread_Handle)
    {
        pthread_join(Thread_Handle, NULL);
        Thread_Handle = 0;
    }

    s32Ret = CVI_AI_Service_DestroyHandle(Ai_Service_Handle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_Service_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        Ai_Service_Handle = NULL;
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

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI MD Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}

int app_ipcam_Ai_MD_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pstMdCfg->bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD not enable\n");
        return CVI_SUCCESS;
    }

    if (bRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI MD has started\n");
        return CVI_SUCCESS;
    }

    s32Ret = app_ipcam_Ai_Proc_Init();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_Proc_Init failed with error %d\n", s32Ret);
        return s32Ret;
    }

    app_ipcam_Ai_MD_ProcStatus_Set(CVI_TRUE);

    // pthread_attr_t pthread_attr;
    // struct sched_param sched_param;
    // pthread_attr_init(&pthread_attr);
    // pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);
    // pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
    // pthread_attr_getschedparam(&pthread_attr, &sched_param);
    // sched_param.sched_priority = 75;
    // pthread_attr_setschedparam(&pthread_attr, &sched_param);
    // s32Ret = pthread_create(&Thread_Handle, &pthread_attr, Thread_MD_Proc, NULL);
    s32Ret = pthread_create(&Thread_Handle, NULL, Thread_MD_Proc, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Thread_Motion_Detecte failed with error %d\n", s32Ret);
        return s32Ret;
    }

    return s32Ret;
}

/*****************************************************************
 *  The following API for command test used             Front
 * **************************************************************/
CVI_S32 app_ipcam_Ai_MD_StatusGet(void)
{
    return Ai_Handle ? 1 : 0;
}

int app_ipcam_CmdTask_Ai_MD_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate)
{
    CVI_CHAR param[512] = {0};
    snprintf(param, sizeof(param), "%s", msg->payload);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "%s param:%s arg2=%d\n", __FUNCTION__, param, msg->arg2);

    CVI_CHAR *temp = strtok(param, ":");
    while (NULL != temp)
    {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "%s switch case -> %c \n", __FUNCTION__, *temp);
        switch (*temp)
        {
            case 's': 
            {
                temp = strtok(NULL, "/");
                CVI_BOOL isEnable = (CVI_BOOL)atoi(temp);
                if (isEnable)
                {
                    app_ipcam_Ai_MD_Start();
                }
                else
                {
                    app_ipcam_Ai_MD_Stop();
                }
                break;
            }
            default:
                return 0;
                break;
        }
        
        temp = strtok(NULL, ":");
    }

    return CVI_SUCCESS;
}

/*****************************************************************
 *  The above API for command test used                 End
 * **************************************************************/
