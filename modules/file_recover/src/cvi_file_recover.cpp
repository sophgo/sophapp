#include "cvi_file_recover.h"
#include "file_recover.hpp"
#include "app_ipcam_comm.h"

using namespace cvi_file_recover;

#define FILE_RECOVERY_MAJOR_VERSION 1
#define FILE_RECOVERY_MINOR_VERSION 4


int CVI_FILE_RECOVER_Create(CVI_FILE_RECOVER_HANDLE *handle)
{
    if (!hasNullptr(*handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle is not null\r\n");
        return -1;
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO,"CVI File Recovery version : %d.%d\r\n",FILE_RECOVERY_MAJOR_VERSION, FILE_RECOVERY_MINOR_VERSION);
    *handle = new FileRecover();

    return 0;
}

int CVI_FILE_RECOVER_Destroy(CVI_FILE_RECOVER_HANDLE *handle)
{
    if (cvi_file_recover::hasNullptr(handle, *handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(*handle);
    delete file_recover;
    *handle = nullptr;

    return 0;
}

int CVI_FILE_RECOVER_Open(CVI_FILE_RECOVER_HANDLE handle, const char* input_file_path)
{
    if (cvi_file_recover::hasNullptr(handle, input_file_path)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle or file path is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    return file_recover->open(input_file_path);
}

int CVI_FILE_RECOVER_Check(CVI_FILE_RECOVER_HANDLE handle)
{
    if (cvi_file_recover::hasNullptr(handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover *>(handle);
    return file_recover->check();
}

int CVI_FILE_RECOVER_Dump(CVI_FILE_RECOVER_HANDLE handle)
{
    if (cvi_file_recover::hasNullptr(handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    return file_recover->dump();
}

void CVI_FILE_RECOVER_PreallOcateState(CVI_FILE_RECOVER_HANDLE handle, bool PreallocFlage)
{
    if (cvi_file_recover::hasNullptr(handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle or output file PreallocFlage is null\r\n");
        return;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    file_recover->preallocatestate(PreallocFlage);
}

int CVI_FILE_RECOVER_Recover(CVI_FILE_RECOVER_HANDLE handle, const char* output_file_path, const char* device_model, bool has_create_time)
{
    if (cvi_file_recover::hasNullptr(handle, output_file_path)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle or output file path is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    return file_recover->recover(output_file_path, device_model, has_create_time);
}

int CVI_FILE_RECOVER_RecoverAsync(CVI_FILE_RECOVER_HANDLE handle, const char* output_file_path, const char* device_model, bool has_create_time)
{
    if (cvi_file_recover::hasNullptr(handle, output_file_path)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle or output file path is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    return file_recover->recoverAsync(output_file_path, device_model, has_create_time);
}

int CVI_FILE_RECOVER_RecoverJoin(CVI_FILE_RECOVER_HANDLE handle)
{
    if (cvi_file_recover::hasNullptr(handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    file_recover->join();

    return 0;
}

int CVI_FILE_RECOVER_Close(CVI_FILE_RECOVER_HANDLE handle)
{
    if (cvi_file_recover::hasNullptr(handle)) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Handle is null\r\n");
        return -1;
    }

    FileRecover *file_recover = static_cast<FileRecover*>(handle);
    file_recover->close();

    return 0;
}