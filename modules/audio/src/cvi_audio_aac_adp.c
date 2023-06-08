/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: cvi_audio_aac_adp.c
 * Description:
 *   Common audio link lib for AAC codec.
 */
 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "app_ipcam_comm.h"
#include "cvi_audio_aac_adp.h"
#include "cvi_audio_dl_adp.h"
#include "cvi_aacdec.h"
#include "cvi_aacenc.h"

#define _VERSION_TAG_AAC_ADP_  "2020210407"


#define CVI_AUDIO_ASSERT(x) {    \
    if (CVI_TRUE != (x)) \
        return -1;  \
}

#define AAC_ENC_LIB_NAME "libaacenc2.so"
#define AAC_DEC_LIB_NAME "libaacdec2.so"
#define CVI_ERR_AENC_NOT_SUPPORT       0xA2000008
#define CVI_ERR_AENC_ILLEGAL_PARAM     0xA2000003
#define CVI_ERR_ADEC_NOT_SUPPORT       0xA3000008
#define CVI_ERR_AENC_NULL_PTR         0xA2000006
#define CVI_ERR_AENC_NOMEM            0xA200000A
#define CVI_ERR_ADEC_NOMEM             0xA300000A
#define CVI_ERR_ADEC_DECODER_ERR       0xA300000F
#define CVI_ERR_ADEC_BUF_LACK          0xA3000010
//# aac enc lib
typedef CVI_S32(*pCVI_AACENC_GetVersion_Callback)(AACENC_VERSION_S *pVersion);
typedef CVI_S32(*pAACInitDefaultConfig_Callback)(AACENC_CONFIG *pstConfig);
typedef CVI_S32(*pAACEncoderOpen_Callback)(AAC_ENCODER_S **phAacPlusEnc, AACENC_CONFIG *pstConfig);
typedef CVI_S32(*pAACEncoderFrame_Callback)(AAC_ENCODER_S *hAacPlusEnc,
                        CVI_S16 *ps16PcmBuf,
                        CVI_U8 *pu8Outbuf,
                        CVI_S32 s32InputBytes,
                        CVI_S32 *ps32NumOutBytes);

typedef CVI_VOID(*pAACEncoderClose_Callback)(AAC_ENCODER_S *hAacPlusEnc);

typedef int(*pCVI_AACDEC_GetVersion_Callback)(AACDEC_VERSION_S *pVersion);

typedef CVIAACDecoder(*pAACInitDecoder_Callback)(AACDECTransportType enTranType);
typedef CVI_VOID(*pAACFreeDecoder_Callback)(CVIAACDecoder CVIAACDecoder);
typedef CVI_S32(*pAACSetRawMode_Callback)(CVIAACDecoder cviAACDecoder, CVI_S32 nChans, CVI_S32 sampRate);
typedef CVI_S32(*pAACDecodeFindSyncHeader_Callback)(CVIAACDecoder cviAACDecoder,
            CVI_U8 **ppInbufPtr,
            CVI_S32 *pBytesLeft);
typedef CVI_S32(*pAACDecodeFrame_Callback)(CVIAACDecoder cviAACDecoder,
            CVI_U8 **ppInbufPtr,
            CVI_S32 *pBytesLeft, CVI_S16 *pOutPcm);
typedef CVI_S32(*pAACGetLastFrameInfo_Callback)(CVIAACDecoder cviAACDecoder, AACFrameInfo * aacFrameInfo);
typedef CVI_S32(*pAACDecoderSetEosFlag_Callback)(CVIAACDecoder cviAACDecoder, CVI_S32 s32Eosflag);
typedef CVI_S32(*pAACFlushCodec_Callback)(CVIAACDecoder cviAACDecoder);

typedef struct
{
    CVI_S32 s32OpenCnt;
    CVI_VOID *pLibHandle;
    pCVI_AACENC_GetVersion_Callback pCVI_AACENC_GetVersion;
    pAACInitDefaultConfig_Callback pAACInitDefaultConfig;
    pAACEncoderOpen_Callback pAACEncoderOpen;
    pAACEncoderFrame_Callback pAACEncoderFrame;
    pAACEncoderClose_Callback pAACEncoderClose;
} AACENC_FUN_S;

typedef struct
{
    CVI_S32 s32OpenCnt;
    CVI_VOID *pLibHandle;
    pCVI_AACDEC_GetVersion_Callback pCVI_AACDEC_GetVersion;
    pAACInitDecoder_Callback pAACInitDecoder;
    pAACFreeDecoder_Callback pAACFreeDecoder;
    pAACSetRawMode_Callback pAACSetRawMode;
    pAACDecodeFindSyncHeader_Callback pAACDecodeFindSyncHeader;
    pAACDecodeFrame_Callback pAACDecodeFrame;
    pAACGetLastFrameInfo_Callback pAACGetLastFrameInfo;
    pAACDecoderSetEosFlag_Callback pAACDecoderSetEosFlag;
    pAACFlushCodec_Callback pAACFlushCodec;
} AACDEC_FUN_S;

static CVI_S32 g_AacEncHandle = -1;
static CVI_S32 g_AacDecHandle = -1;

static AACENC_FUN_S g_stAacEncFunc = {0};
static AACDEC_FUN_S g_stAacDecFunc = {0};

