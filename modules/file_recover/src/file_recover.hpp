#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <memory>
#include "container.hpp"

namespace cvi_file_recover {

template <typename T>
inline bool hasNullptr(T ptr)
{
    return (ptr == nullptr);
}

template <typename First, typename... Rest>
inline bool hasNullptr(First first, Rest... rest)
{
    return hasNullptr(first) || hasNullptr(rest...);
}

class FileRecover final
{
public:
    ~FileRecover();

    int open(const std::string &input_file_path);
    void join();
    void close();
    int check() const;
    int dump() const;
    int recover(const std::string &output_file_path, const std::string &device_model, bool has_create_time);
    int recoverAsync(const std::string &output_file_path, const std::string &device_model, bool has_create_time);
    void preallocatestate(bool PreallocFlage);

private:
    std::unique_ptr<Container> file;
    std::atomic<bool> is_recovering{false};
    std::thread recover_thread;
    std::string file_path;
};

} // namespace cvi_file_recover
