#include "app_ipcam_record.h"
#include "cvi_recorder.h"
#include "cvi_venc.h"
#include "app_ipcam_paramparse.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/statfs.h>
#include "cvi_file_recover.h"
#include <sys/types.h>
#include <dirent.h>


#define RECORD_VENC_CH 0
#define POSTRECTIMESEC 10
#define PRERECTIMESEC 0
#define RECPRESIZE 20
#define RECSHORTFILEMS 10
#define RECMEMPRESEC 0
#define RECSPLITIMEMSEC 5 * 60 * 1000//MS 
#define SDPATH "/mnt/sd"
#define RECORD_SDPATH SDPATH"/record"
#define RECORD_RECOVER_FLAG "_back"
#define RECORD_FILE_TYPE ".mov"
#define SD_FREESIZE_MINI 400 //Mb

static int g_runStatus = 0;
CVI_RECORDER_HANDLE_T g_Video_recorder = NULL;
CVI_RECORDER_ATTR_T g_rec_attr;

static int record_event_cb(CVI_REC_EVENT_E event_type, const char *filename, void *p)
{
    char * _tmpPoint = NULL;
    unsigned int _tmpLen = 0;
    char _tmpString[256] = {0};
    if (event_type == CVI_REC_EVENT_STOP) {
        //note stop record to rename record back note
        if(filename != NULL) {
            _tmpPoint = strstr(filename, RECORD_RECOVER_FLAG);
            if (_tmpPoint) {
                _tmpLen = (unsigned int)(_tmpPoint - filename);
                if (_tmpLen > sizeof(_tmpString)) {
                    _tmpLen = sizeof(_tmpString);
                }
                strncpy(_tmpString, filename , _tmpLen);
                strcat(_tmpString, RECORD_FILE_TYPE);
                rename(filename, _tmpString);
            }
        }
    }
    return 0;
}

static bool is_ts_file(int file_type) {
    return false;//no ts
}

static int rec_stop_all_callback(void *param){
    return 0;
}

static int mem_buffer_stop_callback(void *param) {
    return 0;
}

int CVI_Record_GetStreamStatus(int enType, VENC_PACK_S *ppack, bool *is_I_frame)
{
	H265E_NALU_TYPE_E H265Type;
	H264E_NALU_TYPE_E H264Type;
	*is_I_frame = 0;

	if ((enType != PT_H265) &&
		(enType != PT_H264)) {
		return 0;
	}

	if (enType == PT_H265) {
		H265Type = ppack->DataType.enH265EType;
		if (H265Type == H265E_NALU_ISLICE ||
			H265Type == H265E_NALU_IDRSLICE ||
			H265Type == H265E_NALU_SPS ||
			H265Type == H265E_NALU_VPS ||
			H265Type == H265E_NALU_PPS ||
			H265Type == H265E_NALU_SEI){
			*is_I_frame = 1;
		}
	} else if (enType == PT_H264) {
		H264Type = ppack->DataType.enH264EType;
		if (H264Type == H264E_NALU_ISLICE ||
			H264Type == H264E_NALU_SPS ||
			H264Type == H264E_NALU_IDRSLICE ||
			H264Type == H264E_NALU_SEI ||
			H264Type == H264E_NALU_PPS){
			*is_I_frame = 1;
		}
	}

	return 0;
}

int app_ipcam_Record_VideoInput(int enType, VENC_STREAM_S * pstStream)
{
    //MP4
    if (!g_Video_recorder || g_runStatus == 0) {
        return -1;
    } 
    CVI_FRAME_STREAM_T frame_stream;
    bool iskey = 0;
    for (unsigned i = 0; i < pstStream->u32PackCount; i++) {
        VENC_PACK_S *ppack;
        ppack = &pstStream->pstPack[i];
        frame_stream.data[i] = ppack->pu8Addr + ppack->u32Offset;
        frame_stream.len[i] = ppack->u32Len - ppack->u32Offset;
        frame_stream.vi_pts[i] = pstStream->pstPack[i].u64PTS;
        CVI_Record_GetStreamStatus(enType, ppack, &iskey);
        frame_stream.vftype[i] = iskey;
    }
    frame_stream.num = pstStream->u32PackCount;
    frame_stream.type = CVI_MUXER_FRAME_TYPE_VIDEO;
    frame_stream.thumbnail_len = 0;
    frame_stream.thumbnail_data = NULL;
    cvi_recorder_send_frame(g_Video_recorder, &frame_stream);
    return 0;
}

