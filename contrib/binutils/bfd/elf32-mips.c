/* MIPS-specific support for 32-bit ELF
   Copyright 1993, 94, 95, 96, 97, 1998 Free Software Foundation, Inc.

   Most of the information added by Ian Lance Taylor, Cygnus Support,
   <ian@cygnus.com>.

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

/* This file handles MIPS ELF targets.  SGI Irix 5 uses a slightly
   different MIPS ELF from other targets.  This matters when linking.
   This file supports both, switching at runtime.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
#include "coff/mips.h"
#define ECOFF_32
#include "ecoffswap.h"

static bfd_reloc_status_type mips32_64bit_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static void mips_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static void bfd_mips_elf32_swap_gptab_in
  PARAMS ((bfd *, const Elf32_External_gptab *, Elf32_gptab *));
static void bfd_mips_elf32_swap_gptab_out
  PARAMS ((bfd *, const Elf32_gptab *, Elf32_External_gptab *));
static boolean mips_elf_sym_is_global PARAMS ((bfd *, asymbol *));
static boolean mips_elf32_object_p PARAMS ((bfd *));
static boolean mips_elf_create_procedure_table
  PARAMS ((PTR, bfd *, struct bfd_link_info *, asection *,
	   struct ecoff_debug_info *));
static int mips_elf_additional_program_headers PARAMS ((bfd *));
static boolean mips_elf_modify_segment_map PARAMS ((bfd *));
static INLINE int elf_mips_isa PARAMS ((flagword));
static boolean mips_elf32_section_from_shdr
  PARAMS ((bfd *, Elf32_Internal_Shdr *, char *));
static boolean mips_elf32_section_processing
  PARAMS ((bfd *, Elf32_Internal_Shdr *));
static boolean mips_elf_is_local_label_name
  PARAMS ((bfd *, const char *));
static struct bfd_hash_entry *mips_elf_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *mips_elf_link_hash_table_create
  PARAMS ((bfd *));
static int gptab_compare PARAMS ((const void *, const void *));
static boolean mips_elf_final_link
  PARAMS ((bfd *, struct bfd_link_info *));
static void mips_elf_relocate_hi16
  PARAMS ((bfd *, Elf_Internal_Rela *, Elf_Internal_Rela *, bfd_byte *,
	   bfd_vma));
static boolean mips_elf_relocate_got_local
  PARAMS ((bfd *, bfd *, asection *, Elf_Internal_Rela *,
	   Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static void mips_elf_relocate_global_got
   PARAMS ((bfd *, Elf_Internal_Rela *, bfd_byte *, bfd_vma));
static bfd_reloc_status_type mips16_jump_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips16_gprel_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean mips_elf_adjust_dynindx
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean mips_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *, Elf_Internal_Sym *, asection **));
static boolean mips_elf_link_output_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const char *, Elf_Internal_Sym *,
	   asection *));
static boolean mips_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_create_compact_rel_section
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_create_got_section
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static boolean mips_elf_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static boolean mips_elf_always_size_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static boolean mips_elf_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_add_symbol_hook
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Sym *,
	   const char **, flagword *, asection **, bfd_vma *));
static bfd_reloc_status_type mips_elf_final_gp
  PARAMS ((bfd *, asymbol *, boolean, char **, bfd_vma *));
static bfd_byte *elf32_mips_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));

/* This is true for Irix 5 executables, false for normal MIPS ELF ABI
   executables.  FIXME: At the moment, we default to always generating
   Irix 5 executables.  */

#define SGI_COMPAT(abfd) (1)

/* This structure is used to hold .got information when linking.  It
   is stored in the tdata field of the bfd_elf_section_data structure.  */

struct mips_got_info
{
  /* The symbol index of the first global .got symbol.  */
  unsigned long global_gotsym;
  /* The number of local .got entries.  */
  unsigned int local_gotno;
  /* The number of local .got entries we have used.  */
  unsigned int assigned_gotno;
};

/* The number of local .got entries we reserve.  */
#define MIPS_RESERVED_GOTNO (2)

/* Instructions which appear in a stub.  For some reason the stub is
   slightly different on an SGI system.  */
#define ELF_MIPS_GP_OFFSET(abfd) (SGI_COMPAT (abfd) ? 0x7ff0 : 0x8000)
#define STUB_LW(abfd)					\
  (SGI_COMPAT (abfd)					\
   ? 0x8f998010			/* lw t9,0x8010(gp) */	\
   : 0x8f998000)		/* lw t9,0x8000(gp) */
#define STUB_MOVE 0x03e07825	/* move t7,ra */
#define STUB_JALR 0x0320f809	/* jal t9 */
#define STUB_LI16 0x34180000	/* ori t8,zero,0 */
#define MIPS_FUNCTION_STUB_SIZE (16)

/* Names of sections which appear in the .dynsym section in an Irix 5
   executable.  */

static const char * const mips_elf_dynsym_sec_names[] =
{
  ".text",
  ".init",
  ".fini",
  ".data",
  ".rodata",
  ".sdata",
  ".sbss",
  ".bss",
  NULL
};

#define SIZEOF_MIPS_DYNSYM_SECNAMES \
  (sizeof mips_elf_dynsym_sec_names / sizeof mips_elf_dynsym_sec_names[0])

/* The number of entries in mips_elf_dynsym_sec_names which go in the
   text segment.  */

#define MIPS_TEXT_DYNSYM_SECNO (3)

/* The names of the runtime procedure table symbols used on Irix 5.  */

static const char * const mips_elf_dynsym_rtproc_names[] =
{
  "_procedure_table",
  "_procedure_string_table",
  "_procedure_table_size",
  NULL
};

/* These structures are used to generate the .compact_rel section on
   Irix 5.  */

typedef struct
{
  unsigned long id1;		/* Always one?  */
  unsigned long num;		/* Number of compact relocation entries.  */
  unsigned long id2;		/* Always two?  */
  unsigned long offset;		/* The file offset of the first relocation.  */
  unsigned long reserved0;	/* Zero?  */
  unsigned long reserved1;	/* Zero?  */
} Elf32_compact_rel;

typedef struct
{
  bfd_byte id1[4];
  bfd_byte num[4];
  bfd_byte id2[4];
  bfd_byte offset[4];
  bfd_byte reserved0[4];
  bfd_byte reserved1[4];
} Elf32_External_compact_rel;

typedef struct
{
  unsigned int ctype : 1;	/* 1: long 0: short format. See below.  */
  unsigned int rtype : 4;	/* Relocation types. See below. */
  unsigned int dist2to : 8;
  unsigned int relvaddr : 19;	/* (VADDR - vaddr of the previous entry)/ 4 */
  unsigned long konst;		/* KONST field. See below.  */
  unsigned long vaddr;		/* VADDR to be relocated.  */
} Elf32_crinfo;

typedef struct
{
  unsigned int ctype : 1;	/* 1: long 0: short format. See below.  */
  unsigned int rtype : 4;	/* Relocation types. See below. */
  unsigned int dist2to : 8;
  unsigned int relvaddr : 19;	/* (VADDR - vaddr of the previous entry)/ 4 */
  unsigned long konst;		/* KONST field. See below.  */
} Elf32_crinfo2;

typedef struct
{
  bfd_byte info[4];
  bfd_byte konst[4];
  bfd_byte vaddr[4];
} Elf32_External_crinfo;

typedef struct
{
  bfd_byte info[4];
  bfd_byte konst[4];
} Elf32_External_crinfo2;

/* These are the constants used to swap the bitfields in a crinfo.  */

#define CRINFO_CTYPE (0x1)
#define CRINFO_CTYPE_SH (31)
#define CRINFO_RTYPE (0xf)
#define CRINFO_RTYPE_SH (27)
#define CRINFO_DIST2TO (0xff)
#define CRINFO_DIST2TO_SH (19)
#define CRINFO_RELVADDR (0x7ffff)
#define CRINFO_RELVADDR_SH (0)

/* A compact relocation info has long (3 words) or short (2 words)
   formats.  A short format doesn't have VADDR field and relvaddr
   fields contains ((VADDR - vaddr of the previous entry) >> 2).  */
#define CRF_MIPS_LONG			1
#define CRF_MIPS_SHORT			0

/* There are 4 types of compact relocation at least. The value KONST
   has different meaning for each type:

   (type)		(konst)
   CT_MIPS_REL32	Address in data
   CT_MIPS_WORD		Address in word (XXX)
   CT_MIPS_GPHI_LO	GP - vaddr
   CT_MIPS_JMPAD	Address to jump
   */

#define CRT_MIPS_REL32			0xa
#define CRT_MIPS_WORD			0xb
#define CRT_MIPS_GPHI_LO		0xc
#define CRT_MIPS_JMPAD			0xd

#define mips_elf_set_cr_format(x,format)	((x).ctype = (format))
#define mips_elf_set_cr_type(x,type)		((x).rtype = (type))
#define mips_elf_set_cr_dist2to(x,v)		((x).dist2to = (v))
#define mips_elf_set_cr_relvaddr(x,d)		((x).relvaddr = (d)<<2)

static void bfd_elf32_swap_compact_rel_out
  PARAMS ((bfd *, const Elf32_compact_rel *, Elf32_External_compact_rel *));
static void bfd_elf32_swap_crinfo_out
  PARAMS ((bfd *, const Elf32_crinfo *, Elf32_External_crinfo *));

#define USE_REL	1		/* MIPS uses REL relocations instead of RELA */

enum reloc_type
{
  R_MIPS_NONE = 0,
  R_MIPS_16,		R_MIPS_32,
  R_MIPS_REL32,		R_MIPS_26,
  R_MIPS_HI16,		R_MIPS_LO16,
  R_MIPS_GPREL16,	R_MIPS_LITERAL,
  R_MIPS_GOT16,		R_MIPS_PC16,
  R_MIPS_CALL16,	R_MIPS_GPREL32,
  /* The remaining relocs are defined on Irix, although they are not
     in the MIPS ELF ABI.  */
  R_MIPS_UNUSED1,	R_MIPS_UNUSED2,
  R_MIPS_UNUSED3,
  R_MIPS_SHIFT5,	R_MIPS_SHIFT6,
  R_MIPS_64,		R_MIPS_GOT_DISP,
  R_MIPS_GOT_PAGE,	R_MIPS_GOT_OFST,
  R_MIPS_GOT_HI16,	R_MIPS_GOT_LO16,
  R_MIPS_SUB,		R_MIPS_INSERT_A,
  R_MIPS_INSERT_B,	R_MIPS_DELETE,
  R_MIPS_HIGHER,	R_MIPS_HIGHEST,
  R_MIPS_CALL_HI16,	R_MIPS_CALL_LO16,
  R_MIPS_max,
  /* These relocs are used for the mips16.  */
  R_MIPS16_26 = 100,
  R_MIPS16_GPREL = 101
};

static reloc_howto_type elf_mips_howto_table[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit branch address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_hi16_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_lo16_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_got16_reloc,	/* special_function */
	 "R_MIPS_GOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 _bfd_mips_elf_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

    /* The remaining relocs are defined on Irix 5, although they are
       not defined by the ABI.  */
    { 13 },
    { 14 },
    { 15 },

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 true,			/* partial_inplace */
	 0x000007c0,		/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  /* FIXME: This is not handled correctly; a special function is
     needed to put the most significant bit in the right place.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 true,			/* partial_inplace */
	 0x000007c4,		/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit relocation.  This is used in 32 bit ELF when addresses
     are 64 bits long; the upper 32 bits are simply a sign extension.
     The fields of the howto should be the same as for R_MIPS_32,
     other than the type, name, and special_function.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 mips32_64bit_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit subtraction.  Presumably not used in 32 bit ELF.  */
  { R_MIPS_SUB },

  /* Used to cause the linker to insert and delete instructions?  */
  { R_MIPS_INSERT_A },
  { R_MIPS_INSERT_B },
  { R_MIPS_DELETE },

  /* Get the higher values of a 64 bit addend.  Presumably not used in
     32 bit ELF.  */
  { R_MIPS_HIGHER },
  { R_MIPS_HIGHEST },

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */


};

/* The reloc used for BFD_RELOC_CTOR when doing a 64 bit link.  This
   is a hack to make the linker think that we need 64 bit values.  */
static reloc_howto_type elf_mips_ctor64_howto =
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips32_64bit_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false);		/* pcrel_offset */

/* The reloc used for the mips16 jump instruction.  */
static reloc_howto_type elf_mips16_jump_howto =
  HOWTO (R_MIPS16_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 mips16_jump_reloc,	/* special_function */
	 "R_MIPS16_26",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false);		/* pcrel_offset */

/* The reloc used for the mips16 gprel instruction.  The src_mask and
   dsk_mask for this howto do not reflect the actual instruction, in
   which the value is not contiguous; the masks are for the
   convenience of the relocate_section routine.  */
static reloc_howto_type elf_mips16_gprel_howto =
  HOWTO (R_MIPS16_GPREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips16_gprel_reloc,	/* special_function */
	 "R_MIPS16_GPREL",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false);		/* pcrel_offset */


/* Do a R_MIPS_HI16 relocation.  This has to be done in combination
   with a R_MIPS_LO16 reloc, because there is a carry from the LO16 to
   the HI16.  Here we just save the information we need; we do the
   actual relocation when we see the LO16.  MIPS ELF requires that the
   LO16 immediately follow the HI16.  As a GNU extension, we permit an
   arbitrary number of HI16 relocs to be associated with a single LO16
   reloc.  This extension permits gcc to output the HI and LO relocs
   itself.  */

struct mips_hi16
{
  struct mips_hi16 *next;
  bfd_byte *addr;
  bfd_vma addend;
};

/* FIXME: This should not be a static variable.  */

static struct mips_hi16 *mips_hi16_list;

bfd_reloc_status_type
_bfd_mips_elf_hi16_reloc (abfd,
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
     char **error_message;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  struct mips_hi16 *n;

  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  ret = bfd_reloc_ok;

  if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
    {
      boolean relocateable;
      bfd_vma gp;

      if (ret == bfd_reloc_undefined)
	abort ();

      if (output_bfd != NULL)
	relocateable = true;
      else
	{
	  relocateable = false;
	  output_bfd = symbol->section->output_section->owner;
	}

      ret = mips_elf_final_gp (output_bfd, symbol, relocateable,
			       error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;

      relocation = gp - reloc_entry->address;
    }
  else
    {
      if (bfd_is_und_section (symbol->section)
	  && output_bfd == (bfd *) NULL)
	ret = bfd_reloc_undefined;

      if (bfd_is_com_section (symbol->section))
	relocation = 0;
      else
	relocation = symbol->value;
    }

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Save the information, and let LO16 do the actual relocation.  */
  n = (struct mips_hi16 *) bfd_malloc (sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = mips_hi16_list;
  mips_hi16_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Do a R_MIPS_LO16 relocation.  This is a straightforward 16 bit
   inplace relocation; this function exists in order to do the
   R_MIPS_HI16 relocation described above.  */

bfd_reloc_status_type
_bfd_mips_elf_lo16_reloc (abfd,
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
     char **error_message;
{
  arelent gp_disp_relent;

  if (mips_hi16_list != NULL)
    {
      struct mips_hi16 *l;

      l = mips_hi16_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct mips_hi16 *next;

	  /* Do the HI16 relocation.  Note that we actually don't need
	     to know anything about the LO16 itself, except where to
	     find the low 16 bits of the addend needed by the LO16.  */
	  insn = bfd_get_32 (abfd, l->addr);
	  vallo = (bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address)
		   & 0xffff);
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* The low order 16 bits are always treated as a signed
	     value.  Therefore, a negative value in the low order bits
	     requires an adjustment in the high order bits.  We need
	     to make this adjustment in two ways: once for the bits we
	     took from the data, and once for the bits we are putting
	     back in to the data.  */
	  if ((vallo & 0x8000) != 0)
	    val -= 0x10000;
	  if ((val & 0x8000) != 0)
	    val += 0x10000;

	  insn = (insn &~ 0xffff) | ((val >> 16) & 0xffff);
	  bfd_put_32 (abfd, insn, l->addr);

	  if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
	    {
	      gp_disp_relent = *reloc_entry;
	      reloc_entry = &gp_disp_relent;
	      reloc_entry->addend = l->addend;
	    }

	  next = l->next;
	  free (l);
	  l = next;
	}

      mips_hi16_list = NULL;
    }
  else if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
    {
      bfd_reloc_status_type ret;
      bfd_vma gp, relocation;

      /* FIXME: Does this case ever occur?  */

      ret = mips_elf_final_gp (output_bfd, symbol, true, error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;

      relocation = gp - reloc_entry->address;
      relocation += symbol->section->output_section->vma;
      relocation += symbol->section->output_offset;
      relocation += reloc_entry->addend;

      if (reloc_entry->address > input_section->_cooked_size)
	return bfd_reloc_outofrange;

      gp_disp_relent = *reloc_entry;
      reloc_entry = &gp_disp_relent;
      reloc_entry->addend = relocation - 4;
    }

  /* Now do the LO16 reloc in the usual way.  */
  return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);
}

/* Do a R_MIPS_GOT16 reloc.  This is a reloc against the global offset
   table used for PIC code.  If the symbol is an external symbol, the
   instruction is modified to contain the offset of the appropriate
   entry in the global offset table.  If the symbol is a section
   symbol, the next reloc is a R_MIPS_LO16 reloc.  The two 16 bit
   addends are combined to form the real addend against the section
   symbol; the GOT16 is modified to contain the offset of an entry in
   the global offset table, and the LO16 is modified to offset it
   appropriately.  Thus an offset larger than 16 bits requires a
   modified value in the global offset table.

   This implementation suffices for the assembler, but the linker does
   not yet know how to create global offset tables.  */

bfd_reloc_status_type
_bfd_mips_elf_got16_reloc (abfd,
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
     char **error_message;
{
  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If we're relocating, and this is a local symbol, we can handle it
     just like HI16.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) != 0)
    return _bfd_mips_elf_hi16_reloc (abfd, reloc_entry, symbol, data,
				     input_section, output_bfd, error_message);

  abort ();
}

/* We have to figure out the gp value, so that we can adjust the
   symbol value correctly.  We look up the symbol _gp in the output
   BFD.  If we can't find it, we're stuck.  We cache it in the ELF
   target data.  We don't need to adjust the symbol value for an
   external symbol if we are producing relocateable output.  */

static bfd_reloc_status_type
mips_elf_final_gp (output_bfd, symbol, relocateable, error_message, pgp)
     bfd *output_bfd;
     asymbol *symbol;
     boolean relocateable;
     char **error_message;
     bfd_vma *pgp;
{
  if (bfd_is_und_section (symbol->section)
      && ! relocateable)
    {
      *pgp = 0;
      return bfd_reloc_undefined;
    }

  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp == 0
      && (! relocateable
	  || (symbol->flags & BSF_SECTION_SYM) != 0))
    {
      if (relocateable)
	{
	  /* Make up a value.  */
	  *pgp = symbol->section->output_section->vma + 0x4000;
	  _bfd_set_gp_value (output_bfd, *pgp);
	}
      else
	{
	  unsigned int count;
	  asymbol **sym;
	  unsigned int i;

	  count = bfd_get_symcount (output_bfd);
	  sym = bfd_get_outsymbols (output_bfd);

	  if (sym == (asymbol **) NULL)
	    i = count;
	  else
	    {
	      for (i = 0; i < count; i++, sym++)
		{
		  register CONST char *name;

		  name = bfd_asymbol_name (*sym);
		  if (*name == '_' && strcmp (name, "_gp") == 0)
		    {
		      *pgp = bfd_asymbol_value (*sym);
		      _bfd_set_gp_value (output_bfd, *pgp);
		      break;
		    }
		}
	    }

	  if (i >= count)
	    {
	      /* Only get the error once.  */
	      *pgp = 4;
	      _bfd_set_gp_value (output_bfd, *pgp);
	      *error_message =
		(char *) "GP relative relocation when _gp not defined";
	      return bfd_reloc_dangerous;
	    }
	}
    }

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL16 relocation.  This is a 16 bit value which must
   become the offset from the gp register.  This function also handles
   R_MIPS_LITERAL relocations, although those can be handled more
   cleverly because the entries in the .lit8 and .lit4 sections can be
   merged.  */

static bfd_reloc_status_type gprel16_with_gp PARAMS ((bfd *, asymbol *,
						      arelent *, asection *,
						      boolean, PTR, bfd_vma));

bfd_reloc_status_type
_bfd_mips_elf_gprel16_reloc (abfd, reloc_entry, symbol, data, input_section,
			     output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
			   &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  return gprel16_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, gp);
}

static bfd_reloc_status_type
gprel16_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long insn;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case occurs with the 64-bit MIPS ELF ABI.  */
      val = reloc_entry->addend;
    }
  else
    {
      val = ((insn & 0xffff) + reloc_entry->addend) & 0xffff;
      if (val & 0x8000)
	val -= 0x10000;
    }

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  insn = (insn &~ 0xffff) | (val & 0xffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  /* Make sure it fit in 16 bits.  */
  if (val >= 0x8000 && val < 0xffff8000)
    return bfd_reloc_overflow;

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL32 relocation.  Is this 32 bit value the offset
   from the gp register? XXX */

static bfd_reloc_status_type gprel32_with_gp PARAMS ((bfd *, asymbol *,
						      arelent *, asection *,
						      boolean, PTR, bfd_vma));

bfd_reloc_status_type
_bfd_mips_elf_gprel32_reloc (abfd,
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
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      *error_message = (char *)
	"32bits gp relative relocation occurs for an external symbol";
      return bfd_reloc_outofrange;
    }

  if (output_bfd != (bfd *) NULL)
    {
      relocateable = true;
      gp = _bfd_get_gp_value (output_bfd);
    }
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;

      ret = mips_elf_final_gp (output_bfd, symbol, relocateable,
			       error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;
    }

  return gprel32_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, gp);
}

static bfd_reloc_status_type
gprel32_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case arises with the 64-bit MIPS ELF ABI.  */
      val = 0;
    }
  else
    val = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  val += reloc_entry->addend;

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  bfd_put_32 (abfd, val, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  return bfd_reloc_ok;
}

/* Handle a 64 bit reloc in a 32 bit MIPS ELF file.  These are
   generated when addreses are 64 bits.  The upper 32 bits are a simle
   sign extension.  */

static bfd_reloc_status_type
mips32_64bit_reloc (abfd, reloc_entry, symbol, data, input_section,
		    output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_reloc_status_type r;
  arelent reloc32;
  unsigned long val;
  bfd_size_type addr;

  r = bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
			     input_section, output_bfd, error_message);
  if (r != bfd_reloc_continue)
    return r;

  /* Do a normal 32 bit relocation on the lower 32 bits.  */
  reloc32 = *reloc_entry;
  if (bfd_big_endian (abfd))
    reloc32.address += 4;
  reloc32.howto = &elf_mips_howto_table[R_MIPS_32];
  r = bfd_perform_relocation (abfd, &reloc32, data, input_section,
			      output_bfd, error_message);

  /* Sign extend into the upper 32 bits.  */
  val = bfd_get_32 (abfd, (bfd_byte *) data + reloc32.address);
  if ((val & 0x80000000) != 0)
    val = 0xffffffff;
  else
    val = 0;
  addr = reloc_entry->address;
  if (bfd_little_endian (abfd))
    addr += 4;
  bfd_put_32 (abfd, val, (bfd_byte *) data + addr);

  return r;
}

/* Handle a mips16 jump.  */

static bfd_reloc_status_type
mips16_jump_reloc (abfd, reloc_entry, symbol, data, input_section,
		   output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* FIXME.  */
  {
    static boolean warned;

    if (! warned)
      (*_bfd_error_handler)
	("Linking mips16 objects into %s format is not supported",
	 bfd_get_target (input_section->output_section->owner));
    warned = true;
  }

  return bfd_reloc_undefined;
}

/* Handle a mips16 GP relative reloc.  */

