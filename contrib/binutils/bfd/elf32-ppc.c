/* PowerPC-specific support for 32-bit ELF
   Copyright 1994, 95, 96, 97, 98, 1999 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This file is based on a preliminary PowerPC ELF ABI.  The
   information may not match the final PowerPC ELF ABI.  It includes
   suggestions from the in-progress Embedded PowerPC ABI, and that
   information may also not match.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/ppc.h"

#define USE_RELA		/* we want RELA relocations, not REL */


static reloc_howto_type *ppc_elf_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void ppc_elf_info_to_howto
  PARAMS ((bfd *abfd, arelent *cache_ptr, Elf32_Internal_Rela *dst));
static void ppc_elf_howto_init PARAMS ((void));
static bfd_reloc_status_type ppc_elf_addr16_ha_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean ppc_elf_set_private_flags PARAMS ((bfd *, flagword));
static boolean ppc_elf_copy_private_bfd_data PARAMS ((bfd *, bfd *));
static boolean ppc_elf_merge_private_bfd_data PARAMS ((bfd *, bfd *));

static int ppc_elf_additional_program_headers PARAMS ((bfd *));
static boolean ppc_elf_modify_segment_map PARAMS ((bfd *));

static boolean ppc_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));

static boolean ppc_elf_section_from_shdr PARAMS ((bfd *,
						  Elf32_Internal_Shdr *,
						  char *));
static boolean ppc_elf_fake_sections
  PARAMS ((bfd *, Elf32_Internal_Shdr *, asection *));

static elf_linker_section_t *ppc_elf_create_linker_section
  PARAMS ((bfd *abfd,
	   struct bfd_link_info *info,
	   enum elf_linker_section_enum));

static boolean ppc_elf_check_relocs PARAMS ((bfd *,
					     struct bfd_link_info *,
					     asection *,
					     const Elf_Internal_Rela *));

static asection * ppc_elf_gc_mark_hook PARAMS ((bfd *abfd,
						struct bfd_link_info *info,
						Elf_Internal_Rela *rel,
						struct elf_link_hash_entry *h,
						Elf_Internal_Sym *sym));

static boolean ppc_elf_gc_sweep_hook PARAMS ((bfd *abfd,
					      struct bfd_link_info *info,
					      asection *sec,
					      const Elf_Internal_Rela *relocs));

static boolean ppc_elf_adjust_dynamic_symbol PARAMS ((struct bfd_link_info *,
						      struct elf_link_hash_entry *));

static boolean ppc_elf_size_dynamic_sections PARAMS ((bfd *, struct bfd_link_info *));

static boolean ppc_elf_relocate_section PARAMS ((bfd *,
						 struct bfd_link_info *info,
						 bfd *,
						 asection *,
						 bfd_byte *,
						 Elf_Internal_Rela *relocs,
						 Elf_Internal_Sym *local_syms,
						 asection **));

static boolean ppc_elf_add_symbol_hook  PARAMS ((bfd *,
						 struct bfd_link_info *,
						 const Elf_Internal_Sym *,
						 const char **,
						 flagword *,
						 asection **,
						 bfd_vma *));

static boolean ppc_elf_finish_dynamic_symbol PARAMS ((bfd *,
						      struct bfd_link_info *,
						      struct elf_link_hash_entry *,
						      Elf_Internal_Sym *));

static boolean ppc_elf_finish_dynamic_sections PARAMS ((bfd *, struct bfd_link_info *));

#define BRANCH_PREDICT_BIT 0x200000		/* branch prediction bit for branch taken relocs */
#define RA_REGISTER_MASK 0x001f0000		/* mask to set RA in memory instructions */
#define RA_REGISTER_SHIFT 16			/* value to shift register by to insert RA */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so.1"

/* The size in bytes of an entry in the procedure linkage table.  */
#define PLT_ENTRY_SIZE 12
/* The initial size of the plt reserved for the dynamic linker.  */
#define PLT_INITIAL_ENTRY_SIZE 72
/* The size of the gap between entries in the PLT.  */
#define PLT_SLOT_SIZE 8
/* The number of single-slot PLT entries (the rest use two slots).  */
#define PLT_NUM_SINGLE_ENTRIES 8192


static reloc_howto_type *ppc_elf_howto_table[ (int)R_PPC_max ];

