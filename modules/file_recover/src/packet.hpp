#pragma once

#include <cstdint>
#include <cstdio>
#include "types.hpp"


namespace cvi_file_recover {

constexpr int NAL_HEADER_START_POS = 4;
constexpr int H264_NAL_HEADER_LENGTH = 1;
constexpr int H265_NAL_HEADER_LENGTH = 2;
constexpr int JPEG_SOI_LENGTH = 2;
constexpr int JPEG_EOI_LENGTH = 2;
constexpr uint8_t JPEG_SOI[JPEG_SOI_LENGTH] = {0xFF, 0xD8};
constexpr uint8_t JPEG_EOI[JPEG_EOI_LENGTH] = {0xFF, 0xD9};
constexpr uint8_t SUBTITLE_SOI = 0;

inline bool isVPSNalu(Codec codec, uint8_t nal_type)
{
    if (codec == Codec::H265) {
        return (nal_type == 32);
    }

    return false;
}

inline bool isSPSNalu(Codec codec, uint8_t nal_type)
{
    if (codec == Codec::H264) {
        return (nal_type == 7);
    } else if (codec == Codec::H265) {
        return (nal_type == 33);
    }

    return false;
}

inline bool isPPSNalu(Codec codec, uint8_t nal_type)
{
    if (codec == Codec::H264) {
        return (nal_type == 8);
    } else if (codec == Codec::H265) {
        return (nal_type == 34);
    }

    return false;
}

inline bool isParameterSetNalu(Codec codec, uint8_t nal_type)
{
    return isVPSNalu(codec, nal_type) || isSPSNalu(codec, nal_type) || isPPSNalu(codec, nal_type);
}

inline bool isFrameNalu(Codec codec, uint8_t nal_type)
{
    // is slice or IDR
    if (codec == Codec::H264) {
        return (nal_type == 1) || (nal_type == 5);
    } else if (codec == Codec::H265) {
        return (nal_type == 1) || (nal_type == 19) || (nal_type == 20) || (nal_type == 21);
    }

    return false;
}

inline uint8_t getNalType(Codec codec, uint8_t data)
{
    if (codec == Codec::H264) {
        return (data & 0x1F);
    } else if (codec == Codec::H265) {
        return (data & 0x7E) >> 1;
    }

    return 0;
}

inline bool isNriValid(Codec codec, uint8_t nal_type, uint8_t nri)
{
    if (!isFrameNalu(codec, nal_type)) {
        return true;
    }
    // check frame's nri is valid or not
    if (codec == Codec::H264) {
        return ((nal_type == 1) && (nri == 64)) || ((nal_type == 5) && (nri == 96));
    } else if (codec == Codec::H265) {
        return true;
    }

    return false;
}

inline bool isNaluStart(Codec codec, const uint8_t *data)
{
    if (!data) {
        return false;
    }
    // parse nalu header
    std::vector<uint8_t> nal_header;
    if (codec == Codec::H264) {
        nal_header.push_back(data[NAL_HEADER_START_POS]);
    } else if (codec == Codec::H265) {
        nal_header.push_back(data[NAL_HEADER_START_POS]);
        nal_header.push_back(data[NAL_HEADER_START_POS + 1]);
        uint8_t reserved_zero_bits = ((nal_header[0] & 0x1) << 6) | (nal_header[1] & 0xF8);
        if (reserved_zero_bits != 0) {
            return false;
        }
    }
    if (nal_header.empty()) {
        return false;
    }
    // check forbidden bit
    bool forbidden = (nal_header[0] & 0x80);
    if (forbidden) {
        return false;
    }

    uint8_t nal_type = getNalType(codec, nal_header[0]);
    // check nri bits
    uint8_t nri = (nal_header[0] & 0x60);
    if (!isNriValid(codec, nal_type, nri)) {
        return false;
    }

    return isParameterSetNalu(codec, nal_type) || isFrameNalu(codec, nal_type);
}

inline Codec getVideoCodecByData(const uint8_t *data)
{
    if (!data) {
        return Codec::Unknown;
    }

    Codec codec = Codec::Unknown;
    for (int check_codec = static_cast<int>(Codec::H265);
            check_codec > static_cast<int>(Codec::Unknown); --check_codec) {
        if (isNaluStart(static_cast<Codec>(check_codec), data)) {
            codec = static_cast<Codec>(check_codec);
            break;
        }
    }

    return codec;
}

inline bool isJpegStart(const uint8_t *data)
{
    if (!data) {
        return false;
    }
    // check jpeg soi
    return (data[0] == JPEG_SOI[0]) && (data[1] == JPEG_SOI[1]);
}

inline bool isJpegEnd(const uint8_t *data)
{
    if (!data) {
        return false;
    }
    // check jpeg eoi
    return (data[0] == JPEG_EOI[0]) && (data[1] == JPEG_EOI[1]);
}

inline bool isSubtitleStart(const uint8_t *data)
{
    if (!data) {
        return false;
    }
    // check subtitle soi
    return (data[0] == SUBTITLE_SOI);
}

} // namespace cvi_file_recover
