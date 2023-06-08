#include "file_recover.hpp"
#include "container_factory.hpp"
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include "app_ipcam_comm.h"

namespace cvi_file_recover {

using std::string;
using std::thread;
#define TYPE_MP4 (1)
#define TYPE_MOV (0)
static bool SetPreallocFlage = true;
static int file_type = TYPE_MOV;


static inline std::string getFileExtensionName(const std::string& file_name)
{
    constexpr int min_length = 2;
    if (file_name.length() < min_length) {
        return "";
    }

    std::size_t found_pos = file_name.find_last_of('.');
    if (found_pos == std::string::npos) {
        return "";
    }

    std::string extension = file_name.substr(found_pos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [] (const unsigned char c) {
            return std::tolower(c);
        }
    );

    return extension;
}

static void syncFile(const string &file_path) {
    FILE* file = fopen(file_path.c_str(),"rb+");

    if (file) {
        int fn = fileno(file);
        if(fn != -1) {
            fsync(fn);
        }

        fclose(file);
    }
}

FileRecover::~FileRecover()
{
    join();
    close();
}

int FileRecover::open(const string &input_file_path)
{
    file_path = input_file_path;
    string file_extension = getFileExtensionName(input_file_path);
    file = ContainerFactory::createContainer(file_extension);
    if (!file) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Unsupport file format\r\n");
        return -1;
    }

    char s_file[64];
    strcpy(s_file, file_path.c_str());
    FILE *fp = fopen(s_file, "rb");
    if (NULL == fp) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"file open fail, file is (%s)\r\n", s_file);
        return 1;
    }

    unsigned char read_temp_buf[48] = {0};
    fseek(fp, 0, SEEK_SET);
    fread(read_temp_buf, sizeof(read_temp_buf), 1, fp);
    fclose(fp);
    if ((read_temp_buf[32] == 0X6D) && (read_temp_buf[33] == 0X64) && (read_temp_buf[34] == 0X61) && (read_temp_buf[35] == 0X74)) {
        file_type = TYPE_MOV;
    } else if ((read_temp_buf[44] == 0X6D) && (read_temp_buf[45] == 0X64) && (read_temp_buf[46] == 0X61) && (read_temp_buf[47] == 0X74)) {
        file_type = TYPE_MP4;
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"File no mdat\r\n");

        file.reset();
        return -1;
    }

    if (file->open(input_file_path) != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"File open failed\r\n");
        file.reset();
        return -1;
    }

    return 0;
}

void FileRecover::join()
{
    if (recover_thread.joinable()) {
        recover_thread.join();
    }
}

void FileRecover::close()
{
    if (file) {
        file->close();
        syncFile(file_path);
    }
}

int FileRecover::check() const
{
    if (!file) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"File is not opened\r\n");
        return -1;
    }

    return file->check();
}

int FileRecover::dump() const
{
    if (!file) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"File is not opened\r\n");
        return -1;
    }

    file->dump();

    return 0;
}

void FileRecover::preallocatestate(bool PreallocFlage)
{
    SetPreallocFlage = PreallocFlage;
}

int FileRecover::recover(const string &output_file_path, const string &device_model, bool has_create_time)
{
    if (!file) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"File is not opened\r\n");
        return -1;
    }
    if (is_recovering) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Recover is under progress\r\n");
        return -1;
    }
    // start file recover
    is_recovering = true;
    APP_PROF_LOG_PRINT(LEVEL_INFO,"Recover input file %s...\r\n", file->getFilePath().c_str());
    if (file->recover(device_model, has_create_time, SetPreallocFlage, file_type) != 0) {
        is_recovering = false;
        return -1;
    }
    file->save(output_file_path);
    APP_PROF_LOG_PRINT(LEVEL_INFO,"Recover done, save output file %s\r\n", output_file_path.c_str());
    is_recovering = false;

    return 0;
}

int FileRecover::recoverAsync(const std::string &output_file_path, const std::string &device_model, bool has_create_time)
{
    if (is_recovering) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Recover is under progress\r\n");
        return -1;
    }

    recover_thread = thread([output_file_path, device_model, has_create_time, this]() {
        recover(output_file_path, device_model, has_create_time);
    });

    return 0;
}

} // namespace cvi_file_recover
