/* macro.h - header file for macro support for gas and gasp
   Copyright (C) 1994, 1995 Free Software Foundation, Inc.

   Written by Steve and Judy Chamberlain of Cygnus Support,
      sac@cygnus.com

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
   02111-1307, USA. */

#ifndef MACRO_H

#define MACRO_H

#include "ansidecl.h"
#include "sb.h"

/* Whether any macros have been defined.  */

extern int macro_defined;

/* The macro nesting level.  */

extern int macro_nest;

extern int buffer_and_nest
  PARAMS ((const char *, const char *, sb *, int (*) PARAMS ((sb *))));
extern void macro_init
  PARAMS ((int alternate, int mri, int strip_at,
	   int (*) PARAMS ((const char *, int, sb *, int *))));
extern const char *define_macro
  PARAMS ((int idx, sb *in, sb *label, int (*get_line) PARAMS ((sb *)),
	   const char **namep));
extern int check_macro PARAMS ((const char *, sb *, int, const char **));
extern void delete_macro PARAMS ((const char *));
extern const char *expand_irp
  PARAMS ((int, int, sb *, sb *, int (*) PARAMS ((sb *)), int));

#endif
