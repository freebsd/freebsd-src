/* MC68k ELF support for BFD.
   Copyright 1998, 1999, 2000 Free Software Foundation, Inc.

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

#ifndef _ELF_M68K_H
#define _ELF_M68K_H

#include "elf/reloc-macros.h"

/* Relocation types.  */
START_RELOC_NUMBERS (elf_m68k_reloc_type)
  RELOC_NUMBER (R_68K_NONE, 0)		/* No reloc */
  RELOC_NUMBER (R_68K_32, 1)		/* Direct 32 bit  */
  RELOC_NUMBER (R_68K_16, 2)		/* Direct 16 bit  */
  RELOC_NUMBER (R_68K_8, 3)		/* Direct 8 bit  */
  RELOC_NUMBER (R_68K_PC32, 4)		/* PC relative 32 bit */
  RELOC_NUMBER (R_68K_PC16, 5)		/* PC relative 16 bit */
  RELOC_NUMBER (R_68K_PC8, 6)		/* PC relative 8 bit */
  RELOC_NUMBER (R_68K_GOT32, 7)		/* 32 bit PC relative GOT entry */
  RELOC_NUMBER (R_68K_GOT16, 8)		/* 16 bit PC relative GOT entry */
  RELOC_NUMBER (R_68K_GOT8, 9)		/* 8 bit PC relative GOT entry */
  RELOC_NUMBER (R_68K_GOT32O, 10)	/* 32 bit GOT offset */
  RELOC_NUMBER (R_68K_GOT16O, 11)	/* 16 bit GOT offset */
  RELOC_NUMBER (R_68K_GOT8O, 12)	/* 8 bit GOT offset */
  RELOC_NUMBER (R_68K_PLT32, 13)	/* 32 bit PC relative PLT address */
  RELOC_NUMBER (R_68K_PLT16, 14)	/* 16 bit PC relative PLT address */
  RELOC_NUMBER (R_68K_PLT8, 15)		/* 8 bit PC relative PLT address */
  RELOC_NUMBER (R_68K_PLT32O, 16)	/* 32 bit PLT offset */
  RELOC_NUMBER (R_68K_PLT16O, 17)	/* 16 bit PLT offset */
  RELOC_NUMBER (R_68K_PLT8O, 18)	/* 8 bit PLT offset */
  RELOC_NUMBER (R_68K_COPY, 19)		/* Copy symbol at runtime */
  RELOC_NUMBER (R_68K_GLOB_DAT, 20)	/* Create GOT entry */
  RELOC_NUMBER (R_68K_JMP_SLOT, 21)	/* Create PLT entry */
  RELOC_NUMBER (R_68K_RELATIVE, 22)	/* Adjust by program base */
  /* These are GNU extensions to enable C++ vtable garbage collection.  */
  RELOC_NUMBER (R_68K_GNU_VTINHERIT, 23)
  RELOC_NUMBER (R_68K_GNU_VTENTRY, 24)
END_RELOC_NUMBERS (R_68K_max)

#define EF_CPU32    0x00810000

#endif
