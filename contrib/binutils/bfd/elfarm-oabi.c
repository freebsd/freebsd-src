/* 32-bit ELF support for ARM old abi option.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.

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

#define OLD_ARM_ABI

#include "elf/arm.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

#ifndef NUM_ELEM
#define NUM_ELEM(a) (sizeof (a) / sizeof (a)[0])
#endif

#define USE_RELA

#define TARGET_LITTLE_SYM               bfd_elf32_littlearm_oabi_vec
#define TARGET_LITTLE_NAME              "elf32-littlearm-oabi"
#define TARGET_BIG_SYM                  bfd_elf32_bigarm_oabi_vec
#define TARGET_BIG_NAME                 "elf32-bigarm-oabi"

#define elf_info_to_howto               elf32_arm_info_to_howto
#define elf_info_to_howto_rel           0

#define ARM_ELF_ABI_VERSION		0
#define ARM_ELF_OS_ABI_VERSION		0

static reloc_howto_type * find_howto                  PARAMS ((unsigned int));
static void               elf32_arm_info_to_howto     PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static reloc_howto_type * elf32_arm_reloc_type_lookup PARAMS ((bfd *, bfd_reloc_code_real_type));

static reloc_howto_type elf32_arm_howto_table[] =
  {
    /* No relocation.  */
    HOWTO (R_ARM_NONE,		/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   0,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_NONE",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_PC24,		/* type */
	   2,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   24,			/* bitsize */
	   true,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_signed,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_PC24",	/* name */
	   false,		/* partial_inplace */
	   0x00ffffff,		/* src_mask */
	   0x00ffffff,		/* dst_mask */
	   true),			/* pcrel_offset */

    /* 32 bit absolute.  */
    HOWTO (R_ARM_ABS32,		/* type */
	   0,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   32,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_ABS32",	/* name */
	   false,		/* partial_inplace */
	   0xffffffff,		/* src_mask */
	   0xffffffff,		/* dst_mask */
	   false),		/* pcrel_offset */

    /* Standard 32bit pc-relative reloc.  */
    HOWTO (R_ARM_REL32,		/* type */
	   0,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   32,			/* bitsize */
	   true,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_REL32",	/* name */
	   false,		/* partial_inplace */
	   0xffffffff,		/* src_mask */
	   0xffffffff,		/* dst_mask */
	   true),		/* pcrel_offset */

    /* 8 bit absolute.  */
    HOWTO (R_ARM_ABS8,		/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   8,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_ABS8",	/* name */
	   false,		/* partial_inplace */
	   0x000000ff,		/* src_mask */
	   0x000000ff,		/* dst_mask */
	   false),		/* pcrel_offset */

    /* 16 bit absolute.  */
    HOWTO (R_ARM_ABS16,		/* type */
	   0,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   16,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_ABS16",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false),		/* pcrel_offset */

    /* 12 bit absolute.  */
    HOWTO (R_ARM_ABS12,		/* type */
	   0,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   12,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_ABS12",	/* name */
	   false,		/* partial_inplace */
	   0x000008ff,		/* src_mask */
	   0x000008ff,		/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_THM_ABS5,	/* type */
	   6,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   5,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_bitfield,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_THM_ABS5",	/* name */
	   false,		/* partial_inplace */
	   0x000007e0,		/* src_mask */
	   0x000007e0,		/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_THM_PC22,	/* type */
	   1,			/* rightshift */
	   2,			/* size (0 = byte, 1 = short, 2 = long) */
	   23,			/* bitsize */
	   true,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_signed,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_THM_PC22",	/* name */
	   false,		/* partial_inplace */
	   0x07ff07ff,		/* src_mask */
	   0x07ff07ff,		/* dst_mask */
	   true),			/* pcrel_offset */

    HOWTO (R_ARM_SBREL32,		/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   0,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont,/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_SBREL32",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_AMP_VCALL9,	/* type */
	   1,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   8,			/* bitsize */
	   true,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_signed,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_AMP_VCALL9",	/* name */
	   false,		/* partial_inplace */
	   0x000000ff,		/* src_mask */
	   0x000000ff,		/* dst_mask */
	   true),		/* pcrel_offset */

    /* 12 bit pc relative.  */
    HOWTO (R_ARM_THM_PC11,	/* type */
	   1,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   11,			/* bitsize */
	   true,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_signed,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_THM_PC11",	/* name */
	   false,		/* partial_inplace */
	   0x000007ff,		/* src_mask */
	   0x000007ff,		/* dst_mask */
	   true),		/* pcrel_offset */

    /* 12 bit pc relative.  */
    HOWTO (R_ARM_THM_PC9,	/* type */
	   1,			/* rightshift */
	   1,			/* size (0 = byte, 1 = short, 2 = long) */
	   8,			/* bitsize */
	   true,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_signed,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_THM_PC9",	/* name */
	   false,		/* partial_inplace */
	   0x000000ff,		/* src_mask */
	   0x000000ff,		/* dst_mask */
	   true),		/* pcrel_offset */

    /* GNU extension to record C++ vtable hierarchy.  */
    HOWTO (R_ARM_GNU_VTINHERIT, /* type */
	   0,                     /* rightshift */
	   2,                     /* size (0 = byte, 1 = short, 2 = long) */
	   0,                     /* bitsize */
	   false,                 /* pc_relative */
	   0,                     /* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   NULL,                  /* special_function */
	   "R_ARM_GNU_VTINHERIT", /* name */
	   false,                 /* partial_inplace */
	   0,                     /* src_mask */
	   0,                     /* dst_mask */
	   false),                /* pcrel_offset */

    /* GNU extension to record C++ vtable member usage.  */
    HOWTO (R_ARM_GNU_VTENTRY,     /* type */
	   0,                     /* rightshift */
	   2,                     /* size (0 = byte, 1 = short, 2 = long) */
	   0,                     /* bitsize */
	   false,                 /* pc_relative */
	   0,                     /* bitpos */
	   complain_overflow_dont, /* complain_on_overflow */
	   _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
	   "R_ARM_GNU_VTENTRY",   /* name */
	   false,                 /* partial_inplace */
	   0,                     /* src_mask */
	   0,                     /* dst_mask */
	   false),                /* pcrel_offset */

    /* XXX - gap in index numbering here.  */

    HOWTO (R_ARM_PLT32,		/* type */
	   2,                   /* rightshift */
	   2,                   /* size (0 = byte, 1 = short, 2 = long) */
	   26,                  /* bitsize */
	   true,		/* pc_relative */
	   0,                   /* bitpos */
	   complain_overflow_bitfield,/* complain_on_overflow */
	   bfd_elf_generic_reloc, /* special_function */
	   "R_ARM_PLT32",	/* name */
	   true,		/* partial_inplace */
	   0x00ffffff,		/* src_mask */
	   0x00ffffff,		/* dst_mask */
	   true),			/* pcrel_offset */

    /* XXX - gap in index numbering here.  */

    HOWTO (R_ARM_RREL32,	/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   0,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_RREL32",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_RABS32,	/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   0,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_RABS32",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_RPC24,		/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   0,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_RPC24",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false),		/* pcrel_offset */

    HOWTO (R_ARM_RBASE,		/* type */
	   0,			/* rightshift */
	   0,			/* size (0 = byte, 1 = short, 2 = long) */
	   0,			/* bitsize */
	   false,		/* pc_relative */
	   0,			/* bitpos */
	   complain_overflow_dont,	/* complain_on_overflow */
	   bfd_elf_generic_reloc,	/* special_function */
	   "R_ARM_RBASE",	/* name */
	   false,		/* partial_inplace */
	   0,			/* src_mask */
	   0,			/* dst_mask */
	   false)		/* pcrel_offset */
  };

/* Locate a reloc in the howto table.  This function must be used
   when the entry number is is > R_ARM_GNU_VTINHERIT.  */

static reloc_howto_type *
find_howto (r_type)
     unsigned int r_type;
{
  int i;

  for (i = NUM_ELEM (elf32_arm_howto_table); i--;)
    if (elf32_arm_howto_table [i].type == r_type)
      return elf32_arm_howto_table + i;

  return NULL;
}

static void
elf32_arm_info_to_howto (abfd, bfd_reloc, elf_reloc)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *bfd_reloc;
     Elf32_Internal_Rela *elf_reloc;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (elf_reloc->r_info);

  if (r_type <= R_ARM_GNU_VTINHERIT)
    bfd_reloc->howto = & elf32_arm_howto_table[r_type];
  else
    bfd_reloc->howto = find_howto (r_type);
}

