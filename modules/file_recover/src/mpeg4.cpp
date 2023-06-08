#include "mpeg4.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include "mdat_atom_parser.hpp"
#include "packet.hpp"
#include "types.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include "app_ipcam_comm.h"

namespace cvi_file_recover {

using std::string;
using std::vector;
using std::shared_ptr;

namespace {

using AtomPointer = Atom::AtomPointer;
using TrackPointer = Track::TrackPointer;

//constexpr char H264_REPAIR_TEMPLATE_PATH[] = "/mnt/system/bin/h264_pcm_template.bin";
//constexpr char H264_MP4_REPAIR_TEMPLATE_PATH[] = "/mnt/system/bin/mp4_aac_template.bin";
//constexpr char H265_REPAIR_TEMPLATE_PATH[] = "/mnt/system/bin/h265_pcm_template.bin";
constexpr char H264_REPAIR_TEMPLATE_PATH[] = "/mnt/sd/h264_pcm_template.bin";
constexpr char H264_MP4_REPAIR_TEMPLATE_PATH[] = "/mnt/sd/mp4_aac_template.bin";
constexpr char H265_REPAIR_TEMPLATE_PATH[] = "/mnt/sd/h265_pcm_template.bin";
constexpr char SAMPLE_ENTRY_AVC1_NAME[] = "avc1";
constexpr char SAMPLE_ENTRY_HEV1_NAME[] = "hev1";

void updateTrackHeader(const AtomPointer &track_atom, const TrackPointer& track, int create_time);
void updateTrackMediaHeader(const AtomPointer &track_atom, const TrackPointer& track);
void updateTrackSampleTimeTable(const AtomPointer &track_atom, const TrackPointer& track);
void updateTrackSampleChunk(const AtomPointer &track_atom, const TrackPointer& track);
void updateTrackSampleSizeTable(const AtomPointer &track_atom, const TrackPointer& track);
void updateTrackSampleOffsetTable(const AtomPointer &track_atom, const TrackPointer& track);
void updateTrackSampleDescription(const AtomPointer &track_atom, const TrackPointer& track);
int  getFileCreateTime(const std::string &file_path);

} // anonymous namespace

#define TYPE_MP4 (1)
#define TYPE_MOV (0)
int File_type = TYPE_MOV;

Mpeg4::Mpeg4()
{
    root_atom = Atom::createAtomPointer();
}

void Mpeg4::close()
{
    Container::close();
    reset();
}

void Mpeg4::preallocatestate(bool PreallocFlage)
{
    AtomPointer atom = Atom::createAtomPointer();
    atom->preallocatestate(PreallocFlage);
}

int Mpeg4::parse()
{
    if (isParsed()) {
        return 0;
    }

    while (!file.eof()) {
        AtomPointer atom = Atom::createAtomPointer();
        atom->parse(file);
        if (atom->isValid()) {
            root_atom->addChild(std::move(atom));
            if (atom->getName() == "mdat") {
                return Container::parse();
            }
        } else {
            APP_PROF_LOG_PRINT(LEVEL_ERROR,"Atom %s is invalid\r\n", atom->getName().c_str());
            return -1;
        }
    }

    return Container::parse();
}

int Mpeg4::check()
{
    parse();
    AtomPointer ftyp_atom = getAtomByName(ATOM_FTYP_NAME);
    AtomPointer mdat_atom = getAtomByName(ATOM_MDAT_NAME);
    AtomPointer moov_atom = getAtomByName(ATOM_MOOV_NAME);

    return (ftyp_atom && mdat_atom && moov_atom) ? 0 : -1;
}

void Mpeg4::dump()
{
    parse();
    root_atom->dump();
}

int Mpeg4::recover(const std::string &device_model, bool has_create_time, bool PreallocFlage, int file_type)
{
    preallocatestate(PreallocFlage);

    AtomPointer atom = Atom::createAtomPointer();
    atom->Filetype(file_type);
    File_type = file_type;
    int create_time = 0;
    string replair_template_path;

    if (has_create_time) {
        create_time = getFileCreateTime(file_path) + 0x7C25B080;
    }

    if (parse() != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"parse atom failed\r\n");
        return -1;
    }

