
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "cvi_recorder.h"
#include "cvi_muxer.h"
#include "filesync.h"
#include "cvi_rbuf.h"
#include "app_ipcam_comm.h"
#include <pthread.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CVI_CHECK_CTX_NULL(x) \
do{\
    if(!x) return -1; \
}while(0)

typedef struct cviTASK_INFO_T{
    //cvi_osal_task_handle_t task;
    pthread_t taskId;
    int exitflag;
}CVI_TASK_INFO_T;


typedef struct cviRECORDER_CTX_T{
    CVI_TASK_INFO_T task[REC_TYPE_BUTT];
    CVI_MUXER_ATTR_T stMuxerAttr;
    CVI_RECORDER_ATTR_T stRecAttr;
    void *rbuf[CVI_MUXER_FRAME_TYPE_BUTT];
    volatile uint64_t startTimeStamp[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile uint64_t curTimeStamp[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile uint64_t targetTimeStamp[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile int64_t ptsInBuf[CVI_MUXER_FRAME_TYPE_BUTT];
    volatile int64_t ptsInLastfile[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile int64_t ptsInfile[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile int64_t ptsPerfile[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile int32_t isFirstFrame[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    int32_t gopInx;
    uint32_t appendAudioCnt[REC_TYPE_BUTT];
    volatile int32_t dropIframeFlag;
    char filename[REC_TYPE_BUTT][128];
    char nextFilename[REC_TYPE_BUTT][128];
    int32_t fileCnt[REC_TYPE_BUTT];
    volatile int32_t recStartFlag[REC_TYPE_BUTT];
    volatile int32_t recMemStartFlag;
    void *muxer[REC_TYPE_BUTT];
    volatile int32_t recSplit;
    int32_t postStopFlag;
    volatile int32_t manualStopFlag;
    void *packet[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    int32_t packetSize[REC_TYPE_BUTT][CVI_MUXER_FRAME_TYPE_BUTT];
    volatile uint64_t firstFramePts[CVI_MUXER_FRAME_TYPE_BUTT];
    uint64_t fduration[CVI_MUXER_FRAME_TYPE_BUTT];
    uint64_t rec_start_time[REC_TYPE_BUTT];
    pthread_mutex_t mutex[REC_TYPE_BUTT];
}CVI_RECORDER_CTX_T;

#define CHECK_SPLIT_REC_AHEAD_TIME (50 * 1000)
#define RBUF_LOW_WATER 700 * 1024

#define CVI_REC_ALIGN(s, n) (((s) + (n) - 1) & (~((n) - 1)))
#define CVI_REC_ALIGN_LEN (8)

#define CVI_OSAL_PRI_RT_HIGH       ((int)80)

static int muxer_event_callback(CVI_MUXER_EVENT_E event_type, const char *filename, void *p, void *extend) {
    CVI_RECORDER_ATTR_T *attr = (CVI_RECORDER_ATTR_T *)p;
    CVI_REC_EVENT_CALLBACK_FN_PTR pfn_callback = NULL;
    void *param = NULL;
    if(attr->enRecType == CVI_REC_TYPE_LAPSE) {
        pfn_callback = attr->fncallback.pfn_event_callback[REC_TYPE_LAPSE];
    }else {
        if(strcasestr(filename, "emr") == NULL) {
            pfn_callback = attr->fncallback.pfn_event_callback[REC_TYPE_NORMAL];
        }else {
            pfn_callback = attr->fncallback.pfn_event_callback[REC_TYPE_EVENT];
        }
    }

    if (!pfn_callback) {
        return 0;
    }

    param = attr->fncallback.pfn_event_callback_param;

    if (event_type == CVI_MUXER_SEND_FRAME_FAILED) {
        pfn_callback(CVI_REC_EVENT_WRITE_FRAME_FAILED, filename, param);
    } else if (event_type == CVI_MUXER_SEND_FRAME_TIMEOUT) {
        CVI_REC_EVENT_WRITE_FRAME_TIMEOUT_MS_S timeout = {
            .timeout_ms = *(int *)extend,
            .param = param
        };

        pfn_callback(CVI_REC_EVENT_WRITE_FRAME_TIMEOUT, filename, &timeout);
    } else if (event_type == CVI_MUXER_PTS_JUMP) {
        pfn_callback(CVI_REC_EVENT_WRITE_FRAME_DROP, filename, param);
    } else if (event_type == CVI_MUXER_STOP) {
        if (strcasestr(filename, "emr") != NULL) {
            pfn_callback(CVI_REC_EVENT_END_EMR, filename, param);
        } else {
            pfn_callback(CVI_REC_EVENT_SYNC_DONE, filename, param);
        }
    } else if (event_type == CVI_MUXER_OPEN_FILE_FAILED) {
        pfn_callback(CVI_REC_EVENT_OPEN_FILE_FAILED, filename, param);
    }
    return 0;
}

uint64_t cvi_recorder_get_us(void){
    struct timespec tv;
    // CLOCK_THREAD_CPUTIME_ID CLOCK_BOOTTIME CLOCK_MONOTONIC
    clock_gettime(CLOCK_BOOTTIME, &tv);
    uint64_t s = tv.tv_sec;
    return s * 1000 * 1000 + tv.tv_nsec / 1000;
}

static int cvi_recorder_is_short_file(void *recorder, int type){
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    if(ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] - ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] <= ctx->stRecAttr.short_file_ms * 1000) {
        return 1;
    }
    return 0;
}

static int cvi_recorder_is_audio_en(CVI_RECORDER_ATTR_T attr) {
    return attr.astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].enable;
}

static int cvi_recorder_is_subtitle_en(CVI_RECORDER_ATTR_T attr) {
    return attr.enable_subtitle;
}

static int cvi_recorder_is_key_next_frame(CVI_RECORDER_CTX_T *ctx, int type){
    CVI_FRAME_INFO_T frame;
    memset(&frame, 0x0, sizeof(CVI_FRAME_INFO_T));
return 1;
    int ret = cvi_rbuf_copy_out(ctx->rbuf[CVI_MUXER_FRAME_TYPE_VIDEO], (void *)&frame, sizeof(CVI_FRAME_INFO_T), 0, type);
    if(ret != 0) {
        return 0;
    }

    if(frame.hmagic != 0x5a5a5a5a || frame.tmagic != 0x5a5a5a5a) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "mem of crossing the line\r\n");
        return 0;
    }
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d isKey = %d, gopinx %d\r\n", ctx->stRecAttr.id, frame.isKey, frame.gopInx);
    return frame.isKey;
}

static int cvi_recorder_is_split(CVI_RECORDER_CTX_T *ctx, int type)
{
    uint64_t t = CHECK_SPLIT_REC_AHEAD_TIME;
    if(ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_VIDEO] < ctx->ptsPerfile[type][CVI_MUXER_FRAME_TYPE_VIDEO]){
        return 0;
    }

    // if(cvi_recorder_is_audio_en(ctx->stRecAttr) &&
    //     ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_AUDIO] < ctx->ptsPerfile[type][CVI_MUXER_FRAME_TYPE_AUDIO]){
    //     return 0;
    // }

    uint64_t vduration = 0;
    vduration = ctx->ptsPerfile[type][CVI_MUXER_FRAME_TYPE_VIDEO] * ctx->fduration[CVI_MUXER_FRAME_TYPE_VIDEO];
    // if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_NORMAL){
    //     if(ctx->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO] <= 0 || ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO] <= 0) {
    //         return 0;
    //     }
    //     vduration = ctx->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO] - ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO];
    // }else{
    //     if(ctx->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] <= 0 || ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] <= 0) {
    //         return 0;
    //     }
    //     vduration = ctx->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] - ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO];
    // }
    uint64_t cur = cvi_recorder_get_us();

    // if(cvi_recorder_is_subtitle_en(ctx->stRecAttr) &&
    //     ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_SUBTITLE] < ctx->ptsPerfile[type][CVI_MUXER_FRAME_TYPE_SUBTITLE]){
    //     return 0;
    // }

    // if(!cvi_recorder_is_key_next_frame(ctx, type)){
    //     return 0;
    // }

    // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d %lu %ld %ld", ctx->stRecAttr.id, cur - ctx->rec_start_time[type],
    //     ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_VIDEO], ctx->ptsPerfile[type][CVI_MUXER_FRAME_TYPE_AUDIO]);
    // return 1;


    // if(ctx->stRecAttr.id == 1){
    //     t = t + 50 * 1000;
    // }

    // if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_LAPSE){
    //     t = 0;
    // }
    if(cur >= ctx->rec_start_time[type] + vduration - t){
        if(cvi_recorder_is_key_next_frame(ctx, type)) {
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d %lu %lu %lu\r\n", ctx->stRecAttr.id, ctx->rec_start_time[type], cur, vduration);
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d %lu %ld %ld\r\n", ctx->stRecAttr.id, cur - ctx->rec_start_time[type],
        ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_VIDEO], ctx->ptsPerfile[type][CVI_MUXER_FRAME_TYPE_AUDIO]);
            return 1;
        }
    }
    return 0;
}

