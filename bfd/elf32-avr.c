/* AVR-specific support for 32-bit ELF
   Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2006, 2007
   Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/avr.h"
#include "elf32-avr.h"

/* Enable debugging printout at stdout with this variable.  */
static bfd_boolean debug_relax = FALSE;

/* Enable debugging printout at stdout with this variable.  */
static bfd_boolean debug_stubs = FALSE;

/* Hash table initialization and handling.  Code is taken from the hppa port
   and adapted to the needs of AVR.  */

/* We use two hash tables to hold information for linking avr objects.

   The first is the elf32_avr_link_hash_tablse which is derived from the
   stanard ELF linker hash table.  We use this as a place to attach the other
   hash table and some static information.

   The second is the stub hash table which is derived from the base BFD
   hash table.  The stub hash table holds the information on the linker
   stubs.  */

struct elf32_avr_stub_hash_entry
{
  /* Base hash table entry structure.  */
  struct bfd_hash_entry bh_root;

  /* Offset within stub_sec of the beginning of this stub.  */
  bfd_vma stub_offset;

  /* Given the symbol's value and its section we can determine its final
     value when building the stubs (so the stub knows where to jump).  */
  bfd_vma target_value;

  /* This way we could mark stubs to be no longer necessary.  */
  bfd_boolean is_actually_needed;
};

struct elf32_avr_link_hash_table
{
  /* The main hash table.  */
  struct elf_link_hash_table etab;

  /* The stub hash table.  */
  struct bfd_hash_table bstab;

  bfd_boolean no_stubs;

  /* Linker stub bfd.  */
  bfd *stub_bfd;

  /* The stub section.  */
  asection *stub_sec;

  /* Usually 0, unless we are generating code for a bootloader.  Will
     be initialized by elf32_avr_size_stubs to the vma offset of the
     output section associated with the stub section.  */
  bfd_vma vector_base;

  /* Assorted information used by elf32_avr_size_stubs.  */
  unsigned int        bfd_count;
  int                 top_index;
  asection **         input_list;
  Elf_Internal_Sym ** all_local_syms;

  /* Tables for mapping vma beyond the 128k boundary to the address of the
     corresponding stub.  (AMT)
     "amt_max_entry_cnt" reflects the number of entries that memory is allocated
     for in the "amt_stub_offsets" and "amt_destination_addr" arrays.
     "amt_entry_cnt" informs how many of these entries actually contain
     useful data.  */
  unsigned int amt_entry_cnt;
  unsigned int amt_max_entry_cnt;
  bfd_vma *    amt_stub_offsets;
  bfd_vma *    amt_destination_addr;
};

/* Various hash macros and functions.  */
#define avr_link_hash_table(p) \
  /* PR 3874: Check that we have an AVR style hash table before using it.  */\
  ((p)->hash->table.newfunc != elf32_avr_link_hash_newfunc ? NULL : \
   ((struct elf32_avr_link_hash_table *) ((p)->hash)))

#define avr_stub_hash_entry(ent) \
  ((struct elf32_avr_stub_hash_entry *)(ent))

#define avr_stub_hash_lookup(table, string, create, copy) \
  ((struct elf32_avr_stub_hash_entry *) \
   bfd_hash_lookup ((table), (string), (create), (copy)))

