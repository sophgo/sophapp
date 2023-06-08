#include "app_ipcam_spiflash.h"
#include "app_ipcam_ota.h"

//delete backup firmware
static void DeleteBackupFW(void)
{
    char cmd[256] = {0};
    snprintf(cmd, sizeof(cmd) - 1, "/bin/rm -f %s %s %s", BACKUP_BOOT_IMG_FILE_PATH, BACKUP_FIP_BIN_FILE_PATH, BACKUP_ROOTFS_IMG_FILE_PATH);
    system(cmd);
}

//rm -rf /mnt/usb/.ota && mkdir /mnt/usb/.ota
static int cleanOTADir(void)
{
    char cmd[100] = {0};
    int ret = 0;
    snprintf(cmd, sizeof(cmd) - 1, "/bin/rm -rf %s; /bin/mkdir %s;sync", OTA_ZIP_UNPACK_DIR, OTA_ZIP_UNPACK_DIR);
    ret = system(cmd);
    if (ret != 0)
        printf("%s failed!ret = %d\n", cmd, ret);

    return ret;
}

//bacpup the fip.bin/boot.spinor/rootfs.spinor
static void BackupFW(void)
{
    char cmd[256] = {0};
    snprintf(cmd, sizeof(cmd) - 1, "/bin/cp -f %s %s %s %s;sync", BOOT_IMG_FILE_PATH, FIP_BIN_FILE_PATH, ROOTFS_IMG_FILE_PATH, BACKUP_FW_DIR);
    system(cmd);
}

int app_ipcam_OTA_UnpackZip(const char *fileName, FILE *otaLogFile)
{
    char cmd[256] = {0};
    int ret = 0;
    if (access(fileName, F_OK) != 0)
    {
        fprintf(otaLogFile, "the %s is null.\n", fileName);
        fflush(otaLogFile);
        return OTA_RET_FAIL;
    }

    //clear the firmware for backup
    DeleteBackupFW();
    //clear OTA dir
    ret = cleanOTADir();
    //unzip META/misc_info.txt for check BoardType
    //....
    //unzip the upgrade.zip
    snprintf(cmd, sizeof(cmd) - 1, "/usr/bin/unzip -o %s -d %s;sync", fileName, OTA_ZIP_UNPACK_DIR);
    ret |= system(cmd);
    //check md5 num
    snprintf(cmd, sizeof(cmd) - 1, "cd %s; /usr/bin/md5sum -c META/metadata.txt >> %s", OTA_ZIP_UNPACK_DIR, OTA_STATUS_LOG_FILE);
    ret |= system(cmd);
    if (ret != 0)
    {
        printf("%s failed!ret = %d\n", cmd, ret);
        goto UNPACK_ERR;
    }

    fprintf(otaLogFile, "OTA Unpack %s zip success.\n", fileName);
    fflush(otaLogFile);
    BackupFW();
    //del upgrade.zip
    remove(fileName);
    return ret;
UNPACK_ERR:
    fprintf(otaLogFile, "OTA Unpack %s zip failed.\n", fileName);
    fflush(otaLogFile);
    //del upgrade.zip
    remove(fileName);
    return ret;
}
typedef struct otaUpdateInfo {
    char mtdName[16];
    char fileName[32];
    int mtdNum;
} otaUpdateInfo_t;

