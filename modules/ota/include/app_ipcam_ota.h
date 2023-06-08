#ifndef __CVI_OTA_H__
#define __CVI_OTA_H__
/*
Define a unified external interface for OTA functions
*/

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#define OTA_ROOT_DIR        "/mnt/sd/"
#define OTA_ZIP_UNPACK_DIR OTA_ROOT_DIR"/.ota"
#define FIP_BIN_FILE_PATH OTA_ZIP_UNPACK_DIR "/fip.bin"
#define BOOT_IMG_FILE_PATH OTA_ZIP_UNPACK_DIR "/boot.spinor"
#define ROOTFS_IMG_FILE_PATH OTA_ZIP_UNPACK_DIR "/rootfs.spinor"

#define BACKUP_FW_DIR OTA_ROOT_DIR
#define BACKUP_FIP_BIN_FILE_PATH BACKUP_FW_DIR "/fip.bin"
#define BACKUP_BOOT_IMG_FILE_PATH BACKUP_FW_DIR "/boot.spinor"
#define BACKUP_ROOTFS_IMG_FILE_PATH BACKUP_FW_DIR "/rootfs.spinor"

#define OTA_STATUS_LOG_DIR  OTA_ROOT_DIR"/log"
#define OTA_STATUS_LOG_FILE OTA_STATUS_LOG_DIR"/OTA"
#define OTA_DEBUG_LOG_FILE OTA_STATUS_LOG_DIR"/debugLog"

#define OTA_RET_OK 0
#define OTA_RET_FAIL -1

void *app_ipcam_OTA_UploadFW(void *param);
int app_ipcam_OTA_UnpackZip(const char *fileName, FILE *otaLogFile);
void *app_ipcam_OTA_UnpackAndUpdate(void *param);
int app_ipcam_OTA_UpdataFW(FILE *otaLogFile);

int app_ipcam_OTA_GetSystemVersion(char *buf, int bufLen);
int app_ipcam_OTA_GetUpgradeStatus(char *buf, int bufLen);
int app_ipcam_OTA_CloseThreadBeforeUpgrade(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif