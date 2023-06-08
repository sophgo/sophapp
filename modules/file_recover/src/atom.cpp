#include "atom.hpp"
#include <algorithm>
#include <functional>

//#define PREALLOCATE

namespace cvi_file_recover {

using std::string;
using std::function;
using std::vector;
using std::move;

namespace {

using AtomPointer = Atom::AtomPointer;
using AtomComparator = Atom::AtomComparator;

constexpr int ATOM_NAME_SIZE = 4;
constexpr int ATOM_HEADER_SIZE = 8;
constexpr int ATOM_CONTENT_OFFSET = 8;
static bool PREALL_OCATE = true; // PREALL OCATE
static int File_Type = 0;

inline AtomComparator atomPointerNameComparator(const string &name)
{
    return [name](const AtomPointer& atom) {
        return atom->getName() == name;
    };
}

inline AtomComparator atomPointerComparator(const AtomPointer& target_atom)
{
    return [target_atom](const AtomPointer& atom) {
        return atom.get() == target_atom.get();
    };
}

} // anonymous namespace

Atom::AtomPointer Atom::createAtomPointer()
{
    return std::make_shared<Atom>();
}

void Atom::preallocatestate(bool PreallocFlage)
{
    PREALL_OCATE = PreallocFlage;
}

bool Atom::getpreallocatestate()
{
    return PREALL_OCATE;
}

void Atom::Filetype(int FileFlage)
{
    File_Type = FileFlage;
}

int Atom::parseHeader(File &file)
{
    start_pos = file.pos();
    length = file.read<uint32_t>();
    name = file.readString(ATOM_NAME_SIZE);
    type = utils::getAtomTypeFromName(name);
    // length 1 means use 64 bits to save length, or use 32 bits
    if (length == 1) {
        length = file.read<uint64_t>() - 8;
        start_pos += ATOM_HEADER_SIZE;
    } else {
        if (PREALL_OCATE == false) {
            if (length == 0) {
                length = file.size() - start_pos;
            } else if (length > static_cast<uint64_t>(file.size() - start_pos)) {
                length = file.size() - start_pos;
            }
        } else {
            if ((length == 0) || (length > static_cast<uint64_t>(file.size() - start_pos))) {
                if (1 == File_Type) {
                    length = file.size() - start_pos;
                } else {
                    length = -1;
                    return -1;
                }
            }
        }
    }

    return 0;
/* #ifndef PREALLOCATE
    else if (length == 0) {
        length = file.size() - start_pos;
    } else if (length > static_cast<uint64_t>(file.size() - start_pos)) {
        length = file.size() - start_pos;
    }
#else
    else if ((length == 0) || (length > static_cast<uint64_t>(file.size() - start_pos))) {
        length = -1;
    }
#endif */
}

void Atom::parseContent(File &file)
{
    // check content is enough
    if (length <= ATOM_HEADER_SIZE) {
        return;
    }
    // skip unknown type content
    if (type == AtomType::UNKNOWN) {
        file.offset(getEstimateContentLength());
        return;
    }
    // skip mdat, mdat's content will be parsed by mdat parser
    if (name == ATOM_MDAT_NAME) {
        file.offset(getEstimateContentLength());
        return;
    }

    if (utils::isParentAtom(type)) {
        while (file.pos() < static_cast<int64_t>(start_pos + length)) {
            AtomPointer atom = Atom::createAtomPointer();
            if (atom->parse(file) != 0) {
                removeChildren();
                file.seek(getContentStartPos());
                content = file.read(getEstimateContentLength());
                break;
            }
            appendChild(move(atom));
        }
    } else {
        content = file.read(getEstimateContentLength());
    }
}

int Atom::parse(File &file)
{
    if (parseHeader(file) != 0) {
        return -1;
    }
    if (!isValid()) {
        return -1;
    }
    parseContent(file);

    return 0;
}

void Atom::dump(int depth) const
{
    if (!isRoot()) {
        string atom_indent = "";
        vector<string> indents(depth, "-");
        for (const string &indent : indents) {
            atom_indent += indent;
        }
    }

    for (auto child : children) {
        child->dump(depth + 1);
    }
}

void Atom::save(File& file)
{
    if ((!isRoot() && isValid())) {
        if (utils::getAtomTypeFromName(name) == AtomType::UNKNOWN) {
        } else {
            if (length > UINT32_MAX) {
                file.write<uint32_t>(1);
                file.writeString(name);
                file.write<uint64_t>(length);
            } else {
                file.write<uint32_t>(length);
                file.writeString(name);
            }

            if (!content.empty()) {
                file.write<unsigned char>(content);
            }

            if (save_done_handler) {
                save_done_handler(file);
            }
        }
    }

    for (auto child : children) {
        child->save(file);
    }
}

uint64_t Atom::getLength() const
{
    return length;
}

void Atom::setLength(uint64_t setlength)
{
    length = setlength;
}

uint64_t Atom::getEstimateContentLength() const
{
    return length - ATOM_HEADER_SIZE;
}

uint64_t Atom::getContentLength() const
{
    return content.size();
}

uint64_t Atom::getStartPos() const
{
    return start_pos;
}

void Atom::setStartPos(uint64_t pos)
{
    start_pos = pos;
}

uint64_t Atom::getContentStartPos() const
{
    return start_pos + ATOM_CONTENT_OFFSET;
}

uint64_t Atom::getEndPos() const
{
    return start_pos + length;
}

string Atom::getName() const
{
    return name;
}

bool Atom::isValid() const
{
    return (start_pos >= 0) && (!name.empty()) && (length > 0);
}

bool Atom::isRoot() const
{
    return parent.expired();
}

void Atom::resize(size_t size)
{
    int64_t length_diff = size - content.size();
    updateLengthDiff(length_diff);
    content.resize(size);
}

void Atom::setParent(const AtomPointer& parent)
{
    this->parent = parent;
}

void Atom::setParent(AtomPointer&& parent)
{
    this->parent = move(parent);
}

void Atom::addChild(const AtomPointer& atom)
{
    preprocessChild(atom);
    appendChild(atom);
}

void Atom::addChild(AtomPointer&& atom)
{
    preprocessChild(atom);
    appendChild(atom);
}

void Atom::preprocessChild(const AtomPointer& atom)
{
    updateLengthDiff(atom->getLength());
    if (!children.empty()) {
        atom->setStartPos(children.back()->getEndPos());
    } else {
        atom->setStartPos(0);
    }
}

void Atom::appendChild(const AtomPointer& atom)
{
    atom->setParent(getPointer());
    children.push_back(atom);
}

void Atom::appendChild(AtomPointer&& atom)
{
    atom->setParent(getPointer());
    children.emplace_back(move(atom));
}

void Atom::removeChildren()
{
    children.clear();
}

Atom::AtomPointer Atom::getPointer()
{
    return shared_from_this();
}

Atom::AtomPointer Atom::getAtomByName(const std::string& name)
{
    if (name == this->name) {
        return getPointer();
    }

    Atom::AtomPointer atom = nullptr;
    for (auto child : children) {
        atom = child->getAtomByName(name);
        if (atom) {
            return atom;
        }
    }

    return nullptr;
}

Atom::AtomPointer Atom::getFirstChildByName(const string& name) const
{
    auto iterator = std::find_if(children.begin(), children.end(), atomPointerNameComparator(name));
    return (iterator == children.end()) ? nullptr : *iterator;
}

std::vector<Atom::AtomPointer> Atom::getAtomsByName(const std::string& name)
{
    vector<AtomPointer> results;
    if (name == this->name) {
        results.push_back(getPointer());
    }

    for (auto child : children) {
        vector<AtomPointer> &&temp_results = child->getAtomsByName(name);
        results.insert(results.end(), temp_results.begin(), temp_results.end());
    }

    return results;
}

void Atom::removeAtom(const AtomPointer& atom)
{
    if (!atom) {
        return;
    }

    removeAtomWithComparator(atomPointerComparator(atom));
}

void Atom::removeAtomsByName(const std::string& name)
{
    removeAtomWithComparator(atomPointerNameComparator(name));
}

void Atom::removeAtomWithComparator(const AtomComparator& comparator)
{
    for (auto iterator = children.begin(); iterator != children.end();) {
        const AtomPointer &child = (*iterator);
        if (comparator(child)) {
            int64_t length = child->getLength();
            updateLengthDiff(-length);
            iterator = children.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto child : children) {
        child->removeAtomWithComparator(comparator);
    }
}

void Atom::updateLengthDiff(int64_t diff)
{
    if (isRoot()) {
        return;
    }

    length += diff;
    AtomPointer shared_parent = parent.lock();
    if (shared_parent) {
        shared_parent->updateLengthDiff(diff);
    }
}

void Atom::setSaveDoneHandler(const SaveDoneHandler& handler)
{
    save_done_handler = handler;
}

} // namespace cvi_file_recover