static CVI_S32 InitAacAencLib(void)
{
    if (g_stAacEncFunc.s32OpenCnt == 0)
    {
        AACENC_FUN_S stAacEncFunc;

        memset(&stAacEncFunc, 0, sizeof(AACENC_FUN_S));
#ifdef CVIAUDIO_STATIC
        APP_PROF_LOG_PRINT(LEVEL_WARN, "[cviaudio]Not Using dlopen\n");
        g_stAacEncFunc.pLibHandle = (CVI_VOID *)1;//not using dlopen, give a default value for handler check
        g_stAacEncFunc.pCVI_AACENC_GetVersion = CVI_AACENC_GetVersion;
        g_stAacEncFunc.pAACInitDefaultConfig = AACInitDefaultConfig;
        g_stAacEncFunc.pAACEncoderOpen = AACEncoderOpen;
        g_stAacEncFunc.pAACEncoderFrame = AACEncoderFrame;
        g_stAacEncFunc.pAACEncoderClose = AACEncoderClose;
#else
        CVI_S32 s32Ret = CVI_FAILURE;
        s32Ret = CVI_Audio_Dlopen(&(stAacEncFunc.pLibHandle), AAC_ENC_LIB_NAME);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "load aenc lib fail!\n");
            return CVI_ERR_AENC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacEncFunc.pCVI_AACENC_GetVersion),
                stAacEncFunc.pLibHandle, "CVI_AACENC_GetVersion");
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_AENC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacEncFunc.pAACInitDefaultConfig),
            stAacEncFunc.pLibHandle, "AACInitDefaultConfig");
        if (s32Ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_AENC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacEncFunc.pAACEncoderOpen),
        stAacEncFunc.pLibHandle, "AACEncoderOpen");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_AENC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacEncFunc.pAACEncoderFrame),
        stAacEncFunc.pLibHandle, "AACEncoderFrame");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_AENC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacEncFunc.pAACEncoderClose),
        stAacEncFunc.pLibHandle, "AACEncoderClose");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_AENC_NOT_SUPPORT;
        }

        memcpy(&g_stAacEncFunc, &stAacEncFunc, sizeof(AACENC_FUN_S));
#endif
    }
    g_stAacEncFunc.s32OpenCnt++;
    return CVI_SUCCESS;
}

CVI_VOID DeInitAacAencLib(CVI_VOID)
{
    if (g_stAacEncFunc.s32OpenCnt != 0)
    {
        g_stAacEncFunc.s32OpenCnt--;
    }

    if (g_stAacEncFunc.s32OpenCnt == 0)
    {
#ifdef CVIAUDIO_STATIC
        g_stAacEncFunc.pLibHandle = CVI_NULL;
        memset(&g_stAacEncFunc, 0, sizeof(AACENC_FUN_S));
        g_stAacEncFunc.pCVI_AACENC_GetVersion = CVI_NULL;
        g_stAacEncFunc.pAACInitDefaultConfig = CVI_NULL;
        g_stAacEncFunc.pAACEncoderOpen = CVI_NULL;
        g_stAacEncFunc.pAACEncoderFrame = CVI_NULL;
        g_stAacEncFunc.pAACEncoderClose = CVI_NULL;
#else
        if (g_stAacEncFunc.pLibHandle != CVI_NULL)
        {
            CVI_Audio_Dlclose(g_stAacEncFunc.pLibHandle);
        }

        memset(&g_stAacEncFunc, 0, sizeof(AACENC_FUN_S));
#endif
    }
}

CVI_S32 CVI_AACENC_GetVersion_Adp(AACENC_VERSION_S *pVersion)
{
    if (g_stAacEncFunc.pCVI_AACENC_GetVersion == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_AENC_NOT_SUPPORT;
    }
    return g_stAacEncFunc.pCVI_AACENC_GetVersion(pVersion);
}

CVI_S32 AACInitDefaultConfig_Adp(AACENC_CONFIG *pstConfig)
{
    if (g_stAacEncFunc.pAACInitDefaultConfig == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_AENC_NOT_SUPPORT;
    }
    return g_stAacEncFunc.pAACInitDefaultConfig(pstConfig);
}

CVI_S32 AACEncoderOpen_Adp(AAC_ENCODER_S **phAacPlusEnc, AACENC_CONFIG *pstConfig)
{
    if (g_stAacEncFunc.pAACEncoderOpen == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_AENC_NOT_SUPPORT;
    }

    return g_stAacEncFunc.pAACEncoderOpen(phAacPlusEnc, pstConfig);
}

CVI_S32 AACEncoderFrame_Adp(AAC_ENCODER_S *hAacPlusEnc,
    CVI_S16 *ps16PcmBuf,
    CVI_U8 *pu8Outbuf,
    CVI_S32 s32InputBytes,
    CVI_S32 *ps32NumOutBytes)
{
    if (g_stAacEncFunc.pAACEncoderFrame == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_AENC_NOT_SUPPORT;
    }

    return g_stAacEncFunc.pAACEncoderFrame(hAacPlusEnc, ps16PcmBuf, pu8Outbuf, s32InputBytes, ps32NumOutBytes);
}

CVI_VOID AACEncoderClose_Adp(AAC_ENCODER_S *hAacPlusEnc)
{
    if (g_stAacEncFunc.pAACEncoderClose == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return;
    }

    return g_stAacEncFunc.pAACEncoderClose(hAacPlusEnc);
}

