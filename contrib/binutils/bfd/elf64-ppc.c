/* PowerPC64-specific support for 64-bit ELF.
   Copyright 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Written by Linus Nordberg, Swox AB <info@swox.com>,
   based on elf32-ppc.c by Ian Lance Taylor.

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

/* This file is based on the 64-bit PowerPC ELF ABI.  It is also based
   on the file elf32-ppc.c.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/ppc.h"
#include "elf64-ppc.h"

#define USE_RELA		/* we want RELA relocations, not REL.  */


static void ppc_howto_init
  PARAMS ((void));
static reloc_howto_type *ppc64_elf_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void ppc64_elf_info_to_howto
  PARAMS ((bfd *abfd, arelent *cache_ptr, Elf64_Internal_Rela *dst));
static bfd_reloc_status_type ppc64_elf_addr16_ha_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean ppc64_elf_set_private_flags
  PARAMS ((bfd *, flagword));
static boolean ppc64_elf_merge_private_bfd_data
  PARAMS ((bfd *, bfd *));
static boolean ppc64_elf_section_from_shdr
  PARAMS ((bfd *, Elf64_Internal_Shdr *, char *));
static struct bfd_hash_entry *link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct bfd_link_hash_table *ppc64_elf_link_hash_table_create
  PARAMS ((bfd *));
static boolean create_linkage_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean create_got_section
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean ppc64_elf_create_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static void ppc64_elf_copy_indirect_symbol
  PARAMS ((struct elf_link_hash_entry *, struct elf_link_hash_entry *));
static boolean ppc64_elf_check_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   const Elf_Internal_Rela *));
static asection * ppc64_elf_gc_mark_hook
  PARAMS ((bfd *abfd, struct bfd_link_info *info, Elf_Internal_Rela *rel,
	   struct elf_link_hash_entry *h, Elf_Internal_Sym *sym));
static boolean ppc64_elf_gc_sweep_hook
  PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *sec,
	   const Elf_Internal_Rela *relocs));
static boolean func_desc_adjust
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean ppc64_elf_func_desc_adjust
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean ppc64_elf_adjust_dynamic_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *));
static void ppc64_elf_hide_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *, boolean));
static boolean allocate_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean readonly_dynrelocs
  PARAMS ((struct elf_link_hash_entry *, PTR));
static enum elf_reloc_type_class ppc64_elf_reloc_type_class
  PARAMS ((const Elf_Internal_Rela *));
static boolean ppc64_elf_size_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_byte *build_plt_stub
  PARAMS ((bfd *, bfd_byte *, int, int));
static boolean build_one_stub
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean ppc64_elf_fake_sections
  PARAMS ((bfd *, Elf64_Internal_Shdr *, asection *));
static boolean ppc64_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *info, bfd *, asection *, bfd_byte *,
	   Elf_Internal_Rela *relocs, Elf_Internal_Sym *local_syms,
	   asection **));
static boolean ppc64_elf_finish_dynamic_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   Elf_Internal_Sym *));
static boolean ppc64_elf_finish_dynamic_sections
  PARAMS ((bfd *, struct bfd_link_info *));


/* Mask to set RA in memory instructions.  */
#define RA_REGISTER_MASK 0x001f0000

/* Value to shift register by to insert RA.  */
#define RA_REGISTER_SHIFT 16

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */
#define ELF_DYNAMIC_INTERPRETER "/usr/lib/ld.so.1"

/* The size in bytes of an entry in the procedure linkage table.  */
#define PLT_ENTRY_SIZE 24

/* The initial size of the plt reserved for the dynamic linker.  */
#define PLT_INITIAL_ENTRY_SIZE PLT_ENTRY_SIZE

/* TOC base pointers offset from start of TOC.  */
#define TOC_BASE_OFF (0x8000)

/* .plt call stub instructions.  */
#define ADDIS_R12_R2	0x3d820000	/* addis %r12,%r2,xxx@ha     */
#define STD_R2_40R1	0xf8410028	/* std	 %r2,40(%r1)	     */
#define LD_R11_0R12	0xe96c0000	/* ld	 %r11,xxx+0@l(%r12)  */
#define LD_R2_0R12	0xe84c0000	/* ld	 %r2,xxx+8@l(%r12)   */
#define MTCTR_R11	0x7d6903a6	/* mtctr %r11		     */
					/* ld	 %r11,xxx+16@l(%r12) */
#define BCTR		0x4e800420	/* bctr			     */

/* The normal stub is this size.  */
#define PLT_CALL_STUB_SIZE (7*4)

/* But sometimes the .plt entry crosses a 64k boundary, and we need
   to adjust the high word with this insn.  */
#define ADDIS_R12_R12_1	0x3d8c0001	/* addis %r12,%r12,1	*/

/* The .glink fixup call stub is the same as the .plt call stub, but
   the first instruction restores r2, and the std is omitted.  */
#define LD_R2_40R1	0xe8410028	/* ld    %r2,40(%r1)	*/

/* Always allow this much space.  */
#define GLINK_CALL_STUB_SIZE (8*4)

/* Pad with this.  */
#define NOP		0x60000000

/* .glink entries for the first 32k functions are two instructions. */
#define LI_R0_0		0x38000000	/* li    %r0,0		*/
#define B_DOT		0x48000000	/* b     .		*/

/* After that, we need two instructions to load the index, followed by
   a branch.  */
#define LIS_R0_0	0x3c000000	/* lis   %r0,0		*/
#define ORI_R0_R0_0	0x60000000	/* ori	 %r0,%r0,0	*/

/* Instructions to save and restore floating point regs.  */
#define STFD_FR0_0R1	0xd8010000	/* stfd  %fr0,0(%r1)	*/
#define LFD_FR0_0R1	0xc8010000	/* lfd   %fr0,0(%r1)	*/
#define BLR		0x4e800020	/* blr			*/

/* Since .opd is an array of descriptors and each entry will end up
   with identical R_PPC64_RELATIVE relocs, there is really no need to
   propagate .opd relocs;  The dynamic linker should be taught to
   relocate .opd without reloc entries.  FIXME: .opd should be trimmed
   of unused values.  */
#ifndef NO_OPD_RELOCS
#define NO_OPD_RELOCS 0
#endif

/* Relocation HOWTO's.  */
static reloc_howto_type *ppc64_elf_howto_table[(int) R_PPC_max];