//通过/proc/mtd来获取到分区信息,兼容所有板卡的分区
static int getMtdInfo(otaUpdateInfo_t *mtdInfo)
{
    //定义要升级的分区名
    char *mtdName[MTDMAXNUM] = {"fip" , "BOOT" , "ROOTFS" , NULL , NULL , NULL};
    char *imageName[MTDMAXNUM] = {FIP_BIN_FILE_PATH, BOOT_IMG_FILE_PATH, ROOTFS_IMG_FILE_PATH, NULL , NULL , NULL};
    int mtdNum = 0 , mtdIndex = 0;
    char readBuf[128] = {0};
    // awk '{print $1 $4}' /proc/mtd
    FILE *mtdFile = popen("awk '{print $1 $4}' /proc/mtd" , "r");
    if(mtdFile == NULL || mtdInfo == NULL)
    {
        printf("popen mtd info failed!\n");
        return mtdNum;
    }

    while(fgets(readBuf , sizeof(readBuf) , mtdFile) != NULL)
    {
        //通过分区明找到了需要升级的分区坐标
        if(mtdName[mtdIndex] != NULL && strstr(readBuf , mtdName[mtdIndex]) != NULL)
        {
            if(imageName[mtdIndex] != NULL)
            {
                memcpy(mtdInfo[mtdIndex].fileName , imageName[mtdIndex] , strlen(imageName[mtdIndex]) + 1);
            }
            sscanf(readBuf , "mtd%d:%s\r\n" , &(mtdInfo[mtdIndex].mtdNum) , mtdInfo[mtdIndex].mtdName);
            mtdIndex++;
            mtdNum++;
        }
    }

    pclose(mtdFile);

    return mtdNum;
}
/*
    update firmware
    if update success , del backup firmware.
    if update failed , the next reboot will update from SD card.
*/
int app_ipcam_OTA_UpdataFW(FILE *otaLogFile)
{
    char mtdPartPath[128] = {0};
    int ret = 0, mtdFd = 0, mtdNum = 0, len = 0 , mtdIndex = 0;
    mtd_info_t mtdInfo[MTDMAXNUM];
    erase_info_t eraseInfo;
    CVI_UPGRADE_PARTITION_HEAD_S imageHead;
    CVI_UPGRADE_CHUNK_HEAD_S fileHead;
    char writeBuf[BUFFER_SIZE] = {0};
    int imageFd = 0;
    otaUpdateInfo_t updateInfo[MTDMAXNUM] = {0};
    mtdNum = getMtdInfo(updateInfo);

    for (mtdIndex = 0; mtdIndex < mtdNum; mtdIndex++)
    {
        memset(mtdPartPath, 0, sizeof(mtdPartPath));
        sprintf(mtdPartPath, "%s%d", MTDPATH, updateInfo[mtdIndex].mtdNum);
        printf("open dev name(%s).\n", mtdPartPath);

        mtdFd = open(mtdPartPath, O_RDWR);
        if (mtdFd <= 0)
        {
            printf("open failed!\n");
        }

        if (ioctl(mtdFd, MEMGETINFO, &mtdInfo[mtdIndex]))
        {
            printf("Can't get \"%s\" information.\n", mtdPartPath);
            close(mtdFd);
            continue;
        }

        {
            //打印分区信息
            printf("########## mtdInfo : %s #########\n" , updateInfo[mtdIndex].mtdName);
            printf("type = %d \n", mtdInfo[mtdIndex].type);
            printf("flags = %d \n", mtdInfo[mtdIndex].flags);
            printf("Size = %d \n", mtdInfo[mtdIndex].size);
            printf("eraseSize = %d\n", mtdInfo[mtdIndex].erasesize);
            printf("writeSize = %d\n", mtdInfo[mtdIndex].writesize);
            printf("OobSize = %d\n", mtdInfo[mtdIndex].oobsize);
            //如果文件存在，则继续
            if (access(updateInfo[mtdIndex].fileName, F_OK) == 0)
            {
                imageFd = open(updateInfo[mtdIndex].fileName, O_RDONLY);
                if (imageFd <= 0)
                {
                    printf("open %s failed.\n", updateInfo[mtdIndex].fileName);
                    close(mtdFd);
                    return OTA_RET_FAIL;
                }

                fprintf(otaLogFile, "Update file(%s) to MTD%d \n", updateInfo[mtdIndex].fileName, updateInfo[mtdIndex].mtdNum);
                fflush(otaLogFile);

                if (updateInfo[mtdIndex].mtdNum != MTD_FIPBIN_NUM)
                {
                    //读取image的头信息
                    len = read(imageFd, &imageHead, sizeof(imageHead));
                    if (len != sizeof(imageHead))
                    {
                        printf("read %zd failed.ret = %d\n", sizeof(imageHead), len);
                        close(imageFd);
                        return OTA_RET_FAIL;
                    }

                    //打印image头信息
                    printf("Magic = %d , Version = %d , ChunkHeaderSize = %d , TotalChunk = %d , DataLen = %d\n" ,
                            imageHead.u32Magic , imageHead.u32Version , imageHead.u32ChunkHeaderSize , imageHead.u32TotalChunk , imageHead.u32DataLen);

                    //读取file烧录文件的头信息
                    len = read(imageFd, &fileHead, sizeof(fileHead));
                    if (len != sizeof(fileHead))
                    {
                        printf("read fileHead %zd failed. ret = %d", sizeof(fileHead), len);
                        close(imageFd);
                        return OTA_RET_FAIL;
                    }

                    //打印file烧录文件的头信息
                    printf("ChunkType = %d , DataSize = %d , ProgramOffset = %d , Crc32 = %d\n" ,
                        fileHead.u32ChunkType , fileHead.u32DataSize , fileHead.u32ProgramOffset , fileHead.u32Crc32);
                }

                //进行擦除
                eraseInfo.start = 0x0;
                eraseInfo.length = mtdInfo[mtdIndex].size;
                printf("erase start 0x%x , length 0x%x ...\n", eraseInfo.start, eraseInfo.length);
                if (ioctl(mtdFd, MEMERASE, &eraseInfo))
                {
                    printf("Can't erase \"%s\" length = %d.\n", mtdPartPath, eraseInfo.length);
                    close(mtdFd);
                    return OTA_RET_FAIL;
                }
                printf("erase mtd%d success.\n", updateInfo[mtdIndex].mtdNum);

                //进行烧录
                while (1)
                {
                    //读文件
                    memset(writeBuf, 0, sizeof(writeBuf));
                    len = read(imageFd, writeBuf, BUFFER_SIZE);
                    if (len <= 0)
                    {
                        printf("read end ! ret = %d\n", len);
                        break;
                    }

                    ret = write(mtdFd, writeBuf, len);
                    if (ret <= 0)
                    {
                        printf("write failed! ret = %d\n", ret);
                        break;
                    }
                }
                close(imageFd);
            }
        }
        close(mtdFd);
    }
    //update success,del the backup Firmware.
    printf("[%d] delete backup fw...\n" , __LINE__);
    DeleteBackupFW();

    printf("[%d] fflush log... \n" , __LINE__);
    fprintf(otaLogFile, "OTA Upload Success.\n");
    fflush(otaLogFile);
    return OTA_RET_OK;
}