static CVI_S32 InitAacAdecLib(void)
{
    if (g_stAacDecFunc.s32OpenCnt == 0)
    {
        AACDEC_FUN_S stAacDecFunc;

        memset(&stAacDecFunc, 0, sizeof(AACDEC_FUN_S));
#ifdef CVIAUDIO_STATIC
        APP_PROF_LOG_PRINT(LEVEL_WARN, "[cviaudio]Not Using dlopen\n");
        g_stAacDecFunc.pLibHandle = (CVI_VOID *)1;//give a none-null value for handle
        g_stAacDecFunc.pCVI_AACDEC_GetVersion = CVI_AACDEC_GetVersion;
        g_stAacDecFunc.pAACInitDecoder = AACInitDecoder;
        g_stAacDecFunc.pAACFreeDecoder = AACFreeDecoder;
        g_stAacDecFunc.pAACSetRawMode = AACSetRawMode;
        g_stAacDecFunc.pAACDecodeFindSyncHeader = AACDecodeFindSyncHeader;
        g_stAacDecFunc.pAACDecodeFrame = AACDecodeFrame;
        g_stAacDecFunc.pAACGetLastFrameInfo = AACGetLastFrameInfo;
        g_stAacDecFunc.pAACDecoderSetEosFlag = AACDecoderSetEosFlag;
        g_stAacDecFunc.pAACFlushCodec = AACFlushCodec;

#else
        CVI_S32 s32Ret = CVI_FAILURE;
        s32Ret = CVI_Audio_Dlopen(&(stAacDecFunc.pLibHandle), AAC_DEC_LIB_NAME);
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "load aenc lib fail!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pCVI_AACDEC_GetVersion),
                stAacDecFunc.pLibHandle, "CVI_AACDEC_GetVersion");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACInitDecoder),
            stAacDecFunc.pLibHandle, "AACInitDecoder");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACFreeDecoder),
            stAacDecFunc.pLibHandle, "AACFreeDecoder");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACSetRawMode),
            stAacDecFunc.pLibHandle, "AACSetRawMode");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACDecodeFindSyncHeader),
            stAacDecFunc.pLibHandle, "AACDecodeFindSyncHeader");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACDecodeFrame),
            stAacDecFunc.pLibHandle, "AACDecodeFrame");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACGetLastFrameInfo),
            stAacDecFunc.pLibHandle, "AACGetLastFrameInfo");
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACDecoderSetEosFlag),
            stAacDecFunc.pLibHandle, "AACDecoderSetEosFlag");

        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        s32Ret = CVI_Audio_Dlsym((CVI_VOID **)&(stAacDecFunc.pAACFlushCodec),
            stAacDecFunc.pLibHandle, "AACFlushCodec");
        if (s32Ret != CVI_SUCCESS)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "find symbol error!\n");
            return CVI_ERR_ADEC_NOT_SUPPORT;
        }

        memcpy(&g_stAacDecFunc, &stAacDecFunc, sizeof(AACDEC_FUN_S));
#endif
    }
    g_stAacDecFunc.s32OpenCnt++;
    return CVI_SUCCESS;
}

CVI_VOID DeInitAacAdecLib(CVI_VOID)
{
    if (g_stAacDecFunc.s32OpenCnt != 0)
    {
        g_stAacDecFunc.s32OpenCnt--;
    }


    if (g_stAacDecFunc.s32OpenCnt == 0)
    {
#ifdef CVIAUDIO_STATIC
        g_stAacDecFunc.pLibHandle = CVI_NULL;
        memset(&g_stAacDecFunc, 0, sizeof(AACDEC_FUN_S));
        g_stAacDecFunc.pCVI_AACDEC_GetVersion = CVI_NULL;
        g_stAacDecFunc.pAACInitDecoder = CVI_NULL;
        g_stAacDecFunc.pAACFreeDecoder = CVI_NULL;
        g_stAacDecFunc.pAACSetRawMode = CVI_NULL;
        g_stAacDecFunc.pAACDecodeFindSyncHeader = CVI_NULL;
        g_stAacDecFunc.pAACDecodeFrame = CVI_NULL;
        g_stAacDecFunc.pAACGetLastFrameInfo = CVI_NULL;
        g_stAacDecFunc.pAACDecoderSetEosFlag = CVI_NULL;
        g_stAacDecFunc.pAACFlushCodec = CVI_NULL;

#else
        if (g_stAacDecFunc.pLibHandle != CVI_NULL)
        {
            CVI_Audio_Dlclose(g_stAacDecFunc.pLibHandle);
        }

        memset(&g_stAacDecFunc, 0, sizeof(AACDEC_FUN_S));
#endif
    }
}

CVI_S32 CVI_AACDEC_GetVersion_Adp(AACDEC_VERSION_S *pVersion)
{
    if (g_stAacDecFunc.pCVI_AACDEC_GetVersion == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }

    return g_stAacDecFunc.pCVI_AACDEC_GetVersion(pVersion);
}

CVIAACDecoder AACInitDecoder_Adp(AACDECTransportType enTranType)
{
    if (g_stAacDecFunc.pAACInitDecoder == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_NULL;
    }

    return g_stAacDecFunc.pAACInitDecoder(enTranType);
}

CVI_VOID AACFreeDecoder_Adp(CVIAACDecoder cviAACDecoder)
{
    if (g_stAacDecFunc.pAACFreeDecoder == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return;
    }

    return g_stAacDecFunc.pAACFreeDecoder(cviAACDecoder);
}

CVI_S32 AACSetRawMode_Adp(CVIAACDecoder cviAACDecoder, CVI_S32 nChans, CVI_S32 sampRate)
{
    if (g_stAacDecFunc.pAACSetRawMode == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }


    return g_stAacDecFunc.pAACSetRawMode(cviAACDecoder, nChans, sampRate);
}

CVI_S32 AACDecodeFindSyncHeader_Adp(CVIAACDecoder cviAACDecoder, CVI_U8 **ppInbufPtr, CVI_S32 *pBytesLeft)
{
    if (g_stAacDecFunc.pAACDecodeFindSyncHeader == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }

    return g_stAacDecFunc.pAACDecodeFindSyncHeader(cviAACDecoder, ppInbufPtr, pBytesLeft);
}

