#ifndef __APP_IPCAM_LOADBMP_H__
#define __APP_IPCAM_LOADBMP_H__

#include "cvi_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

/* the color format OSD supported */
typedef enum _OSD_COLOR_FMT_E {
    OSD_COLOR_FMT_RGB444 = 0,
    OSD_COLOR_FMT_RGB4444 = 1,
    OSD_COLOR_FMT_RGB555 = 2,
    OSD_COLOR_FMT_RGB565 = 3,
    OSD_COLOR_FMT_RGB1555 = 4,
	OSD_COLOR_FMT_8BIT_MODE = 5,
    OSD_COLOR_FMT_RGB888 = 6,
    OSD_COLOR_FMT_RGB8888 = 7,
    OSD_COLOR_FMT_BUTT
} OSD_COLOR_FMT_E;

typedef struct _OSD_RGB_S {
    CVI_U8 u8B;
    CVI_U8 u8G;
    CVI_U8 u8R;
    CVI_U8 u8Reserved;
} OSD_RGB_S;

typedef struct _OSD_SURFACE_S {
    OSD_COLOR_FMT_E enColorFmt; /* color format */
    CVI_U16 u16Height; /* operation height */
    CVI_U16 u16Width; /* operation width */
    CVI_U16 u16Stride; /* surface stride */
    CVI_U16 u16Reserved;
} OSD_SURFACE_S;

typedef struct _Logo {
    CVI_U32 width; /* out */
    CVI_U32 height; /* out */
    CVI_U32 stride; /* in */
    CVI_U8 *pRGBBuffer; /* in/out */
} OSD_LOGO_T;

typedef struct _BITMAPINFOHEADER {
    CVI_U32 biSize;
    CVI_U32 biWidth;
    CVI_S32 biHeight;
    CVI_U16 biPlanes;
    CVI_U16 biBitCount;
    CVI_U32 biCompression;
    CVI_U32 biSizeImage;
    CVI_U32 biXPelsPerMeter;
    CVI_U32 biYPelsPerMeter;
    CVI_U32 biClrUsed;
    CVI_U32 biClrImportant;
} OSD_BITMAPINFOHEADER;

typedef struct _BITMAPFILEHEADER {
    CVI_U32 bfSize;
    CVI_U16 bfReserved1;
    CVI_U16 bfReserved2;
    CVI_U32 bfOffBits;
} OSD_BITMAPFILEHEADER;

typedef struct _RGBQUAD {
    CVI_U8 rgbBlue;
    CVI_U8 rgbGreen;
    CVI_U8 rgbRed;
    CVI_U8 rgbReserved;
} OSD_RGBQUAD;

typedef struct _BITFIELD {
    CVI_U32 r_mask;
    CVI_U32 g_mask;
    CVI_U32 b_mask;
    CVI_U32 a_mask;
} OSD_BITFIELD;

typedef struct _BITMAPINFO {
    OSD_BITMAPINFOHEADER bmiHeader;
    union {
        OSD_RGBQUAD bmiColors[1];
        OSD_BITFIELD bitfield;
    };
} OSD_BITMAPINFO;

typedef struct _OSD_COMPONENT_INFO_S {
    int alen;
    int rlen;
    int glen;
    int blen;
} OSD_COMP_INFO;

CVI_S32 CreateSurfaceByBitMap(const CVI_CHAR *pszFileName, OSD_SURFACE_S *pstSurface, CVI_U8 *pu8Virt);
CVI_S32 CreateSurfaceByCanvas(const CVI_CHAR *pszFileName, OSD_SURFACE_S *pstSurface, CVI_U8 *pu8Virt, CVI_U32 u32Width,
                CVI_U32 u32Height, CVI_U32 u32Stride);
CVI_S32 GetBmpInfo(const CVI_CHAR *filename, OSD_BITMAPFILEHEADER *pBmpFileHeader, OSD_BITMAPINFO *pBmpInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* End of #ifndef __LOAD_BMP_H__*/
