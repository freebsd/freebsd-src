/* Renesas / SuperH SH specific support for 32-bit ELF
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006 Free Software Foundation, Inc.
   Contributed by Ian Lance Taylor, Cygnus Support.

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

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/sh.h"
#include "libiberty.h"
#include "../opcodes/sh-opc.h"

static bfd_reloc_status_type sh_elf_reloc
  (bfd *, arelent *, asymbol *, void *, asection *, bfd *, char **);
static bfd_reloc_status_type sh_elf_ignore_reloc
  (bfd *, arelent *, asymbol *, void *, asection *, bfd *, char **);
static bfd_boolean sh_elf_relax_delete_bytes
  (bfd *, asection *, bfd_vma, int);
static bfd_boolean sh_elf_align_loads
  (bfd *, asection *, Elf_Internal_Rela *, bfd_byte *, bfd_boolean *);
#ifndef SH64_ELF
static bfd_boolean sh_elf_swap_insns
  (bfd *, asection *, void *, bfd_byte *, bfd_vma);
#endif
static int sh_elf_optimized_tls_reloc
  (struct bfd_link_info *, int, int);
static bfd_vma dtpoff_base
  (struct bfd_link_info *);
static bfd_vma tpoff
  (struct bfd_link_info *, bfd_vma);

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/usr/lib/libc.so.1"

static reloc_howto_type sh_elf_howto_table[] =
{
  /* No relocation.  */
  HOWTO (R_SH_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
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
	 sh_elf_reloc,		/* special_function */
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
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_REL32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit PC relative branch divided by 2.  */
  HOWTO (R_SH_DIR8WPN,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DIR8WPN",	/* name */
	 TRUE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 12 bit PC relative branch divided by 2.  */
  /* This cannot be partial_inplace because relaxation can't know the
     eventual value of a symbol.  */
  HOWTO (R_SH_IND12W,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 12,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_SH_IND12W",		/* name */
	 FALSE,			/* partial_inplace */
	 0x0,			/* src_mask */
	 0xfff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit unsigned PC relative divided by 4.  */
  HOWTO (R_SH_DIR8WPL,		/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DIR8WPL",	/* name */
	 TRUE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit unsigned PC relative divided by 2.  */
  HOWTO (R_SH_DIR8WPZ,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DIR8WPZ",	/* name */
	 TRUE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit GBR relative.  FIXME: This only makes sense if we have some
     special symbol for the GBR relative area, and that is not
     implemented.  */
  HOWTO (R_SH_DIR8BP,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DIR8BP",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit GBR relative divided by 2.  FIXME: This only makes sense if
     we have some special symbol for the GBR relative area, and that
     is not implemented.  */
  HOWTO (R_SH_DIR8W,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DIR8W",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit GBR relative divided by 4.  FIXME: This only makes sense if
     we have some special symbol for the GBR relative area, and that
     is not implemented.  */
  HOWTO (R_SH_DIR8L,		/* type */
	 2,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DIR8L",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit PC relative divided by 2 - but specified in a very odd way.  */
  HOWTO (R_SH_LOOP_START,	/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_LOOP_START",	/* name */
	 TRUE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* 8 bit PC relative divided by 2 - but specified in a very odd way.  */
  HOWTO (R_SH_LOOP_END,		/* type */
	 1,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_LOOP_END",	/* name */
	 TRUE,			/* partial_inplace */
	 0xff,			/* src_mask */
	 0xff,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

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

  /* The remaining relocs are a GNU extension used for relaxing.  The
     final pass of the linker never needs to do anything with any of
     these relocs.  Any required operations are handled by the
     relaxation code.  */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_SH_GNU_VTINHERIT, /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_SH_GNU_VTINHERIT", /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_SH_GNU_VTENTRY,     /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn,	/* special_function */
	 "R_SH_GNU_VTENTRY",   /* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

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
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH8",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

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
	 sh_elf_ignore_reloc,	/* special_function */
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
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_SWITCH32",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Indicates a .uses pseudo-op.  The compiler will generate .uses
     pseudo-ops when it finds a function call which can be relaxed.
     The offset field holds the PC relative offset to the instruction
     which loads the register used in the function call.  */
  HOWTO (R_SH_USES,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_USES",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The assembler will generate this reloc for addresses referred to
     by the register loads associated with USES relocs.  The offset
     field holds the number of times the address is referenced in the
     object file.  */
  HOWTO (R_SH_COUNT,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_COUNT",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* Indicates an alignment statement.  The offset field is the power
     of 2 to which subsequent portions of the object file must be
     aligned.  */
  HOWTO (R_SH_ALIGN,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_ALIGN",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The assembler will generate this reloc before a block of
     instructions.  A section should be processed as assuming it
     contains data, unless this reloc is seen.  */
  HOWTO (R_SH_CODE,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_CODE",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The assembler will generate this reloc after a block of
     instructions when it sees data that is not instructions.  */
  HOWTO (R_SH_DATA,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_DATA",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The assembler generates this reloc for each label within a block
     of instructions.  This permits the linker to avoid swapping
     instructions which are the targets of branches.  */
  HOWTO (R_SH_LABEL,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 sh_elf_ignore_reloc,	/* special_function */
	 "R_SH_LABEL",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 TRUE),			/* pcrel_offset */

  /* The next 12 are only supported via linking in SHC-generated objects.  */
  HOWTO (R_SH_DIR16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR16",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR8,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR8",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR8UL,		/* type */
	 2,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR8UL",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR8UW,		/* type */
	 1,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR8UW",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR8U,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR8U",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR8SW,		/* type */
	 1,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR8SW",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR8S,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR8S",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xff,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR4UL,		/* type */
	 2,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR4UL",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR4UW,		/* type */
	 1,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR4UW",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_DIR4U,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 4,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_unsigned, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR4U",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_PSHA,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 7,			/* bitsize */
	 FALSE,			/* pc_relative */
	 4,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PSHA",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_PSHL,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 7,			/* bitsize */
	 FALSE,			/* pc_relative */
	 4,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PSHL",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0f,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

#ifdef INCLUDE_SHMEDIA
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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

  /* Used in LD.UW, ST.W et al.	 */
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
	 FALSE),		/* pcrel_offset */

  /* Used in LD.L, FLD.S et al.	 */
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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

#else
  EMPTY_HOWTO (45),
  EMPTY_HOWTO (46),
  EMPTY_HOWTO (47),
  EMPTY_HOWTO (48),
  EMPTY_HOWTO (49),
  EMPTY_HOWTO (50),
  EMPTY_HOWTO (51),
#endif

  EMPTY_HOWTO (52),

  HOWTO (R_SH_DIR16S,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_DIR16S",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

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

  HOWTO (R_SH_TLS_GD_32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_GD_32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_LD_32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_LD_32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_LDO_32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_LDO_32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_IE_32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_IE_32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_LE_32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_LE_32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_DTPMOD32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_DTPMOD32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_DTPOFF32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_DTPOFF32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_TLS_TPOFF32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* */
	 "R_SH_TLS_TPOFF32",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  EMPTY_HOWTO (152),
  EMPTY_HOWTO (153),
  EMPTY_HOWTO (154),
  EMPTY_HOWTO (155),
  EMPTY_HOWTO (156),
  EMPTY_HOWTO (157),
  EMPTY_HOWTO (158),
  EMPTY_HOWTO (159),

  HOWTO (R_SH_GOT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_GOT32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_PLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_PLT32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_SH_COPY,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_COPY",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_GLOB_DAT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_GLOB_DAT",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_JMP_SLOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_JMP_SLOT",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_RELATIVE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_RELATIVE",	/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_GOTOFF,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_GOTOFF",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_GOTPC,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_GOTPC",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

  HOWTO (R_SH_GOTPLT32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* */
	 "R_SH_GOTPLT32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

#ifdef INCLUDE_SHMEDIA
  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOT_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOT_LOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOTPLT_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPLT_LOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_PLT_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_PLT_LOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

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
	 TRUE),			/* pcrel_offset */

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
	 TRUE),			/* pcrel_offset */

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
	 TRUE),			/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOTOFF_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTOFF_LOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_GOTPC_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 TRUE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GOTPC_LOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 TRUE),			/* pcrel_offset */

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
	 TRUE),			/* pcrel_offset */

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
	 TRUE),			/* pcrel_offset */

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
	 TRUE),			/* pcrel_offset */

  /* Used in LD.L, FLD.S et al.	 */
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
	 FALSE),		/* pcrel_offset */

  /* Used in LD.L, FLD.S et al.	 */
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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_COPY64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_COPY64",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_GLOB_DAT64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_GLOB_DAT64",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_JMP_SLOT64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_JMP_SLOT64",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),		/* pcrel_offset */

  HOWTO (R_SH_RELATIVE64,	/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_RELATIVE64",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),		/* pcrel_offset */

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
     use the field being relocated (except R_SH_PT_16).  */

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
	 sh_elf_ignore_reloc,	/* special_function */
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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

  /* Used in MOVI and SHORI (x & 65536).  */
  HOWTO (R_SH_IMM_LOW16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 FALSE,			/* pc_relative */
	 10,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_SH_IMM_LOW16",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0x3fffc00,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 FALSE),		/* pcrel_offset */

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
	 "R_SH_64",		/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 ((bfd_vma) 0) - 1,	/* dst_mask */
	 FALSE),		/* pcrel_offset */

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

#endif
};

static bfd_reloc_status_type
sh_elf_reloc_loop (int r_type ATTRIBUTE_UNUSED, bfd *input_bfd,
		   asection *input_section, bfd_byte *contents,
		   bfd_vma addr, asection *symbol_section,
		   bfd_vma start, bfd_vma end)
{
  static bfd_vma last_addr;
  static asection *last_symbol_section;
  bfd_byte *start_ptr, *ptr, *last_ptr;
  int diff, cum_diff;
  bfd_signed_vma x;
  int insn;

  /* Sanity check the address.  */
  if (addr > bfd_get_section_limit (input_bfd, input_section))
    return bfd_reloc_outofrange;

  /* We require the start and end relocations to be processed consecutively -
     although we allow then to be processed forwards or backwards.  */
  if (! last_addr)
    {
      last_addr = addr;
      last_symbol_section = symbol_section;
      return bfd_reloc_ok;
    }
  if (last_addr != addr)
    abort ();
  last_addr = 0;

  if (! symbol_section || last_symbol_section != symbol_section || end < start)
    return bfd_reloc_outofrange;

  /* Get the symbol_section contents.  */
  if (symbol_section != input_section)
    {
      if (elf_section_data (symbol_section)->this_hdr.contents != NULL)
	contents = elf_section_data (symbol_section)->this_hdr.contents;
      else
	{
	  if (!bfd_malloc_and_get_section (input_bfd, symbol_section,
					   &contents))
	    {
	      if (contents != NULL)
		free (contents);
	      return bfd_reloc_outofrange;
	    }
	}
    }
#define IS_PPI(PTR) ((bfd_get_16 (input_bfd, (PTR)) & 0xfc00) == 0xf800)
  start_ptr = contents + start;
  for (cum_diff = -6, ptr = contents + end; cum_diff < 0 && ptr > start_ptr;)
    {
      for (last_ptr = ptr, ptr -= 4; ptr >= start_ptr && IS_PPI (ptr);)
	ptr -= 2;
      ptr += 2;
      diff = (last_ptr - ptr) >> 1;
      cum_diff += diff & 1;
      cum_diff += diff;
    }
  /* Calculate the start / end values to load into rs / re minus four -
     so that will cancel out the four we would otherwise have to add to
     addr to get the value to subtract in order to get relative addressing.  */
  if (cum_diff >= 0)
    {
      start -= 4;
      end = (ptr + cum_diff * 2) - contents;
    }
  else
    {
      bfd_vma start0 = start - 4;

      while (start0 && IS_PPI (contents + start0))
	start0 -= 2;
      start0 = start - 2 - ((start - start0) & 2);
      start = start0 - cum_diff - 2;
      end = start0;
    }

  if (contents != NULL
      && elf_section_data (symbol_section)->this_hdr.contents != contents)
    free (contents);

  insn = bfd_get_16 (input_bfd, contents + addr);

  x = (insn & 0x200 ? end : start) - addr;
  if (input_section != symbol_section)
    x += ((symbol_section->output_section->vma + symbol_section->output_offset)
	  - (input_section->output_section->vma
	     + input_section->output_offset));
  x >>= 1;
  if (x < -128 || x > 127)
    return bfd_reloc_overflow;

  x = (insn & ~0xff) | (x & 0xff);
  bfd_put_16 (input_bfd, (bfd_vma) x, contents + addr);

  return bfd_reloc_ok;
}

/* This function is used for normal relocs.  This used to be like the COFF
   function, and is almost certainly incorrect for other ELF targets.  */

static bfd_reloc_status_type
sh_elf_reloc (bfd *abfd, arelent *reloc_entry, asymbol *symbol_in,
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

  /* Almost all relocs have to do with relaxing.  If any work must be
     done for them, it has been done in sh_relax_section.  */
  if (r_type == R_SH_IND12W && (symbol_in->flags & BSF_LOCAL) != 0)
    return bfd_reloc_ok;

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
      bfd_put_32 (abfd, (bfd_vma) insn, hit_data);
      break;
    case R_SH_IND12W:
      insn = bfd_get_16 (abfd, hit_data);
      sym_value += reloc_entry->addend;
      sym_value -= (input_section->output_section->vma
		    + input_section->output_offset
		    + addr
		    + 4);
      sym_value += (insn & 0xfff) << 1;
      if (insn & 0x800)
	sym_value -= 0x1000;
      insn = (insn & 0xf000) | (sym_value & 0xfff);
      bfd_put_16 (abfd, (bfd_vma) insn, hit_data);
      if (sym_value < (bfd_vma) -0x1000 || sym_value >= 0x1000)
	return bfd_reloc_overflow;
      break;
    default:
      abort ();
      break;
    }

  return bfd_reloc_ok;
}

/* This function is used for relocs which are only used for relaxing,
   which the linker should otherwise ignore.  */

static bfd_reloc_status_type
sh_elf_ignore_reloc (bfd *abfd ATTRIBUTE_UNUSED, arelent *reloc_entry,
		     asymbol *symbol ATTRIBUTE_UNUSED,
		     void *data ATTRIBUTE_UNUSED, asection *input_section,
		     bfd *output_bfd,
		     char **error_message ATTRIBUTE_UNUSED)
{
  if (output_bfd != NULL)
    reloc_entry->address += input_section->output_offset;
  return bfd_reloc_ok;
}

/* This structure is used to map BFD reloc codes to SH ELF relocs.  */

struct elf_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

/* An array mapping BFD reloc codes to SH ELF relocs.  */

static const struct elf_reloc_map sh_reloc_map[] =
{
  { BFD_RELOC_NONE, R_SH_NONE },
  { BFD_RELOC_32, R_SH_DIR32 },
  { BFD_RELOC_16, R_SH_DIR16 },
  { BFD_RELOC_8, R_SH_DIR8 },
  { BFD_RELOC_CTOR, R_SH_DIR32 },
  { BFD_RELOC_32_PCREL, R_SH_REL32 },
  { BFD_RELOC_SH_PCDISP8BY2, R_SH_DIR8WPN },
  { BFD_RELOC_SH_PCDISP12BY2, R_SH_IND12W },
  { BFD_RELOC_SH_PCRELIMM8BY2, R_SH_DIR8WPZ },
  { BFD_RELOC_SH_PCRELIMM8BY4, R_SH_DIR8WPL },
  { BFD_RELOC_8_PCREL, R_SH_SWITCH8 },
  { BFD_RELOC_SH_SWITCH16, R_SH_SWITCH16 },
  { BFD_RELOC_SH_SWITCH32, R_SH_SWITCH32 },
  { BFD_RELOC_SH_USES, R_SH_USES },
  { BFD_RELOC_SH_COUNT, R_SH_COUNT },
  { BFD_RELOC_SH_ALIGN, R_SH_ALIGN },
  { BFD_RELOC_SH_CODE, R_SH_CODE },
  { BFD_RELOC_SH_DATA, R_SH_DATA },
  { BFD_RELOC_SH_LABEL, R_SH_LABEL },
  { BFD_RELOC_VTABLE_INHERIT, R_SH_GNU_VTINHERIT },
  { BFD_RELOC_VTABLE_ENTRY, R_SH_GNU_VTENTRY },
  { BFD_RELOC_SH_LOOP_START, R_SH_LOOP_START },
  { BFD_RELOC_SH_LOOP_END, R_SH_LOOP_END },
  { BFD_RELOC_SH_TLS_GD_32, R_SH_TLS_GD_32 },
  { BFD_RELOC_SH_TLS_LD_32, R_SH_TLS_LD_32 },
  { BFD_RELOC_SH_TLS_LDO_32, R_SH_TLS_LDO_32 },
  { BFD_RELOC_SH_TLS_IE_32, R_SH_TLS_IE_32 },
  { BFD_RELOC_SH_TLS_LE_32, R_SH_TLS_LE_32 },
  { BFD_RELOC_SH_TLS_DTPMOD32, R_SH_TLS_DTPMOD32 },
  { BFD_RELOC_SH_TLS_DTPOFF32, R_SH_TLS_DTPOFF32 },
  { BFD_RELOC_SH_TLS_TPOFF32, R_SH_TLS_TPOFF32 },
  { BFD_RELOC_32_GOT_PCREL, R_SH_GOT32 },
  { BFD_RELOC_32_PLT_PCREL, R_SH_PLT32 },
  { BFD_RELOC_SH_COPY, R_SH_COPY },
  { BFD_RELOC_SH_GLOB_DAT, R_SH_GLOB_DAT },
  { BFD_RELOC_SH_JMP_SLOT, R_SH_JMP_SLOT },
  { BFD_RELOC_SH_RELATIVE, R_SH_RELATIVE },
  { BFD_RELOC_32_GOTOFF, R_SH_GOTOFF },
  { BFD_RELOC_SH_GOTPC, R_SH_GOTPC },
  { BFD_RELOC_SH_GOTPLT32, R_SH_GOTPLT32 },
#ifdef INCLUDE_SHMEDIA
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
#endif /* not INCLUDE_SHMEDIA */
};

/* Given a BFD reloc code, return the howto structure for the
   corresponding SH ELf reloc.  */

static reloc_howto_type *
sh_elf_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			  bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < sizeof (sh_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (sh_reloc_map[i].bfd_reloc_val == code)
	return &sh_elf_howto_table[(int) sh_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Given an ELF reloc, fill in the howto field of a relent.  */

static void
sh_elf_info_to_howto (bfd *abfd ATTRIBUTE_UNUSED, arelent *cache_ptr,
		      Elf_Internal_Rela *dst)
{
  unsigned int r;

  r = ELF32_R_TYPE (dst->r_info);

  BFD_ASSERT (r < (unsigned int) R_SH_max);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC || r > R_SH_LAST_INVALID_RELOC);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC_2 || r > R_SH_LAST_INVALID_RELOC_2);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC_3 || r > R_SH_LAST_INVALID_RELOC_3);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC_4 || r > R_SH_LAST_INVALID_RELOC_4);
  BFD_ASSERT (r < R_SH_FIRST_INVALID_RELOC_5 || r > R_SH_LAST_INVALID_RELOC_5);

  cache_ptr->howto = &sh_elf_howto_table[r];
}

/* This function handles relaxing for SH ELF.  See the corresponding
   function in coff-sh.c for a description of what this does.  FIXME:
   There is a lot of duplication here between this code and the COFF
   specific code.  The format of relocs and symbols is wound deeply
   into this code, but it would still be better if the duplication
   could be eliminated somehow.  Note in particular that although both
   functions use symbols like R_SH_CODE, those symbols have different
   values; in coff-sh.c they come from include/coff/sh.h, whereas here
   they come from enum elf_sh_reloc_type in include/elf/sh.h.  */

static bfd_boolean
sh_elf_relax_section (bfd *abfd, asection *sec,
		      struct bfd_link_info *link_info, bfd_boolean *again)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  bfd_boolean have_code;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  *again = FALSE;

  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0)
    return TRUE;

#ifdef INCLUDE_SHMEDIA
  if (elf_section_data (sec)->this_hdr.sh_flags
      & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED))
    {
      return TRUE;
    }
#endif

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, sec, NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  have_code = FALSE;

  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma laddr, paddr, symval;
      unsigned short insn;
      Elf_Internal_Rela *irelfn, *irelscan, *irelcount;
      bfd_signed_vma foff;

      if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_CODE)
	have_code = TRUE;

      if (ELF32_R_TYPE (irel->r_info) != (int) R_SH_USES)
	continue;

      /* Get the section contents.  */
      if (contents == NULL)
	{
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      if (!bfd_malloc_and_get_section (abfd, sec, &contents))
		goto error_return;
	    }
	}

      /* The r_addend field of the R_SH_USES reloc will point us to
	 the register load.  The 4 is because the r_addend field is
	 computed as though it were a jump offset, which are based
	 from 4 bytes after the jump instruction.  */
      laddr = irel->r_offset + 4 + irel->r_addend;
      if (laddr >= sec->size)
	{
	  (*_bfd_error_handler) (_("%B: 0x%lx: warning: bad R_SH_USES offset"),
				 abfd,
				 (unsigned long) irel->r_offset);
	  continue;
	}
      insn = bfd_get_16 (abfd, contents + laddr);

      /* If the instruction is not mov.l NN,rN, we don't know what to
	 do.  */
      if ((insn & 0xf000) != 0xd000)
	{
	  ((*_bfd_error_handler)
	   (_("%B: 0x%lx: warning: R_SH_USES points to unrecognized insn 0x%x"),
	    abfd, (unsigned long) irel->r_offset, insn));
	  continue;
	}

      /* Get the address from which the register is being loaded.  The
	 displacement in the mov.l instruction is quadrupled.  It is a
	 displacement from four bytes after the movl instruction, but,
	 before adding in the PC address, two least significant bits
	 of the PC are cleared.  We assume that the section is aligned
	 on a four byte boundary.  */
      paddr = insn & 0xff;
      paddr *= 4;
      paddr += (laddr + 4) &~ (bfd_vma) 3;
      if (paddr >= sec->size)
	{
	  ((*_bfd_error_handler)
	   (_("%B: 0x%lx: warning: bad R_SH_USES load offset"),
	    abfd, (unsigned long) irel->r_offset));
	  continue;
	}

      /* Get the reloc for the address from which the register is
	 being loaded.  This reloc will tell us which function is
	 actually being called.  */
      for (irelfn = internal_relocs; irelfn < irelend; irelfn++)
	if (irelfn->r_offset == paddr
	    && ELF32_R_TYPE (irelfn->r_info) == (int) R_SH_DIR32)
	  break;
      if (irelfn >= irelend)
	{
	  ((*_bfd_error_handler)
	   (_("%B: 0x%lx: warning: could not find expected reloc"),
	    abfd, (unsigned long) paddr));
	  continue;
	}

      /* Read this BFD's symbols if we haven't done so already.  */
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
      if (ELF32_R_SYM (irelfn->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;

	  isym = isymbuf + ELF32_R_SYM (irelfn->r_info);
	  if (isym->st_shndx
	      != (unsigned int) _bfd_elf_section_from_bfd_section (abfd, sec))
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: warning: symbol in unexpected section"),
		abfd, (unsigned long) paddr));
	      continue;
	    }

	  symval = (isym->st_value
		    + sec->output_section->vma
		    + sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  indx = ELF32_R_SYM (irelfn->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
		 symbol.  Just ignore it--it will be caught by the
		 regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      symval += bfd_get_32 (abfd, contents + paddr);

      /* See if this function call can be shortened.  */
      foff = (symval
	      - (irel->r_offset
		 + sec->output_section->vma
		 + sec->output_offset
		 + 4));
      if (foff < -0x1000 || foff >= 0x1000)
	{
	  /* After all that work, we can't shorten this function call.  */
	  continue;
	}

      /* Shorten the function call.  */

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      elf_section_data (sec)->relocs = internal_relocs;
      elf_section_data (sec)->this_hdr.contents = contents;
      symtab_hdr->contents = (unsigned char *) isymbuf;

      /* Replace the jsr with a bsr.  */

      /* Change the R_SH_USES reloc into an R_SH_IND12W reloc, and
	 replace the jsr with a bsr.  */
      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irelfn->r_info), R_SH_IND12W);
      /* We used to test (ELF32_R_SYM (irelfn->r_info) < symtab_hdr->sh_info)
	 here, but that only checks if the symbol is an external symbol,
	 not if the symbol is in a different section.  Besides, we need
	 a consistent meaning for the relocation, so we just assume here that
	 the value of the symbol is not available.  */

      /* We can't fully resolve this yet, because the external
	 symbol value may be changed by future relaxing.  We let
	 the final link phase handle it.  */
      bfd_put_16 (abfd, (bfd_vma) 0xb000, contents + irel->r_offset);

      irel->r_addend = -4;

      /* See if there is another R_SH_USES reloc referring to the same
	 register load.  */
      for (irelscan = internal_relocs; irelscan < irelend; irelscan++)
	if (ELF32_R_TYPE (irelscan->r_info) == (int) R_SH_USES
	    && laddr == irelscan->r_offset + 4 + irelscan->r_addend)
	  break;
      if (irelscan < irelend)
	{
	  /* Some other function call depends upon this register load,
	     and we have not yet converted that function call.
	     Indeed, we may never be able to convert it.  There is
	     nothing else we can do at this point.  */
	  continue;
	}

      /* Look for a R_SH_COUNT reloc on the location where the
	 function address is stored.  Do this before deleting any
	 bytes, to avoid confusion about the address.  */
      for (irelcount = internal_relocs; irelcount < irelend; irelcount++)
	if (irelcount->r_offset == paddr
	    && ELF32_R_TYPE (irelcount->r_info) == (int) R_SH_COUNT)
	  break;

      /* Delete the register load.  */
      if (! sh_elf_relax_delete_bytes (abfd, sec, laddr, 2))
	goto error_return;

      /* That will change things, so, just in case it permits some
	 other function call to come within range, we should relax
	 again.  Note that this is not required, and it may be slow.  */
      *again = TRUE;

      /* Now check whether we got a COUNT reloc.  */
      if (irelcount >= irelend)
	{
	  ((*_bfd_error_handler)
	   (_("%B: 0x%lx: warning: could not find expected COUNT reloc"),
	    abfd, (unsigned long) paddr));
	  continue;
	}

      /* The number of uses is stored in the r_addend field.  We've
	 just deleted one.  */
      if (irelcount->r_addend == 0)
	{
	  ((*_bfd_error_handler) (_("%B: 0x%lx: warning: bad count"),
				  abfd,
				  (unsigned long) paddr));
	  continue;
	}

      --irelcount->r_addend;

      /* If there are no more uses, we can delete the address.  Reload
	 the address from irelfn, in case it was changed by the
	 previous call to sh_elf_relax_delete_bytes.  */
      if (irelcount->r_addend == 0)
	{
	  if (! sh_elf_relax_delete_bytes (abfd, sec, irelfn->r_offset, 4))
	    goto error_return;
	}

      /* We've done all we can with that function call.  */
    }

  /* Look for load and store instructions that we can align on four
     byte boundaries.  */
  if ((elf_elfheader (abfd)->e_flags & EF_SH_MACH_MASK) != EF_SH4
      && have_code)
    {
      bfd_boolean swapped;

      /* Get the section contents.  */
      if (contents == NULL)
	{
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      if (!bfd_malloc_and_get_section (abfd, sec, &contents))
		goto error_return;
	    }
	}

      if (! sh_elf_align_loads (abfd, sec, internal_relocs, contents,
				&swapped))
	goto error_return;

      if (swapped)
	{
	  elf_section_data (sec)->relocs = internal_relocs;
	  elf_section_data (sec)->this_hdr.contents = contents;
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
    }

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
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

/* Delete some bytes from a section while relaxing.  FIXME: There is a
   lot of duplication between this function and sh_relax_delete_bytes
   in coff-sh.c.  */

static bfd_boolean
sh_elf_relax_delete_bytes (bfd *abfd, asection *sec, bfd_vma addr,
			   int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf_Internal_Sym *isymbuf, *isym, *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;
  asection *o;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;
  for (; irel < irelend; irel++)
    {
      if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_ALIGN
	  && irel->r_offset > addr
	  && count < (1 << irel->r_addend))
	{
	  irelalign = irel;
	  toaddr = irel->r_offset;
	  break;
	}
    }

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count,
	   (size_t) (toaddr - addr - count));
  if (irelalign == NULL)
    sec->size -= count;
  else
    {
      int i;

#define NOP_OPCODE (0x0009)

      BFD_ASSERT ((count & 1) == 0);
      for (i = 0; i < count; i += 2)
	bfd_put_16 (abfd, (bfd_vma) NOP_OPCODE, contents + toaddr - count + i);
    }

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      bfd_vma nraddr, stop;
      bfd_vma start = 0;
      int insn = 0;
      int off, adjust, oinsn;
      bfd_signed_vma voff = 0;
      bfd_boolean overflow;

      /* Get the new reloc address.  */
      nraddr = irel->r_offset;
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr)
	  || (ELF32_R_TYPE (irel->r_info) == (int) R_SH_ALIGN
	      && irel->r_offset == toaddr))
	nraddr -= count;

      /* See if this reloc was for the bytes we have deleted, in which
	 case we no longer care about it.  Don't delete relocs which
	 represent addresses, though.  */
      if (irel->r_offset >= addr
	  && irel->r_offset < addr + count
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_ALIGN
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_CODE
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_DATA
	  && ELF32_R_TYPE (irel->r_info) != (int) R_SH_LABEL)
	irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
				     (int) R_SH_NONE);

      /* If this is a PC relative reloc, see if the range it covers
	 includes the bytes we have deleted.  */
      switch ((enum elf_sh_reloc_type) ELF32_R_TYPE (irel->r_info))
	{
	default:
	  break;

	case R_SH_DIR8WPN:
	case R_SH_IND12W:
	case R_SH_DIR8WPZ:
	case R_SH_DIR8WPL:
	  start = irel->r_offset;
	  insn = bfd_get_16 (abfd, contents + nraddr);
	  break;
	}

      switch ((enum elf_sh_reloc_type) ELF32_R_TYPE (irel->r_info))
	{
	default:
	  start = stop = addr;
	  break;

	case R_SH_DIR32:
	  /* If this reloc is against a symbol defined in this
	     section, and the symbol will not be adjusted below, we
	     must check the addend to see it will put the value in
	     range to be adjusted, and hence must be changed.  */
	  if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	    {
	      isym = isymbuf + ELF32_R_SYM (irel->r_info);
	      if (isym->st_shndx == sec_shndx
		  && (isym->st_value <= addr
		      || isym->st_value >= toaddr))
		{
		  bfd_vma val;

		  val = bfd_get_32 (abfd, contents + nraddr);
		  val += isym->st_value;
		  if (val > addr && val < toaddr)
		    bfd_put_32 (abfd, val - count, contents + nraddr);
		}
	    }
	  start = stop = addr;
	  break;

	case R_SH_DIR8WPN:
	  off = insn & 0xff;
	  if (off & 0x80)
	    off -= 0x100;
	  stop = (bfd_vma) ((bfd_signed_vma) start + 4 + off * 2);
	  break;

	case R_SH_IND12W:
	  off = insn & 0xfff;
	  if (! off)
	    {
	      /* This has been made by previous relaxation.  Since the
		 relocation will be against an external symbol, the
		 final relocation will just do the right thing.  */
	      start = stop = addr;
	    }
	  else
	    {
	      if (off & 0x800)
		off -= 0x1000;
	      stop = (bfd_vma) ((bfd_signed_vma) start + 4 + off * 2);

	      /* The addend will be against the section symbol, thus
		 for adjusting the addend, the relevant start is the
		 start of the section.
		 N.B. If we want to abandon in-place changes here and
		 test directly using symbol + addend, we have to take into
		 account that the addend has already been adjusted by -4.  */
	      if (stop > addr && stop < toaddr)
		irel->r_addend -= count;
	    }
	  break;

	case R_SH_DIR8WPZ:
	  off = insn & 0xff;
	  stop = start + 4 + off * 2;
	  break;

	case R_SH_DIR8WPL:
	  off = insn & 0xff;
	  stop = (start & ~(bfd_vma) 3) + 4 + off * 4;
	  break;

	case R_SH_SWITCH8:
	case R_SH_SWITCH16:
	case R_SH_SWITCH32:
	  /* These relocs types represent
	       .word L2-L1
	     The r_addend field holds the difference between the reloc
	     address and L1.  That is the start of the reloc, and
	     adding in the contents gives us the top.  We must adjust
	     both the r_offset field and the section contents.
	     N.B. in gas / coff bfd, the elf bfd r_addend is called r_offset,
	     and the elf bfd r_offset is called r_vaddr.  */

	  stop = irel->r_offset;
	  start = (bfd_vma) ((bfd_signed_vma) stop - (long) irel->r_addend);

	  if (start > addr
	      && start < toaddr
	      && (stop <= addr || stop >= toaddr))
	    irel->r_addend += count;
	  else if (stop > addr
		   && stop < toaddr
		   && (start <= addr || start >= toaddr))
	    irel->r_addend -= count;

	  if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_SWITCH16)
	    voff = bfd_get_signed_16 (abfd, contents + nraddr);
	  else if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_SWITCH8)
	    voff = bfd_get_8 (abfd, contents + nraddr);
	  else
	    voff = bfd_get_signed_32 (abfd, contents + nraddr);
	  stop = (bfd_vma) ((bfd_signed_vma) start + voff);

	  break;

	case R_SH_USES:
	  start = irel->r_offset;
	  stop = (bfd_vma) ((bfd_signed_vma) start
			    + (long) irel->r_addend
			    + 4);
	  break;
	}

      if (start > addr
	  && start < toaddr
	  && (stop <= addr || stop >= toaddr))
	adjust = count;
      else if (stop > addr
	       && stop < toaddr
	       && (start <= addr || start >= toaddr))
	adjust = - count;
      else
	adjust = 0;

      if (adjust != 0)
	{
	  oinsn = insn;
	  overflow = FALSE;
	  switch ((enum elf_sh_reloc_type) ELF32_R_TYPE (irel->r_info))
	    {
	    default:
	      abort ();
	      break;

	    case R_SH_DIR8WPN:
	    case R_SH_DIR8WPZ:
	      insn += adjust / 2;
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = TRUE;
	      bfd_put_16 (abfd, (bfd_vma) insn, contents + nraddr);
	      break;

	    case R_SH_IND12W:
	      insn += adjust / 2;
	      if ((oinsn & 0xf000) != (insn & 0xf000))
		overflow = TRUE;
	      bfd_put_16 (abfd, (bfd_vma) insn, contents + nraddr);
	      break;

	    case R_SH_DIR8WPL:
	      BFD_ASSERT (adjust == count || count >= 4);
	      if (count >= 4)
		insn += adjust / 4;
	      else
		{
		  if ((irel->r_offset & 3) == 0)
		    ++insn;
		}
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = TRUE;
	      bfd_put_16 (abfd, (bfd_vma) insn, contents + nraddr);
	      break;

	    case R_SH_SWITCH8:
	      voff += adjust;
	      if (voff < 0 || voff >= 0xff)
		overflow = TRUE;
	      bfd_put_8 (abfd, voff, contents + nraddr);
	      break;

	    case R_SH_SWITCH16:
	      voff += adjust;
	      if (voff < - 0x8000 || voff >= 0x8000)
		overflow = TRUE;
	      bfd_put_signed_16 (abfd, (bfd_vma) voff, contents + nraddr);
	      break;

	    case R_SH_SWITCH32:
	      voff += adjust;
	      bfd_put_signed_32 (abfd, (bfd_vma) voff, contents + nraddr);
	      break;

	    case R_SH_USES:
	      irel->r_addend += adjust;
	      break;
	    }

	  if (overflow)
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: fatal: reloc overflow while relaxing"),
		abfd, (unsigned long) irel->r_offset));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	}

      irel->r_offset = nraddr;
    }

  /* Look through all the other sections.  If there contain any IMM32
     relocs against internal symbols which we are not going to adjust
     below, we may need to adjust the addends.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      Elf_Internal_Rela *internal_relocs;
      Elf_Internal_Rela *irelscan, *irelscanend;
      bfd_byte *ocontents;

      if (o == sec
	  || (o->flags & SEC_RELOC) == 0
	  || o->reloc_count == 0)
	continue;

      /* We always cache the relocs.  Perhaps, if info->keep_memory is
	 FALSE, we should free them, if we are permitted to, when we
	 leave sh_coff_relax_section.  */
      internal_relocs = (_bfd_elf_link_read_relocs
			 (abfd, o, NULL, (Elf_Internal_Rela *) NULL, TRUE));
      if (internal_relocs == NULL)
	return FALSE;

      ocontents = NULL;
      irelscanend = internal_relocs + o->reloc_count;
      for (irelscan = internal_relocs; irelscan < irelscanend; irelscan++)
	{
	  /* Dwarf line numbers use R_SH_SWITCH32 relocs.  */
	  if (ELF32_R_TYPE (irelscan->r_info) == (int) R_SH_SWITCH32)
	    {
	      bfd_vma start, stop;
	      bfd_signed_vma voff;

	      if (ocontents == NULL)
		{
		  if (elf_section_data (o)->this_hdr.contents != NULL)
		    ocontents = elf_section_data (o)->this_hdr.contents;
		  else
		    {
		      /* We always cache the section contents.
			 Perhaps, if info->keep_memory is FALSE, we
			 should free them, if we are permitted to,
			 when we leave sh_coff_relax_section.  */
		      if (!bfd_malloc_and_get_section (abfd, o, &ocontents))
			{
			  if (ocontents != NULL)
			    free (ocontents);
			  return FALSE;
			}

		      elf_section_data (o)->this_hdr.contents = ocontents;
		    }
		}

	      stop = irelscan->r_offset;
	      start
		= (bfd_vma) ((bfd_signed_vma) stop - (long) irelscan->r_addend);

	      /* STOP is in a different section, so it won't change.  */
	      if (start > addr && start < toaddr)
		irelscan->r_addend += count;

	      voff = bfd_get_signed_32 (abfd, ocontents + irelscan->r_offset);
	      stop = (bfd_vma) ((bfd_signed_vma) start + voff);

	      if (start > addr
		  && start < toaddr
		  && (stop <= addr || stop >= toaddr))
		bfd_put_signed_32 (abfd, (bfd_vma) voff + count,
				   ocontents + irelscan->r_offset);
	      else if (stop > addr
		       && stop < toaddr
		       && (start <= addr || start >= toaddr))
		bfd_put_signed_32 (abfd, (bfd_vma) voff - count,
				   ocontents + irelscan->r_offset);
	    }

	  if (ELF32_R_TYPE (irelscan->r_info) != (int) R_SH_DIR32)
	    continue;

	  if (ELF32_R_SYM (irelscan->r_info) >= symtab_hdr->sh_info)
	    continue;


	  isym = isymbuf + ELF32_R_SYM (irelscan->r_info);
	  if (isym->st_shndx == sec_shndx
	      && (isym->st_value <= addr
		  || isym->st_value >= toaddr))
	    {
	      bfd_vma val;

	      if (ocontents == NULL)
		{
		  if (elf_section_data (o)->this_hdr.contents != NULL)
		    ocontents = elf_section_data (o)->this_hdr.contents;
		  else
		    {
		      /* We always cache the section contents.
			 Perhaps, if info->keep_memory is FALSE, we
			 should free them, if we are permitted to,
			 when we leave sh_coff_relax_section.  */
		      if (!bfd_malloc_and_get_section (abfd, o, &ocontents))
			{
			  if (ocontents != NULL)
			    free (ocontents);
			  return FALSE;
			}

		      elf_section_data (o)->this_hdr.contents = ocontents;
		    }
		}

	      val = bfd_get_32 (abfd, ocontents + irelscan->r_offset);
	      val += isym->st_value;
	      if (val > addr && val < toaddr)
		bfd_put_32 (abfd, val - count,
			    ocontents + irelscan->r_offset);
	    }
	}
    }

  /* Adjust the local symbols defined in this section.  */
  isymend = isymbuf + symtab_hdr->sh_info;
  for (isym = isymbuf; isym < isymend; isym++)
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

  /* See if we can move the ALIGN reloc forward.  We have adjusted
     r_offset for it already.  */
  if (irelalign != NULL)
    {
      bfd_vma alignto, alignaddr;

      alignto = BFD_ALIGN (toaddr, 1 << irelalign->r_addend);
      alignaddr = BFD_ALIGN (irelalign->r_offset,
			     1 << irelalign->r_addend);
      if (alignto != alignaddr)
	{
	  /* Tail recursion.  */
	  return sh_elf_relax_delete_bytes (abfd, sec, alignaddr,
					    (int) (alignto - alignaddr));
	}
    }

  return TRUE;
}

