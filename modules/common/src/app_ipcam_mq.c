#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "app_ipcam_comm.h"
#include "app_ipcam_mq.h"

struct CVI_MQ_ENDPOINT_s {
    CVI_MQ_ENDPOINT_CONFIG_t       config;

    APP_PARAM_MUTEX_HANDLE_T        lock;
    APP_PARAM_TASK_HANDLE_T         recv_task;
    volatile bool                  exit;

    int                            fd_max;
    int                            socket_fd;
    fd_set                         fds;
};

#define CVI_MQ_SOCKET_PATH         "/tmp/CVI_MQ"
#define CVI_MQ_SOCKET_CLIENT_PATH         "/tmp/CVI_MQCLIENT"

static int app_ipcam_MqSocket_Create(CVI_MQ_ENDPOINT_HANDLE_t pEpHdl)
{
    struct sockaddr_un un;
    int socket_fd, addr_len, ret;

    socket_fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        printf("[MQ] create socket fail, ret = %d, errno = %d\n", socket_fd, errno);
        return CVI_MQ_ERR_FAILURE;
    }
    //int recvBuff = CVI_MQ_QUEUE_SIZE * sizeof(struct CVI_MQ_MSG_s);
    //setsockopt(socket_fd , SOL_SOCKET, SO_RCVBUF, &recvBuff, sizeof(int));
    pEpHdl->socket_fd = socket_fd;

    memset(&un, 0, sizeof(un));
    un.sun_family = AF_LOCAL;
    sprintf(un.sun_path, CVI_MQ_SOCKET_PATH"%08x", pEpHdl->config.id);
    addr_len = sizeof(un);
    unlink(un.sun_path);
    ret = bind(socket_fd, (struct sockaddr *)&un, addr_len);
    if (ret < 0) {
        printf("[MQ] bind socket fail, ret = %d, errno = %d, addr = %s",
                ret, errno, un.sun_path);
        close(socket_fd);
        return CVI_MQ_ERR_FAILURE;
    }
    printf("[MQ] Socket bind to %s\n", un.sun_path);

    FD_SET(socket_fd, &pEpHdl->fds);
    if (socket_fd > pEpHdl->fd_max) {
        pEpHdl->fd_max = socket_fd;
    }
    printf("[MQ] listen local socket: %d\n", socket_fd);

    return CVI_MQ_SUCCESS;
}

static int app_ipcam_MqSocket_Destroy(CVI_MQ_ENDPOINT_HANDLE_t pEpHdl)
{
    FD_CLR(pEpHdl->socket_fd, &pEpHdl->fds);
    close(pEpHdl->socket_fd);

    return CVI_MQ_SUCCESS;
}

static void *mq_recv_worker(void *arg)
{
    CVI_MQ_ENDPOINT_HANDLE_t pEpHdl = arg;

    // for debug purpose
    prctl(PR_SET_NAME, (unsigned long int)"MQ_Recv_Task", 0, 0, 0);

    printf("[MQ] Recv Task: start id=0x%x\n", pEpHdl->config.id);

    while (pEpHdl->exit != true) {
        #define MQ_LOOP_TIMEOUT_MS      200
        struct timeval timeout;
        fd_set fds;
        int ret;
        char *dptr;
        int16_t client_id;
        CVI_MQ_MSG_t msg;
        size_t buf_len = sizeof(CVI_MQ_MSG_t);
        ssize_t recv_len = 0;
        struct sockaddr_un remoteClient;
        socklen_t addr_len = sizeof(remoteClient);
        timeout.tv_sec  = MQ_LOOP_TIMEOUT_MS / 1000;
        timeout.tv_usec = MQ_LOOP_TIMEOUT_MS * 1000;

        memcpy(&fds, &pEpHdl->fds, sizeof(fd_set));
        ret = TEMP_FAILURE_RETRY(select(pEpHdl->fd_max + 1, &fds, NULL, NULL, &timeout));
        if (ret > 0) {
            //printf("[MQ] Recv Task: got input on socket %d\n", pEpHdl->socket_fd);
            //recv_len = TEMP_FAILURE_RETRY(recvfrom(pEpHdl->socket_fd,
            //        (void *)&msg, buf_len, MSG_WAITALL, NULL, NULL));
            memset(&msg, 0, sizeof(CVI_MQ_MSG_t));
            recv_len = recvfrom(pEpHdl->socket_fd, (void *)&msg, buf_len, MSG_WAITALL, (struct sockaddr *)&remoteClient, &addr_len);
            printf("[MQ] RX %ld bytes\n", recv_len);
            //CVI_LOGI_MEM(&msg, recv_len, "RX MSG");
            if(msg.needack) {
                dptr = &remoteClient.sun_path[17];
                client_id = atoi(dptr);
                msg.client_id = client_id;
            }
            
            ret = pEpHdl->config.recv_cb(pEpHdl, &msg, pEpHdl->config.recv_cb_arg);
            if (ret != CVI_MQ_SUCCESS) {
                printf("[MQ] recv cb return err %d\n", ret);
                break;
            }
        } else if (ret < 0) {
            printf("[MQ[ Select fail!\n");
        } else {
            app_ipcam_task_resched();
        }
    }

    printf("[MQ] Recv Task: exit id=0x%x\n", pEpHdl->config.id);

    return NULL;
}

