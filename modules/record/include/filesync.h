
#ifndef REC_FILE_SYNC_H
#define REC_FILE_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif
void sync_push(char *filename);
int sync_init(void);
int sync_deinit(void);


#ifdef __cplusplus
}
#endif
#endif