/* Look for loads and stores which we can align to four byte
   boundaries.  This is like sh_align_loads in coff-sh.c.  */

static bfd_boolean
sh_elf_align_loads (bfd *abfd ATTRIBUTE_UNUSED, asection *sec,
		    Elf_Internal_Rela *internal_relocs,
		    bfd_byte *contents ATTRIBUTE_UNUSED,
		    bfd_boolean *pswapped)
{
  Elf_Internal_Rela *irel, *irelend;
  bfd_vma *labels = NULL;
  bfd_vma *label, *label_end;
  bfd_size_type amt;

  *pswapped = FALSE;

  irelend = internal_relocs + sec->reloc_count;

  /* Get all the addresses with labels on them.  */
  amt = sec->reloc_count;
  amt *= sizeof (bfd_vma);
  labels = (bfd_vma *) bfd_malloc (amt);
  if (labels == NULL)
    goto error_return;
  label_end = labels;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_LABEL)
	{
	  *label_end = irel->r_offset;
	  ++label_end;
	}
    }

  /* Note that the assembler currently always outputs relocs in
     address order.  If that ever changes, this code will need to sort
     the label values and the relocs.  */

  label = labels;

  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma start, stop;

      if (ELF32_R_TYPE (irel->r_info) != (int) R_SH_CODE)
	continue;

      start = irel->r_offset;

      for (irel++; irel < irelend; irel++)
	if (ELF32_R_TYPE (irel->r_info) == (int) R_SH_DATA)
	  break;
      if (irel < irelend)
	stop = irel->r_offset;
      else
	stop = sec->size;

      if (! _bfd_sh_align_load_span (abfd, sec, contents, sh_elf_swap_insns,
				     internal_relocs, &label,
				     label_end, start, stop, pswapped))
	goto error_return;
    }

  free (labels);

  return TRUE;

 error_return:
  if (labels != NULL)
    free (labels);
  return FALSE;
}