struct elf32_arm_reloc_map
  {
    bfd_reloc_code_real_type bfd_reloc_val;
    unsigned char elf_reloc_val;
  };

static const struct elf32_arm_reloc_map elf32_arm_reloc_map[] =
  {
    {BFD_RELOC_NONE,                 R_ARM_NONE },
    {BFD_RELOC_ARM_PCREL_BRANCH,     R_ARM_PC24 },
    {BFD_RELOC_32,                   R_ARM_ABS32 },
    {BFD_RELOC_32_PCREL,             R_ARM_REL32 },
    {BFD_RELOC_8,                    R_ARM_ABS8 },
    {BFD_RELOC_16,                   R_ARM_ABS16 },
    {BFD_RELOC_ARM_OFFSET_IMM,       R_ARM_ABS12 },
    {BFD_RELOC_ARM_THUMB_OFFSET,     R_ARM_THM_ABS5 },
    {BFD_RELOC_THUMB_PCREL_BRANCH23, R_ARM_THM_PC22 },
    {BFD_RELOC_NONE,                 R_ARM_SBREL32 },
    {BFD_RELOC_NONE,                 R_ARM_AMP_VCALL9 },
    {BFD_RELOC_THUMB_PCREL_BRANCH12, R_ARM_THM_PC11 },
    {BFD_RELOC_THUMB_PCREL_BRANCH9,  R_ARM_THM_PC9 },
    {BFD_RELOC_VTABLE_INHERIT,       R_ARM_GNU_VTINHERIT },
    {BFD_RELOC_VTABLE_ENTRY,         R_ARM_GNU_VTENTRY }
  };

static reloc_howto_type *
elf32_arm_reloc_type_lookup (abfd, code)
     bfd * abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = NUM_ELEM (elf32_arm_reloc_map); i--;)
    if (elf32_arm_reloc_map[i].bfd_reloc_val == code)
      return & elf32_arm_howto_table [elf32_arm_reloc_map[i].elf_reloc_val];

  if (code == BFD_RELOC_ARM_PLT32)
    return find_howto (R_ARM_PLT32);

  return NULL;
}

#define bfd_elf32_arm_allocate_interworking_sections \
	bfd_elf32_arm_oabi_allocate_interworking_sections
#define bfd_elf32_arm_get_bfd_for_interworking \
	bfd_elf32_arm_oabi_get_bfd_for_interworking
#define bfd_elf32_arm_process_before_allocation \
	bfd_elf32_arm_oabi_process_before_allocation

#include "elf32-arm.h"