static reloc_howto_type ppc_elf_howto_raw[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_PPC_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_PPC_ADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 26 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC_ADDR24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR24",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 16 bit relocation.  */
  HOWTO (R_PPC_ADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 16 bit relocation without overflow.  */
  HOWTO (R_PPC_ADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of an address.  */
  HOWTO (R_PPC_ADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of an address, plus 1 if the contents of
     the low 16 bits, treated as a signed number, is negative.	*/
  HOWTO (R_PPC_ADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_ADDR16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC_ADDR14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR14",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is expected to be taken.	The lower two
     bits must be zero.	 */
  HOWTO (R_PPC_ADDR14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR14_BRTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is not expected to be taken.  The lower
     two bits must be zero.  */
  HOWTO (R_PPC_ADDR14_BRNTAKEN, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_ADDR14_BRNTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A relative 26 bit branch; the lower two bits must be zero.	 */
  HOWTO (R_PPC_REL24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL24",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch; the lower two bits must be zero.	 */
  HOWTO (R_PPC_REL14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL14",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch.	Bit 10 should be set to indicate that
     the branch is expected to be taken.  The lower two bits must be
     zero.  */
  HOWTO (R_PPC_REL14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL14_BRTAKEN",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch.	Bit 10 should be set to indicate that
     the branch is not expected to be taken.  The lower two bits must
     be zero.  */
  HOWTO (R_PPC_REL14_BRNTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL14_BRNTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC_ADDR16, but referring to the GOT table entry for the
     symbol.  */
  HOWTO (R_PPC_GOT16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_GOT16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_LO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_GOT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_HI, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_GOT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		 /* pcrel_offset */

  /* Like R_PPC_ADDR16_HA, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC_GOT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_GOT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_REL24, but referring to the procedure linkage table
     entry for the symbol.  */
  HOWTO (R_PPC_PLTREL24,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,  /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLTREL24",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object, because the object being
     run has to have the data at some particular address.  */
  HOWTO (R_PPC_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_COPY",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR32, but used when setting global offset table
     entries.  */
  HOWTO (R_PPC_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_GLOB_DAT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Marks a procedure linkage table entry for a symbol.  */
  HOWTO (R_PPC_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_JMP_SLOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     longword is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_PPC_RELATIVE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC_RELATIVE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_REL24, but uses the value of the symbol within the
     object rather than the final value.  Normally used for
     _GLOBAL_OFFSET_TABLE_.  */
  HOWTO (R_PPC_LOCAL24PC,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_LOCAL24PC",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC_ADDR32, but may be unaligned.  */
  HOWTO (R_PPC_UADDR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_UADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16, but may be unaligned.  */
  HOWTO (R_PPC_UADDR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_UADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit PC relative */
  HOWTO (R_PPC_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_REL32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 32-bit relocation to the symbol's procedure linkage table.
     FIXME: not supported. */
  HOWTO (R_PPC_PLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLT32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit PC relative relocation to the symbol's procedure linkage table.
     FIXME: not supported. */
  HOWTO (R_PPC_PLTREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLTREL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC_ADDR16_LO, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC_ADDR16_HI, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_PLT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		 /* pcrel_offset */

  /* Like R_PPC_ADDR16_HA, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC_PLT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_PLT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A sign-extended 16 bit value relative to _SDA_BASE_, for use with
     small data items.  */
  HOWTO (R_PPC_SDAREL16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SDAREL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit section relative relocation. */
  HOWTO (R_PPC_SECTOFF,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SECTOFF",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16-bit lower half section relative relocation. */
  HOWTO (R_PPC_SECTOFF_LO,	  /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SECTOFF_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16-bit upper half section relative relocation. */
  HOWTO (R_PPC_SECTOFF_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_SECTOFF_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		 /* pcrel_offset */

  /* 16-bit upper half adjusted section relative relocation. */
  HOWTO (R_PPC_SECTOFF_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_SECTOFF_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The remaining relocs are from the Embedded ELF ABI, and are not
     in the SVR4 ELF ABI.  */

  /* 32 bit value resulting from the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_NADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit value resulting from the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_NADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit value resulting from the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_ADDR16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of the addend minus the symbol */
  HOWTO (R_PPC_EMB_NADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_NADDR16_HI", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high order 16 bits of the result of the addend minus the address,
     plus 1 if the contents of the low 16 bits, treated as a signed number,
     is negative.  */
  HOWTO (R_PPC_EMB_NADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC_EMB_NADDR16_HA", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit value resulting from allocating a 4 byte word to hold an
     address in the .sdata section, and returning the offset from
     _SDA_BASE_ for that relocation */
  HOWTO (R_PPC_EMB_SDAI16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDAI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit value resulting from allocating a 4 byte word to hold an
     address in the .sdata2 section, and returning the offset from
     _SDA2_BASE_ for that relocation */
  HOWTO (R_PPC_EMB_SDA2I16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDA2I16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A sign-extended 16 bit value relative to _SDA2_BASE_, for use with
     small data items.	 */
  HOWTO (R_PPC_EMB_SDA2REL,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDA2REL",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Relocate against either _SDA_BASE_ or _SDA2_BASE_, filling in the 16 bit
     signed offset from the appropriate base, and filling in the register
     field with the appropriate register (0, 2, or 13).  */
  HOWTO (R_PPC_EMB_SDA21,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_SDA21",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Relocation not handled: R_PPC_EMB_MRKREF */
  /* Relocation not handled: R_PPC_EMB_RELSEC16 */
  /* Relocation not handled: R_PPC_EMB_RELST_LO */
  /* Relocation not handled: R_PPC_EMB_RELST_HI */
  /* Relocation not handled: R_PPC_EMB_RELST_HA */
  /* Relocation not handled: R_PPC_EMB_BIT_FLD */

  /* PC relative relocation against either _SDA_BASE_ or _SDA2_BASE_, filling
     in the 16 bit signed offset from the appropriate base, and filling in the
     register field with the appropriate register (0, 2, or 13).  */
  HOWTO (R_PPC_EMB_RELSDA,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_EMB_RELSDA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_PPC_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_PPC_GNU_VTINHERIT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_PPC_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_PPC_GNU_VTENTRY",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Phony reloc to handle AIX style TOC entries */
  HOWTO (R_PPC_TOC16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC_TOC16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */
};


/* Initialize the ppc_elf_howto_table, so that linear accesses can be done.  */

static void
ppc_elf_howto_init ()
{
  unsigned int i, type;

  for (i = 0; i < sizeof (ppc_elf_howto_raw) / sizeof (ppc_elf_howto_raw[0]); i++)
    {
      type = ppc_elf_howto_raw[i].type;
      BFD_ASSERT (type < sizeof(ppc_elf_howto_table) / sizeof(ppc_elf_howto_table[0]));
      ppc_elf_howto_table[type] = &ppc_elf_howto_raw[i];
    }
}


/* This function handles relaxing for the PPC with option --mpc860c0[=<n>].

   The MPC860, revision C0 or earlier contains a bug in the die.
   If all of the following conditions are true, the next instruction
   to be executed *may* be treated as a no-op.
   1/ A forward branch is executed.
   2/ The branch is predicted as not taken.
   3/ The branch is taken.
   4/ The branch is located in the last 5 words of a page.
      (The EOP limit is 5 by default but may be specified as any value from 1-10.)
   
   Our software solution is to detect these problematic branches in a
   linker pass and modify them as follows:
   1/ Unconditional branches - Since these are always predicted taken,
      there is no problem and no action is required.
   2/ Conditional backward branches - No problem, no action required.
   3/ Conditional forward branches - Ensure that the "inverse prediction
      bit" is set (ensure it is predicted taken).
   4/ Conditional register branches - Ensure that the "y bit" is set
      (ensure it is predicted taken).
*/

/* Sort sections by address.  */

static int
ppc_elf_sort_rela (arg1, arg2)
    const void *arg1;
    const void *arg2;
{
  const Elf_Internal_Rela **rela1 = (const Elf_Internal_Rela**) arg1;
  const Elf_Internal_Rela **rela2 = (const Elf_Internal_Rela**) arg2;

  /* Sort by offset. */
  return ((*rela1)->r_offset - (*rela2)->r_offset);
}

static boolean
ppc_elf_relax_section (abfd, isec, link_info, again)
    bfd *abfd;
    asection *isec;
    struct bfd_link_info *link_info;
    boolean *again;
{
#define PAGESIZE 0x1000

  bfd_byte *contents = NULL;
  bfd_byte *free_contents = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela **rela_comb = NULL;
  int comb_curr, comb_count;

  /* We never have to do this more than once per input section. */
  *again = false;

  /* If needed, initialize this section's cooked size.  */
  if (isec->_cooked_size == 0)
      isec->_cooked_size = isec->_raw_size;

  /* We're only interested in text sections which overlap the
  troublesome area at the end of a page. */
  if (link_info->mpc860c0 && (isec->flags & SEC_CODE) && isec->_cooked_size)
    {
      bfd_vma dot, end_page, end_section;
      boolean section_modified;

      /* Get the section contents. */
      /* Get cached copy if it exists.  */
      if (elf_section_data (isec)->this_hdr.contents != NULL)
	  contents = elf_section_data (isec)->this_hdr.contents;
      else
	{
	  /* Go get them off disk.  */
	  contents = (bfd_byte *) bfd_malloc (isec->_raw_size);
	  if (contents == NULL)
	      goto error_return;
	  free_contents = contents;

	  if (! bfd_get_section_contents (abfd, isec, contents,
					  (file_ptr) 0, isec->_raw_size))
	      goto error_return;
	}

      comb_curr = 0;
      comb_count = 0;
      if (isec->reloc_count)
        {
          unsigned n;

          /* Get a copy of the native relocations.  */
          internal_relocs = _bfd_elf32_link_read_relocs (
    	    abfd, isec, (PTR) NULL, (Elf_Internal_Rela *) NULL, 
    	    link_info->keep_memory);
          if (internal_relocs == NULL)
    	      goto error_return;
          if (! link_info->keep_memory)
    	      free_relocs = internal_relocs;
    
          /* Setup a faster access method for the reloc info we need. */
          rela_comb = (Elf_Internal_Rela**)
	    bfd_malloc (isec->reloc_count*sizeof(Elf_Internal_Rela*));
          if (rela_comb == NULL)
              goto error_return;
          for (n=0; n<isec->reloc_count; ++n)
            {
              long r_type;
    
              r_type = ELF32_R_TYPE (internal_relocs[n].r_info);
              if (r_type < 0 || r_type >= (int)R_PPC_max)
                  goto error_return;
    
              /* Prologue constants are sometimes present in the ".text"
              sections and they can be identified by their associated relocation.
              We don't want to process those words and some others which
              can also be identified by their relocations.  However, not all
              conditional branches will have a relocation so we will
              only ignore words that 1) have a reloc, and 2) the reloc
              is not applicable to a conditional branch.
              The array rela_comb is built here for use in the EOP scan loop. */
              switch (r_type)
                {
                case R_PPC_ADDR14_BRNTAKEN:     /* absolute, predicted not taken */
                case R_PPC_REL14:               /* relative cond. br. */
                case R_PPC_REL14_BRNTAKEN:      /* rel. cond. br., predicted not taken */
                  /* We should check the instruction. */
                  break;
                default:
                  /* The word is not a conditional branch - ignore it. */
                  rela_comb[comb_count++] = &internal_relocs[n];
                  break;
                }
            }
          if (comb_count > 1)
              qsort (rela_comb, (size_t) comb_count, sizeof (int), ppc_elf_sort_rela);
        }

      /* Enumerate each EOP region that overlaps this section. */
      end_section = isec->vma + isec->_cooked_size;
      dot = end_page = (isec->vma | (PAGESIZE - 1)) + 1;
      dot -= link_info->mpc860c0;
      section_modified = false;
      if (dot < isec->vma)      /* Increment the start position if this section */
          dot = isec->vma;      /* begins in the middle of its first EOP region. */
      for (;
           dot < end_section;
           dot += PAGESIZE, end_page += PAGESIZE)
        {

          /* Check each word in this EOP region. */
          for (; dot < end_page; dot += 4)
            {
              bfd_vma isec_offset;
              unsigned long insn;
              boolean skip, modified;

              /* Don't process this word if there is a relocation for it and
              the relocation indicates the word is not a conditional branch. */
              skip = false;
              isec_offset = dot - isec->vma;
              for (; comb_curr<comb_count; ++comb_curr)
                {
                  bfd_vma r_offset;

                  r_offset = rela_comb[comb_curr]->r_offset;
                  if (r_offset >= isec_offset)
                    {
                      if (r_offset == isec_offset) skip = true;
                      break;
                    }
                }
              if (skip) continue;

              /* Check the current word for a problematic conditional branch. */
#define BO0(insn) ((insn) & 0x02000000)
#define BO2(insn) ((insn) & 0x00800000)
#define BO4(insn) ((insn) & 0x00200000)
              insn = (unsigned long) bfd_get_32 (abfd, contents + isec_offset);
              modified = false;
              if ((insn & 0xFc000000) == 0x40000000)
                {
                  /* Instruction is BCx */
                  if ((!BO0(insn) || !BO2(insn)) && !BO4(insn))
                    {
                      bfd_vma target;
                      /* This branch is predicted as "normal".
                      If this is a forward branch, it is problematic. */

                      target = insn & 0x0000Fffc;               /*extract*/
                      target = (target ^ 0x8000) - 0x8000;      /*sign extend*/
                      if ((insn & 0x00000002) == 0)
                          target += dot;                        /*convert to abs*/
                      if (target > dot)
                        {
                          insn |= 0x00200000;   /* set the prediction bit */
                          modified = true;
                        }
                    }
                }
              else if ((insn & 0xFc00Fffe) == 0x4c000420)
                {
                  /* Instruction is BCCTRx */
                  if ((!BO0(insn) || !BO2(insn)) && !BO4(insn))
		    {
		      /* This branch is predicted as not-taken.
		      If this is a forward branch, it is problematic.
                      Since we can't tell statically if it will branch forward,
                      always set the prediction bit. */
                      insn |= 0x00200000;   /* set the prediction bit */
                      modified = true;
		    }
                }
              else if ((insn & 0xFc00Fffe) == 0x4c000020)
                {
                  /* Instruction is BCLRx */
                  if ((!BO0(insn) || !BO2(insn)) && !BO4(insn))
		    {
		      /* This branch is predicted as not-taken.
		      If this is a forward branch, it is problematic.
                      Since we can't tell statically if it will branch forward,
                      always set the prediction bit. */
                      insn |= 0x00200000;   /* set the prediction bit */
                      modified = true;
		    }
                }
#undef BO0
#undef BO2
#undef BO4
              if (modified)
	        {
                  bfd_put_32 (abfd, insn, contents + isec_offset);
		  section_modified = true;
	        }
            }
        }
      if (section_modified)
	{
	  elf_section_data (isec)->this_hdr.contents = contents;
	  free_contents = NULL;
	}
    }

  if (rela_comb != NULL)
    {
      free (rela_comb);
      rela_comb = NULL;
    }

  if (free_relocs != NULL)
    {
      free (free_relocs);
      free_relocs = NULL;
    }

  if (free_contents != NULL)
    {
      if (! link_info->keep_memory)
	free (free_contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (isec)->this_hdr.contents = contents;
	}
      free_contents = NULL;
    }

  return true;

error_return:
  if (rela_comb != NULL)
    free (rela_comb);
  if (free_relocs != NULL)
    free (free_relocs);
  if (free_contents != NULL)
    free (free_contents);
  return false;
}


static reloc_howto_type *
ppc_elf_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  enum elf_ppc_reloc_type ppc_reloc = R_PPC_NONE;

  if (!ppc_elf_howto_table[R_PPC_ADDR32])
    /* Initialize howto table if needed */
    ppc_elf_howto_init ();

  switch ((int)code)
    {
    default:
      return (reloc_howto_type *)NULL;

    case BFD_RELOC_NONE:		ppc_reloc = R_PPC_NONE;			break;
    case BFD_RELOC_32:			ppc_reloc = R_PPC_ADDR32;		break;
    case BFD_RELOC_PPC_BA26:		ppc_reloc = R_PPC_ADDR24;		break;
    case BFD_RELOC_16:			ppc_reloc = R_PPC_ADDR16;		break;
    case BFD_RELOC_LO16:		ppc_reloc = R_PPC_ADDR16_LO;		break;
    case BFD_RELOC_HI16:		ppc_reloc = R_PPC_ADDR16_HI;		break;
    case BFD_RELOC_HI16_S:		ppc_reloc = R_PPC_ADDR16_HA;		break;
    case BFD_RELOC_PPC_BA16:		ppc_reloc = R_PPC_ADDR14;		break;
    case BFD_RELOC_PPC_BA16_BRTAKEN:	ppc_reloc = R_PPC_ADDR14_BRTAKEN;	break;
    case BFD_RELOC_PPC_BA16_BRNTAKEN:	ppc_reloc = R_PPC_ADDR14_BRNTAKEN;	break;
    case BFD_RELOC_PPC_B26:		ppc_reloc = R_PPC_REL24;		break;
    case BFD_RELOC_PPC_B16:		ppc_reloc = R_PPC_REL14;		break;
    case BFD_RELOC_PPC_B16_BRTAKEN:	ppc_reloc = R_PPC_REL14_BRTAKEN;	break;
    case BFD_RELOC_PPC_B16_BRNTAKEN:	ppc_reloc = R_PPC_REL14_BRNTAKEN;	break;
    case BFD_RELOC_16_GOTOFF:		ppc_reloc = R_PPC_GOT16;		break;
    case BFD_RELOC_LO16_GOTOFF:		ppc_reloc = R_PPC_GOT16_LO;		break;
    case BFD_RELOC_HI16_GOTOFF:		ppc_reloc = R_PPC_GOT16_HI;		break;
    case BFD_RELOC_HI16_S_GOTOFF:	ppc_reloc = R_PPC_GOT16_HA;		break;
    case BFD_RELOC_24_PLT_PCREL:	ppc_reloc = R_PPC_PLTREL24;		break;
    case BFD_RELOC_PPC_COPY:		ppc_reloc = R_PPC_COPY;			break;
    case BFD_RELOC_PPC_GLOB_DAT:	ppc_reloc = R_PPC_GLOB_DAT;		break;
    case BFD_RELOC_PPC_LOCAL24PC:	ppc_reloc = R_PPC_LOCAL24PC;		break;
    case BFD_RELOC_32_PCREL:		ppc_reloc = R_PPC_REL32;		break;
    case BFD_RELOC_32_PLTOFF:		ppc_reloc = R_PPC_PLT32;		break;
    case BFD_RELOC_32_PLT_PCREL:	ppc_reloc = R_PPC_PLTREL32;		break;
    case BFD_RELOC_LO16_PLTOFF:		ppc_reloc = R_PPC_PLT16_LO;		break;
    case BFD_RELOC_HI16_PLTOFF:		ppc_reloc = R_PPC_PLT16_HI;		break;
    case BFD_RELOC_HI16_S_PLTOFF:	ppc_reloc = R_PPC_PLT16_HA;		break;
    case BFD_RELOC_GPREL16:		ppc_reloc = R_PPC_SDAREL16;		break;
    case BFD_RELOC_32_BASEREL:		ppc_reloc = R_PPC_SECTOFF;		break;
    case BFD_RELOC_LO16_BASEREL:	ppc_reloc = R_PPC_SECTOFF_LO;		break;
    case BFD_RELOC_HI16_BASEREL:	ppc_reloc = R_PPC_SECTOFF_HI;		break;
    case BFD_RELOC_HI16_S_BASEREL:	ppc_reloc = R_PPC_SECTOFF_HA;		break;
    case BFD_RELOC_CTOR:		ppc_reloc = R_PPC_ADDR32;		break;
    case BFD_RELOC_PPC_TOC16:		ppc_reloc = R_PPC_TOC16;		break;
    case BFD_RELOC_PPC_EMB_NADDR32:	ppc_reloc = R_PPC_EMB_NADDR32;		break;
    case BFD_RELOC_PPC_EMB_NADDR16:	ppc_reloc = R_PPC_EMB_NADDR16;		break;
    case BFD_RELOC_PPC_EMB_NADDR16_LO:	ppc_reloc = R_PPC_EMB_NADDR16_LO;	break;
    case BFD_RELOC_PPC_EMB_NADDR16_HI:	ppc_reloc = R_PPC_EMB_NADDR16_HI;	break;
    case BFD_RELOC_PPC_EMB_NADDR16_HA:	ppc_reloc = R_PPC_EMB_NADDR16_HA;	break;
    case BFD_RELOC_PPC_EMB_SDAI16:	ppc_reloc = R_PPC_EMB_SDAI16;		break;
    case BFD_RELOC_PPC_EMB_SDA2I16:	ppc_reloc = R_PPC_EMB_SDA2I16;		break;
    case BFD_RELOC_PPC_EMB_SDA2REL:	ppc_reloc = R_PPC_EMB_SDA2REL;		break;
    case BFD_RELOC_PPC_EMB_SDA21:	ppc_reloc = R_PPC_EMB_SDA21;		break;
    case BFD_RELOC_PPC_EMB_MRKREF:	ppc_reloc = R_PPC_EMB_MRKREF;		break;
    case BFD_RELOC_PPC_EMB_RELSEC16:	ppc_reloc = R_PPC_EMB_RELSEC16;		break;
    case BFD_RELOC_PPC_EMB_RELST_LO:	ppc_reloc = R_PPC_EMB_RELST_LO;		break;
    case BFD_RELOC_PPC_EMB_RELST_HI:	ppc_reloc = R_PPC_EMB_RELST_HI;		break;
    case BFD_RELOC_PPC_EMB_RELST_HA:	ppc_reloc = R_PPC_EMB_RELST_HA;		break;
    case BFD_RELOC_PPC_EMB_BIT_FLD:	ppc_reloc = R_PPC_EMB_BIT_FLD;		break;
    case BFD_RELOC_PPC_EMB_RELSDA:	ppc_reloc = R_PPC_EMB_RELSDA;		break;
    case BFD_RELOC_VTABLE_INHERIT:	ppc_reloc = R_PPC_GNU_VTINHERIT;	break;
    case BFD_RELOC_VTABLE_ENTRY:	ppc_reloc = R_PPC_GNU_VTENTRY;		break;
    }

  return ppc_elf_howto_table[ (int)ppc_reloc ];
};

/* Set the howto pointer for a PowerPC ELF reloc.  */

static void
ppc_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf32_Internal_Rela *dst;
{
  if (!ppc_elf_howto_table[ R_PPC_ADDR32 ])	/* Initialize howto table if needed */
    ppc_elf_howto_init ();

  BFD_ASSERT (ELF32_R_TYPE (dst->r_info) < (unsigned int) R_PPC_max);
  cache_ptr->howto = ppc_elf_howto_table[ELF32_R_TYPE (dst->r_info)];
}

/* Handle the R_PPC_ADDR16_HA reloc.  */

static bfd_reloc_status_type
ppc_elf_addr16_ha_reloc (abfd, reloc_entry, symbol, data, input_section,
			 output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  bfd_vma relocation;

  if (output_bfd != NULL)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  reloc_entry->addend += (relocation & 0x8000) << 1;

  return bfd_reloc_continue;
}

/* Function to set whether a module needs the -mrelocatable bit set. */

static boolean
ppc_elf_set_private_flags (abfd, flags)
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
static boolean
ppc_elf_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  BFD_ASSERT (!elf_flags_init (obfd)
	      || elf_elfheader (obfd)->e_flags == elf_elfheader (ibfd)->e_flags);

  elf_elfheader (obfd)->e_flags = elf_elfheader (ibfd)->e_flags;
  elf_flags_init (obfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking */
static boolean
ppc_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags;
  flagword new_flags;
  boolean error;

  /* Check if we have the same endianess */
  if (ibfd->xvec->byteorder != obfd->xvec->byteorder
      && obfd->xvec->byteorder != BFD_ENDIAN_UNKNOWN)
    {
      const char *msg;

      if (bfd_big_endian (ibfd))
	msg = _("%s: compiled for a big endian system and target is little endian");
      else
	msg = _("%s: compiled for a little endian system and target is big endian");

      (*_bfd_error_handler) (msg, bfd_get_filename (ibfd));

      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;
  if (!elf_flags_init (obfd))	/* First call, no flags set */
    {
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  else if (new_flags == old_flags)	/* Compatible flags are ok */
    ;

  else					/* Incompatible flags */
    {
      /* Warn about -mrelocatable mismatch.  Allow -mrelocatable-lib to be linked
         with either.  */
      error = false;
      if ((new_flags & EF_PPC_RELOCATABLE) != 0
	  && (old_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0)
	{
	  error = true;
	  (*_bfd_error_handler)
	    (_("%s: compiled with -mrelocatable and linked with modules compiled normally"),
	     bfd_get_filename (ibfd));
	}
      else if ((new_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0
	       && (old_flags & EF_PPC_RELOCATABLE) != 0)
	{
	  error = true;
	  (*_bfd_error_handler)
	    (_("%s: compiled normally and linked with modules compiled with -mrelocatable"),
	     bfd_get_filename (ibfd));
	}

      /* The output is -mrelocatable-lib iff both the input files are.  */
      if (! (new_flags & EF_PPC_RELOCATABLE_LIB))
	elf_elfheader (obfd)->e_flags &= ~EF_PPC_RELOCATABLE_LIB;

      /* The output is -mrelocatable iff it can't be -mrelocatable-lib,
         but each input file is either -mrelocatable or -mrelocatable-lib.  */
      if (! (elf_elfheader (obfd)->e_flags & EF_PPC_RELOCATABLE_LIB)
	  && (new_flags & (EF_PPC_RELOCATABLE_LIB | EF_PPC_RELOCATABLE))
	  && (old_flags & (EF_PPC_RELOCATABLE_LIB | EF_PPC_RELOCATABLE)))
	elf_elfheader (obfd)->e_flags |= EF_PPC_RELOCATABLE;

      /* Do not warn about eabi vs. V.4 mismatch, just or in the bit if any module uses it */
      elf_elfheader (obfd)->e_flags |= (new_flags & EF_PPC_EMB);

      new_flags &= ~ (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB | EF_PPC_EMB);
      old_flags &= ~ (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB | EF_PPC_EMB);

      /* Warn about any other mismatches */
      if (new_flags != old_flags)
	{
	  error = true;
	  (*_bfd_error_handler)
	    (_("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)"),
	     bfd_get_filename (ibfd), (long)new_flags, (long)old_flags);
	}

      if (error)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
    }

  return true;
}


/* Handle a PowerPC specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.  */

static boolean
ppc_elf_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     char *name;
{
  asection *newsect;
  flagword flags;

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;

  newsect = hdr->bfd_section;
  flags = bfd_get_section_flags (abfd, newsect);
  if (hdr->sh_flags & SHF_EXCLUDE)
    flags |= SEC_EXCLUDE;

  if (hdr->sh_type == SHT_ORDERED)
    flags |= SEC_SORT_ENTRIES;

  bfd_set_section_flags (abfd, newsect, flags);
  return true;
}


/* Set up any other section flags and such that may be necessary.  */

static boolean
ppc_elf_fake_sections (abfd, shdr, asect)
     bfd *abfd ATTRIBUTE_UNUSED;
     Elf32_Internal_Shdr *shdr;
     asection *asect;
{
  if ((asect->flags & SEC_EXCLUDE) != 0)
    shdr->sh_flags |= SHF_EXCLUDE;

  if ((asect->flags & SEC_SORT_ENTRIES) != 0)
    shdr->sh_type = SHT_ORDERED;

  return true;
}


/* Create a special linker section */
static elf_linker_section_t *
ppc_elf_create_linker_section (abfd, info, which)
     bfd *abfd;
     struct bfd_link_info *info;
     enum elf_linker_section_enum which;
{
  bfd *dynobj = elf_hash_table (info)->dynobj;
  elf_linker_section_t *lsect;

  /* Record the first bfd section that needs the special section */
  if (!dynobj)
    dynobj = elf_hash_table (info)->dynobj = abfd;

  /* If this is the first time, create the section */
  lsect = elf_linker_section (dynobj, which);
  if (!lsect)
    {
      elf_linker_section_t defaults;
      static elf_linker_section_t zero_section;

      defaults = zero_section;
      defaults.which = which;
      defaults.hole_written_p = false;
      defaults.alignment = 2;

      /* Both of these sections are (technically) created by the user
	 putting data in them, so they shouldn't be marked
	 SEC_LINKER_CREATED.

	 The linker creates them so it has somewhere to attach their
	 respective symbols. In fact, if they were empty it would
	 be OK to leave the symbol set to 0 (or any random number), because
	 the appropriate register should never be used.  */
      defaults.flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
			| SEC_IN_MEMORY);

      switch (which)
	{
	default:
	  (*_bfd_error_handler) (_("%s: Unknown special linker type %d"),
				 bfd_get_filename (abfd),
				 (int)which);

	  bfd_set_error (bfd_error_bad_value);
	  return (elf_linker_section_t *)0;

	case LINKER_SECTION_SDATA:	/* .sdata/.sbss section */
	  defaults.name		  = ".sdata";
	  defaults.rel_name	  = ".rela.sdata";
	  defaults.bss_name	  = ".sbss";
	  defaults.sym_name	  = "_SDA_BASE_";
	  defaults.sym_offset	  = 32768;
	  break;

	case LINKER_SECTION_SDATA2:	/* .sdata2/.sbss2 section */
	  defaults.name		  = ".sdata2";
	  defaults.rel_name	  = ".rela.sdata2";
	  defaults.bss_name	  = ".sbss2";
	  defaults.sym_name	  = "_SDA2_BASE_";
	  defaults.sym_offset	  = 32768;
	  defaults.flags	 |= SEC_READONLY;
	  break;
	}

      lsect = _bfd_elf_create_linker_section (abfd, info, which, &defaults);
    }

  return lsect;
}


/* If we have a non-zero sized .sbss2 or .PPC.EMB.sbss0 sections, we need to bump up
   the number of section headers.  */

static int
ppc_elf_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;
  int ret;

  ret = 0;

  s = bfd_get_section_by_name (abfd, ".interp");
  if (s != NULL)
    ++ret;

  s = bfd_get_section_by_name (abfd, ".sbss2");
  if (s != NULL && (s->flags & SEC_LOAD) != 0 && s->_raw_size > 0)
    ++ret;

  s = bfd_get_section_by_name (abfd, ".PPC.EMB.sbss0");
  if (s != NULL && (s->flags & SEC_LOAD) != 0 && s->_raw_size > 0)
    ++ret;

  return ret;
}

/* Modify the segment map if needed */

static boolean
ppc_elf_modify_segment_map (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
  return true;
}

/* We have to create .dynsbss and .rela.sbss here so that they get mapped
   to output sections (just like _bfd_elf_create_dynamic_sections has
   to create .dynbss and .rela.bss).  */

static boolean
ppc_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  register asection *s;
  flagword flags;

  if (!_bfd_elf_create_dynamic_sections(abfd, info))
    return false;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".dynsbss");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
    return false;

  if (! info->shared)
    {
      s = bfd_make_section (abfd, ".rela.sbss");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return false;
    }
  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
ppc_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *s;
  unsigned int power_of_two;
  bfd_vma plt_offset;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_adjust_dynamic_symbol called for %s\n", h->root.root.string);
#endif

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
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! elf_hash_table (info)->dynamic_sections_created
	  || ((!info->shared || info->symbolic || h->dynindx == -1)
	      && (h->elf_link_hash_flags
		  & ELF_LINK_HASH_DEF_REGULAR) != 0)
	  || (info->shared && h->plt.refcount <= 0))
	{
	  /* A PLT entry is not required/allowed when:

	     1. We are not using ld.so; because then the PLT entry
	     can't be set up, so we can't use one.

	     2. We know for certain that a symbol is defined in
	     this object, because this object is the application,
	     is linked with -Bsymbolic, or because the symbol is local.

	     3. GC has rendered the entry unused.
	     Note, however, that in an executable all references to the
	     symbol go to the PLT, so we can't turn it off in that case.
	     ??? The correct thing to do here is to reference count
	     all uses of the symbol, not just those to the GOT or PLT.  */
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	  return true;
	}

      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	    return false;
	}
      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->_raw_size == 0)
	s->_raw_size += PLT_INITIAL_ENTRY_SIZE;

      /* The PowerPC PLT is actually composed of two parts, the first part
	 is 2 words (for a load and a jump), and then there is a remaining
	 word available at the end.  */
      plt_offset = (PLT_INITIAL_ENTRY_SIZE
		    + (PLT_SLOT_SIZE
		       * ((s->_raw_size - PLT_INITIAL_ENTRY_SIZE)
			  / PLT_ENTRY_SIZE)));

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (! info->shared
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = plt_offset;
	}

      h->plt.offset = plt_offset;

      /* Make room for this entry.  After the 8192nd entry, room
         for two entries is allocated.  */
      if ((s->_raw_size - PLT_INITIAL_ENTRY_SIZE) / PLT_ENTRY_SIZE
	  >= PLT_NUM_SINGLE_ENTRIES)
	s->_raw_size += 2 * PLT_ENTRY_SIZE;
      else
	s->_raw_size += PLT_ENTRY_SIZE;

      /* We also need to make an entry in the .rela.plt section.  */
      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->_raw_size += sizeof (Elf32_External_Rela);

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
     same memory location for the variable.

     Of course, if the symbol is sufficiently small, we must instead
     allocate it in .sbss.  FIXME: It would be better to do this if and
     only if there were actually SDAREL relocs for that symbol.  */

  if (h->size <= elf_gp_size (dynobj))
    s = bfd_get_section_by_name (dynobj, ".dynsbss");
  else
    s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_PPC_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      if (h->size <= elf_gp_size (dynobj))
	srel = bfd_get_section_by_name (dynobj, ".rela.sbss");
      else
	srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->_raw_size += sizeof (Elf32_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
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
ppc_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean plt;
  boolean relocs;
  boolean reltext;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_size_dynamic_sections called\n");
#endif

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
      /* We may have created entries in the .rela.got, .rela.sdata, and
	 .rela.sdata2 sections.  However, if we are not creating the
	 dynamic sections, we will not actually use these entries.  Reset
	 the size of .rela.got, et al, which will cause it to get
	 stripped from the output file below.  */
      static char *rela_sections[] = { ".rela.got", ".rela.sdata",
				       ".rela.sdata2", ".rela.sbss",
				       (char *)0 };
      char **p;

      for (p = rela_sections; *p != (char *)0; p++)
	{
	  s = bfd_get_section_by_name (dynobj, *p);
	  if (s != NULL)
	    s->_raw_size = 0;
	}
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = false;
  relocs = false;
  reltext = false;
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

      if (strcmp (name, ".plt") == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* Strip this section if we don't need it; see the
                 comment below.  */
	      strip = true;
	    }
	  else
	    {
	      /* Remember whether there is a PLT.  */
	      plt = true;
	    }
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the
		 output file.  This is mostly to handle .rela.bss and
		 .rela.plt.  We must create both sections in
		 create_dynamic_sections, because they must be created
		 before the linker maps input sections to output
		 sections.  The linker does that before
		 adjust_dynamic_symbol is called, and it is that
		 function which decides whether anything needs to go
		 into these sections.  */
	      strip = true;
	    }
	  else
	    {
	      asection *target;
	      const char *outname;

	      /* Remember whether there are any relocation sections. */
	      relocs = true;

	      /* If this relocation section applies to a read only
		 section, then we probably need a DT_TEXTREL entry.  */
	      outname = bfd_get_section_name (output_bfd,
					      s->output_section);
	      target = bfd_get_section_by_name (output_bfd, outname + 5);
	      if (target != NULL
		  && (target->flags & SEC_READONLY) != 0
		  && (target->flags & SEC_ALLOC) != 0)
		reltext = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strcmp (name, ".got") != 0
	       && strcmp (name, ".sdata") != 0
	       && strcmp (name, ".sdata2") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in ppc_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_DEBUG, 0))
	    return false;
	}

      if (plt)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_PLTGOT, 0)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_PLTREL, DT_RELA)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_JMPREL, 0))
	    return false;
	}

      if (relocs)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_RELA, 0)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_RELASZ, 0)
	      || ! bfd_elf32_add_dynamic_entry (info, DT_RELAENT,
						sizeof (Elf32_External_Rela)))
	    return false;
	}

      if (reltext)
	{
	  if (! bfd_elf32_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	}
    }

  return true;
}


/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static boolean
ppc_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  bfd_signed_vma *local_got_refcounts;
  elf_linker_section_t *sdata;
  elf_linker_section_t *sdata2;
  asection *sreloc;
  asection *sgot = NULL;
  asection *srelgot = NULL;

  if (info->relocateable)
    return true;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_check_relocs called for section %s in %s\n",
	   bfd_get_section_name (abfd, sec),
	   bfd_get_filename (abfd));
#endif

  /* Create the linker generated sections all the time so that the
     special symbols are created.  */

  if ((sdata = elf_linker_section (abfd, LINKER_SECTION_SDATA)) == NULL)
    {
      sdata = ppc_elf_create_linker_section (abfd, info, LINKER_SECTION_SDATA);
      if (!sdata)
	return false;
    }


  if ((sdata2 = elf_linker_section (abfd, LINKER_SECTION_SDATA2)) == NULL)
    {
      sdata2 = ppc_elf_create_linker_section (abfd, info, LINKER_SECTION_SDATA2);
      if (!sdata2)
	return false;
    }

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  local_got_refcounts = elf_local_got_refcounts (abfd);

  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      /* If a relocation refers to _GLOBAL_OFFSET_TABLE_, create the .got.
	 This shows up in particular in an R_PPC_ADDR32 in the eabi
	 startup code.  */
      if (h && strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	{
	  if (sgot == NULL)
	    {
	      if (dynobj == NULL)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _bfd_elf_create_got_section (dynobj, info))
		return false;
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }
	}

      switch (ELF32_R_TYPE (rel->r_info))
	{
	/* GOT16 relocations */
	case R_PPC_GOT16:
	case R_PPC_GOT16_LO:
	case R_PPC_GOT16_HI:
	case R_PPC_GOT16_HA:
	  /* This symbol requires a global offset table entry.  */

	  if (sgot == NULL)
	    {
	      if (dynobj == NULL)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _bfd_elf_create_got_section (dynobj, info))
		return false;
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (srelgot == NULL
	      && (h != NULL || info->shared))
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
		      || ! bfd_set_section_alignment (dynobj, srelgot, 2))
		    return false;
		}
	    }

	  if (h != NULL)
	    {
	      if (h->got.refcount == -1)
		{
		  /* Make sure this symbol is output as a dynamic symbol.  */
		  if (h->dynindx == -1)
		    if (!bfd_elf32_link_record_dynamic_symbol (info, h))
		      return false;

		  /* Allocate space in the .got.  */
		  sgot->_raw_size += 4;
		  /* Allocate relocation space.  */
		  srelgot->_raw_size += sizeof (Elf32_External_Rela);

		  h->got.refcount = 1;
		}
	      else
		h->got.refcount++;
	    }
	  else
	    {
	      /* This is a global offset table entry for a local symbol.  */
	      if (local_got_refcounts == NULL)
		{
		  size_t size;

		  size = symtab_hdr->sh_info * sizeof (bfd_signed_vma);
		  local_got_refcounts = (bfd_signed_vma *)
		    bfd_alloc (abfd, size);
		  if (local_got_refcounts == NULL)
		    return false;
		  elf_local_got_refcounts (abfd) = local_got_refcounts;
		  memset (local_got_refcounts, -1, size);
		}
	      if (local_got_refcounts[r_symndx] == -1)
		{
		  sgot->_raw_size += 4;

		  /* If we are generating a shared object, we need to
                     output a R_PPC_RELATIVE reloc so that the
                     dynamic linker can adjust this GOT entry.  */
		  if (info->shared)
		    srelgot->_raw_size += sizeof (Elf32_External_Rela);

		  local_got_refcounts[r_symndx] = 1;
		}
	      else
		local_got_refcounts[r_symndx]++;
	    }
	  break;

	/* Indirect .sdata relocation */
	case R_PPC_EMB_SDAI16:
	  if (info->shared)
	    {
	      ((*_bfd_error_handler)
	       (_("%s: relocation %s cannot be used when making a shared object"),
		bfd_get_filename (abfd), "R_PPC_EMB_SDAI16"));
	       return false;
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
		      || ! bfd_set_section_alignment (dynobj, srelgot, 2))
		    return false;
		}
	    }

	  if (!bfd_elf32_create_pointer_linker_section (abfd, info, sdata, h, rel))
	    return false;

	  break;

	/* Indirect .sdata2 relocation */
	case R_PPC_EMB_SDA2I16:
	  if (info->shared)
	    {
	      ((*_bfd_error_handler)
	       (_("%s: relocation %s cannot be used when making a shared object"),
		bfd_get_filename (abfd), "R_PPC_EMB_SDA2I16"));
	      return false;
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
		      || ! bfd_set_section_alignment (dynobj, srelgot, 2))
		    return false;
		}
	    }

	  if (!bfd_elf32_create_pointer_linker_section (abfd, info, sdata2, h, rel))
	    return false;

	  break;

	case R_PPC_SDAREL16:
	case R_PPC_EMB_SDA2REL:
	case R_PPC_EMB_SDA21:
	  if (info->shared)
	    {
	      ((*_bfd_error_handler)
	       (_("%s: relocation %s cannot be used when making a shared object"),
		bfd_get_filename (abfd),
		ppc_elf_howto_table[(int)ELF32_R_TYPE (rel->r_info)]->name));
	      return false;
	    }
	  break;

	case R_PPC_PLT32:
	case R_PPC_PLTREL24:
	case R_PPC_PLT16_LO:
	case R_PPC_PLT16_HI:
	case R_PPC_PLT16_HA:
#ifdef DEBUG
	  fprintf (stderr, "Reloc requires a PLT entry\n");
#endif
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
	      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
		return false;
	    }
	  if (h->plt.refcount == -1)
	    {
	      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	      h->plt.refcount = 1;
	    }
	  else
	    h->plt.refcount++;
	  break;

	  /* The following relocations don't need to propagate the
	     relocation if linking a shared object since they are
	     section relative.  */
	case R_PPC_SECTOFF:
	case R_PPC_SECTOFF_LO:
	case R_PPC_SECTOFF_HI:
	case R_PPC_SECTOFF_HA:
	  break;

	  /* This refers only to functions defined in the shared library */
	case R_PPC_LOCAL24PC:
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_PPC_GNU_VTINHERIT:
	  if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_PPC_GNU_VTENTRY:
	  if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return false;
	  break;

	  /* When creating a shared object, we must copy these
	     relocs into the output file.  We create a reloc
	     section in dynobj and make room for the reloc.  */
	case R_PPC_REL24:
	case R_PPC_REL14:
	case R_PPC_REL14_BRTAKEN:
	case R_PPC_REL14_BRNTAKEN:
	case R_PPC_REL32:
	  if (h == NULL
	      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	  /* fall through */

	default:
	  if (info->shared)
	    {
#ifdef DEBUG
	      fprintf (stderr, "ppc_elf_check_relocs need to create relocation for %s\n",
		       (h && h->root.root.string) ? h->root.root.string : "<unknown>");
#endif
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
			  || ! bfd_set_section_alignment (dynobj, sreloc, 2))
			return false;
		    }
		}

	      sreloc->_raw_size += sizeof (Elf32_External_Rela);

	      /* FIXME: We should here do what the m68k and i386
		 backends do: if the reloc is pc-relative, record it
		 in case it turns out that the reloc is unnecessary
		 because the symbol is forced local by versioning or
		 we are linking with -Bdynamic.  Fortunately this
		 case is not frequent.  */
	    }

	  break;
	}
    }

  return true;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
ppc_elf_gc_mark_hook (abfd, info, rel, h, sym)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_PPC_GNU_VTINHERIT:
	case R_PPC_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    {
      if (!(elf_bad_symtab (abfd)
	    && ELF_ST_BIND (sym->st_info) != STB_LOCAL)
	  && ! ((sym->st_shndx <= 0 || sym->st_shndx >= SHN_LORESERVE)
		&& sym->st_shndx != SHN_COMMON))
	{
	  return bfd_section_from_elf_index (abfd, sym->st_shndx);
	}
    }

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static boolean
ppc_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  unsigned long r_symndx;
  struct elf_link_hash_entry *h;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_PPC_GOT16:
      case R_PPC_GOT16_LO:
      case R_PPC_GOT16_HI:
      case R_PPC_GOT16_HA:
	r_symndx = ELF32_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->got.refcount > 0)
	      h->got.refcount--;
	  }
	else
	  {
	    if (local_got_refcounts[r_symndx] > 0)
	      local_got_refcounts[r_symndx]--;
	  }
        break;

      case R_PPC_PLT32:
      case R_PPC_PLTREL24:
      case R_PPC_PLT16_LO:
      case R_PPC_PLT16_HI:
      case R_PPC_PLT16_HA:
	r_symndx = ELF32_R_SYM (rel->r_info);
	if (r_symndx >= symtab_hdr->sh_info)
	  {
	    h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	    if (h->plt.refcount > 0)
	      h->plt.refcount--;
	  }
	break;

      default:
	break;
      }

  return true;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We use it to put .comm items in .sbss, and not .bss.  */

/*ARGSUSED*/
static boolean
ppc_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep ATTRIBUTE_UNUSED;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (sym->st_shndx == SHN_COMMON
      && !info->relocateable
      && sym->st_size <= (bfd_vma) bfd_get_gp_size (abfd))
    {
      /* Common symbols less than or equal to -G nn bytes are automatically
	 put into .sdata.  */
      elf_linker_section_t *sdata
	= ppc_elf_create_linker_section (abfd, info, LINKER_SECTION_SDATA);

      if (!sdata->bss_section)
	{
	  /* We don't go through bfd_make_section, because we don't
             want to attach this common section to DYNOBJ.  The linker
             will move the symbols to the appropriate output section
             when it defines common symbols.  */
	  sdata->bss_section = ((asection *)
				bfd_zalloc (abfd, sizeof (asection)));
	  if (sdata->bss_section == NULL)
	    return false;
	  sdata->bss_section->name = sdata->bss_name;
	  sdata->bss_section->flags = SEC_IS_COMMON;
	  sdata->bss_section->output_section = sdata->bss_section;
	  sdata->bss_section->symbol =
	    (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
	  sdata->bss_section->symbol_ptr_ptr =
	    (asymbol **) bfd_zalloc (abfd, sizeof (asymbol *));
	  if (sdata->bss_section->symbol == NULL
	      || sdata->bss_section->symbol_ptr_ptr == NULL)
	    return false;
	  sdata->bss_section->symbol->name = sdata->bss_name;
	  sdata->bss_section->symbol->flags = BSF_SECTION_SYM;
	  sdata->bss_section->symbol->section = sdata->bss_section;
	  *sdata->bss_section->symbol_ptr_ptr = sdata->bss_section->symbol;
	}

      *secp = sdata->bss_section;
      *valp = sym->st_size;
    }

  return true;
}


/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
ppc_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_finish_dynamic_symbol called for %s",
	   h->root.root.string);
#endif

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *srela;
      Elf_Internal_Rela rela;
      bfd_vma reloc_index;

#ifdef DEBUG
      fprintf (stderr, ", plt_offset = %d", h->plt.offset);
#endif

      /* This symbol has an entry in the procedure linkage table.  Set
         it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      srela = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && srela != NULL);

      /* We don't need to fill in the .plt.  The ppc dynamic linker
	 will fill it in.  */

      /* Fill in the entry in the .rela.plt section.  */
      rela.r_offset = (splt->output_section->vma
		       + splt->output_offset
		       + h->plt.offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_PPC_JMP_SLOT);
      rela.r_addend = 0;

      reloc_index = (h->plt.offset - PLT_INITIAL_ENTRY_SIZE) / PLT_SLOT_SIZE;
      if (reloc_index > PLT_NUM_SINGLE_ENTRIES)
	reloc_index -= (reloc_index - PLT_NUM_SINGLE_ENTRIES) / 2;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) srela->contents
				  + reloc_index));

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

      /* This symbol has an entry in the global offset table.  Set it
         up.  */

      sgot = bfd_get_section_by_name (dynobj, ".got");
      srela = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srela != NULL);

      rela.r_offset = (sgot->output_section->vma
		       + sgot->output_offset
		       + (h->got.offset &~ 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  The entry in
	 the global offset table will already have been initialized in
	 the relocate_section function.  */
      if (info->shared
	  && (info->symbolic || h->dynindx == -1)
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  rela.r_info = ELF32_R_INFO (0, R_PPC_RELATIVE);
	  rela.r_addend = (h->root.u.def.value
			   + h->root.u.def.section->output_section->vma
			   + h->root.u.def.section->output_offset);
	}
      else
	{
	  BFD_ASSERT((h->got.offset & 1) == 0);
	  bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + h->got.offset);
	  rela.r_info = ELF32_R_INFO (h->dynindx, R_PPC_GLOB_DAT);
	  rela.r_addend = 0;
	}

      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) srela->contents
				  + srela->reloc_count));
      ++srela->reloc_count;
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      asection *s;
      Elf_Internal_Rela rela;

      /* This symbols needs a copy reloc.  Set it up.  */