static bfd_reloc_status_type
mips16_gprel_reloc (abfd, reloc_entry, symbol, data, input_section,
		    output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;
  unsigned short extend, insn;
  unsigned long final;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
			   &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Pick up the mips16 extend instruction and the real instruction.  */
  extend = bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address);
  insn = bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address + 2);

  /* Stuff the current addend back as a 32 bit value, do the usual
     relocation, and then clean up.  */
  bfd_put_32 (abfd,
	      (((extend & 0x1f) << 11)
	       | (extend & 0x7e0)
	       | (insn & 0x1f)),
	      (bfd_byte *) data + reloc_entry->address);

  ret = gprel16_with_gp (abfd, symbol, reloc_entry, input_section,
			 relocateable, data, gp);

  final = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd,
	      ((extend & 0xf800)
	       | ((final >> 11) & 0x1f)
	       | (final & 0x7e0)),
	      (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd,
	      ((insn & 0xffe0)
	       | (final & 0x1f)),
	      (bfd_byte *) data + reloc_entry->address + 2);

  return ret;
}

/* Return the ISA for a MIPS e_flags value.  */

static INLINE int
elf_mips_isa (flags)
     flagword flags;
{
  switch (flags & EF_MIPS_ARCH)
    {
    case E_MIPS_ARCH_1:
      return 1;
    case E_MIPS_ARCH_2:
      return 2;
    case E_MIPS_ARCH_3:
      return 3;
    case E_MIPS_ARCH_4:
      return 4;
    }
  return 4;
}

/* A mapping from BFD reloc types to MIPS ELF reloc types.  */

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  enum reloc_type elf_reloc_val;
};

static CONST struct elf_reloc_map mips_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MIPS_NONE, },
  { BFD_RELOC_16, R_MIPS_16 },
  { BFD_RELOC_32, R_MIPS_32 },
  { BFD_RELOC_64, R_MIPS_64 },
  { BFD_RELOC_MIPS_JMP, R_MIPS_26 },
  { BFD_RELOC_HI16_S, R_MIPS_HI16 },
  { BFD_RELOC_LO16, R_MIPS_LO16 },
  { BFD_RELOC_MIPS_GPREL, R_MIPS_GPREL16 },
  { BFD_RELOC_MIPS_LITERAL, R_MIPS_LITERAL },
  { BFD_RELOC_MIPS_GOT16, R_MIPS_GOT16 },
  { BFD_RELOC_16_PCREL, R_MIPS_PC16 },
  { BFD_RELOC_MIPS_CALL16, R_MIPS_CALL16 },
  { BFD_RELOC_MIPS_GPREL32, R_MIPS_GPREL32 },
  { BFD_RELOC_MIPS_GOT_HI16, R_MIPS_GOT_HI16 },
  { BFD_RELOC_MIPS_GOT_LO16, R_MIPS_GOT_LO16 },
  { BFD_RELOC_MIPS_CALL_HI16, R_MIPS_CALL_HI16 },
  { BFD_RELOC_MIPS_CALL_LO16, R_MIPS_CALL_LO16 },
};

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (mips_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (mips_reloc_map[i].bfd_reloc_val == code)
	return &elf_mips_howto_table[(int) mips_reloc_map[i].elf_reloc_val];
    }

  /* We need to handle BFD_RELOC_CTOR specially.

     Select the right relocation (R_MIPS_32 or R_MIPS_64) based on the
     size of addresses on this architecture.  */
  if (code == BFD_RELOC_CTOR)
    {
      if (bfd_arch_bits_per_address (abfd) == 32)
	return &elf_mips_howto_table[(int) R_MIPS_32];
      else
	return &elf_mips_ctor64_howto;
    }

  /* Special handling for the MIPS16 relocs, since they are made up
     reloc types with a large value.  */
  if (code == BFD_RELOC_MIPS16_JMP)
    return &elf_mips16_jump_howto;
  else if (code == BFD_RELOC_MIPS16_GPREL)
    return &elf_mips16_gprel_howto;

  return NULL;
}

/* Given a MIPS reloc type, fill in an arelent structure.  */

static void
mips_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  if (r_type == R_MIPS16_26)
    cache_ptr->howto = &elf_mips16_jump_howto;
  else if (r_type == R_MIPS16_GPREL)
    cache_ptr->howto = &elf_mips16_gprel_howto;
  else
    {
      BFD_ASSERT (r_type < (unsigned int) R_MIPS_max);
      cache_ptr->howto = &elf_mips_howto_table[r_type];
    }

  /* The addend for a GPREL16 or LITERAL relocation comes from the GP
     value for the object file.  We get the addend now, rather than
     when we do the relocation, because the symbol manipulations done
     by the linker may cause us to lose track of the input BFD.  */
  if (((*cache_ptr->sym_ptr_ptr)->flags & BSF_SECTION_SYM) != 0
      && (r_type == (unsigned int) R_MIPS_GPREL16
	  || r_type == (unsigned int) R_MIPS_LITERAL))
    cache_ptr->addend = elf_gp (abfd);
}

/* A .reginfo section holds a single Elf32_RegInfo structure.  These
   routines swap this structure in and out.  They are used outside of
   BFD, so they are globally visible.  */

void
bfd_mips_elf32_swap_reginfo_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_RegInfo *ex;
     Elf32_RegInfo *in;
{
  in->ri_gprmask = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_gprmask);
  in->ri_cprmask[0] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[0]);
  in->ri_cprmask[1] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[1]);
  in->ri_cprmask[2] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[2]);
  in->ri_cprmask[3] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[3]);
  in->ri_gp_value = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_gp_value);
}

void
bfd_mips_elf32_swap_reginfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_RegInfo *in;
     Elf32_External_RegInfo *ex;
{
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_gprmask,
		(bfd_byte *) ex->ri_gprmask);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[0],
		(bfd_byte *) ex->ri_cprmask[0]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[1],
		(bfd_byte *) ex->ri_cprmask[1]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[2],
		(bfd_byte *) ex->ri_cprmask[2]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[3],
		(bfd_byte *) ex->ri_cprmask[3]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_gp_value,
		(bfd_byte *) ex->ri_gp_value);
}

/* In the 64 bit ABI, the .MIPS.options section holds register
   information in an Elf64_Reginfo structure.  These routines swap
   them in and out.  They are globally visible because they are used
   outside of BFD.  These routines are here so that gas can call them
   without worrying about whether the 64 bit ABI has been included.  */

void
bfd_mips_elf64_swap_reginfo_in (abfd, ex, in)
     bfd *abfd;
     const Elf64_External_RegInfo *ex;
     Elf64_Internal_RegInfo *in;
{
  in->ri_gprmask = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_gprmask);
  in->ri_pad = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_pad);
  in->ri_cprmask[0] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[0]);
  in->ri_cprmask[1] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[1]);
  in->ri_cprmask[2] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[2]);
  in->ri_cprmask[3] = bfd_h_get_32 (abfd, (bfd_byte *) ex->ri_cprmask[3]);
  in->ri_gp_value = bfd_h_get_64 (abfd, (bfd_byte *) ex->ri_gp_value);
}

void
bfd_mips_elf64_swap_reginfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf64_Internal_RegInfo *in;
     Elf64_External_RegInfo *ex;
{
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_gprmask,
		(bfd_byte *) ex->ri_gprmask);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_pad,
		(bfd_byte *) ex->ri_pad);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[0],
		(bfd_byte *) ex->ri_cprmask[0]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[1],
		(bfd_byte *) ex->ri_cprmask[1]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[2],
		(bfd_byte *) ex->ri_cprmask[2]);
  bfd_h_put_32 (abfd, (bfd_vma) in->ri_cprmask[3],
		(bfd_byte *) ex->ri_cprmask[3]);
  bfd_h_put_64 (abfd, (bfd_vma) in->ri_gp_value,
		(bfd_byte *) ex->ri_gp_value);
}

/* Swap an entry in a .gptab section.  Note that these routines rely
   on the equivalence of the two elements of the union.  */

static void
bfd_mips_elf32_swap_gptab_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_gptab *ex;
     Elf32_gptab *in;
{
  in->gt_entry.gt_g_value = bfd_h_get_32 (abfd, ex->gt_entry.gt_g_value);
  in->gt_entry.gt_bytes = bfd_h_get_32 (abfd, ex->gt_entry.gt_bytes);
}

static void
bfd_mips_elf32_swap_gptab_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_gptab *in;
     Elf32_External_gptab *ex;
{
  bfd_h_put_32 (abfd, (bfd_vma) in->gt_entry.gt_g_value,
		ex->gt_entry.gt_g_value);
  bfd_h_put_32 (abfd, (bfd_vma) in->gt_entry.gt_bytes,
		ex->gt_entry.gt_bytes);
}

static void
bfd_elf32_swap_compact_rel_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_compact_rel *in;
     Elf32_External_compact_rel *ex;
{
  bfd_h_put_32 (abfd, (bfd_vma) in->id1, ex->id1);
  bfd_h_put_32 (abfd, (bfd_vma) in->num, ex->num);
  bfd_h_put_32 (abfd, (bfd_vma) in->id2, ex->id2);
  bfd_h_put_32 (abfd, (bfd_vma) in->offset, ex->offset);
  bfd_h_put_32 (abfd, (bfd_vma) in->reserved0, ex->reserved0);
  bfd_h_put_32 (abfd, (bfd_vma) in->reserved1, ex->reserved1);
}

static void
bfd_elf32_swap_crinfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_crinfo *in;
     Elf32_External_crinfo *ex;
{
  unsigned long l;

  l = (((in->ctype & CRINFO_CTYPE) << CRINFO_CTYPE_SH)
       | ((in->rtype & CRINFO_RTYPE) << CRINFO_RTYPE_SH)
       | ((in->dist2to & CRINFO_DIST2TO) << CRINFO_DIST2TO_SH)
       | ((in->relvaddr & CRINFO_RELVADDR) << CRINFO_RELVADDR_SH));
  bfd_h_put_32 (abfd, (bfd_vma) l, ex->info);
  bfd_h_put_32 (abfd, (bfd_vma) in->konst, ex->konst);
  bfd_h_put_32 (abfd, (bfd_vma) in->vaddr, ex->vaddr);
}

/* Swap in an options header.  */

void
bfd_mips_elf_swap_options_in (abfd, ex, in)
     bfd *abfd;
     const Elf_External_Options *ex;
     Elf_Internal_Options *in;
{
  in->kind = bfd_h_get_8 (abfd, ex->kind);
  in->size = bfd_h_get_8 (abfd, ex->size);
  in->section = bfd_h_get_16 (abfd, ex->section);
  in->info = bfd_h_get_32 (abfd, ex->info);
}

/* Swap out an options header.  */

void
bfd_mips_elf_swap_options_out (abfd, in, ex)
     bfd *abfd;
     const Elf_Internal_Options *in;
     Elf_External_Options *ex;
{
  bfd_h_put_8 (abfd, in->kind, ex->kind);
  bfd_h_put_8 (abfd, in->size, ex->size);
  bfd_h_put_16 (abfd, in->section, ex->section);
  bfd_h_put_32 (abfd, in->info, ex->info);
}

/* Determine whether a symbol is global for the purposes of splitting
   the symbol table into global symbols and local symbols.  At least
   on Irix 5, this split must be between section symbols and all other
   symbols.  On most ELF targets the split is between static symbols
   and externally visible symbols.  */

/*ARGSUSED*/
static boolean
mips_elf_sym_is_global (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  return (sym->flags & BSF_SECTION_SYM) == 0 ? true : false;
}

/* Set the right machine number for a MIPS ELF file.  This is used for
   both the 32-bit and the 64-bit ABI.  */

boolean
_bfd_mips_elf_object_p (abfd)
     bfd *abfd;
{
  switch (elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH)
    {
    default:
    case E_MIPS_ARCH_1:
      (void) bfd_default_set_arch_mach (abfd, bfd_arch_mips, 3000);
      break;

    case E_MIPS_ARCH_2:
      (void) bfd_default_set_arch_mach (abfd, bfd_arch_mips, 6000);
      break;

    case E_MIPS_ARCH_3:
      (void) bfd_default_set_arch_mach (abfd, bfd_arch_mips, 4000);
      break;

    case E_MIPS_ARCH_4:
      (void) bfd_default_set_arch_mach (abfd, bfd_arch_mips, 8000);
      break;
    }

  return true;
}

/* Set the right machine number for a 32-bit MIPS ELF file.  */

static boolean
mips_elf32_object_p (abfd)
     bfd *abfd;
{
  /* Irix 5 is broken.  Object file symbol tables are not always
     sorted correctly such that local symbols precede global symbols,
     and the sh_info field in the symbol table is not always right.  */
  elf_bad_symtab (abfd) = true;

  return _bfd_mips_elf_object_p (abfd);
}

/* The final processing done just before writing out a MIPS ELF object
   file.  This gets the MIPS architecture right based on the machine
   number.  This is used by both the 32-bit and the 64-bit ABI.  */

/*ARGSUSED*/
void
_bfd_mips_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker;
{
  unsigned long val;
  unsigned int i;
  Elf_Internal_Shdr **hdrpp;
  const char *name;
  asection *sec;

  switch (bfd_get_mach (abfd))
    {
    case 3000:
      val = E_MIPS_ARCH_1;
      break;

    case 6000:
      val = E_MIPS_ARCH_2;
      break;

    case 4000:
      val = E_MIPS_ARCH_3;
      break;

    case 8000:
      val = E_MIPS_ARCH_4;
      break;

    default:
      val = 0;
      break;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_MIPS_ARCH;
  elf_elfheader (abfd)->e_flags |= val;

  /* Set the sh_info field for .gptab sections.  */
  for (i = 1, hdrpp = elf_elfsections (abfd) + 1;
       i < elf_elfheader (abfd)->e_shnum;
       i++, hdrpp++)
    {
      switch ((*hdrpp)->sh_type)
	{
	case SHT_MIPS_LIBLIST:
	  sec = bfd_get_section_by_name (abfd, ".dynstr");
	  if (sec != NULL)
	    (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_GPTAB:
	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL
		      && strncmp (name, ".gptab.", sizeof ".gptab." - 1) == 0);
	  sec = bfd_get_section_by_name (abfd, name + sizeof ".gptab" - 1);
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_info = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_CONTENT:
	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL
		      && strncmp (name, ".MIPS.content",
				  sizeof ".MIPS.content" - 1) == 0);
	  sec = bfd_get_section_by_name (abfd,
					 name + sizeof ".MIPS.content" - 1);
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_info = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_SYMBOL_LIB:
	  sec = bfd_get_section_by_name (abfd, ".dynsym");
	  if (sec != NULL)
	    (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  sec = bfd_get_section_by_name (abfd, ".liblist");
	  if (sec != NULL)
	    (*hdrpp)->sh_info = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_EVENTS:
	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL);
	  if (strncmp (name, ".MIPS.events", sizeof ".MIPS.events" - 1) == 0)
	    sec = bfd_get_section_by_name (abfd,
					   name + sizeof ".MIPS.events" - 1);
	  else
	    {
	      BFD_ASSERT (strncmp (name, ".MIPS.post_rel",
				   sizeof ".MIPS.post_rel" - 1) == 0);
	      sec = bfd_get_section_by_name (abfd,
					     (name
					      + sizeof ".MIPS.post_rel" - 1));
	    }
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  break;
	}
    }
}

/* Function to keep MIPS specific file flags like as EF_MIPS_PIC. */

boolean
_bfd_mips_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = true;
  return true;
}

/* Copy backend specific data from one object module to another */

boolean
_bfd_mips_elf_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || (elf_elfheader (obfd)->e_flags
		  == elf_elfheader (ibfd)->e_flags));

  elf_gp (obfd) = elf_gp (ibfd);
  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

boolean
_bfd_mips_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags;
  flagword new_flags;
  boolean ok;

  /* Check if we have the same endianess */
  if (ibfd->xvec->byteorder != obfd->xvec->byteorder
      && obfd->xvec->byteorder != BFD_ENDIAN_UNKNOWN)
    {
      (*_bfd_error_handler)
	("%s: compiled for a %s endian system and target is %s endian",
	 bfd_get_filename (ibfd),
	 bfd_big_endian (ibfd) ? "big" : "little",
	 bfd_big_endian (obfd) ? "big" : "little");

      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  elf_elfheader (obfd)->e_flags |= new_flags & EF_MIPS_NOREORDER;
  old_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	{
	  if (! bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
				   bfd_get_mach (ibfd)))
	    return false;
	}

      return true;
    }

  /* Check flag compatibility.  */

  new_flags &= ~EF_MIPS_NOREORDER;
  old_flags &= ~EF_MIPS_NOREORDER;

  if (new_flags == old_flags)
    return true;

  ok = true;

  if ((new_flags & EF_MIPS_PIC) != (old_flags & EF_MIPS_PIC))
    {
      new_flags &= ~EF_MIPS_PIC;
      old_flags &= ~EF_MIPS_PIC;
      (*_bfd_error_handler)
	("%s: linking PIC files with non-PIC files",
	 bfd_get_filename (ibfd));
      ok = false;
    }

  if ((new_flags & EF_MIPS_CPIC) != (old_flags & EF_MIPS_CPIC))
    {
      new_flags &= ~EF_MIPS_CPIC;
      old_flags &= ~EF_MIPS_CPIC;
      (*_bfd_error_handler)
	("%s: linking abicalls files with non-abicalls files",
	 bfd_get_filename (ibfd));
      ok = false;
    }

  /* Don't warn about mixing -mips1 and -mips2 code, or mixing -mips3
     and -mips4 code.  They will normally use the same data sizes and
     calling conventions.  */
  if ((new_flags & EF_MIPS_ARCH) != (old_flags & EF_MIPS_ARCH))
    {
      int new_isa, old_isa;

      new_isa = elf_mips_isa (new_flags);
      old_isa = elf_mips_isa (old_flags);
      if ((new_isa == 1 || new_isa == 2)
	  ? (old_isa != 1 && old_isa != 2)
	  : (old_isa == 1 || old_isa == 2))
	{
	  (*_bfd_error_handler)
	    ("%s: ISA mismatch (-mips%d) with previous modules (-mips%d)",
	     bfd_get_filename (ibfd), new_isa, old_isa);
	  ok = false;
	}

      new_flags &= ~ EF_MIPS_ARCH;
      old_flags &= ~ EF_MIPS_ARCH;
    }

  /* Warn about any other mismatches */
  if (new_flags != old_flags)
    {
      (*_bfd_error_handler)
	("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)",
	 bfd_get_filename (ibfd), (unsigned long) new_flags,
	 (unsigned long) old_flags);
      ok = false;
    }

  if (! ok)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  return true;
}

/* Handle a MIPS specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.
   This routine supports both the 32-bit and 64-bit ELF ABI.

   FIXME: We need to handle the SHF_MIPS_GPREL flag, but I'm not sure
   how to.  */

boolean
_bfd_mips_elf_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     const char *name;
{
  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  Fortunately, the ABI gives
     suggested names for all the MIPS specific sections, so we will
     probably get away with this.  */
  switch (hdr->sh_type)
    {
    case SHT_MIPS_LIBLIST:
      if (strcmp (name, ".liblist") != 0)
	return false;
      break;
    case SHT_MIPS_MSYM:
      if (strcmp (name, ".msym") != 0)
	return false;
      break;
    case SHT_MIPS_CONFLICT:
      if (strcmp (name, ".conflict") != 0)
	return false;
      break;
    case SHT_MIPS_GPTAB:
      if (strncmp (name, ".gptab.", sizeof ".gptab." - 1) != 0)
	return false;
      break;
    case SHT_MIPS_UCODE:
      if (strcmp (name, ".ucode") != 0)
	return false;
      break;
    case SHT_MIPS_DEBUG:
      if (strcmp (name, ".mdebug") != 0)
	return false;
      break;
    case SHT_MIPS_REGINFO:
      if (strcmp (name, ".reginfo") != 0
	  || hdr->sh_size != sizeof (Elf32_External_RegInfo))
	return false;
      break;
    case SHT_MIPS_IFACE:
      if (strcmp (name, ".MIPS.interfaces") != 0)
	return false;
      break;
    case SHT_MIPS_CONTENT:
      if (strncmp (name, ".MIPS.content", sizeof ".MIPS.content" - 1) != 0)
	return false;
      break;
    case SHT_MIPS_OPTIONS:
      if (strcmp (name, ".options") != 0
	  && strcmp (name, ".MIPS.options") != 0)
	return false;
      break;
    case SHT_MIPS_DWARF:
      if (strncmp (name, ".debug_", sizeof ".debug_" - 1) != 0)
	return false;
      break;
    case SHT_MIPS_SYMBOL_LIB:
      if (strcmp (name, ".MIPS.symlib") != 0)
	return false;
      break;
    case SHT_MIPS_EVENTS:
      if (strncmp (name, ".MIPS.events", sizeof ".MIPS.events" - 1) != 0
	  && strncmp (name, ".MIPS.post_rel",
		      sizeof ".MIPS.post_rel" - 1) != 0)
	return false;
      break;
    default:
      return false;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;

  if (hdr->sh_type == SHT_MIPS_DEBUG)
    {
      if (! bfd_set_section_flags (abfd, hdr->bfd_section,
				   (bfd_get_section_flags (abfd,
							   hdr->bfd_section)
				    | SEC_DEBUGGING)))
	return false;
    }

  return true;
}

/* Handle a 32-bit MIPS ELF specific section.  */

static boolean
mips_elf32_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     char *name;
{
  if (! _bfd_mips_elf_section_from_shdr (abfd, hdr, name))
    return false;

  /* FIXME: We should record sh_info for a .gptab section.  */

  /* For a .reginfo section, set the gp value in the tdata information
     from the contents of this section.  We need the gp value while
     processing relocs, so we just get it now.  The .reginfo section
     is not used in the 64-bit MIPS ELF ABI.  */
  if (hdr->sh_type == SHT_MIPS_REGINFO)
    {
      Elf32_External_RegInfo ext;
      Elf32_RegInfo s;

      if (! bfd_get_section_contents (abfd, hdr->bfd_section, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
      bfd_mips_elf32_swap_reginfo_in (abfd, &ext, &s);
      elf_gp (abfd) = s.ri_gp_value;
    }

  /* For a SHT_MIPS_OPTIONS section, look for a ODK_REGINFO entry, and
     set the gp value based on what we find.  We may see both
     SHT_MIPS_REGINFO and SHT_MIPS_OPTIONS/ODK_REGINFO; in that case,
     they should agree.  */
  if (hdr->sh_type == SHT_MIPS_OPTIONS)
    {
      bfd_byte *contents, *l, *lend;

      contents = (bfd_byte *) bfd_malloc (hdr->sh_size);
      if (contents == NULL)
	return false;
      if (! bfd_get_section_contents (abfd, hdr->bfd_section, contents,
				      (file_ptr) 0, hdr->sh_size))
	{
	  free (contents);
	  return false;
	}
      l = contents;
      lend = contents + hdr->sh_size;
      while (l + sizeof (Elf_External_Options) <= lend)
	{
	  Elf_Internal_Options intopt;

	  bfd_mips_elf_swap_options_in (abfd, (Elf_External_Options *) l,
					&intopt);
	  if (intopt.kind == ODK_REGINFO)
	    {
	      Elf32_RegInfo intreg;

	      bfd_mips_elf32_swap_reginfo_in
		(abfd,
		 ((Elf32_External_RegInfo *)
		  (l + sizeof (Elf_External_Options))),
		 &intreg);
	      elf_gp (abfd) = intreg.ri_gp_value;
	    }
	  l += intopt.size;
	}
      free (contents);
    }

  return true;
}

/* Set the correct type for a MIPS ELF section.  We do this by the
   section name, which is a hack, but ought to work.  This routine is
   used by both the 32-bit and the 64-bit ABI.  */

boolean
_bfd_mips_elf_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".liblist") == 0)
    {
      hdr->sh_type = SHT_MIPS_LIBLIST;
      hdr->sh_info = sec->_raw_size / sizeof (Elf32_Lib);
      /* The sh_link field is set in final_write_processing.  */
    }
  else if (strcmp (name, ".msym") == 0)
    {
      hdr->sh_type = SHT_MIPS_MSYM;
      hdr->sh_entsize = 8;
      /* FIXME: Set the sh_info field.  */
    }
  else if (strcmp (name, ".conflict") == 0)
    hdr->sh_type = SHT_MIPS_CONFLICT;
  else if (strncmp (name, ".gptab.", sizeof ".gptab." - 1) == 0)
    {
      hdr->sh_type = SHT_MIPS_GPTAB;
      hdr->sh_entsize = sizeof (Elf32_External_gptab);
      /* The sh_info field is set in final_write_processing.  */
    }
  else if (strcmp (name, ".ucode") == 0)
    hdr->sh_type = SHT_MIPS_UCODE;
  else if (strcmp (name, ".mdebug") == 0)
    {
      hdr->sh_type = SHT_MIPS_DEBUG;
      /* In a shared object on Irix 5.3, the .mdebug section has an
         entsize of 0.  FIXME: Does this matter?  */
      if (SGI_COMPAT (abfd) && (abfd->flags & DYNAMIC) != 0)
	hdr->sh_entsize = 0;
      else
	hdr->sh_entsize = 1;
    }
  else if (strcmp (name, ".reginfo") == 0)
    {
      hdr->sh_type = SHT_MIPS_REGINFO;
      /* In a shared object on Irix 5.3, the .reginfo section has an
         entsize of 0x18.  FIXME: Does this matter?  */
      if (SGI_COMPAT (abfd) && (abfd->flags & DYNAMIC) != 0)
	hdr->sh_entsize = sizeof (Elf32_External_RegInfo);
      else
	hdr->sh_entsize = 1;

      /* Force the section size to the correct value, even if the
	 linker thinks it is larger.  The link routine below will only
	 write out this much data for .reginfo.  */
      hdr->sh_size = sec->_raw_size = sizeof (Elf32_External_RegInfo);
    }
  else if (SGI_COMPAT (abfd)
	   && (strcmp (name, ".hash") == 0
	       || strcmp (name, ".dynamic") == 0
	       || strcmp (name, ".dynstr") == 0))
    {
      hdr->sh_entsize = 0;
      hdr->sh_info = SIZEOF_MIPS_DYNSYM_SECNAMES;
    }
  else if (strcmp (name, ".got") == 0
	   || strcmp (name, ".sdata") == 0
	   || strcmp (name, ".sbss") == 0
	   || strcmp (name, ".lit4") == 0
	   || strcmp (name, ".lit8") == 0)
    hdr->sh_flags |= SHF_MIPS_GPREL;
  else if (strcmp (name, ".MIPS.interfaces") == 0)
    {
      hdr->sh_type = SHT_MIPS_IFACE;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
    }
  else if (strcmp (name, ".MIPS.content") == 0)
    {
      hdr->sh_type = SHT_MIPS_CONTENT;
      /* The sh_info field is set in final_write_processing.  */
    }
  else if (strcmp (name, ".options") == 0
	   || strcmp (name, ".MIPS.options") == 0)
    {
      hdr->sh_type = SHT_MIPS_OPTIONS;
      hdr->sh_entsize = 1;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
    }
  else if (strncmp (name, ".debug_", sizeof ".debug_" - 1) == 0)
    hdr->sh_type = SHT_MIPS_DWARF;
  else if (strcmp (name, ".MIPS.symlib") == 0)
    {
      hdr->sh_type = SHT_MIPS_SYMBOL_LIB;
      /* The sh_link and sh_info fields are set in
         final_write_processing.  */
    }
  else if (strncmp (name, ".MIPS.events", sizeof ".MIPS.events" - 1) == 0
	   || strncmp (name, ".MIPS.post_rel",
		       sizeof ".MIPS.post_rel" - 1) == 0)
    {
      hdr->sh_type = SHT_MIPS_EVENTS;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
      /* The sh_link field is set in final_write_processing.  */
    }

  return true;
}

