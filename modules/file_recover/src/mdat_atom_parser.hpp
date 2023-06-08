#pragma once

#include <vector>
#include "atom.hpp"
#include "track.hpp"
#include "file.hpp"

namespace cvi_file_recover {

class MdatAtomParser final
{
public:
    using DataBuffer = std::vector<unsigned char>;
    using AtomPointer = Atom::AtomPointer;
    using TrackPointer = Track::TrackPointer;

    MdatAtomParser();
    explicit MdatAtomParser(const AtomPointer &atom) noexcept;
    explicit MdatAtomParser(AtomPointer &&atom) noexcept;

    int parse(File& file);
    TrackPointer getTrack(TrackType type);
    void filetype(int type);

private:
    void reset();
    double getRecoverPercentage(uint64_t current_pos);
    void dumpProgress(uint64_t current_pos, uint64_t total_skip_bytes);
    void dumpSummary(uint64_t parsed_pos, uint64_t total_skip_bytes);
    bool readDataFromFile(File& file, uint64_t offset, uint64_t length, DataBuffer& data);
    bool readUint32FromFile(File& file, uint64_t offset, uint32_t& value);
    bool isThumbnailPacket(const DataBuffer &readed_data, uint64_t data_offset,
        File& file, uint64_t start_pos, uint32_t &packet_length);
    bool isVideoPacket(const DataBuffer &readed_data, uint64_t data_offset,
        File& file, uint64_t start_pos, uint32_t &packet_length);
    bool isAudioPacket(const DataBuffer &readed_data, uint64_t data_offset,
        File& file, uint64_t start_pos, uint32_t &packets_length);
    bool isSubtitlePacket(const DataBuffer &readed_data, uint64_t data_offset,
        uint64_t start_pos, uint32_t &packet_length);
    uint32_t readParameterSetLength(File& file, uint64_t pos);
    bool parseSps(File& file, uint64_t offset);
    bool loop_end(bool find_end, int check_count);

private:
    AtomPointer mdat_atom;
    bool parsed_sps{false};
    double prev_dump_percentage{0};
    int read_fail_count{0};
    std::shared_ptr<AudioTrack> audio_track;
    std::shared_ptr<VideoTrack> video_track;
    std::shared_ptr<SubtitleTrack> subtitle_track;
    std::shared_ptr<MetaTrack> thumbnail_track;
};

} // namespace cvi_file_recover