#ifdef DEBUG
      fprintf (stderr, ", copy");
#endif

      BFD_ASSERT (h->dynindx != -1);

      if (h->size <= elf_gp_size (dynobj))
	s = bfd_get_section_by_name (h->root.u.def.section->owner,
				     ".rela.sbss");
      else
	s = bfd_get_section_by_name (h->root.u.def.section->owner,
				     ".rela.bss");
      BFD_ASSERT (s != NULL);

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF32_R_INFO (h->dynindx, R_PPC_COPY);
      rela.r_addend = 0;
      bfd_elf32_swap_reloca_out (output_bfd, &rela,
				 ((Elf32_External_Rela *) s->contents
				  + s->reloc_count));
      ++s->reloc_count;
    }

#ifdef DEBUG
  fprintf (stderr, "\n");
#endif

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0
      || strcmp (h->root.root.string, "_PROCEDURE_LINKAGE_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}


/* Finish up the dynamic sections.  */

static boolean
ppc_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  asection *sdyn;
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *sgot = bfd_get_section_by_name (dynobj, ".got");

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_finish_dynamic_sections called\n");
#endif

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      splt = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (splt != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  boolean size;

	  bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    case DT_PLTGOT:   name = ".plt";	  size = false; break;
	    case DT_PLTRELSZ: name = ".rela.plt"; size = true;  break;
	    case DT_JMPREL:   name = ".rela.plt"; size = false; break;
	    default:	      name = NULL;	  size = false; break;
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
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	    }
	}
    }

  /* Add a blrl instruction at _GLOBAL_OFFSET_TABLE_-4 so that a function can
     easily find the address of the _GLOBAL_OFFSET_TABLE_.  */
  if (sgot)
    {
      unsigned char *contents = sgot->contents;
      bfd_put_32 (output_bfd, 0x4e800021 /* blrl */, contents);

      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, contents+4);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    contents+4);

      elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;
    }

  return true;
}


