/* BFD back-end for Motorola M68K COFF LynxOS files.
   Copyright 1993, 1994, 1995, 1996, 1997 Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define TARGET_SYM	m68klynx_coff_vec
#define TARGET_NAME	"coff-m68k-lynx"
#define LYNXOS
#define COFF_LONG_FILENAMES
#define STATIC_RELOCS
#define COFF_COMMON_ADDEND

#include "coff-m68k.c"
