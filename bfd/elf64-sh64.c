/* SuperH SH64-specific support for 64-bit ELF
   Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define SH64_ELF64

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/sh.h"

/* Add a suffix for datalabel indirection symbols.  It must not match any
   other symbols; user symbols with or without version or other
   decoration.  It must only be used internally and not emitted by any
   means.  */
#define DATALABEL_SUFFIX " DL"

#define GOT_BIAS (-((long)-32768))

#define PLT_ENTRY_SIZE 64

/* Return size of a PLT entry.  */
#define elf_sh64_sizeof_plt(info) PLT_ENTRY_SIZE

/* Return offset of the PLT0 address in an absolute PLT entry.  */
#define elf_sh64_plt_plt0_offset(info) 32

/* Return offset of the linker in PLT0 entry.  */
#define elf_sh64_plt0_gotplt_offset(info) 0

/* Return offset of the trampoline in PLT entry */
#define elf_sh64_plt_temp_offset(info) 33 /* Add one because it's SHmedia.  */

/* Return offset of the symbol in PLT entry.  */
#define elf_sh64_plt_symbol_offset(info) 0

/* Return offset of the relocation in PLT entry.  */
#define elf_sh64_plt_reloc_offset(info) (info->shared ? 52 : 44)

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/libc.so.1"

/* The sh linker needs to keep track of the number of relocs that it
   decides to copy in check_relocs for each symbol.  This is so that
   it can discard PC relative relocs if it doesn't need them when
   linking with -Bsymbolic.  We store the information in a field
   extending the regular ELF linker hash table.  */

/* This structure keeps track of the number of PC relative relocs we
   have copied for a given symbol.  */

struct elf_sh64_pcrel_relocs_copied
{
  /* Next section.  */
  struct elf_sh64_pcrel_relocs_copied *next;
  /* A section in dynobj.  */
  asection *section;
  /* Number of relocs copied in this section.  */
  bfd_size_type count;
};

/* sh ELF linker hash entry.  */

struct elf_sh64_link_hash_entry
{
  struct elf_link_hash_entry root;

  bfd_vma datalabel_got_offset;

  /* Number of PC relative relocs copied for this symbol.  */
  struct elf_sh64_pcrel_relocs_copied *pcrel_relocs_copied;
};

/* sh ELF linker hash table.  */

struct elf_sh64_link_hash_table
{
  struct elf_link_hash_table root;
};

/* Traverse an sh ELF linker hash table.  */

#define sh64_elf64_link_hash_traverse(table, func, info)		\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (bfd_boolean (*) (struct elf_link_hash_entry *, void *)) (func), \
    (info)))

/* Get the sh ELF linker hash table from a link_info structure.  */

#define sh64_elf64_hash_table(p) \
  ((struct elf_sh64_link_hash_table *) ((p)->hash))

static bfd_reloc_status_type sh_elf64_ignore_reloc
  (bfd *, arelent *, asymbol *, void *, asection *, bfd *, char **);
static bfd_reloc_status_type sh_elf64_reloc
  (bfd *, arelent *, asymbol *, void *, asection *, bfd *, char **);