static reloc_howto_type elf_avr_howto_table[] =
{
  HOWTO (R_AVR_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_AVR_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 7 bit PC relative relocation.  */
  HOWTO (R_AVR_7_PCREL,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 7,			/* bitsize */
	 TRUE,			/* pc_relative */
	 3,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_AVR_7_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 13 bit PC relative relocation.  */
  HOWTO (R_AVR_13_PCREL,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 13,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_AVR_13_PCREL",	/* name */
	 FALSE,			/* partial_inplace */
	 0xfff,			/* src_mask */
	 0xfff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* A 16 bit absolute relocation.  */
  HOWTO (R_AVR_16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_16",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 16 bit absolute relocation for command address
     Will be changed when linker stubs are needed.  */
  HOWTO (R_AVR_16_PM,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_16_PM",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_LO8_LDI,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HI8_LDI,		/* type */
	 8,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A high 6 bit absolute relocation of 22 bit address.
     For LDI command.  As well second most significant 8 bit value of
     a 32 bit link-time constant.  */
  HOWTO (R_AVR_HH8_LDI,		/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A negative low 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_LO8_LDI_NEG,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A negative high 8 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HI8_LDI_NEG,	/* type */
	 8,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A negative high 6 bit absolute relocation of 22 bit address.
     For LDI command.  */
  HOWTO (R_AVR_HH8_LDI_NEG,	/* type */
	 16,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will not be changed when linker stubs are needed. */
  HOWTO (R_AVR_LO8_LDI_PM,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI_PM",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will not be changed when linker stubs are needed. */
  HOWTO (R_AVR_HI8_LDI_PM,	/* type */
	 9,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI_PM",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will not be changed when linker stubs are needed. */
  HOWTO (R_AVR_HH8_LDI_PM,	/* type */
	 17,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI_PM",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will not be changed when linker stubs are needed. */
  HOWTO (R_AVR_LO8_LDI_PM_NEG,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LO8_LDI_PM_NEG", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will not be changed when linker stubs are needed. */
  HOWTO (R_AVR_HI8_LDI_PM_NEG,	/* type */
	 9,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HI8_LDI_PM_NEG", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will not be changed when linker stubs are needed. */
  HOWTO (R_AVR_HH8_LDI_PM_NEG,	/* type */
	 17,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_HH8_LDI_PM_NEG", /* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* Relocation for CALL command in ATmega.  */
  HOWTO (R_AVR_CALL,		/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 23,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_CALL",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),			/* pcrel_offset */
  /* A 16 bit absolute relocation of 16 bit address.
     For LDI command.  */
  HOWTO (R_AVR_LDI,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_LDI",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A 6 bit absolute relocation of 6 bit offset.
     For ldd/sdd command.  */
  HOWTO (R_AVR_6,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_6",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* A 6 bit absolute relocation of 6 bit offset.
     For sbiw/adiw command.  */
  HOWTO (R_AVR_6_ADIW,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_6_ADIW",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* Most significant 8 bit value of a 32 bit link-time constant.  */
  HOWTO (R_AVR_MS8_LDI,		/* type */
	 24,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_MS8_LDI",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
  /* Negative most significant 8 bit value of a 32 bit link-time constant.  */
  HOWTO (R_AVR_MS8_LDI_NEG,	/* type */
	 24,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_AVR_MS8_LDI_NEG",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE), 		/* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will be changed when linker stubs are needed. */
  HOWTO (R_AVR_LO8_LDI_GS,      /* type */
         1,                     /* rightshift */
         1,                     /* size (0 = byte, 1 = short, 2 = long) */
         8,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_AVR_LO8_LDI_GS",    /* name */
         FALSE,                 /* partial_inplace */
         0xffff,                /* src_mask */
         0xffff,                /* dst_mask */
         FALSE),                /* pcrel_offset */
  /* A low 8 bit absolute relocation of 24 bit program memory address.
     For LDI command.  Will be changed when linker stubs are needed. */
  HOWTO (R_AVR_HI8_LDI_GS,      /* type */
         9,                     /* rightshift */
         1,                     /* size (0 = byte, 1 = short, 2 = long) */
         8,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_AVR_HI8_LDI_GS",    /* name */
         FALSE,                 /* partial_inplace */
         0xffff,                /* src_mask */
         0xffff,                /* dst_mask */
         FALSE)                 /* pcrel_offset */
};

/* Map BFD reloc types to AVR ELF reloc types.  */

struct avr_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int elf_reloc_val;
};

static const struct avr_reloc_map avr_reloc_map[] =
{
  { BFD_RELOC_NONE,                 R_AVR_NONE },
  { BFD_RELOC_32,                   R_AVR_32 },
  { BFD_RELOC_AVR_7_PCREL,          R_AVR_7_PCREL },
  { BFD_RELOC_AVR_13_PCREL,         R_AVR_13_PCREL },
  { BFD_RELOC_16,                   R_AVR_16 },
  { BFD_RELOC_AVR_16_PM,            R_AVR_16_PM },
  { BFD_RELOC_AVR_LO8_LDI,          R_AVR_LO8_LDI},
  { BFD_RELOC_AVR_HI8_LDI,          R_AVR_HI8_LDI },
  { BFD_RELOC_AVR_HH8_LDI,          R_AVR_HH8_LDI },
  { BFD_RELOC_AVR_MS8_LDI,          R_AVR_MS8_LDI },
  { BFD_RELOC_AVR_LO8_LDI_NEG,      R_AVR_LO8_LDI_NEG },
  { BFD_RELOC_AVR_HI8_LDI_NEG,      R_AVR_HI8_LDI_NEG },
  { BFD_RELOC_AVR_HH8_LDI_NEG,      R_AVR_HH8_LDI_NEG },
  { BFD_RELOC_AVR_MS8_LDI_NEG,      R_AVR_MS8_LDI_NEG },
  { BFD_RELOC_AVR_LO8_LDI_PM,       R_AVR_LO8_LDI_PM },
  { BFD_RELOC_AVR_LO8_LDI_GS,       R_AVR_LO8_LDI_GS },
  { BFD_RELOC_AVR_HI8_LDI_PM,       R_AVR_HI8_LDI_PM },
  { BFD_RELOC_AVR_HI8_LDI_GS,       R_AVR_HI8_LDI_GS },
  { BFD_RELOC_AVR_HH8_LDI_PM,       R_AVR_HH8_LDI_PM },
  { BFD_RELOC_AVR_LO8_LDI_PM_NEG,   R_AVR_LO8_LDI_PM_NEG },
  { BFD_RELOC_AVR_HI8_LDI_PM_NEG,   R_AVR_HI8_LDI_PM_NEG },
  { BFD_RELOC_AVR_HH8_LDI_PM_NEG,   R_AVR_HH8_LDI_PM_NEG },
  { BFD_RELOC_AVR_CALL,             R_AVR_CALL },
  { BFD_RELOC_AVR_LDI,              R_AVR_LDI  },
  { BFD_RELOC_AVR_6,                R_AVR_6    },
  { BFD_RELOC_AVR_6_ADIW,           R_AVR_6_ADIW }
};

/* Meant to be filled one day with the wrap around address for the
   specific device.  I.e. should get the value 0x4000 for 16k devices,
   0x8000 for 32k devices and so on.

   We initialize it here with a value of 0x1000000 resulting in
   that we will never suggest a wrap-around jump during relaxation.
   The logic of the source code later on assumes that in
   avr_pc_wrap_around one single bit is set.  */
static bfd_vma avr_pc_wrap_around = 0x10000000;

/* If this variable holds a value different from zero, the linker relaxation
   machine will try to optimize call/ret sequences by a single jump
   instruction. This option could be switched off by a linker switch.  */
static int avr_replace_call_ret_sequences = 1;

/* Initialize an entry in the stub hash table.  */

static struct bfd_hash_entry *
stub_hash_newfunc (struct bfd_hash_entry *entry,
                   struct bfd_hash_table *table,
                   const char *string)
{
  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (entry == NULL)
    {
      entry = bfd_hash_allocate (table,
                                 sizeof (struct elf32_avr_stub_hash_entry));
      if (entry == NULL)
        return entry;
    }

  /* Call the allocation method of the superclass.  */
  entry = bfd_hash_newfunc (entry, table, string);
  if (entry != NULL)
    {
      struct elf32_avr_stub_hash_entry *hsh;

      /* Initialize the local fields.  */
      hsh = avr_stub_hash_entry (entry);
      hsh->stub_offset = 0;
      hsh->target_value = 0;
    }

  return entry;
}

/* This function is just a straight passthrough to the real
   function in linker.c.  Its prupose is so that its address
   can be compared inside the avr_link_hash_table macro.  */

static struct bfd_hash_entry *
elf32_avr_link_hash_newfunc (struct bfd_hash_entry * entry,
			     struct bfd_hash_table * table,
			     const char * string)
{
  return _bfd_elf_link_hash_newfunc (entry, table, string);
}

/* Create the derived linker hash table.  The AVR ELF port uses the derived
   hash table to keep information specific to the AVR ELF linker (without
   using static variables).  */

static struct bfd_link_hash_table *
elf32_avr_link_hash_table_create (bfd *abfd)
{
  struct elf32_avr_link_hash_table *htab;
  bfd_size_type amt = sizeof (*htab);

  htab = bfd_malloc (amt);
  if (htab == NULL)
    return NULL;

  if (!_bfd_elf_link_hash_table_init (&htab->etab, abfd,
                                      elf32_avr_link_hash_newfunc,
                                      sizeof (struct elf_link_hash_entry)))
    {
      free (htab);
      return NULL;
    }

  /* Init the stub hash table too.  */
  if (!bfd_hash_table_init (&htab->bstab, stub_hash_newfunc,
                            sizeof (struct elf32_avr_stub_hash_entry)))
    return NULL;

  htab->stub_bfd = NULL;
  htab->stub_sec = NULL;

  /* Initialize the address mapping table.  */
  htab->amt_stub_offsets = NULL;
  htab->amt_destination_addr = NULL;
  htab->amt_entry_cnt = 0;
  htab->amt_max_entry_cnt = 0;

  return &htab->etab.root;
}

/* Free the derived linker hash table.  */

static void
elf32_avr_link_hash_table_free (struct bfd_link_hash_table *btab)
{
  struct elf32_avr_link_hash_table *htab
    = (struct elf32_avr_link_hash_table *) btab;

  /* Free the address mapping table.  */
  if (htab->amt_stub_offsets != NULL)
    free (htab->amt_stub_offsets);
  if (htab->amt_destination_addr != NULL)
    free (htab->amt_destination_addr);

  bfd_hash_table_free (&htab->bstab);
  _bfd_generic_link_hash_table_free (btab);
}

/* Calculates the effective distance of a pc relative jump/call.  */

static int
avr_relative_distance_considering_wrap_around (unsigned int distance)
{
  unsigned int wrap_around_mask = avr_pc_wrap_around - 1;
  int dist_with_wrap_around = distance & wrap_around_mask;

  if (dist_with_wrap_around > ((int) (avr_pc_wrap_around >> 1)))
    dist_with_wrap_around -= avr_pc_wrap_around;

  return dist_with_wrap_around;
}


static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0;
       i < sizeof (avr_reloc_map) / sizeof (struct avr_reloc_map);
       i++)
    if (avr_reloc_map[i].bfd_reloc_val == code)
      return &elf_avr_howto_table[avr_reloc_map[i].elf_reloc_val];

  return NULL;
}

static reloc_howto_type *
bfd_elf32_bfd_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 const char *r_name)
{
  unsigned int i;

  for (i = 0;
       i < sizeof (elf_avr_howto_table) / sizeof (elf_avr_howto_table[0]);
       i++)
    if (elf_avr_howto_table[i].name != NULL
	&& strcasecmp (elf_avr_howto_table[i].name, r_name) == 0)
      return &elf_avr_howto_table[i];

  return NULL;
}

/* Set the howto pointer for an AVR ELF reloc.  */

static void
avr_info_to_howto_rela (bfd *abfd ATTRIBUTE_UNUSED,
			arelent *cache_ptr,
			Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_AVR_max);
  cache_ptr->howto = &elf_avr_howto_table[r_type];
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
elf32_avr_check_relocs (bfd *abfd,
			struct bfd_link_info *info,
			asection *sec,
			const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}
    }

  return TRUE;
}

static bfd_boolean
avr_stub_is_required_for_16_bit_reloc (bfd_vma relocation)
{
  return (relocation >= 0x020000);
}

/* Returns the address of the corresponding stub if there is one.
   Returns otherwise an address above 0x020000.  This function
   could also be used, if there is no knowledge on the section where
   the destination is found.  */

static bfd_vma
avr_get_stub_addr (bfd_vma srel,
                   struct elf32_avr_link_hash_table *htab)
{
  unsigned int index;
  bfd_vma stub_sec_addr =
              (htab->stub_sec->output_section->vma +
	       htab->stub_sec->output_offset);

  for (index = 0; index < htab->amt_max_entry_cnt; index ++)
    if (htab->amt_destination_addr[index] == srel)
      return htab->amt_stub_offsets[index] + stub_sec_addr;

  /* Return an address that could not be reached by 16 bit relocs.  */
  return 0x020000;
}

/* Perform a single relocation.  By default we use the standard BFD
   routines, but a few relocs, we have to do them ourselves.  */

static bfd_reloc_status_type
avr_final_link_relocate (reloc_howto_type *                 howto,
			 bfd *                              input_bfd,
			 asection *                         input_section,
			 bfd_byte *                         contents,
			 Elf_Internal_Rela *                rel,
                         bfd_vma                            relocation,
                         struct elf32_avr_link_hash_table * htab)
{
  bfd_reloc_status_type r = bfd_reloc_ok;
  bfd_vma               x;
  bfd_signed_vma	srel;
  bfd_signed_vma	reloc_addr;
  bfd_boolean           use_stubs = FALSE;
  /* Usually is 0, unless we are generating code for a bootloader.  */
  bfd_signed_vma        base_addr = htab->vector_base;

  /* Absolute addr of the reloc in the final excecutable.  */
  reloc_addr = rel->r_offset + input_section->output_section->vma
	       + input_section->output_offset;

  switch (howto->type)
    {
    case R_AVR_7_PCREL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      srel -= 2;	/* Branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;
      if (srel > ((1 << 7) - 1) || (srel < - (1 << 7)))
	return bfd_reloc_overflow;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xfc07) | (((srel >> 1) << 3) & 0x3f8);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_13_PCREL:
      contents   += rel->r_offset;
      srel = (bfd_signed_vma) relocation;
      srel += rel->r_addend;
      srel -= rel->r_offset;
      srel -= 2;	/* Branch instructions add 2 to the PC...  */
      srel -= (input_section->output_section->vma +
	       input_section->output_offset);

      if (srel & 1)
	return bfd_reloc_outofrange;

      srel = avr_relative_distance_considering_wrap_around (srel);

      /* AVR addresses commands as words.  */
      srel >>= 1;

      /* Check for overflow.  */
      if (srel < -2048 || srel > 2047)
	{
          /* Relative distance is too large.  */

	  /* Always apply WRAPAROUND for avr2 and avr4.  */
	  switch (bfd_get_mach (input_bfd))
	    {
	    case bfd_mach_avr2:
	    case bfd_mach_avr4:
	      break;

	    default:
	      return bfd_reloc_overflow;
	    }
	}

      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf000) | (srel & 0xfff);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (((srel > 0) && (srel & 0xffff) > 255)
	  || ((srel < 0) && ((-srel) & 0xffff) > 128))
        /* Remove offset for data/eeprom section.  */
        return bfd_reloc_overflow;

      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_6:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (((srel & 0xffff) > 63) || (srel < 0))
	/* Remove offset for data/eeprom section.  */
	return bfd_reloc_overflow;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xd3f8) | ((srel & 7) | ((srel & (3 << 3)) << 7)
                       | ((srel & (1 << 5)) << 8));
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_6_ADIW:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (((srel & 0xffff) > 63) || (srel < 0))
	/* Remove offset for data/eeprom section.  */
	return bfd_reloc_overflow;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xff30) | (srel & 0xf) | ((srel & 0x30) << 2);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_MS8_LDI:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = (srel >> 24) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_MS8_LDI_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      srel = (srel >> 24) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI_GS:
      use_stubs = (!htab->no_stubs);
      /* Fall through.  */
    case R_AVR_LO8_LDI_PM:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;

      if (use_stubs
          && avr_stub_is_required_for_16_bit_reloc (srel - base_addr))
        {
          bfd_vma old_srel = srel;

          /* We need to use the address of the stub instead.  */
          srel = avr_get_stub_addr (srel, htab);
          if (debug_stubs)
            printf ("LD: Using jump stub (at 0x%x) with destination 0x%x for "
                    "reloc at address 0x%x.\n",
                    (unsigned int) srel,
                    (unsigned int) old_srel,
                    (unsigned int) reloc_addr);

	  if (avr_stub_is_required_for_16_bit_reloc (srel - base_addr))
	    return bfd_reloc_outofrange;
        }

      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI_GS:
      use_stubs = (!htab->no_stubs);
      /* Fall through.  */
    case R_AVR_HI8_LDI_PM:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;

      if (use_stubs
          && avr_stub_is_required_for_16_bit_reloc (srel - base_addr))
        {
          bfd_vma old_srel = srel;

          /* We need to use the address of the stub instead.  */
          srel = avr_get_stub_addr (srel, htab);
          if (debug_stubs)
            printf ("LD: Using jump stub (at 0x%x) with destination 0x%x for "
                    "reloc at address 0x%x.\n",
                    (unsigned int) srel,
                    (unsigned int) old_srel,
                    (unsigned int) reloc_addr);

	  if (avr_stub_is_required_for_16_bit_reloc (srel - base_addr))
	    return bfd_reloc_outofrange;
        }

      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI_PM:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_LO8_LDI_PM_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HI8_LDI_PM_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 8) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_HH8_LDI_PM_NEG:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      srel = -srel;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      srel = (srel >> 16) & 0xff;
      x = bfd_get_16 (input_bfd, contents);
      x = (x & 0xf0f0) | (srel & 0xf) | ((srel << 4) & 0xf00);
      bfd_put_16 (input_bfd, x, contents);
      break;

    case R_AVR_CALL:
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;
      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      x = bfd_get_16 (input_bfd, contents);
      x |= ((srel & 0x10000) | ((srel << 3) & 0x1f00000)) >> 16;
      bfd_put_16 (input_bfd, x, contents);
      bfd_put_16 (input_bfd, (bfd_vma) srel & 0xffff, contents+2);
      break;

    case R_AVR_16_PM:
      use_stubs = (!htab->no_stubs);
      contents += rel->r_offset;
      srel = (bfd_signed_vma) relocation + rel->r_addend;

      if (use_stubs
          && avr_stub_is_required_for_16_bit_reloc (srel - base_addr))
        {
          bfd_vma old_srel = srel;

          /* We need to use the address of the stub instead.  */
          srel = avr_get_stub_addr (srel,htab);
          if (debug_stubs)
            printf ("LD: Using jump stub (at 0x%x) with destination 0x%x for "
                    "reloc at address 0x%x.\n",
                    (unsigned int) srel,
                    (unsigned int) old_srel,
                    (unsigned int) reloc_addr);

	  if (avr_stub_is_required_for_16_bit_reloc (srel - base_addr))
	    return bfd_reloc_outofrange;
        }

      if (srel & 1)
	return bfd_reloc_outofrange;
      srel = srel >> 1;
      bfd_put_16 (input_bfd, (bfd_vma) srel &0x00ffff, contents);
      break;

    default:
      r = _bfd_final_link_relocate (howto, input_bfd, input_section,
				    contents, rel->r_offset,
				    relocation, rel->r_addend);
    }

  return r;
}

/* Relocate an AVR ELF section.  */

static bfd_boolean
elf32_avr_relocate_section (bfd *output_bfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info *info,
			    bfd *input_bfd,
			    asection *input_section,
			    bfd_byte *contents,
			    Elf_Internal_Rela *relocs,
			    Elf_Internal_Sym *local_syms,
			    asection **local_sections)
{
  Elf_Internal_Shdr *           symtab_hdr;
  struct elf_link_hash_entry ** sym_hashes;
  Elf_Internal_Rela *           rel;
  Elf_Internal_Rela *           relend;
  struct elf32_avr_link_hash_table * htab = avr_link_hash_table (info);

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name;
      int                          r_type;

      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = elf_avr_howto_table + ELF32_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, sec) : name;
	}
      else
	{
	  bfd_boolean unresolved_reloc, warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  name = h->root.root.string;
	}

      if (sec != NULL && elf_discarded_section (sec))
	{
	  /* For relocs against symbols from removed linkonce sections,
	     or sections discarded by a linker script, we just want the
	     section contents zeroed.  Avoid any special processing.  */
	  _bfd_clear_contents (howto, input_bfd, contents + rel->r_offset);
	  rel->r_info = 0;
	  rel->r_addend = 0;
	  continue;
	}

      if (info->relocatable)
	continue;

      r = avr_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel, relocation, htab);

      if (r != bfd_reloc_ok)
	{
	  const char * msg = (const char *) NULL;

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      r = info->callbacks->reloc_overflow
		(info, (h ? &h->root : NULL),
		 name, howto->name, (bfd_vma) 0,
		 input_bfd, input_section, rel->r_offset);
	      break;

	    case bfd_reloc_undefined:
	      r = info->callbacks->undefined_symbol
		(info, name, input_bfd, input_section, rel->r_offset, TRUE);
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      break;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      break;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous relocation");
	      break;

	    default:
	      msg = _("internal error: unknown error");
	      break;
	    }

	  if (msg)
	    r = info->callbacks->warning
	      (info, msg, name, input_bfd, input_section, rel->r_offset);

	  if (! r)
	    return FALSE;
	}
    }

  return TRUE;
}

/* The final processing done just before writing out a AVR ELF object
   file.  This gets the AVR architecture right based on the machine
   number.  */

static void
bfd_elf_avr_final_write_processing (bfd *abfd,
				    bfd_boolean linker ATTRIBUTE_UNUSED)
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_avr2:
      val = E_AVR_MACH_AVR2;
      break;

    case bfd_mach_avr1:
      val = E_AVR_MACH_AVR1;
      break;

    case bfd_mach_avr3:
      val = E_AVR_MACH_AVR3;
      break;

    case bfd_mach_avr4:
      val = E_AVR_MACH_AVR4;
      break;

    case bfd_mach_avr5:
      val = E_AVR_MACH_AVR5;
      break;

    case bfd_mach_avr6:
      val = E_AVR_MACH_AVR6;
      break;
    }

  elf_elfheader (abfd)->e_machine = EM_AVR;
  elf_elfheader (abfd)->e_flags &= ~ EF_AVR_MACH;
  elf_elfheader (abfd)->e_flags |= val;
  elf_elfheader (abfd)->e_flags |= EF_AVR_LINKRELAX_PREPARED;
}

/* Set the right machine number.  */

static bfd_boolean
elf32_avr_object_p (bfd *abfd)
{
  unsigned int e_set = bfd_mach_avr2;

  if (elf_elfheader (abfd)->e_machine == EM_AVR
      || elf_elfheader (abfd)->e_machine == EM_AVR_OLD)
    {
      int e_mach = elf_elfheader (abfd)->e_flags & EF_AVR_MACH;

      switch (e_mach)
	{
	default:
	case E_AVR_MACH_AVR2:
	  e_set = bfd_mach_avr2;
	  break;

	case E_AVR_MACH_AVR1:
	  e_set = bfd_mach_avr1;
	  break;

	case E_AVR_MACH_AVR3:
	  e_set = bfd_mach_avr3;
	  break;

	case E_AVR_MACH_AVR4:
	  e_set = bfd_mach_avr4;
	  break;

	case E_AVR_MACH_AVR5:
	  e_set = bfd_mach_avr5;
	  break;

	case E_AVR_MACH_AVR6:
	  e_set = bfd_mach_avr6;
	  break;
	}
    }
  return bfd_default_set_arch_mach (abfd, bfd_arch_avr,
				    e_set);
}


/* Delete some bytes from a section while changing the size of an instruction.
   The parameter "addr" denotes the section-relative offset pointing just
   behind the shrinked instruction. "addr+count" point at the first
   byte just behind the original unshrinked instruction.  */

static bfd_boolean
elf32_avr_relax_delete_bytes (bfd *abfd,
                              asection *sec,
                              bfd_vma addr,
                              int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymbuf = NULL;
  Elf_Internal_Sym *isymend;
  bfd_vma toaddr;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);
  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  if (toaddr - addr - count > 0)
    memmove (contents + addr, contents + addr + count,
             (size_t) (toaddr - addr - count));
  sec->size -= count;

  /* Adjust all the reloc addresses.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      bfd_vma old_reloc_address;
      bfd_vma shrinked_insn_address;

      old_reloc_address = (sec->output_section->vma
                           + sec->output_offset + irel->r_offset);
      shrinked_insn_address = (sec->output_section->vma
                              + sec->output_offset + addr - count);

      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
           && irel->r_offset < toaddr))
        {
          if (debug_relax)
            printf ("Relocation at address 0x%x needs to be moved.\n"
                    "Old section offset: 0x%x, New section offset: 0x%x \n",
                    (unsigned int) old_reloc_address,
                    (unsigned int) irel->r_offset,
                    (unsigned int) ((irel->r_offset) - count));

          irel->r_offset -= count;
        }

    }

   /* The reloc's own addresses are now ok. However, we need to readjust
      the reloc's addend, i.e. the reloc's value if two conditions are met:
      1.) the reloc is relative to a symbol in this section that
          is located in front of the shrinked instruction
      2.) symbol plus addend end up behind the shrinked instruction.

      The most common case where this happens are relocs relative to
      the section-start symbol.

      This step needs to be done for all of the sections of the bfd.  */

  {
    struct bfd_section *isec;

    for (isec = abfd->sections; isec; isec = isec->next)
     {
       bfd_vma symval;
       bfd_vma shrinked_insn_address;

       shrinked_insn_address = (sec->output_section->vma
                                + sec->output_offset + addr - count);

       irelend = elf_section_data (isec)->relocs + isec->reloc_count;
       for (irel = elf_section_data (isec)->relocs;
            irel < irelend;
            irel++)
         {
           /* Read this BFD's local symbols if we haven't done
              so already.  */
           if (isymbuf == NULL && symtab_hdr->sh_info != 0)
             {
               isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
               if (isymbuf == NULL)
                 isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
                                                 symtab_hdr->sh_info, 0,
                                                 NULL, NULL, NULL);
               if (isymbuf == NULL)
                 return FALSE;
             }

           /* Get the value of the symbol referred to by the reloc.  */
           if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
             {
               /* A local symbol.  */
               Elf_Internal_Sym *isym;
               asection *sym_sec;

               isym = isymbuf + ELF32_R_SYM (irel->r_info);
               sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
               symval = isym->st_value;
               /* If the reloc is absolute, it will not have
                  a symbol or section associated with it.  */
               if (sym_sec == sec)
                 {
                   symval += sym_sec->output_section->vma
                             + sym_sec->output_offset;

                   if (debug_relax)
                     printf ("Checking if the relocation's "
                             "addend needs corrections.\n"
                             "Address of anchor symbol: 0x%x \n"
                             "Address of relocation target: 0x%x \n"
                             "Address of relaxed insn: 0x%x \n",
                             (unsigned int) symval,
                             (unsigned int) (symval + irel->r_addend),
                             (unsigned int) shrinked_insn_address);

                   if (symval <= shrinked_insn_address
                       && (symval + irel->r_addend) > shrinked_insn_address)
                     {
                       irel->r_addend -= count;

                       if (debug_relax)
                         printf ("Relocation's addend needed to be fixed \n");
                     }
                 }
	       /* else...Reference symbol is absolute.  No adjustment needed.  */
	     }
	   /* else...Reference symbol is extern.  No need for adjusting
	      the addend.  */
	 }
     }
  }

  /* Adjust the local symbols defined in this section.  */
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  isymend = isym + symtab_hdr->sh_info;
  for (; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
          && isym->st_value > addr
          && isym->st_value < toaddr)
        isym->st_value -= count;
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
              - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
           || sym_hash->root.type == bfd_link_hash_defweak)
          && sym_hash->root.u.def.section == sec
          && sym_hash->root.u.def.value > addr
          && sym_hash->root.u.def.value < toaddr)
        {
          sym_hash->root.u.def.value -= count;
        }
    }

  return TRUE;
}