    // parse mdat atom
    AtomPointer mdat_atom = getAtomByName(ATOM_MDAT_NAME);
    if (!mdat_atom || !mdat_atom->isValid()) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Mdat atom is invalid\r\n");
        return -1;
    }
    MdatAtomParser mdat_parser(std::move(mdat_atom));
    mdat_parser.filetype(file_type);
    if (mdat_parser.parse(file) != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Mdat parse failed\r\n");
        return -1;
    }
    // replace moov atom to template
    root_atom->removeAtomsByName(ATOM_MOOV_NAME);
    Mpeg4 repair_template_file;
    shared_ptr<VideoTrack> video_track =
        std::dynamic_pointer_cast<VideoTrack>(mdat_parser.getTrack(TrackType::Video));
    if (TYPE_MOV == file_type) {
        replair_template_path = (video_track && video_track->getCodec() == Codec::H264) ?
            H264_REPAIR_TEMPLATE_PATH : H265_REPAIR_TEMPLATE_PATH;
    } else {
        replair_template_path = (video_track && video_track->getCodec() == Codec::H264) ?
            H264_MP4_REPAIR_TEMPLATE_PATH : H265_REPAIR_TEMPLATE_PATH;
    }
    int ret = 0;
    std::ios_base::openmode mode = std::fstream::in | std::fstream::binary;
    ret = repair_template_file.open(replair_template_path, mode);
    if (ret != 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't open repair template file %s ret = %d\r\n", replair_template_path.c_str(), ret);
        return -1;
    }
    repair_template_file.parse();
    AtomPointer repair_moov_atom = repair_template_file.getAtomByName(ATOM_MOOV_NAME);
    if (!repair_moov_atom || !repair_moov_atom->isValid()) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Repair template is invalid\r\n");
        return -1;
    }
    root_atom->addChild(std::move(repair_moov_atom));
    // update atoms
    updateMvhd(mdat_parser.getTrack(TrackType::Video), create_time);
    updateTrack(mdat_parser.getTrack(TrackType::Video), create_time);
    updateTrack(mdat_parser.getTrack(TrackType::Audio), create_time);
    updateTrack(mdat_parser.getTrack(TrackType::Subtitle), create_time);
    updateTrack(mdat_parser.getTrack(TrackType::Meta), create_time);

    if (device_model.length()){
        updateUdta(device_model);
    }

    return 0;
}

void Mpeg4::save(const string &file_path)
{
    // if output file is same as input file, only save updated atoms
    if (file_path == this->file_path) {
        vector<string> need_update_atoms_name = {ATOM_MDAT_NAME, ATOM_MOOV_NAME};
        for (const string& atom_name : need_update_atoms_name) {
            AtomPointer atom = getAtomByName(atom_name);
            if (!atom) {
                continue;
            }
            file.seek(atom->getStartPos());
            atom->save(file);
        }
    } else {
        // set atom's save done handler for write mdat content from file
        AtomPointer mdat_atom = getAtomByName(ATOM_MDAT_NAME);
        if (mdat_atom) {
            mdat_atom->setSaveDoneHandler([atom = AtomWeakPointer(mdat_atom), this](File& output_file) {
                saveAtomContentFromFile(output_file, atom);
            });
        }
        // save all atoms to output file
        File output_file;
        output_file.open(file_path, std::fstream::out | std::fstream::binary);
        root_atom->save(output_file);
        output_file.close();
    }
}

void Mpeg4::saveAtomContentFromFile(File& output_file, const AtomWeakPointer& atom)
{
    AtomPointer shared_atom = atom.lock();
    if (!shared_atom) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Atom is null\r\n");
        return;
    }

    int64_t file_pos = file.pos();
    file.seek(shared_atom->getContentStartPos());
    uint64_t save_bytes = 0, once_save_bytes = 5120;
    uint64_t total_bytes = shared_atom->getEstimateContentLength();
    vector<unsigned char> buffer;
    // read content data from input file and write those data to output file
    while (save_bytes < total_bytes) {
        once_save_bytes = std::min(once_save_bytes, total_bytes - save_bytes);
        buffer = file.read(once_save_bytes);
        output_file.write<unsigned char>(buffer);
        save_bytes += once_save_bytes;
    }
    // restore file position
    file.seek(file_pos);
}

void Mpeg4::saveAtomByName(const std::string &file_path, const std::string &atom_name)
{
    Atom::AtomPointer atom = getAtomByName(atom_name);
    if (atom) {
        File save_file;
        save_file.open(file_path, std::fstream::out | std::fstream::binary);
        atom->save(save_file);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't save atom %s, because not find this atom\r\n", atom_name.c_str());
    }
}

