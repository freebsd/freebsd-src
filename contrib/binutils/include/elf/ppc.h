/* PPC ELF support for BFD.
   Copyright 1995, 1996, 1998, 2000, 2001 Free Software Foundation, Inc.

   By Michael Meissner, Cygnus Support, <meissner@cygnus.com>, from information
   in the System V Application Binary Interface, PowerPC Processor Supplement
   and the PowerPC Embedded Application Binary Interface (eabi).

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

/* This file holds definitions specific to the PPC ELF ABI.  Note
   that most of this is not actually implemented by BFD.  */

#ifndef _ELF_PPC_H
#define _ELF_PPC_H

#include "elf/reloc-macros.h"

/* Relocations.  */
START_RELOC_NUMBERS (elf_ppc_reloc_type)
  RELOC_NUMBER (R_PPC_NONE, 0)
  RELOC_NUMBER (R_PPC_ADDR32, 1)
  RELOC_NUMBER (R_PPC_ADDR24, 2)
  RELOC_NUMBER (R_PPC_ADDR16, 3)
  RELOC_NUMBER (R_PPC_ADDR16_LO, 4)
  RELOC_NUMBER (R_PPC_ADDR16_HI, 5)
  RELOC_NUMBER (R_PPC_ADDR16_HA, 6)
  RELOC_NUMBER (R_PPC_ADDR14, 7)
  RELOC_NUMBER (R_PPC_ADDR14_BRTAKEN, 8)
  RELOC_NUMBER (R_PPC_ADDR14_BRNTAKEN, 9)
  RELOC_NUMBER (R_PPC_REL24, 10)
  RELOC_NUMBER (R_PPC_REL14, 11)
  RELOC_NUMBER (R_PPC_REL14_BRTAKEN, 12)
  RELOC_NUMBER (R_PPC_REL14_BRNTAKEN, 13)
  RELOC_NUMBER (R_PPC_GOT16, 14)
  RELOC_NUMBER (R_PPC_GOT16_LO, 15)
  RELOC_NUMBER (R_PPC_GOT16_HI, 16)
  RELOC_NUMBER (R_PPC_GOT16_HA, 17)
  RELOC_NUMBER (R_PPC_PLTREL24, 18)
  RELOC_NUMBER (R_PPC_COPY, 19)
  RELOC_NUMBER (R_PPC_GLOB_DAT, 20)
  RELOC_NUMBER (R_PPC_JMP_SLOT, 21)
  RELOC_NUMBER (R_PPC_RELATIVE, 22)
  RELOC_NUMBER (R_PPC_LOCAL24PC, 23)
  RELOC_NUMBER (R_PPC_UADDR32, 24)
  RELOC_NUMBER (R_PPC_UADDR16, 25)
  RELOC_NUMBER (R_PPC_REL32, 26)
  RELOC_NUMBER (R_PPC_PLT32, 27)
  RELOC_NUMBER (R_PPC_PLTREL32, 28)
  RELOC_NUMBER (R_PPC_PLT16_LO, 29)
  RELOC_NUMBER (R_PPC_PLT16_HI, 30)
  RELOC_NUMBER (R_PPC_PLT16_HA, 31)
  RELOC_NUMBER (R_PPC_SDAREL16, 32)
  RELOC_NUMBER (R_PPC_SECTOFF, 33)
  RELOC_NUMBER (R_PPC_SECTOFF_LO, 34)
  RELOC_NUMBER (R_PPC_SECTOFF_HI, 35)
  RELOC_NUMBER (R_PPC_SECTOFF_HA, 36)
  RELOC_NUMBER (R_PPC_ADDR30, 37)

