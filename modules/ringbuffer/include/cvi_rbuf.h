#ifndef _CVI_RINGBUF_H_
#define _CVI_RINGBUF_H_

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CVI_ALIGN_UP(x, a) ((x + a - 1) & (~(a - 1)))
#define RINGBUF_ALIGN_SIZE 4096
#define FRAMEDATA_ALIGN_SIZE 64

typedef enum CVI_RECORD_TYPE_E {
    CVI_RECORD_TYPE_NORMAL,
    //CVI_RECORD_TYPE_TIMELAPSE,
    //CVI_RECORD_TYPE_EMR,
    //CVI_RECORD_TYPE_MEM,
    CVI_RECORD_TYPE_BUTT
} CVI_RECORD_TYPE_E;



int cvi_rbuf_create(void **rbuf, uint32_t size, const char *name);
void cvi_rbuf_destroy(void *rbuf);
void *cvi_rbuf_req_mem(void *rbuf, uint32_t size);
int cvi_rbuf_refresh_write_pos(void *rbuf, uint32_t offs);
void *cvi_rbuf_read_data(void *rbuf, CVI_RECORD_TYPE_E type);
int cvi_rbuf_refresh_read_pos(void *rbuf, uint32_t offs, CVI_RECORD_TYPE_E type);
void cvi_rbuf_write_rollback(void *rbuf, void *pos, uint32_t offs);
uint64_t cvi_rbuf_get_data_cnt(void *rbuf);
uint32_t cvi_rbuf_get_remain_size(void *rbuf);
void cvi_rbuf_show_meminfo(void *rbuf);


int cvi_rbuf_init(void **rbuf, int size, const char *name);
void cvi_rbuf_deinit(void *rbuf);
unsigned int cvi_rbuf_data_cnt(void *rbuf, int type);
int cvi_rbuf_copy_in(void *rbuf, void *src, int len, int off);
int cvi_rbuf_copy_out(void *rbuf, void *dst, int len, int off, int type);
unsigned int cvi_rbuf_unused(void *rbuf);
void cvi_rbuf_refresh_in(void *rbuf, int off);
void cvi_rbuf_refresh_out(void *rbuf, int off, int type);
void cvi_rbuf_show_log(void *rbuf);
int cvi_rbuf_get_totalsize(void *rbuf);
int cvi_rbuf_reset(void *rbuf);
unsigned long long cvi_rbuf_get_in_size(void *rbuf);


#ifdef __cplusplus
}
#endif


#endif