#ifndef SH64_ELF
/* Swap two SH instructions.  This is like sh_swap_insns in coff-sh.c.  */

static bfd_boolean
sh_elf_swap_insns (bfd *abfd, asection *sec, void *relocs,
		   bfd_byte *contents, bfd_vma addr)
{
  Elf_Internal_Rela *internal_relocs = (Elf_Internal_Rela *) relocs;
  unsigned short i1, i2;
  Elf_Internal_Rela *irel, *irelend;

  /* Swap the instructions themselves.  */
  i1 = bfd_get_16 (abfd, contents + addr);
  i2 = bfd_get_16 (abfd, contents + addr + 2);
  bfd_put_16 (abfd, (bfd_vma) i2, contents + addr);
  bfd_put_16 (abfd, (bfd_vma) i1, contents + addr + 2);

  /* Adjust all reloc addresses.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      enum elf_sh_reloc_type type;
      int add;

      /* There are a few special types of relocs that we don't want to
	 adjust.  These relocs do not apply to the instruction itself,
	 but are only associated with the address.  */
      type = (enum elf_sh_reloc_type) ELF32_R_TYPE (irel->r_info);
      if (type == R_SH_ALIGN
	  || type == R_SH_CODE
	  || type == R_SH_DATA
	  || type == R_SH_LABEL)
	continue;

      /* If an R_SH_USES reloc points to one of the addresses being
	 swapped, we must adjust it.  It would be incorrect to do this
	 for a jump, though, since we want to execute both
	 instructions after the jump.  (We have avoided swapping
	 around a label, so the jump will not wind up executing an
	 instruction it shouldn't).  */
      if (type == R_SH_USES)
	{
	  bfd_vma off;

	  off = irel->r_offset + 4 + irel->r_addend;
	  if (off == addr)
	    irel->r_offset += 2;
	  else if (off == addr + 2)
	    irel->r_offset -= 2;
	}

      if (irel->r_offset == addr)
	{
	  irel->r_offset += 2;
	  add = -2;
	}
      else if (irel->r_offset == addr + 2)
	{
	  irel->r_offset -= 2;
	  add = 2;
	}
      else
	add = 0;

      if (add != 0)
	{
	  bfd_byte *loc;
	  unsigned short insn, oinsn;
	  bfd_boolean overflow;

	  loc = contents + irel->r_offset;
	  overflow = FALSE;
	  switch (type)
	    {
	    default:
	      break;

	    case R_SH_DIR8WPN:
	    case R_SH_DIR8WPZ:
	      insn = bfd_get_16 (abfd, loc);
	      oinsn = insn;
	      insn += add / 2;
	      if ((oinsn & 0xff00) != (insn & 0xff00))
		overflow = TRUE;
	      bfd_put_16 (abfd, (bfd_vma) insn, loc);
	      break;

	    case R_SH_IND12W:
	      insn = bfd_get_16 (abfd, loc);
	      oinsn = insn;
	      insn += add / 2;
	      if ((oinsn & 0xf000) != (insn & 0xf000))
		overflow = TRUE;
	      bfd_put_16 (abfd, (bfd_vma) insn, loc);
	      break;

	    case R_SH_DIR8WPL:
	      /* This reloc ignores the least significant 3 bits of
		 the program counter before adding in the offset.
		 This means that if ADDR is at an even address, the
		 swap will not affect the offset.  If ADDR is an at an
		 odd address, then the instruction will be crossing a
		 four byte boundary, and must be adjusted.  */
	      if ((addr & 3) != 0)
		{
		  insn = bfd_get_16 (abfd, loc);
		  oinsn = insn;
		  insn += add / 2;
		  if ((oinsn & 0xff00) != (insn & 0xff00))
		    overflow = TRUE;
		  bfd_put_16 (abfd, (bfd_vma) insn, loc);
		}

	      break;
	    }

	  if (overflow)
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: fatal: reloc overflow while relaxing"),
		abfd, (unsigned long) irel->r_offset));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	}
    }

  return TRUE;
}
#endif /* defined SH64_ELF */

#ifdef INCLUDE_SHMEDIA

/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 64

/* First entry in an absolute procedure linkage table look like this.  */

static const bfd_byte elf_sh_plt0_entry_be[PLT_ENTRY_SIZE] =
{
  0xcc, 0x00, 0x01, 0x10, /* movi  .got.plt >> 16, r17 */
  0xc8, 0x00, 0x01, 0x10, /* shori .got.plt & 65535, r17 */
  0x89, 0x10, 0x09, 0x90, /* ld.l  r17, 8, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x89, 0x10, 0x05, 0x10, /* ld.l  r17, 4, r17 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
};

static const bfd_byte elf_sh_plt0_entry_le[PLT_ENTRY_SIZE] =
{
  0x10, 0x01, 0x00, 0xcc, /* movi  .got.plt >> 16, r17 */
  0x10, 0x01, 0x00, 0xc8, /* shori .got.plt & 65535, r17 */
  0x90, 0x09, 0x10, 0x89, /* ld.l  r17, 8, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0x10, 0x05, 0x10, 0x89, /* ld.l  r17, 4, r17 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
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

static const bfd_byte elf_sh_plt_entry_be[PLT_ENTRY_SIZE] =
{
  0xcc, 0x00, 0x01, 0x90, /* movi  nameN-in-GOT >> 16, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori nameN-in-GOT & 65535, r25 */
  0x89, 0x90, 0x01, 0x90, /* ld.l  r25, 0, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0xcc, 0x00, 0x01, 0x90, /* movi  .PLT0 >> 16, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori .PLT0 & 65535, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0xcc, 0x00, 0x01, 0x50, /* movi  reloc-offset >> 16, r21 */
  0xc8, 0x00, 0x01, 0x50, /* shori reloc-offset & 65535, r21 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
};

static const bfd_byte elf_sh_plt_entry_le[PLT_ENTRY_SIZE] =
{
  0x90, 0x01, 0x00, 0xcc, /* movi  nameN-in-GOT >> 16, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori nameN-in-GOT & 65535, r25 */
  0x90, 0x01, 0x90, 0x89, /* ld.l  r25, 0, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0x90, 0x01, 0x00, 0xcc, /* movi  .PLT0 >> 16, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori .PLT0 & 65535, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0x50, 0x01, 0x00, 0xcc, /* movi  reloc-offset >> 16, r21 */
  0x50, 0x01, 0x00, 0xc8, /* shori reloc-offset & 65535, r21 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
};

/* Entries in a PIC procedure linkage table look like this.  */

static const bfd_byte elf_sh_pic_plt_entry_be[PLT_ENTRY_SIZE] =
{
  0xcc, 0x00, 0x01, 0x90, /* movi  nameN@GOT >> 16, r25 */
  0xc8, 0x00, 0x01, 0x90, /* shori nameN@GOT & 65535, r25 */
  0x40, 0xc2, 0x65, 0x90, /* ldx.l r12, r25, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0x6f, 0xf0, 0xff, 0xf0, /* nop */
  0xce, 0x00, 0x01, 0x10, /* movi  -GOT_BIAS, r17 */
  0x00, 0xc8, 0x45, 0x10, /* add.l r12, r17, r17 */
  0x89, 0x10, 0x09, 0x90, /* ld.l  r17, 8, r25 */
  0x6b, 0xf1, 0x66, 0x00, /* ptabs r25, tr0 */
  0x89, 0x10, 0x05, 0x10, /* ld.l  r17, 4, r17 */
  0xcc, 0x00, 0x01, 0x50, /* movi  reloc-offset >> 16, r21 */
  0xc8, 0x00, 0x01, 0x50, /* shori reloc-offset & 65535, r21 */
  0x44, 0x01, 0xff, 0xf0, /* blink tr0, r63 */
};

static const bfd_byte elf_sh_pic_plt_entry_le[PLT_ENTRY_SIZE] =
{
  0x90, 0x01, 0x00, 0xcc, /* movi  nameN@GOT >> 16, r25 */
  0x90, 0x01, 0x00, 0xc8, /* shori nameN@GOT & 65535, r25 */
  0x90, 0x65, 0xc2, 0x40, /* ldx.l r12, r25, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0xf0, 0xff, 0xf0, 0x6f, /* nop */
  0x10, 0x01, 0x00, 0xce, /* movi  -GOT_BIAS, r17 */
  0x10, 0x45, 0xc8, 0x00, /* add.l r12, r17, r17 */
  0x90, 0x09, 0x10, 0x89, /* ld.l  r17, 8, r25 */
  0x00, 0x66, 0xf1, 0x6b, /* ptabs r25, tr0 */
  0x10, 0x05, 0x10, 0x89, /* ld.l  r17, 4, r17 */
  0x50, 0x01, 0x00, 0xcc, /* movi  reloc-offset >> 16, r21 */
  0x50, 0x01, 0x00, 0xc8, /* shori reloc-offset & 65535, r21 */
  0xf0, 0xff, 0x01, 0x44, /* blink tr0, r63 */
};

static const bfd_byte *elf_sh_plt0_entry;
static const bfd_byte *elf_sh_plt_entry;
static const bfd_byte *elf_sh_pic_plt_entry;

/* Return size of a PLT entry.  */
#define elf_sh_sizeof_plt(info) PLT_ENTRY_SIZE

/* Return offset of the PLT0 address in an absolute PLT entry.  */
#define elf_sh_plt_plt0_offset(info) 32

/* Return offset of the linker in PLT0 entry.  */
#define elf_sh_plt0_gotplt_offset(info) 0

/* Return offset of the trampoline in PLT entry */
#define elf_sh_plt_temp_offset(info) 33 /* Add one because it's SHmedia.  */

/* Return offset of the symbol in PLT entry.  */
#define elf_sh_plt_symbol_offset(info) 0

/* Return offset of the relocation in PLT entry.  */
#define elf_sh_plt_reloc_offset(info) (info->shared ? 52 : 44)

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

#else
/* The size in bytes of an entry in the procedure linkage table.  */

#define PLT_ENTRY_SIZE 28

/* First entry in an absolute procedure linkage table look like this.  */

/* Note - this code has been "optimised" not to use r2.  r2 is used by
   GCC to return the address of large structures, so it should not be
   corrupted here.  This does mean however, that this PLT does not conform
   to the SH PIC ABI.  That spec says that r0 contains the type of the PLT
   and r2 contains the GOT id.  This version stores the GOT id in r0 and
   ignores the type.  Loaders can easily detect this difference however,
   since the type will always be 0 or 8, and the GOT ids will always be
   greater than or equal to 12.  */
static const bfd_byte elf_sh_plt0_entry_be[PLT_ENTRY_SIZE] =
{
  0xd0, 0x05,	/* mov.l 2f,r0 */
  0x60, 0x02,	/* mov.l @r0,r0 */
  0x2f, 0x06,	/* mov.l r0,@-r15 */
  0xd0, 0x03,	/* mov.l 1f,r0 */
  0x60, 0x02,	/* mov.l @r0,r0 */
  0x40, 0x2b,	/* jmp @r0 */
  0x60, 0xf6,	/*  mov.l @r15+,r0 */
  0x00, 0x09,	/* nop */
  0x00, 0x09,	/* nop */
  0x00, 0x09,	/* nop */
  0, 0, 0, 0,	/* 1: replaced with address of .got.plt + 8.  */
  0, 0, 0, 0,	/* 2: replaced with address of .got.plt + 4.  */
};

static const bfd_byte elf_sh_plt0_entry_le[PLT_ENTRY_SIZE] =
{
  0x05, 0xd0,	/* mov.l 2f,r0 */
  0x02, 0x60,	/* mov.l @r0,r0 */
  0x06, 0x2f,	/* mov.l r0,@-r15 */
  0x03, 0xd0,	/* mov.l 1f,r0 */
  0x02, 0x60,	/* mov.l @r0,r0 */
  0x2b, 0x40,	/* jmp @r0 */
  0xf6, 0x60,	/*  mov.l @r15+,r0 */
  0x09, 0x00,	/* nop */
  0x09, 0x00,	/* nop */
  0x09, 0x00,	/* nop */
  0, 0, 0, 0,	/* 1: replaced with address of .got.plt + 8.  */
  0, 0, 0, 0,	/* 2: replaced with address of .got.plt + 4.  */
};

/* Sebsequent entries in an absolute procedure linkage table look like
   this.  */

static const bfd_byte elf_sh_plt_entry_be[PLT_ENTRY_SIZE] =
{
  0xd0, 0x04,	/* mov.l 1f,r0 */
  0x60, 0x02,	/* mov.l @r0,r0 */
  0xd1, 0x02,	/* mov.l 0f,r1 */
  0x40, 0x2b,   /* jmp @r0 */
  0x60, 0x13,	/*  mov r1,r0 */
  0xd1, 0x03,	/* mov.l 2f,r1 */
  0x40, 0x2b,	/* jmp @r0 */
  0x00, 0x09,	/* nop */
  0, 0, 0, 0,	/* 0: replaced with address of .PLT0.  */
  0, 0, 0, 0,	/* 1: replaced with address of this symbol in .got.  */
  0, 0, 0, 0,	/* 2: replaced with offset into relocation table.  */
};

static const bfd_byte elf_sh_plt_entry_le[PLT_ENTRY_SIZE] =
{
  0x04, 0xd0,	/* mov.l 1f,r0 */
  0x02, 0x60,	/* mov.l @r0,r0 */
  0x02, 0xd1,	/* mov.l 0f,r1 */
  0x2b, 0x40,   /* jmp @r0 */
  0x13, 0x60,	/*  mov r1,r0 */
  0x03, 0xd1,	/* mov.l 2f,r1 */
  0x2b, 0x40,	/* jmp @r0 */
  0x09, 0x00,	/*  nop */
  0, 0, 0, 0,	/* 0: replaced with address of .PLT0.  */
  0, 0, 0, 0,	/* 1: replaced with address of this symbol in .got.  */
  0, 0, 0, 0,	/* 2: replaced with offset into relocation table.  */
};

/* Entries in a PIC procedure linkage table look like this.  */

static const bfd_byte elf_sh_pic_plt_entry_be[PLT_ENTRY_SIZE] =
{
  0xd0, 0x04,	/* mov.l 1f,r0 */
  0x00, 0xce,	/* mov.l @(r0,r12),r0 */
  0x40, 0x2b,	/* jmp @r0 */
  0x00, 0x09,	/*  nop */
  0x50, 0xc2,	/* mov.l @(8,r12),r0 */
  0xd1, 0x03,	/* mov.l 2f,r1 */
  0x40, 0x2b,	/* jmp @r0 */
  0x50, 0xc1,	/*  mov.l @(4,r12),r0 */
  0x00, 0x09,	/* nop */
  0x00, 0x09,	/* nop */
  0, 0, 0, 0,	/* 1: replaced with address of this symbol in .got.  */
  0, 0, 0, 0    /* 2: replaced with offset into relocation table.  */
};

static const bfd_byte elf_sh_pic_plt_entry_le[PLT_ENTRY_SIZE] =
{
  0x04, 0xd0,	/* mov.l 1f,r0 */
  0xce, 0x00,	/* mov.l @(r0,r12),r0 */
  0x2b, 0x40,	/* jmp @r0 */
  0x09, 0x00,	/*  nop */
  0xc2, 0x50,	/* mov.l @(8,r12),r0 */
  0x03, 0xd1,	/* mov.l 2f,r1 */
  0x2b, 0x40,	/* jmp @r0 */
  0xc1, 0x50,	/*  mov.l @(4,r12),r0 */
  0x09, 0x00,	/*  nop */
  0x09, 0x00,	/* nop */
  0, 0, 0, 0,	/* 1: replaced with address of this symbol in .got.  */
  0, 0, 0, 0    /* 2: replaced with offset into relocation table.  */
};

static const bfd_byte *elf_sh_plt0_entry;
static const bfd_byte *elf_sh_plt_entry;
static const bfd_byte *elf_sh_pic_plt_entry;

/* Return size of a PLT entry.  */
#define elf_sh_sizeof_plt(info) PLT_ENTRY_SIZE

/* Return offset of the PLT0 address in an absolute PLT entry.  */
#define elf_sh_plt_plt0_offset(info) 16

/* Return offset of the linker in PLT0 entry.  */
#define elf_sh_plt0_linker_offset(info) 20

/* Return offset of the GOT id in PLT0 entry.  */
#define elf_sh_plt0_gotid_offset(info) 24

/* Return offset of the temporary in PLT entry */
#define elf_sh_plt_temp_offset(info) 8

/* Return offset of the symbol in PLT entry.  */
#define elf_sh_plt_symbol_offset(info) 20

/* Return offset of the relocation in PLT entry.  */
#define elf_sh_plt_reloc_offset(info) 24
#endif

/* The sh linker needs to keep track of the number of relocs that it
   decides to copy as dynamic relocs in check_relocs for each symbol.
   This is so that it can later discard them if they are found to be
   unnecessary.  We store the information in a field extending the
   regular ELF linker hash table.  */

struct elf_sh_dyn_relocs
{
  struct elf_sh_dyn_relocs *next;

  /* The input section of the reloc.  */
  asection *sec;

  /* Total number of relocs copied for the input section.  */
  bfd_size_type count;

  /* Number of pc-relative relocs copied for the input section.  */
  bfd_size_type pc_count;
};

/* sh ELF linker hash entry.  */

struct elf_sh_link_hash_entry
{
  struct elf_link_hash_entry root;

#ifdef INCLUDE_SHMEDIA
  union
  {
    bfd_signed_vma refcount;
    bfd_vma offset;
  } datalabel_got;
#endif

  /* Track dynamic relocs copied for this symbol.  */
  struct elf_sh_dyn_relocs *dyn_relocs;

  bfd_signed_vma gotplt_refcount;

  enum {
    GOT_UNKNOWN = 0, GOT_NORMAL, GOT_TLS_GD, GOT_TLS_IE
  } tls_type;
};

#define sh_elf_hash_entry(ent) ((struct elf_sh_link_hash_entry *)(ent))

struct sh_elf_obj_tdata
{
  struct elf_obj_tdata root;

  /* tls_type for each local got entry.  */
  char *local_got_tls_type;
};

#define sh_elf_tdata(abfd) \
  ((struct sh_elf_obj_tdata *) (abfd)->tdata.any)

#define sh_elf_local_got_tls_type(abfd) \
  (sh_elf_tdata (abfd)->local_got_tls_type)

/* Override the generic function because we need to store sh_elf_obj_tdata
   as the specific tdata.  */

static bfd_boolean
sh_elf_mkobject (bfd *abfd)
{
  bfd_size_type amt = sizeof (struct sh_elf_obj_tdata);
  abfd->tdata.any = bfd_zalloc (abfd, amt);
  if (abfd->tdata.any == NULL)
    return FALSE;
  return TRUE;
}

/* sh ELF linker hash table.  */

struct elf_sh_link_hash_table
{
  struct elf_link_hash_table root;

  /* Short-cuts to get to dynamic linker sections.  */
  asection *sgot;
  asection *sgotplt;
  asection *srelgot;
  asection *splt;
  asection *srelplt;
  asection *sdynbss;
  asection *srelbss;

  /* Small local sym to section mapping cache.  */
  struct sym_sec_cache sym_sec;

  /* A counter or offset to track a TLS got entry.  */
  union
    {
      bfd_signed_vma refcount;
      bfd_vma offset;
    } tls_ldm_got;
};

/* Traverse an sh ELF linker hash table.  */

#define sh_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (bfd_boolean (*) (struct elf_link_hash_entry *, void *)) (func), \
    (info)))