/* This function handles relaxing for the avr.
   Many important relaxing opportunities within functions are already
   realized by the compiler itself.
   Here we try to replace  call (4 bytes) ->  rcall (2 bytes)
   and jump -> rjmp (safes also 2 bytes).
   As well we now optimize seqences of
     - call/rcall function
     - ret
   to yield
     - jmp/rjmp function
     - ret
   . In case that within a sequence
     - jmp/rjmp label
     - ret
   the ret could no longer be reached it is optimized away. In order
   to check if the ret is no longer needed, it is checked that the ret's address
   is not the target of a branch or jump within the same section, it is checked
   that there is no skip instruction before the jmp/rjmp and that there
   is no local or global label place at the address of the ret.

   We refrain from relaxing within sections ".vectors" and
   ".jumptables" in order to maintain the position of the instructions.
   There, however, we substitute jmp/call by a sequence rjmp,nop/rcall,nop
   if possible. (In future one could possibly use the space of the nop
   for the first instruction of the irq service function.

   The .jumptables sections is meant to be used for a future tablejump variant
   for the devices with 3-byte program counter where the table itself
   contains 4-byte jump instructions whose relative offset must not
   be changed.  */

static bfd_boolean
elf32_avr_relax_section (bfd *abfd,
			 asection *sec,
                         struct bfd_link_info *link_info,
                         bfd_boolean *again)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  static asection *last_input_section = NULL;
  static Elf_Internal_Rela *last_reloc = NULL;
  struct elf32_avr_link_hash_table *htab;

  htab = avr_link_hash_table (link_info);
  if (htab == NULL)
    return FALSE;

  /* Assume nothing changes.  */
  *again = FALSE;

  if ((!htab->no_stubs) && (sec == htab->stub_sec))
    {
      /* We are just relaxing the stub section.
	 Let's calculate the size needed again.  */
      bfd_size_type last_estimated_stub_section_size = htab->stub_sec->size;

      if (debug_relax)
        printf ("Relaxing the stub section. Size prior to this pass: %i\n",
                (int) last_estimated_stub_section_size);

      elf32_avr_size_stubs (htab->stub_sec->output_section->owner,
                            link_info, FALSE);

      /* Check if the number of trampolines changed.  */
      if (last_estimated_stub_section_size != htab->stub_sec->size)
        *again = TRUE;

      if (debug_relax)
        printf ("Size of stub section after this pass: %i\n",
                (int) htab->stub_sec->size);

      return TRUE;
    }

  /* We don't have to do anything for a relocatable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  /* Check if the object file to relax uses internal symbols so that we
     could fix up the relocations.  */
  if (!(elf_elfheader (abfd)->e_flags & EF_AVR_LINKRELAX_PREPARED))
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf_link_read_relocs
                     (abfd, sec, NULL, NULL, link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  if (sec != last_input_section)
    last_reloc = NULL;

  last_input_section = sec;

  /* Walk through the relocs looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      if (   ELF32_R_TYPE (irel->r_info) != R_AVR_13_PCREL
          && ELF32_R_TYPE (irel->r_info) != R_AVR_7_PCREL
          && ELF32_R_TYPE (irel->r_info) != R_AVR_CALL)
        continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
        {
          /* Get cached copy if it exists.  */
          if (elf_section_data (sec)->this_hdr.contents != NULL)
            contents = elf_section_data (sec)->this_hdr.contents;
          else
            {
              /* Go get them off disk.  */
              if (! bfd_malloc_and_get_section (abfd, sec, &contents))
                goto error_return;
            }
        }

     /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL && symtab_hdr->sh_info != 0)
        {
          isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
          if (isymbuf == NULL)
            isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
                                            symtab_hdr->sh_info, 0,
                                            NULL, NULL, NULL);
          if (isymbuf == NULL)
            goto error_return;
        }


      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
        {
          /* A local symbol.  */
          Elf_Internal_Sym *isym;
          asection *sym_sec;

          isym = isymbuf + ELF32_R_SYM (irel->r_info);
          sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
          symval = isym->st_value;
          /* If the reloc is absolute, it will not have
             a symbol or section associated with it.  */
          if (sym_sec)
            symval += sym_sec->output_section->vma
              + sym_sec->output_offset;
        }
      else
        {
          unsigned long indx;
          struct elf_link_hash_entry *h;

          /* An external symbol.  */
          indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
          h = elf_sym_hashes (abfd)[indx];
          BFD_ASSERT (h != NULL);
          if (h->root.type != bfd_link_hash_defined
              && h->root.type != bfd_link_hash_defweak)
	    /* This appears to be a reference to an undefined
	       symbol.  Just ignore it--it will be caught by the
	       regular reloc processing.  */
	    continue;

          symval = (h->root.u.def.value
                    + h->root.u.def.section->output_section->vma
                    + h->root.u.def.section->output_offset);
        }

      /* For simplicity of coding, we are going to modify the section
         contents, the section relocs, and the BFD symbol table.  We
         must tell the rest of the code not to free up this
         information.  It would be possible to instead create a table
         of changes which have to be made, as is done in coff-mips.c;
         that would be more work, but would require less memory when
         the linker is run.  */
      switch (ELF32_R_TYPE (irel->r_info))
        {
         /* Try to turn a 22-bit absolute call/jump into an 13-bit
            pc-relative rcall/rjmp.  */
         case R_AVR_CALL:
          {
            bfd_vma value = symval + irel->r_addend;
            bfd_vma dot, gap;
            int distance_short_enough = 0;

            /* Get the address of this instruction.  */
            dot = (sec->output_section->vma
                   + sec->output_offset + irel->r_offset);

            /* Compute the distance from this insn to the branch target.  */
            gap = value - dot;

            /* If the distance is within -4094..+4098 inclusive, then we can
               relax this jump/call.  +4098 because the call/jump target
               will be closer after the relaxation.  */
            if ((int) gap >= -4094 && (int) gap <= 4098)
              distance_short_enough = 1;

            /* Here we handle the wrap-around case.  E.g. for a 16k device
               we could use a rjmp to jump from address 0x100 to 0x3d00!
               In order to make this work properly, we need to fill the
               vaiable avr_pc_wrap_around with the appropriate value.
               I.e. 0x4000 for a 16k device.  */
            {
               /* Shrinking the code size makes the gaps larger in the
                  case of wrap-arounds.  So we use a heuristical safety
                  margin to avoid that during relax the distance gets
                  again too large for the short jumps.  Let's assume
                  a typical code-size reduction due to relax for a
                  16k device of 600 bytes.  So let's use twice the
                  typical value as safety margin.  */
               int rgap;
               int safety_margin;

               int assumed_shrink = 600;
               if (avr_pc_wrap_around > 0x4000)
                 assumed_shrink = 900;

               safety_margin = 2 * assumed_shrink;

               rgap = avr_relative_distance_considering_wrap_around (gap);

               if (rgap >= (-4092 + safety_margin)
                   && rgap <= (4094 - safety_margin))
		 distance_short_enough = 1;
            }

            if (distance_short_enough)
              {
                unsigned char code_msb;
                unsigned char code_lsb;

                if (debug_relax)
                  printf ("shrinking jump/call instruction at address 0x%x"
                          " in section %s\n\n",
                          (int) dot, sec->name);

                /* Note that we've changed the relocs, section contents,
                   etc.  */
                elf_section_data (sec)->relocs = internal_relocs;
                elf_section_data (sec)->this_hdr.contents = contents;
                symtab_hdr->contents = (unsigned char *) isymbuf;

                /* Get the instruction code for relaxing.  */
                code_lsb = bfd_get_8 (abfd, contents + irel->r_offset);
                code_msb = bfd_get_8 (abfd, contents + irel->r_offset + 1);

                /* Mask out the relocation bits.  */
                code_msb &= 0x94;
                code_lsb &= 0x0E;
                if (code_msb == 0x94 && code_lsb == 0x0E)
                  {
                    /* we are changing call -> rcall .  */
                    bfd_put_8 (abfd, 0x00, contents + irel->r_offset);
                    bfd_put_8 (abfd, 0xD0, contents + irel->r_offset + 1);
                  }
                else if (code_msb == 0x94 && code_lsb == 0x0C)
                  {
                    /* we are changeing jump -> rjmp.  */
                    bfd_put_8 (abfd, 0x00, contents + irel->r_offset);
                    bfd_put_8 (abfd, 0xC0, contents + irel->r_offset + 1);
                  }
                else
                  abort ();

                /* Fix the relocation's type.  */
                irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
                                             R_AVR_13_PCREL);

                /* Check for the vector section. There we don't want to
                   modify the ordering!  */

                if (!strcmp (sec->name,".vectors")
                    || !strcmp (sec->name,".jumptables"))
                  {
                    /* Let's insert a nop.  */
                    bfd_put_8 (abfd, 0x00, contents + irel->r_offset + 2);
                    bfd_put_8 (abfd, 0x00, contents + irel->r_offset + 3);
                  }
                else
                  {
                    /* Delete two bytes of data.  */
                    if (!elf32_avr_relax_delete_bytes (abfd, sec,
                                                       irel->r_offset + 2, 2))
                      goto error_return;

                    /* That will change things, so, we should relax again.
                       Note that this is not required, and it may be slow.  */
                    *again = TRUE;
                  }
              }
          }

        default:
          {
            unsigned char code_msb;
            unsigned char code_lsb;
            bfd_vma dot;

            code_msb = bfd_get_8 (abfd, contents + irel->r_offset + 1);
            code_lsb = bfd_get_8 (abfd, contents + irel->r_offset + 0);

            /* Get the address of this instruction.  */
            dot = (sec->output_section->vma
                   + sec->output_offset + irel->r_offset);

            /* Here we look for rcall/ret or call/ret sequences that could be
               safely replaced by rjmp/ret or jmp/ret.  */
            if (((code_msb & 0xf0) == 0xd0)
                && avr_replace_call_ret_sequences)
              {
                /* This insn is a rcall.  */
                unsigned char next_insn_msb = 0;
                unsigned char next_insn_lsb = 0;

                if (irel->r_offset + 3 < sec->size)
                  {
                    next_insn_msb =
                        bfd_get_8 (abfd, contents + irel->r_offset + 3);
                    next_insn_lsb =
                        bfd_get_8 (abfd, contents + irel->r_offset + 2);
                  }

		if ((0x95 == next_insn_msb) && (0x08 == next_insn_lsb))
                  {
                    /* The next insn is a ret. We now convert the rcall insn
                       into a rjmp instruction.  */
                    code_msb &= 0xef;
                    bfd_put_8 (abfd, code_msb, contents + irel->r_offset + 1);
                    if (debug_relax)
                      printf ("converted rcall/ret sequence at address 0x%x"
                              " into rjmp/ret sequence. Section is %s\n\n",
                              (int) dot, sec->name);
                    *again = TRUE;
                    break;
                  }
              }
            else if ((0x94 == (code_msb & 0xfe))
		     && (0x0e == (code_lsb & 0x0e))
		     && avr_replace_call_ret_sequences)
              {
                /* This insn is a call.  */
                unsigned char next_insn_msb = 0;
                unsigned char next_insn_lsb = 0;

                if (irel->r_offset + 5 < sec->size)
                  {
                    next_insn_msb =
                        bfd_get_8 (abfd, contents + irel->r_offset + 5);
                    next_insn_lsb =
                        bfd_get_8 (abfd, contents + irel->r_offset + 4);
                  }

                if ((0x95 == next_insn_msb) && (0x08 == next_insn_lsb))
                  {
                    /* The next insn is a ret. We now convert the call insn
                       into a jmp instruction.  */

                    code_lsb &= 0xfd;
                    bfd_put_8 (abfd, code_lsb, contents + irel->r_offset);
                    if (debug_relax)
                      printf ("converted call/ret sequence at address 0x%x"
                              " into jmp/ret sequence. Section is %s\n\n",
                              (int) dot, sec->name);
                    *again = TRUE;
                    break;
                  }
              }
            else if ((0xc0 == (code_msb & 0xf0))
                     || ((0x94 == (code_msb & 0xfe))
                         && (0x0c == (code_lsb & 0x0e))))
              {
                /* This insn is a rjmp or a jmp.  */
                unsigned char next_insn_msb = 0;
                unsigned char next_insn_lsb = 0;
                int insn_size;

                if (0xc0 == (code_msb & 0xf0))
                  insn_size = 2; /* rjmp insn */
                else
                  insn_size = 4; /* jmp insn */

                if (irel->r_offset + insn_size + 1 < sec->size)
                  {
                    next_insn_msb =
                        bfd_get_8 (abfd, contents + irel->r_offset
                                         + insn_size + 1);
                    next_insn_lsb =
                        bfd_get_8 (abfd, contents + irel->r_offset
                                         + insn_size);
                  }

                if ((0x95 == next_insn_msb) && (0x08 == next_insn_lsb))
                  {
                    /* The next insn is a ret. We possibly could delete
                       this ret. First we need to check for preceeding
                       sbis/sbic/sbrs or cpse "skip" instructions.  */

                    int there_is_preceeding_non_skip_insn = 1;
                    bfd_vma address_of_ret;

                    address_of_ret = dot + insn_size;

                    if (debug_relax && (insn_size == 2))
                      printf ("found rjmp / ret sequence at address 0x%x\n",
                              (int) dot);
                    if (debug_relax && (insn_size == 4))
                      printf ("found jmp / ret sequence at address 0x%x\n",
                              (int) dot);

                    /* We have to make sure that there is a preceeding insn.  */
                    if (irel->r_offset >= 2)
                      {
                        unsigned char preceeding_msb;
                        unsigned char preceeding_lsb;
                        preceeding_msb =
                            bfd_get_8 (abfd, contents + irel->r_offset - 1);
                        preceeding_lsb =
                            bfd_get_8 (abfd, contents + irel->r_offset - 2);

                        /* sbic.  */
                        if (0x99 == preceeding_msb)
                          there_is_preceeding_non_skip_insn = 0;

                        /* sbis.  */
                        if (0x9b == preceeding_msb)
                          there_is_preceeding_non_skip_insn = 0;

                        /* sbrc */
                        if ((0xfc == (preceeding_msb & 0xfe)
                            && (0x00 == (preceeding_lsb & 0x08))))
                          there_is_preceeding_non_skip_insn = 0;

                        /* sbrs */
                        if ((0xfe == (preceeding_msb & 0xfe)
                            && (0x00 == (preceeding_lsb & 0x08))))
                          there_is_preceeding_non_skip_insn = 0;

                        /* cpse */
                        if (0x10 == (preceeding_msb & 0xfc))
                          there_is_preceeding_non_skip_insn = 0;

                        if (there_is_preceeding_non_skip_insn == 0)
                          if (debug_relax)
                            printf ("preceeding skip insn prevents deletion of"
                                    " ret insn at addr 0x%x in section %s\n",
                                    (int) dot + 2, sec->name);
                      }
                    else
                      {
                        /* There is no previous instruction.  */
                        there_is_preceeding_non_skip_insn = 0;
                      }

                    if (there_is_preceeding_non_skip_insn)
                      {
                        /* We now only have to make sure that there is no
                           local label defined at the address of the ret
                           instruction and that there is no local relocation
                           in this section pointing to the ret.  */

                        int deleting_ret_is_safe = 1;
                        unsigned int section_offset_of_ret_insn =
                                          irel->r_offset + insn_size;
                        Elf_Internal_Sym *isym, *isymend;
                        unsigned int sec_shndx;

                        sec_shndx =
			  _bfd_elf_section_from_bfd_section (abfd, sec);

                        /* Check for local symbols.  */
                        isym = (Elf_Internal_Sym *) symtab_hdr->contents;
                        isymend = isym + symtab_hdr->sh_info;
                        for (; isym < isymend; isym++)
                         {
                           if (isym->st_value == section_offset_of_ret_insn
                               && isym->st_shndx == sec_shndx)
                             {
                               deleting_ret_is_safe = 0;
                               if (debug_relax)
                                 printf ("local label prevents deletion of ret "
                                         "insn at address 0x%x\n",
                                         (int) dot + insn_size);
                             }
                         }

                         /* Now check for global symbols.  */
                         {
                           int symcount;
                           struct elf_link_hash_entry **sym_hashes;
                           struct elf_link_hash_entry **end_hashes;

                           symcount = (symtab_hdr->sh_size
                                       / sizeof (Elf32_External_Sym)
                                       - symtab_hdr->sh_info);
                           sym_hashes = elf_sym_hashes (abfd);
                           end_hashes = sym_hashes + symcount;
                           for (; sym_hashes < end_hashes; sym_hashes++)
                            {
                              struct elf_link_hash_entry *sym_hash =
                                                                 *sym_hashes;
                              if ((sym_hash->root.type == bfd_link_hash_defined
                                  || sym_hash->root.type ==
				   bfd_link_hash_defweak)
                                  && sym_hash->root.u.def.section == sec
                                  && sym_hash->root.u.def.value == section_offset_of_ret_insn)
                                {
                                  deleting_ret_is_safe = 0;
                                  if (debug_relax)
                                    printf ("global label prevents deletion of "
                                            "ret insn at address 0x%x\n",
                                            (int) dot + insn_size);
                                }
                            }
                         }
                         /* Now we check for relocations pointing to ret.  */
                         {
                           Elf_Internal_Rela *irel;
                           Elf_Internal_Rela *relend;
                           Elf_Internal_Shdr *symtab_hdr;

                           symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
                           relend = elf_section_data (sec)->relocs
                                    + sec->reloc_count;

                           for (irel = elf_section_data (sec)->relocs;
                                irel < relend; irel++)
                             {
                               bfd_vma reloc_target = 0;
                               bfd_vma symval;
                               Elf_Internal_Sym *isymbuf = NULL;

                               /* Read this BFD's local symbols if we haven't
                                  done so already.  */
                               if (isymbuf == NULL && symtab_hdr->sh_info != 0)
                                 {
                                   isymbuf = (Elf_Internal_Sym *)
                                             symtab_hdr->contents;
                                   if (isymbuf == NULL)
                                     isymbuf = bfd_elf_get_elf_syms
				       (abfd,
					symtab_hdr,
					symtab_hdr->sh_info, 0,
					NULL, NULL, NULL);
                                   if (isymbuf == NULL)
                                     break;
                                  }

                               /* Get the value of the symbol referred to
                                  by the reloc.  */
                               if (ELF32_R_SYM (irel->r_info)
                                   < symtab_hdr->sh_info)
                                 {
                                   /* A local symbol.  */
                                   Elf_Internal_Sym *isym;
                                   asection *sym_sec;

                                   isym = isymbuf
                                          + ELF32_R_SYM (irel->r_info);
                                   sym_sec = bfd_section_from_elf_index
				     (abfd, isym->st_shndx);
                                   symval = isym->st_value;

                                   /* If the reloc is absolute, it will not
                                      have a symbol or section associated
                                      with it.  */

                                   if (sym_sec)
                                     {
                                       symval +=
                                           sym_sec->output_section->vma
                                           + sym_sec->output_offset;
                                       reloc_target = symval + irel->r_addend;
                                     }
                                   else
                                     {
                                       reloc_target = symval + irel->r_addend;
                                       /* Reference symbol is absolute.  */
                                     }
                                 }
			       /* else ... reference symbol is extern.  */

                               if (address_of_ret == reloc_target)
                                 {
                                   deleting_ret_is_safe = 0;
                                   if (debug_relax)
                                     printf ("ret from "
                                             "rjmp/jmp ret sequence at address"
                                             " 0x%x could not be deleted. ret"
                                             " is target of a relocation.\n",
                                             (int) address_of_ret);
                                 }
                             }
                         }

                         if (deleting_ret_is_safe)
                           {
                             if (debug_relax)
                               printf ("unreachable ret instruction "
                                       "at address 0x%x deleted.\n",
                                       (int) dot + insn_size);

                             /* Delete two bytes of data.  */
                             if (!elf32_avr_relax_delete_bytes (abfd, sec,
                                        irel->r_offset + insn_size, 2))
                               goto error_return;

                             /* That will change things, so, we should relax
                                again. Note that this is not required, and it
                                may be slow.  */
                             *again = TRUE;
                             break;
                           }
                      }

                  }
              }
            break;
          }
        }
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
        free (contents);
      else
        {
          /* Cache the section contents for elf_link_input_bfd.  */
          elf_section_data (sec)->this_hdr.contents = contents;
        }
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return FALSE;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses elf32_avr_relocate_section.

   For avr it's essentially a cut and paste taken from the H8300 port.
   The author of the relaxation support patch for avr had absolutely no
   clue what is happening here but found out that this part of the code
   seems to be important.  */

