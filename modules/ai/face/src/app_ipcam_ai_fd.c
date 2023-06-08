#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <dirent.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"

// Ai Model info
/*****************************************************************
 * Model Func : Face detection
 * Model ID   : CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PERSON_VEHICLE
 * Inference  : CVI_AI_MobileDetV2_Person_Vehicle
 * Model file : mobiledetv2-person-vehicle-ls-768.cvimodel
 *              mobiledetv2-person-vehicle-ls.cvimodel
 *================================================================
 * Model Func : Face recognition
 * Model ID   : CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_AI_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel
 *              mobiledetv2-pedestrian-d1-ls.cvimodel
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel
 ****************************************************************
 * Model Func : Mask Face detection
 * Model ID   : CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_AI_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel
 *              mobiledetv2-pedestrian-d1-ls.cvimodel
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel
 ****************************************************************
  * Model Func : Face capture,
 * Model ID   : CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN_D0
 * Inference  : CVI_AI_MobileDetV2_Pedestrian_D0
 * Model file : mobiledetv2-pedestrian-d0-ls-384.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-640.cvimodel
 *              mobiledetv2-pedestrian-d0-ls-768.cvimodel
 *              mobiledetv2-pedestrian-d1-ls.cvimodel
 *              mobiledetv2-pedestrian-d1-ls-1024.cvimodel
 ****************************************************************/


/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/

SMT_MUTEXAUTOLOCK_INIT(FD_Mutex);
static pthread_mutex_t Ai_Fd_Mutex = PTHREAD_MUTEX_INITIALIZER;
/**************************************************************************
 *                           C O N S T A N T S                            *
 **************************************************************************/

/**************************************************************************
 *                          D A T A    T Y P E S                          *
 **************************************************************************/

/**************************************************************************
 *                         G L O B A L    D A T A                         *
 **************************************************************************/
static APP_PARAM_AI_FD_CFG_S stFdCfg;

static APP_PARAM_AI_FD_CFG_S *pstFdCfg = &stFdCfg;

static volatile bool bRunning = CVI_FALSE;
static volatile bool bPause = CVI_FALSE;
static pthread_t Thread_Fd_Handle;
static cviai_handle_t Ai_Fd_Handle = NULL;
static cviai_service_handle_t Ai_Fd_Service_Handle = NULL;
static cvai_face_t stFdjDraw;


#define IMAGE_DIR "/mnt/sd/picture/"
struct LinkList
{
    int index;
    char name[128];
    struct LinkList *next;
};
static CVI_U32 fd_fps;
static CVI_U32 fd_proc;
static CVI_U32 face_cnt = 0;
static struct LinkList *head;
static struct LinkList *tail;
static IVE_HANDLE g_stIveHandle = NULL;
static cvai_service_feature_array_t featureArray;


/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/

APP_PARAM_AI_FD_CFG_S *app_ipcam_Ai_FD_Param_Get(void)
{
    return pstFdCfg;
}

CVI_VOID app_ipcam_Ai_FD_ProcStatus_Set(CVI_BOOL flag)
{
    bRunning = flag;
}

CVI_BOOL app_ipcam_Ai_FD_ProcStatus_Get(void)
{
    return bRunning;
}

CVI_VOID app_ipcam_Ai_FD_Pause_Set(CVI_BOOL flag)
{
    pthread_mutex_lock(&Ai_Fd_Mutex);
    bPause = flag;
    pthread_mutex_unlock(&Ai_Fd_Mutex);
}

CVI_BOOL app_ipcam_Ai_FD_Pause_Get(void)
{
    return bPause;
}

CVI_U32 app_ipcam_Ai_FD_ProcFps_Get(void)
{
    return fd_fps;
}

CVI_S32 app_ipcam_Ai_FD_ProcTime_Get(void)
{
    return fd_proc;
}

