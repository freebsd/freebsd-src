/* ARC ELF support for BFD.
   Copyright (C) 1995, 1997 Free Software Foundation, Inc.
   Contributed by Doug Evans, (dje@cygnus.com)

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

/* This file holds definitions specific to the ARC ELF ABI.  */

#ifndef _ELF_ARC_H
#define _ELF_ARC_H

#include "elf/reloc-macros.h"

/* Relocations.  */
START_RELOC_NUMBERS (elf_arc_reloc_type)
  RELOC_NUMBER (R_ARC_NONE, 0)
  RELOC_NUMBER (R_ARC_32, 1)
  RELOC_NUMBER (R_ARC_B26, 2)
  RELOC_NUMBER (R_ARC_B22_PCREL, 3)
  EMPTY_RELOC  (R_ARC_max)
END_RELOC_NUMBERS

/* Processor specific flags for the ELF header e_flags field.  */

/* Four bit ARC machine type field.  */
#define EF_ARC_MACH		0x0000000f

/* Various CPU types.  */
#define E_ARC_MACH_BASE		0x00000000
#define E_ARC_MACH_UNUSED1	0x00000001
#define E_ARC_MACH_UNUSED2	0x00000002
#define E_ARC_MACH_UNUSED4	0x00000003

/* Leave bits 0xf0 alone in case we ever have more than 16 cpu types.
   Highly unlikely, but what the heck.  */

/* File contains position independent code.  */
#define EF_ARC_PIC		0x00000100

#endif /* _ELF_ARC_H */
