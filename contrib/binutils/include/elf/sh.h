/* SH ELF support for BFD.
   Copyright (C) 1998, 2000 Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_SH_H
#define _ELF_SH_H

/* Processor specific flags for the ELF header e_flags field.  */

#define EF_SH_MACH_MASK	0x1f
#define EF_SH_UNKNOWN	   0 /* For backwards compatibility.  */
#define EF_SH1		   1
#define EF_SH2		   2
#define EF_SH3		   3
#define EF_SH_HAS_DSP(flags) ((flags) & 4)
#define EF_SH_DSP	   4
#define EF_SH3_DSP	   5
#define EF_SH_HAS_FP(flags) ((flags) & 8)
#define EF_SH3E		   8
#define EF_SH4		   9

#define EF_SH_MERGE_MACH(mach1, mach2) \
  (((((mach1) == EF_SH3 || (mach1) == EF_SH_UNKNOWN) && (mach2) == EF_SH_DSP) \
    || ((mach1) == EF_SH_DSP \
	&& ((mach2) == EF_SH3 || (mach2) == EF_SH_UNKNOWN))) \
   ? EF_SH3_DSP \
   : (((mach1) < EF_SH3 && (mach2) == EF_SH_UNKNOWN) \
      || ((mach2) < EF_SH3 && (mach1) == EF_SH_UNKNOWN)) \
   ? EF_SH3 \
   : (((mach1) == EF_SH3E && (mach2) == EF_SH_UNKNOWN) \
      || ((mach2) == EF_SH3E && (mach1) == EF_SH_UNKNOWN)) \
   ? EF_SH4 \
   : ((mach1) > (mach2) ? (mach1) : (mach2)))

#include "elf/reloc-macros.h"

/* Relocations.  */
/* Relocations 25ff are GNU extensions.
   25..33 are used for relaxation and use the same constants as COFF uses.  */
START_RELOC_NUMBERS (elf_sh_reloc_type)
  RELOC_NUMBER (R_SH_NONE, 0)
  RELOC_NUMBER (R_SH_DIR32, 1)
  RELOC_NUMBER (R_SH_REL32, 2)
  RELOC_NUMBER (R_SH_DIR8WPN, 3)
  RELOC_NUMBER (R_SH_IND12W, 4)
  RELOC_NUMBER (R_SH_DIR8WPL, 5)
  RELOC_NUMBER (R_SH_DIR8WPZ, 6)
  RELOC_NUMBER (R_SH_DIR8BP, 7)
  RELOC_NUMBER (R_SH_DIR8W, 8)
  RELOC_NUMBER (R_SH_DIR8L, 9)
  FAKE_RELOC (R_SH_FIRST_INVALID_RELOC, 10)
  FAKE_RELOC (R_SH_LAST_INVALID_RELOC, 24)
  RELOC_NUMBER (R_SH_SWITCH16, 25)
  RELOC_NUMBER (R_SH_SWITCH32, 26)
  RELOC_NUMBER (R_SH_USES, 27)
  RELOC_NUMBER (R_SH_COUNT, 28)
  RELOC_NUMBER (R_SH_ALIGN, 29)
  RELOC_NUMBER (R_SH_CODE, 30)
  RELOC_NUMBER (R_SH_DATA, 31)
  RELOC_NUMBER (R_SH_LABEL, 32)
  RELOC_NUMBER (R_SH_SWITCH8, 33)
  RELOC_NUMBER (R_SH_GNU_VTINHERIT, 34)
  RELOC_NUMBER (R_SH_GNU_VTENTRY, 35)
  RELOC_NUMBER (R_SH_LOOP_START, 36)
  RELOC_NUMBER (R_SH_LOOP_END, 37)
  FAKE_RELOC (R_SH_FIRST_INVALID_RELOC_2, 38)
  FAKE_RELOC (R_SH_LAST_INVALID_RELOC_2, 159)
  RELOC_NUMBER (R_SH_GOT32, 160)
  RELOC_NUMBER (R_SH_PLT32, 161)
  RELOC_NUMBER (R_SH_COPY, 162)
  RELOC_NUMBER (R_SH_GLOB_DAT, 163)
  RELOC_NUMBER (R_SH_JMP_SLOT, 164)
  RELOC_NUMBER (R_SH_RELATIVE, 165)
  RELOC_NUMBER (R_SH_GOTOFF, 166)
  RELOC_NUMBER (R_SH_GOTPC, 167)
END_RELOC_NUMBERS (R_SH_max)

#endif
