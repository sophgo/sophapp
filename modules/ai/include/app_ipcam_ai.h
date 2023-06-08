#ifndef __APP_IPCAM_AI_H__
#define __APP_IPCAM_AI_H__

#include "cvi_type.h"
#include "cviai.h"
#include "linux/cvi_comm_video.h"
#include "cvi_vpss.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_venc.h"
#include "app/cviai_app.h"
#include "cvi_ive.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MODEL_PATH_LEN  128


#define SMT_MUTEXAUTOLOCK_INIT(mutex) pthread_mutex_t AUTOLOCK_##mutex = PTHREAD_MUTEX_INITIALIZER;

#define SMT_MutexAutoLock(mutex, lock) __attribute__((cleanup(AutoUnLock))) \
       pthread_mutex_t *lock= &AUTOLOCK_##mutex;\
       pthread_mutex_lock(lock);

__attribute__ ((always_inline)) inline void AutoUnLock(void *mutex) {
       pthread_mutex_unlock( *(pthread_mutex_t**) mutex);
}

#define SEND_SIGNAL_DRAW_AI_OBJS_RECT(mutex, osdcCmprCond)  \
    do {                                                    \
        pthread_mutex_lock(&mutex);                         \
        pthread_cond_signal(&osdcCmprCond);                 \
        pthread_mutex_unlock(&mutex);                       \
    } while(0)

#define WAIT_SIGNAL_DRAW_AI_OBJS_RECT(mutex, osdcCmprCond, timeout)  \
    do {                                                             \
        pthread_mutex_lock(&mutex);                                  \
        pthread_cond_timedwait(&osdcCmprCond, &mutex, &timeout);     \
        pthread_mutex_unlock(&mutex);                                \
    } while(0)

typedef CVI_S32 (*pfpInferenceFunc)(cviai_handle_t handle, VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj);
typedef CVI_S32 (*pfpRescaleFunc)(const VIDEO_FRAME_INFO_S *frame, cvai_object_t *obj);



typedef struct APP_PARAM_AI_MD_CFG_T {
    CVI_BOOL bEnable;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    CVI_U32 u32GrpWidth;
	CVI_U32 u32GrpHeight;
    CVI_U32 threshold;
    CVI_U32 u32BgUpPeriod;
    CVI_U32 miniArea;
    cvai_service_brush_t rect_brush;
} APP_PARAM_AI_MD_CFG_S;


typedef struct APP_PARAM_AI_CRY_CFG_T {
    CVI_BOOL bEnable;
    CVI_AI_SUPPORTED_MODEL_E model_id;
    char model_path[MODEL_PATH_LEN];
} APP_PARAM_AI_CRY_CFG_S;

typedef struct APP_PARAM_AI_PD_CFG_T {
    CVI_BOOL bEnable;
    CVI_BOOL Intrusion_bEnable;
    CVI_BOOL capture_enable;
    CVI_S32 capture_frames;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    CVI_U32 u32GrpWidth;
	CVI_U32 u32GrpHeight;
    CVI_U32 model_size_w;
    CVI_U32 model_size_h;
    CVI_U32 region_stRect_x1;
    CVI_U32 region_stRect_y1;
    CVI_U32 region_stRect_x2;
    CVI_U32 region_stRect_y2;
    CVI_U32 region_stRect_x3;
    CVI_U32 region_stRect_y3;
    CVI_U32 region_stRect_x4;
    CVI_U32 region_stRect_y4;
    CVI_U32 region_stRect_x5;
    CVI_U32 region_stRect_y5;
    CVI_U32 region_stRect_x6;
    CVI_U32 region_stRect_y6;
    CVI_BOOL bVpssPreProcSkip;
    float threshold;
    CVI_AI_SUPPORTED_MODEL_E model_id;
    char model_path[MODEL_PATH_LEN];
    cvai_service_brush_t rect_brush;
} APP_PARAM_AI_PD_CFG_S;