/* The following relocs are from the 64-bit PowerPC ELF ABI. */
  RELOC_NUMBER (R_PPC64_ADDR64,		 38)
  RELOC_NUMBER (R_PPC64_ADDR16_HIGHER,	 39)
  RELOC_NUMBER (R_PPC64_ADDR16_HIGHERA,	 40)
  RELOC_NUMBER (R_PPC64_ADDR16_HIGHEST,	 41)
  RELOC_NUMBER (R_PPC64_ADDR16_HIGHESTA, 42)
  RELOC_NUMBER (R_PPC64_UADDR64,	 43)
  RELOC_NUMBER (R_PPC64_REL64,		 44)
  RELOC_NUMBER (R_PPC64_PLT64,		 45)
  RELOC_NUMBER (R_PPC64_PLTREL64,	 46)
  RELOC_NUMBER (R_PPC64_TOC16,		 47)
  RELOC_NUMBER (R_PPC64_TOC16_LO,	 48)
  RELOC_NUMBER (R_PPC64_TOC16_HI,	 49)
  RELOC_NUMBER (R_PPC64_TOC16_HA,	 50)
  RELOC_NUMBER (R_PPC64_TOC,		 51)
  RELOC_NUMBER (R_PPC64_PLTGOT16,	 52)
  RELOC_NUMBER (R_PPC64_PLTGOT16_LO,	 53)
  RELOC_NUMBER (R_PPC64_PLTGOT16_HI,	 54)
  RELOC_NUMBER (R_PPC64_PLTGOT16_HA,	 55)

/* The following relocs were added in the 64-bit PowerPC ELF ABI revision 1.2. */
  RELOC_NUMBER (R_PPC64_ADDR16_DS,       56)
  RELOC_NUMBER (R_PPC64_ADDR16_LO_DS,    57)
  RELOC_NUMBER (R_PPC64_GOT16_DS,        58)
  RELOC_NUMBER (R_PPC64_GOT16_LO_DS,     59)
  RELOC_NUMBER (R_PPC64_PLT16_LO_DS,     60)
  RELOC_NUMBER (R_PPC64_SECTOFF_DS,      61)
  RELOC_NUMBER (R_PPC64_SECTOFF_LO_DS,   62)
  RELOC_NUMBER (R_PPC64_TOC16_DS,        63)
  RELOC_NUMBER (R_PPC64_TOC16_LO_DS,     64)
  RELOC_NUMBER (R_PPC64_PLTGOT16_DS,     65)
  RELOC_NUMBER (R_PPC64_PLTGOT16_LO_DS,  66)

/* The remaining relocs are from the Embedded ELF ABI, and are not
   in the SVR4 ELF ABI.  */
  RELOC_NUMBER (R_PPC_EMB_NADDR32, 101)
  RELOC_NUMBER (R_PPC_EMB_NADDR16, 102)
  RELOC_NUMBER (R_PPC_EMB_NADDR16_LO, 103)
  RELOC_NUMBER (R_PPC_EMB_NADDR16_HI, 104)
  RELOC_NUMBER (R_PPC_EMB_NADDR16_HA, 105)
  RELOC_NUMBER (R_PPC_EMB_SDAI16, 106)
  RELOC_NUMBER (R_PPC_EMB_SDA2I16, 107)
  RELOC_NUMBER (R_PPC_EMB_SDA2REL, 108)
  RELOC_NUMBER (R_PPC_EMB_SDA21, 109)
  RELOC_NUMBER (R_PPC_EMB_MRKREF, 110)
  RELOC_NUMBER (R_PPC_EMB_RELSEC16, 111)
  RELOC_NUMBER (R_PPC_EMB_RELST_LO, 112)
  RELOC_NUMBER (R_PPC_EMB_RELST_HI, 113)
  RELOC_NUMBER (R_PPC_EMB_RELST_HA, 114)
  RELOC_NUMBER (R_PPC_EMB_BIT_FLD, 115)
  RELOC_NUMBER (R_PPC_EMB_RELSDA, 116)

  /* These are GNU extensions to enable C++ vtable garbage collection.  */
  RELOC_NUMBER (R_PPC_GNU_VTINHERIT, 253)
  RELOC_NUMBER (R_PPC_GNU_VTENTRY, 254)

/* This is a phony reloc to handle any old fashioned TOC16 references
   that may still be in object files.  */
  RELOC_NUMBER (R_PPC_TOC16, 255)

END_RELOC_NUMBERS (R_PPC_max)

