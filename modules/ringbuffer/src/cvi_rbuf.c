
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app_ipcam_comm.h"
#include "cvi_rbuf.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct CVI_RBUF_S{
    unsigned long addr;
    uint32_t size;
    char name[64];
    unsigned long rpos;
    uint64_t rcnt;
    unsigned long wpos;
    uint64_t wcnt;
}CVI_RBUF_T;


void cvi_rbuf_destroy(void *rbuf)
{
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL)
    {
        return;
    }

    if(buf->addr) {
         unsigned char * _tmp_point = (unsigned char *)buf->addr;
        free(_tmp_point);
        //cvi_mem_free((void *)buf->addr);
    }

    free((void *)buf);
}

int cvi_rbuf_create(void **rbuf, uint32_t size, const char *name)
{
    CVI_RBUF_T *buf = (CVI_RBUF_T *)malloc(sizeof(CVI_RBUF_T));
    unsigned char * _tmp_point = NULL;
    if(buf == NULL) {
        *rbuf = NULL;
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"out of mem size %d name %s", size, name);
        return -1;
    }

    memset(buf, 0x0, sizeof(CVI_RBUF_T));
    int s = CVI_ALIGN_UP(size, RINGBUF_ALIGN_SIZE);
    //buf->addr = (unsigned long)cvi_mem_allocate(s, name);
    _tmp_point = (unsigned char *)malloc(s);
    buf->addr =  (unsigned long)_tmp_point;
    if(!buf->addr) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"rbuf create failed! the args size %d name %s", size, name);
        cvi_rbuf_destroy((void *)buf);
    }
    buf->size = s;
    buf->wpos = buf->addr;
    buf->rpos = buf->addr;
    strncpy(buf->name, name, sizeof(buf->name) - 1);
    *rbuf = buf;
    return 0;
}

uint32_t cvi_rbuf_get_remain_size(void *rbuf)
{
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return 0;
    }

    if(buf->wcnt != buf->rcnt && buf->wpos == buf->rpos){
        return 0;
    }

    if(buf->wpos == buf->rpos) {
        return buf->size;
    }

    if(buf->wpos > buf->rpos) {
        return buf->size - (buf->wpos - buf->rpos);
    } else {
        return buf->rpos - buf->wpos;
    }
}

void *cvi_rbuf_req_mem(void *rbuf, uint32_t size)
{
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return NULL;
    }

    void *mem = NULL;
    if(buf->wcnt == 0 && size <= buf->size) {
        buf->wcnt++;
        mem = (void *)buf->addr;
        buf->wpos = buf->addr + size;
    }else {
        if(buf->wpos > buf->rpos) {
            if(buf->size + buf->addr - buf->wpos >= size) {
                buf->wcnt++;
                mem = (void *)buf->wpos;
                buf->wpos += size;
            } else {
                ((char *)buf->wpos)[0] = 0x00;
                if(buf->rpos > size) {
                    buf->wcnt++;
                    mem = (void *)buf->addr;
                    buf->wpos = buf->addr + size;
                }
            }
        } else if(buf->wpos < buf->rpos) {
            if(buf->rpos - buf->wpos > size) {
                buf->wcnt++;
                mem = (void *)buf->wpos;
                buf->wpos += size;
            }
        } else {

        }
    }

    return mem;
}


int cvi_rbuf_refresh_write_pos(void *rbuf, uint32_t offs) {
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return -1;
    }
    (void)offs;
    return 0;
}


void *cvi_rbuf_read_data(void *rbuf, CVI_RECORD_TYPE_E type) {
    (void)type;
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return NULL;
    }

    if(buf->wcnt == buf->rcnt) {
        return NULL;
    }

    void *mem = NULL;
    if(((unsigned char *)buf->rpos)[0] != 0x5a) {
        buf->rpos = buf->addr;
        mem = (void *)buf->addr;
    } else {
        mem = (void *)buf->rpos;
    }

    return mem;
}

int cvi_rbuf_refresh_read_pos(void *rbuf, uint32_t offs, CVI_RECORD_TYPE_E type) {
    (void)type;
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return -1;
    }

    if(buf->rpos + offs > buf->addr + buf->size) {
        buf->rpos = buf->addr + offs;
    } else {
        buf->rpos += offs;
    }
    buf->rcnt++;

    return 0;
}

uint64_t cvi_rbuf_get_data_cnt(void *rbuf) {
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return 0;
    }
    return buf->wcnt - buf->rcnt;
}

void cvi_rbuf_show_meminfo(void *rbuf) {
    CVI_RBUF_T *buf = (CVI_RBUF_T *)rbuf;
    if(buf == NULL || buf->addr == 0) {
        return;
    }
    uint32_t rmsize = cvi_rbuf_get_remain_size(rbuf);
    APP_PROF_LOG_PRINT(LEVEL_DEBUG,"%p wcnt %lu, rcnt %lu, wpos %#lx, rpos %#lx, bsize %u, rsize %u", rbuf, buf->wcnt, buf->rcnt, buf->wpos, buf->rpos, buf->size, rmsize);
}