static bfd_byte *
elf32_avr_get_relocated_section_contents (bfd *output_bfd,
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
          (size_t) input_section->size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      asection **secpp;
      Elf_Internal_Sym *isym, *isymend;
      bfd_size_type amt;

      internal_relocs = (_bfd_elf_link_read_relocs
                         (input_bfd, input_section, NULL, NULL, FALSE));
      if (internal_relocs == NULL)
        goto error_return;

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

      amt = symtab_hdr->sh_info;
      amt *= sizeof (asection *);
      sections = bfd_malloc (amt);
      if (sections == NULL && amt != 0)
        goto error_return;

      isymend = isymbuf + symtab_hdr->sh_info;
      for (isym = isymbuf, secpp = sections; isym < isymend; ++isym, ++secpp)
        {
          asection *isec;

          if (isym->st_shndx == SHN_UNDEF)
            isec = bfd_und_section_ptr;
          else if (isym->st_shndx == SHN_ABS)
            isec = bfd_abs_section_ptr;
          else if (isym->st_shndx == SHN_COMMON)
            isec = bfd_com_section_ptr;
          else
            isec = bfd_section_from_elf_index (input_bfd, isym->st_shndx);

          *secpp = isec;
        }

      if (! elf32_avr_relocate_section (output_bfd, link_info, input_bfd,
                                        input_section, data, internal_relocs,
                                        isymbuf, sections))
        goto error_return;

      if (sections != NULL)
        free (sections);
      if (isymbuf != NULL
          && symtab_hdr->contents != (unsigned char *) isymbuf)
        free (isymbuf);
      if (elf_section_data (input_section)->relocs != internal_relocs)
        free (internal_relocs);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (input_section)->relocs != internal_relocs)
    free (internal_relocs);
  return NULL;
}


