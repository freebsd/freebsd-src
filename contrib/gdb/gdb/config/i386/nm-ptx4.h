/* Definitions to make GDB run on a Sequent Symmetry under ptx
   with Weitek 1167 and i387 support.
   Copyright 1986, 1987, 1989, 1992  Free Software Foundation, Inc.

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

#include "nm-sysv4.h"

#undef USE_PROC_FS

#include "nm-symmetry.h"

#define PTRACE_READ_REGS(pid,regaddr) mptrace (XPT_RREGS, (pid), (regaddr), 0)
#define PTRACE_WRITE_REGS(pid,regaddr) \
  mptrace (XPT_WREGS, (pid), (regaddr), 0)

/* Override copies of {fetch,store}_inferior_registers in infptrace.c.  */

#define FETCH_INFERIOR_REGISTERS

/* We must fetch all the regs before storing, since we store all at once.  */

#define CHILD_PREPARE_TO_STORE() read_register_bytes (0, NULL, REGISTER_BYTES)

#define CHILD_WAIT
struct target_waitstatus;
extern int child_wait PARAMS ((int, struct target_waitstatus *));

/*
 * ptx does attach as of ptx version 2.1.  Prior to that, the interface
 * exists but does not work.
 *
 * FIXME: Using attach/detach requires using the ptx MPDEBUGGER
 * interface.  There are still problems with that, so for now don't
 * enable attach/detach.  If you turn it on anyway, it will mostly
 * work, but has a number of bugs. -fubar, 2/94.
 */
/*#define ATTACH_DETACH 1*/
#undef ATTACH_DETACH
#define PTRACE_ATTACH XPT_DEBUG
#define PTRACE_DETACH XPT_UNDEBUG
/*
 * The following drivel is needed because there are two ptrace-ish
 * calls on ptx: ptrace() and mptrace(), each of which does about half
 * of the ptrace functions.
 */
#define PTRACE_ATTACH_CALL(pid)  ptx_do_attach(pid)
#define PTRACE_DETACH_CALL(pid, signo) ptx_do_detach(pid, signo)