/* Given a BFD section, try to locate the corresponding ELF section
   index.  This is used by both the 32-bit and the 64-bit ABI.
   Actually, it's not clear to me that the 64-bit ABI supports these,
   but for non-PIC objects we will certainly want support for at least
   the .scommon section.  */

boolean
_bfd_mips_elf_section_from_bfd_section (abfd, hdr, sec, retval)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     asection *sec;
     int *retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    {
      *retval = SHN_MIPS_SCOMMON;
      return true;
    }
  if (strcmp (bfd_get_section_name (abfd, sec), ".acommon") == 0)
    {
      *retval = SHN_MIPS_ACOMMON;
      return true;
    }
  return false;
}

/* When are writing out the .options or .MIPS.options section,
   remember the bytes we are writing out, so that we can install the
   GP value in the section_processing routine.  */

boolean
_bfd_mips_elf_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (strcmp (section->name, ".options") == 0
      || strcmp (section->name, ".MIPS.options") == 0)
    {
      bfd_byte *c;

      if (elf_section_data (section) == NULL)
	{
	  section->used_by_bfd =
	    (PTR) bfd_zalloc (abfd, sizeof (struct bfd_elf_section_data));
	  if (elf_section_data (section) == NULL)
	    return false;
	}
      c = (bfd_byte *) elf_section_data (section)->tdata;
      if (c == NULL)
	{
	  bfd_size_type size;

	  if (section->_cooked_size != 0)
	    size = section->_cooked_size;
	  else
	    size = section->_raw_size;
	  c = (bfd_byte *) bfd_zalloc (abfd, size);
	  if (c == NULL)
	    return false;
	  elf_section_data (section)->tdata = (PTR) c;
	}

      memcpy (c + offset, location, count);
    }

  return _bfd_elf_set_section_contents (abfd, section, location, offset,
					count);
}

/* Work over a section just before writing it out.  This routine is
   used by both the 32-bit and the 64-bit ABI.  FIXME: We recognize
   sections that need the SHF_MIPS_GPREL flag by name; there has to be
   a better way.  */

boolean
_bfd_mips_elf_section_processing (abfd, hdr)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
{
  if (hdr->bfd_section != NULL)
    {
      const char *name = bfd_get_section_name (abfd, hdr->bfd_section);

      if (strcmp (name, ".sdata") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".sbss") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_NOBITS;
	}
      else if (strcmp (name, ".lit8") == 0
	       || strcmp (name, ".lit4") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".compact_rel") == 0)
	{
	  hdr->sh_flags = 0;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".rtproc") == 0)
	{
	  if (hdr->sh_addralign != 0 && hdr->sh_entsize == 0)
	    {
	      unsigned int adjust;

	      adjust = hdr->sh_size % hdr->sh_addralign;
	      if (adjust != 0)
		hdr->sh_size += hdr->sh_addralign - adjust;
	    }
	}
    }

  return true;
}

/* Work over a section just before writing it out.  We update the GP
   value in the SHT_MIPS_REGINFO and SHT_MIPS_OPTIONS sections based
   on the value we are using.  */

static boolean
mips_elf32_section_processing (abfd, hdr)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
{
  if (hdr->sh_type == SHT_MIPS_REGINFO)
    {
      bfd_byte buf[4];

      BFD_ASSERT (hdr->sh_size == sizeof (Elf32_External_RegInfo));
      BFD_ASSERT (hdr->contents == NULL);

      if (bfd_seek (abfd,
		    hdr->sh_offset + sizeof (Elf32_External_RegInfo) - 4,
		    SEEK_SET) == -1)
	return false;
      bfd_h_put_32 (abfd, (bfd_vma) elf_gp (abfd), buf);
      if (bfd_write (buf, (bfd_size_type) 1, (bfd_size_type) 4, abfd) != 4)
	return false;
    }

  if (hdr->sh_type == SHT_MIPS_OPTIONS
      && hdr->bfd_section != NULL
      && elf_section_data (hdr->bfd_section) != NULL
      && elf_section_data (hdr->bfd_section)->tdata != NULL)
    {
      bfd_byte *contents, *l, *lend;

      /* We stored the section contents in the elf_section_data tdata
	 field in the set_section_contents routine.  We save the
	 section contents so that we don't have to read them again.
	 At this point we know that elf_gp is set, so we can look
	 through the section contents to see if there is an
	 ODK_REGINFO structure.  */

      contents = (bfd_byte *) elf_section_data (hdr->bfd_section)->tdata;
      l = contents;
      lend = contents + hdr->sh_size;
      while (l + sizeof (Elf_External_Options) <= lend)
	{
	  Elf_Internal_Options intopt;

	  bfd_mips_elf_swap_options_in (abfd, (Elf_External_Options *) l,
					&intopt);
	  if (intopt.kind == ODK_REGINFO)
	    {
	      bfd_byte buf[4];

	      if (bfd_seek (abfd,
			    (hdr->sh_offset
			     + (l - contents)
			     + sizeof (Elf_External_Options)
			     + (sizeof (Elf32_External_RegInfo) - 4)),
			     SEEK_SET) == -1)
		return false;
	      bfd_h_put_32 (abfd, elf_gp (abfd), buf);
	      if (bfd_write (buf, 1, 4, abfd) != 4)
		return false;
	    }
	  l += intopt.size;
	}
    }

  return _bfd_mips_elf_section_processing (abfd, hdr);
}

/* MIPS ELF uses two common sections.  One is the usual one, and the
   other is for small objects.  All the small objects are kept
   together, and then referenced via the gp pointer, which yields
   faster assembler code.  This is what we use for the small common
   section.  This approach is copied from ecoff.c.  */
static asection mips_elf_scom_section;
static asymbol mips_elf_scom_symbol;
static asymbol *mips_elf_scom_symbol_ptr;

/* MIPS ELF also uses an acommon section, which represents an
   allocated common symbol which may be overridden by a
   definition in a shared library.  */
static asection mips_elf_acom_section;
static asymbol mips_elf_acom_symbol;
static asymbol *mips_elf_acom_symbol_ptr;

/* The Irix 5 support uses two virtual sections, which represent
   text/data symbols defined in dynamic objects.  */
static asection mips_elf_text_section;
static asection *mips_elf_text_section_ptr;
static asymbol mips_elf_text_symbol;
static asymbol *mips_elf_text_symbol_ptr;

static asection mips_elf_data_section;
static asection *mips_elf_data_section_ptr;
static asymbol mips_elf_data_symbol;
static asymbol *mips_elf_data_symbol_ptr;

/* Handle the special MIPS section numbers that a symbol may use.
   This is used for both the 32-bit and the 64-bit ABI.  */

void
_bfd_mips_elf_symbol_processing (abfd, asym)
     bfd *abfd;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;
  switch (elfsym->internal_elf_sym.st_shndx)
    {
    case SHN_MIPS_ACOMMON:
      /* This section is used in a dynamically linked executable file.
	 It is an allocated common section.  The dynamic linker can
	 either resolve these symbols to something in a shared
	 library, or it can just leave them here.  For our purposes,
	 we can consider these symbols to be in a new section.  */
      if (mips_elf_acom_section.name == NULL)
	{
	  /* Initialize the acommon section.  */
	  mips_elf_acom_section.name = ".acommon";
	  mips_elf_acom_section.flags = SEC_ALLOC;
	  mips_elf_acom_section.output_section = &mips_elf_acom_section;
	  mips_elf_acom_section.symbol = &mips_elf_acom_symbol;
	  mips_elf_acom_section.symbol_ptr_ptr = &mips_elf_acom_symbol_ptr;
	  mips_elf_acom_symbol.name = ".acommon";
	  mips_elf_acom_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_acom_symbol.section = &mips_elf_acom_section;
	  mips_elf_acom_symbol_ptr = &mips_elf_acom_symbol;
	}
      asym->section = &mips_elf_acom_section;
      break;

    case SHN_COMMON:
      /* Common symbols less than the GP size are automatically
	 treated as SHN_MIPS_SCOMMON symbols.  */
      if (asym->value > elf_gp_size (abfd))
	break;
      /* Fall through.  */
    case SHN_MIPS_SCOMMON:
      if (mips_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  mips_elf_scom_section.name = ".scommon";
	  mips_elf_scom_section.flags = SEC_IS_COMMON;
	  mips_elf_scom_section.output_section = &mips_elf_scom_section;
	  mips_elf_scom_section.symbol = &mips_elf_scom_symbol;
	  mips_elf_scom_section.symbol_ptr_ptr = &mips_elf_scom_symbol_ptr;
	  mips_elf_scom_symbol.name = ".scommon";
	  mips_elf_scom_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_scom_symbol.section = &mips_elf_scom_section;
	  mips_elf_scom_symbol_ptr = &mips_elf_scom_symbol;
	}
      asym->section = &mips_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_MIPS_SUNDEFINED:
      asym->section = bfd_und_section_ptr;
      break;

#if 0 /* for SGI_COMPAT */
    case SHN_MIPS_TEXT:
      asym->section = mips_elf_text_section_ptr;
      break;

    case SHN_MIPS_DATA:
      asym->section = mips_elf_data_section_ptr;
      break;
#endif
    }
}

/* When creating an Irix 5 executable, we need REGINFO and RTPROC
   segments.  */

static int
mips_elf_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;
  int ret;

  ret = 0;

  if (! SGI_COMPAT (abfd))
    return ret;

  s = bfd_get_section_by_name (abfd, ".reginfo");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      /* We need a PT_MIPS_REGINFO segment.  */
      ++ret;
    }

  if (bfd_get_section_by_name (abfd, ".dynamic") != NULL
      && bfd_get_section_by_name (abfd, ".mdebug") != NULL)
    {
      /* We need a PT_MIPS_RTPROC segment.  */
      ++ret;
    }

  return ret;
}

/* Modify the segment map for an Irix 5 executable.  */

static boolean
mips_elf_modify_segment_map (abfd)
     bfd *abfd;
{
  asection *s;
  struct elf_segment_map *m, **pm;

  if (! SGI_COMPAT (abfd))
    return true;

  /* If there is a .reginfo section, we need a PT_MIPS_REGINFO
     segment.  */
  s = bfd_get_section_by_name (abfd, ".reginfo");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	if (m->p_type == PT_MIPS_REGINFO)
	  break;
      if (m == NULL)
	{
	  m = (struct elf_segment_map *) bfd_zalloc (abfd, sizeof *m);
	  if (m == NULL)
	    return false;

	  m->p_type = PT_MIPS_REGINFO;
	  m->count = 1;
	  m->sections[0] = s;

	  /* We want to put it after the PHDR and INTERP segments.  */
	  pm = &elf_tdata (abfd)->segment_map;
	  while (*pm != NULL
		 && ((*pm)->p_type == PT_PHDR
		     || (*pm)->p_type == PT_INTERP))
	    pm = &(*pm)->next;

	  m->next = *pm;
	  *pm = m;
	}
    }

  /* If there are .dynamic and .mdebug sections, we make a room for
     the RTPROC header.  FIXME: Rewrite without section names.  */
  if (bfd_get_section_by_name (abfd, ".interp") == NULL
      && bfd_get_section_by_name (abfd, ".dynamic") != NULL
      && bfd_get_section_by_name (abfd, ".mdebug") != NULL)
    {
      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	if (m->p_type == PT_MIPS_RTPROC)
	  break;
      if (m == NULL)
	{
	  m = (struct elf_segment_map *) bfd_zalloc (abfd, sizeof *m);
	  if (m == NULL)
	    return false;

	  m->p_type = PT_MIPS_RTPROC;

	  s = bfd_get_section_by_name (abfd, ".rtproc");
	  if (s == NULL)
	    {
	      m->count = 0;
	      m->p_flags = 0;
	      m->p_flags_valid = 1;
	    }
	  else
	    {
	      m->count = 1;
	      m->sections[0] = s;
	    }

	  /* We want to put it after the DYNAMIC segment.  */
	  pm = &elf_tdata (abfd)->segment_map;
	  while (*pm != NULL && (*pm)->p_type != PT_DYNAMIC)
	    pm = &(*pm)->next;
	  if (*pm != NULL)
	    pm = &(*pm)->next;

	  m->next = *pm;
	  *pm = m;
	}
    }

  /* On Irix 5, the PT_DYNAMIC segment includes the .dynamic, .dynstr,
     .dynsym, and .hash sections, and everything in between.  */
  for (pm = &elf_tdata (abfd)->segment_map; *pm != NULL; pm = &(*pm)->next)
    if ((*pm)->p_type == PT_DYNAMIC)
      break;
  m = *pm;
  if (m != NULL
      && m->count == 1
      && strcmp (m->sections[0]->name, ".dynamic") == 0)
    {
      static const char *sec_names[] =
	{ ".dynamic", ".dynstr", ".dynsym", ".hash" };
      bfd_vma low, high;
      unsigned int i, c;
      struct elf_segment_map *n;

      low = 0xffffffff;
      high = 0;
      for (i = 0; i < sizeof sec_names / sizeof sec_names[0]; i++)
	{
	  s = bfd_get_section_by_name (abfd, sec_names[i]);
	  if (s != NULL && (s->flags & SEC_LOAD) != 0)
	    {
	      bfd_size_type sz;

	      if (low > s->vma)
		low = s->vma;
	      sz = s->_cooked_size;
	      if (sz == 0)
		sz = s->_raw_size;
	      if (high < s->vma + sz)
		high = s->vma + sz;
	    }
	}

      c = 0;
      for (s = abfd->sections; s != NULL; s = s->next)
	if ((s->flags & SEC_LOAD) != 0
	    && s->vma >= low
	    && ((s->vma
		 + (s->_cooked_size != 0 ? s->_cooked_size : s->_raw_size))
		<= high))
	  ++c;

      n = ((struct elf_segment_map *)
	   bfd_zalloc (abfd, sizeof *n + (c - 1) * sizeof (asection *)));
      if (n == NULL)
	return false;
      *n = *m;
      n->count = c;

      i = 0;
      for (s = abfd->sections; s != NULL; s = s->next)
	{
	  if ((s->flags & SEC_LOAD) != 0
	      && s->vma >= low
	      && ((s->vma
		   + (s->_cooked_size != 0 ? s->_cooked_size : s->_raw_size))
		  <= high))
	    {
	      n->sections[i] = s;
	      ++i;
	    }
	}

      *pm = n;
    }

  return true;
}

/* The structure of the runtime procedure descriptor created by the
   loader for use by the static exception system.  */

typedef struct runtime_pdr {
	bfd_vma	adr;		/* memory address of start of procedure */
	long	regmask;	/* save register mask */
	long	regoffset;	/* save register offset */
	long	fregmask;	/* save floating point register mask */
	long	fregoffset;	/* save floating point register offset */
	long	frameoffset;	/* frame size */
	short	framereg;	/* frame pointer register */
	short	pcreg;		/* offset or reg of return pc */
	long	irpss;		/* index into the runtime string table */
	long	reserved;
	struct exception_info *exception_info;/* pointer to exception array */
} RPDR, *pRPDR;
#define cbRPDR sizeof(RPDR)
#define rpdNil ((pRPDR) 0)

/* Swap RPDR (runtime procedure table entry) for output.  */

static void ecoff_swap_rpdr_out
  PARAMS ((bfd *, const RPDR *, struct rpdr_ext *));

static void
ecoff_swap_rpdr_out (abfd, in, ex)
     bfd *abfd;
     const RPDR *in;
     struct rpdr_ext *ex;
{
  /* ecoff_put_off was defined in ecoffswap.h.  */
  ecoff_put_off (abfd, in->adr, (bfd_byte *) ex->p_adr);
  bfd_h_put_32 (abfd, in->regmask, (bfd_byte *) ex->p_regmask);
  bfd_h_put_32 (abfd, in->regoffset, (bfd_byte *) ex->p_regoffset);
  bfd_h_put_32 (abfd, in->fregmask, (bfd_byte *) ex->p_fregmask);
  bfd_h_put_32 (abfd, in->fregoffset, (bfd_byte *) ex->p_fregoffset);
  bfd_h_put_32 (abfd, in->frameoffset, (bfd_byte *) ex->p_frameoffset);

  bfd_h_put_16 (abfd, in->framereg, (bfd_byte *) ex->p_framereg);
  bfd_h_put_16 (abfd, in->pcreg, (bfd_byte *) ex->p_pcreg);

  bfd_h_put_32 (abfd, in->irpss, (bfd_byte *) ex->p_irpss);
#if 0 /* FIXME */
  ecoff_put_off (abfd, in->exception_info, (bfd_byte *) ex->p_exception_info);
#endif
}

/* Read ECOFF debugging information from a .mdebug section into a
   ecoff_debug_info structure.  */

boolean
_bfd_mips_elf_read_ecoff_info (abfd, section, debug)
     bfd *abfd;
     asection *section;
     struct ecoff_debug_info *debug;
{
  HDRR *symhdr;
  const struct ecoff_debug_swap *swap;
  char *ext_hdr = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

  ext_hdr = (char *) bfd_malloc ((size_t) swap->external_hdr_size);
  if (ext_hdr == NULL && swap->external_hdr_size != 0)
    goto error_return;

  if (bfd_get_section_contents (abfd, section, ext_hdr, (file_ptr) 0,
				swap->external_hdr_size)
      == false)
    goto error_return;

  symhdr = &debug->symbolic_header;
  (*swap->swap_hdr_in) (abfd, ext_hdr, symhdr);

  /* The symbolic header contains absolute file offsets and sizes to
     read.  */
#define READ(ptr, offset, count, size, type)				\
  if (symhdr->count == 0)						\
    debug->ptr = NULL;							\
  else									\
    {									\
      debug->ptr = (type) bfd_malloc ((size_t) (size * symhdr->count));	\
      if (debug->ptr == NULL)						\
	goto error_return;						\
      if (bfd_seek (abfd, (file_ptr) symhdr->offset, SEEK_SET) != 0	\
	  || (bfd_read (debug->ptr, size, symhdr->count,		\
			abfd) != size * symhdr->count))			\
	goto error_return;						\
    }

  READ (line, cbLineOffset, cbLine, sizeof (unsigned char), unsigned char *);
  READ (external_dnr, cbDnOffset, idnMax, swap->external_dnr_size, PTR);
  READ (external_pdr, cbPdOffset, ipdMax, swap->external_pdr_size, PTR);
  READ (external_sym, cbSymOffset, isymMax, swap->external_sym_size, PTR);
  READ (external_opt, cbOptOffset, ioptMax, swap->external_opt_size, PTR);
  READ (external_aux, cbAuxOffset, iauxMax, sizeof (union aux_ext),
	union aux_ext *);
  READ (ss, cbSsOffset, issMax, sizeof (char), char *);
  READ (ssext, cbSsExtOffset, issExtMax, sizeof (char), char *);
  READ (external_fdr, cbFdOffset, ifdMax, swap->external_fdr_size, PTR);
  READ (external_rfd, cbRfdOffset, crfd, swap->external_rfd_size, PTR);
  READ (external_ext, cbExtOffset, iextMax, swap->external_ext_size, PTR);
#undef READ

  debug->fdr = NULL;
  debug->adjust = NULL;

  return true;

 error_return:
  if (ext_hdr != NULL)
    free (ext_hdr);
  if (debug->line != NULL)
    free (debug->line);
  if (debug->external_dnr != NULL)
    free (debug->external_dnr);
  if (debug->external_pdr != NULL)
    free (debug->external_pdr);
  if (debug->external_sym != NULL)
    free (debug->external_sym);
  if (debug->external_opt != NULL)
    free (debug->external_opt);
  if (debug->external_aux != NULL)
    free (debug->external_aux);
  if (debug->ss != NULL)
    free (debug->ss);
  if (debug->ssext != NULL)
    free (debug->ssext);
  if (debug->external_fdr != NULL)
    free (debug->external_fdr);
  if (debug->external_rfd != NULL)
    free (debug->external_rfd);
  if (debug->external_ext != NULL)
    free (debug->external_ext);
  return false;
}

/* MIPS ELF local labels start with '$', not 'L'.  */

/*ARGSUSED*/
static boolean
mips_elf_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == '$')
    return true;

  /* On Irix 6, the labels go back to starting with '.', so we accept
     the generic ELF local label syntax as well.  */
  return _bfd_elf_is_local_label_name (abfd, name);
}

/* MIPS ELF uses a special find_nearest_line routine in order the
   handle the ECOFF debugging information.  */

