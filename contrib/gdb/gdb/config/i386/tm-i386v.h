/* Macro definitions for i386, Unix System V.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1998, 1999,
   2000, 2001 Free Software Foundation, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef TM_I386V_H
#define TM_I386V_H 1

/* First pick up the generic *86 target file. */

#include "i386/tm-i386.h"

/* Number of traps that happen between exec'ing the shell to run an
   inferior, and when we finally get to the inferior code.  This is
   2 on most implementations.  Override here to 4. */

#undef  START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 4

#endif /* ifndef TM_I386V_H */