static void *cvi_recorder_get_frame(CVI_RECORDER_CTX_T *ctx, CVI_MUXER_FRAME_TYPE_E ftype, REC_TYPE_INDEX_E type) {

    CVI_FRAME_INFO_T frame;
    void *f = NULL;

    int ret = cvi_rbuf_copy_out(ctx->rbuf[ftype], (void *)&frame, sizeof(CVI_FRAME_INFO_T), 0, type);
    if(ret != 0) {
        return f;
    }

    if(frame.hmagic != 0x5a5a5a5a || frame.tmagic != 0x5a5a5a5a) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "mem of crossing the line %#x %#x %d\r\n", frame.hmagic, frame.tmagic, frame.dataLen);
        cvi_rbuf_show_log(ctx->rbuf[ftype]);
        cvi_rbuf_reset(ctx->rbuf[ftype]);
        return f;
    }

    if(ctx->startTimeStamp[type][ftype] == 0){
        ctx->startTimeStamp[type][ftype] = frame.vpts;
        if(type == 0) {
            ctx->targetTimeStamp[type][ftype] = ctx->stRecAttr.stSplitAttr.u64SplitTimeLenMSec * 1000 + ctx->startTimeStamp[type][ftype];
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d reset startTimeStamp %lu targetTimeStamp %lu\r\n",
                    ctx->stRecAttr.id, ctx->startTimeStamp[type][ftype], ctx->targetTimeStamp[type][ftype]);
        }else{
            ctx->targetTimeStamp[type][ftype] = ctx->stRecAttr.u32PostRecTimeSec * 1000 * 1000 + ctx->stRecAttr.u32PreRecTimeSec * 1000 * 1000 + ctx->startTimeStamp[type][ftype];
            if(ctx->filename[type][0] != '\0')
                APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d reset startTimeStamp %lu targetTimeStamp %lu\r\n",
                    ctx->stRecAttr.id, ctx->startTimeStamp[type][ftype], ctx->targetTimeStamp[type][ftype]);
        }
    }

    /*solutions for loss frame by full buffer*/
    // if(frame.pts > ctx->ptsInfile[type][ftype] + ctx->ptsInLastfile[type][ftype]) {
    //     // APP_PROF_LOG_PRINT(LEVEL_ERROR, "type %d frame loss occurred: %ld != %ld", type, frame.pts, ctx->ptsInfile[type][ftype] + ctx->ptsInLastfile[type][ftype]);
    //     ctx->ptsInfile[type][ftype]++;
    //     ctx->curTimeStamp[type][ftype] = frame.vpts + ctx->fduration[ftype];
    //     return f;
    // } else if(frame.pts < ctx->ptsInfile[type][ftype] + ctx->ptsInLastfile[type][ftype]) {
    //     // APP_PROF_LOG_PRINT(LEVEL_ERROR, "type %d frame error: %ld != %ld", type, frame.pts, ctx->ptsInfile[type][ftype] + ctx->ptsInLastfile[type][ftype]);
    //     cvi_rbuf_refresh_out(ctx->rbuf[ftype], frame.totalSize, type);
    //     return f;
    // }

    if(ctx->isFirstFrame[type][ftype] == 0 && frame.isKey == 1) {
        if(ftype != CVI_MUXER_FRAME_TYPE_VIDEO && ctx->isFirstFrame[type][CVI_MUXER_FRAME_TYPE_VIDEO] == 0){
            ctx->ptsInLastfile[type][ftype]++;
            ctx->targetTimeStamp[type][ftype] = frame.vpts + ctx->fduration[ftype];
            cvi_rbuf_refresh_out(ctx->rbuf[ftype], frame.totalSize, type);
            // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d video iframe not recv", ctx->stRecAttr.id);
            return f;
        }
        ctx->isFirstFrame[type][ftype] = 1;
    }else if(ctx->isFirstFrame[type][ftype] == 0){
        ctx->ptsInLastfile[type][ftype]++;
        ctx->targetTimeStamp[type][ftype] = frame.vpts + ctx->fduration[ftype];
        cvi_rbuf_refresh_out(ctx->rbuf[ftype], frame.totalSize, type);
        // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d first frame must be Iframe", ctx->stRecAttr.id);
        return f;
    }

    if(frame.dataLen > 0) {
        if(type == 0 && ctx->recSplit == 1 && frame.type == CVI_MUXER_FRAME_TYPE_VIDEO) {
            ctx->recSplit = 0;
        }

        frame.pts = ctx->ptsInfile[type][ftype];

        int32_t totalSize = frame.totalSize;
        if(ftype == CVI_MUXER_FRAME_TYPE_AUDIO){
            totalSize += CVI_MUXER_EXT_DATA_LEN;
        }

        if(totalSize > ctx->packetSize[type][ftype]) {
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d totalSize %d packetSize %d\r\n", ctx->stRecAttr.id, totalSize, ctx->packetSize[type][ftype]);
            if(ctx->packet[type][ftype]) free(ctx->packet[type][ftype]);
            ctx->packet[type][ftype] = NULL;
            if(ftype == CVI_MUXER_FRAME_TYPE_VIDEO) totalSize += 100 * 1024;
            void *p = malloc(totalSize);
            if(p == NULL){
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "%d out of memory %d !!!!!!!\r\n", ctx->stRecAttr.id, totalSize);
                ctx->packetSize[type][ftype] = 0;
                return f;
            }
            ctx->packet[type][ftype] = p;
            ctx->packetSize[type][ftype] = totalSize;
        }

        int off = 0;
        char *buf = (char *)(ctx->packet[type][ftype]);
        memcpy(buf + off, &frame, sizeof(CVI_FRAME_INFO_T));
        off += sizeof(CVI_FRAME_INFO_T);

        if(ftype == CVI_MUXER_FRAME_TYPE_AUDIO){
            memcpy(buf + off, g_ext_audio_data, CVI_MUXER_EXT_DATA_LEN);
            ret = cvi_rbuf_copy_out(ctx->rbuf[ftype], buf + off + CVI_MUXER_EXT_DATA_LEN, frame.dataLen, off, type);
        }else{
            ret = cvi_rbuf_copy_out(ctx->rbuf[ftype], buf + off, frame.totalSize - off, off, type);
        }

        cvi_rbuf_refresh_out(ctx->rbuf[ftype], frame.totalSize, type);

        f = buf;
        if(ctx->ptsInfile[type][ftype] == 0 && frame.isKey == 1 && ftype == CVI_MUXER_FRAME_TYPE_VIDEO){
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "####### [%d] first thmb len %d\n", ctx->stRecAttr.id, frame.extraLen);
        }
        ctx->curTimeStamp[type][ftype] = frame.vpts + ctx->fduration[ftype];
        ctx->ptsInfile[type][ftype]++;
    }else{
        cvi_rbuf_refresh_out(ctx->rbuf[ftype], frame.totalSize, type);
    }
    return f;
}

static CVI_MUXER_FRAME_TYPE_E cvi_recorder_get_frame_type(CVI_RECORDER_CTX_T *ctx, int type) {
    if(!cvi_recorder_is_audio_en(ctx->stRecAttr) && !cvi_recorder_is_subtitle_en(ctx->stRecAttr)){
        return CVI_MUXER_FRAME_TYPE_VIDEO;
    }

    uint64_t duration[CVI_MUXER_FRAME_TYPE_BUTT] = {0, 0, 0, 0};
    // if(ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] == 0 || ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] == 0) {
    //     duration[CVI_MUXER_FRAME_TYPE_VIDEO] = 0;
    // }else {
    //     duration[CVI_MUXER_FRAME_TYPE_VIDEO] = ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] - ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO];
    // }

    if(ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_VIDEO] == 0){
        duration[CVI_MUXER_FRAME_TYPE_VIDEO] = 0;
        return CVI_MUXER_FRAME_TYPE_VIDEO;
    }else{
        duration[CVI_MUXER_FRAME_TYPE_VIDEO] = ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_VIDEO] * ctx->fduration[CVI_MUXER_FRAME_TYPE_VIDEO];
    }

    if(cvi_recorder_is_audio_en(ctx->stRecAttr)){
        // if(ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO] == 0 || ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO] == 0) {
        //     duration[CVI_MUXER_FRAME_TYPE_AUDIO] = 0;
        // }else {
        //     duration[CVI_MUXER_FRAME_TYPE_AUDIO] = ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO] - ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_AUDIO];
        // }
        if(ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_AUDIO] == 0){
            duration[CVI_MUXER_FRAME_TYPE_AUDIO] = 0;
        }else{
            duration[CVI_MUXER_FRAME_TYPE_AUDIO] =
                (ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_AUDIO] - ctx->appendAudioCnt[type]) * ctx->fduration[CVI_MUXER_FRAME_TYPE_AUDIO];
        }
    }

    if(cvi_recorder_is_subtitle_en(ctx->stRecAttr)){
        // if(ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_SUBTITLE] == 0 || ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_SUBTITLE] == 0) {
        //     duration[CVI_MUXER_FRAME_TYPE_SUBTITLE] = 0;
        // }else {
        //     duration[CVI_MUXER_FRAME_TYPE_SUBTITLE] = ctx->curTimeStamp[type][CVI_MUXER_FRAME_TYPE_SUBTITLE] - ctx->startTimeStamp[type][CVI_MUXER_FRAME_TYPE_SUBTITLE];
        // }
        if(ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_SUBTITLE] == 0){
            duration[CVI_MUXER_FRAME_TYPE_SUBTITLE] = 0;
        }else{
            duration[CVI_MUXER_FRAME_TYPE_SUBTITLE] = ctx->ptsInfile[type][CVI_MUXER_FRAME_TYPE_SUBTITLE] * ctx->fduration[CVI_MUXER_FRAME_TYPE_SUBTITLE];
        }
    }

    if(cvi_recorder_is_audio_en(ctx->stRecAttr) && duration[CVI_MUXER_FRAME_TYPE_AUDIO] < duration[CVI_MUXER_FRAME_TYPE_VIDEO]){
        return CVI_MUXER_FRAME_TYPE_AUDIO;
    }else if(cvi_recorder_is_subtitle_en(ctx->stRecAttr) && duration[CVI_MUXER_FRAME_TYPE_SUBTITLE] < duration[CVI_MUXER_FRAME_TYPE_VIDEO]){
        return CVI_MUXER_FRAME_TYPE_SUBTITLE;
    }

    return CVI_MUXER_FRAME_TYPE_VIDEO;
}