/* Aliases for R_PPC64-relocs. */
#define R_PPC64_NONE              R_PPC_NONE
#define R_PPC64_ADDR32            R_PPC_ADDR32
#define R_PPC64_ADDR24            R_PPC_ADDR24
#define R_PPC64_ADDR16            R_PPC_ADDR16
#define R_PPC64_ADDR16_LO         R_PPC_ADDR16_LO
#define R_PPC64_ADDR16_HI         R_PPC_ADDR16_HI
#define R_PPC64_ADDR16_HA         R_PPC_ADDR16_HA
#define R_PPC64_ADDR14            R_PPC_ADDR14
#define R_PPC64_ADDR14_BRTAKEN    R_PPC_ADDR14_BRTAKEN
#define R_PPC64_ADDR14_BRNTAKEN   R_PPC_ADDR14_BRNTAKEN
#define R_PPC64_REL24             R_PPC_REL24
#define R_PPC64_REL14             R_PPC_REL14
#define R_PPC64_REL14_BRTAKEN     R_PPC_REL14_BRTAKEN
#define R_PPC64_REL14_BRNTAKEN    R_PPC_REL14_BRNTAKEN
#define R_PPC64_GOT16             R_PPC_GOT16
#define R_PPC64_GOT16_LO          R_PPC_GOT16_LO
#define R_PPC64_GOT16_HI          R_PPC_GOT16_HI
#define R_PPC64_GOT16_HA          R_PPC_GOT16_HA
#define R_PPC64_COPY              R_PPC_COPY
#define R_PPC64_GLOB_DAT          R_PPC_GLOB_DAT
#define R_PPC64_JMP_SLOT          R_PPC_JMP_SLOT
#define R_PPC64_RELATIVE          R_PPC_RELATIVE
#define R_PPC64_UADDR32           R_PPC_UADDR32
#define R_PPC64_UADDR16           R_PPC_UADDR16
#define R_PPC64_REL32             R_PPC_REL32
#define R_PPC64_PLT32             R_PPC_PLT32
#define R_PPC64_PLTREL32          R_PPC_PLTREL32
#define R_PPC64_PLT16_LO          R_PPC_PLT16_LO
#define R_PPC64_PLT16_HI          R_PPC_PLT16_HI
#define R_PPC64_PLT16_HA          R_PPC_PLT16_HA
#define R_PPC64_SECTOFF           R_PPC_SECTOFF
#define R_PPC64_SECTOFF_LO        R_PPC_SECTOFF_LO
#define R_PPC64_SECTOFF_HI        R_PPC_SECTOFF_HI
#define R_PPC64_SECTOFF_HA        R_PPC_SECTOFF_HA
#define R_PPC64_ADDR30            R_PPC_ADDR30
#define R_PPC64_GNU_VTINHERIT	  R_PPC_GNU_VTINHERIT
#define R_PPC64_GNU_VTENTRY	  R_PPC_GNU_VTENTRY

/* Specify the start of the .glink section.  */
#define DT_PPC64_GLINK		DT_LOPROC

/* Specify the start and size of the .opd section.  */
#define DT_PPC64_OPD		(DT_LOPROC + 1)
#define DT_PPC64_OPDSZ		(DT_LOPROC + 2)

/* Processor specific flags for the ELF header e_flags field.  */

#define	EF_PPC_EMB		0x80000000	/* PowerPC embedded flag.  */

#define	EF_PPC_RELOCATABLE	0x00010000	/* PowerPC -mrelocatable flag.  */
#define	EF_PPC_RELOCATABLE_LIB	0x00008000	/* PowerPC -mrelocatable-lib flag.  */

/* Processor specific section headers, sh_type field.  */

#define SHT_ORDERED		SHT_HIPROC	/* Link editor is to sort the \
						   entries in this section \
						   based on the address \
						   specified in the associated \
						   symbol table entry.  */

/* Processor specific section flags, sh_flags field.  */

#define SHF_EXCLUDE		0x80000000	/* Link editor is to exclude \
						   this section from executable \
						   and shared objects that it \
						   builds when those objects \
						   are not to be furhter \
						   relocated.  */
#endif /* _ELF_PPC_H */
