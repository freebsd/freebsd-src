/* M32R ELF support for BFD.
   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.

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
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_M32R_H
#define _ELF_M32R_H

#include "elf/reloc-macros.h"

/* Relocations.  */
START_RELOC_NUMBERS (elf_m32r_reloc_type)
  RELOC_NUMBER (R_M32R_NONE, 0)
  RELOC_NUMBER (R_M32R_16, 1)
  RELOC_NUMBER (R_M32R_32, 2)
  RELOC_NUMBER (R_M32R_24, 3)
  RELOC_NUMBER (R_M32R_10_PCREL, 4)
  RELOC_NUMBER (R_M32R_18_PCREL, 5)
  RELOC_NUMBER (R_M32R_26_PCREL, 6)
  RELOC_NUMBER (R_M32R_HI16_ULO, 7)
  RELOC_NUMBER (R_M32R_HI16_SLO, 8)
  RELOC_NUMBER (R_M32R_LO16, 9)
  RELOC_NUMBER (R_M32R_SDA16, 10)
  RELOC_NUMBER (R_M32R_GNU_VTINHERIT, 11)
  RELOC_NUMBER (R_M32R_GNU_VTENTRY, 12)
  EMPTY_RELOC  (R_M32R_max)
END_RELOC_NUMBERS

/* Processor specific section indices.  These sections do not actually
   exist.  Symbols with a st_shndx field corresponding to one of these
   values have a special meaning.  */

/* Small common symbol.  */
#define SHN_M32R_SCOMMON	0xff00

/* Processor specific section flags.  */

/* This section contains sufficient relocs to be relaxed.
   When relaxing, even relocs of branch instructions the assembler could
   complete must be present because relaxing may cause the branch target to
   move.  */
#define SHF_M32R_CAN_RELAX	0x10000000

/* Processor specific flags for the ELF header e_flags field.  */

/* Two bit m32r architecture field.  */
#define EF_M32R_ARCH		0x30000000

/* m32r code.  */
#define E_M32R_ARCH		0x00000000
/* m32rx code.  */
#define E_M32RX_ARCH		0x10000000

#endif
