/* Target-dependent code for i386 BSD's.
   Copyright 2001 Free Software Foundation, Inc.

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

#include "defs.h"
#include "frame.h"
#include "gdbcore.h"
#include "regcache.h"

/* Support for signal handlers.  */

/* Range in which to find the signaltramp routine, traditionally found
   on the use stack, just below the user area.  Initialized to values
   that work for NetBSD and FreeBSD.  */

CORE_ADDR i386bsd_sigtramp_start = 0xbfbfdf20;
CORE_ADDR i386bsd_sigtramp_end = 0xbfbfdff0;

/* Return whether PC is in a BSD sigtramp routine.  */

int
i386bsd_in_sigtramp (CORE_ADDR pc, char *name)
{
  return (pc >= i386bsd_sigtramp_start && pc < i386bsd_sigtramp_end);
}

/* Offset in the sigcontext structure of the program counter.
   Initialized to the value from 4.4 BSD Lite.  */
int i386bsd_sigcontext_pc_offset = 20;

/* Assuming FRAME is for a BSD sigtramp routine, return the address of
   the associated sigcontext structure.  */

static CORE_ADDR
i386bsd_sigcontext_addr (struct frame_info *frame)
{
  if (frame->next)
    /* If this isn't the top frame, the next frame must be for the
       signal handler itself.  A pointer to the sigcontext structure
       is passed as the third argument to the signal handler.  */
    return read_memory_unsigned_integer (frame->next->frame + 16, 4);

  /* This is the top frame.  We'll have to find the address of the
     sigcontext structure by looking at the stack pointer.  */
  return read_memory_unsigned_integer (read_register (SP_REGNUM) + 8, 4);
}

/* Assuming FRAME is for a BSD sigtramp routine, return the saved
   program counter.  */

static CORE_ADDR
i386bsd_sigtramp_saved_pc (struct frame_info *frame)
{
  CORE_ADDR addr;
  addr = i386bsd_sigcontext_addr (frame);
  return read_memory_unsigned_integer (addr + i386bsd_sigcontext_pc_offset, 4);
}

/* Return the saved program counter for FRAME.  */

CORE_ADDR
i386bsd_frame_saved_pc (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return i386bsd_sigtramp_saved_pc (frame);

  return read_memory_unsigned_integer (frame->frame + 4, 4);
}
