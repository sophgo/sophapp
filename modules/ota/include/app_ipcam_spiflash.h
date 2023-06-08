#ifndef __CVI_182X_SPIFLASH_OP_H__
#define __CVI_182X_SPIFLASH_OP_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <mtd/mtd-user.h>

#ifdef __cplusplus
extern "C"
{
#endif
    /*
 *	CIMG Header format total 64 bytes
 *	4 Bytes: Magic
 *	4 Bytes: Version
 *	4 Bytes: Chunk header size
 *	4 Bytes: Total chunks
 *	4 Bytes: File size
 *	32 Bytes: Extra Flags
 *	12 Bytes: Reserved
 */
typedef struct _CVI_UPGRADE_PARTITION_HEAD_S
{
    unsigned int u32Magic;
    unsigned int u32Version;
    unsigned int u32ChunkHeaderSize;
    unsigned int u32TotalChunk;
    unsigned int u32DataLen;
    unsigned char szExtraFlags[32];
    unsigned char reserved[12];
} CVI_UPGRADE_PARTITION_HEAD_S;

    /*
 *	CIMG Chunk format total 64 bytes
 *	4 Bytes: Chunk Type
 *	4 Bytes: Chunk data size
 *	4 Bytes: Program offset
 *	4 Bytes: Crc32 checksum
 *	48 Bytes: Reserved
 */
typedef struct _CVI_UPGRADE_CHUNK_HEAD_S
{
    unsigned int u32ChunkType;
    unsigned int u32DataSize;
    unsigned int u32ProgramOffset;
    unsigned int u32Crc32;
    unsigned int reserved[12];
} CVI_UPGRADE_CHUNK_HEAD_S;

#define MTDPATH "/dev/mtd"
#define MTDMAXNUM 6
#define MTD_BOOT_NUM 1
#define MTD_FIPBIN_NUM 0

#define BUFFER_SIZE 4096

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
