/* m68hc11 & m68hc12 ELF support for BFD.
   Copyright 1999, 2000 Free Software Foundation, Inc.

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

#ifndef _ELF_M68HC11_H
#define _ELF_M68HC11_H

#include "elf/reloc-macros.h"

/* Relocation types.  */
START_RELOC_NUMBERS (elf_m68hc11_reloc_type)
  RELOC_NUMBER (R_M68HC11_NONE, 0)
  RELOC_NUMBER (R_M68HC11_8, 1)
  RELOC_NUMBER (R_M68HC11_HI8, 2)
  RELOC_NUMBER (R_M68HC11_LO8, 3)
  RELOC_NUMBER (R_M68HC11_PCREL_8, 4)
  RELOC_NUMBER (R_M68HC11_16, 5)
  RELOC_NUMBER (R_M68HC11_32, 6)
  RELOC_NUMBER (R_M68HC11_3B, 7)
  RELOC_NUMBER (R_M68HC11_PCREL_16, 8)

     /* These are GNU extensions to enable C++ vtable garbage collection.  */
  RELOC_NUMBER (R_M68HC11_GNU_VTINHERIT, 9)
  RELOC_NUMBER (R_M68HC11_GNU_VTENTRY, 10)
END_RELOC_NUMBERS (R_M68HC11_max)

#endif