struct mips_elf_find_line
{
  struct ecoff_debug_info d;
  struct ecoff_find_line i;
};

boolean
_bfd_mips_elf_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
				 functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *line_ptr;
{
  asection *msec;

  if (_bfd_dwarf2_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr, 
				     line_ptr))
    return true;

  msec = bfd_get_section_by_name (abfd, ".mdebug");
  if (msec != NULL)
    {
      flagword origflags;
      struct mips_elf_find_line *fi;
      const struct ecoff_debug_swap * const swap =
	get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

      /* If we are called during a link, mips_elf_final_link may have
	 cleared the SEC_HAS_CONTENTS field.  We force it back on here
	 if appropriate (which it normally will be).  */
      origflags = msec->flags;
      if (elf_section_data (msec)->this_hdr.sh_type != SHT_NOBITS)
	msec->flags |= SEC_HAS_CONTENTS;

      fi = elf_tdata (abfd)->find_line_info;
      if (fi == NULL)
	{
	  bfd_size_type external_fdr_size;
	  char *fraw_src;
	  char *fraw_end;
	  struct fdr *fdr_ptr;

	  fi = ((struct mips_elf_find_line *)
		bfd_zalloc (abfd, sizeof (struct mips_elf_find_line)));
	  if (fi == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  if (! _bfd_mips_elf_read_ecoff_info (abfd, msec, &fi->d))
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  /* Swap in the FDR information.  */
	  fi->d.fdr = ((struct fdr *)
		       bfd_alloc (abfd,
				  (fi->d.symbolic_header.ifdMax *
				   sizeof (struct fdr))));
	  if (fi->d.fdr == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }
	  external_fdr_size = swap->external_fdr_size;
	  fdr_ptr = fi->d.fdr;
	  fraw_src = (char *) fi->d.external_fdr;
	  fraw_end = (fraw_src
		      + fi->d.symbolic_header.ifdMax * external_fdr_size);
	  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
	    (*swap->swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

	  elf_tdata (abfd)->find_line_info = fi;

	  /* Note that we don't bother to ever free this information.
             find_nearest_line is either called all the time, as in
             objdump -l, so the information should be saved, or it is
             rarely called, as in ld error messages, so the memory
             wasted is unimportant.  Still, it would probably be a
             good idea for free_cached_info to throw it away.  */
	}

      if (_bfd_ecoff_locate_line (abfd, section, offset, &fi->d, swap,
				  &fi->i, filename_ptr, functionname_ptr,
				  line_ptr))
	{
	  msec->flags = origflags;
	  return true;
	}

      msec->flags = origflags;
    }

  /* Fall back on the generic ELF find_nearest_line routine.  */

  return _bfd_elf_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr);
}

  /* The mips16 compiler uses a couple of special sections to handle
     floating point arguments.

     Section names that look like .mips16.fn.FNNAME contain stubs that
     copy floating point arguments from the fp regs to the gp regs and
     then jump to FNNAME.  If any 32 bit function calls FNNAME, the
     call should be redirected to the stub instead.  If no 32 bit
     function calls FNNAME, the stub should be discarded.  We need to
     consider any reference to the function, not just a call, because
     if the address of the function is taken we will need the stub,
     since the address might be passed to a 32 bit function.

     Section names that look like .mips16.call.FNNAME contain stubs
     that copy floating point arguments from the gp regs to the fp
     regs and then jump to FNNAME.  If FNNAME is a 32 bit function,
     then any 16 bit function that calls FNNAME should be redirected
     to the stub instead.  If FNNAME is not a 32 bit function, the
     stub should be discarded.

     .mips16.call.fp.FNNAME sections are similar, but contain stubs
     which call FNNAME and then copy the return value from the fp regs
     to the gp regs.  These stubs store the return value in $18 while
     calling FNNAME; any function which might call one of these stubs
     must arrange to save $18 around the call.  (This case is not
     needed for 32 bit functions that call 16 bit functions, because
     16 bit functions always return floating point values in both
     $f0/$f1 and $2/$3.)

     Note that in all cases FNNAME might be defined statically.
     Therefore, FNNAME is not used literally.  Instead, the relocation
     information will indicate which symbol the section is for.

     We record any stubs that we find in the symbol table.  */

#define FN_STUB ".mips16.fn."
#define CALL_STUB ".mips16.call."
#define CALL_FP_STUB ".mips16.call.fp."

/* The MIPS ELF linker needs additional information for each symbol in
   the global hash table.  */

struct mips_elf_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* External symbol information.  */
  EXTR esym;

  /* Number of MIPS_32 or MIPS_REL32 relocs against this symbol.  */
  unsigned int mips_32_relocs;

  /* If there is a stub that 32 bit functions should use to call this
     16 bit function, this points to the section containing the stub.  */
  asection *fn_stub;

  /* Whether we need the fn_stub; this is set if this symbol appears
     in any relocs other than a 16 bit call.  */
  boolean need_fn_stub;

  /* If there is a stub that 16 bit functions should use to call this
     32 bit function, this points to the section containing the stub.  */
  asection *call_stub;

  /* This is like the call_stub field, but it is used if the function
     being called returns a floating point value.  */
  asection *call_fp_stub;
};

/* MIPS ELF linker hash table.  */

struct mips_elf_link_hash_table
{
  struct elf_link_hash_table root;
  /* String section indices for the dynamic section symbols.  */
  bfd_size_type dynsym_sec_strindex[SIZEOF_MIPS_DYNSYM_SECNAMES];
  /* The number of .rtproc entries.  */
  bfd_size_type procedure_count;
  /* The size of the .compact_rel section (if SGI_COMPAT).  */
  bfd_size_type compact_rel_size;
  /* This flag indicates that the value of DT_MIPS_RLD_MAP dynamic
     entry is set to the address of __rld_obj_head as in Irix 5. */
  boolean use_rld_obj_head;
  /* This is the value of the __rld_map or __rld_obj_head symbol.  */
  bfd_vma rld_value;
  /* This is set if we see any mips16 stub sections. */
  boolean mips16_stubs_seen;
};

/* Look up an entry in a MIPS ELF linker hash table.  */

#define mips_elf_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct mips_elf_link_hash_entry *)					\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a MIPS ELF linker hash table.  */

#define mips_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the MIPS ELF linker hash table from a link_info structure.  */

#define mips_elf_hash_table(p) \
  ((struct mips_elf_link_hash_table *) ((p)->hash))

static boolean mips_elf_output_extsym
  PARAMS ((struct mips_elf_link_hash_entry *, PTR));

/* Create an entry in a MIPS ELF linker hash table.  */

static struct bfd_hash_entry *
mips_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct mips_elf_link_hash_entry *ret =
    (struct mips_elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct mips_elf_link_hash_entry *) NULL)
    ret = ((struct mips_elf_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct mips_elf_link_hash_entry)));
  if (ret == (struct mips_elf_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct mips_elf_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct mips_elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      memset (&ret->esym, 0, sizeof (EXTR));
      /* We use -2 as a marker to indicate that the information has
	 not been set.  -1 means there is no associated ifd.  */
      ret->esym.ifd = -2;
      ret->mips_32_relocs = 0;
      ret->fn_stub = NULL;
      ret->need_fn_stub = false;
      ret->call_stub = NULL;
      ret->call_fp_stub = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create a MIPS ELF linker hash table.  */

static struct bfd_link_hash_table *
mips_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct mips_elf_link_hash_table *ret;
  unsigned int i;

  ret = ((struct mips_elf_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct mips_elf_link_hash_table)));
  if (ret == (struct mips_elf_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       mips_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  for (i = 0; i < SIZEOF_MIPS_DYNSYM_SECNAMES; i++)
    ret->dynsym_sec_strindex[i] = (bfd_size_type) -1;
  ret->procedure_count = 0;
  ret->compact_rel_size = 0;
  ret->use_rld_obj_head = false;
  ret->rld_value = 0;
  ret->mips16_stubs_seen = false;

  return &ret->root.root;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special MIPS section numbers here.  */

/*ARGSUSED*/
static boolean
mips_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp;
     asection **secp;
     bfd_vma *valp;
{
  if (SGI_COMPAT (abfd)
      && (abfd->flags & DYNAMIC) != 0
      && strcmp (*namep, "_rld_new_interface") == 0)
    {
      /* Skip Irix 5 rld entry name.  */
      *namep = NULL;
      return true;
    }

  switch (sym->st_shndx)
    {
    case SHN_COMMON:
      /* Common symbols less than the GP size are automatically
	 treated as SHN_MIPS_SCOMMON symbols.  */
      if (sym->st_size > elf_gp_size (abfd))
	break;
      /* Fall through.  */
    case SHN_MIPS_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_MIPS_TEXT:
      /* This section is used in a shared object.  */
      if (mips_elf_text_section_ptr == NULL)
	{
	  /* Initialize the section.  */
	  mips_elf_text_section.name = ".text";
	  mips_elf_text_section.flags = SEC_NO_FLAGS;
	  mips_elf_text_section.output_section = NULL;
	  mips_elf_text_section.symbol = &mips_elf_text_symbol;
	  mips_elf_text_section.symbol_ptr_ptr = &mips_elf_text_symbol_ptr;
	  mips_elf_text_symbol.name = ".text";
	  mips_elf_text_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_text_symbol.section = &mips_elf_text_section;
	  mips_elf_text_symbol_ptr = &mips_elf_text_symbol;
	  mips_elf_text_section_ptr = &mips_elf_text_section;
	}
      /* This code used to do *secp = bfd_und_section_ptr if
         info->shared.  I don't know why, and that doesn't make sense,
         so I took it out.  */
      *secp = mips_elf_text_section_ptr;
      break;

    case SHN_MIPS_ACOMMON:
      /* Fall through. XXX Can we treat this as allocated data?  */
    case SHN_MIPS_DATA:
      /* This section is used in a shared object.  */
      if (mips_elf_data_section_ptr == NULL)
	{
	  /* Initialize the section.  */
	  mips_elf_data_section.name = ".data";
	  mips_elf_data_section.flags = SEC_NO_FLAGS;
	  mips_elf_data_section.output_section = NULL;
	  mips_elf_data_section.symbol = &mips_elf_data_symbol;
	  mips_elf_data_section.symbol_ptr_ptr = &mips_elf_data_symbol_ptr;
	  mips_elf_data_symbol.name = ".data";
	  mips_elf_data_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_data_symbol.section = &mips_elf_data_section;
	  mips_elf_data_symbol_ptr = &mips_elf_data_symbol;
	  mips_elf_data_section_ptr = &mips_elf_data_section;
	}
      /* This code used to do *secp = bfd_und_section_ptr if
         info->shared.  I don't know why, and that doesn't make sense,
         so I took it out.  */
      *secp = mips_elf_data_section_ptr;
      break;

    case SHN_MIPS_SUNDEFINED:
      *secp = bfd_und_section_ptr;
      break;
    }

  if (SGI_COMPAT (abfd)
      && ! info->shared
      && info->hash->creator == abfd->xvec
      && strcmp (*namep, "__rld_obj_head") == 0)
    {
      struct elf_link_hash_entry *h;

      /* Mark __rld_obj_head as dynamic.  */
      h = NULL;
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, *namep, BSF_GLOBAL, *secp,
	      (bfd_vma) *valp, (const char *) NULL, false,
	      get_elf_backend_data (abfd)->collect,
	      (struct bfd_link_hash_entry **) &h)))
	return false;
      h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	return false;

      mips_elf_hash_table (info)->use_rld_obj_head = true;
    }

  /* If this is a mips16 text symbol, add 1 to the value to make it
     odd.  This will cause something like .word SYM to come up with
     the right value when it is loaded into the PC.  */
  if (sym->st_other == STO_MIPS16)
    ++*valp;

  return true;
}

/* Structure used to pass information to mips_elf_output_extsym.  */

struct extsym_info
{
  bfd *abfd;
  struct bfd_link_info *info;
  struct ecoff_debug_info *debug;
  const struct ecoff_debug_swap *swap;
  boolean failed;
};

/* This routine is used to write out ECOFF debugging external symbol
   information.  It is called via mips_elf_link_hash_traverse.  The
   ECOFF external symbol information must match the ELF external
   symbol information.  Unfortunately, at this point we don't know
   whether a symbol is required by reloc information, so the two
   tables may wind up being different.  We must sort out the external
   symbol information before we can set the final size of the .mdebug
   section, and we must set the size of the .mdebug section before we
   can relocate any sections, and we can't know which symbols are
   required by relocation until we relocate the sections.
   Fortunately, it is relatively unlikely that any symbol will be
   stripped but required by a reloc.  In particular, it can not happen
   when generating a final executable.  */