int app_ipcam_Record_AudioInput(AUDIO_FRAME_S * pstStream)
{
    if (!g_Video_recorder || g_runStatus == 0) {
        return -1;
    }
    CVI_FRAME_STREAM_T frame;
    memset(&frame, 0x0, sizeof(CVI_FRAME_STREAM_T));
    frame.type = CVI_MUXER_FRAME_TYPE_AUDIO;
    frame.num = 1;
    frame.data[0] = pstStream->u64VirAddr[0];
    //frame.len[0] = pstStream->u32Len / s32sampleBitWidth;
    frame.len[0] = pstStream->u32Len;
    frame.vi_pts[0] = pstStream->u64TimeStamp;
    cvi_recorder_send_frame(g_Video_recorder, &frame);
    return 0;
}

static int recorder_request_idr_cb(void *param) {
    APP_PROF_LOG_PRINT(LEVEL_INFO, "RS[%d] recorder_request_idr_cb request idr for new video\n", 0);
    CVI_VENC_RequestIDR(0, 1);
    return 0;
}

#if 0
static int get_dtcf_type(int file_type) {
    //CVI_RECORD_SERVICE_RECORD_FILE_TYPE_MP4 = 0,
    return 0;
}
#endif

static int recorder_get_filename_cb(void *param, char *filename, int filename_len) {
    struct statfs diskInfo;
    unsigned long long availableDisk = 0;
    //当剩余空间小于200M 时候只覆盖第一个录像文件
    if (access(RECORD_SDPATH, F_OK) != 0) {
        mkdir(RECORD_SDPATH, 0666);
    }
    static int recordNum = 0;
    if (statfs(SDPATH, &diskInfo) == -1) {
        snprintf(filename, filename_len, "%s/CvitekRecord%d%s",RECORD_SDPATH, 0, RECORD_FILE_TYPE);
        if(access(filename, F_OK) == 0) {
            remove(filename);
            sync();
            snprintf(filename, filename_len, "%s/CvitekRecord%d%s%s",RECORD_SDPATH, 0, RECORD_RECOVER_FLAG, RECORD_FILE_TYPE);
            return 0;
        }
    } else {
        availableDisk = (unsigned long long)(diskInfo.f_bfree) * (unsigned long long)(diskInfo.f_bsize);
        availableDisk = availableDisk /1024 /1024;
        APP_PROF_LOG_PRINT(LEVEL_INFO, "The availableDisk is %lld \r\n", availableDisk);
        if(availableDisk < SD_FREESIZE_MINI) {
            snprintf(filename, filename_len, "%s/CvitekRecord%d%s",RECORD_SDPATH, 0, RECORD_FILE_TYPE);
            remove(filename);
            sync();
            snprintf(filename, filename_len, "%s/CvitekRecord%d%s%s",RECORD_SDPATH, 0, RECORD_RECOVER_FLAG, RECORD_FILE_TYPE);
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "The SDCare free space %lld over write %s \r\n", availableDisk, filename);
            return 0;
        }
    }
    snprintf(filename, filename_len, "%s/CvitekRecord%d%s",RECORD_SDPATH, recordNum, RECORD_FILE_TYPE);
    if(access(filename, F_OK) == 0) {
        while(access(filename, F_OK) == 0)
        {
            recordNum++;
            snprintf(filename, filename_len, "%s/CvitekRecord%d%s",RECORD_SDPATH, recordNum, RECORD_FILE_TYPE);
        }
        recordNum++;
    } else {
        recordNum++;
    }
    snprintf(filename, filename_len, "%s/CvitekRecord%d%s%s",RECORD_SDPATH, recordNum-1, RECORD_RECOVER_FLAG, RECORD_FILE_TYPE);//补充back
    return 0;
}