/* Get the sh ELF linker hash table from a link_info structure.  */

#define sh_elf_hash_table(p) \
  ((struct elf_sh_link_hash_table *) ((p)->hash))

/* Create an entry in an sh ELF linker hash table.  */

static struct bfd_hash_entry *
sh_elf_link_hash_newfunc (struct bfd_hash_entry *entry,
			  struct bfd_hash_table *table,
			  const char *string)
{
  struct elf_sh_link_hash_entry *ret =
    (struct elf_sh_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_sh_link_hash_entry *) NULL)
    ret = ((struct elf_sh_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct elf_sh_link_hash_entry)));
  if (ret == (struct elf_sh_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_sh_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct elf_sh_link_hash_entry *) NULL)
    {
      ret->dyn_relocs = NULL;
      ret->gotplt_refcount = 0;
#ifdef INCLUDE_SHMEDIA
      ret->datalabel_got.refcount = ret->root.got.refcount;
#endif
      ret->tls_type = GOT_UNKNOWN;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Create an sh ELF linker hash table.  */

static struct bfd_link_hash_table *
sh_elf_link_hash_table_create (bfd *abfd)
{
  struct elf_sh_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct elf_sh_link_hash_table);

  ret = (struct elf_sh_link_hash_table *) bfd_malloc (amt);
  if (ret == (struct elf_sh_link_hash_table *) NULL)
    return NULL;

  if (!_bfd_elf_link_hash_table_init (&ret->root, abfd,
				      sh_elf_link_hash_newfunc,
				      sizeof (struct elf_sh_link_hash_entry)))
    {
      free (ret);
      return NULL;
    }

  ret->sgot = NULL;
  ret->sgotplt = NULL;
  ret->srelgot = NULL;
  ret->splt = NULL;
  ret->srelplt = NULL;
  ret->sdynbss = NULL;
  ret->srelbss = NULL;
  ret->sym_sec.abfd = NULL;
  ret->tls_ldm_got.refcount = 0;

  return &ret->root.root;
}

/* Create .got, .gotplt, and .rela.got sections in DYNOBJ, and set up
   shortcuts to them in our hash table.  */

static bfd_boolean
create_got_section (bfd *dynobj, struct bfd_link_info *info)
{
  struct elf_sh_link_hash_table *htab;

  if (! _bfd_elf_create_got_section (dynobj, info))
    return FALSE;

  htab = sh_elf_hash_table (info);
  htab->sgot = bfd_get_section_by_name (dynobj, ".got");
  htab->sgotplt = bfd_get_section_by_name (dynobj, ".got.plt");
  if (! htab->sgot || ! htab->sgotplt)
    abort ();

  htab->srelgot = bfd_make_section_with_flags (dynobj, ".rela.got",
					       (SEC_ALLOC | SEC_LOAD
						| SEC_HAS_CONTENTS
						| SEC_IN_MEMORY
						| SEC_LINKER_CREATED
						| SEC_READONLY));
  if (htab->srelgot == NULL
      || ! bfd_set_section_alignment (dynobj, htab->srelgot, 2))
    return FALSE;
  return TRUE;
}

/* Create dynamic sections when linking against a dynamic object.  */

static bfd_boolean
sh_elf_create_dynamic_sections (bfd *abfd, struct bfd_link_info *info)
{
  struct elf_sh_link_hash_table *htab;
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

  htab = sh_elf_hash_table (info);
  if (htab->root.dynamic_sections_created)
    return TRUE;

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
  htab->splt = s;
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
	      (bfd_vma) 0, (const char *) NULL, FALSE,
	      get_elf_backend_data (abfd)->collect, &bh)))
	return FALSE;

      h = (struct elf_link_hash_entry *) bh;
      h->def_regular = 1;
      h->type = STT_OBJECT;
      htab->root.hplt = h;

      if (info->shared
	  && ! bfd_elf_link_record_dynamic_symbol (info, h))
	return FALSE;
    }

  s = bfd_make_section_with_flags (abfd,
				   bed->default_use_rela_p ? ".rela.plt" : ".rel.plt",
				   flags | SEC_READONLY);
  htab->srelplt = s;
  if (s == NULL
      || ! bfd_set_section_alignment (abfd, s, ptralign))
    return FALSE;

  if (htab->sgot == NULL
      && !create_got_section (abfd, info))
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
	relname = (char *) bfd_malloc ((bfd_size_type) strlen (secname) + 6);
	strcpy (relname, ".rela");
	strcat (relname, secname);
	if (bfd_get_section_by_name (abfd, secname))
	  continue;
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
      htab->sdynbss = s;
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
	  htab->srelbss = s;
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
sh_elf_adjust_dynamic_symbol (struct bfd_link_info *info,
			      struct elf_link_hash_entry *h)
{
  struct elf_sh_link_hash_table *htab;
  struct elf_sh_link_hash_entry *eh;
  struct elf_sh_dyn_relocs *p;
  asection *s;
  unsigned int power_of_two;

  htab = sh_elf_hash_table (info);

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (htab->root.dynobj != NULL
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
      if (h->plt.refcount <= 0
	  || SYMBOL_CALLS_LOCAL (info, h)
	  || (ELF_ST_VISIBILITY (h->other) != STV_DEFAULT
	      && h->root.type == bfd_link_hash_undefweak))
	{
	  /* This case can occur if we saw a PLT reloc in an input
	     file, but the symbol was never referred to by a dynamic
	     object.  In such a case, we don't actually need to build
	     a procedure linkage table, and we can just do a REL32
	     reloc instead.  */
	  h->plt.offset = (bfd_vma) -1;
	  h->needs_plt = 0;
	}

      return TRUE;
    }
  else
    h->plt.offset = (bfd_vma) -1;

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->u.weakdef != NULL)
    {
      BFD_ASSERT (h->u.weakdef->root.type == bfd_link_hash_defined
		  || h->u.weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->u.weakdef->root.u.def.section;
      h->root.u.def.value = h->u.weakdef->root.u.def.value;
      if (info->nocopyreloc)
	h->non_got_ref = h->u.weakdef->non_got_ref;
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

  /* If -z nocopyreloc was given, we won't generate them either.  */
  if (info->nocopyreloc)
    {
      h->non_got_ref = 0;
      return TRUE;
    }

  eh = (struct elf_sh_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      s = p->sec->output_section;
      if (s != NULL && (s->flags & (SEC_READONLY | SEC_HAS_CONTENTS)) != 0)
	break;
    }

  /* If we didn't find any dynamic relocs in sections which needs the
     copy reloc, then we'll be keeping the dynamic relocs and avoiding
     the copy reloc.  */
  if (p == NULL)
    {
      h->non_got_ref = 0;
      return TRUE;
    }

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

  s = htab->sdynbss;
  BFD_ASSERT (s != NULL);

  /* We must generate a R_SH_COPY reloc to tell the dynamic linker to
     copy the initial value out of the dynamic object and into the
     runtime process image.  We need to remember the offset into the
     .rela.bss section we are going to use.  */
  if ((h->root.u.def.section->flags & SEC_ALLOC) != 0)
    {
      asection *srel;

      srel = htab->srelbss;
      BFD_ASSERT (srel != NULL);
      srel->size += sizeof (Elf32_External_Rela);
      h->needs_copy = 1;
    }

  /* We need to figure out the alignment required for this symbol.  I
     have no idea how ELF linkers handle this.  */
  power_of_two = bfd_log2 (h->size);
  if (power_of_two > 3)
    power_of_two = 3;

  /* Apply the required alignment.  */
  s->size = BFD_ALIGN (s->size, (bfd_size_type) (1 << power_of_two));
  if (power_of_two > bfd_get_section_alignment (htab->root.dynobj, s))
    {
      if (! bfd_set_section_alignment (htab->root.dynobj, s, power_of_two))
	return FALSE;
    }

  /* Define the symbol as being at this point in the section.  */
  h->root.u.def.section = s;
  h->root.u.def.value = s->size;

  /* Increment the section size to make room for the symbol.  */
  s->size += h->size;

  return TRUE;
}

/* Allocate space in .plt, .got and associated reloc sections for
   dynamic relocs.  */

static bfd_boolean
allocate_dynrelocs (struct elf_link_hash_entry *h, void *inf)
{
  struct bfd_link_info *info;
  struct elf_sh_link_hash_table *htab;
  struct elf_sh_link_hash_entry *eh;
  struct elf_sh_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_indirect)
    return TRUE;

  if (h->root.type == bfd_link_hash_warning)
    /* When warning symbols are created, they **replace** the "real"
       entry in the hash table, thus we never get to see the real
       symbol in a hash traversal.  So look at it now.  */
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  info = (struct bfd_link_info *) inf;
  htab = sh_elf_hash_table (info);

  eh = (struct elf_sh_link_hash_entry *) h;
  if ((h->got.refcount > 0
       || h->forced_local)
      && eh->gotplt_refcount > 0)
    {
      /* The symbol has been forced local, or we have some direct got refs,
	 so treat all the gotplt refs as got refs. */
      h->got.refcount += eh->gotplt_refcount;
      if (h->plt.refcount >= eh->gotplt_refcount)
	h->plt.refcount -= eh->gotplt_refcount;
    }

  if (htab->root.dynamic_sections_created
      && h->plt.refcount > 0
      && (ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
	  || h->root.type != bfd_link_hash_undefweak))
    {
      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && !h->forced_local)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      if (info->shared
	  || WILL_CALL_FINISH_DYNAMIC_SYMBOL (1, 0, h))
	{
	  asection *s = htab->splt;

	  /* If this is the first .plt entry, make room for the special
	     first entry.  */
	  if (s->size == 0)
	    s->size += PLT_ENTRY_SIZE;

	  h->plt.offset = s->size;

	  /* If this symbol is not defined in a regular file, and we are
	     not generating a shared library, then set the symbol to this
	     location in the .plt.  This is required to make function
	     pointers compare as equal between the normal executable and
	     the shared library.  */
	  if (! info->shared
	      && !h->def_regular)
	    {
	      h->root.u.def.section = s;
	      h->root.u.def.value = h->plt.offset;
	    }

	  /* Make room for this entry.  */
	  s->size += PLT_ENTRY_SIZE;

	  /* We also need to make an entry in the .got.plt section, which
	     will be placed in the .got section by the linker script.  */
	  htab->sgotplt->size += 4;

	  /* We also need to make an entry in the .rel.plt section.  */
	  htab->srelplt->size += sizeof (Elf32_External_Rela);
	}
      else
	{
	  h->plt.offset = (bfd_vma) -1;
	  h->needs_plt = 0;
	}
    }
  else
    {
      h->plt.offset = (bfd_vma) -1;
      h->needs_plt = 0;
    }

  if (h->got.refcount > 0)
    {
      asection *s;
      bfd_boolean dyn;
      int tls_type = sh_elf_hash_entry (h)->tls_type;

      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && !h->forced_local)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = htab->sgot;
      h->got.offset = s->size;
      s->size += 4;
      /* R_SH_TLS_GD needs 2 consecutive GOT slots.  */
      if (tls_type == GOT_TLS_GD)
	s->size += 4;
      dyn = htab->root.dynamic_sections_created;
      /* R_SH_TLS_IE_32 needs one dynamic relocation if dynamic,
	 R_SH_TLS_GD needs one if local symbol and two if global.  */
      if ((tls_type == GOT_TLS_GD && h->dynindx == -1)
	  || (tls_type == GOT_TLS_IE && dyn))
	htab->srelgot->size += sizeof (Elf32_External_Rela);
      else if (tls_type == GOT_TLS_GD)
	htab->srelgot->size += 2 * sizeof (Elf32_External_Rela);
      else if ((ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
		|| h->root.type != bfd_link_hash_undefweak)
	       && (info->shared
		   || WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, 0, h)))
	htab->srelgot->size += sizeof (Elf32_External_Rela);
    }
  else
    h->got.offset = (bfd_vma) -1;

#ifdef INCLUDE_SHMEDIA
  if (eh->datalabel_got.refcount > 0)
    {
      asection *s;
      bfd_boolean dyn;

      /* Make sure this symbol is output as a dynamic symbol.
	 Undefined weak syms won't yet be marked as dynamic.  */
      if (h->dynindx == -1
	  && !h->forced_local)
	{
	  if (! bfd_elf_link_record_dynamic_symbol (info, h))
	    return FALSE;
	}

      s = htab->sgot;
      eh->datalabel_got.offset = s->size;
      s->size += 4;
      dyn = htab->root.dynamic_sections_created;
      if (WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h))
	htab->srelgot->size += sizeof (Elf32_External_Rela);
    }
  else
    eh->datalabel_got.offset = (bfd_vma) -1;