int app_ipcam_MqEndpoint_Create(
    CVI_MQ_ENDPOINT_CONFIG_t      *config,
    CVI_MQ_ENDPOINT_HANDLE_t      *pEpHdl)
{
    CVI_MQ_ENDPOINT_HANDLE_t pEpHandle;
    APP_PARAM_MUTEX_ATTR_T m_attr;
    APP_PARAM_TASK_ATTR_T t_attr;
    int rc;

    printf("[MQ] create pEpHdl id=0x%x\n", config->id);

    // Allocate route table together with client object
    pEpHandle = calloc(1, sizeof(CVI_MQ_ENDPOINT_t));
    if (pEpHandle == NULL) {
        printf("[MQ]: create pEpHdl alloc fail, errno = %d\n", errno);
        return CVI_MQ_ERR_NOMEM;
    }
    memcpy(&pEpHandle->config, config, sizeof(CVI_MQ_ENDPOINT_CONFIG_t));

    // Create mutex
    m_attr.name = "MQ_Mutex";
    rc = app_ipcam_mutex_create(&m_attr, &pEpHandle->lock);
    if (rc != CVI_OSAL_SUCCESS) {
        printf("[MQ]: create pEpHdl create mutex fail, rc = %d\n", rc);
        rc = CVI_MQ_ERR_FAILURE;
        goto err_mutex;
    }

    // create socket
    rc = app_ipcam_MqSocket_Create(pEpHandle);
    if (rc != CVI_OSAL_SUCCESS) {
        printf("[MQ]: create pEpHdl create socket fail, rc = %d\n", rc);
        rc = CVI_MQ_ERR_FAILURE;
        goto err_socket;
    }

    // Create recv task
    if (config->recv_cb) {
        pEpHandle->exit = false;
        t_attr.name = "MQ Task";
        t_attr.entry = mq_recv_worker;
        t_attr.param = pEpHandle;
        t_attr.priority = CVI_OSAL_PRI_NORMAL;
        t_attr.detached = false;
        rc = app_ipcam_task_create(&t_attr, &pEpHandle->recv_task);
        if (rc != CVI_OSAL_SUCCESS) {
            printf("[MQ]: create pEpHdl create task fail, rc = %d\n", rc);
            rc = CVI_MQ_ERR_FAILURE;
            goto err_task;
        }
    } else {
        printf("[MQ] pEpHdl id=0x%x for TX only\n", config->id);
    }

    *pEpHdl= pEpHandle;
    return rc;

err_task:
    app_ipcam_MqSocket_Destroy(pEpHandle);
err_socket:
    app_ipcam_mutex_destroy(pEpHandle->lock);
err_mutex:
    free(pEpHandle);

    return rc;
}