static void app_ipcam_Ai_Fd_Param_dump(void)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "FD_bEnable=%d FR_bEnable=%d MASK_bEnable=%d FACE_AE_bEnable=%d Grp=%d Chn=%d GrpW=%d GrpH=%d\n", 
        pstFdCfg->FD_bEnable, pstFdCfg->FR_bEnable,pstFdCfg->MASK_bEnable, pstFdCfg->FACE_AE_bEnable,\
                pstFdCfg->VpssGrp, pstFdCfg->VpssChn, pstFdCfg->u32GrpWidth, pstFdCfg->u32GrpHeight);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "model_w=%d model_h=%d bSkip=%d FdPoolId=%d threshold_fd=%f threshold_fr=%f  threshold_mask=%f \n",
        pstFdCfg->model_size_w, pstFdCfg->model_size_h, pstFdCfg->bVpssPreProcSkip, pstFdCfg->FdPoolId,\
        pstFdCfg->threshold_fd,pstFdCfg->threshold_fr,pstFdCfg->threshold_mask);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " model_id_fd=%d model_path_fd=%s model_id_fr=%d model_path_fr=%s model_id_mask=%d model_path_mask=%s\n",
                    pstFdCfg->model_id_fd, pstFdCfg->model_path_fd,\
                    pstFdCfg->model_id_fr, pstFdCfg->model_path_fr,\
                    pstFdCfg->model_id_mask, pstFdCfg->model_path_mask);
    APP_PROF_LOG_PRINT(LEVEL_INFO, " color r=%f g=%f b=%f size=%d\n",
        pstFdCfg->rect_brush.color.r, pstFdCfg->rect_brush.color.g, \
        pstFdCfg->rect_brush.color.b, pstFdCfg->rect_brush.size);
}


static void App_Clean_List(struct LinkList *List_Head)
{
    struct LinkList *pHead = NULL;
    struct LinkList *p = NULL;

    if (List_Head == NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Pkg Invalid input!\n");
        return ;
    }

    pHead = List_Head;
    while (pHead != NULL)
    {
        p = pHead->next;
        free(pHead);
        pHead = NULL;
        pHead = p;
    }
    List_Head = NULL;
    return ;
}

static CVI_S32 app_ipcam_Ai_FR_Init(void)
{
   /* face_name list */
    CVI_S32 ret = CVI_SUCCESS;
    head= (struct LinkList *)malloc(sizeof(struct LinkList));
    head->next = NULL;
    tail = head;
    /* face  Facial Feature definition */
    featureArray.data_num = 1000;
    featureArray.feature_length = 256;
    featureArray.ptr = (int8_t *)malloc(featureArray.data_num * featureArray.feature_length);
    featureArray.type = TYPE_INT8;
    /* face  Face registet modle init */
    ret = CVI_AI_OpenModel(Ai_Fd_Handle, pstFdCfg->model_id_fr, pstFdCfg->model_path_fr);
    if (ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_SetModelPath failed with %#x!\n",  pstFdCfg->model_path_fr, ret);
        return ret;
    }

    ret = CVI_AI_SetModelThreshold(Ai_Fd_Handle, pstFdCfg->model_id_fr, pstFdCfg->threshold_fr);
    if (ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_SetModelThreshold failed with %#x!\n", pstFdCfg->model_path_fr, ret);
        return ret;
    }
    
    return ret;
}


