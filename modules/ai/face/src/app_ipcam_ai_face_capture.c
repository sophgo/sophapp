#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"
#include "turbojpeg.h"

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

#define FACE_CAPTURE_FILENAME         "/mnt/sd/capture_face_%lu_%f.jpg"
#define MAIN_CAPTURE_FILENAME         "/mnt/sd/capture_main_%lu_%f.png"
#define FACE_CAPTURE_SIZE            (15)
#define FACE_CAPTURE_STABLE_TIME     (15)        /*face stable time(fps) start capture*/
#define FACE_UNMATCHED_NUM           (15)

#define FACE_SCORE_THRESHOLD         (70)
#define MAX_STRING_LEN               (255)
#define FD_THRESHOLD                 (0.7)

static cviai_app_handle_t g_stFaceCaptureHandle = NULL;
static face_capture_config_t g_stFaceCaptureCfg =
{
    .thr_size_min = 20,         /*face size less than it, quality = 0*/
    .thr_size_max = 1024,        /*face size more than it, quality = 0*/
    .qa_method = 0,                /*use fq?*/
    .thr_quality = 0.1,            /*capture new face quality difference*/
    // .thr_quality_high = 0.95,    /*stop capture new face quality*/
    .thr_yaw = 0.75,            /*angle more than it, quality = 0*/
    .thr_pitch = 0.75,            /*angle more than it, quality = 0*/
    .thr_roll = 0.75,            /*angle more than it, quality = 0*/
    .thr_laplacian = 20,
    .miss_time_limit = 40,        /*face leave time(fps)*/
    .fast_m_interval = 25,        /*fast mode capture time(fps)*/
    .fast_m_capture_num = 5,    /*fast mode how many face capture */
    .cycle_m_interval = 20,        /*cycle mode capture interval(fps)*/
    .auto_m_time_limit = 0,        /*auto mode min time(fps) to capture*/
    .auto_m_fast_cap = 1,        /*auto mode fase capture?*/
    // .capture_aligned_face = 0,    /*capture face aligned?*/
    // .capture_extended_face = 1,
    // .store_RGB888 = 0,
    .store_feature = 0,
    .img_capture_flag = 0,
};


CVI_S32 app_ipcam_Ai_Face_Capture_Init(cviai_handle_t *handle)
{
        CVI_S32 s32Ret = CVI_SUCCESS;

        // APP_PARAM_AI_FD_CFG_S *g_pstFdCfg = app_ipcam_Ai_FD_Param_Get(); 
        s32Ret |= CVI_AI_APP_CreateHandle(&g_stFaceCaptureHandle, *(handle));
        s32Ret |= CVI_AI_APP_FaceCapture_Init(g_stFaceCaptureHandle, FACE_CAPTURE_SIZE);
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "set FaceCapture failed with %#x!\n", s32Ret);
            return s32Ret;
        }

        CVI_AI_SetModelThreshold(*(handle), CVI_AI_SUPPORTED_MODEL_RETINAFACE, FD_THRESHOLD);
        CVI_AI_APP_FaceCapture_SetMode(g_stFaceCaptureHandle, FAST);

        APP_PARAM_AI_FD_CFG_S *FdParam = app_ipcam_Ai_FD_Param_Get();
        // APP_PARAM_VPSS_CFG_T *VpssParam = app_ipcam_Vpss_Param_Get();
        // if (PIXEL_FORMAT_RGB_888 == VpssParam->astVpssGrpCfg[FdParam->VpssGrp].astVpssChnAttr[FdParam->VpssChn].enPixelFormat)
        // {
        //      g_stFaceCaptureCfg.store_RGB888 = 0;
        // } 
        // else {
        //     g_stFaceCaptureCfg.store_RGB888 = 1;
        // }

        g_stFaceCaptureCfg.thr_size_max = FdParam->thr_size_max;
        g_stFaceCaptureCfg.thr_size_min = FdParam->thr_size_min;
        g_stFaceCaptureCfg.thr_laplacian = FdParam->thr_laplacian;

        g_stFaceCaptureHandle->face_cpt_info->fd_inference = CVI_AI_ScrFDFace;
        g_stFaceCaptureHandle->face_cpt_info->fr_inference = CVI_AI_FaceRecognition;

        // if (g_pstFdCfg->FR_bEnable)
        // {
        //     g_stFaceCaptureHandle->face_cpt_info->do_FR = 1;
        // }

        /* Init DeepSORT */
        CVI_AI_DeepSORT_Init(*(handle), false);
        cvai_deepsort_config_t ds_conf;
        memset(&ds_conf, 0, sizeof(ds_conf));
        CVI_AI_DeepSORT_GetDefaultConfig(&ds_conf);

        ds_conf.ktracker_conf.max_unmatched_num = FACE_UNMATCHED_NUM;
        ds_conf.ktracker_conf.accreditation_threshold = FACE_CAPTURE_STABLE_TIME;
        ds_conf.ktracker_conf.P_beta[2] = 0.1;
        ds_conf.ktracker_conf.P_beta[6] = 2.5e-2;
        ds_conf.kfilter_conf.Q_beta[2] = 0.1;
        ds_conf.kfilter_conf.Q_beta[6] = 2.5e-2;
        ds_conf.kfilter_conf.R_beta[2] = 0.1;

        ds_conf.max_distance_consine = 0.2;
        ds_conf.ktracker_conf.feature_budget_size = 5;
        ds_conf.ktracker_conf.feature_update_interval = 2;
        ds_conf.kfilter_conf.confidence_level = L025;
        ds_conf.kfilter_conf.enable_bounding_stay = true;
        ds_conf.kfilter_conf.enable_X_constraint_0 = true;
        ds_conf.ktracker_conf.enable_QA_feature_update = true;
        ds_conf.ktracker_conf.enable_QA_feature_init = true;
        ds_conf.kfilter_conf.X_constraint_max[0] = 2560; /* necessary if enable_bounding_stay */
        ds_conf.kfilter_conf.X_constraint_max[1] = 1440; /* necessary if enable_bounding_stay */
        ds_conf.kfilter_conf.X_constraint_min[2] = 0.75;
        ds_conf.kfilter_conf.X_constraint_max[2] = 1.25;
        ds_conf.enable_internal_FQ = true;
        ds_conf.ktracker_conf.feature_update_quality_threshold = 0.75;
        ds_conf.ktracker_conf.feature_init_quality_threshold = 0.75;
        CVI_AI_DeepSORT_SetConfig(*(handle), &ds_conf, -1, false);

        CVI_AI_APP_FaceCapture_SetConfig(g_stFaceCaptureHandle, &g_stFaceCaptureCfg);
        return s32Ret;
}

