/* Definitions to target GDB to GNU/Linux on 386.

   Copyright 1992, 1993, 1995, 1996, 1998, 1999, 2000, 2001, 2002 Free
   Software Foundation, Inc.

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

#ifndef TM_LINUX_H
#define TM_LINUX_H

#define I386_GNULINUX_TARGET
#define HAVE_I387_REGS
#ifdef HAVE_PTRACE_GETFPXREGS
#define HAVE_SSE_REGS
#endif

#include "i386/tm-i386.h"
#include "tm-linux.h"

/* Register number for the "orig_eax" pseudo-register.  If this
   pseudo-register contains a value >= 0 it is interpreted as the
   system call number that the kernel is supposed to restart.  */
#define I386_LINUX_ORIG_EAX_REGNUM (NUM_GREGS + NUM_FREGS + NUM_SSE_REGS)

/* Adjust a few macros to deal with this extra register.  */

#undef NUM_REGS
#define NUM_REGS (NUM_GREGS + NUM_FREGS + NUM_SSE_REGS + 1)

#undef MAX_NUM_REGS
#define MAX_NUM_REGS (16 + 16 + 9 + 1)

#undef REGISTER_BYTES
#define REGISTER_BYTES \
  (SIZEOF_GREGS + SIZEOF_FPU_REGS + SIZEOF_FPU_CTRL_REGS + SIZEOF_SSE_REGS + 4)

#undef REGISTER_NAME
#define REGISTER_NAME(reg) i386_linux_register_name ((reg))
extern char *i386_linux_register_name (int reg);

#undef REGISTER_BYTE
#define REGISTER_BYTE(reg) i386_linux_register_byte ((reg))
extern int i386_linux_register_byte (int reg);

#undef REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(reg) i386_linux_register_raw_size ((reg))
extern int i386_linux_register_raw_size (int reg);

/* GNU/Linux ELF uses stabs-in-ELF with the DWARF register numbering
   scheme by default, so we must redefine STAB_REG_TO_REGNUM.  This
   messes up the floating-point registers for a.out, but there is not
   much we can do about that.  */
#undef STAB_REG_TO_REGNUM
#define STAB_REG_TO_REGNUM(reg) i386_dwarf_reg_to_regnum ((reg))

/* Use target_specific function to define link map offsets.  */
extern struct link_map_offsets *i386_linux_svr4_fetch_link_map_offsets (void);
#define SVR4_FETCH_LINK_MAP_OFFSETS() i386_linux_svr4_fetch_link_map_offsets ()

/* The following works around a problem with /usr/include/sys/procfs.h  */
#define sys_quotactl 1

/* When the i386 Linux kernel calls a signal handler, the return
   address points to a bit of code on the stack.  These definitions
   are used to identify this bit of code as a signal trampoline in
   order to support backtracing through calls to signal handlers.  */

#define IN_SIGTRAMP(pc, name) i386_linux_in_sigtramp (pc, name)
extern int i386_linux_in_sigtramp (CORE_ADDR, char *);

#undef FRAME_CHAIN
#define FRAME_CHAIN(frame) i386_linux_frame_chain (frame)
extern CORE_ADDR i386_linux_frame_chain (struct frame_info *frame);

#undef FRAME_SAVED_PC
#define FRAME_SAVED_PC(frame) i386_linux_frame_saved_pc (frame)
extern CORE_ADDR i386_linux_frame_saved_pc (struct frame_info *frame);

#undef SAVED_PC_AFTER_CALL
#define SAVED_PC_AFTER_CALL(frame) i386_linux_saved_pc_after_call (frame)
extern CORE_ADDR i386_linux_saved_pc_after_call (struct frame_info *);

#define TARGET_WRITE_PC(pc, ptid) i386_linux_write_pc (pc, ptid)
extern void i386_linux_write_pc (CORE_ADDR pc, ptid_t ptid);

/* When we call a function in a shared library, and the PLT sends us
   into the dynamic linker to find the function's real address, we
   need to skip over the dynamic linker call.  This function decides
   when to skip, and where to skip to.  See the comments for
   SKIP_SOLIB_RESOLVER at the top of infrun.c.  */
#define SKIP_SOLIB_RESOLVER i386_linux_skip_solib_resolver
extern CORE_ADDR i386_linux_skip_solib_resolver (CORE_ADDR pc);

/* N_FUN symbols in shared libaries have 0 for their values and need
   to be relocated. */
#define SOFUN_ADDRESS_MAYBE_MISSING


/* Support for longjmp.  */

/* Details about jmp_buf.  It's supposed to be an array of integers.  */

#define JB_ELEMENT_SIZE 4	/* Size of elements in jmp_buf.  */
#define JB_PC		5	/* Array index of saved PC.  */

/* Figure out where the longjmp will land.  Slurp the args out of the
   stack.  We expect the first arg to be a pointer to the jmp_buf
   structure from which we extract the pc (JB_PC) that we will land
   at.  The pc is copied into ADDR.  This routine returns true on
   success.  */

#define GET_LONGJMP_TARGET(addr) get_longjmp_target (addr)
extern int get_longjmp_target (CORE_ADDR *addr);

#endif /* #ifndef TM_LINUX_H */
