/* BFD back-end for Motorola 68000 COFF binaries having underscore with name.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Written by Cygnus Support.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define TARGET_SYM	m68kcoffun_vec
#define TARGET_NAME	"coff-m68k-un"

#define NAMES_HAVE_UNDERSCORE

/* define this to not have multiple copy of m68k_rtype2howto
   in the executable file */
#define ONLY_DECLARE_RELOCS

/* This magic number indicates that the names have underscores.
   Other 68k magic numbers indicate that the names do not have
   underscores.  */
#define BADMAG(x) ((x).f_magic != MC68KBCSMAGIC)

#include "coff-m68k.c"
