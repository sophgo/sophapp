#ifndef _CVI_MUXER_H_
#define _CVI_MUXER_H_


#include <stdint.h>
#include <stddef.h>

typedef enum cviTrack_VideoCodec_E
{
    CVI_TRACK_VIDEO_CODEC_H264 = 96,
    CVI_TRACK_VIDEO_CODEC_H265 = 98,
    CVI_TRACK_VIDEO_CODEC_MJPEG = 102,
    CVI_TRACK_VIDEO_CODEC_BUTT
} CVI_Track_VideoCodec_E;


typedef enum cviTrack_AudioCodec_E
{
    CVI_TRACK_AUDIO_CODEC_G711Mu  = 0,   /**< G.711 Mu           */
    CVI_TRACK_AUDIO_CODEC_G711A   = 8,   /**< G.711 A            */
    CVI_TRACK_AUDIO_CODEC_G726    = 97,   /**< G.726              */
    CVI_TRACK_AUDIO_CODEC_AMR     = 101,   /**< AMR encoder format */
    CVI_TRACK_AUDIO_CODEC_ADPCM  = 104,   /**< ADPCM              */
    CVI_TRACK_AUDIO_CODEC_AAC = 105,
    CVI_TRACK_AUDIO_CODEC_WAV  = 108,   /**< WAV encoder        */
    CVI_TRACK_AUDIO_CODEC_MP3 = 109,
    CVI_TRACK_AUDIO_CODEC_BUTT
} CVI_Track_AudioCodec_E;


typedef struct cviCODEC_VIDEO_T{
    int32_t en;
    float framerate;
    CVI_Track_VideoCodec_E codec;
    uint32_t w;
    uint32_t h;
}CVI_CODEC_VIDEO_T;

typedef struct cviCODEC_AUDIO_T{
    int32_t en;
    CVI_Track_AudioCodec_E codec;
    uint32_t samplerate;
    uint32_t chns;
    float framerate;
}CVI_CODEC_AUDIO_T;

typedef struct cviCODEC_SUBTITLE_T{
    int32_t en;
    float framerate;
    uint32_t timebase;
}CVI_CODEC_SUBTITLE_T;

typedef struct cviCODEC_THUMBNAIL_T{
    int32_t en;
    int32_t res;
}CVI_CODEC_THUMBNAIL_T;

typedef enum CVI_MUXER_EVENT_E {
    CVI_MUXER_OPEN_FILE_FAILED,
    CVI_MUXER_SEND_FRAME_FAILED,
    CVI_MUXER_SEND_FRAME_TIMEOUT,
    CVI_MUXER_PTS_JUMP,
    CVI_MUXER_STOP,
    CVI_MUXER_EVENT_BUTT
} CVI_MUXER_EVENT_E;

typedef enum CVI_MUXER_FRAME_TYPE_E {
    CVI_MUXER_FRAME_TYPE_VIDEO,
    CVI_MUXER_FRAME_TYPE_AUDIO,
    CVI_MUXER_FRAME_TYPE_SUBTITLE,
    CVI_MUXER_FRAME_TYPE_THUMBNAIL,
    CVI_MUXER_FRAME_TYPE_BUTT
} CVI_MUXER_FRAME_TYPE_E;

typedef struct cviFRAME_INFO_T{
    uint32_t hmagic;
    CVI_MUXER_FRAME_TYPE_E type;
    int32_t isKey;
    int32_t gopInx;
    int64_t pts;
    uint64_t vpts;
    int32_t dataLen;
    int32_t extraLen;
    int32_t totalSize;
    uint32_t tmagic;
}CVI_FRAME_INFO_T;



typedef int (*CVI_MUXER_EVENT_CALLBACK_FN_PTR)(CVI_MUXER_EVENT_E event_type, const char *filename, void *p, void *extend);

typedef struct cviMUXER_ATTR_T{
    CVI_CODEC_VIDEO_T stvideocodec;
    CVI_CODEC_AUDIO_T staudiocodec;
    CVI_CODEC_SUBTITLE_T stsubtitlecodec;
    CVI_CODEC_THUMBNAIL_T stthumbnailcodec;
    char *devmod;
    int32_t alignflag;
    int32_t presize;
    CVI_MUXER_EVENT_CALLBACK_FN_PTR pfncallback;
    void *pfnparam;
}CVI_MUXER_ATTR_T;

int32_t cvi_muxer_create(CVI_MUXER_ATTR_T attr, void **muxer);
int cvi_muxer_start(void *muxer, const char *fname);
int cvi_muxer_write_packet(void *muxer, CVI_FRAME_INFO_T *packet);
void cvi_muxer_stop(void *muxer);
void cvi_muxer_destroy(void *muxer);

#define CVI_MUXER_EXT_DATA_LEN 4
extern const unsigned char g_ext_audio_data[CVI_MUXER_EXT_DATA_LEN];


#endif