static  CVI_S32 app_ipcam_Face_Registeration(CVI_VOID)
{
    CVI_S32 ret = CVI_SUCCESS;
    DIR * dir = NULL;
    cvai_face_t face_test;
    char image_path[512];
    VIDEO_FRAME_INFO_S fdFrame ={0};
    dir = opendir(IMAGE_DIR);
    if (dir == NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "%s is open fail\n",IMAGE_DIR);
        return -1;
    }
    rewinddir(dir);
    struct dirent * ptr = NULL;

    if (g_stIveHandle == NULL)
    {
        g_stIveHandle = CVI_IVE_CreateHandle();
    }
    for (int i = 0; i < 1000; i++)
    {
        if ((ptr = readdir(dir)) != NULL)
        { 
            if (strcmp( ptr->d_name, ".") == 0 || strcmp( ptr->d_name, "..") == 0)
            {
                continue;
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "%s is registering\n", ptr->d_name);
            sprintf(image_path, "/mnt/sd/picture/%s", ptr->d_name);
            IVE_IMAGE_S image = CVI_IVE_ReadImage(g_stIveHandle, image_path, IVE_IMAGE_TYPE_U8C3_PACKAGE);
            if (image.u32Width == 0)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s Read image failed with !\n", ptr->d_name);
                goto REGISTER_EXIT;
            }


            ret = CVI_IVE_Image2VideoFrameInfo(&image, &fdFrame);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s Convert to video frame failed with %#x!\n", ptr->d_name , ret);
                goto REGISTER_EXIT;
            }

            memset(&face_test, 0, sizeof(cvai_face_t));
            ret = CVI_AI_ScrFDFace(Ai_Fd_Handle, &fdFrame, &face_test);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_ScrFDFace fail\n",ptr->d_name);
                goto REGISTER_EXIT;
            }

            if (face_test.size == 0)
            {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "%s picture not have face\n",ptr->d_name);
                goto REGISTER_EXIT;
            }
            ret = CVI_AI_FaceRecognition(Ai_Fd_Handle, &fdFrame, &face_test);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_FaceRecognition fail \n",ptr->d_name);
                goto REGISTER_EXIT;
            }

            usleep(200);
            face_cnt ++;
            
            for (CVI_U32 j = 0; j < featureArray.data_num; j++) 
            {
                for (CVI_U32 i = 0; i < featureArray.feature_length; i++) 
                {
                    if(j == face_cnt)
                    {
                        ((int8_t *)featureArray.ptr)[j * featureArray.feature_length + i] =  face_test.info[0].feature.ptr[i];
                    }
                }
            }

            ret = CVI_AI_Service_RegisterFeatureArray(Ai_Fd_Service_Handle, featureArray, COS_SIMILARITY);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_Service_RegisterFeatureArray fail \n");
                continue;
            }

            struct LinkList *node = (struct LinkList *) malloc(sizeof(struct LinkList));
            char *put;
            char image_name[128]={0};
            memset(image_name, 0, sizeof(image_name));
            put = strrchr(ptr->d_name, '.');
            strncpy(image_name, ptr->d_name, (strlen(ptr->d_name) - strlen(put)));
            node->index = face_cnt;
            strcpy(node->name,image_name);
            tail->next = node;
            node->next = NULL;
            tail = node;
            APP_PROF_LOG_PRINT(LEVEL_INFO, "'%s' is registering face\n",tail->name);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "face register index is %d\n",face_cnt);
REGISTER_EXIT:
            CVI_AI_Free(&face_test);
            CVI_SYS_FreeI(g_stIveHandle, &image);
        }
    }
    
    closedir(dir);
    ptr = NULL;
    dir = NULL;
    
    return ret;
}

static  CVI_S32 app_ipcam_Face_Recognition(VIDEO_FRAME_INFO_S *pstFrame, cvai_face_t *pstFace , int face_idx)
{
    CVI_S32 ret = CVI_SUCCESS;
    
    CVI_FLOAT value;
    cvai_feature_t db_feature;
    db_feature.ptr = (int8_t *)malloc(featureArray.feature_length);
    db_feature.size = featureArray.feature_length;
    db_feature.type = TYPE_INT8;

    if (!pstFdCfg->CAPTURE_bEnable)
    {
        ret = CVI_AI_FaceRecognitionOne(Ai_Fd_Handle, pstFrame, pstFace, face_idx);
        if (ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FaceRecognitionOne fail \n");
            free(db_feature.ptr);
            return ret;
        }
    }

    for (CVI_U32 i = 0; i < featureArray.data_num; i++)
    {
        for (CVI_U32 k = 0; k < featureArray.feature_length; k++)
        {
            ((int8_t *)db_feature.ptr)[k]=((int8_t *)featureArray.ptr)[i * featureArray.feature_length + k];
        }

        CVI_AI_Service_CalculateSimilarity(Ai_Fd_Service_Handle, &pstFace->info[face_idx].feature, &db_feature, &value);
        if(value > 0.6)
        {
            struct LinkList *display;
            display=head;
            while (display->next != NULL)
            {
                if(display->next->index== (int)i)
                {
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "face_name:%s\n",display->next->name);
                    strcpy(pstFace->info[face_idx].name,display->next->name);
                    break;
                }
                display = display->next;
            }
            APP_PROF_LOG_PRINT(LEVEL_DEBUG,    "index: %d\n",i);
            APP_PROF_LOG_PRINT(LEVEL_DEBUG,    "value %4.2f\n",value);
        }
    }

    free(db_feature.ptr);    
    return ret;
}

