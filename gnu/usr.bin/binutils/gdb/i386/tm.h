/* $FreeBSD$ */
/* Target macro definitions for i386 running FreeBSD
   Copyright (C) 1997 Free Software Foundation, Inc.

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

#ifndef TM_FBSD_H
#define TM_FBSD_H 1

#include "i386/tm-i386bsd.h"

#undef NUM_REGS
#define NUM_REGS 16

extern struct frame_info *setup_arbitrary_frame PARAMS ((int, CORE_ADDR *));

#define	SETUP_ARBITRARY_FRAME(argc, argv) setup_arbitrary_frame (argc, argv)

extern void i386_float_info PARAMS ((void));

#define FLOAT_INFO i386_float_info ()

#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) STREQ (name, "_DYNAMIC")

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */

extern CORE_ADDR fbsd_kern_frame_saved_pc (struct frame_info *);
#undef FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) \
  (kernel_debugging ? fbsd_kern_frame_saved_pc(FRAME) : \
  (((FRAME)->signal_handler_caller \
    ? fbsd_sigtramp_saved_pc (FRAME) \
    : read_memory_integer ((FRAME)->frame + 4, 4)) \
   ))

/* On FreeBSD, sigtramp has size 0x48 and is immediately below the
   ps_strings struct which has size 0x10 and is at the top of the
   user stack.  */

#undef SIGTRAMP_START
#undef SIGTRAMP_END
#define SIGTRAMP_START(pc)	0xbfbfffa8
#define SIGTRAMP_END(pc)	0xbfbffff0
#undef  SIGCONTEXT_PC_OFFSET
#define OSIGCONTEXT_PC_OFFSET 20
#define NSIGCONTEXT_PC_OFFSET 76
#define OSIGCODE_MAGIC_OFFSET 20

struct objfile;
void freebsd_uthread_new_objfile PARAMS ((struct objfile *objfile));
#define target_new_objfile(OBJFILE) freebsd_uthread_new_objfile (OBJFILE)

extern char *freebsd_uthread_pid_to_str PARAMS ((int pid));
#define target_pid_to_str(PID) freebsd_uthread_pid_to_str (PID)

#endif  /* ifndef TM_FBSD_H */
