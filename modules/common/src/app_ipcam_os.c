#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#include "app_ipcam_os.h"
#include "app_ipcam_comm.h"

/**********************************************
 *                 time
 *********************************************/

int app_ipcam_boot_time_get(uint64_t *time_us)
{
    struct timespec timeo;

    clock_gettime(CLOCK_MONOTONIC, &timeo);
    *time_us = (uint64_t)timeo.tv_sec * 1000000 + timeo.tv_nsec / 1000;

    return APP_IPCAM_SUCCESS;
}

/**********************************************
 *                 mutex
 *********************************************/

int app_ipcam_mutex_create(
    APP_PARAM_MUTEX_ATTR_T      *attr,
    APP_PARAM_MUTEX_HANDLE_T    *mutex)
{
    APP_PARAM_MUTEX_HANDLE_T pMutexHandle;
    pMutexHandle = calloc(sizeof(APP_PARAM_MUTEX_T), 1);
    if (pMutexHandle == NULL) {
        return CVI_OSAL_ERR_NOMEM;
    }
    if (attr) {
        memcpy(&pMutexHandle->attr, attr, sizeof(APP_PARAM_MUTEX_ATTR_T));
    }
    
    sem_init((sem_t *)&pMutexHandle->mutex, 0, 1);
    *mutex = pMutexHandle;

    return APP_IPCAM_SUCCESS;
}


int app_ipcam_mutex_destroy(
    APP_PARAM_MUTEX_HANDLE_T    mutex)
{
    sem_destroy((sem_t *)&mutex->mutex);
    free(mutex);

    return APP_IPCAM_SUCCESS;
}

int app_ipcam_mutex_lock(
    APP_PARAM_MUTEX_HANDLE_T    mutex,
    int64_t                     timeout_us)
{
    struct timespec timeo;

    switch (timeout_us) {
    case CVI_OSAL_NO_WAIT:
        if (sem_trywait((sem_t *)&mutex->mutex)) {
            return CVI_OSAL_ERR_TIMEOUT;
        }
        break;
    case CVI_OSAL_WAIT_FOREVER:
        if (sem_wait((sem_t *)&mutex->mutex)) {
            return CVI_OSAL_ERR_FAILURE;
        }
        break;
    default:
        clock_gettime(CLOCK_REALTIME, &timeo);
        timeo.tv_sec += timeout_us / 1000000;
        timeo.tv_nsec += (timeout_us % 1000000) * 1000;
        if (timeo.tv_nsec > 1000000000) {
            timeo.tv_sec++;
            timeo.tv_nsec -= 1000000000;
        }
        if (sem_timedwait((sem_t *)&mutex->mutex, &timeo)) {
            return CVI_OSAL_ERR_TIMEOUT;
        }
        break;
    }

    return APP_IPCAM_SUCCESS;
}

int app_ipcam_mutex_unlock(
    APP_PARAM_MUTEX_HANDLE_T    mutex)
{
    if (sem_post((sem_t *)&mutex->mutex))
        return CVI_OSAL_ERR_FAILURE;

    return APP_IPCAM_SUCCESS;
}


/**********************************************
 *                 task
 *********************************************/

static void *app_ipcam_task_entry(void *arg)
{
    APP_PARAM_TASK_HANDLE_T task = (APP_PARAM_TASK_HANDLE_T)arg;
    struct sched_param sched_param;
    int policy;

    prctl(PR_SET_NAME, (unsigned long int)task->attr.name, 0, 0, 0);
    pthread_getschedparam(pthread_self(), &policy, &sched_param);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "%s: PID:%d,TID:%ld, PTHREAD:%lu, POLICY=%d, PRIORITY=%d\n",
        task->attr.name, getpid(), syscall(SYS_gettid), (unsigned long int)pthread_self(), policy, sched_param.sched_priority);

    task->attr.entry(task->attr.param);

    if (task->attr.detached) {
        free(arg);
    }

    return NULL;
}

int app_ipcam_task_create(
    APP_PARAM_TASK_ATTR_T       *attr,
    APP_PARAM_TASK_HANDLE_T     *task)
{
    APP_PARAM_TASK_HANDLE_T pTaskHandle;
    pthread_attr_t pthread_attr;
    struct sched_param sched_param;

    pTaskHandle = calloc(sizeof(APP_PARAM_TASK_T), 1);
    if (pTaskHandle == NULL) {
        return CVI_OSAL_ERR_NOMEM;
    }
    memcpy(&pTaskHandle->attr, attr, sizeof(APP_PARAM_TASK_ATTR_T));

    pthread_attr_init(&pthread_attr);
    if (attr->detached) {
        pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
    } else {
        pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_JOINABLE);
    }
    if (attr->priority != CVI_OSAL_PRI_NORMAL) {
        pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&pthread_attr, SCHED_RR);
        pthread_attr_getschedparam(&pthread_attr, &sched_param);
        sched_param.sched_priority = attr->priority;
        pthread_attr_setschedparam(&pthread_attr, &sched_param);
    } else {
        pthread_attr_setschedpolicy(&pthread_attr, SCHED_OTHER);
    }

    pthread_create((pthread_t *)&pTaskHandle->task, &pthread_attr, app_ipcam_task_entry, pTaskHandle);

    pthread_attr_destroy(&pthread_attr);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "PID:%d, TID:%ld, pPID: %lu\n",
        getpid(), syscall(SYS_gettid), (unsigned long)pthread_self());

    *task = pTaskHandle;

    return APP_IPCAM_SUCCESS;
}

int app_ipcam_task_destroy(APP_PARAM_TASK_HANDLE_T *task)
{
    if (*task == NULL) {
        return CVI_OSAL_ERR_FAILURE;
    }

    free(*task);
    *task = NULL;

    return APP_IPCAM_SUCCESS;
}


int app_ipcam_task_join(APP_PARAM_TASK_HANDLE_T task)
{
    int rc;
    rc = pthread_join((pthread_t)task->task, NULL);
    return rc < 0 ? CVI_OSAL_ERR_FAILURE : APP_IPCAM_SUCCESS;
}

void app_ipcam_task_sleep(int64_t time_us)
{
    usleep(time_us);
}

void app_ipcam_task_resched(void)
{
    sched_yield();
}