static CVI_S32 app_ipcam_Ai_Fd_Proc_Init(CVI_VOID)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD init ------------------> start \n");

    CVI_S32 s32Ret = CVI_SUCCESS;

    app_ipcam_Ai_Fd_Param_dump();

    /* 1.Creat Handle And  Service Handle */
    if (Ai_Fd_Handle == NULL)
    {
        s32Ret = CVI_AI_CreateHandle(&Ai_Fd_Handle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FD_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FD_CreateHandle has created\n");
        return s32Ret;
    }

    if (Ai_Fd_Service_Handle == NULL)
    {
        s32Ret = CVI_AI_Service_CreateHandle(&Ai_Fd_Service_Handle, Ai_Fd_Handle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FD_CreateHandle failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FD_Service_CreateHandle has created\n");
        return s32Ret;
    }

    /* 2.Set Modle Path And threshold */
    //retinaface
    s32Ret = CVI_AI_OpenModel(Ai_Fd_Handle, pstFdCfg->model_id_fd, pstFdCfg->model_path_fd);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_SetModelPath failed with %#x!\n", pstFdCfg->model_path_fd, s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_AI_SetModelThreshold(Ai_Fd_Handle,  pstFdCfg->model_id_fd, pstFdCfg->threshold_fd);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_SetModelThreshold failed with %#x!\n", pstFdCfg->model_path_fd, s32Ret);
        return s32Ret;
    }

    /* 3. Set Skip Vpss Preprocess And Timeout*/
  
    CVI_BOOL bSkip = pstFdCfg->bVpssPreProcSkip;
    s32Ret = CVI_AI_SetSkipVpssPreprocess(Ai_Fd_Handle,  pstFdCfg->model_id_fd, bSkip);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Fd Detection Not Support Skip VpssPreprocess %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = CVI_AI_SetVpssTimeout(Ai_Fd_Handle, 2000);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Fd Set vpss timeout failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    /* 4.Attach Vb POOL For FD Function */
    s32Ret = CVI_AI_SetVBPool(Ai_Fd_Handle, 0, pstFdCfg->FdPoolId);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Fd Set vpss vbpool failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    if (pstFdCfg->FR_bEnable)
    {
        /* 5.Face Recognition function */
        s32Ret = app_ipcam_Ai_FR_Init();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "app_ipcam_Ai_FR_Init init failed with %#x!\n", s32Ret);
        }

        s32Ret = app_ipcam_Face_Registeration();
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Face regestering failed with %#x!\n", s32Ret);
        }
    }

    if (pstFdCfg->CAPTURE_bEnable)
    {
        s32Ret = app_ipcam_Ai_Face_Capture_Init(&Ai_Fd_Handle);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Face Capture init failed with %#x!\n", s32Ret);
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD init ------------------> done \n");
    return CVI_SUCCESS;
}

