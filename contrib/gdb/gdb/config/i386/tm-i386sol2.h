/* Macro definitions for GDB on an Intel i386 running Solaris 2.
   Copyright (C) 1998 Free Software Foundation, Inc.

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

#ifndef TM_I386SOL2_H
#define TM_I386SOL2_H 1

#include "i386/tm-i386v4.h"
 
/* Signal handler frames under Solaris 2 are recognized by a return address
   of 0xFFFFFFFF, the third parameter on the signal handler stack is
   a pointer to an ucontext.  */
#undef sigtramp_saved_pc
#undef I386V4_SIGTRAMP_SAVED_PC
#define SIGCONTEXT_PC_OFFSET (36 + 14 * 4)
#undef IN_SIGTRAMP
#define IN_SIGTRAMP(pc, name) (pc == 0xFFFFFFFF)

/* The SunPRO compiler puts out 0 instead of the address in N_SO symbols,
   and for SunPRO 3.0, N_FUN symbols too.  */
#define SOFUN_ADDRESS_MAYBE_MISSING

extern char *sunpro_static_transform_name PARAMS ((char *));
#define STATIC_TRANSFORM_NAME(x) sunpro_static_transform_name (x)
#define IS_STATIC_TRANSFORM_NAME(name) ((name)[0] == '.')

#define FAULTED_USE_SIGINFO

/* Macros to extract process id and thread id from a composite pid/tid */
#define PIDGET(pid) ((pid) & 0xffff)
#define TIDGET(pid) (((pid) >> 16) & 0xffff)

/* Macro to extract carry from given regset.  */
#define PS_FLAG_CARRY 0x1	/* Carry bit in PS */
#define PROCFS_GET_CARRY(regset) ((regset)[EFL] & PS_FLAG_CARRY)

#ifdef HAVE_THREAD_DB_LIB

extern char *solaris_pid_to_str PARAMS ((int pid));
#define target_pid_to_str(PID) solaris_pid_to_str (PID)

#else

extern char *procfs_pid_to_str PARAMS ((int pid));
#define target_pid_to_str(PID) procfs_pid_to_str (PID)

#endif

#endif  /* ifndef TM_I386SOL2_H */
