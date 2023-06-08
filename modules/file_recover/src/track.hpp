#pragma once

#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
#include "utils_track.hpp"
#include "types.hpp"

namespace cvi_file_recover {

struct TrackSample
{
    uint64_t offset{0};
    uint32_t size{0};
};

class Track
{
public:
    using TrackPointer = std::shared_ptr<Track>;

    explicit Track(TrackType type, int track_id) :
    type(type),track_id(track_id)
    {}

    ~Track() = default;

    bool empty() const
    {
        return (samples.size() == 0);
    }

    void pushSample(const TrackSample &sample)
    {
        samples.push_back(sample);
    }

    void pushSample(TrackSample &&sample)
    {
        samples.emplace_back(std::move(sample));
    }

    const std::vector<TrackSample> &getSamples() const
    {
        return samples;
    }

    size_t getSamplesSize() const
    {
        return samples.size();
    }

    size_t getTotalSize() const
    {
        size_t total_size = 0;
        for (const TrackSample &sample : samples) {
            total_size += sample.size;
        }

        return total_size;
    }

    TrackType getType() const
    {
        return type;
    }

    int getTrackId() const
    {
        return track_id;
    }

    virtual uint32_t getDuration() const
    {
        return samples.size() * SAMPLE_DURATION_MS;
    }

    virtual void clear()
    {
        samples.clear();
    }

protected:
    TrackType type{TrackType::Unknown};
    std::vector<TrackSample> samples;
    int track_id;
};

class AudioTrack : public Track
{
public:
    AudioTrack() : Track(TrackType::Audio, 2)
    {}
};

class VideoTrack : public Track
{
public:
    VideoTrack() : Track(TrackType::Video, 1)
    {}

    virtual void clear() override
    {
        codec = Codec::Unknown;
        frame_info = {};
        Track::clear();
    }

    uint64_t getEstimateMaxPacketLength() const
    {
        if ((frame_info.width == 0) || (frame_info.height == 0)) {
            return std::numeric_limits<uint64_t>::max();
        }

        return std::ceil((frame_info.width*frame_info.height)*1.5);
    }

    uint64_t getEstimateMinPacketLength() const
    {
        return 128;
    }

    Codec getCodec() const
    {
        return codec;
    }

    void setCodec(Codec codec)
    {
        this->codec = codec;
    }

    Info getFrameInfo() const
    {
        return frame_info;
    }

    void setFrameInfo(Info info)
    {
        frame_info = info;
    }

    void setSpsBuffer(const std::vector<unsigned char>& buffer)
    {
        sps_buffer = buffer;
    }

    std::vector<unsigned char> getSpsBuffer() const
    {
        return sps_buffer;
    }

    virtual uint32_t getDuration() const
    {
        return samples.size() * 1000 / frame_info.fps;
    }

private:
    Info frame_info{};
    Codec codec{Codec::Unknown};
    std::vector<unsigned char> sps_buffer;
};

class SubtitleTrack : public Track
{
public:
    SubtitleTrack() : Track(TrackType::Subtitle, 3)
    {}

    virtual uint32_t getDuration() const override
    {
        return samples.size() * SUBTITLE_SAMPLE_DURATION_MS;
    }
};

class MetaTrack : public Track
{
public:
    MetaTrack() : Track(TrackType::Meta, 4)
    {}

    virtual uint32_t getDuration() const override
    {
        // thumbnail meta duration is 1
        return 1;
    }
};

} // namespace cvi_file_recover