/* Determines the hash entry name for a particular reloc. It consists of
   the identifier of the symbol section and the added reloc addend and
   symbol offset relative to the section the symbol is attached to.  */

static char *
avr_stub_name (const asection *symbol_section,
               const bfd_vma symbol_offset,
               const Elf_Internal_Rela *rela)
{
  char *stub_name;
  bfd_size_type len;

  len = 8 + 1 + 8 + 1 + 1;
  stub_name = bfd_malloc (len);

  sprintf (stub_name, "%08x+%08x",
           symbol_section->id & 0xffffffff,
           (unsigned int) ((rela->r_addend & 0xffffffff) + symbol_offset));

  return stub_name;
}


/* Add a new stub entry to the stub hash.  Not all fields of the new
   stub entry are initialised.  */

static struct elf32_avr_stub_hash_entry *
avr_add_stub (const char *stub_name,
              struct elf32_avr_link_hash_table *htab)
{
  struct elf32_avr_stub_hash_entry *hsh;

  /* Enter this entry into the linker stub hash table.  */
  hsh = avr_stub_hash_lookup (&htab->bstab, stub_name, TRUE, FALSE);

  if (hsh == NULL)
    {
      (*_bfd_error_handler) (_("%B: cannot create stub entry %s"),
                             NULL, stub_name);
      return NULL;
    }

