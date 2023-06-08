#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <dirent.h>
#include <pthread.h>
#include <math.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "app_ipcam_ll.h"
#include "app_ipcam_ai.h"
#include <dirent.h>


#define AI_VPSSDEV 0
#define AI_VPSSGRP 15
#define FEATURE_GALLERY_DIR "/mnt/sd/gallery"//特征库目录
#define FEATURE_NUM_MAX 100

#define LL_INIT(N) ((N)->next = (N)->prev = (N))

#define LL_HEAD(H) struct llhead H = { &H, &H }

#define LL_EMPTY(N) ((N)->next == (N))

#define LL_DEL(N) do {                  \
    ((N)->next)->prev = ((N)->prev);    \
    ((N)->prev)->next = ((N)->next);    \
    LL_INIT(N);                         \
} while (0)

#define LL_FOREACH(H,N) for (N = (H)->next; N != (H); N = (N)->next)

#define LL_FOREACH_SAFE(H,N,T)  for (N = (H)->next, T = (N)->next; N != (H); N = (T), T = (N)->next)

#define LL_DATA_NODE_DEL(pNode) do {                \
    if (pNode != NULL) {                            \
        ((pNode)->next)->prev = ((pNode)->prev);    \
        ((pNode)->prev)->next = ((pNode)->next);    \
    }                                               \
} while(0)

#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *mptr = ptr;  \
    (type *)((char *)mptr-offsetof(type, member));  \
})

#if 0
#define LINK_LIST_DATA_POP_FRONT(PopPoint, Head, member) ({             \
    if((Head)->next == (Head)) {                                            \
        PopPoint = NULL;                                                \
    } else {                                                            \
        PopPoint = container_of((Head)->next, typeof(*PopPoint), member); \
        LL_DATA_NODE_DEL((Head)->next);                                   \
    }                                                                   \
})
#endif

#define LINK_LIST_DATA_PUSH_TAIL(H, N) do { \
    ((H)->prev)->next = (N);                \
    (N)->prev = ((H)->prev);                \
    (N)->next = (H);                        \
    (H)->prev = (N);                        \
} while (0)

typedef struct _APP_IRFACE_FEATURE_DATA_S
{
    CVI_CHAR name[64];
    cvai_feature_t stFeature;
}APP_IRFACE_FEATURE_DATA_S;

typedef struct _APP_IRFACE_FEATURE_NODE_S
{
    APP_IRFACE_FEATURE_DATA_S data;
    APP_LINK_LIST_S stList;
}APP_IRFACE_FEATURE_NODE_S;

typedef struct _APP_IRFACE_FEATURE_LIST_S
{
    CVI_S32 s32listDepth;
    APP_IRFACE_FEATURE_NODE_S stHead;
    pthread_mutex_t stLock;
}APP_IRFACE_FEATURE_LIST_S;

static APP_PARAM_AI_IR_FD_CFG_S g_AI_IrFdCtx = {0};
static cviai_handle_t g_AI_Handle = NULL;
static pthread_t g_TaskPthreadID;
static CVI_S32 g_TaskRunStatus = 0;
static pthread_mutex_t g_IrFaceMutex;
static APP_IRFACE_FEATURE_LIST_S g_IrFace_FeatureList = {0};

APP_PARAM_AI_IR_FD_CFG_S * app_ipcam_Ai_IR_FD_Param_Get(void)
{
    return &g_AI_IrFdCtx;
}

static CVI_S32 _list_push_back(APP_IRFACE_FEATURE_LIST_S * pstHead, cvai_feature_t * pstNode, char * name)
{
    if (pstHead == NULL || pstNode == NULL) {
        return CVI_FAILURE;
    }
    pthread_mutex_lock(&pstHead->stLock);
    if (pstHead->s32listDepth >= FEATURE_NUM_MAX) {
        pthread_mutex_unlock(&pstHead->stLock);
        return CVI_FAILURE;
    }
    //入队
    APP_IRFACE_FEATURE_NODE_S * newNode = (APP_IRFACE_FEATURE_NODE_S *)malloc(sizeof(APP_IRFACE_FEATURE_NODE_S));
    if (newNode == NULL) {
        pthread_mutex_unlock(&pstHead->stLock);
        return CVI_FAILURE;
    }
    memset(newNode, 0 , sizeof(APP_IRFACE_FEATURE_NODE_S));
    newNode->data.stFeature.ptr = malloc(pstNode->size);
    if (newNode->data.stFeature.ptr == NULL) {
        free(newNode);
        pthread_mutex_unlock(&pstHead->stLock);
        return CVI_FAILURE;
    }
    snprintf(newNode->data.name, sizeof(newNode->data.name), "%s", name);
    memcpy(newNode->data.stFeature.ptr, pstNode->ptr ,pstNode->size);
    newNode->data.stFeature.size = pstNode->size;
    newNode->data.stFeature.type = pstNode->type;
    LINK_LIST_DATA_PUSH_TAIL(&pstHead->stHead.stList, &newNode->stList);
    pstHead->s32listDepth++;
    pthread_mutex_unlock(&pstHead->stLock);
    return CVI_SUCCESS;
}

