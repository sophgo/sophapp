#ifndef __CVI_OTA_CGIC_H__
#define __CVI_OTA_CGIC_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* UPLOAD_FW_TEMP_DIR is the only setting you are likely to need
	to change in this file. */
/* Used only in Unix environments, in conjunction with mkstemp(). 
	Elsewhere (Windows), temporary files go where the tmpnam() 
	function suggests. If this behavior does not work for you, 
	modify the GetTempFile() function to suit your needs. */
#define UPLOAD_FW_TEMP_DIR "/mnt/sd/tmp"
#define UPLOAD_FW_MAX_SIZE 1073741824

// #define CGICDEBUG 1
#if CGICDEBUG
#define CGICDEBUGLOG(xx, arg...)                    \
    do                                              \
    {                                               \
        FILE *dout;                                 \
        dout = fopen(OTA_DEBUG_LOG_FILE, "a");      \
        fprintf(dout, "[%d]-" xx, __LINE__, ##arg); \
        fclose(dout);                               \
    } while (0)
#else /* CGICDEBUG */
#define CGICDEBUGLOG(xx, arg...)
#endif /* CGICDEBUG */

typedef enum
{
    cgiParseSuccess,
    cgiParseMemory,
    cgiParseIO
} CgiParseResultType_E;

/* One form entry, consisting of an attribute-value pair,
and an optional filename and content type. All of
these are guaranteed to be valid null-terminated strings,
which will be of length zero in the event that the
field is not present, with the exception of tfileName
which will be null when 'in' is null. DO NOT MODIFY THESE 
VALUES. Make local copies if modifications are desired. */

typedef struct CgiFormEntryStruct
{
    char *attr;
    /* value is populated for regular form fields only.
    For file uploads, it points to an empty string, and file
    upload data should be read from the file tfileName. */
    char *value;
    /* When fileName is not an empty string, tfileName is not null,
    and 'value' points to an empty string. */
    /* Valid for both files and regular fields; does not include
    terminating null of regular fields. */
    int valueLength;
    char *fileName;
    char *contentType;
    /* Temporary file descriptor for working storage of file uploads. */
    FILE *tFile;
    struct CgiFormEntryStruct *next;
} CgiFormEntry_S;

typedef struct
{
    /* Buffer for putting characters back */
    char putback[1024];
    /* Position in putback from which next character will be read.
    If readPos == writePos, then next character should
    come from s_httpConnFd. */
    int readPos;
    /* Position in putback to which next character will be put back.
    If writePos catches up to readPos, as opposed to the other
    way around, the stream no longer functions properly.
    Calling code must guarantee that no more than 
    sizeof(putback) bytes are put back at any given time. */
    int writePos;
    /* Offset in the virtual datastream; can be compared
    to cgiContentLength */
    int offset;
} MpStream_S, *MpStreamPtr_S;

typedef enum
{
    cgiEscapeRest,
    cgiEscapeFirst,
    cgiEscapeSecond
} CgiEscapeState_E;

typedef enum
{
    cgiUnescapeSuccess,
    cgiUnescapeMemory
} CgiUnescapeResultType_E;

static CgiParseResultType_E CgiParseGetFormInput();
static CgiParseResultType_E CgiParsePostFormInput();
static CgiParseResultType_E CgiParsePostMultipartInput();
static CgiParseResultType_E CgiParseFormInput(char *data, int length);
static void CgiGetEnv(char **s, char *var);
static void CgiSetupConstants();
static void CgiFreeResources();
static int CgiStrEqNc(char *s1, char *s2);
static int CgiStrBeginsNc(char *s1, char *s2);
static void CgiHeaderStatus(int status, char *statusMessage);
static CgiParseResultType_E GetTempFile(FILE **tFile);
static CgiUnescapeResultType_E CgiUnescapeChars(char **sp, char *cp, int len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif