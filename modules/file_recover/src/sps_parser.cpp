#include "sps_parser.hpp"
#include <stdexcept>
#include "app_ipcam_comm.h"

namespace cvi_file_recover {

using std::vector;

constexpr uint32_t ONE_BYTE_BITS = 8;
constexpr uint32_t MAX_COUNT = 32;

bool SPSParser::parse(Codec codec, const vector<uint8_t>& buffer)
{
    try {
        init(buffer);
        if (codec == Codec::H264) {
            ebspToRbsp();
            parseH264();
        } else if (codec == Codec::H265) {
            ebspToRbsp();
            parseH265();
        }
    } catch (const std::exception& e) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Parse exception %s\r\n", e.what());
        return false;
    }

    return true;
}

void SPSParser::init(const vector<uint8_t>& buffer)
{
    data = buffer;
    current_bit = 0;
}

size_t SPSParser::getDataSize() const
{
    return data.size();
}

void SPSParser::ebspToRbsp()
{
    constexpr uint64_t look_forward_bytes = 3;
    std::vector<uint8_t> rbsp_data;
    size_t data_length = getDataSize();
    for (uint64_t i = 0; i < data_length;) {
        uint8_t byte = 0;
        // emulation prevention three byte
        if (((i + look_forward_bytes) < data_length) && (getBits(16, ONE_BYTE_BITS) == 0x03)) {
            byte = readBits(ONE_BYTE_BITS);
            rbsp_data.emplace_back(byte);
            byte = readBits(ONE_BYTE_BITS);
            rbsp_data.emplace_back(byte);
            readBits(ONE_BYTE_BITS);
            i += look_forward_bytes;
        } else {
            byte = readBits(ONE_BYTE_BITS);
            rbsp_data.emplace_back(byte);
            ++i;
        }
    }

    current_bit = 0;
    data = rbsp_data;
}

