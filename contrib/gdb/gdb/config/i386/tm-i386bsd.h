/* Macro definitions for i386 running under BSD Unix.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996
   Free Software Foundation, Inc.

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

#ifndef TM_I386BSD_H
#define TM_I386BSD_H 1

#include "i386/tm-i386.h"

/* 386BSD cannot handle the segment registers. */
/* BSDI can't handle them either.  */

#undef NUM_REGS
#define NUM_REGS 10

/* On 386 bsd, sigtramp is above the user stack and immediately below
   the user area. Using constants here allows for cross debugging.
   These are tested for BSDI but should work on 386BSD.  */

#define SIGTRAMP_START(pc)	0xfdbfdfc0
#define SIGTRAMP_END(pc)	0xfdbfe000

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */

/* Offset to saved PC in sigcontext, from <sys/signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 20

#endif /* ifndef TM_I386BSD_H */
