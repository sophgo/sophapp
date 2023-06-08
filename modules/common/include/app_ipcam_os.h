#ifndef __APP_IPCAM_OS_H__
#define __APP_IPCAM_OS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/select.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define CVI_OSAL_SUCCESS           ((int)(0))
#define CVI_OSAL_ERR_FAILURE       ((int)(-1001))
#define CVI_OSAL_ERR_NOMEM         ((int)(-1002))
#define CVI_OSAL_ERR_TIMEOUT       ((int)(-1003))

#define CVI_OSAL_NO_WAIT           ((int64_t)(0))
#define CVI_OSAL_WAIT_FOREVER      ((int64_t)(-1))

#define CVI_OSAL_PRI_NORMAL        ((int)0)
#define CVI_OSAL_PRI_RT_LOWEST     ((int)1)
#define CVI_OSAL_PRI_RT_LOW        ((int)9)
#define CVI_OSAL_PRI_RT_MID        ((int)49)
#define CVI_OSAL_PRI_RT_HIGH       ((int)89)
#define CVI_OSAL_PRI_RT_HIGHEST    ((int)99)

#ifdef __cplusplus
extern "C"
{
#endif

/**********************************************
 *                 time
 *********************************************/

int app_ipcam_boot_time_get(uint64_t *time_us);

/**********************************************
 *                 mutex
 *********************************************/
typedef struct APP_PARAM_MUTEX_ATTR_S{
    const char                  *name;
} APP_PARAM_MUTEX_ATTR_T;

typedef struct APP_PARAM_MUTEX_S{
    APP_PARAM_MUTEX_ATTR_T      attr;
    sem_t                       mutex;
} APP_PARAM_MUTEX_T, *APP_PARAM_MUTEX_HANDLE_T;

int app_ipcam_mutex_create(
    APP_PARAM_MUTEX_ATTR_T      *attr,
    APP_PARAM_MUTEX_HANDLE_T    *mutex);

int app_ipcam_mutex_destroy(
    APP_PARAM_MUTEX_HANDLE_T    mutex);

int app_ipcam_mutex_lock(
    APP_PARAM_MUTEX_HANDLE_T    mutex,
    int64_t                     timeout_us);

int app_ipcam_mutex_unlock(
    APP_PARAM_MUTEX_HANDLE_T    mutex);

/**********************************************
 *                 task
 *********************************************/

typedef void *(*task_entry)(void *param);

typedef struct APP_PARAM_TASK_ATTR_S {
    const char                  *name;
    task_entry                  entry;
    void                        *param;
    int                         priority;
    bool                        detached;
} APP_PARAM_TASK_ATTR_T;

typedef struct APP_PARAM_TASK_S {
    APP_PARAM_TASK_ATTR_T       attr;
    pthread_t                   task;
} APP_PARAM_TASK_T, *APP_PARAM_TASK_HANDLE_T;

int app_ipcam_task_create(
    APP_PARAM_TASK_ATTR_T       *attr,
    APP_PARAM_TASK_HANDLE_T     *task);

int app_ipcam_task_destroy(
    APP_PARAM_TASK_HANDLE_T     *task);

int app_ipcam_task_join(
    APP_PARAM_TASK_HANDLE_T     task);

void app_ipcam_task_sleep(int64_t time_us);
void app_ipcam_task_resched(void);

#ifdef __cplusplus
}
#endif

#endif






