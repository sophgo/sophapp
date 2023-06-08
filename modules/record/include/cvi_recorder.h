#ifndef _CVI_RECORDER_H_
#define _CVI_RECORDER_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "cvi_muxer.h"


#define MAX_CONTEXT_CNT 3

typedef void *CVI_RECORDER_HANDLE_T;

typedef enum CVI_REC_EVENT_E
{
    CVI_REC_EVENT_START,
    CVI_REC_EVENT_STOP,
    CVI_REC_EVENT_STOP_FAILED,
    CVI_REC_EVENT_SPLIT,
    CVI_REC_EVENT_WRITE_FRAME_DROP,
    CVI_REC_EVENT_WRITE_FRAME_TIMEOUT,
    CVI_REC_EVENT_WRITE_FRAME_FAILED,
    CVI_REC_EVENT_OPEN_FILE_FAILED,
    CVI_REC_EVENT_CLOSE_FILE_FAILED,
    CVI_REC_EVENT_SHORT_FILE,
    CVI_REC_EVENT_PIV_START,
    CVI_REC_EVENT_PIV_END,
    CVI_REC_EVENT_SYNC_DONE,
    CVI_REC_EVENT_SPLIT_START,
    CVI_REC_EVENT_START_EMR,
    CVI_REC_EVENT_END_EMR,
    CVI_REC_EVENT_FRAME_DROP,
    CVI_REC_EVENT_BUTT
} CVI_REC_EVENT_E;

typedef enum CVI_REC_PTS_STATE_E {
    CVI_REC_PTS_STATE_INIT,
    CVI_REC_PTS_STATE_SETTED,
    CVI_REC_PTS_STATE_CHECKED,
    CVI_REC_PTS_STATE_BUTT
} CVI_REC_PTS_STATE_E;

typedef struct CVI_REC_EVENT_WRITE_FRAME_TIMEOUT_MS_S{
    int timeout_ms;
    void *param;
} CVI_REC_EVENT_WRITE_FRAME_TIMEOUT_MS_S;

typedef int (*CVI_GET_FILENAME_CALLBACK_FN_PTR)(void *p, char *filename, int filename_len);
typedef int (*CVI_REC_EVENT_CALLBACK_FN_PTR)(CVI_REC_EVENT_E event_type, const char *filename, void *p);
typedef int (*CVI_GET_SUBTITLE_CALLBACK_FN_PTR)(void *p, char *str, int str_len);
typedef int (*CVI_GET_MEM_BUFFER_STOP_CALLBACK_FN_PTR)(void *p);
typedef int (*CVI_REQUEST_IDR_CALLBACK_FN_PTR)(void *p);
typedef int (*CVI_REC_STOP_CALLBACK_FN_PTR)(void *p);
typedef int (*CVI_REC_REMOVE_FILE_CALLBACK_FN_PTR)(char *filename);
typedef int (*CVI_REC_ADD_FILE_CALLBACK_FN_PTR)(char *filename);

typedef enum CVI_CALLBACK_TYPE{
    CVI_CALLBACK_TYPE_NORMAL = 0,
    CVI_CALLBACK_TYPE_LAPSE,
    CVI_CALLBACK_TYPE_EVENT,
    CVI_CALLBACK_TYPE_BUTT
}CVI_CALLBACK_TYPE_E;

typedef enum REC_TYPE_INDEX_E{
    REC_TYPE_NORMAL = 0,
    REC_TYPE_LAPSE = 0,
    REC_TYPE_EVENT = 1,
    REC_TYPE_BUTT = 2
}REC_TYPE_INDEX_E;


