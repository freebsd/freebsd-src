/* ARC-specific support for 32-bit ELF
   Copyright (C) 1994, 1995, 1997, 1999 Free Software Foundation, Inc.
   Contributed by Doug Evans (dje@cygnus.com).

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
#include "elf/arc.h"

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
 PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void arc_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static boolean arc_elf_object_p PARAMS ((bfd *));
static void arc_elf_final_write_processing PARAMS ((bfd *, boolean));

/* Try to minimize the amount of space occupied by relocation tables
   on the ROM (not that the ROM won't be swamped by other ELF overhead).  */
#define USE_REL

static reloc_howto_type elf_arc_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_ARC_NONE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARC_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A standard 32 bit relocation.  */
  HOWTO (R_ARC_32,		/* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_ARC_32",		/* name */
	 false,	                /* partial_inplace */
	 0xffffffff,	        /* src_mask */
	 0xffffffff,   		/* dst_mask */
	 false),                /* pcrel_offset */

  /* A 26 bit absolute branch, right shifted by 2.  */
  HOWTO (R_ARC_B26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARC_B26",		/* name */
	 false,			/* partial_inplace */
	 0x00ffffff,		/* src_mask */
	 0x00ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A relative 22 bit branch; bits 21-2 are stored in bits 26-7.  */
  HOWTO (R_ARC_B22_PCREL,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 22,			/* bitsize */
	 true,			/* pc_relative */
	 7,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_ARC_B22_PCREL",	/* name */
	 false,			/* partial_inplace */
	 0x07ffff80,		/* src_mask */
	 0x07ffff80,		/* dst_mask */
	 true),			/* pcrel_offset */

};

/* Map BFD reloc types to ARC ELF reloc types.  */

struct arc_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct arc_reloc_map arc_reloc_map[] =
{
  { BFD_RELOC_NONE, R_ARC_NONE, },
  { BFD_RELOC_32, R_ARC_32 },
  { BFD_RELOC_CTOR, R_ARC_32 },
  { BFD_RELOC_ARC_B26, R_ARC_B26 },
  { BFD_RELOC_ARC_B22_PCREL, R_ARC_B22_PCREL },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (arc_reloc_map) / sizeof (struct arc_reloc_map);
       i++)
    {
      if (arc_reloc_map[i].bfd_reloc_val == code)
	return &elf_arc_howto_table[arc_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an ARC ELF reloc.  */

static void
arc_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_ARC_max);
  cache_ptr->howto = &elf_arc_howto_table[r_type];
}

/* Set the right machine number for an ARC ELF file.  */

static boolean
arc_elf_object_p (abfd)
     bfd *abfd;
{
  int mach;
  unsigned long arch = elf_elfheader (abfd)->e_flags & EF_ARC_MACH;

  switch (arch)
    {
    case E_ARC_MACH_BASE:
      mach = bfd_mach_arc_base;
      break;
    default:
      /* Unknown cpu type.  ??? What to do?  */
      return false;
    }

  (void) bfd_default_set_arch_mach (abfd, bfd_arch_arc, mach);
  return true;
}

/* The final processing done just before writing out an ARC ELF object file.
   This gets the ARC architecture right based on the machine number.  */

static void
arc_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  int mach;
  unsigned long val;

  switch (mach = bfd_get_mach (abfd))
    {
    case bfd_mach_arc_base:
      val = E_ARC_MACH_BASE;
      break;
    default:
      return;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_ARC_MACH;
  elf_elfheader (abfd)->e_flags |= val;
}

#define TARGET_LITTLE_SYM	bfd_elf32_littlearc_vec
#define TARGET_LITTLE_NAME	"elf32-littlearc"
#define TARGET_BIG_SYM		bfd_elf32_bigarc_vec
#define TARGET_BIG_NAME		"elf32-bigarc"
#define ELF_ARCH		bfd_arch_arc
#define ELF_MACHINE_CODE	EM_CYGNUS_ARC
#define ELF_MAXPAGESIZE		0x1000

#define elf_info_to_howto	0
#define elf_info_to_howto_rel	arc_info_to_howto_rel
#define elf_backend_object_p	arc_elf_object_p
#define elf_backend_final_write_processing \
				arc_elf_final_write_processing

#include "elf32-target.h"
