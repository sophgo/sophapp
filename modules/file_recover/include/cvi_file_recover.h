#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* CVI_FILE_RECOVER_HANDLE;

int CVI_FILE_RECOVER_Create(CVI_FILE_RECOVER_HANDLE *handle);
int CVI_FILE_RECOVER_Destroy(CVI_FILE_RECOVER_HANDLE *handle);
int CVI_FILE_RECOVER_Open(CVI_FILE_RECOVER_HANDLE handle, const char* input_file_path);
int CVI_FILE_RECOVER_Check(CVI_FILE_RECOVER_HANDLE handle);
int CVI_FILE_RECOVER_Dump(CVI_FILE_RECOVER_HANDLE handle);
int CVI_FILE_RECOVER_Recover(CVI_FILE_RECOVER_HANDLE handle, const char* output_file_path, const char* device_model, bool has_create_time);
int CVI_FILE_RECOVER_RecoverAsync(CVI_FILE_RECOVER_HANDLE handle, const char* output_file_path, const char* device_model, bool has_create_time);
int CVI_FILE_RECOVER_RecoverJoin(CVI_FILE_RECOVER_HANDLE handle);
int CVI_FILE_RECOVER_Close(CVI_FILE_RECOVER_HANDLE handle);
void CVI_FILE_RECOVER_PreallOcateState(CVI_FILE_RECOVER_HANDLE handle, bool PreallocFlage);

#ifdef __cplusplus
}
#endif