  hsh->stub_offset = 0;
  return hsh;
}

/* We assume that there is already space allocated for the stub section
   contents and that before building the stubs the section size is
   initialized to 0.  We assume that within the stub hash table entry,
   the absolute position of the jmp target has been written in the
   target_value field.  We write here the offset of the generated jmp insn
   relative to the trampoline section start to the stub_offset entry in
   the stub hash table entry.  */

static  bfd_boolean
avr_build_one_stub (struct bfd_hash_entry *bh, void *in_arg)
{
  struct elf32_avr_stub_hash_entry *hsh;
  struct bfd_link_info *info;
  struct elf32_avr_link_hash_table *htab;
  bfd *stub_bfd;
  bfd_byte *loc;
  bfd_vma target;
  bfd_vma starget;

  /* Basic opcode */
  bfd_vma jmp_insn = 0x0000940c;

  /* Massage our args to the form they really have.  */
  hsh = avr_stub_hash_entry (bh);

  if (!hsh->is_actually_needed)
    return TRUE;

  info = (struct bfd_link_info *) in_arg;

  htab = avr_link_hash_table (info);
  if (htab == NULL)
    return FALSE;

  target = hsh->target_value;

  /* Make a note of the offset within the stubs for this entry.  */
  hsh->stub_offset = htab->stub_sec->size;
  loc = htab->stub_sec->contents + hsh->stub_offset;

  stub_bfd = htab->stub_sec->owner;

  if (debug_stubs)
    printf ("Building one Stub. Address: 0x%x, Offset: 0x%x\n",
             (unsigned int) target,
             (unsigned int) hsh->stub_offset);

  /* We now have to add the information on the jump target to the bare
     opcode bits already set in jmp_insn.  */

  /* Check for the alignment of the address.  */
  if (target & 1)
     return FALSE;

  starget = target >> 1;
  jmp_insn |= ((starget & 0x10000) | ((starget << 3) & 0x1f00000)) >> 16;
  bfd_put_16 (stub_bfd, jmp_insn, loc);
  bfd_put_16 (stub_bfd, (bfd_vma) starget & 0xffff, loc + 2);

  htab->stub_sec->size += 4;

  /* Now add the entries in the address mapping table if there is still
     space left.  */
  {
    unsigned int nr;

    nr = htab->amt_entry_cnt + 1;
    if (nr <= htab->amt_max_entry_cnt)
      {
        htab->amt_entry_cnt = nr;

        htab->amt_stub_offsets[nr - 1] = hsh->stub_offset;
        htab->amt_destination_addr[nr - 1] = target;
      }
  }

  return TRUE;
}

