/* Macro definitions for i386 running the GNU Hurd.
   Copyright 1992, 1999, 2000 Free Software Foundation, Inc.

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

#ifndef TM_I386GNU_H
#define TM_I386GNU_H 1

/* Include common definitions for GNU systems.
   FIXME: This does not belong here since this is supposed to contain
   only native-dependent information.  */
#include "nm-gnu.h"

/* Thread flavors used in re-setting the T bit.
   FIXME: This is native-dependent.  */
#define THREAD_STATE_FLAVOR		i386_REGS_SEGS_STATE
#define THREAD_STATE_SIZE		i386_THREAD_STATE_COUNT
#define THREAD_STATE_SET_TRACED(state) \
  	((struct i386_thread_state *)state)->efl |= 0x100
#define THREAD_STATE_CLEAR_TRACED(state) \
  	((((struct i386_thread_state *)state)->efl &= ~0x100), 1)

/* We can attach and detach.
   FIXME: This is probably native-dependent too.  */
#define ATTACH_DETACH 1

#define HAVE_I387_REGS
#include "i386/tm-i386.h"

/* We use stabs-in-ELF with the DWARF register numbering scheme.  */

#undef STAB_REG_TO_REGNUM
#define STAB_REG_TO_REGNUM(reg) i386_dwarf_reg_to_regnum ((reg))

/* Offset to saved PC in sigcontext.  */
#define SIGCONTEXT_PC_OFFSET 68

/* We need this file for the SOLIB_TRAMPOLINE stuff.  */
#include "tm-sysv4.h"

#endif /* TM_I386GNU_H */