typedef struct CVI_CALLBACK_HANDLES_S{
    CVI_REC_REMOVE_FILE_CALLBACK_FN_PTR pfn_remove_file_callback;
    CVI_REC_ADD_FILE_CALLBACK_FN_PTR pfn_add_file_callback;
    CVI_GET_SUBTITLE_CALLBACK_FN_PTR pfn_get_subtitle_callback;
    void *pfn_get_subtitle_callback_param;
    CVI_GET_FILENAME_CALLBACK_FN_PTR pfn_get_filename;
    void *pfn_get_filename_param[CVI_CALLBACK_TYPE_BUTT];
    CVI_REQUEST_IDR_CALLBACK_FN_PTR pfn_request_idr;
    void *pfn_request_idr_param;
    CVI_REC_EVENT_CALLBACK_FN_PTR pfn_event_callback[REC_TYPE_BUTT]; /*normal && lapse share*/
    void *pfn_event_callback_param;
    CVI_GET_MEM_BUFFER_STOP_CALLBACK_FN_PTR pfn_mem_buffer_stop_callback;
    void *pfn_mem_buffer_stop_callback_param;
    CVI_REC_STOP_CALLBACK_FN_PTR pfn_rec_stop_callback;
    void *pfn_rec_stop_callback_param;
}CVI_CALLBACK_HANDLES_T;

typedef enum CVI_RECORDER_RBUF_TYPE_E{
    CVI_RECORDER_RBUF_VIDEO = 0,
    CVI_RECORDER_RBUF_AUDIO,
    CVI_RECORDER_RBUF_SUBTITLE,
    CVI_RECORDER_RBUF_BUTT
}CVI_RECORDER_RBUF_TYPE_E;


typedef struct CVI_RECORDER_RBUF_ATTR_S{
    uint32_t size;
    const char *name;
}CVI_RECORDER_RBUF_ATTR_T;


#define CVI_REC_TRACK_MAX_CNT (CVI_TRACK_SOURCE_TYPE_BUTT)

typedef enum cviTrack_SourceType_E {
    CVI_TRACK_SOURCE_TYPE_VIDEO = 0,
    CVI_TRACK_SOURCE_TYPE_AUDIO,
    CVI_TRACK_SOURCE_TYPE_PRIV,
    CVI_TRACK_SOURCE_TYPE_BUTT
} CVI_Track_SourceType_E;


/* record type enum */
typedef enum cviREC_TYPE_E {
    CVI_REC_TYPE_NORMAL = 0, /* normal record */
    CVI_REC_TYPE_LAPSE,      /* time lapse record, record a frame by an fixed time interval */
    CVI_REC_TYPE_BUTT
} CVI_REC_TYPE_E;

#define CVI_REC_STREAM_MAX_CNT (4)

/* splite define */
/* record split type enum */
typedef enum cviREC_SPLIT_TYPE_E {
    CVI_REC_SPLIT_TYPE_NONE = 0, /* means split is disabled */
    CVI_REC_SPLIT_TYPE_TIME,     /* record split when time reaches */
    CVI_REC_SPLIT_TYPE_BUTT
} CVI_REC_SPLIT_TYPE_E;

/* record split attribute param */
typedef struct cviREC_SPLIT_ATTR_S {
    CVI_REC_SPLIT_TYPE_E enSplitType; /* split type */
    uint64_t u64SplitTimeLenMSec;       /* split time, unit in msecond(ms) */
} CVI_REC_SPLIT_ATTR_T;


/* normal record attribute param */
typedef struct cviREC_NORMAL_ATTR_S {
    uint32_t u32Rsv; /* reserve */
} CVI_REC_NORMAL_ATTR_S;

/* lapse record attribute param */
typedef struct cviREC_LAPSE_ATTR_S {
    uint32_t u32IntervalMs; /* lapse record time interval, unit in millisecord(ms) */
    float fFramerate;
} CVI_REC_LAPSE_ATTR_S;


typedef struct cviTrack_VideoSourceInfo_S {
    CVI_Track_VideoCodec_E enCodecType;
    uint32_t u32Width;
    uint32_t u32Height;
    uint32_t u32BitRate;
    float fFrameRate;
    uint32_t u32Gop;
    float fSpeed;
} CVI_Track_VideoSourceInfo_S;

typedef struct cviTrack_AudioSourceInfo_S {
    CVI_Track_AudioCodec_E enCodecType;
    uint32_t u32ChnCnt;
    uint32_t u32SampleRate;
    uint32_t u32AvgBytesPerSec;
    uint32_t u32SamplesPerFrame;
    unsigned short u16SampleBitWidth;
    float fFramerate;
} CVI_Track_AudioSourceInfo_S;