static bfd_boolean
avr_mark_stub_not_to_be_necessary (struct bfd_hash_entry *bh,
                                   void *in_arg)
{
  struct elf32_avr_stub_hash_entry *hsh;
  struct elf32_avr_link_hash_table *htab;

  htab = in_arg;
  hsh = avr_stub_hash_entry (bh);
  hsh->is_actually_needed = FALSE;

  return TRUE;
}

static bfd_boolean
avr_size_one_stub (struct bfd_hash_entry *bh, void *in_arg)
{
  struct elf32_avr_stub_hash_entry *hsh;
  struct elf32_avr_link_hash_table *htab;
  int size;

  /* Massage our args to the form they really have.  */
  hsh = avr_stub_hash_entry (bh);
  htab = in_arg;

  if (hsh->is_actually_needed)
    size = 4;
  else
    size = 0;

  htab->stub_sec->size += size;
  return TRUE;
}

void
elf32_avr_setup_params (struct bfd_link_info *info,
                        bfd *avr_stub_bfd,
                        asection *avr_stub_section,
                        bfd_boolean no_stubs,
                        bfd_boolean deb_stubs,
                        bfd_boolean deb_relax,
                        bfd_vma pc_wrap_around,
                        bfd_boolean call_ret_replacement)
{
  struct elf32_avr_link_hash_table *htab = avr_link_hash_table (info);

  if (htab == NULL)
    return;
  htab->stub_sec = avr_stub_section;
  htab->stub_bfd = avr_stub_bfd;
  htab->no_stubs = no_stubs;

  debug_relax = deb_relax;
  debug_stubs = deb_stubs;
  avr_pc_wrap_around = pc_wrap_around;
  avr_replace_call_ret_sequences = call_ret_replacement;
}


/* Set up various things so that we can make a list of input sections
   for each output section included in the link.  Returns -1 on error,
   0 when no stubs will be needed, and 1 on success.  It also sets
   information on the stubs bfd and the stub section in the info
   struct.  */

int
elf32_avr_setup_section_lists (bfd *output_bfd,
                               struct bfd_link_info *info)
{
  bfd *input_bfd;
  unsigned int bfd_count;
  int top_id, top_index;
  asection *section;
  asection **input_list, **list;
  bfd_size_type amt;
  struct elf32_avr_link_hash_table *htab = avr_link_hash_table(info);

  if (htab == NULL || htab->no_stubs)
    return 0;

  /* Count the number of input BFDs and find the top input section id.  */
  for (input_bfd = info->input_bfds, bfd_count = 0, top_id = 0;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next)
    {
      bfd_count += 1;
      for (section = input_bfd->sections;
           section != NULL;
           section = section->next)
	if (top_id < section->id)
	  top_id = section->id;
    }

  htab->bfd_count = bfd_count;

  /* We can't use output_bfd->section_count here to find the top output
     section index as some sections may have been removed, and
     strip_excluded_output_sections doesn't renumber the indices.  */
  for (section = output_bfd->sections, top_index = 0;
       section != NULL;
       section = section->next)
    if (top_index < section->index)
      top_index = section->index;

  htab->top_index = top_index;
  amt = sizeof (asection *) * (top_index + 1);
  input_list = bfd_malloc (amt);
  htab->input_list = input_list;
  if (input_list == NULL)
    return -1;

  /* For sections we aren't interested in, mark their entries with a
     value we can check later.  */
  list = input_list + top_index;
  do
    *list = bfd_abs_section_ptr;
  while (list-- != input_list);

  for (section = output_bfd->sections;
       section != NULL;
       section = section->next)
    if ((section->flags & SEC_CODE) != 0)
      input_list[section->index] = NULL;

  return 1;
}


/* Read in all local syms for all input bfds, and create hash entries
   for export stubs if we are building a multi-subspace shared lib.
   Returns -1 on error, 0 otherwise.  */

