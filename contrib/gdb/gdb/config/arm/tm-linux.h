/* Target definitions for GNU/Linux on ARM, for GDB.
   Copyright 1999, 2000 Free Software Foundation, Inc.

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

#ifndef TM_ARMLINUX_H
#define TM_ARMLINUX_H

#ifdef GDBSERVER
#define	ARM_GNULINUX_TARGET
#endif

/* Include the common ARM target definitions.  */
#include "arm/tm-arm.h"

#include "tm-linux.h"

/* Use target-specific function to define link map offsets.  */
extern struct link_map_offsets *arm_linux_svr4_fetch_link_map_offsets (void);
#define SVR4_FETCH_LINK_MAP_OFFSETS() arm_linux_svr4_fetch_link_map_offsets ()

/* Offset to saved PC in sigcontext structure, from <asm/sigcontext.h> */
#define SIGCONTEXT_PC_OFFSET	(sizeof(unsigned long) * 18)

/* We've multi-arched this.  */
#undef IN_SOLIB_CALL_TRAMPOLINE

/* On ARM GNU/Linux, a call to a library routine does not have to go
   through any trampoline code.  */
#define IN_SOLIB_RETURN_TRAMPOLINE(pc, name)	0

/* We've multi-arched this.  */
#undef SKIP_TRAMPOLINE_CODE

/* When we call a function in a shared library, and the PLT sends us
   into the dynamic linker to find the function's real address, we
   need to skip over the dynamic linker call.  This function decides
   when to skip, and where to skip to.  See the comments for
   SKIP_SOLIB_RESOLVER at the top of infrun.c.  */
extern CORE_ADDR arm_linux_skip_solib_resolver (CORE_ADDR pc);
#define SKIP_SOLIB_RESOLVER arm_linux_skip_solib_resolver

/* When we call a function in a shared library, and the PLT sends us
   into the dynamic linker to find the function's real address, we
   need to skip over the dynamic linker call.  This function decides
   when to skip, and where to skip to.  See the comments for
   SKIP_SOLIB_RESOLVER at the top of infrun.c.  */
#if 0   
#undef IN_SOLIB_DYNSYM_RESOLVE_CODE
extern CORE_ADDR arm_in_solib_dynsym_resolve_code (CORE_ADDR pc, char *name);
#define IN_SOLIB_DYNSYM_RESOLVE_CODE  arm_in_solib_dynsym_resolve_code
/* ScottB: Current definition is 
extern CORE_ADDR in_svr4_dynsym_resolve_code (CORE_ADDR pc, char *name);
#define IN_SOLIB_DYNSYM_RESOLVE_CODE  in_svr4_dynsym_resolve_code */
#endif

/* When the ARM Linux kernel invokes a signal handler, the return
   address points at a special instruction which'll trap back into
   the kernel.  These definitions are used to identify this bit of
   code as a signal trampoline in order to support backtracing
   through calls to signal handlers. */

int arm_linux_in_sigtramp (CORE_ADDR pc, char *name);
#define IN_SIGTRAMP(pc, name) arm_linux_in_sigtramp (pc, name)

/* Each OS has different mechanisms for accessing the various
   registers stored in the sigcontext structure.  These definitions
   provide a mechanism by which the generic code in arm-tdep.c can
   find the addresses at which various registers are saved at in the
   sigcontext structure.  If SIGCONTEXT_REGISTER_ADDRESS is not
   defined, arm-tdep.c will define it to be 0.  (See ia64-tdep.c and
   ia64-linux-tdep.c to see what a similar mechanism looks like when
   multi-arched.) */

extern CORE_ADDR arm_linux_sigcontext_register_address (CORE_ADDR, CORE_ADDR,
                                                        int);
#define SIGCONTEXT_REGISTER_ADDRESS arm_linux_sigcontext_register_address

#endif /* TM_ARMLINUX_H */