void SPSParser::parseH264()
{
    int profile_idc = readBits(8);
    // constraint_set0_flag
    readBit();
    // constraint_set1_flag
    readBit();
    // constraint_set2_flag
    readBit();
    // constraint_set3_flag
    readBit();
    // constraint_set4_flag
    readBit();
    // constraint_set4_flag
    readBit();
    // reserved_zero_2bits
    readBits(2);
    // level_idc
    readBits(8);
    // seq_parameter_set_id
    readExponentialGolombCode();
    if (profile_idc == 100 || profile_idc == 110 ||
        profile_idc == 122 || profile_idc == 244 ||
        profile_idc == 44 || profile_idc == 83 ||
        profile_idc == 86 || profile_idc == 118 )
    {
        int chroma_format_idc = readExponentialGolombCode();
        if (chroma_format_idc == 3) {
            // residual_colour_transform_flag
            readBit();
        }
        // bit_depth_luma_minus8
        readExponentialGolombCode();
        // bit_depth_chroma_minus8
        readExponentialGolombCode();
        // qpprime_y_zero_transform_bypass_flag
        readBit();
        int seq_scaling_matrix_present_flag = readBit();
        if (seq_scaling_matrix_present_flag) {
            for (int i = 0; i < ((chroma_format_idc != 3)?8:12); ++i) {
                int seq_scaling_list_present_flag = readBit();
                if (seq_scaling_list_present_flag) {
                    int size_of_scaling_list = (i < 6) ? 16 : 64;
                    int last_scale = 8;
                    int next_scale = 8;
                    for (int j = 0; j < size_of_scaling_list; j++) {
                        if (next_scale != 0) {
                            int delta_scale = readSE();
                            next_scale = (last_scale + delta_scale + 256) % 256;
                        }
                        last_scale = (next_scale == 0) ? last_scale : next_scale;
                    }
                }
            }
        }
    }

    // log2_max_frame_num_minus4
    readExponentialGolombCode();
    int pic_order_cnt_type = readExponentialGolombCode();
    if (pic_order_cnt_type == 0) {
        // log2_max_pic_order_cnt_lsb_minus4
        readExponentialGolombCode();
    } else if (pic_order_cnt_type == 1) {
        // delta_pic_order_always_zero_flag
        readBit();
        // offset_for_non_ref_pic
        readSE();
        // offset_for_top_to_bottom_field
        readSE();
        int num_ref_frames_in_pic_order_cnt_cycle = readExponentialGolombCode();
        for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
            readSE();
        }
    }
    // max_num_ref_frames
    readExponentialGolombCode();
    // gaps_in_frame_num_value_allowed_flag
    readBit();
    int pic_width_in_mbs_minus1 = readExponentialGolombCode();
    int pic_height_in_map_units_minus1 = readExponentialGolombCode();
    int frame_mbs_only_flag = readBit();
    if (!frame_mbs_only_flag) {
        // mb_adaptive_frame_field_flag
        readBit();
    }
    // direct_8x8_inference_flag
    readBit();
    int frame_crop_left_offset = 0;
    int frame_crop_right_offset = 0;
    int frame_crop_top_offset = 0;
    int frame_crop_bottom_offset = 0;
    int frame_cropping_flag = readBit();
    if (frame_cropping_flag) {
        frame_crop_left_offset = readExponentialGolombCode();
        frame_crop_right_offset = readExponentialGolombCode();
        frame_crop_top_offset = readExponentialGolombCode();
        frame_crop_bottom_offset = readExponentialGolombCode();
    }

    // vui_parameters_present_flag
    int vui_parameters_present_flag = readBit();
    if (vui_parameters_present_flag) {
        // aspect_ratio_info_present_flag
        int aspect_ratio_info_present_flag = readBit();
        if (aspect_ratio_info_present_flag) {
            int aspect_ratio_idc = readBits(8);
            if (aspect_ratio_idc == 255) {      //Extended_SAR
                readBits(16);      //sar_width
                readBits(16);      //sar_height
            }
        }

        int overscan_info_present_flag = readBit();
        if (overscan_info_present_flag) {
            readBit();       //overscan_appropriate_flag
        }

        int video_signal_type_present_flag = readBit();
        if (video_signal_type_present_flag) {
            readBits(3);      //video_format
            readBit();       //video_full_range_flag
            int colour_description_present_flag = readBit();
            if (colour_description_present_flag) {
                readBits(8);      //colour_primaries
                readBits(8);       //transfer_characteristics
                readBits(8);       //matrix_coefficients
            }
        }

        int chroma_loc_info_present_flag = readBit();
        if (chroma_loc_info_present_flag) {
            readExponentialGolombCode();     //chroma_sample_loc_type_top_field
            readExponentialGolombCode();     //chroma_sample_loc_type_bottom_field
        }

        int timing_info_present_flag = readBit();
        if (timing_info_present_flag) {
            int num_units_in_tick = readBits(32);
            int time_scale = readBits(32);
            int fixed_frame_rate_flag = readBit();
            float fps;

            if (fixed_frame_rate_flag) {
                time_scale >>= 1;
            }

            if (time_scale > (100 << 16)) {
                time_scale -= (100 << 16);
                fps = ((float)time_scale / (float)num_units_in_tick) / 100;
            } else {
                fps = ((float)time_scale / (float)num_units_in_tick);
            }

            frame_info.fps = fps;
        }
    }

    frame_info.width = ((pic_width_in_mbs_minus1+1)*16) -
        (frame_crop_left_offset*2) - (frame_crop_right_offset*2);
    frame_info.height = ((2-frame_mbs_only_flag)* (pic_height_in_map_units_minus1+1)*16) -
        (frame_crop_top_offset*2) - (frame_crop_bottom_offset*2);
}

void SPSParser::parseH265()
{
    // video_parameter_set_id
    readBits(4);
    // max_sub_layers_minus1
    int max_sub_layers_minus1 = readBits(3);
    // temporal_id_nesting_flag
    readBit();
    // parse profile tier level
    parseH265TierProfile(max_sub_layers_minus1);
    // seq_parameter_set_id
    readExponentialGolombCode();
    int chroma_format_idc = readExponentialGolombCode();
    if (chroma_format_idc == 3) {
        // separate_colour_plane_flag
        readBit();
    } else if (chroma_format_idc > 3) {
        /// TODO: chroma_format_idc range is in [0, 3], workaround for strange format
        frame_info.width = 1920;
        frame_info.height = 1080;
        return;
    }

    int pic_width_in_luma_samples = readExponentialGolombCode();
    int pic_height_in_luma_samples = readExponentialGolombCode();
    frame_info.width = pic_width_in_luma_samples;
    frame_info.height = pic_height_in_luma_samples;
}