static CVI_S32 _list_pop(APP_IRFACE_FEATURE_LIST_S * pstHead, char * name)
{
    APP_IRFACE_FEATURE_NODE_S * pstNode = NULL;
    APP_LINK_LIST_S * pstNext = NULL;
    APP_LINK_LIST_S * pstTmp = NULL;
    LL_FOREACH_SAFE(&pstHead->stHead.stList, pstNext, pstTmp) {
        pstNode = container_of(pstNext, APP_IRFACE_FEATURE_NODE_S, stList);
        if (pstNode) {
            if(strcmp(pstNode->data.name, name) == 0) {
                LL_DATA_NODE_DEL(pstNext);
                if (pstNode->data.stFeature.ptr) {
                    free(pstNode->data.stFeature.ptr);
                }
                free(pstNode);
            }
        }
    }
    return CVI_SUCCESS;
}

static CVI_S32 _list_pop_front(APP_IRFACE_FEATURE_LIST_S * pstHead)
{
    APP_IRFACE_FEATURE_NODE_S * pstNode = NULL;
    APP_LINK_LIST_S * pstNext = NULL;
    APP_LINK_LIST_S * pstTmp = NULL;
    LL_FOREACH_SAFE(&pstHead->stHead.stList, pstNext, pstTmp) {
        pstNode = container_of(pstNext, APP_IRFACE_FEATURE_NODE_S, stList);
        if (pstNode) {
            LL_DATA_NODE_DEL(pstNext);
            if (pstNode->data.stFeature.ptr) {
                free(pstNode->data.stFeature.ptr);
            }
            free(pstNode);
            break;
        }
    }
#if 0
    APP_IRFACE_FEATURE_NODE_S * pstNode = NULL;
    LINK_LIST_DATA_POP_FRONT(pstNode, &pstHead->stHead.stList, stList);
    if (pstNode) {
        free(pstNode->data.name);
        printf("test pstNode->data.name is %s \r\n", pstNode->data.name);
        if (pstNode->data.stFeature.ptr) {
            free(pstNode->data.stFeature.ptr);
            pstNode->data.stFeature.ptr = NULL;
        }
        free(pstNode);
    }
#endif
    return 0;
}

static CVI_S32 ai_deinit_gallery()
{
    pthread_mutex_unlock(&g_IrFace_FeatureList.stLock);
    while (!LL_EMPTY(&g_IrFace_FeatureList.stHead.stList)) {
        _list_pop_front(&g_IrFace_FeatureList);
    }
    pthread_mutex_unlock(&g_IrFace_FeatureList.stLock);
    return CVI_SUCCESS;
}

static CVI_S32 ai_init_gallery()
{
    char filepath[512] = {0};
    DIR  *dirp = NULL;
    struct dirent * entryp;
    struct stat statbuf;
    int size = 0;
    if ((dirp = opendir(FEATURE_GALLERY_DIR)) == NULL) {
        return -1;
    }
    //上电初始化数据底库
    pthread_mutex_unlock(&g_IrFace_FeatureList.stLock);
    //读取文件中底库
    while ((entryp = readdir(dirp)) != NULL) {
        if (entryp->d_type == DT_DIR || strcmp(entryp->d_name, ".") == 0 || strcmp(entryp->d_name, "..") == 0) {
            continue;
        }
        snprintf(filepath, sizeof(filepath), "%s/%s", FEATURE_GALLERY_DIR, entryp->d_name);
        stat(filepath, &statbuf);
        size = statbuf.st_size;
        int filefd = open(filepath, O_RDONLY);
        if (filefd <= 0) {
            continue;
        }
        cvai_feature_t  stNode = {0};
        stNode.ptr = malloc(size);
        if(stNode.ptr) {
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "insert gallery name %s \r\n", entryp->d_name);
            read(filefd, stNode.ptr, size);
            stNode.size = size;
            _list_push_back(&g_IrFace_FeatureList, &stNode, entryp->d_name);
            free(stNode.ptr);
        }
        close(filefd);
    }
    pthread_mutex_unlock(&g_IrFace_FeatureList.stLock);
    closedir(dirp);
    return 0;
}