static inline int get_video_buffer_size(int bitrate_kbps, int buffer_sec) {
    // Ratio * (bit rate * buffer duration) + thumbnail size
    return (1.5 * ((bitrate_kbps * buffer_sec) >> 3) + 500);
}

static inline int get_audio_pcm_buffer_size(int sample_rate, int channels, int sample_size, int buffer_sec) {
    return (sample_rate * channels * sample_size * buffer_sec * 6);
}

static int cvi_master_create_rec() {
    APP_PROF_LOG_PRINT(LEVEL_INFO, "R: Create Recorder\r\n");
    //CVI_RECORDER_ATTR_T rec_attr;
    APP_VENC_CHN_CFG_S * pstVencChnCfg = app_ipcam_VencChnCfg_Get(RECORD_VENC_CH);
#ifdef AUDIO_SUPPORT
    APP_PARAM_AUDIO_CFG_T * pstAudioCfg = app_ipcam_Audio_Param_Get();
#endif
    memset(&g_rec_attr, 0x00, sizeof(CVI_RECORDER_ATTR_T));
    g_rec_attr.astStreamAttr.u32TrackCnt = 2;
    //memcpy(&g_rec_attr.astStreamAttr, &p->astStreamAttr[0], sizeof(CVI_REC_STREAM_ATTR_S));
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].enable = 1;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].enTrackType = CVI_TRACK_SOURCE_TYPE_VIDEO;
    if (pstVencChnCfg->enType == PT_H264) {
        g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.enCodecType = CVI_TRACK_VIDEO_CODEC_H264;
    } else if(pstVencChnCfg->enType == PT_H265) {
        g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.enCodecType = CVI_TRACK_VIDEO_CODEC_H265;
    }
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32Width = pstVencChnCfg->u32Width;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32Height = pstVencChnCfg->u32Height;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32BitRate = pstVencChnCfg->u32BitRate;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.fFrameRate = pstVencChnCfg->u32DstFrameRate;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32Gop = pstVencChnCfg->u32Gop;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.fSpeed = 0;
#ifdef AUDIO_SUPPORT
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].enable = 1;
    switch(pstAudioCfg->astAudioCfg.enBitwidth) {
        case AUDIO_BIT_WIDTH_8:
            g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u16SampleBitWidth = 8;
        break;
        case AUDIO_BIT_WIDTH_16:
            g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u16SampleBitWidth = 16;
        break;
        case AUDIO_BIT_WIDTH_24:
            g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u16SampleBitWidth = 24;
        break;
        case AUDIO_BIT_WIDTH_32:
            g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u16SampleBitWidth = 32;
        break;
        default:
            g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u16SampleBitWidth = 16;
        break;
    }
    //only support AAC and PCM
    if (pstAudioCfg->astAudioCfg.enAencType == PT_AAC) {
        g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.enCodecType = CVI_TRACK_AUDIO_CODEC_AAC;
    } else {
        g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.enCodecType = CVI_TRACK_AUDIO_CODEC_ADPCM;
    }
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32SampleRate = pstAudioCfg->astAudioCfg.enSamplerate;
    if (pstAudioCfg->astAudioCfg.enSoundmode == AUDIO_SOUND_MODE_MONO) {
        g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32ChnCnt = 1;
    } else {
        g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32ChnCnt = 2;
    }
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame = pstAudioCfg->astAudioCfg.u32PtNumPerFrm;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.fFramerate = 1;
#else
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].enable = 0; 
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.enCodecType = CVI_TRACK_AUDIO_CODEC_ADPCM;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32ChnCnt = 2;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32SampleRate = 16000;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32AvgBytesPerSec = 0;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame = 320;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u16SampleBitWidth = 16;
    g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.fFramerate = 1;
