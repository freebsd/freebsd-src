/* Native support for GNU/Linux x86.

   Copyright 1986, 1987, 1989, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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

/* GNU/Linux supports the i386 hardware debugging registers.  */
#define I386_USE_GENERIC_WATCHPOINTS

#include "i386/nm-i386.h"
#include "nm-linux.h"

/* Support for the user area.  */

/* Return the size of the user struct.  */
extern int kernel_u_size (void);
#define KERNEL_U_SIZE kernel_u_size()

/* This is the amount to substract from u.u_ar0 to get the offset in
   the core file of the register values.  */
#define KERNEL_U_ADDR 0

/* Offset of the registers within the user area.  */
#define U_REGS_OFFSET 0

extern CORE_ADDR register_u_addr (CORE_ADDR blockend, int regnum);
#define REGISTER_U_ADDR(addr, blockend, regnum) \
  (addr) = register_u_addr (blockend, regnum)

/* Provide access to the i386 hardware debugging registers.  */

extern void i386_linux_dr_set_control (unsigned long control);
#define I386_DR_LOW_SET_CONTROL(control) \
  i386_linux_dr_set_control (control)

extern void i386_linux_dr_set_addr (int regnum, CORE_ADDR addr);
#define I386_DR_LOW_SET_ADDR(regnum, addr) \
  i386_linux_dr_set_addr (regnum, addr)

extern void i386_linux_dr_reset_addr (int regnum);
#define I386_DR_LOW_RESET_ADDR(regnum) \
  i386_linux_dr_reset_addr (regnum)

extern unsigned long i386_linux_dr_get_status (void);
#define I386_DR_LOW_GET_STATUS() \
  i386_linux_dr_get_status ()


/* Override copies of {fetch,store}_inferior_registers in `infptrace.c'.  */
#define FETCH_INFERIOR_REGISTERS

/* Nevertheless, define CANNOT_{FETCH,STORE}_REGISTER, because we
   might fall back on the code `infptrace.c' (well a copy of that code
   in `i386-linux-nat.c' for now) and we can access only the
   general-purpose registers in that way.  */
extern int cannot_fetch_register (int regno);
extern int cannot_store_register (int regno);
#define CANNOT_FETCH_REGISTER(regno) cannot_fetch_register (regno)
#define CANNOT_STORE_REGISTER(regno) cannot_store_register (regno)

/* Override child_resume in `infptrace.c'.  */
#define CHILD_RESUME

#endif /* nm-linux.h */
