/* Target-dependent definitions for FreeBSD/i386.
   Copyright 1997, 1999, 2000, 2001 Free Software Foundation, Inc.

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

/* $FreeBSD$ */

#ifndef TM_FBSD_H
#define TM_FBSD_H

#define HAVE_I387_REGS
#include "i386/tm-i386.h"

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* FreeBSD/ELF uses stabs-in-ELF with the DWARF register numbering
   scheme by default, so we must redefine STAB_REG_TO_REGNUM.  This
   messes up the floating-point registers for a.out, but there is not
   much we can do about that.  */

#undef STAB_REG_TO_REGNUM
#define STAB_REG_TO_REGNUM(reg) i386_dwarf_reg_to_regnum ((reg))

/* FreeBSD uses the old gcc convention for struct returns.  */

#define USE_STRUCT_CONVENTION(gcc_p, type) \
  generic_use_struct_convention (1, type)


/* Support for longjmp.  */

/* Details about jmp_buf.  It's supposed to be an array of integers.  */

#define JB_ELEMENT_SIZE 4	/* Size of elements in jmp_buf.  */
#define JB_PC		0	/* Array index of saved PC.  */

/* Figure out where the longjmp will land.  Store the address that
   longjmp will jump to in *ADDR, and return non-zero if successful.  */

#define GET_LONGJMP_TARGET(addr) get_longjmp_target (addr)
extern int get_longjmp_target (CORE_ADDR *addr);


/* Support for signal handlers.  */

#define IN_SIGTRAMP(pc, name) i386bsd_in_sigtramp (pc, name)
extern int i386bsd_in_sigtramp (CORE_ADDR pc, char *name);

/* These defines allow the recognition of sigtramps as a function name
   <sigtramp>.

   FIXME: kettenis/2001-07-13: These should be added to the target
   vector and turned into functions when we go "multi-arch".  */

#define SIGTRAMP_START(pc) i386bsd_sigtramp_start
#define SIGTRAMP_END(pc) i386bsd_sigtramp_end
extern CORE_ADDR i386bsd_sigtramp_start;
extern CORE_ADDR i386bsd_sigtramp_end;

/* Override FRAME_SAVED_PC to enable the recognition of signal handlers.  */

#undef FRAME_SAVED_PC
#define FRAME_SAVED_PC(frame) i386bsd_frame_saved_pc (frame)
extern CORE_ADDR i386bsd_frame_saved_pc (struct frame_info *frame);


/* Shared library support.  */

#ifndef SVR4_SHARED_LIBS

/* Return non-zero if we are in a shared library trampoline code stub.  */

#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) \
  (name && !strcmp(name, "_DYNAMIC"))

#endif /* !SVR4_SHARED_LIBS */

#endif /* TM_FBSD_H */