static boolean
mips_elf_output_extsym (h, data)
     struct mips_elf_link_hash_entry *h;
     PTR data;
{
  struct extsym_info *einfo = (struct extsym_info *) data;
  boolean strip;
  asection *sec, *output_section;

  if (h->root.indx == -2)
    strip = false;
  else if (((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (einfo->info->strip == strip_all
	   || (einfo->info->strip == strip_some
	       && bfd_hash_lookup (einfo->info->keep_hash,
				   h->root.root.root.string,
				   false, false) == NULL))
    strip = true;
  else
    strip = false;

  if (strip)
    return true;

  if (h->esym.ifd == -2)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (SGI_COMPAT (einfo->abfd)
	  && (h->root.root.type == bfd_link_hash_undefined
	      || h->root.root.type == bfd_link_hash_undefweak))
	{
	  const char *name;

	  /* Use undefined class.  Also, set class and type for some
             special symbols.  */
	  name = h->root.root.root.string;
	  if (strcmp (name, mips_elf_dynsym_rtproc_names[0]) == 0
	      || strcmp (name, mips_elf_dynsym_rtproc_names[1]) == 0)
	    {
	      h->esym.asym.sc = scData;
	      h->esym.asym.st = stLabel;
	      h->esym.asym.value = 0;
	    }
	  else if (strcmp (name, mips_elf_dynsym_rtproc_names[2]) == 0)
	    {
	      h->esym.asym.sc = scAbs;
	      h->esym.asym.st = stLabel;
	      h->esym.asym.value =
		mips_elf_hash_table (einfo->info)->procedure_count;
	    }
	  else if (strcmp (name, "_gp_disp") == 0)
	    {
	      h->esym.asym.sc = scAbs;
	      h->esym.asym.st = stLabel;
	      h->esym.asym.value = elf_gp (einfo->abfd);
	    }
	  else
	    h->esym.asym.sc = scUndefined;
	}
      else if (h->root.root.type != bfd_link_hash_defined
	  && h->root.root.type != bfd_link_hash_defweak)
	h->esym.asym.sc = scAbs;
      else
	{
	  const char *name;

	  sec = h->root.root.u.def.section;
	  output_section = sec->output_section;

	  /* When making a shared library and symbol h is the one from
	     the another shared library, OUTPUT_SECTION may be null.  */
	  if (output_section == NULL)
	    h->esym.asym.sc = scUndefined;
	  else
	    {
	      name = bfd_section_name (output_section->owner, output_section);

	      if (strcmp (name, ".text") == 0)
		h->esym.asym.sc = scText;
	      else if (strcmp (name, ".data") == 0)
		h->esym.asym.sc = scData;
	      else if (strcmp (name, ".sdata") == 0)
		h->esym.asym.sc = scSData;
	      else if (strcmp (name, ".rodata") == 0
		       || strcmp (name, ".rdata") == 0)
		h->esym.asym.sc = scRData;
	      else if (strcmp (name, ".bss") == 0)
		h->esym.asym.sc = scBss;
	      else if (strcmp (name, ".sbss") == 0)
		h->esym.asym.sc = scSBss;
	      else if (strcmp (name, ".init") == 0)
		h->esym.asym.sc = scInit;
	      else if (strcmp (name, ".fini") == 0)
		h->esym.asym.sc = scFini;
	      else
		h->esym.asym.sc = scAbs;
	    }
	}

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }

  if (h->root.root.type == bfd_link_hash_common)
    h->esym.asym.value = h->root.root.u.c.size;
  else if (h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
    {
      if (h->esym.asym.sc == scCommon)
	h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
	h->esym.asym.sc = scSBss;

      sec = h->root.root.u.def.section;
      output_section = sec->output_section;
      if (output_section != NULL)
	h->esym.asym.value = (h->root.root.u.def.value
			      + sec->output_offset
			      + output_section->vma);
      else
	h->esym.asym.value = 0;
    }
  else if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      /* Set type and value for a symbol with a function stub.  */
      h->esym.asym.st = stProc;
      sec = h->root.root.u.def.section;
      if (sec == NULL)
	h->esym.asym.value = 0;
      else
	{
	  output_section = sec->output_section;
	  if (output_section != NULL)
	    h->esym.asym.value = (h->root.plt_offset
				  + sec->output_offset
				  + output_section->vma);
	  else
	    h->esym.asym.value = 0;
	}
#if 0 /* FIXME?  */
      h->esym.ifd = 0;
#endif
    }

  if (! bfd_ecoff_debug_one_external (einfo->abfd, einfo->debug, einfo->swap,
				      h->root.root.root.string,
				      &h->esym))
    {
      einfo->failed = true;
      return false;
    }

  return true;
}

/* Create a runtime procedure table from the .mdebug section.  */

static boolean
mips_elf_create_procedure_table (handle, abfd, info, s, debug)
     PTR handle;
     bfd *abfd;
     struct bfd_link_info *info;
     asection *s;
     struct ecoff_debug_info *debug;
{
  const struct ecoff_debug_swap *swap;
  HDRR *hdr = &debug->symbolic_header;
  RPDR *rpdr, *rp;
  struct rpdr_ext *erp;
  PTR rtproc;
  struct pdr_ext *epdr;
  struct sym_ext *esym;
  char *ss, **sv;
  char *str;
  unsigned long size, count;
  unsigned long sindex;
  unsigned long i;
  PDR pdr;
  SYMR sym;
  const char *no_name_func = "static procedure (no name)";

  epdr = NULL;
  rpdr = NULL;
  esym = NULL;
  ss = NULL;
  sv = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

  sindex = strlen (no_name_func) + 1;
  count = hdr->ipdMax;
  if (count > 0)
    {
      size = swap->external_pdr_size;

      epdr = (struct pdr_ext *) bfd_malloc (size * count);
      if (epdr == NULL)
	goto error_return;

      if (! _bfd_ecoff_get_accumulated_pdr (handle, (PTR) epdr))
	goto error_return;

      size = sizeof (RPDR);
      rp = rpdr = (RPDR *) bfd_malloc (size * count);
      if (rpdr == NULL)
	goto error_return;

      sv = (char **) bfd_malloc (sizeof (char *) * count);
      if (sv == NULL)
	goto error_return;

      count = hdr->isymMax;
      size = swap->external_sym_size;
      esym = (struct sym_ext *) bfd_malloc (size * count);
      if (esym == NULL)
	goto error_return;

      if (! _bfd_ecoff_get_accumulated_sym (handle, (PTR) esym))
	goto error_return;

      count = hdr->issMax;
      ss = (char *) bfd_malloc (count);
      if (ss == NULL)
	goto error_return;
      if (! _bfd_ecoff_get_accumulated_ss (handle, (PTR) ss))
	goto error_return;

      count = hdr->ipdMax;
      for (i = 0; i < count; i++, rp++)
	{
	  (*swap->swap_pdr_in) (abfd, (PTR) (epdr + i), &pdr);
	  (*swap->swap_sym_in) (abfd, (PTR) &esym[pdr.isym], &sym);
	  rp->adr = sym.value;
	  rp->regmask = pdr.regmask;
	  rp->regoffset = pdr.regoffset;
	  rp->fregmask = pdr.fregmask;
	  rp->fregoffset = pdr.fregoffset;
	  rp->frameoffset = pdr.frameoffset;
	  rp->framereg = pdr.framereg;
	  rp->pcreg = pdr.pcreg;
	  rp->irpss = sindex;
	  sv[i] = ss + sym.iss;
	  sindex += strlen (sv[i]) + 1;
	}
    }

  size = sizeof (struct rpdr_ext) * (count + 2) + sindex;
  size = BFD_ALIGN (size, 16);
  rtproc = (PTR) bfd_alloc (abfd, size);
  if (rtproc == NULL)
    {
      mips_elf_hash_table (info)->procedure_count = 0;
      goto error_return;
    }

  mips_elf_hash_table (info)->procedure_count = count + 2;

  erp = (struct rpdr_ext *) rtproc;
  memset (erp, 0, sizeof (struct rpdr_ext));
  erp++;
  str = (char *) rtproc + sizeof (struct rpdr_ext) * (count + 2);
  strcpy (str, no_name_func);
  str += strlen (no_name_func) + 1;
  for (i = 0; i < count; i++)
    {
      ecoff_swap_rpdr_out (abfd, rpdr + i, erp + i);
      strcpy (str, sv[i]);
      str += strlen (sv[i]) + 1;
    }
  ecoff_put_off (abfd, (bfd_vma) -1, (bfd_byte *) (erp + count)->p_adr);

  /* Set the size and contents of .rtproc section.  */
  s->_raw_size = size;
  s->contents = (bfd_byte *) rtproc;

  /* Skip this section later on (I don't think this currently
     matters, but someday it might).  */
  s->link_order_head = (struct bfd_link_order *) NULL;

  if (epdr != NULL)
    free (epdr);
  if (rpdr != NULL)
    free (rpdr);
  if (esym != NULL)
    free (esym);
  if (ss != NULL)
    free (ss);
  if (sv != NULL)
    free (sv);

  return true;

 error_return:
  if (epdr != NULL)
    free (epdr);
  if (rpdr != NULL)
    free (rpdr);
  if (esym != NULL)
    free (esym);
  if (ss != NULL)
    free (ss);
  if (sv != NULL)
    free (sv);
  return false;
}

/* A comparison routine used to sort .gptab entries.  */

static int
gptab_compare (p1, p2)
     const PTR p1;
     const PTR p2;
{
  const Elf32_gptab *a1 = (const Elf32_gptab *) p1;
  const Elf32_gptab *a2 = (const Elf32_gptab *) p2;

  return a1->gt_entry.gt_g_value - a2->gt_entry.gt_g_value;
}

/* We need to use a special link routine to handle the .reginfo and
   the .mdebug sections.  We need to merge all instances of these
   sections together, not write them all out sequentially.  */

static boolean
mips_elf_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection **secpp;
  asection *o;
  struct bfd_link_order *p;
  asection *reginfo_sec, *mdebug_sec, *gptab_data_sec, *gptab_bss_sec;
  asection *rtproc_sec;
  Elf32_RegInfo reginfo;
  struct ecoff_debug_info debug;
  const struct ecoff_debug_swap *swap
    = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  HDRR *symhdr = &debug.symbolic_header;
  PTR mdebug_handle = NULL;

  /* Drop the .options section, since it has special semantics which I
     haven't bothered to figure out.  */
  for (secpp = &abfd->sections; *secpp != NULL; secpp = &(*secpp)->next)
    {
      if (strcmp ((*secpp)->name, ".options") == 0)
	{
	  for (p = (*secpp)->link_order_head; p != NULL; p = p->next)
	    if (p->type == bfd_indirect_link_order)
	      p->u.indirect.section->flags &=~ SEC_HAS_CONTENTS;
	  (*secpp)->link_order_head = NULL;
	  *secpp = (*secpp)->next;
	  --abfd->section_count;
	  break;
	}
    }

  /* Get a value for the GP register.  */
  if (elf_gp (abfd) == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_gp", false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	elf_gp (abfd) = (h->u.def.value
			 + h->u.def.section->output_section->vma
			 + h->u.def.section->output_offset);
      else if (info->relocateable)
	{
	  bfd_vma lo;

	  /* Make up a value.  */
	  lo = (bfd_vma) -1;
	  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	    {
	      if (o->vma < lo
		  && (strcmp (o->name, ".sbss") == 0
		      || strcmp (o->name, ".sdata") == 0
		      || strcmp (o->name, ".lit4") == 0
		      || strcmp (o->name, ".lit8") == 0))
		lo = o->vma;
	    }
	  elf_gp (abfd) = lo + ELF_MIPS_GP_OFFSET (abfd);
	}
      else
	{
	  /* If the relocate_section function needs to do a reloc
	     involving the GP value, it should make a reloc_dangerous
	     callback to warn that GP is not defined.  */
	}
    }

  /* Go through the sections and collect the .reginfo and .mdebug
     information.  */
  reginfo_sec = NULL;
  mdebug_sec = NULL;
  gptab_data_sec = NULL;
  gptab_bss_sec = NULL;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      if (strcmp (o->name, ".reginfo") == 0)
	{
	  memset (&reginfo, 0, sizeof reginfo);

	  /* We have found the .reginfo section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      Elf32_External_RegInfo ext;
	      Elf32_RegInfo sub;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* The linker emulation code has probably clobbered the
                 size to be zero bytes.  */
	      if (input_section->_raw_size == 0)
		input_section->_raw_size = sizeof (Elf32_External_RegInfo);

	      if (! bfd_get_section_contents (input_bfd, input_section,
					      (PTR) &ext,
					      (file_ptr) 0,
					      sizeof ext))
		return false;

	      bfd_mips_elf32_swap_reginfo_in (input_bfd, &ext, &sub);

	      reginfo.ri_gprmask |= sub.ri_gprmask;
	      reginfo.ri_cprmask[0] |= sub.ri_cprmask[0];
	      reginfo.ri_cprmask[1] |= sub.ri_cprmask[1];
	      reginfo.ri_cprmask[2] |= sub.ri_cprmask[2];
	      reginfo.ri_cprmask[3] |= sub.ri_cprmask[3];

	      /* ri_gp_value is set by the function
		 mips_elf32_section_processing when the section is
		 finally written out.  */

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* Force the section size to the value we want.  */
	  o->_raw_size = sizeof (Elf32_External_RegInfo);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  reginfo_sec = o;
	}

      if (strcmp (o->name, ".mdebug") == 0)
	{
	  struct extsym_info einfo;

	  /* We have found the .mdebug section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  symhdr->magic = swap->sym_magic;
	  /* FIXME: What should the version stamp be?  */
	  symhdr->vstamp = 0;
	  symhdr->ilineMax = 0;
	  symhdr->cbLine = 0;
	  symhdr->idnMax = 0;
	  symhdr->ipdMax = 0;
	  symhdr->isymMax = 0;
	  symhdr->ioptMax = 0;
	  symhdr->iauxMax = 0;
	  symhdr->issMax = 0;
	  symhdr->issExtMax = 0;
	  symhdr->ifdMax = 0;
	  symhdr->crfd = 0;
	  symhdr->iextMax = 0;

	  /* We accumulate the debugging information itself in the
	     debug_info structure.  */
	  debug.line = NULL;
	  debug.external_dnr = NULL;
	  debug.external_pdr = NULL;
	  debug.external_sym = NULL;
	  debug.external_opt = NULL;
	  debug.external_aux = NULL;
	  debug.ss = NULL;
	  debug.ssext = debug.ssext_end = NULL;
	  debug.external_fdr = NULL;
	  debug.external_rfd = NULL;
	  debug.external_ext = debug.external_ext_end = NULL;

	  mdebug_handle = bfd_ecoff_debug_init (abfd, &debug, swap, info);
	  if (mdebug_handle == (PTR) NULL)
	    return false;

	  if (SGI_COMPAT (abfd))
	    {
	      asection *s;
	      EXTR esym;
	      bfd_vma last;
	      unsigned int i;
	      static const char * const name[] =
		{ ".text", ".init", ".fini", ".data",
		    ".rodata", ".sdata", ".sbss", ".bss" };
	      static const int sc[] = { scText, scInit, scFini, scData,
					  scRData, scSData, scSBss, scBss };

	      esym.jmptbl = 0;
	      esym.cobol_main = 0;
	      esym.weakext = 0;
	      esym.reserved = 0;
	      esym.ifd = ifdNil;
	      esym.asym.iss = issNil;
	      esym.asym.st = stLocal;
	      esym.asym.reserved = 0;
	      esym.asym.index = indexNil;
	      last = 0;
	      for (i = 0; i < 8; i++)
		{
		  esym.asym.sc = sc[i];
		  s = bfd_get_section_by_name (abfd, name[i]);
		  if (s != NULL)
		    {
		      esym.asym.value = s->vma;
		      last = s->vma + s->_raw_size;
		    }
		  else
		    esym.asym.value = last;

		  if (! bfd_ecoff_debug_one_external (abfd, &debug, swap,
						      name[i], &esym))
		    return false;
		}
	    }

	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      const struct ecoff_debug_swap *input_swap;
	      struct ecoff_debug_info input_debug;
	      char *eraw_src;
	      char *eraw_end;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      if (bfd_get_flavour (input_bfd) != bfd_target_elf_flavour
		  || (get_elf_backend_data (input_bfd)
		      ->elf_backend_ecoff_debug_swap) == NULL)
		{
		  /* I don't know what a non MIPS ELF bfd would be
		     doing with a .mdebug section, but I don't really
		     want to deal with it.  */
		  continue;
		}

	      input_swap = (get_elf_backend_data (input_bfd)
			    ->elf_backend_ecoff_debug_swap);

	      BFD_ASSERT (p->size == input_section->_raw_size);

	      /* The ECOFF linking code expects that we have already
		 read in the debugging information and set up an
		 ecoff_debug_info structure, so we do that now.  */
	      if (! _bfd_mips_elf_read_ecoff_info (input_bfd, input_section,
						   &input_debug))
		return false;

	      if (! (bfd_ecoff_debug_accumulate
		     (mdebug_handle, abfd, &debug, swap, input_bfd,
		      &input_debug, input_swap, info)))
		return false;

	      /* Loop through the external symbols.  For each one with
		 interesting information, try to find the symbol in
		 the linker global hash table and save the information
		 for the output external symbols.  */
	      eraw_src = input_debug.external_ext;
	      eraw_end = (eraw_src
			  + (input_debug.symbolic_header.iextMax
			     * input_swap->external_ext_size));
	      for (;
		   eraw_src < eraw_end;
		   eraw_src += input_swap->external_ext_size)
		{
		  EXTR ext;
		  const char *name;
		  struct mips_elf_link_hash_entry *h;

		  (*input_swap->swap_ext_in) (input_bfd, (PTR) eraw_src, &ext);
		  if (ext.asym.sc == scNil
		      || ext.asym.sc == scUndefined
		      || ext.asym.sc == scSUndefined)
		    continue;

		  name = input_debug.ssext + ext.asym.iss;
		  h = mips_elf_link_hash_lookup (mips_elf_hash_table (info),
						 name, false, false, true);
		  if (h == NULL || h->esym.ifd != -2)
		    continue;

		  if (ext.ifd != -1)
		    {
		      BFD_ASSERT (ext.ifd
				  < input_debug.symbolic_header.ifdMax);
		      ext.ifd = input_debug.ifdmap[ext.ifd];
		    }

		  h->esym = ext;
		}

	      /* Free up the information we just read.  */
	      free (input_debug.line);
	      free (input_debug.external_dnr);
	      free (input_debug.external_pdr);
	      free (input_debug.external_sym);
	      free (input_debug.external_opt);
	      free (input_debug.external_aux);
	      free (input_debug.ss);
	      free (input_debug.ssext);
	      free (input_debug.external_fdr);
	      free (input_debug.external_rfd);
	      free (input_debug.external_ext);

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  if (SGI_COMPAT (abfd) && info->shared)
	    {
	      /* Create .rtproc section.  */
	      rtproc_sec = bfd_get_section_by_name (abfd, ".rtproc");
	      if (rtproc_sec == NULL)
		{
		  flagword flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY
				    | SEC_LINKER_CREATED | SEC_READONLY);

		  rtproc_sec = bfd_make_section (abfd, ".rtproc");
		  if (rtproc_sec == NULL
		      || ! bfd_set_section_flags (abfd, rtproc_sec, flags)
		      || ! bfd_set_section_alignment (abfd, rtproc_sec, 4))
		    return false;
		}

	      if (! mips_elf_create_procedure_table (mdebug_handle, abfd,
						     info, rtproc_sec, &debug))
		return false;
	    }

	  /* Build the external symbol information.  */
	  einfo.abfd = abfd;
	  einfo.info = info;
	  einfo.debug = &debug;
	  einfo.swap = swap;
	  einfo.failed = false;
	  mips_elf_link_hash_traverse (mips_elf_hash_table (info),
				       mips_elf_output_extsym,
				       (PTR) &einfo);
	  if (einfo.failed)
	    return false;

	  /* Set the size of the .mdebug section.  */
	  o->_raw_size = bfd_ecoff_debug_size (abfd, &debug, swap);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  mdebug_sec = o;
	}

      if (strncmp (o->name, ".gptab.", sizeof ".gptab." - 1) == 0)
	{
	  const char *subname;
	  unsigned int c;
	  Elf32_gptab *tab;
	  Elf32_External_gptab *ext_tab;
	  unsigned int i;

	  /* The .gptab.sdata and .gptab.sbss sections hold
	     information describing how the small data area would
	     change depending upon the -G switch.  These sections
	     not used in executables files.  */
	  if (! info->relocateable)
	    {
	      asection **secpp;

	      for (p = o->link_order_head;
		   p != (struct bfd_link_order *) NULL;
		   p = p->next)
		{
		  asection *input_section;

		  if (p->type != bfd_indirect_link_order)
		    {
		      if (p->type == bfd_fill_link_order)
			continue;
		      abort ();
		    }

		  input_section = p->u.indirect.section;

		  /* Hack: reset the SEC_HAS_CONTENTS flag so that
		     elf_link_input_bfd ignores this section.  */
		  input_section->flags &=~ SEC_HAS_CONTENTS;
		}

	      /* Skip this section later on (I don't think this
		 currently matters, but someday it might).  */
	      o->link_order_head = (struct bfd_link_order *) NULL;

	      /* Really remove the section.  */
	      for (secpp = &abfd->sections;
		   *secpp != o;
		   secpp = &(*secpp)->next)
		;
	      *secpp = (*secpp)->next;
	      --abfd->section_count;

	      continue;
	    }

	  /* There is one gptab for initialized data, and one for
	     uninitialized data.  */
	  if (strcmp (o->name, ".gptab.sdata") == 0)
	    gptab_data_sec = o;
	  else if (strcmp (o->name, ".gptab.sbss") == 0)
	    gptab_bss_sec = o;
	  else
	    {
	      (*_bfd_error_handler)
		("%s: illegal section name `%s'",
		 bfd_get_filename (abfd), o->name);
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  /* The linker script always combines .gptab.data and
	     .gptab.sdata into .gptab.sdata, and likewise for
	     .gptab.bss and .gptab.sbss.  It is possible that there is
	     no .sdata or .sbss section in the output file, in which
	     case we must change the name of the output section.  */
	  subname = o->name + sizeof ".gptab" - 1;
	  if (bfd_get_section_by_name (abfd, subname) == NULL)
	    {
	      if (o == gptab_data_sec)
		o->name = ".gptab.data";
	      else
		o->name = ".gptab.bss";
	      subname = o->name + sizeof ".gptab" - 1;
	      BFD_ASSERT (bfd_get_section_by_name (abfd, subname) != NULL);
	    }

	  /* Set up the first entry.  */
	  c = 1;
	  tab = (Elf32_gptab *) bfd_malloc (c * sizeof (Elf32_gptab));
	  if (tab == NULL)
	    return false;
	  tab[0].gt_header.gt_current_g_value = elf_gp_size (abfd);
	  tab[0].gt_header.gt_unused = 0;

	  /* Combine the input sections.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      bfd_size_type size;
	      unsigned long last;
	      bfd_size_type gpentry;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* Combine the gptab entries for this input section one
		 by one.  We know that the input gptab entries are
		 sorted by ascending -G value.  */
	      size = bfd_section_size (input_bfd, input_section);
	      last = 0;
	      for (gpentry = sizeof (Elf32_External_gptab);
		   gpentry < size;
		   gpentry += sizeof (Elf32_External_gptab))
		{
		  Elf32_External_gptab ext_gptab;
		  Elf32_gptab int_gptab;
		  unsigned long val;
		  unsigned long add;
		  boolean exact;
		  unsigned int look;

		  if (! (bfd_get_section_contents
			 (input_bfd, input_section, (PTR) &ext_gptab,
			  gpentry, sizeof (Elf32_External_gptab))))
		    {
		      free (tab);
		      return false;
		    }

		  bfd_mips_elf32_swap_gptab_in (input_bfd, &ext_gptab,
						&int_gptab);
		  val = int_gptab.gt_entry.gt_g_value;
		  add = int_gptab.gt_entry.gt_bytes - last;

		  exact = false;
		  for (look = 1; look < c; look++)
		    {
		      if (tab[look].gt_entry.gt_g_value >= val)
			tab[look].gt_entry.gt_bytes += add;

		      if (tab[look].gt_entry.gt_g_value == val)
			exact = true;
		    }

		  if (! exact)
		    {
		      Elf32_gptab *new_tab;
		      unsigned int max;

		      /* We need a new table entry.  */
		      new_tab = ((Elf32_gptab *)
				 bfd_realloc ((PTR) tab,
					      (c + 1) * sizeof (Elf32_gptab)));
		      if (new_tab == NULL)
			{
			  free (tab);
			  return false;
			}
		      tab = new_tab;
		      tab[c].gt_entry.gt_g_value = val;
		      tab[c].gt_entry.gt_bytes = add;

		      /* Merge in the size for the next smallest -G
			 value, since that will be implied by this new
			 value.  */
		      max = 0;
		      for (look = 1; look < c; look++)
			{
			  if (tab[look].gt_entry.gt_g_value < val
			      && (max == 0
				  || (tab[look].gt_entry.gt_g_value
				      > tab[max].gt_entry.gt_g_value)))
			    max = look;
			}
		      if (max != 0)
			tab[c].gt_entry.gt_bytes +=
			  tab[max].gt_entry.gt_bytes;

		      ++c;
		    }

		  last = int_gptab.gt_entry.gt_bytes;
		}

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &=~ SEC_HAS_CONTENTS;
	    }

	  /* The table must be sorted by -G value.  */
	  if (c > 2)
	    qsort (tab + 1, c - 1, sizeof (tab[0]), gptab_compare);

	  /* Swap out the table.  */
	  ext_tab = ((Elf32_External_gptab *)
		     bfd_alloc (abfd, c * sizeof (Elf32_External_gptab)));
	  if (ext_tab == NULL)
	    {
	      free (tab);
	      return false;
	    }

	  for (i = 0; i < c; i++)
	    bfd_mips_elf32_swap_gptab_out (abfd, tab + i, ext_tab + i);
	  free (tab);

	  o->_raw_size = c * sizeof (Elf32_External_gptab);
	  o->contents = (bfd_byte *) ext_tab;

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;
	}
    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (! bfd_elf32_bfd_final_link (abfd, info))
    return false;

  /* Now write out the computed sections.  */

  if (reginfo_sec != (asection *) NULL)
    {
      Elf32_External_RegInfo ext;

      bfd_mips_elf32_swap_reginfo_out (abfd, &reginfo, &ext);
      if (! bfd_set_section_contents (abfd, reginfo_sec, (PTR) &ext,
				      (file_ptr) 0, sizeof ext))
	return false;
    }

  if (mdebug_sec != (asection *) NULL)
    {
      BFD_ASSERT (abfd->output_has_begun);
      if (! bfd_ecoff_write_accumulated_debug (mdebug_handle, abfd, &debug,
					       swap, info,
					       mdebug_sec->filepos))
	return false;

      bfd_ecoff_debug_free (mdebug_handle, abfd, &debug, swap, info);
    }

  if (gptab_data_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_data_sec,
				      gptab_data_sec->contents,
				      (file_ptr) 0,
				      gptab_data_sec->_raw_size))
	return false;
    }

  if (gptab_bss_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_bss_sec,
				      gptab_bss_sec->contents,
				      (file_ptr) 0,
				      gptab_bss_sec->_raw_size))
	return false;
    }

  if (SGI_COMPAT (abfd))
    {
      rtproc_sec = bfd_get_section_by_name (abfd, ".rtproc");
      if (rtproc_sec != NULL)
	{
	  if (! bfd_set_section_contents (abfd, rtproc_sec,
					  rtproc_sec->contents,
					  (file_ptr) 0,
					  rtproc_sec->_raw_size))
	    return false;
	}
    }

  return true;
}

/* Handle a MIPS ELF HI16 reloc.  */

static void
mips_elf_relocate_hi16 (input_bfd, relhi, rello, contents, addend)
     bfd *input_bfd;
     Elf_Internal_Rela *relhi;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma addend;
{
  bfd_vma insn;
  bfd_vma addlo;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  addlo = bfd_get_32 (input_bfd, contents + rello->r_offset);
  addlo &= 0xffff;

  addend += ((insn & 0xffff) << 16) + addlo;

  if ((addlo & 0x8000) != 0)
    addend -= 0x10000;
  if ((addend & 0x8000) != 0)
    addend += 0x10000;

  bfd_put_32 (input_bfd,
	      (insn & 0xffff0000) | ((addend >> 16) & 0xffff),
	      contents + relhi->r_offset);
}

/* Handle a MIPS ELF local GOT16 reloc.  */

