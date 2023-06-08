#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include "utils_binary.hpp"
#include "app_ipcam_comm.h"

namespace cvi_file_recover {

class File
{
public:
    ~File()
    {
        close();
    }

    bool isOpened() const
    {
        return file.is_open();
    }

    int open(const std::string& file_path,
             std::ios_base::openmode mode = std::fstream::in | std::fstream::out | std::fstream::binary)
    {
        file.open(file_path, mode);
        if (!isOpened()) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"Open file %s failed", file_path.c_str());
            file.close();
            return -1;
        }

        return 0;
    }

    int64_t size()
    {
        off_t current_pos = pos();
        if (current_pos < 0) {
            return -1;
        }

        file.seekg(0, std::ios::end);
        off_t size = pos();
        file.seekg(current_pos, std::ios::beg);

        return size;
    }

    int64_t pos()
    {
        return file.tellg();
    }

    bool eof()
    {
        return file.eof() || (pos() == size());
    }

    void clear()
    {
        file.clear();
    }

    void seek(int64_t pos)
    {
        file.seekg(pos, std::ios::beg);
    }

    void offset(int64_t offset)
    {
        file.seekg(offset, std::ios::cur);
    }

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    T read()
    {
        assert(isOpened());

        T value;
        const uint8_t *value_ptr = reinterpret_cast<const uint8_t*>(&value);
        file.read(reinterpret_cast<char*>(&value), sizeof(T));
        return utils::read<T>(value_ptr);
    }

    std::vector<unsigned char> read(size_t size)
    {
        assert(isOpened());

        std::vector<unsigned char> value(size);
        file.read(reinterpret_cast<char*>(&value[0]), size);
        size_t read_length = file.gcount();
        if (read_length != size) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"read() Could not read size %zd, real read %zd", size, read_length);
            value.resize(read_length);
        }

        return value;
    }

    std::string readString(size_t size)
    {
        assert(isOpened());

        char value[size + 1];
        file.read(value, size);
        size_t read_length = file.gcount();
        if (read_length != size) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"readString() Could not read size %zd, real read %zd", size, read_length);
        }
        value[size] = '\0';

        return std::string(value);
    }

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    void write(const T &value)
    {
        assert(isOpened());

        uint8_t value_array[sizeof(T)];
        utils::write<T>(value_array, value);
        file.write(reinterpret_cast<char*>(value_array), sizeof(T));
    }

    template <typename T>
    void write(std::vector<T> &value)
    {
        assert(isOpened());

        file.write(reinterpret_cast<char*>(&value[0]), sizeof(T) * value.size());
    }

    void writeString(std::string &value)
    {
        assert(isOpened());

        file << value;
    }

    void close()
    {
        if (file.is_open()) {
            file.close();
        }
    }

private:
    std::fstream file;
};

} // namespace cvi_file_recover
