/* Macro definitions for x86 running under FreeBSD Unix.
   Copyright 1996 Free Software Foundation, Inc.

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

/* FRAME_CHAIN takes a frame's nominal address and produces the frame's
   chain-pointer.
   In the case of the i386, the frame's nominal address
   is the address of a 4-byte word containing the calling frame's address.  */

extern CORE_ADDR fbsd_kern_frame_chain (struct frame_info *);
#undef FRAME_CHAIN
#define FRAME_CHAIN(thisframe)  \
  (kernel_debugging ? fbsd_kern_frame_chain(thisframe) : \
  ((thisframe)->signal_handler_caller \
   ? (thisframe)->frame \
   : (!inside_entry_file ((thisframe)->pc) \
      ? read_memory_integer ((thisframe)->frame, 4) \
      : 0)))

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */

extern CORE_ADDR fbsd_kern_frame_saved_pc (struct frame_info *);
#undef FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) \
  (kernel_debugging ? fbsd_kern_frame_saved_pc(FRAME) : \
  (((FRAME)->signal_handler_caller \
    ? sigtramp_saved_pc (FRAME) \
    : read_memory_integer ((FRAME)->frame + 4, 4)) \
   ))

/* On FreeBSD, sigtramp has size 0x18 and is immediately below the
   ps_strings struct which has size 0x10 and is at the top of the
   user stack.  */

#undef SIGTRAMP_START
#undef SIGTRAMP_END
#define SIGTRAMP_START	0xefbfdfd8
#define SIGTRAMP_END	0xefbfdff0
 
#endif  /* ifndef TM_FBSD_H */