CVI_S32 AACDecodeFrame_Adp(CVIAACDecoder cviAACDecoder, CVI_U8 **ppInbufPtr, CVI_S32 *pBytesLeft, CVI_S16 *pOutPcm)
{
    if (g_stAacDecFunc.pAACDecodeFrame == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }

    return g_stAacDecFunc.pAACDecodeFrame(cviAACDecoder, ppInbufPtr, pBytesLeft, pOutPcm);
}

CVI_S32 AACGetLastFrameInfo_Adp(CVIAACDecoder cviAACDecoder, AACFrameInfo *aacFrameInfo)
{
    if (g_stAacDecFunc.pAACGetLastFrameInfo == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }

    return g_stAacDecFunc.pAACGetLastFrameInfo(cviAACDecoder, aacFrameInfo);
}

CVI_S32 AACDecoderSetEosFlag_Adp(CVIAACDecoder cviAACDecoder, CVI_S32 s32Eosflag)
{
    if (g_stAacDecFunc.pAACDecoderSetEosFlag == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }

    return g_stAacDecFunc.pAACDecoderSetEosFlag(cviAACDecoder, s32Eosflag);
}

CVI_S32 AACFlushCodec_Adp(CVIAACDecoder cviAACDecoder)
{
    if (g_stAacDecFunc.pAACFlushCodec == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "call aac function fail!\n");
        return CVI_ERR_ADEC_NOT_SUPPORT;
    }

    return g_stAacDecFunc.pAACFlushCodec(cviAACDecoder);
}

static CVI_S32 AencCheckAACAttr(const AENC_ATTR_AAC_S *pstAACAttr)
{
    if (pstAACAttr->enBitWidth != AUDIO_BIT_WIDTH_16)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "invalid bitwidth for AAC encoder");
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pstAACAttr->enSoundMode >= AUDIO_SOUND_MODE_BUTT) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "invalid sound mode for AAC encoder");
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if ((pstAACAttr->enAACType == AAC_TYPE_EAACPLUS) &&
        (pstAACAttr->enSoundMode != AUDIO_SOUND_MODE_STEREO))
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "invalid sound mode for AAC encoder");
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pstAACAttr->enTransType == AAC_TRANS_TYPE_ADTS)
    {
        if ((pstAACAttr->enAACType == AAC_TYPE_AACLD) ||
            (pstAACAttr->enAACType == AAC_TYPE_AACELD))
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "AACLD or AACELD not support AAC_TRANS_TYPE_ADTS");
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }
    }

    return CVI_SUCCESS;
}

static CVI_S32 AencCheckAACLCConfig(AACENC_CONFIG *pconfig)
{
    CVI_S32 s32MinBitRate = 0;
    CVI_S32 s32MaxBitRate = 0;
    CVI_S32 s32RecommendRate = 0;

    if (pconfig->coderFormat == AACLC)
    {
        if (pconfig->nChannelsOut != pconfig->nChannelsIn)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC nChannelsOut(%d) in not equal to nChannelsIn(%d)\n",
                                            pconfig->nChannelsOut, pconfig->nChannelsIn);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }

        if (pconfig->sampleRate == 32000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 192000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate) 
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 32000 Hz bitRate(%d) should be %d ~ %d, recommended %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 44100)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 48000 : 48000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 265000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 44100 Hz bitRate(%d) should be %d ~ %d, recommended %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 48000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 48000 : 48000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 288000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 48000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 16000) {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 24000 : 24000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 96000 : 192000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 48000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 16000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 8000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 16000 : 16000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 48000 : 96000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 24000 : 32000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 8000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 24000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 144000 : 288000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 48000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 24000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 22050)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 132000 : 265000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 48000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC 22050 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLC invalid samplerate(%d)\n", pconfig->sampleRate);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        /* return erro code*/
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    return CVI_SUCCESS;
}

static CVI_S32 AencCheckEAACConfig(AACENC_CONFIG *pconfig)
{
    CVI_S32 s32MinBitRate = 0;
    CVI_S32 s32MaxBitRate = 0;
    CVI_S32 s32RecommendRate = 0;

    if (pconfig->coderFormat == EAAC)
    {
        if (pconfig->nChannelsOut != pconfig->nChannelsIn)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC nChannelsOut(%d) is not equal to nChannelsIn(%d)\n",
                                            pconfig->nChannelsOut, pconfig->nChannelsIn);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }

        if (pconfig->sampleRate == 32000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 64000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC 32000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 44100)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 64000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC 44100 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 48000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 64000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC 48000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 16000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 24000 : 24000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 48000 : 96000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC 16000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        } else if (pconfig->sampleRate == 22050) {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 64000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC 22050 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 24000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 64000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC 24000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                                                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAAC invalid samplerate(%d)\n", pconfig->sampleRate);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        /* return error code */
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    return CVI_SUCCESS;
}

static CVI_S32 AencCheckEAACPLUSConfig(AACENC_CONFIG *pconfig)
{
    CVI_S32 s32MinBitRate = 0;
    CVI_S32 s32MaxBitRate = 0;
    CVI_S32 s32RecommendRate = 0;

    if (pconfig->coderFormat == EAACPLUS)
    {
        if (pconfig->nChannelsOut != 2 || pconfig->nChannelsIn != 2)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS nChannelsOut(%d) and nChannelsIn(%d) should be 2\n",
            pconfig->nChannelsOut, pconfig->nChannelsIn);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }

        if (pconfig->sampleRate == 32000)
        {
            s32MinBitRate = 16000;
            s32MaxBitRate = 64000;
            s32RecommendRate = 32000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS 32000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 44100)
        {
            s32MinBitRate = 16000;
            s32MaxBitRate = 64000;
            s32RecommendRate = 48000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS 44100 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 48000)
        {
            s32MinBitRate = 16000;
            s32MaxBitRate = 64000;
            s32RecommendRate = 48000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS 48000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 16000)
        {
            s32MinBitRate = 16000;
            s32MaxBitRate = 48000;
            s32RecommendRate = 32000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS 16000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 22050)
        {
            s32MinBitRate = 16000;
            s32MaxBitRate = 64000;
            s32RecommendRate = 32000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS 22050 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 24000)
        {
            s32MinBitRate = 16000;
            s32MaxBitRate = 64000;
            s32RecommendRate = 32000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS 24000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "EAACPLUS invalid samplerate(%d)\n", pconfig->sampleRate);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        /* return error code */
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    return CVI_SUCCESS;
}

