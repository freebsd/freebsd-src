/* Definitions to make GDB run on a mips box under 4.3bsd.
   Copyright (C) 1986, 1987, 1989 Free Software Foundation, Inc.
   Contributed by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin
   and by Alessandro Forin(af@cs.cmu.edu) at CMU

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if !defined (HOST_BYTE_ORDER)
#define HOST_BYTE_ORDER LITTLE_ENDIAN
#endif

#ifdef ultrix
/* Needed for DECstation core files.  */
#include <machine/param.h>
#define KERNEL_U_ADDR UADDR
#endif

#ifdef ultrix
extern char *strdup();
#endif

#if ! defined (__STDC__) && ! defined (offsetof)
# define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

/* Only used for core files on DECstations.
   First four registers at u.u_ar0 are saved arguments, and
   there is no r0 saved.   Float registers are saved
   in u_pcb.pcb_fpregs, not relative to u.u_ar0.  */

#define REGISTER_U_ADDR(addr, blockend, regno) 		\
	{ \
	  if (regno < FP0_REGNUM) \
	    addr = blockend + sizeof(int) * (4 + regno - 1); \
	  else \
	    addr = offsetof (struct user, u_pcb.pcb_fpregs[0]) + \
		   sizeof (int) * (regno - FP0_REGNUM); \
	}

/* Kernel is a bit tenacious about sharing text segments, disallowing bpts.  */
#define	ONE_PROCESS_WRITETEXT

/* HAVE_SGTTY also works, last we tried.

   But we have termios, at least as of Ultrix 4.2A, so use it.  */
#define HAVE_TERMIOS