/* The RELOCATE_SECTION function is called by the ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjust the section contents as
   necessary, and (if using Rela relocs and generating a
   relocateable output file) adjusting the reloc addend as
   necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocateable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static boolean
ppc_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr		  = &elf_tdata (input_bfd)->symtab_hdr;
  struct elf_link_hash_entry **sym_hashes = elf_sym_hashes (input_bfd);
  bfd *dynobj				  = elf_hash_table (info)->dynobj;
  elf_linker_section_t *sdata		  = (dynobj) ? elf_linker_section (dynobj, LINKER_SECTION_SDATA)  : NULL;
  elf_linker_section_t *sdata2		  = (dynobj) ? elf_linker_section (dynobj, LINKER_SECTION_SDATA2) : NULL;
  Elf_Internal_Rela *rel		  = relocs;
  Elf_Internal_Rela *relend		  = relocs + input_section->reloc_count;
  asection *sreloc			  = NULL;
  asection *splt;
  asection *sgot;
  bfd_vma *local_got_offsets;
  boolean ret				  = true;
  long insn;

#ifdef DEBUG
  fprintf (stderr, "ppc_elf_relocate_section called for %s section %s, %ld relocations%s\n",
	   bfd_get_filename (input_bfd),
	   bfd_section_name(input_bfd, input_section),
	   (long)input_section->reloc_count,
	   (info->relocateable) ? " (relocatable)" : "");
#endif

  if (!ppc_elf_howto_table[ R_PPC_ADDR32 ])	/* Initialize howto table if needed */
    ppc_elf_howto_init ();

  local_got_offsets = elf_local_got_offsets (input_bfd);

  splt = sgot = NULL;
  if (dynobj != NULL)
    {
      splt = bfd_get_section_by_name (dynobj, ".plt");
      sgot = bfd_get_section_by_name (dynobj, ".got");
    }

  for (; rel < relend; rel++)
    {
      enum elf_ppc_reloc_type r_type	= (enum elf_ppc_reloc_type)ELF32_R_TYPE (rel->r_info);
      bfd_vma offset			= rel->r_offset;
      bfd_vma addend			= rel->r_addend;
      bfd_reloc_status_type r		= bfd_reloc_other;
      Elf_Internal_Sym *sym		= (Elf_Internal_Sym *)0;
      asection *sec			= (asection *)0;
      struct elf_link_hash_entry *h	= (struct elf_link_hash_entry *)0;
      const char *sym_name		= (const char *)0;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      bfd_vma relocation;

      /* Unknown relocation handling */
      if ((unsigned)r_type >= (unsigned)R_PPC_max || !ppc_elf_howto_table[(int)r_type])
	{
	  (*_bfd_error_handler) (_("%s: unknown relocation type %d"),
				 bfd_get_filename (input_bfd),
				 (int)r_type);

	  bfd_set_error (bfd_error_bad_value);
	  ret = false;
	  continue;
	}

      howto = ppc_elf_howto_table[(int)r_type];
      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocateable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if ((unsigned)ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  addend = rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

#ifdef DEBUG
	  fprintf (stderr, "\ttype = %s (%d), symbol index = %ld, offset = %ld, addend = %ld\n",
		   howto->name,
		   (int)r_type,
		   r_symndx,
		   (long)offset,
		   (long)addend);
#endif
	  continue;
	}

      /* This is a final link.  */
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  sym_name = "<local symbol>";

	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
	}
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  sym_name = h->root.root.string;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      if ((r_type == R_PPC_PLT32
		   && h->plt.offset != (bfd_vma) -1)
		  || (r_type == R_PPC_LOCAL24PC
		      && sec->output_section == NULL)
		  || ((r_type == R_PPC_GOT16
		       || r_type == R_PPC_GOT16_LO
		       || r_type == R_PPC_GOT16_HI
		       || r_type == R_PPC_GOT16_HA)
		      && elf_hash_table (info)->dynamic_sections_created
		      && (! info->shared
			  || (! info->symbolic && h->dynindx != -1)
			  || (h->elf_link_hash_flags
			      & ELF_LINK_HASH_DEF_REGULAR) == 0))
		  || (info->shared
		      && ((! info->symbolic && h->dynindx != -1)
			  || (h->elf_link_hash_flags
			      & ELF_LINK_HASH_DEF_REGULAR) == 0)
		      && ((input_section->flags & SEC_ALLOC) != 0
			  /* Testing SEC_DEBUGGING here may be wrong.
                             It's here to avoid a crash when
                             generating a shared library with DWARF
                             debugging information.  */
		          || (input_section->flags & SEC_DEBUGGING) != 0)
		      && (r_type == R_PPC_ADDR32
			  || r_type == R_PPC_ADDR24
			  || r_type == R_PPC_ADDR16
			  || r_type == R_PPC_ADDR16_LO
			  || r_type == R_PPC_ADDR16_HI
			  || r_type == R_PPC_ADDR16_HA
			  || r_type == R_PPC_ADDR14
			  || r_type == R_PPC_ADDR14_BRTAKEN
			  || r_type == R_PPC_ADDR14_BRNTAKEN
			  || r_type == R_PPC_PLTREL24
			  || r_type == R_PPC_COPY
			  || r_type == R_PPC_GLOB_DAT
			  || r_type == R_PPC_JMP_SLOT
			  || r_type == R_PPC_UADDR32
			  || r_type == R_PPC_UADDR16
			  || r_type == R_PPC_SDAREL16
			  || r_type == R_PPC_EMB_NADDR32
			  || r_type == R_PPC_EMB_NADDR16
			  || r_type == R_PPC_EMB_NADDR16_LO
			  || r_type == R_PPC_EMB_NADDR16_HI
			  || r_type == R_PPC_EMB_NADDR16_HA
			  || r_type == R_PPC_EMB_SDAI16
			  || r_type == R_PPC_EMB_SDA2I16
			  || r_type == R_PPC_EMB_SDA2REL
			  || r_type == R_PPC_EMB_SDA21
			  || r_type == R_PPC_EMB_MRKREF
			  || r_type == R_PPC_EMB_BIT_FLD
			  || r_type == R_PPC_EMB_RELSDA
			  || ((r_type == R_PPC_REL24
			       || r_type == R_PPC_REL32
			       || r_type == R_PPC_REL14
			       || r_type == R_PPC_REL14_BRTAKEN
			       || r_type == R_PPC_REL14_BRNTAKEN
			       || r_type == R_PPC_RELATIVE)
			      && strcmp (h->root.root.string,
					 "_GLOBAL_OFFSET_TABLE_") != 0))))
		{
		  /* In these cases, we don't need the relocation
                     value.  We check specially because in some
                     obscure cases sec->output_section will be NULL.  */
		  relocation = 0;
		}
	      else if (sec->output_section == NULL)
	        {
                  (*_bfd_error_handler)
                    (_("%s: warning: unresolvable relocation against symbol `%s' from %s section"),
                     bfd_get_filename (input_bfd), h->root.root.string,
                     bfd_get_section_name (input_bfd, input_section));
		  relocation = 0;
	        }
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->shared && !info->symbolic
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (! (*info->callbacks->undefined_symbol)(info,
							 h->root.root.string,
							 input_bfd,
							 input_section,
							 rel->r_offset,
							 (!info->shared
							  || info->no_undefined
							  || ELF_ST_VISIBILITY (h->other))))
		return false;
	      relocation = 0;
	    }
	}

      switch ((int)r_type)
	{
	default:
	  (*_bfd_error_handler) (_("%s: unknown relocation type %d for symbol %s"),
				 bfd_get_filename (input_bfd),
				 (int)r_type, sym_name);

	  bfd_set_error (bfd_error_bad_value);
	  ret = false;
	  continue;

	/* Relocations that need no special processing.  */
	case (int)R_PPC_LOCAL24PC:
	  /* It makes no sense to point a local relocation
	     at a symbol not in this object.  */
	  if (h != NULL
	      && (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak)
	      && sec->output_section == NULL)
	    {
	      if (! (*info->callbacks->undefined_symbol) (info,
							  h->root.root.string,
							  input_bfd,
							  input_section,
							  rel->r_offset,
							  true))
		return false;
	      continue;
	    }
	  break;

	/* Relocations that may need to be propagated if this is a shared
           object.  */
	case (int)R_PPC_REL24:
	case (int)R_PPC_REL32:
	case (int)R_PPC_REL14:
	  /* If these relocations are not to a named symbol, they can be
	     handled right here, no need to bother the dynamic linker.  */
	  if (h == NULL
	      || strcmp (h->root.root.string, "_GLOBAL_OFFSET_TABLE_") == 0)
	    break;
	/* fall through */

	/* Relocations that always need to be propagated if this is a shared
           object.  */
	case (int)R_PPC_NONE:
	case (int)R_PPC_ADDR32:
	case (int)R_PPC_ADDR24:
	case (int)R_PPC_ADDR16:
	case (int)R_PPC_ADDR16_LO:
	case (int)R_PPC_ADDR16_HI:
	case (int)R_PPC_ADDR16_HA:
	case (int)R_PPC_ADDR14:
	case (int)R_PPC_UADDR32:
	case (int)R_PPC_UADDR16:
	  if (info->shared)
	    {
	      Elf_Internal_Rela outrel;
	      boolean skip;

#ifdef DEBUG
	      fprintf (stderr, "ppc_elf_relocate_section need to create relocation for %s\n",
		       (h && h->root.root.string) ? h->root.root.string : "<unknown>");
#endif

	      /* When generating a shared object, these relocations
                 are copied into the output file to be resolved at run
                 time.  */

	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (input_bfd,
			   elf_elfheader (input_bfd)->e_shstrndx,
			   elf_section_data (input_section)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
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

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      /* h->dynindx may be -1 if this symbol was marked to
                 become local.  */
	      else if (h != NULL
		       && ((! info->symbolic && h->dynindx != -1)
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))
		{
		  BFD_ASSERT (h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  if (r_type == R_PPC_ADDR32)
		    {
		      outrel.r_info = ELF32_R_INFO (0, R_PPC_RELATIVE);
		      outrel.r_addend = relocation + rel->r_addend;
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
			  BFD_ASSERT(indx > 0);
#ifdef DEBUG
			  if (indx <= 0)
			    {
			      printf("indx=%d section=%s flags=%08x name=%s\n",
				     indx, osec->name, osec->flags,
				     h->root.root.string);
			    }
#endif
			}

		      outrel.r_info = ELF32_R_INFO (indx, r_type);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
					 (((Elf32_External_Rela *)
					   sreloc->contents)
					  + sreloc->reloc_count));
	      ++sreloc->reloc_count;

	      /* This reloc will be computed at runtime, so there's no
                 need to do anything now, unless this is a RELATIVE
                 reloc in an unallocated section.  */
	      if (skip
		  || (input_section->flags & SEC_ALLOC) != 0
		  || ELF32_R_TYPE (outrel.r_info) != R_PPC_RELATIVE)
		continue;
	    }

	  /* Arithmetic adjust relocations that aren't going into a
	     shared object.  */
	  if (r_type == R_PPC_ADDR16_HA
	      /* It's just possible that this symbol is a weak symbol
		 that's not actually defined anywhere. In that case,
		 'sec' would be NULL, and we should leave the symbol
		 alone (it will be set to zero elsewhere in the link).  */
	      && sec != NULL)
	    {
	      addend += ((relocation + addend) & 0x8000) << 1;
	    }
	  break;

	/* branch taken prediction relocations */
	case (int)R_PPC_ADDR14_BRTAKEN:
	case (int)R_PPC_REL14_BRTAKEN:
	  insn = bfd_get_32 (output_bfd, contents + offset);
	  if ((relocation - offset) & 0x8000)
	    insn &= ~BRANCH_PREDICT_BIT;
	  else
	    insn |= BRANCH_PREDICT_BIT;
	  bfd_put_32 (output_bfd, insn, contents + offset);
	  break;

	/* branch not taken predicition relocations */
	case (int)R_PPC_ADDR14_BRNTAKEN:
	case (int)R_PPC_REL14_BRNTAKEN:
	  insn = bfd_get_32 (output_bfd, contents + offset);
	  if ((relocation - offset) & 0x8000)
	    insn |= BRANCH_PREDICT_BIT;
	  else
	    insn &= ~BRANCH_PREDICT_BIT;
	  bfd_put_32 (output_bfd, insn, contents + offset);
	  break;

	/* GOT16 relocations */
	case (int)R_PPC_GOT16:
	case (int)R_PPC_GOT16_LO:
	case (int)R_PPC_GOT16_HI:
	case (int)R_PPC_GOT16_HA:
	  /* Relocation is to the entry for this symbol in the global
             offset table.  */
	  BFD_ASSERT (sgot != NULL);

	  if (h != NULL)
	    {
	      bfd_vma off;

	      off = h->got.offset;
	      BFD_ASSERT (off != (bfd_vma) -1);

	      if (! elf_hash_table (info)->dynamic_sections_created
		  || (info->shared
		      && (info->symbolic || h->dynindx == -1)
		      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
		{
		  /* This is actually a static link, or it is a
                     -Bsymbolic link and the symbol is defined
                     locally.  We must initialize this entry in the
                     global offset table.  Since the offset must
                     always be a multiple of 4, we use the least
                     significant bit to record whether we have
                     initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.  This
		     is done in the finish_dynamic_symbol routine.  */
		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_32 (output_bfd, relocation,
				  sgot->contents + off);
		      h->got.offset |= 1;
		    }
		}

	      relocation = sgot->output_offset + off - 4;
	    }
	  else
	    {
	      bfd_vma off;

	      BFD_ASSERT (local_got_offsets != NULL
			  && local_got_offsets[r_symndx] != (bfd_vma) -1);

	      off = local_got_offsets[r_symndx];

	      /* The offset must always be a multiple of 4.  We use
		 the least significant bit to record whether we have
		 already processed this entry.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  bfd_put_32 (output_bfd, relocation, sgot->contents + off);

		  if (info->shared)
		    {
		      asection *srelgot;
		      Elf_Internal_Rela outrel;

		      /* We need to generate a R_PPC_RELATIVE reloc
			 for the dynamic linker.  */
		      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
		      BFD_ASSERT (srelgot != NULL);

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);
		      outrel.r_info = ELF32_R_INFO (0, R_PPC_RELATIVE);
		      outrel.r_addend = relocation;
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel,
						 (((Elf32_External_Rela *)
						   srelgot->contents)
						  + srelgot->reloc_count));
		      ++srelgot->reloc_count;
		    }

		  local_got_offsets[r_symndx] |= 1;
		}

	      relocation = sgot->output_offset + off - 4;
	    }
	  break;

	/* Indirect .sdata relocation */
	case (int)R_PPC_EMB_SDAI16:
	  BFD_ASSERT (sdata != NULL);
	  relocation = bfd_elf32_finish_pointer_linker_section (output_bfd, input_bfd, info,
								sdata, h, relocation, rel,
								R_PPC_RELATIVE);
	  break;

	/* Indirect .sdata2 relocation */
	case (int)R_PPC_EMB_SDA2I16:
	  BFD_ASSERT (sdata2 != NULL);
	  relocation = bfd_elf32_finish_pointer_linker_section (output_bfd, input_bfd, info,
								sdata2, h, relocation, rel,
								R_PPC_RELATIVE);
	  break;

	/* Handle the TOC16 reloc.  We want to use the offset within the .got
	   section, not the actual VMA.  This is appropriate when generating
	   an embedded ELF object, for which the .got section acts like the
	   AIX .toc section.  */
	case (int)R_PPC_TOC16:			/* phony GOT16 relocations */
	  BFD_ASSERT (sec != (asection *)0);
	  BFD_ASSERT (bfd_is_und_section (sec)
		      || strcmp (bfd_get_section_name (abfd, sec), ".got") == 0
		      || strcmp (bfd_get_section_name (abfd, sec), ".cgot") == 0)

	  addend -= sec->output_section->vma + sec->output_offset + 0x8000;
	  break;

	case (int)R_PPC_PLTREL24:
	  /* Relocation is to the entry for this symbol in the
             procedure linkage table.  */
	  BFD_ASSERT (h != NULL);

	  if (h->plt.offset == (bfd_vma) -1
	      || splt == NULL)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
                 happens when statically linking PIC code, or when
                 using -Bsymbolic.  */
	      break;
	    }

	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt.offset);
 	  break;

	/* relocate against _SDA_BASE_ */
	case (int)R_PPC_SDAREL16:
	  {
	    const char *name;

	    BFD_ASSERT (sec != (asection *)0);
	    name = bfd_get_section_name (abfd, sec->output_section);
	    if (strcmp (name, ".sdata") != 0
		&& strcmp (name, ".sbss") != 0)
	      {
		(*_bfd_error_handler) (_("%s: The target (%s) of a %s relocation is in the wrong output section (%s)"),
				       bfd_get_filename (input_bfd),
				       sym_name,
				       ppc_elf_howto_table[ (int)r_type ]->name,
				       name);
	      }
	    addend -= (sdata->sym_hash->root.u.def.value
		       + sdata->sym_hash->root.u.def.section->output_section->vma
		       + sdata->sym_hash->root.u.def.section->output_offset);
	  }
	  break;


	/* relocate against _SDA2_BASE_ */
	case (int)R_PPC_EMB_SDA2REL:
	  {
	    const char *name;

	    BFD_ASSERT (sec != (asection *)0);
	    name = bfd_get_section_name (abfd, sec->output_section);
	    if (strcmp (name, ".sdata2") != 0 && strcmp (name, ".sbss2") != 0)
	      {
		(*_bfd_error_handler) (_("%s: The target (%s) of a %s relocation is in the wrong output section (%s)"),
				       bfd_get_filename (input_bfd),
				       sym_name,
				       ppc_elf_howto_table[ (int)r_type ]->name,
				       name);
		
		bfd_set_error (bfd_error_bad_value);
		ret = false;
		continue;
	      }
	    addend -= (sdata2->sym_hash->root.u.def.value
		       + sdata2->sym_hash->root.u.def.section->output_section->vma
		       + sdata2->sym_hash->root.u.def.section->output_offset);
	  }
	  break;


	/* relocate against either _SDA_BASE_, _SDA2_BASE_, or 0 */
	case (int)R_PPC_EMB_SDA21:
	case (int)R_PPC_EMB_RELSDA:
	  {
	    const char *name;
	    int reg;

	    BFD_ASSERT (sec != (asection *)0);
	    name = bfd_get_section_name (abfd, sec->output_section);
	    if (strcmp (name, ".sdata") == 0 || strcmp (name, ".sbss") == 0)
	      {
		reg = 13;
		addend -= (sdata->sym_hash->root.u.def.value
			   + sdata->sym_hash->root.u.def.section->output_section->vma
			   + sdata->sym_hash->root.u.def.section->output_offset);
	      }

	    else if (strcmp (name, ".sdata2") == 0 || strcmp (name, ".sbss2") == 0)
	      {
		reg = 2;
		addend -= (sdata2->sym_hash->root.u.def.value
			   + sdata2->sym_hash->root.u.def.section->output_section->vma
			   + sdata2->sym_hash->root.u.def.section->output_offset);
	      }

	    else if (strcmp (name, ".PPC.EMB.sdata0") == 0 || strcmp (name, ".PPC.EMB.sbss0") == 0)
	      {
		reg = 0;
	      }

	    else
	      {
		(*_bfd_error_handler) (_("%s: The target (%s) of a %s relocation is in the wrong output section (%s)"),
				       bfd_get_filename (input_bfd),
				       sym_name,
				       ppc_elf_howto_table[ (int)r_type ]->name,
				       name);

		bfd_set_error (bfd_error_bad_value);
		ret = false;
		continue;
	      }

	    if (r_type == R_PPC_EMB_SDA21)
	      {			/* fill in register field */
		insn = bfd_get_32 (output_bfd, contents + offset);
		insn = (insn & ~RA_REGISTER_MASK) | (reg << RA_REGISTER_SHIFT);
		bfd_put_32 (output_bfd, insn, contents + offset);
	      }
	  }
	  break;

	/* Relocate against the beginning of the section */
	case (int)R_PPC_SECTOFF:
	case (int)R_PPC_SECTOFF_LO:
	case (int)R_PPC_SECTOFF_HI:
	  BFD_ASSERT (sec != (asection *)0);
	  addend -= sec->output_section->vma;
	  break;

	case (int)R_PPC_SECTOFF_HA:
	  BFD_ASSERT (sec != (asection *)0);
	  addend -= sec->output_section->vma;
	  addend += ((relocation + addend) & 0x8000) << 1;
	  break;

	/* Negative relocations */
	case (int)R_PPC_EMB_NADDR32:
	case (int)R_PPC_EMB_NADDR16:
	case (int)R_PPC_EMB_NADDR16_LO:
	case (int)R_PPC_EMB_NADDR16_HI:
	  addend -= 2*relocation;
	  break;

	case (int)R_PPC_EMB_NADDR16_HA:
	  addend -= 2*relocation;
	  addend += ((relocation + addend) & 0x8000) << 1;
	  break;

	/* NOP relocation that prevents garbage collecting linkers from omitting a
	   reference.  */
	case (int)R_PPC_EMB_MRKREF:
	  continue;

	case (int)R_PPC_COPY:
	case (int)R_PPC_GLOB_DAT:
	case (int)R_PPC_JMP_SLOT:
	case (int)R_PPC_RELATIVE:
	case (int)R_PPC_PLT32:
	case (int)R_PPC_PLTREL32:
	case (int)R_PPC_PLT16_LO:
	case (int)R_PPC_PLT16_HI:
	case (int)R_PPC_PLT16_HA:
	case (int)R_PPC_EMB_RELSEC16:
	case (int)R_PPC_EMB_RELST_LO:
	case (int)R_PPC_EMB_RELST_HI:
	case (int)R_PPC_EMB_RELST_HA:
	case (int)R_PPC_EMB_BIT_FLD:
	  (*_bfd_error_handler) (_("%s: Relocation %s is not yet supported for symbol %s."),
				 bfd_get_filename (input_bfd),
				 ppc_elf_howto_table[ (int)r_type ]->name,
				 sym_name);

	  bfd_set_error (bfd_error_invalid_operation);
	  ret = false;
	  continue;

	case (int)R_PPC_GNU_VTINHERIT:
	case (int)R_PPC_GNU_VTENTRY:
	  /* These are no-ops in the end.  */
	  continue;
	}