static int cvi_recorder_auto_split(CVI_RECORDER_CTX_T *ctx, int type){

    ctx->rec_start_time[type] = cvi_recorder_get_us();

    for(int i = 0; i < CVI_MUXER_FRAME_TYPE_BUTT; i++) {
        if(CVI_MUXER_FRAME_TYPE_THUMBNAIL == i || (CVI_MUXER_FRAME_TYPE_SUBTITLE == i && ctx->stRecAttr.enable_subtitle == 0))
            continue;
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d type %d ptsInfile %ld ptsInLastfile %ld\r\n", ctx->stRecAttr.id, type, ctx->ptsInfile[type][i], ctx->ptsInLastfile[type][i]);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d type %d startTimeStamp %lu targetTimeStamp %lu\r\n", ctx->stRecAttr.id, type, ctx->startTimeStamp[type][i], ctx->targetTimeStamp[type][i]);
        ctx->ptsInLastfile[type][i] += ctx->ptsInfile[type][i];
        ctx->ptsInfile[type][i] = 0;
        ctx->isFirstFrame[type][i] = 0;
        ctx->startTimeStamp[type][i] = ctx->curTimeStamp[type][i];
        ctx->targetTimeStamp[type][i] = ctx->stRecAttr.stSplitAttr.u64SplitTimeLenMSec * 1000 + ctx->startTimeStamp[type][i];
    }

    ctx->fileCnt[type]++;
    memset(ctx->nextFilename[type], 0x0, sizeof(ctx->nextFilename[type]));
    if(type != CVI_CALLBACK_TYPE_EVENT && ctx->manualStopFlag == 0){
        if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_NORMAL)
            ctx->stRecAttr.fncallback.pfn_get_filename(ctx->stRecAttr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_NORMAL], ctx->nextFilename[type], sizeof(ctx->nextFilename[type]) - 1);
        else if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_LAPSE)
            ctx->stRecAttr.fncallback.pfn_get_filename(ctx->stRecAttr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_LAPSE], ctx->nextFilename[type], sizeof(ctx->nextFilename[type]) - 1);
    }
    cvi_muxer_stop(ctx->muxer[type]);

    CVI_REC_EVENT_CALLBACK_FN_PTR callback = ctx->stRecAttr.fncallback.pfn_event_callback[type];

    if(cvi_recorder_is_short_file((void *)ctx, type)) {
        if (callback)
            callback(CVI_REC_EVENT_SHORT_FILE, ctx->filename[type], ctx->stRecAttr.fncallback.pfn_event_callback_param);
    }

    if(type != REC_TYPE_EVENT) {
        //callback(CVI_REC_EVENT_SPLIT, ctx->filename[t], ctx->stRecAttr.fncallback.pfn_event_callback_param);
        // callback(CVI_REC_EVENT_SPLIT_START, ctx->filename[t], ctx->stRecAttr.fncallback.pfn_event_callback_param);
        if(ctx->recSplit == 0) {
            if (callback)
                callback(CVI_REC_EVENT_STOP, ctx->filename[type], ctx->stRecAttr.fncallback.pfn_event_callback_param);
        }
        ctx->recSplit = 1;
        //callback(CVI_REC_EVENT_SYNC_DONE, ctx->filename[t], ctx->stRecAttr.fncallback.pfn_event_callback_param);
    }
    else {
        if (callback)
            callback(CVI_REC_EVENT_STOP, ctx->filename[type], ctx->stRecAttr.fncallback.pfn_event_callback_param);
        //callback = ctx->stRecAttr.fncallback.pfn_event_callback[CVI_CALLBACK_TYPE_NORMAL];
        //callback(CVI_REC_EVENT_END_EMR, ctx->filename[t], ctx->stRecAttr.fncallback.pfn_event_callback_param);
    }
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "stop fileCnt %d filename %s\r\n", ctx->fileCnt[type], ctx->filename[type]);
    sync_push(ctx->filename[type]);
    memset(ctx->filename[type], 0x0, sizeof(ctx->filename[type]));
    return 0;
}

static void cvi_recorder_append_audio_frame(CVI_RECORDER_CTX_T *ctx, REC_TYPE_INDEX_E type){
    uint32_t vcnt = cvi_rbuf_data_cnt(ctx->rbuf[CVI_MUXER_FRAME_TYPE_VIDEO], type);
    uint32_t acnt = cvi_rbuf_data_cnt(ctx->rbuf[CVI_MUXER_FRAME_TYPE_AUDIO], type);
    uint64_t vdua = ctx->fduration[CVI_MUXER_FRAME_TYPE_VIDEO];
    uint64_t adua = ctx->fduration[CVI_MUXER_FRAME_TYPE_AUDIO];
#define REC_AV_DUA_INBUF_DIFF 150 * 1000
#define REC_APPEND_AUDIO_CNT 2
    if(vcnt * vdua + REC_AV_DUA_INBUF_DIFF < acnt * adua){
        for(int i = 0; i < REC_APPEND_AUDIO_CNT; i++){
            void *f = cvi_recorder_get_frame(ctx, CVI_MUXER_FRAME_TYPE_AUDIO, type);
            if(f){
                cvi_muxer_write_packet(ctx->muxer[type], (CVI_FRAME_INFO_T *)f);
                ctx->appendAudioCnt[type]++;
            }
        }
    }
}

static void * cvi_recorder_event_task(void *arg) {
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)arg;

    REC_TYPE_INDEX_E rec_type = REC_TYPE_EVENT;
    void *f = NULL;

    while(!ctx->task[rec_type].exitflag){
        pthread_mutex_lock(&ctx->mutex[rec_type]);
#define RECORDER_PRETIME 50 * 1000
        if(ctx->recStartFlag[rec_type] == 0){
            for(int i = 0; i < CVI_MUXER_FRAME_TYPE_BUTT; i++){
                int fcnt = cvi_rbuf_data_cnt(ctx->rbuf[i], rec_type);
                if(fcnt * ctx->fduration[i] > ctx->stRecAttr.u32PreRecTimeSec * 1000 * 1000 + RECORDER_PRETIME){
                    CVI_FRAME_INFO_T frame;
                    int ret = cvi_rbuf_copy_out(ctx->rbuf[i], (void *)&frame, sizeof(CVI_FRAME_INFO_T), 0, rec_type);
                    if(ret == 0){
                        cvi_rbuf_refresh_out(ctx->rbuf[i], frame.totalSize, rec_type);
                        ctx->ptsInLastfile[rec_type][i]++;
                    }
                }
            }
            pthread_mutex_unlock(&ctx->mutex[rec_type]);
            usleep(20 * 1000);
            continue;
        }

        CVI_MUXER_FRAME_TYPE_E frame_type = cvi_recorder_get_frame_type(ctx, rec_type);
        f = cvi_recorder_get_frame(ctx, frame_type, rec_type);
        if(f){
            cvi_muxer_write_packet(ctx->muxer[rec_type], (CVI_FRAME_INFO_T *)f);
        }

        if (cvi_recorder_is_split(ctx, rec_type)) {
            cvi_recorder_auto_split(ctx, rec_type);
            ctx->recStartFlag[rec_type] = 0;
        }

        pthread_mutex_unlock(&ctx->mutex[rec_type]);

        usleep(10 * 1000);
    }
    return 0;
}

static void * cvi_recorder_task(void *arg) {
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)arg;

    REC_TYPE_INDEX_E rec_type = REC_TYPE_NORMAL;
    unsigned int frameCount = 0;
    int ret = 0;

    while(!ctx->task[rec_type].exitflag){
        pthread_mutex_lock(&ctx->mutex[rec_type]);
        if(ctx->recStartFlag[rec_type] == 0 && ctx->recMemStartFlag == 0) {
            pthread_mutex_unlock(&ctx->mutex[rec_type]);
            usleep(20 * 1000);
            continue;
        }
        uint64_t start = cvi_recorder_get_us() / 1000;
        CVI_MUXER_FRAME_TYPE_E frame_type = cvi_recorder_get_frame_type(ctx, rec_type);
        int fcnt = cvi_rbuf_data_cnt(ctx->rbuf[frame_type], rec_type);
        if(fcnt < 1){
            pthread_mutex_unlock(&ctx->mutex[rec_type]);
            usleep(20 * 1000);
            continue;
        }
        void *f = cvi_recorder_get_frame(ctx, frame_type, rec_type);
        if(f){
            ret = cvi_muxer_write_packet(ctx->muxer[rec_type], (CVI_FRAME_INFO_T *)f);
            if(frameCount++ % 2000 == 0 || access("/tmp/rbuf_debug", F_OK) == 0){
                APP_PROF_LOG_PRINT(LEVEL_DEBUG, "show %d rbuf info:\r\n", ctx->stRecAttr.id);
                remove("/tmp/rbuf_debug");
                cvi_rbuf_show_log(ctx->rbuf[CVI_MUXER_FRAME_TYPE_VIDEO]);
                cvi_rbuf_show_log(ctx->rbuf[CVI_MUXER_FRAME_TYPE_AUDIO]);
                cvi_rbuf_show_log(ctx->rbuf[CVI_MUXER_FRAME_TYPE_SUBTITLE]);
            }
        }
        uint64_t end = cvi_recorder_get_us() / 1000;
        uint64_t t = end - start;
        if(t > 80) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%d %d]: write_file take [%lu] ms\r\n", ctx->stRecAttr.id, frame_type, t);
            if(t > 200){
                muxer_event_callback(CVI_MUXER_SEND_FRAME_TIMEOUT, ctx->filename[rec_type], (void *)&ctx->stRecAttr, (void *)&t);
            }
        }
        cvi_recorder_append_audio_frame(ctx, rec_type);
        if(cvi_recorder_is_split(ctx, rec_type)) {
            cvi_recorder_auto_split(ctx, rec_type);
            pthread_mutex_unlock(&ctx->mutex[rec_type]);
            if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_NORMAL) {
                cvi_recorder_start_normal_rec((void *)ctx);
            } else {
                cvi_recorder_start_lapse_rec((void *)ctx);
            }
        }else{
            pthread_mutex_unlock(&ctx->mutex[rec_type]);
        }
        if(ret < 0){
            usleep(20 * 1000);
        }
        usleep(5 * 1000);
    }
    return 0;
}


void cvi_recorder_destroy(void *recorder) {
    if(!recorder){
        return;
    }

    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    while(ctx->manualStopFlag) {
        usleep(20 * 1000);
    }

    for(int i = 0; i < REC_TYPE_BUTT; i++) {
        ctx->task[i].exitflag = 1;
        pthread_join(ctx->task[i].taskId, NULL);
    }
    for(int i = 0; i < REC_TYPE_BUTT; i++) {
        pthread_mutex_lock(&ctx->mutex[i]);
        if(ctx->muxer[i] != NULL) {
            if(ctx->recStartFlag[i] == 1) {
                ctx->recStartFlag[i] = 0;
                cvi_muxer_stop(ctx->muxer[i]);
            }
            cvi_muxer_destroy(ctx->muxer[i]);
            ctx->muxer[i] = NULL;
        }
        pthread_mutex_unlock(&ctx->mutex[i]);
    }

    for(int j = 0; j < REC_TYPE_BUTT; j++){
        pthread_mutex_lock(&ctx->mutex[j]);
        for(int i = 0; i < CVI_MUXER_FRAME_TYPE_BUTT; i++) {
            if(ctx->rbuf[i] != NULL){
                cvi_rbuf_deinit(ctx->rbuf[i]);
                ctx->rbuf[i] = NULL;
            }
            if(ctx->packet[j][i]) {
                free(ctx->packet[j][i]);
                ctx->packet[j][i] = NULL;
            }
        }
        pthread_mutex_unlock(&ctx->mutex[j]);
    }

    for(int i = 0; i < REC_TYPE_BUTT; i++){
        pthread_mutex_destroy(&ctx->mutex[i]);
    }

    for(int i = 0; i < CVI_CALLBACK_TYPE_BUTT; i++){
        if(ctx->stRecAttr.fncallback.pfn_get_filename_param[i] != NULL) {
            //free(ctx->stRecAttr.fncallback.pfn_get_filename_param[i]);
            ctx->stRecAttr.fncallback.pfn_get_filename_param[i] = NULL;
        }
    }

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d recorder type %d destroy\r\n", ctx->stRecAttr.id, ctx->stRecAttr.enRecType);
    free(ctx);
}

int cvi_recorder_create(void **recorder, CVI_RECORDER_ATTR_T *attr){
    int rc = -1;
    //cvi_osal_task_attr_t ta;
    void *rbuf = NULL;
    uint32_t size = 0;
    static char n_name[16] = {0};
    static char e_name[16] = {0};

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "sizeof(CVI_RECORDER_CTX_T) %lu\r\n", sizeof(CVI_RECORDER_CTX_T));
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)malloc(sizeof(CVI_RECORDER_CTX_T));
    if(ctx == NULL){
        goto FAILED;
    }
    memset(ctx, 0x0, sizeof(CVI_RECORDER_CTX_T));
    memcpy(&ctx->stRecAttr, attr, sizeof(CVI_RECORDER_ATTR_T));
    ctx->stMuxerAttr.devmod = attr->device_model;
    ctx->stMuxerAttr.alignflag = attr->enable_file_alignment;
    ctx->stMuxerAttr.presize = attr->prealloc_size;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "prealloc_size=%d\r\n", ctx->stMuxerAttr.presize);
    ctx->stMuxerAttr.stvideocodec.en = 1;
    ctx->stMuxerAttr.stvideocodec.codec = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.enCodecType;
    ctx->stMuxerAttr.stvideocodec.framerate = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.fFrameRate;
    ctx->ptsPerfile[REC_TYPE_NORMAL][CVI_MUXER_FRAME_TYPE_VIDEO] = attr->stSplitAttr.u64SplitTimeLenMSec / 1000 * ctx->stMuxerAttr.stvideocodec.framerate;
    ctx->fduration[CVI_MUXER_FRAME_TYPE_VIDEO] = 1000 * 1000 / ctx->stMuxerAttr.stvideocodec.framerate;
    if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_LAPSE){
        ctx->stMuxerAttr.stvideocodec.framerate = attr->unRecAttr.stLapseRecAttr.fFramerate;
        ctx->stRecAttr.stSplitAttr.u64SplitTimeLenMSec = attr->stSplitAttr.u64SplitTimeLenMSec * ctx->stMuxerAttr.stvideocodec.framerate * attr->unRecAttr.stLapseRecAttr.u32IntervalMs / 1000;
        ctx->ptsPerfile[REC_TYPE_LAPSE][CVI_MUXER_FRAME_TYPE_VIDEO] = attr->stSplitAttr.u64SplitTimeLenMSec / 1000 * ctx->stMuxerAttr.stvideocodec.framerate;
    }
    ctx->stMuxerAttr.stvideocodec.w = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32Width;
    ctx->stMuxerAttr.stvideocodec.h = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_VIDEO].unTrackSourceAttr.stVideoInfo.u32Height;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "video duration per frame %lu ptsPerfile %ld\r\n", ctx->fduration[CVI_MUXER_FRAME_TYPE_VIDEO], ctx->ptsPerfile[REC_TYPE_NORMAL][CVI_MUXER_FRAME_TYPE_VIDEO]);
    ctx->stMuxerAttr.staudiocodec.en = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].enable;
    ctx->stMuxerAttr.staudiocodec.chns = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32ChnCnt;
    ctx->stMuxerAttr.staudiocodec.samplerate = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.u32SampleRate;
    ctx->stMuxerAttr.staudiocodec.framerate = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.fFramerate;
    ctx->stMuxerAttr.staudiocodec.codec = attr->astStreamAttr.aHTrackSrcHandle[CVI_TRACK_SOURCE_TYPE_AUDIO].unTrackSourceAttr.stAudioInfo.enCodecType;
    ctx->fduration[CVI_MUXER_FRAME_TYPE_AUDIO] = 1000 * 1000 / ctx->stMuxerAttr.staudiocodec.framerate;
    ctx->ptsPerfile[REC_TYPE_NORMAL][CVI_MUXER_FRAME_TYPE_AUDIO] = attr->stSplitAttr.u64SplitTimeLenMSec / 1000 * ctx->stMuxerAttr.staudiocodec.framerate;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "audio duration per frame %lu ptsPerfile %ld\r\n", ctx->fduration[CVI_MUXER_FRAME_TYPE_AUDIO], ctx->ptsPerfile[REC_TYPE_NORMAL][CVI_MUXER_FRAME_TYPE_AUDIO]);

    ctx->stMuxerAttr.stthumbnailcodec.en = 1;

    ctx->stMuxerAttr.stsubtitlecodec.en = attr->enable_subtitle;
    ctx->stMuxerAttr.stsubtitlecodec.framerate = 1;
    ctx->stMuxerAttr.stsubtitlecodec.timebase = 1000000;
    ctx->fduration[CVI_MUXER_FRAME_TYPE_SUBTITLE] = 1000 * 1000 / ctx->stMuxerAttr.stsubtitlecodec.framerate;
    ctx->ptsPerfile[REC_TYPE_NORMAL][CVI_MUXER_FRAME_TYPE_SUBTITLE] = attr->stSplitAttr.u64SplitTimeLenMSec / 1000 * ctx->stMuxerAttr.stsubtitlecodec.framerate;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "subtitle duration per frame %lu ptsPerfile %ld\r\n", ctx->fduration[CVI_MUXER_FRAME_TYPE_SUBTITLE], ctx->ptsPerfile[REC_TYPE_NORMAL][CVI_MUXER_FRAME_TYPE_SUBTITLE]);

    ctx->ptsPerfile[REC_TYPE_EVENT][CVI_MUXER_FRAME_TYPE_VIDEO] =
        (ctx->stRecAttr.u32PreRecTimeSec + ctx->stRecAttr.u32PostRecTimeSec) * ctx->stMuxerAttr.stvideocodec.framerate;
    ctx->ptsPerfile[REC_TYPE_EVENT][CVI_MUXER_FRAME_TYPE_AUDIO] =
        (ctx->stRecAttr.u32PreRecTimeSec + ctx->stRecAttr.u32PostRecTimeSec)* ctx->stMuxerAttr.stvideocodec.framerate;
    ctx->ptsPerfile[REC_TYPE_EVENT][CVI_MUXER_FRAME_TYPE_SUBTITLE] =
        (ctx->stRecAttr.u32PreRecTimeSec + ctx->stRecAttr.u32PostRecTimeSec)* ctx->stMuxerAttr.stsubtitlecodec.framerate;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "event ptsPerfile %ld %ld %ld\r\n", ctx->ptsPerfile[REC_TYPE_EVENT][CVI_MUXER_FRAME_TYPE_VIDEO],
        ctx->ptsPerfile[REC_TYPE_EVENT][CVI_MUXER_FRAME_TYPE_AUDIO], ctx->ptsPerfile[REC_TYPE_EVENT][CVI_MUXER_FRAME_TYPE_SUBTITLE]);

    ctx->stMuxerAttr.pfncallback = muxer_event_callback;
    ctx->stMuxerAttr.pfnparam = (void *)&ctx->stRecAttr;

    //if(ctx->stRecAttr.enRecType == CVI_REC_TYPE_LAPSE){
    //    rc = cvi_muxer_create(ctx->stMuxerAttr, &ctx->muxer[REC_TYPE_LAPSE]);
    //}else {
        rc = cvi_muxer_create(ctx->stMuxerAttr, &ctx->muxer[REC_TYPE_NORMAL]);
        ctx->stMuxerAttr.presize = attr->prealloc_size / 2;
        //rc |= cvi_muxer_create(ctx->stMuxerAttr, &ctx->muxer[REC_TYPE_EVENT]);
        //ctx->stMuxerAttr.presize = attr->prealloc_size;
    //}

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "enRecType = %d, enSplitType %d, u64SplitTimeLenMSec = %lu\r\n", ctx->stRecAttr.enRecType, ctx->stRecAttr.stSplitAttr.enSplitType, ctx->stRecAttr.stSplitAttr.u64SplitTimeLenMSec);
    size = attr->stRbufAttr[CVI_RECORDER_RBUF_VIDEO].size;
    if(cvi_rbuf_init(&rbuf, size, attr->stRbufAttr[CVI_MUXER_FRAME_TYPE_VIDEO].name)){
        goto FAILED;
    }
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "video rbuf %p %d\r\n", rbuf, size);
    ctx->rbuf[CVI_MUXER_FRAME_TYPE_VIDEO] = rbuf;
    ctx->recSplit = 1;
    ctx->dropIframeFlag = 0;

    if(ctx->stMuxerAttr.staudiocodec.en){
        size = attr->stRbufAttr[CVI_RECORDER_RBUF_AUDIO].size;
        rbuf = ctx->rbuf[CVI_MUXER_FRAME_TYPE_AUDIO];
        if(cvi_rbuf_init(&rbuf, size, attr->stRbufAttr[CVI_MUXER_FRAME_TYPE_AUDIO].name)){
            goto FAILED;
        }
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "audio rbuf %p %d\r\n", rbuf, size);
        ctx->rbuf[CVI_MUXER_FRAME_TYPE_AUDIO] = rbuf;
    }
    if(ctx->stMuxerAttr.stsubtitlecodec.en) {
        size = attr->stRbufAttr[CVI_RECORDER_RBUF_SUBTITLE].size;
        rbuf = ctx->rbuf[CVI_MUXER_FRAME_TYPE_SUBTITLE];
        if(cvi_rbuf_init(&rbuf, size, attr->stRbufAttr[CVI_MUXER_FRAME_TYPE_SUBTITLE].name)){
            goto FAILED;
        }
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "subtitle rbuf %p %d\r\n", rbuf, size);
        ctx->rbuf[CVI_MUXER_FRAME_TYPE_SUBTITLE] = rbuf;
    }
    for(int i = 0; i < REC_TYPE_BUTT; i++){
        pthread_mutex_init(&ctx->mutex[i], NULL);
    }

    snprintf(n_name, sizeof(n_name), "rc_normal_%d\r\n", attr->id);
    ctx->task[0].exitflag = 0;
    //ta.name = n_name;
    //ta.entry = cvi_recorder_task;
    //ta.param = (void *)ctx;
    //ta.priority = CVI_OSAL_PRI_RT_HIGH;
    //ta.detached = false;
    //rc = cvi_osal_task_create(&ta, &ctx->task[REC_TYPE_NORMAL].task);
    pthread_attr_t record_task_attr;
    struct sched_param record_sched_param = {0};
    pthread_attr_init(&record_task_attr);
    pthread_attr_setschedpolicy(&record_task_attr, SCHED_RR);
    record_sched_param.sched_priority = CVI_OSAL_PRI_RT_HIGH;
    pthread_attr_setschedparam(&record_task_attr, &record_sched_param);