#endif

  if (eh->dyn_relocs == NULL)
    return TRUE;

  /* In the shared -Bsymbolic case, discard space allocated for
     dynamic pc-relative relocs against symbols which turn out to be
     defined in regular objects.  For the normal shared case, discard
     space for pc-relative relocs that have become local due to symbol
     visibility changes.  */

  if (info->shared)
    {
      if (SYMBOL_CALLS_LOCAL (info, h))
	{
	  struct elf_sh_dyn_relocs **pp;

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

      /* Also discard relocs on undefined weak syms with non-default
	 visibility.  */
      if (eh->dyn_relocs != NULL
	  && h->root.type == bfd_link_hash_undefweak)
	{
	  if (ELF_ST_VISIBILITY (h->other) != STV_DEFAULT)
	    eh->dyn_relocs = NULL;

	  /* Make sure undefined weak symbols are output as a dynamic
	     symbol in PIEs.  */
	  else if (h->dynindx == -1
		   && !h->forced_local)
	    {
	      if (! bfd_elf_link_record_dynamic_symbol (info, h))
		return FALSE;
	    }
	}
    }
  else
    {
      /* For the non-shared case, discard space for relocs against
	 symbols which turn out to need copy relocs or are not
	 dynamic.  */

      if (!h->non_got_ref
	  && ((h->def_dynamic
	       && !h->def_regular)
	      || (htab->root.dynamic_sections_created
		  && (h->root.type == bfd_link_hash_undefweak
		      || h->root.type == bfd_link_hash_undefined))))
	{
	  /* Make sure this symbol is output as a dynamic symbol.
	     Undefined weak syms won't yet be marked as dynamic.  */
	  if (h->dynindx == -1
	      && !h->forced_local)
	    {
	      if (! bfd_elf_link_record_dynamic_symbol (info, h))
		return FALSE;
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
      sreloc->size += p->count * sizeof (Elf32_External_Rela);
    }

  return TRUE;
}

/* Find any dynamic relocs that apply to read-only sections.  */

static bfd_boolean
readonly_dynrelocs (struct elf_link_hash_entry *h, void *inf)
{
  struct elf_sh_link_hash_entry *eh;
  struct elf_sh_dyn_relocs *p;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  eh = (struct elf_sh_link_hash_entry *) h;
  for (p = eh->dyn_relocs; p != NULL; p = p->next)
    {
      asection *s = p->sec->output_section;

      if (s != NULL && (s->flags & SEC_READONLY) != 0)
	{
	  struct bfd_link_info *info = (struct bfd_link_info *) inf;

	  info->flags |= DF_TEXTREL;

	  /* Not an error, just cut short the traversal.  */
	  return FALSE;
	}
    }
  return TRUE;
}

/* Set the sizes of the dynamic sections.  */

static bfd_boolean
sh_elf_size_dynamic_sections (bfd *output_bfd ATTRIBUTE_UNUSED,
			      struct bfd_link_info *info)
{
  struct elf_sh_link_hash_table *htab;
  bfd *dynobj;
  asection *s;
  bfd_boolean relocs;
  bfd *ibfd;

  htab = sh_elf_hash_table (info);
  dynobj = htab->root.dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (htab->root.dynamic_sections_created)
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

  /* Set up .got offsets for local syms, and space for local dynamic
     relocs.  */
  for (ibfd = info->input_bfds; ibfd != NULL; ibfd = ibfd->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_signed_vma *end_local_got;
      char *local_tls_type;
      bfd_size_type locsymcount;
      Elf_Internal_Shdr *symtab_hdr;
      asection *srel;

      if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour)
	continue;

      for (s = ibfd->sections; s != NULL; s = s->next)
	{
	  struct elf_sh_dyn_relocs *p;

	  for (p = ((struct elf_sh_dyn_relocs *)
		    elf_section_data (s)->local_dynrel);
	       p != NULL;
	       p = p->next)
	    {
	      if (! bfd_is_abs_section (p->sec)
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
		  srel->size += p->count * sizeof (Elf32_External_Rela);
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
#ifdef INCLUDE_SHMEDIA
      /* Count datalabel local GOT.  */
      locsymcount *= 2;
#endif
      end_local_got = local_got + locsymcount;
      local_tls_type = sh_elf_local_got_tls_type (ibfd);
      s = htab->sgot;
      srel = htab->srelgot;
      for (; local_got < end_local_got; ++local_got)
	{
	  if (*local_got > 0)
	    {
	      *local_got = s->size;
	      s->size += 4;
	      if (*local_tls_type == GOT_TLS_GD)
		s->size += 4;
	      if (info->shared)
		srel->size += sizeof (Elf32_External_Rela);
	    }
	  else
	    *local_got = (bfd_vma) -1;
	  ++local_tls_type;
	}
    }

  if (htab->tls_ldm_got.refcount > 0)
    {
      /* Allocate 2 got entries and 1 dynamic reloc for R_SH_TLS_LD_32
	 relocs.  */
      htab->tls_ldm_got.offset = htab->sgot->size;
      htab->sgot->size += 8;
      htab->srelgot->size += sizeof (Elf32_External_Rela);
    }
  else
    htab->tls_ldm_got.offset = -1;

  /* Allocate global sym .plt and .got entries, and space for global
     sym dynamic relocs.  */
  elf_link_hash_traverse (&htab->root, allocate_dynrelocs, info);

  /* We now have determined the sizes of the various dynamic sections.
     Allocate memory for them.  */
  relocs = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      if (s == htab->splt
	  || s == htab->sgot
	  || s == htab->sgotplt
	  || s == htab->sdynbss)
	{
	  /* Strip this section if we don't need it; see the
	     comment below.  */
	}
      else if (strncmp (bfd_get_section_name (dynobj, s), ".rela", 5) == 0)
	{
	  if (s->size != 0 && s != htab->srelplt)
	    relocs = TRUE;

	  /* We use the reloc_count field as a counter if we need
	     to copy relocs into the output file.  */
	  s->reloc_count = 0;
	}
      else
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

      /* Allocate memory for the section contents.  We use bfd_zalloc
	 here in case unused entries are not reclaimed before the
	 section's contents are written out.  This should not happen,
	 but this way if it does, we get a R_SH_NONE reloc instead
	 of garbage.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->size);
      if (s->contents == NULL)
	return FALSE;
    }

  if (htab->root.dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in sh_elf_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (info->executable)
	{
	  if (! add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (htab->splt->size != 0)
	{
	  if (! add_dynamic_entry (DT_PLTGOT, 0)
	      || ! add_dynamic_entry (DT_PLTRELSZ, 0)
	      || ! add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || ! add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relocs)
	{
	  if (! add_dynamic_entry (DT_RELA, 0)
	      || ! add_dynamic_entry (DT_RELASZ, 0)
	      || ! add_dynamic_entry (DT_RELAENT,
				      sizeof (Elf32_External_Rela)))
	    return FALSE;

	  /* If any dynamic relocs apply to a read-only section,
	     then we need a DT_TEXTREL entry.  */
	  if ((info->flags & DF_TEXTREL) == 0)
	    elf_link_hash_traverse (&htab->root, readonly_dynrelocs, info);

	  if ((info->flags & DF_TEXTREL) != 0)
	    {
	      if (! add_dynamic_entry (DT_TEXTREL, 0))
		return FALSE;
	    }
	}
    }
#undef add_dynamic_entry

  return TRUE;
}

/* Relocate an SH ELF section.  */

static bfd_boolean
sh_elf_relocate_section (bfd *output_bfd, struct bfd_link_info *info,
			 bfd *input_bfd, asection *input_section,
			 bfd_byte *contents, Elf_Internal_Rela *relocs,
			 Elf_Internal_Sym *local_syms,
			 asection **local_sections)
{
  struct elf_sh_link_hash_table *htab;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;
  bfd *dynobj;
  bfd_vma *local_got_offsets;
  asection *sgot;
  asection *sgotplt;
  asection *splt;
  asection *sreloc;
  asection *srelgot;

  htab = sh_elf_hash_table (info);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  dynobj = htab->root.dynobj;
  local_got_offsets = elf_local_got_offsets (input_bfd);

  sgot = htab->sgot;
  sgotplt = htab->sgotplt;
  splt = htab->splt;
  sreloc = NULL;
  srelgot = NULL;

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
      bfd_vma addend = (bfd_vma) 0;
      bfd_reloc_status_type r;
      int seen_stt_datalabel = 0;
      bfd_vma off;
      int tls_type;

      r_symndx = ELF32_R_SYM (rel->r_info);

      r_type = ELF32_R_TYPE (rel->r_info);

      /* Many of the relocs are only used for relaxing, and are
	 handled entirely by the relaxation code.  */
      if (r_type >= (int) R_SH_GNU_VTINHERIT
	  && r_type <= (int) R_SH_LABEL)
	continue;
      if (r_type == (int) R_SH_NONE)
	continue;

      if (r_type < 0
	  || r_type >= R_SH_max
	  || (r_type >= (int) R_SH_FIRST_INVALID_RELOC
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC)
	  || (   r_type >= (int) R_SH_FIRST_INVALID_RELOC_3
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC_3)
	  || (   r_type >= (int) R_SH_FIRST_INVALID_RELOC_4
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC_4)
	  || (   r_type >= (int) R_SH_FIRST_INVALID_RELOC_5
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC_5)
	  || (r_type >= (int) R_SH_FIRST_INVALID_RELOC_2
	      && r_type <= (int) R_SH_LAST_INVALID_RELOC_2))
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      howto = sh_elf_howto_table + r_type;

      /* For relocs that aren't partial_inplace, we get the addend from
	 the relocation.  */
      if (! howto->partial_inplace)
	addend = rel->r_addend;

      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = (sec->output_section->vma
			+ sec->output_offset
			+ sym->st_value);
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
		{
		  if (! howto->partial_inplace)
		    {
		      /* For relocations with the addend in the
			 relocation, we need just to update the addend.
			 All real relocs are of type partial_inplace; this
			 code is mostly for completeness.  */
		      rel->r_addend += sec->output_offset + sym->st_value;

		      continue;
		    }

		  /* Relocs of type partial_inplace need to pick up the
		     contents in the contents and add the offset resulting
		     from the changed location of the section symbol.
		     Using _bfd_final_link_relocate (e.g. goto
		     final_link_relocate) here would be wrong, because
		     relocations marked pc_relative would get the current
		     location subtracted, and we must only do that at the
		     final link.  */
		  r = _bfd_relocate_contents (howto, input_bfd,
					      sec->output_offset
					      + sym->st_value,
					      contents + rel->r_offset);
		  goto relocation_done;
		}

	      continue;
	    }
	  else if (! howto->partial_inplace)
	    {
	      relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	      addend = rel->r_addend;
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
	  /* FIXME: Ought to make use of the RELOC_FOR_GLOBAL_SYMBOL macro.  */

	  /* Section symbol are never (?) placed in the hash table, so
	     we can just ignore hash relocations when creating a
	     relocatable object file.  */
	  if (info->relocatable)
	    continue;

	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    {
#ifdef INCLUDE_SHMEDIA
	      /* If the reference passes a symbol marked with
		 STT_DATALABEL, then any STO_SH5_ISA32 on the final value
		 doesn't count.  */
	      seen_stt_datalabel |= h->type == STT_DATALABEL;
#endif
	      h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      bfd_boolean dyn;

	      dyn = htab->root.dynamic_sections_created;
	      sec = h->root.u.def.section;
	      /* In these cases, we don't need the relocation value.
		 We check specially because in some obscure cases
		 sec->output_section will be NULL.  */
	      if (r_type == R_SH_GOTPC
		  || r_type == R_SH_GOTPC_LOW16
		  || r_type == R_SH_GOTPC_MEDLOW16
		  || r_type == R_SH_GOTPC_MEDHI16
		  || r_type == R_SH_GOTPC_HI16
		  || ((r_type == R_SH_PLT32
		       || r_type == R_SH_PLT_LOW16
		       || r_type == R_SH_PLT_MEDLOW16
		       || r_type == R_SH_PLT_MEDHI16
		       || r_type == R_SH_PLT_HI16)
		      && h->plt.offset != (bfd_vma) -1)
		  || ((r_type == R_SH_GOT32
		       || r_type == R_SH_GOT_LOW16
		       || r_type == R_SH_GOT_MEDLOW16
		       || r_type == R_SH_GOT_MEDHI16
		       || r_type == R_SH_GOT_HI16)
		      && WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h)
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
		      && ((r_type == R_SH_DIR32
			   && !h->forced_local)
			  || (r_type == R_SH_REL32
			      && !SYMBOL_CALLS_LOCAL (info, h)))
		      && ((input_section->flags & SEC_ALLOC) != 0
			  /* DWARF will emit R_SH_DIR32 relocations in its
			     sections against symbols defined externally
			     in shared libraries.  We can't do anything
			     with them here.  */
			  || ((input_section->flags & SEC_DEBUGGING) != 0
			      && h->def_dynamic)))
		  /* Dynamic relocs are not propagated for SEC_DEBUGGING
		     sections because such sections are not SEC_ALLOC and
		     thus ld.so will not process them.  */
		  || (sec->output_section == NULL
		      && ((input_section->flags & SEC_DEBUGGING) != 0
			  && h->def_dynamic))
		  || (sec->output_section == NULL
		      && (sh_elf_hash_entry (h)->tls_type == GOT_TLS_IE
			  || sh_elf_hash_entry (h)->tls_type == GOT_TLS_GD)))
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
		  return FALSE;
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
	      if (! info->callbacks->undefined_symbol
		  (info, h->root.root.string, input_bfd,
		   input_section, rel->r_offset,
		   (info->unresolved_syms_in_objects == RM_GENERATE_ERROR
		    || ELF_ST_VISIBILITY (h->other))))
		return FALSE;
	      relocation = 0;
	    }
	}

      switch ((int) r_type)
	{
	final_link_relocate:
	  /* COFF relocs don't use the addend. The addend is used for
	     R_SH_DIR32 to be compatible with other compilers.  */
	  r = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents, rel->r_offset,
					relocation, addend);
	  break;

	case R_SH_IND12W:
	  goto final_link_relocate;

	case R_SH_DIR8WPN:
	case R_SH_DIR8WPZ:
	case R_SH_DIR8WPL:
	  /* If the reloc is against the start of this section, then
	     the assembler has already taken care of it and the reloc
	     is here only to assist in relaxing.  If the reloc is not
	     against the start of this section, then it's against an
	     external symbol and we must deal with it ourselves.  */
	  if (input_section->output_section->vma + input_section->output_offset
	      != relocation)
	    {
	      int disp = (relocation
			  - input_section->output_section->vma
			  - input_section->output_offset
			  - rel->r_offset);
	      int mask = 0;
	      switch (r_type)
		{
		case R_SH_DIR8WPN:
		case R_SH_DIR8WPZ: mask = 1; break;
		case R_SH_DIR8WPL: mask = 3; break;
		default: mask = 0; break;
		}
	      if (disp & mask)
		{
		  ((*_bfd_error_handler)
		   (_("%B: 0x%lx: fatal: unaligned branch target for relax-support relocation"),
		    input_section->owner,
		    (unsigned long) rel->r_offset));
		  bfd_set_error (bfd_error_bad_value);
		  return FALSE;
		}
	      relocation -= 4;
	      goto final_link_relocate;
	    }
	  r = bfd_reloc_ok;
	  break;

	default:
#ifdef INCLUDE_SHMEDIA
	  if (shmedia_prepare_reloc (info, input_bfd, input_section,
				     contents, rel, &relocation))
	    goto final_link_relocate;
#endif
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;

	case R_SH_DIR16:
	case R_SH_DIR8:
	case R_SH_DIR8U:
	case R_SH_DIR8S:
	case R_SH_DIR4U:
	  goto final_link_relocate;

	case R_SH_DIR8UL:
	case R_SH_DIR4UL:
	  if (relocation & 3)
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: fatal: unaligned %s relocation 0x%lx"),
		input_section->owner,
		(unsigned long) rel->r_offset, howto->name, 
		(unsigned long) relocation));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	  goto final_link_relocate;

	case R_SH_DIR8UW:
	case R_SH_DIR8SW:
	case R_SH_DIR4UW:
	  if (relocation & 1)
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: fatal: unaligned %s relocation 0x%lx"),
		input_section->owner,
		(unsigned long) rel->r_offset, howto->name, 
		(unsigned long) relocation));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	  goto final_link_relocate;

	case R_SH_PSHA:
	  if ((signed int)relocation < -32
	      || (signed int)relocation > 32)
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: fatal: R_SH_PSHA relocation %d not in range -32..32"),
		input_section->owner,
		(unsigned long) rel->r_offset,
		(unsigned long) relocation));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	  goto final_link_relocate;

	case R_SH_PSHL:
	  if ((signed int)relocation < -16
	      || (signed int)relocation > 16)
	    {
	      ((*_bfd_error_handler)
	       (_("%B: 0x%lx: fatal: R_SH_PSHL relocation %d not in range -32..32"),
		input_section->owner,
		(unsigned long) rel->r_offset,
		(unsigned long) relocation));
	      bfd_set_error (bfd_error_bad_value);
	      return FALSE;
	    }
	  goto final_link_relocate;

	case R_SH_DIR32:
	case R_SH_REL32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_IMM_LOW16_PCREL:
	case R_SH_IMM_MEDLOW16_PCREL:
	case R_SH_IMM_MEDHI16_PCREL:
	case R_SH_IMM_HI16_PCREL:
#endif
	  if (info->shared
	      && (h == NULL
		  || ELF_ST_VISIBILITY (h->other) == STV_DEFAULT
		  || h->root.type != bfd_link_hash_undefweak)
	      && r_symndx != 0
	      && (input_section->flags & SEC_ALLOC) != 0
	      && (r_type == R_SH_DIR32
		  || !SYMBOL_CALLS_LOCAL (info, h)))
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

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info, input_section,
					 rel->r_offset);
	      if (outrel.r_offset == (bfd_vma) -1)
		skip = TRUE;
	      else if (outrel.r_offset == (bfd_vma) -2)
		skip = TRUE, relocate = TRUE;
	      outrel.r_offset += (input_section->output_section->vma
				  + input_section->output_offset);

	      if (skip)
		memset (&outrel, 0, sizeof outrel);
	      else if (r_type == R_SH_REL32)
		{
		  BFD_ASSERT (h != NULL && h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, R_SH_REL32);
		  outrel.r_addend
		    = bfd_get_32 (input_bfd, contents + rel->r_offset);
		}
#ifdef INCLUDE_SHMEDIA
	      else if (r_type == R_SH_IMM_LOW16_PCREL
		       || r_type == R_SH_IMM_MEDLOW16_PCREL
		       || r_type == R_SH_IMM_MEDHI16_PCREL
		       || r_type == R_SH_IMM_HI16_PCREL)
		{
		  BFD_ASSERT (h != NULL && h->dynindx != -1);
		  outrel.r_info = ELF32_R_INFO (h->dynindx, r_type);
		  outrel.r_addend = addend;
		}
#endif
	      else
		{
		  /* h->dynindx may be -1 if this symbol was marked to
		     become local.  */
		  if (h == NULL
		      || ((info->symbolic || h->dynindx == -1)
			  && h->def_regular))
		    {
		      relocate = TRUE;
		      outrel.r_info = ELF32_R_INFO (0, R_SH_RELATIVE);
		      outrel.r_addend
			= relocation + bfd_get_32 (input_bfd,
						   contents + rel->r_offset);
		    }
		  else
		    {
		      BFD_ASSERT (h->dynindx != -1);
		      outrel.r_info = ELF32_R_INFO (h->dynindx, R_SH_DIR32);
		      outrel.r_addend
			= relocation + bfd_get_32 (input_bfd,
						   contents + rel->r_offset);
		    }
		}

	      loc = sreloc->contents;
	      loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      /* If this reloc is against an external symbol, we do
		 not want to fiddle with the addend.  Otherwise, we
		 need to include the symbol value so that it becomes
		 an addend for the dynamic reloc.  */
	      if (! relocate)
		continue;
	    }
	  goto final_link_relocate;

	case R_SH_GOTPLT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOTPLT_LOW16:
	case R_SH_GOTPLT_MEDLOW16:
	case R_SH_GOTPLT_MEDHI16:
	case R_SH_GOTPLT_HI16:
	case R_SH_GOTPLT10BY4:
	case R_SH_GOTPLT10BY8:
#endif
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  if (h == NULL
	      || h->forced_local
	      || ! info->shared
	      || info->symbolic
	      || h->dynindx == -1
	      || h->plt.offset == (bfd_vma) -1
	      || h->got.offset != (bfd_vma) -1)
	    goto force_got;

	  /* Relocation is to the entry for this symbol in the global
	     offset table extension for the procedure linkage table.  */

	  BFD_ASSERT (sgotplt != NULL);
	  relocation = (sgotplt->output_offset
			+ ((h->plt.offset / elf_sh_sizeof_plt (info)
			    - 1 + 3) * 4));

#ifdef GOT_BIAS
	  relocation -= GOT_BIAS;
#endif

	  goto final_link_relocate;

	force_got:
	case R_SH_GOT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOT_LOW16:
	case R_SH_GOT_MEDLOW16:
	case R_SH_GOT_MEDHI16:
	case R_SH_GOT_HI16:
	case R_SH_GOT10BY4:
	case R_SH_GOT10BY8:
#endif
	  /* Relocation is to the entry for this symbol in the global
	     offset table.  */

	  BFD_ASSERT (sgot != NULL);

	  if (h != NULL)
	    {
	      bfd_boolean dyn;

	      off = h->got.offset;
#ifdef INCLUDE_SHMEDIA
	      if (seen_stt_datalabel)
		{
		  struct elf_sh_link_hash_entry *hsh;

		  hsh = (struct elf_sh_link_hash_entry *)h;
		  off = hsh->datalabel_got.offset;
		}
#endif
	      BFD_ASSERT (off != (bfd_vma) -1);

	      dyn = htab->root.dynamic_sections_created;
	      if (! WILL_CALL_FINISH_DYNAMIC_SYMBOL (dyn, info->shared, h)
		  || (info->shared
		      && SYMBOL_REFERENCES_LOCAL (info, h))
		  || (ELF_ST_VISIBILITY (h->other)
		      && h->root.type == bfd_link_hash_undefweak))
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
		      bfd_put_32 (output_bfd, relocation,
				  sgot->contents + off);
#ifdef INCLUDE_SHMEDIA
		      if (seen_stt_datalabel)
			{
			  struct elf_sh_link_hash_entry *hsh;

			  hsh = (struct elf_sh_link_hash_entry *)h;
			  hsh->datalabel_got.offset |= 1;
			}
		      else
#endif
			h->got.offset |= 1;
		    }
		}

	      relocation = sgot->output_offset + off;
	    }
	  else
	    {
#ifdef INCLUDE_SHMEDIA
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
#endif
	      BFD_ASSERT (local_got_offsets != NULL
			  && local_got_offsets[r_symndx] != (bfd_vma) -1);

	      off = local_got_offsets[r_symndx];
#ifdef INCLUDE_SHMEDIA
		}