typedef struct CVI_RBUF_INFO_S{
    int size;
    unsigned long long in;
    unsigned long long inCnt;
    unsigned long long out[2];
    unsigned long long outCnt[2];
    void *data;
    char name[64];
}CVI_RBUF_INFO_T;

void cvi_rbuf_show_log(void *rbuf){
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf){
        APP_PROF_LOG_PRINT(LEVEL_DEBUG,"%p %08d %016llu %08llu %016llu %08llu %016llu %08llu", rbuf, buf->size,
            buf->in, buf->inCnt, buf->out[0], buf->outCnt[0], buf->out[1], buf->outCnt[1]);
    }
}

int cvi_rbuf_reset(void *rbuf){
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        buf->in = 0;
        buf->inCnt = 0;
        buf->out[0] = 0;
        buf->outCnt[0] = 0;
        buf->out[1] = 0;
        buf->outCnt[1] = 0;
    }
    return 0;
}

int cvi_rbuf_init(void **rbuf, int size, const char *name) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)malloc(sizeof(CVI_RBUF_INFO_T));
    if(buf == NULL) {
        *rbuf = NULL;
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"out of mem size %d name %s", size, name);
        return -1;
    }

    memset(buf, 0x0, sizeof(CVI_RBUF_INFO_T));
    int s = CVI_ALIGN_UP(size, RINGBUF_ALIGN_SIZE);
    buf->data = (void *)malloc(s);
    //buf->data = cvi_mem_allocate(s, name);
    if(!buf->data) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"rbuf create failed! the args size %d name %s", size, name);
        cvi_rbuf_deinit((void *)buf);
        return -1;
    }
    strncpy(buf->name, name, sizeof(buf->name) - 1);
    cvi_rbuf_reset((void *)buf);
    buf->size = s;
    *rbuf = buf;
    APP_PROF_LOG_PRINT(LEVEL_DEBUG,"rbuf %s is created, size %d", buf->name, buf->size);
    return 0;
}

void cvi_rbuf_deinit(void *rbuf) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        if(buf->data) {
            free(buf->data);
            //cvi_mem_free(buf->data);
        }
        APP_PROF_LOG_PRINT(LEVEL_DEBUG,"rbuf %s is free, size %d", buf->name, buf->size);
        free(buf);
    }
}

unsigned int cvi_rbuf_unused(void *rbuf) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        unsigned long long out0 = buf->out[0];
        unsigned long long out1 = buf->out[1];
        return buf->size - (buf->in - ((out0 < out1) ? out0 : out1));
    }
    return 0;
}

int cvi_rbuf_get_totalsize(void *rbuf){
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        return buf->size;
    }
    return 0;
}

unsigned int cvi_rbuf_data_cnt(void *rbuf, int type) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        return buf->inCnt - buf->outCnt[type];
    }
    return 0;
}

unsigned long long cvi_rbuf_get_in_size(void *rbuf) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        return buf->in;
    }
    return 0;
}

int cvi_rbuf_copy_in(void *rbuf, void *src, int len, int off) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf && src) {
        if(cvi_rbuf_unused(rbuf) < (unsigned int)len) {
            return -1;
        }
        unsigned int ll = (buf->in + off) % buf->size;
        int l = ((buf->size - ll) < (unsigned int)len) ? (buf->size - ll) : (unsigned int)len;
        memcpy((char *)buf->data + ll, src, l);
        memcpy(buf->data, (char *)src + l, len - l);
        return 0;
    }
    return -1;
}

void cvi_rbuf_refresh_in(void *rbuf, int off) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        buf->inCnt++;
        buf->in += off;
    }
}

void cvi_rbuf_refresh_out(void *rbuf, int off, int type) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf) {
        buf->outCnt[type]++;
        buf->out[type] += off;
    }
}


int cvi_rbuf_copy_out(void *rbuf, void *dst, int len, int off, int type) {
    CVI_RBUF_INFO_T *buf = (CVI_RBUF_INFO_T *)rbuf;
    if(buf && dst) {
        if(buf->in - buf->out[type] - off < (unsigned int)len) {
            //APP_PROF_LOG_PRINT(LEVEL_ERROR,"%llu %llu %d %d %d", buf->in, buf->out[type], off, len, type);
            return -1;
        }
        unsigned int ll = (buf->out[type] + off) % buf->size;
        int l = ((buf->size - ll) < (unsigned int)len) ? (buf->size - ll) : (unsigned int)len;
        memcpy(dst, (char *)buf->data + ll, l);
        memcpy((char *)dst + l, buf->data, len - l);
        return 0;
    }
    APP_PROF_LOG_PRINT(LEVEL_ERROR,"invalid argument %p %p", buf, dst);
    return -1;
}


#ifdef __cplusplus
}
#endif

