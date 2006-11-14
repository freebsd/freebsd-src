/* 32-bit ELF support for ARM new abi option.
   Copyright 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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

#include "elf/arm.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

#ifndef NUM_ELEM
#define NUM_ELEM(a)  (sizeof (a) / (sizeof (a)[0]))
#endif

#define USE_REL	1

#define TARGET_LITTLE_SYM               bfd_elf32_littlearm_vec
#define TARGET_LITTLE_NAME              "elf32-littlearm"
#define TARGET_BIG_SYM                  bfd_elf32_bigarm_vec
#define TARGET_BIG_NAME                 "elf32-bigarm"

#define elf_info_to_howto               0
#define elf_info_to_howto_rel           elf32_arm_info_to_howto

#define ARM_ELF_ABI_VERSION		0
#define ARM_ELF_OS_ABI_VERSION		ELFOSABI_ARM

static reloc_howto_type * elf32_arm_reloc_type_lookup
  PARAMS ((bfd * abfd, bfd_reloc_code_real_type code));
static bfd_boolean elf32_arm_nabi_grok_prstatus
  PARAMS ((bfd *abfd, Elf_Internal_Note *note));
static bfd_boolean elf32_arm_nabi_grok_psinfo
  PARAMS ((bfd *abfd, Elf_Internal_Note *note));

/* Note: code such as elf32_arm_reloc_type_lookup expect to use e.g.
   R_ARM_PC24 as an index into this, and find the R_ARM_PC24 HOWTO
   in that slot.  */