Atom::AtomPointer Mpeg4::getAtomByName(const string &name)
{
    return root_atom->getAtomByName(name);
}

void Mpeg4::reset()
{
    root_atom.reset(new Atom());
}

void Mpeg4::updateMvhd(const TrackPointer &track, int create_time)
{
    constexpr uint64_t duration_offset = 16;
    constexpr uint64_t create_time_offset = 4;
    constexpr uint64_t modification_time_offset = 8;
    constexpr uint64_t version_offset = 0;
    AtomPointer mvhd_atom = getAtomByName(ATOM_MVHD_NAME);
    if (mvhd_atom) {
        mvhd_atom->write(1, version_offset);
        mvhd_atom->write(create_time, create_time_offset);
        mvhd_atom->write(create_time, modification_time_offset);
        mvhd_atom->write(track->getDuration(), duration_offset);
    }
}

void Mpeg4::updateTrack(const TrackPointer& track, int create_time)
{
    constexpr uint64_t track_id_offset = 12;
    vector<AtomPointer> track_atoms = root_atom->getAtomsByName(ATOM_TRAK_NAME);
    // find match type track atom
    auto iterator = std::find_if(track_atoms.begin(), track_atoms.end(), [&track](const AtomPointer &atom) {
        AtomPointer tkhd_atom = atom->getAtomByName(ATOM_TKHD_NAME);
        if (tkhd_atom) {
            int track_id = tkhd_atom->read<int32_t>(track_id_offset);
            return (track_id == track->getTrackId());
        }

        return false;
    });

    if (iterator == track_atoms.end()) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find match track\r\n");
        return;
    }

    AtomPointer track_atom = (*iterator);
    // remove trak atom if track is empty
    if (track->empty()) {
        root_atom->removeAtom(track_atom);
        return;
    }
    // update track related atom
    updateTrackHeader(track_atom, track, create_time);
    updateTrackMediaHeader(track_atom, track);
    updateTrackSampleTimeTable(track_atom, track);
    updateTrackSampleChunk(track_atom, track);
    updateTrackSampleSizeTable(track_atom, track);
    updateTrackSampleOffsetTable(track_atom, track);
    updateTrackSampleDescription(track_atom, track);
    // remove edts atom
    track_atom->removeAtomsByName(ATOM_EDTS_NAME);
}

void Mpeg4::updateUdta(const std::string &device_model)
{
    AtomPointer udta_atom = root_atom->getAtomByName(ATOM_UDTA_NAME);
    if (udta_atom) {
        uint32_t title_size = 12 + device_model.length();
        udta_atom->resize(25 +  // entries
                          title_size); // sample for chunk
        udta_atom->write(title_size, 25);
        udta_atom->write(static_cast<uint32_t>(0xA96E616D), 29);
        udta_atom->write(static_cast<uint16_t>(device_model.length()), 33);
        udta_atom->write(static_cast<uint16_t>(0x55C4), 35);
        udta_atom->write(const_cast<char*>(device_model.c_str()), device_model.length(), 37);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_UDTA_NAME);
    }
}