static CVI_S32 AencCheckAACLDConfig(AACENC_CONFIG *pconfig)
{
    CVI_S32 s32MinBitRate = 0;
    CVI_S32 s32MaxBitRate = 0;
    CVI_S32 s32RecommendRate = 0;

    if (pconfig->coderFormat == AACLD)
    {
        if (pconfig->nChannelsOut != pconfig->nChannelsIn)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD nChannelsOut(%d) in not equal to nChannelsIn(%d)\n",
            pconfig->nChannelsOut, pconfig->nChannelsIn);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }

        if (pconfig->sampleRate == 32000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 48000 : 64000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 320000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 32000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 44100)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 44000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 320000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 128000 : 256000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 44100 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 48000) {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 64000 : 64000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 320000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 128000 : 256000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 48000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 16000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 24000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 192000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 96000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 16000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 8000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 16000 : 16000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 96000 : 192000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 24000 : 48000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 8000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 24000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 48000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 256000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 24000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 22050)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 48000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 256000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 96000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD 22050 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACLD invalid samplerate(%d)\n", pconfig->sampleRate);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        /* return error code */
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    return CVI_SUCCESS;
}

static CVI_S32 AencCheckAACELDConfig(AACENC_CONFIG *pconfig)
{
    CVI_S32 s32MinBitRate = 0;
    CVI_S32 s32MaxBitRate = 0;
    CVI_S32 s32RecommendRate = 0;

    if (pconfig->coderFormat == AACELD)
    {
        if (pconfig->nChannelsOut != pconfig->nChannelsIn)
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD nChannelsOut(%d) in not equal to nChannelsIn(%d)\n",
            pconfig->nChannelsOut, pconfig->nChannelsIn);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }

        if (pconfig->sampleRate == 32000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 64000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 320000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 32000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 44100)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 96000 : 192000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 320000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 128000 : 256000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 44100 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 48000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 96000 : 192000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 320000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 128000 : 256000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 48000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 16000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 16000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 256000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 96000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 16000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 8000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 32000 : 64000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 96000 : 192000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 32000 : 64000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 8000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 24000)
        {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 24000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 256000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 64000 : 128000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 24000 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else if (pconfig->sampleRate == 22050) {
            s32MinBitRate = (pconfig->nChannelsIn == 1) ? 24000 : 32000;
            s32MaxBitRate = (pconfig->nChannelsIn == 1) ? 256000 : 320000;
            s32RecommendRate = (pconfig->nChannelsIn == 1) ? 48000 : 96000;
            if (pconfig->bitRate < s32MinBitRate || pconfig->bitRate > s32MaxBitRate)
            {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD 22050 Hz bitRate(%d) should be %d ~ %d, recommed %d\n",
                pconfig->bitRate, s32MinBitRate, s32MaxBitRate, s32RecommendRate);
                return CVI_ERR_AENC_ILLEGAL_PARAM;
            }
        }
        else
        {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "AACELD invalid samplerate(%d)\n", pconfig->sampleRate);
            return CVI_ERR_AENC_ILLEGAL_PARAM;
        }
    }
    else
    {
        /* return error code */
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    return CVI_SUCCESS;
}

CVI_S32 AencAACCheckConfig(AACENC_CONFIG *pconfig)
{
    CVI_S32 s32Ret = 0;

    if (pconfig == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "pconfig is null");
        return CVI_ERR_AENC_NULL_PTR;
    }

    if (pconfig->coderFormat != AACLC && pconfig->coderFormat != EAAC &&
        pconfig->coderFormat != EAACPLUS && pconfig->coderFormat != AACLD &&
        pconfig->coderFormat != AACELD)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "aacenc coderFormat(%d) invalid\n", pconfig->coderFormat);
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pconfig->quality != AU_QualityExcellent &&
        pconfig->quality != AU_QualityHigh &&
        pconfig->quality != AU_QualityMedium &&
        pconfig->quality != AU_QualityLow)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "aacenc quality(%d) invalid\n", pconfig->quality);
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pconfig->bitsPerSample != 16)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "aacenc bitsPerSample(%d) should be 16\n", pconfig->bitsPerSample);
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pconfig->transtype < 0 || pconfig->transtype > 2)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "invalid transtype(%d), not in [0, 2]\n", pconfig->transtype);
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pconfig->bandWidth != 0 &&
        (pconfig->bandWidth < 1000 || pconfig->bandWidth > pconfig->sampleRate / 2))
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "AAC bandWidth(%d) should be 0, or 1000 ~ %d\n",
        pconfig->bandWidth, pconfig->sampleRate / 2);
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    if (pconfig->coderFormat == AACLC)
    {
        s32Ret = AencCheckAACLCConfig(pconfig);
    }
    else if (pconfig->coderFormat == EAAC)
    {
        s32Ret = AencCheckEAACConfig(pconfig);
    }
    else if (pconfig->coderFormat == EAACPLUS)
    {
        s32Ret = AencCheckEAACPLUSConfig(pconfig);
    }
    else if (pconfig->coderFormat == AACLD)
    {
        s32Ret = AencCheckAACLDConfig(pconfig);
    }
    else if (pconfig->coderFormat == AACELD)
    {
        s32Ret = AencCheckAACELDConfig(pconfig);
    }

    return s32Ret;
}

