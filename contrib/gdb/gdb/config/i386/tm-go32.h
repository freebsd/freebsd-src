/* Target-dependent definitions for Intel x86 running DJGPP.
   Copyright 1995, 1996, 1997, 1999, 2000 Free Software Foundation, Inc.

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

#ifndef TM_GO32_H
#define TM_GO32_H

#undef HAVE_SSE_REGS	/* FIXME! go32-nat.c needs to support XMMi registers */
#define HAVE_I387_REGS

#include "i386/tm-i386.h"

/* FRAME_CHAIN takes a frame's nominal address and produces the frame's
   chain-pointer.
   In the case of the i386, the frame's nominal address
   is the address of a 4-byte word containing the calling frame's address.
   DJGPP doesn't have any special frames for signal handlers, they are
   just normal C functions. */
#undef  FRAME_CHAIN
#define FRAME_CHAIN(thisframe) \
  (!inside_entry_file ((thisframe)->pc) ? \
   read_memory_integer ((thisframe)->frame, 4) :\
   0)

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#undef  FRAMELESS_FUNCTION_INVOCATION
#define FRAMELESS_FUNCTION_INVOCATION(FI) \
     (frameless_look_for_prologue(FI))

extern CORE_ADDR i386go32_frame_saved_pc (struct frame_info *frame);
#undef  FRAME_SAVED_PC
#define FRAME_SAVED_PC(FRAME) (i386go32_frame_saved_pc ((FRAME)))

/* Support for longjmp.  */

/* Details about jmp_buf.  It's supposed to be an array of integers.  */

#define JB_ELEMENT_SIZE 4	/* Size of elements in jmp_buf.  */
#define JB_PC		8	/* Array index of saved PC inside jmp_buf.  */

/* Figure out where the longjmp will land.  Slurp the args out of the
   stack.  We expect the first arg to be a pointer to the jmp_buf
   structure from which we extract the pc (JB_PC) that we will land
   at.  The pc is copied into ADDR.  This routine returns true on
   success.  */

#define GET_LONGJMP_TARGET(addr) get_longjmp_target (addr)
extern int get_longjmp_target (CORE_ADDR *addr);

#endif /* TM_GO32_H */
