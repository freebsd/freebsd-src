/* x86_64 ELF support for BFD.
   Copyright (C) 2000 Free Software Foundation, Inc.
   Contributed by Jan Hubicka <jh@suse.cz>

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

#ifndef _ELF_X86_64_H
#define _ELF_X86_64_H

#include "elf/reloc-macros.h"

START_RELOC_NUMBERS (elf_x86_64_reloc_type)
     RELOC_NUMBER (R_X86_64_NONE,     0)      /* No reloc */
     RELOC_NUMBER (R_X86_64_64,               1)      /* Direct 64 bit  */
     RELOC_NUMBER (R_X86_64_PC32,     2)      /* PC relative 32 bit signed */
     RELOC_NUMBER (R_X86_64_GOT32,    3)      /* 32 bit GOT entry */
     RELOC_NUMBER (R_X86_64_PLT32,    4)      /* 32 bit PLT address */
     RELOC_NUMBER (R_X86_64_COPY,     5)      /* Copy symbol at runtime */
     RELOC_NUMBER (R_X86_64_GLOB_DAT, 6)      /* Create GOT entry */
     RELOC_NUMBER (R_X86_64_JUMP_SLOT,        7)      /* Create PLT entry */
     RELOC_NUMBER (R_X86_64_RELATIVE, 8)      /* Adjust by program base */
     RELOC_NUMBER (R_X86_64_GOTPCREL, 9)      /* 32 bit signed pc relative
                                                 offset to GOT */
     RELOC_NUMBER (R_X86_64_32,               10)     /* Direct 32 bit zero extended */
     RELOC_NUMBER (R_X86_64_32S,              11)     /* Direct 32 bit sign extended */
     RELOC_NUMBER (R_X86_64_16,               12)     /* Direct 16 bit zero extended */
     RELOC_NUMBER (R_X86_64_PC16,     13)     /* 16 bit sign extended pc relative*/
     RELOC_NUMBER (R_X86_64_8,                14)     /* Direct 8 bit sign extended */
     RELOC_NUMBER (R_X86_64_PC8,              15)     /* 8 bit sign extended pc relative*/
END_RELOC_NUMBERS (R_X86_64_max)

#endif