static boolean
mips_elf_relocate_got_local (output_bfd, input_bfd, sgot, relhi, rello,
			     contents, addend)
     bfd *output_bfd;
     bfd *input_bfd;
     asection *sgot;
     Elf_Internal_Rela *relhi;
     Elf_Internal_Rela *rello;
     bfd_byte *contents;
     bfd_vma addend;
{
  unsigned int assigned_gotno;
  unsigned int i;
  bfd_vma insn;
  bfd_vma addlo;
  bfd_vma address;
  bfd_vma hipage;
  bfd_byte *got_contents;
  struct mips_got_info *g;

  insn = bfd_get_32 (input_bfd, contents + relhi->r_offset);

  addlo = bfd_get_32 (input_bfd, contents + rello->r_offset);
  addlo &= 0xffff;

  addend += ((insn & 0xffff) << 16) + addlo;

  if ((addlo & 0x8000) != 0)
    addend -= 0x10000;
  if ((addend & 0x8000) != 0)
    addend += 0x10000;

  /* Get a got entry representing requested hipage.  */
  BFD_ASSERT (elf_section_data (sgot) != NULL);
  g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
  BFD_ASSERT (g != NULL);

  assigned_gotno = g->assigned_gotno;
  got_contents = sgot->contents;
  hipage = addend & 0xffff0000;

  for (i = MIPS_RESERVED_GOTNO; i < assigned_gotno; i++)
    {
      address = bfd_get_32 (input_bfd, got_contents + i * 4);
      if (hipage == (address & 0xffff0000))
	break;
    }

  if (i == assigned_gotno)
    {
      if (assigned_gotno >= g->local_gotno)
	{
	  (*_bfd_error_handler)
	    ("more got entries are needed for hipage relocations");
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      bfd_put_32 (input_bfd, hipage, got_contents + assigned_gotno * 4);
      ++g->assigned_gotno;
    }

  i = - ELF_MIPS_GP_OFFSET (output_bfd) + i * 4;
  bfd_put_32 (input_bfd, (insn & 0xffff0000) | (i & 0xffff),
	      contents + relhi->r_offset);

  return true;
}

/* Handle MIPS ELF CALL16 reloc and global GOT16 reloc.  */

static void
mips_elf_relocate_global_got (input_bfd, rel, contents, offset)
     bfd *input_bfd;
     Elf_Internal_Rela *rel;
     bfd_byte *contents;
     bfd_vma offset;
{
  bfd_vma insn;

  insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
  bfd_put_32 (input_bfd,
	      (insn & 0xffff0000) | (offset & 0xffff),
	      contents + rel->r_offset);
}

/* Relocate a MIPS ELF section.  */

static boolean
mips_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr;
  size_t locsymcount;
  size_t extsymoff;
  asection *sgot, *sreloc, *scpt;
  bfd *dynobj;
  bfd_vma gp;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  struct mips_got_info *g;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  sgot = NULL;
  sreloc = NULL;
  if (dynobj == NULL || ! SGI_COMPAT (output_bfd))
    scpt = NULL;
  else
    scpt = bfd_get_section_by_name (dynobj, ".compact_rel");
  g = NULL;

  if (elf_bad_symtab (input_bfd))
    {
      locsymcount = symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
      extsymoff = 0;
    }
  else
    {
      locsymcount = symtab_hdr->sh_info;
      extsymoff = symtab_hdr->sh_info;
    }

  gp = _bfd_get_gp_value (output_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      bfd_vma addend;
      struct elf_link_hash_entry *h;
      asection *sec;
      Elf_Internal_Sym *sym;
      struct mips_elf_link_hash_entry *mh;
      int other;
      bfd_reloc_status_type r;

      r_type = ELF32_R_TYPE (rel->r_info);
      if ((r_type < 0 || r_type >= (int) R_MIPS_max)
	  && r_type != R_MIPS16_26
	  && r_type != R_MIPS16_GPREL)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      if (r_type == R_MIPS16_26)
	howto = &elf_mips16_jump_howto;
      else if (r_type == R_MIPS16_GPREL)
	howto = &elf_mips16_gprel_howto;
      else
	howto = elf_mips_howto_table + r_type;

      if (dynobj != NULL
	  && (r_type == R_MIPS_CALL16
	      || r_type == R_MIPS_GOT16
	      || r_type == R_MIPS_CALL_HI16
	      || r_type == R_MIPS_CALL_LO16
	      || r_type == R_MIPS_GOT_HI16
	      || r_type == R_MIPS_GOT_LO16))
	{
	  /* We need the .got section.  */
	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	      BFD_ASSERT (elf_section_data (sgot) != NULL);
	      g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
	      BFD_ASSERT (g != NULL);
	    }
	}

      r_symndx = ELF32_R_SYM (rel->r_info);

      /* Mix in the change in GP address for a GP relative reloc.  */
      if (r_type != R_MIPS_GPREL16
	  && r_type != R_MIPS_LITERAL
	  && r_type != R_MIPS_GPREL32
	  && r_type != R_MIPS16_GPREL)
	addend = 0;
      else
	{
	  if (gp == 0)
	    {
	      if (! ((*info->callbacks->reloc_dangerous)
		     (info,
		      "GP relative relocation when GP not defined",
		      input_bfd, input_section,
		      rel->r_offset)))
		return false;
	      /* Only give the error once per link.  */
	      gp = 4;
	      _bfd_set_gp_value (output_bfd, gp);
	    }

	  if (r_symndx < extsymoff
	      || (elf_bad_symtab (input_bfd)
		  && local_sections[r_symndx] != NULL))
	    {
	      /* This is a relocation against a section.  The current
		 addend in the instruction is the difference between
		 INPUT_SECTION->vma and the GP value of INPUT_BFD.  We
		 must change this to be the difference between the
		 final definition (which will end up in RELOCATION)
		 and the GP value of OUTPUT_BFD (which is in GP).  */
	      addend = elf_gp (input_bfd) - gp;
	    }
	  else if (! info->relocateable)
	    {
	      /* We are doing a final link.  The current addend in the
		 instruction is simply the desired offset into the
		 symbol (normally zero).  We want the instruction to
		 hold the difference between the final definition of
		 the symbol (which will end up in RELOCATION) and the
		 GP value of OUTPUT_BFD (which is in GP).  */
	      addend = - gp;
	    }
	  else
	    {
	      /* We are generating relocateable output, and we aren't
		 going to define this symbol, so we just leave the
		 instruction alone.  */
	      addend = 0;
	    }
	}

      h = NULL;
      sym = NULL;
      sec = NULL;
      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx >= locsymcount
	      || (elf_bad_symtab (input_bfd)
		  && local_sections[r_symndx] == NULL))
	    r = bfd_reloc_ok;
	  else
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
		r = bfd_reloc_ok;
	      else
		{
		  sec = local_sections[r_symndx];

		  /* It would be logical to add sym->st_value here,
		     but Irix 5 sometimes generates a garbage symbol
		     value.  */
		  addend += sec->output_offset;

		  /* If this is HI16 or GOT16 with an associated LO16,
		     adjust the addend accordingly.  Otherwise, just
		     relocate.  */
		  if (r_type == R_MIPS_64 && bfd_big_endian (input_bfd))
		    r = _bfd_relocate_contents (howto, input_bfd,
						addend,
						contents + rel->r_offset + 4);
		  else if (r_type != R_MIPS_HI16 && r_type != R_MIPS_GOT16)
		    r = _bfd_relocate_contents (howto, input_bfd,
						addend,
						contents + rel->r_offset);
		  else
		    {
		      Elf_Internal_Rela *lorel;

		      /* As a GNU extension, permit an arbitrary
			 number of R_MIPS_HI16 relocs before the
			 R_MIPS_LO16 reloc.  This permits gcc to emit
			 the HI and LO relocs itself.  */
		      if (r_type == R_MIPS_GOT16)
			lorel = rel + 1;
		      else
			{
			  for (lorel = rel + 1;
			       (lorel < relend
				&& (ELF32_R_TYPE (lorel->r_info)
				    == R_MIPS_HI16));
			       lorel++)
			    ;
			}
		      if (lorel < relend
			  && ELF32_R_TYPE (lorel->r_info) == R_MIPS_LO16)
			{
			  mips_elf_relocate_hi16 (input_bfd, rel, lorel,
						  contents, addend);
			  r = bfd_reloc_ok;
			}
		      else
			r = _bfd_relocate_contents (howto, input_bfd,
						    addend,
						    contents + rel->r_offset);
		    }
		}
	    }
	}
      else
	{
	  bfd_vma relocation;
	  boolean local;

	  /* This is a final link.  */
	  sym = NULL;
	  if (r_symndx < extsymoff
	      || (elf_bad_symtab (input_bfd)
		  && local_sections[r_symndx] != NULL))
	    {
	      local = true;
	      sym = local_syms + r_symndx;
	      sec = local_sections[r_symndx];
	      relocation = (sec->output_section->vma
			    + sec->output_offset);

	      /* It would be logical to always add sym->st_value here,
		 but Irix 5 sometimes generates a garbage symbol
		 value.  */
	      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
		relocation += sym->st_value;

	      /* mips16 text labels should be treated as odd.  */
	      if (sym->st_other == STO_MIPS16)
		++relocation;
	    }
	  else
	    {
	      long indx;

	      local = false;
	      indx = r_symndx - extsymoff;
	      h = elf_sym_hashes (input_bfd)[indx];
	      while (h->root.type == bfd_link_hash_indirect
		     || h->root.type == bfd_link_hash_warning)
		h = (struct elf_link_hash_entry *) h->root.u.i.link;
	      if (strcmp (h->root.root.string, "_gp_disp") == 0)
		{
		  if (gp == 0)
		    {
		      if (! ((*info->callbacks->reloc_dangerous)
			     (info,
			      "_gp_disp used when GP not defined",
			      input_bfd, input_section,
			      rel->r_offset)))
			return false;
		      /* Only give the error once per link.  */
		      gp = 4;
		      _bfd_set_gp_value (output_bfd, gp);
		      relocation = 0;
		    }
		  else
		    {
		      sec = input_section;
		      if (sec->output_section != NULL)
			relocation = (gp
				      - (rel->r_offset
					 + sec->output_section->vma
					 + sec->output_offset));
		      else
			relocation = gp - rel->r_offset;
		      if (r_type == R_MIPS_LO16)
			relocation += 4;
		    }
		}
	      else if (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
		{
		  sec = h->root.u.def.section;
		  if (sec->output_section == NULL)
		    relocation = 0;
		  else
		    relocation = (h->root.u.def.value
				  + sec->output_section->vma
				  + sec->output_offset);
		}
	      else if (h->root.type == bfd_link_hash_undefweak)
		relocation = 0;
	      else if (info->shared && ! info->symbolic)
		relocation = 0;
	      else if (strcmp (h->root.root.string, "_DYNAMIC_LINK") == 0)
		{
		  /* If this is a dynamic link, we should have created
                     a _DYNAMIC_LINK symbol in
                     mips_elf_create_dynamic_sections.  Otherwise, we
                     should define the symbol with a value of 0.
                     FIXME: It should probably get into the symbol
                     table somehow as well.  */
		  BFD_ASSERT (! info->shared);
		  BFD_ASSERT (bfd_get_section_by_name (output_bfd,
						       ".dynamic") == NULL);
		  relocation = 0;
		}
	      else
		{
		  if (! ((*info->callbacks->undefined_symbol)
			 (info, h->root.root.string, input_bfd,
			  input_section, rel->r_offset)))
		    return false;
		  relocation = 0;
		}
	    }

	  mh = (struct mips_elf_link_hash_entry *) h;
	  if (h != NULL)
	    other = h->other;
	  else if (sym != NULL)
	    other = sym->st_other;
	  else
	    other = 0;

	  /* If this function has an fn_stub, then it is a mips16
	     function which needs a stub if it is called by a 32 bit
	     function.  If this reloc is anything other than a 16 bit
	     call, redirect the reloc to the stub.  We don't redirect
	     relocs from other stub functions.  */
	  if (r_type != R_MIPS16_26
	      && ((mh != NULL
		   && mh->fn_stub != NULL)
		  || (mh == NULL
		      && elf_tdata (input_bfd)->local_stubs != NULL
		      && elf_tdata (input_bfd)->local_stubs[r_symndx] != NULL))
	      && strncmp (bfd_get_section_name (input_bfd, input_section),
			  FN_STUB, sizeof FN_STUB - 1) != 0
	      && strncmp (bfd_get_section_name (input_bfd, input_section),
			  CALL_STUB, sizeof CALL_STUB - 1) != 0
	      && strncmp (bfd_get_section_name (input_bfd, input_section),
			  CALL_FP_STUB, sizeof CALL_FP_STUB - 1) != 0)
	    {
	      if (mh != NULL)
		{
		  BFD_ASSERT (mh->need_fn_stub);
		  relocation = (mh->fn_stub->output_section->vma
				+ mh->fn_stub->output_offset);
		}
	      else
		{
		  asection *fn_stub;

		  fn_stub = elf_tdata (input_bfd)->local_stubs[r_symndx];
		  relocation = (fn_stub->output_section->vma
				+ fn_stub->output_offset);
		}

	      /* RELOCATION now points to 32 bit code.  */
	      other = 0;
	    }

	  /* If this function has a call_stub, then it is called by a
             mips16 function; the call needs to go through a stub if
             this function is a 32 bit function.  If this reloc is a
             16 bit call, and the symbol is not a 16 bit function,
             then redirect the reloc to the stub.  Note that we don't
             need to worry about calling the function through a
             function pointer; such calls are handled by routing
             through a special mips16 routine.  We don't have to check
             whether this call is from a stub; it can't be, because a
             stub contains 32 bit code, and hence can not have a 16
             bit reloc.  */
	  if (r_type == R_MIPS16_26
	      && mh != NULL
	      && (mh->call_stub != NULL || mh->call_fp_stub != NULL)
	      && other != STO_MIPS16)
	    {
	      asection *stub;

	      /* If both call_stub and call_fp_stub are defined, we
                 can figure out which one to use by seeing which one
                 appears in the input file.  */
	      if (mh->call_stub != NULL && mh->call_fp_stub != NULL)
		{
		  asection *o;

		  stub = NULL;
		  for (o = input_bfd->sections; o != NULL; o = o->next)
		    {
		      if (strncmp (bfd_get_section_name (input_bfd, o),
				   CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
			{
			  stub = mh->call_fp_stub;
			  break;
			}
		    }
		  if (stub == NULL)
		    stub = mh->call_stub;
		}
	      else if (mh->call_stub != NULL)
		stub = mh->call_stub;
	      else
		stub = mh->call_fp_stub;

	      BFD_ASSERT (stub->_raw_size > 0);
	      relocation = stub->output_section->vma + stub->output_offset;
	    }

	  if (r_type == R_MIPS_HI16)
	    {
	      Elf_Internal_Rela *lorel;

	      /* As a GNU extension, permit an arbitrary number of
		 R_MIPS_HI16 relocs before the R_MIPS_LO16 reloc.
		 This permits gcc to emit the HI and LO relocs itself.  */
	      for (lorel = rel + 1;
		   (lorel < relend
		    && ELF32_R_TYPE (lorel->r_info) == R_MIPS_HI16);
		   lorel++)
		;
	      if (lorel < relend
		  && ELF32_R_TYPE (lorel->r_info) == R_MIPS_LO16)
		{
		  mips_elf_relocate_hi16 (input_bfd, rel, lorel,
					  contents, relocation + addend);
		  r = bfd_reloc_ok;
		}
	      else
		r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					      contents, rel->r_offset,
					      relocation, addend);
	    }
	  else if (r_type == R_MIPS_GOT16 && local)
	    {
	      /* GOT16 must also have an associated LO16 in the local
		 case.  In this case, the addend is extracted and the
		 section in which the referenced object is determined.
		 Then the final address of the object is computed and
		 the GOT entry for the hipage (an aligned 64kb chunk)
		 is added to .got section if needed.  The offset field
		 of the GOT16-relocated instruction is replaced by the
		 index of this GOT entry for the hipage.  */
	      if ((rel + 1) < relend
		  && ELF32_R_TYPE ((rel + 1)->r_info) == R_MIPS_LO16)
		{
		  if (! mips_elf_relocate_got_local (output_bfd, input_bfd,
						     sgot, rel, rel + 1,
						     contents,
						     relocation + addend))
		    return false;
		  r = bfd_reloc_ok;
		}
	      else
		r = bfd_reloc_outofrange;
	    }
	  else if (r_type == R_MIPS_CALL16
		   || r_type == R_MIPS_GOT16
		   || r_type == R_MIPS_CALL_LO16
		   || r_type == R_MIPS_GOT_LO16)
	    {
	      bfd_vma offset;

	      /* This symbol must be registered as a global symbol
		 having the corresponding got entry.  */
	      BFD_ASSERT (h->got_offset != (bfd_vma) -1);

	      offset = (h->dynindx - g->global_gotsym + g->local_gotno) * 4;
	      BFD_ASSERT (g->local_gotno <= offset
			  && offset < sgot->_raw_size);
	      bfd_put_32 (output_bfd, relocation + addend,
			  sgot->contents + offset);
	      offset = (sgot->output_section->vma + sgot->output_offset
			+ offset - gp);
	      mips_elf_relocate_global_got (input_bfd, rel, contents,
					    offset);
	      r = bfd_reloc_ok;
	    }
	  else if (r_type == R_MIPS_CALL_HI16
		   || r_type == R_MIPS_GOT_HI16)
	    {
	      bfd_vma offset;

	      /* This must be a global symbol with a got entry.  The
                 next reloc must be the corresponding LO16 reloc.  */
	      BFD_ASSERT (h != NULL && h->got_offset != (bfd_vma) -1);
	      BFD_ASSERT ((rel + 1) < relend);
	      BFD_ASSERT ((int) ELF32_R_TYPE ((rel + 1)->r_info)
			  == (r_type == R_MIPS_CALL_HI16
			      ? (int) R_MIPS_CALL_LO16
			      : (int) R_MIPS_GOT_LO16));

	      offset = (h->dynindx - g->global_gotsym + g->local_gotno) * 4;
	      BFD_ASSERT (g->local_gotno <= offset
			  && offset < sgot->_raw_size);
	      bfd_put_32 (output_bfd, relocation + addend,
			  sgot->contents + offset);
	      offset = (sgot->output_section->vma + sgot->output_offset
			+ offset - gp);
	      mips_elf_relocate_hi16 (input_bfd, rel, rel + 1, contents,
				      offset);
	      r = bfd_reloc_ok;
	    }
	  else if (r_type == R_MIPS_REL32
		   || r_type == R_MIPS_32)
	    {
	      Elf_Internal_Rel outrel;
	      Elf32_crinfo cptrel;
	      bfd_byte *cr;

	      if ((info->shared
		   || (elf_hash_table (info)->dynamic_sections_created
		       && h != NULL
		       && ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)
			   == 0)))
		  && (input_section->flags & SEC_ALLOC) != 0)
		{
		  boolean skip;

		  /* When generating a shared object, these
		     relocations are copied into the output file to be
		     resolved at run time.  */
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_get_section_by_name (dynobj, ".rel.dyn");
		      BFD_ASSERT (sreloc != NULL);
		    }

		  skip = false;

		  if (elf_section_data (input_section)->stab_info == NULL)
		    outrel.r_offset = rel->r_offset;
		  else
		    {
		      bfd_vma off;

		      off = (_bfd_stab_section_offset
			     (output_bfd, &elf_hash_table (info)->stab_info,
			      input_section,
			      &elf_section_data (input_section)->stab_info,
			      rel->r_offset));
		      if (off == (bfd_vma) -1)
			skip = true;
		      outrel.r_offset = off;
		    }

		  outrel.r_offset += (input_section->output_section->vma
				      + input_section->output_offset);

		  addend = bfd_get_32 (input_bfd, contents + rel->r_offset);

		  if (skip)
		    memset (&outrel, 0, sizeof outrel);
		  else if (h != NULL
			   && (! info->symbolic
			       || (h->elf_link_hash_flags
				   & ELF_LINK_HASH_DEF_REGULAR) == 0))
		    {
		      BFD_ASSERT (h->dynindx != -1);
		      outrel.r_info = ELF32_R_INFO (h->dynindx, R_MIPS_REL32);
		      sec = input_section;
		    }
		  else
		    {
		      long indx;

		      if (h == NULL)
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
			  if (indx == 0)
			    abort ();
			}

		      outrel.r_info = ELF32_R_INFO (indx, R_MIPS_REL32);
		      addend += relocation;
		    }

		  if (! skip)
		    bfd_put_32 (output_bfd, addend, contents + rel->r_offset);

		  bfd_elf32_swap_reloc_out (output_bfd, &outrel,
					     (((Elf32_External_Rel *)
					       sreloc->contents)
					      + sreloc->reloc_count));
		  ++sreloc->reloc_count;

		  if (! skip && SGI_COMPAT (output_bfd))
		    {
		      if (scpt == NULL)
			continue;

		      /* Make an entry of compact relocation info.  */
		      mips_elf_set_cr_format (cptrel, CRF_MIPS_LONG);
		      cptrel.vaddr = (rel->r_offset
				      + input_section->output_section->vma
				      + input_section->output_offset);
		      if (r_type == R_MIPS_REL32)
			mips_elf_set_cr_type (cptrel, CRT_MIPS_REL32);
		      else
			mips_elf_set_cr_type (cptrel, CRT_MIPS_WORD);
		      mips_elf_set_cr_dist2to (cptrel, 0);
		      cptrel.konst = addend;

		      cr = (scpt->contents
			    + sizeof (Elf32_External_compact_rel));
		      bfd_elf32_swap_crinfo_out (output_bfd, &cptrel,
						 ((Elf32_External_crinfo *) cr
						  + scpt->reloc_count));
		      ++scpt->reloc_count;
		    }

		  /* This reloc will be computed at runtime, so
		     there's no need to do anything now.  */
		  continue;
		}
	      else
		r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					      contents, rel->r_offset,
					      relocation, addend);
	    }
	  else if (r_type == R_MIPS_64)
	    {
	      bfd_size_type addr;
	      unsigned long val;

	      /* Do a 32 bit relocation, and sign extend to 64 bits.  */
	      addr = rel->r_offset;
	      if (bfd_big_endian (input_bfd))
		addr += 4;
	      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents, addr, relocation,
					    addend);
	      val = bfd_get_32 (input_bfd, contents + addr);
	      if ((val & 0x80000000) != 0)
		val = 0xffffffff;
	      else
		val = 0;
	      addr = rel->r_offset;
	      if (bfd_little_endian (input_bfd))
		addr += 4;
	      bfd_put_32 (input_bfd, val, contents + addr);
	    }
	  else if (r_type == R_MIPS_26 && other == STO_MIPS16)
	    {
	      unsigned long insn;

	      /* This is a jump to a mips16 routine from a mips32
                 routine.  We need to change jal into jalx.  */
	      insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
	      if (((insn >> 26) & 0x3f) != 0x3
		  && ((insn >> 26) & 0x3f) != 0x1d)
		{
		  (*_bfd_error_handler)
		    ("%s: %s+0x%lx: jump to mips16 routine which is not jal",
		     bfd_get_filename (input_bfd),
		     input_section->name,
		     (unsigned long) rel->r_offset);
		  bfd_set_error (bfd_error_bad_value);
		  return false;
		}
	      insn = (insn & 0x3ffffff) | (0x1d << 26);
	      bfd_put_32 (input_bfd, insn, contents + rel->r_offset);
	      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents, rel->r_offset,
					    relocation, addend);
	    }
	  else if (r_type == R_MIPS16_26)
	    {
	      /* It's easiest to do the normal relocation, and then
                 dig out the instruction and swap the first word the
                 way the mips16 expects it.  If this is little endian,
                 though, we need to swap the two words first, and then
                 swap them back again later, so that the address looks
                 right.  */

	      if (bfd_little_endian (input_bfd))
		{
		  unsigned long insn;

		  insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
		  insn = ((insn >> 16) & 0xffff) | ((insn & 0xffff) << 16);
		  bfd_put_32 (input_bfd, insn, contents + rel->r_offset);
		}

	      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents, rel->r_offset,
					    relocation, addend);
	      if (r == bfd_reloc_ok)
		{
		  unsigned long insn;

		  if (bfd_little_endian (input_bfd))
		    {
		      insn = bfd_get_32 (input_bfd, contents + rel->r_offset);
		      insn = ((insn >> 16) & 0xffff) | ((insn & 0xffff) << 16);
		      bfd_put_32 (input_bfd, insn, contents + rel->r_offset);
		    }

		  insn = bfd_get_16 (input_bfd, contents + rel->r_offset);
		  insn = ((insn & 0xfc00)
			  | ((insn & 0x1f) << 5)
			  | ((insn & 0x3e0) >> 5));
		  /* If this is a jump to a 32 bit routine, then make
		     it jalx.  */
		  if (other != STO_MIPS16)
		    insn |= 0x400;
		  bfd_put_16 (input_bfd, insn, contents + rel->r_offset);
		}
	    }
	  else if (r_type == R_MIPS16_GPREL)
	    {
	      unsigned short extend, insn;
	      bfd_byte buf[4];
	      unsigned long final;

	      /* Extract the addend into buf, run the regular reloc,
                 and stuff the resulting value back into the
                 instructions.  */
	      if (rel->r_offset > input_section->_raw_size)
		r = bfd_reloc_outofrange;
	      else
		{
		  extend = bfd_get_16 (input_bfd, contents + rel->r_offset);
		  insn = bfd_get_16 (input_bfd, contents + rel->r_offset + 2);
		  bfd_put_32 (input_bfd,
			      (((extend & 0x1f) << 11)
			       | (extend & 0x7e0)
			       | (insn & 0x1f)),
			      buf);
		  r = _bfd_final_link_relocate (howto, input_bfd,
						input_section, buf,
						(bfd_vma) 0, relocation,
						addend);
		  final = bfd_get_32 (input_bfd, buf);
		  bfd_put_16 (input_bfd,
			      ((extend & 0xf800)
			       | ((final >> 11) & 0x1f)
			       | (final & 0x7e0)),
			      contents + rel->r_offset);
		  bfd_put_16 (input_bfd,
			      ((insn & 0xffe0)
			       | (final & 0x1f)),
			      contents + rel->r_offset + 2);
		}
	    }
	  else
	    r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					  contents, rel->r_offset,
					  relocation, addend);

	  /* The jal instruction can only jump to an address which is
             divisible by 4, and it can only jump to an address with
             the same upper 4 bits as the PC.  */
	  if (r == bfd_reloc_ok
	      && (r_type == R_MIPS16_26 || r_type == R_MIPS_26))
	    {
	      bfd_vma addr;

	      addr = relocation;
	      if (other == STO_MIPS16)
		addr &= ~ (bfd_vma) 1;
	      addr += addend;
	      if ((addr & 3) != 0
		  || ((addr & 0xf0000000)
		      != ((input_section->output_section->vma
			   + input_section->output_offset
			   + rel->r_offset)
			  & 0xf0000000)))
		r = bfd_reloc_overflow;
	    }

	  if (SGI_COMPAT (abfd)
	      && scpt != NULL
	      && (input_section->flags & SEC_ALLOC) != 0)
	    {
	      Elf32_crinfo cptrel;
	      bfd_byte *cr;

	      /* Make an entry of compact relocation info.  */
	      mips_elf_set_cr_format (cptrel, CRF_MIPS_LONG);
	      cptrel.vaddr = (rel->r_offset
			      + input_section->output_section->vma
			      + input_section->output_offset);

	      switch (r_type)
		{
		case R_MIPS_26:
		  mips_elf_set_cr_type (cptrel, CRT_MIPS_JMPAD);
		  /* XXX How should we set dist2to in this case. */
		  mips_elf_set_cr_dist2to (cptrel, 8);
		  cptrel.konst = addend + relocation;
		  cr = scpt->contents + sizeof (Elf32_External_compact_rel);
		  bfd_elf32_swap_crinfo_out (output_bfd, &cptrel,
					     ((Elf32_External_crinfo *) cr
					      + scpt->reloc_count));
		  ++scpt->reloc_count;
		  break;

		case R_MIPS_GPREL16:
		case R_MIPS_LITERAL:
		case R_MIPS_GPREL32:
		  mips_elf_set_cr_type (cptrel, CRT_MIPS_GPHI_LO);
		  cptrel.konst = gp - cptrel.vaddr;
		  mips_elf_set_cr_dist2to (cptrel, 4);
		  cr = scpt->contents + sizeof (Elf32_External_compact_rel);
		  bfd_elf32_swap_crinfo_out (output_bfd, &cptrel,
					     ((Elf32_External_crinfo *) cr
					      + scpt->reloc_count));
		  ++scpt->reloc_count;
		  break;

		default:
		  break;
		}
	    }
	}

      if (r != bfd_reloc_ok)
	{
	  switch (r)
	    {
	    default:
	    case bfd_reloc_outofrange:
	      abort ();
	    case bfd_reloc_overflow:
	      {
		const char *name;

		if (h != NULL)
		  name = h->root.root.string;
		else
		  {
		    name = bfd_elf_string_from_elf_section (input_bfd,
							    symtab_hdr->sh_link,
							    sym->st_name);
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
    }

  return true;
}

/* This hook function is called before the linker writes out a global
   symbol.  We mark symbols as small common if appropriate.  This is
   also where we undo the increment of the value for a mips16 symbol.  */

/*ARGSIGNORED*/
static boolean
mips_elf_link_output_symbol_hook (abfd, info, name, sym, input_sec)
     bfd *abfd;
     struct bfd_link_info *info;
     const char *name;
     Elf_Internal_Sym *sym;
     asection *input_sec;
{
  /* If we see a common symbol, which implies a relocatable link, then
     if a symbol was small common in an input file, mark it as small
     common in the output file.  */
  if (sym->st_shndx == SHN_COMMON
      && strcmp (input_sec->name, ".scommon") == 0)
    sym->st_shndx = SHN_MIPS_SCOMMON;

  if (sym->st_other == STO_MIPS16
      && (sym->st_value & 1) != 0)
    --sym->st_value;

  return true;
}

/* Functions for the dynamic linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/libc.so.1"

/* Create dynamic sections when linking against a dynamic object.  */

static boolean
mips_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct elf_link_hash_entry *h;
  flagword flags;
  register asection *s;
  const char * const *namep;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED | SEC_READONLY);

  /* Mips ABI requests the .dynamic section to be read only.  */
  s = bfd_get_section_by_name (abfd, ".dynamic");
  if (s != NULL)
    {
      if (! bfd_set_section_flags (abfd, s, flags))
	return false;
    }

  /* We need to create .got section.  */
  if (! mips_elf_create_got_section (abfd, info))
    return false;

  /* Create .stub section.  */
  if (bfd_get_section_by_name (abfd, ".stub") == NULL)
    {
      s = bfd_make_section (abfd, ".stub");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return false;
    }

  if (SGI_COMPAT (abfd)
      && !info->shared
      && bfd_get_section_by_name (abfd, ".rld_map") == NULL)
    {
      s = bfd_make_section (abfd, ".rld_map");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags & ~SEC_READONLY)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return false;
    }

  if (SGI_COMPAT (abfd))
    {
      for (namep = mips_elf_dynsym_rtproc_names; *namep != NULL; namep++)
	{
	  h = NULL;
	  if (! (_bfd_generic_link_add_one_symbol
		 (info, abfd, *namep, BSF_GLOBAL, bfd_und_section_ptr,
		  (bfd_vma) 0, (const char *) NULL, false,
		  get_elf_backend_data (abfd)->collect,
		  (struct bfd_link_hash_entry **) &h)))
	    return false;
	  h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  h->type = STT_SECTION;

	  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	    return false;
	}

      /* We need to create a .compact_rel section.  */
      if (! mips_elf_create_compact_rel_section (abfd, info))
	return false;

      /* Change aligments of some sections.  */
      s = bfd_get_section_by_name (abfd, ".hash");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".dynsym");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".dynstr");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".reginfo");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".dynamic");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
    }

  if (!info->shared)
    {
      h = NULL;
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, "_DYNAMIC_LINK", BSF_GLOBAL, bfd_abs_section_ptr,
	      (bfd_vma) 0, (const char *) NULL, false,
	      get_elf_backend_data (abfd)->collect,
	      (struct bfd_link_hash_entry **) &h)))
	return false;
      h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_SECTION;

      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	return false;

      if (! mips_elf_hash_table (info)->use_rld_obj_head)
	{
	  /* __rld_map is a four byte word located in the .data section
	     and is filled in by the rtld to contain a pointer to
	     the _r_debug structure. Its symbol value will be set in
	     mips_elf_finish_dynamic_symbol.  */
	  s = bfd_get_section_by_name (abfd, ".rld_map");
	  BFD_ASSERT (s != NULL);

	  h = NULL;
	  if (! (_bfd_generic_link_add_one_symbol
		 (info, abfd, "__rld_map", BSF_GLOBAL, s,
		  (bfd_vma) 0, (const char *) NULL, false,
		  get_elf_backend_data (abfd)->collect,
		  (struct bfd_link_hash_entry **) &h)))
	    return false;
	  h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  h->type = STT_OBJECT;

	  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	    return false;
	}
    }

  return true;
}

