/* SPARC-specific support for 64-bit ELF
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "opcode/sparc.h"

/* This is defined if one wants to build upward compatible binaries
   with the original sparc64-elf toolchain.  The support is kept in for
   now but is turned off by default.  dje 970930  */
/*#define SPARC64_OLD_RELOCS*/

#include "elf/sparc.h"

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value.  */
#define MINUS_ONE (~ (bfd_vma) 0)

static struct bfd_link_hash_table * sparc64_elf_bfd_link_hash_table_create
  PARAMS ((bfd *));
static bfd_reloc_status_type init_insn_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *,
	   bfd *, bfd_vma *, bfd_vma *));
static reloc_howto_type *sparc64_elf_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void sparc64_elf_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static void sparc64_elf_build_plt
  PARAMS ((bfd *, unsigned char *, int));
static bfd_vma sparc64_elf_plt_entry_offset
  PARAMS ((bfd_vma));
static bfd_vma sparc64_elf_plt_ptr_offset
  PARAMS ((bfd_vma, bfd_vma));

static boolean sparc64_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *sec,
	   const Elf_Internal_Rela *));
static boolean sparc64_elf_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean sparc64_elf_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static int sparc64_elf_get_symbol_type
  PARAMS (( Elf_Internal_Sym *, int));
static boolean sparc64_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static boolean sparc64_elf_output_arch_syms
  PARAMS ((bfd *, struct bfd_link_info *, PTR,
	   boolean (*) (PTR, const char *, Elf_Internal_Sym *, asection *)));
static void sparc64_elf_symbol_processing
  PARAMS ((bfd *, asymbol *));

static boolean sparc64_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));

static const char *sparc64_elf_print_symbol_all
  PARAMS ((bfd *, PTR, asymbol *));
static boolean sparc64_elf_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, boolean *));
static boolean sparc64_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean sparc64_elf_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static boolean sparc64_elf_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean sparc64_elf_object_p PARAMS ((bfd *));
static long sparc64_elf_get_reloc_upper_bound PARAMS ((bfd *, asection *));
static long sparc64_elf_get_dynamic_reloc_upper_bound PARAMS ((bfd *));
static boolean sparc64_elf_slurp_one_reloc_table
  PARAMS ((bfd *, asection *, Elf_Internal_Shdr *, asymbol **, boolean));
static boolean sparc64_elf_slurp_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, boolean));
static long sparc64_elf_canonicalize_dynamic_reloc
  PARAMS ((bfd *, arelent **, asymbol **));
static void sparc64_elf_write_relocs PARAMS ((bfd *, asection *, PTR));
static enum elf_reloc_type_class sparc64_elf_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));

/* The relocation "howto" table.  */

static bfd_reloc_status_type sparc_elf_notsup_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_wdisp16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_hix22_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type sparc_elf_lox10_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));