static reloc_howto_type sh_elf64_howto_table[] = {
  /* No relocation.  */
  HOWTO (R_SH_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 sh_elf64_ignore_reloc,	/* special_function */
	 "R_SH_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32 bit absolute relocation.  Setting partial_inplace to TRUE and
     src_mask to a non-zero value is similar to the COFF toolchain.  */
  HOWTO (R_SH_DIR32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 sh_elf64_reloc,		/* special_function */
	 "R_SH_DIR32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 32 bit PC relative relocation.  */
  HOWTO (R_SH_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf64_ignore_reloc,	/* special_function */
	 "R_SH_REL32",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* For 32-bit sh, this is R_SH_DIR8WPN.  */
  EMPTY_HOWTO (3),

  /* For 32-bit sh, this is R_SH_IND12W.  */
  EMPTY_HOWTO (4),

  /* For 32-bit sh, this is R_SH_DIR8WPL.  */
  EMPTY_HOWTO (5),

  /* For 32-bit sh, this is R_SH_DIR8WPZ.  */
  EMPTY_HOWTO (6),

  /* For 32-bit sh, this is R_SH_DIR8BP.  */
  EMPTY_HOWTO (7),

  /* For 32-bit sh, this is R_SH_DIR8W.  */
  EMPTY_HOWTO (8),

  /* For 32-bit sh, this is R_SH_DIR8L.  */
  EMPTY_HOWTO (9),

  EMPTY_HOWTO (10),
  EMPTY_HOWTO (11),
  EMPTY_HOWTO (12),
  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),
  EMPTY_HOWTO (16),
  EMPTY_HOWTO (17),
  EMPTY_HOWTO (18),
  EMPTY_HOWTO (19),
  EMPTY_HOWTO (20),
  EMPTY_HOWTO (21),
  EMPTY_HOWTO (22),
  EMPTY_HOWTO (23),
  EMPTY_HOWTO (24),

  /* The remaining relocs are a GNU extension used for relaxing.  The
     final pass of the linker never needs to do anything with any of
     these relocs.  Any required operations are handled by the
     relaxation code.  */

  /* A 16 bit switch table entry.  This is generated for an expression
     such as ``.word L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_SH_SWITCH16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf64_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 32 bit switch table entry.  This is generated for an expression
     such as ``.long L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_SH_SWITCH32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf64_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* For 32-bit sh, this is R_SH_USES.  */
  EMPTY_HOWTO (27),

  /* For 32-bit sh, this is R_SH_COUNT.  */
  EMPTY_HOWTO (28),

  /* For 32-bit sh, this is R_SH_ALIGN.  FIXME: For linker relaxation,
     this might be emitted.  When linker relaxation is implemented, we
     might want to use it.  */
  EMPTY_HOWTO (29),

  /* For 32-bit sh, this is R_SH_CODE.  FIXME: For linker relaxation,
     this might be emitted.  When linker relaxation is implemented, we
     might want to use it.  */
  EMPTY_HOWTO (30),

  /* For 32-bit sh, this is R_SH_DATA.  FIXME: For linker relaxation,
     this might be emitted.  When linker relaxation is implemented, we
     might want to use it.  */
  EMPTY_HOWTO (31),

  /* For 32-bit sh, this is R_SH_LABEL.  FIXME: For linker relaxation,
     this might be emitted.  When linker relaxation is implemented, we
     might want to use it.  */
  EMPTY_HOWTO (32),

  /* An 8 bit switch table entry.  This is generated for an expression
     such as ``.word L1 - L2''.  The offset holds the difference
     between the reloc address and L2.  */
  HOWTO (R_SH_SWITCH8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf64_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_SH_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_SH_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_SH_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_SH_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE),                /* pcrel_offset */

  /* For 32-bit sh, this is R_SH_LOOP_START.  */
  EMPTY_HOWTO (36),

  /* For 32-bit sh, this is R_SH_LOOP_END.  */
  EMPTY_HOWTO (37),

  EMPTY_HOWTO (38),
  EMPTY_HOWTO (39),
  EMPTY_HOWTO (40),
  EMPTY_HOWTO (41),
  EMPTY_HOWTO (42),
  EMPTY_HOWTO (43),
  EMPTY_HOWTO (44),

  /* Used in SHLLI.L and SHLRI.L.  */
  HOWTO (R_SH_DIR5U,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR5U",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in SHARI, SHLLI et al.  */
  HOWTO (R_SH_DIR6U,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR6U",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in BxxI, LDHI.L et al.  */
  HOWTO (R_SH_DIR6S,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR6S",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xfc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in ADDI, ANDI et al.  */
  HOWTO (R_SH_DIR10S,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 10,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR10S",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in LD.UW, ST.W et al.  */
  HOWTO (R_SH_DIR10SW,	/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR10SW",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in LD.L, FLD.S et al.  */
  HOWTO (R_SH_DIR10SL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR10SL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in FLD.D, FST.P et al.  */
  HOWTO (R_SH_DIR10SQ,	/* type */
	 3,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 13,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR10SQ",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  EMPTY_HOWTO (52),
  EMPTY_HOWTO (53),
  EMPTY_HOWTO (54),
  EMPTY_HOWTO (55),
  EMPTY_HOWTO (56),
  EMPTY_HOWTO (57),
  EMPTY_HOWTO (58),
  EMPTY_HOWTO (59),
  EMPTY_HOWTO (60),
  EMPTY_HOWTO (61),
  EMPTY_HOWTO (62),
  EMPTY_HOWTO (63),
  EMPTY_HOWTO (64),
  EMPTY_HOWTO (65),
  EMPTY_HOWTO (66),
  EMPTY_HOWTO (67),
  EMPTY_HOWTO (68),
  EMPTY_HOWTO (69),
  EMPTY_HOWTO (70),
  EMPTY_HOWTO (71),
  EMPTY_HOWTO (72),
  EMPTY_HOWTO (73),
  EMPTY_HOWTO (74),
  EMPTY_HOWTO (75),
  EMPTY_HOWTO (76),
  EMPTY_HOWTO (77),
  EMPTY_HOWTO (78),
  EMPTY_HOWTO (79),
  EMPTY_HOWTO (80),
  EMPTY_HOWTO (81),
  EMPTY_HOWTO (82),
  EMPTY_HOWTO (83),
  EMPTY_HOWTO (84),
  EMPTY_HOWTO (85),
  EMPTY_HOWTO (86),
  EMPTY_HOWTO (87),
  EMPTY_HOWTO (88),
  EMPTY_HOWTO (89),
  EMPTY_HOWTO (90),
  EMPTY_HOWTO (91),
  EMPTY_HOWTO (92),
  EMPTY_HOWTO (93),
  EMPTY_HOWTO (94),
  EMPTY_HOWTO (95),
  EMPTY_HOWTO (96),
  EMPTY_HOWTO (97),
  EMPTY_HOWTO (98),
  EMPTY_HOWTO (99),
  EMPTY_HOWTO (100),
  EMPTY_HOWTO (101),
  EMPTY_HOWTO (102),
  EMPTY_HOWTO (103),
  EMPTY_HOWTO (104),
  EMPTY_HOWTO (105),
  EMPTY_HOWTO (106),
  EMPTY_HOWTO (107),
  EMPTY_HOWTO (108),
  EMPTY_HOWTO (109),
  EMPTY_HOWTO (110),
  EMPTY_HOWTO (111),
  EMPTY_HOWTO (112),
  EMPTY_HOWTO (113),
  EMPTY_HOWTO (114),
  EMPTY_HOWTO (115),
  EMPTY_HOWTO (116),
  EMPTY_HOWTO (117),
  EMPTY_HOWTO (118),
  EMPTY_HOWTO (119),
  EMPTY_HOWTO (120),
  EMPTY_HOWTO (121),
  EMPTY_HOWTO (122),
  EMPTY_HOWTO (123),
  EMPTY_HOWTO (124),
  EMPTY_HOWTO (125),
  EMPTY_HOWTO (126),
  EMPTY_HOWTO (127),
  EMPTY_HOWTO (128),
  EMPTY_HOWTO (129),
  EMPTY_HOWTO (130),
  EMPTY_HOWTO (131),
  EMPTY_HOWTO (132),
  EMPTY_HOWTO (133),
  EMPTY_HOWTO (134),
  EMPTY_HOWTO (135),
  EMPTY_HOWTO (136),
  EMPTY_HOWTO (137),
  EMPTY_HOWTO (138),
  EMPTY_HOWTO (139),
  EMPTY_HOWTO (140),
  EMPTY_HOWTO (141),
  EMPTY_HOWTO (142),
  EMPTY_HOWTO (143),
  EMPTY_HOWTO (144),
  EMPTY_HOWTO (145),
  EMPTY_HOWTO (146),
  EMPTY_HOWTO (147),
  EMPTY_HOWTO (148),
  EMPTY_HOWTO (149),
  EMPTY_HOWTO (150),
  EMPTY_HOWTO (151),
  EMPTY_HOWTO (152),
  EMPTY_HOWTO (153),
  EMPTY_HOWTO (154),
  EMPTY_HOWTO (155),
  EMPTY_HOWTO (156),
  EMPTY_HOWTO (157),
  EMPTY_HOWTO (158),
  EMPTY_HOWTO (159),

  /* Relocs for dynamic linking for 32-bit SH would follow.  We don't have
     any dynamic linking support for 64-bit SH at present.  */

  EMPTY_HOWTO (160),
  EMPTY_HOWTO (161),
  EMPTY_HOWTO (162),
  EMPTY_HOWTO (163),
  EMPTY_HOWTO (164),
  EMPTY_HOWTO (165),
  EMPTY_HOWTO (166),
  EMPTY_HOWTO (167),
  EMPTY_HOWTO (168),

  /* Back to SH5 relocations.  */
  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOT_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT_LOW16",    	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 16) & 65536).  */
  HOWTO (R_SH_GOT_MEDLOW16,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT_MEDLOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 32) & 65536).  */
  HOWTO (R_SH_GOT_MEDHI16,	/* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT_MEDHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 48) & 65536).  */
  HOWTO (R_SH_GOT_HI16,		/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOTPLT_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT_LOW16",   /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 16) & 65536).  */
  HOWTO (R_SH_GOTPLT_MEDLOW16,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT_MEDLOW16", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 32) & 65536).  */
  HOWTO (R_SH_GOTPLT_MEDHI16,	/* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT_MEDHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 48) & 65536).  */
  HOWTO (R_SH_GOTPLT_HI16,	/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_PLT_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PLT_LOW16",    	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 16) & 65536).  */
  HOWTO (R_SH_PLT_MEDLOW16,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PLT_MEDLOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 32) & 65536).  */
  HOWTO (R_SH_PLT_MEDHI16,	/* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PLT_MEDHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 48) & 65536).  */
  HOWTO (R_SH_PLT_HI16,		/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PLT_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOTOFF_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTOFF_LOW16",   /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 16) & 65536).  */
  HOWTO (R_SH_GOTOFF_MEDLOW16,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTOFF_MEDLOW16", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 32) & 65536).  */
  HOWTO (R_SH_GOTOFF_MEDHI16,	/* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTOFF_MEDHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 48) & 65536).  */
  HOWTO (R_SH_GOTOFF_HI16,	/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTOFF_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOTPC_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPC_LOW16",    /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 16) & 65536).  */
  HOWTO (R_SH_GOTPC_MEDLOW16,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPC_MEDLOW16", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 32) & 65536).  */
  HOWTO (R_SH_GOTPC_MEDHI16,	/* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPC_MEDHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 48) & 65536).  */
  HOWTO (R_SH_GOTPC_HI16,	/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPC_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),		       	/* pcrel_offset */

  /* Used in LD.L, FLD.S et al.  */
  HOWTO (R_SH_GOT10BY4,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT10BY4",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in LD.L, FLD.S et al.  */
  HOWTO (R_SH_GOTPLT10BY4,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT10BY4",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in FLD.D, FST.P et al.  */
  HOWTO (R_SH_GOT10BY8,		/* type */
	 3,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 13,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT10BY8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in FLD.D, FST.P et al.  */
  HOWTO (R_SH_GOTPLT10BY8,	/* type */
	 3,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 13,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT10BY8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  HOWTO (R_SH_COPY64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_COPY64", 	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  HOWTO (R_SH_GLOB_DAT64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GLOB_DAT64", 	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  HOWTO (R_SH_JMP_SLOT64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_JMP_SLOT64", 	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  HOWTO (R_SH_RELATIVE64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_RELATIVE64", 	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  EMPTY_HOWTO (197),
  EMPTY_HOWTO (198),
  EMPTY_HOWTO (199),
  EMPTY_HOWTO (200),
  EMPTY_HOWTO (201),
  EMPTY_HOWTO (202),
  EMPTY_HOWTO (203),
  EMPTY_HOWTO (204),
  EMPTY_HOWTO (205),
  EMPTY_HOWTO (206),
  EMPTY_HOWTO (207),
  EMPTY_HOWTO (208),
  EMPTY_HOWTO (209),
  EMPTY_HOWTO (210),
  EMPTY_HOWTO (211),
  EMPTY_HOWTO (212),
  EMPTY_HOWTO (213),
  EMPTY_HOWTO (214),
  EMPTY_HOWTO (215),
  EMPTY_HOWTO (216),
  EMPTY_HOWTO (217),
  EMPTY_HOWTO (218),
  EMPTY_HOWTO (219),
  EMPTY_HOWTO (220),
  EMPTY_HOWTO (221),
  EMPTY_HOWTO (222),
  EMPTY_HOWTO (223),
  EMPTY_HOWTO (224),
  EMPTY_HOWTO (225),
  EMPTY_HOWTO (226),
  EMPTY_HOWTO (227),
  EMPTY_HOWTO (228),
  EMPTY_HOWTO (229),
  EMPTY_HOWTO (230),
  EMPTY_HOWTO (231),
  EMPTY_HOWTO (232),
  EMPTY_HOWTO (233),
  EMPTY_HOWTO (234),
  EMPTY_HOWTO (235),
  EMPTY_HOWTO (236),
  EMPTY_HOWTO (237),
  EMPTY_HOWTO (238),
  EMPTY_HOWTO (239),
  EMPTY_HOWTO (240),
  EMPTY_HOWTO (241),

  /* Relocations for SHmedia code.  None of these are partial_inplace or
     use the field being relocated.  */

  /* The assembler will generate this reloc before a block of SHmedia
     instructions.  A section should be processed as assuming it contains
     data, unless this reloc is seen.  Note that a block of SHcompact
     instructions are instead preceded by R_SH_CODE.
     This is currently not implemented, but should be used for SHmedia
     linker relaxation.  */
  HOWTO (R_SH_SHMEDIA_CODE,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf64_ignore_reloc,	/* special_function */
	 "R_SH_SHMEDIA_CODE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The assembler will generate this reloc at a PTA or PTB instruction,
     and the linker checks the right type of target, or changes a PTA to a
     PTB, if the original insn was PT.  */
  HOWTO (R_SH_PT_16,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 18,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PT_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Used in unexpanded MOVI.  */
  HOWTO (R_SH_IMMS16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMMS16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in SHORI.  */
  HOWTO (R_SH_IMMU16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMMU16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_IMM_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_LOW16",    	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI ((x - $) & 65536).  */
  HOWTO (R_SH_IMM_LOW16_PCREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_LOW16_PCREL", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 16) & 65536).  */
  HOWTO (R_SH_IMM_MEDLOW16,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_MEDLOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (((x - $) >> 16) & 65536).  */
  HOWTO (R_SH_IMM_MEDLOW16_PCREL, /* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_MEDLOW16_PCREL", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 32) & 65536).  */
  HOWTO (R_SH_IMM_MEDHI16,	/* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_MEDHI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (((x - $) >> 32) & 65536).  */
  HOWTO (R_SH_IMM_MEDHI16_PCREL, /* type */
	 32,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_MEDHI16_PCREL", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Used in MOVI and SHORI ((x >> 48) & 65536).  */
  HOWTO (R_SH_IMM_HI16,		/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_HI16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* Used in MOVI and SHORI (((x - $) >> 48) & 65536).  */
  HOWTO (R_SH_IMM_HI16_PCREL,	/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_HI16_PCREL", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* For the .uaquad pseudo.  */
  HOWTO (R_SH_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_64", 		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),	       	/* pcrel_offset */

  /* For the .uaquad pseudo, (x - $).  */
  HOWTO (R_SH_64_PCREL,		/* type */
	 48,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_64_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 TRUE),			/* pcrel_offset */

};

/* This function is used for relocs which are only used for relaxing,
   which the linker should otherwise ignore.  */

static bfd_reloc_status_type
sh_elf64_ignore_reloc (bfd *abfd ATTRIBUTE_UNUSED, arelent *reloc_entry,
		       asymbol *symbol ATTRIBUTE_UNUSED,
		       void *data ATTRIBUTE_UNUSED, asection *input_section,
		       bfd *output_bfd,
		       char **error_message ATTRIBUTE_UNUSED)
{
  if (output_bfd != NULL)
    reloc_entry->address += input_section->output_offset;
  return bfd_reloc_ok;
}

/* This function is used for normal relocs.  This used to be like the COFF
   function, and is almost certainly incorrect for other ELF targets.

   See sh_elf_reloc in elf32-sh.c for the original.  */

static bfd_reloc_status_type
sh_elf64_reloc (bfd *abfd, arelent *reloc_entry, asymbol *symbol_in,
		void *data, asection *input_section, bfd *output_bfd,
		char **error_message ATTRIBUTE_UNUSED)
{
  unsigned long insn;
  bfd_vma sym_value;
  enum elf_sh_reloc_type r_type;
  bfd_vma addr = reloc_entry->address;
  bfd_byte *hit_data = addr + (bfd_byte *) data;

  r_type = (enum elf_sh_reloc_type) reloc_entry->howto->type;

  if (output_bfd != NULL)
    {
      /* Partial linking--do nothing.  */
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (symbol_in != NULL
      && bfd_is_und_section (symbol_in->section))
    return bfd_reloc_undefined;

  if (bfd_is_com_section (symbol_in->section))
    sym_value = 0;
  else
    sym_value = (symbol_in->value +
		 symbol_in->section->output_section->vma +
		 symbol_in->section->output_offset);

  switch (r_type)
    {
    case R_SH_DIR32:
      insn = bfd_get_32 (abfd, hit_data);
      insn += sym_value + reloc_entry->addend;
      bfd_put_32 (abfd, insn, hit_data);
      break;

    default:
      abort ();
      break;
    }

  return bfd_reloc_ok;
}

/* This structure is used to map BFD reloc codes to SH ELF relocs.  */

struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

/* An array mapping BFD reloc codes to SH ELF relocs.  */

static const struct elf_reloc_map sh64_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SH_NONE },
  { BFD_RELOC_32, R_SH_DIR32 },
  { BFD_RELOC_CTOR, R_SH_DIR32 },
  { BFD_RELOC_32_PCREL, R_SH_REL32 },
  { BFD_RELOC_8_PCREL, R_SH_SWITCH8 },
  { BFD_RELOC_SH_SWITCH16, R_SH_SWITCH16 },
  { BFD_RELOC_SH_SWITCH32, R_SH_SWITCH32 },
  { BFD_RELOC_VTABLE_INHERIT, R_SH_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_SH_GNU_VTENTRY },
  { BFD_RELOC_SH_GOT_LOW16, R_SH_GOT_LOW16 },
  { BFD_RELOC_SH_GOT_MEDLOW16, R_SH_GOT_MEDLOW16 },
  { BFD_RELOC_SH_GOT_MEDHI16, R_SH_GOT_MEDHI16 },
  { BFD_RELOC_SH_GOT_HI16, R_SH_GOT_HI16 },
  { BFD_RELOC_SH_GOTPLT_LOW16, R_SH_GOTPLT_LOW16 },
  { BFD_RELOC_SH_GOTPLT_MEDLOW16, R_SH_GOTPLT_MEDLOW16 },
  { BFD_RELOC_SH_GOTPLT_MEDHI16, R_SH_GOTPLT_MEDHI16 },
  { BFD_RELOC_SH_GOTPLT_HI16, R_SH_GOTPLT_HI16 },
  { BFD_RELOC_SH_PLT_LOW16, R_SH_PLT_LOW16 },
  { BFD_RELOC_SH_PLT_MEDLOW16, R_SH_PLT_MEDLOW16 },
  { BFD_RELOC_SH_PLT_MEDHI16, R_SH_PLT_MEDHI16 },
  { BFD_RELOC_SH_PLT_HI16, R_SH_PLT_HI16 },
  { BFD_RELOC_SH_GOTOFF_LOW16, R_SH_GOTOFF_LOW16 },
  { BFD_RELOC_SH_GOTOFF_MEDLOW16, R_SH_GOTOFF_MEDLOW16 },
  { BFD_RELOC_SH_GOTOFF_MEDHI16, R_SH_GOTOFF_MEDHI16 },
  { BFD_RELOC_SH_GOTOFF_HI16, R_SH_GOTOFF_HI16 },
  { BFD_RELOC_SH_GOTPC_LOW16, R_SH_GOTPC_LOW16 },
  { BFD_RELOC_SH_GOTPC_MEDLOW16, R_SH_GOTPC_MEDLOW16 },
  { BFD_RELOC_SH_GOTPC_MEDHI16, R_SH_GOTPC_MEDHI16 },
  { BFD_RELOC_SH_GOTPC_HI16, R_SH_GOTPC_HI16 },
  { BFD_RELOC_SH_COPY64, R_SH_COPY64 },
  { BFD_RELOC_SH_GLOB_DAT64, R_SH_GLOB_DAT64 },
  { BFD_RELOC_SH_JMP_SLOT64, R_SH_JMP_SLOT64 },
  { BFD_RELOC_SH_RELATIVE64, R_SH_RELATIVE64 },
  { BFD_RELOC_SH_GOT10BY4, R_SH_GOT10BY4 },
  { BFD_RELOC_SH_GOT10BY8, R_SH_GOT10BY8 },
  { BFD_RELOC_SH_GOTPLT10BY4, R_SH_GOTPLT10BY4 },
  { BFD_RELOC_SH_GOTPLT10BY8, R_SH_GOTPLT10BY8 },
  { BFD_RELOC_SH_PT_16, R_SH_PT_16 },
  { BFD_RELOC_SH_SHMEDIA_CODE, R_SH_SHMEDIA_CODE },
  { BFD_RELOC_SH_IMMU5, R_SH_DIR5U },
  { BFD_RELOC_SH_IMMS6, R_SH_DIR6S },
  { BFD_RELOC_SH_IMMU6, R_SH_DIR6U },
  { BFD_RELOC_SH_IMMS10, R_SH_DIR10S },
  { BFD_RELOC_SH_IMMS10BY2, R_SH_DIR10SW },
  { BFD_RELOC_SH_IMMS10BY4, R_SH_DIR10SL },
  { BFD_RELOC_SH_IMMS10BY8, R_SH_DIR10SQ },
  { BFD_RELOC_SH_IMMS16, R_SH_IMMS16 },
  { BFD_RELOC_SH_IMMU16, R_SH_IMMU16 },
  { BFD_RELOC_SH_IMM_LOW16, R_SH_IMM_LOW16 },
  { BFD_RELOC_SH_IMM_LOW16_PCREL, R_SH_IMM_LOW16_PCREL },
  { BFD_RELOC_SH_IMM_MEDLOW16, R_SH_IMM_MEDLOW16 },
  { BFD_RELOC_SH_IMM_MEDLOW16_PCREL, R_SH_IMM_MEDLOW16_PCREL },
  { BFD_RELOC_SH_IMM_MEDHI16, R_SH_IMM_MEDHI16 },
  { BFD_RELOC_SH_IMM_MEDHI16_PCREL, R_SH_IMM_MEDHI16_PCREL },
  { BFD_RELOC_SH_IMM_HI16, R_SH_IMM_HI16 },
  { BFD_RELOC_SH_IMM_HI16_PCREL, R_SH_IMM_HI16_PCREL },
  { BFD_RELOC_64, R_SH_64 },
  { BFD_RELOC_64_PCREL, R_SH_64_PCREL },
};

/* Given a BFD reloc code, return the howto structure for the
   corresponding SH ELf reloc.  */

static reloc_howto_type *
sh_elf64_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < sizeof (sh64_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (sh64_reloc_map[i].bfd_reloc_val == code)
	return &sh_elf64_howto_table[(int) sh64_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Given an ELF reloc, fill in the howto field of a relent.

   See sh_elf_info_to_howto in elf32-sh.c for the original.  */

static void
sh_elf64_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED, arelent *cache_ptr,
			Elf_Internal_Rela *dst)
{
  unsigned int r;

  r = ELF64_R_TYPE (dst->r_info);

  BFD_ASSERT (r <= (unsigned int) R_SH_64_PCREL);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC || r > R_SH_LAST_INVALID_RELOC);
  BFD_ASSERT (r < R_SH_DIR8WPN || r > R_SH_LAST_INVALID_RELOC_2);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC_3 || r > R_SH_GOTPLT32);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC_4 || r > R_SH_LAST_INVALID_RELOC_4);

  cache_ptr->howto = &sh_elf64_howto_table[r];
}

/* Relocate an SH ELF section.

   See sh_elf_info_to_howto in elf32-sh.c for the original.  */

static bfd_boolean
sh_elf64_relocate_section (bfd *output_bfd ATTRIBUTE_UNUSED,
			   struct bfd_link_info *info, bfd *input_bfd,
			   asection *input_section, bfd_byte *contents,
			   Elf_Internal_Rela *relocs,
			   Elf_Internal_Sym *local_syms,
			   asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  asection *sgot;
  asection *sgotplt;
  asection *splt;
  asection *sreloc;
  bfd_vma disp, dropped;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  dynobj = elf_hash_table (info)->dynobj;
  local_got_offsets = elf_local_got_offsets (input_bfd);

  sgot = NULL;
  sgotplt = NULL;
  splt = NULL;
  sreloc = NULL;

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_vma addend = (bfd_vma)0;
      bfd_reloc_status_type r;
      int seen_stt_datalabel = 0;

      r_symndx = ELF64_R_SYM (rel->r_info);

      r_type = ELF64_R_TYPE (rel->r_info);

      if (r_type == (int) R_SH_NONE)
	continue;

      if (r_type < 0
	  || r_type > R_SH_64_PCREL
	  || (r_type >= (int) R_SH_FIRST_INVALID_RELOC
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC)
	  || (r_type >= (int) R_SH_DIR8WPN
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC)
	  || (r_type >= (int) R_SH_GNU_VTINHERIT
	      && r_type <= (int) R_SH_PSHL)
	  || (r_type >= (int) R_SH_FIRST_INVALID_RELOC_2
	      && r_type <= R_SH_GOTPLT32)
	  || (r_type >= (int) R_SH_FIRST_INVALID_RELOC_4
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC_4))
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      howto = sh_elf64_howto_table + r_type;

      /* This is a final link.  */
      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = ((sec->output_section->vma
			 + sec->output_offset
			 + sym->st_value)
			| ((sym->st_other & STO_SH5_ISA32) != 0));

	  /* A local symbol never has STO_SH5_ISA32, so we don't need
	     datalabel processing here.  Make sure this does not change
	     without notice.  */
	  if ((sym->st_other & STO_SH5_ISA32) != 0)
	    ((*info->callbacks->reloc_dangerous)
	     (info,
	      _("Unexpected STO_SH5_ISA32 on local symbol is not handled"),
	      input_bfd, input_section, rel->r_offset));

	  if (info->relocatable)
	    {
	      /* This is a relocatable link.  We don't have to change
		 anything, unless the reloc is against a section symbol,
		 in which case we have to adjust according to where the
		 section symbol winds up in the output section.  */
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		goto final_link_relocate;

	      continue;
	    }
	  else if (! howto->partial_inplace)
	    {
	      relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	      relocation |= ((sym->st_other & STO_SH5_ISA32) != 0);
	    }
	  else if ((sec->flags & SEC_MERGE)
		   && ELF_ST_TYPE (sym->st_info) == STT_SECTION)
	    {
	      asection *msec;

	      if (howto->rightshift || howto->src_mask != 0xffffffff)
		{
		  (*_bfd_error_handler)
		    (_("%B(%A+0x%lx): %s relocation against SEC_MERGE section"),
		     input_bfd, input_section,
		     (long) rel->r_offset, howto->name);
		  return FALSE;
		}

              addend = bfd_get_32 (input_bfd, contents + rel->r_offset);
              msec = sec;
              addend =
		_bfd_elf_rel_local_sym (output_bfd, sym, &msec, addend)
		- relocation;
	      addend += msec->output_section->vma + msec->output_offset;
	      bfd_put_32 (input_bfd, addend, contents + rel->r_offset);
	      addend = 0;
	    }
	}
      else
	{
	  /* ??? Could we use the RELOC_FOR_GLOBAL_SYMBOL macro here ?  */

	  /* Section symbols are never (?) placed in the hash table, so
	     we can just ignore hash relocations when creating a
	     relocatable object file.  */
	  if (info->relocatable)
	    continue;

	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    {
	      /* If the reference passes a symbol marked with
		 STT_DATALABEL, then any STO_SH5_ISA32 on the final value
		 doesn't count.  */
	      seen_stt_datalabel |= h->type == STT_DATALABEL;
	      h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      sec = h->root.u.def.section;
	      /* In these cases, we don't need the relocation value.
		 We check specially because in some obscure cases
		 sec->output_section will be NULL.  */
	      if (r_type == R_SH_GOTPC_LOW16
		  || r_type == R_SH_GOTPC_MEDLOW16
		  || r_type == R_SH_GOTPC_MEDHI16
		  || r_type == R_SH_GOTPC_HI16
		  || ((r_type == R_SH_PLT_LOW16
		       || r_type == R_SH_PLT_MEDLOW16
		       || r_type == R_SH_PLT_MEDHI16
		       || r_type == R_SH_PLT_HI16)
		      && h->plt.offset != (bfd_vma) -1)
		  || ((r_type == R_SH_GOT_LOW16
		       || r_type == R_SH_GOT_MEDLOW16
		       || r_type == R_SH_GOT_MEDHI16
		       || r_type == R_SH_GOT_HI16)
		      && elf_hash_table (info)->dynamic_sections_created
		      && (! info->shared
			  || (! info->symbolic && h->dynindx != -1)
			  || !h->def_regular))
		  /* The cases above are those in which relocation is
		     overwritten in the switch block below.  The cases
		     below are those in which we must defer relocation
		     to run-time, because we can't resolve absolute
		     addresses when creating a shared library.  */
		  || (info->shared
		      && ((! info->symbolic && h->dynindx != -1)
			  || !h->def_regular)
		      && ((r_type == R_SH_64
			   && !(ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
				|| ELF_ST_VISIBILITY (h->other) == STV_HIDDEN))
			  || r_type == R_SH_64_PCREL)
		      && ((input_section->flags & SEC_ALLOC) != 0
			  /* DWARF will emit R_SH_DIR32 relocations in its
			     sections against symbols defined externally
			     in shared libraries.  We can't do anything
			     with them here.  */
			  || (input_section->flags & SEC_DEBUGGING) != 0))
		  /* Dynamic relocs are not propagated for SEC_DEBUGGING
		     sections because such sections are not SEC_ALLOC and
		     thus ld.so will not process them.  */
		  || (sec->output_section == NULL
		      && ((input_section->flags & SEC_DEBUGGING) != 0
			  && h->def_dynamic)))
		relocation = 0;
	      else if (sec->output_section == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%B(%A+0x%lx): unresolvable %s relocation against symbol `%s'"),
		     input_bfd,
		     input_section,
		     (long) rel->r_offset,
		     howto->name,
		     h->root.root.string);
		  relocation = 0;
		}
	      else
		relocation = ((h->root.u.def.value
			       + sec->output_section->vma
			       + sec->output_offset)
			      /* A STO_SH5_ISA32 causes a "bitor 1" to the
				 symbol value, unless we've seen
				 STT_DATALABEL on the way to it.  */
			      | ((h->other & STO_SH5_ISA32) != 0
				 && ! seen_stt_datalabel));
	    }
	  else if (h->root.type == bfd_link_hash_undefweak)
	    relocation = 0;
	  else if (info->unresolved_syms_in_objects == RM_IGNORE
		   && ELF_ST_VISIBILITY (h->other) == STV_DEFAULT)
	    relocation = 0;
	  else
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd,
		      input_section, rel->r_offset,
		      (info->unresolved_syms_in_objects == RM_GENERATE_ERROR
		       || ELF_ST_VISIBILITY (h->other)))))
		return FALSE;
	      relocation = 0;
	    }
	}

      disp = (relocation
	      - input_section->output_section->vma
	      - input_section->output_offset
	      - rel->r_offset);
      dropped = 0;
      switch ((int)r_type)
	{
	case R_SH_PT_16:     dropped = disp & 2; break;
	case R_SH_DIR10SW: dropped = disp & 1; break;
	case R_SH_DIR10SL: dropped = disp & 3; break;
	case R_SH_DIR10SQ: dropped = disp & 7; break;
	}
      if (dropped != 0)
	{
	  (*_bfd_error_handler)
	    (_("%s: error: unaligned relocation type %d at %08x reloc %08x\n"),
	     bfd_get_filename (input_bfd), (int)r_type, (unsigned)rel->r_offset, (unsigned)relocation);
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
      switch ((int)r_type)
	{
	case R_SH_64:
	case R_SH_64_PCREL:
	  if (info->shared
	      && (input_section->flags & SEC_ALLOC) != 0
	      && (r_type != R_SH_64_PCREL
		  || (h != NULL
		      && h->dynindx != -1
		      && (! info->symbolic
			  || !h->def_regular))))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      bfd_boolean skip, relocate;

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
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (input_bfd,
							       input_section),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  BFD_ASSERT (sreloc != NULL);
		}

	      skip = FALSE;
	      relocate = FALSE;

	      outrel.r_offset
		= _bfd_elf_section_offset (output_bfd, info,
					   input_section, rel->r_offset);

	      if (outrel.r_offset == (bfd_vma) -1)
		skip = TRUE;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = TRUE, relocate = TRUE;

	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      else if (r_type == R_SH_64_PCREL)
		{
		  BFD_ASSERT (h != NULL && h->dynindx != -1);
		  outrel.r_info = ELF64_R_INFO (h->dynindx, R_SH_64_PCREL);
		  outrel.r_addend = rel->r_addend;
		}
	      else
		{
		  /* h->dynindx may be -1 if this symbol was marked to
		     become local.  */
		  if (h == NULL
		      || ((info->symbolic || h->dynindx == -1)
			  && h->def_regular))
		    {
		      relocate = TRUE;
		      outrel.r_info = ELF64_R_INFO (0, R_SH_RELATIVE64);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		  else
		    {
		      BFD_ASSERT (h->dynindx != -1);
		      outrel.r_info = ELF64_R_INFO (h->dynindx, R_SH_64);
		      outrel.r_addend = relocation + rel->r_addend;
		    }
		}

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf64_External_Rela);
	      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);

	      /* If this reloc is against an external symbol, we do
		 not want to fiddle with the addend.  Otherwise, we
		 need to include the symbol value so that it becomes
		 an addend for the dynamic reloc.  */
	      if (! relocate)
		continue;
	    }
	  else if (r_type == R_SH_64)
	    addend = rel->r_addend;
	  goto final_link_relocate;

	case R_SH_GOTPLT_LOW16:
	case R_SH_GOTPLT_MEDLOW16:
	case R_SH_GOTPLT_MEDHI16:
	case R_SH_GOTPLT_HI16:
	case R_SH_GOTPLT10BY4:
	case R_SH_GOTPLT10BY8:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  if (h == NULL
	      || ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
	      || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN
	      || ! info->shared
	      || info->symbolic
	      || h->dynindx == -1
	      || h->plt.offset == (bfd_vma) -1
	      || h->got.offset != (bfd_vma) -1)
	    goto force_got;

	  /* Relocation is to the entry for this symbol in the global
	     offset table extension for the procedure linkage table.  */
	  if (sgotplt == NULL)
	    {
	      sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
	      BFD_ASSERT (sgotplt != NULL);
	    }

	  relocation = (sgotplt->output_offset
			+ ((h->plt.offset / elf_sh64_sizeof_plt (info)
			    - 1 + 3) * 8));

	  relocation -= GOT_BIAS;

	  goto final_link_relocate;

	force_got:
	case R_SH_GOT_LOW16:
	case R_SH_GOT_MEDLOW16:
	case R_SH_GOT_MEDHI16:
	case R_SH_GOT_HI16:
	case R_SH_GOT10BY4:
	case R_SH_GOT10BY8:
	  /* Relocation is to the entry for this symbol in the global
	     offset table.  */
	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (h != NULL)
	    {
	      bfd_vma off;

	      off = h->got.offset;
	      if (seen_stt_datalabel)
		{
		  struct elf_sh64_link_hash_entry *hsh;

		  hsh = (struct elf_sh64_link_hash_entry *)h;
		  off = hsh->datalabel_got_offset;
		}
	      BFD_ASSERT (off != (bfd_vma) -1);

	      if (! elf_hash_table (info)->dynamic_sections_created
		  || (info->shared
		      && (info->symbolic || h->dynindx == -1
			  || ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
			  || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN)
		      && h->def_regular))
		{
		  /* This is actually a static link, or it is a
		     -Bsymbolic link and the symbol is defined
		     locally, or the symbol was forced to be local
		     because of a version file.  We must initialize
		     this entry in the global offset table.  Since the
		     offset must always be a multiple of 4, we use the
		     least significant bit to record whether we have
		     initialized it already.

		     When doing a dynamic link, we create a .rela.got
		     relocation entry to initialize the value.  This
		     is done in the finish_dynamic_symbol routine.  */
		  if ((off & 1) != 0)
		    off &= ~1;
		  else
		    {
		      bfd_put_64 (output_bfd, relocation,
				  sgot->contents + off);
		      if (seen_stt_datalabel)
			{
			  struct elf_sh64_link_hash_entry *hsh;

			  hsh = (struct elf_sh64_link_hash_entry *)h;
			  hsh->datalabel_got_offset |= 1;
			}
		      else
			h->got.offset |= 1;
		    }
		}

	      relocation = sgot->output_offset + off;
	    }
	  else
	    {
	      bfd_vma off;

	      if (rel->r_addend)
		{
		  BFD_ASSERT (local_got_offsets != NULL
			      && (local_got_offsets[symtab_hdr->sh_info
						    + r_symndx]
				  != (bfd_vma) -1));

		  off = local_got_offsets[symtab_hdr->sh_info
					  + r_symndx];
		}
	      else
		{
		  BFD_ASSERT (local_got_offsets != NULL
			      && local_got_offsets[r_symndx] != (bfd_vma) -1);

		  off = local_got_offsets[r_symndx];
		}

	      /* The offset must always be a multiple of 8.  We use
		 the least significant bit to record whether we have
		 already generated the necessary reloc.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  bfd_put_64 (output_bfd, relocation, sgot->contents + off);

		  if (info->shared)
		    {
		      asection *s;
		      Elf_Internal_Rela outrel;
		      bfd_byte *loc;

		      s = bfd_get_section_by_name (dynobj, ".rela.got");
		      BFD_ASSERT (s != NULL);

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);
		      outrel.r_info = ELF64_R_INFO (0, R_SH_RELATIVE64);
		      outrel.r_addend = relocation;
		      loc = s->contents;
		      loc += s->reloc_count++ * sizeof (Elf64_External_Rela);
		      bfd_elf64_swap_reloca_out (output_bfd, &outrel, loc);
		    }

		  if (rel->r_addend)
		    local_got_offsets[symtab_hdr->sh_info + r_symndx] |= 1;
		  else
		    local_got_offsets[r_symndx] |= 1;
		}

	      relocation = sgot->output_offset + off;
	    }

 	  relocation -= GOT_BIAS;

	  goto final_link_relocate;

	case R_SH_GOTOFF_LOW16:
	case R_SH_GOTOFF_MEDLOW16:
	case R_SH_GOTOFF_MEDHI16:
	case R_SH_GOTOFF_HI16:
	  /* Relocation is relative to the start of the global offset
	     table.  */

	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  /* Note that sgot->output_offset is not involved in this
	     calculation.  We always want the start of .got.  If we
	     defined _GLOBAL_OFFSET_TABLE in a different way, as is
	     permitted by the ABI, we might have to change this
	     calculation.  */
	  relocation -= sgot->output_section->vma;

	  relocation -= GOT_BIAS;

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_GOTPC_LOW16:
	case R_SH_GOTPC_MEDLOW16:
	case R_SH_GOTPC_MEDHI16:
	case R_SH_GOTPC_HI16:
	  /* Use global offset table as symbol value.  */

	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  relocation = sgot->output_section->vma;

	  relocation += GOT_BIAS;

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_PLT_LOW16:
	case R_SH_PLT_MEDLOW16:
	case R_SH_PLT_MEDHI16:
	case R_SH_PLT_HI16:
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLT reloc against a local symbol directly,
	     without using the procedure linkage table.  */
	  if (h == NULL)
	    goto final_link_relocate;

	  if (ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
	      || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN)
	    goto final_link_relocate;

	  if (h->plt.offset == (bfd_vma) -1)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      goto final_link_relocate;
	    }

	  if (splt == NULL)
	    {
	      splt = bfd_get_section_by_name (dynobj, ".plt");
	      BFD_ASSERT (splt != NULL);
	    }

	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt.offset);
	  relocation++;

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_DIR32:
	case R_SH_SHMEDIA_CODE:
	case R_SH_PT_16:
	case R_SH_DIR5U:
	case R_SH_DIR6S:
	case R_SH_DIR6U:
	case R_SH_DIR10S:
	case R_SH_DIR10SW:
	case R_SH_DIR10SL:
	case R_SH_DIR10SQ:
	case R_SH_IMMS16:
	case R_SH_IMMU16:
	case R_SH_IMM_LOW16:
	case R_SH_IMM_LOW16_PCREL:
	case R_SH_IMM_MEDLOW16:
	case R_SH_IMM_MEDLOW16_PCREL:
	case R_SH_IMM_MEDHI16:
	case R_SH_IMM_MEDHI16_PCREL:
	case R_SH_IMM_HI16:
	case R_SH_IMM_HI16_PCREL:
	  addend = rel->r_addend;
	  /* Fall through.  */
	case R_SH_REL32:
	final_link_relocate:
	  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents, rel->r_offset,
					relocation, addend);
	  break;

	default:
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;

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
		  name = NULL;
		else
		  {
		    name = (bfd_elf_string_from_elf_section
			    (input_bfd, symtab_hdr->sh_link, sym->st_name));
		    if (name == NULL)
		      return FALSE;
		    if (*name == '\0')
		      name = bfd_section_name (input_bfd, sec);
		  }
		if (! ((*info->callbacks->reloc_overflow)
		       (info, (h ? &h->root : NULL), name, howto->name,
			(bfd_vma) 0, input_bfd, input_section,
			rel->r_offset)))
		  return FALSE;
	      }
	      break;
	    }
	}
    }

  return TRUE;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   that uses sh_elf64_relocate_section.

   See sh_elf_relocate_section in elf32-sh.c for the original.  */

static bfd_byte *
sh_elf64_get_relocated_section_contents (bfd *output_bfd,
					 struct bfd_link_info *link_info,
					 struct bfd_link_order *link_order,
					 bfd_byte *data,
					 bfd_boolean relocatable,
					 asymbol **symbols)
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocatable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocatable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  input_section->size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isymp;
      Elf_Internal_Sym *isymend;
      asection **secpp;

      /* Read this BFD's local symbols.  */
      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      internal_relocs = (_bfd_elf_link_read_relocs
			 (input_bfd, input_section, NULL,
			  (Elf_Internal_Rela *) NULL, FALSE));
      if (internal_relocs == NULL)
	goto error_return;

      sections = (asection **) bfd_malloc (symtab_hdr->sh_info
					   * sizeof (asection *));
      if (sections == NULL && symtab_hdr->sh_info > 0)
	goto error_return;

      secpp = sections;
      isymend = isymbuf + symtab_hdr->sh_info;
      for (isymp = isymbuf; isymp < isymend; ++isymp, ++secpp)
	{
	  asection *isec;

	  if (isymp->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isymp->st_shndx > 0 && isymp->st_shndx < SHN_LORESERVE)
	    isec = bfd_section_from_elf_index (input_bfd, isymp->st_shndx);
	  else if (isymp->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isymp->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else
	    {
	      /* Who knows?  */
	      isec = NULL;
	    }

	  *secpp = isec;
	}

      if (! sh_elf64_relocate_section (output_bfd, link_info, input_bfd,
				       input_section, data, internal_relocs,
				       isymbuf, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      if (internal_relocs != elf_section_data (input_section)->relocs)
	free (internal_relocs);
      if (isymbuf != NULL
	  && (unsigned char *) isymbuf != symtab_hdr->contents)
	free (isymbuf);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (internal_relocs != NULL
      && internal_relocs != elf_section_data (input_section)->relocs)
    free (internal_relocs);
  if (isymbuf != NULL
      && (unsigned char *) isymbuf != symtab_hdr->contents)
    free (isymbuf);
  return NULL;
}

/* Set the SHF_SH5_ISA32 flag for ISA SHmedia code sections.  */

static bfd_boolean
sh64_elf64_fake_sections (bfd *output_bfd ATTRIBUTE_UNUSED,
			  Elf_Internal_Shdr *elf_section_hdr,
			  asection *asect)
{
  /* Code sections can only contain SH64 code, so mark them as such.  */
  if (bfd_get_section_flags (output_bfd, asect) & SEC_CODE)
    elf_section_hdr->sh_flags |= SHF_SH5_ISA32;

  return TRUE;
}

static bfd_boolean
sh_elf64_set_mach_from_flags (bfd *abfd)
{
  flagword flags = elf_elfheader (abfd)->e_flags;

  switch (flags & EF_SH_MACH_MASK)
    {
    case EF_SH5:
      /* Just one, but keep the switch construct to make additions easy.  */
      bfd_default_set_arch_mach (abfd, bfd_arch_sh, bfd_mach_sh5);
      break;

    default:
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }
  return TRUE;
}

/* Function to keep SH64 specific file flags.

   See sh64_elf_set_private_flags in elf32-sh64.c for the original.  */

static bfd_boolean
sh_elf64_set_private_flags (bfd *abfd, flagword flags)
{
  BFD_ASSERT (! elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return sh_elf64_set_mach_from_flags (abfd);
}

/* Copy the SHF_SH5_ISA32 attribute that we keep on all sections with
   code, to keep attributes the same as for SHmedia in 32-bit ELF.  */

static bfd_boolean
sh_elf64_copy_private_data_internal (bfd *ibfd, bfd *obfd)
{
  Elf_Internal_Shdr **o_shdrp;
  asection *isec;
  asection *osec;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  o_shdrp = elf_elfsections (obfd);
  for (osec = obfd->sections; osec; osec = osec->next)
    {
      int oIndex = ((struct bfd_elf_section_data *) elf_section_data (osec))->this_idx;
      for (isec = ibfd->sections; isec; isec = isec->next)
	{
	  if (strcmp (osec->name, isec->name) == 0)
	    {
	      /* Note that we're not disallowing mixing data and code.  */
	      if ((elf_section_data (isec)->this_hdr.sh_flags
		   & SHF_SH5_ISA32) != 0)
		o_shdrp[oIndex]->sh_flags |= SHF_SH5_ISA32;
	      break;
	    }
	}
    }

  return sh_elf64_set_private_flags (obfd, elf_elfheader (ibfd)->e_flags);
}

static bfd_boolean
sh_elf64_copy_private_data (bfd *ibfd, bfd *obfd)
{
  return sh_elf64_copy_private_data_internal (ibfd, obfd);
}

static bfd_boolean
sh_elf64_merge_private_data (bfd *ibfd, bfd *obfd)
{
  flagword old_flags, new_flags;

  if (! _bfd_generic_verify_endian_match (ibfd, obfd))
    return FALSE;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  if (bfd_get_arch_size (ibfd) != bfd_get_arch_size (obfd))
    {
      const char *msg;

      if (bfd_get_arch_size (ibfd) == 32
	  && bfd_get_arch_size (obfd) == 64)
	msg = _("%s: compiled as 32-bit object and %s is 64-bit");
      else if (bfd_get_arch_size (ibfd) == 64
	       && bfd_get_arch_size (obfd) == 32)
	msg = _("%s: compiled as 64-bit object and %s is 32-bit");
      else
	msg = _("%s: object size does not match that of target %s");

      (*_bfd_error_handler) (msg, bfd_get_filename (ibfd),
			     bfd_get_filename (obfd));
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  old_flags = elf_elfheader (obfd)->e_flags;
  new_flags = elf_elfheader (ibfd)->e_flags;
  if (! elf_flags_init (obfd))
    {
      /* This happens when ld starts out with a 'blank' output file.  */
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = old_flags = new_flags;
    }
  /* We don't allow linking in anything else than SH64 code, and since
     this is a 64-bit ELF, we assume the 64-bit ABI is used.  Add code
     here as things change.  */
  else if ((new_flags & EF_SH_MACH_MASK) != EF_SH5)
    {
      (*_bfd_error_handler)
	("%s: does not use the SH64 64-bit ABI as previous modules do",
	 bfd_get_filename (ibfd));
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  sh_elf64_copy_private_data_internal (ibfd, obfd);

  /* I can't think of anything sane other than old_flags being EF_SH5 and
     that we need to preserve that.  */
  elf_elfheader (obfd)->e_flags = old_flags;

  return sh_elf64_set_mach_from_flags (obfd);
}

/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
sh_elf64_gc_mark_hook (asection *sec,
		       struct bfd_link_info *info ATTRIBUTE_UNUSED,
		       Elf_Internal_Rela *rel,
		       struct elf_link_hash_entry *h,
		       Elf_Internal_Sym *sym)
{
  if (h != NULL)
    {
      switch (ELF64_R_TYPE (rel->r_info))
	{
	case R_SH_GNU_VTINHERIT:
	case R_SH_GNU_VTENTRY:
	  break;

	default:
	  while (h->root.type == bfd_link_hash_indirect
		 && h->root.u.i.link)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
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
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

static bfd_boolean
sh_elf64_gc_sweep_hook (bfd *abfd ATTRIBUTE_UNUSED,
			struct bfd_link_info *info ATTRIBUTE_UNUSED,
			asection *sec ATTRIBUTE_UNUSED,
			const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED)
{
  /* No got and plt entries for 64-bit SH at present.  */
  return TRUE;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
sh_elf64_check_relocs (bfd *abfd, struct bfd_link_info *info,
		       asection *sec, const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;

  sgot = NULL;
  srelgot = NULL;
  sreloc = NULL;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof(Elf64_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  dynobj = elf_hash_table (info)->dynobj;
  local_got_offsets = elf_local_got_offsets (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      /* Some relocs require a global offset table.  */
      if (dynobj == NULL)
	{
	  switch (ELF64_R_TYPE (rel->r_info))
	    {
	    case R_SH_GOTPLT_LOW16:
	    case R_SH_GOTPLT_MEDLOW16:
	    case R_SH_GOTPLT_MEDHI16:
	    case R_SH_GOTPLT_HI16:
	    case R_SH_GOTPLT10BY4:
	    case R_SH_GOTPLT10BY8:
	    case R_SH_GOT_LOW16:
	    case R_SH_GOT_MEDLOW16:
	    case R_SH_GOT_MEDHI16:
	    case R_SH_GOT_HI16:
	    case R_SH_GOT10BY4:
	    case R_SH_GOT10BY8:
	    case R_SH_GOTOFF_LOW16:
	    case R_SH_GOTOFF_MEDLOW16:
	    case R_SH_GOTOFF_MEDHI16:
	    case R_SH_GOTOFF_HI16:
	    case R_SH_GOTPC_LOW16:
	    case R_SH_GOTPC_MEDLOW16:
	    case R_SH_GOTPC_MEDHI16:
	    case R_SH_GOTPC_HI16:
	      elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! _bfd_elf_create_got_section (dynobj, info))
		return FALSE;
	      break;

	    default:
	      break;
	    }
	}

      switch (ELF64_R_TYPE (rel->r_info))
        {
	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
        case R_SH_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
        case R_SH_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;

	force_got:
	case R_SH_GOT_LOW16:
	case R_SH_GOT_MEDLOW16:
	case R_SH_GOT_MEDHI16:
	case R_SH_GOT_HI16:
	case R_SH_GOT10BY4:
	case R_SH_GOT10BY8:
	  /* This symbol requires a global offset table entry.  */

	  if (sgot == NULL)
	    {
	      sgot = bfd_get_section_by_name (dynobj, ".got");
	      BFD_ASSERT (sgot != NULL);
	    }

	  if (srelgot == NULL
	      && (h != NULL || info->shared))
	    {
	      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
	      if (srelgot == NULL)
		{
		  srelgot = bfd_make_section_with_flags (dynobj,
							 ".rela.got",
							 (SEC_ALLOC
							  | SEC_LOAD
							  | SEC_HAS_CONTENTS
							  | SEC_IN_MEMORY
							  | SEC_LINKER_CREATED
							  | SEC_READONLY));
		  if (srelgot == NULL
		      || ! bfd_set_section_alignment (dynobj, srelgot, 2))
		    return FALSE;
		}
	    }

	  if (h != NULL)
	    {
	      if (h->type == STT_DATALABEL)
		{
		  struct elf_sh64_link_hash_entry *hsh;

		  h = (struct elf_link_hash_entry *) h->root.u.i.link;
		  hsh = (struct elf_sh64_link_hash_entry *)h;
		  if (hsh->datalabel_got_offset != (bfd_vma) -1)
		    break;

		  hsh->datalabel_got_offset = sgot->size;
		}
	      else
		{
		  if (h->got.offset != (bfd_vma) -1)
		    {
		      /* We have already allocated space in the .got.  */
		      break;
		    }
		  h->got.offset = sgot->size;
		}

	      /* Make sure this symbol is output as a dynamic symbol.  */
	      if (h->dynindx == -1)
		{
		  if (! bfd_elf_link_record_dynamic_symbol (info, h))
		    return FALSE;
		}

	      srelgot->size += sizeof (Elf64_External_Rela);
	    }
	  else
	    {
     	      /* This is a global offset table entry for a local
		 symbol.  */
	      if (local_got_offsets == NULL)
		{
		  size_t size;
		  register unsigned int i;

		  size = symtab_hdr->sh_info * sizeof (bfd_vma);
		  /* Reserve space for both the datalabel and
		     codelabel local GOT offsets.  */
		  size *= 2;
		  local_got_offsets = (bfd_vma *) bfd_alloc (abfd, size);
		  if (local_got_offsets == NULL)
		    return FALSE;
		  elf_local_got_offsets (abfd) = local_got_offsets;
		  for (i = 0; i < symtab_hdr->sh_info; i++)
		    local_got_offsets[i] = (bfd_vma) -1;
		  for (; i < 2 * symtab_hdr->sh_info; i++)
		    local_got_offsets[i] = (bfd_vma) -1;
		}
	      if ((rel->r_addend & 1) != 0)
		{
		  if (local_got_offsets[symtab_hdr->sh_info
					+ r_symndx] != (bfd_vma) -1)
		    {
		      /* We have already allocated space in the .got.  */
		      break;
		    }
		  local_got_offsets[symtab_hdr->sh_info
				    + r_symndx] = sgot->size;
		}
	      else
		{
		  if (local_got_offsets[r_symndx] != (bfd_vma) -1)
		    {
		      /* We have already allocated space in the .got.  */
		      break;
		    }
		  local_got_offsets[r_symndx] = sgot->size;
		}

	      if (info->shared)
		{
		  /* If we are generating a shared object, we need to
		     output a R_SH_RELATIVE reloc so that the dynamic
		     linker can adjust this GOT entry.  */
		  srelgot->size += sizeof (Elf64_External_Rela);
		}
	    }

	  sgot->size += 8;

	  break;

	case R_SH_GOTPLT_LOW16:
	case R_SH_GOTPLT_MEDLOW16:
	case R_SH_GOTPLT_MEDHI16:
	case R_SH_GOTPLT_HI16:
	case R_SH_GOTPLT10BY4:
	case R_SH_GOTPLT10BY8:
	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.  */

	  if (h == NULL
	      || ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
	      || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN
	      || ! info->shared
	      || info->symbolic
	      || h->dynindx == -1
	      || h->got.offset != (bfd_vma) -1)
	    goto force_got;

	  /* Make sure this symbol is output as a dynamic symbol.  */
	  if (h->dynindx == -1)
	    {
	      if (! bfd_elf_link_record_dynamic_symbol (info, h))
		return FALSE;
	    }

	  h->needs_plt = 1;

	  break;

	case R_SH_PLT_LOW16:
	case R_SH_PLT_MEDLOW16:
	case R_SH_PLT_MEDHI16:
	case R_SH_PLT_HI16:
	  /* This symbol requires a procedure linkage table entry.  We
	     actually build the entry in adjust_dynamic_symbol,
	     because this might be a case of linking PIC code which is
	     never referenced by a dynamic object, in which case we
	     don't need to generate a procedure linkage table entry
	     after all.  */

	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.  */
	  if (h == NULL)
	    continue;

	  if (ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
	      || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN)
	    break;

	  h->needs_plt = 1;

	  break;

	case R_SH_64:
	case R_SH_64_PCREL:
	  if (h != NULL)
	    h->non_got_ref = 1;

	  /* If we are creating a shared library, and this is a reloc
	     against a global symbol, or a non PC relative reloc
	     against a local symbol, then we need to copy the reloc
	     into the shared library.  However, if we are linking with
	     -Bsymbolic, we do not need to copy a reloc against a
	     global symbol which is defined in an object we are
	     including in the link (i.e., DEF_REGULAR is set).  At
	     this point we have not seen all the input files, so it is
	     possible that DEF_REGULAR is not set now but will be set
	     later (it is never cleared).  We account for that
	     possibility below by storing information in the
	     pcrel_relocs_copied field of the hash table entry.  */
	  if (info->shared
	      && (sec->flags & SEC_ALLOC) != 0
	      && (ELF32_R_TYPE (rel->r_info) != R_SH_64_PCREL
		  || (h != NULL
		      && (! info->symbolic
			  || !h->def_regular))))
	    {
	      /* When creating a shared object, we must copy these
		 reloc types into the output file.  We create a reloc
		 section in dynobj and make room for this reloc.  */
	      if (sreloc == NULL)
		{
		  const char *name;

		  name = (bfd_elf_string_from_elf_section
			  (abfd,
			   elf_elfheader (abfd)->e_shstrndx,
			   elf_section_data (sec)->rel_hdr.sh_name));
		  if (name == NULL)
		    return FALSE;

		  BFD_ASSERT (strncmp (name, ".rela", 5) == 0
			      && strcmp (bfd_get_section_name (abfd, sec),
					 name + 5) == 0);

		  sreloc = bfd_get_section_by_name (dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      sreloc = bfd_make_section_with_flags (dynobj,
							    name,
							    flags);
		      if (sreloc == NULL
			  || ! bfd_set_section_alignment (dynobj, sreloc, 2))
			return FALSE;
		    }
		}

	      sreloc->size += sizeof (Elf64_External_Rela);

	      /* If we are linking with -Bsymbolic, and this is a
		 global symbol, we count the number of PC relative
		 relocations we have entered for this symbol, so that
		 we can discard them again if the symbol is later
		 defined by a regular object.  Note that this function
		 is only called if we are using an elf_sh linker
		 hash table, which means that h is really a pointer to
		 an elf_sh_link_hash_entry.  */
	      if (h != NULL && info->symbolic
		  && ELF64_R_TYPE (rel->r_info) == R_SH_64_PCREL)
		{
		  struct elf_sh64_link_hash_entry *eh;
		  struct elf_sh64_pcrel_relocs_copied *p;

		  eh = (struct elf_sh64_link_hash_entry *) h;

		  for (p = eh->pcrel_relocs_copied; p != NULL; p = p->next)
		    if (p->section == sreloc)
		      break;

		  if (p == NULL)
		    {
		      p = ((struct elf_sh64_pcrel_relocs_copied *)
			   bfd_alloc (dynobj, sizeof *p));
		      if (p == NULL)
			return FALSE;
		      p->next = eh->pcrel_relocs_copied;
		      eh->pcrel_relocs_copied = p;
		      p->section = sreloc;
		      p->count = 0;
		    }

		  ++p->count;
		}
	    }

	  break;
        }
    }

  return TRUE;
}

static int
sh64_elf64_get_symbol_type (Elf_Internal_Sym * elf_sym, int type)
{
  if (ELF_ST_TYPE (elf_sym->st_info) == STT_DATALABEL)
    return STT_DATALABEL;

  return type;
}

/* FIXME: This is a copy of sh64_elf_add_symbol_hook in elf32-sh64.c.
   Either file can presumably exist without the other, but do not differ
   in elf-size-ness.  How to share?

   Hook called by the linker routine which adds symbols from an object
   file.  We must make indirect symbols for undefined symbols marked with
   STT_DATALABEL, so relocations passing them will pick up that attribute
   and neutralize STO_SH5_ISA32 found on the symbol definition.

   There is a problem, though: We want to fill in the hash-table entry for
   this symbol and signal to the caller that no further processing is
   needed.  But we don't have the index for this hash-table entry.  We
   rely here on that the current entry is the first hash-entry with NULL,
   which seems brittle.  Also, iterating over the hash-table to find that
   entry is a linear operation on the number of symbols in this input
   file, and this function should take constant time, so that's not good
   too.  Only comfort is that DataLabel references should only be found in
   hand-written assembly code and thus be rare.  FIXME: Talk maintainers
   into adding an option to elf_add_symbol_hook (preferably) for the index
   or the hash entry, alternatively adding the index to Elf_Internal_Sym
   (not so good).  */

static bfd_boolean
sh64_elf64_add_symbol_hook (bfd *abfd, struct bfd_link_info *info,
			    Elf_Internal_Sym *sym, const char **namep,
			    flagword *flagsp ATTRIBUTE_UNUSED,
			    asection **secp, bfd_vma *valp)
{
  /* We want to do this for relocatable as well as final linking.  */
  if (ELF_ST_TYPE (sym->st_info) == STT_DATALABEL
      && is_elf_hash_table (info->hash))
    {
      struct elf_link_hash_entry *h;

      /* For relocatable links, we register the DataLabel sym in its own
	 right, and tweak the name when it's output.  Otherwise, we make
	 an indirect symbol of it.  */
      flagword flags
	= info->relocatable || info->emitrelocations
	? BSF_GLOBAL : BSF_GLOBAL | BSF_INDIRECT;

      char *dl_name
	= bfd_malloc (strlen (*namep) + sizeof (DATALABEL_SUFFIX));
      struct elf_link_hash_entry ** sym_hash = elf_sym_hashes (abfd);

      BFD_ASSERT (sym_hash != NULL);

      /* Allocation may fail.  */
      if (dl_name == NULL)
	return FALSE;

      strcpy (dl_name, *namep);
      strcat (dl_name, DATALABEL_SUFFIX);

      h = (struct elf_link_hash_entry *)
	bfd_link_hash_lookup (info->hash, dl_name, FALSE, FALSE, FALSE);

      if (h == NULL)
	{
	  /* No previous datalabel symbol.  Make one.  */
	  struct bfd_link_hash_entry *bh = NULL;
	  const struct elf_backend_data *bed = get_elf_backend_data (abfd);

	  if (! _bfd_generic_link_add_one_symbol (info, abfd, dl_name,
						  flags, *secp, *valp,
						  *namep, FALSE,
						  bed->collect, &bh))
	    {
	      free (dl_name);
	      return FALSE;
	    }

	  h = (struct elf_link_hash_entry *) bh;
	  h->non_elf = 0;
	  h->type = STT_DATALABEL;
	}
      else
	/* If a new symbol was created, it holds the allocated name.
	   Otherwise, we don't need it anymore and should deallocate it.  */
	free (dl_name);

      if (h->type != STT_DATALABEL
	  || ((info->relocatable || info->emitrelocations)
	      && h->root.type != bfd_link_hash_undefined)
	  || (! info->relocatable && !info->emitrelocations
	      && h->root.type != bfd_link_hash_indirect))
	{
	  /* Make sure we don't get confused on invalid input.  */
	  (*_bfd_error_handler)
	    (_("%s: encountered datalabel symbol in input"),
	     bfd_get_filename (abfd));
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      /* Now find the hash-table slot for this entry and fill it in.  */
      while (*sym_hash != NULL)
	sym_hash++;
      *sym_hash = h;

      /* Signal to caller to skip this symbol - we've handled it.  */
      *namep = NULL;
    }

  return TRUE;
}

/* This hook function is called before the linker writes out a global
   symbol.  For relocatable links, DataLabel symbols will be present in
   linker output.  We cut off the special suffix on those symbols, so the
   right name appears in the output.

   When linking and emitting relocations, there can appear global symbols
   that are not referenced by relocs, but rather only implicitly through
   DataLabel references, a relation that is not visible to the linker.
   Since no stripping of global symbols in done when doing such linking,
   we don't need to look up and make sure to emit the main symbol for each
   DataLabel symbol.  */

static bfd_boolean
sh64_elf64_link_output_symbol_hook (struct bfd_link_info *info,
				    const char *cname,
				    Elf_Internal_Sym *sym,
				    asection *input_sec ATTRIBUTE_UNUSED,
				    struct elf_link_hash_entry *h ATTRIBUTE_UNUSED)
{
  char *name = (char *) cname;

  if (info->relocatable || info->emitrelocations)
    {
      if (ELF_ST_TYPE (sym->st_info) == STT_DATALABEL)
	name[strlen (name) - strlen (DATALABEL_SUFFIX)] = 0;
    }

  return TRUE;
}

/* Set bit 0 on the entry address; it always points to SHmedia code.  This
   is mostly for symmetry with the 32-bit format, where code can be
   SHcompact and we need to make a distinction to make sure execution
   starts in the right ISA mode.  It is also convenient for a loader,
   which would otherwise have to set this bit when loading a TR register
   before jumping to the program entry.  */

static void
sh64_elf64_final_write_processing (bfd *abfd,
				   bfd_boolean linker ATTRIBUTE_UNUSED)
{
  /* FIXME: Perhaps we shouldn't do this if the entry address was supplied
     numerically, but we currently lack the infrastructure to recognize
     that: The entry symbol, and info whether it is numeric or a symbol
     name is kept private in the linker.  */
  if (elf_elfheader (abfd)->e_type == ET_EXEC)
    elf_elfheader (abfd)->e_entry |= 1;
}

/* First entry in an absolute procedure linkage table look like this.  */

static const bfd_byte elf_sh64_plt0_entry_be[PLT_ENTRY_SIZE] =
{
  0xcc, 0x00, 0x01, 0x10, /* movi  .got.plt >> 48, r17 */
  0xc8, 0x00, 0x01, 0x10, /* shori (.got.plt >> 32) & 65535, r17 */
  0xc8, 0x00, 0x01, 0x10, /* shori (.got.plt >> 16) & 65535, r17 */
  0xc8, 0x00, 0x01, 0x10, /* shori .got.plt & 65535, r17 */
  0x8d, 0x10, 0x09, 0x90, /* ld.q  r17, 16, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x8d, 0x10, 0x05, 0x10, /* ld.q  r17, 8, r17 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
};

static const bfd_byte elf_sh64_plt0_entry_le[PLT_ENTRY_SIZE] =
{
  0x10, 0x01, 0x00, 0xcc, /* movi  .got.plt >> 16, r17 */
  0x10, 0x01, 0x00, 0xc8, /* shori (.got.plt >> 32) & 65535, r17 */
  0x10, 0x01, 0x00, 0xc8, /* shori (.got.plt >> 16) & 65535, r17 */
  0x10, 0x01, 0x00, 0xc8, /* shori .got.plt & 65535, r17 */
  0x90, 0x09, 0x10, 0x8d, /* ld.q  r17, 16, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0x10, 0x05, 0x10, 0x8d, /* ld.q  r17, 8, r17 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
};

/* Sebsequent entries in an absolute procedure linkage table look like
   this.  */

static const bfd_byte elf_sh64_plt_entry_be[PLT_ENTRY_SIZE] =
{
  0xcc, 0x00, 0x01, 0x90, /* movi  nameN-in-GOT >> 48, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori (nameN-in-GOT >> 32) & 65535, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori (nameN-in-GOT >> 16) & 65535, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori nameN-in-GOT & 65535, r25 */
  0x8d, 0x90, 0x01, 0x90, /* ld.q  r25, 0, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0xcc, 0x00, 0x01, 0x90, /* movi  (.+8-.PLT0) >> 16, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori (.+4-.PLT0) & 65535, r25 */
  0x6b, 0xf5, 0x66, 0x00, /* ptrel r25, tr0 */
  0xcc, 0x00, 0x01, 0x50, /* movi  reloc-offset >> 16, r21 */
  0xc8, 0x00, 0x01, 0x50, /* shori reloc-offset & 65535, r21 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
};

static const bfd_byte elf_sh64_plt_entry_le[PLT_ENTRY_SIZE] =
{
  0x90, 0x01, 0x00, 0xcc, /* movi  nameN-in-GOT >> 16, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori nameN-in-GOT & 65535, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori nameN-in-GOT & 65535, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori nameN-in-GOT & 65535, r25 */
  0x90, 0x01, 0x90, 0x8d, /* ld.q  r25, 0, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0x90, 0x01, 0x00, 0xcc, /* movi  (.+8-.PLT0) >> 16, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori (.+4-.PLT0) & 65535, r25 */
  0x00, 0x66, 0xf5, 0x6b, /* ptrel r25, tr0 */
  0x50, 0x01, 0x00, 0xcc, /* movi  reloc-offset >> 16, r21 */
  0x50, 0x01, 0x00, 0xc8, /* shori reloc-offset & 65535, r21 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
};

/* Entries in a PIC procedure linkage table look like this.  */

static const bfd_byte elf_sh64_pic_plt_entry_be[PLT_ENTRY_SIZE] =
{
  0xcc, 0x00, 0x01, 0x90, /* movi  nameN@GOT >> 16, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori nameN@GOT & 65535, r25 */
  0x40, 0xc3, 0x65, 0x90, /* ldx.q r12, r25, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0xce, 0x00, 0x01, 0x10, /* movi  -GOT_BIAS, r17 */
  0x00, 0xc9, 0x45, 0x10, /* add   r12, r17, r17 */
  0x8d, 0x10, 0x09, 0x90, /* ld.q  r17, 16, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x8d, 0x10, 0x05, 0x10, /* ld.q  r17, 8, r17 */
  0xcc, 0x00, 0x01, 0x50, /* movi  reloc-offset >> 16, r21 */
  0xc8, 0x00, 0x01, 0x50, /* shori reloc-offset & 65535, r21 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
};

static const bfd_byte elf_sh64_pic_plt_entry_le[PLT_ENTRY_SIZE] =
{
  0x90, 0x01, 0x00, 0xcc, /* movi  nameN@GOT >> 16, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori nameN@GOT & 65535, r25 */
  0x90, 0x65, 0xc3, 0x40, /* ldx.q r12, r25, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0x10, 0x01, 0x00, 0xce, /* movi  -GOT_BIAS, r17 */
  0x10, 0x45, 0xc9, 0x00, /* add   r12, r17, r17 */
  0x90, 0x09, 0x10, 0x8d, /* ld.q  r17, 16, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0x10, 0x05, 0x10, 0x8d, /* ld.q  r17, 8, r17 */
  0x50, 0x01, 0x00, 0xcc, /* movi  reloc-offset >> 16, r21 */
  0x50, 0x01, 0x00, 0xc8, /* shori reloc-offset & 65535, r21 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
};

static const bfd_byte *elf_sh64_plt0_entry;
static const bfd_byte *elf_sh64_plt_entry;
static const bfd_byte *elf_sh64_pic_plt_entry;

/* Create an entry in an sh ELF linker hash table.  */

static struct bfd_hash_entry *
sh64_elf64_link_hash_newfunc (struct bfd_hash_entry *entry,
			      struct bfd_hash_table *table,
			      const char *string)
{
  struct elf_sh64_link_hash_entry *ret =
    (struct elf_sh64_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_sh64_link_hash_entry *) NULL)
    ret = ((struct elf_sh64_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf_sh64_link_hash_entry)));
  if (ret == (struct elf_sh64_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_sh64_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct elf_sh64_link_hash_entry *) NULL)
    {
      ret->pcrel_relocs_copied = NULL;
      ret->datalabel_got_offset = (bfd_vma) -1;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create an sh64 ELF linker hash table.  */

static struct bfd_link_hash_table *
sh64_elf64_link_hash_table_create (bfd *abfd)
{
  struct elf_sh64_link_hash_table *ret;

  ret = ((struct elf_sh64_link_hash_table *)
	 bfd_malloc (sizeof (struct elf_sh64_link_hash_table)));
  if (ret == (struct elf_sh64_link_hash_table *) NULL)
    return NULL;

  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      sh64_elf64_link_hash_newfunc,
				      sizeof (struct elf_sh64_link_hash_entry)))
    {
      free (ret);
      return NULL;
    }

  return &ret->root.root;
}

inline static void
movi_shori_putval (bfd *output_bfd, unsigned long value, bfd_byte *addr)
{
  bfd_put_32 (output_bfd,
	      bfd_get_32 (output_bfd, addr)
	      | ((value >> 6) & 0x3fffc00),
	      addr);
  bfd_put_32 (output_bfd,
	      bfd_get_32 (output_bfd, addr + 4)
	      | ((value << 10) & 0x3fffc00),
	      addr + 4);
}

inline static void
movi_3shori_putval (bfd *output_bfd, bfd_vma value, bfd_byte *addr)
{
  bfd_put_32 (output_bfd,
	      bfd_get_32 (output_bfd, addr)
	      | ((value >> 38) & 0x3fffc00),
	      addr);
  bfd_put_32 (output_bfd,
	      bfd_get_32 (output_bfd, addr + 4)
	      | ((value >> 22) & 0x3fffc00),
	      addr + 4);
  bfd_put_32 (output_bfd,
	      bfd_get_32 (output_bfd, addr + 8)
	      | ((value >> 6) & 0x3fffc00),
	      addr + 8);
  bfd_put_32 (output_bfd,
	      bfd_get_32 (output_bfd, addr + 12)
	      | ((value << 10) & 0x3fffc00),
	      addr + 12);
}

/* Create dynamic sections when linking against a dynamic object.  */

static bfd_boolean
sh64_elf64_create_dynamic_sections (bfd *abfd, struct bfd_link_info *info)
{
  flagword flags, pltflags;
  register asection *s;
  const struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int ptralign = 0;

  switch (bed->s->arch_size)
    {
    case 32:
      ptralign = 2;
      break;

    case 64:
      ptralign = 3;
      break;

    default:
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  /* We need to create .plt, .rel[a].plt, .got, .got.plt, .dynbss, and
     .rel[a].bss sections.  */

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  pltflags = flags;
  pltflags |= SEC_CODE;
  if (bed->plt_not_loaded)
    pltflags &= ~ (SEC_LOAD | SEC_HAS_CONTENTS);
  if (bed->plt_readonly)
    pltflags |= SEC_READONLY;

  s = bfd_make_section_with_flags (abfd, ".plt", pltflags);
  if (s == NULL
      || ! bfd_set_section_alignment (abfd, s, bed->plt_alignment))
    return FALSE;

  if (bed->want_plt_sym)
    {
      /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
	 .plt section.  */
      struct elf_link_hash_entry *h;
      struct bfd_link_hash_entry *bh = NULL;

      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s,
	      (bfd_vma) 0, (const char *) NULL, FALSE, bed->collect, &bh)))
	return FALSE;

      h = (struct elf_link_hash_entry *) bh;
      h->def_regular = 1;
      h->type = STT_OBJECT;
      elf_hash_table (info)->hplt = h;

      if (info->shared
	  && ! bfd_elf_link_record_dynamic_symbol (info, h))
	return FALSE;
    }

  s = bfd_make_section_with_flags (abfd,
				   bed->default_use_rela_p ? ".rela.plt" : ".rel.plt",
				   flags | SEC_READONLY);
  if (s == NULL
      || ! bfd_set_section_alignment (abfd, s, ptralign))
    return FALSE;

  if (! _bfd_elf_create_got_section (abfd, info))
    return FALSE;

  {
    const char *secname;
    char *relname;
    flagword secflags;
    asection *sec;

    for (sec = abfd->sections; sec; sec = sec->next)
      {
	secflags = bfd_get_section_flags (abfd, sec);
	if ((secflags & (SEC_DATA | SEC_LINKER_CREATED))
	    || ((secflags & SEC_HAS_CONTENTS) != SEC_HAS_CONTENTS))
	  continue;
	secname = bfd_get_section_name (abfd, sec);
	relname = (char *) bfd_malloc (strlen (secname) + 6);
	strcpy (relname, ".rela");
	strcat (relname, secname);
	s = bfd_make_section_with_flags (abfd, relname,
					 flags | SEC_READONLY);
	if (s == NULL
	    || ! bfd_set_section_alignment (abfd, s, ptralign))
	  return FALSE;
      }
  }

  if (bed->want_dynbss)
    {
      /* The .dynbss section is a place to put symbols which are defined
	 by dynamic objects, are referenced by regular objects, and are
	 not functions.  We must allocate space for them in the process
	 image and use a R_*_COPY reloc to tell the dynamic linker to
	 initialize them at run time.  The linker script puts the .dynbss
	 section into the .bss section of the final image.  */
      s = bfd_make_section_with_flags (abfd, ".dynbss",
				       SEC_ALLOC | SEC_LINKER_CREATED);
      if (s == NULL)
	return FALSE;

      /* The .rel[a].bss section holds copy relocs.  This section is not
	 normally needed.  We need to create it here, though, so that the
	 linker will map it to an output section.  We can't just create it
	 only if we need it, because we will not know whether we need it
	 until we have seen all the input files, and the first time the
	 main linker code calls BFD after examining all the input files
	 (size_dynamic_sections) the input sections have already been
	 mapped to the output sections.  If the section turns out not to
	 be needed, we can discard it later.  We will never need this
	 section when generating a shared object, since they do not use
	 copy relocs.  */
      if (! info->shared)
	{
	  s = bfd_make_section_with_flags (abfd,
					   (bed->default_use_rela_p
					    ? ".rela.bss" : ".rel.bss"),
					   flags | SEC_READONLY);
	  if (s == NULL
	      || ! bfd_set_section_alignment (abfd, s, ptralign))
	    return FALSE;
	}
    }

  return TRUE;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
sh64_elf64_adjust_dynamic_symbol (struct bfd_link_info *info,
				  struct elf_link_hash_entry *h)
{
  bfd *dynobj;
  asection *s;
  unsigned int power_of_two;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && (h->needs_plt
		  || h->u.weakdef != NULL
		  || (h->def_dynamic
		      && h->ref_regular
		      && !h->def_regular)));

  /* If this is a function, put it in the procedure linkage table.  We
     will fill in the contents of the procedure linkage table later,
     when we know the address of the .got section.  */
  if (h->type == STT_FUNC
      || h->needs_plt)
    {
      if (! info->shared
	  && !h->def_dynamic
	  && !h->ref_dynamic)
	{
	  /* This case can occur if we saw a PLT reloc in an input
	     file, but the symbol was never referred to by a dynamic
	     object.  In such a case, we don't actually need to build
	     a procedure linkage table, and we can just do a REL64
	     reloc instead.  */
	  BFD_ASSERT (h->needs_plt);
	  return TRUE;
	}

      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = bfd_get_section_by_name (dynobj, ".plt");
      BFD_ASSERT (s != NULL);

      /* If this is the first .plt entry, make room for the special
	 first entry.  */
      if (s->size == 0)
	s->size += PLT_ENTRY_SIZE;

      /* If this symbol is not defined in a regular file, and we are
	 not generating a shared library, then set the symbol to this
	 location in the .plt.  This is required to make function
	 pointers compare as equal between the normal executable and
	 the shared library.  */
      if (! info->shared
	  && !h->def_regular)
	{
	  h->root.u.def.section = s;
	  h->root.u.def.value = s->size;
	}

      h->plt.offset = s->size;

      /* Make room for this entry.  */
      s->size += elf_sh64_sizeof_plt (info);

      /* We also need to make an entry in the .got.plt section, which
	 will be placed in the .got section by the linker script.  */

      s = bfd_get_section_by_name (dynobj, ".got.plt");
      BFD_ASSERT (s != NULL);
      s->size += 8;

      /* We also need to make an entry in the .rela.plt section.  */

      s = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (s != NULL);
      s->size += sizeof (Elf64_External_Rela);

      return TRUE;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->u.weakdef != NULL)
    {
      BFD_ASSERT (h->u.weakdef->root.type == bfd_link_hash_defined
		  || h->u.weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->u.weakdef->root.u.def.section;
      h->root.u.def.value = h->u.weakdef->root.u.def.value;
      return TRUE;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  /* If we are creating a shared library, we must presume that the
     only references to the symbol are via the global offset table.
     For such cases we need not do anything here; the relocations will
     be handled correctly by relocate_section.  */
  if (info->shared)
    return TRUE;

  /* If there are no references to this symbol that do not use the
     GOT, we don't need to generate a copy reloc.  */
  if (!h->non_got_ref)
    return TRUE;

  if (h->size == 0)
    {
      (*_bfd_error_handler) (_("dynamic variable `%s' is zero size"),
			     h->root.root.string);
      return TRUE;
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

  s = bfd_get_section_by_name (dynobj, ".dynbss");
  BFD_ASSERT (s != NULL);

  /* We must generate a R_SH_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = bfd_get_section_by_name (dynobj, ".rela.bss");
      BFD_ASSERT (srel != NULL);
      srel->size += sizeof (Elf64_External_Rela);
      h->needs_copy = 1;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s->size = BFD_ALIGN (s->size, (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (dynobj, s))
    {
      if (! bfd_set_section_alignment (dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->size;

  /* Increment the section size to make room for the symbol.  */
  s->size += h->size;

  return TRUE;
}

/* This function is called via sh_elf_link_hash_traverse if we are
   creating a shared object with -Bsymbolic.  It discards the space
   allocated to copy PC relative relocs against symbols which are
   defined in regular objects.  We allocated space for them in the
   check_relocs routine, but we won't fill them in in the
   relocate_section routine.  */

static bfd_boolean
sh64_elf64_discard_copies (struct elf_sh64_link_hash_entry *h,
			   void *ignore ATTRIBUTE_UNUSED)
{
  struct elf_sh64_pcrel_relocs_copied *s;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct elf_sh64_link_hash_entry *) h->root.root.u.i.link;

  /* We only discard relocs for symbols defined in a regular object.  */
  if (!h->root.def_regular)
    return TRUE;

  for (s = h->pcrel_relocs_copied; s != NULL; s = s->next)
    s->section->size -= s->count * sizeof (Elf64_External_Rela);

  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
sh64_elf64_size_dynamic_sections (bfd *output_bfd,
				  struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *s;
  bfd_boolean plt;
  bfd_boolean relocs;
  bfd_boolean reltext;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->size = sizeof ELF_DYNAMIC_INTERPRETER;
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
	s->size = 0;
    }

  /* If this is a -Bsymbolic shared link, then we need to discard all
     PC relative relocs against symbols defined in a regular object.
     We allocated space for them in the check_relocs routine, but we
     will not fill them in in the relocate_section routine.  */
  if (info->shared && info->symbolic)
    sh64_elf64_link_hash_traverse (sh64_elf64_hash_table (info),
				   sh64_elf64_discard_copies, NULL);

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = FALSE;
  relocs = FALSE;
  reltext = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if (strcmp (name, ".plt") == 0)
	{
	  /* Remember whether there is a PLT.  */
	  plt = s->size != 0;
	}
      else if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->size != 0)
	    {
	      asection *target;

	      /* Remember whether there are any reloc sections other
		 than .rela.plt.  */
	      if (strcmp (name, ".rela.plt") != 0)
		{
		  const char *outname;

		  relocs = TRUE;

		  /* If this relocation section applies to a read only
		     section, then we probably need a DT_TEXTREL
		     entry.  The entries in the .rela.plt section
		     really apply to the .got section, which we
		     created ourselves and so know is not readonly.  */
		  outname = bfd_get_section_name (output_bfd,
						  s->output_section);
		  target = bfd_get_section_by_name (output_bfd, outname + 5);
		  if (target != NULL
		      && (target->flags & SEC_READONLY) != 0
		      && (target->flags & SEC_ALLOC) != 0)
		    reltext = TRUE;
		}

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) != 0
	       && strcmp (name, ".dynbss") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->size == 0)
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
	  s->flags |= SEC_EXCLUDE;
	  continue;
	}

      if ((s->flags & SEC_HAS_CONTENTS) == 0)
	continue;

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->size);
      if (s->contents == NULL)
	return FALSE;
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in sh64_elf64_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (info->executable)
	{
	  if (!_bfd_elf_add_dynamic_entry (info, DT_DEBUG, 0))
	    return FALSE;
	}

      if (plt)
	{
	  if (!_bfd_elf_add_dynamic_entry (info, DT_PLTGOT, 0)
	      || !_bfd_elf_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || !_bfd_elf_add_dynamic_entry (info, DT_PLTREL, DT_RELA)
	      || !_bfd_elf_add_dynamic_entry (info, DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (!_bfd_elf_add_dynamic_entry (info, DT_RELA, 0)
	      || !_bfd_elf_add_dynamic_entry (info, DT_RELASZ, 0)
	      || !_bfd_elf_add_dynamic_entry (info, DT_RELAENT,
					      sizeof (Elf64_External_Rela)))
	    return FALSE;
	}

      if (reltext)
	{
	  if (!_bfd_elf_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return FALSE;
	}
    }

  return TRUE;
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
sh64_elf64_finish_dynamic_symbol (bfd *output_bfd,
				  struct bfd_link_info *info,
				  struct elf_link_hash_entry *h,
				  Elf_Internal_Sym *sym)
{
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *splt;
      asection *sgot;
      asection *srel;

      bfd_vma plt_index;
      bfd_vma got_offset;
      Elf_Internal_Rela rel;
      bfd_byte *loc;

      /* This symbol has an entry in the procedure linkage table.  Set
	 it up.  */

      BFD_ASSERT (h->dynindx != -1);

      splt = bfd_get_section_by_name (dynobj, ".plt");
      sgot = bfd_get_section_by_name (dynobj, ".got.plt");
      srel = bfd_get_section_by_name (dynobj, ".rela.plt");
      BFD_ASSERT (splt != NULL && sgot != NULL && srel != NULL);

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = h->plt.offset / elf_sh64_sizeof_plt (info) - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.  Each .got entry is 8 bytes.
	 The first three are reserved.  */
      got_offset = (plt_index + 3) * 8;

      if (info->shared)
	got_offset -= GOT_BIAS;

      /* Fill in the entry in the procedure linkage table.  */
      if (! info->shared)
	{
	  if (elf_sh64_plt_entry == NULL)
	    {
	      elf_sh64_plt_entry = (bfd_big_endian (output_bfd) ?
				  elf_sh64_plt_entry_be : elf_sh64_plt_entry_le);
	    }
	  memcpy (splt->contents + h->plt.offset, elf_sh64_plt_entry,
		  elf_sh64_sizeof_plt (info));
	  movi_3shori_putval (output_bfd,
			      (sgot->output_section->vma
			       + sgot->output_offset
			       + got_offset),
			      (splt->contents + h->plt.offset
			       + elf_sh64_plt_symbol_offset (info)));

	  /* Set bottom bit because its for a branch to SHmedia */
	  movi_shori_putval (output_bfd,
			     -(h->plt.offset
			      + elf_sh64_plt_plt0_offset (info) + 8)
			     | 1,
			     (splt->contents + h->plt.offset
			      + elf_sh64_plt_plt0_offset (info)));
	}
      else
	{
	  if (elf_sh64_pic_plt_entry == NULL)
	    {
	      elf_sh64_pic_plt_entry = (bfd_big_endian (output_bfd) ?
				      elf_sh64_pic_plt_entry_be :
				      elf_sh64_pic_plt_entry_le);
	    }
	  memcpy (splt->contents + h->plt.offset, elf_sh64_pic_plt_entry,
		  elf_sh64_sizeof_plt (info));
	  movi_shori_putval (output_bfd, got_offset,
			     (splt->contents + h->plt.offset
			      + elf_sh64_plt_symbol_offset (info)));
	}

      if (info->shared)
	got_offset += GOT_BIAS;

      movi_shori_putval (output_bfd,
			 plt_index * sizeof (Elf64_External_Rela),
			 (splt->contents + h->plt.offset
			  + elf_sh64_plt_reloc_offset (info)));

      /* Fill in the entry in the global offset table.  */
      bfd_put_64 (output_bfd,
		  (splt->output_section->vma
		   + splt->output_offset
		   + h->plt.offset
		   + elf_sh64_plt_temp_offset (info)),
		  sgot->contents + got_offset);

      /* Fill in the entry in the .rela.plt section.  */
      rel.r_offset = (sgot->output_section->vma
		      + sgot->output_offset
		      + got_offset);
      rel.r_info = ELF64_R_INFO (h->dynindx, R_SH_JMP_SLOT64);
      rel.r_addend = 0;
      rel.r_addend = GOT_BIAS;
      loc = srel->contents + plt_index * sizeof (Elf64_External_Rela);
      bfd_elf64_swap_reloca_out (output_bfd, &rel, loc);

      if (!h->def_regular)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	}
    }

  if (h->got.offset != (bfd_vma) -1)
    {
      asection *sgot;
      asection *srel;
      Elf_Internal_Rela rel;
      bfd_byte *loc;

      /* This symbol has an entry in the global offset table.  Set it
	 up.  */

      sgot = bfd_get_section_by_name (dynobj, ".got");
      srel = bfd_get_section_by_name (dynobj, ".rela.got");
      BFD_ASSERT (sgot != NULL && srel != NULL);

      rel.r_offset = (sgot->output_section->vma
		      + sgot->output_offset
		      + (h->got.offset &~ 1));

      /* If this is a -Bsymbolic link, and the symbol is defined
	 locally, we just want to emit a RELATIVE reloc.  Likewise if
	 the symbol was forced to be local because of a version file.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && (info->symbolic || h->dynindx == -1)
	  && h->def_regular)
	{
	  rel.r_info = ELF64_R_INFO (0, R_SH_RELATIVE64);
	  rel.r_addend = (h->root.u.def.value
			  + h->root.u.def.section->output_section->vma
			  + h->root.u.def.section->output_offset);
	}
      else
	{
	  bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents + h->got.offset);
	  rel.r_info = ELF64_R_INFO (h->dynindx, R_SH_GLOB_DAT64);
	  rel.r_addend = 0;
	}

      loc = srel->contents;
      loc += srel->reloc_count++ * sizeof (Elf64_External_Rela);
      bfd_elf64_swap_reloca_out (output_bfd, &rel, loc);
    }

  if (h->needs_copy)
    {
      asection *s;
      Elf_Internal_Rela rel;
      bfd_byte *loc;

      /* This symbol needs a copy reloc.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1
		  && (h->root.type == bfd_link_hash_defined
		      || h->root.type == bfd_link_hash_defweak));

      s = bfd_get_section_by_name (h->root.u.def.section->owner,
				   ".rela.bss");
      BFD_ASSERT (s != NULL);

      rel.r_offset = (h->root.u.def.value
		      + h->root.u.def.section->output_section->vma
		      + h->root.u.def.section->output_offset);
      rel.r_info = ELF64_R_INFO (h->dynindx, R_SH_COPY64);
      rel.r_addend = 0;
      loc = s->contents;
      loc += s->reloc_count++ * sizeof (Elf64_External_Rela);
      bfd_elf64_swap_reloca_out (output_bfd, &rel, loc);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || h == elf_hash_table (info)->hgot)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
sh64_elf64_finish_dynamic_sections (bfd *output_bfd,
				    struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *sgot;
  asection *sdyn;

  dynobj = elf_hash_table (info)->dynobj;

  sgot = bfd_get_section_by_name (dynobj, ".got.plt");
  BFD_ASSERT (sgot != NULL);
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      asection *splt;
      Elf64_External_Dyn *dyncon, *dynconend;

      BFD_ASSERT (sdyn != NULL);

      dyncon = (Elf64_External_Dyn *) sdyn->contents;
      dynconend = (Elf64_External_Dyn *) (sdyn->contents + sdyn->size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  asection *s;
	  struct elf_link_hash_entry *h;

	  bfd_elf64_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

	    case DT_INIT:
	      name = info->init_function;
	      goto get_sym;

	    case DT_FINI:
	      name = info->fini_function;
	    get_sym:
	      if (dyn.d_un.d_val != 0)
		{
		  h = elf_link_hash_lookup (elf_hash_table (info), name,
					    FALSE, FALSE, TRUE);
		  if (h != NULL && (h->other & STO_SH5_ISA32))
		    {
		      dyn.d_un.d_val |= 1;
		      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
		    }
		}
	      break;

	    case DT_PLTGOT:
	      name = ".got";
	      goto get_vma;

	    case DT_JMPREL:
	      name = ".rela.plt";
	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_val = s->size;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_RELASZ:
	      /* My reading of the SVR4 ABI indicates that the
		 procedure linkage table relocs (DT_JMPREL) should be
		 included in the overall relocs (DT_RELA).  This is
		 what Solaris does.  However, UnixWare can not handle
		 that case.  Therefore, we override the DT_RELASZ entry
		 here to make it not include the JMPREL relocs.  Since
		 the linker script arranges for .rela.plt to follow all
		 other relocation sections, we don't have to worry
		 about changing the DT_RELA entry.  */
	      s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	      if (s != NULL)
		dyn.d_un.d_val -= s->size;
	      bfd_elf64_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;
	    }
	}

      /* Fill in the first entry in the procedure linkage table.  */
      splt = bfd_get_section_by_name (dynobj, ".plt");
      if (splt && splt->size > 0)
	{
	  if (info->shared)
	    {
	      if (elf_sh64_pic_plt_entry == NULL)
		{
		  elf_sh64_pic_plt_entry = (bfd_big_endian (output_bfd) ?
					  elf_sh64_pic_plt_entry_be :
					  elf_sh64_pic_plt_entry_le);
		}
	      memcpy (splt->contents, elf_sh64_pic_plt_entry,
		      elf_sh64_sizeof_plt (info));
	    }
	  else
	    {
	      if (elf_sh64_plt0_entry == NULL)
		{
		  elf_sh64_plt0_entry = (bfd_big_endian (output_bfd) ?
				       elf_sh64_plt0_entry_be :
				       elf_sh64_plt0_entry_le);
		}
	      memcpy (splt->contents, elf_sh64_plt0_entry, PLT_ENTRY_SIZE);
	      movi_3shori_putval (output_bfd,
				  sgot->output_section->vma
				  + sgot->output_offset,
				  splt->contents
				  + elf_sh64_plt0_gotplt_offset (info));
	    }

	  /* UnixWare sets the entsize of .plt to 8, although that doesn't
	     really seem like the right value.  */
	  elf_section_data (splt->output_section)->this_hdr.sh_entsize = 8;
	}
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot->size > 0)
    {
      if (sdyn == NULL)
	bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_64 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
      bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents + 8);
      bfd_put_64 (output_bfd, (bfd_vma) 0, sgot->contents + 16);
    }

  elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 8;

  return TRUE;
}

/* Merge non visibility st_other attribute when the symbol comes from
   a dynamic object.  */
static void
sh64_elf64_merge_symbol_attribute (struct elf_link_hash_entry *h,
				   const Elf_Internal_Sym *isym,
				   bfd_boolean definition,
				   bfd_boolean dynamic)
{
  if (isym->st_other != 0 && dynamic)
    {
      unsigned char other;

      /* Take the balance of OTHER from the definition.  */
      other = (definition ? isym->st_other : h->other);
      other &= ~ ELF_ST_VISIBILITY (-1);
      h->other = other | ELF_ST_VISIBILITY (h->other);
    }

  return;
}

static const struct bfd_elf_special_section sh64_elf64_special_sections[]=
{
  { ".cranges", 8, 0, SHT_PROGBITS, 0 },
  { NULL,       0, 0, 0,            0 }
};

#define TARGET_BIG_SYM		bfd_elf64_sh64_vec
#define TARGET_BIG_NAME		"elf64-sh64"
#define TARGET_LITTLE_SYM	bfd_elf64_sh64l_vec
#define TARGET_LITTLE_NAME	"elf64-sh64l"
#define ELF_ARCH		bfd_arch_sh
#define ELF_MACHINE_CODE	EM_SH
#define ELF_MAXPAGESIZE		128

#define elf_symbol_leading_char '_'

#define bfd_elf64_bfd_reloc_type_lookup	sh_elf64_reloc_type_lookup
#define elf_info_to_howto		sh_elf64_info_to_howto

/* Note: there's no relaxation at present.  */

#define elf_backend_relocate_section	sh_elf64_relocate_section
#define bfd_elf64_bfd_get_relocated_section_contents \
					sh_elf64_get_relocated_section_contents
#define elf_backend_object_p		sh_elf64_set_mach_from_flags
#define bfd_elf64_bfd_set_private_flags \
					sh_elf64_set_private_flags
#define bfd_elf64_bfd_copy_private_bfd_data \
					sh_elf64_copy_private_data
#define bfd_elf64_bfd_merge_private_bfd_data \
					sh_elf64_merge_private_data
#define elf_backend_fake_sections	sh64_elf64_fake_sections

#define elf_backend_gc_mark_hook        sh_elf64_gc_mark_hook
#define elf_backend_gc_sweep_hook       sh_elf64_gc_sweep_hook
#define elf_backend_check_relocs        sh_elf64_check_relocs

#define elf_backend_can_gc_sections	1

#define elf_backend_get_symbol_type	sh64_elf64_get_symbol_type

#define elf_backend_add_symbol_hook	sh64_elf64_add_symbol_hook

#define elf_backend_link_output_symbol_hook \
	sh64_elf64_link_output_symbol_hook

#define	elf_backend_merge_symbol_attribute \
	sh64_elf64_merge_symbol_attribute

#define elf_backend_final_write_processing \
 	sh64_elf64_final_write_processing

#define elf_backend_create_dynamic_sections \
					sh64_elf64_create_dynamic_sections
#define bfd_elf64_bfd_link_hash_table_create \
					sh64_elf64_link_hash_table_create
#define elf_backend_adjust_dynamic_symbol \
					sh64_elf64_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					sh64_elf64_size_dynamic_sections
#define elf_backend_finish_dynamic_symbol \
					sh64_elf64_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					sh64_elf64_finish_dynamic_sections
#define elf_backend_special_sections	sh64_elf64_special_sections

#define elf_backend_want_got_plt	1
#define elf_backend_plt_readonly	1
#define elf_backend_want_plt_sym	0
#define elf_backend_got_header_size	24

#include "elf64-target.h"

/* NetBSD support.  */
#undef	TARGET_BIG_SYM
#define	TARGET_BIG_SYM			bfd_elf64_sh64nbsd_vec
#undef	TARGET_BIG_NAME
#define	TARGET_BIG_NAME			"elf64-sh64-nbsd"
#undef	TARGET_LITTLE_SYM
#define	TARGET_LITTLE_SYM		bfd_elf64_sh64lnbsd_vec
#undef	TARGET_LITTLE_NAME
#define	TARGET_LITTLE_NAME		"elf64-sh64l-nbsd"
#undef	ELF_MAXPAGESIZE
#define	ELF_MAXPAGESIZE			0x10000
#undef	elf_symbol_leading_char
#define	elf_symbol_leading_char		0

#define	elf64_bed			elf64_sh64_nbsd_bed

#include "elf64-target.h"

/* Linux support.  */
#undef	TARGET_BIG_SYM
#define	TARGET_BIG_SYM			bfd_elf64_sh64blin_vec
#undef	TARGET_BIG_NAME
#define	TARGET_BIG_NAME			"elf64-sh64big-linux"
#undef	TARGET_LITTLE_SYM
#define	TARGET_LITTLE_SYM		bfd_elf64_sh64lin_vec
#undef	TARGET_LITTLE_NAME
#define	TARGET_LITTLE_NAME		"elf64-sh64-linux"

#define	INCLUDED_TARGET_FILE
#include "elf64-target.h"