static int
get_local_syms (bfd *input_bfd, struct bfd_link_info *info)
{
  unsigned int bfd_indx;
  Elf_Internal_Sym *local_syms, **all_local_syms;
  struct elf32_avr_link_hash_table *htab = avr_link_hash_table (info);

  if (htab == NULL)
    return -1;

  /* We want to read in symbol extension records only once.  To do this
     we need to read in the local symbols in parallel and save them for
     later use; so hold pointers to the local symbols in an array.  */
  bfd_size_type amt = sizeof (Elf_Internal_Sym *) * htab->bfd_count;
  all_local_syms = bfd_zmalloc (amt);
  htab->all_local_syms = all_local_syms;
  if (all_local_syms == NULL)
    return -1;

  /* Walk over all the input BFDs, swapping in local symbols.
     If we are creating a shared library, create hash entries for the
     export stubs.  */
  for (bfd_indx = 0;
       input_bfd != NULL;
       input_bfd = input_bfd->link_next, bfd_indx++)
    {
      Elf_Internal_Shdr *symtab_hdr;

      /* We'll need the symbol table in a second.  */
      symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
      if (symtab_hdr->sh_info == 0)
	continue;

      /* We need an array of the local symbols attached to the input bfd.  */
      local_syms = (Elf_Internal_Sym *) symtab_hdr->contents;
      if (local_syms == NULL)
	{
	  local_syms = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					     symtab_hdr->sh_info, 0,
					     NULL, NULL, NULL);
	  /* Cache them for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) local_syms;
	}
      if (local_syms == NULL)
	return -1;

      all_local_syms[bfd_indx] = local_syms;
    }

  return 0;
}

#define ADD_DUMMY_STUBS_FOR_DEBUGGING 0

bfd_boolean
elf32_avr_size_stubs (bfd *output_bfd,
                      struct bfd_link_info *info,
                      bfd_boolean is_prealloc_run)
{
  struct elf32_avr_link_hash_table *htab;
  int stub_changed = 0;

  htab = avr_link_hash_table (info);
  if (htab == NULL)
    return FALSE;

  /* At this point we initialize htab->vector_base
     To the start of the text output section.  */
  htab->vector_base = htab->stub_sec->output_section->vma;

  if (get_local_syms (info->input_bfds, info))
    {
      if (htab->all_local_syms)
	goto error_ret_free_local;
      return FALSE;
    }

  if (ADD_DUMMY_STUBS_FOR_DEBUGGING)
    {
      struct elf32_avr_stub_hash_entry *test;

      test = avr_add_stub ("Hugo",htab);
      test->target_value = 0x123456;
      test->stub_offset = 13;

      test = avr_add_stub ("Hugo2",htab);
      test->target_value = 0x84210;
      test->stub_offset = 14;
    }

  while (1)
    {
      bfd *input_bfd;
      unsigned int bfd_indx;

      /* We will have to re-generate the stub hash table each time anything
         in memory has changed.  */

      bfd_hash_traverse (&htab->bstab, avr_mark_stub_not_to_be_necessary, htab);
      for (input_bfd = info->input_bfds, bfd_indx = 0;
           input_bfd != NULL;
           input_bfd = input_bfd->link_next, bfd_indx++)
        {
          Elf_Internal_Shdr *symtab_hdr;
          asection *section;
          Elf_Internal_Sym *local_syms;

          /* We'll need the symbol table in a second.  */
          symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
          if (symtab_hdr->sh_info == 0)
            continue;

          local_syms = htab->all_local_syms[bfd_indx];

          /* Walk over each section attached to the input bfd.  */
          for (section = input_bfd->sections;
               section != NULL;
               section = section->next)
            {
              Elf_Internal_Rela *internal_relocs, *irelaend, *irela;

              /* If there aren't any relocs, then there's nothing more
                 to do.  */
              if ((section->flags & SEC_RELOC) == 0
                  || section->reloc_count == 0)
                continue;

              /* If this section is a link-once section that will be
                 discarded, then don't create any stubs.  */
              if (section->output_section == NULL
                  || section->output_section->owner != output_bfd)
                continue;

              /* Get the relocs.  */
              internal_relocs
                = _bfd_elf_link_read_relocs (input_bfd, section, NULL, NULL,
                                             info->keep_memory);
              if (internal_relocs == NULL)
                goto error_ret_free_local;

              /* Now examine each relocation.  */
              irela = internal_relocs;
              irelaend = irela + section->reloc_count;
              for (; irela < irelaend; irela++)
                {
                  unsigned int r_type, r_indx;
                  struct elf32_avr_stub_hash_entry *hsh;
                  asection *sym_sec;
                  bfd_vma sym_value;
                  bfd_vma destination;
                  struct elf_link_hash_entry *hh;
                  char *stub_name;

                  r_type = ELF32_R_TYPE (irela->r_info);
                  r_indx = ELF32_R_SYM (irela->r_info);

                  /* Only look for 16 bit GS relocs. No other reloc will need a
                     stub.  */
                  if (!((r_type == R_AVR_16_PM)
                        || (r_type == R_AVR_LO8_LDI_GS)
                        || (r_type == R_AVR_HI8_LDI_GS)))
                    continue;

                  /* Now determine the call target, its name, value,
                     section.  */
                  sym_sec = NULL;
                  sym_value = 0;
                  destination = 0;
                  hh = NULL;
                  if (r_indx < symtab_hdr->sh_info)
                    {
                      /* It's a local symbol.  */
                      Elf_Internal_Sym *sym;
                      Elf_Internal_Shdr *hdr;

                      sym = local_syms + r_indx;
                      hdr = elf_elfsections (input_bfd)[sym->st_shndx];
                      sym_sec = hdr->bfd_section;
                      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
                        sym_value = sym->st_value;
                      destination = (sym_value + irela->r_addend
                                     + sym_sec->output_offset
                                     + sym_sec->output_section->vma);
                    }
                  else
                    {
                      /* It's an external symbol.  */
                      int e_indx;

                      e_indx = r_indx - symtab_hdr->sh_info;
                      hh = elf_sym_hashes (input_bfd)[e_indx];

                      while (hh->root.type == bfd_link_hash_indirect
                             || hh->root.type == bfd_link_hash_warning)
                        hh = (struct elf_link_hash_entry *)
                              (hh->root.u.i.link);

                      if (hh->root.type == bfd_link_hash_defined
                          || hh->root.type == bfd_link_hash_defweak)
                        {
                          sym_sec = hh->root.u.def.section;
                          sym_value = hh->root.u.def.value;
                          if (sym_sec->output_section != NULL)
                          destination = (sym_value + irela->r_addend
                                         + sym_sec->output_offset
                                         + sym_sec->output_section->vma);
                        }
                      else if (hh->root.type == bfd_link_hash_undefweak)
                        {
                          if (! info->shared)
                            continue;
                        }
                      else if (hh->root.type == bfd_link_hash_undefined)
                        {
                          if (! (info->unresolved_syms_in_objects == RM_IGNORE
                                 && (ELF_ST_VISIBILITY (hh->other)
                                     == STV_DEFAULT)))
                             continue;
                        }
                      else
                        {
                          bfd_set_error (bfd_error_bad_value);

                          error_ret_free_internal:
                          if (elf_section_data (section)->relocs == NULL)
                            free (internal_relocs);
                          goto error_ret_free_local;
                        }
                    }

                  if (! avr_stub_is_required_for_16_bit_reloc
		      (destination - htab->vector_base))
                    {
                      if (!is_prealloc_run)
			/* We are having a reloc that does't need a stub.  */
			continue;

		      /* We don't right now know if a stub will be needed.
			 Let's rather be on the safe side.  */
                    }

                  /* Get the name of this stub.  */
                  stub_name = avr_stub_name (sym_sec, sym_value, irela);

                  if (!stub_name)
                    goto error_ret_free_internal;


                  hsh = avr_stub_hash_lookup (&htab->bstab,
                                              stub_name,
                                              FALSE, FALSE);
                  if (hsh != NULL)
                    {
                      /* The proper stub has already been created.  Mark it
                         to be used and write the possibly changed destination
                         value.  */
                      hsh->is_actually_needed = TRUE;
                      hsh->target_value = destination;
                      free (stub_name);
                      continue;
                    }

                  hsh = avr_add_stub (stub_name, htab);
                  if (hsh == NULL)
                    {
                      free (stub_name);
                      goto error_ret_free_internal;
                    }

                  hsh->is_actually_needed = TRUE;
                  hsh->target_value = destination;

                  if (debug_stubs)
                    printf ("Adding stub with destination 0x%x to the"
                            " hash table.\n", (unsigned int) destination);
                  if (debug_stubs)
                    printf ("(Pre-Alloc run: %i)\n", is_prealloc_run);

                  stub_changed = TRUE;
                }

              /* We're done with the internal relocs, free them.  */
              if (elf_section_data (section)->relocs == NULL)
                free (internal_relocs);
            }
        }

      /* Re-Calculate the number of needed stubs.  */
      htab->stub_sec->size = 0;
      bfd_hash_traverse (&htab->bstab, avr_size_one_stub, htab);

      if (!stub_changed)
        break;

      stub_changed = FALSE;
    }

  free (htab->all_local_syms);
  return TRUE;

 error_ret_free_local:
  free (htab->all_local_syms);
  return FALSE;
}


/* Build all the stubs associated with the current output file.  The
   stubs are kept in a hash table attached to the main linker hash
   table.  We also set up the .plt entries for statically linked PIC
   functions here.  This function is called via hppaelf_finish in the
   linker.  */

bfd_boolean
elf32_avr_build_stubs (struct bfd_link_info *info)
{
  asection *stub_sec;
  struct bfd_hash_table *table;
  struct elf32_avr_link_hash_table *htab;
  bfd_size_type total_size = 0;

  htab = avr_link_hash_table (info);
  if (htab == NULL)
    return FALSE;

  /* In case that there were several stub sections:  */
  for (stub_sec = htab->stub_bfd->sections;
       stub_sec != NULL;
       stub_sec = stub_sec->next)
    {
      bfd_size_type size;

      /* Allocate memory to hold the linker stubs.  */
      size = stub_sec->size;
      total_size += size;

      stub_sec->contents = bfd_zalloc (htab->stub_bfd, size);
      if (stub_sec->contents == NULL && size != 0)
	return FALSE;
      stub_sec->size = 0;
    }

  /* Allocate memory for the adress mapping table.  */
  htab->amt_entry_cnt = 0;
  htab->amt_max_entry_cnt = total_size / 4;
  htab->amt_stub_offsets = bfd_malloc (sizeof (bfd_vma)
                                       * htab->amt_max_entry_cnt);
  htab->amt_destination_addr = bfd_malloc (sizeof (bfd_vma)
					   * htab->amt_max_entry_cnt );

  if (debug_stubs)
    printf ("Allocating %i entries in the AMT\n", htab->amt_max_entry_cnt);

  /* Build the stubs as directed by the stub hash table.  */
  table = &htab->bstab;
  bfd_hash_traverse (table, avr_build_one_stub, info);

  if (debug_stubs)
    printf ("Final Stub section Size: %i\n", (int) htab->stub_sec->size);

  return TRUE;
}

#define ELF_ARCH		bfd_arch_avr
#define ELF_MACHINE_CODE	EM_AVR
#define ELF_MACHINE_ALT1	EM_AVR_OLD
#define ELF_MAXPAGESIZE		1

#define TARGET_LITTLE_SYM       bfd_elf32_avr_vec
#define TARGET_LITTLE_NAME	"elf32-avr"

#define bfd_elf32_bfd_link_hash_table_create elf32_avr_link_hash_table_create
#define bfd_elf32_bfd_link_hash_table_free   elf32_avr_link_hash_table_free

#define elf_info_to_howto	             avr_info_to_howto_rela
#define elf_info_to_howto_rel	             NULL
#define elf_backend_relocate_section         elf32_avr_relocate_section
#define elf_backend_check_relocs             elf32_avr_check_relocs
#define elf_backend_can_gc_sections          1
#define elf_backend_rela_normal		     1
#define elf_backend_final_write_processing \
					bfd_elf_avr_final_write_processing
#define elf_backend_object_p		elf32_avr_object_p

#define bfd_elf32_bfd_relax_section elf32_avr_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
                                        elf32_avr_get_relocated_section_contents

#include "elf32-target.h"