static reloc_howto_type sparc64_elf_howto_table[] =
{
  HOWTO(R_SPARC_NONE,      0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_NONE",    false,0,0x00000000,true),
  HOWTO(R_SPARC_8,         0,0, 8,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_8",       false,0,0x000000ff,true),
  HOWTO(R_SPARC_16,        0,1,16,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_16",      false,0,0x0000ffff,true),
  HOWTO(R_SPARC_32,        0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_32",      false,0,0xffffffff,true),
  HOWTO(R_SPARC_DISP8,     0,0, 8,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP8",   false,0,0x000000ff,true),
  HOWTO(R_SPARC_DISP16,    0,1,16,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP16",  false,0,0x0000ffff,true),
  HOWTO(R_SPARC_DISP32,    0,2,32,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP32",  false,0,0xffffffff,true),
  HOWTO(R_SPARC_WDISP30,   2,2,30,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP30", false,0,0x3fffffff,true),
  HOWTO(R_SPARC_WDISP22,   2,2,22,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP22", false,0,0x003fffff,true),
  HOWTO(R_SPARC_HI22,     10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HI22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_22,        0,2,22,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_22",      false,0,0x003fffff,true),
  HOWTO(R_SPARC_13,        0,2,13,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_13",      false,0,0x00001fff,true),
  HOWTO(R_SPARC_LO10,      0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LO10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT10,     0,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT10",   false,0,0x000003ff,true),
  HOWTO(R_SPARC_GOT13,     0,2,13,false,0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_GOT13",   false,0,0x00001fff,true),
  HOWTO(R_SPARC_GOT22,    10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GOT22",   false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC10,      0,2,10,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_PC10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC22,     10,2,22,true, 0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PC22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WPLT30,    2,2,30,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WPLT30",  false,0,0x3fffffff,true),
  HOWTO(R_SPARC_COPY,      0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_COPY",    false,0,0x00000000,true),
  HOWTO(R_SPARC_GLOB_DAT,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_GLOB_DAT",false,0,0x00000000,true),
  HOWTO(R_SPARC_JMP_SLOT,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_JMP_SLOT",false,0,0x00000000,true),
  HOWTO(R_SPARC_RELATIVE,  0,0,00,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_RELATIVE",false,0,0x00000000,true),
  HOWTO(R_SPARC_UA32,      0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA32",    false,0,0xffffffff,true),
#ifndef SPARC64_OLD_RELOCS
  HOWTO(R_SPARC_PLT32,     0,2,32,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PLT32",   false,0,0xffffffff,true),
  /* These aren't implemented yet.  */
  HOWTO(R_SPARC_HIPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsup_reloc, "R_SPARC_HIPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_LOPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsup_reloc, "R_SPARC_LOPLT10",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT32,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsup_reloc, "R_SPARC_PCPLT32",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT22,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsup_reloc, "R_SPARC_PCPLT22",  false,0,0x00000000,true),
  HOWTO(R_SPARC_PCPLT10,   0,0,00,false,0,complain_overflow_dont,    sparc_elf_notsup_reloc, "R_SPARC_PCPLT10",  false,0,0x00000000,true),
#endif
  HOWTO(R_SPARC_10,        0,2,10,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_10",      false,0,0x000003ff,true),
  HOWTO(R_SPARC_11,        0,2,11,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_11",      false,0,0x000007ff,true),
  HOWTO(R_SPARC_64,        0,4,64,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_64",      false,0,MINUS_ONE, true),
  HOWTO(R_SPARC_OLO10,     0,2,13,false,0,complain_overflow_signed,  sparc_elf_notsup_reloc, "R_SPARC_OLO10",   false,0,0x00001fff,true),
  HOWTO(R_SPARC_HH22,     42,2,22,false,0,complain_overflow_unsigned,bfd_elf_generic_reloc,  "R_SPARC_HH22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_HM10,     32,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_HM10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_LM22,     10,2,22,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_LM22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC_HH22,  42,2,22,true, 0,complain_overflow_unsigned,bfd_elf_generic_reloc,  "R_SPARC_PC_HH22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_PC_HM10,  32,2,10,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_PC_HM10",    false,0,0x000003ff,true),
  HOWTO(R_SPARC_PC_LM22,  10,2,22,true, 0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_PC_LM22",    false,0,0x003fffff,true),
  HOWTO(R_SPARC_WDISP16,   2,2,16,true, 0,complain_overflow_signed,  sparc_elf_wdisp16_reloc,"R_SPARC_WDISP16", false,0,0x00000000,true),
  HOWTO(R_SPARC_WDISP19,   2,2,19,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_WDISP19", false,0,0x0007ffff,true),
  HOWTO(R_SPARC_UNUSED_42, 0,0, 0,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_UNUSED_42",false,0,0x00000000,true),
  HOWTO(R_SPARC_7,         0,2, 7,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_7",       false,0,0x0000007f,true),
  HOWTO(R_SPARC_5,         0,2, 5,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_5",       false,0,0x0000001f,true),
  HOWTO(R_SPARC_6,         0,2, 6,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_6",       false,0,0x0000003f,true),
  HOWTO(R_SPARC_DISP64,    0,4,64,true, 0,complain_overflow_signed,  bfd_elf_generic_reloc,  "R_SPARC_DISP64",  false,0,MINUS_ONE, true),
  HOWTO(R_SPARC_PLT64,     0,4,64,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_PLT64",   false,0,MINUS_ONE, true),
  HOWTO(R_SPARC_HIX22,     0,4, 0,false,0,complain_overflow_bitfield,sparc_elf_hix22_reloc,  "R_SPARC_HIX22",   false,0,MINUS_ONE, false),
  HOWTO(R_SPARC_LOX10,     0,4, 0,false,0,complain_overflow_dont,    sparc_elf_lox10_reloc,  "R_SPARC_LOX10",   false,0,MINUS_ONE, false),
  HOWTO(R_SPARC_H44,      22,2,22,false,0,complain_overflow_unsigned,bfd_elf_generic_reloc,  "R_SPARC_H44",     false,0,0x003fffff,false),
  HOWTO(R_SPARC_M44,      12,2,10,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_M44",     false,0,0x000003ff,false),
  HOWTO(R_SPARC_L44,       0,2,13,false,0,complain_overflow_dont,    bfd_elf_generic_reloc,  "R_SPARC_L44",     false,0,0x00000fff,false),
  HOWTO(R_SPARC_REGISTER,  0,4, 0,false,0,complain_overflow_bitfield,sparc_elf_notsup_reloc, "R_SPARC_REGISTER",false,0,MINUS_ONE, false),
  HOWTO(R_SPARC_UA64,        0,4,64,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA64",      false,0,MINUS_ONE, true),
  HOWTO(R_SPARC_UA16,        0,1,16,false,0,complain_overflow_bitfield,bfd_elf_generic_reloc,  "R_SPARC_UA16",      false,0,0x0000ffff,true)
};

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct elf_reloc_map sparc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SPARC_NONE, },
  { BFD_RELOC_16, R_SPARC_16, },
  { BFD_RELOC_16_PCREL, R_SPARC_DISP16 },
  { BFD_RELOC_8, R_SPARC_8 },
  { BFD_RELOC_8_PCREL, R_SPARC_DISP8 },
  { BFD_RELOC_CTOR, R_SPARC_64 },
  { BFD_RELOC_32, R_SPARC_32 },
  { BFD_RELOC_32_PCREL, R_SPARC_DISP32 },
  { BFD_RELOC_HI22, R_SPARC_HI22 },
  { BFD_RELOC_LO10, R_SPARC_LO10, },
  { BFD_RELOC_32_PCREL_S2, R_SPARC_WDISP30 },
  { BFD_RELOC_64_PCREL, R_SPARC_DISP64 },
  { BFD_RELOC_SPARC22, R_SPARC_22 },
  { BFD_RELOC_SPARC13, R_SPARC_13 },
  { BFD_RELOC_SPARC_GOT10, R_SPARC_GOT10 },
  { BFD_RELOC_SPARC_GOT13, R_SPARC_GOT13 },
  { BFD_RELOC_SPARC_GOT22, R_SPARC_GOT22 },
  { BFD_RELOC_SPARC_PC10, R_SPARC_PC10 },
  { BFD_RELOC_SPARC_PC22, R_SPARC_PC22 },
  { BFD_RELOC_SPARC_WPLT30, R_SPARC_WPLT30 },
  { BFD_RELOC_SPARC_COPY, R_SPARC_COPY },
  { BFD_RELOC_SPARC_GLOB_DAT, R_SPARC_GLOB_DAT },
  { BFD_RELOC_SPARC_JMP_SLOT, R_SPARC_JMP_SLOT },
  { BFD_RELOC_SPARC_RELATIVE, R_SPARC_RELATIVE },
  { BFD_RELOC_SPARC_WDISP22, R_SPARC_WDISP22 },
  { BFD_RELOC_SPARC_UA16, R_SPARC_UA16 },
  { BFD_RELOC_SPARC_UA32, R_SPARC_UA32 },
  { BFD_RELOC_SPARC_UA64, R_SPARC_UA64 },
  { BFD_RELOC_SPARC_10, R_SPARC_10 },
  { BFD_RELOC_SPARC_11, R_SPARC_11 },
  { BFD_RELOC_SPARC_64, R_SPARC_64 },
  { BFD_RELOC_SPARC_OLO10, R_SPARC_OLO10 },
  { BFD_RELOC_SPARC_HH22, R_SPARC_HH22 },
  { BFD_RELOC_SPARC_HM10, R_SPARC_HM10 },
  { BFD_RELOC_SPARC_LM22, R_SPARC_LM22 },
  { BFD_RELOC_SPARC_PC_HH22, R_SPARC_PC_HH22 },
  { BFD_RELOC_SPARC_PC_HM10, R_SPARC_PC_HM10 },
  { BFD_RELOC_SPARC_PC_LM22, R_SPARC_PC_LM22 },
  { BFD_RELOC_SPARC_WDISP16, R_SPARC_WDISP16 },
  { BFD_RELOC_SPARC_WDISP19, R_SPARC_WDISP19 },
  { BFD_RELOC_SPARC_7, R_SPARC_7 },
  { BFD_RELOC_SPARC_5, R_SPARC_5 },
  { BFD_RELOC_SPARC_6, R_SPARC_6 },
  { BFD_RELOC_SPARC_DISP64, R_SPARC_DISP64 },
#ifndef SPARC64_OLD_RELOCS
  { BFD_RELOC_SPARC_PLT32, R_SPARC_PLT32 },
#endif
  { BFD_RELOC_SPARC_PLT64, R_SPARC_PLT64 },
  { BFD_RELOC_SPARC_HIX22, R_SPARC_HIX22 },
  { BFD_RELOC_SPARC_LOX10, R_SPARC_LOX10 },
  { BFD_RELOC_SPARC_H44, R_SPARC_H44 },
  { BFD_RELOC_SPARC_M44, R_SPARC_M44 },
  { BFD_RELOC_SPARC_L44, R_SPARC_L44 },
  { BFD_RELOC_SPARC_REGISTER, R_SPARC_REGISTER }
};

static reloc_howto_type *
sparc64_elf_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;
  for (i = 0; i < sizeof (sparc_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (sparc_reloc_map[i].bfd_reloc_val == code)
	return &sparc64_elf_howto_table[(int) sparc_reloc_map[i].elf_reloc_val];
    }
  return 0;
}

static void
sparc64_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf64_Internal_Rela *dst;
{
  BFD_ASSERT (ELF64_R_TYPE_ID (dst->r_info) < (unsigned int) R_SPARC_max_std);
  cache_ptr->howto = &sparc64_elf_howto_table[ELF64_R_TYPE_ID (dst->r_info)];
}

/* Due to the way how we handle R_SPARC_OLO10, each entry in a SHT_RELA
   section can represent up to two relocs, we must tell the user to allocate
   more space.  */

static long
sparc64_elf_get_reloc_upper_bound (abfd, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
{
  return (sec->reloc_count * 2 + 1) * sizeof (arelent *);
}

static long
sparc64_elf_get_dynamic_reloc_upper_bound (abfd)
     bfd *abfd;
{
  return _bfd_elf_get_dynamic_reloc_upper_bound (abfd) * 2;
}

/* Read  relocations for ASECT from REL_HDR.  There are RELOC_COUNT of
   them.  We cannot use generic elf routines for this,  because R_SPARC_OLO10
   has secondary addend in ELF64_R_TYPE_DATA.  We handle it as two relocations
   for the same location,  R_SPARC_LO10 and R_SPARC_13.  */

static boolean
sparc64_elf_slurp_one_reloc_table (abfd, asect, rel_hdr, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     Elf_Internal_Shdr *rel_hdr;
     asymbol **symbols;
     boolean dynamic;
{
  PTR allocated = NULL;
  bfd_byte *native_relocs;
  arelent *relent;
  unsigned int i;
  int entsize;
  bfd_size_type count;
  arelent *relents;

  allocated = (PTR) bfd_malloc (rel_hdr->sh_size);
  if (allocated == NULL)
    goto error_return;

  if (bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0
      || bfd_bread (allocated, rel_hdr->sh_size, abfd) != rel_hdr->sh_size)
    goto error_return;

  native_relocs = (bfd_byte *) allocated;

  relents = asect->relocation + asect->reloc_count;

  entsize = rel_hdr->sh_entsize;
  BFD_ASSERT (entsize == sizeof (Elf64_External_Rela));

  count = rel_hdr->sh_size / entsize;

  for (i = 0, relent = relents; i < count;
       i++, relent++, native_relocs += entsize)
    {
      Elf_Internal_Rela rela;

      bfd_elf64_swap_reloca_in (abfd, (Elf64_External_Rela *) native_relocs, &rela);

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a normal BFD reloc is always section relative,
	 and the address of a dynamic reloc is absolute..  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0 || dynamic)
	relent->address = rela.r_offset;
      else
	relent->address = rela.r_offset - asect->vma;

      if (ELF64_R_SYM (rela.r_info) == 0)
	relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      else
	{
	  asymbol **ps, *s;

	  ps = symbols + ELF64_R_SYM (rela.r_info) - 1;
	  s = *ps;

	  /* Canonicalize ELF section symbols.  FIXME: Why?  */
	  if ((s->flags & BSF_SECTION_SYM) == 0)
	    relent->sym_ptr_ptr = ps;
	  else
	    relent->sym_ptr_ptr = s->section->symbol_ptr_ptr;
	}

      relent->addend = rela.r_addend;

      BFD_ASSERT (ELF64_R_TYPE_ID (rela.r_info) < (unsigned int) R_SPARC_max_std);
      if (ELF64_R_TYPE_ID (rela.r_info) == R_SPARC_OLO10)
	{
	  relent->howto = &sparc64_elf_howto_table[R_SPARC_LO10];
	  relent[1].address = relent->address;
	  relent++;
	  relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	  relent->addend = ELF64_R_TYPE_DATA (rela.r_info);
	  relent->howto = &sparc64_elf_howto_table[R_SPARC_13];
	}
      else
	relent->howto = &sparc64_elf_howto_table[ELF64_R_TYPE_ID (rela.r_info)];
    }

  asect->reloc_count += relent - relents;

  if (allocated != NULL)
    free (allocated);

  return true;

 error_return:
  if (allocated != NULL)
    free (allocated);
  return false;
}

/* Read in and swap the external relocs.  */

static boolean
sparc64_elf_slurp_reloc_table (abfd, asect, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     boolean dynamic;
{
  struct bfd_elf_section_data * const d = elf_section_data (asect);
  Elf_Internal_Shdr *rel_hdr;
  Elf_Internal_Shdr *rel_hdr2;
  bfd_size_type amt;

  if (asect->relocation != NULL)
    return true;

  if (! dynamic)
    {
      if ((asect->flags & SEC_RELOC) == 0
	  || asect->reloc_count == 0)
	return true;

      rel_hdr = &d->rel_hdr;
      rel_hdr2 = d->rel_hdr2;

      BFD_ASSERT (asect->rel_filepos == rel_hdr->sh_offset
		  || (rel_hdr2 && asect->rel_filepos == rel_hdr2->sh_offset));
    }
  else
    {
      /* Note that ASECT->RELOC_COUNT tends not to be accurate in this
	 case because relocations against this section may use the
	 dynamic symbol table, and in that case bfd_section_from_shdr
	 in elf.c does not update the RELOC_COUNT.  */
      if (asect->_raw_size == 0)
	return true;

      rel_hdr = &d->this_hdr;
      asect->reloc_count = NUM_SHDR_ENTRIES (rel_hdr);
      rel_hdr2 = NULL;
    }

  amt = asect->reloc_count;
  amt *= 2 * sizeof (arelent);
  asect->relocation = (arelent *) bfd_alloc (abfd, amt);
  if (asect->relocation == NULL)
    return false;

  /* The sparc64_elf_slurp_one_reloc_table routine increments reloc_count.  */
  asect->reloc_count = 0;

  if (!sparc64_elf_slurp_one_reloc_table (abfd, asect, rel_hdr, symbols,
					  dynamic))
    return false;

  if (rel_hdr2
      && !sparc64_elf_slurp_one_reloc_table (abfd, asect, rel_hdr2, symbols,
					     dynamic))
    return false;

  return true;
}

/* Canonicalize the dynamic relocation entries.  Note that we return
   the dynamic relocations as a single block, although they are
   actually associated with particular sections; the interface, which
   was designed for SunOS style shared libraries, expects that there
   is only one set of dynamic relocs.  Any section that was actually
   installed in the BFD, and has type SHT_REL or SHT_RELA, and uses
   the dynamic symbol table, is considered to be a dynamic reloc
   section.  */

static long
sparc64_elf_canonicalize_dynamic_reloc (abfd, storage, syms)
     bfd *abfd;
     arelent **storage;
     asymbol **syms;
{
  asection *s;
  long ret;

  if (elf_dynsymtab (abfd) == 0)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  ret = 0;
  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if (elf_section_data (s)->this_hdr.sh_link == elf_dynsymtab (abfd)
	  && (elf_section_data (s)->this_hdr.sh_type == SHT_RELA))
	{
	  arelent *p;
	  long count, i;

	  if (! sparc64_elf_slurp_reloc_table (abfd, s, syms, true))
	    return -1;
	  count = s->reloc_count;
	  p = s->relocation;
	  for (i = 0; i < count; i++)
	    *storage++ = p++;
	  ret += count;
	}
    }

  *storage = NULL;

  return ret;
}

/* Write out the relocs.  */

static void
sparc64_elf_write_relocs (abfd, sec, data)
     bfd *abfd;
     asection *sec;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  Elf_Internal_Shdr *rela_hdr;
  Elf64_External_Rela *outbound_relocas, *src_rela;
  unsigned int idx, count;
  asymbol *last_sym = 0;
  int last_sym_idx = 0;

  /* If we have already failed, don't do anything.  */
  if (*failedp)
    return;

  if ((sec->flags & SEC_RELOC) == 0)
    return;

  /* The linker backend writes the relocs out itself, and sets the
     reloc_count field to zero to inhibit writing them here.  Also,
     sometimes the SEC_RELOC flag gets set even when there aren't any
     relocs.  */
  if (sec->reloc_count == 0)
    return;

  /* We can combine two relocs that refer to the same address
     into R_SPARC_OLO10 if first one is R_SPARC_LO10 and the
     latter is R_SPARC_13 with no associated symbol.  */
  count = 0;
  for (idx = 0; idx < sec->reloc_count; idx++)
    {
      bfd_vma addr;

      ++count;

      addr = sec->orelocation[idx]->address;
      if (sec->orelocation[idx]->howto->type == R_SPARC_LO10
	  && idx < sec->reloc_count - 1)
	{
	  arelent *r = sec->orelocation[idx + 1];

	  if (r->howto->type == R_SPARC_13
	      && r->address == addr
	      && bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      && (*r->sym_ptr_ptr)->value == 0)
	    ++idx;
	}
    }

  rela_hdr = &elf_section_data (sec)->rel_hdr;

  rela_hdr->sh_size = rela_hdr->sh_entsize * count;
  rela_hdr->contents = (PTR) bfd_alloc (abfd, rela_hdr->sh_size);
  if (rela_hdr->contents == NULL)
    {
      *failedp = true;
      return;
    }

  /* Figure out whether the relocations are RELA or REL relocations.  */
  if (rela_hdr->sh_type != SHT_RELA)
    abort ();

  /* orelocation has the data, reloc_count has the count...  */
  outbound_relocas = (Elf64_External_Rela *) rela_hdr->contents;
  src_rela = outbound_relocas;

  for (idx = 0; idx < sec->reloc_count; idx++)
    {
      Elf_Internal_Rela dst_rela;
      arelent *ptr;
      asymbol *sym;
      int n;

      ptr = sec->orelocation[idx];

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a BFD reloc is always section relative.  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	dst_rela.r_offset = ptr->address;
      else
	dst_rela.r_offset = ptr->address + sec->vma;

      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
      else if (bfd_is_abs_section (sym->section) && sym->value == 0)
	n = STN_UNDEF;
      else
	{
	  last_sym = sym;
	  n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	  if (n < 0)
	    {
	      *failedp = true;
	      return;
	    }
	  last_sym_idx = n;
	}

      if ((*ptr->sym_ptr_ptr)->the_bfd != NULL
	  && (*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      if (ptr->howto->type == R_SPARC_LO10
	  && idx < sec->reloc_count - 1)
	{
	  arelent *r = sec->orelocation[idx + 1];

	  if (r->howto->type == R_SPARC_13
	      && r->address == ptr->address
	      && bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      && (*r->sym_ptr_ptr)->value == 0)
	    {
	      idx++;
	      dst_rela.r_info
		= ELF64_R_INFO (n, ELF64_R_TYPE_INFO (r->addend,
						      R_SPARC_OLO10));
	    }
	  else
	    dst_rela.r_info = ELF64_R_INFO (n, R_SPARC_LO10);
	}
      else
	dst_rela.r_info = ELF64_R_INFO (n, ptr->howto->type);

      dst_rela.r_addend = ptr->addend;
      bfd_elf64_swap_reloca_out (abfd, &dst_rela, src_rela);
      ++src_rela;
    }
}

/* Sparc64 ELF linker hash table.  */

struct sparc64_elf_app_reg
{
  unsigned char bind;
  unsigned short shndx;
  bfd *abfd;
  char *name;
};

struct sparc64_elf_link_hash_table
{
  struct elf_link_hash_table root;

  struct sparc64_elf_app_reg app_regs [4];
};

/* Get the Sparc64 ELF linker hash table from a link_info structure.  */

#define sparc64_elf_hash_table(p) \
  ((struct sparc64_elf_link_hash_table *) ((p)->hash))

/* Create a Sparc64 ELF linker hash table.  */

static struct bfd_link_hash_table *
sparc64_elf_bfd_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct sparc64_elf_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct sparc64_elf_link_hash_table);

  ret = (struct sparc64_elf_link_hash_table *) bfd_zalloc (abfd, amt);
  if (ret == (struct sparc64_elf_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       _bfd_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root.root;
}

/* Utility for performing the standard initial work of an instruction
   relocation.
   *PRELOCATION will contain the relocated item.
   *PINSN will contain the instruction from the input stream.
   If the result is `bfd_reloc_other' the caller can continue with
   performing the relocation.  Otherwise it must stop and return the
   value to its caller.  */

static bfd_reloc_status_type
init_insn_reloc (abfd,
		 reloc_entry,
		 symbol,
		 data,
		 input_section,
		 output_bfd,
		 prelocation,
		 pinsn)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     bfd_vma *prelocation;
     bfd_vma *pinsn;
{
  bfd_vma relocation;
  reloc_howto_type *howto = reloc_entry->howto;

  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* This works because partial_inplace == false.  */
  if (output_bfd != NULL)
    return bfd_reloc_continue;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  relocation = (symbol->value
		+ symbol->section->output_section->vma
		+ symbol->section->output_offset);
  relocation += reloc_entry->addend;
  if (howto->pc_relative)
    {
      relocation -= (input_section->output_section->vma
		     + input_section->output_offset);
      relocation -= reloc_entry->address;
    }

  *prelocation = relocation;
  *pinsn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  return bfd_reloc_other;
}

/* For unsupported relocs.  */

static bfd_reloc_status_type
sparc_elf_notsup_reloc (abfd,
			reloc_entry,
			symbol,
			data,
			input_section,
			output_bfd,
			error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  return bfd_reloc_notsupported;
}

/* Handle the WDISP16 reloc.  */

static bfd_reloc_status_type
sparc_elf_wdisp16_reloc (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_vma insn;
  bfd_reloc_status_type status;

  status = init_insn_reloc (abfd, reloc_entry, symbol, data,
			    input_section, output_bfd, &relocation, &insn);
  if (status != bfd_reloc_other)
    return status;

  insn &= ~ (bfd_vma) 0x303fff;
  insn |= (((relocation >> 2) & 0xc000) << 6) | ((relocation >> 2) & 0x3fff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if ((bfd_signed_vma) relocation < - 0x40000
      || (bfd_signed_vma) relocation > 0x3ffff)
    return bfd_reloc_overflow;
  else
    return bfd_reloc_ok;
}

/* Handle the HIX22 reloc.  */

static bfd_reloc_status_type
sparc_elf_hix22_reloc (abfd,
		       reloc_entry,
		       symbol,
		       data,
		       input_section,
		       output_bfd,
		       error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_vma insn;
  bfd_reloc_status_type status;

  status = init_insn_reloc (abfd, reloc_entry, symbol, data,
			    input_section, output_bfd, &relocation, &insn);
  if (status != bfd_reloc_other)
    return status;

  relocation ^= MINUS_ONE;
  insn = (insn &~ (bfd_vma) 0x3fffff) | ((relocation >> 10) & 0x3fffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if ((relocation & ~ (bfd_vma) 0xffffffff) != 0)
    return bfd_reloc_overflow;
  else
    return bfd_reloc_ok;
}

/* Handle the LOX10 reloc.  */

static bfd_reloc_status_type
sparc_elf_lox10_reloc (abfd,
		       reloc_entry,
		       symbol,
		       data,
		       input_section,
		       output_bfd,
		       error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;
  bfd_vma insn;
  bfd_reloc_status_type status;

  status = init_insn_reloc (abfd, reloc_entry, symbol, data,
			    input_section, output_bfd, &relocation, &insn);
  if (status != bfd_reloc_other)
    return status;

  insn = (insn &~ (bfd_vma) 0x1fff) | 0x1c00 | (relocation & 0x3ff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  return bfd_reloc_ok;
}

/* PLT/GOT stuff */

/* Both the headers and the entries are icache aligned.  */
#define PLT_ENTRY_SIZE		32
#define PLT_HEADER_SIZE		(4 * PLT_ENTRY_SIZE)
#define LARGE_PLT_THRESHOLD	32768
#define GOT_RESERVED_ENTRIES	1

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/sparcv9/ld.so.1"

/* Fill in the .plt section.  */

static void
sparc64_elf_build_plt (output_bfd, contents, nentries)
     bfd *output_bfd;
     unsigned char *contents;
     int nentries;
{
  const unsigned int nop = 0x01000000;
  int i, j;

  /* The first four entries are reserved, and are initially undefined.
     We fill them with `illtrap 0' to force ld.so to do something.  */

  for (i = 0; i < PLT_HEADER_SIZE/4; ++i)
    bfd_put_32 (output_bfd, (bfd_vma) 0, contents+i*4);

  /* The first 32768 entries are close enough to plt1 to get there via
     a straight branch.  */

  for (i = 4; i < LARGE_PLT_THRESHOLD && i < nentries; ++i)
    {
      unsigned char *entry = contents + i * PLT_ENTRY_SIZE;
      unsigned int sethi, ba;

      /* sethi (. - plt0), %g1 */
      sethi = 0x03000000 | (i * PLT_ENTRY_SIZE);

      /* ba,a,pt %xcc, plt1 */
      ba = 0x30680000 | (((contents+PLT_ENTRY_SIZE) - (entry+4)) / 4 & 0x7ffff);

      bfd_put_32 (output_bfd, (bfd_vma) sethi, entry);
      bfd_put_32 (output_bfd, (bfd_vma) ba,    entry + 4);
      bfd_put_32 (output_bfd, (bfd_vma) nop,   entry + 8);
      bfd_put_32 (output_bfd, (bfd_vma) nop,   entry + 12);
      bfd_put_32 (output_bfd, (bfd_vma) nop,   entry + 16);
      bfd_put_32 (output_bfd, (bfd_vma) nop,   entry + 20);
      bfd_put_32 (output_bfd, (bfd_vma) nop,   entry + 24);
      bfd_put_32 (output_bfd, (bfd_vma) nop,   entry + 28);
    }

  /* Now the tricky bit.  Entries 32768 and higher are grouped in blocks of
     160: 160 entries and 160 pointers.  This is to separate code from data,
     which is much friendlier on the cache.  */

  for (; i < nentries; i += 160)
    {
      int block = (i + 160 <= nentries ? 160 : nentries - i);
      for (j = 0; j < block; ++j)
	{
	  unsigned char *entry, *ptr;
	  unsigned int ldx;

	  entry = contents + i*PLT_ENTRY_SIZE + j*4*6;
	  ptr = contents + i*PLT_ENTRY_SIZE + block*4*6 + j*8;

	  /* ldx [%o7 + ptr - (entry+4)], %g1 */
	  ldx = 0xc25be000 | ((ptr - (entry+4)) & 0x1fff);

	  /* mov %o7,%g5
	     call .+8
	     nop
	     ldx [%o7+P],%g1
	     jmpl %o7+%g1,%g1
	     mov %g5,%o7  */
	  bfd_put_32 (output_bfd, (bfd_vma) 0x8a10000f, entry);
	  bfd_put_32 (output_bfd, (bfd_vma) 0x40000002, entry + 4);
	  bfd_put_32 (output_bfd, (bfd_vma) nop,        entry + 8);
	  bfd_put_32 (output_bfd, (bfd_vma) ldx,        entry + 12);
	  bfd_put_32 (output_bfd, (bfd_vma) 0x83c3c001, entry + 16);
	  bfd_put_32 (output_bfd, (bfd_vma) 0x9e100005, entry + 20);

	  bfd_put_64 (output_bfd, (bfd_vma) (contents - (entry + 4)), ptr);
	}
    }
}

/* Return the offset of a particular plt entry within the .plt section.  */

static bfd_vma
sparc64_elf_plt_entry_offset (index)
     bfd_vma index;
{
  bfd_vma block, ofs;

  if (index < LARGE_PLT_THRESHOLD)
    return index * PLT_ENTRY_SIZE;

  /* See above for details.  */

  block = (index - LARGE_PLT_THRESHOLD) / 160;
  ofs = (index - LARGE_PLT_THRESHOLD) % 160;

  return (LARGE_PLT_THRESHOLD + block * 160) * PLT_ENTRY_SIZE + ofs * 6 * 4;
}

static bfd_vma
sparc64_elf_plt_ptr_offset (index, max)
     bfd_vma index;
     bfd_vma max;
{
  bfd_vma block, ofs, last;

  BFD_ASSERT(index >= LARGE_PLT_THRESHOLD);

  /* See above for details.  */

  block = (((index - LARGE_PLT_THRESHOLD) / 160) * 160) + LARGE_PLT_THRESHOLD;
  ofs = index - block;
  if (block + 160 > max)
    last = (max - LARGE_PLT_THRESHOLD) % 160;
  else
    last = 160;

  return (block * PLT_ENTRY_SIZE
	  + last * 6*4
	  + ofs * 8);
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static boolean
sparc64_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  if (info->relocateable || !(sec->flags & SEC_ALLOC))
    return true;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_offsets = elf_local_got_offsets (abfd);

  sgot = NULL;
  srelgot = NULL;
  sreloc = NULL;

  rel_end = relocs + NUM_SHDR_ENTRIES (& elf_section_data (sec)->rel_hdr);
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      switch (ELF64_R_TYPE_ID (rel->r_info))
	{
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* This symbol requires a global offset table entry.  */

	  if (dynobj == NULL)
	    {
	      /* Create the .got section.  */
	      elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _bfd_elf_create_got_section (dynobj, info))
		return false;
	    }

	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (srelgot == NULL && (h != NULL || info->shared))
	    {
	      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
	      if (srelgot == NULL)
		{
		  srelgot = bfd_make_section (dynobj, ".rela.got");
		  if (srelgot == NULL
		      || ! bfd_set_section_flags (dynobj, srelgot,
						  (SEC_ALLOC
						   | SEC_LOAD
						   | SEC_HAS_CONTENTS
						   | SEC_IN_MEMORY
						   | SEC_LINKER_CREATED
						   | SEC_READONLY))
		      || ! bfd_set_section_alignment (dynobj, srelgot, 3))
		    return false;
		}
	    }

	  if (h != NULL)
	    {
	      if (h->got.offset != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      h->got.offset = sgot->_raw_size;

	      /* Make sure this symbol is output as a dynamic symbol.  */
	      if (h->dynindx == -1)
		{
		  if (! bfd_elf64_link_record_dynamic_symbol (info, h))
		    return false;
		}

	      srelgot->_raw_size += sizeof (Elf64_External_Rela);
	    }
	  else
	    {
	      /* This is a global offset table entry for a local
                 symbol.  */
	      if (local_got_offsets == NULL)
		{
		  bfd_size_type size;
		  register unsigned int i;

		  size = symtab_hdr->sh_info;
		  size *= sizeof (bfd_vma);
		  local_got_offsets = (bfd_vma *) bfd_alloc (abfd, size);
		  if (local_got_offsets == NULL)
		    return false;
		  elf_local_got_offsets (abfd) = local_got_offsets;
		  for (i = 0; i < symtab_hdr->sh_info; i++)
		    local_got_offsets[i] = (bfd_vma) -1;
		}
	      if (local_got_offsets[r_symndx] != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      local_got_offsets[r_symndx] = sgot->_raw_size;

	      if (info->shared)
		{
		  /* If we are generating a shared object, we need to
                     output a R_SPARC_RELATIVE reloc so that the
                     dynamic linker can adjust this GOT entry.  */
		  srelgot->_raw_size += sizeof (Elf64_External_Rela);
		}
	    }

	  sgot->_raw_size += 8;

#if 0
	  /* Doesn't work for 64-bit -fPIC, since sethi/or builds
	     unsigned numbers.  If we permit ourselves to modify
	     code so we get sethi/xor, this could work.
	     Question: do we consider conditionally re-enabling
             this for -fpic, once we know about object code models?  */
	  /* If the .got section is more than 0x1000 bytes, we add
	     0x1000 to the value of _GLOBAL_OFFSET_TABLE_, so that 13
	     bit relocations have a greater chance of working.  */
	  if (sgot->_raw_size >= 0x1000
	      && elf_hash_table (info)->hgot->root.u.def.value == 0)
	    elf_hash_table (info)->hgot->root.u.def.value = 0x1000;
#endif

	  break;

	case R_SPARC_WPLT30:
	case R_SPARC_PLT32:
	case R_SPARC_HIPLT22:
	case R_SPARC_LOPLT10:
	case R_SPARC_PCPLT32:
	case R_SPARC_PCPLT22:
	case R_SPARC_PCPLT10:
	case R_SPARC_PLT64:
	  /* This symbol requires a procedure linkage table entry.  We
             actually build the entry in adjust_dynamic_symbol,
             because this might be a case of linking PIC code without
             linking in any dynamic objects, in which case we don't
             need to generate a procedure linkage table after all.  */

	  if (h == NULL)
	    {
	      /* It does not make sense to have a procedure linkage
                 table entry for a local symbol.  */
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }

	  /* Make sure this symbol is output as a dynamic symbol.  */
	  if (h->dynindx == -1)
	    {
	      if (! bfd_elf64_link_record_dynamic_symbol (info, h))
		return false;
	    }

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  if (ELF64_R_TYPE_ID (rel->r_info) != R_SPARC_PLT32
	      && ELF64_R_TYPE_ID (rel->r_info) != R_SPARC_PLT64)
	    break;
	  /* Fall through.  */
	case R_SPARC_PC10:
	case R_SPARC_PC22:
	case R_SPARC_PC_HH22:
	case R_SPARC_PC_HM10:
	case R_SPARC_PC_LM22:
	  if (h != NULL
	      && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* Fall through.  */
	case R_SPARC_DISP8:
	case R_SPARC_DISP16:
	case R_SPARC_DISP32:
	case R_SPARC_DISP64:
	case R_SPARC_WDISP30:
	case R_SPARC_WDISP22:
	case R_SPARC_WDISP19:
	case R_SPARC_WDISP16:
	  if (h == NULL)
	    break;
	  /* Fall through.  */
	case R_SPARC_8:
	case R_SPARC_16:
	case R_SPARC_32:
	case R_SPARC_HI22:
	case R_SPARC_22:
	case R_SPARC_13:
	case R_SPARC_LO10:
	case R_SPARC_UA32:
	case R_SPARC_10:
	case R_SPARC_11:
	case R_SPARC_64:
	case R_SPARC_OLO10:
	case R_SPARC_HH22:
	case R_SPARC_HM10:
	case R_SPARC_LM22:
	case R_SPARC_7:
	case R_SPARC_5:
	case R_SPARC_6:
	case R_SPARC_HIX22:
	case R_SPARC_LOX10:
	case R_SPARC_H44:
	case R_SPARC_M44:
	case R_SPARC_L44:
	case R_SPARC_UA64:
	case R_SPARC_UA16:
	  /* When creating a shared object, we must copy these relocs
	     into the output file.  We create a reloc section in
	     dynobj and make room for the reloc.

	     But don't do this for debugging sections -- this shows up
	     with DWARF2 -- first because they are not loaded, and
	     second because DWARF sez the debug info is not to be
	     biased by the load address.  */
	  if (info->shared && (sec->flags & SEC_ALLOC))
	    {
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      sreloc = bfd_make_section (dynobj, name);
		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc, flags)
			  || ! bfd_set_section_alignment (dynobj, sreloc, 3))
			return false;
		    }
		  if (sec->flags & SEC_READONLY)
		    info->flags |= DF_TEXTREL;
		}

	      sreloc->_raw_size += sizeof (Elf64_External_Rela);
	    }
	  break;

	case R_SPARC_REGISTER:
	  /* Nothing to do.  */
	  break;

	default:
	  (*_bfd_error_handler) (_("%s: check_relocs: unhandled reloc type %d"),
				bfd_archive_filename (abfd),
				ELF64_R_TYPE_ID (rel->r_info));
	  return false;
	}
    }

  return true;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We use it for STT_REGISTER symbols.  */

static boolean
sparc64_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp ATTRIBUTE_UNUSED;
     bfd_vma *valp ATTRIBUTE_UNUSED;
{
  static const char *const stt_types[] = { "NOTYPE", "OBJECT", "FUNCTION" };

  if (ELF_ST_TYPE (sym->st_info) == STT_REGISTER)
    {
      int reg;
      struct sparc64_elf_app_reg *p;

      reg = (int)sym->st_value;
      switch (reg & ~1)
	{
	case 2: reg -= 2; break;
	case 6: reg -= 4; break;
	default:
          (*_bfd_error_handler)
            (_("%s: Only registers %%g[2367] can be declared using STT_REGISTER"),
             bfd_archive_filename (abfd));
	  return false;
	}

      if (info->hash->creator != abfd->xvec
	  || (abfd->flags & DYNAMIC) != 0)
        {
	  /* STT_REGISTER only works when linking an elf64_sparc object.
	     If STT_REGISTER comes from a dynamic object, don't put it into
	     the output bfd.  The dynamic linker will recheck it.  */
	  *namep = NULL;
	  return true;
        }

      p = sparc64_elf_hash_table(info)->app_regs + reg;

      if (p->name != NULL && strcmp (p->name, *namep))
	{
          (*_bfd_error_handler)
            (_("Register %%g%d used incompatibly: %s in %s, previously %s in %s"),
             (int) sym->st_value,
             **namep ? *namep : "#scratch", bfd_archive_filename (abfd),
             *p->name ? p->name : "#scratch", bfd_archive_filename (p->abfd));
	  return false;
	}

      if (p->name == NULL)
	{
	  if (**namep)
	    {
	      struct elf_link_hash_entry *h;

	      h = (struct elf_link_hash_entry *)
		bfd_link_hash_lookup (info->hash, *namep, false, false, false);

	      if (h != NULL)
		{
		  unsigned char type = h->type;

		  if (type > STT_FUNC)
		    type = 0;
		  (*_bfd_error_handler)
		    (_("Symbol `%s' has differing types: REGISTER in %s, previously %s in %s"),
		     *namep, bfd_archive_filename (abfd),
		     stt_types[type], bfd_archive_filename (p->abfd));
		  return false;
		}

	      p->name = bfd_hash_allocate (&info->hash->table,
					   strlen (*namep) + 1);
	      if (!p->name)
		return false;

	      strcpy (p->name, *namep);
	    }
	  else
	    p->name = "";
	  p->bind = ELF_ST_BIND (sym->st_info);
	  p->abfd = abfd;
	  p->shndx = sym->st_shndx;
	}
      else
	{
	  if (p->bind == STB_WEAK
	      && ELF_ST_BIND (sym->st_info) == STB_GLOBAL)
	    {
	      p->bind = STB_GLOBAL;
	      p->abfd = abfd;
	    }
	}
      *namep = NULL;
      return true;
    }
  else if (! *namep || ! **namep)
    return true;
  else
    {
      int i;
      struct sparc64_elf_app_reg *p;

      p = sparc64_elf_hash_table(info)->app_regs;
      for (i = 0; i < 4; i++, p++)
	if (p->name != NULL && ! strcmp (p->name, *namep))
	  {
	    unsigned char type = ELF_ST_TYPE (sym->st_info);

	    if (type > STT_FUNC)
	      type = 0;
	    (*_bfd_error_handler)
	      (_("Symbol `%s' has differing types: %s in %s, previously REGISTER in %s"),
	       *namep, stt_types[type], bfd_archive_filename (abfd),
	       bfd_archive_filename (p->abfd));
	    return false;
	  }
    }
  return true;
}

/* This function takes care of emiting STT_REGISTER symbols
   which we cannot easily keep in the symbol hash table.  */

static boolean
sparc64_elf_output_arch_syms (output_bfd, info, finfo, func)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
     PTR finfo;
     boolean (*func) PARAMS ((PTR, const char *,
			      Elf_Internal_Sym *, asection *));
{
  int reg;
  struct sparc64_elf_app_reg *app_regs =
    sparc64_elf_hash_table(info)->app_regs;
  Elf_Internal_Sym sym;

  /* We arranged in size_dynamic_sections to put the STT_REGISTER entries
     at the end of the dynlocal list, so they came at the end of the local
     symbols in the symtab.  Except that they aren't STB_LOCAL, so we need
     to back up symtab->sh_info.  */
  if (elf_hash_table (info)->dynlocal)
    {
      bfd * dynobj = elf_hash_table (info)->dynobj;
      asection *dynsymsec = bfd_get_section_by_name (dynobj, ".dynsym");
      struct elf_link_local_dynamic_entry *e;

      for (e = elf_hash_table (info)->dynlocal; e ; e = e->next)
	if (e->input_indx == -1)
	  break;
      if (e)
	{
	  elf_section_data (dynsymsec->output_section)->this_hdr.sh_info
	    = e->dynindx;
	}
    }

  if (info->strip == strip_all)
    return true;

  for (reg = 0; reg < 4; reg++)
    if (app_regs [reg].name != NULL)
      {
	if (info->strip == strip_some
	    && bfd_hash_lookup (info->keep_hash,
				app_regs [reg].name,
				false, false) == NULL)
	  continue;

	sym.st_value = reg < 2 ? reg + 2 : reg + 4;
	sym.st_size = 0;
	sym.st_other = 0;
	sym.st_info = ELF_ST_INFO (app_regs [reg].bind, STT_REGISTER);
	sym.st_shndx = app_regs [reg].shndx;
	if (! (*func) (finfo, app_regs [reg].name, &sym,
		       sym.st_shndx == SHN_ABS
			 ? bfd_abs_section_ptr : bfd_und_section_ptr))
	  return false;
      }

  return true;
}

static int
sparc64_elf_get_symbol_type (elf_sym, type)
     Elf_Internal_Sym * elf_sym;
     int type;
{
  if (ELF_ST_TYPE (elf_sym->st_info) == STT_REGISTER)
    return STT_REGISTER;
  else
    return type;
}

/* A STB_GLOBAL,STT_REGISTER symbol should be BSF_GLOBAL
   even in SHN_UNDEF section.  */

static void
sparc64_elf_symbol_processing (abfd, asym)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;
  if (elfsym->internal_elf_sym.st_info
      == ELF_ST_INFO (STB_GLOBAL, STT_REGISTER))
    {
      asym->flags |= BSF_GLOBAL;
    }
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
sparc64_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT)
		  || h->weakdef != NULL
		  || ((h->elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_REF_REGULAR) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)));

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later
     (although we could actually do it here).  The STT_NOTYPE
     condition is a hack specifically for the Oracle libraries
     delivered for Solaris; for some inexplicable reason, they define
     some of their functions as STT_NOTYPE when they really should be
     STT_FUNC.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0
      || (h->type == STT_NOTYPE
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	  && (h->root.u.def.section->flags & SEC_CODE) != 0))
    {
      if (! elf_hash_table (info)->dynamic_sections_created)
	{
	  /* This case can occur if we saw a WPLT30 reloc in an input
             file, but none of the input files were dynamic objects.
             In such a case, we don't actually need to build a
             procedure linkage table, and we can just do a WDISP30
             reloc instead.  */
	  BFD_ASSERT ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0);
	  return true;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* The first four bit in .plt is reserved.  */
      if (s->_raw_size == 0)
	s->_raw_size = PLT_HEADER_SIZE;

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (! info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;
	}

      /* To simplify matters later, just store the plt index here.  */
      h->plt.offset = s->_raw_size / PLT_ENTRY_SIZE;

      /* Make room for this entry.  */
      s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .rela.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);

      s->_raw_size += sizeof (Elf64_External_Rela);

      /* The procedure linkage table size is bounded by the magnitude
	 of the offset we can describe in the entry.  */
      if (s->_raw_size >= (bfd_vma)1 << 32)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      return true;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return true;

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_SPARC_COPY reloc to tell the dynamic linker
     to copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rel.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += sizeof (Elf64_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  16-bytes is the size
     of the largest type that requires hard alignment -- long double.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 4)
    power_of_two = 4;

  /* Apply the required alignment.  */
  s->_raw_size = BFD_ALIGN (s->_raw_size,
			    (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
sparc64_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean relplt;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }
  else
    {
      /* We may have created entries in the .rela.got section.
         However, if we are not creating the dynamic sections, we will
         not actually use these entries.  Reset the size of .rela.got,
         which will cause it to get stripped from the output file
         below.  */
      s = bfd_get_section_by_name (dynobj, ".rela.got");
      if (s != NULL)
	s->_raw_size = 0;
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  relplt = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      strip = false;

      if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the
		 output file.  This is to handle .rela.bss and
		 .rel.plt.  We must create it in
		 create_dynamic_sections, because it must be created
		 before the linker maps input sections to output
		 sections.  The linker does that before
		 adjust_dynamic_symbol is called, and it is that
		 function which decides whether anything needs to go
		 into these sections.  */
	      strip = true;
	    }
	  else
	    {
	      if (strcmp (name, ".rela.plt") == 0)
		relplt = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp (name, ".plt") != 0
	       && strncmp (name, ".got", 4) != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  Zero the memory
	 for the benefit of .rela.plt, which has 4 unused entries
	 at the beginning, and we don't want garbage.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in sparc64_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  bfd_elf64_add_dynamic_entry (info, (bfd_vma) (TAG), (bfd_vma) (VAL))

      int reg;
      struct sparc64_elf_app_reg * app_regs;
      struct elf_strtab_hash *dynstr;
      struct elf_link_hash_table *eht = elf_hash_table (info);

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return false;
	}

      if (relplt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return false;
	}

      if (!add_dynamic_entry (DT_RELA, 0)
	  || !add_dynamic_entry (DT_RELASZ, 0)
	  || !add_dynamic_entry (DT_RELAENT, sizeof (Elf64_External_Rela)))
	return false;

      if (info->flags & DF_TEXTREL)
	{
	  if (!add_dynamic_entry (DT_TEXTREL, 0))
	    return false;
	}

      /* Add dynamic STT_REGISTER symbols and corresponding DT_SPARC_REGISTER
	 entries if needed.  */
      app_regs = sparc64_elf_hash_table (info)->app_regs;
      dynstr = eht->dynstr;

      for (reg = 0; reg < 4; reg++)
	if (app_regs [reg].name != NULL)
	  {
	    struct elf_link_local_dynamic_entry *entry, *e;

	    if (!add_dynamic_entry (DT_SPARC_REGISTER, 0))
	      return false;

	    entry = (struct elf_link_local_dynamic_entry *)
	      bfd_hash_allocate (&info->hash->table, sizeof (*entry));
	    if (entry == NULL)
	      return false;

	    /* We cheat here a little bit: the symbol will not be local, so we
	       put it at the end of the dynlocal linked list.  We will fix it
	       later on, as we have to fix other fields anyway.  */
	    entry->isym.st_value = reg < 2 ? reg + 2 : reg + 4;
	    entry->isym.st_size = 0;
	    if (*app_regs [reg].name != '\0')
	      entry->isym.st_name
		= _bfd_elf_strtab_add (dynstr, app_regs[reg].name, false);
	    else
	      entry->isym.st_name = 0;
	    entry->isym.st_other = 0;
	    entry->isym.st_info = ELF_ST_INFO (app_regs [reg].bind,
					       STT_REGISTER);
	    entry->isym.st_shndx = app_regs [reg].shndx;
	    entry->next = NULL;
	    entry->input_bfd = output_bfd;
	    entry->input_indx = -1;

	    if (eht->dynlocal == NULL)
	      eht->dynlocal = entry;
	    else
	      {
		for (e = eht->dynlocal; e->next; e = e->next)
		  ;
		e->next = entry;
	      }
	    eht->dynsymcount++;
	  }
    }
#undef add_dynamic_entry

  return true;
}

#define SET_SEC_DO_RELAX(section) do { elf_section_data(section)->tdata = (void *)1; } while (0)
#define SEC_DO_RELAX(section) (elf_section_data(section)->tdata == (void *)1)

static boolean
sparc64_elf_relax_section (abfd, section, link_info, again)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section ATTRIBUTE_UNUSED;
     struct bfd_link_info *link_info ATTRIBUTE_UNUSED;
     boolean *again;
{
  *again = false;
  SET_SEC_DO_RELAX (section);
  return true;
}

/* Relocate a SPARC64 ELF section.  */

static boolean
sparc64_elf_relocate_section (output_bfd, info, input_bfd, input_section,
			      contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_vma *local_got_offsets;
  bfd_vma got_base;
  asection *sgot;
  asection *splt;
  asection *sreloc;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  local_got_offsets = elf_local_got_offsets (input_bfd);

  if (elf_hash_table(info)->hgot == NULL)
    got_base = 0;
  else
    got_base = elf_hash_table (info)->hgot->root.u.def.value;

  sgot = splt = sreloc = NULL;

  rel = relocs;
  relend = relocs + NUM_SHDR_ENTRIES (& elf_section_data (input_section)->rel_hdr);
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      boolean is_plt = false;

      r_type = ELF64_R_TYPE_ID (rel->r_info);
      if (r_type < 0 || r_type >= (int) R_SPARC_max_std)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      howto = sparc64_elf_howto_table + r_type;

      r_symndx = ELF64_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  continue;
	}

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      boolean skip_it = false;
	      sec = h->root.u.def.section;

	      switch (r_type)
		{
		case R_SPARC_WPLT30:
		case R_SPARC_PLT32:
		case R_SPARC_HIPLT22:
		case R_SPARC_LOPLT10:
		case R_SPARC_PCPLT32:
		case R_SPARC_PCPLT22:
		case R_SPARC_PCPLT10:
		case R_SPARC_PLT64:
		  if (h->plt.offset != (bfd_vma) -1)
		    skip_it = true;
		  break;

		case R_SPARC_GOT10:
		case R_SPARC_GOT13:
		case R_SPARC_GOT22:
		  if (elf_hash_table(info)->dynamic_sections_created
		      && (!info->shared
			  || (!info->symbolic && h->dynindx != -1)
			  || !(h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR)))
		    skip_it = true;
		  break;

		case R_SPARC_PC10:
		case R_SPARC_PC22:
		case R_SPARC_PC_HH22:
		case R_SPARC_PC_HM10:
		case R_SPARC_PC_LM22:
		  if (!strcmp(h->root.root.string, "_GLOBAL_OFFSET_TABLE_"))
		    break;
		  /* FALLTHRU */

		case R_SPARC_8:
		case R_SPARC_16:
		case R_SPARC_32:
		case R_SPARC_DISP8:
		case R_SPARC_DISP16:
		case R_SPARC_DISP32:
		case R_SPARC_WDISP30:
		case R_SPARC_WDISP22:
		case R_SPARC_HI22:
		case R_SPARC_22:
		case R_SPARC_13:
		case R_SPARC_LO10:
		case R_SPARC_UA32:
		case R_SPARC_10:
		case R_SPARC_11:
		case R_SPARC_64:
		case R_SPARC_OLO10:
		case R_SPARC_HH22:
		case R_SPARC_HM10:
		case R_SPARC_LM22:
		case R_SPARC_WDISP19:
		case R_SPARC_WDISP16:
		case R_SPARC_7:
		case R_SPARC_5:
		case R_SPARC_6:
		case R_SPARC_DISP64:
		case R_SPARC_HIX22:
		case R_SPARC_LOX10:
		case R_SPARC_H44:
		case R_SPARC_M44:
		case R_SPARC_L44:
		case R_SPARC_UA64:
		case R_SPARC_UA16:
		  if (info->shared
		      && ((!info->symbolic && h->dynindx != -1)
			  || !(h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR))
		      && ((input_section->flags & SEC_ALLOC) != 0
			  /* DWARF will emit R_SPARC_{32,64} relocations in
			     its sections against symbols defined externally
			     in shared libraries.  We can't do anything
			     with them here.  */
			  || ((input_section->flags & SEC_DEBUGGING) != 0
			      && (h->elf_link_hash_flags
				  & ELF_LINK_HASH_DEF_DYNAMIC) != 0)))
		    skip_it = true;
		  break;
		}

	      if (skip_it)
		{
		  /* In these cases, we don't need the relocation
                     value.  We check specially because in some
                     obscure cases sec->output_section will be NULL.  */
		  relocation = 0;
		}
	      else
		{
		  relocation = (h->root.u.def.value
				+ sec->output_section->vma
				+ sec->output_offset);
		}
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared
		   && (!info->symbolic || info->allow_shlib_undefined)
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset,
		      (!info->shared || info->no_undefined
		       || ELF_ST_VISIBILITY (h->other)))))
		return false;

	      /* To avoid generating warning messages about truncated
		 relocations, set the relocation's address to be the same as
		 the start of this section.  */

	      if (input_section->output_section != NULL)
		relocation = input_section->output_section->vma;
	      else
		relocation = 0;
	    }
	}

do_dynreloc:
      /* When generating a shared object, these relocations are copied
	 into the output file to be resolved at run time.  */
      if (info->shared && r_symndx != 0 && (input_section->flags & SEC_ALLOC))
	{
	  switch (r_type)
	    {
	    case R_SPARC_PC10:
	    case R_SPARC_PC22:
	    case R_SPARC_PC_HH22:
	    case R_SPARC_PC_HM10:
	    case R_SPARC_PC_LM22:
	      if (h != NULL
		  && !strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_"))
		break;
	      /* Fall through.  */
	    case R_SPARC_DISP8:
	    case R_SPARC_DISP16:
	    case R_SPARC_DISP32:
	    case R_SPARC_WDISP30:
	    case R_SPARC_WDISP22:
	    case R_SPARC_WDISP19:
	    case R_SPARC_WDISP16:
	    case R_SPARC_DISP64:
	      if (h == NULL)
		break;
	      /* Fall through.  */
	    case R_SPARC_8:
	    case R_SPARC_16:
	    case R_SPARC_32:
	    case R_SPARC_HI22:
	    case R_SPARC_22:
	    case R_SPARC_13:
	    case R_SPARC_LO10:
	    case R_SPARC_UA32:
	    case R_SPARC_10:
	    case R_SPARC_11:
	    case R_SPARC_64:
	    case R_SPARC_OLO10:
	    case R_SPARC_HH22:
	    case R_SPARC_HM10:
	    case R_SPARC_LM22:
	    case R_SPARC_7:
	    case R_SPARC_5:
	    case R_SPARC_6:
	    case R_SPARC_HIX22:
	    case R_SPARC_LOX10:
	    case R_SPARC_H44:
	    case R_SPARC_M44:
	    case R_SPARC_L44:
	    case R_SPARC_UA64:
	    case R_SPARC_UA16:
	      {
		Elf_Internal_Rela outrel;
		boolean skip, relocate;

		if (sreloc == NULL)
		  {
		    const char *name =
		      (bfd_elf_string_from_elf_section
		       (input_bfd,
			elf_elfheader (input_bfd)->e_shstrndx,
			elf_section_data (input_section)->rel_hdr.sh_name));

		    if (name == NULL)
		      return false;

		    BFD_ASSERT (strncmp (name, ".rela", 5) == 0
				&& strcmp (bfd_get_section_name(input_bfd,
								input_section),
					   name + 5) == 0);

		    sreloc = bfd_get_section_by_name (dynobj, name);
		    BFD_ASSERT (sreloc != NULL);
		  }

		skip = false;
		relocate = false;

		outrel.r_offset =
		  _bfd_elf_section_offset (output_bfd, info, input_section,
					   rel->r_offset);
		if (outrel.r_offset == (bfd_vma) -1)
		  skip = true;
		else if (outrel.r_offset == (bfd_vma) -2)
		  skip = true, relocate = true;

		outrel.r_offset += (input_section->output_section->vma
				    + input_section->output_offset);

		/* Optimize unaligned reloc usage now that we know where
		   it finally resides.  */
		switch (r_type)
		  {
		  case R_SPARC_16:
		    if (outrel.r_offset & 1) r_type = R_SPARC_UA16;
		    break;
		  case R_SPARC_UA16:
		    if (!(outrel.r_offset & 1)) r_type = R_SPARC_16;
		    break;
		  case R_SPARC_32:
		    if (outrel.r_offset & 3) r_type = R_SPARC_UA32;
		    break;
		  case R_SPARC_UA32:
		    if (!(outrel.r_offset & 3)) r_type = R_SPARC_32;
		    break;
		  case R_SPARC_64:
		    if (outrel.r_offset & 7) r_type = R_SPARC_UA64;
		    break;
		  case R_SPARC_UA64:
		    if (!(outrel.r_offset & 7)) r_type = R_SPARC_64;
		    break;
		  }

		if (skip)
		  memset (&outrel, 0, sizeof outrel);
		/* h->dynindx may be -1 if the symbol was marked to
		   become local.  */
		else if (h != NULL && ! is_plt
			 && ((! info->symbolic && h->dynindx != -1)
			     || (h->elf_link_hash_flags
				 & ELF_LINK_HASH_DEF_REGULAR) == 0))
		  {
		    BFD_ASSERT (h->dynindx != -1);
		    outrel.r_info
		      = ELF64_R_INFO (h->dynindx,
				      ELF64_R_TYPE_INFO (
					ELF64_R_TYPE_DATA (rel->r_info),
							   r_type));
		    outrel.r_addend = rel->r_addend;
		  }
		else
		  {
		    if (r_type == R_SPARC_64)
		      {
			outrel.r_info = ELF64_R_INFO (0, R_SPARC_RELATIVE);
			outrel.r_addend = relocation + rel->r_addend;
		      }
		    else
		      {
			long indx;

			if (is_plt)
			  sec = splt;
			else if (h == NULL)
			  sec = local_sections[r_symndx];
			else
			  {
			    BFD_ASSERT (h->root.type == bfd_link_hash_defined
					|| (h->root.type
					    == bfd_link_hash_defweak));
			    sec = h->root.u.def.section;
			  }
			if (sec != NULL && bfd_is_abs_section (sec))
			  indx = 0;
			else if (sec == NULL || sec->owner == NULL)
			  {
			    bfd_set_error (bfd_error_bad_value);
			    return false;
			  }
			else
			  {
			    asection *osec;

			    osec = sec->output_section;
			    indx = elf_section_data (osec)->dynindx;

			    /* FIXME: we really should be able to link non-pic
			       shared libraries.  */
			    if (indx == 0)
			      {
				BFD_FAIL ();
				(*_bfd_error_handler)
				  (_("%s: probably compiled without -fPIC?"),
				   bfd_archive_filename (input_bfd));
				bfd_set_error (bfd_error_bad_value);
				return false;
			      }
			  }

			outrel.r_info
			  = ELF64_R_INFO (indx,
					  ELF64_R_TYPE_INFO (
					    ELF64_R_TYPE_DATA (rel->r_info),
							       r_type));
			outrel.r_addend = relocation + rel->r_addend;
		      }
		  }

		bfd_elf64_swap_reloca_out (output_bfd, &outrel,
					   (((Elf64_External_Rela *)
					     sreloc->contents)
					    + sreloc->reloc_count));
		++sreloc->reloc_count;

		/* This reloc will be computed at runtime, so there's no
		   need to do anything now.  */
		if (! relocate)
		  continue;
	      }
	    break;
	    }
	}

      switch (r_type)
	{
	case R_SPARC_GOT10:
	case R_SPARC_GOT13:
	case R_SPARC_GOT22:
	  /* Relocation is to the entry for this symbol in the global
	     offset table.  */
	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (h != NULL)
	    {
	      bfd_vma off = h->got.offset;
	      BFD_ASSERT (off != (bfd_vma) -1);

	      if (! elf_hash_table (info)->dynamic_sections_created
		  || (info->shared
		      && (info->symbolic || h->dynindx == -1)
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR)))
		{
		  /* This is actually a static link, or it is a -Bsymbolic
		     link and the symbol is defined locally, or the symbol
		     was forced to be local because of a version file.  We
		     must initialize this entry in the global offset table.
		     Since the offset must always be a multiple of 8, we
		     use the least significant bit to record whether we
		     have initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.  This is
		     done in the finish_dynamic_symbol routine.  */

		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_64 (output_bfd, relocation,
				  sgot->contents + off);
		      h->got.offset |= 1;
		    }
		}
	      relocation = sgot->output_offset + off - got_base;
	    }
	  else
	    {
	      bfd_vma off;

	      BFD_ASSERT (local_got_offsets != NULL);
	      off = local_got_offsets[r_symndx];
	      BFD_ASSERT (off != (bfd_vma) -1);

	      /* The offset must always be a multiple of 8.  We use
		 the least significant bit to record whether we have
		 already processed this entry.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  local_got_offsets[r_symndx] |= 1;

		  if (info->shared)
		    {
		      asection *srelgot;
		      Elf_Internal_Rela outrel;

		      /* The Solaris 2.7 64-bit linker adds the contents
			 of the location to the value of the reloc.
			 Note this is different behaviour to the
			 32-bit linker, which both adds the contents
			 and ignores the addend.  So clear the location.  */
		      bfd_put_64 (output_bfd, (bfd_vma) 0,
				  sgot->contents + off);

		      /* We need to generate a R_SPARC_RELATIVE reloc
			 for the dynamic linker.  */
		      srelgot = bfd_get_section_by_name(dynobj, ".rela.got");
		      BFD_ASSERT (srelgot != NULL);

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);
		      outrel.r_info = ELF64_R_INFO (0, R_SPARC_RELATIVE);
		      outrel.r_addend = relocation;
		      bfd_elf64_swap_reloca_out (output_bfd, &outrel,
						 (((Elf64_External_Rela *)
						   srelgot->contents)
						  + srelgot->reloc_count));
		      ++srelgot->reloc_count;
		    }
		  else
		    bfd_put_64 (output_bfd, relocation, sgot->contents + off);
		}
	      relocation = sgot->output_offset + off - got_base;
	    }
	  goto do_default;

	case R_SPARC_WPLT30:
	case R_SPARC_PLT32:
	case R_SPARC_HIPLT22:
	case R_SPARC_LOPLT10:
	case R_SPARC_PCPLT32:
	case R_SPARC_PCPLT22:
	case R_SPARC_PCPLT10:
	case R_SPARC_PLT64:
	  /* Relocation is to the entry for this symbol in the
             procedure linkage table.  */
	  BFD_ASSERT (h != NULL);

	  if (h->plt.offset == (bfd_vma) -1)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      goto do_default;
	    }

	  if (splt == NULL)
	    {
	      splt = bfd_get_section_by_name (dynobj, ".plt");
	      BFD_ASSERT (splt != NULL);
	    }

	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ sparc64_elf_plt_entry_offset (h->plt.offset));
	  if (r_type == R_SPARC_WPLT30)
	    goto do_wplt30;
	  if (r_type == R_SPARC_PLT32 || r_type == R_SPARC_PLT64)
	    {
	      r_type = r_type == R_SPARC_PLT32 ? R_SPARC_32 : R_SPARC_64;
	      is_plt = true;
	      goto do_dynreloc;
	    }
	  goto do_default;

	case R_SPARC_OLO10:
	  {
	    bfd_vma x;

	    relocation += rel->r_addend;
	    relocation = (relocation & 0x3ff) + ELF64_R_TYPE_DATA (rel->r_info);

	    x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	    x = (x & ~(bfd_vma) 0x1fff) | (relocation & 0x1fff);
	    bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	    r = bfd_check_overflow (howto->complain_on_overflow,
				    howto->bitsize, howto->rightshift,
				    bfd_arch_bits_per_address (input_bfd),
				    relocation);
	  }
	  break;

	case R_SPARC_WDISP16:
	  {
	    bfd_vma x;

	    relocation += rel->r_addend;
	    /* Adjust for pc-relative-ness.  */
	    relocation -= (input_section->output_section->vma
			   + input_section->output_offset);
	    relocation -= rel->r_offset;

	    x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	    x &= ~(bfd_vma) 0x303fff;
	    x |= ((((relocation >> 2) & 0xc000) << 6)
		  | ((relocation >> 2) & 0x3fff));
	    bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	    r = bfd_check_overflow (howto->complain_on_overflow,
				    howto->bitsize, howto->rightshift,
				    bfd_arch_bits_per_address (input_bfd),
				    relocation);
	  }
	  break;

	case R_SPARC_HIX22:
	  {
	    bfd_vma x;

	    relocation += rel->r_addend;
	    relocation = relocation ^ MINUS_ONE;

	    x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	    x = (x & ~(bfd_vma) 0x3fffff) | ((relocation >> 10) & 0x3fffff);
	    bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	    r = bfd_check_overflow (howto->complain_on_overflow,
				    howto->bitsize, howto->rightshift,
				    bfd_arch_bits_per_address (input_bfd),
				    relocation);
	  }
	  break;

	case R_SPARC_LOX10:
	  {
	    bfd_vma x;

	    relocation += rel->r_addend;
	    relocation = (relocation & 0x3ff) | 0x1c00;

	    x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	    x = (x & ~(bfd_vma) 0x1fff) | relocation;
	    bfd_put_32 (input_bfd, x, contents + rel->r_offset);

	    r = bfd_reloc_ok;
	  }
	  break;

	case R_SPARC_WDISP30:
	do_wplt30:
	  if (SEC_DO_RELAX (input_section)
	      && rel->r_offset + 4 < input_section->_raw_size)
	    {
#define G0		0
#define O7		15
#define XCC		(2 << 20)
#define COND(x)		(((x)&0xf)<<25)
#define CONDA		COND(0x8)
#define INSN_BPA	(F2(0,1) | CONDA | BPRED | XCC)
#define INSN_BA		(F2(0,2) | CONDA)
#define INSN_OR		F3(2, 0x2, 0)
#define INSN_NOP	F2(0,4)

	      bfd_vma x, y;

	      /* If the instruction is a call with either:
		 restore
		 arithmetic instruction with rd == %o7
		 where rs1 != %o7 and rs2 if it is register != %o7
		 then we can optimize if the call destination is near
		 by changing the call into a branch always.  */
	      x = bfd_get_32 (input_bfd, contents + rel->r_offset);
	      y = bfd_get_32 (input_bfd, contents + rel->r_offset + 4);
	      if ((x & OP(~0)) == OP(1) && (y & OP(~0)) == OP(2))
		{
		  if (((y & OP3(~0)) == OP3(0x3d) /* restore */
		       || ((y & OP3(0x28)) == 0 /* arithmetic */
			   && (y & RD(~0)) == RD(O7)))
		      && (y & RS1(~0)) != RS1(O7)
		      && ((y & F3I(~0))
			  || (y & RS2(~0)) != RS2(O7)))
		    {
		      bfd_vma reloc;

		      reloc = relocation + rel->r_addend - rel->r_offset;
		      reloc -= (input_section->output_section->vma
				+ input_section->output_offset);
		      if (reloc & 3)
			goto do_default;

		      /* Ensure the branch fits into simm22.  */
		      if ((reloc & ~(bfd_vma)0x7fffff)
			   && ((reloc | 0x7fffff) != MINUS_ONE))
			goto do_default;
		      reloc >>= 2;

		      /* Check whether it fits into simm19.  */
		      if ((reloc & 0x3c0000) == 0
			  || (reloc & 0x3c0000) == 0x3c0000)
			x = INSN_BPA | (reloc & 0x7ffff); /* ba,pt %xcc */
		      else
			x = INSN_BA | (reloc & 0x3fffff); /* ba */
		      bfd_put_32 (input_bfd, x, contents + rel->r_offset);
		      r = bfd_reloc_ok;
		      if (rel->r_offset >= 4
			  && (y & (0xffffffff ^ RS1(~0)))
			     == (INSN_OR | RD(O7) | RS2(G0)))
			{
			  bfd_vma z;
			  unsigned int reg;

			  z = bfd_get_32 (input_bfd,
					  contents + rel->r_offset - 4);
			  if ((z & (0xffffffff ^ RD(~0)))
			      != (INSN_OR | RS1(O7) | RS2(G0)))
			    break;

			  /* The sequence was
			     or %o7, %g0, %rN
			     call foo
			     or %rN, %g0, %o7

			     If call foo was replaced with ba, replace
			     or %rN, %g0, %o7 with nop.  */

			  reg = (y & RS1(~0)) >> 14;
			  if (reg != ((z & RD(~0)) >> 25)
			      || reg == G0 || reg == O7)
			    break;

			  bfd_put_32 (input_bfd, (bfd_vma) INSN_NOP,
				      contents + rel->r_offset + 4);
			}
		      break;
		    }
		}
	    }
	  /* FALLTHROUGH */

	default:
	do_default:
	  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents, rel->r_offset,
					relocation, rel->r_addend);
	  break;
	}

      switch (r)
	{
	case bfd_reloc_ok:
	  break;

	default:
	case bfd_reloc_outofrange:
	  abort ();

	case bfd_reloc_overflow:
	  {
	    const char *name;

	    /* The Solaris native linker silently disregards
	       overflows.  We don't, but this breaks stabs debugging
	       info, whose relocations are only 32-bits wide.  Ignore
	       overflows in this case.  */
	    if (r_type == R_SPARC_32
		&& (input_section->flags & SEC_DEBUGGING) != 0
		&& strcmp (bfd_section_name (input_bfd, input_section),
			   ".stab") == 0)
	      break;

	    if (h != NULL)
	      {
		if (h->root.type == bfd_link_hash_undefweak
		    && howto->pc_relative)
		  {
		    /* Assume this is a call protected by other code that
		       detect the symbol is undefined.  If this is the case,
		       we can safely ignore the overflow.  If not, the
		       program is hosed anyway, and a little warning isn't
		       going to help.  */
		    break;
		  }

	        name = h->root.root.string;
	      }
	    else
	      {
		name = (bfd_elf_string_from_elf_section
			(input_bfd,
			 symtab_hdr->sh_link,
			 sym->st_name));
		if (name == NULL)
		  return false;
		if (*name == '\0')
		  name = bfd_section_name (input_bfd, sec);
	      }
	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto->name, (bfd_vma) 0,
		    input_bfd, input_section, rel->r_offset)))
	      return false;
	  }
	break;
	}
    }

  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
sparc64_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the PLT.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && srela != NULL);

      /* Fill in the entry in the .rela.plt section.  */

      if (h->plt.offset < LARGE_PLT_THRESHOLD)
	{
	  rela.r_offset = sparc64_elf_plt_entry_offset (h->plt.offset);
	  rela.r_addend = 0;
	}
      else
	{
	  bfd_vma max = splt->_raw_size / PLT_ENTRY_SIZE;
	  rela.r_offset = sparc64_elf_plt_ptr_offset (h->plt.offset, max);
	  rela.r_addend = -(sparc64_elf_plt_entry_offset (h->plt.offset) + 4)
			  -(splt->output_section->vma + splt->output_offset);
	}
      rela.r_offset += (splt->output_section->vma + splt->output_offset);
      rela.r_info = ELF64_R_INFO (h->dynindx, R_SPARC_JMP_SLOT);

      /* Adjust for the first 4 reserved elements in the .plt section
	 when setting the offset in the .rela.plt section.
	 Sun forgot to read their own ABI and copied elf32-sparc behaviour,
	 thus .plt[4] has corresponding .rela.plt[0] and so on.  */

      bfd_elf64_swap_reloca_out (output_bfd, &rela,
				 ((Elf64_External_Rela *) srela->contents
				  + (h->plt.offset - 4)));

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	  /* If the symbol is weak, we do need to clear the value.
	     Otherwise, the PLT entry would provide a definition for
	     the symbol even if the symbol wasn't defined anywhere,
	     and so the symbol would never be NULL.  */
	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK)
	      == 0)
	    sym->st_value = 0;
	}
    }

  if (h->got.offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srela;
      Elf_Internal_Rela rela;

      /* This symbol has an entry in the GOT.  Set it up.  */

      sgot = bfd_get_section_by_name (dynobj, ".got");
      srela = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  Likewise if
	 the symbol was forced to be local because of a version file.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && (info->symbolic || h->dynindx == -1)
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  asection *sec = h->root.u.def.section;
	  rela.r_info = ELF64_R_INFO (0, R_SPARC_RELATIVE);
	  rela.r_addend = (h->root.u.def.value
			   + sec->output_section->vma
			   + sec->output_offset);
	}
      else
	{
	  bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents + h->got.offset);
	  rela.r_info = ELF64_R_INFO (h->dynindx, R_SPARC_GLOB_DAT);
	  rela.r_addend = 0;
	}

      bfd_elf64_swap_reloca_out (output_bfd, &rela,
				 ((Elf64_External_Rela *) srela->contents
				  + srela->reloc_count));
      ++srela->reloc_count;
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;

      /* This symbols needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF64_R_INFO (h->dynindx, R_SPARC_COPY);
      rela.r_addend = 0;
      bfd_elf64_swap_reloca_out (output_bfd, &rela,
				 ((Elf64_External_Rela *) s->contents
				  + s->reloc_count));
      ++s->reloc_count;
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}

/* Finish up the dynamic sections.  */

static boolean
sparc64_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  int stt_regidx = -1;
  asection *sdyn;
  asection *sgot;

  dynobj = elf_hash_table (info)->dynobj;

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf64_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  boolean size;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:   name = ".plt"; size = false; break;
	    case DT_PLTRELSZ: name = ".rela.plt"; size = true; break;
	    case DT_JMPREL:   name = ".rela.plt"; size = false; break;
	    case DT_SPARC_REGISTER:
	      if (stt_regidx == -1)
		{
		  stt_regidx =
		    _bfd_elf_link_lookup_local_dynindx (info, output_bfd, -1);
		  if (stt_regidx == -1)
		    return false;
		}
	      dyn.d_un.d_val = stt_regidx++;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      /* fallthrough */
	    default:	      name = NULL; size = false; break;
	    }

	  if (name != NULL)
	    {
	      asection *s;

	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s == NULL)
		dyn.d_un.d_val = 0;
	      else
		{
		  if (! size)
		    dyn.d_un.d_ptr = s->vma;
		  else
		    {
		      if (s->_cooked_size != 0)
			dyn.d_un.d_val = s->_cooked_size;
		      else
			dyn.d_un.d_val = s->_raw_size;
		    }
		}
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	    }
	}

      /* Initialize the contents of the .plt section.  */
      if (splt->_raw_size > 0)
	{
	  sparc64_elf_build_plt (output_bfd, splt->contents,
				 (int) (splt->_raw_size / PLT_ENTRY_SIZE));
	}

      elf_section_data (splt->output_section)->this_hdr.sh_entsize =
	PLT_ENTRY_SIZE;
    }

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");
  BFD_ASSERT (sgot != NULL);
  if (sgot->_raw_size > 0)
    {
      if (sdyn == NULL)
	bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_64 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 8;

  return true;
}

static enum elf_reloc_type_class
sparc64_elf_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  switch ((int) ELF64_R_TYPE (rela->r_info))
    {
    case R_SPARC_RELATIVE:
      return reloc_class_relative;
    case R_SPARC_JMP_SLOT:
      return reloc_class_plt;
    case R_SPARC_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

/* Functions for dealing with the e_flags field.  */

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static boolean
sparc64_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  boolean error;
  flagword new_flags, old_flags;
  int new_mm, old_mm;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;

  if (!elf_flags_init (obfd))   /* First call, no flags set */
    {
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  else if (new_flags == old_flags)      /* Compatible flags are ok */
    ;

  else                                  /* Incompatible flags */
    {
      error = false;

#define EF_SPARC_ISA_EXTENSIONS \
  (EF_SPARC_SUN_US1 | EF_SPARC_SUN_US3 | EF_SPARC_HAL_R1)

      if ((ibfd->flags & DYNAMIC) != 0)
	{
	  /* We don't want dynamic objects memory ordering and
	     architecture to have any role. That's what dynamic linker
	     should do.  */
	  new_flags &= ~(EF_SPARCV9_MM | EF_SPARC_ISA_EXTENSIONS);
	  new_flags |= (old_flags
			& (EF_SPARCV9_MM | EF_SPARC_ISA_EXTENSIONS));
	}
      else
	{
	  /* Choose the highest architecture requirements.  */
	  old_flags |= (new_flags & EF_SPARC_ISA_EXTENSIONS);
	  new_flags |= (old_flags & EF_SPARC_ISA_EXTENSIONS);
	  if ((old_flags & (EF_SPARC_SUN_US1 | EF_SPARC_SUN_US3))
	      && (old_flags & EF_SPARC_HAL_R1))
	    {
	      error = true;
	      (*_bfd_error_handler)
		(_("%s: linking UltraSPARC specific with HAL specific code"),
		 bfd_archive_filename (ibfd));
	    }
	  /* Choose the most restrictive memory ordering.  */
	  old_mm = (old_flags & EF_SPARCV9_MM);
	  new_mm = (new_flags & EF_SPARCV9_MM);
	  old_flags &= ~EF_SPARCV9_MM;
	  new_flags &= ~EF_SPARCV9_MM;
	  if (new_mm < old_mm)
	    old_mm = new_mm;
	  old_flags |= old_mm;
	  new_flags |= old_mm;
	}

      /* Warn about any other mismatches */
      if (new_flags != old_flags)
        {
          error = true;
          (*_bfd_error_handler)
            (_("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)"),
             bfd_archive_filename (ibfd), (long) new_flags, (long) old_flags);
        }

      elf_elfheader (obfd)->e_flags = old_flags;

      if (error)
        {
          bfd_set_error (bfd_error_bad_value);
          return false;
        }
    }
  return true;
}

/* Print a STT_REGISTER symbol to file FILE.  */

static const char *
sparc64_elf_print_symbol_all (abfd, filep, symbol)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR filep;
     asymbol *symbol;
{
  FILE *file = (FILE *) filep;
  int reg, type;

  if (ELF_ST_TYPE (((elf_symbol_type *) symbol)->internal_elf_sym.st_info)
      != STT_REGISTER)
    return NULL;

  reg = ((elf_symbol_type *) symbol)->internal_elf_sym.st_value;
  type = symbol->flags;
  fprintf (file, "REG_%c%c%11s%c%c    R", "GOLI" [reg / 8], '0' + (reg & 7), "",
		 ((type & BSF_LOCAL)
		  ? (type & BSF_GLOBAL) ? '!' : 'l'
	          : (type & BSF_GLOBAL) ? 'g' : ' '),
	         (type & BSF_WEAK) ? 'w' : ' ');
  if (symbol->name == NULL || symbol->name [0] == '\0')
    return "#scratch";
  else
    return symbol->name;
}

/* Set the right machine number for a SPARC64 ELF file.  */

static boolean
sparc64_elf_object_p (abfd)
     bfd *abfd;
{
  unsigned long mach = bfd_mach_sparc_v9;

  if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US3)
    mach = bfd_mach_sparc_v9b;
  else if (elf_elfheader (abfd)->e_flags & EF_SPARC_SUN_US1)
    mach = bfd_mach_sparc_v9a;
  return bfd_default_set_arch_mach (abfd, bfd_arch_sparc, mach);
}

/* Relocations in the 64 bit SPARC ELF ABI are more complex than in
   standard ELF, because R_SPARC_OLO10 has secondary addend in
   ELF64_R_TYPE_DATA field.  This structure is used to redirect the
   relocation handling routines.  */

const struct elf_size_info sparc64_elf_size_info =
{
  sizeof (Elf64_External_Ehdr),
  sizeof (Elf64_External_Phdr),
  sizeof (Elf64_External_Shdr),
  sizeof (Elf64_External_Rel),
  sizeof (Elf64_External_Rela),
  sizeof (Elf64_External_Sym),
  sizeof (Elf64_External_Dyn),
  sizeof (Elf_External_Note),
  4,		/* hash-table entry size */
  /* internal relocations per external relocations.
     For link purposes we use just 1 internal per
     1 external, for assembly and slurp symbol table
     we use 2.  */
  1,
  64,		/* arch_size */
  8,		/* file_align */
  ELFCLASS64,
  EV_CURRENT,
  bfd_elf64_write_out_phdrs,
  bfd_elf64_write_shdrs_and_ehdr,
  sparc64_elf_write_relocs,
  bfd_elf64_swap_symbol_out,
  sparc64_elf_slurp_reloc_table,
  bfd_elf64_slurp_symbol_table,
  bfd_elf64_swap_dyn_in,
  bfd_elf64_swap_dyn_out,
  NULL,
  NULL,
  NULL,
  NULL
};

#define TARGET_BIG_SYM	bfd_elf64_sparc_vec
#define TARGET_BIG_NAME	"elf64-sparc"
#define ELF_ARCH	bfd_arch_sparc
#define ELF_MAXPAGESIZE 0x100000

/* This is the official ABI value.  */
#define ELF_MACHINE_CODE EM_SPARCV9

/* This is the value that we used before the ABI was released.  */
#define ELF_MACHINE_ALT1 EM_OLD_SPARCV9

#define bfd_elf64_bfd_link_hash_table_create \
  sparc64_elf_bfd_link_hash_table_create

#define elf_info_to_howto \
  sparc64_elf_info_to_howto
#define bfd_elf64_get_reloc_upper_bound \
  sparc64_elf_get_reloc_upper_bound
#define bfd_elf64_get_dynamic_reloc_upper_bound \
  sparc64_elf_get_dynamic_reloc_upper_bound
#define bfd_elf64_canonicalize_dynamic_reloc \
  sparc64_elf_canonicalize_dynamic_reloc
#define bfd_elf64_bfd_reloc_type_lookup \
  sparc64_elf_reloc_type_lookup
#define bfd_elf64_bfd_relax_section \
  sparc64_elf_relax_section

#define elf_backend_create_dynamic_sections \
  _bfd_elf_create_dynamic_sections
#define elf_backend_add_symbol_hook \
  sparc64_elf_add_symbol_hook
#define elf_backend_get_symbol_type \
  sparc64_elf_get_symbol_type
#define elf_backend_symbol_processing \
  sparc64_elf_symbol_processing
#define elf_backend_check_relocs \
  sparc64_elf_check_relocs
#define elf_backend_adjust_dynamic_symbol \
  sparc64_elf_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
  sparc64_elf_size_dynamic_sections
#define elf_backend_relocate_section \
  sparc64_elf_relocate_section
#define elf_backend_finish_dynamic_symbol \
  sparc64_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
  sparc64_elf_finish_dynamic_sections
#define elf_backend_print_symbol_all \
  sparc64_elf_print_symbol_all
#define elf_backend_output_arch_syms \
  sparc64_elf_output_arch_syms
#define bfd_elf64_bfd_merge_private_bfd_data \
  sparc64_elf_merge_private_bfd_data

#define elf_backend_size_info \
  sparc64_elf_size_info
#define elf_backend_object_p \
  sparc64_elf_object_p
#define elf_backend_reloc_type_class \
  sparc64_elf_reloc_type_class

#define elf_backend_want_got_plt 0
#define elf_backend_plt_readonly 0
#define elf_backend_want_plt_sym 1

/* Section 5.2.4 of the ABI specifies a 256-byte boundary for the table.  */
#define elf_backend_plt_alignment 8

#define elf_backend_got_header_size 8
#define elf_backend_plt_header_size PLT_HEADER_SIZE

#include "elf64-target.h"