int app_ipcam_MqEndpoint_Destroy(
    CVI_MQ_ENDPOINT_HANDLE_t       pEpHdl)
{
    app_ipcam_mutex_lock(pEpHdl->lock, CVI_OSAL_WAIT_FOREVER);
    pEpHdl->exit = true;
    app_ipcam_task_join(pEpHdl->recv_task);
    app_ipcam_task_destroy(&pEpHdl->recv_task);
    app_ipcam_MqSocket_Destroy(pEpHdl);
    app_ipcam_mutex_destroy(pEpHdl->lock);
    free(pEpHdl);
    return CVI_MQ_SUCCESS;
}

int CVI_MQ_Send_RAW(
    CVI_MQ_MSG_t                  *msg)
{
    struct sockaddr_un un;
    int addr_len, ret, rc = CVI_MQ_SUCCESS;
    int socket_fd;

    /* Create local socket (no connection mode) */
    socket_fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        printf("[MQ] create socket fail, ret = %d, errno = %d\n",
                socket_fd, errno);
        return CVI_MQ_ERR_FAILURE;
    }
    //int sendBuff = CVI_MQ_QUEUE_SIZE * sizeof(struct CVI_MQ_MSG_s);
    //setsockopt(socket_fd , SOL_SOCKET, SO_SNDBUF, &sendBuff, sizeof(int));

    memset(&un, 0, sizeof(un));
    un.sun_family = AF_LOCAL;
    sprintf(un.sun_path, CVI_MQ_SOCKET_PATH"%08x", msg->target_id);
    addr_len = sizeof(un);

    //CVI_LOGI_MEM(msg, msg->len, "TX MSG");
    printf("[MQ] TX %d bytes, to 0x%x\n", msg->len, msg->target_id);
    ret = sendto(socket_fd, (void*)msg, msg->len,
            MSG_DONTWAIT, (struct sockaddr *)&un, addr_len);
    if (ret < 0) {
        printf("[MQ] Send message fail, ret = %d, errno = %d, addr = %s",
                ret, errno, un.sun_path);
        if (errno == EAGAIN) {
            rc = CVI_MQ_ERR_AGAIN;
        } else {
            rc = CVI_MQ_ERR_FAILURE;
        }
    }
    close(socket_fd);

    return rc;
}

int CVI_MQ_Send(
    CVI_MQ_ID_t                    target_id,
    int32_t                        arg1,
    int32_t                        arg2,
    int16_t                        seq_no,
    char                          *payload,
    int16_t                        payload_len)
{
    CVI_MQ_MSG_t msg;

    if (payload_len > CVI_MQ_MSG_PAYLOAD_LEN) {
        return CVI_MQ_ERR_FAILURE;
    }
    msg.target_id = target_id;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg.seq_no = seq_no;
    msg.len = CVI_MQ_MSG_HEADER_LEN + payload_len;
    msg.needack = 0;
    uint64_t boot_time;
    app_ipcam_boot_time_get(&boot_time);
    msg.crete_time = boot_time;
    memcpy(msg.payload, payload, payload_len);

    return CVI_MQ_Send_RAW(&msg);
}

int CVI_MQ_Send_Ack(CVI_MQ_ENDPOINT_HANDLE_t pEpHdl,
    MSG_ACK_t   *ack_msg, int16_t client_id) {
    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_LOCAL;
    sprintf(un.sun_path, CVI_MQ_SOCKET_CLIENT_PATH"%d", client_id);

    int addr_len = sizeof(struct sockaddr_un);
    int ret = sendto(pEpHdl->socket_fd, (void*)ack_msg, sizeof(MSG_ACK_t), 0, (struct sockaddr*) &un, addr_len);
    if (ret < 0) {
        printf("[MQ] Send Ack fail, ret = %d, errno = %d",
                ret, errno);
        if (errno == EAGAIN) {
            ret = CVI_MQ_ERR_AGAIN;
        } else {
            ret = CVI_MQ_ERR_FAILURE;
        }
    } else if (ret == sizeof(MSG_ACK_t)) {
        ret = 0;
    }
    return ret;
}

