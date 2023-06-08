#pragma once

#include <string>
#include "file.hpp"

namespace cvi_file_recover {

class Container
{
public:
    virtual ~Container()
    {
        close();
    }

    virtual int open(const std::string& file_path,std::ios_base::openmode mode = std::fstream::in | std::fstream::out | std::fstream::binary)
    {
        close();
        this->file_path = file_path;
        return file.open(file_path, mode);
    }

    virtual void close()
    {
        parsed = false;
        file_path = "";
        file.close();
    }

    virtual int parse()
    {
        parsed = true;
        file.clear();
        return 0;
    }

    virtual int check()
    {
        return 0;
    };

    virtual void dump()
    {};

    virtual int recover(const std::string &device_model, bool has_create_time, bool PreallocFlage, int file_type) = 0;
    virtual void save(const std::string &file_path) = 0;

    std::string getFilePath() const
    {
        return file_path;
    }

    bool isParsed() const
    {
        return parsed;
    }

protected:
    std::string file_path{""};
    bool parsed{false};
    File file;
};

} // namespace cvi_file_recover