#endif
    g_rec_attr.enRecType = CVI_REC_TYPE_NORMAL;
    CVI_Track_Source_S *handle = &g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO];
    {
        CVI_Track_Source_S *thandle = &g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO];
        if(g_rec_attr.enRecType == CVI_REC_TYPE_NORMAL){
            thandle->enable = 1;
        }
        thandle->unTrackSourceAttr.stAudioInfo.u32ChnCnt = handle->unTrackSourceAttr.stAudioInfo.u32ChnCnt;
        thandle->unTrackSourceAttr.stAudioInfo.u32SampleRate = handle->unTrackSourceAttr.stAudioInfo.u32SampleRate;
        if (CVI_TRACK_AUDIO_CODEC_ADPCM == handle->unTrackSourceAttr.stAudioInfo.enCodecType) {
            thandle->unTrackSourceAttr.stAudioInfo.fFramerate =
                (float)handle->unTrackSourceAttr.stAudioInfo.u32SampleRate / handle->unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame;
        } else if (CVI_TRACK_AUDIO_CODEC_AAC == handle->unTrackSourceAttr.stAudioInfo.enCodecType) {
            thandle->unTrackSourceAttr.stAudioInfo.fFramerate =
                (float)handle->unTrackSourceAttr.stAudioInfo.u32SampleRate / handle->unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame;
        }
        APP_PROF_LOG_PRINT(LEVEL_WARN, "audio u32SampleRate %u u32SamplesPerFrame %u\r\n", thandle->unTrackSourceAttr.stAudioInfo.u32SampleRate, thandle->unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame);
        APP_PROF_LOG_PRINT(LEVEL_WARN, "audio en %d enRecType %d fFramerate %f\r\n", thandle->enable, g_rec_attr.enRecType, thandle->unTrackSourceAttr.stAudioInfo.fFramerate);
    }

    g_rec_attr.fncallback.pfn_request_idr = (CVI_REQUEST_IDR_CALLBACK_FN_PTR)recorder_request_idr_cb;
    g_rec_attr.fncallback.pfn_request_idr_param = NULL;
    g_rec_attr.enable_subtitle = 0;

    g_rec_attr.enable_file_alignment = !is_ts_file(0);
    g_rec_attr.fncallback.pfn_get_filename = NULL;
    g_rec_attr.fncallback.pfn_get_filename = (CVI_GET_FILENAME_CALLBACK_FN_PTR)recorder_get_filename_cb;
    //g_rec_attr.fncallback.pfn_remove_file_callback = (CVI_REC_REMOVE_FILE_CALLBACK_FN_PTR)p->stCallback.pfnRemoveFile;
    //g_rec_attr.fncallback.pfn_add_file_callback = (CVI_REC_ADD_FILE_CALLBACK_FN_PTR)p->stCallback.pfnAddFile;
    g_rec_attr.fncallback.pfn_remove_file_callback = NULL;
    g_rec_attr.fncallback.pfn_add_file_callback = NULL;
    //recorder_get_filename_param_t *fparam;
    if(g_rec_attr.enRecType == CVI_REC_TYPE_NORMAL) {
        g_rec_attr.fncallback.pfn_event_callback[REC_TYPE_NORMAL] = record_event_cb;
        //g_rec_attr.fncallback.pfn_event_callback[REC_TYPE_NORMAL] = p->stCallback.pfnNormalRecCallback;
        g_rec_attr.fncallback.pfn_event_callback_param = NULL;
        #if 0
        fparam = (recorder_get_filename_param_t *)malloc(sizeof(recorder_get_filename_param_t));
        //fparam->rs = rs;
        fparam->rs = NULL;
        fparam->dir_type = 0;
        fparam->file_type = get_dtcf_type(0);
        #endif
        g_rec_attr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_NORMAL] = recorder_get_filename_cb;
        g_rec_attr.fncallback.pfn_event_callback[REC_TYPE_EVENT] = NULL;
        //g_rec_attr.fncallback.pfn_event_callback[REC_TYPE_EVENT] = p->stCallback.pfnEventRecCallback;
        g_rec_attr.u32PostRecTimeSec = POSTRECTIMESEC;
        g_rec_attr.u32PreRecTimeSec = PRERECTIMESEC;
        #if 0
        fparam = (recorder_get_filename_param_t *)malloc(sizeof(recorder_get_filename_param_t));
        fparam->rs = NULL;
        fparam->dir_type = 0;
        fparam->file_type = get_dtcf_type(0);
        #endif
        g_rec_attr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_EVENT] = NULL;
    } else {
        return -1;
    }
    g_rec_attr.device_model = NULL;
    g_rec_attr.short_file_ms = RECSHORTFILEMS;
    g_rec_attr.prealloc_size = RECPRESIZE;//20M
    g_rec_attr.s32MemRecPreSec = RECMEMPRESEC;
    g_rec_attr.stSplitAttr.enSplitType = CVI_REC_SPLIT_TYPE_TIME;
    g_rec_attr.stSplitAttr.u64SplitTimeLenMSec = RECSPLITIMEMSEC;
    //g_rec_attr.stSplitAttr.u64SplitTimeLenMSec = p->stSplitAttr.u64SplitTimeLenMSec;

    g_rec_attr.fncallback.pfn_mem_buffer_stop_callback = mem_buffer_stop_callback;
    g_rec_attr.fncallback.pfn_mem_buffer_stop_callback_param = NULL;
    g_rec_attr.fncallback.pfn_rec_stop_callback = rec_stop_all_callback;
    g_rec_attr.fncallback.pfn_rec_stop_callback_param = NULL;

    CVI_S32 presec = ((g_rec_attr.u32PreRecTimeSec >= (CVI_U32)g_rec_attr.s32MemRecPreSec) ? g_rec_attr.u32PreRecTimeSec : (CVI_U32)g_rec_attr.s32MemRecPreSec);
    if(presec == 0 || g_rec_attr.enRecType == CVI_REC_TYPE_LAPSE) {
        presec = 1;
    }else {
        presec += 1;
    }
    CVI_U32 bitrate = g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32BitRate;
    CVI_U32 sampleRate = g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32SampleRate;
    CVI_U32 chns = g_rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32ChnCnt;
    APP_PROF_LOG_PRINT(LEVEL_WARN, "video bitrate %u audio sampleRate %u chns %u presec %d\r\n", bitrate, sampleRate, chns, presec);
    g_rec_attr.stRbufAttr[CVI_RECORDER_RBUF_VIDEO].size = get_video_buffer_size(bitrate, presec) * 1024;
    g_rec_attr.stRbufAttr[CVI_RECORDER_RBUF_VIDEO].name = (const CVI_CHAR *)"rs_v";
    g_rec_attr.stRbufAttr[CVI_RECORDER_RBUF_AUDIO].size =  get_audio_pcm_buffer_size(sampleRate, chns, 2, presec);
    g_rec_attr.stRbufAttr[CVI_RECORDER_RBUF_AUDIO].name = (const CVI_CHAR *)"rs_a";
    g_rec_attr.stRbufAttr[CVI_RECORDER_RBUF_SUBTITLE].size = 0;
    g_rec_attr.stRbufAttr[CVI_RECORDER_RBUF_SUBTITLE].name = (const CVI_CHAR *)"rs_s";
