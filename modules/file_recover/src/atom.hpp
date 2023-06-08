#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include "file.hpp"
#include "utils_atom.hpp"
#include "utils_binary.hpp"

namespace cvi_file_recover {

class Atom : public std::enable_shared_from_this<Atom>
{
public:
    using AtomPointer = std::shared_ptr<Atom>;
    using AtomWeakPointer = std::weak_ptr<Atom>;
    using AtomComparator = std::function<bool(const AtomPointer&)>;
    using SaveDoneHandler = std::function<void(File& file)>;

    Atom() = default;
    virtual ~Atom() = default;

    // not copyable
    Atom(const Atom&) = delete;
    Atom& operator=(const Atom&) = delete;

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    T read(uint64_t offset)
    {
        assert(content.size() >= (offset + sizeof(T)));
        return utils::read<T>(content.begin() + offset);
    }

    template <typename T>
    T read(uint64_t offset, size_t size)
    {
        assert(content.size() >= (offset + size));
        auto start = content.begin() + offset;
        auto end = start + size;

        return T(start, end);
    }

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    void write(T value, uint64_t offset)
    {
        assert(content.size() >= (offset + sizeof(T)));
        return utils::write<T>(&content[offset], value);
    }

    void write(char* value, size_t size, uint64_t offset)
    {
        assert(content.size() >= (offset + size));
        return utils::write(&content[offset], value, size);
    }

    static AtomPointer createAtomPointer();

    int parse(File &file);
    void dump(int depth = 0) const;
    void save(File& file);
    uint64_t getLength() const;
    uint64_t getEstimateContentLength() const;
    uint64_t getContentLength() const;
    uint64_t getStartPos() const;
    void setStartPos(uint64_t pos);
    uint64_t getContentStartPos() const;
    uint64_t getEndPos() const;
    std::string getName() const;
    bool isValid() const;
    bool isRoot() const;
    void resize(size_t size);
    void addChild(const AtomPointer& atom);
    void addChild(AtomPointer&& atom);
    void removeChildren();
    AtomPointer getPointer();
    AtomPointer getAtomByName(const std::string& name);
    AtomPointer getFirstChildByName(const std::string& name) const;
    std::vector<AtomPointer> getAtomsByName(const std::string& name);
    void removeAtom(const AtomPointer& atom);
    void removeAtomsByName(const std::string& name);
    void setSaveDoneHandler(const SaveDoneHandler& handler);
    void preallocatestate(bool PreallocFlage);
    void Filetype(int FileFlage);
    void setLength(uint64_t setlength);
    bool getpreallocatestate();

private:
    int parseHeader(File &file);
    void parseContent(File &file);
    void setParent(const AtomPointer& parent);
    void setParent(AtomPointer&& parent);
    void preprocessChild(const AtomPointer& atom);
    void appendChild(const AtomPointer& atom);
    void appendChild(AtomPointer&& atom);
    void removeAtomWithComparator(const AtomComparator& comparator);
    void updateLengthDiff(int64_t diff);

private:
    int64_t start_pos{0};
    uint64_t length{0};
    AtomType type{AtomType::UNKNOWN};
    std::string name{""};
    std::vector<unsigned char> content;
    AtomWeakPointer parent;
    std::list<AtomPointer> children;
    SaveDoneHandler save_done_handler;
};

} // namespace cvi_file_recover
