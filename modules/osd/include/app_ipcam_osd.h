#ifndef __APP_IPCAM_OSD_H__
#define __APP_IPCAM_OSD_H__

#include <stdbool.h>
#include "cvi_type.h"
#include "linux/cvi_comm_region.h"
#include "app_ipcam_mq.h"
#include "cvi_region.h"
#include <cvi_osdc.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define MAX_FILE_LEN    32
#define DEBUG_STR_LEN   32
#define APP_OSD_STR_LEN_MAX     64
#define OSDC_OBJS_MAX 128
#define OSDC_NUM_MAX 2

#define COLOR_WHITE(FORMATE)  ((FORMATE) ? 0xFFFFFFFF : 0xFFFF)
#define COLOR_BLACK(FORMATE)  ((FORMATE) ? 0xFF000000 : 0x8000)
#define COLOR_BLUE(FORMATE)   ((FORMATE) ? 0xFF0000FF : 0x801F)
#define COLOR_GREEN(FORMATE)  ((FORMATE) ? 0xFF00FF00 : 0x83E0)
#define COLOR_RED(FORMATE)    ((FORMATE) ? 0xFFFF0000 : 0xFC00)
#define COLOR_CYAN(FORMATE)   ((FORMATE) ? 0xFF00FFFF : 0x83FF)
#define COLOR_YELLOW(FORMATE) ((FORMATE) ? 0xFFFFFF00 : 0xFFE0)
#define COLOR_PINK(FORMATE)   ((FORMATE) ? 0xFFFF00FF : 0xFC1F)

#define IDX_TO_COLOR(FORMATE, IDX, COLOR)  do {         \
    if ((IDX) == 0) (COLOR) = COLOR_WHITE(FORMATE);       \
    else if ((IDX) == 1) (COLOR) = COLOR_WHITE(FORMATE);  \
    else if ((IDX) == 2) (COLOR) = COLOR_BLACK(FORMATE);  \
    else if ((IDX) == 3) (COLOR) = COLOR_BLUE(FORMATE);  \
    else if ((IDX) == 4) (COLOR) = COLOR_GREEN(FORMATE);  \
    else if ((IDX) == 5) (COLOR) = COLOR_RED(FORMATE);  \
    else if ((IDX) == 6) (COLOR) = COLOR_CYAN(FORMATE);  \
    else if ((IDX) == 7) (COLOR) = COLOR_YELLOW(FORMATE);  \
    else if ((IDX) == 8) (COLOR) = COLOR_PINK(FORMATE);  \
} while(0)

#define COLOR_TO_IDX(FORMATE, IDX, COLOR)  do {         \
    if ((COLOR) == COLOR_WHITE(FORMATE)) (IDX) = 0;       \
    else if ((COLOR) == COLOR_WHITE(FORMATE)) (IDX) = 1;  \
    else if ((COLOR) == COLOR_BLACK(FORMATE)) (IDX) = 2;  \
    else if ((COLOR) == COLOR_BLUE(FORMATE)) (IDX) = 3;  \
    else if ((COLOR) == COLOR_GREEN(FORMATE)) (IDX) = 4;  \
    else if ((COLOR) == COLOR_RED(FORMATE)) (IDX) = 5;  \
    else if ((COLOR) == COLOR_CYAN(FORMATE)) (IDX) = 6;  \
    else if ((COLOR) == COLOR_YELLOW(FORMATE)) (IDX) = 7;  \
    else if ((COLOR) == COLOR_PINK(FORMATE)) (IDX) = 8;  \
} while(0)

typedef enum OSD_TYPE_T {
    TYPE_PICTURE,
    TYPE_STRING,
    TYPE_TIME,
    TYPE_DEBUG,
    TYPE_END
} OSD_TYPE_E;



typedef struct APP_OSDC_OBJS_INFO_T {
    CVI_BOOL bShow;
    RGN_CMPR_TYPE_E type;
    CVI_U32 color;
    CVI_U32 x1;
    CVI_U32 y1;
    CVI_U32 x2;
    CVI_U32 y2;
    CVI_U32 width;
    CVI_U32 height;
    CVI_BOOL filled;
    CVI_S32 thickness;
    OSD_TYPE_E enType;
    CVI_S32 maxlen;
    union {
        char filename[MAX_FILE_LEN];
        char str[APP_OSD_STR_LEN_MAX];
    };
    CVI_U64 u64BitmapPhyAddr;
    CVI_VOID *pBitmapVirAddr;
} APP_OSDC_OBJS_INFO_S;

typedef struct APP_PARAM_OSDC_CFG_T {
    CVI_BOOL enable;
    RGN_HANDLE handle[OSDC_NUM_MAX];
    MMF_CHN_S  mmfChn[OSDC_NUM_MAX];
    CVI_BOOL bShow[OSDC_NUM_MAX];
    CVI_U32 VpssGrp[OSDC_NUM_MAX];
    CVI_U32 VpssChn[OSDC_NUM_MAX];
    CVI_U32 CompressedSize[OSDC_NUM_MAX];
    OSDC_OSD_FORMAT_E format[OSDC_NUM_MAX];
    CVI_BOOL bShowPdRect[OSDC_NUM_MAX];
    CVI_BOOL bShowMdRect[OSDC_NUM_MAX];
    CVI_BOOL bShowFdRect[OSDC_NUM_MAX];
    CVI_U32 osdcObjNum[OSDC_NUM_MAX];
    APP_OSDC_OBJS_INFO_S osdcObj[OSDC_NUM_MAX][OSDC_OBJS_MAX];
} APP_PARAM_OSDC_CFG_S;



APP_PARAM_OSDC_CFG_S *app_ipcam_Osdc_Param_Get(void);
int app_ipcam_Osdc_Init(void);
int app_ipcam_Osdc_DeInit(void);

#ifdef WEB_SOCKET
APP_OSDC_OBJS_INFO_S *app_ipcam_OsdcPrivacy_Param_Get(void);
#endif
/*****************************************************************
 *  The following API for command test used             S
 * **************************************************************/
void app_ipcam_Osdc_Status(APP_PARAM_OSDC_CFG_S *pstOsdcCfg);
int app_ipcam_CmdTask_Rect_Switch(CVI_MQ_MSG_t *msg, CVI_VOID *userdate);
/*****************************************************************
 *  The above API for command test used                 E
 * **************************************************************/

#ifdef __cplusplus
}
#endif

#endif