static reloc_howto_type ppc64_elf_howto_raw[] = {
  /* This reloc does nothing.  */
  HOWTO (R_PPC64_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_NONE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_PPC64_ADDR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 26 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC64_ADDR24,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR24",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 16 bit relocation.  */
  HOWTO (R_PPC64_ADDR16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 16 bit relocation without overflow.  */
  HOWTO (R_PPC64_ADDR16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Bits 16-31 of an address.  */
  HOWTO (R_PPC64_ADDR16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Bits 16-31 of an address, plus 1 if the contents of the low 16
     bits, treated as a signed number, is negative.  */
  HOWTO (R_PPC64_ADDR16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_ADDR16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch; the lower two bits must be zero.
     FIXME: we don't check that, we just clear them.  */
  HOWTO (R_PPC64_ADDR14,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR14",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is expected to be taken.  The lower two
     bits must be zero.  */
  HOWTO (R_PPC64_ADDR14_BRTAKEN, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR14_BRTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* An absolute 16 bit branch, for which bit 10 should be set to
     indicate that the branch is not expected to be taken.  The lower
     two bits must be zero.  */
  HOWTO (R_PPC64_ADDR14_BRNTAKEN, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR14_BRNTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A relative 26 bit branch; the lower two bits must be zero.  */
  HOWTO (R_PPC64_REL24,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_REL24",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch; the lower two bits must be zero.  */
  HOWTO (R_PPC64_REL14,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_REL14",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch.  Bit 10 should be set to indicate that
     the branch is expected to be taken.  The lower two bits must be
     zero.  */
  HOWTO (R_PPC64_REL14_BRTAKEN,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_REL14_BRTAKEN", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* A relative 16 bit branch.  Bit 10 should be set to indicate that
     the branch is not expected to be taken.  The lower two bits must
     be zero.  */
  HOWTO (R_PPC64_REL14_BRNTAKEN, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_REL14_BRNTAKEN",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC64_ADDR16, but referring to the GOT table entry for the
     symbol.  */
  HOWTO (R_PPC64_GOT16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_GOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16_LO, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC64_GOT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_GOT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16_HI, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC64_GOT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_GOT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16_HA, but referring to the GOT table entry for
     the symbol.  */
  HOWTO (R_PPC64_GOT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_GOT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* This is used only by the dynamic linker.  The symbol should exist
     both in the object being run and in some shared library.  The
     dynamic linker copies the data addressed by the symbol from the
     shared library into the object, because the object being
     run has to have the data at some particular address.  */
  HOWTO (R_PPC64_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC64_COPY",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR64, but used when setting global offset table
     entries.  */
  HOWTO (R_PPC64_GLOB_DAT,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	 /* special_function */
	 "R_PPC64_GLOB_DAT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* Created by the link editor.  Marks a procedure linkage table
     entry for a symbol.  */
  HOWTO (R_PPC64_JMP_SLOT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_JMP_SLOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used only by the dynamic linker.  When the object is run, this
     doubleword64 is set to the load address of the object, plus the
     addend.  */
  HOWTO (R_PPC64_RELATIVE,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_RELATIVE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR32, but may be unaligned.  */
  HOWTO (R_PPC64_UADDR32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_UADDR32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16, but may be unaligned.  */
  HOWTO (R_PPC64_UADDR16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_UADDR16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit PC relative.  */
  HOWTO (R_PPC64_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 /* FIXME: Verify.  Was complain_overflow_bitfield. */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_REL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 32-bit relocation to the symbol's procedure linkage table.  */
  HOWTO (R_PPC64_PLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLT32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit PC relative relocation to the symbol's procedure linkage table.
     FIXME: R_PPC64_PLTREL32 not supported.  */
  HOWTO (R_PPC64_PLTREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTREL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC64_ADDR16_LO, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC64_PLT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16_HI, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC64_PLT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16_HA, but referring to the PLT table entry for
     the symbol.  */
  HOWTO (R_PPC64_PLT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_PLT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32-bit section relative relocation.  */
  /* FIXME: Verify R_PPC64_SECTOFF.  Seems strange with size=2 and
     dst_mask=0.  */
  HOWTO (R_PPC64_SECTOFF,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_SECTOFF",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16-bit lower half section relative relocation.  */
  HOWTO (R_PPC64_SECTOFF_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_SECTOFF_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16-bit upper half section relative relocation.  */
  HOWTO (R_PPC64_SECTOFF_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_SECTOFF_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16-bit upper half adjusted section relative relocation.  */
  HOWTO (R_PPC64_SECTOFF_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_SECTOFF_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_REL24 without touching the two least significant
     bits.  */
  /* FIXME: Verify R_PPC64_ADDR30.  */
  HOWTO (R_PPC64_ADDR30,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 30,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_PPC64_ADDR30",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffffffc,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* Relocs in the 64-bit PowerPC ELF ABI, not in the 32-bit ABI.  */

  /* A standard 64-bit relocation.  */
  HOWTO (R_PPC64_ADDR64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR64",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* The bits 32-47 of an address.  */
  HOWTO (R_PPC64_ADDR16_HIGHER,	/* type */
	 32,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16_HIGHER", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The bits 32-47 of an address, plus 1 if the contents of the low
     16 bits, treated as a signed number, is negative.  */
  HOWTO (R_PPC64_ADDR16_HIGHERA, /* type */
	 32,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_ADDR16_HIGHERA", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The bits 48-63 of an address.  */
  HOWTO (R_PPC64_ADDR16_HIGHEST,/* type */
	 48,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16_HIGHEST", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The bits 48-63 of an address, plus 1 if the contents of the low
     16 bits, treated as a signed number, is negative.  */
  HOWTO (R_PPC64_ADDR16_HIGHESTA,/* type */
	 48,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_ADDR16_HIGHESTA", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like ADDR64, but may be unaligned.  */
  HOWTO (R_PPC64_UADDR64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_UADDR64",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64-bit relative relocation.  */
  HOWTO (R_PPC64_REL64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_REL64",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 true),			/* pcrel_offset */

  /* 64-bit relocation to the symbol's procedure linkage table. */
  HOWTO (R_PPC64_PLT64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLT64",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64-bit PC relative relocation to the symbol's procedure linkage
     table.  */
  /* FIXME: R_PPC64_PLTREL64 not supported.  */
  HOWTO (R_PPC64_PLTREL64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTREL64",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit TOC-relative relocation.  */

  /* R_PPC64_TOC16	  47	   half16*	S + A - .TOC.  */
  HOWTO (R_PPC64_TOC16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_TOC16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit TOC-relative relocation without overflow.  */

  /* R_PPC64_TOC16_LO	  48	   half16	 #lo (S + A - .TOC.)  */
  HOWTO (R_PPC64_TOC16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_TOC16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit TOC-relative relocation, high 16 bits.  */

  /* R_PPC64_TOC16_HI	  49	   half16	 #hi (S + A - .TOC.)  */
  HOWTO (R_PPC64_TOC16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_TOC16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit TOC-relative relocation, high 16 bits, plus 1 if the
     contents of the low 16 bits, treated as a signed number, is
     negative.  */

  /* R_PPC64_TOC16_HA	  50	   half16	 #ha (S + A - .TOC.)  */
  HOWTO (R_PPC64_TOC16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_TOC16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64-bit relocation; insert value of TOC base (.TOC.).  */

  /* R_PPC64_TOC		  51	   doubleword64	 .TOC.  */
  HOWTO (R_PPC64_TOC,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0=byte, 1=short, 2=long, 4=64 bits) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_TOC",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffffffffffff,	/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_GOT16, but also informs the link editor that the
     value to relocate may (!) refer to a PLT entry which the link
     editor (a) may replace with the symbol value.  If the link editor
     is unable to fully resolve the symbol, it may (b) create a PLT
     entry and store the address to the new PLT entry in the GOT.
     This permits lazy resolution of function symbols at run time.
     The link editor may also skip all of this and just (c) emit a
     R_PPC64_GLOB_DAT to tie the symbol to the GOT entry.  */
  /* FIXME: R_PPC64_PLTGOT16 not implemented.  */
    HOWTO (R_PPC64_PLTGOT16,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTGOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_PLTGOT16, but without overflow.  */
  /* FIXME: R_PPC64_PLTGOT16_LO not implemented.  */
  HOWTO (R_PPC64_PLTGOT16_LO,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTGOT16_LO",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_PLT_GOT16, but using bits 16-31 of the address.  */
  /* FIXME: R_PPC64_PLTGOT16_HI not implemented.  */
  HOWTO (R_PPC64_PLTGOT16_HI,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTGOT16_HI",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_PLT_GOT16, but using bits 16-31 of the address, plus
     1 if the contents of the low 16 bits, treated as a signed number,
     is negative.  */
  /* FIXME: R_PPC64_PLTGOT16_HA not implemented.  */
  HOWTO (R_PPC64_PLTGOT16_HA,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 ppc64_elf_addr16_ha_reloc, /* special_function */
	 "R_PPC64_PLTGOT16_HA",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16, but for instructions with a DS field.  */
  HOWTO (R_PPC64_ADDR16_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_ADDR16_LO, but for instructions with a DS field.  */
  HOWTO (R_PPC64_ADDR16_LO_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_ADDR16_LO_DS",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_GOT16, but for instructions with a DS field.  */
  HOWTO (R_PPC64_GOT16_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_GOT16_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_GOT16_LO, but for instructions with a DS field.  */
  HOWTO (R_PPC64_GOT16_LO_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_GOT16_LO_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_PLT16_LO, but for instructions with a DS field.  */
  HOWTO (R_PPC64_PLT16_LO_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLT16_LO_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_SECTOFF, but for instructions with a DS field.  */
  /* FIXME: Verify R_PPC64_SECTOFF.  Seems strange with size=2 and
     dst_mask=0.  */
  HOWTO (R_PPC64_SECTOFF_DS,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_SECTOFF_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 true),			/* pcrel_offset */

  /* Like R_PPC64_SECTOFF_LO, but for instructions with a DS field.  */
  HOWTO (R_PPC64_SECTOFF_LO_DS, /* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_SECTOFF_LO_DS",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_TOC16, but for instructions with a DS field.  */
  HOWTO (R_PPC64_TOC16_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_TOC16_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_TOC16_LO, but for instructions with a DS field.  */
  HOWTO (R_PPC64_TOC16_LO_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_TOC16_LO_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_PLTGOT16, but for instructions with a DS field.  */
  /* FIXME: R_PPC64_PLTGOT16_DS not implemented.  */
    HOWTO (R_PPC64_PLTGOT16_DS,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTGOT16_DS",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Like R_PPC64_PLTGOT16_LO, but for instructions with a DS field.  */
  /* FIXME: R_PPC64_PLTGOT16_LO not implemented.  */
  HOWTO (R_PPC64_PLTGOT16_LO_DS,/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_PPC64_PLTGOT16_LO_DS",/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfffc,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_PPC64_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_PPC64_GNU_VTINHERIT", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_PPC64_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_PPC64_GNU_VTENTRY",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */
};


/* Initialize the ppc64_elf_howto_table, so that linear accesses can
   be done.  */

static void
ppc_howto_init ()
{
  unsigned int i, type;

  for (i = 0;
       i < sizeof (ppc64_elf_howto_raw) / sizeof (ppc64_elf_howto_raw[0]);
       i++)
    {
      type = ppc64_elf_howto_raw[i].type;
      BFD_ASSERT (type < (sizeof (ppc64_elf_howto_table)
			  / sizeof (ppc64_elf_howto_table[0])));
      ppc64_elf_howto_table[type] = &ppc64_elf_howto_raw[i];
    }
}

static reloc_howto_type *
ppc64_elf_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  enum elf_ppc_reloc_type ppc_reloc = R_PPC_NONE;

  if (!ppc64_elf_howto_table[R_PPC64_ADDR32])
    /* Initialize howto table if needed.  */
    ppc_howto_init ();

  switch ((int) code)
    {
    default:
      return (reloc_howto_type *) NULL;

    case BFD_RELOC_NONE:		 ppc_reloc = R_PPC64_NONE;
      break;
    case BFD_RELOC_32:			 ppc_reloc = R_PPC64_ADDR32;
      break;
    case BFD_RELOC_PPC_BA26:		 ppc_reloc = R_PPC64_ADDR24;
      break;
    case BFD_RELOC_16:			 ppc_reloc = R_PPC64_ADDR16;
      break;
    case BFD_RELOC_LO16:		 ppc_reloc = R_PPC64_ADDR16_LO;
      break;
    case BFD_RELOC_HI16:		 ppc_reloc = R_PPC64_ADDR16_HI;
      break;
    case BFD_RELOC_HI16_S:		 ppc_reloc = R_PPC64_ADDR16_HA;
      break;
    case BFD_RELOC_PPC_BA16:		 ppc_reloc = R_PPC64_ADDR14;
      break;
    case BFD_RELOC_PPC_BA16_BRTAKEN:	 ppc_reloc = R_PPC64_ADDR14_BRTAKEN;
      break;
    case BFD_RELOC_PPC_BA16_BRNTAKEN:	 ppc_reloc = R_PPC64_ADDR14_BRNTAKEN;
      break;
    case BFD_RELOC_PPC_B26:		 ppc_reloc = R_PPC64_REL24;
      break;
    case BFD_RELOC_PPC_B16:		 ppc_reloc = R_PPC64_REL14;
      break;
    case BFD_RELOC_PPC_B16_BRTAKEN:	 ppc_reloc = R_PPC64_REL14_BRTAKEN;
      break;
    case BFD_RELOC_PPC_B16_BRNTAKEN:	 ppc_reloc = R_PPC64_REL14_BRNTAKEN;
      break;
    case BFD_RELOC_16_GOTOFF:		 ppc_reloc = R_PPC64_GOT16;
      break;
    case BFD_RELOC_LO16_GOTOFF:		 ppc_reloc = R_PPC64_GOT16_LO;
      break;
    case BFD_RELOC_HI16_GOTOFF:		 ppc_reloc = R_PPC64_GOT16_HI;
      break;
    case BFD_RELOC_HI16_S_GOTOFF:	 ppc_reloc = R_PPC64_GOT16_HA;
      break;
    case BFD_RELOC_PPC_COPY:		 ppc_reloc = R_PPC64_COPY;
      break;
    case BFD_RELOC_PPC_GLOB_DAT:	 ppc_reloc = R_PPC64_GLOB_DAT;
      break;
    case BFD_RELOC_32_PCREL:		 ppc_reloc = R_PPC64_REL32;
      break;
    case BFD_RELOC_32_PLTOFF:		 ppc_reloc = R_PPC64_PLT32;
      break;
    case BFD_RELOC_32_PLT_PCREL:	 ppc_reloc = R_PPC64_PLTREL32;
      break;
    case BFD_RELOC_LO16_PLTOFF:		 ppc_reloc = R_PPC64_PLT16_LO;
      break;
    case BFD_RELOC_HI16_PLTOFF:		 ppc_reloc = R_PPC64_PLT16_HI;
      break;
    case BFD_RELOC_HI16_S_PLTOFF:	 ppc_reloc = R_PPC64_PLT16_HA;
      break;
    case BFD_RELOC_32_BASEREL:		 ppc_reloc = R_PPC64_SECTOFF;
      break;
    case BFD_RELOC_LO16_BASEREL:	 ppc_reloc = R_PPC64_SECTOFF_LO;
      break;
    case BFD_RELOC_HI16_BASEREL:	 ppc_reloc = R_PPC64_SECTOFF_HI;
      break;
    case BFD_RELOC_HI16_S_BASEREL:	 ppc_reloc = R_PPC64_SECTOFF_HA;
      break;
    case BFD_RELOC_CTOR:		 ppc_reloc = R_PPC64_ADDR64;
      break;
    case BFD_RELOC_64:			 ppc_reloc = R_PPC64_ADDR64;
      break;
    case BFD_RELOC_PPC64_HIGHER:	 ppc_reloc = R_PPC64_ADDR16_HIGHER;
      break;
    case BFD_RELOC_PPC64_HIGHER_S:	 ppc_reloc = R_PPC64_ADDR16_HIGHERA;
      break;
    case BFD_RELOC_PPC64_HIGHEST:	 ppc_reloc = R_PPC64_ADDR16_HIGHEST;
      break;
    case BFD_RELOC_PPC64_HIGHEST_S:	 ppc_reloc = R_PPC64_ADDR16_HIGHESTA;
      break;
    case BFD_RELOC_64_PCREL:		 ppc_reloc = R_PPC64_REL64;
      break;
    case BFD_RELOC_64_PLTOFF:		 ppc_reloc = R_PPC64_PLT64;
      break;
    case BFD_RELOC_64_PLT_PCREL:	 ppc_reloc = R_PPC64_PLTREL64;
      break;
    case BFD_RELOC_PPC_TOC16:		 ppc_reloc = R_PPC64_TOC16;
      break;
    case BFD_RELOC_PPC64_TOC16_LO:	 ppc_reloc = R_PPC64_TOC16_LO;
      break;
    case BFD_RELOC_PPC64_TOC16_HI:	 ppc_reloc = R_PPC64_TOC16_HI;
      break;
    case BFD_RELOC_PPC64_TOC16_HA:	 ppc_reloc = R_PPC64_TOC16_HA;
      break;
    case BFD_RELOC_PPC64_TOC:		 ppc_reloc = R_PPC64_TOC;
      break;
    case BFD_RELOC_PPC64_PLTGOT16:	 ppc_reloc = R_PPC64_PLTGOT16;
      break;
    case BFD_RELOC_PPC64_PLTGOT16_LO:	 ppc_reloc = R_PPC64_PLTGOT16_LO;
      break;
    case BFD_RELOC_PPC64_PLTGOT16_HI:	 ppc_reloc = R_PPC64_PLTGOT16_HI;
      break;
    case BFD_RELOC_PPC64_PLTGOT16_HA:	 ppc_reloc = R_PPC64_PLTGOT16_HA;
      break;
    case BFD_RELOC_PPC64_ADDR16_DS:      ppc_reloc = R_PPC64_ADDR16_DS;
      break;
    case BFD_RELOC_PPC64_ADDR16_LO_DS:   ppc_reloc = R_PPC64_ADDR16_LO_DS;
      break;
    case BFD_RELOC_PPC64_GOT16_DS:       ppc_reloc = R_PPC64_GOT16_DS;
      break;
    case BFD_RELOC_PPC64_GOT16_LO_DS:    ppc_reloc = R_PPC64_GOT16_LO_DS;
      break;
    case BFD_RELOC_PPC64_PLT16_LO_DS:    ppc_reloc = R_PPC64_PLT16_LO_DS;
      break;
    case BFD_RELOC_PPC64_SECTOFF_DS:     ppc_reloc = R_PPC64_SECTOFF_DS;
      break;
    case BFD_RELOC_PPC64_SECTOFF_LO_DS:  ppc_reloc = R_PPC64_SECTOFF_LO_DS;
      break;
    case BFD_RELOC_PPC64_TOC16_DS:       ppc_reloc = R_PPC64_TOC16_DS;
      break;
    case BFD_RELOC_PPC64_TOC16_LO_DS:    ppc_reloc = R_PPC64_TOC16_LO_DS;
      break;
    case BFD_RELOC_PPC64_PLTGOT16_DS:    ppc_reloc = R_PPC64_PLTGOT16_DS;
      break;
    case BFD_RELOC_PPC64_PLTGOT16_LO_DS: ppc_reloc = R_PPC64_PLTGOT16_LO_DS;
      break;
    case BFD_RELOC_VTABLE_INHERIT:	 ppc_reloc = R_PPC64_GNU_VTINHERIT;
      break;
    case BFD_RELOC_VTABLE_ENTRY:	 ppc_reloc = R_PPC64_GNU_VTENTRY;
      break;
    }

  return ppc64_elf_howto_table[(int) ppc_reloc];
};

/* Set the howto pointer for a PowerPC ELF reloc.  */

static void
ppc64_elf_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf64_Internal_Rela *dst;
{
  unsigned int type;

  if (!ppc64_elf_howto_table[R_PPC64_ADDR32])
    /* Initialize howto table if needed.  */
    ppc_howto_init ();

  type = ELF64_R_TYPE (dst->r_info);
  BFD_ASSERT (type < (sizeof (ppc64_elf_howto_table)
		      / sizeof (ppc64_elf_howto_table[0])));
  cache_ptr->howto = ppc64_elf_howto_table[type];
}

/* Handle the R_PPC_ADDR16_HA and similar relocs.  */

static bfd_reloc_status_type
ppc64_elf_addr16_ha_reloc (abfd, reloc_entry, symbol, data, input_section,
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

/* Function to set whether a module needs the -mrelocatable bit set.  */

static boolean
ppc64_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */
static boolean
ppc64_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags;
  flagword new_flags;
  boolean error;

  /* Check if we have the same endianess.  */
  if (ibfd->xvec->byteorder != obfd->xvec->byteorder
      && obfd->xvec->byteorder != BFD_ENDIAN_UNKNOWN)
    {
      const char *msg;

      if (bfd_big_endian (ibfd))
	msg = _("%s: compiled for a big endian system and target is little endian");
      else
	msg = _("%s: compiled for a little endian system and target is big endian");

      (*_bfd_error_handler) (msg, bfd_archive_filename (ibfd));

      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  old_flags = elf_elfheader (obfd)->e_flags;
  if (!elf_flags_init (obfd))
    {
      /* First call, no flags set.  */
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;
    }

  else if (new_flags == old_flags)
    /* Compatible flags are ok.  */
    ;

  else
    {
      /* Incompatible flags.  Warn about -mrelocatable mismatch.
	 Allow -mrelocatable-lib to be linked with either.  */
      error = false;
      if ((new_flags & EF_PPC_RELOCATABLE) != 0
	  && (old_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0)
	{
	  error = true;
	  (*_bfd_error_handler)
	    (_("%s: compiled with -mrelocatable and linked with modules compiled normally"),
	     bfd_archive_filename (ibfd));
	}
      else if ((new_flags & (EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB)) == 0
	       && (old_flags & EF_PPC_RELOCATABLE) != 0)
	{
	  error = true;
	  (*_bfd_error_handler)
	    (_("%s: compiled normally and linked with modules compiled with -mrelocatable"),
	     bfd_archive_filename (ibfd));
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

      /* Do not warn about eabi vs. V.4 mismatch, just or in the bit
	 if any module uses it.  */
      elf_elfheader (obfd)->e_flags |= (new_flags & EF_PPC_EMB);

      new_flags &= ~(EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB | EF_PPC_EMB);
      old_flags &= ~(EF_PPC_RELOCATABLE | EF_PPC_RELOCATABLE_LIB | EF_PPC_EMB);

      /* Warn about any other mismatches.  */
      if (new_flags != old_flags)
	{
	  error = true;
	  (*_bfd_error_handler)
	    (_("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)"),
	     bfd_archive_filename (ibfd), (long) new_flags, (long) old_flags);
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
ppc64_elf_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf64_Internal_Shdr *hdr;
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

/* The following functions are specific to the ELF linker, while
   functions above are used generally.  Those named ppc64_elf_* are
   called by the main ELF linker code.  They appear in this file more
   or less in the order in which they are called.  eg.
   ppc64_elf_check_relocs is called early in the link process,
   ppc64_elf_finish_dynamic_sections is one of the last functions
   called.

   PowerPC64-ELF uses a similar scheme to PowerPC64-XCOFF in that
   functions have both a function code symbol and a function descriptor
   symbol.  A call to foo in a relocatable object file looks like:

   .		.text
   .	x:
   .		bl	.foo
   .		nop

   The function definition in another object file might be:

   .		.section .opd
   .	foo:	.quad	.foo
   .		.quad	.TOC.@tocbase
   .		.quad	0
   .
   .		.text
   .	.foo:	blr

   When the linker resolves the call during a static link, the branch
   unsurprisingly just goes to .foo and the .opd information is unused.
   If the function definition is in a shared library, things are a little
   different:  The call goes via a plt call stub, the opd information gets
   copied to the plt, and the linker patches the nop.

   .	x:
   .		bl	.foo_stub
   .		ld	2,40(1)
   .
   .
   .	.foo_stub:
   .		addis	12,2,Lfoo@toc@ha	# in practice, the call stub
   .		addi	12,12,Lfoo@toc@l	# is slightly optimised, but
   .		std	2,40(1)			# this is the general idea
   .		ld	11,0(12)
   .		ld	2,8(12)
   .		mtctr	11
   .		ld	11,16(12)
   .		bctr
   .
   .		.section .plt
   .	Lfoo:	reloc (R_PPC64_JMP_SLOT, foo)

   The "reloc ()" notation is supposed to indicate that the linker emits
   an R_PPC64_JMP_SLOT reloc against foo.  The dynamic linker does the opd
   copying.

   What are the difficulties here?  Well, firstly, the relocations
   examined by the linker in check_relocs are against the function code
   sym .foo, while the dynamic relocation in the plt is emitted against
   the function descriptor symbol, foo.  Somewhere along the line, we need
   to carefully copy dynamic link information from one symbol to the other.
   Secondly, the generic part of the elf linker will make .foo a dynamic
   symbol as is normal for most other backends.  We need foo dynamic
   instead, at least for an application final link.  However, when
   creating a shared library containing foo, we need to have both symbols
   dynamic so that references to .foo are satisfied during the early
   stages of linking.  Otherwise the linker might decide to pull in a
   definition from some other object, eg. a static library.  */

/* The linker needs to keep track of the number of relocs that it
   decides to copy as dynamic relocs in check_relocs for each symbol.
   This is so that it can later discard them if they are found to be
   unnecessary.  We store the information in a field extending the
   regular ELF linker hash table.  */

struct ppc_dyn_relocs
{
  struct ppc_dyn_relocs *next;

  /* The input section of the reloc.  */
  asection *sec;

  /* Total number of relocs copied for the input section.  */
  bfd_size_type count;

  /* Number of pc-relative relocs copied for the input section.  */
  bfd_size_type pc_count;
};

/* Of those relocs that might be copied as dynamic relocs, this macro
   selects between relative and absolute types.  */

#define IS_ABSOLUTE_RELOC(RTYPE)		\
  ((RTYPE) != R_PPC64_REL14			\
   && (RTYPE) != R_PPC64_REL14_BRNTAKEN		\
   && (RTYPE) != R_PPC64_REL14_BRTAKEN		\
   && (RTYPE) != R_PPC64_REL24			\
   && (RTYPE) != R_PPC64_REL32			\
   && (RTYPE) != R_PPC64_REL64)

/* ppc64 ELF linker hash entry.  */

struct ppc_link_hash_entry
{
  struct elf_link_hash_entry elf;

  /* Track dynamic relocs copied for this symbol.  */
  struct ppc_dyn_relocs *dyn_relocs;

  /* Flag function code and descriptor symbols.  */
  unsigned int is_func:1;
  unsigned int is_func_descriptor:1;
};

/* ppc64 ELF linker hash table.  */

struct ppc_link_hash_table
{
  struct elf_link_hash_table elf;

  /* Short-cuts to get to dynamic linker sections.  */
  asection *sgot;
  asection *srelgot;
  asection *splt;
  asection *srelplt;
  asection *sdynbss;
  asection *srelbss;
  asection *sstub;
  asection *sglink;
  asection *sfpr;

  /* Set on error.  */
  int plt_overflow;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;
};

/* Get the ppc64 ELF linker hash table from a link_info structure.  */

#define ppc_hash_table(p) \
  ((struct ppc_link_hash_table *) ((p)->hash))

/* Create an entry in a ppc64 ELF linker hash table.  */

static struct bfd_hash_entry *
link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (entry == NULL)
    {
      entry = bfd_hash_allocate (table, sizeof (struct ppc_link_hash_entry));
      if (entry == NULL)
	return entry;
    }

  /* Call the allocation method of the superclass.  */
  entry = _bfd_elf_link_hash_newfunc (entry, table, string);
  if (entry != NULL)
    {
      struct ppc_link_hash_entry *eh = (struct ppc_link_hash_entry *) entry;

      eh->dyn_relocs = NULL;
      eh->is_func = 0;
      eh->is_func_descriptor = 0;
    }

  return entry;
}

/* Create a ppc64 ELF linker hash table.  */

static struct bfd_link_hash_table *
ppc64_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct ppc_link_hash_table *htab;
  bfd_size_type amt = sizeof (struct ppc_link_hash_table);

  htab = (struct ppc_link_hash_table *) bfd_alloc (abfd, amt);
  if (htab == NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&htab->elf, abfd, link_hash_newfunc))
    {
      bfd_release (abfd, htab);
      return NULL;
    }

  htab->sgot = NULL;
  htab->srelgot = NULL;
  htab->splt = NULL;
  htab->srelplt = NULL;
  htab->sdynbss = NULL;
  htab->srelbss = NULL;
  htab->sstub = NULL;
  htab->sglink = NULL;
  htab->sfpr = NULL;
  htab->plt_overflow = 0;
  htab->sym_sec.abfd = NULL;

  return &htab->elf.root;
}

/* Create sections for linker generated code.  */

static boolean
create_linkage_sections (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab;
  flagword flags;

  htab = ppc_hash_table (info);

  /* Create .sfpr for code to save and restore fp regs.  */
  flags = (SEC_ALLOC | SEC_LOAD | SEC_CODE | SEC_READONLY
	   | SEC_HAS_CONTENTS | SEC_IN_MEMORY | SEC_LINKER_CREATED);
  htab->sfpr = bfd_make_section (dynobj, ".sfpr");
  if (htab->sfpr == NULL
      || ! bfd_set_section_flags (dynobj, htab->sfpr, flags)
      || ! bfd_set_section_alignment (dynobj, htab->sfpr, 2))
    return false;

  /* Create .stub and .glink for global linkage functions.  */
  htab->sstub = bfd_make_section (dynobj, ".stub");
  if (htab->sstub == NULL
      || ! bfd_set_section_flags (dynobj, htab->sstub, flags)
      || ! bfd_set_section_alignment (dynobj, htab->sstub, 2))
    return false;
  htab->sglink = bfd_make_section (dynobj, ".glink");
  if (htab->sglink == NULL
      || ! bfd_set_section_flags (dynobj, htab->sglink, flags)
      || ! bfd_set_section_alignment (dynobj, htab->sglink, 2))
    return false;

  return true;
}

/* Create .got and .rela.got sections in DYNOBJ, and set up
   shortcuts to them in our hash table.  */

static boolean
create_got_section (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab;

  if (! _bfd_elf_create_got_section (dynobj, info))
    return false;

  htab = ppc_hash_table (info);
  htab->sgot = bfd_get_section_by_name (dynobj, ".got");
  if (!htab->sgot)
    abort ();

  htab->srelgot = bfd_make_section (dynobj, ".rela.got");
  if (!htab->srelgot
      || ! bfd_set_section_flags (dynobj, htab->srelgot,
				  (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
				   | SEC_IN_MEMORY | SEC_LINKER_CREATED
				   | SEC_READONLY))
      || ! bfd_set_section_alignment (dynobj, htab->srelgot, 3))
    return false;
  return true;
}

/* Create the dynamic sections, and set up shortcuts.  */

static boolean
ppc64_elf_create_dynamic_sections (dynobj, info)
     bfd *dynobj;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab;

  htab = ppc_hash_table (info);
  if (!htab->sgot && !create_got_section (dynobj, info))
    return false;

  if (!_bfd_elf_create_dynamic_sections (dynobj, info))
    return false;

  htab->splt = bfd_get_section_by_name (dynobj, ".plt");
  htab->srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
  htab->sdynbss = bfd_get_section_by_name (dynobj, ".dynbss");
  if (!info->shared)
    htab->srelbss = bfd_get_section_by_name (dynobj, ".rela.bss");

  if (!htab->splt || !htab->srelplt || !htab->sdynbss
      || (!info->shared && !htab->srelbss))
    abort ();

  return true;
}

/* Copy the extra info we tack onto an elf_link_hash_entry.  */

static void
ppc64_elf_copy_indirect_symbol (dir, ind)
     struct elf_link_hash_entry *dir, *ind;
{
  struct ppc_link_hash_entry *edir, *eind;

  edir = (struct ppc_link_hash_entry *) dir;
  eind = (struct ppc_link_hash_entry *) ind;

  if (eind->dyn_relocs != NULL)
    {
      if (edir->dyn_relocs != NULL)
	{
	  struct ppc_dyn_relocs **pp;
	  struct ppc_dyn_relocs *p;

	  if (ind->root.type == bfd_link_hash_indirect)
	    abort ();

	  /* Add reloc counts against the weak sym to the strong sym
	     list.  Merge any entries against the same section.  */
	  for (pp = &eind->dyn_relocs; (p = *pp) != NULL; )
	    {
	      struct ppc_dyn_relocs *q;

	      for (q = edir->dyn_relocs; q != NULL; q = q->next)
		if (q->sec == p->sec)
		  {
		    q->pc_count += p->pc_count;
		    q->count += p->count;
		    *pp = p->next;
		    break;
		  }
	      if (q == NULL)
		pp = &p->next;
	    }
	  *pp = edir->dyn_relocs;
	}

      edir->dyn_relocs = eind->dyn_relocs;
      eind->dyn_relocs = NULL;
    }

  edir->is_func |= eind->is_func;
  edir->is_func_descriptor |= eind->is_func_descriptor;

  _bfd_elf_link_hash_copy_indirect (dir, ind);
}

/* Look through the relocs for a section during the first phase, and
   calculate needed space in the global offset table, procedure
   linkage table, and dynamic reloc sections.  */

static boolean
ppc64_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  struct ppc_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sreloc;
  boolean is_opd;

  if (info->relocateable)
    return true;

  htab = ppc_hash_table (info);
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = (sym_hashes
		    + symtab_hdr->sh_size / sizeof (Elf64_External_Sym));
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  sreloc = NULL;
  is_opd = strcmp (bfd_get_section_name (abfd, sec), ".opd") == 0;

  if (htab->elf.dynobj == NULL)
    htab->elf.dynobj = abfd;
  if (htab->sfpr == NULL
      && !create_linkage_sections (htab->elf.dynobj, info))
    return false;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      enum elf_ppc_reloc_type r_type;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	h = sym_hashes[r_symndx - symtab_hdr->sh_info];

      r_type = (enum elf_ppc_reloc_type) ELF64_R_TYPE (rel->r_info);
      switch (r_type)
	{
	  /* GOT16 relocations */
	case R_PPC64_GOT16:
	case R_PPC64_GOT16_DS:
	case R_PPC64_GOT16_HA:
	case R_PPC64_GOT16_HI:
	case R_PPC64_GOT16_LO:
	case R_PPC64_GOT16_LO_DS:

	  /* This symbol requires a global offset table entry.  */
	  if (htab->sgot == NULL
	      && !create_got_section (htab->elf.dynobj, info))
	    return false;

	  if (h != NULL)
	    {
	      h->got.refcount += 1;
	    }
	  else
	    {
	      bfd_signed_vma *local_got_refcounts;

	      /* This is a global offset table entry for a local symbol.  */
	      local_got_refcounts = elf_local_got_refcounts (abfd);
	      if (local_got_refcounts == NULL)
		{
		  bfd_size_type size;

		  size = symtab_hdr->sh_info;
		  size *= sizeof (bfd_signed_vma);
		  local_got_refcounts = ((bfd_signed_vma *)
					 bfd_zalloc (abfd, size));
		  if (local_got_refcounts == NULL)
		    return false;
		  elf_local_got_refcounts (abfd) = local_got_refcounts;
		}
	      local_got_refcounts[r_symndx] += 1;
	    }
	  break;

	case R_PPC64_PLT16_HA:
	case R_PPC64_PLT16_HI:
	case R_PPC64_PLT16_LO:
	case R_PPC64_PLT32:
	case R_PPC64_PLT64:
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

	  h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	  h->plt.refcount += 1;
	  ((struct ppc_link_hash_entry *) h)->is_func = 1;
	  break;

	  /* The following relocations don't need to propagate the
	     relocation if linking a shared object since they are
	     section relative.  */
	case R_PPC64_SECTOFF:
	case R_PPC64_SECTOFF_LO:
	case R_PPC64_SECTOFF_HI:
	case R_PPC64_SECTOFF_HA:
	case R_PPC64_SECTOFF_DS:
	case R_PPC64_SECTOFF_LO_DS:
	case R_PPC64_TOC16:
	case R_PPC64_TOC16_LO:
	case R_PPC64_TOC16_HI:
	case R_PPC64_TOC16_HA:
	case R_PPC64_TOC16_DS:
	case R_PPC64_TOC16_LO_DS:
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_PPC64_GNU_VTINHERIT:
	  if (!_bfd_elf64_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_PPC64_GNU_VTENTRY:
	  if (!_bfd_elf64_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return false;
	  break;

	case R_PPC64_REL24:
	  if (h != NULL
	      && h->root.root.string[0] == '.'
	      && h->root.root.string[1] != 0)
	    {
	      /* We may need a .plt entry if the function this reloc
		 refers to is in a shared lib.  */
	      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	      h->plt.refcount += 1;
	      ((struct ppc_link_hash_entry *) h)->is_func = 1;
	    }
	  break;

	case R_PPC64_ADDR64:
	  if (is_opd
	      && h != NULL
	      && h->root.root.string[0] == '.'
	      && h->root.root.string[1] != 0)
	    {
	      struct elf_link_hash_entry *fdh;

	      fdh = elf_link_hash_lookup (&htab->elf, h->root.root.string + 1,
					  false, false, false);
	      if (fdh != NULL)
		{
		  /* Ensure the function descriptor symbol string is
		     part of the code symbol string.  We aren't
		     changing the name here, just allowing some tricks
		     in ppc64_elf_hide_symbol.  */
		  fdh->root.root.string = h->root.root.string + 1;
		  ((struct ppc_link_hash_entry *) fdh)->is_func_descriptor = 1;
		  ((struct ppc_link_hash_entry *) h)->is_func = 1;
		}
	    }
	  /* Fall through.  */

	case R_PPC64_REL64:
	case R_PPC64_REL32:
	case R_PPC64_REL14:
	case R_PPC64_REL14_BRTAKEN:
	case R_PPC64_REL14_BRNTAKEN:
	case R_PPC64_ADDR14:
	case R_PPC64_ADDR14_BRNTAKEN:
	case R_PPC64_ADDR14_BRTAKEN:
	case R_PPC64_ADDR16:
	case R_PPC64_ADDR16_DS:
	case R_PPC64_ADDR16_HA:
	case R_PPC64_ADDR16_HI:
	case R_PPC64_ADDR16_HIGHER:
	case R_PPC64_ADDR16_HIGHERA:
	case R_PPC64_ADDR16_HIGHEST:
	case R_PPC64_ADDR16_HIGHESTA:
	case R_PPC64_ADDR16_LO:
	case R_PPC64_ADDR16_LO_DS:
	case R_PPC64_ADDR24:
	case R_PPC64_ADDR30:
	case R_PPC64_ADDR32:
	case R_PPC64_UADDR16:
	case R_PPC64_UADDR32:
	case R_PPC64_UADDR64:
	case R_PPC64_TOC:
	  /* Don't propagate .opd relocs.  */
	  if (NO_OPD_RELOCS && is_opd)
	    break;

	  /* If we are creating a shared library, and this is a reloc
	     against a global symbol, or a non PC relative reloc
	     against a local symbol, then we need to copy the reloc
	     into the shared library.  However, if we are linking with
	     -Bsymbolic, we do not need to copy a reloc against a
	     global symbol which is defined in an object we are
	     including in the link (i.e., DEF_REGULAR is set).  At
	     this point we have not seen all the input files, so it is
	     possible that DEF_REGULAR is not set now but will be set
	     later (it is never cleared).  In case of a weak definition,
	     DEF_REGULAR may be cleared later by a strong definition in
	     a shared library.  We account for that possibility below by
	     storing information in the relocs_copied field of the hash
	     table entry.  A similar situation occurs when creating
	     shared libraries and symbol visibility changes render the
	     symbol local.

	     If on the other hand, we are creating an executable, we
	     may need to keep relocations for symbols satisfied by a
	     dynamic library if we manage to avoid copy relocs for the
	     symbol.  */
	  if ((info->shared
	       && (sec->flags & SEC_ALLOC) != 0
	       && (IS_ABSOLUTE_RELOC (r_type)
		   || (h != NULL
		       && (! info->symbolic
			   || h->root.type == bfd_link_hash_defweak
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (!info->shared
		  && (sec->flags & SEC_ALLOC) != 0
		  && h != NULL
		  && (h->root.type == bfd_link_hash_defweak
		      || (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	    {
	      struct ppc_dyn_relocs *p;
	      struct ppc_dyn_relocs **head;

	      /* We must copy these reloc types into the output file.
		 Create a reloc section in dynobj and make room for
		 this reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;
		  bfd *dynobj;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return false;

		  if (strncmp (name, ".rela", 5) != 0
		      || strcmp (bfd_get_section_name (abfd, sec),
				 name + 5) != 0)
		    {
		      (*_bfd_error_handler)
			(_("%s: bad relocation section name `%s\'"),
			 bfd_archive_filename (abfd), name);
		      bfd_set_error (bfd_error_bad_value);
		    }

		  dynobj = htab->elf.dynobj;
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
		  elf_section_data (sec)->sreloc = sreloc;
		}

	      /* If this is a global symbol, we count the number of
		 relocations we need for this symbol.  */
	      if (h != NULL)
		{
		  head = &((struct ppc_link_hash_entry *) h)->dyn_relocs;
		}
	      else
		{
		  /* Track dynamic relocs needed for local syms too.
		     We really need local syms available to do this
		     easily.  Oh well.  */

		  asection *s;
		  s = bfd_section_from_r_symndx (abfd, &htab->sym_sec,
						 sec, r_symndx);
		  if (s == NULL)
		    return false;

		  head = ((struct ppc_dyn_relocs **)
			  &elf_section_data (s)->local_dynrel);
		}

	      p = *head;
	      if (p == NULL || p->sec != sec)
		{
		  p = ((struct ppc_dyn_relocs *)
		       bfd_alloc (htab->elf.dynobj,
				  (bfd_size_type) sizeof *p));
		  if (p == NULL)
		    return false;
		  p->next = *head;
		  *head = p;
		  p->sec = sec;
		  p->count = 0;
		  p->pc_count = 0;
		}

	      p->count += 1;
	      if (!IS_ABSOLUTE_RELOC (r_type))
		p->pc_count += 1;
	    }
	  break;

	default:
	  break;
	}
    }

  return true;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
ppc64_elf_gc_mark_hook (abfd, info, rel, h, sym)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  if (h != NULL)
    {
      enum elf_ppc_reloc_type r_type;

      r_type = (enum elf_ppc_reloc_type) ELF64_R_TYPE (rel->r_info);
      switch (r_type)
	{
	case R_PPC64_GNU_VTINHERIT:
	case R_PPC64_GNU_VTENTRY:
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
      return bfd_section_from_elf_index (abfd, sym->st_shndx);
    }

  return NULL;
}

/* Update the .got, .plt. and dynamic reloc reference counts for the
   section being removed.  */

static boolean
ppc64_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;

  elf_section_data (sec)->local_dynrel = NULL;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      enum elf_ppc_reloc_type r_type;
      struct elf_link_hash_entry *h;

      r_symndx = ELF64_R_SYM (rel->r_info);
      r_type = (enum elf_ppc_reloc_type) ELF64_R_TYPE (rel->r_info);
      switch (r_type)
	{
	case R_PPC64_GOT16:
	case R_PPC64_GOT16_DS:
	case R_PPC64_GOT16_HA:
	case R_PPC64_GOT16_HI:
	case R_PPC64_GOT16_LO:
	case R_PPC64_GOT16_LO_DS:
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

	case R_PPC64_PLT16_HA:
	case R_PPC64_PLT16_HI:
	case R_PPC64_PLT16_LO:
	case R_PPC64_PLT32:
	case R_PPC64_PLT64:
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      if (h->plt.refcount > 0)
		h->plt.refcount--;
	    }
	  break;

	case R_PPC64_REL24:
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      if (h->plt.refcount > 0)
		h->plt.refcount--;
	    }
	  break;

	case R_PPC64_REL14:
	case R_PPC64_REL14_BRNTAKEN:
	case R_PPC64_REL14_BRTAKEN:
	case R_PPC64_REL32:
	case R_PPC64_REL64:
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      struct ppc_link_hash_entry *eh;
	      struct ppc_dyn_relocs **pp;
	      struct ppc_dyn_relocs *p;

	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      eh = (struct ppc_link_hash_entry *) h;

	      for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
		if (p->sec == sec)
		  {
		    p->pc_count -= 1;
		    p->count -= 1;
		    if (p->count == 0)
		      *pp = p->next;
		    break;
		  }
	    }
	  break;

	case R_PPC64_ADDR14:
	case R_PPC64_ADDR14_BRNTAKEN:
	case R_PPC64_ADDR14_BRTAKEN:
	case R_PPC64_ADDR16:
	case R_PPC64_ADDR16_DS:
	case R_PPC64_ADDR16_HA:
	case R_PPC64_ADDR16_HI:
	case R_PPC64_ADDR16_HIGHER:
	case R_PPC64_ADDR16_HIGHERA:
	case R_PPC64_ADDR16_HIGHEST:
	case R_PPC64_ADDR16_HIGHESTA:
	case R_PPC64_ADDR16_LO:
	case R_PPC64_ADDR16_LO_DS:
	case R_PPC64_ADDR24:
	case R_PPC64_ADDR30:
	case R_PPC64_ADDR32:
	case R_PPC64_ADDR64:
	case R_PPC64_UADDR16:
	case R_PPC64_UADDR32:
	case R_PPC64_UADDR64:
	case R_PPC64_TOC:
	  if (r_symndx >= symtab_hdr->sh_info)
	    {
	      struct ppc_link_hash_entry *eh;
	      struct ppc_dyn_relocs **pp;
	      struct ppc_dyn_relocs *p;

	      h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	      eh = (struct ppc_link_hash_entry *) h;

	      for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
		if (p->sec == sec)
		  {
		    p->count -= 1;
		    if (p->count == 0)
		      *pp = p->next;
		    break;
		  }
	    }
	  break;

	default:
	  break;
	}
    }
  return true;
}

/* Called via elf_link_hash_traverse to transfer dynamic linking
   information on function code symbol entries to their corresponding
   function descriptor symbol entries.  */
static boolean
func_desc_adjust (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info;
  struct ppc_link_hash_table *htab;

  if (h->root.type == bfd_link_hash_indirect)
    return true;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  info = (struct bfd_link_info *) inf;
  htab = ppc_hash_table (info);

  /* If this is a function code symbol, transfer dynamic linking
     information to the function descriptor symbol.  */
  if (!((struct ppc_link_hash_entry *) h)->is_func)
    return true;

  if (h->plt.refcount > 0
      && h->root.root.string[0] == '.'
      && h->root.root.string[1] != '\0')
    {
      struct elf_link_hash_entry *fdh;
      boolean force_local;

      /* Find the corresponding function descriptor symbol.  Create it
	 as undefined if necessary.  */

      fdh = elf_link_hash_lookup (&htab->elf, h->root.root.string + 1,
				  false, false, true);

      if (fdh == NULL
	  && info->shared
	  && (h->root.type == bfd_link_hash_undefined
	      || h->root.type == bfd_link_hash_undefweak))
	{
	  bfd *abfd;
	  asymbol *newsym;

	  abfd = h->root.u.undef.abfd;
	  newsym = bfd_make_empty_symbol (abfd);
	  newsym->name = h->root.root.string + 1;
	  newsym->section = bfd_und_section_ptr;
	  newsym->value = 0;
	  newsym->flags = BSF_OBJECT;
	  if (h->root.type == bfd_link_hash_undefweak)
	    newsym->flags |= BSF_WEAK;

	  if ( !(_bfd_generic_link_add_one_symbol
		 (info, abfd, newsym->name, newsym->flags,
		  newsym->section, newsym->value, NULL, false, false,
		  (struct bfd_link_hash_entry **) &fdh)))
	    {
	      return false;
	    }
	  fdh->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;
	}

      if (fdh != NULL
	  && (fdh->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0
	  && (info->shared
	      || (fdh->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	      || (fdh->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0))
	{
	  if (fdh->dynindx == -1)
	    if (! bfd_elf64_link_record_dynamic_symbol (info, fdh))
	      return false;
	  fdh->elf_link_hash_flags |= (h->elf_link_hash_flags
				       & (ELF_LINK_HASH_REF_REGULAR
					  | ELF_LINK_HASH_REF_DYNAMIC
					  | ELF_LINK_HASH_REF_REGULAR_NONWEAK
					  | ELF_LINK_NON_GOT_REF));
	  if (ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    {
	      fdh->plt.refcount = h->plt.refcount;
	      fdh->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	    }
	  ((struct ppc_link_hash_entry *) fdh)->is_func_descriptor = 1;
	  fdh->root.root.string = h->root.root.string + 1;
	}

      /* Now that the info is on the function descriptor, clear the
	 function code sym info.  Any function code syms for which we
	 don't have a definition in a regular file, we force local.
	 This prevents a shared library from exporting syms that have
	 been imported from another library.  Function code syms that
	 are really in the library we must leave global to prevent the
	 linker dragging in a definition from a static library.  */
      force_local = ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
		     && info->shared);
      _bfd_elf_link_hash_hide_symbol (info, h, force_local);
    }

  return true;
}

#define MIN_SAVE_FPR 14
#define MAX_SAVE_FPR 31

/* Called near the start of bfd_elf_size_dynamic_sections.  We use
   this hook to a) provide some gcc support functions, and b) transfer
   dynamic linking information gathered so far on function code symbol
   entries, to their corresponding function descriptor symbol entries.  */
static boolean
ppc64_elf_func_desc_adjust (obfd, info)
     bfd *obfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab;
  unsigned int lowest_savef = MAX_SAVE_FPR + 2;
  unsigned int lowest_restf = MAX_SAVE_FPR + 2;
  unsigned int i;
  struct elf_link_hash_entry *h;
  char sym[10];

  htab = ppc_hash_table (info);

  if (htab->sfpr == NULL)
    /* We don't have any relocs.  */
    return true;

  /* First provide any missing ._savef* and ._restf* functions.  */
  memcpy (sym, "._savef14", 10);
  for (i = MIN_SAVE_FPR; i <= MAX_SAVE_FPR; i++)
    {
      sym[7] = i / 10 + '0';
      sym[8] = i % 10 + '0';
      h = elf_link_hash_lookup (&htab->elf, sym, false, false, true);
      if (h != NULL
	  && h->root.type == bfd_link_hash_undefined)
	{
	  if (lowest_savef > i)
	    lowest_savef = i;
	  h->root.type = bfd_link_hash_defined;
	  h->root.u.def.section = htab->sfpr;
	  h->root.u.def.value = (i - lowest_savef) * 4;
	  h->type = STT_FUNC;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  _bfd_elf_link_hash_hide_symbol (info, h, info->shared);
	}
    }

  memcpy (sym, "._restf14", 10);
  for (i = MIN_SAVE_FPR; i <= MAX_SAVE_FPR; i++)
    {
      sym[7] = i / 10 + '0';
      sym[8] = i % 10 + '0';
      h = elf_link_hash_lookup (&htab->elf, sym, false, false, true);
      if (h != NULL
	  && h->root.type == bfd_link_hash_undefined)
	{
	  if (lowest_restf > i)
	    lowest_restf = i;
	  h->root.type = bfd_link_hash_defined;
	  h->root.u.def.section = htab->sfpr;
	  h->root.u.def.value = ((MAX_SAVE_FPR + 2 - lowest_savef) * 4
				 + (i - lowest_restf) * 4);
	  h->type = STT_FUNC;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  _bfd_elf_link_hash_hide_symbol (info, h, info->shared);
	}
    }

  htab->sfpr->_raw_size = ((MAX_SAVE_FPR + 2 - lowest_savef) * 4
			   + (MAX_SAVE_FPR + 2 - lowest_restf) * 4);

  if (htab->sfpr->_raw_size == 0)
    {
      _bfd_strip_section_from_output (info, htab->sfpr);
    }
  else
    {
      bfd_byte *p = (bfd_byte *) bfd_alloc (htab->elf.dynobj,
					    htab->sfpr->_raw_size);
      if (p == NULL)
	return false;
      htab->sfpr->contents = p;

      for (i = lowest_savef; i <= MAX_SAVE_FPR; i++)
	{
	  unsigned int fpr = i << 21;
	  unsigned int stackoff = (1 << 16) - (MAX_SAVE_FPR + 1 - i) * 8;
	  bfd_put_32 (htab->elf.dynobj, STFD_FR0_0R1 + fpr + stackoff, p);
	  p += 4;
	}
      bfd_put_32 (htab->elf.dynobj, BLR, p);
      p += 4;

      for (i = lowest_restf; i <= MAX_SAVE_FPR; i++)
	{
	  unsigned int fpr = i << 21;
	  unsigned int stackoff = (1 << 16) - (MAX_SAVE_FPR + 1 - i) * 8;
	  bfd_put_32 (htab->elf.dynobj, LFD_FR0_0R1 + fpr + stackoff, p);
	  p += 4;
	}
      bfd_put_32 (htab->elf.dynobj, BLR, p);
      p += 4;
    }

  elf_link_hash_traverse (&htab->elf, func_desc_adjust, (PTR) info);
  return true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static boolean
ppc64_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  struct ppc_link_hash_table *htab;
  struct ppc_link_hash_entry * eh;
  struct ppc_dyn_relocs *p;
  asection *s;
  unsigned int power_of_two;

  htab = ppc_hash_table (info);

  /* Deal with function syms.  */
  if (h->type == STT_FUNC
      || (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      /* Clear procedure linkage table information for any symbol that
	 won't need a .plt entry.  */
      if (!((struct ppc_link_hash_entry *) h)->is_func_descriptor
	  || h->plt.refcount <= 0
	  || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
	  || (! info->shared
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) == 0))
	{
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}
      return true;
    }
  else
    h->plt.offset = (bfd_vma) -1;

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

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0)
    return true;

  eh = (struct ppc_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      s = p->sec->output_section;
      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	break;
    }

  /* If we didn't find any dynamic relocs in read-only sections, then
     we'll be keeping the dynamic relocs and avoiding the copy reloc.  */
  if (p == NULL)
    {
      h->elf_link_hash_flags &= ~ELF_LINK_NON_GOT_REF;
      return true;
    }

  /* We must allocate the symbol in our .dynbss section, which will
     become part of the .bss section of the executable.  There will be
     an entry for this symbol in the .dynsym section.  The dynamic
     object will contain position independent code, so all references
     from the dynamic object to this symbol will go through the global
     offset table.  The dynamic linker will use the .dynsym entry to
     determine the address it must put in the global offset table, so
     both the dynamic object and the regular object will refer to the
     same memory location for the variable.  */

  /* We must generate a R_PPC_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      htab->srelbss->_raw_size += sizeof (Elf64_External_Rela);
      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_COPY;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 4)
    power_of_two = 4;

  /* Apply the required alignment.  */
  s = htab->sdynbss;
  s->_raw_size = BFD_ALIGN (s->_raw_size, (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (htab->elf.dynobj, s))
    {
      if (! bfd_set_section_alignment (htab->elf.dynobj, s, power_of_two))
	return false;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->_raw_size;

  /* Increment the section size to make room for the symbol.  */
  s->_raw_size += h->size;

  return true;
}

/* If given a function descriptor symbol, hide both the function code
   sym and the descriptor.  */
static void
ppc64_elf_hide_symbol (info, h, force_local)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     boolean force_local;
{
  _bfd_elf_link_hash_hide_symbol (info, h, force_local);

  if (((struct ppc_link_hash_entry *) h)->is_func_descriptor)
    {
      const char *name;
      struct elf_link_hash_entry *fh;
      struct ppc_link_hash_table *htab;

      name = h->root.root.string - 1;
      htab = ppc_hash_table (info);
      fh = elf_link_hash_lookup (&htab->elf, name, false, false, false);
      if (fh != NULL)
	_bfd_elf_link_hash_hide_symbol (info, fh, force_local);
    }
}

/* This is the condition under which ppc64_elf_finish_dynamic_symbol
   will be called from elflink.h.  If elflink.h doesn't call our
   finish_dynamic_symbol routine, we'll need to do something about
   initializing any .plt and .got entries in ppc64_elf_relocate_section.  */
#define WILL_CALL_FINISH_DYNAMIC_SYMBOL(DYN, INFO, H) \
  ((DYN)								\
   && ((INFO)->shared							\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)	\
   && ((H)->dynindx != -1						\
       || ((H)->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0))

/* Allocate space in .plt, .got and associated reloc sections for
   dynamic relocs.  */

static boolean
allocate_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info;
  struct ppc_link_hash_table *htab;
  asection *s;
  struct ppc_link_hash_entry *eh;
  struct ppc_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return true;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  info = (struct bfd_link_info *) inf;
  htab = ppc_hash_table (info);

  if (htab->elf.dynamic_sections_created
      && h->plt.refcount > 0
      && h->dynindx != -1)
    {
      BFD_ASSERT (((struct ppc_link_hash_entry *) h)->is_func_descriptor);

      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (1, info, h))
	{
	  /* If this is the first .plt entry, make room for the special
	     first entry.  */
	  s = htab->splt;
	  if (s->_raw_size == 0)
	    s->_raw_size += PLT_INITIAL_ENTRY_SIZE;

	  h->plt.offset = s->_raw_size;

	  /* Make room for this entry.  */
	  s->_raw_size += PLT_ENTRY_SIZE;

	  /* Make room for the .stub and .glink code.  */
	  s = htab->sstub;
	  s->_raw_size += PLT_CALL_STUB_SIZE;

	  s = htab->sglink;
	  if (s->_raw_size == 0)
	    s->_raw_size += GLINK_CALL_STUB_SIZE;
	  /* We need bigger stubs past index 32767.  */
	  if (s->_raw_size >= GLINK_CALL_STUB_SIZE + 32768*2*4)
	    s->_raw_size += 4;
	  s->_raw_size += 2*4;

	  /* We also need to make an entry in the .rela.plt section.  */
	  s = htab->srelplt;
	  s->_raw_size += sizeof (Elf64_External_Rela);
	}
      else
	{
	  h->plt.offset = (bfd_vma) -1;
	  h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
	}
    }
  else
    {
      h->plt.offset = (bfd_vma) -1;
      h->elf_link_hash_flags &= ~ELF_LINK_HASH_NEEDS_PLT;
    }

  if (h->got.refcount > 0)
    {
      boolean dyn;

      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	{
	  if (! bfd_elf64_link_record_dynamic_symbol (info, h))
	    return false;
	}

      s = htab->sgot;
      h->got.offset = s->_raw_size;
      s->_raw_size += 8;
      dyn = htab->elf.dynamic_sections_created;
      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info, h))
	htab->srelgot->_raw_size += sizeof (Elf64_External_Rela);
    }
  else
    h->got.offset = (bfd_vma) -1;

  eh = (struct ppc_link_hash_entry *) h;
  if (eh->dyn_relocs == NULL)
    return true;

  /* In the shared -Bsymbolic case, discard space allocated for
     dynamic pc-relative relocs against symbols which turn out to be
     defined in regular objects.  For the normal shared case, discard
     space for relocs that have become local due to symbol visibility
     changes.  */

  if (info->shared)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
	  && ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0
	      || info->symbolic))
	{
	  struct ppc_dyn_relocs **pp;

	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; )
	    {
	      p->count -= p->pc_count;
	      p->pc_count = 0;
	      if (p->count == 0)
		*pp = p->next;
	      else
		pp = &p->next;
	    }
	}
    }
  else
    {
      /* For the non-shared case, discard space for relocs against
	 symbols which turn out to need copy relocs or are not
	 dynamic.  */

      if ((h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
	  && (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	      || (htab->elf.dynamic_sections_created
		  && (h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined))))
	{
	  /* Make sure this symbol is output as a dynamic symbol.
	     Undefined weak syms won't yet be marked as dynamic.  */
	  if (h->dynindx == -1
	      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	    {
	      if (! bfd_elf64_link_record_dynamic_symbol (info, h))
		return false;
	    }

	  /* If that succeeded, we know we'll be keeping all the
	     relocs.  */
	  if (h->dynindx != -1)
	    goto keep;
	}

      eh->dyn_relocs = NULL;

    keep: ;
    }

  /* Finally, allocate space.  */
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *sreloc = elf_section_data (p->sec)->sreloc;
      sreloc->_raw_size += p->count * sizeof (Elf64_External_Rela);
    }

  return true;
}

/* Find any dynamic relocs that apply to read-only sections.  */

static boolean
readonly_dynrelocs (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct ppc_link_hash_entry *eh;
  struct ppc_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  eh = (struct ppc_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *s = p->sec->output_section;

      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	{
	  struct bfd_link_info *info = (struct bfd_link_info *) inf;

	  info->flags |= DF_TEXTREL;

	  /* Not an error, just cut short the traversal.  */
	  return false;
	}
    }
  return true;
}

/* Set the sizes of the dynamic sections.  */

static boolean
ppc64_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab;
  bfd *dynobj;
  asection *s;
  boolean relocs;
  bfd *ibfd;

  htab = ppc_hash_table (info);
  dynobj = htab->elf.dynobj;
  if (dynobj == NULL)
    abort ();

  if (htab->elf.dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  if (s == NULL)
	    abort ();
	  s->_raw_size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}
    }

  /* Set up .got offsets for local syms, and space for local dynamic
     relocs.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_signed_vma *end_local_got;
      bfd_size_type locsymcount;
      Elf_Internal_Shdr *symtab_hdr;
      asection *srel;

      if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour)
	continue;

      for (s = ibfd->sections; s != NULL; s = s->next)
	{
	  struct ppc_dyn_relocs *p;

	  for (p = *((struct ppc_dyn_relocs **)
		     &elf_section_data (s)->local_dynrel);
	       p != NULL;
	       p = p->next)
	    {
	      if (!bfd_is_abs_section (p->sec)
		  && bfd_is_abs_section (p->sec->output_section))
		{
		  /* Input section has been discarded, either because
		     it is a copy of a linkonce section or due to
		     linker script /DISCARD/, so we'll be discarding
		     the relocs too.  */
		}
	      else if (p->count != 0)
		{
		  srel = elf_section_data (p->sec)->sreloc;
		  srel->_raw_size += p->count * sizeof (Elf64_External_Rela);
		  if ((p->sec->output_section->flags & SEC_READONLY) != 0)
		    info->flags |= DF_TEXTREL;
		}
	    }
	}

      local_got = elf_local_got_refcounts (ibfd);
      if (!local_got)
	continue;

      symtab_hdr = &elf_tdata (ibfd)->symtab_hdr;
      locsymcount = symtab_hdr->sh_info;
      end_local_got = local_got + locsymcount;
      s = htab->sgot;
      srel = htab->srelgot;
      for (; local_got < end_local_got; ++local_got)
	{
	  if (*local_got > 0)
	    {
	      *local_got = s->_raw_size;
	      s->_raw_size += 8;
	      if (info->shared)
		srel->_raw_size += sizeof (Elf64_External_Rela);
	    }
	  else
	    *local_got = (bfd_vma) -1;
	}
    }

  /* Allocate global sym .plt and .got entries, and space for global
     sym dynamic relocs.  */
  elf_link_hash_traverse (&htab->elf, allocate_dynrelocs, (PTR) info);

  /* We now have determined the sizes of the various dynamic sections.
     Allocate memory for them.  */
  relocs = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      bfd_vma size;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      if (s == htab->splt
	  || s == htab->sgot
	  || s == htab->sstub
	  || s == htab->sglink)
	{
	  /* Strip this section if we don't need it; see the
	     comment below.  */
	}
      else if (strncmp (bfd_get_section_name (dynobj, s), ".rela", 5) == 0)
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
	    }
	  else
	    {
	      if (s != htab->srelplt)
		relocs = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->_raw_size == 0)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  We use bfd_zalloc
	 here in case unused entries are not reclaimed before the
	 section's contents are written out.  This should not happen,
	 but this way if it does, we get a R_PPC64_NONE reloc instead
	 of garbage.  */
      size = s->_raw_size;
      if (s == htab->sstub)
	{
	  /* .stub may grow.  Allocate enough for the maximum growth.  */
	  size += (size + 65536 + 65535) / 65536 * 4;
	}
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, size);
      if (s->contents == NULL)
	return false;
    }

  if (htab->elf.dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in ppc64_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  bfd_elf64_add_dynamic_entry (info, (bfd_vma) (TAG), (bfd_vma) (VAL))

      if (!info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return false;
	}

      if (htab->splt->_raw_size != 0)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0)
	      || !add_dynamic_entry (DT_PPC64_GLINK, 0))
	    return false;
	}

      if (NO_OPD_RELOCS)
	{
	  if (!add_dynamic_entry (DT_PPC64_OPD, 0)
	      || !add_dynamic_entry (DT_PPC64_OPDSZ, 0))
	    return false;
	}

      if (relocs)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf64_External_Rela)))
	    return false;

	  /* If any dynamic relocs apply to a read-only section,
	     then we need a DT_TEXTREL entry.  */
	  if ((info->flags & DF_TEXTREL) == 0)
	    elf_link_hash_traverse (&htab->elf, readonly_dynrelocs,
				    (PTR) info);

	  if ((info->flags & DF_TEXTREL) != 0)
	    {
	      if (!add_dynamic_entry (DT_TEXTREL, 0))
		return false;
	    }
	}
    }
#undef add_dynamic_entry

  return true;
}

/* Called after we have seen all the input files/sections, but before
   final symbol resolution and section placement has been determined.

   We use this hook to provide a value for TOCstart, which we store in
   the output bfd elf_gp.  */

boolean
ppc64_elf_set_toc (obfd, info)
     bfd *obfd;
     struct bfd_link_info *info;
{
  if (!info->relocateable)
    {
      asection *s;
      bfd_vma TOCstart;

      /* The TOC consists of sections .got, .toc, .tocbss, .plt in that
	 order.  The TOC starts where the first of these sections starts.  */
      s = bfd_get_section_by_name (obfd, ".got");
      if (s == NULL)
	s = bfd_get_section_by_name (obfd, ".toc");
      if (s == NULL)
	s = bfd_get_section_by_name (obfd, ".tocbss");
      if (s == NULL)
	s = bfd_get_section_by_name (obfd, ".plt");
      if (s == NULL)
	{
	  /* This may happen for
	     o  references to TOC base (SYM@toc / TOC[tc0]) without a
	     .toc directive
	     o  bad linker script
	     o --gc-sections and empty TOC sections

	     FIXME: Warn user?  */

	  /* Look for a likely section.  We probably won't even be
	     using TOCstart.  */
	  for (s = obfd->sections; s != NULL; s = s->next)
	    if ((s->flags & (SEC_ALLOC | SEC_SMALL_DATA | SEC_READONLY))
		== (SEC_ALLOC | SEC_SMALL_DATA))
	      break;
	  if (s == NULL)
	    for (s = obfd->sections; s != NULL; s = s->next)
	      if ((s->flags & (SEC_ALLOC | SEC_SMALL_DATA))
		  == (SEC_ALLOC | SEC_SMALL_DATA))
		break;
	  if (s == NULL)
	    for (s = obfd->sections; s != NULL; s = s->next)
	      if ((s->flags & (SEC_ALLOC | SEC_READONLY)) == SEC_ALLOC)
		break;
	  if (s == NULL)
	    for (s = obfd->sections; s != NULL; s = s->next)
	      if ((s->flags & SEC_ALLOC) == SEC_ALLOC)
		break;
	}

      TOCstart = 0;
      if (s != NULL)
	TOCstart = s->output_section->vma + s->output_offset;

      elf_gp (obfd) = TOCstart;
    }
  return true;
}

/* PowerPC64 .plt entries are 24 bytes long, which doesn't divide
   evenly into 64k.  Sometimes with a large enough .plt, we'll need to
   use offsets differing in the high 16 bits when accessing a .plt
   entry from a .plt call stub.  This function adjusts the size of
   .stub to accommodate the extra stub instruction needed in such
   cases.  */

boolean
ppc64_elf_size_stubs (obfd, info, changed)
     bfd *obfd;
     struct bfd_link_info *info;
     int *changed;
{
  struct ppc_link_hash_table *htab = ppc_hash_table (info);
  bfd_vma plt_offset, next_64k;
  long base, num, extra;

  /* .plt and .stub should be both present, or both absent.  */
  if ((htab->splt == NULL || htab->splt->_raw_size == 0)
      != (htab->sstub == NULL || htab->sstub->_raw_size == 0))
    abort ();

  /* If no .plt, then nothing to do.  */
  if (htab->splt == NULL || htab->splt->_raw_size == 0)
    return true;

  plt_offset = (htab->splt->output_section->vma
		+ htab->splt->output_offset
		- elf_gp (obfd));
  next_64k = (plt_offset + 65535) & -65536;

  /* If the .plt doesn't have any entries crossing a 64k boundary,
     then there is no need for bigger stubs.  */
  if (plt_offset + htab->splt->_raw_size <= next_64k)
    return true;

  /* OK, so we have at least one transition.  Since .plt entries are
     24 bytes long, we'll strike it lucky every 3*64k, with the 64k
     boundary between .plt entries.  */
  base = next_64k / 65536;
  num = (plt_offset + htab->splt->_raw_size - next_64k) / 65536;
  extra = (base % 3 + num + 1) * 2 / 3;

  /* Allow one extra instruction for each EXTRA.  The change in .stub
     may change the location of .toc and .plt.  .toc and .plt ought to
     move as a group, but someone might be playing with eg. .plt
     alignment, so don't allow .stub size to decrease.  */
  if (htab->sstub->_cooked_size < htab->sstub->_raw_size + extra * 4)
    {
      htab->sstub->_cooked_size = htab->sstub->_raw_size + extra * 4;
      *changed = true;
    }
  return true;
}

/* Build a .plt call stub.  */

static bfd_byte *
build_plt_stub (obfd, p, offset, glink)
     bfd *obfd;
     bfd_byte *p;
     int offset;
     int glink;
{
#define PPC_LO(v) ((v) & 0xffff)
#define PPC_HI(v) (((v) >> 16) & 0xffff)
#define PPC_HA(v) PPC_HI ((v) + 0x8000)

  if (glink)
    bfd_put_32 (obfd, LD_R2_40R1, p),			p += 4;
  bfd_put_32 (obfd, ADDIS_R12_R2 | PPC_HA (offset), p),	p += 4;
  if (!glink)
    bfd_put_32 (obfd, STD_R2_40R1, p),			p += 4;
  bfd_put_32 (obfd, LD_R11_0R12 | PPC_LO (offset), p),	p += 4;
  if (PPC_HA (offset + 8) != PPC_HA (offset))
    bfd_put_32 (obfd, ADDIS_R12_R12_1, p),		p += 4;
  offset += 8;
  bfd_put_32 (obfd, LD_R2_0R12 | PPC_LO (offset), p),	p += 4;
  if (PPC_HA (offset + 8) != PPC_HA (offset))
    bfd_put_32 (obfd, ADDIS_R12_R12_1, p),		p += 4;
  offset += 8;
  bfd_put_32 (obfd, MTCTR_R11, p),			p += 4;
  bfd_put_32 (obfd, LD_R11_0R12 | PPC_LO (offset), p),	p += 4;
  bfd_put_32 (obfd, BCTR, p),				p += 4;
  return p;
}

/* Build the stubs for one function call.  */

static boolean
build_one_stub (h, inf)
     struct elf_link_hash_entry *h;
     PTR inf;
{
  struct bfd_link_info *info;
  struct ppc_link_hash_table *htab;

  if (h->root.type == bfd_link_hash_indirect
      || h->root.type == bfd_link_hash_warning)
    return true;

  info = (struct bfd_link_info *) inf;
  htab = ppc_hash_table (info);

  if (htab->elf.dynamic_sections_created
      && h->plt.offset != (bfd_vma) -1
      && ((struct ppc_link_hash_entry *) h)->is_func_descriptor)
    {
      struct elf_link_hash_entry *fh;
      asection *s;
      bfd_vma plt_r2;
      bfd_byte *p;
      unsigned int indx;

      fh = elf_link_hash_lookup (&htab->elf, h->root.root.string - 1,
				 false, false, true);

      if (fh == NULL)
	abort ();

      BFD_ASSERT (((struct ppc_link_hash_entry *) fh)->is_func);

      /* Build the .plt call stub.  */
      plt_r2 = (htab->splt->output_section->vma
		+ htab->splt->output_offset
		+ h->plt.offset
		- elf_gp (htab->splt->output_section->owner)
		- TOC_BASE_OFF);

      if (plt_r2 + 0x80000000 > 0xffffffff
	  || (plt_r2 & 3) != 0)
	{
	  (*_bfd_error_handler)
	    (_("linkage table error against `%s'"),
	     h->root.root.string);
	  bfd_set_error (bfd_error_bad_value);
	  htab->plt_overflow = true;
	  return false;
	}

      s = htab->sstub;
      /* Steal plt.offset to store the stub offset.  */
      fh->plt.offset = s->_cooked_size;
      p = s->contents + s->_cooked_size;
      p = build_plt_stub (s->owner, p, (int) plt_r2, 0);
      s->_cooked_size = p - s->contents;

      /* Build the .glink lazy link call stub.  */
      s = htab->sglink;
      p = s->contents + s->_cooked_size;
      indx = s->reloc_count;
      if (indx < 0x8000)
	{
	  bfd_put_32 (s->owner, LI_R0_0 | indx, p);
	  p += 4;
	}
      else
	{
	  bfd_put_32 (s->owner, LIS_R0_0 | PPC_HI (indx), p);
	  p += 4;
	  bfd_put_32 (s->owner, ORI_R0_R0_0 | PPC_LO (indx), p);
	  p += 4;
	}
      bfd_put_32 (s->owner, B_DOT | ((s->contents - p) & 0x3fffffc), p);
      p += 4;
      s->_cooked_size = p - s->contents;
      s->reloc_count += 1;
    }
  return true;
}

boolean
ppc64_elf_build_stubs (obfd, info)
     bfd *obfd;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab = ppc_hash_table (info);
  bfd_vma old_stub_size;
  bfd_vma plt_r2;
  bfd_byte *p;

  /* If no .plt stubs, then nothing to do.  */
  if (htab->sstub == NULL || htab->sstub->_raw_size == 0)
    return true;

  old_stub_size = htab->sstub->_cooked_size;
  htab->sstub->_cooked_size = 0;

  /* Build the .glink plt call stub.  */
  plt_r2 = (htab->splt->output_section->vma
	    + htab->splt->output_offset
	    - elf_gp (obfd)
	    - TOC_BASE_OFF);
  p = htab->sglink->contents;
  p = build_plt_stub (htab->sglink->owner, p, (int) plt_r2, 1);
  while (p - htab->sglink->contents < GLINK_CALL_STUB_SIZE)
    {
      bfd_put_32 (htab->sglink->owner, NOP, p);
      p += 4;
    }
  htab->sglink->_cooked_size = p - htab->sglink->contents;

  /* Use reloc_count to count entries.  */
  htab->sglink->reloc_count = 0;

  elf_link_hash_traverse (&htab->elf, build_one_stub, (PTR) info);
  htab->sglink->reloc_count = 0;

  if (htab->plt_overflow)
    return false;

  if (old_stub_size != htab->sstub->_cooked_size
      || htab->sglink->_raw_size != htab->sglink->_cooked_size)
    {
      (*_bfd_error_handler)
	(_("stub section size doesn't match calculated size"));
      bfd_set_error (bfd_error_bad_value);
      return false;
    }
  return true;
}

/* Set up any other section flags and such that may be necessary.  */

static boolean
ppc64_elf_fake_sections (abfd, shdr, asect)
     bfd *abfd ATTRIBUTE_UNUSED;
     Elf64_Internal_Shdr *shdr;
     asection *asect;
{
  if ((asect->flags & SEC_EXCLUDE) != 0)
    shdr->sh_flags |= SHF_EXCLUDE;

  if ((asect->flags & SEC_SORT_ENTRIES) != 0)
    shdr->sh_type = SHT_ORDERED;

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
ppc64_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  struct ppc_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  bfd_vma *local_got_offsets;
  bfd_vma TOCstart;
  boolean ret = true;
  boolean is_opd;
  /* Disabled until we sort out how ld should choose 'y' vs 'at'.  */
  boolean is_power4 = false;

  /* Initialize howto table if needed.  */
  if (!ppc64_elf_howto_table[R_PPC64_ADDR32])
    ppc_howto_init ();

  htab = ppc_hash_table (info);
  local_got_offsets = elf_local_got_offsets (input_bfd);
  TOCstart = elf_gp (output_bfd);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  is_opd = strcmp (bfd_get_section_name (abfd, input_section), ".opd") == 0;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      enum elf_ppc_reloc_type r_type;
      bfd_vma offset;
      bfd_vma addend;
      bfd_reloc_status_type r;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      const char *sym_name;
      unsigned long r_symndx;
      bfd_vma relocation;
      boolean unresolved_reloc;
      boolean has_nop;
      long insn;

      r_type = (enum elf_ppc_reloc_type) ELF64_R_TYPE (rel->r_info);
      r_symndx = ELF64_R_SYM (rel->r_info);

      if (info->relocateable)
	{
	  /* This is a relocatable link.  We don't have to change
	     anything, unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if ((unsigned) ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }
	  continue;
	}

      /* This is a final link.  */

      offset = rel->r_offset;
      addend = rel->r_addend;
      r = bfd_reloc_other;
      sym = (Elf_Internal_Sym *) 0;
      sec = (asection *) 0;
      h = (struct elf_link_hash_entry *) 0;
      sym_name = (const char *) 0;
      unresolved_reloc = false;

      if (r_type == R_PPC64_TOC)
	{
	  /* Relocation value is TOC base.  Symbol is ignored.  */
	  relocation = TOCstart + TOC_BASE_OFF;
	}
      else if (r_symndx < symtab_hdr->sh_info)
	{
	  /* It's a local symbol.  */
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  sym_name = "<local symbol>";

	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, sec, rel);
	  addend = rel->r_addend;
	}
      else
	{
	  /* It's a global symbol.  */
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	  sym_name = h->root.root.string;
	  relocation = 0;
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      if (sec->output_section == NULL)
		/* Set a flag that will be cleared later if we find a
		   relocation value for this symbol.  output_section
		   is typically NULL for symbols satisfied by a shared
		   library.  */
		unresolved_reloc = true;
	      else
		relocation = (h->root.u.def.value
			      + sec->output_section->vma
			      + sec->output_offset);
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    ;
	  else if (info->shared
		   && (!info->symbolic || info->allow_shlib_undefined)
		   && !info->no_undefined
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    ;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      offset, (!info->shared
			       || info->no_undefined
			       || ELF_ST_VISIBILITY (h->other)))))
		return false;
	    }
	}

      /* First handle relocations that tweak non-addend part of insn.  */
      insn = 0;
      switch (r_type)
	{
	default:
	  break;

	  /* Branch taken prediction relocations.  */
	case R_PPC64_ADDR14_BRTAKEN:
	case R_PPC64_REL14_BRTAKEN:
	  insn = 0x01 << 21; /* 'y' or 't' bit, lowest bit of BO field. */
	  /* Fall thru. */

	  /* Branch not taken prediction relocations.  */
	case R_PPC64_ADDR14_BRNTAKEN:
	case R_PPC64_REL14_BRNTAKEN:
	  insn |= bfd_get_32 (output_bfd, contents + offset) & ~(0x01 << 21);
	  if (is_power4)
	    {
	      /* Set 'a' bit.  This is 0b00010 in BO field for branch
		 on CR(BI) insns (BO == 001at or 011at), and 0b01000
		 for branch on CTR insns (BO == 1a00t or 1a01t).  */
	      if ((insn & (0x14 << 21)) == (0x04 << 21))
		insn |= 0x02 << 21;
	      else if ((insn & (0x14 << 21)) == (0x10 << 21))
		insn |= 0x08 << 21;
	      else
		break;
	    }
	  else
	    {
	      /* Invert 'y' bit if not the default.  */
	      if ((bfd_signed_vma) (relocation - offset) < 0)
		insn ^= 0x01 << 21;
	    }

	  bfd_put_32 (output_bfd, (bfd_vma) insn, contents + offset);
	  break;

	case R_PPC64_REL24:
	case R_PPC64_ADDR24:
	  /* An ADDR24 or REL24 branching to a linkage function may be
	     followed by a nop that we have to replace with a ld in
	     order to restore the TOC base pointer.  Only calls to
	     shared objects need to alter the TOC base.  These are
	     recognized by their need for a PLT entry.  */
	  has_nop = 0;
	  if (h != NULL
	      && h->plt.offset != (bfd_vma) -1
	      && htab->sstub != NULL)
	    {
	      /* plt.offset here is the offset into the stub section.  */
	      relocation = (htab->sstub->output_section->vma
			    + htab->sstub->output_offset
			    + h->plt.offset);
	      unresolved_reloc = false;

	      /* Make sure that there really is an instruction after
                 the branch that we can decode.  */
	      if (offset + 8 <= input_section->_cooked_size)
		{
		  bfd_byte *pnext;

		  pnext = contents + offset + 4;
		  insn = bfd_get_32 (input_bfd, pnext);

		  if (insn == 0x60000000	 /* nop (ori  r0,r0,0) */
		      || insn == 0x4def7b82	 /* cror 15,15,15 */
		      || insn == 0x4ffffb82) /* cror 31,31,31 */
		    {
		      bfd_put_32 (input_bfd,
				  (bfd_vma) 0xe8410028, /* ld r2,40(r1) */
				  pnext);
		      has_nop = 1;
		    }
		}
	    }

	  if (h != NULL
	      && h->root.type == bfd_link_hash_undefweak
	      && r_type == R_PPC64_REL24
	      && addend == 0
	      && relocation == 0)
	    {
	      /* Tweak calls to undefined weak functions to behave as
		 if the "called" function immediately returns.  We can
		 thus call to a weak function without first checking
		 whether the function is defined.  */
	      relocation = 4;
	      if (has_nop)
		relocation = 8;
	    }
	  break;
	}

      /* Set `addend'.  */
      switch (r_type)
	{
	default:
	  (*_bfd_error_handler)
	    (_("%s: unknown relocation type %d for symbol %s"),
	     bfd_archive_filename (input_bfd), (int) r_type, sym_name);

	  bfd_set_error (bfd_error_bad_value);
	  ret = false;
	  continue;

	case R_PPC64_NONE:
	case R_PPC_GNU_VTINHERIT:
	case R_PPC_GNU_VTENTRY:
	  continue;

	  /* GOT16 relocations.  Like an ADDR16 using the symbol's
	     address in the GOT as relocation value instead of the
	     symbols value itself.  Also, create a GOT entry for the
	     symbol and put the symbol value there.  */
	case R_PPC64_GOT16:
	case R_PPC64_GOT16_LO:
	case R_PPC64_GOT16_HI:
	case R_PPC64_GOT16_HA:
	case R_PPC64_GOT16_DS:
	case R_PPC64_GOT16_LO_DS:
	  {
	    /* Relocation is to the entry for this symbol in the global
	       offset table.  */
	    bfd_vma off;

	    if (htab->sgot == NULL)
	      abort ();

	    if (h != NULL)
	      {
		boolean dyn;

		off = h->got.offset;
		dyn = htab->elf.dynamic_sections_created;
		if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info, h)
		    || (info->shared
			&& (info->symbolic
			    || h->dynindx == -1
			    || (h->elf_link_hash_flags
				& ELF_LINK_FORCED_LOCAL))
			&& (h->elf_link_hash_flags
			    & ELF_LINK_HASH_DEF_REGULAR)))
		  {
		    /* This is actually a static link, or it is a
		       -Bsymbolic link and the symbol is defined
		       locally, or the symbol was forced to be local
		       because of a version file.  We must initialize
		       this entry in the global offset table.  Since the
		       offset must always be a multiple of 8, we use the
		       least significant bit to record whether we have
		       initialized it already.

		       When doing a dynamic link, we create a .rel.got
		       relocation entry to initialize the value.  This
		       is done in the finish_dynamic_symbol routine.  */
		    if ((off & 1) != 0)
		      off &= ~1;
		    else
		      {
			bfd_put_64 (output_bfd, relocation,
				    htab->sgot->contents + off);
			h->got.offset |= 1;
		      }
		  }
		else
		  unresolved_reloc = false;
	      }
	    else
	      {
		if (local_got_offsets == NULL)
		  abort ();

		off = local_got_offsets[r_symndx];

		/* The offset must always be a multiple of 8.  We use
		   the least significant bit to record whether we have
		   already processed this entry.  */
		if ((off & 1) != 0)
		  off &= ~1;
		else
		  {
		    bfd_put_64 (output_bfd, relocation,
				htab->sgot->contents + off);

		    if (info->shared)
		      {
			Elf_Internal_Rela outrel;
			Elf64_External_Rela *loc;

			/* We need to generate a R_PPC64_RELATIVE reloc
			   for the dynamic linker.  */
			outrel.r_offset = (htab->sgot->output_section->vma
					   + htab->sgot->output_offset
					   + off);
			outrel.r_info = ELF64_R_INFO (0, R_PPC64_RELATIVE);
			outrel.r_addend = relocation;
			loc = (Elf64_External_Rela *) htab->srelgot->contents;
			loc += htab->srelgot->reloc_count++;
			bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);
		      }

		    local_got_offsets[r_symndx] |= 1;
		  }
	      }

	    if (off >= (bfd_vma) -2)
	      abort ();

	    relocation = htab->sgot->output_offset + off;

	    /* TOC base (r2) is TOC start plus 0x8000.  */
	    addend -= TOC_BASE_OFF;
	  }
	  break;

	case R_PPC64_PLT16_HA:
	case R_PPC64_PLT16_HI:
	case R_PPC64_PLT16_LO:
	case R_PPC64_PLT32:
	case R_PPC64_PLT64:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLT reloc against a local symbol directly,
	     without using the procedure linkage table.  */
	  if (h == NULL)
	    break;

	  if (h->plt.offset == (bfd_vma) -1
	      || htab->splt == NULL)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      break;
	    }

	  relocation = (htab->splt->output_section->vma
			+ htab->splt->output_offset
			+ h->plt.offset);
	  unresolved_reloc = false;
	  break;

	  /* TOC16 relocs.  We want the offset relative to the TOC base,
	     which is the address of the start of the TOC plus 0x8000.
	     The TOC consists of sections .got, .toc, .tocbss, and .plt,
	     in this order.  */
	case R_PPC64_TOC16:
	case R_PPC64_TOC16_LO:
	case R_PPC64_TOC16_HI:
	case R_PPC64_TOC16_DS:
	case R_PPC64_TOC16_LO_DS:
	case R_PPC64_TOC16_HA:
	  addend -= TOCstart + TOC_BASE_OFF;
	  break;

	  /* Relocate against the beginning of the section.  */
	case R_PPC64_SECTOFF:
	case R_PPC64_SECTOFF_LO:
	case R_PPC64_SECTOFF_HI:
	case R_PPC64_SECTOFF_DS:
	case R_PPC64_SECTOFF_LO_DS:
	case R_PPC64_SECTOFF_HA:
	  if (sec != (asection *) 0)
	    addend -= sec->output_section->vma;
	  break;

	case R_PPC64_REL24:
	  break;

	  /* Relocations that may need to be propagated if this is a
	     dynamic object.  */
	case R_PPC64_REL14:
	case R_PPC64_REL14_BRNTAKEN:
	case R_PPC64_REL14_BRTAKEN:
	case R_PPC64_REL32:
	case R_PPC64_REL64:
	case R_PPC64_ADDR14:
	case R_PPC64_ADDR14_BRNTAKEN:
	case R_PPC64_ADDR14_BRTAKEN:
	case R_PPC64_ADDR16:
	case R_PPC64_ADDR16_DS:
	case R_PPC64_ADDR16_HA:
	case R_PPC64_ADDR16_HI:
	case R_PPC64_ADDR16_HIGHER:
	case R_PPC64_ADDR16_HIGHERA:
	case R_PPC64_ADDR16_HIGHEST:
	case R_PPC64_ADDR16_HIGHESTA:
	case R_PPC64_ADDR16_LO:
	case R_PPC64_ADDR16_LO_DS:
	case R_PPC64_ADDR24:
	case R_PPC64_ADDR30:
	case R_PPC64_ADDR32:
	case R_PPC64_ADDR64:
	case R_PPC64_UADDR16:
	case R_PPC64_UADDR32:
	case R_PPC64_UADDR64:
	  /* r_symndx will be zero only for relocs against symbols
	     from removed linkonce sections, or sections discarded by
	     a linker script.  */
	  if (r_symndx == 0)
	    break;
	  /* Fall thru.  */

	case R_PPC64_TOC:
	  if ((input_section->flags & SEC_ALLOC) == 0)
	    break;

	  if (NO_OPD_RELOCS && is_opd)
	    break;

	  if ((info->shared
	       && (IS_ABSOLUTE_RELOC (r_type)
		   || (h != NULL
		       && h->dynindx != -1
		       && (! info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))))
	      || (!info->shared
		  && h != NULL
		  && h->dynindx != -1
		  && (h->elf_link_hash_flags & ELF_LINK_NON_GOT_REF) == 0
		  && (((h->elf_link_hash_flags
			& ELF_LINK_HASH_DEF_DYNAMIC) != 0
		       && (h->elf_link_hash_flags
			   & ELF_LINK_HASH_DEF_REGULAR) == 0)
		      || h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined)))
	    {
	      Elf_Internal_Rela outrel;
	      boolean skip, relocate;
	      asection *sreloc;
	      Elf64_External_Rela *loc;

	      /* When generating a dynamic object, these relocations
		 are copied into the output file to be resolved at run
		 time.  */

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
	      outrel.r_addend = addend;

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      else if (h != NULL
		       && h->dynindx != -1
		       && !is_opd
		       && (!IS_ABSOLUTE_RELOC (r_type)
			   || !info->shared
			   || !info->symbolic
			   || (h->elf_link_hash_flags
			       & ELF_LINK_HASH_DEF_REGULAR) == 0))
		outrel.r_info = ELF64_R_INFO (h->dynindx, r_type);
	      else
		{
		  /* This symbol is local, or marked to become local,
		     or this is an opd section reloc which must point
		     at a local function.  */
		  outrel.r_addend += relocation;
		  relocate = true;
		  if (r_type == R_PPC64_ADDR64 || r_type == R_PPC64_TOC)
		    {
		      outrel.r_info = ELF64_R_INFO (0, R_PPC64_RELATIVE);
		    }
		  else
		    {
		      long indx = 0;

		      if (bfd_is_abs_section (sec))
			;
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

			  /* We are turning this relocation into one
			     against a section symbol, so subtract out
			     the output section's address but not the
			     offset of the input section in the output
			     section.  */
			  outrel.r_addend -= osec->vma;
			}

		      outrel.r_info = ELF64_R_INFO (indx, r_type);
		    }
		}

	      sreloc = elf_section_data (input_section)->sreloc;
	      if (sreloc == NULL)
		abort ();

	      loc = (Elf64_External_Rela *) sreloc->contents;
	      loc += sreloc->reloc_count++;
	      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);

	      /* If this reloc is against an external symbol, it will
		 be computed at runtime, so there's no need to do
		 anything now.  */
	      if (! relocate)
		continue;
	    }
	  break;

	case R_PPC64_COPY:
	case R_PPC64_GLOB_DAT:
	case R_PPC64_JMP_SLOT:
	case R_PPC64_RELATIVE:
	  /* We shouldn't ever see these dynamic relocs in relocatable
	     files.  */
	  /* Fall thru */

	case R_PPC64_PLTGOT16:
	case R_PPC64_PLTGOT16_DS:
	case R_PPC64_PLTGOT16_HA:
	case R_PPC64_PLTGOT16_HI:
	case R_PPC64_PLTGOT16_LO:
	case R_PPC64_PLTGOT16_LO_DS:
	case R_PPC64_PLTREL32:
	case R_PPC64_PLTREL64:
	  /* These ones haven't been implemented yet.  */

	  (*_bfd_error_handler)
	    (_("%s: Relocation %s is not supported for symbol %s."),
	     bfd_archive_filename (input_bfd),
	     ppc64_elf_howto_table[(int) r_type]->name, sym_name);

	  bfd_set_error (bfd_error_invalid_operation);
	  ret = false;
	  continue;
	}

      /* Do any further special processing.  */
      switch (r_type)
	{
	default:
	  break;

	case R_PPC64_ADDR16_HA:
	case R_PPC64_ADDR16_HIGHERA:
	case R_PPC64_ADDR16_HIGHESTA:
	case R_PPC64_PLT16_HA:
	case R_PPC64_TOC16_HA:
	case R_PPC64_SECTOFF_HA:
	  /* It's just possible that this symbol is a weak symbol
	     that's not actually defined anywhere. In that case,
	     'sec' would be NULL, and we should leave the symbol
	     alone (it will be set to zero elsewhere in the link).  */
	  if (sec != NULL)
	    /* Add 0x10000 if sign bit in 0:15 is set.  */
	    addend += ((relocation + addend) & 0x8000) << 1;
	  break;

	case R_PPC64_ADDR16_DS:
	case R_PPC64_ADDR16_LO_DS:
	case R_PPC64_GOT16_DS:
	case R_PPC64_GOT16_LO_DS:
	case R_PPC64_PLT16_LO_DS:
	case R_PPC64_SECTOFF_DS:
	case R_PPC64_SECTOFF_LO_DS:
	case R_PPC64_TOC16_DS:
	case R_PPC64_TOC16_LO_DS:
	case R_PPC64_PLTGOT16_DS:
	case R_PPC64_PLTGOT16_LO_DS:
	  if (((relocation + addend) & 3) != 0)
	    {
	      (*_bfd_error_handler)
		(_("%s: error: relocation %s not a multiple of 4"),
		 bfd_archive_filename (input_bfd),
		 ppc64_elf_howto_table[(int) r_type]->name);
	      bfd_set_error (bfd_error_bad_value);
	      ret = false;
	      continue;
	    }
	  break;
	}

      /* FIXME: Why do we allow debugging sections to escape this error?
	 More importantly, why do we not emit dynamic relocs above in
	 debugging sections (which are ! SEC_ALLOC)?  If we had
	 emitted the dynamic reloc, we could remove the fudge here.  */
      if (unresolved_reloc
	  && !(info->shared
	       && (input_section->flags & SEC_DEBUGGING) != 0
	       && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0))
	(*_bfd_error_handler)
	  (_("%s(%s+0x%lx): unresolvable relocation against symbol `%s'"),
	   bfd_archive_filename (input_bfd),
	   bfd_get_section_name (input_bfd, input_section),
	   (long) rel->r_offset,
	   h->root.root.string);

      r = _bfd_final_link_relocate (ppc64_elf_howto_table[(int) r_type],
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
		  && ppc64_elf_howto_table[(int) r_type]->pc_relative)
		{
		  /* Assume this is a call protected by other code that
		     detects the symbol is undefined.  If this is the case,
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

	  if (! ((*info->callbacks->reloc_overflow)
		 (info, name, ppc64_elf_howto_table[(int) r_type]->name,
		  (bfd_vma) 0, input_bfd, input_section, offset)))
	    return false;
	}
      else
	ret = false;
    }

  return ret;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static boolean
ppc64_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  struct ppc_link_hash_table *htab;
  bfd *dynobj;

  htab = ppc_hash_table (info);
  dynobj = htab->elf.dynobj;

  if (h->plt.offset != (bfd_vma) -1
      && ((struct ppc_link_hash_entry *) h)->is_func_descriptor)
    {
      Elf_Internal_Rela rela;
      Elf64_External_Rela *loc;

      /* This symbol has an entry in the procedure linkage table.  Set
         it up.  */

      if (htab->splt == NULL
	  || htab->srelplt == NULL
	  || htab->sglink == NULL)
	abort ();

      /* Create a JMP_SLOT reloc to inform the dynamic linker to
	 fill in the PLT entry.  */

      rela.r_offset = (htab->splt->output_section->vma
		       + htab->splt->output_offset
		       + h->plt.offset);
      rela.r_info = ELF64_R_INFO (h->dynindx, R_PPC64_JMP_SLOT);
      rela.r_addend = 0;

      loc = (Elf64_External_Rela *) htab->srelplt->contents;
      loc += (h->plt.offset - PLT_INITIAL_ENTRY_SIZE) / PLT_ENTRY_SIZE;
      bfd_elf64_swap_reloca_out (output_bfd, &rela, loc);
    }

  if (h->got.offset != (bfd_vma) -1)
    {
      Elf_Internal_Rela rela;
      Elf64_External_Rela *loc;

      /* This symbol has an entry in the global offset table.  Set it
         up.  */

      if (htab->sgot == NULL || htab->srelgot == NULL)
	abort ();

      rela.r_offset = (htab->sgot->output_section->vma
		       + htab->sgot->output_offset
		       + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a static link, or it is a -Bsymbolic link and the
	 symbol is defined locally or was forced to be local because
	 of a version file, we just want to emit a RELATIVE reloc.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && (info->symbolic
	      || h->dynindx == -1
	      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL))
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR))
	{
	  BFD_ASSERT((h->got.offset & 1) != 0);
	  rela.r_info = ELF64_R_INFO (0, R_PPC64_RELATIVE);
	  rela.r_addend = (h->root.u.def.value
			   + h->root.u.def.section->output_section->vma
			   + h->root.u.def.section->output_offset);
	}
      else
	{
	  BFD_ASSERT ((h->got.offset & 1) == 0);
	  bfd_put_64 (output_bfd, (bfd_vma) 0,
		      htab->sgot->contents + h->got.offset);
	  rela.r_info = ELF64_R_INFO (h->dynindx, R_PPC64_GLOB_DAT);
	  rela.r_addend = 0;
	}

      loc = (Elf64_External_Rela *) htab->srelgot->contents;
      loc += htab->srelgot->reloc_count++;
      bfd_elf64_swap_reloca_out (output_bfd, &rela, loc);
    }

  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_COPY) != 0)
    {
      Elf_Internal_Rela rela;
      Elf64_External_Rela *loc;

      /* This symbol needs a copy reloc.  Set it up.  */

      if (h->dynindx == -1
	  || (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	  || htab->srelbss == NULL)
	abort ();

      rela.r_offset = (h->root.u.def.value
		       + h->root.u.def.section->output_section->vma
		       + h->root.u.def.section->output_offset);
      rela.r_info = ELF64_R_INFO (h->dynindx, R_PPC64_COPY);
      rela.r_addend = 0;
      loc = (Elf64_External_Rela *) htab->srelbss->contents;
      loc += htab->srelbss->reloc_count++;
      bfd_elf64_swap_reloca_out (output_bfd, &rela, loc);
    }

  /* Mark some specially defined symbols as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0)
    sym->st_shndx = SHN_ABS;

  return true;
}

/* Used to decide how to sort relocs in an optimal manner for the
   dynamic linker, before writing them out.  */

static enum elf_reloc_type_class
ppc64_elf_reloc_type_class (rela)
     const Elf_Internal_Rela *rela;
{
  enum elf_ppc_reloc_type r_type;

  r_type = (enum elf_ppc_reloc_type) ELF64_R_TYPE (rela->r_info);
  switch (r_type)
    {
    case R_PPC64_RELATIVE:
      return reloc_class_relative;
    case R_PPC64_JMP_SLOT:
      return reloc_class_plt;
    case R_PPC64_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

/* Finish up the dynamic sections.  */

static boolean
ppc64_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  struct ppc_link_hash_table *htab;
  bfd *dynobj;
  asection *sdyn;

  htab = ppc_hash_table (info);
  dynobj = htab->elf.dynobj;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (htab->elf.dynamic_sections_created)
    {
      Elf64_External_Dyn *dyncon, *dynconend;

      if (sdyn == NULL || htab->sgot == NULL)
	abort ();

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  asection *s;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      continue;

	    case DT_PPC64_GLINK:
	      dyn.d_un.d_ptr = (htab->sglink->output_section->vma
				+ htab->sglink->output_offset);
	      break;

	    case DT_PPC64_OPD:
	      s = bfd_get_section_by_name (output_bfd, ".opd");
	      if (s != NULL)
		dyn.d_un.d_ptr = s->vma;
	      break;

	    case DT_PPC64_OPDSZ:
	      s = bfd_get_section_by_name (output_bfd, ".opd");
	      if (s != NULL)
		dyn.d_un.d_val = s->_raw_size;
	      break;

	    case DT_PLTGOT:
	      dyn.d_un.d_ptr = (htab->splt->output_section->vma
				+ htab->splt->output_offset);
	      break;

	    case DT_JMPREL:
	      dyn.d_un.d_ptr = (htab->srelplt->output_section->vma
				+ htab->srelplt->output_offset);
	      break;

	    case DT_PLTRELSZ:
	      dyn.d_un.d_val = htab->srelplt->_raw_size;
	      break;

	    case DT_RELASZ:
	      /* Don't count procedure linkage table relocs in the
		 overall reloc count.  */
	      if (htab->srelplt != NULL)
		dyn.d_un.d_val -= htab->srelplt->_raw_size;
	      break;
	    }

	  bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	}
    }

  if (htab->sgot != NULL && htab->sgot->_raw_size != 0)
    {
      /* Fill in the first entry in the global offset table.
	 We use it to hold the link-time TOCbase.  */
      bfd_put_64 (output_bfd,
		  elf_gp (output_bfd) + TOC_BASE_OFF,
		  htab->sgot->contents);

      /* Set .got entry size.  */
      elf_section_data (htab->sgot->output_section)->this_hdr.sh_entsize = 8;
    }

  if (htab->splt != NULL && htab->splt->_raw_size != 0)
    {
      /* Set .plt entry size.  */
      elf_section_data (htab->splt->output_section)->this_hdr.sh_entsize
	= PLT_ENTRY_SIZE;
    }

  return true;
}

#define TARGET_LITTLE_SYM	bfd_elf64_powerpcle_vec
#define TARGET_LITTLE_NAME	"elf64-powerpcle"
#define TARGET_BIG_SYM		bfd_elf64_powerpc_vec
#define TARGET_BIG_NAME		"elf64-powerpc"
#define ELF_ARCH		bfd_arch_powerpc
#define ELF_MACHINE_CODE	EM_PPC64
#define ELF_MAXPAGESIZE		0x10000
#define elf_info_to_howto	ppc64_elf_info_to_howto

#ifdef  EM_CYGNUS_POWERPC
#define ELF_MACHINE_ALT1	EM_CYGNUS_POWERPC
#endif

#ifdef EM_PPC_OLD
#define ELF_MACHINE_ALT2	EM_PPC_OLD
#endif

#define elf_backend_want_got_sym 0
#define elf_backend_want_plt_sym 0
#define elf_backend_plt_alignment 3
#define elf_backend_plt_not_loaded 1
#define elf_backend_got_symbol_offset 0
#define elf_backend_got_header_size 8
#define elf_backend_plt_header_size PLT_INITIAL_ENTRY_SIZE
#define elf_backend_can_gc_sections 1
#define elf_backend_can_refcount 1

#define bfd_elf64_bfd_reloc_type_lookup	      ppc64_elf_reloc_type_lookup
#define bfd_elf64_bfd_set_private_flags	      ppc64_elf_set_private_flags
#define bfd_elf64_bfd_merge_private_bfd_data  ppc64_elf_merge_private_bfd_data
#define bfd_elf64_bfd_link_hash_table_create  ppc64_elf_link_hash_table_create

#define elf_backend_section_from_shdr	      ppc64_elf_section_from_shdr
#define elf_backend_create_dynamic_sections   ppc64_elf_create_dynamic_sections
#define elf_backend_copy_indirect_symbol      ppc64_elf_copy_indirect_symbol
#define elf_backend_check_relocs	      ppc64_elf_check_relocs
#define elf_backend_gc_mark_hook	      ppc64_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	      ppc64_elf_gc_sweep_hook
#define elf_backend_adjust_dynamic_symbol     ppc64_elf_adjust_dynamic_symbol
#define elf_backend_hide_symbol		      ppc64_elf_hide_symbol
#define elf_backend_always_size_sections      ppc64_elf_func_desc_adjust
#define elf_backend_size_dynamic_sections     ppc64_elf_size_dynamic_sections
#define elf_backend_fake_sections	      ppc64_elf_fake_sections
#define elf_backend_relocate_section	      ppc64_elf_relocate_section
#define elf_backend_finish_dynamic_symbol     ppc64_elf_finish_dynamic_symbol
#define elf_backend_reloc_type_class	      ppc64_elf_reloc_type_class
#define elf_backend_finish_dynamic_sections   ppc64_elf_finish_dynamic_sections

#include "elf64-target.h"
