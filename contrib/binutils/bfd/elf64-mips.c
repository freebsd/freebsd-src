/* MIPS-specific support for 64-bit ELF
   Copyright 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
   Ian Lance Taylor, Cygnus Support
   Linker support added by Mark Mitchell, CodeSourcery, LLC.
   <mark@codesourcery.com>

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

/* This file supports the 64-bit MIPS ELF ABI.

   The MIPS 64-bit ELF ABI uses an unusual reloc format.  This file
   overrides the usual ELF reloc handling, and handles reading and
   writing the relocations here.

   The MIPS 64-bit ELF ABI also uses an unusual archive map format.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "aout/ar.h"
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  The 64-bit ABI is not supposed to
   use ECOFF.  However, we support it anyhow for an easier changeover.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
/* The 64 bit versions of the mdebug data structures are in alpha.h.  */
#include "coff/alpha.h"
#define ECOFF_64
#include "ecoffswap.h"

static void mips_elf64_swap_reloc_in
  PARAMS ((bfd *, const Elf64_Mips_External_Rel *,
	   Elf64_Mips_Internal_Rel *));
static void mips_elf64_swap_reloca_in
  PARAMS ((bfd *, const Elf64_Mips_External_Rela *,
	   Elf64_Mips_Internal_Rela *));
static void mips_elf64_swap_reloc_out
  PARAMS ((bfd *, const Elf64_Mips_Internal_Rel *,
	   Elf64_Mips_External_Rel *));
static void mips_elf64_swap_reloca_out
  PARAMS ((bfd *, const Elf64_Mips_Internal_Rela *,
	   Elf64_Mips_External_Rela *));
static void mips_elf64_be_swap_reloc_in
  PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rel *));
static void mips_elf64_be_swap_reloc_out
  PARAMS ((bfd *, const Elf_Internal_Rel *, bfd_byte *));
static void mips_elf64_be_swap_reloca_in
  PARAMS ((bfd *, const bfd_byte *, Elf_Internal_Rela *));
static void mips_elf64_be_swap_reloca_out
  PARAMS ((bfd *, const Elf_Internal_Rela *, bfd_byte *));
static reloc_howto_type *mips_elf64_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static long mips_elf64_get_reloc_upper_bound PARAMS ((bfd *, asection *));
static boolean mips_elf64_slurp_one_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, const Elf_Internal_Shdr *));
static boolean mips_elf64_slurp_reloc_table
  PARAMS ((bfd *, asection *, asymbol **, boolean));
static void mips_elf64_write_relocs PARAMS ((bfd *, asection *, PTR));
static boolean mips_elf64_slurp_armap PARAMS ((bfd *));
static boolean mips_elf64_write_armap
  PARAMS ((bfd *, unsigned int, struct orl *, unsigned int, int));

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

/* The number of local .got entries we reserve.  */
#define MIPS_RESERVED_GOTNO (2)

/* The relocation table used for SHT_REL sections.  */

static reloc_howto_type mips_elf64_howto_table_rel[] =
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

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
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

  /* 64 bit substraction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the higher value of a 64 bit addend.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

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

  /* I'm not sure what the remaining relocs are, but they are defined
     on Irix 6.  */

  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_ADD_IMMEDIATE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_ADD_IMMEDIATE", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_PJUMP,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PJUMP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No 
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The relocation table used for SHT_RELA sections.  */

static reloc_howto_type mips_elf64_howto_table_rela[] =
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
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
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit substraction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the higher value of a 64 bit addend.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 true,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

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
	 0,			/* src_mask */
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
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* I'm not sure what the remaining relocs are, but they are defined
     on Irix 6.  */

  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_ADD_IMMEDIATE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_ADD_IMMEDIATE", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_PJUMP,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PJUMP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No 
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* Swap in a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_External_Rel *src;
     Elf64_Mips_Internal_Rel *dst;
{
  dst->r_offset = bfd_h_get_64 (abfd, (bfd_byte *) src->r_offset);
  dst->r_sym = bfd_h_get_32 (abfd, (bfd_byte *) src->r_sym);
  dst->r_ssym = bfd_h_get_8 (abfd, (bfd_byte *) src->r_ssym);
  dst->r_type3 = bfd_h_get_8 (abfd, (bfd_byte *) src->r_type3);
  dst->r_type2 = bfd_h_get_8 (abfd, (bfd_byte *) src->r_type2);
  dst->r_type = bfd_h_get_8 (abfd, (bfd_byte *) src->r_type);
}