CVI_S32 app_ipcam_Ai_Face_Capture(VIDEO_FRAME_INFO_S *stfdFrame,cvai_face_t *capture_face)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    CVI_U32 i = 0;

    FILE *fp = NULL;
    tjhandle tj_handle = NULL;
    unsigned char *outjpg_buf=NULL;
    int flags = 0;
    int subsamp = TJSAMP_422;
    int pixelfmt = TJPF_RGB;
    int jpegQual = 80;
    unsigned long outjpg_size;

    s32Ret = CVI_AI_APP_FaceCapture_Run(g_stFaceCaptureHandle, stfdFrame);
    if (NULL == g_stFaceCaptureHandle->face_cpt_info) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "face_cpt_info is NULL\n");
         return 0;
    }
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_AI_APP_FaceCapture_Run fail \n");
        return s32Ret;
    }

    memcpy(capture_face, &g_stFaceCaptureHandle->face_cpt_info->last_faces, sizeof(cvai_face_t));

    if (NULL == g_stFaceCaptureHandle->face_cpt_info->data)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "g_stFaceCaptureHandle->face_cpt_info->data[%d] is NULL\n", i);
        return 0;
    }
    for (i = 0; i < g_stFaceCaptureHandle->face_cpt_info->size; i++)
    {
        if (!g_stFaceCaptureHandle->face_cpt_info->_output[i])
            continue;
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "unique_id:%lu imgae[w:%u h:%u stride:(%u %u %u) quality:%f score:%f length:(%u %u %u)]\n",
        g_stFaceCaptureHandle->face_cpt_info->data[i].info.unique_id,
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.width, g_stFaceCaptureHandle->face_cpt_info->data[i].image.height, 
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.stride[0], g_stFaceCaptureHandle->face_cpt_info->data[i].image.stride[1], g_stFaceCaptureHandle->face_cpt_info->data[i].image.stride[2],
        g_stFaceCaptureHandle->face_cpt_info->data[i].info.face_quality, g_stFaceCaptureHandle->face_cpt_info->data[i].info.bbox.score, 
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.length[0], g_stFaceCaptureHandle->face_cpt_info->data[i].image.length[1], g_stFaceCaptureHandle->face_cpt_info->data[i].image.length[2]);
        CVI_CHAR captureFileName[MAX_STRING_LEN];
        memset(captureFileName, 0, sizeof(captureFileName));
        snprintf(captureFileName, sizeof(captureFileName), FACE_CAPTURE_FILENAME ,
        g_stFaceCaptureHandle->face_cpt_info->data[i].info.unique_id , g_stFaceCaptureHandle->face_cpt_info->data[i].info.face_quality);

        tj_handle = tjInitCompress();
        if (NULL == tj_handle){
            goto jpeg_exit;
        }

        int ret = tjCompress2(tj_handle, g_stFaceCaptureHandle->face_cpt_info->data[i].image.pix[0],g_stFaceCaptureHandle->face_cpt_info->data[i].image.width,0, 
        g_stFaceCaptureHandle->face_cpt_info->data[i].image.height,pixelfmt,&outjpg_buf,&outjpg_size,subsamp,jpegQual, flags);
        if (0 != ret) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "tjCompress2 fail\n");
            goto jpeg_exit;
        }

        fp = fopen(captureFileName, "w");
        if(NULL == fp)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "%s fopen fail\n",captureFileName);
            goto jpeg_exit;
        }
        fwrite(outjpg_buf,1,outjpg_size,fp);
            
    jpeg_exit:
        if(NULL != fp)
        {
            fclose(fp);
            fp = NULL;
        }
        if(NULL != tj_handle)
        {
            tjDestroy(tj_handle);
        }
    }
    return s32Ret;
}

CVI_S32 app_ipcam_Ai_Face_Capture_Stop(void)
{
    CVI_S32 ret = CVI_SUCCESS;
    ret=CVI_AI_APP_DestroyHandle(g_stFaceCaptureHandle);
    if(ret != CVI_SUCCESS) 
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"CVI_AI_APP_DestroyHandle fail \n");
        return ret;
    }
    g_stFaceCaptureHandle = NULL;
    return ret;
}

