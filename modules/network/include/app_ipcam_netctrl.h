#ifndef __APP_IPCAM_NETCTRL_H__
#define __APP_IPCAM_NETCTRL_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define APP_VENC_CHN_NUM    2
#define APP_JPEG_VENC_CHN   2
#define DARW_BOX_ON_CANVAS_FRAME_WIDTH 640 

typedef struct _CVI_IMG_INFO_S {
    uint32_t brightness;
    uint32_t sharpness;
    uint32_t chroma;
    uint32_t contrast;
    uint32_t saturation;
    uint32_t noise2d;
    uint32_t noise3d;
    uint32_t compensationEnabled;
    uint32_t compensation;
    uint32_t suppressionEnabled;
    uint32_t suppression;
    uint32_t whitebalance;
    uint32_t red_gain;
    uint32_t blue_gain;
    uint32_t defogEnabled;
    uint32_t defog;
    uint32_t shutterEnabled;
    uint32_t shutter;
    uint32_t distortionEnabled;
    uint32_t distortion;

    // switch
    uint32_t frequency;
    uint32_t antiflashEnabled;
    uint32_t hflipEnabled;
    uint32_t vflipEnabled;
    uint32_t wdrEnabled;
    uint32_t irCutEnabled;
    uint32_t irCutEnabledManual;
    uint32_t keepColorEnabled;
    uint32_t disEnabled;
    uint32_t dsEnabled;
} CVI_IMG_INFO_S;

typedef enum _IPC_OSD_NAME {
    IPC_OSD0_TIME,          //  时间
    IPC_OSD1_STR,               //  cvitek
    IPC_OSD2_STR,               //  E:113.95 GPS
    IPC_OSD3_STR,               //  IPCamera
    IPC_OSD4_BMP,               //  picture
} IPC_OSD_NAME_E;

typedef struct APP_VENC_ATTR_INFO_T {
    CVI_BOOL bNeedChange;
    int chn;
    int enabled;
    int codec;
    int resolution;
    int widthMax;
    int heightMax;
    int fps;
    int profile;
    int rc;
    int bitrate;
    // int gop;
    // int fixed_IQP;
    // int fixed_QPQ;
    // int statTime;
    // int firstFrmstartQp;
    // int initialDelay;
    // int MaxIprop;
    // int MinIprop;
    // int MaxIQp;
    // int MinIQp;
    // int MaxQp;
    // int MinQp;
    // int ChangePos;
    // int MinStillPercent;
    // int MinStillPSNR;
    // int MaxStillQP;
} APP_VENC_ATTR_INFO_S;

typedef struct APP_VENC_ATTR_CHANGE_T {
    CVI_BOOL bNeedStopVenc;
    CVI_BOOL bCodec;
    CVI_BOOL bResolution;
    CVI_BOOL bFps;
    CVI_BOOL bProfile;
    CVI_BOOL bRCMode;
    CVI_BOOL bBitrate;
} APP_VENC_ATTR_CHANGE_S;

typedef enum APP_VIDEOENCTYPE_T {
    APP_VIDEOENCTYPE_H264 = 0,
    APP_VIDEOENCTYPE_H265 = 1,
    APP_VIDEOENCTYPE_MJPEG = 2,
    APP_VIDEOENCTYPE_JPEG = 3,
    APP_VIDEOENCTYPE_BUTT
} APP_VIDEOENCTYPE_E;

typedef struct _APP_MD_INFO_S {
    bool enabled;
    uint32_t threshold;
} APP_MD_INFO_S;

typedef struct _APP_CRY_INFO_S {
    bool enabled;
} APP_CRY_INFO_S;
typedef struct _APP_PD_INFO_S {
    bool enabled;
    bool Intrusion_enabled;
    float threshold;
    uint32_t region_stRect_x1;
    uint32_t region_stRect_y1;
    uint32_t region_stRect_x2;
    uint32_t region_stRect_y2;
    uint32_t region_stRect_x3;
    uint32_t region_stRect_y3;
    uint32_t region_stRect_x4;
    uint32_t region_stRect_y4;
    uint32_t region_stRect_x5;
    uint32_t region_stRect_y5;
    uint32_t region_stRect_x6;
    uint32_t region_stRect_y6;
} APP_PD_INFO_S;


typedef enum APP_VIDEO_RESOLUTION_T {
    APP_RESOLUTION_1440P_W = 2560,
    APP_RESOLUTION_1440P_H = 1440,
    APP_RESOLUTION_1296P_W = 2304,
    APP_RESOLUTION_1296P_H = 1296,
    APP_RESOLUTION_1080P_W = 1920,
    APP_RESOLUTION_1080P_H = 1080,
    APP_RESOLUTION_720P_W  = 1280,
    APP_RESOLUTION_720P_H  = 720,
    APP_RESOLUTION_576P_W  = 720,
    APP_RESOLUTION_576P_H  = 576,
    APP_RESOLUTION_360P_W  = 640,
    APP_RESOLUTION_360P_H  = 360,
} APP_VIDEO_RESOLUTION_E;

typedef enum APP_VIDEOPROFILE_T {
    APP_VIDEOPROFILE_BASE = 0,
    APP_APP_VIDEOPROFILE_HIGH,
    APP_VIDEOPROFILE_MAIN,
    APP_VIDEOPROFILE_HIGH,
    APP_VIDEOPROFILE_BUTT
} APP_VIDEOPROFILE_E;

typedef enum APP_VIDEORCMODE_T {
    APP_VIDEORCMODE_CBR = 0,
    APP_VIDEORCMODE_VBR = 1,
    APP_VIDEORCMODE_AVBR = 2,
} APP_VIDEORCMODE_E;

int app_ipcam_NetCtrl_Init();
int app_ipcam_NetCtrl_DeInit();

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


#endif