/* Swap in a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_External_Rela *src;
     Elf64_Mips_Internal_Rela *dst;
{
  dst->r_offset = bfd_h_get_64 (abfd, (bfd_byte *) src->r_offset);
  dst->r_sym = bfd_h_get_32 (abfd, (bfd_byte *) src->r_sym);
  dst->r_ssym = bfd_h_get_8 (abfd, (bfd_byte *) src->r_ssym);
  dst->r_type3 = bfd_h_get_8 (abfd, (bfd_byte *) src->r_type3);
  dst->r_type2 = bfd_h_get_8 (abfd, (bfd_byte *) src->r_type2);
  dst->r_type = bfd_h_get_8 (abfd, (bfd_byte *) src->r_type);
  dst->r_addend = bfd_h_get_signed_64 (abfd, (bfd_byte *) src->r_addend);
}

/* Swap out a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_Internal_Rel *src;
     Elf64_Mips_External_Rel *dst;
{
  bfd_h_put_64 (abfd, src->r_offset, (bfd_byte *) dst->r_offset);
  bfd_h_put_32 (abfd, src->r_sym, (bfd_byte *) dst->r_sym);
  bfd_h_put_8 (abfd, src->r_ssym, (bfd_byte *) dst->r_ssym);
  bfd_h_put_8 (abfd, src->r_type3, (bfd_byte *) dst->r_type3);
  bfd_h_put_8 (abfd, src->r_type2, (bfd_byte *) dst->r_type2);
  bfd_h_put_8 (abfd, src->r_type, (bfd_byte *) dst->r_type);
}

/* Swap out a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf64_Mips_Internal_Rela *src;
     Elf64_Mips_External_Rela *dst;
{
  bfd_h_put_64 (abfd, src->r_offset, (bfd_byte *) dst->r_offset);
  bfd_h_put_32 (abfd, src->r_sym, (bfd_byte *) dst->r_sym);
  bfd_h_put_8 (abfd, src->r_ssym, (bfd_byte *) dst->r_ssym);
  bfd_h_put_8 (abfd, src->r_type3, (bfd_byte *) dst->r_type3);
  bfd_h_put_8 (abfd, src->r_type2, (bfd_byte *) dst->r_type2);
  bfd_h_put_8 (abfd, src->r_type, (bfd_byte *) dst->r_type);
  bfd_h_put_64 (abfd, src->r_addend, (bfd_byte *) dst->r_addend);
}

/* Swap in a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_be_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const bfd_byte *src;
     Elf_Internal_Rel *dst;
{
  Elf64_Mips_Internal_Rel mirel;

  mips_elf64_swap_reloc_in (abfd, 
			    (const Elf64_Mips_External_Rel *) src,
			    &mirel);

  dst[0].r_offset = mirel.r_offset;
  dst[0].r_info = ELF32_R_INFO (mirel.r_sym, mirel.r_type);
  dst[1].r_offset = mirel.r_offset;
  dst[1].r_info = ELF32_R_INFO (mirel.r_ssym, mirel.r_type2);
  dst[2].r_offset = mirel.r_offset;
  dst[2].r_info = ELF32_R_INFO (STN_UNDEF, mirel.r_type3);
}

/* Swap in a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_be_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const bfd_byte *src;
     Elf_Internal_Rela *dst;
{
  Elf64_Mips_Internal_Rela mirela;

  mips_elf64_swap_reloca_in (abfd, 
			     (const Elf64_Mips_External_Rela *) src,
			     &mirela);

  dst[0].r_offset = mirela.r_offset;
  dst[0].r_info = ELF32_R_INFO (mirela.r_sym, mirela.r_type);
  dst[0].r_addend = mirela.r_addend;
  dst[1].r_offset = mirela.r_offset;
  dst[1].r_info = ELF32_R_INFO (mirela.r_ssym, mirela.r_type2);
  dst[1].r_addend = 0;
  dst[2].r_offset = mirela.r_offset;
  dst[2].r_info = ELF32_R_INFO (STN_UNDEF, mirela.r_type3);
  dst[2].r_addend = 0;
}

/* Swap out a MIPS 64-bit Rel reloc.  */

