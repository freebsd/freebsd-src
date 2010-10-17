/* winduni.h -- header file for unicode support for windres program.
   Copyright 1997, 1998, 2002 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "ansidecl.h"

/* This header file declares the types and functions we use for
   unicode support in windres.  Our unicode support is very limited at
   present.

   We don't put this stuff in windres.h so that winduni.c doesn't have
   to include windres.h.  winduni.c needs to includes windows.h, and
   that would conflict with the definitions of Windows macros we
   already have in windres.h.  */

/* We use this type to hold a unicode character.  */

typedef unsigned short unichar;

/* Escape character translations.  */

#define ESCAPE_A (007)
#define ESCAPE_B (010)
#define ESCAPE_F (014)
#define ESCAPE_N (012)
#define ESCAPE_R (015)
#define ESCAPE_T (011)
#define ESCAPE_V (013)

/* Convert an ASCII string to a unicode string.  */

extern void unicode_from_ascii
  PARAMS ((int *, unichar **, const char *));

/* Print a unicode string to a file.  */

extern void unicode_print PARAMS ((FILE *, const unichar *, int));

/* Windres support routine called by unicode_from_ascii.  This is both
   here and in windres.h.  It should probably be in a separate header
   file, but it hardly seems worth it for one function.  */

extern PTR res_alloc PARAMS ((size_t));