//RT proi
    rc = pthread_create(&ctx->task[REC_TYPE_NORMAL].taskId, &record_task_attr,cvi_recorder_task, (void *)ctx);
    if (rc != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "rc_normal task create failed, %d\r\n", rc);
        goto FAILED;
    } else {
        pthread_setname_np(ctx->task[REC_TYPE_NORMAL].taskId, e_name);
    }
    pthread_attr_t record_event_task_attr;
    struct sched_param record_event_sched_param = {0};
    pthread_attr_init(&record_event_task_attr);
    pthread_attr_setschedpolicy(&record_event_task_attr, SCHED_RR);
    record_event_sched_param.sched_priority = CVI_OSAL_PRI_RT_HIGH;
    pthread_attr_setschedparam(&record_event_task_attr, &record_event_sched_param);
    rc = pthread_create(&ctx->task[REC_TYPE_EVENT].taskId, &record_event_task_attr, cvi_recorder_event_task, (void *)ctx);
    if (rc != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "rc_normal task create failed, %d\r\n", rc);
        goto FAILED;
    } else {
        pthread_setname_np(ctx->task[REC_TYPE_EVENT].taskId, e_name);
    }
    sync_init();
    *recorder = (void *)ctx;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "success\r\n");
    return 0;

FAILED:
    cvi_recorder_destroy(ctx);
    *recorder = NULL;
    return rc;
}

static void cvi_recorder_init_muxer_frame(CVI_FRAME_INFO_T *mframe)
{
    mframe->hmagic = 0x5a5a5a5a;
    mframe->type = CVI_MUXER_FRAME_TYPE_BUTT;
    mframe->isKey = 0;
    mframe->pts = 0;
    mframe->gopInx = 0;
    mframe->vpts = 0;
    mframe->dataLen = 0;
    mframe->extraLen = 0;
    mframe->totalSize = 0;
    mframe->tmagic = 0x5a5a5a5a;
}

int cvi_recorder_send_frame(void *recorder, CVI_FRAME_STREAM_T *frame)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    if(ctx->rbuf[frame->type] == NULL) {
        return 0;
    }
#if SUPPORT_POST_STOP
    if(ctx->recStartFlag[REC_TYPE_EVENT] == 0 && ctx->recStartFlag[REC_TYPE_NORMAL] == 0
    && ctx->recStartFlag[REC_TYPE_LAPSE] == 0 && ctx->recMemStartFlag == 0){
        return 0;
    }