#endif

	      /* The offset must always be a multiple of 4.  We use
		 the least significant bit to record whether we have
		 already generated the necessary reloc.  */
	      if ((off & 1) != 0)
		off &= ~1;
	      else
		{
		  bfd_put_32 (output_bfd, relocation, sgot->contents + off);

		  if (info->shared)
		    {
		      Elf_Internal_Rela outrel;
		      bfd_byte *loc;

		      if (srelgot == NULL)
			{
			  srelgot = bfd_get_section_by_name (dynobj,
							     ".rela.got");
			  BFD_ASSERT (srelgot != NULL);
			}

		      outrel.r_offset = (sgot->output_section->vma
					 + sgot->output_offset
					 + off);
		      outrel.r_info = ELF32_R_INFO (0, R_SH_RELATIVE);
		      outrel.r_addend = relocation;
		      loc = srelgot->contents;
		      loc += srelgot->reloc_count++ * sizeof (Elf32_External_Rela);
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
		    }

#ifdef INCLUDE_SHMEDIA
		  if (rel->r_addend)
		    local_got_offsets[symtab_hdr->sh_info + r_symndx] |= 1;
		  else
#endif
		    local_got_offsets[r_symndx] |= 1;
		}

	      relocation = sgot->output_offset + off;
	    }

#ifdef GOT_BIAS
	  relocation -= GOT_BIAS;
#endif

	  goto final_link_relocate;

	case R_SH_GOTOFF:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOTOFF_LOW16:
	case R_SH_GOTOFF_MEDLOW16:
	case R_SH_GOTOFF_MEDHI16:
	case R_SH_GOTOFF_HI16:
#endif
	  /* Relocation is relative to the start of the global offset
	     table.  */

	  BFD_ASSERT (sgot != NULL);

	  /* Note that sgot->output_offset is not involved in this
	     calculation.  We always want the start of .got.  If we
	     defined _GLOBAL_OFFSET_TABLE in a different way, as is
	     permitted by the ABI, we might have to change this
	     calculation.  */
	  relocation -= sgot->output_section->vma;

#ifdef GOT_BIAS
	  relocation -= GOT_BIAS;
#endif

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_GOTPC:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOTPC_LOW16:
	case R_SH_GOTPC_MEDLOW16:
	case R_SH_GOTPC_MEDHI16:
	case R_SH_GOTPC_HI16:
#endif
	  /* Use global offset table as symbol value.  */

	  BFD_ASSERT (sgot != NULL);
	  relocation = sgot->output_section->vma;

#ifdef GOT_BIAS
	  relocation += GOT_BIAS;
#endif

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_PLT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_PLT_LOW16:
	case R_SH_PLT_MEDLOW16:
	case R_SH_PLT_MEDHI16:
	case R_SH_PLT_HI16:
#endif
	  /* Relocation is to the entry for this symbol in the
	     procedure linkage table.  */

	  /* Resolve a PLT reloc against a local symbol directly,
	     without using the procedure linkage table.  */
	  if (h == NULL)
	    goto final_link_relocate;

	  if (h->forced_local)
	    goto final_link_relocate;

	  if (h->plt.offset == (bfd_vma) -1)
	    {
	      /* We didn't make a PLT entry for this symbol.  This
		 happens when statically linking PIC code, or when
		 using -Bsymbolic.  */
	      goto final_link_relocate;
	    }

	  BFD_ASSERT (splt != NULL);
	  relocation = (splt->output_section->vma
			+ splt->output_offset
			+ h->plt.offset);

#ifdef INCLUDE_SHMEDIA
	  relocation++;
#endif

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_LOOP_START:
	  {
	    static bfd_vma start, end;

	    start = (relocation + rel->r_addend
		     - (sec->output_section->vma + sec->output_offset));
	    r = sh_elf_reloc_loop (r_type, input_bfd, input_section, contents,
				   rel->r_offset, sec, start, end);
	    break;

	case R_SH_LOOP_END:
	    end = (relocation + rel->r_addend
		   - (sec->output_section->vma + sec->output_offset));
	    r = sh_elf_reloc_loop (r_type, input_bfd, input_section, contents,
				   rel->r_offset, sec, start, end);
	    break;
	  }

	case R_SH_TLS_GD_32:
	case R_SH_TLS_IE_32:
	  r_type = sh_elf_optimized_tls_reloc (info, r_type, h == NULL);
	  tls_type = GOT_UNKNOWN;
	  if (h == NULL && local_got_offsets)
	    tls_type = sh_elf_local_got_tls_type (input_bfd) [r_symndx];
	  else if (h != NULL)
	    {
	      tls_type = sh_elf_hash_entry (h)->tls_type;
	      if (! info->shared
		  && (h->dynindx == -1
		      || h->def_regular))
		r_type = R_SH_TLS_LE_32;
	    }

	  if (r_type == R_SH_TLS_GD_32 && tls_type == GOT_TLS_IE)
	    r_type = R_SH_TLS_IE_32;

	  if (r_type == R_SH_TLS_LE_32)
	    {
	      bfd_vma offset;
	      unsigned short insn;

	      if (ELF32_R_TYPE (rel->r_info) == R_SH_TLS_GD_32)
		{
		  /* GD->LE transition:
		       mov.l 1f,r4; mova 2f,r0; mov.l 2f,r1; add r0,r1;
		       jsr @r1; add r12,r4; bra 3f; nop; .align 2;
		       1: .long x$TLSGD; 2: .long __tls_get_addr@PLT; 3:
		     We change it into:
		       mov.l 1f,r4; stc gbr,r0; add r4,r0; nop;
		       nop; nop; ...
		       1: .long x@TPOFF; 2: .long __tls_get_addr@PLT; 3:.  */

		  offset = rel->r_offset;
		  BFD_ASSERT (offset >= 16);
		  /* Size of GD instructions is 16 or 18.  */
		  offset -= 16;
		  insn = bfd_get_16 (input_bfd, contents + offset + 0);
		  if ((insn & 0xff00) == 0xc700)
		    {
		      BFD_ASSERT (offset >= 2);
		      offset -= 2;
		      insn = bfd_get_16 (input_bfd, contents + offset + 0);
		    }

		  BFD_ASSERT ((insn & 0xff00) == 0xd400);
		  insn = bfd_get_16 (input_bfd, contents + offset + 2);
		  BFD_ASSERT ((insn & 0xff00) == 0xc700);
		  insn = bfd_get_16 (input_bfd, contents + offset + 4);
		  BFD_ASSERT ((insn & 0xff00) == 0xd100);
		  insn = bfd_get_16 (input_bfd, contents + offset + 6);
		  BFD_ASSERT (insn == 0x310c);
		  insn = bfd_get_16 (input_bfd, contents + offset + 8);
		  BFD_ASSERT (insn == 0x410b);
		  insn = bfd_get_16 (input_bfd, contents + offset + 10);
		  BFD_ASSERT (insn == 0x34cc);

		  bfd_put_16 (output_bfd, 0x0012, contents + offset + 2);
		  bfd_put_16 (output_bfd, 0x304c, contents + offset + 4);
		  bfd_put_16 (output_bfd, 0x0009, contents + offset + 6);
		  bfd_put_16 (output_bfd, 0x0009, contents + offset + 8);
		  bfd_put_16 (output_bfd, 0x0009, contents + offset + 10);
		}
	      else
		{
		  int index;

		  /* IE->LE transition:
		     mov.l 1f,r0; stc gbr,rN; mov.l @(r0,r12),rM;
		     bra 2f; add ...; .align 2; 1: x@GOTTPOFF; 2:
		     We change it into:
		     mov.l .Ln,rM; stc gbr,rN; nop; ...;
		     1: x@TPOFF; 2:.  */

		  offset = rel->r_offset;
		  BFD_ASSERT (offset >= 16);
		  /* Size of IE instructions is 10 or 12.  */
		  offset -= 10;
		  insn = bfd_get_16 (input_bfd, contents + offset + 0);
		  if ((insn & 0xf0ff) == 0x0012)
		    {
		      BFD_ASSERT (offset >= 2);
		      offset -= 2;
		      insn = bfd_get_16 (input_bfd, contents + offset + 0);
		    }

		  BFD_ASSERT ((insn & 0xff00) == 0xd000);
		  index = insn & 0x00ff;
		  insn = bfd_get_16 (input_bfd, contents + offset + 2);
		  BFD_ASSERT ((insn & 0xf0ff) == 0x0012);
		  insn = bfd_get_16 (input_bfd, contents + offset + 4);
		  BFD_ASSERT ((insn & 0xf0ff) == 0x00ce);
		  insn = 0xd000 | (insn & 0x0f00) | index;
		  bfd_put_16 (output_bfd, insn, contents + offset + 0);
		  bfd_put_16 (output_bfd, 0x0009, contents + offset + 4);
		}

	      bfd_put_32 (output_bfd, tpoff (info, relocation),
			  contents + rel->r_offset);
	      continue;
	    }

	  sgot = htab->sgot;
	  if (sgot == NULL)
	    abort ();

	  if (h != NULL)
	    off = h->got.offset;
	  else
	    {
	      if (local_got_offsets == NULL)
		abort ();

	      off = local_got_offsets[r_symndx];
	    }

	  /* Relocate R_SH_TLS_IE_32 directly when statically linking.  */
	  if (r_type == R_SH_TLS_IE_32
	      && ! htab->root.dynamic_sections_created)
	    {
	      off &= ~1;
	      bfd_put_32 (output_bfd, tpoff (info, relocation),
			  sgot->contents + off);
	      bfd_put_32 (output_bfd, sgot->output_offset + off,
			  contents + rel->r_offset);
	      continue;
	    }

	  if ((off & 1) != 0)
	    off &= ~1;
	  else
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      int dr_type, indx;

	      if (srelgot == NULL)
		{
		  srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
		  BFD_ASSERT (srelgot != NULL);
		}

	      outrel.r_offset = (sgot->output_section->vma
				 + sgot->output_offset + off);

	      if (h == NULL || h->dynindx == -1)
		indx = 0;
	      else
		indx = h->dynindx;

	      dr_type = (r_type == R_SH_TLS_GD_32 ? R_SH_TLS_DTPMOD32 :
			 R_SH_TLS_TPOFF32);
	      if (dr_type == R_SH_TLS_TPOFF32 && indx == 0)
		outrel.r_addend = relocation - dtpoff_base (info);
	      else
		outrel.r_addend = 0;
	      outrel.r_info = ELF32_R_INFO (indx, dr_type);
	      loc = srelgot->contents;
	      loc += srelgot->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);

	      if (r_type == R_SH_TLS_GD_32)
		{
		  if (indx == 0)
		    {
		      bfd_put_32 (output_bfd,
				  relocation - dtpoff_base (info),
				  sgot->contents + off + 4);
		    }
		  else
		    {
		      outrel.r_info = ELF32_R_INFO (indx,
						    R_SH_TLS_DTPOFF32);
		      outrel.r_offset += 4;
		      outrel.r_addend = 0;
		      srelgot->reloc_count++;
		      loc += sizeof (Elf32_External_Rela);
		      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
		    }
		}

	      if (h != NULL)
		h->got.offset |= 1;
	      else
		local_got_offsets[r_symndx] |= 1;
	    }

	  if (off >= (bfd_vma) -2)
	    abort ();

	  if (r_type == (int) ELF32_R_TYPE (rel->r_info))
	    relocation = sgot->output_offset + off;
	  else
	    {
	      bfd_vma offset;
	      unsigned short insn;

	      /* GD->IE transition:
		   mov.l 1f,r4; mova 2f,r0; mov.l 2f,r1; add r0,r1;
		   jsr @r1; add r12,r4; bra 3f; nop; .align 2;
		   1: .long x$TLSGD; 2: .long __tls_get_addr@PLT; 3:
		 We change it into:
		   mov.l 1f,r0; stc gbr,r4; mov.l @(r0,r12),r0; add r4,r0;
		   nop; nop; bra 3f; nop; .align 2;
		   1: .long x@TPOFF; 2:...; 3:.  */

	      offset = rel->r_offset;
	      BFD_ASSERT (offset >= 16);
	      /* Size of GD instructions is 16 or 18.  */
	      offset -= 16;
	      insn = bfd_get_16 (input_bfd, contents + offset + 0);
	      if ((insn & 0xff00) == 0xc700)
		{
		  BFD_ASSERT (offset >= 2);
		  offset -= 2;
		  insn = bfd_get_16 (input_bfd, contents + offset + 0);
		}

	      BFD_ASSERT ((insn & 0xff00) == 0xd400);

	      /* Replace mov.l 1f,R4 with mov.l 1f,r0.  */
	      bfd_put_16 (output_bfd, insn & 0xf0ff, contents + offset);

	      insn = bfd_get_16 (input_bfd, contents + offset + 2);
	      BFD_ASSERT ((insn & 0xff00) == 0xc700);
	      insn = bfd_get_16 (input_bfd, contents + offset + 4);
	      BFD_ASSERT ((insn & 0xff00) == 0xd100);
	      insn = bfd_get_16 (input_bfd, contents + offset + 6);
	      BFD_ASSERT (insn == 0x310c);
	      insn = bfd_get_16 (input_bfd, contents + offset + 8);
	      BFD_ASSERT (insn == 0x410b);
	      insn = bfd_get_16 (input_bfd, contents + offset + 10);
	      BFD_ASSERT (insn == 0x34cc);

	      bfd_put_16 (output_bfd, 0x0412, contents + offset + 2);
	      bfd_put_16 (output_bfd, 0x00ce, contents + offset + 4);
	      bfd_put_16 (output_bfd, 0x304c, contents + offset + 6);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 8);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 10);

	      bfd_put_32 (output_bfd, sgot->output_offset + off,
			  contents + rel->r_offset);

	      continue;
	  }

	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_TLS_LD_32:
	  if (! info->shared)
	    {
	      bfd_vma offset;
	      unsigned short insn;

	      /* LD->LE transition:
		   mov.l 1f,r4; mova 2f,r0; mov.l 2f,r1; add r0,r1;
		   jsr @r1; add r12,r4; bra 3f; nop; .align 2;
		   1: .long x$TLSLD; 2: .long __tls_get_addr@PLT; 3:
		 We change it into:
		   stc gbr,r0; nop; nop; nop;
		   nop; nop; bra 3f; ...; 3:.  */

	      offset = rel->r_offset;
	      BFD_ASSERT (offset >= 16);
	      /* Size of LD instructions is 16 or 18.  */
	      offset -= 16;
	      insn = bfd_get_16 (input_bfd, contents + offset + 0);
	      if ((insn & 0xff00) == 0xc700)
		{
		  BFD_ASSERT (offset >= 2);
		  offset -= 2;
		  insn = bfd_get_16 (input_bfd, contents + offset + 0);
		}

	      BFD_ASSERT ((insn & 0xff00) == 0xd400);
	      insn = bfd_get_16 (input_bfd, contents + offset + 2);
	      BFD_ASSERT ((insn & 0xff00) == 0xc700);
	      insn = bfd_get_16 (input_bfd, contents + offset + 4);
	      BFD_ASSERT ((insn & 0xff00) == 0xd100);
	      insn = bfd_get_16 (input_bfd, contents + offset + 6);
	      BFD_ASSERT (insn == 0x310c);
	      insn = bfd_get_16 (input_bfd, contents + offset + 8);
	      BFD_ASSERT (insn == 0x410b);
	      insn = bfd_get_16 (input_bfd, contents + offset + 10);
	      BFD_ASSERT (insn == 0x34cc);

	      bfd_put_16 (output_bfd, 0x0012, contents + offset + 0);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 2);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 4);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 6);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 8);
	      bfd_put_16 (output_bfd, 0x0009, contents + offset + 10);

	      continue;
	    }

	  sgot = htab->sgot;
	  if (sgot == NULL)
	    abort ();

	  off = htab->tls_ldm_got.offset;
	  if (off & 1)
	    off &= ~1;
	  else
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;

	      srelgot = htab->srelgot;
	      if (srelgot == NULL)
		abort ();

	      outrel.r_offset = (sgot->output_section->vma
				 + sgot->output_offset + off);
	      outrel.r_addend = 0;
	      outrel.r_info = ELF32_R_INFO (0, R_SH_TLS_DTPMOD32);
	      loc = srelgot->contents;
	      loc += srelgot->reloc_count++ * sizeof (Elf32_External_Rela);
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
	      htab->tls_ldm_got.offset |= 1;
	    }

	  relocation = sgot->output_offset + off;
	  addend = rel->r_addend;

	  goto final_link_relocate;

	case R_SH_TLS_LDO_32:
	  if (! info->shared)
	    relocation = tpoff (info, relocation);
	  else
	    relocation -= dtpoff_base (info);

	  addend = rel->r_addend;
	  goto final_link_relocate;

	case R_SH_TLS_LE_32:
	  {
	    int indx;
	    Elf_Internal_Rela outrel;
	    bfd_byte *loc;

	    if (! info->shared)
	      {
		relocation = tpoff (info, relocation);
		addend = rel->r_addend;
		goto final_link_relocate;
	      }

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

	    if (h == NULL || h->dynindx == -1)
	      indx = 0;
	    else
	      indx = h->dynindx;

	    outrel.r_offset = (input_section->output_section->vma
			       + input_section->output_offset
			       + rel->r_offset);
	    outrel.r_info = ELF32_R_INFO (indx, R_SH_TLS_TPOFF32);
	    if (indx == 0)
	      outrel.r_addend = relocation - dtpoff_base (info);
	    else
	      outrel.r_addend = 0;

	    loc = sreloc->contents;
	    loc += sreloc->reloc_count++ * sizeof (Elf32_External_Rela);
	    bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
	    continue;
	  }
	}

    relocation_done:
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
   which uses sh_elf_relocate_section.  */