namespace {

void updateTrackHeader(const AtomPointer &track_atom, const TrackPointer& track, int create_time)
{
    AtomPointer tkhd_atom = track_atom->getAtomByName(ATOM_TKHD_NAME);
    if (tkhd_atom) {
        constexpr uint64_t create_time_offset = 4;
        constexpr uint64_t modification_time_offset = 8;
        constexpr uint64_t duration_offset = 20;
        tkhd_atom->write(create_time, create_time_offset);
        tkhd_atom->write(create_time, modification_time_offset);
        tkhd_atom->write(track->getDuration(), duration_offset);
        // update video track frame size
        if (track->getType() == TrackType::Video) {
            shared_ptr<VideoTrack> video_track = std::dynamic_pointer_cast<VideoTrack>(track);
            if (!video_track) {
                return;
            }
            constexpr uint64_t width_offset = 76;
            constexpr uint64_t height_offset = 80;
            Info frame_size = video_track->getFrameInfo();
            uint32_t width = frame_size.width*pow(256, 2);
            if (width != 0) {
                tkhd_atom->write(width, width_offset);
            }
            uint32_t height = frame_size.height*pow(256, 2);
            if (height != 0) {
                tkhd_atom->write(height, height_offset);
            }
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_TKHD_NAME);
    }
}

void updateTrackMediaHeader(const AtomPointer &track_atom, const TrackPointer& track)
{
    AtomPointer mdhd_atom = track_atom->getAtomByName(ATOM_MDHD_NAME);
    if (mdhd_atom) {
        constexpr uint64_t timescale_offset = 12;
        constexpr uint64_t duration_offset = 16;
        if (track->getType() == TrackType::Meta) {
            mdhd_atom->write(track->getDuration(), duration_offset);
        } else if (track->getType() == TrackType::Video) {
            shared_ptr<VideoTrack> video_track = std::dynamic_pointer_cast<VideoTrack>(track);
            if (!video_track) {
                return;
            }

            Info frame_size = video_track->getFrameInfo();
            uint32_t timescale = 512 * frame_size.fps;
            mdhd_atom->write(timescale, timescale_offset);
            uint32_t new_duration = static_cast<uint32_t>(track->getDuration() * (timescale/1000.0));
            mdhd_atom->write(new_duration, duration_offset);
        } else {
            uint32_t timescale = mdhd_atom->read<uint32_t>(timescale_offset);
            uint32_t new_duration = static_cast<uint32_t>(track->getDuration() * (timescale/1000.0));
            mdhd_atom->write(new_duration, duration_offset);
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_MDHD_NAME);
    }
}

void updateTrackSampleTimeTable(const AtomPointer &track_atom, const TrackPointer& track)
{
    AtomPointer stts_atom = track_atom->getAtomByName(ATOM_STTS_NAME);
    if (stts_atom) {
        constexpr uint64_t entries_offset = 4;
        constexpr uint64_t time_table_offset = 8;
        stts_atom->resize(4 + // version
                          4 + // entries
                          8); // time table
        if (track->getType() == TrackType::Audio) {
            stts_atom->write(1, entries_offset);
            stts_atom->write(static_cast<uint32_t>(track->getTotalSize()/AUDIO_SAMPLE_BYTE_WIDTH),
                time_table_offset);
        } else {
            stts_atom->write(1, entries_offset);
            stts_atom->write(static_cast<uint32_t>(track->getSamplesSize()), time_table_offset);
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_STTS_NAME);
    }
}

void updateTrackSampleChunk(const AtomPointer &track_atom, const TrackPointer& track)
{
    AtomPointer stsc_atom = track_atom->getAtomByName(ATOM_STSC_NAME);
    if (stsc_atom) {
        stsc_atom->resize(4 +  // version
                          4 +  // entries
                          12); // sample for chunk
        if (track->getType() == TrackType::Audio) {
            stsc_atom->write(1, 4);
            stsc_atom->write(1, 8);
            stsc_atom->write(AUDIO_SAMPLE_SIZE, 12);
            stsc_atom->write(1, 16);
        } else {
            stsc_atom->write(1, 4);
            stsc_atom->write(1, 8);
            stsc_atom->write(1, 12);
            stsc_atom->write(1, 16);
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_STSC_NAME);
    }
}

void updateTrackSampleSizeTable(const AtomPointer &track_atom, const TrackPointer& track)
{
    AtomPointer stsz_atom = track_atom->getAtomByName(ATOM_STSZ_NAME);
    if (stsz_atom) {
        constexpr uint64_t entries_offset = 8;
        if (track->getType() == TrackType::Audio) {
            stsz_atom->write(static_cast<uint32_t>(track->getTotalSize()/AUDIO_SAMPLE_BYTE_WIDTH), entries_offset);
        } else if (track->getType() == TrackType::Meta) {
            constexpr uint64_t default_size_offset = 4;
            stsz_atom->write(static_cast<uint32_t>(track->getTotalSize()), default_size_offset);
        } else {
            stsz_atom->resize(4 +  // version
                              4 +  // default size
                              4 +  // entries
                              track->getSamplesSize()*4); // size table
            stsz_atom->write(static_cast<uint32_t>(track->getSamplesSize()), entries_offset);
            uint64_t offset = 12;
            const std::vector<TrackSample> &samples = track->getSamples();
            for (const TrackSample &sample : samples) {
                stsz_atom->write(sample.size, offset);
                offset += 4;
            }
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_STSZ_NAME);
    }
}

void updateTrackSampleOffsetTable(const AtomPointer &track_atom, const TrackPointer& track)
{
    AtomPointer stco_atom = track_atom->getAtomByName(ATOM_STCO_NAME);
    if (stco_atom) {
        constexpr uint64_t entries_offset = 4;
        stco_atom->resize(4 +  // version
                          4 +  // entries
                          track->getSamplesSize()*4); // offset table
        stco_atom->write(static_cast<uint32_t>(track->getSamplesSize()), entries_offset);
        uint64_t offset = 8;
        const std::vector<TrackSample> &samples = track->getSamples();
        for (const TrackSample &sample : samples) {
            stco_atom->write(static_cast<uint32_t>(sample.offset), offset);
            offset += 4;
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_ERROR,"Can't find %s atom in track\r\n", ATOM_STCO_NAME);
    }
}

void updateTrackSampleDescription(const AtomPointer &track_atom, const TrackPointer& track)
{
    if (track->getType() != TrackType::Video) {
        return;
    }

    AtomPointer stsd_atom = track_atom->getAtomByName(ATOM_STSD_NAME);
    if (stsd_atom) {
        shared_ptr<VideoTrack> video_track = std::dynamic_pointer_cast<VideoTrack>(track);
        if (!video_track) {
            return;
        }
        if (video_track->getCodec() == Codec::H264) {
            constexpr int64_t avc1_offset = 8;
            constexpr int64_t avc1_width_offset = avc1_offset + 32;
            constexpr int64_t avc1_height_offset = avc1_width_offset + 2;
            string entry_name = stsd_atom->read<string>(avc1_offset + 4, 4);
            if (entry_name != SAMPLE_ENTRY_AVC1_NAME) {
                return;
            }
            Info frame_size = video_track->getFrameInfo();
            stsd_atom->write(static_cast<uint16_t>(frame_size.width), avc1_width_offset);
            stsd_atom->write(static_cast<uint16_t>(frame_size.height), avc1_height_offset);
            // update sps
            constexpr int64_t sps_offset = 111;
            std::vector<unsigned char> sps_buffer = video_track->getSpsBuffer();
            if (1280 == (frame_size.width) && 720 == (frame_size.height)) {
                for (uint32_t i = 0; i < sps_buffer.size(); ++i) {
                    stsd_atom->write(sps_buffer[i], sps_offset + i);
                }
            } else if (2560 == (frame_size.width) && 1440 == (frame_size.height)){
                stsd_atom->write(static_cast<uint16_t>(23), sps_offset - 3);
                stsd_atom->write(static_cast<uint16_t>(50), sps_offset - 7);
                for (uint32_t i = 0; i < sps_buffer.size(); ++i) {
                    stsd_atom->write(sps_buffer[i], sps_offset + i);
                }
            }
            std::vector<unsigned char>().swap(sps_buffer);
        } else if (video_track->getCodec() == Codec::H265) {
            constexpr int64_t hev1_offset = 8;
            constexpr int64_t hev1_width_offset = hev1_offset + 32;
            constexpr int64_t hev1_height_offset = hev1_width_offset + 2;
            string entry_name = stsd_atom->read<string>(hev1_offset + 4, 4);
            if (entry_name != SAMPLE_ENTRY_HEV1_NAME) {
                return;
            }
            Info frame_size = video_track->getFrameInfo();
            stsd_atom->write(static_cast<uint16_t>(frame_size.width), hev1_width_offset);
            stsd_atom->write(static_cast<uint16_t>(frame_size.height), hev1_height_offset);
            // update sps
            constexpr int64_t sps_offset = 161;
            std::vector<unsigned char> sps_buffer = video_track->getSpsBuffer();
            if (!sps_buffer.empty() && stsd_atom->getContentLength() >= (uint64_t)(sps_offset + sps_buffer.size())) {
                for (uint32_t i = 0; i < sps_buffer.size(); ++i) {
                    stsd_atom->write(sps_buffer[i], sps_offset + i);
                }
            }
        }
    }
}

int getFileCreateTime(const std::string &file_path)
{
    struct stat buf;
    int result = -1;

    result = stat(file_path.c_str(), &buf);
    if (result == 0){
        return buf.st_ctime;
    }

    return -1;
}

} // anonymous namespace

} // namespace cvi_file_recover