typedef struct APP_PARAM_AI_FD_CFG_T {
    CVI_BOOL FD_bEnable;
    CVI_BOOL FR_bEnable;
    CVI_BOOL MASK_bEnable;
    CVI_BOOL CAPTURE_bEnable;
    CVI_BOOL FACE_AE_bEnable;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VB_POOL FdPoolId;
    CVI_U32 u32GrpWidth;
    CVI_U32 u32GrpHeight;
    CVI_U32 model_size_w;
    CVI_U32 model_size_h;
    CVI_BOOL bVpssPreProcSkip;
    float threshold_fd;
    float threshold_fr;
    float threshold_mask;
    CVI_U32 thr_size_min;
    CVI_U32 thr_size_max;
    float thr_laplacian;
    CVI_AI_SUPPORTED_MODEL_E model_id_fd;
    CVI_AI_SUPPORTED_MODEL_E model_id_fr;
    CVI_AI_SUPPORTED_MODEL_E model_id_mask;
    char model_path_fd[MODEL_PATH_LEN];
    char model_path_fr[MODEL_PATH_LEN];
    char model_path_mask[MODEL_PATH_LEN];
    cvai_service_brush_t rect_brush;
} APP_PARAM_AI_FD_CFG_S;

typedef struct APP_PARAM_AI_FACE_AE_CFG_T {
    CVI_BOOL bEnable;
    CVI_U32 Face_ae_restore_time;
	CVI_U32 Face_target_Luma;
    CVI_U32 Face_target_Luma_L_range;
    CVI_U32 Face_target_Luma_H_range;
    CVI_U32 Face_target_Evbias_L_range;
    CVI_U32 Face_target_Evbias_H_range;
    CVI_U32 AE_Channel_GB;
    CVI_U32 AE_Channel_B;
    CVI_U32 AE_Channel_GR;
    CVI_U32 AE_Channel_R;
    CVI_U32 AE_Grid_Row;
    CVI_U32 AE_Grid_Column;
    CVI_U32 Face_AE_Min_Cnt;
    CVI_U32 Face_Score_Threshold;
} APP_PARAM_AI_FACE_AE_CFG_S;

typedef struct APP_PARAM_AI_IR_FD_CFG_T {
    CVI_BOOL bEnable;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    CVI_S32 attachPoolId;
    CVI_BOOL bVpssPreProcSkip;
    char model_path_fd[MODEL_PATH_LEN];
    char model_path_ln[MODEL_PATH_LEN];
    char model_path_fr[MODEL_PATH_LEN];
    
}APP_PARAM_AI_IR_FD_CFG_S;

/* Personnel detection function */
APP_PARAM_AI_PD_CFG_S *app_ipcam_Ai_PD_Param_Get(void);
CVI_VOID app_ipcam_Ai_PD_ProcStatus_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_PD_ProcStatus_Get(void);
CVI_VOID app_ipcam_Ai_PD_Pause_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_PD_Pause_Get(void);
int app_ipcam_Ai_PD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame);
int app_ipcam_Ai_PD_Start(void);
int app_ipcam_Ai_PD_Stop(void);
int app_ipcam_Ai_PD_ObjDrawInfo_Get(cvai_object_t *pstAiObj);
CVI_U32 app_ipcam_Ai_PD_ProcFps_Get(void);
CVI_S32 app_ipcam_Ai_PD_ProcTime_Get(void);
CVI_S32 app_ipcam_Pd_threshold_Set(float threshold);
CVI_S32 app_ipcam_Ai_Pd_Intrusion_Init(void);
CVI_U32 app_ipcam_Ai_PD_ProcIntrusion_Num_Get(void);
CVI_S32 app_ipcam_Ai_PD_StatusGet(void);