#endif
    CVI_FRAME_INFO_T mframe;
    cvi_recorder_init_muxer_frame(&mframe);
    if (frame->type == CVI_MUXER_FRAME_TYPE_VIDEO) {
        if(ctx->firstFramePts[CVI_MUXER_FRAME_TYPE_VIDEO] == 0){
            if(frame->vftype[0] == 0){
                APP_PROF_LOG_PRINT(LEVEL_DEBUG, "start rec %d, first frame must be iframe\r\n", ctx->stRecAttr.id);
                return 0;
            }
            ctx->firstFramePts[CVI_MUXER_FRAME_TYPE_VIDEO] = cvi_recorder_get_us();
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d video firstFramePts %lu\r\n", ctx->stRecAttr.id, ctx->firstFramePts[CVI_MUXER_FRAME_TYPE_VIDEO]);
        }
        mframe.type = CVI_MUXER_FRAME_TYPE_VIDEO;

        for (int i = 0; i < frame->num; i++) {
            if(frame->vftype[i] == 1){
                int icnt = 0;
                mframe.dataLen = 0;
                for(int j = i; j < frame->num && frame->vftype[j] == 1; j++){
                    mframe.dataLen += frame->len[j];
                    icnt++;
                }
                if(icnt == 1){
                    continue;
                }
                if(icnt == 2){
                    i+=1;
                    icnt = 1;
                }

                mframe.vpts = ctx->firstFramePts[mframe.type] + ctx->ptsInBuf[mframe.type] * ctx->fduration[mframe.type];
                mframe.gopInx = 0;
                mframe.isKey = 1;
                mframe.pts = ctx->ptsInBuf[mframe.type];
                mframe.extraLen = frame->thumbnail_len;
                mframe.totalSize = mframe.dataLen + sizeof(CVI_FRAME_INFO_T) + mframe.extraLen;
                mframe.totalSize = CVI_REC_ALIGN(mframe.totalSize, CVI_REC_ALIGN_LEN);
                if(cvi_rbuf_unused(ctx->rbuf[mframe.type]) <= (unsigned int)mframe.totalSize){
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%d]: in buf %ld %ld\r\n", ctx->stRecAttr.id, ctx->ptsInBuf[CVI_MUXER_FRAME_TYPE_VIDEO], ctx->ptsInBuf[CVI_MUXER_FRAME_TYPE_AUDIO]);
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%d]: drop VIDEO Iframe data %u %d\r\n", ctx->stRecAttr.id, cvi_rbuf_unused(ctx->rbuf[mframe.type]), mframe.totalSize);
                    cvi_rbuf_show_log(ctx->rbuf[CVI_MUXER_FRAME_TYPE_VIDEO]);
                    cvi_rbuf_show_log(ctx->rbuf[CVI_MUXER_FRAME_TYPE_AUDIO]);
                    ctx->gopInx++;
                    ctx->dropIframeFlag = 1;
                    return -1;
                }
                if(frame->data[0][4] != 0x67){
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%d]: Warning !! I frame not match %#x\n", ctx->stRecAttr.id, frame->data[0][4]);
                }

                int off = 0;
                cvi_rbuf_copy_in(ctx->rbuf[mframe.type], (void *)&mframe, sizeof(CVI_FRAME_INFO_T), 0);
                off += sizeof(CVI_FRAME_INFO_T);
                for(int j = i; j < i + icnt; j++){
                    cvi_rbuf_copy_in(ctx->rbuf[mframe.type], frame->data[j], frame->len[j], off);
                    off += frame->len[j];
                }

                if(frame->thumbnail_len > 0) {
                    cvi_rbuf_copy_in(ctx->rbuf[mframe.type], frame->thumbnail_data, frame->thumbnail_len, off);
                }

                cvi_rbuf_refresh_in(ctx->rbuf[mframe.type], mframe.totalSize);
                if((ctx->gopInx > 0 && ctx->gopInx < 14) || ctx->gopInx > 14){
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%d] Warning drop frame %d %d!!!!!!!!!!!!!\r\n", ctx->stRecAttr.id, frame->num, ctx->gopInx);
                }
                ctx->gopInx = 0;
                ctx->dropIframeFlag = 0;
                i += (icnt - 1);
            }else if(frame->vftype[i] == 0){
                if(ctx->dropIframeFlag == 1){
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%d], drop I frame, so drop gop group\r\n", ctx->stRecAttr.id);
                    continue;
                }
                mframe.gopInx = ctx->gopInx;
                mframe.isKey = 0;
                mframe.pts = ctx->ptsInBuf[mframe.type];
                mframe.vpts = ctx->firstFramePts[mframe.type] + ctx->ptsInBuf[mframe.type] * ctx->fduration[mframe.type];
                mframe.dataLen = frame->len[i];
                mframe.extraLen = 0;
                mframe.totalSize = mframe.dataLen + sizeof(CVI_FRAME_INFO_T);
                mframe.totalSize = CVI_REC_ALIGN(mframe.totalSize, CVI_REC_ALIGN_LEN);

                int off = 0;
                if(cvi_rbuf_unused(ctx->rbuf[mframe.type]) < (unsigned int)mframe.totalSize){
                    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "[%d]: drop VIDEO pframe data %u %d\r\n", ctx->stRecAttr.id, cvi_rbuf_unused(ctx->rbuf[mframe.type]), mframe.totalSize);
                    ctx->gopInx++;
                    continue;
                }
                cvi_rbuf_copy_in(ctx->rbuf[mframe.type], (void *)&mframe, sizeof(CVI_FRAME_INFO_T), off);
                off += sizeof(CVI_FRAME_INFO_T);
                cvi_rbuf_copy_in(ctx->rbuf[mframe.type], frame->data[i], frame->len[i], off);
                off += frame->len[i];
                cvi_rbuf_refresh_in(ctx->rbuf[mframe.type], mframe.totalSize);
                ctx->gopInx++;
            }
            ctx->ptsInBuf[mframe.type]++;
        }
    }
    else if (frame->type == CVI_MUXER_FRAME_TYPE_AUDIO){
        mframe.type = CVI_MUXER_FRAME_TYPE_AUDIO;
        if(ctx->firstFramePts[CVI_MUXER_FRAME_TYPE_VIDEO] == 0){
            // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "id %d video has no frame, drop audio frame", ctx->stRecAttr.id);
            return 0;
        }

        if(ctx->dropIframeFlag == 1){
            return 0;
        }

        if(ctx->firstFramePts[mframe.type] == 0){
            ctx->firstFramePts[mframe.type] = cvi_recorder_get_us();
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d audio firstFramePts %lu\r\n", ctx->stRecAttr.id, ctx->firstFramePts[mframe.type]);
        }

        if(ctx->ptsInBuf[CVI_MUXER_FRAME_TYPE_AUDIO] * ctx->fduration[CVI_MUXER_FRAME_TYPE_AUDIO]
         > ctx->ptsInBuf[CVI_MUXER_FRAME_TYPE_VIDEO] * ctx->fduration[CVI_MUXER_FRAME_TYPE_VIDEO] + 100 * 1000){
            return 0;
        }

        mframe.vpts = ctx->firstFramePts[mframe.type] + ctx->ptsInBuf[mframe.type] * ctx->fduration[mframe.type];
        mframe.isKey = 1;
        mframe.dataLen = frame->len[0];
        mframe.pts = ctx->ptsInBuf[mframe.type];
        mframe.totalSize = mframe.dataLen + sizeof(CVI_FRAME_INFO_T);
        if(cvi_rbuf_unused(ctx->rbuf[mframe.type]) > (unsigned int)mframe.totalSize) {
            int off = 0;
            cvi_rbuf_copy_in(ctx->rbuf[mframe.type], (void *)&mframe, sizeof(CVI_FRAME_INFO_T), off);
            off = sizeof(CVI_FRAME_INFO_T);
            for (int i = 0; i < frame->num; i++) {
                cvi_rbuf_copy_in(ctx->rbuf[mframe.type], frame->data[i], frame->len[i], off);
                off += frame->len[i];
            }
            cvi_rbuf_refresh_in(ctx->rbuf[mframe.type], mframe.totalSize);
            ctx->ptsInBuf[mframe.type]++;
        }
    }
    else if(frame->type == CVI_MUXER_FRAME_TYPE_SUBTITLE){
        mframe.type = CVI_MUXER_FRAME_TYPE_SUBTITLE;
        if(ctx->firstFramePts[CVI_MUXER_FRAME_TYPE_VIDEO] == 0){
            // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d frame type %d video has no frame, drop subtitle frame", ctx->stRecAttr.id, frame->type);
            return 0;
        }

        if(ctx->firstFramePts[mframe.type] == 0){
            ctx->firstFramePts[mframe.type] = frame->vi_pts[0];
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%d subtile firstFramePts %lu\r\n", ctx->stRecAttr.id, ctx->firstFramePts[mframe.type]);
        }
        mframe.vpts = ctx->firstFramePts[mframe.type] + ctx->ptsInBuf[mframe.type] * ctx->fduration[mframe.type];;
        mframe.isKey = 1;
        mframe.pts = ctx->ptsInBuf[mframe.type];
        mframe.dataLen = frame->len[0];
        mframe.totalSize = mframe.dataLen + sizeof(CVI_FRAME_INFO_T);
        ctx->ptsInBuf[mframe.type]++;
        if(cvi_rbuf_unused(ctx->rbuf[mframe.type]) > (uint32_t)mframe.totalSize) {
            unsigned char buf[2];
            buf[0] = 0;
            buf[1] = mframe.dataLen;
            mframe.dataLen += sizeof(buf);
            mframe.totalSize += sizeof(buf);

            cvi_rbuf_copy_in(ctx->rbuf[mframe.type], (void *)&mframe, sizeof(CVI_FRAME_INFO_T), 0);
            int off = sizeof(CVI_FRAME_INFO_T);

            cvi_rbuf_copy_in(ctx->rbuf[mframe.type], (void *)buf, sizeof(buf), off);
            off += sizeof(buf);

            cvi_rbuf_copy_in(ctx->rbuf[mframe.type], frame->data[0], frame->len[0], off);
            cvi_rbuf_refresh_in(ctx->rbuf[mframe.type], mframe.totalSize);
        }
    }else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "[%d: frame type [%d] invailed\r\n", ctx->stRecAttr.id, frame->type);
    }
    return 0;
}

static void * cvi_recorder_manual_stop_thread(void *arg){
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)arg;

    CVI_REC_TYPE_E type = ctx->stRecAttr.enRecType;
    REC_TYPE_INDEX_E index = REC_TYPE_NORMAL;
    if(type == CVI_REC_TYPE_LAPSE) {
        index = REC_TYPE_LAPSE;
    }

#if SUPPORT_POST_STOP
    uint32_t post_time = ctx->stRecAttr.u32PostRecTimeSec;
    if(post_time == 0){
        ctx->recStartFlag[index] = 0;
    }

    uint32_t end_time = (uint32_t)(cvi_recorder_get_us() / 1000 / 1000) + (uint32_t)post_time;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "post_time %u, end_time %u\r\n", post_time, end_time);

    if(ctx->postStopFlag == 0) {
        post_time = 0;
    }

    while(ctx->manualStopFlag){
        if(cvi_rbuf_data_cnt(ctx->rbuf[CVI_MUXER_FRAME_TYPE_VIDEO], 0) > 0){
            CVI_MUXER_FRAME_TYPE_E type = cvi_recorder_get_frame_type(ctx, 0);
            void *f = cvi_recorder_get_frame(ctx, type, 0);
            if(f){
                cvi_muxer_write_packet(ctx->muxer[index], (CVI_FRAME_INFO_T *)f);
            }

            if(post_time > 0) {
                clock_gettime(CLOCK_MONOTONIC, &tv);
                if(tv.tv_sec > end_time) {
                    break;
                }
            }
        }else {
            if(post_time == 0) {
                break;
            }
        }
        usleep(10 * 1000);
    }