static CVI_VOID *Thread_FD_PROC(CVI_VOID *arg)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD start running!\n");
    prctl(PR_SET_NAME, "Thread_FD_PROC");

    VPSS_GRP VpssGrp = pstFdCfg->VpssGrp;
    VPSS_CHN VpssChn = pstFdCfg->VpssChn;

    VIDEO_FRAME_INFO_S stfdFrame = {0};
    cvai_face_t face;
    
    CVI_U32 fd_frame = 0;
    CVI_S32 iTime_start, iTime_proc,iTime_fps;
    float iTime_gop;
    iTime_start = GetCurTimeInMsec();

    while (app_ipcam_Ai_FD_ProcStatus_Get()) {
        pthread_mutex_lock(&Ai_Fd_Mutex);
        s32Ret = app_ipcam_Ai_FD_Pause_Get();
        
        if (s32Ret) {
            pthread_mutex_unlock(&Ai_Fd_Mutex);
            usleep(1000*1000);
            continue;
        }
        /* 1.Get frame */
        s32Ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stfdFrame, 3000);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) get frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
            pthread_mutex_unlock(&Ai_Fd_Mutex);
            usleep(100*1000);
            continue;
        }
        pthread_mutex_unlock(&Ai_Fd_Mutex);
        iTime_proc = GetCurTimeInMsec();
        /* 2. Face Detect*/
        memset(&face, 0, sizeof(cvai_face_t));

        if (pstFdCfg->CAPTURE_bEnable)
        {
            s32Ret=app_ipcam_Ai_Face_Capture(&stfdFrame,&face);
            if (s32Ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Ai_Face_Capture fail \n");
                goto FD_FAILURE;
            }
        }
        else
        {
            s32Ret = CVI_AI_ScrFDFace(Ai_Fd_Handle, &stfdFrame, &face);
            if (s32Ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_AI_ScrFDFace fail \n");
                goto FD_FAILURE;
            }
        }

        for (uint32_t i = 0; i < face.size; i++)
        {
            if (face.info[i].mask_score >= pstFdCfg->threshold_mask)
            {
                strncpy(face.info[i].name, "Wearing", sizeof(face.info[i].name));
            }
            else 
            {
                if (pstFdCfg->FR_bEnable)
                {
                    s32Ret = app_ipcam_Face_Recognition(&stfdFrame, &face, i);
                    if (s32Ret != CVI_SUCCESS)
                    {
                        APP_PROF_LOG_PRINT(LEVEL_ERROR,"app_ipcam_Face_Recognition fail \n");
                        goto FD_FAILURE;
                    }
                }
            }
        }

        /* 3.add face funtion*/
        if (pstFdCfg->FACE_AE_bEnable)
        {
            app_ipcam_Ai_FD_AEStart(&stfdFrame, &face);
        }

        fd_proc = GetCurTimeInMsec() - iTime_proc;
    FD_FAILURE:
        /* 4.Release resources and assign values for cvai_face_t */
        s32Ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stfdFrame);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Grp(%d)-Chn(%d) release frame failed with %#x\n", VpssGrp, VpssChn, s32Ret);
        }

        fd_frame ++;
        iTime_fps = GetCurTimeInMsec();
        iTime_gop = (float)(iTime_fps - iTime_start)/1000;
        if(iTime_gop >= 1)
        {
            fd_fps = fd_frame/iTime_gop;
            fd_frame = 0;
            iTime_start = iTime_fps;
        }

        if (face.size == 0)
        {
            if (!pstFdCfg->CAPTURE_bEnable)
            {
                CVI_AI_Free(&face);
            }
            continue;
        }

        SMT_MutexAutoLock(FD_Mutex, lock);
        if (stFdjDraw.info)
        {
            CVI_AI_Free(&stFdjDraw);
        }
        memset(&stFdjDraw, 0, sizeof(cvai_face_t));
        memcpy(&stFdjDraw, &face, sizeof face);
        stFdjDraw.info = (cvai_face_info_t *)malloc(face.size * sizeof(cvai_face_info_t));
        memset(stFdjDraw.info, 0, sizeof(cvai_face_info_t) * stFdjDraw.size);
        for (CVI_U32 i = 0; i < face.size; i++) 
        {
            CVI_AI_CopyInfo(&face.info[i], &stFdjDraw.info[i]);
        }

        if (!pstFdCfg->CAPTURE_bEnable)
        {
            CVI_AI_Free(&face);
        }
    }

    pthread_exit(NULL);

    return NULL;
}

int app_ipcam_Ai_FD_ObjDrawInfo_Get(cvai_face_t *pstAiObj)
{
    _NULL_POINTER_CHECK_(pstAiObj, -1);

    SMT_MutexAutoLock(FD_Mutex, lock);

    if (stFdjDraw.size == 0)
    {
        return CVI_SUCCESS;
    }
    else
    {
        memcpy(pstAiObj, &stFdjDraw, sizeof stFdjDraw);
        pstAiObj->info = (cvai_face_info_t *)malloc(stFdjDraw.size * sizeof(cvai_face_info_t));
        _NULL_POINTER_CHECK_(pstAiObj->info, -1);
        memset(pstAiObj->info, 0, sizeof(cvai_face_info_t) * stFdjDraw.size);
        for (CVI_U32 i = 0; i < stFdjDraw.size; i++)
        {
            CVI_AI_CopyInfo(&stFdjDraw.info[i], &pstAiObj->info[i]);
        }
        CVI_AI_Free(&stFdjDraw);
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_FD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame)
{
    CVI_S32 iTime;

    if (app_ipcam_Ai_FD_ProcStatus_Get())
    {
        SMT_MutexAutoLock(FD_Mutex, lock);
        iTime = GetCurTimeInMsec();
        CVI_AI_RescaleMetaRB(pstVencFrame, &stFdjDraw);
        CVI_AI_Service_FaceDrawRect(
                                Ai_Fd_Service_Handle, 
                                &stFdjDraw, 
                                pstVencFrame, 
                                CVI_TRUE,
                                pstFdCfg->rect_brush);

        CVI_AI_Free(&stFdjDraw);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "FD draw result takes %u ms \n", (GetCurTimeInMsec() - iTime));
    }

    return CVI_SUCCESS;
}

