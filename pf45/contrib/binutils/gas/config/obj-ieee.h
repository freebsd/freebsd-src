/* This file is obj-ieee.h
   Copyright 1987, 1988, 1989, 1990, 1991, 1992, 2000, 2002, 2003
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define BFD 1

#include "bfd.h"

typedef struct
{
  asymbol sy;
  int seg;
}
obj_symbol_type;

#define S_GET_NAME(s) (((s)->sy_symbol.sy.name))

/* Return true for symbols that should not be reduced to section
   symbols or eliminated from expressions, because they may be
   overridden by the linker.  */
#define S_FORCE_RELOC(s, strict) (!SEG_NORMAL (x->sy_symbol.seg))

typedef struct
  {
    int x;
  }
object_headers;

#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE 1

int lineno_rootP;

#define IEEE_STYLE