#endif
    pthread_mutex_lock(&ctx->mutex[index]);
    if(ctx->recStartFlag[index] == 1){
        ctx->recStartFlag[index] = 0;
        cvi_recorder_auto_split(ctx, index);
    }

    pthread_mutex_unlock(&ctx->mutex[index]);
    cvi_recorder_force_stop_event_rec(arg);

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "rec id %d : %d %d %d %d\r\n", ctx->stRecAttr.id, ctx->recStartFlag[REC_TYPE_NORMAL], ctx->recStartFlag[REC_TYPE_LAPSE],
        ctx->recStartFlag[REC_TYPE_EVENT], ctx->recMemStartFlag);
    usleep(50 * 1000);
    if(ctx->recStartFlag[REC_TYPE_NORMAL] == 0 && ctx->recStartFlag[REC_TYPE_LAPSE] == 0
    && ctx->recStartFlag[REC_TYPE_EVENT] == 0 && ctx->recMemStartFlag == 0){
        ctx->gopInx = 0;
        ctx->dropIframeFlag = 0;
        ctx->stRecAttr.fncallback.pfn_rec_stop_callback(ctx->stRecAttr.fncallback.pfn_rec_stop_callback_param);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "rec %d\r\n", ctx->stRecAttr.id);
        for(int j = 0; j < REC_TYPE_BUTT; j++){
            pthread_mutex_lock(&ctx->mutex[j]);
            for(int i = 0; i < CVI_MUXER_FRAME_TYPE_BUTT; i++) {
                if(ctx->rbuf[i] != NULL){
                    cvi_rbuf_reset((void *)ctx->rbuf[i]);
                }
                ctx->ptsInLastfile[j][i] = 0;
                ctx->ptsInfile[j][i] = 0;
                ctx->isFirstFrame[j][i] = 0;
                ctx->startTimeStamp[j][i] = 0;
                ctx->curTimeStamp[j][i] = 0;
                ctx->targetTimeStamp[j][i] = 0;
                ctx->ptsInBuf[i] = 0;
                ctx->firstFramePts[i] = 0;
            }
            ctx->rec_start_time[j] = 0;
            ctx->fileCnt[j] = 0;
            ctx->appendAudioCnt[j] = 0;
            memset(ctx->filename[j], 0x0, sizeof(ctx->filename[j]));
            memset(ctx->nextFilename[j], 0x0, sizeof(ctx->nextFilename[j]));
            pthread_mutex_unlock(&ctx->mutex[j]);
        }
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "rec manual stop %d end\r\n", ctx->stRecAttr.id);
    }
    ctx->manualStopFlag = 0;
    return 0;
}

int cvi_recorder_start_mem_rec(void *recorder){
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    ctx->recMemStartFlag = 1;
    return 0;
}

int cvi_recorder_stop_mem_rec(void *recorder){
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    if(ctx->recMemStartFlag == 0) {
        return 0;
    }
    ctx->recMemStartFlag = 0;
    return 0;
}

int cvi_recorder_start_normal_rec(void *recorder){
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    while(ctx->manualStopFlag) {
        usleep(10 * 1000);
    }
    pthread_mutex_lock(&ctx->mutex[REC_TYPE_NORMAL]);
    if(ctx->recStartFlag[REC_TYPE_NORMAL] == 1 && ctx->recSplit == 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_NORMAL]);
        return 0;
    }
    ctx->rec_start_time[REC_TYPE_NORMAL] = cvi_recorder_get_us();
    if(ctx->nextFilename[REC_TYPE_NORMAL][0] == '\0'){
        ctx->stRecAttr.fncallback.pfn_get_filename(ctx->stRecAttr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_NORMAL],
            ctx->nextFilename[REC_TYPE_NORMAL], sizeof(ctx->nextFilename[REC_TYPE_NORMAL]) - 1);
    }
    memset(ctx->filename[REC_TYPE_NORMAL], 0x0, sizeof(ctx->filename[REC_TYPE_NORMAL]));
    strncpy(ctx->filename[REC_TYPE_NORMAL], ctx->nextFilename[REC_TYPE_NORMAL], sizeof(ctx->filename[REC_TYPE_NORMAL]) - 1);
    int ret = cvi_muxer_start(ctx->muxer[REC_TYPE_NORMAL], (const char *)ctx->filename[REC_TYPE_NORMAL]);
    if(ret < 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_NORMAL]);
        return -1;
    }
    ctx->recStartFlag[REC_TYPE_NORMAL] = 1;
    ctx->appendAudioCnt[REC_TYPE_NORMAL] = 0;
    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_NORMAL]);
    if(ctx->recSplit == 1) {
        CVI_REC_EVENT_CALLBACK_FN_PTR callback = ctx->stRecAttr.fncallback.pfn_event_callback[REC_TYPE_NORMAL];
        if(callback)
            callback(CVI_REC_EVENT_START, ctx->filename[REC_TYPE_NORMAL], ctx->stRecAttr.fncallback.pfn_event_callback_param);
    }
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "start filename %s\r\n", ctx->filename[REC_TYPE_NORMAL]);
    return ret;
}

int cvi_recorder_stop_normal_rec(void *recorder)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    pthread_mutex_lock(&ctx->mutex[REC_TYPE_NORMAL]);
    if(ctx->recStartFlag[REC_TYPE_NORMAL] == 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_NORMAL]);
        return 0;
    }
    ctx->manualStopFlag = 1;
    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_NORMAL]);

    static char name[16] = {0};
    snprintf(name, sizeof(name), "manual_stop_%d", ctx->stRecAttr.id);
    pthread_t taskId;
    int rc = pthread_create(&taskId, NULL, cvi_recorder_manual_stop_thread, (void *)ctx);
    if (rc != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "pthread_create task create failed, %d\r\n", rc);
    } else {
        pthread_setname_np(taskId, name);
    }

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "stop normal rec %s\r\n", ctx->filename[REC_TYPE_NORMAL]);
    return 0;
}

int cvi_recorder_start_event_rec(void *recorder)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    pthread_mutex_lock(&ctx->mutex[REC_TYPE_EVENT]);
    if(ctx->recStartFlag[REC_TYPE_EVENT] == 1 || ctx->recStartFlag[REC_TYPE_EVENT] == 1) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_EVENT]);
        return 0;
    }

    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_EVENT]);

    while(ctx->manualStopFlag) {
        usleep(10 * 1000);
    }

    ctx->rec_start_time[REC_TYPE_EVENT] = cvi_recorder_get_us();
    for(int i = 0; i < CVI_MUXER_FRAME_TYPE_BUTT; i++) {
        if(CVI_MUXER_FRAME_TYPE_THUMBNAIL == i || (CVI_MUXER_FRAME_TYPE_SUBTITLE == i && ctx->stRecAttr.enable_subtitle == 0))
            continue;
        ctx->ptsInLastfile[REC_TYPE_EVENT][i] += ctx->ptsInfile[REC_TYPE_EVENT][i];
        ctx->ptsInfile[REC_TYPE_EVENT][i] = 0;
        ctx->isFirstFrame[REC_TYPE_EVENT][i] = 0;
        ctx->startTimeStamp[REC_TYPE_EVENT][i] = ctx->curTimeStamp[REC_TYPE_EVENT][i];
        ctx->targetTimeStamp[REC_TYPE_EVENT][i] = ctx->stRecAttr.u32PostRecTimeSec * 1000 * 1000 + ctx->stRecAttr.u32PreRecTimeSec * 1000 * 1000 + ctx->startTimeStamp[REC_TYPE_EVENT][i];
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "event %d sTimeStamp %lu tTimeStamp %lu\r\n", ctx->stRecAttr.id, ctx->startTimeStamp[REC_TYPE_EVENT][i], ctx->targetTimeStamp[REC_TYPE_EVENT][i]);
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "event %d ptsInfile %ld ptsInLastfile %ld\r\n", ctx->stRecAttr.id, ctx->ptsInfile[REC_TYPE_EVENT][i], ctx->ptsInLastfile[REC_TYPE_EVENT][i]);
    }

    pthread_mutex_lock(&ctx->mutex[REC_TYPE_EVENT]);
    ctx->stRecAttr.fncallback.pfn_get_filename(ctx->stRecAttr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_EVENT],
        ctx->nextFilename[REC_TYPE_EVENT], sizeof(ctx->nextFilename[REC_TYPE_EVENT]) - 1);
    strncpy(ctx->filename[REC_TYPE_EVENT], ctx->nextFilename[REC_TYPE_EVENT], sizeof(ctx->filename[REC_TYPE_EVENT]) - 1);
    int ret = cvi_muxer_start(ctx->muxer[REC_TYPE_EVENT], (const char *)ctx->filename[REC_TYPE_EVENT]);
    if(ret < 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_EVENT]);
        return -1;
    }
    ctx->recStartFlag[REC_TYPE_EVENT] = 1;
    ctx->appendAudioCnt[REC_TYPE_EVENT] = 0;
    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_EVENT]);
    CVI_REC_EVENT_CALLBACK_FN_PTR callback = ctx->stRecAttr.fncallback.pfn_event_callback[REC_TYPE_EVENT];
    if (callback)
        callback(CVI_REC_EVENT_START_EMR, ctx->filename[REC_TYPE_EVENT], ctx->stRecAttr.fncallback.pfn_event_callback_param);

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "start filename %s\r\n", ctx->filename[REC_TYPE_EVENT]);
    return ret;
}

