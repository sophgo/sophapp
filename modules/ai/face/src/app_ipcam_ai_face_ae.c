#include <stdio.h>
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>
#include "app_ipcam_ai.h"
#include "cvi_ae.h"
#include "cvi_isp.h"
// Ai Model info
/*****************************************************************
 * Model Func : Face detection
 * Model ID   : CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PERSON_VEHICLE
 * Inference  : CVI_AI_MobileDetV2_Person_Vehicle
 * Model file : mobiledetv2-person-vehicle-ls-768.cvimodel
 *              mobiledetv2-person-vehicle-ls.cvimodel
 *================================================================*/


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
#if 1
static APP_PARAM_AI_FACE_AE_CFG_S stFaceAeCfg = {
    .bEnable = CVI_TRUE,
    .Face_ae_restore_time = 50,
    .Face_target_Luma = 90,
    .Face_target_Luma_L_range = 80,
    .Face_target_Luma_H_range = 200,
    .Face_target_Evbias_L_range = 512,
    .Face_target_Evbias_H_range = 4096,
    .AE_Channel_GB = 0,
    .AE_Channel_B  = 1,
    .AE_Channel_GR = 2,
    .AE_Channel_R  = 3,
    .AE_Grid_Row   = 15,
    .AE_Grid_Column = 17,
    .Face_AE_Min_Cnt = 6,
    .Face_Score_Threshold = 92,
};
#else
static APP_PARAM_AI_FACE_AE_CFG_S stFaceAeCfg;
#endif

static APP_PARAM_AI_FACE_AE_CFG_S *pstFaceAeCfg = &stFaceAeCfg;

typedef struct {
    CVI_U32 x1;
    CVI_U32 y1;
    CVI_U32 x2;
    CVI_U32 y2;
} Coordinate_t;

static CVI_U32 app_ipcam_Ai_FD_GetStatistics(VIDEO_FRAME_INFO_S *pstFrame, Coordinate_t tMax)
{
    if (NULL == pstFrame)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstFrame is NULL\n");
        return 0;
    }

    CVI_S32 ret = CVI_SUCCESS;
    CVI_U8 i = 0, j = 0;
    CVI_U16 gridPerSizeX = 0, gridPerSizeY = 0;
    CVI_U16 fdPosX = 0, fdPosY = 0, fdWidth = 0, fdHeight = 0;
    CVI_U8 rowStartIdx = 0, columnStartIdx = 0, rowEndIdx = 0, columnEndIdx = 0;
    CVI_U16 RValue = 0, GValue = 0, BValue = 0, maxValue = 0;
    CVI_U32 frameLumaSum = 0;
    CVI_U32 frameAvgLuma = 0;
    CVI_U32 aeWinCount = 0;

    APP_PARAM_VI_CTX_S *pstViChnCfg = app_ipcam_Vi_Param_Get();
    ISP_AE_STATISTICS_S stAeStat;
    memset(&stAeStat, 0, sizeof(ISP_AE_STATISTICS_S));

    gridPerSizeX = (pstViChnCfg->astChnInfo[0].u32Width / pstFaceAeCfg->AE_Grid_Column) ? (pstViChnCfg->astChnInfo[0].u32Width / pstFaceAeCfg->AE_Grid_Column) : 1;
    gridPerSizeY = (pstViChnCfg->astChnInfo[0].u32Height / pstFaceAeCfg->AE_Grid_Row) ? (pstViChnCfg->astChnInfo[0].u32Height / pstFaceAeCfg->AE_Grid_Row) : 1;

    Coordinate_t tCur;
    memset(&tCur, 0, sizeof(tCur));
    tCur.x1 = (tMax.x1 * pstViChnCfg->astChnInfo[0].u32Width) / pstFrame->stVFrame.u32Width;
    tCur.x2 = (tMax.x2 * pstViChnCfg->astChnInfo[0].u32Width) / pstFrame->stVFrame.u32Width;
    tCur.y1 = (tMax.y1 * pstViChnCfg->astChnInfo[0].u32Height) / pstFrame->stVFrame.u32Height;
    tCur.y2 = (tMax.y2 * pstViChnCfg->astChnInfo[0].u32Height) / pstFrame->stVFrame.u32Height;

    fdPosX = tCur.x1 + ((tCur.x2 - tCur.x1) / 4);
    fdPosY = tCur.y1 + ((tCur.y2 - tCur.y1) / 4);
    fdWidth = (tCur.x2 - tCur.x1) / 2;
    fdHeight = (tCur.y2 - tCur.y1) / 2;

    rowStartIdx = fdPosY / gridPerSizeY;
    columnStartIdx = fdPosX / gridPerSizeX;

    rowEndIdx = (fdPosY + fdHeight) / gridPerSizeY;
    columnEndIdx = (fdPosX + fdWidth) / gridPerSizeX;

    ret = CVI_ISP_GetAEStatistics(0, &stAeStat);
    if (ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"get get ae statistics error!!!\n");
        return CVI_FALSE;
    }

    for (i = rowStartIdx; i < rowEndIdx; i++)
    {
        for (j = columnStartIdx; j < columnEndIdx; j++)
        {
            RValue = stAeStat.au16FEZoneAvg[0][0][i][j][pstFaceAeCfg->AE_Channel_R];
            BValue = stAeStat.au16FEZoneAvg[0][0][i][j][pstFaceAeCfg->AE_Channel_B];
            GValue = stAeStat.au16FEZoneAvg[0][0][i][j][pstFaceAeCfg->AE_Channel_GR] +
                stAeStat.au16FEZoneAvg[0][0][i][j][pstFaceAeCfg->AE_Channel_GB];
            GValue = GValue / 2;
            
            maxValue = RValue > GValue ? RValue : GValue;
            maxValue = maxValue > BValue ? maxValue : BValue;
            
            frameLumaSum += maxValue;
            aeWinCount++;
        }
    }

    if (aeWinCount > pstFaceAeCfg->Face_AE_Min_Cnt)
    {
        frameAvgLuma = (frameLumaSum / aeWinCount) / 4;
    }
    return frameAvgLuma;
}

