/* Target-dependent code for GNU/Linux running on x86-64, for GDB.

   Copyright 2001 Free Software Foundation, Inc.

   Contributed by Jiri Smid, SuSE Labs.

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
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include "x86-64-tdep.h"
#include "dwarf2cfi.h"

#define LINUX_SIGTRAMP_INSN0 (0x48)	/* mov $NNNNNNNN,%rax */
#define LINUX_SIGTRAMP_OFFSET0 (0)
#define LINUX_SIGTRAMP_INSN1 (0x0f)	/* syscall */
#define LINUX_SIGTRAMP_OFFSET1 (7)

static const unsigned char linux_sigtramp_code[] = {
  LINUX_SIGTRAMP_INSN0, 0xc7, 0xc0, 0x89, 0x00, 0x00, 0x00,	/*  mov $0x89,%rax */
  LINUX_SIGTRAMP_INSN1, 0x05	/* syscall */
};

#define LINUX_SIGTRAMP_LEN (sizeof linux_sigtramp_code)

/* If PC is in a sigtramp routine, return the address of the start of
   the routine.  Otherwise, return 0.  */

static CORE_ADDR
x86_64_linux_sigtramp_start (CORE_ADDR pc)
{
  unsigned char buf[LINUX_SIGTRAMP_LEN];
  if (read_memory_nobpt (pc, (char *) buf, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  if (buf[0] != LINUX_SIGTRAMP_INSN0)
    {
      if (buf[0] != LINUX_SIGTRAMP_INSN1)
	return 0;

      pc -= LINUX_SIGTRAMP_OFFSET1;

      if (read_memory_nobpt (pc, (char *) buf, LINUX_SIGTRAMP_LEN) != 0)
	return 0;
    }

  if (memcmp (buf, linux_sigtramp_code, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

#define LINUX_SIGINFO_SIZE 128

/* Offset to struct sigcontext in ucontext, from <asm/ucontext.h>.  */
#define LINUX_UCONTEXT_SIGCONTEXT_OFFSET (36)

/* Assuming FRAME is for a GNU/Linux sigtramp routine, return the
   address of the associated sigcontext structure.  */
CORE_ADDR
x86_64_linux_sigcontext_addr (struct frame_info *frame)
{
  CORE_ADDR pc;

  pc = x86_64_linux_sigtramp_start (frame->pc);
  if (pc)
    {
      if (frame->next)
	/* If this isn't the top frame, the next frame must be for the
	   signal handler itself.  The sigcontext structure is part of
	   the user context. */
	return frame->next->frame + LINUX_SIGINFO_SIZE +
	  LINUX_UCONTEXT_SIGCONTEXT_OFFSET;


      /* This is the top frame. */
      return read_register (SP_REGNUM) + LINUX_SIGINFO_SIZE +
	LINUX_UCONTEXT_SIGCONTEXT_OFFSET;

    }

  error ("Couldn't recognize signal trampoline.");
  return 0;
}

/* Offset to saved PC in sigcontext, from <asm/sigcontext.h>.  */
#define LINUX_SIGCONTEXT_PC_OFFSET (136)

/* Assuming FRAME is for a GNU/Linux sigtramp routine, return the
   saved program counter.  */

CORE_ADDR
x86_64_linux_sigtramp_saved_pc (struct frame_info *frame)
{
  CORE_ADDR addr;

  addr = x86_64_linux_sigcontext_addr (frame);
  return read_memory_integer (addr + LINUX_SIGCONTEXT_PC_OFFSET, 8);
}

/* Immediately after a function call, return the saved pc.  */

CORE_ADDR
x86_64_linux_saved_pc_after_call (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return x86_64_linux_sigtramp_saved_pc (frame);

  return read_memory_integer (read_register (SP_REGNUM), 8);
}

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */
CORE_ADDR
x86_64_linux_frame_saved_pc (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return x86_64_linux_sigtramp_saved_pc (frame);
  return cfi_get_ra (frame);
}
