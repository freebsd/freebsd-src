/* Native definitions for alpha running FreeBSD.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   argument regs.  A0_REGNUM points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

/* $FreeBSD$ */

#include <sys/types.h>

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

/* Tell gdb that we can attach and detach other processes */
#define ATTACH_DETACH

/* We define our own fetch/store methods */
#define FETCH_INFERIOR_REGISTERS

extern CORE_ADDR alpha_u_regs_offset();
#define U_REGS_OFFSET alpha_u_regs_offset()

#define PTRACE_ARG3_TYPE caddr_t

/* ptrace transfers longs, the ptrace man page is lying.  */

#define PTRACE_XFER_TYPE int

/* The alpha does not step over a breakpoint, the manpage is lying again.  */

#define CANNOT_STEP_BREAKPOINT

/* Linux has shared libraries.  */

#define GDB_TARGET_HAS_SHARED_LIBS

/* Support for shared libraries.  */

#include "solib.h"

#ifdef __ELF__
#define SVR4_SHARED_LIBS
#define TARGET_ELF64
#endif

/* This is a lie.  It's actually in stdio.h. */

#define PSIGNAL_IN_SIGNAL_H

/* Given a pointer to either a gregset_t or fpregset_t, return a
   pointer to the first register.  */
#define ALPHA_REGSET_BASE(regsetp)  ((long *) (regsetp))

extern int kernel_debugging;
extern int kernel_writablecore;

#define ADDITIONAL_OPTIONS \
        {"kernel", no_argument, &kernel_debugging, 1}, \
        {"k", no_argument, &kernel_debugging, 1}, \
        {"wcore", no_argument, &kernel_writablecore, 1}, \
        {"w", no_argument, &kernel_writablecore, 1},

#define ADDITIONAL_OPTION_HELP \
        "\
  --kernel           Enable kernel debugging.\n\
  --wcore            Make core file writable (only works for /dev/mem).\n\
                     This option only works while debugging a kernel !!\n\
"

#define DEFAULT_PROMPT kernel_debugging?"(kgdb) ":"(gdb) "

/* misuse START_PROGRESS to test whether we're running as kgdb */
/* START_PROGRESS is called at the top of main */
#undef START_PROGRESS
#define START_PROGRESS(STR,N) \
  if (!strcmp(STR, "kgdb")) \
     kernel_debugging = 1;