static bfd_byte *
sh_elf_get_relocated_section_contents (bfd *output_bfd,
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
			 (input_bfd, input_section, NULL,
			  (Elf_Internal_Rela *) NULL, FALSE));
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
      sections = (asection **) bfd_malloc (amt);
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

      if (! sh_elf_relocate_section (output_bfd, link_info, input_bfd,
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

/* Return the base VMA address which should be subtracted from real addresses
   when resolving @dtpoff relocation.
   This is PT_TLS segment p_vaddr.  */

static bfd_vma
dtpoff_base (struct bfd_link_info *info)
{
  /* If tls_sec is NULL, we should have signalled an error already.  */
  if (elf_hash_table (info)->tls_sec == NULL)
    return 0;
  return elf_hash_table (info)->tls_sec->vma;
}

/* Return the relocation value for R_SH_TLS_TPOFF32..  */

static bfd_vma
tpoff (struct bfd_link_info *info, bfd_vma address)
{
  /* If tls_sec is NULL, we should have signalled an error already.  */
  if (elf_hash_table (info)->tls_sec == NULL)
    return 0;
  /* SH TLS ABI is variant I and static TLS block start just after tcbhead
     structure which has 2 pointer fields.  */
  return (address - elf_hash_table (info)->tls_sec->vma
	  + align_power ((bfd_vma) 8,
			 elf_hash_table (info)->tls_sec->alignment_power));
}

static asection *
sh_elf_gc_mark_hook (asection *sec,
		     struct bfd_link_info *info ATTRIBUTE_UNUSED,
		     Elf_Internal_Rela *rel, struct elf_link_hash_entry *h,
		     Elf_Internal_Sym *sym)
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_SH_GNU_VTINHERIT:
	case R_SH_GNU_VTENTRY:
	  break;

	default:
#ifdef INCLUDE_SHMEDIA
	  while (h->root.type == bfd_link_hash_indirect
		 && h->root.u.i.link)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
#endif
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
sh_elf_gc_sweep_hook (bfd *abfd, struct bfd_link_info *info,
		      asection *sec, const Elf_Internal_Rela *relocs)
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
      unsigned int r_type;
      struct elf_link_hash_entry *h = NULL;
#ifdef INCLUDE_SHMEDIA
      int seen_stt_datalabel = 0;
#endif

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx >= symtab_hdr->sh_info)
	{
	  struct elf_sh_link_hash_entry *eh;
	  struct elf_sh_dyn_relocs **pp;
	  struct elf_sh_dyn_relocs *p;

	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    {
#ifdef INCLUDE_SHMEDIA
	      seen_stt_datalabel |= h->type == STT_DATALABEL;
#endif
	      h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }
	  eh = (struct elf_sh_link_hash_entry *) h;
	  for (pp = &eh->dyn_relocs; (p = *pp) != NULL; pp = &p->next)
	    if (p->sec == sec)
	      {
		/* Everything must go for SEC.  */
		*pp = p->next;
		break;
	      }
	}

      r_type = ELF32_R_TYPE (rel->r_info);
      switch (sh_elf_optimized_tls_reloc (info, r_type, h != NULL))
	{
	case R_SH_TLS_LD_32:
	  if (sh_elf_hash_table (info)->tls_ldm_got.refcount > 0)
	    sh_elf_hash_table (info)->tls_ldm_got.refcount -= 1;
	  break;

	case R_SH_GOT32:
	case R_SH_GOTOFF:
	case R_SH_GOTPC:
#ifdef INCLUDE_SHMEDIA
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
#endif
	case R_SH_TLS_GD_32:
	case R_SH_TLS_IE_32:
	  if (h != NULL)
	    {
#ifdef INCLUDE_SHMEDIA
	      if (seen_stt_datalabel)
		{
		  struct elf_sh_link_hash_entry *eh;
		  eh = (struct elf_sh_link_hash_entry *) h;
		  if (eh->datalabel_got.refcount > 0)
		    eh->datalabel_got.refcount -= 1;
		}
	      else
#endif
		if (h->got.refcount > 0)
		  h->got.refcount -= 1;
	    }
	  else if (local_got_refcounts != NULL)
	    {
#ifdef INCLUDE_SHMEDIA
	      if (rel->r_addend & 1)
		{
		  if (local_got_refcounts[symtab_hdr->sh_info + r_symndx] > 0)
		    local_got_refcounts[symtab_hdr->sh_info + r_symndx] -= 1;
		}
	      else
#endif
		if (local_got_refcounts[r_symndx] > 0)
		  local_got_refcounts[r_symndx] -= 1;
	    }
	  break;

	case R_SH_DIR32:
	case R_SH_REL32:
	  if (info->shared)
	    break;
	  /* Fall thru */

	case R_SH_PLT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_PLT_LOW16:
	case R_SH_PLT_MEDLOW16:
	case R_SH_PLT_MEDHI16:
	case R_SH_PLT_HI16:
#endif
	  if (h != NULL)
	    {
	      if (h->plt.refcount > 0)
		h->plt.refcount -= 1;
	    }
	  break;

	case R_SH_GOTPLT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOTPLT_LOW16:
	case R_SH_GOTPLT_MEDLOW16:
	case R_SH_GOTPLT_MEDHI16:
	case R_SH_GOTPLT_HI16:
	case R_SH_GOTPLT10BY4:
	case R_SH_GOTPLT10BY8:
#endif
	  if (h != NULL)
	    {
	      struct elf_sh_link_hash_entry *eh;
	      eh = (struct elf_sh_link_hash_entry *) h;
	      if (eh->gotplt_refcount > 0)
		{
		  eh->gotplt_refcount -= 1;
		  if (h->plt.refcount > 0)
		    h->plt.refcount -= 1;
		}
#ifdef INCLUDE_SHMEDIA
	      else if (seen_stt_datalabel)
		{
		  if (eh->datalabel_got.refcount > 0)
		    eh->datalabel_got.refcount -= 1;
		}
#endif
	      else if (h->got.refcount > 0)
		h->got.refcount -= 1;
	    }
	  else if (local_got_refcounts != NULL)
	    {
#ifdef INCLUDE_SHMEDIA
	      if (rel->r_addend & 1)
		{
		  if (local_got_refcounts[symtab_hdr->sh_info + r_symndx] > 0)
		    local_got_refcounts[symtab_hdr->sh_info + r_symndx] -= 1;
		}
	      else
#endif
		if (local_got_refcounts[r_symndx] > 0)
		  local_got_refcounts[r_symndx] -= 1;
	    }
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

/* Copy the extra info we tack onto an elf_link_hash_entry.  */

static void
sh_elf_copy_indirect_symbol (struct bfd_link_info *info,
			     struct elf_link_hash_entry *dir,
			     struct elf_link_hash_entry *ind)
{
  struct elf_sh_link_hash_entry *edir, *eind;

  edir = (struct elf_sh_link_hash_entry *) dir;
  eind = (struct elf_sh_link_hash_entry *) ind;

  if (eind->dyn_relocs != NULL)
    {
      if (edir->dyn_relocs != NULL)
	{
	  struct elf_sh_dyn_relocs **pp;
	  struct elf_sh_dyn_relocs *p;

	  /* Add reloc counts against the indirect sym to the direct sym
	     list.  Merge any entries against the same section.  */
	  for (pp = &eind->dyn_relocs; (p = *pp) != NULL; )
	    {
	      struct elf_sh_dyn_relocs *q;

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
  edir->gotplt_refcount = eind->gotplt_refcount;
  eind->gotplt_refcount = 0;
#ifdef INCLUDE_SHMEDIA
  edir->datalabel_got.refcount += eind->datalabel_got.refcount;
  eind->datalabel_got.refcount = 0;
#endif

  if (ind->root.type == bfd_link_hash_indirect
      && dir->got.refcount <= 0)
    {
      edir->tls_type = eind->tls_type;
      eind->tls_type = GOT_UNKNOWN;
    }

  if (ind->root.type != bfd_link_hash_indirect
      && dir->dynamic_adjusted)
    {
      /* If called to transfer flags for a weakdef during processing
	 of elf_adjust_dynamic_symbol, don't copy non_got_ref.
	 We clear it ourselves for ELIMINATE_COPY_RELOCS.  */
      dir->ref_dynamic |= ind->ref_dynamic;
      dir->ref_regular |= ind->ref_regular;
      dir->ref_regular_nonweak |= ind->ref_regular_nonweak;
      dir->needs_plt |= ind->needs_plt;
    }
  else
    _bfd_elf_link_hash_copy_indirect (info, dir, ind);
}

static int
sh_elf_optimized_tls_reloc (struct bfd_link_info *info, int r_type,
			    int is_local)
{
  if (info->shared)
    return r_type;

  switch (r_type)
    {
    case R_SH_TLS_GD_32:
    case R_SH_TLS_IE_32:
      if (is_local)
	return R_SH_TLS_LE_32;
      return R_SH_TLS_IE_32;
    case R_SH_TLS_LD_32:
      return R_SH_TLS_LE_32;
    }

  return r_type;
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc.  */

static bfd_boolean
sh_elf_check_relocs (bfd *abfd, struct bfd_link_info *info, asection *sec,
		     const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  struct elf_sh_link_hash_table *htab;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  bfd_vma *local_got_offsets;
  asection *sgot;
  asection *srelgot;
  asection *sreloc;
  unsigned int r_type;
  int tls_type, old_tls_type;

  sgot = NULL;
  srelgot = NULL;
  sreloc = NULL;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + symtab_hdr->sh_size/sizeof (Elf32_External_Sym);
  if (!elf_bad_symtab (abfd))
    sym_hashes_end -= symtab_hdr->sh_info;

  htab = sh_elf_hash_table (info);
  local_got_offsets = elf_local_got_offsets (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;
#ifdef INCLUDE_SHMEDIA
      int seen_stt_datalabel = 0;
#endif

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);

      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    {
#ifdef INCLUDE_SHMEDIA
	      seen_stt_datalabel |= h->type == STT_DATALABEL;
#endif
	      h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }
	}

      r_type = sh_elf_optimized_tls_reloc (info, r_type, h == NULL);
      if (! info->shared
	  && r_type == R_SH_TLS_IE_32
	  && h != NULL
	  && h->root.type != bfd_link_hash_undefined
	  && h->root.type != bfd_link_hash_undefweak
	  && (h->dynindx == -1
	      || h->def_regular))
	r_type = R_SH_TLS_LE_32;

      /* Some relocs require a global offset table.  */
      if (htab->sgot == NULL)
	{
	  switch (r_type)
	    {
	    case R_SH_GOTPLT32:
	    case R_SH_GOT32:
	    case R_SH_GOTOFF:
	    case R_SH_GOTPC:
#ifdef INCLUDE_SHMEDIA
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
#endif
	    case R_SH_TLS_GD_32:
	    case R_SH_TLS_LD_32:
	    case R_SH_TLS_IE_32:
	      if (htab->sgot == NULL)
		{
		  if (htab->root.dynobj == NULL)
		    htab->root.dynobj = abfd;
		  if (!create_got_section (htab->root.dynobj, info))
		    return FALSE;
		}
	      break;

	    default:
	      break;
	    }
	}

      switch (r_type)
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

	case R_SH_TLS_IE_32:
	  if (info->shared)
	    info->flags |= DF_STATIC_TLS;

	  /* FALLTHROUGH */
	force_got:
	case R_SH_TLS_GD_32:
	case R_SH_GOT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOT_LOW16:
	case R_SH_GOT_MEDLOW16:
	case R_SH_GOT_MEDHI16:
	case R_SH_GOT_HI16:
	case R_SH_GOT10BY4:
	case R_SH_GOT10BY8:
#endif
	  switch (r_type)
	    {
	    default:
	      tls_type = GOT_NORMAL;
	      break;
	    case R_SH_TLS_GD_32:
	      tls_type = GOT_TLS_GD;
	      break;
	    case R_SH_TLS_IE_32:
	      tls_type = GOT_TLS_IE;
	      break;
	    }

	  if (h != NULL)
	    {
#ifdef INCLUDE_SHMEDIA
	      if (seen_stt_datalabel)
		{
		  struct elf_sh_link_hash_entry *eh
		    = (struct elf_sh_link_hash_entry *) h;

		  eh->datalabel_got.refcount += 1;
		}
	      else
#endif
		h->got.refcount += 1;
	      old_tls_type = sh_elf_hash_entry (h)->tls_type;
	    }
	  else
	    {
	      bfd_signed_vma *local_got_refcounts;

	      /* This is a global offset table entry for a local
		 symbol.  */
	      local_got_refcounts = elf_local_got_refcounts (abfd);
	      if (local_got_refcounts == NULL)
		{
		  bfd_size_type size;

		  size = symtab_hdr->sh_info;
		  size *= sizeof (bfd_signed_vma);
#ifdef INCLUDE_SHMEDIA
		  /* Reserve space for both the datalabel and
		     codelabel local GOT offsets.  */
		  size *= 2;
#endif
		  size += symtab_hdr->sh_info;
		  local_got_refcounts = ((bfd_signed_vma *)
					 bfd_zalloc (abfd, size));
		  if (local_got_refcounts == NULL)
		    return FALSE;
		  elf_local_got_refcounts (abfd) = local_got_refcounts;
#ifdef 	INCLUDE_SHMEDIA
		  /* Take care of both the datalabel and codelabel local
		     GOT offsets.  */
		  sh_elf_local_got_tls_type (abfd)
		    = (char *) (local_got_refcounts + 2 * symtab_hdr->sh_info);
#else
		  sh_elf_local_got_tls_type (abfd)
		    = (char *) (local_got_refcounts + symtab_hdr->sh_info);
#endif
		}
#ifdef INCLUDE_SHMEDIA
	      if (rel->r_addend & 1)
		local_got_refcounts[symtab_hdr->sh_info + r_symndx] += 1;
	      else
#endif
		local_got_refcounts[r_symndx] += 1;
	      old_tls_type = sh_elf_local_got_tls_type (abfd) [r_symndx];
	    }

	  /* If a TLS symbol is accessed using IE at least once,
	     there is no point to use dynamic model for it.  */
	  if (old_tls_type != tls_type && old_tls_type != GOT_UNKNOWN
	      && (old_tls_type != GOT_TLS_GD || tls_type != GOT_TLS_IE))
	    {
	      if (old_tls_type == GOT_TLS_IE && tls_type == GOT_TLS_GD)
		tls_type = GOT_TLS_IE;
	      else
		{
		  (*_bfd_error_handler)
		    (_("%B: `%s' accessed both as normal and thread local symbol"),
		     abfd, h->root.root.string);
		  return FALSE;
		}
	    }

	  if (old_tls_type != tls_type)
	    {
	      if (h != NULL)
		sh_elf_hash_entry (h)->tls_type = tls_type;
	      else
		sh_elf_local_got_tls_type (abfd) [r_symndx] = tls_type;
	    }

	  break;

	case R_SH_TLS_LD_32:
	  sh_elf_hash_table(info)->tls_ldm_got.refcount += 1;
	  break;

	case R_SH_GOTPLT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_GOTPLT_LOW16:
	case R_SH_GOTPLT_MEDLOW16:
	case R_SH_GOTPLT_MEDHI16:
	case R_SH_GOTPLT_HI16:
	case R_SH_GOTPLT10BY4:
	case R_SH_GOTPLT10BY8:
#endif
	  /* If this is a local symbol, we resolve it directly without
	     creating a procedure linkage table entry.  */

	  if (h == NULL
	      || h->forced_local
	      || ! info->shared
	      || info->symbolic
	      || h->dynindx == -1)
	    goto force_got;

	  h->needs_plt = 1;
	  h->plt.refcount += 1;
	  ((struct elf_sh_link_hash_entry *) h)->gotplt_refcount += 1;

	  break;

	case R_SH_PLT32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_PLT_LOW16:
	case R_SH_PLT_MEDLOW16:
	case R_SH_PLT_MEDHI16:
	case R_SH_PLT_HI16:
#endif
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

	  if (h->forced_local)
	    break;

	  h->needs_plt = 1;
	  h->plt.refcount += 1;
	  break;

	case R_SH_DIR32:
	case R_SH_REL32:
#ifdef INCLUDE_SHMEDIA
	case R_SH_IMM_LOW16_PCREL:
	case R_SH_IMM_MEDLOW16_PCREL:
	case R_SH_IMM_MEDHI16_PCREL:
	case R_SH_IMM_HI16_PCREL:
#endif
	  if (h != NULL && ! info->shared)
	    {
	      h->non_got_ref = 1;
	      h->plt.refcount += 1;
	    }

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
	     dyn_relocs field of the hash table entry. A similar
	     situation occurs when creating shared libraries and symbol
	     visibility changes render the symbol local.

	     If on the other hand, we are creating an executable, we
	     may need to keep relocations for symbols satisfied by a
	     dynamic library if we manage to avoid copy relocs for the
	     symbol.  */
	  if ((info->shared
	       && (sec->flags & SEC_ALLOC) != 0
	       && (r_type != R_SH_REL32
		   || (h != NULL
		       && (! info->symbolic
			   || h->root.type == bfd_link_hash_defweak
			   || !h->def_regular))))
	      || (! info->shared
		  && (sec->flags & SEC_ALLOC) != 0
		  && h != NULL
		  && (h->root.type == bfd_link_hash_defweak
		      || !h->def_regular)))
	    {
	      struct elf_sh_dyn_relocs *p;
	      struct elf_sh_dyn_relocs **head;

	      if (htab->root.dynobj == NULL)
		htab->root.dynobj = abfd;

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

		  sreloc = bfd_get_section_by_name (htab->root.dynobj, name);
		  if (sreloc == NULL)
		    {
		      flagword flags;

		      flags = (SEC_HAS_CONTENTS | SEC_READONLY
			       | SEC_IN_MEMORY | SEC_LINKER_CREATED);
		      if ((sec->flags & SEC_ALLOC) != 0)
			flags |= SEC_ALLOC | SEC_LOAD;
		      sreloc = bfd_make_section_with_flags (htab->root.dynobj,
							    name,
							    flags);
		      if (sreloc == NULL
			  || ! bfd_set_section_alignment (htab->root.dynobj,
							  sreloc, 2))
			return FALSE;
		    }
		  elf_section_data (sec)->sreloc = sreloc;
		}

	      /* If this is a global symbol, we count the number of
		 relocations we need for this symbol.  */
	      if (h != NULL)
		head = &((struct elf_sh_link_hash_entry *) h)->dyn_relocs;
	      else
		{
		  asection *s;
		  void *vpp;

		  /* Track dynamic relocs needed for local syms too.  */
		  s = bfd_section_from_r_symndx (abfd, &htab->sym_sec,
						 sec, r_symndx);
		  if (s == NULL)
		    return FALSE;

		  vpp = &elf_section_data (s)->local_dynrel;
		  head = (struct elf_sh_dyn_relocs **) vpp;
		}

	      p = *head;
	      if (p == NULL || p->sec != sec)
		{
		  bfd_size_type amt = sizeof (*p);
		  p = bfd_alloc (htab->root.dynobj, amt);
		  if (p == NULL)
		    return FALSE;
		  p->next = *head;
		  *head = p;
		  p->sec = sec;
		  p->count = 0;
		  p->pc_count = 0;
		}

	      p->count += 1;
	      if (r_type == R_SH_REL32
#ifdef INCLUDE_SHMEDIA
		  || r_type == R_SH_IMM_LOW16_PCREL
		  || r_type == R_SH_IMM_MEDLOW16_PCREL
		  || r_type == R_SH_IMM_MEDHI16_PCREL
		  || r_type == R_SH_IMM_HI16_PCREL
#endif
		  )
		p->pc_count += 1;
	    }

	  break;

	case R_SH_TLS_LE_32:
	  if (info->shared)
	    {
	      (*_bfd_error_handler)
		(_("%B: TLS local exec code cannot be linked into shared objects"),
		 abfd);
	      return FALSE;
	    }

	  break;

	case R_SH_TLS_LDO_32:
	  /* Nothing to do.  */
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}

#ifndef sh_elf_set_mach_from_flags
static unsigned int sh_ef_bfd_table[] = { EF_SH_BFD_TABLE };

static bfd_boolean
sh_elf_set_mach_from_flags (bfd *abfd)
{
  flagword flags = elf_elfheader (abfd)->e_flags & EF_SH_MACH_MASK;

  if (flags >= sizeof(sh_ef_bfd_table))
    return FALSE;

  if (sh_ef_bfd_table[flags] == 0)
    return FALSE;
  
  bfd_default_set_arch_mach (abfd, bfd_arch_sh, sh_ef_bfd_table[flags]);

  return TRUE;
}


/* Reverse table lookup for sh_ef_bfd_table[].
   Given a bfd MACH value from archures.c
   return the equivalent ELF flags from the table.
   Return -1 if no match is found.  */

int
sh_elf_get_flags_from_mach (unsigned long mach)
{
  int i = ARRAY_SIZE (sh_ef_bfd_table) - 1;
  
  for (; i>0; i--)
    if (sh_ef_bfd_table[i] == mach)
      return i;
  
  /* shouldn't get here */
  BFD_FAIL();

  return -1;
}
#endif /* not sh_elf_set_mach_from_flags */

#ifndef sh_elf_set_private_flags
/* Function to keep SH specific file flags.  */

static bfd_boolean
sh_elf_set_private_flags (bfd *abfd, flagword flags)
{
  BFD_ASSERT (! elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return sh_elf_set_mach_from_flags (abfd);
}
#endif /* not sh_elf_set_private_flags */

#ifndef sh_elf_copy_private_data
/* Copy backend specific data from one object module to another */

static bfd_boolean
sh_elf_copy_private_data (bfd * ibfd, bfd * obfd)
{
  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  return sh_elf_set_private_flags (obfd, elf_elfheader (ibfd)->e_flags);
}
#endif /* not sh_elf_copy_private_data */

#ifndef sh_elf_merge_private_data

/* This function returns the ELF architecture number that
   corresponds to the given arch_sh* flags.  */

int
sh_find_elf_flags (unsigned int arch_set)
{
  extern unsigned long sh_get_bfd_mach_from_arch_set (unsigned int);
  unsigned long bfd_mach = sh_get_bfd_mach_from_arch_set (arch_set);

  return sh_elf_get_flags_from_mach (bfd_mach);
}

/* This routine initialises the elf flags when required and
   calls sh_merge_bfd_arch() to check dsp/fpu compatibility.  */

static bfd_boolean
sh_elf_merge_private_data (bfd *ibfd, bfd *obfd)
{
  extern bfd_boolean sh_merge_bfd_arch (bfd *, bfd *);

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  if (! elf_flags_init (obfd))
    {
      /* This happens when ld starts out with a 'blank' output file.  */
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = EF_SH1;
      sh_elf_set_mach_from_flags (obfd);
    }

  if (! sh_merge_bfd_arch (ibfd, obfd))
    {
      _bfd_error_handler ("%B: uses instructions which are incompatible "
			  "with instructions used in previous modules",
			  ibfd);
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  elf_elfheader (obfd)->e_flags =
    sh_elf_get_flags_from_mach (bfd_get_mach (obfd));
  
  return TRUE;
}
#endif /* not sh_elf_merge_private_data */

/* Override the generic function because we need to store sh_elf_obj_tdata
   as the specific tdata.  We set also the machine architecture from flags
   here.  */

static bfd_boolean
sh_elf_object_p (bfd *abfd)
{
  return sh_elf_set_mach_from_flags (abfd);
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

static bfd_boolean
sh_elf_finish_dynamic_symbol (bfd *output_bfd, struct bfd_link_info *info,
			      struct elf_link_hash_entry *h,
			      Elf_Internal_Sym *sym)
{
  struct elf_sh_link_hash_table *htab;

  htab = sh_elf_hash_table (info);

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

      splt = htab->splt;
      sgot = htab->sgotplt;
      srel = htab->srelplt;
      BFD_ASSERT (splt != NULL && sgot != NULL && srel != NULL);

      /* Get the index in the procedure linkage table which
	 corresponds to this symbol.  This is the index of this symbol
	 in all the symbols for which we are making plt entries.  The
	 first entry in the procedure linkage table is reserved.  */
      plt_index = h->plt.offset / elf_sh_sizeof_plt (info) - 1;

      /* Get the offset into the .got table of the entry that
	 corresponds to this function.  Each .got entry is 4 bytes.
	 The first three are reserved.  */
      got_offset = (plt_index + 3) * 4;

#ifdef GOT_BIAS
      if (info->shared)
	got_offset -= GOT_BIAS;
#endif

      /* Fill in the entry in the procedure linkage table.  */
      if (! info->shared)
	{
	  if (elf_sh_plt_entry == NULL)
	    {
	      elf_sh_plt_entry = (bfd_big_endian (output_bfd) ?
				  elf_sh_plt_entry_be : elf_sh_plt_entry_le);
	    }
	  memcpy (splt->contents + h->plt.offset, elf_sh_plt_entry,
		  elf_sh_sizeof_plt (info));
#ifdef INCLUDE_SHMEDIA
	  movi_shori_putval (output_bfd,
			     (sgot->output_section->vma
			      + sgot->output_offset
			      + got_offset),
			     (splt->contents + h->plt.offset
			      + elf_sh_plt_symbol_offset (info)));

	  /* Set bottom bit because its for a branch to SHmedia */
	  movi_shori_putval (output_bfd,
			     (splt->output_section->vma + splt->output_offset)
			     | 1,
			     (splt->contents + h->plt.offset
			      + elf_sh_plt_plt0_offset (info)));
#else
	  bfd_put_32 (output_bfd,
		      (sgot->output_section->vma
		       + sgot->output_offset
		       + got_offset),
		      (splt->contents + h->plt.offset
		       + elf_sh_plt_symbol_offset (info)));

	  bfd_put_32 (output_bfd,
		      (splt->output_section->vma + splt->output_offset),
		      (splt->contents + h->plt.offset
		       + elf_sh_plt_plt0_offset (info)));
#endif
	}
      else
	{
	  if (elf_sh_pic_plt_entry == NULL)
	    {
	      elf_sh_pic_plt_entry = (bfd_big_endian (output_bfd) ?
				      elf_sh_pic_plt_entry_be :
				      elf_sh_pic_plt_entry_le);
	    }
	  memcpy (splt->contents + h->plt.offset, elf_sh_pic_plt_entry,
		  elf_sh_sizeof_plt (info));
#ifdef INCLUDE_SHMEDIA
	  movi_shori_putval (output_bfd, got_offset,
			     (splt->contents + h->plt.offset
			      + elf_sh_plt_symbol_offset (info)));
#else
	  bfd_put_32 (output_bfd, got_offset,
		      (splt->contents + h->plt.offset
		       + elf_sh_plt_symbol_offset (info)));
#endif
	}

#ifdef GOT_BIAS
      if (info->shared)
	got_offset += GOT_BIAS;
#endif

#ifdef INCLUDE_SHMEDIA
      movi_shori_putval (output_bfd,
			 plt_index * sizeof (Elf32_External_Rela),
			 (splt->contents + h->plt.offset
			  + elf_sh_plt_reloc_offset (info)));
#else
      bfd_put_32 (output_bfd, plt_index * sizeof (Elf32_External_Rela),
		  (splt->contents + h->plt.offset
		   + elf_sh_plt_reloc_offset (info)));
#endif

      /* Fill in the entry in the global offset table.  */
      bfd_put_32 (output_bfd,
		  (splt->output_section->vma
		   + splt->output_offset
		   + h->plt.offset
		   + elf_sh_plt_temp_offset (info)),
		  sgot->contents + got_offset);

      /* Fill in the entry in the .rela.plt section.  */
      rel.r_offset = (sgot->output_section->vma
		      + sgot->output_offset
		      + got_offset);
      rel.r_info = ELF32_R_INFO (h->dynindx, R_SH_JMP_SLOT);
      rel.r_addend = 0;
#ifdef GOT_BIAS
      rel.r_addend = GOT_BIAS;
#endif
      loc = srel->contents + plt_index * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rel, loc);

      if (!h->def_regular)
	{
	  /* Mark the symbol as undefined, rather than as defined in
	     the .plt section.  Leave the value alone.  */
	  sym->st_shndx = SHN_UNDEF;
	}
    }

  if (h->got.offset != (bfd_vma) -1
      && sh_elf_hash_entry (h)->tls_type != GOT_TLS_GD
      && sh_elf_hash_entry (h)->tls_type != GOT_TLS_IE)
    {
      asection *sgot;
      asection *srel;
      Elf_Internal_Rela rel;
      bfd_byte *loc;

      /* This symbol has an entry in the global offset table.  Set it
	 up.  */

      sgot = htab->sgot;
      srel = htab->srelgot;
      BFD_ASSERT (sgot != NULL && srel != NULL);

      rel.r_offset = (sgot->output_section->vma
		      + sgot->output_offset
		      + (h->got.offset &~ (bfd_vma) 1));

      /* If this is a static link, or it is a -Bsymbolic link and the
	 symbol is defined locally or was forced to be local because
	 of a version file, we just want to emit a RELATIVE reloc.
	 The entry in the global offset table will already have been
	 initialized in the relocate_section function.  */
      if (info->shared
	  && SYMBOL_REFERENCES_LOCAL (info, h))
	{
	  rel.r_info = ELF32_R_INFO (0, R_SH_RELATIVE);
	  rel.r_addend = (h->root.u.def.value
			  + h->root.u.def.section->output_section->vma
			  + h->root.u.def.section->output_offset);
	}
      else
	{
	  bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + h->got.offset);
	  rel.r_info = ELF32_R_INFO (h->dynindx, R_SH_GLOB_DAT);
	  rel.r_addend = 0;
	}

      loc = srel->contents;
      loc += srel->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rel, loc);
    }

#ifdef INCLUDE_SHMEDIA
  {
    struct elf_sh_link_hash_entry *eh;

    eh = (struct elf_sh_link_hash_entry *) h;
    if (eh->datalabel_got.offset != (bfd_vma) -1)
      {
	asection *sgot;
	asection *srel;
	Elf_Internal_Rela rel;
	bfd_byte *loc;

	/* This symbol has a datalabel entry in the global offset table.
	   Set it up.  */

	sgot = htab->sgot;
	srel = htab->srelgot;
	BFD_ASSERT (sgot != NULL && srel != NULL);

	rel.r_offset = (sgot->output_section->vma
			+ sgot->output_offset
			+ (eh->datalabel_got.offset &~ (bfd_vma) 1));

	/* If this is a static link, or it is a -Bsymbolic link and the
	   symbol is defined locally or was forced to be local because
	   of a version file, we just want to emit a RELATIVE reloc.
	   The entry in the global offset table will already have been
	   initialized in the relocate_section function.  */
	if (info->shared
	    && SYMBOL_REFERENCES_LOCAL (info, h))
	  {
	    rel.r_info = ELF32_R_INFO (0, R_SH_RELATIVE);
	    rel.r_addend = (h->root.u.def.value
			    + h->root.u.def.section->output_section->vma
			    + h->root.u.def.section->output_offset);
	  }
	else
	  {
	    bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents
			+ eh->datalabel_got.offset);
	    rel.r_info = ELF32_R_INFO (h->dynindx, R_SH_GLOB_DAT);
	    rel.r_addend = 0;
	  }

	loc = srel->contents;
	loc += srel->reloc_count++ * sizeof (Elf32_External_Rela);
	bfd_elf32_swap_reloca_out (output_bfd, &rel, loc);
      }
  }
#endif

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
      rel.r_info = ELF32_R_INFO (h->dynindx, R_SH_COPY);
      rel.r_addend = 0;
      loc = s->contents + s->reloc_count++ * sizeof (Elf32_External_Rela);
      bfd_elf32_swap_reloca_out (output_bfd, &rel, loc);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || h == htab->root.hgot)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}

/* Finish up the dynamic sections.  */

static bfd_boolean
sh_elf_finish_dynamic_sections (bfd *output_bfd, struct bfd_link_info *info)
{
  struct elf_sh_link_hash_table *htab;
  asection *sgot;
  asection *sdyn;

  htab = sh_elf_hash_table (info);
  sgot = htab->sgotplt;
  sdyn = bfd_get_section_by_name (htab->root.dynobj, ".dynamic");

  if (htab->root.dynamic_sections_created)
    {
      asection *splt;
      Elf32_External_Dyn *dyncon, *dynconend;

      BFD_ASSERT (sgot != NULL && sdyn != NULL);

      dyncon = (Elf32_External_Dyn *) sdyn->contents;
      dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  asection *s;
#ifdef INCLUDE_SHMEDIA
	  const char *name;
#endif

	  bfd_elf32_swap_dyn_in (htab->root.dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;

#ifdef INCLUDE_SHMEDIA
	    case DT_INIT:
	      name = info->init_function;
	      goto get_sym;

	    case DT_FINI:
	      name = info->fini_function;
	    get_sym:
	      if (dyn.d_un.d_val != 0)
		{
		  struct elf_link_hash_entry *h;

		  h = elf_link_hash_lookup (&htab->root, name,
					    FALSE, FALSE, TRUE);
		  if (h != NULL && (h->other & STO_SH5_ISA32))
		    {
		      dyn.d_un.d_val |= 1;
		      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
		    }
		}
	      break;
#endif

	    case DT_PLTGOT:
	      s = htab->sgot->output_section;
	      goto get_vma;

	    case DT_JMPREL:
	      s = htab->srelplt->output_section;
	    get_vma:
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;

	    case DT_PLTRELSZ:
	      s = htab->srelplt->output_section;
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_val = s->size;
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
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
	      if (htab->srelplt != NULL)
		{
		  s = htab->srelplt->output_section;
		  dyn.d_un.d_val -= s->size;
		}
	      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
	      break;
	    }
	}

      /* Fill in the first entry in the procedure linkage table.  */
      splt = htab->splt;
      if (splt && splt->size > 0)
	{
	  if (info->shared)
	    {
	      if (elf_sh_pic_plt_entry == NULL)
		{
		  elf_sh_pic_plt_entry = (bfd_big_endian (output_bfd) ?
					  elf_sh_pic_plt_entry_be :
					  elf_sh_pic_plt_entry_le);
		}
	      memcpy (splt->contents, elf_sh_pic_plt_entry,
		      elf_sh_sizeof_plt (info));
	    }
	  else
	    {
	      if (elf_sh_plt0_entry == NULL)
		{
		  elf_sh_plt0_entry = (bfd_big_endian (output_bfd) ?
				       elf_sh_plt0_entry_be :
				       elf_sh_plt0_entry_le);
		}
	      memcpy (splt->contents, elf_sh_plt0_entry, PLT_ENTRY_SIZE);
#ifdef INCLUDE_SHMEDIA
	      movi_shori_putval (output_bfd,
				 sgot->output_section->vma
				 + sgot->output_offset,
				 splt->contents
				 + elf_sh_plt0_gotplt_offset (info));
#else
	      bfd_put_32 (output_bfd,
			  sgot->output_section->vma + sgot->output_offset + 4,
			  splt->contents + elf_sh_plt0_gotid_offset (info));
	      bfd_put_32 (output_bfd,
			  sgot->output_section->vma + sgot->output_offset + 8,
			  splt->contents + elf_sh_plt0_linker_offset (info));
#endif
	    }

	  /* UnixWare sets the entsize of .plt to 4, although that doesn't
	     really seem like the right value.  */
	  elf_section_data (splt->output_section)->this_hdr.sh_entsize = 4;
	}
    }

  /* Fill in the first three entries in the global offset table.  */
  if (sgot && sgot->size > 0)
    {
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 4);
      bfd_put_32 (output_bfd, (bfd_vma) 0, sgot->contents + 8);

      elf_section_data (sgot->output_section)->this_hdr.sh_entsize = 4;
    }

  return TRUE;
}

