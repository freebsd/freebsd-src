/* Native support for GNU/Linux, for GDB, the GNU debugger.
   Copyright 1999, 2000, 2001
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

#ifndef NM_LINUX_H
#define NM_LINUX_H

#include "nm-linux.h"

/* Note:  It seems likely that we'll have to eventually define
   FETCH_INFERIOR_REGISTERS.  But until that time, we'll make do
   with the following. */

#define CANNOT_FETCH_REGISTER(regno) ia64_cannot_fetch_register(regno)
extern int ia64_cannot_fetch_register (int regno);

#define CANNOT_STORE_REGISTER(regno) ia64_cannot_store_register(regno)
extern int ia64_cannot_store_register (int regno);

#ifdef GDBSERVER
#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = ia64_register_u_addr ((blockend),(regno));

extern int ia64_register_u_addr(int, int);
#endif /* GDBSERVER */

#define U_REGS_OFFSET 0

#define PTRACE_ARG3_TYPE long
#define PTRACE_XFER_TYPE long

/* Hardware watchpoints */

#define TARGET_HAS_HARDWARE_WATCHPOINTS

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) 1

/* The IA-64 architecture can step over a watch point (without triggering
   it again) if the "dd" (data debug fault disable) bit in the processor
   status word is set.
   
   This PSR bit is set in ia64_linux_stopped_by_watchpoint when the
   code there has determined that a hardware watchpoint has indeed
   been hit.  The CPU will then be able to execute one instruction 
   without triggering a watchpoint. */
#define HAVE_STEPPABLE_WATCHPOINT 1

#define STOPPED_BY_WATCHPOINT(W) \
  ia64_linux_stopped_by_watchpoint (inferior_ptid)
extern CORE_ADDR ia64_linux_stopped_by_watchpoint (ptid_t ptid);

#define target_insert_watchpoint(addr, len, type) \
  ia64_linux_insert_watchpoint (inferior_ptid, addr, len, type)
extern int ia64_linux_insert_watchpoint (ptid_t ptid, CORE_ADDR addr,
                                         int len, int rw);

#define target_remove_watchpoint(addr, len, type) \
  ia64_linux_remove_watchpoint (inferior_ptid, addr, len)
extern int ia64_linux_remove_watchpoint (ptid_t ptid, CORE_ADDR addr,
                                         int len);

#endif /* #ifndef NM_LINUX_H */
