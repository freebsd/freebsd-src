/* Macro definitions for GDB on an Intel i386 running SCO Open Server 5.
   Copyright (C) 1998 Free Software Foundation, Inc.
   Written by J. Kean Johnston (jkj@sco.com).

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

#ifndef TM_I386SCO5_H
#define TM_I386SCO5_H 1

/* Pick up most of what we need from the generic i386 target include file. */

#include "i386/tm-i386.h"

/* Pick up more stuff from the generic SYSV and SVR4 host include files. */
#include "i386/tm-i386v.h"
#include "tm-sysv4.h"

#define KERNEL_U_SIZE kernel_u_size()

/*
 * SCO is unlike other SVR3 targets in that it has SVR4 style shared
 * libs, with a slight twist. We expect 3 traps (2 for the exec and
 * one for the dynamic loader).  After the third trap we insert the
 * SOLIB breakpoints, then wait for the 4th trap.
 */
#undef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 3

/* We can also do hardware watchpoints */
#define TARGET_HAS_HARDWARE_WATCHPOINTS
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) 1

/* After a watchpoint trap, the PC points to the instruction which
   caused the trap.  But we can continue over it without disabling the
   trap. */
#define HAVE_CONTINUABLE_WATCHPOINT
#define HAVE_STEPPABLE_WATCHPOINT

#define STOPPED_BY_WATCHPOINT(W)  \
  i386_stopped_by_watchpoint (inferior_pid)

#define target_insert_watchpoint(addr, len, type)  \
  i386_insert_watchpoint (inferior_pid, addr, len, type)

#define target_remove_watchpoint(addr, len, type)  \
  i386_remove_watchpoint (inferior_pid, addr, len)

#endif  /* ifndef TM_I386SCO5_H */
