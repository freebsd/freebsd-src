/* Target-dependent code for Intel 386 running LynxOS.
   Copyright 1993 Free Software Foundation, Inc.

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

#include "defs.h"
#include "inferior.h"
#include "target.h"

/* Return the PC of the caller from the call frame.  Assumes the subr prologue
   has already been executed, and the frame pointer setup.  If this is the
   outermost frame, we check to see if we are in a system call by examining the
   previous instruction.  If so, then the return PC is actually at SP+4 because
   system calls use a different calling sequence.  */

CORE_ADDR
i386lynx_saved_pc_after_call (frame)
     struct frame_info *frame;
{
  char opcode[7];
  static const unsigned char call_inst[] = {0x9a, 0, 0, 0, 0, 8, 0}; /* lcall 0x8,0x0 */

  read_memory (frame->pc - 7, opcode, 7);
  if (memcmp (opcode, call_inst, 7) == 0)
    return read_memory_integer (read_register (SP_REGNUM) + 4, 4);

  return read_memory_integer (read_register (SP_REGNUM), 4);
}