#ifdef FACE_SUPPORT
/* Face detection function */
APP_PARAM_AI_FD_CFG_S *app_ipcam_Ai_FD_Param_Get(void);
CVI_VOID app_ipcam_Ai_FD_ProcStatus_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_FD_ProcStatus_Get(void);
CVI_VOID app_ipcam_Ai_FD_Pause_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_FD_Pause_Get(void);
int app_ipcam_Ai_FD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame);
int app_ipcam_Ai_FD_Start(void);
int app_ipcam_Ai_FD_Stop(void);
int app_ipcam_Ai_FD_ObjDrawInfo_Get(cvai_face_t *pstAiObj);
CVI_U32 app_ipcam_Ai_FD_ProcFps_Get(void);
CVI_S32 app_ipcam_Ai_FD_ProcTime_Get(void);
#endif

#ifdef IR_FACE_SUPPORT
/* IR Face detection function */
CVI_S32 app_ipcam_Ai_IR_FD_Start(void);
CVI_S32 app_ipcam_Ai_IR_FD_Stop(void);
APP_PARAM_AI_IR_FD_CFG_S * app_ipcam_Ai_IR_FD_Param_Get(void);
CVI_S32 app_ipcam_Ai_IR_FD_UnRegister(char * gallery_name);
CVI_S32 app_ipcam_Ai_IR_FD_Register(char * galler_name);
#endif

/* motion detection function */
APP_PARAM_AI_MD_CFG_S *app_ipcam_Ai_MD_Param_Get(void);
CVI_VOID app_ipcam_Ai_MD_ProcStatus_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_MD_ProcStatus_Get(void);
CVI_VOID app_ipcam_Ai_MD_Pause_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_MD_Pause_Get(void);
int app_ipcam_Ai_MD_Rect_Draw(VIDEO_FRAME_INFO_S *pstVencFrame);
int app_ipcam_Ai_MD_Start(void);
int app_ipcam_Ai_MD_Stop(void);
int app_ipcam_Ai_MD_ObjDrawInfo_Get(cvai_object_t *pstAiObj);
CVI_U32 app_ipcam_Ai_MD_ProcFps_Get(void);
CVI_S32 app_ipcam_Ai_MD_ProcTime_Get(void);
CVI_VOID app_ipcam_Ai_MD_Thresold_Set(CVI_U32 value);
CVI_U32 app_ipcam_Ai_MD_Thresold_Get(void);
CVI_S32 app_ipcam_Ai_MD_StatusGet(void);

/* baby_cry detection function */
#if defined AUDIO_SUPPORT && defined AI_BABYCRY_SUPPORT
APP_PARAM_AI_CRY_CFG_S *app_ipcam_Ai_Cry_Param_Get(void);
CVI_VOID app_ipcam_Ai_Cry_ProcStatus_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_Cry_ProcStatus_Get(void);
CVI_VOID app_ipcam_Ai_Cry_Pause_Set(CVI_BOOL flag);
CVI_BOOL app_ipcam_Ai_Cry_Pause_Get(void);
int app_ipcam_Ai_Cry_Stop(void);
int app_ipcam_Ai_Cry_Start(void);
CVI_S32 app_ipcam_Ai_Cry_StatusGet(void);
#endif

#ifdef FACE_SUPPORT
/* face ae */
CVI_VOID app_ipcam_Ai_FD_AEStart(VIDEO_FRAME_INFO_S *pstFrame, cvai_face_t *pstFace);

/* face capture*/
CVI_S32 app_ipcam_Ai_Face_Capture_Init(cviai_handle_t *handle);
CVI_S32 app_ipcam_Ai_Face_Capture(VIDEO_FRAME_INFO_S *stfdFrame,cvai_face_t *capture_face);
CVI_S32 app_ipcam_Ai_Face_Capture_Stop(void);
#endif

/*****************************************************************
 *  The following API for command test used             S
 * **************************************************************/
int app_ipcam_CmdTask_Ai_PD_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
int app_ipcam_CmdTask_Ai_MD_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
#ifdef FACE_SUPPORT
int app_ipcam_CmdTask_Ai_FD_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
#endif
/*****************************************************************
 *  The above API for command test used                 E
 * **************************************************************/


#ifdef __cplusplus
}
#endif

#endif
