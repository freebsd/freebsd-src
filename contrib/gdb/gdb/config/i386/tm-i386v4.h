/* Macro definitions for GDB on an Intel i386 running SVR4.
   Copyright (C) 1991, 1994 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support (fnf@cygnus.com)

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

#ifndef TM_I386V4_H
#define TM_I386V4_H 1

/* Pick up most of what we need from the generic i386 target include file. */

#include "i386/tm-i386.h"

/* Pick up more stuff from the generic SVR4 host include file. */

#include "tm-sysv4.h"

/* Use the alternate method of determining valid frame chains. */

#define FRAME_CHAIN_VALID(fp,fi) alternate_frame_chain_valid (fp, fi)

/* Offsets (in target ints) into jmp_buf.  Not defined in any system header
   file, so we have to step through setjmp/longjmp with a debugger and figure
   them out.  Note that <setjmp> defines _JBLEN as 10, which is the default
   if no specific machine is selected, even though we only use 6 slots. */

#define JB_ELEMENT_SIZE sizeof(int)	/* jmp_buf[_JBLEN] is array of ints */

#define JB_EBX	0
#define JB_ESI	1
#define JB_EDI	2
#define JB_EBP	3
#define JB_ESP	4
#define JB_EDX	5

#define JB_PC	JB_EDX	/* Setjmp()'s return PC saved in EDX */

/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)

/* The following redefines make backtracing through sigtramp work.
   They manufacture a fake sigtramp frame and obtain the saved pc in sigtramp
   from the ucontext structure which is pushed by the kernel on the
   user stack. Unfortunately there are three variants of sigtramp handlers.  */

#define I386V4_SIGTRAMP_SAVED_PC
#define IN_SIGTRAMP(pc, name) ((name)					\
			       && (STREQ ("_sigreturn", name)		\
				   || STREQ ("_sigacthandler", name)	\
				   || STREQ ("sigvechandler", name)))

/* Saved Pc.  Get it from ucontext if within sigtramp.  */

#define sigtramp_saved_pc i386v4_sigtramp_saved_pc
extern CORE_ADDR i386v4_sigtramp_saved_pc PARAMS ((struct frame_info *));

#endif  /* ifndef TM_I386V4_H */