static CVI_S32 ai_cal_cos_sim(cvai_feature_t *a, cvai_feature_t *b, float *score)
{
    if (a->ptr == NULL || b->ptr == NULL || a->size != b->size) {
        return CVI_FAILURE;
    }
    float A = 0, B = 0, AB = 0;
    for (uint32_t i = 0; i < a->size; i++) {
        A += (int)a->ptr[i] * (int)a->ptr[i];
        B += (int)b->ptr[i] * (int)b->ptr[i];
        AB += (int)a->ptr[i] * (int)b->ptr[i];
    }
    A = sqrt(A);
    B = sqrt(B);
    *score = AB / (A * B);
    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Ai_FD_init()
{
    //CVI_S32 s32Ret = CVI_AI_CreateHandle2(&g_AI_Handle, AI_VPSSGRP, AI_VPSSDEV);
    //初始化句柄
    CVI_S32 s32Ret = CVI_AI_CreateHandle(&g_AI_Handle);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, " CVI_AI_CreateHandle err ret %d \r\n", s32Ret);
        return s32Ret;
    }
    //打开模型FD : scrfd_320_256.cvimodel
    s32Ret = CVI_AI_OpenModel(g_AI_Handle, CVI_AI_SUPPORTED_MODEL_SCRFDFACE, app_ipcam_Ai_IR_FD_Param_Get()->model_path_fd);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, " CVI_AI_OpenModel err ret %d module_path: %s \r\n", s32Ret, app_ipcam_Ai_IR_FD_Param_Get()->model_path_fd);
    }
    //打开模型LN : liveness.cvimodel
    s32Ret = CVI_AI_OpenModel(g_AI_Handle, CVI_AI_SUPPORTED_MODEL_IRLIVENESS, app_ipcam_Ai_IR_FD_Param_Get()->model_path_fr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, " CVI_AI_OpenModel err ret %d module_path: %s \r\n", s32Ret, app_ipcam_Ai_IR_FD_Param_Get()->model_path_fr);
    }
    //打开模型IRFR : ir_recogition.cvimodel
    s32Ret = CVI_AI_OpenModel(g_AI_Handle, CVI_AI_SUPPORTED_MODEL_FACERECOGNITION, app_ipcam_Ai_IR_FD_Param_Get()->model_path_fr);
    if (s32Ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, " CVI_AI_OpenModel err ret %d module_path: %s \r\n", s32Ret, app_ipcam_Ai_IR_FD_Param_Get()->model_path_fr);
    }
    if (app_ipcam_Ai_IR_FD_Param_Get()->bVpssPreProcSkip) {
        //预处理
        CVI_AI_SetSkipVpssPreprocess(g_AI_Handle, CVI_AI_SUPPORTED_MODEL_SCRFDFACE, app_ipcam_Ai_IR_FD_Param_Get()->bVpssPreProcSkip);
        CVI_AI_SetSkipVpssPreprocess(g_AI_Handle, CVI_AI_SUPPORTED_MODEL_IRLIVENESS, app_ipcam_Ai_IR_FD_Param_Get()->bVpssPreProcSkip);
        CVI_AI_SetSkipVpssPreprocess(g_AI_Handle, CVI_AI_SUPPORTED_MODEL_FACERECOGNITION, app_ipcam_Ai_IR_FD_Param_Get()->bVpssPreProcSkip);
    }
    if (app_ipcam_Ai_IR_FD_Param_Get()->attachPoolId != -1) {
        s32Ret = CVI_AI_SetVBPool(g_AI_Handle, 0, app_ipcam_Ai_IR_FD_Param_Get()->attachPoolId);
    }
    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Ai_IR_FD_Recognition(VIDEO_FRAME_INFO_S *pstFrame, cvai_face_t * pstFaceResult)
{
    int ret = 0;
    //IR人脸识别
    //input: pstFrame IR图像 分辨率需求：无需求
    //out : pstFaceResult 人脸识别结果
    //返回-1识别失败 或无人脸
    if (g_AI_Handle == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AiHandle == NULL \r\n");
        return CVI_FAILURE;
    }
    ret = CVI_AI_ScrFDFace(g_AI_Handle, pstFrame, pstFaceResult);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_ScrFDFace failed ret %#x!\r\n", ret);
        return CVI_FAILURE;
    }
    if (pstFaceResult->size != 1) {
        //APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_AI_ScrFDFace err face_meta.size %d\r\n", pstFaceResult->size);
        return CVI_FAILURE;
    }
    CVI_AI_IrLiveness(g_AI_Handle, pstFrame, pstFaceResult);
    CVI_AI_FaceRecognition(g_AI_Handle, pstFrame, pstFaceResult);
    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Ai_IR_FD_UnRegister(char * gallery_name)
{
    CVI_CHAR _filePath[128] = {0};
    snprintf(_filePath, sizeof(_filePath), "%s/%s", FEATURE_GALLERY_DIR, gallery_name);
    pthread_mutex_lock(&g_IrFaceMutex);
    if (access(_filePath, F_OK) == 0) {
        remove(_filePath);
    }
    _list_pop(&g_IrFace_FeatureList, gallery_name);
    pthread_mutex_unlock(&g_IrFaceMutex);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "UnRegister %s success\r\n", gallery_name);
    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Ai_IR_FD_Register(char * gallery_name)
{
    //IR人脸注册 保存FEATURE_GALLERY_DIR目录
    //保存识别底库
    //gallery_name: 保存底库名称
    CVI_S32 ret = 0;
    CVI_CHAR _filePath[128] = {0};
    cvai_face_t stFaceMeta;
    VIDEO_FRAME_INFO_S stFrame;
    CVI_S32 VpssGrp = app_ipcam_Ai_IR_FD_Param_Get()->VpssGrp;
    CVI_S32 VpssChn = app_ipcam_Ai_IR_FD_Param_Get()->VpssChn;

    pthread_mutex_lock(&g_IrFaceMutex);
    ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stFrame, 3000);
    if (ret != CVI_SUCCESS) {
            pthread_mutex_unlock(&g_IrFaceMutex);
            return CVI_FAILURE;
    }
    memset(&stFaceMeta, 0, sizeof(cvai_face_t));
    CVI_AI_ScrFDFace(g_AI_Handle, &stFrame, &stFaceMeta);
    if (stFaceMeta.size != 1) {
        //注册时候不能同时存在多张人脸
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Register face err pstFaceMeta.size %d\r\n", stFaceMeta.size);
        CVI_AI_Free(&stFaceMeta);
        CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
        pthread_mutex_unlock(&g_IrFaceMutex);
        return CVI_FAILURE;
    }
    //活体检测
    CVI_AI_IrLiveness(g_AI_Handle, &stFrame, &stFaceMeta);
    if (stFaceMeta.info[0].liveness_score > 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Register face err liveness_score %f\r\n", stFaceMeta.info[0].liveness_score);
        CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
        pthread_mutex_unlock(&g_IrFaceMutex);
        return CVI_FAILURE;
    }
    // 人脸识别
    CVI_AI_FaceRecognition(g_AI_Handle, &stFrame, &stFaceMeta);
    CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
    pthread_mutex_unlock(&g_IrFaceMutex);
    // 保存人脸底库
    if (stFaceMeta.size == 1) {
        const unsigned char * ptr = (const unsigned char *)stFaceMeta.info[0].feature.ptr;
        snprintf(_filePath, sizeof(_filePath), "%s/%s", FEATURE_GALLERY_DIR, gallery_name);
        if (access(_filePath, F_OK) == 0) {
            remove(_filePath);
        }
        int filefd = open(_filePath, O_CREAT | O_WRONLY);
        if (filefd < 0) {
            CVI_AI_Free(&stFaceMeta);
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "Register file open err %s\r\n", _filePath);
            return CVI_FAILURE;
        }
        write(filefd, ptr, stFaceMeta.info[0].feature.size);
        close(filefd);
        _list_push_back(&g_IrFace_FeatureList, &stFaceMeta.info[0].feature, gallery_name);
    }
    //释放资源
    APP_PROF_LOG_PRINT(LEVEL_INFO, "Register %s success\r\n", gallery_name);
    CVI_AI_Free(&stFaceMeta);
    return CVI_SUCCESS;
}