static void
mips_elf64_be_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rel *src;
     bfd_byte *dst;
{
  Elf64_Mips_Internal_Rel mirel;

  mirel.r_offset = src->r_offset;
  mirel.r_type = ELF32_R_TYPE (src->r_info);
  mirel.r_sym = ELF32_R_SYM (src->r_info);
  mirel.r_type2 = R_MIPS_NONE;
  mirel.r_ssym = STN_UNDEF;
  mirel.r_type3 = R_MIPS_NONE;

  mips_elf64_swap_reloc_out (abfd, &mirel, 
			     (Elf64_Mips_External_Rel *) dst);
}

/* Swap out a MIPS 64-bit Rela reloc.  */

static void
mips_elf64_be_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rela *src;
     bfd_byte *dst;
{
  Elf64_Mips_Internal_Rela mirela;

  mirela.r_offset = src->r_offset;
  mirela.r_type = ELF32_R_TYPE (src->r_info);
  mirela.r_addend = src->r_addend;
  mirela.r_sym = ELF32_R_SYM (src->r_info);
  mirela.r_type2 = R_MIPS_NONE;
  mirela.r_ssym = STN_UNDEF;
  mirela.r_type3 = R_MIPS_NONE;

  mips_elf64_swap_reloca_out (abfd, &mirela, 
			      (Elf64_Mips_External_Rela *) dst);
}

/* A mapping from BFD reloc types to MIPS ELF reloc types.  */

struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  enum elf_mips_reloc_type elf_reloc_val;
};

static CONST struct elf_reloc_map mips_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MIPS_NONE, },
  { BFD_RELOC_16, R_MIPS_16 },
  { BFD_RELOC_32, R_MIPS_32 },
  { BFD_RELOC_64, R_MIPS_64 },
  { BFD_RELOC_CTOR, R_MIPS_64 },
  { BFD_RELOC_32_PCREL, R_MIPS_REL32 },
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
  { BFD_RELOC_MIPS_SUB, R_MIPS_SUB },
  { BFD_RELOC_MIPS_GOT_PAGE, R_MIPS_GOT_PAGE },
  { BFD_RELOC_MIPS_GOT_OFST, R_MIPS_GOT_OFST },
  { BFD_RELOC_MIPS_GOT_DISP, R_MIPS_GOT_DISP }
};

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
mips_elf64_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (mips_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (mips_reloc_map[i].bfd_reloc_val == code)
	{
	  int v;

	  v = (int) mips_reloc_map[i].elf_reloc_val;
	  return &mips_elf64_howto_table_rel[v];
	}
    }

  return NULL;
}

/* Since each entry in an SHT_REL or SHT_RELA section can represent up
   to three relocs, we must tell the user to allocate more space.  */

static long
mips_elf64_get_reloc_upper_bound (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  return (sec->reloc_count * 3 + 1) * sizeof (arelent *);
}

/* Read the relocations from one reloc section.  */

