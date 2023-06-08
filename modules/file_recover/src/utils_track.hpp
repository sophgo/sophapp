#pragma once

namespace cvi_file_recover {

constexpr uint32_t SAMPLE_DURATION_MS = 40;
constexpr uint32_t SUBTITLE_SAMPLE_DURATION_MS = 1000;
constexpr uint32_t AUDIO_SAMPLE_SIZE = 640;
constexpr uint32_t AUDIO_SAMPLE_BYTE_WIDTH = 2;

constexpr char HDLR_ATOM_AUDIO_TYPE_NAME[] = "soun";
constexpr char HDLR_ATOM_VIDEO_TYPE_NAME[] = "vide";
constexpr char HDLR_ATOM_SUBTITLE_TYPE_NAME[] = "text";
constexpr char HDLR_ATOM_META_TYPE_NAME[] = "meta";

enum class TrackType : int
{
    Unknown,
    Video,
    Audio,
    Subtitle,
    Meta,
};

namespace utils {

inline TrackType getTrackTypeByName(const std::string &name)
{
    TrackType type{TrackType::Unknown};
    if (name == HDLR_ATOM_AUDIO_TYPE_NAME) {
        type = TrackType::Audio;
    } else if (name == HDLR_ATOM_VIDEO_TYPE_NAME) {
        type = TrackType::Video;
    } else if (name == HDLR_ATOM_SUBTITLE_TYPE_NAME) {
        type = TrackType::Subtitle;
    } else if (name == HDLR_ATOM_META_TYPE_NAME) {
        type = TrackType::Meta;
    }

    return type;
}

} // namespace utils
} // namespace cvi_file_recover
