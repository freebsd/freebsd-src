/* Generic support for 64-bit ELF
   Copyright 1993, 1995, 1998 Free Software Foundation, Inc.

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

/* This does not include any relocation information, but should be
   good enough for GDB or objdump to read the file.  */

static reloc_howto_type dummy =
  HOWTO (0,			/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "UNKNOWN",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false);		/* pcrel_offset */

static void
elf_generic_info_to_howto (abfd, bfd_reloc, elf_reloc)
     bfd *abfd;
     arelent *bfd_reloc;
     Elf64_Internal_Rela *elf_reloc;
{
  bfd_reloc->howto = &dummy;
}

static void
elf_generic_info_to_howto_rel (abfd, bfd_reloc, elf_reloc)
     bfd *abfd;
     arelent *bfd_reloc;
     Elf64_Internal_Rel *elf_reloc;
{
  bfd_reloc->howto = &dummy;
}

#define TARGET_LITTLE_SYM		bfd_elf64_little_generic_vec
#define TARGET_LITTLE_NAME		"elf64-little"
#define TARGET_BIG_SYM			bfd_elf64_big_generic_vec
#define TARGET_BIG_NAME			"elf64-big"
#define ELF_ARCH			bfd_arch_unknown
#define ELF_MACHINE_CODE		EM_NONE
#define ELF_MAXPAGESIZE			0x1
#define bfd_elf64_bfd_reloc_type_lookup bfd_default_reloc_type_lookup
#define elf_info_to_howto		elf_generic_info_to_howto
#define elf_info_to_howto_rel		elf_generic_info_to_howto_rel

#include "elf64-target.h"
