/* ALPHA ELF support for BFD.
   Copyright 1996, 1998, 2000 Free Software Foundation, Inc.

   By Eric Youngdale, <eric@aib.com>.  No processor supplement available
   for this platform.

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

/* This file holds definitions specific to the ALPHA ELF ABI.  Note
   that most of this is not actually implemented by BFD.  */

#ifndef _ELF_ALPHA_H
#define _ELF_ALPHA_H

/* Processor specific flags for the ELF header e_flags field.  */

/* All addresses must be below 2GB.  */
#define EF_ALPHA_32BIT		0x00000001

/* All relocations needed for relaxation with code movement are present.  */
#define EF_ALPHA_CANRELAX	0x00000002

/* Processor specific section flags.  */

/* This section must be in the global data area.  */
#define SHF_ALPHA_GPREL		0x10000000

/* Section contains some sort of debugging information.  The exact
   format is unspecified.  It's probably ECOFF symbols.  */
#define SHT_ALPHA_DEBUG		0x70000001

/* Section contains register usage information.  */
#define SHT_ALPHA_REGINFO	0x70000002

/* A section of type SHT_MIPS_REGINFO contains the following
   structure.  */
typedef struct
{
  /* Mask of general purpose registers used.  */
  unsigned long ri_gprmask;
  /* Mask of co-processor registers used.  */
  unsigned long ri_cprmask[4];
  /* GP register value for this object file.  */
  long ri_gp_value;
} Elf64_RegInfo;

/* Special values for the st_other field in the symbol table.  */

#define STO_ALPHA_NOPV		0x80
#define STO_ALPHA_STD_GPLOAD	0x88

#include "elf/reloc-macros.h"

/* Alpha relocs.  */
START_RELOC_NUMBERS (elf_alpha_reloc_type)
  RELOC_NUMBER (R_ALPHA_NONE, 0)	/* No reloc */
  RELOC_NUMBER (R_ALPHA_REFLONG, 1)	/* Direct 32 bit */
  RELOC_NUMBER (R_ALPHA_REFQUAD, 2)	/* Direct 64 bit */
  RELOC_NUMBER (R_ALPHA_GPREL32, 3)	/* GP relative 32 bit */
  RELOC_NUMBER (R_ALPHA_LITERAL, 4)	/* GP relative 16 bit w/optimization */
  RELOC_NUMBER (R_ALPHA_LITUSE, 5)	/* Optimization hint for LITERAL */
  RELOC_NUMBER (R_ALPHA_GPDISP, 6)	/* Add displacement to GP */
  RELOC_NUMBER (R_ALPHA_BRADDR, 7)	/* PC+4 relative 23 bit shifted */
  RELOC_NUMBER (R_ALPHA_HINT, 8)	/* PC+4 relative 16 bit shifted */
  RELOC_NUMBER (R_ALPHA_SREL16, 9)	/* PC relative 16 bit */
  RELOC_NUMBER (R_ALPHA_SREL32, 10)	/* PC relative 32 bit */
  RELOC_NUMBER (R_ALPHA_SREL64, 11)	/* PC relative 64 bit */

/* Inherited these from ECOFF, but they are not particularly useful
   and are depreciated.  And not implemented in the BFD, btw.  */
  RELOC_NUMBER (R_ALPHA_OP_PUSH, 12)	/* OP stack push */
  RELOC_NUMBER (R_ALPHA_OP_STORE, 13)	/* OP stack pop and store */
  RELOC_NUMBER (R_ALPHA_OP_PSUB, 14)	/* OP stack subtract */
  RELOC_NUMBER (R_ALPHA_OP_PRSHIFT, 15)	/* OP stack right shift */

  RELOC_NUMBER (R_ALPHA_GPVALUE, 16)
  RELOC_NUMBER (R_ALPHA_GPRELHIGH, 17)
  RELOC_NUMBER (R_ALPHA_GPRELLOW, 18)
  RELOC_NUMBER (R_ALPHA_IMMED_GP_16, 19)
  RELOC_NUMBER (R_ALPHA_IMMED_GP_HI32, 20)
  RELOC_NUMBER (R_ALPHA_IMMED_SCN_HI32, 21)
  RELOC_NUMBER (R_ALPHA_IMMED_BR_HI32, 22)
  RELOC_NUMBER (R_ALPHA_IMMED_LO32, 23)

/* These relocations are specific to shared libraries.  */
  RELOC_NUMBER (R_ALPHA_COPY, 24)	/* Copy symbol at runtime */
  RELOC_NUMBER (R_ALPHA_GLOB_DAT, 25)	/* Create GOT entry */
  RELOC_NUMBER (R_ALPHA_JMP_SLOT, 26)	/* Create PLT entry */
  RELOC_NUMBER (R_ALPHA_RELATIVE, 27)	/* Adjust by program base */

END_RELOC_NUMBERS (R_ALPHA_max)

#endif /* _ELF_ALPHA_H */