/* Create the .compact_rel section.  */

static boolean
mips_elf_create_compact_rel_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;

  if (bfd_get_section_by_name (abfd, ".compact_rel") == NULL)
    {
      flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY | SEC_LINKER_CREATED
	       | SEC_READONLY);

      s = bfd_make_section (abfd, ".compact_rel");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return false;

      s->_raw_size = sizeof (Elf32_External_compact_rel);
    }

  return true;
}

/* Create the .got section to hold the global offset table. */

static boolean
mips_elf_create_got_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct mips_got_info *g;

  /* This function may be called more than once.  */
  if (bfd_get_section_by_name (abfd, ".got") != NULL)
    return true;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags)
      || ! bfd_set_section_alignment (abfd, s, 4))
    return false;

  /* Define the symbol _GLOBAL_OFFSET_TABLE_.  We don't do this in the
     linker script because we don't want to define the symbol if we
     are not creating a global offset table.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s,
	  (bfd_vma) 0, (const char *) NULL, false,
	  get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! bfd_elf32_link_record_dynamic_symbol (info, h))
    return false;

  /* The first several global offset table entries are reserved.  */
  s->_raw_size = MIPS_RESERVED_GOTNO * 4;

  g = (struct mips_got_info *) bfd_alloc (abfd,
					  sizeof (struct mips_got_info));
  if (g == NULL)
    return false;
  g->global_gotsym = 0;
  g->local_gotno = MIPS_RESERVED_GOTNO;
  g->assigned_gotno = MIPS_RESERVED_GOTNO;
  if (elf_section_data (s) == NULL)
    {
      s->used_by_bfd =
	(PTR) bfd_zalloc (abfd, sizeof (struct bfd_elf_section_data));
      if (elf_section_data (s) == NULL)
	return false;
    }
  elf_section_data (s)->tdata = (PTR) g;

  return true;
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table.  */

static boolean
mips_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  const char *name;
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  struct mips_got_info *g;
  size_t extsymoff;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *sreloc;

  if (info->relocateable)
    return true;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  extsymoff = (elf_bad_symtab (abfd)) ? 0 : symtab_hdr->sh_info;

  /* Check for the mips16 stub sections.  */

  name = bfd_get_section_name (abfd, sec);
  if (strncmp (name, FN_STUB, sizeof FN_STUB - 1) == 0)
    {
      unsigned long r_symndx;

      /* Look at the relocation information to figure out which symbol
         this is for.  */

      r_symndx = ELF32_R_SYM (relocs->r_info);

      if (r_symndx < extsymoff
	  || sym_hashes[r_symndx - extsymoff] == NULL)
	{
	  asection *o;

	  /* This stub is for a local symbol.  This stub will only be
             needed if there is some relocation in this BFD, other
             than a 16 bit function call, which refers to this symbol.  */
	  for (o = abfd->sections; o != NULL; o = o->next)
	    {
	      Elf_Internal_Rela *sec_relocs;
	      const Elf_Internal_Rela *r, *rend;

	      /* We can ignore stub sections when looking for relocs.  */
	      if ((o->flags & SEC_RELOC) == 0
		  || o->reloc_count == 0
		  || strncmp (bfd_get_section_name (abfd, o), FN_STUB,
			      sizeof FN_STUB - 1) == 0
		  || strncmp (bfd_get_section_name (abfd, o), CALL_STUB,
			      sizeof CALL_STUB - 1) == 0
		  || strncmp (bfd_get_section_name (abfd, o), CALL_FP_STUB,
			      sizeof CALL_FP_STUB - 1) == 0)
		continue;

	      sec_relocs = (_bfd_elf32_link_read_relocs
			    (abfd, o, (PTR) NULL,
			     (Elf_Internal_Rela *) NULL,
			     info->keep_memory));
	      if (sec_relocs == NULL)
		return false;

	      rend = sec_relocs + o->reloc_count;
	      for (r = sec_relocs; r < rend; r++)
		if (ELF32_R_SYM (r->r_info) == r_symndx
		    && ELF32_R_TYPE (r->r_info) != R_MIPS16_26)
		  break;

	      if (! info->keep_memory)
		free (sec_relocs);

	      if (r < rend)
		break;
	    }

	  if (o == NULL)
	    {
	      /* There is no non-call reloc for this stub, so we do
                 not need it.  Since this function is called before
                 the linker maps input sections to output sections, we
                 can easily discard it by setting the SEC_EXCLUDE
                 flag.  */
	      sec->flags |= SEC_EXCLUDE;
	      return true;
	    }

	  /* Record this stub in an array of local symbol stubs for
             this BFD. */
	  if (elf_tdata (abfd)->local_stubs == NULL)
	    {
	      unsigned long symcount;
	      asection **n;

	      if (elf_bad_symtab (abfd))
		symcount = symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
	      else
		symcount = symtab_hdr->sh_info;
	      n = (asection **) bfd_zalloc (abfd,
					    symcount * sizeof (asection *));
	      if (n == NULL)
		return false;
	      elf_tdata (abfd)->local_stubs = n;
	    }

	  elf_tdata (abfd)->local_stubs[r_symndx] = sec;

	  /* We don't need to set mips16_stubs_seen in this case.
             That flag is used to see whether we need to look through
             the global symbol table for stubs.  We don't need to set
             it here, because we just have a local stub.  */
	}
      else
	{
	  struct mips_elf_link_hash_entry *h;

	  h = ((struct mips_elf_link_hash_entry *)
	       sym_hashes[r_symndx - extsymoff]);

	  /* H is the symbol this stub is for.  */

	  h->fn_stub = sec;
	  mips_elf_hash_table (info)->mips16_stubs_seen = true;
	}
    }
  else if (strncmp (name, CALL_STUB, sizeof CALL_STUB - 1) == 0
	   || strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
    {
      unsigned long r_symndx;
      struct mips_elf_link_hash_entry *h;
      asection **loc;

      /* Look at the relocation information to figure out which symbol
         this is for.  */

      r_symndx = ELF32_R_SYM (relocs->r_info);

      if (r_symndx < extsymoff
	  || sym_hashes[r_symndx - extsymoff] == NULL)
	{
	  /* This stub was actually built for a static symbol defined
	     in the same file.  We assume that all static symbols in
	     mips16 code are themselves mips16, so we can simply
	     discard this stub.  Since this function is called before
	     the linker maps input sections to output sections, we can
	     easily discard it by setting the SEC_EXCLUDE flag.  */
	  sec->flags |= SEC_EXCLUDE;
	  return true;
	}

      h = ((struct mips_elf_link_hash_entry *)
	   sym_hashes[r_symndx - extsymoff]);

      /* H is the symbol this stub is for.  */

      if (strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
	loc = &h->call_fp_stub;
      else
	loc = &h->call_stub;

      /* If we already have an appropriate stub for this function, we
	 don't need another one, so we can discard this one.  Since
	 this function is called before the linker maps input sections
	 to output sections, we can easily discard it by setting the
	 SEC_EXCLUDE flag.  We can also discard this section if we
	 happen to already know that this is a mips16 function; it is
	 not necessary to check this here, as it is checked later, but
	 it is slightly faster to check now.  */
      if (*loc != NULL || h->root.other == STO_MIPS16)
	{
	  sec->flags |= SEC_EXCLUDE;
	  return true;
	}

      *loc = sec;
      mips_elf_hash_table (info)->mips16_stubs_seen = true;
    }

  if (dynobj == NULL)
    {
      sgot = NULL;
      g = NULL;
    }
  else
    {
      sgot = bfd_get_section_by_name (dynobj, ".got");
      if (sgot == NULL)
	g = NULL;
      else
	{
	  BFD_ASSERT (elf_section_data (sgot) != NULL);
	  g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
	  BFD_ASSERT (g != NULL);
	}
    }

  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (r_symndx < extsymoff)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - extsymoff];

	  /* This may be an indirect symbol created because of a version.  */
	  if (h != NULL)
	    {
	      while (h->root.type == bfd_link_hash_indirect)
		h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }
	}

      /* Some relocs require a global offset table.  */
      if (dynobj == NULL || sgot == NULL)
	{
	  switch (ELF32_R_TYPE (rel->r_info))
	    {
	    case R_MIPS_GOT16:
	    case R_MIPS_CALL16:
	    case R_MIPS_CALL_HI16:
	    case R_MIPS_CALL_LO16:
	    case R_MIPS_GOT_HI16:
	    case R_MIPS_GOT_LO16:
	      if (dynobj == NULL)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! mips_elf_create_got_section (dynobj, info))
		return false;
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	      BFD_ASSERT (elf_section_data (sgot) != NULL);
	      g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
	      BFD_ASSERT (g != NULL);
	      break;

	    case R_MIPS_32:
	    case R_MIPS_REL32:
	      if (dynobj == NULL
		  && (info->shared || h != NULL)
		  && (sec->flags & SEC_ALLOC) != 0)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      break;

	    default:
	      break;
	    }
	}

      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_MIPS_CALL16:
	case R_MIPS_CALL_HI16:
	case R_MIPS_CALL_LO16:
	  /* This symbol requires a global offset table entry.  */

	  if (h == NULL)
	    {
	      (*_bfd_error_handler)
		("%s: CALL16 reloc at 0x%lx not against global symbol",
		 bfd_get_filename (abfd), (unsigned long) rel->r_offset);
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }

	  /* Make sure this symbol is output as a dynamic symbol.  */
	  if (h->dynindx == -1)
	    {
	      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		return false;
	    }

	  if (h->got_offset != (bfd_vma) -1)
	    {
	      /* We have already allocated space in the .got.  */
	      break;
	    }

	  /* Note the index of the first global got symbol in .dynsym.  */
	  if (g->global_gotsym == 0
	      || g->global_gotsym > (unsigned long) h->dynindx)
	    g->global_gotsym = h->dynindx;

	  /* Make this symbol to have the corresponding got entry.  */
	  h->got_offset = 0;

	  /* We need a stub, not a plt entry for the undefined
	     function.  But we record it as if it needs plt.  See
	     elf_adjust_dynamic_symbol in elflink.h.  */
	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->type = STT_FUNC;

	  break;

	case R_MIPS_GOT16:
	case R_MIPS_GOT_HI16:
	case R_MIPS_GOT_LO16:
	  /* This symbol requires a global offset table entry.  */

	  if (h != NULL)
	    {
	      /* Make sure this symbol is output as a dynamic symbol.  */
	      if (h->dynindx == -1)
		{
		  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		    return false;
		}

	      if (h->got_offset != (bfd_vma) -1)
		{
		  /* We have already allocated space in the .got.  */
		  break;
		}
	      /* Note the index of the first global got symbol in
                 .dynsym.  */
	      if (g->global_gotsym == 0
		  || g->global_gotsym > (unsigned long) h->dynindx)
		g->global_gotsym = h->dynindx;

	      /* Make this symbol to be the global got symbol.  */
	      h->got_offset = 0;
	    }

	  break;

	case R_MIPS_32:
	case R_MIPS_REL32:
	  if ((info->shared || h != NULL)
	      && (sec->flags & SEC_ALLOC) != 0)
	    {
	      if (sreloc == NULL)
		{
		  const char *name = ".rel.dyn";

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_make_section (dynobj, name);
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc,
						      (SEC_ALLOC
						       | SEC_LOAD
						       | SEC_HAS_CONTENTS
						       | SEC_IN_MEMORY
						       | SEC_LINKER_CREATED
						       | SEC_READONLY))
			  || ! bfd_set_section_alignment (dynobj, sreloc,
							  4))
			return false;
		    }
		}
	      if (info->shared)
		{
		  /* When creating a shared object, we must copy these
		     reloc types into the output file as R_MIPS_REL32
		     relocs.  We make room for this reloc in the
		     .rel.dyn reloc section */
		  if (sreloc->_raw_size == 0)
		    {
		      /* Add a null element. */
		      sreloc->_raw_size += sizeof (Elf32_External_Rel);
		      ++sreloc->reloc_count;
		    }
		  sreloc->_raw_size += sizeof (Elf32_External_Rel);
		}
	      else
		{
		  struct mips_elf_link_hash_entry *hmips;

		  /* We only need to copy this reloc if the symbol is
                     defined in a dynamic object.  */
		  hmips = (struct mips_elf_link_hash_entry *) h;
		  ++hmips->mips_32_relocs;
		}
	    }

	  if (SGI_COMPAT (abfd))
	    mips_elf_hash_table (info)->compact_rel_size +=
	      sizeof (Elf32_External_crinfo);

	  break;

	case R_MIPS_26:
	case R_MIPS_GPREL16:
	case R_MIPS_LITERAL:
	case R_MIPS_GPREL32:
	  if (SGI_COMPAT (abfd))
	    mips_elf_hash_table (info)->compact_rel_size +=
	      sizeof (Elf32_External_crinfo);
	  break;

	default:
	  break;
	}

      /* If this reloc is not a 16 bit call, and it has a global
         symbol, then we will need the fn_stub if there is one.
         References from a stub section do not count. */
      if (h != NULL
	  && ELF32_R_TYPE (rel->r_info) != R_MIPS16_26
	  && strncmp (bfd_get_section_name (abfd, sec), FN_STUB,
		      sizeof FN_STUB - 1) != 0
	  && strncmp (bfd_get_section_name (abfd, sec), CALL_STUB,
		      sizeof CALL_STUB - 1) != 0
	  && strncmp (bfd_get_section_name (abfd, sec), CALL_FP_STUB,
		      sizeof CALL_FP_STUB - 1) != 0)
	{
	  struct mips_elf_link_hash_entry *mh;

	  mh = (struct mips_elf_link_hash_entry *) h;
	  mh->need_fn_stub = true;
	}
    }

  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
mips_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  struct mips_elf_link_hash_entry *hmips;
  asection *s;

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

  /* If this symbol is defined in a dynamic object, we need to copy
     any R_MIPS_32 or R_MIPS_REL32 relocs against it into the output
     file.  */
  hmips = (struct mips_elf_link_hash_entry *) h;
  if (! info->relocateable
      && hmips->mips_32_relocs != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    {
      s = bfd_get_section_by_name (dynobj, ".rel.dyn");
      BFD_ASSERT (s != NULL);

      if (s->_raw_size == 0)
	{
	  /* Make room for a null element. */
	  s->_raw_size += sizeof (Elf32_External_Rel);
	  ++s->reloc_count;
	}
      s->_raw_size += hmips->mips_32_relocs * sizeof (Elf32_External_Rel);
    }

  /* For a function, create a stub, if needed. */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! elf_hash_table (info)->dynamic_sections_created)
	return true;

      /* If this symbol is not defined in a regular file, then set
	 the symbol to the stub location.  This is required to make
	 function pointers compare as equal between the normal
	 executable and the shared library.  */
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* We need .stub section.  */
	  s = bfd_get_section_by_name (dynobj, ".stub");
	  BFD_ASSERT (s != NULL);

	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;

	  /* XXX Write this stub address somewhere.  */
	  h->plt_offset = s->_raw_size;

	  /* Make room for this stub code.  */
	  s->_raw_size += MIPS_FUNCTION_STUB_SIZE;

	  /* The last half word of the stub will be filled with the index
	     of this symbol in .dynsym section.  */
	  return true;
	}
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

  return true;
}

/* This function is called after all the input files have been read,
   and the input sections have been assigned to output sections.  We
   check for any mips16 stub sections that we can discard.  */

static boolean mips_elf_check_mips16_stubs
  PARAMS ((struct mips_elf_link_hash_entry *, PTR));

static boolean
mips_elf_always_size_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  if (info->relocateable
      || ! mips_elf_hash_table (info)->mips16_stubs_seen)
    return true;

  mips_elf_link_hash_traverse (mips_elf_hash_table (info),
			       mips_elf_check_mips16_stubs,
			       (PTR) NULL);

  return true;
}

/* Check the mips16 stubs for a particular symbol, and see if we can
   discard them.  */

/*ARGSUSED*/
static boolean
mips_elf_check_mips16_stubs (h, data)
     struct mips_elf_link_hash_entry *h;
     PTR data;
{
  if (h->fn_stub != NULL
      && ! h->need_fn_stub)
    {
      /* We don't need the fn_stub; the only references to this symbol
         are 16 bit calls.  Clobber the size to 0 to prevent it from
         being included in the link.  */
      h->fn_stub->_raw_size = 0;
      h->fn_stub->_cooked_size = 0;
      h->fn_stub->flags &= ~ SEC_RELOC;
      h->fn_stub->reloc_count = 0;
      h->fn_stub->flags |= SEC_EXCLUDE;
    }

  if (h->call_stub != NULL
      && h->root.other == STO_MIPS16)
    {
      /* We don't need the call_stub; this is a 16 bit function, so
         calls from other 16 bit functions are OK.  Clobber the size
         to 0 to prevent it from being included in the link.  */
      h->call_stub->_raw_size = 0;
      h->call_stub->_cooked_size = 0;
      h->call_stub->flags &= ~ SEC_RELOC;
      h->call_stub->reloc_count = 0;
      h->call_stub->flags |= SEC_EXCLUDE;
    }

  if (h->call_fp_stub != NULL
      && h->root.other == STO_MIPS16)
    {
      /* We don't need the call_stub; this is a 16 bit function, so
         calls from other 16 bit functions are OK.  Clobber the size
         to 0 to prevent it from being included in the link.  */
      h->call_fp_stub->_raw_size = 0;
      h->call_fp_stub->_cooked_size = 0;
      h->call_fp_stub->flags &= ~ SEC_RELOC;
      h->call_fp_stub->reloc_count = 0;
      h->call_fp_stub->flags |= SEC_EXCLUDE;
    }

  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
mips_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean reltext;
  asection *sgot;
  struct mips_got_info *g;

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

  /* Recompute the size of .got for local entires (reserved and
     hipages) if needed.  To estimate it, get the upper bound of total
     size of loadable sections.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");

  if (sgot != NULL)
    {
      bfd_size_type loadable_size = 0;
      bfd_size_type local_gotno;
      struct _bfd *sub;

      BFD_ASSERT (elf_section_data (sgot) != NULL);
      g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
      BFD_ASSERT (g != NULL);

      for (sub = info->input_bfds; sub; sub = sub->link_next)
	for (s = sub->sections; s != NULL; s = s->next)
	  {
	    if ((s->flags & SEC_ALLOC) == 0)
	      continue;
	    loadable_size += (s->_raw_size + 0xf) & ~0xf;
	  }

      loadable_size += MIPS_FUNCTION_STUB_SIZE;

      /* Assume there are two loadable segments consisting of
	 contiguous sections.  Is 5 enough? */
      local_gotno = (loadable_size >> 16) + 5 + MIPS_RESERVED_GOTNO;
      g->local_gotno = local_gotno;
      sgot->_raw_size += local_gotno * 4;
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  reltext = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      strip = false;

      if (strncmp (name, ".rel", 4) == 0)
	{
	  if (s->_raw_size == 0)
	    strip = true;
	  else
	    {
	      const char *outname;
	      asection *target;

	      /* If this relocation section applies to a read only
                 section, then we probably need a DT_TEXTREL entry.
                 If the relocation section is .rel.dyn, we always
                 assert a DT_TEXTREL entry rather than testing whether
                 there exists a relocation to a read only section or
                 not.  */
	      outname = bfd_get_section_name (output_bfd,
					      s->output_section);
	      target = bfd_get_section_by_name (output_bfd, outname + 4);
	      if ((target != NULL
		   && (target->flags & SEC_READONLY) != 0
		   && (target->flags & SEC_ALLOC) != 0)
		  || strcmp (outname, ".rel.dyn") == 0)
		reltext = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      if (strcmp (name, ".rel.dyn") != 0)
		s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) == 0)
	{
	  int i;

	  BFD_ASSERT (elf_section_data (s) != NULL);
	  g = (struct mips_got_info *) elf_section_data (s)->tdata;
	  BFD_ASSERT (g != NULL);

	  /* Fix the size of .got section for the correspondence of
	     global symbols and got entries. This adds some useless
	     got entries. Is this required by ABI really?  */
	  i = elf_hash_table (info)->dynsymcount - g->global_gotsym;
	  s->_raw_size += i * 4;
	}
      else if (strncmp (name, ".stub", 5) == 0)
	{
	  /* Irix rld assumes that the function stub isn't at the end
	     of .text section. So put a dummy. XXX  */
	  s->_raw_size += MIPS_FUNCTION_STUB_SIZE;
	}
      else if (! info->shared
	       && ! mips_elf_hash_table (info)->use_rld_obj_head
	       && strncmp (name, ".rld_map", 8) == 0)
	{
	  /* We add a room for __rld_map. It will be filled in by the
	     rtld to contain a pointer to the _r_debug structure.  */
	  s->_raw_size += 4;
	}
      else if (SGI_COMPAT (output_bfd)
	       && strncmp (name, ".compact_rel", 12) == 0)
	s->_raw_size += mips_elf_hash_table (info)->compact_rel_size;
      else if (strncmp (name, ".init", 5) != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  asection **spp;

	  for (spp = &s->output_section->owner->sections;
	       *spp != s->output_section;
	       spp = &(*spp)->next)
	    ;
	  *spp = s->output_section->next;
	  --s->output_section->owner->section_count;

	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_alloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
      memset (s->contents, 0, s->_raw_size);
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_mips_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  if (SGI_COMPAT (output_bfd))
	    {
	      /* SGI object has the equivalence of DT_DEBUG in the
		 DT_MIPS_RLD_MAP entry.  */
	      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_RLD_MAP, 0))
		return false;
	    }
	  else
	    if (! bfd_elf32_add_dynamic_entry (info, DT_DEBUG, 0))
	      return false;
	}

      if (reltext)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	}

      if (! bfd_elf32_add_dynamic_entry (info, DT_PLTGOT, 0))
	return false;

      if (bfd_get_section_by_name (dynobj, ".rel.dyn"))
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_REL, 0))
	    return false;

	  if (! bfd_elf32_add_dynamic_entry (info, DT_RELSZ, 0))
	    return false;

	  if (! bfd_elf32_add_dynamic_entry (info, DT_RELENT, 0))
	    return false;
	}

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_CONFLICTNO, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_LIBLISTNO, 0))
	return false;

      if (bfd_get_section_by_name (dynobj, ".conflict") != NULL)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_CONFLICT, 0))
	    return false;

	  s = bfd_get_section_by_name (dynobj, ".liblist");
	  BFD_ASSERT (s != NULL);

	  if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_LIBLIST, 0))
	    return false;
	}

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_RLD_VERSION, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_FLAGS, 0))
	return false;

