/* CRIS ELF support for BFD.
   Copyright 2000, 2001 Free Software Foundation, Inc.
   Contributed by Axis Communications AB, Lund, Sweden.
   Written by Hans-Peter Nilsson.

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

#ifndef _ELF_CRIS_H
#define _ELF_CRIS_H

#include "elf/reloc-macros.h"

/* Relocations.  */
START_RELOC_NUMBERS (elf_cris_reloc_type)
  RELOC_NUMBER (R_CRIS_NONE,		0)
  RELOC_NUMBER (R_CRIS_8,		1)
  RELOC_NUMBER (R_CRIS_16,		2)
  RELOC_NUMBER (R_CRIS_32,		3)
  RELOC_NUMBER (R_CRIS_8_PCREL,		4)
  RELOC_NUMBER (R_CRIS_16_PCREL,	5)
  RELOC_NUMBER (R_CRIS_32_PCREL,	6)

  RELOC_NUMBER (R_CRIS_GNU_VTINHERIT,	7)
  RELOC_NUMBER (R_CRIS_GNU_VTENTRY,	8)

  /* No other relocs must be visible outside the assembler.  */

END_RELOC_NUMBERS (R_CRIS_max)

/* User symbols in this file have a leading underscore.  */
#define EF_CRIS_UNDERSCORE		0x00000001

#endif /* _ELF_CRIS_H */
