/* Native-dependent definitions for FreeBSD/sparc64.
   Copyright 2002
   Free Software Foundation, Inc.
   Contributed by David E. O'Brien <obrien@FreeBSD.org>.

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

#ifndef NM_FBSD_H
#define NM_FBSD_H

/* Type of the third argument to the `ptrace' system call.  */
#define PTRACE_ARG3_TYPE caddr_t

/* Override copies of {fetch,store}_inferior_registers in `infptrace.c'.  */
#define FETCH_INFERIOR_REGISTERS

/* We can attach and detach.  */
#define ATTACH_DETACH


/* Shared library support.  */

#define SVR4_SHARED_LIBS

#include "solib.h"		/* Support for shared libraries. */
#include "elf/common.h"		/* Additional ELF shared library info. */

/* Make things match up with what is expected in sparc-nat.c.  */

#define PTRACE_GETREGS	 PT_GETREGS
#define PTRACE_SETREGS	 PT_SETREGS
#define PTRACE_GETFPREGS PT_GETFPREGS
#define PTRACE_SETFPREGS PT_SETFPREGS

#define GDB_GREGSET_T	struct reg
#define GDB_FPREGSET_T	struct fpreg

#define regs		trapframe
#define r_g1		tf_global[1]
#define r_ps		tf_tstate
#define r_pc		tf_tpc
#define r_npc		tf_tnpc
#define r_y		tf_y

#define FPU_FSR_TYPE	unsigned long
#define fp_status	fpreg		/* our reg.h */
#define fpu		fpreg		/* our reg.h */
#define fpu_regs	fr_regs		/* one field of fpu_fr on Solaris */
#define fpu_fr		fr_regs		/* a union w/in struct fpu on Solaris */
#define fpu_fsr		fr_fsr
#define Fpu_fsr		fr_fsr

#endif /* NM_FBSD_H */
