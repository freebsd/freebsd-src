/* Infineon XC16X ELF support for BFD.
   Copyright 2006 Free Software Foundation, Inc.
   Contributed by KPIT Cummins Infosystems 

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
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */


#ifndef _ELF_XC16X_H
#define _ELF_XC16X_H

#include "elf/reloc-macros.h"

/* Relocations.  */
START_RELOC_NUMBERS (elf_xc16x_reloc_type)
  RELOC_NUMBER (R_XC16X_NONE, 0)
  RELOC_NUMBER (R_XC16X_ABS_8, 1)
  RELOC_NUMBER (R_XC16X_ABS_16, 2)
  RELOC_NUMBER (R_XC16X_ABS_32, 3)
  RELOC_NUMBER (R_XC16X_8_PCREL, 4)
  RELOC_NUMBER (R_XC16X_PAG, 5)
  RELOC_NUMBER (R_XC16X_POF, 6)
  RELOC_NUMBER (R_XC16X_SEG, 7)
  RELOC_NUMBER (R_XC16X_SOF, 8)

END_RELOC_NUMBERS (R_XC16X_max)

#endif /* _ELF_XC16X_H */
