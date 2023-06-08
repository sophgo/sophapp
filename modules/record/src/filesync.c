#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "filesync.h"
#include "app_ipcam_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FILE_LIST_S{
    char filename[256];
    struct FILE_LIST_S *next;
}FILE_LIST_T;

typedef struct SYNC_INFO_S{
    FILE_LIST_T *file_list;
    pthread_t pthreadTaskId;
}SYNC_INFO_T;

static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static SYNC_INFO_T *g_sync_info = NULL;
static int g_task_flag = 0;

void sync_push(char *filename){
    pthread_mutex_lock(&sync_mutex);
    if(g_sync_info && filename && strlen(filename) > 0){
        FILE_LIST_T *file_list = (FILE_LIST_T *)malloc(sizeof(FILE_LIST_T));
        file_list->next = NULL;
        strncpy(file_list->filename, filename, sizeof(file_list->filename) - 1);
        if(g_sync_info->file_list == NULL){
            g_sync_info->file_list = file_list;
        }else{
            FILE_LIST_T *head = g_sync_info->file_list;
            while(head->next){
                head = head->next;
            }
            head->next = file_list;
        }
    }
    pthread_mutex_unlock(&sync_mutex);
}

static void sync_pop(char *filename){
    pthread_mutex_lock(&sync_mutex);
    if(g_sync_info){
        if(g_sync_info->file_list != NULL){
            FILE_LIST_T *file_list = g_sync_info->file_list;
            g_sync_info->file_list = g_sync_info->file_list->next;
            strcpy(filename, file_list->filename);
            free(file_list);
        }
    }
    pthread_mutex_unlock(&sync_mutex);
}

static void * sync_task(void *arg){
    SYNC_INFO_T *sync = (SYNC_INFO_T *)arg;
    char filename[256];
    while(!g_task_flag){
        if(sync == NULL || sync->file_list == NULL){
            //cvi_osal_task_sleep(50 * 1000);
            usleep(50 * 1000);
            continue;
        }
        memset(filename, 0x0, sizeof(filename));
        sync_pop(filename);
        if(strlen(filename) > 0){
            int fd = open(filename, O_WRONLY);
            if(fd > 0){
                fsync(fd);
                close(fd);
                APP_PROF_LOG_PRINT(LEVEL_DEBUG, "%s sync finished", filename);
            }
        }
        //cvi_osal_task_sleep(20 * 1000);
        usleep(20 * 1000);
    }
    return 0;
}

int sync_init(void){
    pthread_mutex_lock(&sync_mutex);
    if(g_sync_info){
        pthread_mutex_unlock(&sync_mutex);
        return 0;
    }

    if(g_sync_info == NULL){
        g_sync_info = (SYNC_INFO_T *)malloc(sizeof(SYNC_INFO_T));
    }

    if(g_sync_info == NULL){
        pthread_mutex_unlock(&sync_mutex);
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "g_sync_info malloc falied");
        return -1;
    }
    memset(g_sync_info, 0x0, sizeof(SYNC_INFO_T));
    g_task_flag = 0;
    //cvi_osal_task_attr_t ta;
    //ta.name = "sync_file";
    //ta.entry = sync_task;
    //ta.param = (void *)g_sync_info;
    //ta.priority = CVI_OSAL_PRI_NORMAL;
    //ta.detached = false;
    //cvi_osal_task_create(&ta, &g_sync_info->task);
    int rc = pthread_create(&g_sync_info->pthreadTaskId, NULL, sync_task, NULL);
    if (rc == 0) {
        pthread_setname_np(g_sync_info->pthreadTaskId, "sync_file");
    }
    pthread_mutex_unlock(&sync_mutex);
    return 0;
}

int sync_deinit(void){
    pthread_mutex_lock(&sync_mutex);
    // if(g_sync_info){
    //     while(g_sync_info->file_list){
    //         cvi_osal_task_sleep(100 * 1000);
    //     }
    //     g_task_flag = 1;
    //     cvi_osal_task_join(g_sync_info->task);
    //     cvi_osal_task_destroy(&g_sync_info->task);
    //     free(g_sync_info);
    //     g_sync_info = NULL;
    // }
    pthread_mutex_unlock(&sync_mutex);
    return 0;
}


#ifdef __cplusplus
}
#endif