CVI_S32 OpenAACEncoder(CVI_VOID *pEncoderAttr, CVI_VOID **ppEncoder)
{
    AENC_AAC_ENCODER_S *pstEncoder = CVI_NULL;
    AENC_ATTR_AAC_S *pstAttr = CVI_NULL;
    CVI_S32 s32Ret;
    AACENC_CONFIG config;

    CVI_AUDIO_ASSERT(pEncoderAttr != CVI_NULL);
    CVI_AUDIO_ASSERT(ppEncoder != CVI_NULL);

    /* check attribute of encoder */
    pstAttr = (AENC_ATTR_AAC_S *)pEncoderAttr;
    s32Ret = AencCheckAACAttr(pstAttr);
    if (s32Ret)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "s32Ret:0x%x.#########\n", s32Ret);
        return s32Ret;
    }

    /* allocate memory for encoder */
    pstEncoder = (AENC_AAC_ENCODER_S *)malloc(sizeof(AENC_AAC_ENCODER_S));
    if (pstEncoder == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "no memory");
        return CVI_ERR_AENC_NOMEM;
    }
    memset(pstEncoder, 0, sizeof(AENC_AAC_ENCODER_S));
    *ppEncoder = (CVI_VOID *)pstEncoder;

    /* set default config to encoder */
    s32Ret = AACInitDefaultConfig_Adp(&config);
    if (s32Ret)
    {
        free(pstEncoder);
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "s32Ret:0x%x.#########\n", s32Ret);
        return s32Ret;
    }

    config.coderFormat = (AuEncoderFormat)pstAttr->enAACType;
    config.bitRate = pstAttr->enBitRate;
    config.bitsPerSample = 8 * (1 << (pstAttr->enBitWidth));
    config.sampleRate = pstAttr->enSmpRate;
    config.bandWidth = pstAttr->s16BandWidth; //config.sampleRate/2;
    config.transtype = (AACENCTransportType)pstAttr->enTransType;
    APP_PROF_LOG_PRINT(LEVEL_ERROR, "[OpenAACEncoder][%d][%d][%d][%d][%d][%d]\n",
        config.coderFormat,
        config.bitRate,
        config.bitsPerSample,
        config.sampleRate,
        config.bandWidth,
        config.transtype);
    if (pstAttr->enSoundMode ==  AUDIO_SOUND_MODE_MONO &&
        pstAttr->enAACType != AAC_TYPE_EAACPLUS)
    {
        config.nChannelsIn = 1;
        config.nChannelsOut = 1;
    }
    else
    {
        config.nChannelsIn = 2;
        config.nChannelsOut = 2;
    }

    config.quality = AU_QualityHigh;


    s32Ret = AencAACCheckConfig(&config);
    if (s32Ret)
    {
        free(pstEncoder);
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "#########\n");
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }

    /* create encoder */
    s32Ret = AACEncoderOpen_Adp(&pstEncoder->pstAACState, &config);
    if (s32Ret)
    {
        free(pstEncoder);
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "s32Ret:0x%x.#########\n", s32Ret);
        return s32Ret;
    }

    memcpy(&pstEncoder->stAACAttr, pstAttr, sizeof(AENC_ATTR_AAC_S));

    APP_PROF_LOG_PRINT(LEVEL_WARN, "entering .....\n");
    return CVI_SUCCESS;
}

CVI_S32 EncodeAACFrm(CVI_VOID *pEncoder, CVI_S16 *inputdata,
                            CVI_U8 *pu8Outbuf,
                            CVI_S32 s32InputSizeBytes, CVI_U32 *pu32OutLen)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    AENC_AAC_ENCODER_S *pstEncoder = CVI_NULL;
    CVI_U32 u32PtNums; //samples number
    //CVI_S32 i;
    CVI_U32 u32WaterLine;

    CVI_AUDIO_ASSERT(pEncoder != CVI_NULL);
    CVI_AUDIO_ASSERT(inputdata != CVI_NULL);
    CVI_AUDIO_ASSERT(pu8Outbuf != CVI_NULL);
    CVI_AUDIO_ASSERT(pu32OutLen != CVI_NULL);

    pstEncoder = (AENC_AAC_ENCODER_S *)pEncoder;


    /*WaterLine, equals to the frame sample frame of protocol*/
    if (pstEncoder->stAACAttr.enAACType == AAC_TYPE_AACLC)
    {
        u32WaterLine = AACLC_SAMPLES_PER_FRAME;
    }
    else if (pstEncoder->stAACAttr.enAACType == AAC_TYPE_EAAC ||
             pstEncoder->stAACAttr.enAACType == AAC_TYPE_EAACPLUS)
    {
        u32WaterLine = AACPLUS_SAMPLES_PER_FRAME;
    }
    else if (pstEncoder->stAACAttr.enAACType == AAC_TYPE_AACLD ||
             pstEncoder->stAACAttr.enAACType == AAC_TYPE_AACELD)
    {
        u32WaterLine = AACLD_SAMPLES_PER_FRAME;
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n","invalid AAC coder type");
        return CVI_ERR_AENC_ILLEGAL_PARAM;
    }
    /* calculate point number */
    u32PtNums = s32InputSizeBytes / 2;

    //CVI_S32 s32InputBytes = u32WaterLine * 2;  //input bytes per frame
    s32Ret = AACEncoderFrame_Adp(pstEncoder->pstAACState, inputdata, pu8Outbuf, s32InputSizeBytes,
            (CVI_S32 *)pu32OutLen);

    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%su32WaterLine[%d] u32PtNums[%d]\n",
                "AAC encode failed",
                u32WaterLine,
                u32PtNums);
    }

    return s32Ret;
}

CVI_S32 CloseAACEncoder(CVI_VOID *pEncoder)
{
    AENC_AAC_ENCODER_S *pstEncoder = CVI_NULL;

    CVI_AUDIO_ASSERT(pEncoder != CVI_NULL);
    pstEncoder = (AENC_AAC_ENCODER_S *)pEncoder;

    AACEncoderClose_Adp(pstEncoder->pstAACState);

    free(pstEncoder);
    return CVI_SUCCESS;
}

CVI_S32 OpenAACDecoder(CVI_VOID *pDecoderAttr, CVI_VOID **ppDecoder)
{
    ADEC_AAC_DECODER_S *pstDecoder = CVI_NULL;
    ADEC_ATTR_AAC_S *pstAttr = CVI_NULL;
    CVI_BOOL bRawMode = CVI_FALSE;
    // if bRawMode is CVI_TRUE, means there are no any headers in stream.

    CVI_AUDIO_ASSERT(pDecoderAttr != CVI_NULL);
    CVI_AUDIO_ASSERT(ppDecoder != CVI_NULL);

    pstAttr = (ADEC_ATTR_AAC_S *)pDecoderAttr;

    /* allocate memory for decoder */
    pstDecoder = (ADEC_AAC_DECODER_S *)malloc(sizeof(ADEC_AAC_DECODER_S));
    if (pstDecoder == CVI_NULL)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "no memory");
        return CVI_ERR_ADEC_NOMEM;
    }
    memset(pstDecoder, 0, sizeof(ADEC_AAC_DECODER_S));
    *ppDecoder = (CVI_VOID *)pstDecoder;

    /* create decoder */

    if (bRawMode == CVI_FALSE)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "~~~~~~~~ADEC trans type:%d ~~~~~~~~~\n", pstAttr->enTransType);
        pstDecoder->pstAACState = AACInitDecoder_Adp((AACDECTransportType)pstAttr->enTransType);
        if (!pstDecoder->pstAACState)
        {
            free(pstDecoder);
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "AACInitDecoder failed");
            return CVI_ERR_ADEC_DECODER_ERR;
        }
    }
    else
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "~~~~~~~~ADEC trans type: No header / Raw Mode~~~~~~~~~\n");
        pstDecoder->pstAACState = AACInitDecoder_Adp((AACDECTransportType)AAC_TRANS_TYPE_BUTT);
        if (!pstDecoder->pstAACState)
        {
            free(pstDecoder);
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "AACInitDecoder failed");
            return CVI_ERR_ADEC_DECODER_ERR;
        }
        AACSetRawMode_Adp(pstDecoder->pstAACState,
                          (CVI_S32)(pstAttr->enSoundMode + 1),
                          (CVI_S32)pstAttr->enSmpRate);
    }

    memcpy(&pstDecoder->stAACAttr, pstAttr, sizeof(ADEC_ATTR_AAC_S));
    return CVI_SUCCESS;
}

CVI_S32 DecodeAACFrm(CVI_VOID *pDecoder, CVI_U8 **pu8Inbuf, CVI_S32 *ps32LeftByte,
            CVI_U16 *pu16Outbuf, CVI_U32 *pu32OutLen, CVI_U32 *pu32Chns)
{
    CVI_S32 s32Ret = CVI_SUCCESS;
    ADEC_AAC_DECODER_S *pstDecoder = CVI_NULL;
    CVI_S32 s32Samples, s32SampleBytes;
    AACFrameInfo aacFrameInfo;

    CVI_AUDIO_ASSERT(pDecoder != CVI_NULL);
    CVI_AUDIO_ASSERT(pu8Inbuf != CVI_NULL);
    CVI_AUDIO_ASSERT(ps32LeftByte != CVI_NULL);
    CVI_AUDIO_ASSERT(pu16Outbuf != CVI_NULL);
    CVI_AUDIO_ASSERT(pu32OutLen != CVI_NULL);
    CVI_AUDIO_ASSERT(pu32Chns != CVI_NULL);

    pstDecoder = (ADEC_AAC_DECODER_S *)pDecoder;

    /*Notes: pInbuf will updated*/
    s32Ret = AACDecodeFrame_Adp(pstDecoder->pstAACState, pu8Inbuf, ps32LeftByte, (CVI_S16 *)pu16Outbuf);

    if (s32Ret)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "aac decoder failed!, s32Ret:0x%x\n", s32Ret);
        return s32Ret;
    }

    AACGetLastFrameInfo_Adp(pstDecoder->pstAACState, &aacFrameInfo);
    aacFrameInfo.nChans = ((aacFrameInfo.nChans != 0) ? aacFrameInfo.nChans : 1);
    /* samples per frame of one sound track*/
    s32Samples = aacFrameInfo.outputSamps;

    if ((s32Samples != AACLC_SAMPLES_PER_FRAME) &&
        (s32Samples != AACPLUS_SAMPLES_PER_FRAME) &&
        (s32Samples != AACLD_SAMPLES_PER_FRAME))
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "aac decoder failed! [%d]\n", s32Samples);
        return CVI_ERR_ADEC_DECODER_ERR;
    }

    s32SampleBytes = s32Samples * sizeof(CVI_U16) * aacFrameInfo.nChans;
    *pu32Chns = aacFrameInfo.nChans;
    //APP_PROF_LOG_PRINT(LEVEL_INFO, "s32Samples[%d] pu32Chns[%d]\n", s32Samples, *pu32Chns);
    *pu32OutLen = s32SampleBytes;

    /* NOTICE: our audio frame format is same as AAC decoder L/L/L/... R/R/R/...*/
    return s32Ret;
}