#if 0
    if(rec_attr.enRecType == CVI_REC_TYPE_LAPSE)
    {
        float ffps = rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.fFrameRate;
        ffps = ffps * rec_attr.unRecAttr.stLapseRecAttr.u32IntervalMs / 1000;
        int fps = (int)rec_attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.fFrameRate;
        fps = fps * rec_attr.unRecAttr.stLapseRecAttr.u32IntervalMs / 1000;
        float diff = ffps - (float)fps;
        if(diff < 0.001)
        {
            gstRecFrameInfo[rs->id].diff = 0.0;
            gstRecFrameInfo[rs->id].flag = -1;
        }
        else
        {
            gstRecFrameInfo[0].diff = diff;
            gstRecFrameInfo[0].flag = (int)(1.0 / diff);
        }
        gstRecFrameInfo[0].ifps = fps;
        gstRecFrameInfo[0].writecounts = 0;
        gstRecFrameInfo[0].totalcounts = 0;
        gstRecFrameInfo[0].off = 0;
        gstRecFrameInfo[0].index = 0;
    }
#endif
    g_rec_attr.id = 0;
    cvi_recorder_create(&g_Video_recorder, &g_rec_attr);
    if(g_rec_attr.enRecType == CVI_REC_TYPE_NORMAL)
    {
        //if(p->astStreamAttr[0].aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].enable == 1){
        //    stop_mute(rs);
        //}else{
        //    start_mute(rs);
        //}
    }
    return 0;
}