#ifdef DEBUG
      fprintf (stderr, "\ttype = %s (%d), name = %s, symbol index = %ld, offset = %ld, addend = %ld\n",
	       howto->name,
	       (int)r_type,
	       sym_name,
	       r_symndx,
	       (long)offset,
	       (long)addend);
#endif

      r = _bfd_final_link_relocate (howto,
				    input_bfd,
				    input_section,
				    contents,
				    offset,
				    relocation,
				    addend);

      if (r == bfd_reloc_ok)
	;
      else if (r == bfd_reloc_overflow)
	{
	  const char *name;

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

		  continue;
		}

	      name = h->root.root.string;
	    }
	  else
	    {
	      name = bfd_elf_string_from_elf_section (input_bfd,
						      symtab_hdr->sh_link,
						      sym->st_name);
	      if (name == NULL)
		continue;
	      if (*name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  if (! (*info->callbacks->reloc_overflow)(info,
						   name,
						   howto->name,
						   (bfd_vma) 0,
						   input_bfd,
						   input_section,
						   offset))
	    return false;
	}
      else
	ret = false;
    }

#ifdef DEBUG
  fprintf (stderr, "\n");
#endif

  return ret;
}


#define TARGET_LITTLE_SYM	bfd_elf32_powerpcle_vec
#define TARGET_LITTLE_NAME	"elf32-powerpcle"
#define TARGET_BIG_SYM		bfd_elf32_powerpc_vec
#define TARGET_BIG_NAME		"elf32-powerpc"
#define ELF_ARCH		bfd_arch_powerpc
#define ELF_MACHINE_CODE	EM_PPC
#define ELF_MAXPAGESIZE		0x10000
#define elf_info_to_howto	ppc_elf_info_to_howto

