// elfcpp_internal.h -- internals for elfcpp   -*- C++ -*-

// This is included by elfcpp.h, the external interface, but holds
// information which we want to keep private.

#include "elfcpp_config.h"

#ifndef ELFCPP_INTERNAL_H
#define ELFCPP_INTERNAL_H

namespace elfcpp
{

namespace internal
{

// The ELF file header.

template<int size>
struct Ehdr_data
{
  unsigned char e_ident[EI_NIDENT];
  Elf_Half e_type;
  Elf_Half e_machine;
  Elf_Word e_version;
  typename Elf_types<size>::Elf_Addr e_entry;
  typename Elf_types<size>::Elf_Off e_phoff;
  typename Elf_types<size>::Elf_Off e_shoff;
  Elf_Word e_flags;
  Elf_Half e_ehsize;
  Elf_Half e_phentsize;
  Elf_Half e_phnum;
  Elf_Half e_shentsize;
  Elf_Half e_shnum;
  Elf_Half e_shstrndx;
};

// An ELF section header.

template<int size>
struct Shdr_data
{
  Elf_Word sh_name;
  Elf_Word sh_type;
  typename Elf_types<size>::Elf_WXword sh_flags;
  typename Elf_types<size>::Elf_Addr sh_addr;
  typename Elf_types<size>::Elf_Off sh_offset;
  typename Elf_types<size>::Elf_WXword sh_size;
  Elf_Word sh_link;
  Elf_Word sh_info;
  typename Elf_types<size>::Elf_WXword sh_addralign;
  typename Elf_types<size>::Elf_WXword sh_entsize;
};

// An ELF segment header.  We use template specialization for the
// 32-bit and 64-bit versions because the fields are in a different
// order.

template<int size>
struct Phdr_data;

template<>
struct Phdr_data<32>
{
  Elf_Word p_type;
  Elf_types<32>::Elf_Off p_offset;
  Elf_types<32>::Elf_Addr p_vaddr;
  Elf_types<32>::Elf_Addr p_paddr;
  Elf_Word p_filesz;
  Elf_Word p_memsz;
  Elf_Word p_flags;
  Elf_Word p_align;
};

template<>
struct Phdr_data<64>
{
  Elf_Word p_type;
  Elf_Word p_flags;
  Elf_types<64>::Elf_Off p_offset;
  Elf_types<64>::Elf_Addr p_vaddr;
  Elf_types<64>::Elf_Addr p_paddr;
  Elf_Xword p_filesz;
  Elf_Xword p_memsz;
  Elf_Xword p_align;
};

// An ELF symbol table entry.  We use template specialization for the
// 32-bit and 64-bit versions because the fields are in a different
// order.

template<int size>
struct Sym_data;

template<>
struct Sym_data<32>
{
  Elf_Word st_name;
  Elf_types<32>::Elf_Addr st_value;
  Elf_Word st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf_Half st_shndx;
};

template<>
struct Sym_data<64>
{
  Elf_Word st_name;
  unsigned char st_info;
  unsigned char st_other;
  Elf_Half st_shndx;
  Elf_types<64>::Elf_Addr st_value;
  Elf_Xword st_size;
};

// ELF relocation table entries.

template<int size>
struct Rel_data
{
  typename Elf_types<size>::Elf_Addr r_offset;
  typename Elf_types<size>::Elf_WXword r_info;
};

template<int size>
struct Rela_data
{
  typename Elf_types<size>::Elf_Addr r_offset;
  typename Elf_types<size>::Elf_WXword r_info;
  typename Elf_types<size>::Elf_Swxword r_addend;
};

// An entry in the ELF SHT_DYNAMIC section aka PT_DYNAMIC segment.

template<int size>
struct Dyn_data
{
  typename Elf_types<size>::Elf_Swxword d_tag;
  typename Elf_types<size>::Elf_WXword d_val;
};

// An entry in a SHT_GNU_verdef section.  This structure is the same
// in 32-bit and 64-bit ELF files.

struct Verdef_data
{
  // Version number of structure (VER_DEF_*).
  Elf_Half vd_version;
  // Bit flags (VER_FLG_*).
  Elf_Half vd_flags;
  // Version index.
  Elf_Half vd_ndx;
  // Number of auxiliary Verdaux entries.
  Elf_Half vd_cnt;
  // Hash of name.
  Elf_Word vd_hash;
  // Byte offset to first Verdaux entry.
  Elf_Word vd_aux;
  // Byte offset to next Verdef entry.
  Elf_Word vd_next;
};

// An auxiliary entry in a SHT_GNU_verdef section.  This structure is
// the same in 32-bit and 64-bit ELF files.

struct Verdaux_data
{
  // Offset in string table of version name.
  Elf_Word vda_name;
  // Byte offset to next Verdaux entry.
  Elf_Word vda_next;
};

// An entry in a SHT_GNU_verneed section.  This structure is the same
// in 32-bit and 64-bit ELF files.

struct Verneed_data
{
  // Version number of structure (VER_NEED_*).
  Elf_Half vn_version;
  // Number of auxiliary Vernaux entries.
  Elf_Half vn_cnt;
  // Offset in string table of library name.
  Elf_Word vn_file;
  // Byte offset to first Vernaux entry.
  Elf_Word vn_aux;
  // Byt eoffset to next Verneed entry.
  Elf_Word vn_next;
};

// An auxiliary entry in a SHT_GNU_verneed section.  This structure is
// the same in 32-bit and 64-bit ELF files.

struct Vernaux_data
{
  // Hash of dependency name.
  Elf_Word vna_hash;
  // Bit flags (VER_FLG_*).
  Elf_Half vna_flags;
  // Version index used in SHT_GNU_versym entries.
  Elf_Half vna_other;
  // Offset in string table of version name.
  Elf_Word vna_name;
  // Byte offset to next Vernaux entry.
  Elf_Word vna_next;
};

} // End namespace internal.

} // End namespace elfcpp.

#endif // !defined(ELFCPP_INTERNAL_H)