int CVI_MQ_Send_RAW_ACK(
    CVI_MQ_MSG_t                  *msg,
    MSG_ACK_t                     *msg_ack,
    int16_t                        client_id,
    int16_t                       recv_timeout)
{
    return CVI_MQ_SUCCESS;

    struct sockaddr_un un,un_self;
    int addr_len, ret, rc = CVI_MQ_SUCCESS;
    int socket_fd;
    static int max_fd;
    /* Create local socket (no connection mode) */
    socket_fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        printf("[MQ] create socket fail, ret = %d, errno = %d\n",
                socket_fd, errno);
        return CVI_MQ_ERR_FAILURE;
    }
    if(socket_fd > max_fd) {
        max_fd = socket_fd;
    }
    //int sendBuff = CVI_MQ_QUEUE_SIZE * sizeof(struct CVI_MQ_MSG_s);
    //setsockopt(socket_fd , SOL_SOCKET, SO_SNDBUF, &sendBuff, sizeof(int));
    memset(&un_self, 0, sizeof(un_self));
    un_self.sun_family = AF_LOCAL;
    sprintf(un_self.sun_path, CVI_MQ_SOCKET_CLIENT_PATH"%d", client_id);
    addr_len = sizeof(un_self);
    unlink(un_self.sun_path);
    ret = bind(socket_fd, (struct sockaddr *)&un_self, addr_len);
    if(ret < 0) {
        printf("[MQ] bind fail, ret = %d, errno = %d, addr = %s",
                ret, errno, un.sun_path);
    }
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_LOCAL;
    sprintf(un.sun_path, CVI_MQ_SOCKET_PATH"%08x", msg->target_id);
    addr_len = sizeof(un);


    //CVI_LOGI_MEM(msg, msg->len, "TX MSG");
    printf("[MQ] TX %d bytes, to 0x%x\n", msg->len, msg->target_id);
    ret = sendto(socket_fd, (void*)msg, msg->len, 0, (struct sockaddr *)&un, addr_len);
    if (ret < 0) {
        printf("[MQ] Send message fail, ret = %d, errno = %d, addr = %s",
                ret, errno, un.sun_path);
        if (errno == EAGAIN) {
            rc = CVI_MQ_ERR_AGAIN;
        } else {
            rc = CVI_MQ_ERR_FAILURE;
        }
    }
    fd_set fds;
    struct timeval timeout;
    timeout.tv_sec  = recv_timeout / 1000;
    timeout.tv_usec = (recv_timeout - timeout.tv_sec * 1000 )* 1000;
    int16_t recv_len;
    FD_ZERO(&fds);
    FD_SET(socket_fd, &fds);
    printf("tv_sec = %ld tv_usec = %ld\n",timeout.tv_sec, timeout.tv_usec);
    ret = TEMP_FAILURE_RETRY(select(max_fd + 1, &fds, NULL, NULL, &timeout));
    if(ret > 0) {
        recv_len = recv(socket_fd, (void *)msg_ack, sizeof(MSG_ACK_t), MSG_WAITALL);
        printf("[MQ] Ack %d bytes\n", recv_len);
    } else if (ret < 0) {
        printf("[MQ[ Select fail! ret = %d errno = %d\n", ret, errno);
        msg_ack->status = ret;
    } else {
        msg_ack->status = -110;
        printf("[MQ[ Ack Timeout !\n");
    }

    close(socket_fd);
    return rc;

}


int CVI_MQ_Send_NeedAck(
    CVI_MQ_ID_t                    target_id,
    int32_t                        arg1,
    int32_t                        arg2,
    int16_t                        seq_no,
    char                          *payload,
    int16_t                        payload_len,
    MSG_ACK_t                      *ack_msg,
    int16_t                         client_id,
    int32_t                         timeout_ms)
{
    CVI_MQ_MSG_t msg;

    if (payload_len > CVI_MQ_MSG_PAYLOAD_LEN) {
        return CVI_MQ_ERR_FAILURE;
    }
    msg.target_id = target_id;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg.seq_no = seq_no;
    msg.len = CVI_MQ_MSG_HEADER_LEN + payload_len;
    msg.needack = 1;
    uint64_t boot_time;
    app_ipcam_boot_time_get(&boot_time);
    msg.crete_time = boot_time;
    memcpy(msg.payload, payload, payload_len);

    return CVI_MQ_Send_RAW_ACK(&msg, ack_msg, client_id, timeout_ms);
}
