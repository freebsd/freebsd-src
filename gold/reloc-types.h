// reloc-types.h -- ELF relocation templates for gold  -*- C++ -*-

// This header files defines a few convenient templated types for use
// when handling ELF relocations.

#ifndef GOLD_RELOC_TYPES_H
#define GOLD_RELOC_TYPES_H

#include "elfcpp.h"

namespace gold
{

// Pick the ELF relocation accessor class and the size based on
// SH_TYPE, which is either elfcpp::SHT_REL or elfcpp::SHT_RELA.

template<int sh_type, int size, bool big_endian>
struct Reloc_types;

template<int size, bool big_endian>
struct Reloc_types<elfcpp::SHT_REL, size, big_endian>
{
  typedef typename elfcpp::Rel<size, big_endian> Reloc;
  static const int reloc_size = elfcpp::Elf_sizes<size>::rel_size;
};

template<int size, bool big_endian>
struct Reloc_types<elfcpp::SHT_RELA, size, big_endian>
{
  typedef typename elfcpp::Rela<size, big_endian> Reloc;
  static const int reloc_size = elfcpp::Elf_sizes<size>::rela_size;
};

}; // End namespace gold.

#endif // !defined(GOLD_RELOC_TYPE_SH)
