/* te-sun3.h -- Sun-3 target environment declarations.
   Copyright 1987, 1990, 1991, 1992, 2000 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This header file contains the #defines specific
   to SUN computer SUN 3 series computers.  (The only kind
   we have around here, unfortunately.)

   Rumor has it that this file will work on the Sun-2 if the assembler
   is called with -m68010  This is not tested.  */

#define TE_SUN3	1

#define LOCAL_LABELS_DOLLAR 1
#define LOCAL_LABELS_FB 1

/* Could also be :
   #define S_LOCAL_NAME(s)	(S_GET_NAME(s)[0] == '.' &&
   S_GET_NAME(s)[1] == 'L' ||
   S_GET_NAME(s)[1] == '.')
   */

#include "obj-format.h"

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of te-sun3.h */