int app_ipcam_Ai_FD_Stop(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pstFdCfg->FD_bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD not enable\n");
        return CVI_SUCCESS;
    }

    if (!app_ipcam_Ai_FD_ProcStatus_Get())
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD has not running!\n");
        return s32Ret;
    }

    app_ipcam_Ai_FD_ProcStatus_Set(CVI_FALSE);

    CVI_S32 iTime = GetCurTimeInMsec();

    if (Thread_Fd_Handle)
    {
        pthread_join(Thread_Fd_Handle, NULL);
        Thread_Fd_Handle = 0;
    }

    if (pstFdCfg->CAPTURE_bEnable)
    {
        app_ipcam_Ai_Face_Capture_Stop();
    }

    if (pstFdCfg->FR_bEnable)
    {
        free(featureArray.ptr);
        featureArray.ptr = NULL;
        App_Clean_List(head);
        head = NULL;
        tail = NULL;
        if (g_stIveHandle)
        {
            CVI_IVE_DestroyHandle(g_stIveHandle);
            g_stIveHandle = NULL;
        }
        face_cnt = 0;
    }

    s32Ret = CVI_AI_Service_DestroyHandle(Ai_Fd_Service_Handle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FD_Service_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        Ai_Fd_Service_Handle = NULL;
    }

    s32Ret = CVI_AI_DestroyHandle(Ai_Fd_Handle);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_FD_DestroyHandle failed with 0x%x!\n", s32Ret);
        return s32Ret;
    }
    else
    {
        Ai_Fd_Handle = NULL;
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "AI FD Thread exit takes %u ms\n", (GetCurTimeInMsec() - iTime));

    return CVI_SUCCESS;
}


int app_ipcam_Ai_FD_Start(void)
{
    CVI_S32 s32Ret = CVI_SUCCESS;

    if (!pstFdCfg->FD_bEnable)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD not enable\n");
        return CVI_SUCCESS;
    }

    if (bRunning)
    {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "AI FD has started\n");
        return CVI_SUCCESS;
    }

    app_ipcam_Ai_Fd_Proc_Init();

    app_ipcam_Ai_FD_ProcStatus_Set(CVI_TRUE);

    s32Ret = pthread_create(&Thread_Fd_Handle, NULL, Thread_FD_PROC, NULL);
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AI Fd_pthread_create failed!\n");
        return s32Ret;
    }

    return CVI_SUCCESS;
}

/*****************************************************************
 *  The following API for command test used             Front
 * **************************************************************/

int app_ipcam_CmdTask_Ai_FD_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate)
{
    CVI_CHAR param[512] = {0};
    CVI_S32 s32Ret = CVI_SUCCESS;
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
                    app_ipcam_Ai_FD_Start();
                }
                else
                {
                    app_ipcam_Ai_FD_Stop();
                }
                break;
            }
            case 't':
            {
                temp = strtok(NULL, "/");
                float threshold = (float)atoi(temp)/100;
                APP_PROF_LOG_PRINT(LEVEL_INFO, "threshold:%lf\n", threshold);
                s32Ret = CVI_AI_SetModelThreshold(Ai_Fd_Handle,  pstFdCfg->model_id_fd, threshold);
                if (s32Ret != CVI_SUCCESS)
                {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_SetModelThreshold failed with %#x!\n", pstFdCfg->model_path_fd, s32Ret);
                    return s32Ret;
                }
                s32Ret = CVI_AI_SetModelThreshold(Ai_Fd_Handle,  pstFdCfg->model_id_fr, threshold);
                if (s32Ret != CVI_SUCCESS)
                {
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s CVI_AI_SetModelThreshold failed with %#x!\n", pstFdCfg->model_path_fr, s32Ret);
                    return s32Ret;
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