void SPSParser::parseH265TierProfile(int max_sub_layers_minus1)
{
    // general_profile_space
    readBits(2);
    // general_tier_flag
    readBit();
    // general_profile_idc
    readBits(5);
    for (int i = 0; i < 32; ++i) {
        // general_profile_compatibility_flag
        readBit();
    }
    // general_progressive_source_flag
    readBit();
    // general_interlaced_source_flag
    readBit();
    // general_non_packed_constraint_flag
    readBit();
    // general_frame_only_constraint_flag
    readBit();
    // general_reserved_zero_44bits
    readBits(32);
    readBits(12);
    // general_level_idc
    readBits(8);
    std::vector<int> sub_layer_profile_present_flag;
    std::vector<int> sub_layer_level_present_flag;
    for (int i = 0; i < max_sub_layers_minus1; ++i) {
        // sub_layer_profile_present_flag
        sub_layer_profile_present_flag.emplace_back(readBit());
        // sub_layer_level_present_flag
        sub_layer_level_present_flag.emplace_back(readBit());
    }
    if (max_sub_layers_minus1 > 0) {
        for (int i = max_sub_layers_minus1; i < 8; ++i) {
            // reserved_zero_2bits
            readBits(2);
        }
    }
    for (int i = 0; i < max_sub_layers_minus1; ++i) {
        if (sub_layer_profile_present_flag[i]) {
            // sub_layer_profile_space
            readBits(2);
            // sub_layer_tier_flag
            readBit();
            // sub_layer_profile_idc
            readBits(5);
            for (int j = 0; j < 32; ++j) {
                // sub_layer_profile_compatibility_flag
                readBit();
            }
            // sub_layer_progressive_source_flag
            readBit();
            // sub_layer_interlaced_source_flag
            readBit();
            // sub_layer_non_packed_constraint_flag
            readBit();
            // sub_layer_frame_only_constraint_flag
            readBit();
            // sub_layer_reserved_zero_44bits
            readBits(32);
            readBits(12);
        }
        if (sub_layer_level_present_flag[i]) {
            // sub_layer_level_idc
            readBits(8);
        }
    }
}

Info SPSParser::getFrameInfo() const
{
    return frame_info;
}

int SPSParser::getBits(uint64_t bits_offset, uint32_t n)
{
    uint64_t original_bit_pos = current_bit;
    current_bit += bits_offset;
    int value = readBits(n);
    current_bit = original_bit_pos;

    return value;
}

int SPSParser::readBit()
{
    if (current_bit >= (getDataSize() * ONE_BYTE_BITS)) {
        throw std::out_of_range("out of data range");
    }

    uint64_t pos = current_bit/ONE_BYTE_BITS;
    uint8_t offset = (current_bit % ONE_BYTE_BITS) + 1;
    current_bit++;

    return (data[pos] >> (ONE_BYTE_BITS-offset)) & 0x01;
}

int SPSParser::readBits(uint32_t n)
{
    if (n > (sizeof(int)*ONE_BYTE_BITS)) {
        throw std::invalid_argument("read bits n need <= 32");
    }

    int value = 0;
    for (uint32_t i = 0; i < n; i++) {
        value |= (readBit() << (n - i - 1));
    }

    return value;
}

int SPSParser::readExponentialGolombCode()
{
    int value = 0;
    uint32_t count = 0;
    while ((readBit() == 0) && (count < MAX_COUNT) ) {
        ++count;
    }

    value = readBits(count);
    value += (1 << count) - 1;

    return value;
}

int SPSParser::readSE()
{
    int value = readExponentialGolombCode();
    if (value & 0x01) {
        value = (value + 1)/2;
    } else {
        value = -(value/2);
    }

    return value;
}

} // namespace cvi_file_recover
