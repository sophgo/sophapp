#pragma once

#include <string>

namespace cvi_file_recover {

constexpr char ATOM_FTYP_NAME[] = "ftyp";
constexpr char ATOM_WIDE_NAME[] = "wide";
constexpr char ATOM_MDAT_NAME[] = "mdat";
constexpr char ATOM_MOOV_NAME[] = "moov";
constexpr char ATOM_MVHD_NAME[] = "mvhd";
constexpr char ATOM_TRAK_NAME[] = "trak";
constexpr char ATOM_TKHD_NAME[] = "tkhd";
constexpr char ATOM_EDTS_NAME[] = "edts";
constexpr char ATOM_ELST_NAME[] = "elst";
constexpr char ATOM_MDIA_NAME[] = "mdia";
constexpr char ATOM_MDHD_NAME[] = "mdhd";
constexpr char ATOM_MINF_NAME[] = "minf";
constexpr char ATOM_VMHD_NAME[] = "vmhd";
constexpr char ATOM_HDLR_NAME[] = "hdlr";
constexpr char ATOM_SMHD_NAME[] = "smhd";
constexpr char ATOM_GMHD_NAME[] = "gmhd";
constexpr char ATOM_DINF_NAME[] = "dinf";
constexpr char ATOM_STBL_NAME[] = "stbl";
constexpr char ATOM_STSD_NAME[] = "stsd";
constexpr char ATOM_STTS_NAME[] = "stts";
constexpr char ATOM_STSC_NAME[] = "stsc";
constexpr char ATOM_STSZ_NAME[] = "stsz";
constexpr char ATOM_STCO_NAME[] = "stco";
constexpr char ATOM_UDTA_NAME[] = "udta";
constexpr char ATOM_STSS_NAME[] = "stss";
constexpr char ATOM_SGPD_NAME[] = "sgpd";
constexpr char ATOM_SBGP_NAME[] = "sbgp";

enum class AtomType : int
{
    UNKNOWN,
    PARENT,
    CHILD
};

namespace utils {

AtomType getAtomTypeFromName(const std::string &name);
bool isParentAtom(AtomType type);

} // namespace utils
} // namespace cvi_file_recover