static CVI_S32 app_ipcam_Ai_IR_FD_Deinit()
{
    if (g_AI_Handle != NULL) {
        CVI_AI_DestroyHandle(g_AI_Handle);
        g_AI_Handle = NULL;
    }
    return CVI_SUCCESS;
}

void * Ir_Face_Task(void * args)
{
    CVI_S32 VpssGrp = app_ipcam_Ai_IR_FD_Param_Get()->VpssGrp;
    CVI_S32 VpssChn = app_ipcam_Ai_IR_FD_Param_Get()->VpssChn;
    CVI_S32 ret = 0;

    g_TaskRunStatus = 1;
    prctl(PR_SET_NAME, "IR_FD_TASK");
    VIDEO_FRAME_INFO_S stFrame;
    cvai_face_t  stFaceResult;
    float score;
    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    while(g_TaskRunStatus)
    {
        pthread_mutex_lock(&g_IrFaceMutex);
        ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stFrame, 3000);
        if (ret != CVI_SUCCESS) {
            pthread_mutex_unlock(&g_IrFaceMutex);
            continue;
        }
        CVI_AI_Free(&stFaceResult);
        if (app_ipcam_Ai_IR_FD_Recognition(&stFrame, &stFaceResult) == CVI_SUCCESS) {
            pthread_mutex_lock(&g_IrFace_FeatureList.stLock);
            if (LL_EMPTY(&g_IrFace_FeatureList.stHead.stList)) {
                pthread_mutex_unlock(&g_IrFace_FeatureList.stLock);
                CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
                pthread_mutex_unlock(&g_IrFaceMutex);
                continue;
            }
            //遍历指针
            APP_LINK_LIST_S * ptmpList;
            APP_IRFACE_FEATURE_NODE_S * ptmpNode;
            //比对人脸结果
            LL_FOREACH(&g_IrFace_FeatureList.stHead.stList, ptmpList) {
                ptmpNode = container_of(ptmpList, APP_IRFACE_FEATURE_NODE_S, stList);
                if (ptmpNode && ai_cal_cos_sim(&stFaceResult.info[0].feature, &ptmpNode->data.stFeature, &score) == 0) {
                    if (score > 0.5) {
                        //人脸结果输出
                        printf("liveness successfully,  match successfully, score: %f id: %s \n",score, ptmpNode->data.name);
                    }
                }
            }
            pthread_mutex_unlock(&g_IrFace_FeatureList.stLock);
        }
        CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrame);
        pthread_mutex_unlock(&g_IrFaceMutex);
        usleep(5 * 1000);
    }
    return 0;
}

