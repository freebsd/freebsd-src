/* Definitions to make GDB run on an Alpha box under Linux.  The
   definitions here are used when the _target_ system is running Linux.
   Copyright 1996 Free Software Foundation, Inc.

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

#ifndef TM_LINUXALPHA_H
#define TM_LINUXALPHA_H

#include "alpha/tm-alpha.h"

/* Are we currently handling a signal ?  */

extern long alpha_linux_sigtramp_offset PARAMS ((CORE_ADDR));
#undef IN_SIGTRAMP
#define IN_SIGTRAMP(pc, name)	(alpha_linux_sigtramp_offset (pc) >= 0)

/* Get start and end address of sigtramp handler.  */

#define SIGTRAMP_START(pc)	(pc - alpha_linux_sigtramp_offset (pc))
#define SIGTRAMP_END(pc)	(SIGTRAMP_START(pc) + 3*4)


/* Number of traps that happen between exec'ing the shell to run an
   inferior, and when we finally get to the inferior code.  This is 2
   on Linux and most implementations.  */

#undef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 2

/* Return TRUE if procedure descriptor PROC is a procedure descriptor
   that refers to a dynamically generated sigtramp function.  */

#undef PROC_DESC_IS_DYN_SIGTRAMP
#define PROC_SIGTRAMP_MAGIC	0x0e0f0f0f
#define PROC_DESC_IS_DYN_SIGTRAMP(proc) ((proc)->pdr.isym		\
					 == PROC_SIGTRAMP_MAGIC)
#undef SET_PROC_DESC_IS_DYN_SIGTRAMP
#define SET_PROC_DESC_IS_DYN_SIGTRAMP(proc) ((proc)->pdr.isym		\
					     = PROC_SIGTRAMP_MAGIC)

/* If PC is inside a dynamically generated sigtramp function, return
   how many bytes the program counter is beyond the start of that
   function.  Otherwise, return a negative value.  */

#undef DYNAMIC_SIGTRAMP_OFFSET
#define DYNAMIC_SIGTRAMP_OFFSET(pc)	(alpha_linux_sigtramp_offset (pc))

/* Translate a signal handler frame into the address of the sigcontext
   structure.  */

#undef SIGCONTEXT_ADDR
#define SIGCONTEXT_ADDR(frame)			((frame)->frame - 0x298)

/* If FRAME refers to a sigtramp frame, return the address of the next frame.

   Under Linux, sigtramp handlers have dynamically generated procedure
   descriptors that make this hack unnecessary.  */

#undef FRAME_PAST_SIGTRAMP_FRAME
#define FRAME_PAST_SIGTRAMP_FRAME(frame, pc)	(0)

/* We need this for the SOLIB_TRAMPOLINE stuff.  */
#include "tm-sysv4.h"

#endif /* TM_LINUXALPHA_H */
