#include "app_ipcam_dump.h"
#include "app_ipcam_paramparse.h"
#include "linux/cvi_math.h"

int app_ipcam_Sensor_Raw_Dump(void)
{
    return 0;
}

int app_ipcam_Vi_Yuv_Dump(void)
{
    VIDEO_FRAME_INFO_S stVideoFrame;
    VI_CROP_INFO_S crop_info = {0};
    char yuvFileName[APP_IPCAM_MAX_FILE_LEN] = {0};
    snprintf(yuvFileName, APP_IPCAM_MAX_FILE_LEN, "/mnt/data/vi_0.yuv");

    if (CVI_VI_GetChnFrame(0, 0, &stVideoFrame, 3000) == 0) {
        FILE *output = NULL;
        size_t image_size = stVideoFrame.stVFrame.u32Length[0] + stVideoFrame.stVFrame.u32Length[1]
                + stVideoFrame.stVFrame.u32Length[2];
        CVI_VOID *vir_addr;
        CVI_U32 plane_offset, u32LumaSize, u32ChromaSize;

        APP_PROF_LOG_PRINT(LEVEL_INFO, "width: %d, height: %d, total_buf_length: %zu\n",
            stVideoFrame.stVFrame.u32Width,
            stVideoFrame.stVFrame.u32Height, image_size);

        snprintf(yuvFileName, sizeof(yuvFileName), "vi_0.yuv");

        output = fopen(yuvFileName, "wb");
        if (output == NULL) {
            CVI_VI_ReleaseChnFrame(0, 0, &stVideoFrame);
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "fopen %s fail\n", yuvFileName);
            return CVI_FAILURE;
        }

        u32LumaSize =  stVideoFrame.stVFrame.u32Stride[0] * stVideoFrame.stVFrame.u32Height;
        u32ChromaSize =  stVideoFrame.stVFrame.u32Stride[1] * stVideoFrame.stVFrame.u32Height / 2;
        CVI_VI_GetChnCrop(0, 0, &crop_info);
        if (crop_info.bEnable) {
            u32LumaSize = ALIGN((crop_info.stCropRect.u32Width * 8 + 7) >> 3, DEFAULT_ALIGN) *
                ALIGN(crop_info.stCropRect.u32Height, 2);
            u32ChromaSize = (ALIGN(((crop_info.stCropRect.u32Width >> 1) * 8 + 7) >> 3, DEFAULT_ALIGN) *
                ALIGN(crop_info.stCropRect.u32Height, 2)) >> 1;
        }
        vir_addr = CVI_SYS_Mmap(stVideoFrame.stVFrame.u64PhyAddr[0], image_size);
        CVI_SYS_IonInvalidateCache(stVideoFrame.stVFrame.u64PhyAddr[0], vir_addr, image_size);
        plane_offset = 0;
        for (int i = 0; i < 3; i++) {
            if (stVideoFrame.stVFrame.u32Length[i] != 0) {
                stVideoFrame.stVFrame.pu8VirAddr[i] = vir_addr + plane_offset;
                plane_offset += stVideoFrame.stVFrame.u32Length[i];
                // APP_PROF_LOG_PRINT(LEVEL_WARN,
                // 	   "plane(%d): paddr(%#"PRIx64") vaddr(%p) stride(%d) length(%d)\n",
                // 	   i, stVideoFrame.stVFrame.u64PhyAddr[i],
                // 	   stVideoFrame.stVFrame.pu8VirAddr[i],
                // 	   stVideoFrame.stVFrame.u32Stride[i],
                // 	   stVideoFrame.stVFrame.u32Length[i]);
                fwrite((void *)stVideoFrame.stVFrame.pu8VirAddr[i]
                    , (i == 0) ? u32LumaSize : u32ChromaSize, 1, output);
            }
        }
        CVI_SYS_Munmap(vir_addr, image_size);

        if (CVI_VI_ReleaseChnFrame(0, 0, &stVideoFrame) != 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "CVI_VI_ReleaseChnFrame NG\n");
        }

        fclose(output);
        return CVI_SUCCESS;
    }

    return 0;
}

int app_ipcam_VpssChn_Yuv_Dump(VENC_CHN VencChn, VIDEO_FRAME_INFO_S *pFrame)
{
    char yuvFileName[APP_IPCAM_MAX_FILE_LEN] = {0};
    snprintf(yuvFileName, APP_IPCAM_MAX_FILE_LEN, "/mnt/data/vpss%d.yuv", VencChn);
    FILE *yuvpFile = fopen(yuvFileName, "wb");
    if (yuvpFile == NULL) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "open file err, %s\n", yuvFileName);
        return 0;
    }

    CVI_VOID *vir_addr = NULL;
    CVI_U32 plane_offset, u32LumaSize, u32ChromaSize;
    size_t image_size;

    image_size = pFrame->stVFrame.u32Length[0] + pFrame->stVFrame.u32Length[1]
                + pFrame->stVFrame.u32Length[2];

    u32LumaSize =  pFrame->stVFrame.u32Stride[0] * pFrame->stVFrame.u32Height;
    if (pFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_RGB_888_PLANAR)
        u32ChromaSize = u32LumaSize;
    else if (pFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_422)
        u32ChromaSize =  pFrame->stVFrame.u32Stride[1] * pFrame->stVFrame.u32Height;
    else if (pFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_420)
        u32ChromaSize =  pFrame->stVFrame.u32Stride[1] * pFrame->stVFrame.u32Height / 2;
    else if (pFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_NV21)
        u32ChromaSize =  pFrame->stVFrame.u32Stride[1] * pFrame->stVFrame.u32Height / 2;
    else if (pFrame->stVFrame.enPixelFormat == PIXEL_FORMAT_YUV_400)
        u32ChromaSize =  0;
    else
        u32ChromaSize =  0;

    vir_addr = CVI_SYS_Mmap(pFrame->stVFrame.u64PhyAddr[0], image_size);
    plane_offset = 0;
    for (int i = 0; i < 3; i++) {
        if (pFrame->stVFrame.u32Length[i] == 0)
            continue;

        pFrame->stVFrame.pu8VirAddr[i] = vir_addr + plane_offset;
        plane_offset += pFrame->stVFrame.u32Length[i];
        
        // APP_PROF_LOG_PRINT(LEVEL_INFO, "plane(%d): paddr(#%I64x) vaddr(%p) stride(%d)\n",
        // 	   i,
        //        pFrame->stVFrame.u64PhyAddr[i],
        // 	   pFrame->stVFrame.pu8VirAddr[i],
        // 	   pFrame->stVFrame.u32Stride[i]);

        fwrite((void *)(uintptr_t)pFrame->stVFrame.pu8VirAddr[i], 
                (i == 0) ? u32LumaSize : u32ChromaSize, 1, yuvpFile);
    }

    CVI_SYS_Munmap(vir_addr, image_size);

    fclose(yuvpFile);

    return 0;
}

int app_ipcam_Venc_streaming_Dump(void)
{
    return 0;
}

