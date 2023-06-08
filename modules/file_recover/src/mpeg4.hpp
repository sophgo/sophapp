#pragma once

#include "container.hpp"
#include <string>
#include "atom.hpp"
#include "track.hpp"

namespace cvi_file_recover {

class Mpeg4 final : public Container
{
public:
    using AtomPointer = Atom::AtomPointer;
    using AtomWeakPointer = Atom::AtomWeakPointer;
    using TrackPointer = Track::TrackPointer;

    Mpeg4();

    virtual void close() override;
    virtual int parse() override;
    virtual int check() override;
    virtual void dump() override;
    virtual int recover(const std::string &device_model, bool has_create_time, bool PreallocFlage, int file_type) override;
    virtual void save(const std::string &file_path) override;
    void saveAtomContentFromFile(File& output_file, const AtomWeakPointer& atom);
    void saveAtomByName(const std::string &file_path, const std::string &atom_name);
    AtomPointer getAtomByName(const std::string &name);

private:
    void reset();
    void updateMvhd(const TrackPointer& track, int create_time);
    void updateTrack(const TrackPointer& track, int create_time);
    void updateUdta(const std::string &device_model);
    void preallocatestate(bool PreallocFlage);

private:
    AtomPointer root_atom;
};

} // namespace cvi_file_recover