static enum elf_reloc_type_class
sh_elf_reloc_type_class (const Elf_Internal_Rela *rela)
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_SH_RELATIVE:
      return reloc_class_relative;
    case R_SH_JMP_SLOT:
      return reloc_class_plt;
    case R_SH_COPY:
      return reloc_class_copy;
    default:
      return reloc_class_normal;
    }
}

#if !defined SH_TARGET_ALREADY_DEFINED
/* Support for Linux core dump NOTE sections.  */

static bfd_boolean
elf32_shlin_grok_prstatus (bfd *abfd, Elf_Internal_Note *note)
{
  int offset;
  unsigned int size;

  switch (note->descsz)
    {
      default:
	return FALSE;

      case 168:		/* Linux/SH */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

	/* pr_reg */
	offset = 72;
	size = 92;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  size, note->descpos + offset);
}

static bfd_boolean
elf32_shlin_grok_psinfo (bfd *abfd, Elf_Internal_Note *note)
{
  switch (note->descsz)
    {
      default:
	return FALSE;

      case 124:		/* Linux/SH elf_prpsinfo */
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
#endif /* not SH_TARGET_ALREADY_DEFINED */

 
/* Return address for Ith PLT stub in section PLT, for relocation REL
   or (bfd_vma) -1 if it should not be included.  */

static bfd_vma
sh_elf_plt_sym_val (bfd_vma i, const asection *plt,
		    const arelent *rel ATTRIBUTE_UNUSED)
{
  return plt->vma + (i + 1) * PLT_ENTRY_SIZE;
}

#if !defined SH_TARGET_ALREADY_DEFINED
#define TARGET_BIG_SYM		bfd_elf32_sh_vec
#define TARGET_BIG_NAME		"elf32-sh"
#define TARGET_LITTLE_SYM	bfd_elf32_shl_vec
#define TARGET_LITTLE_NAME	"elf32-shl"
#endif

#define ELF_ARCH		bfd_arch_sh
#define ELF_MACHINE_CODE	EM_SH
#ifdef __QNXTARGET__
#define ELF_MAXPAGESIZE		0x1000
#else
#define ELF_MAXPAGESIZE		0x80
#endif

#define elf_symbol_leading_char '_'

#define bfd_elf32_bfd_reloc_type_lookup	sh_elf_reloc_type_lookup
#define elf_info_to_howto		sh_elf_info_to_howto
#define bfd_elf32_bfd_relax_section	sh_elf_relax_section
#define elf_backend_relocate_section	sh_elf_relocate_section
#define bfd_elf32_bfd_get_relocated_section_contents \
					sh_elf_get_relocated_section_contents
#define bfd_elf32_mkobject		sh_elf_mkobject
#define elf_backend_object_p		sh_elf_object_p
#define bfd_elf32_bfd_set_private_bfd_flags \
					sh_elf_set_private_flags
#define bfd_elf32_bfd_copy_private_bfd_data \
					sh_elf_copy_private_data
#define bfd_elf32_bfd_merge_private_bfd_data \
					sh_elf_merge_private_data

#define elf_backend_gc_mark_hook	sh_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	sh_elf_gc_sweep_hook
#define elf_backend_check_relocs	sh_elf_check_relocs
#define elf_backend_copy_indirect_symbol \
					sh_elf_copy_indirect_symbol
#define elf_backend_create_dynamic_sections \
					sh_elf_create_dynamic_sections
#define bfd_elf32_bfd_link_hash_table_create \
					sh_elf_link_hash_table_create
#define elf_backend_adjust_dynamic_symbol \
					sh_elf_adjust_dynamic_symbol
#define elf_backend_size_dynamic_sections \
					sh_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_symbol \
					sh_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					sh_elf_finish_dynamic_sections
#define elf_backend_reloc_type_class	sh_elf_reloc_type_class
#define elf_backend_plt_sym_val		sh_elf_plt_sym_val

#define elf_backend_can_gc_sections	1
#define elf_backend_can_refcount	1
#define elf_backend_want_got_plt	1
#define elf_backend_plt_readonly	1
#define elf_backend_want_plt_sym	0
#define elf_backend_got_header_size	12

#if !defined INCLUDE_SHMEDIA && !defined SH_TARGET_ALREADY_DEFINED

#include "elf32-target.h"

/* NetBSD support.  */
#undef	TARGET_BIG_SYM
#define	TARGET_BIG_SYM			bfd_elf32_shnbsd_vec
#undef	TARGET_BIG_NAME
#define	TARGET_BIG_NAME			"elf32-sh-nbsd"
#undef	TARGET_LITTLE_SYM
#define	TARGET_LITTLE_SYM		bfd_elf32_shlnbsd_vec
#undef	TARGET_LITTLE_NAME
#define	TARGET_LITTLE_NAME		"elf32-shl-nbsd"
#undef	ELF_MAXPAGESIZE
#define	ELF_MAXPAGESIZE			0x10000
#undef	elf_symbol_leading_char
#define	elf_symbol_leading_char		0
#undef	elf32_bed
#define	elf32_bed			elf32_sh_nbsd_bed

#include "elf32-target.h"


/* Linux support.  */
#undef	TARGET_BIG_SYM
#define	TARGET_BIG_SYM			bfd_elf32_shblin_vec
#undef	TARGET_BIG_NAME
#define	TARGET_BIG_NAME			"elf32-shbig-linux"
#undef	TARGET_LITTLE_SYM
#define	TARGET_LITTLE_SYM		bfd_elf32_shlin_vec
#undef	TARGET_LITTLE_NAME
#define	TARGET_LITTLE_NAME		"elf32-sh-linux"

#undef	elf_backend_grok_prstatus
#define	elf_backend_grok_prstatus	elf32_shlin_grok_prstatus
#undef	elf_backend_grok_psinfo
#define	elf_backend_grok_psinfo		elf32_shlin_grok_psinfo
#undef	elf32_bed
#define	elf32_bed			elf32_sh_lin_bed

#include "elf32-target.h"

#endif /* neither INCLUDE_SHMEDIA nor SH_TARGET_ALREADY_DEFINED */