#ifdef  EM_CYGNUS_POWERPC
#define ELF_MACHINE_ALT1	EM_CYGNUS_POWERPC
#endif

#ifdef EM_PPC_OLD
#define ELF_MACHINE_ALT2	EM_PPC_OLD
#endif

#define elf_backend_plt_not_loaded	1
#define elf_backend_got_symbol_offset	4
#define elf_backend_can_gc_sections	1
#define elf_backend_got_header_size	12
#define elf_backend_plt_header_size	PLT_INITIAL_ENTRY_SIZE

#define bfd_elf32_bfd_copy_private_bfd_data	ppc_elf_copy_private_bfd_data
#define bfd_elf32_bfd_merge_private_bfd_data	ppc_elf_merge_private_bfd_data
#define bfd_elf32_bfd_relax_section             ppc_elf_relax_section
#define bfd_elf32_bfd_reloc_type_lookup		ppc_elf_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags		ppc_elf_set_private_flags
#define bfd_elf32_bfd_final_link		_bfd_elf32_gc_common_final_link

#define elf_backend_gc_mark_hook		ppc_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook		ppc_elf_gc_sweep_hook
#define elf_backend_section_from_shdr		ppc_elf_section_from_shdr
#define elf_backend_relocate_section		ppc_elf_relocate_section
#define elf_backend_create_dynamic_sections	ppc_elf_create_dynamic_sections
#define elf_backend_check_relocs		ppc_elf_check_relocs
#define elf_backend_adjust_dynamic_symbol	ppc_elf_adjust_dynamic_symbol
#define elf_backend_add_symbol_hook		ppc_elf_add_symbol_hook
#define elf_backend_size_dynamic_sections	ppc_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_symbol	ppc_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections	ppc_elf_finish_dynamic_sections
#define elf_backend_fake_sections		ppc_elf_fake_sections
#define elf_backend_additional_program_headers	ppc_elf_additional_program_headers
#define elf_backend_modify_segment_map		ppc_elf_modify_segment_map

#include "elf32-target.h"