CVI_S32 app_ipcam_Ai_IR_FD_Stop(void)
{
    if (g_TaskRunStatus == 1) {
        g_TaskRunStatus = 0;
        pthread_join(g_TaskPthreadID, NULL);
    }
    app_ipcam_Ai_IR_FD_Deinit();
    ai_deinit_gallery();
    pthread_mutex_destroy(&g_IrFace_FeatureList.stLock);
    pthread_mutex_destroy(&g_IrFaceMutex);
    return CVI_SUCCESS;
}

CVI_S32 app_ipcam_Ai_IR_FD_Start(void)
{
    //关闭IRCUT自动 进入IR夜视 IR Senosr不需要这步
    extern void app_ipcam_IRCutMode_ManualCtrl(CVI_S32 value, CVI_S32 state);
    app_ipcam_IRCutMode_ManualCtrl(CVI_TRUE, 1);
    if (access(FEATURE_GALLERY_DIR, F_OK) != 0) {
        mkdir(FEATURE_GALLERY_DIR, 0666);
    }
    pthread_mutex_init(&g_IrFaceMutex, NULL);
    pthread_mutex_init(&g_IrFace_FeatureList.stLock, NULL);
    LL_INIT(&g_IrFace_FeatureList.stHead.stList);
    ai_init_gallery();
    app_ipcam_Ai_FD_init();
    pthread_create(&g_TaskPthreadID, NULL, Ir_Face_Task, NULL);
    return CVI_SUCCESS;
}
