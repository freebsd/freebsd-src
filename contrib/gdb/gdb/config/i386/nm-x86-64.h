/* Native support for GNU/Linux x86-64.

   Copyright 2001, 2002 Free Software Foundation, Inc.  Contributed by
   Jiri Smid, SuSE Labs.

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

#ifndef NM_X86_64_H
#define NM_X86_64_H

#include "nm-linux.h"

#define I386_USE_GENERIC_WATCHPOINTS
#include "i386/nm-i386.h"

/* Support for 8-byte wide hw watchpoints.  */
#define TARGET_HAS_DR_LEN_8 1

/* Provide access to the i386 hardware debugging registers.  */

extern void x86_64_linux_dr_set_control (unsigned long control);
#define I386_DR_LOW_SET_CONTROL(control) \
  x86_64_linux_dr_set_control (control)

extern void x86_64_linux_dr_set_addr (int regnum, CORE_ADDR addr);
#define I386_DR_LOW_SET_ADDR(regnum, addr) \
  x86_64_linux_dr_set_addr (regnum, addr)

extern void x86_64_linux_dr_reset_addr (int regnum);
#define I386_DR_LOW_RESET_ADDR(regnum) \
  x86_64_linux_dr_reset_addr (regnum)

extern unsigned long x86_64_linux_dr_get_status (void);
#define I386_DR_LOW_GET_STATUS() \
  x86_64_linux_dr_get_status ()


#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = x86_64_register_u_addr ((blockend),(regno));
CORE_ADDR x86_64_register_u_addr (CORE_ADDR, int);

/* Return the size of the user struct.  */
#define KERNEL_U_SIZE kernel_u_size()
extern int kernel_u_size (void);

/* Offset of the registers within the user area.  */
#define U_REGS_OFFSET 0

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */
#define KERNEL_U_ADDR 0x0

#define PTRACE_ARG3_TYPE void*
#define PTRACE_XFER_TYPE unsigned long


/* We define this if link.h is available, because with ELF we use SVR4 style
   shared libraries. */

#ifdef HAVE_LINK_H
#define SVR4_SHARED_LIBS
#include "solib.h"		/* Support for shared libraries. */
#endif

/* Override copies of {fetch,store}_inferior_registers in `infptrace.c'.  */
#define FETCH_INFERIOR_REGISTERS

#undef PREPARE_TO_PROCEED

#include <signal.h>

extern void lin_thread_get_thread_signals (sigset_t * mask);
#define GET_THREAD_SIGNALS(mask) lin_thread_get_thread_signals (mask)

#endif /* NM_X86_64.h */