CVI_S32 GetAACFrmInfo(CVI_VOID *pDecoder, CVI_VOID *pInfo)
{
    ADEC_AAC_DECODER_S *pstDecoder = CVI_NULL;
    AACFrameInfo aacFrameInfo;
    AAC_FRAME_INFO_S *pstAacFrm = CVI_NULL;

    CVI_AUDIO_ASSERT(pDecoder != CVI_NULL);
    CVI_AUDIO_ASSERT(pInfo != CVI_NULL);

    pstDecoder = (ADEC_AAC_DECODER_S *)pDecoder;
    pstAacFrm = (AAC_FRAME_INFO_S *)pInfo;

    AACGetLastFrameInfo_Adp(pstDecoder->pstAACState, &aacFrameInfo);

    pstAacFrm->s32Samplerate = aacFrameInfo.sampRateOut;
    pstAacFrm->s32BitRate = aacFrameInfo.bitRate;
    pstAacFrm->s32Profile = aacFrameInfo.profile;
    pstAacFrm->s32TnsUsed = aacFrameInfo.tnsUsed;
    pstAacFrm->s32PnsUsed = aacFrameInfo.pnsUsed;

    return aacFrameInfo.bytespassDec;
}

CVI_S32 CloseAACDecoder(CVI_VOID *pDecoder)
{
    ADEC_AAC_DECODER_S *pstDecoder = CVI_NULL;

    CVI_AUDIO_ASSERT(pDecoder != CVI_NULL);
    pstDecoder = (ADEC_AAC_DECODER_S *)pDecoder;

    AACFreeDecoder_Adp(pstDecoder->pstAACState);

    free(pstDecoder);

    return CVI_SUCCESS;
}

CVI_S32 ResetAACDecoder(CVI_VOID *pDecoder)
{
    ADEC_AAC_DECODER_S *pstDecoder = CVI_NULL;

    CVI_AUDIO_ASSERT(pDecoder != CVI_NULL);
    pstDecoder = (ADEC_AAC_DECODER_S *)pDecoder;

    AACFreeDecoder_Adp(pstDecoder->pstAACState);

    /* create decoder */
    pstDecoder->pstAACState = AACInitDecoder_Adp((AACDECTransportType)pstDecoder->stAACAttr.enTransType);
    if (!pstDecoder->pstAACState)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[Info]:%s\n", "AACResetDecoder failed");
        return CVI_ERR_ADEC_DECODER_ERR;
    }

    return CVI_SUCCESS;
}

CVI_S32 CVI_MPI_AENC_AacInit(CVI_VOID)
{
    CVI_S32 s32Handle, s32Ret;
    AAC_AENC_ENCODER_S stAacEnc;

    s32Ret = InitAacAencLib();
    if (s32Ret != CVI_SUCCESS)
    {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "Error in AAC encode\n");
        return 0;
    }

    stAacEnc.enType = PT_AAC;
    snprintf(stAacEnc.aszName, sizeof(stAacEnc.aszName), "Aac");
    stAacEnc.u32MaxFrmLen = 100;//MAX_AAC_MAINBUF_SIZE;
    stAacEnc.pfnOpenEncoder = OpenAACEncoder;
    stAacEnc.pfnEncodeFrm = EncodeAACFrm;
    stAacEnc.pfnCloseEncoder = CloseAACEncoder;
    s32Ret = CVI_AENC_RegisterExternalEncoder(&s32Handle, &stAacEnc);
    if (s32Ret)
    {
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_WARN, "start register AAC encoder end\n");
    g_AacEncHandle = s32Handle;
    return CVI_SUCCESS;
}

CVI_S32 CVI_MPI_AENC_AacDeInit(CVI_VOID)
{
    CVI_S32 s32Ret;
    s32Ret = CVI_AENC_UnRegisterExternalEncoder(g_AacEncHandle);
    if (s32Ret)
    {
        return s32Ret;
    }

    DeInitAacAencLib();
    return CVI_SUCCESS;
}

CVI_S32 CVI_MPI_ADEC_AacInit(CVI_VOID)
{
    CVI_S32 s32Handle, s32Ret;
    ADEC_DECODER_S stAacDec;

    s32Ret = InitAacAdecLib();
    if (s32Ret)
    {
        return s32Ret;
    }

    stAacDec.enType = PT_AAC;
    snprintf(stAacDec.aszName, sizeof(stAacDec.aszName), "Aac");
    stAacDec.pfnOpenDecoder = OpenAACDecoder;
    stAacDec.pfnDecodeFrm = DecodeAACFrm;
    stAacDec.pfnGetFrmInfo = GetAACFrmInfo;
    stAacDec.pfnCloseDecoder = CloseAACDecoder;
    stAacDec.pfnResetDecoder = ResetAACDecoder;
    s32Ret = CVI_ADEC_RegisterExternalDecoder(&s32Handle, &stAacDec);
    if (s32Ret)
    {
        return s32Ret;
    }

    APP_PROF_LOG_PRINT(LEVEL_WARN, "start register AAC decoder end\n");
    g_AacDecHandle = s32Handle;
    return CVI_SUCCESS;
}

CVI_S32 CVI_MPI_ADEC_AacDeInit(CVI_VOID)
{
    CVI_S32 s32Ret;
    s32Ret = CVI_ADEC_UnRegisterExternalDecoder(g_AacDecHandle);
    if (s32Ret)
    {
        return s32Ret;
    }

    DeInitAacAdecLib();
    return CVI_SUCCESS;
}
