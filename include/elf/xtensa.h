/* Xtensa ELF support for BFD.
   Copyright 2003 Free Software Foundation, Inc.
   Contributed by Bob Wilson (bwilson@tensilica.com) at Tensilica.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

/* This file holds definitions specific to the Xtensa ELF ABI.  */

#ifndef _ELF_XTENSA_H
#define _ELF_XTENSA_H

#include "elf/reloc-macros.h"

/* Relocations.  */
START_RELOC_NUMBERS (elf_xtensa_reloc_type)
     RELOC_NUMBER (R_XTENSA_NONE, 0)
     RELOC_NUMBER (R_XTENSA_32, 1)
     RELOC_NUMBER (R_XTENSA_RTLD, 2)
     RELOC_NUMBER (R_XTENSA_GLOB_DAT, 3)
     RELOC_NUMBER (R_XTENSA_JMP_SLOT, 4)
     RELOC_NUMBER (R_XTENSA_RELATIVE, 5)
     RELOC_NUMBER (R_XTENSA_PLT, 6)
     RELOC_NUMBER (R_XTENSA_OP0, 8)
     RELOC_NUMBER (R_XTENSA_OP1, 9)
     RELOC_NUMBER (R_XTENSA_OP2, 10) 
     RELOC_NUMBER (R_XTENSA_ASM_EXPAND, 11)
     RELOC_NUMBER (R_XTENSA_ASM_SIMPLIFY, 12)
     RELOC_NUMBER (R_XTENSA_GNU_VTINHERIT, 15)
     RELOC_NUMBER (R_XTENSA_GNU_VTENTRY, 16)
END_RELOC_NUMBERS (R_XTENSA_max)

/* Processor-specific flags for the ELF header e_flags field.  */

/* Four-bit Xtensa machine type field.  */
#define EF_XTENSA_MACH			0x0000000f

/* Various CPU types.  */
#define E_XTENSA_MACH			0x00000000

/* Leave bits 0xf0 alone in case we ever have more than 16 cpu types.
   Highly unlikely, but what the heck.  */

#define EF_XTENSA_XT_INSN		0x00000100
#define EF_XTENSA_XT_LIT		0x00000200


/* Processor-specific dynamic array tags.  */

/* Offset of the table that records the GOT location(s).  */
#define DT_XTENSA_GOT_LOC_OFF		0x70000000

/* Number of entries in the GOT location table.  */
#define DT_XTENSA_GOT_LOC_SZ		0x70000001


/* Definitions for instruction and literal property tables.  The
   tables for ".gnu.linkonce.*" sections are placed in the following
   sections:

   instruction tables:	.gnu.linkonce.x.*
   literal tables:	.gnu.linkonce.p.*
*/

#define XTENSA_INSN_SEC_NAME ".xt.insn"
#define XTENSA_LIT_SEC_NAME  ".xt.lit"

typedef struct property_table_entry_t
{
  bfd_vma address;
  bfd_vma size;
} property_table_entry;

#endif /* _ELF_XTENSA_H */
