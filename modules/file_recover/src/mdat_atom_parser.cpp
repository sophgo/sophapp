#include "mdat_atom_parser.hpp"
#include <array>
#include <cmath>
#include <time.h>
#include "packet.hpp"
#include "sps_parser.hpp"
#include <string.h>
#include "app_ipcam_comm.h"

namespace cvi_file_recover {

using std::array;
using std::string;
using std::vector;

constexpr int READ_DATA_LENGTH = 300;
constexpr int MAX_READ_FAIL_COUNT = 10;
//constexpr int MAX_AUDIO_CHECK_COUNT = 300;
constexpr uint16_t MAX_AUDIO_AMPLITUDE = 10000;
constexpr int MAX_VIDEO_PARAMETER_SET_LENGTH = 1000;
constexpr array<uint32_t, 1> AUDIO_SAMPLE_RATES = {1280};
constexpr array<uint32_t, 1> AUDIO_SAMPLE_RATES_MP4 = {1};
constexpr int SUBTITLE_HEADER_LENGTH = 2;
constexpr int SUBTITLE_DATA_LENGTH = 200;
constexpr int SUBTITLE_TOTAL_LENGTH = SUBTITLE_DATA_LENGTH + SUBTITLE_HEADER_LENGTH;
constexpr int SKIP_BYTE_THRESHOLD = 2000;
constexpr int SKIP_BYTE_HARD_THRESHOLD = 30000;
constexpr int RECOVER_PERCENTAGE_THRESHOLD = 80;
constexpr int THUMBNAIL_MAX_SEARCH_BYTES = 10000000;
constexpr int RECOVER_PERCENTAGE_OVERTIME = 30;
#ifdef CLOCKS_PER_SEC
#undef CLOCKS_PER_SEC
#endif
#define CLOCKS_PER_SEC (10000000L)
#define TYPE_MP4 (1)
#define TYPE_MOV (0)
static int file_type = TYPE_MOV;
constexpr bool isPacketLengthValid(uint64_t length, uint64_t min_length, uint64_t max_length)
{
    return (length != 0) && (length >= min_length) && (length <= max_length);
}

constexpr bool isPacketEndPosValid(uint64_t pos, uint64_t max_pos)
{
    return (pos <= max_pos);
}

MdatAtomParser::MdatAtomParser()
{
    audio_track = std::make_shared<AudioTrack>();
    video_track = std::make_shared<VideoTrack>();
    subtitle_track = std::make_shared<SubtitleTrack>();
    thumbnail_track = std::make_shared<MetaTrack>();
}

MdatAtomParser::MdatAtomParser(const AtomPointer &atom) noexcept :
MdatAtomParser()
{
    mdat_atom = atom;
}

MdatAtomParser::MdatAtomParser(AtomPointer &&atom) noexcept :
MdatAtomParser()
{
    mdat_atom = std::move(atom);
}

void MdatAtomParser::filetype(int type)
{
    file_type = type;
    return;
}

int MdatAtomParser::parse(File& file)
{
    reset();
    bool g_preallocatestate = mdat_atom->getpreallocatestate();
    constexpr uint8_t skip_step_bytes = 1;
    uint64_t total_skip_bytes = 0;
    uint64_t offset = 0;
    DataBuffer data;
    bool is_thumbnail_found = false;
/*     bool skipflage = false;
    uint64_t skip_bytes = 20000; */
    clock_t start;
    start = clock();
    int v_count = 0, a_count = 0, s_count = 0;

    while (readDataFromFile(file, offset, READ_DATA_LENGTH, data)) {
        if (read_fail_count > MAX_READ_FAIL_COUNT) {
            break;
        }

        uint64_t read_start = offset;
        uint64_t read_end = offset + READ_DATA_LENGTH;

        while (offset + 6 < read_end) {
            uint32_t packet_length = 0;
            uint64_t data_offset = offset - read_start;

            // parse data to packet
            if (!is_thumbnail_found && isThumbnailPacket(data, data_offset, file, offset, packet_length)) {
                is_thumbnail_found = true;

                thumbnail_track->pushSample(TrackSample {
                    .offset = offset + mdat_atom->getContentStartPos(),
                    .size = packet_length
                });

                offset += packet_length;
            } else if (isSubtitlePacket(data, data_offset, offset, packet_length)) {
                if (packet_length == SUBTITLE_TOTAL_LENGTH) {
                    subtitle_track->pushSample(TrackSample {
                        .offset = offset + mdat_atom->getContentStartPos(),
                        .size = packet_length
                    });
                }

                offset += packet_length;
                s_count += 1;
            } else if (isVideoPacket(data, data_offset, file, offset, packet_length)) {
                video_track->pushSample(TrackSample {
                    .offset = offset + mdat_atom->getContentStartPos(),
                    .size = packet_length
                });

                offset += packet_length;
                v_count += 1;
            } else if (isAudioPacket(data, data_offset, file, offset, packet_length)) {
                audio_track->pushSample(TrackSample {
                    .offset = offset + mdat_atom->getContentStartPos() + 4,
                    .size = packet_length
                });

                offset += packet_length + 4;
                a_count += 1;
            } else {
                // try skip some bytes
                if (g_preallocatestate) {
                    if (0 == v_count) {
                        file.clear();

                        return 1;
                    }
                    goto END;
                }
/*                 if (!skipflage) {
                    if (total_skip_bytes < (skip_bytes - 202)) {
                        offset += (total_skip_bytes / 2);
                    } else {
                        offset += skip_bytes;
                    }
                    skipflage = true;
                } else {
                    offset += skip_step_bytes;
                } */
                offset += skip_step_bytes;
                total_skip_bytes += skip_step_bytes;

                if ((total_skip_bytes >= SKIP_BYTE_THRESHOLD) &&
                    (getRecoverPercentage(offset) >= RECOVER_PERCENTAGE_THRESHOLD)) {
                    goto END;
                }

                if (TYPE_MOV == file_type) {
                    if (total_skip_bytes >= SKIP_BYTE_HARD_THRESHOLD) {
                        goto END;
                    }
                }

                if ((mdat_atom->getEstimateContentLength() - offset) < 202) {
                    goto END;
                }

                if((clock() - start) >= CLOCKS_PER_SEC) {
                    if (getRecoverPercentage(offset) >= RECOVER_PERCENTAGE_OVERTIME) {
                        goto END;
                    }
                    dumpSummary(offset, total_skip_bytes);
                    file.clear();

                    return 1;
                }

                continue;
            }
        }

        dumpProgress(offset, total_skip_bytes);
    }

END:

    if (g_preallocatestate) {
        mdat_atom->setLength(offset);
    }
    dumpSummary(offset, total_skip_bytes);
    file.clear();

    if (g_preallocatestate) {
        mdat_atom->setLength(offset);
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO,"v_count = %d, a_count = %d, s_count = %d, offset = %ld\r\n", v_count, a_count, s_count, offset);
    return 0;
}

MdatAtomParser::TrackPointer MdatAtomParser::getTrack(TrackType type)
{
    if (type == TrackType::Video) {
        return std::dynamic_pointer_cast<Track>(video_track);
    } else if (type == TrackType::Audio) {
        return std::dynamic_pointer_cast<Track>(audio_track);
    } else if (type == TrackType::Subtitle) {
        return std::dynamic_pointer_cast<Track>(subtitle_track);
    } else if (type == TrackType::Meta) {
        return std::dynamic_pointer_cast<Track>(thumbnail_track);
    }

    return std::dynamic_pointer_cast<Track>(video_track);
}

void MdatAtomParser::reset()
{
    parsed_sps = false;
    prev_dump_percentage = 0;
    read_fail_count = 0;
    audio_track->clear();
    video_track->clear();
    subtitle_track->clear();
    thumbnail_track->clear();
}

double MdatAtomParser::getRecoverPercentage(uint64_t current_pos) {
    uint64_t total_data_length = mdat_atom->getEstimateContentLength();
    if (total_data_length == 0) {
        return 100.0;
    }

    return (current_pos/static_cast<double>(total_data_length))*100;
}

void MdatAtomParser::dumpProgress(uint64_t current_pos, uint64_t total_skip_bytes)
{
    constexpr double dump_percentage_diff = 5.0;
    uint64_t total_data_length = mdat_atom->getEstimateContentLength();
    if (total_data_length == 0) {
        return;
    }

    if (!mdat_atom->getpreallocatestate()) {
        double percentage = (current_pos/static_cast<double>(total_data_length))*100;
        if ((percentage - prev_dump_percentage) > dump_percentage_diff) {
            APP_PROF_LOG_PRINT(LEVEL_INFO,"Parse progress ... %lu/%lu (%.1lf%%), total_skip_bytes=[%lu]\r\n",
                current_pos, total_data_length, percentage, total_skip_bytes);
            prev_dump_percentage = percentage;
        }
    } else {
    }
}

void MdatAtomParser::dumpSummary(uint64_t parsed_pos, uint64_t total_skip_bytes)
{
    if (!mdat_atom->getpreallocatestate()) {
        uint64_t total_data_length = mdat_atom->getEstimateContentLength();
        double percentage = (parsed_pos/static_cast<double>(total_data_length))*100;
        APP_PROF_LOG_PRINT(LEVEL_INFO,"Parse finish at pos %lu/%lu (%.1lf%%)\r\n", parsed_pos, total_data_length, percentage);
    }
}

bool MdatAtomParser::readDataFromFile(File& file, uint64_t offset, uint64_t length,
        DataBuffer& data)
{
    if (!mdat_atom->getpreallocatestate()) {
        if ((offset + length) > mdat_atom->getEstimateContentLength()) {
            return false;
        }
    }

    if (/* ((offset + length) > mdat_atom->getEstimateContentLength()) || */
        !file.isOpened()) {
        return false;
    }

    file.seek(mdat_atom->getContentStartPos() + offset);
    data = file.read(length);
    if (data.size() != length) {
        read_fail_count++;
        return false;
    }

    return true;
}

bool MdatAtomParser::readUint32FromFile(File& file, uint64_t offset, uint32_t& value)
{
    DataBuffer data;
    if (!readDataFromFile(file, offset, sizeof(value), data)) {
        return false;
    }

    value = utils::read<uint32_t>(data.begin());

    return true;
}

bool MdatAtomParser::isThumbnailPacket(const DataBuffer &readed_data, uint64_t data_offset,
        File& file, uint64_t start_pos, uint32_t &packet_length)
{
    if (!isJpegStart(&readed_data[data_offset])) {
        return false;
    }

    uint64_t offset = start_pos + JPEG_SOI_LENGTH;
    DataBuffer check_buffer;

    while (readDataFromFile(file, offset, READ_DATA_LENGTH, check_buffer)) {
        uint64_t buffer_begin = offset;
        uint64_t buffer_end = offset + READ_DATA_LENGTH;

        while (offset + JPEG_EOI_LENGTH <= buffer_end) {
            auto i = offset - buffer_begin;

            if (isJpegEnd(&check_buffer[i])) {
                offset += JPEG_EOI_LENGTH;
                packet_length = offset - start_pos;
                return true;
            }

            ++offset;
        }

        if (offset - start_pos > THUMBNAIL_MAX_SEARCH_BYTES) {
            APP_PROF_LOG_PRINT(LEVEL_INFO,"Thumbnail search over [%d] bytes\r\n", THUMBNAIL_MAX_SEARCH_BYTES);
            break;
        }
    }

    return false;
}

bool MdatAtomParser::isVideoPacket(const DataBuffer &readed_data, uint64_t data_offset,
        File& file, uint64_t start_pos, uint32_t &packet_length)
{
    // parse data to get video codec
    if (video_track->getCodec() == Codec::Unknown) {
        video_track->setCodec(getVideoCodecByData(&readed_data[data_offset]));
    }
    // video packet may contains multiple parameter nalus + one frame nalu
    Codec codec = video_track->getCodec();
    if (isNaluStart(codec, &readed_data[data_offset])) {
        uint64_t offset = start_pos;
        uint32_t data_length = 0;
        // get total length of parameter set nalus, and add to offset
        if (isParameterSetNalu(codec, getNalType(codec, readed_data[data_offset+NAL_HEADER_START_POS]))) {
            do {
                data_length = readParameterSetLength(file, offset);
                if (data_length > MAX_VIDEO_PARAMETER_SET_LENGTH) {
                    return false;
                } else if (data_length != 0) {
                    offset += (data_length + sizeof(data_length));
                }
            } while (data_length != 0);
        }
        if (offset != start_pos) {
            // check last nalu valid, which is appended by parameter set nalus
            DataBuffer check_buffer;
            if (!readDataFromFile(file, offset, READ_DATA_LENGTH, check_buffer)) {
                return false;
            }
            if (!isNaluStart(codec, &check_buffer[0])) {
                return false;
            }
        }
        // read length of frame nalu
        if (!readUint32FromFile(file, offset, data_length)) {
            return false;
        }
        packet_length = (offset - start_pos + data_length + sizeof(data_length));
        if (!isPacketLengthValid(data_length, video_track->getEstimateMinPacketLength(),
                video_track->getEstimateMaxPacketLength()) ||
            !isPacketEndPosValid(start_pos + packet_length, mdat_atom->getEstimateContentLength())) {
            return false;
        }

        return true;
    }

    return false;
}

bool MdatAtomParser::loop_end(bool find_end, int check_count)
{
    int MAX_AUDIO_CHECK_COUNT = 30;
    if (TYPE_MP4 == file_type) {
        MAX_AUDIO_CHECK_COUNT = 1000;
    }
    return (find_end) || (check_count >= MAX_AUDIO_CHECK_COUNT);
}

bool MdatAtomParser::isAudioPacket(const DataBuffer &readed_data, uint64_t data_offset,
        File& file, uint64_t start_pos, uint32_t &packet_length)
{
    bool find_end = false;
    bool need_check_data = false;
    uint64_t offset = start_pos;
    int check_count = 0;
    uint64_t read_pos = 0;
    if (TYPE_MP4 == file_type) {
        return false;
    }
    if (readed_data[data_offset] == 255 && readed_data[data_offset + 1] == 241 && readed_data[data_offset + 2] == 96 && readed_data[data_offset + 3] == 64) {
        packet_length = 1280;
        return true;
    } else {
        return false;
    }

    while (!loop_end(find_end, check_count)) {
        if (TYPE_MP4 == file_type) {
            uint32_t match_sampe_rates = AUDIO_SAMPLE_RATES_MP4[0];
            for (uint32_t sampe_rates : AUDIO_SAMPLE_RATES_MP4) {
                read_pos = offset + sampe_rates;
                // check read pos is reach data's end
                if (read_pos == mdat_atom->getEstimateContentLength()) {
                    match_sampe_rates = sampe_rates;
                    find_end = true;
                    need_check_data = true;
                    break;
                }

                DataBuffer check_buffer;
                if (!readDataFromFile(file, read_pos, READ_DATA_LENGTH, check_buffer)) {
                    return false;
                }
                static uint32_t dummy_length;
                if (isVideoPacket(check_buffer, 0, file, read_pos, dummy_length) ||
                    isSubtitlePacket(check_buffer, 0, read_pos, dummy_length)) {
                    match_sampe_rates = sampe_rates;
                    find_end = true;
                    break;
                }
            }
            offset += match_sampe_rates;
            check_count++;
        } else {
            uint32_t match_sampe_rate = AUDIO_SAMPLE_RATES[0];
            for (uint32_t sampe_rate : AUDIO_SAMPLE_RATES) {
                read_pos = offset + sampe_rate;
                // check read pos is reach data's end
                if (!mdat_atom->getpreallocatestate()) {
                    if (read_pos == mdat_atom->getEstimateContentLength()) {
                        match_sampe_rate = sampe_rate;
                        find_end = true;
                        need_check_data = true;
                        break;
                    }
                }

                DataBuffer check_buffer;
                if (!readDataFromFile(file, read_pos, READ_DATA_LENGTH, check_buffer)) {
                    return false;
                }
                static uint32_t dummy_length;
                if (isVideoPacket(check_buffer, 0, file, read_pos, dummy_length) ||
                    isSubtitlePacket(check_buffer, 0, read_pos, dummy_length)) {
                    match_sampe_rate = sampe_rate;
                    find_end = true;
                    break;
                }
            }
            offset += match_sampe_rate;
            check_count++;
        }
    }

    if (find_end) {
        packet_length = offset - start_pos;
        // check amplitude avoid boom sound
        if (need_check_data) {
            DataBuffer audio_buffer;
            if (readDataFromFile(file, start_pos, packet_length, audio_buffer)) {
                // read 2 bytes (little endian)
                for (uint32_t i = 0; i < audio_buffer.size(); i += 2) {
                    int16_t amplitude = audio_buffer[i] | (audio_buffer[i+1] << 8);
                    if (abs(amplitude) > MAX_AUDIO_AMPLITUDE) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    return false;
}

bool MdatAtomParser::isSubtitlePacket(const DataBuffer &readed_data, uint64_t data_offset,
        uint64_t start_pos, uint32_t &packet_length)
{
    if (isSubtitleStart(&readed_data[data_offset])) {
        uint8_t length = readed_data[data_offset+1];
        // check is dummy empty subtitle, which will append another subtitle
        if ((length == 0) && isSubtitleStart(&readed_data[data_offset+2]) &&
            (readed_data[data_offset+3] == SUBTITLE_DATA_LENGTH)) {
            packet_length = SUBTITLE_HEADER_LENGTH;
            return true;
        // subtitle has fixed length
        } else if (length == SUBTITLE_DATA_LENGTH) {
            if (TYPE_MP4 == file_type) {
                if (((readed_data[data_offset + 2] == 35) && (readed_data[data_offset + 3] == 65)) ||
                    ((readed_data[data_offset + 2] == 103) && (readed_data[data_offset + 3] == 115))) {
                    packet_length = SUBTITLE_TOTAL_LENGTH;
                    return true;
                } else {
                    return false;
                }
            } else {
                packet_length = SUBTITLE_TOTAL_LENGTH;
                return true;
            }
        }
    }

    return false;
}

uint32_t MdatAtomParser::readParameterSetLength(File& file, uint64_t offset)
{
    DataBuffer check_buffer;
    if (!readDataFromFile(file, offset, READ_DATA_LENGTH, check_buffer)) {
        return 0;
    }
    Codec codec = video_track->getCodec();
    uint8_t nalu_type = getNalType(codec, check_buffer[NAL_HEADER_START_POS]);
    if (!isParameterSetNalu(codec, nalu_type)) {
        return 0;
    }
    // parse first sps
    if (!parsed_sps && isSPSNalu(codec, nalu_type)) {
        parsed_sps = parseSps(file, offset);
    }
    // read nalu length
    uint32_t packet_length = 0;
    if (!readUint32FromFile(file, offset, packet_length)) {
        return 0;
    }

    return packet_length;
}

bool MdatAtomParser::parseSps(File& file, uint64_t offset)
{
    uint32_t packet_length = 0;
    if (!readUint32FromFile(file, offset, packet_length)) {
        return false;
    }
    // check length valid
    if ((packet_length == 0) || (packet_length > MAX_VIDEO_PARAMETER_SET_LENGTH)) {
        return false;
    }
    // 4 bytes length and 1 or 2 bytes for nalu header (h265 nalu header has 2 bytes)
    Codec codec = video_track->getCodec();
    int sps_header_length = (codec == Codec::H265) ? H265_NAL_HEADER_LENGTH : H264_NAL_HEADER_LENGTH;
    int sps_start_offset = NAL_HEADER_START_POS + sps_header_length;
    DataBuffer sps_buffer;
    if (!readDataFromFile(file, offset + sps_start_offset, packet_length - sps_header_length, sps_buffer)) {
        return false;
    }
    // parse sps to get frame size
    SPSParser sps_parser;
    if (!sps_parser.parse(codec, sps_buffer)) {
        return false;
    }
    video_track->setSpsBuffer(sps_buffer);
    video_track->setFrameInfo(sps_parser.getFrameInfo());

    return true;
}

} // namespace cvi_file_recover