#if 0
      /* Time stamps in executable files are a bad idea.  */
      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_TIME_STAMP, 0))
	return false;
#endif

#if 0 /* FIXME  */
      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_ICHECKSUM, 0))
	return false;
#endif

#if 0 /* FIXME  */
      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_IVERSION, 0))
	return false;
#endif

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_BASE_ADDRESS, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_LOCAL_GOTNO, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_SYMTABNO, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_UNREFEXTNO, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_GOTSYM, 0))
	return false;

      if (! bfd_elf32_add_dynamic_entry (info, DT_MIPS_HIPAGENO, 0))
	return false;

#if 0 /* (SGI_COMPAT) */
      if (! bfd_get_section_by_name (dynobj, ".init"))
	if (! bfd_elf32_add_dynamic_entry (info, DT_INIT, 0))
	  return false;

      if (! bfd_get_section_by_name (dynobj, ".fini"))
	if (! bfd_elf32_add_dynamic_entry (info, DT_FINI, 0))
	  return false;
#endif
    }

  /* If we use dynamic linking, we generate a section symbol for each
     output section.  These are local symbols, which means that they
     must come first in the dynamic symbol table.
     That means we must increment the dynamic symbol index of every
     other dynamic symbol.  */
  {
    const char * const *namep;
    unsigned int c, i;
    bfd_size_type strindex;
    struct bfd_strtab_hash *dynstr;
    struct mips_got_info *g;

    c = 0;
    if (elf_hash_table (info)->dynamic_sections_created)
      {
	if (SGI_COMPAT (output_bfd))
	  {
	    c = SIZEOF_MIPS_DYNSYM_SECNAMES - 1;
	    elf_link_hash_traverse (elf_hash_table (info),
				    mips_elf_adjust_dynindx,
				    (PTR) &c);
	    elf_hash_table (info)->dynsymcount += c;

	    dynstr = elf_hash_table (info)->dynstr;
	    BFD_ASSERT (dynstr != NULL);

	    for (i = 1, namep = mips_elf_dynsym_sec_names;
		 *namep != NULL;
		 i++, namep++)
	      {
		s = bfd_get_section_by_name (output_bfd, *namep);
		if (s != NULL)
		  elf_section_data (s)->dynindx = i;

		strindex = _bfd_stringtab_add (dynstr, *namep, true, false);
		if (strindex == (bfd_size_type) -1)
		  return false;

		mips_elf_hash_table (info)->dynsym_sec_strindex[i] = strindex;
	      }
	  }
	else
	  {
	    c = bfd_count_sections (output_bfd);
	    elf_link_hash_traverse (elf_hash_table (info),
				    mips_elf_adjust_dynindx,
				    (PTR) &c);
	    elf_hash_table (info)->dynsymcount += c;

	    for (i = 1, s = output_bfd->sections; s != NULL; s = s->next, i++)
	      {
		elf_section_data (s)->dynindx = i;
		/* These symbols will have no names, so we don't need to
		   fiddle with dynstr_index.  */
	      }
	  }
      }

    if (sgot != NULL)
      {
	BFD_ASSERT (elf_section_data (sgot) != NULL);
	g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
	BFD_ASSERT (g != NULL);

	/* If there are no global got symbols, fake the last symbol so
	   for safety.  */
	if (g->global_gotsym)
	  g->global_gotsym += c;
	else
	  g->global_gotsym = elf_hash_table (info)->dynsymcount - 1;
      }
  }

  return true;
}

/* Increment the index of a dynamic symbol by a given amount.  Called
   via elf_link_hash_traverse.  */

static boolean
mips_elf_adjust_dynindx (h, cparg)
     struct elf_link_hash_entry *h;
     PTR cparg;
{
  unsigned int *cp = (unsigned int *) cparg;

  if (h->dynindx != -1)
    h->dynindx += *cp;
  return true;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
mips_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;
  bfd_vma gval;
  asection *sgot;
  struct mips_got_info *g;
  const char *name;

  dynobj = elf_hash_table (info)->dynobj;
  gval = sym->st_value;

  if (h->plt_offset != (bfd_vma) -1)
    {
      asection *s;
      bfd_byte *p;
      bfd_byte stub[MIPS_FUNCTION_STUB_SIZE];

      /* This symbol has a stub.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (dynobj, ".stub");
      BFD_ASSERT (s != NULL);

      /* Fill the stub.  */
      p = stub;
      bfd_put_32 (output_bfd, STUB_LW(output_bfd), p);
      p += 4;
      bfd_put_32 (output_bfd, STUB_MOVE, p);
      p += 4;

      /* FIXME: Can h->dynindex be more than 64K?  */
      if (h->dynindx & 0xffff0000)
	return false;

      bfd_put_32 (output_bfd, STUB_JALR, p);
      p += 4;
      bfd_put_32 (output_bfd, STUB_LI16 + h->dynindx, p);

      BFD_ASSERT (h->plt_offset <= s->_raw_size);
      memcpy (s->contents + h->plt_offset, stub, MIPS_FUNCTION_STUB_SIZE);

      /* Mark the symbol as undefined.  plt_offset != -1 occurs
	 only for the referenced symbol.  */
      sym->st_shndx = SHN_UNDEF;

      /* The run-time linker uses the st_value field of the symbol
	 to reset the global offset table entry for this external
	 to its stub address when unlinking a shared object.  */
      gval = s->output_section->vma + s->output_offset + h->plt_offset;
      sym->st_value = gval;
    }

  BFD_ASSERT (h->dynindx != -1);

  sgot = bfd_get_section_by_name (dynobj, ".got");
  BFD_ASSERT (sgot != NULL);
  BFD_ASSERT (elf_section_data (sgot) != NULL);
  g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
  BFD_ASSERT (g != NULL);

  if ((unsigned long) h->dynindx >= g->global_gotsym)
    {
      bfd_size_type offset;

      /* This symbol has an entry in the global offset table.  Set its
	 value to the corresponding got entry, if needed.  */
      if (h->got_offset == (bfd_vma) -1)
	{
	  offset = (h->dynindx - g->global_gotsym + g->local_gotno) * 4;
	  BFD_ASSERT (g->local_gotno * 4 <= offset
		      && offset < sgot->_raw_size);
	  bfd_put_32 (output_bfd, gval, sgot->contents + offset);
	}
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  name = h->root.root.string;
  if (strcmp (name, "_DYNAMIC") == 0
      || strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;
  else if (strcmp (name, "_DYNAMIC_LINK") == 0)
    {
      sym->st_shndx = SHN_ABS;
      sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
      sym->st_value = 1;
    }
  else if (SGI_COMPAT (output_bfd))
    {
      if (strcmp (name, "_gp_disp") == 0)
	{
	  sym->st_shndx = SHN_ABS;
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
	  sym->st_value = elf_gp (output_bfd);
	}
      else if (strcmp (name, mips_elf_dynsym_rtproc_names[0]) == 0
	       || strcmp (name, mips_elf_dynsym_rtproc_names[1]) == 0)
	{
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
	  sym->st_other = STO_PROTECTED;
	  sym->st_value = 0;
	  sym->st_shndx = SHN_MIPS_DATA;
	}
      else if (strcmp (name, mips_elf_dynsym_rtproc_names[2]) == 0)
	{
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
	  sym->st_other = STO_PROTECTED;
	  sym->st_value = mips_elf_hash_table (info)->procedure_count;
	  sym->st_shndx = SHN_ABS;
	}
      else if (sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS)
	{
	  if (h->type == STT_FUNC)
	    sym->st_shndx = SHN_MIPS_TEXT;
	  else if (h->type == STT_OBJECT)
	    sym->st_shndx = SHN_MIPS_DATA;
	}
    }

  if (SGI_COMPAT (output_bfd)
      && ! info->shared)
    {
      if (! mips_elf_hash_table (info)->use_rld_obj_head
	  && strcmp (name, "__rld_map") == 0)
	{
	  asection *s = bfd_get_section_by_name (dynobj, ".rld_map");
	  BFD_ASSERT (s != NULL);
	  sym->st_value = s->output_section->vma + s->output_offset;
	  bfd_put_32 (output_bfd, (bfd_vma) 0, s->contents);
	  if (mips_elf_hash_table (info)->rld_value == 0)
	    mips_elf_hash_table (info)->rld_value = sym->st_value;
	}
      else if (mips_elf_hash_table (info)->use_rld_obj_head
	       && strcmp (name, "__rld_obj_head") == 0)
	{
	  asection *s = bfd_get_section_by_name (dynobj, ".rld_map");
	  BFD_ASSERT (s != NULL);
	  mips_elf_hash_table (info)->rld_value = sym->st_value;
	}
    }

  /* If this is a mips16 symbol, force the value to be even.  */
  if (sym->st_other == STO_MIPS16
      && (sym->st_value & 1) != 0)
    --sym->st_value;

  return true;
}

/* Finish up the dynamic sections.  */

static boolean
mips_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;
  struct mips_got_info *g;

  dynobj = elf_hash_table (info)->dynobj;

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  sgot = bfd_get_section_by_name (dynobj, ".got");
  if (sgot == NULL)
    g = NULL;
  else
    {
      BFD_ASSERT (elf_section_data (sgot) != NULL);
      g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
      BFD_ASSERT (g != NULL);
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      Elf32_External_Dyn *dyncon, *dynconend;

      BFD_ASSERT (sdyn != NULL);
      BFD_ASSERT (g != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  size_t elemsize;
	  asection *s;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_RELENT:
	      s = bfd_get_section_by_name (dynobj, ".rel.dyn");
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_val = sizeof (Elf32_External_Rel);
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_STRSZ:
	      /* Rewrite DT_STRSZ.  */
	      dyn.d_un.d_val =
		_bfd_stringtab_size (elf_hash_table (info)->dynstr);
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTGOT:
	      name = ".got";
	      goto get_vma;
	    case DT_MIPS_CONFLICT:
	      name = ".conflict";
	      goto get_vma;
	    case DT_MIPS_LIBLIST:
	      name = ".liblist";
	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_RLD_VERSION:
	      dyn.d_un.d_val = 1; /* XXX */
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_FLAGS:
	      dyn.d_un.d_val = RHF_NOTPOT; /* XXX */
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_CONFLICTNO:
	      name = ".conflict";
	      elemsize = sizeof (Elf32_Conflict);
	      goto set_elemno;

	    case DT_MIPS_LIBLISTNO:
	      name = ".liblist";
	      elemsize = sizeof (Elf32_Lib);
	    set_elemno:
	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s != NULL)
		{
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val = s->_cooked_size / elemsize;
		  else
		    dyn.d_un.d_val = s->_raw_size / elemsize;
		}
	      else
		    dyn.d_un.d_val = 0;

	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_TIME_STAMP:
	      time ((time_t *) &dyn.d_un.d_val);
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_ICHECKSUM:
	      /* XXX FIXME: */
	      break;

	    case DT_MIPS_IVERSION:
	      /* XXX FIXME: */
	      break;

	    case DT_MIPS_BASE_ADDRESS:
	      s = output_bfd->sections;
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma & ~(0xffff);
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_LOCAL_GOTNO:
	      dyn.d_un.d_val = g->local_gotno;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_SYMTABNO:
	      name = ".dynsym";
	      elemsize = sizeof (Elf32_External_Sym);
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);

	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size / elemsize;
	      else
		dyn.d_un.d_val = s->_raw_size / elemsize;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_UNREFEXTNO:
	      /* XXX FIXME: */
	      dyn.d_un.d_val = SIZEOF_MIPS_DYNSYM_SECNAMES;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_GOTSYM:
	      dyn.d_un.d_val = g->global_gotsym;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_HIPAGENO:
	      dyn.d_un.d_val = g->local_gotno - MIPS_RESERVED_GOTNO;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_MIPS_RLD_MAP:
	      dyn.d_un.d_ptr = mips_elf_hash_table (info)->rld_value;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    }
	}
    }

  /* The first entry of the global offset table will be filled at
     runtime. The second entry will be used by some runtime loaders.
     This isn't the case of Irix rld. */
  if (sgot != NULL && sgot->_raw_size > 0)
    {
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0x80000000, sgot->contents + 4);
    }

  if (sgot != NULL)
    elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;

  {
    asection *sdynsym;
    asection *s;
    unsigned int i;
    bfd_vma last;
    Elf_Internal_Sym sym;
    long dindx;
    const char *name;
    const char * const * namep = mips_elf_dynsym_sec_names;
    Elf32_compact_rel cpt;

    /* Set up the section symbols for the output sections. SGI sets
       the STT_NOTYPE attribute for these symbols.  Should we do so?  */

    sdynsym = bfd_get_section_by_name (dynobj, ".dynsym");
    if (sdynsym != NULL)
      {
	if (SGI_COMPAT (output_bfd))
	  {
	    sym.st_size = 0;
	    sym.st_name = 0;
	    sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_NOTYPE);
	    sym.st_other = 0;

	    i = 0;
	    last = 0;
	    dindx = 0;
	    while ((name = *namep++) != NULL)
	      {
		s = bfd_get_section_by_name (output_bfd, name);
		if (s != NULL)
		  {
		    sym.st_value = s->vma;
		    dindx = elf_section_data (s)->dynindx;
		    last = s->vma + s->_raw_size;
		  }
		else
		  {
		    sym.st_value = last;
		    dindx++;
		  }

		sym.st_shndx = (i < MIPS_TEXT_DYNSYM_SECNO
				? SHN_MIPS_TEXT
				: SHN_MIPS_DATA);
		++i;
		sym.st_name =
		  mips_elf_hash_table (info)->dynsym_sec_strindex[dindx];

		bfd_elf32_swap_symbol_out (output_bfd, &sym,
					   (((Elf32_External_Sym *)
					     sdynsym->contents)
					    + dindx));
	      }

	    /* Set the sh_info field of the output .dynsym section to
	       the index of the first global symbol.  */
	    elf_section_data (sdynsym->output_section)->this_hdr.sh_info =
	      SIZEOF_MIPS_DYNSYM_SECNAMES;
	  }
	else
	  {
	    sym.st_size = 0;
	    sym.st_name = 0;
	    sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
	    sym.st_other = 0;

	    for (s = output_bfd->sections; s != NULL; s = s->next)
	      {
		int indx;

		sym.st_value = s->vma;

		indx = elf_section_data (s)->this_idx;
		BFD_ASSERT (indx > 0);
		sym.st_shndx = indx;

		bfd_elf32_swap_symbol_out (output_bfd, &sym,
					   (((Elf32_External_Sym *)
					     sdynsym->contents)
					    + elf_section_data (s)->dynindx));
	      }

	    /* Set the sh_info field of the output .dynsym section to
	       the index of the first global symbol.  */
	    elf_section_data (sdynsym->output_section)->this_hdr.sh_info =
	      bfd_count_sections (output_bfd) + 1;
	  }
      }

    if (SGI_COMPAT (output_bfd))
      {
	/* Write .compact_rel section out.  */
	s = bfd_get_section_by_name (dynobj, ".compact_rel");
	if (s != NULL)
	  {
	    cpt.id1 = 1;
	    cpt.num = s->reloc_count;
	    cpt.id2 = 2;
	    cpt.offset = (s->output_section->filepos
			  + sizeof (Elf32_External_compact_rel));
	    cpt.reserved0 = 0;
	    cpt.reserved1 = 0;
	    bfd_elf32_swap_compact_rel_out (output_bfd, &cpt,
					    ((Elf32_External_compact_rel *)
					     s->contents));

	    /* Clean up a dummy stub function entry in .text.  */
	    s = bfd_get_section_by_name (dynobj, ".stub");
	    if (s != NULL)
	      {
		file_ptr dummy_offset;

		BFD_ASSERT (s->_raw_size >= MIPS_FUNCTION_STUB_SIZE);
		dummy_offset = s->_raw_size - MIPS_FUNCTION_STUB_SIZE;
		memset (s->contents + dummy_offset, 0,
			MIPS_FUNCTION_STUB_SIZE);
	      }
	  }
      }

    /* Clean up a first relocation in .rel.dyn.  */
    s = bfd_get_section_by_name (dynobj, ".rel.dyn");
    if (s != NULL && s->_raw_size > 0)
      memset (s->contents, 0, sizeof (Elf32_External_Rel));
  }

  return true;
}

/* This is almost identical to bfd_generic_get_... except that some
   MIPS relocations need to be handled specially.  Sigh.  */

static bfd_byte *
elf32_mips_get_relocated_section_contents (abfd, link_info, link_order, data,
					   relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;

  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;

  if (reloc_size < 0)
    goto error_return;

  reloc_vector = (arelent **) bfd_malloc (reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    goto error_return;

  /* read in the section */
  if (!bfd_get_section_contents (input_bfd,
				 input_section,
				 (PTR) data,
				 0,
				 input_section->_raw_size))
    goto error_return;

  /* We're not relaxing the section, so just copy the size info */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = true;

  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    goto error_return;

  if (reloc_count > 0)
    {
      arelent **parent;
      /* for mips */
      int gp_found;
      bfd_vma gp = 0x12345678;	/* initialize just to shut gcc up */

      {
	struct bfd_hash_entry *h;
	struct bfd_link_hash_entry *lh;
	/* Skip all this stuff if we aren't mixing formats.  */
	if (abfd && input_bfd
	    && abfd->xvec == input_bfd->xvec)
	  lh = 0;
	else
	  {
	    h = bfd_hash_lookup (&link_info->hash->table, "_gp", false, false);
	    lh = (struct bfd_link_hash_entry *) h;
	  }
      lookup:
	if (lh)
	  {
	    switch (lh->type)
	      {
	      case bfd_link_hash_undefined:
	      case bfd_link_hash_undefweak:
	      case bfd_link_hash_common:
		gp_found = 0;
		break;
	      case bfd_link_hash_defined:
	      case bfd_link_hash_defweak:
		gp_found = 1;
		gp = lh->u.def.value;
		break;
	      case bfd_link_hash_indirect:
	      case bfd_link_hash_warning:
		lh = lh->u.i.link;
		/* @@FIXME  ignoring warning for now */
		goto lookup;
	      case bfd_link_hash_new:
	      default:
		abort ();
	      }
	  }
	else
	  gp_found = 0;
      }
      /* end mips */
      for (parent = reloc_vector; *parent != (arelent *) NULL;
	   parent++)
	{
	  char *error_message = (char *) NULL;
	  bfd_reloc_status_type r;

	  /* Specific to MIPS: Deal with relocation types that require
	     knowing the gp of the output bfd.  */
	  asymbol *sym = *(*parent)->sym_ptr_ptr;
	  if (bfd_is_abs_section (sym->section) && abfd)
	    {
	      /* The special_function wouldn't get called anyways.  */
	    }
	  else if (!gp_found)
	    {
	      /* The gp isn't there; let the special function code
		 fall over on its own.  */
	    }
	  else if ((*parent)->howto->special_function
		   == _bfd_mips_elf_gprel16_reloc)
	    {
	      /* bypass special_function call */
	      r = gprel16_with_gp (input_bfd, sym, *parent, input_section,
				   relocateable, (PTR) data, gp);
	      goto skip_bfd_perform_relocation;
	    }
	  /* end mips specific stuff */

	  r = bfd_perform_relocation (input_bfd,
				      *parent,
				      (PTR) data,
				      input_section,
				      relocateable ? abfd : (bfd *) NULL,
				      &error_message);
	skip_bfd_perform_relocation:

	  if (relocateable)
	    {
	      asection *os = input_section->output_section;

	      /* A partial link, so keep the relocs */
	      os->orelocation[os->reloc_count] = *parent;
	      os->reloc_count++;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		case bfd_reloc_undefined:
		  if (!((*link_info->callbacks->undefined_symbol)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_dangerous:
		  BFD_ASSERT (error_message != (char *) NULL);
		  if (!((*link_info->callbacks->reloc_dangerous)
			(link_info, error_message, input_bfd, input_section,
			 (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_overflow:
		  if (!((*link_info->callbacks->reloc_overflow)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 (*parent)->howto->name, (*parent)->addend,
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_outofrange:
		default:
		  abort ();
		  break;
		}

	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  return data;

error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}
#define bfd_elf32_bfd_get_relocated_section_contents \
  elf32_mips_get_relocated_section_contents

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf32_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym,
  /* Alignment of debugging information.  E.g., 4.  */
  4,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  _bfd_mips_elf_read_ecoff_info
};

#define TARGET_LITTLE_SYM		bfd_elf32_littlemips_vec
#define TARGET_LITTLE_NAME		"elf32-littlemips"
#define TARGET_BIG_SYM			bfd_elf32_bigmips_vec
#define TARGET_BIG_NAME			"elf32-bigmips"
#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS

/* The SVR4 MIPS ABI says that this should be 0x10000, but Irix 5 uses
   a value of 0x1000, and we are compatible.  */
#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_collect		true
#define elf_backend_type_change_ok	true
#define elf_info_to_howto		0
#define elf_info_to_howto_rel		mips_info_to_howto_rel
#define elf_backend_sym_is_global	mips_elf_sym_is_global
#define elf_backend_object_p		mips_elf32_object_p
#define elf_backend_section_from_shdr	mips_elf32_section_from_shdr
#define elf_backend_fake_sections	_bfd_mips_elf_fake_sections
#define elf_backend_section_from_bfd_section \
					_bfd_mips_elf_section_from_bfd_section
#define elf_backend_section_processing	mips_elf32_section_processing
#define elf_backend_symbol_processing	_bfd_mips_elf_symbol_processing
#define elf_backend_additional_program_headers \
					mips_elf_additional_program_headers
#define elf_backend_modify_segment_map	mips_elf_modify_segment_map
#define elf_backend_final_write_processing \
					_bfd_mips_elf_final_write_processing
#define elf_backend_ecoff_debug_swap	&mips_elf32_ecoff_debug_swap

#define bfd_elf32_bfd_is_local_label_name \
					mips_elf_is_local_label_name
#define bfd_elf32_find_nearest_line	_bfd_mips_elf_find_nearest_line
#define bfd_elf32_set_section_contents	_bfd_mips_elf_set_section_contents
#define bfd_elf32_bfd_link_hash_table_create \
					mips_elf_link_hash_table_create
#define bfd_elf32_bfd_final_link	mips_elf_final_link
#define bfd_elf32_bfd_copy_private_bfd_data \
					_bfd_mips_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data \
					_bfd_mips_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags	_bfd_mips_elf_set_private_flags
#define elf_backend_add_symbol_hook	mips_elf_add_symbol_hook
#define elf_backend_create_dynamic_sections \
					mips_elf_create_dynamic_sections
#define elf_backend_check_relocs	mips_elf_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					mips_elf_adjust_dynamic_symbol
#define elf_backend_always_size_sections \
					mips_elf_always_size_sections
#define elf_backend_size_dynamic_sections \
					mips_elf_size_dynamic_sections
#define elf_backend_relocate_section	mips_elf_relocate_section
#define elf_backend_link_output_symbol_hook \
					mips_elf_link_output_symbol_hook
#define elf_backend_finish_dynamic_symbol \
					mips_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					mips_elf_finish_dynamic_sections

#include "elf32-target.h"
