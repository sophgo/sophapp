#pragma once

#include <cstdint>

namespace cvi_file_recover {

struct Info
{
    uint32_t width{0};
    uint32_t height{0};
    float fps{0};
};

enum class Codec
{
    Unknown,
    H264,
    H265
};

} // namespace cvi_file_recover