static boolean
mips_elf64_slurp_one_reloc_table (abfd, asect, symbols, rel_hdr)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     const Elf_Internal_Shdr *rel_hdr;
{
  PTR allocated = NULL;
  bfd_byte *native_relocs;
  arelent *relents;
  arelent *relent;
  unsigned int count;
  unsigned int i;
  int entsize;
  reloc_howto_type *howto_table;

  allocated = (PTR) bfd_malloc (rel_hdr->sh_size);
  if (allocated == NULL)
    goto error_return;

  if (bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0
      || (bfd_read (allocated, 1, rel_hdr->sh_size, abfd) != rel_hdr->sh_size))
    goto error_return;

  native_relocs = (bfd_byte *) allocated;

  relents = asect->relocation + asect->reloc_count;

  entsize = rel_hdr->sh_entsize;
  BFD_ASSERT (entsize == sizeof (Elf64_Mips_External_Rel)
	      || entsize == sizeof (Elf64_Mips_External_Rela));

  count = rel_hdr->sh_size / entsize;

  if (entsize == sizeof (Elf64_Mips_External_Rel))
    howto_table = mips_elf64_howto_table_rel;
  else
    howto_table = mips_elf64_howto_table_rela;

  relent = relents;
  for (i = 0; i < count; i++, native_relocs += entsize)
    {
      Elf64_Mips_Internal_Rela rela;
      boolean used_sym, used_ssym;
      int ir;

      if (entsize == sizeof (Elf64_Mips_External_Rela))
	mips_elf64_swap_reloca_in (abfd,
				   (Elf64_Mips_External_Rela *) native_relocs,
				   &rela);
      else
	{
	  Elf64_Mips_Internal_Rel rel;

	  mips_elf64_swap_reloc_in (abfd,
				    (Elf64_Mips_External_Rel *) native_relocs,
				    &rel);
	  rela.r_offset = rel.r_offset;
	  rela.r_sym = rel.r_sym;
	  rela.r_ssym = rel.r_ssym;
	  rela.r_type3 = rel.r_type3;
	  rela.r_type2 = rel.r_type2;
	  rela.r_type = rel.r_type;
	  rela.r_addend = 0;
	}

      /* Each entry represents up to three actual relocations.  */

      used_sym = false;
      used_ssym = false;
      for (ir = 0; ir < 3; ir++)
	{
	  enum elf_mips_reloc_type type;

	  switch (ir)
	    {
	    default:
	      abort ();
	    case 0:
	      type = (enum elf_mips_reloc_type) rela.r_type;
	      break;
	    case 1:
	      type = (enum elf_mips_reloc_type) rela.r_type2;
	      break;
	    case 2:
	      type = (enum elf_mips_reloc_type) rela.r_type3;
	      break;
	    }

	  if (type == R_MIPS_NONE)
	    {
	      /* There are no more relocations in this entry.  If this
                 is the first entry, we need to generate a dummy
                 relocation so that the generic linker knows that
                 there has been a break in the sequence of relocations
                 applying to a particular address.  */
	      if (ir == 0)
		{
		  relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
		  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
		    relent->address = rela.r_offset;
		  else
		    relent->address = rela.r_offset - asect->vma;
		  relent->addend = 0;
		  relent->howto = &howto_table[(int) R_MIPS_NONE];
		  ++relent;
		}
	      break;
	    }

	  /* Some types require symbols, whereas some do not.  */
	  switch (type)
	    {
	    case R_MIPS_NONE:
	    case R_MIPS_LITERAL:
	    case R_MIPS_INSERT_A:
	    case R_MIPS_INSERT_B:
	    case R_MIPS_DELETE:
	      relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	      break;

	    default:
	      if (! used_sym)
		{
		  if (rela.r_sym == 0)
		    relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
		  else
		    {
		      asymbol **ps, *s;

		      ps = symbols + rela.r_sym - 1;
		      s = *ps;
		      if ((s->flags & BSF_SECTION_SYM) == 0)
			relent->sym_ptr_ptr = ps;
		      else
			relent->sym_ptr_ptr = s->section->symbol_ptr_ptr;
		    }

		  used_sym = true;
		}
	      else if (! used_ssym)
		{
		  switch (rela.r_ssym)
		    {
		    case RSS_UNDEF:
		      relent->sym_ptr_ptr =
			bfd_abs_section_ptr->symbol_ptr_ptr;
		      break;

		    case RSS_GP:
		    case RSS_GP0:
		    case RSS_LOC:
		      /* FIXME: I think these need to be handled using
                         special howto structures.  */
		      BFD_ASSERT (0);
		      break;

		    default:
		      BFD_ASSERT (0);
		      break;
		    }

		  used_ssym = true;
		}
	      else
		relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;

	      break;
	    }

	  /* The address of an ELF reloc is section relative for an
	     object file, and absolute for an executable file or
	     shared library.  The address of a BFD reloc is always
	     section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	    relent->address = rela.r_offset;
	  else
	    relent->address = rela.r_offset - asect->vma;

	  relent->addend = rela.r_addend;

	  relent->howto = &howto_table[(int) type];

	  ++relent;
	}
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

/* Read the relocations.  On Irix 6, there can be two reloc sections
   associated with a single data section.  */

static boolean
mips_elf64_slurp_reloc_table (abfd, asect, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     boolean dynamic;
{
  struct bfd_elf_section_data * const d = elf_section_data (asect);

  if (dynamic)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (asect->relocation != NULL
      || (asect->flags & SEC_RELOC) == 0
      || asect->reloc_count == 0)
    return true;

  /* Allocate space for 3 arelent structures for each Rel structure.  */
  asect->relocation = ((arelent *)
		       bfd_alloc (abfd,
				  asect->reloc_count * 3 * sizeof (arelent)));
  if (asect->relocation == NULL)
    return false;

  /* The slurp_one_reloc_table routine increments reloc_count.  */
  asect->reloc_count = 0;

  if (! mips_elf64_slurp_one_reloc_table (abfd, asect, symbols, &d->rel_hdr))
    return false;
  if (d->rel_hdr2 != NULL)
    {
      if (! mips_elf64_slurp_one_reloc_table (abfd, asect, symbols,
					      d->rel_hdr2))
	return false;
    }

  return true;
}

/* Write out the relocations.  */

static void
mips_elf64_write_relocs (abfd, sec, data)
     bfd *abfd;
     asection *sec;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  unsigned int count;
  Elf_Internal_Shdr *rela_hdr;
  Elf64_Mips_External_Rela *ext_rela;
  unsigned int idx;
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

  /* We can combine up to three relocs that refer to the same address
     if the latter relocs have no associated symbol.  */
  count = 0;
  for (idx = 0; idx < sec->reloc_count; idx++)
    {
      bfd_vma addr;
      unsigned int i;

      ++count;

      addr = sec->orelocation[idx]->address;
      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != addr
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

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

  ext_rela = (Elf64_Mips_External_Rela *) rela_hdr->contents;
  for (idx = 0; idx < sec->reloc_count; idx++, ext_rela++)
    {
      arelent *ptr;
      Elf64_Mips_Internal_Rela int_rela;
      asymbol *sym;
      int n;
      unsigned int i;

      ptr = sec->orelocation[idx];

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a BFD reloc is always section relative.  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	int_rela.r_offset = ptr->address;
      else
	int_rela.r_offset = ptr->address + sec->vma;

      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
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

      int_rela.r_sym = n;

      int_rela.r_addend = ptr->addend;

      int_rela.r_ssym = RSS_UNDEF;

      if ((*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      int_rela.r_type = ptr->howto->type;
      int_rela.r_type2 = (int) R_MIPS_NONE;
      int_rela.r_type3 = (int) R_MIPS_NONE;

      for (i = 0; i < 2; i++)
	{
	  arelent *r;

	  if (idx + 1 >= sec->reloc_count)
	    break;
	  r = sec->orelocation[idx + 1];
	  if (r->address != ptr->address
	      || ! bfd_is_abs_section ((*r->sym_ptr_ptr)->section)
	      || (*r->sym_ptr_ptr)->value != 0)
	    break;

	  /* We can merge the reloc at IDX + 1 with the reloc at IDX.  */

	  if (i == 0)
	    int_rela.r_type2 = r->howto->type;
	  else
	    int_rela.r_type3 = r->howto->type;

	  ++idx;
	}

      mips_elf64_swap_reloca_out (abfd, &int_rela, ext_rela);
    }

  BFD_ASSERT (ext_rela - (Elf64_Mips_External_Rela *) rela_hdr->contents
	      == count);
}

/* Irix 6 defines a brand new archive map format, so that they can
   have archives more than 4 GB in size.  */

/* Read an Irix 6 armap.  */

static boolean
mips_elf64_slurp_armap (abfd)
     bfd *abfd;
{
  struct artdata *ardata = bfd_ardata (abfd);
  char nextname[17];
  file_ptr arhdrpos;
  bfd_size_type i, parsed_size, nsymz, stringsize, carsym_size, ptrsize;
  struct areltdata *mapdata;
  bfd_byte int_buf[8];
  char *stringbase;
  bfd_byte *raw_armap = NULL;
  carsym *carsyms;

  ardata->symdefs = NULL;

  /* Get the name of the first element.  */
  arhdrpos = bfd_tell (abfd);
  i = bfd_read ((PTR) nextname, 1, 16, abfd);
  if (i == 0)
    return true;
  if (i != 16)
    return false;

  if (bfd_seek (abfd, (file_ptr) - 16, SEEK_CUR) != 0)
    return false;

  /* Archives with traditional armaps are still permitted.  */
  if (strncmp (nextname, "/               ", 16) == 0)
    return bfd_slurp_armap (abfd);

  if (strncmp (nextname, "/SYM64/         ", 16) != 0)
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  mapdata = (struct areltdata *) _bfd_read_ar_hdr (abfd);
  if (mapdata == NULL)
    return false;
  parsed_size = mapdata->parsed_size;
  bfd_release (abfd, (PTR) mapdata);

  if (bfd_read (int_buf, 1, 8, abfd) != 8)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
      return false;
    }

  nsymz = bfd_getb64 (int_buf);
  stringsize = parsed_size - 8 * nsymz - 8;

  carsym_size = nsymz * sizeof (carsym);
  ptrsize = 8 * nsymz;

  ardata->symdefs = (carsym *) bfd_zalloc (abfd, carsym_size + stringsize + 1);
  if (ardata->symdefs == NULL)
    return false;
  carsyms = ardata->symdefs;
  stringbase = ((char *) ardata->symdefs) + carsym_size;

  raw_armap = (bfd_byte *) bfd_alloc (abfd, ptrsize);
  if (raw_armap == NULL)
    goto error_return;

  if (bfd_read (raw_armap, 1, ptrsize, abfd) != ptrsize
      || bfd_read (stringbase, 1, stringsize, abfd) != stringsize)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
      goto error_return;
    }

  for (i = 0; i < nsymz; i++)
    {
      carsyms->file_offset = bfd_getb64 (raw_armap + i * 8);
      carsyms->name = stringbase;
      stringbase += strlen (stringbase) + 1;
      ++carsyms;
    }
  *stringbase = '\0';

  ardata->symdef_count = nsymz;
  ardata->first_file_filepos = arhdrpos + sizeof (struct ar_hdr) + parsed_size;

  bfd_has_map (abfd) = true;
  bfd_release (abfd, raw_armap);

  return true;

 error_return:
  if (raw_armap != NULL)
    bfd_release (abfd, raw_armap);
  if (ardata->symdefs != NULL)
    bfd_release (abfd, ardata->symdefs);
  return false;
}

/* Write out an Irix 6 armap.  The Irix 6 tools are supposed to be
   able to handle ordinary ELF armaps, but at least on Irix 6.2 the
   linker crashes.  */

static boolean
mips_elf64_write_armap (arch, elength, map, symbol_count, stridx)
     bfd *arch;
     unsigned int elength;
     struct orl *map;
     unsigned int symbol_count;
     int stridx;
{
  unsigned int ranlibsize = (symbol_count * 8) + 8;
  unsigned int stringsize = stridx;
  unsigned int mapsize = stringsize + ranlibsize;
  file_ptr archive_member_file_ptr;
  bfd *current = arch->archive_head;
  unsigned int count;
  struct ar_hdr hdr;
  unsigned int i;
  int padding;
  bfd_byte buf[8];

  padding = BFD_ALIGN (mapsize, 8) - mapsize;
  mapsize += padding;

  /* work out where the first object file will go in the archive */
  archive_member_file_ptr = (mapsize
			     + elength
			     + sizeof (struct ar_hdr)
			     + SARMAG);

  memset ((char *) (&hdr), 0, sizeof (struct ar_hdr));
  strcpy (hdr.ar_name, "/SYM64/");
  sprintf (hdr.ar_size, "%-10d", (int) mapsize);
  sprintf (hdr.ar_date, "%ld", (long) time (NULL));
  /* This, at least, is what Intel coff sets the values to.: */
  sprintf ((hdr.ar_uid), "%d", 0);
  sprintf ((hdr.ar_gid), "%d", 0);
  sprintf ((hdr.ar_mode), "%-7o", (unsigned) 0);
  strncpy (hdr.ar_fmag, ARFMAG, 2);

  for (i = 0; i < sizeof (struct ar_hdr); i++)
    if (((char *) (&hdr))[i] == '\0')
      (((char *) (&hdr))[i]) = ' ';

  /* Write the ar header for this item and the number of symbols */

  if (bfd_write ((PTR) &hdr, 1, sizeof (struct ar_hdr), arch)
      != sizeof (struct ar_hdr))
    return false;

  bfd_putb64 (symbol_count, buf);
  if (bfd_write (buf, 1, 8, arch) != 8)
    return false;

  /* Two passes, first write the file offsets for each symbol -
     remembering that each offset is on a two byte boundary.  */

  /* Write out the file offset for the file associated with each
     symbol, and remember to keep the offsets padded out.  */

  current = arch->archive_head;
  count = 0;
  while (current != (bfd *) NULL && count < symbol_count)
    {
      /* For each symbol which is used defined in this object, write out
	 the object file's address in the archive */

      while (((bfd *) (map[count]).pos) == current)
	{
	  bfd_putb64 (archive_member_file_ptr, buf);
	  if (bfd_write (buf, 1, 8, arch) != 8)
	    return false;
	  count++;
	}
      /* Add size of this archive entry */
      archive_member_file_ptr += (arelt_size (current)
				  + sizeof (struct ar_hdr));
      /* remember about the even alignment */
      archive_member_file_ptr += archive_member_file_ptr % 2;
      current = current->next;
    }

  /* now write the strings themselves */
  for (count = 0; count < symbol_count; count++)
    {
      size_t len = strlen (*map[count].name) + 1;

      if (bfd_write (*map[count].name, 1, len, arch) != len)
	return false;
    }

  /* The spec says that this should be padded to an 8 byte boundary.
     However, the Irix 6.2 tools do not appear to do this.  */
  while (padding != 0)
    {
      if (bfd_write ("", 1, 1, arch) != 1)
	return false;
      --padding;
    }

  return true;
}

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf64_ecoff_debug_swap =
{
  /* Symbol table magic number.  */
  magicSym2,
  /* Alignment of debugging information.  E.g., 4.  */
  8,
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

/* Relocations in the 64 bit MIPS ELF ABI are more complex than in
   standard ELF.  This structure is used to redirect the relocation
   handling routines.  */

const struct elf_size_info mips_elf64_size_info =
{
  sizeof (Elf64_External_Ehdr),
  sizeof (Elf64_External_Phdr),
  sizeof (Elf64_External_Shdr),
  sizeof (Elf64_Mips_External_Rel),
  sizeof (Elf64_Mips_External_Rela),
  sizeof (Elf64_External_Sym),
  sizeof (Elf64_External_Dyn),
  sizeof (Elf_External_Note),
  4,            /* hash-table entry size */
  3,            /* internal relocations per external relocations */
  64,		/* arch_size */
  8,		/* file_align */
  ELFCLASS64,
  EV_CURRENT,
  bfd_elf64_write_out_phdrs,
  bfd_elf64_write_shdrs_and_ehdr,
  mips_elf64_write_relocs,
  bfd_elf64_swap_symbol_out,
  mips_elf64_slurp_reloc_table,
  bfd_elf64_slurp_symbol_table,
  bfd_elf64_swap_dyn_in,
  bfd_elf64_swap_dyn_out,
  mips_elf64_be_swap_reloc_in,
  mips_elf64_be_swap_reloc_out,
  mips_elf64_be_swap_reloca_in,
  mips_elf64_be_swap_reloca_out
};

#define TARGET_LITTLE_SYM		bfd_elf64_littlemips_vec
#define TARGET_LITTLE_NAME		"elf64-littlemips"
#define TARGET_BIG_SYM			bfd_elf64_bigmips_vec
#define TARGET_BIG_NAME			"elf64-bigmips"
#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS

#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_collect		true
#define elf_backend_type_change_ok	true
#define elf_backend_can_gc_sections	true
#define elf_backend_size_info		mips_elf64_size_info
#define elf_backend_object_p		_bfd_mips_elf_object_p
#define elf_backend_section_from_shdr	_bfd_mips_elf_section_from_shdr
#define elf_backend_fake_sections	_bfd_mips_elf_fake_sections
#define elf_backend_section_from_bfd_section \
					_bfd_mips_elf_section_from_bfd_section
#define elf_backend_section_processing	_bfd_mips_elf_section_processing
#define elf_backend_symbol_processing	_bfd_mips_elf_symbol_processing
#define elf_backend_additional_program_headers \
					_bfd_mips_elf_additional_program_headers
#define elf_backend_modify_segment_map	_bfd_mips_elf_modify_segment_map
#define elf_backend_final_write_processing \
					_bfd_mips_elf_final_write_processing
#define elf_backend_ecoff_debug_swap	&mips_elf64_ecoff_debug_swap
#define elf_backend_add_symbol_hook	_bfd_mips_elf_add_symbol_hook
#define elf_backend_create_dynamic_sections \
					_bfd_mips_elf_create_dynamic_sections
#define elf_backend_check_relocs	_bfd_mips_elf_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					_bfd_mips_elf_adjust_dynamic_symbol
#define elf_backend_always_size_sections \
					_bfd_mips_elf_always_size_sections
#define elf_backend_size_dynamic_sections \
					_bfd_mips_elf_size_dynamic_sections
#define elf_backend_relocate_section    _bfd_mips_elf_relocate_section
#define elf_backend_link_output_symbol_hook \
					_bfd_mips_elf_link_output_symbol_hook
#define elf_backend_finish_dynamic_symbol \
					_bfd_mips_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					_bfd_mips_elf_finish_dynamic_sections
#define elf_backend_gc_mark_hook	_bfd_mips_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	_bfd_mips_elf_gc_sweep_hook
#define elf_backend_got_header_size	(4*MIPS_RESERVED_GOTNO)
#define elf_backend_plt_header_size	0
#define elf_backend_may_use_rel_p       1

/* We don't set bfd_elf64_bfd_is_local_label_name because the 32-bit 
   MIPS-specific function only applies to IRIX5, which had no 64-bit
   ABI.  */
#define bfd_elf64_find_nearest_line	_bfd_mips_elf_find_nearest_line
#define bfd_elf64_set_section_contents	_bfd_mips_elf_set_section_contents
#define bfd_elf64_bfd_link_hash_table_create \
					_bfd_mips_elf_link_hash_table_create
#define bfd_elf64_bfd_final_link	_bfd_mips_elf_final_link
#define bfd_elf64_bfd_copy_private_bfd_data \
					_bfd_mips_elf_copy_private_bfd_data
#define bfd_elf64_bfd_merge_private_bfd_data \
					_bfd_mips_elf_merge_private_bfd_data
#define bfd_elf64_bfd_set_private_flags	_bfd_mips_elf_set_private_flags
#define bfd_elf64_bfd_print_private_bfd_data \
					_bfd_mips_elf_print_private_bfd_data

#define bfd_elf64_get_reloc_upper_bound mips_elf64_get_reloc_upper_bound
#define bfd_elf64_bfd_reloc_type_lookup	mips_elf64_reloc_type_lookup
#define bfd_elf64_archive_functions
#define bfd_elf64_archive_slurp_armap	mips_elf64_slurp_armap
#define bfd_elf64_archive_slurp_extended_name_table \
				_bfd_archive_coff_slurp_extended_name_table
#define bfd_elf64_archive_construct_extended_name_table \
				_bfd_archive_coff_construct_extended_name_table
#define bfd_elf64_archive_truncate_arname \
					_bfd_archive_coff_truncate_arname
#define bfd_elf64_archive_write_armap	mips_elf64_write_armap
#define bfd_elf64_archive_read_ar_hdr	_bfd_archive_coff_read_ar_hdr
#define bfd_elf64_archive_openr_next_archived_file \
				_bfd_archive_coff_openr_next_archived_file
#define bfd_elf64_archive_get_elt_at_index \
					_bfd_archive_coff_get_elt_at_index
#define bfd_elf64_archive_generic_stat_arch_elt \
					_bfd_archive_coff_generic_stat_arch_elt
#define bfd_elf64_archive_update_armap_timestamp \
				_bfd_archive_coff_update_armap_timestamp

#include "elf64-target.h"