static reloc_howto_type elf32_arm_howto_table[] =
{
  /* No relocation */
  HOWTO (R_ARM_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_NONE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_PC24,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_PC24",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00ffffff,		/* src_mask */
	 0x00ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 32 bit absolute */
  HOWTO (R_ARM_ABS32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_ABS32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* standard 32bit pc-relative reloc */
  HOWTO (R_ARM_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_REL32",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit absolute */
  HOWTO (R_ARM_PC13,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_PC13",		/* name */
	 FALSE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

   /* 16 bit absolute */
  HOWTO (R_ARM_ABS16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_ABS16",		/* name */
	 FALSE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 12 bit absolute */
  HOWTO (R_ARM_ABS12,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_ABS12",		/* name */
	 FALSE,			/* partial_inplace */
	 0x000008ff,		/* src_mask */
	 0x000008ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_THM_ABS5,	/* type */
	 6,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_THM_ABS5",	/* name */
	 FALSE,			/* partial_inplace */
	 0x000007e0,		/* src_mask */
	 0x000007e0,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* 8 bit absolute */
  HOWTO (R_ARM_ABS8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_ABS8",		/* name */
	 FALSE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_SBREL32,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_SBREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_THM_PC22,	/* type */
	 1,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 23,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_THM_PC22",	/* name */
	 FALSE,			/* partial_inplace */
	 0x07ff07ff,		/* src_mask */
	 0x07ff07ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_ARM_THM_PC8,	        /* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_THM_PC8",	/* name */
	 FALSE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_ARM_AMP_VCALL9,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_AMP_VCALL9",	/* name */
	 FALSE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_ARM_SWI24,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_SWI24",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x00000000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_THM_SWI8,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_SWI8",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x00000000,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* BLX instruction for the ARM.  */
  HOWTO (R_ARM_XPC25,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 25,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_XPC25",		/* name */
	 FALSE,			/* partial_inplace */
	 0x00ffffff,		/* src_mask */
	 0x00ffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* BLX instruction for the Thumb.  */
  HOWTO (R_ARM_THM_XPC22,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 22,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_THM_XPC22",	/* name */
	 FALSE,			/* partial_inplace */
	 0x07ff07ff,		/* src_mask */
	 0x07ff07ff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* These next three relocs are not defined, but we need to fill the space.  */

  HOWTO (R_ARM_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_unknown_17",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_unknown_18",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_unknown_19",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* Relocs used in ARM Linux */

  HOWTO (R_ARM_COPY,		/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_COPY",		/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         FALSE),                /* pcrel_offset */

  HOWTO (R_ARM_GLOB_DAT,	/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_GLOB_DAT",	/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         FALSE),                /* pcrel_offset */

  HOWTO (R_ARM_JUMP_SLOT,	/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_JUMP_SLOT",	/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         FALSE),                /* pcrel_offset */

  HOWTO (R_ARM_RELATIVE,	/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_RELATIVE",	/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         FALSE),                /* pcrel_offset */

  HOWTO (R_ARM_GOTOFF,		/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_GOTOFF",	/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         FALSE),                /* pcrel_offset */

  HOWTO (R_ARM_GOTPC,		/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         TRUE,			/* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_GOTPC",		/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         TRUE),			/* pcrel_offset */

  HOWTO (R_ARM_GOT32,		/* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         32,                    /* bitsize */
         FALSE,			/* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_GOT32",		/* name */
         TRUE,			/* partial_inplace */
         0xffffffff,		/* src_mask */
         0xffffffff,		/* dst_mask */
         FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_PLT32,		/* type */
         2,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         26,                    /* bitsize */
         TRUE,			/* pc_relative */
         0,                     /* bitpos */
         complain_overflow_bitfield,/* complain_on_overflow */
         bfd_elf_generic_reloc, /* special_function */
         "R_ARM_PLT32",		/* name */
         TRUE,			/* partial_inplace */
         0x00ffffff,		/* src_mask */
         0x00ffffff,		/* dst_mask */
         TRUE),			/* pcrel_offset */

  /* End of relocs used in ARM Linux */

  HOWTO (R_ARM_RREL32,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_RREL32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_RABS32,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_RABS32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_RPC24,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_RPC24",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_ARM_RBASE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_RBASE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

};

  /* GNU extension to record C++ vtable hierarchy */
static reloc_howto_type elf32_arm_vtinherit_howto =
  HOWTO (R_ARM_GNU_VTINHERIT, /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         NULL,                  /* special_function */
         "R_ARM_GNU_VTINHERIT", /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE);                /* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
static reloc_howto_type elf32_arm_vtentry_howto =
  HOWTO (R_ARM_GNU_VTENTRY,     /* type */
         0,                     /* rightshift */
         2,                     /* size (0 = byte, 1 = short, 2 = long) */
         0,                     /* bitsize */
         FALSE,                 /* pc_relative */
         0,                     /* bitpos */
         complain_overflow_dont, /* complain_on_overflow */
         _bfd_elf_rel_vtable_reloc_fn,  /* special_function */
         "R_ARM_GNU_VTENTRY",   /* name */
         FALSE,                 /* partial_inplace */
         0,                     /* src_mask */
         0,                     /* dst_mask */
         FALSE);                /* pcrel_offset */

  /* 12 bit pc relative */
static reloc_howto_type elf32_arm_thm_pc11_howto =
  HOWTO (R_ARM_THM_PC11,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 11,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_THM_PC11",	/* name */
	 FALSE,			/* partial_inplace */
	 0x000007ff,		/* src_mask */
	 0x000007ff,		/* dst_mask */
	 TRUE);			/* pcrel_offset */

  /* 12 bit pc relative */
static reloc_howto_type elf32_arm_thm_pc9_howto =
  HOWTO (R_ARM_THM_PC9,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARM_THM_PC9",	/* name */
	 FALSE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 TRUE);			/* pcrel_offset */

static void elf32_arm_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));

static void
elf32_arm_info_to_howto (abfd, bfd_reloc, elf_reloc)
     bfd * abfd ATTRIBUTE_UNUSED;
     arelent * bfd_reloc;
     Elf_Internal_Rela * elf_reloc;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (elf_reloc->r_info);

  switch (r_type)
    {
    case R_ARM_GNU_VTINHERIT:
      bfd_reloc->howto = & elf32_arm_vtinherit_howto;
      break;

    case R_ARM_GNU_VTENTRY:
      bfd_reloc->howto = & elf32_arm_vtentry_howto;
      break;

    case R_ARM_THM_PC11:
      bfd_reloc->howto = & elf32_arm_thm_pc11_howto;
      break;

    case R_ARM_THM_PC9:
      bfd_reloc->howto = & elf32_arm_thm_pc9_howto;
      break;

    default:
      if (r_type >= NUM_ELEM (elf32_arm_howto_table))
	bfd_reloc->howto = NULL;
      else
        bfd_reloc->howto = & elf32_arm_howto_table[r_type];
      break;
    }
}

struct elf32_arm_reloc_map
  {
    bfd_reloc_code_real_type  bfd_reloc_val;
    unsigned char             elf_reloc_val;
  };

static const struct elf32_arm_reloc_map elf32_arm_reloc_map[] =
  {
    {BFD_RELOC_NONE,                 R_ARM_NONE},
    {BFD_RELOC_ARM_PCREL_BRANCH,     R_ARM_PC24},
    {BFD_RELOC_ARM_PCREL_BLX,        R_ARM_XPC25},
    {BFD_RELOC_THUMB_PCREL_BLX,      R_ARM_THM_XPC22},
    {BFD_RELOC_32,                   R_ARM_ABS32},
    {BFD_RELOC_32_PCREL,             R_ARM_REL32},
    {BFD_RELOC_8,                    R_ARM_ABS8},
    {BFD_RELOC_16,                   R_ARM_ABS16},
    {BFD_RELOC_ARM_OFFSET_IMM,       R_ARM_ABS12},
    {BFD_RELOC_ARM_THUMB_OFFSET,     R_ARM_THM_ABS5},
    {BFD_RELOC_THUMB_PCREL_BRANCH23, R_ARM_THM_PC22},
    {BFD_RELOC_ARM_COPY,             R_ARM_COPY},
    {BFD_RELOC_ARM_GLOB_DAT,         R_ARM_GLOB_DAT},
    {BFD_RELOC_ARM_JUMP_SLOT,        R_ARM_JUMP_SLOT},
    {BFD_RELOC_ARM_RELATIVE,         R_ARM_RELATIVE},
    {BFD_RELOC_ARM_GOTOFF,           R_ARM_GOTOFF},
    {BFD_RELOC_ARM_GOTPC,            R_ARM_GOTPC},
    {BFD_RELOC_ARM_GOT32,            R_ARM_GOT32},
    {BFD_RELOC_ARM_PLT32,            R_ARM_PLT32}
  };

static reloc_howto_type *
elf32_arm_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  switch (code)
    {
    case BFD_RELOC_VTABLE_INHERIT:
      return & elf32_arm_vtinherit_howto;

    case BFD_RELOC_VTABLE_ENTRY:
      return & elf32_arm_vtentry_howto;

    case BFD_RELOC_THUMB_PCREL_BRANCH12:
      return & elf32_arm_thm_pc11_howto;

    case BFD_RELOC_THUMB_PCREL_BRANCH9:
      return & elf32_arm_thm_pc9_howto;

    default:
      for (i = 0; i < NUM_ELEM (elf32_arm_reloc_map); i ++)
	if (elf32_arm_reloc_map[i].bfd_reloc_val == code)
	  return & elf32_arm_howto_table[elf32_arm_reloc_map[i].elf_reloc_val];

      return NULL;
   }
}

/* Support for core dump NOTE sections */
static bfd_boolean
elf32_arm_nabi_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  size_t raw_size;

  switch (note->descsz)
    {
      default:
	return FALSE;

      case 148:		/* Linux/ARM 32-bit*/
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

	/* pr_reg */
	offset = 72;
	raw_size = 72;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  raw_size, note->descpos + offset);
}

static bfd_boolean
elf32_arm_nabi_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
      default:
	return FALSE;

      case 124:		/* Linux/ARM elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 28, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 44, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return TRUE;
}

#define elf_backend_grok_prstatus	elf32_arm_nabi_grok_prstatus
#define elf_backend_grok_psinfo		elf32_arm_nabi_grok_psinfo

#include "elf32-arm.h"
