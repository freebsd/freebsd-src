/* Macro definitions for i386 running under BSD Unix.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996
   Free Software Foundation, Inc.


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

#ifndef TM_I386OS9K_H
#define TM_I386OS9K_H 1

#include "i386/tm-i386.h"

/* Number of machine registers */

#undef  NUM_REGS
#define NUM_REGS (16)		/* Basic i*86 regs */

/* Initializer for an array of names of registers.  There should be at least
   NUM_REGS strings in this initializer.  Any excess ones are simply ignored.
   The order of the first 8 registers must match the compiler's numbering
   scheme (which is the same as the 386 scheme) and also regmap in the various
   *-nat.c files. */

#undef REGISTER_NAME
#define REGISTER_NAMES { "eax", "ecx", "edx", "ebx", \
                         "esp", "ebp", "esi", "edi", \
                         "eip", "eflags", "cs", "ss", \
                         "ds", "es", "fs", "gs", \
                         }

#define DATABASE_REG 	3	/* ebx */

/* Amount PC must be decremented by after a breakpoint.  This is often the
   number of bytes in BREAKPOINT but not always (such as now). */

#undef DECR_PC_AFTER_BREAK
#define DECR_PC_AFTER_BREAK 0

/* On 386 bsd, sigtramp is above the user stack and immediately below
   the user area. Using constants here allows for cross debugging.
   These are tested for BSDI but should work on 386BSD.  */
#define SIGTRAMP_START(pc)	0xfdbfdfc0
#define SIGTRAMP_END(pc)	0xfdbfe000

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */

/* Offset to saved PC in sigcontext, from <sys/signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 20

#define BELIEVE_PCC_PROMOTION 1

#endif /* #ifndef TM_I386OS9K_H */