static int start_normal_rec()
{
    return cvi_recorder_start_normal_rec(g_Video_recorder);
}

static int record_recover(const char * filePath)
{
    int ret = 0;
    CVI_FILE_RECOVER_HANDLE handle = NULL;
    char * _tmpPoint = NULL;
    unsigned int _tmpLen = 0;
    char _tmpString[256] = {0};

    ret = CVI_FILE_RECOVER_Create(&handle);
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "record_recover err ret %#x \r\n", ret);
        return -1;
    }
    ret = CVI_FILE_RECOVER_Open(handle, filePath);
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_FILE_RECOVER_Open %s err ret %#x \r\n",filePath, ret);
        goto exit;
    }
    _tmpPoint = strstr(filePath, RECORD_RECOVER_FLAG);
    if (_tmpPoint) {
        _tmpLen = (unsigned int)(_tmpPoint - filePath);
        if (_tmpLen > sizeof(_tmpString)) {
            _tmpLen = sizeof(_tmpString);
        }
        strncpy(_tmpString, filePath , _tmpLen);
        strcat(_tmpString, RECORD_FILE_TYPE);
    }
    ret = CVI_FILE_RECOVER_Recover(handle, _tmpString, "", false);
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_FILE_RECOVER_Recover %s err ret %#x \r\n",filePath, ret);
    }
    remove(filePath);
exit:
    CVI_FILE_RECOVER_Destroy(&handle);
    return ret;
}

int app_ipcam_Record_Recover_Init()
{
    char filepath[512] = {0};
    DIR  *dirp = NULL;
    struct dirent * entryp;

    if ((dirp = opendir(RECORD_SDPATH)) == NULL) {
        return -1;
    }
    while ((entryp = readdir(dirp)) != NULL) {
        if (entryp->d_type == DT_DIR || strcmp(entryp->d_name, ".") == 0 || strcmp(entryp->d_name, "..") == 0) {
            continue;
        }
        snprintf(filepath, sizeof(filepath), "%s/%s", RECORD_SDPATH, entryp->d_name);
        if (strstr(filepath, RECORD_RECOVER_FLAG)) {
            record_recover(filepath);
            sync();
        }
    }
    return 0;
}

int app_ipcam_Record_Init()
{
    if (g_runStatus == 0) {
        cvi_master_create_rec();
        if (start_normal_rec() == 0) {
            g_runStatus = 1;
        }
    }
    return 0;
}

int app_ipcam_Record_UnInit()
{
    g_runStatus = 0;
    cvi_recorder_stop_normal_rec(g_Video_recorder);
    cvi_recorder_destroy(g_Video_recorder);
    g_Video_recorder = NULL;
    return 0;
}