static CVI_U32 app_ipcam_Ai_FD_GetLuma(VIDEO_FRAME_INFO_S *pstFrame, Coordinate_t tMax)
{
    if (NULL == pstFrame)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstFrame is NULL\n");
        return 0;
    }

    CVI_U32 i = 0, j = 0, iFactor = 0;
    CVI_U32 R = 0, G = 0, B = 0, MaxValue = 0, StartOffset = 0, EndOffset = 0, iAvgValue = 0;
    CVI_U64 iTotalValue = 0, iTotalPixel = 0;

    iFactor = (pstFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_RGB_888) ? 3 : 1;

    pstFrame->stVFrame.pu8VirAddr[0] = CVI_SYS_Mmap(pstFrame->stVFrame.u64PhyAddr[0], pstFrame->stVFrame.u32Length[0]);
    if (pstFrame->stVFrame.pu8VirAddr[0] == NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstFrame->stVFrame.pu8VirAddr[0] is NULL\n");
        return 0;
    }
    for (i = tMax.y1; i < tMax.y2; i++)
    {
        StartOffset = (pstFrame->stVFrame.u32Stride[0] * i) + (tMax.x1 * iFactor);
        EndOffset = (pstFrame->stVFrame.u32Stride[0] * i) + (tMax.x2 * iFactor);
        if (EndOffset > pstFrame->stVFrame.u32Length[0])
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "EndOffset:%d > u32Length:%d\n", (EndOffset + iFactor), pstFrame->stVFrame.u32Length[0]);
            return iAvgValue;
        }
        for (j = StartOffset; j < EndOffset; j += iFactor)
        {
            if (pstFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_RGB_888)
            {
                R = *(pstFrame->stVFrame.pu8VirAddr[0] + j);
                G = *(pstFrame->stVFrame.pu8VirAddr[0] + (j + 1));
                B = *(pstFrame->stVFrame.pu8VirAddr[0] + (j + 2));
                MaxValue = (R > G ? R : G) > B ? (R > G ? R : G) : B;
            }
            else
            {
                MaxValue = *(pstFrame->stVFrame.pu8VirAddr[0] + j);
            }
            iTotalValue += MaxValue;
            iTotalPixel++;
        }
    }
    CVI_SYS_Munmap(pstFrame->stVFrame.pu8VirAddr[0], pstFrame->stVFrame.u32Length[0]);
    pstFrame->stVFrame.pu8VirAddr[0] = NULL;

    if (iTotalPixel > 0)
    {
        iAvgValue = iTotalValue / iTotalPixel;
    }
    return iAvgValue;
}
CVI_VOID app_ipcam_Ai_FD_AEStart(VIDEO_FRAME_INFO_S *pstFrame, cvai_face_t *pstFace)
{
    if (NULL == pstFrame || NULL == pstFace)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pstFrame/pstFace is NULL\n");
        return ;
    }
    CVI_S32 ret = CVI_SUCCESS;
    VI_PIPE ViPipe = 0;
    static CVI_U32 stFrameCount = 0;
    static CVI_U32 bSetFaceAe = 0;

    ISP_EXPOSURE_ATTR_S stExpAttr;
    
    CVI_U32 i = 0;
    CVI_U32 iValue = 0;
    Coordinate_t tCurrent;
    Coordinate_t tMax;
    memset(&tCurrent, 0, sizeof(tCurrent));
    memset(&tMax, 0, sizeof(tMax));
    
    if (bSetFaceAe) 
    {
        stFrameCount++;
    }
    if (pstFace->size > 0) 
    {
        for (i = 0; i < pstFace->size; i++) 
        {
            if ((pstFace->info[i].bbox.score * 100) < pstFaceAeCfg->Face_Score_Threshold)//侦测框是人脸的概率
            {
                continue;
            }
            //选取最大人脸框
            tCurrent.x1 = (CVI_U32)pstFace->info[i].bbox.x1;
            tCurrent.x2 = (CVI_U32)pstFace->info[i].bbox.x2;
            tCurrent.y1 = (CVI_U32)pstFace->info[i].bbox.y1;
            tCurrent.y2 = (CVI_U32)pstFace->info[i].bbox.y2;
            if (((tCurrent.y2 - tCurrent.y1) * (tCurrent.x2 - tCurrent.x1)) > ((tMax.y2 - tMax.y1) * (tMax.x2 - tMax.x1))) 
            {
                memcpy(&tMax, &tCurrent, sizeof(Coordinate_t));
            }
        }

        //去除超范围的坐标
        if ((tMax.x1 >= tMax.x2) || (tMax.y1 >= tMax.y2) ||
            (tMax.x1 > pstFrame->stVFrame.u32Width) || (tMax.x2 > pstFrame->stVFrame.u32Width) ||
            (tMax.y1 >  pstFrame->stVFrame.u32Height) || (tMax.y2 >  pstFrame->stVFrame.u32Height))
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "pararm X(%d %d) Y(%d %d) WH(%d %d) is invalid\n",
                                            tMax.x1, tMax.x2, tMax.y1, tMax.y2,
                                            pstFrame->stVFrame.u32Width, pstFrame->stVFrame.u32Height);
            return ;
        }

        iValue = app_ipcam_Ai_FD_GetStatistics(pstFrame, tMax);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "app_ipcam_Ai_FD_GetStatistics iValue:%d\n", iValue);
        if (iValue == 0) 
        {
            iValue = app_ipcam_Ai_FD_GetLuma(pstFrame, tMax);
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "app_ipcam_Ai_FD_GetLuma iValue:%d\n", iValue);
        }

        if ((iValue > 0) && ((iValue >= pstFaceAeCfg->Face_target_Luma_H_range) || (iValue <= pstFaceAeCfg->Face_target_Luma_L_range))) 
        {
            memset(&stExpAttr, 0, sizeof(stExpAttr));
            CVI_ISP_GetExposureAttr(ViPipe, &stExpAttr);
            stExpAttr.stAuto.u16EVBias = (pstFaceAeCfg->Face_target_Luma * 1024) / iValue;
            if (stExpAttr.stAuto.u16EVBias < pstFaceAeCfg->Face_target_Evbias_L_range)
            {
                stExpAttr.stAuto.u16EVBias =  pstFaceAeCfg->Face_target_Evbias_L_range;
            }
            else if (stExpAttr.stAuto.u16EVBias >  pstFaceAeCfg->Face_target_Evbias_H_range)
            {
                stExpAttr.stAuto.u16EVBias =  pstFaceAeCfg->Face_target_Evbias_H_range;
            }

            ret = CVI_ISP_SetExposureAttr(ViPipe, &stExpAttr);
            if (ret != CVI_SUCCESS)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ISP_SetFaceAeInfo failed with %#x\n", ret);
            }
            else
            {
                bSetFaceAe = 1;
            }
        }
        stFrameCount = 0;
    } 
    else 
    {
        if (bSetFaceAe && (stFrameCount > pstFaceAeCfg->Face_ae_restore_time)) 
        {
            memset(&stExpAttr, 0, sizeof(stExpAttr));
            CVI_ISP_GetExposureAttr(ViPipe, &stExpAttr);
            stExpAttr.stAuto.u16EVBias = 1024;
            ret = CVI_ISP_SetExposureAttr(ViPipe, &stExpAttr);
            if (ret != CVI_SUCCESS) 
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_ISP_SetFaceAeInfo failed with %#x\n", ret);
            }
            else
            {
                stFrameCount = 0;
                bSetFaceAe = 0;
            }
        }
    }
}