typedef struct cviTrack_PrivateSourceInfo_S
{
    uint32_t u32PrivateData;
    uint32_t u32FrameRate;
    uint32_t u32BytesPerSec;
    int32_t bStrictSync;
} CVI_Track_PrivateSourceInfo_S;

typedef struct cviTrack_Source_S
{
    CVI_Track_SourceType_E enTrackType;
    int32_t enable;
    union
    {
        CVI_Track_VideoSourceInfo_S stVideoInfo;
        CVI_Track_AudioSourceInfo_S stAudioInfo;
        CVI_Track_PrivateSourceInfo_S stPrivInfo;
    } unTrackSourceAttr;
}CVI_Track_Source_S;


/* record stream attribute */
typedef struct cviREC_STREAM_ATTR_S {
    uint32_t u32TrackCnt;                                            /* track cnt */
    CVI_Track_Source_S aHTrackSrcHandle[CVI_REC_TRACK_MAX_CNT]; /* array of track source cnt */
} CVI_REC_STREAM_ATTR_S;


typedef struct CVI_RECORDER_ATTR_S {
    CVI_REC_STREAM_ATTR_S astStreamAttr;
    CVI_CALLBACK_HANDLES_T fncallback;
    CVI_REC_TYPE_E enRecType; /* record type */
    union {
        CVI_REC_NORMAL_ATTR_S stNormalRecAttr; /* normal record attribute */
        CVI_REC_LAPSE_ATTR_S stLapseRecAttr;   /* lapse record attribute */
    } unRecAttr;
    CVI_RECORDER_RBUF_ATTR_T stRbufAttr[CVI_RECORDER_RBUF_BUTT];
    CVI_REC_SPLIT_ATTR_T stSplitAttr; /* record split attribute */
    int32_t enable_subtitle;
    int32_t enable_file_alignment;
    float subtitle_framerate;
    uint32_t u32PreRecTimeSec;                                   /*  pre record time */
    uint32_t u32PostRecTimeSec;                                   /*  post record time */
    int32_t s32MemRecPreSec;
    char *device_model;
    int32_t prealloc_size;
    float short_file_ms;
    int32_t id;
} CVI_RECORDER_ATTR_T;

#define CVI_FRAME_STREAM_SEGMENT_MAX_NUM (8)
#define CVI_SUBTITLE_MAX_LEN (200)
#define CVI_SEND_FRAME_TIMEOUT_MS (1000)

typedef struct CVI_FRAME_STREAM_S {
    CVI_MUXER_FRAME_TYPE_E type;
    bool vftype[CVI_FRAME_STREAM_SEGMENT_MAX_NUM];
    int32_t num;
    uint64_t vi_pts[CVI_FRAME_STREAM_SEGMENT_MAX_NUM];
    unsigned char *data[CVI_FRAME_STREAM_SEGMENT_MAX_NUM];
    size_t len[CVI_FRAME_STREAM_SEGMENT_MAX_NUM];
    unsigned char *thumbnail_data;
    size_t thumbnail_len;
} CVI_FRAME_STREAM_T;


int cvi_recorder_send_frame(void *recorder, CVI_FRAME_STREAM_T *frame);
int cvi_recorder_start_mem_rec(void *recorder);
int cvi_recorder_stop_mem_rec(void *recorder);
int cvi_recorder_start_normal_rec(void *recorder);
int cvi_recorder_stop_normal_rec(void *recorder);
int cvi_recorder_start_event_rec(void *recorder);
int cvi_recorder_stop_event_rec(void *recorder);
int cvi_recorder_force_stop_event_rec(void *recorder);
int cvi_recorder_stop_event_rec_post(void *recorder);
int cvi_recorder_start_lapse_rec(void *recorder);
int cvi_recorder_stop_lapse_rec(void *recorder);
void cvi_recorder_destroy(void *recorder);
int cvi_recorder_create(void **recorder, CVI_RECORDER_ATTR_T *attr);
int cvi_recorder_split(void *recorder);
int cvi_recorder_adjust_target_ts(void *recorder0, void *recorder1, int type);
int cvi_recorder_adjust_filename(void *recorder0, void *recorder1, int type);

uint64_t cvi_recorder_get_us(void);


#ifdef __cplusplus
}
#endif
#endif
