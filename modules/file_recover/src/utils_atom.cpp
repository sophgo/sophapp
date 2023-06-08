#include "atom.hpp"
#include <algorithm>
#include <vector>

namespace cvi_file_recover {
namespace utils {

struct AtomTableItem
{
    std::string name;
    AtomType type{AtomType::UNKNOWN};
};

const std::vector<AtomTableItem> atom_table = {
    {ATOM_FTYP_NAME, AtomType::CHILD},
    {ATOM_MDAT_NAME, AtomType::CHILD},
    {ATOM_MOOV_NAME, AtomType::PARENT},
    {ATOM_MVHD_NAME, AtomType::CHILD},
    {ATOM_TRAK_NAME, AtomType::PARENT},
    {ATOM_TKHD_NAME, AtomType::CHILD},
    {ATOM_EDTS_NAME, AtomType::PARENT},
    {ATOM_ELST_NAME, AtomType::CHILD},
    {ATOM_MDIA_NAME, AtomType::PARENT},
    {ATOM_MDHD_NAME, AtomType::CHILD},
    {ATOM_MINF_NAME, AtomType::PARENT},
    {ATOM_VMHD_NAME, AtomType::CHILD},
    {ATOM_HDLR_NAME, AtomType::CHILD},
    {ATOM_SMHD_NAME, AtomType::CHILD},
    {ATOM_GMHD_NAME, AtomType::CHILD},
    {ATOM_DINF_NAME, AtomType::CHILD},
    {ATOM_STBL_NAME, AtomType::PARENT},
    {ATOM_STSD_NAME, AtomType::CHILD},
    {ATOM_STTS_NAME, AtomType::CHILD},
    {ATOM_STSS_NAME, AtomType::CHILD},
    {ATOM_STSC_NAME, AtomType::CHILD},
    {ATOM_STSZ_NAME, AtomType::CHILD},
    {ATOM_STCO_NAME, AtomType::CHILD},
    {ATOM_SGPD_NAME, AtomType::CHILD},
    {ATOM_SBGP_NAME, AtomType::CHILD},
    {ATOM_UDTA_NAME, AtomType::CHILD},
    {ATOM_WIDE_NAME, AtomType::PARENT},
};

AtomType getAtomTypeFromName(const std::string &name)
{
    auto iterator = std::find_if(atom_table.begin(), atom_table.end(), [name](const AtomTableItem &item) {
        return (item.name == name);
    });

    return (iterator == atom_table.end()) ? AtomType::UNKNOWN : (*iterator).type;
}

bool isParentAtom(AtomType type)
{
    return (type == AtomType::PARENT);
}

} // namespace utils
} // namespace cvi_file_recover