int cvi_recorder_force_stop_event_rec(void *recorder){
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    pthread_mutex_lock(&ctx->mutex[REC_TYPE_EVENT]);
    if(ctx->recStartFlag[REC_TYPE_EVENT] == 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_EVENT]);
        return 0;
    }
    ctx->recStartFlag[REC_TYPE_EVENT] = 0;
    cvi_recorder_auto_split(ctx, REC_TYPE_EVENT);
    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_EVENT]);
    return 0;
}

int cvi_recorder_stop_event_rec(void *recorder)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    if(ctx->recStartFlag[REC_TYPE_EVENT] == 0) {
        return 0;
    }

    while(ctx->recStartFlag[REC_TYPE_EVENT] == 1){
        usleep(50 * 1000);
    }
    return 0;
}

int cvi_recorder_stop_event_rec_post(void *recorder)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    ctx->recStartFlag[REC_TYPE_EVENT] = 0;
    usleep(10 * 1000);
    return 0;
}

int cvi_recorder_start_lapse_rec(void *recorder)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    while(ctx->manualStopFlag) {
        usleep(10 * 1000);
    }

    pthread_mutex_lock(&ctx->mutex[REC_TYPE_LAPSE]);
    if(ctx->recStartFlag[REC_TYPE_LAPSE] == 1 && ctx->recSplit == 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_LAPSE]);
        return 0;
    }
    ctx->rec_start_time[REC_TYPE_LAPSE] = cvi_recorder_get_us();
    if(ctx->nextFilename[REC_TYPE_LAPSE][0] == '\0'){
        ctx->stRecAttr.fncallback.pfn_get_filename(ctx->stRecAttr.fncallback.pfn_get_filename_param[CVI_CALLBACK_TYPE_LAPSE],
            ctx->nextFilename[REC_TYPE_LAPSE], sizeof(ctx->nextFilename[REC_TYPE_LAPSE]) - 1);
    }
    memset(ctx->filename[REC_TYPE_LAPSE], 0x0, sizeof(ctx->filename[REC_TYPE_LAPSE]));
    strncpy(ctx->filename[REC_TYPE_LAPSE], ctx->nextFilename[REC_TYPE_LAPSE], sizeof(ctx->filename[REC_TYPE_LAPSE]) - 1);

    int ret = cvi_muxer_start(ctx->muxer[REC_TYPE_LAPSE], (const char *)ctx->filename[REC_TYPE_LAPSE]);
    if(ret < 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_LAPSE]);
        return -1;
    }
    ctx->recStartFlag[REC_TYPE_LAPSE] = 1;
    ctx->appendAudioCnt[REC_TYPE_LAPSE] = 0;
    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_LAPSE]);

    if(ctx->recSplit == 1) {
        CVI_REC_EVENT_CALLBACK_FN_PTR callback = ctx->stRecAttr.fncallback.pfn_event_callback[REC_TYPE_LAPSE];
        if(callback)
            callback(CVI_REC_EVENT_START, ctx->filename[REC_TYPE_LAPSE], ctx->stRecAttr.fncallback.pfn_event_callback_param);
    }

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "start filename %s\r\n", ctx->filename[REC_TYPE_LAPSE]);
    return ret;
}

int cvi_recorder_stop_lapse_rec(void *recorder)
{
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    pthread_mutex_lock(&ctx->mutex[REC_TYPE_LAPSE]);
    if(ctx->recStartFlag[REC_TYPE_LAPSE] == 0) {
        pthread_mutex_unlock(&ctx->mutex[REC_TYPE_LAPSE]);
        return 0;
    }
    ctx->manualStopFlag = 1;
    pthread_mutex_unlock(&ctx->mutex[REC_TYPE_LAPSE]);
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "stop lapse rec %s\r\n", ctx->filename[REC_TYPE_LAPSE]);
    static char name[16] = {0};
    snprintf(name, sizeof(name), "manual_stop_%d", ctx->stRecAttr.id);
    //cvi_osal_task_attr_t ta;
    //cvi_osal_task_handle_t task;
    //ta.name = name;
    //ta.entry = cvi_recorder_manual_stop_thread;
    //ta.param = (void *)ctx;
    //ta.priority = CVI_OSAL_PRI_NORMAL;
    //ta.detached = true;
    //cvi_osal_task_create(&ta, &task);
    pthread_t pthreadId;
    int rc = pthread_create(&pthreadId, NULL, cvi_recorder_manual_stop_thread, NULL);
    if (rc == 0) {
        pthread_setname_np(ctx->task[REC_TYPE_NORMAL].taskId, name);
    }
    return 0;
}

int cvi_recorder_split(void *recorder) {
    CVI_RECORDER_CTX_T *ctx = (CVI_RECORDER_CTX_T *)recorder;
    CVI_CHECK_CTX_NULL(ctx);
    if(ctx->recSplit == 1) {
        return 0;
    }
    return 0;
}

int cvi_recorder_adjust_target_ts(void *recorder0, void *recorder1, int type){
    CVI_RECORDER_CTX_T *ctx0 = (CVI_RECORDER_CTX_T *)recorder0;
    CVI_CHECK_CTX_NULL(ctx0);
    CVI_RECORDER_CTX_T *ctx1 = (CVI_RECORDER_CTX_T *)recorder1;
    CVI_CHECK_CTX_NULL(ctx1);

    if(type == 1){
        return 0;
    }

    if(ctx0->stRecAttr.enRecType != ctx1->stRecAttr.enRecType){
        return 0;
    }

    // uint64_t t0 = ctx0->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO];
    // uint64_t t1 = ctx1->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO];
    uint64_t t0 = ctx0->rec_start_time[type];
    uint64_t t1 = ctx1->rec_start_time[type];
    if(t0 == 0 || t1 == 0){
        return 0;
    }

    if(ctx0->fileCnt[type] != ctx1->fileCnt[type]){
        return 0;
    }

    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "type %d\r\n", type);
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "before t0 %lu t1 %lu\r\n", ctx0->rec_start_time[type], ctx1->rec_start_time[type]);
    // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "before s0 %u s1 %u", ctx0->stRecAttr.stSplitAttr.u64SplitTimeLenMSec, ctx1->stRecAttr.stSplitAttr.u64SplitTimeLenMSec);
    if(t0 > t1 + 200 * 1000) {
        ctx1->rec_start_time[type] = ctx0->rec_start_time[type];
        // ctx1->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] = ctx0->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO];
        // ctx1->stRecAttr.stSplitAttr.u64SplitTimeLenMSec = ctx0->stRecAttr.stSplitAttr.u64SplitTimeLenMSec;
    }else if(t1 > t0 + 200 * 1000) {
        ctx0->rec_start_time[type] = ctx1->rec_start_time[type];
        // ctx0->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO] = ctx1->targetTimeStamp[type][CVI_MUXER_FRAME_TYPE_VIDEO];
        // ctx0->stRecAttr.stSplitAttr.u64SplitTimeLenMSec = ctx1->stRecAttr.stSplitAttr.u64SplitTimeLenMSec;
    }
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "after t0 %lu t1 %lu\r\n", ctx0->rec_start_time[type], ctx1->rec_start_time[type]);
    // APP_PROF_LOG_PRINT(LEVEL_DEBUG, "after s0 %u s1 %u\r\n", ctx0->stRecAttr.stSplitAttr.u64SplitTimeLenMSec, ctx1->stRecAttr.stSplitAttr.u64SplitTimeLenMSec);
    return 0;
}

int cvi_recorder_adjust_filename(void *recorder0, void *recorder1, int type){
    CVI_RECORDER_CTX_T *ctx0 = (CVI_RECORDER_CTX_T *)recorder0;
    CVI_CHECK_CTX_NULL(ctx0);
    CVI_RECORDER_CTX_T *ctx1 = (CVI_RECORDER_CTX_T *)recorder1;
    CVI_CHECK_CTX_NULL(ctx1);
    return 0;
    if(type == 1){
        return 0;
    }

    if(ctx0->stRecAttr.enRecType != ctx1->stRecAttr.enRecType){
        return 0;
    }

    if(ctx0->fileCnt[type] != ctx1->fileCnt[type]){
        return 0;
    }

    if(ctx0->nextFilename[type][0] == '\0' || ctx1->nextFilename[type][0] == '\0') {
        return 0;
    }

    const char *tag = "/2022_01_14_230014_00";
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "type %d\r\n", type);
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "before %s %s", ctx0->nextFilename[type], ctx1->nextFilename[type]);
    char *tmp0 = strrchr(ctx0->nextFilename[type], '/');
    char *tmp1 = strrchr(ctx1->nextFilename[type], '/');
    if(strncasecmp(tmp0, tmp1, strlen(tag)) < 0) {
        ctx1->stRecAttr.fncallback.pfn_remove_file_callback(ctx1->nextFilename[type]);
        strncpy(tmp1, tmp0, strlen(tag));
        ctx1->stRecAttr.fncallback.pfn_add_file_callback(ctx1->nextFilename[type]);
    }else if(strncasecmp(tmp0, tmp1, strlen(tag)) > 0){
        ctx0->stRecAttr.fncallback.pfn_remove_file_callback(ctx0->nextFilename[type]);
        strncpy(tmp0, tmp1, strlen(tag));
        ctx0->stRecAttr.fncallback.pfn_add_file_callback(ctx0->nextFilename[type]);
    }
    APP_PROF_LOG_PRINT(LEVEL_DEBUG, "after %s %s\r\n", ctx0->nextFilename[type], ctx1->nextFilename[type]);
    return 0;
}


#ifdef __cplusplus
}
#endif


