/* Cache and manage the values of registers for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1994, 1995, 1996, 1998, 2000, 2001
   Free Software Foundation, Inc.

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
#include "target.h"
#include "value.h"
#include "inferior.h"	/* for inferior_ptid */
#include "regcache.h"

/* FIND_SAVED_REGISTER ()

   Return the address in which frame FRAME's value of register REGNUM
   has been saved in memory.  Or return zero if it has not been saved.
   If REGNUM specifies the SP, the value we return is actually
   the SP value, not an address where it was saved.  */

CORE_ADDR
find_saved_register (struct frame_info *frame, int regnum)
{
  register struct frame_info *frame1 = NULL;
  register CORE_ADDR addr = 0;

  if (frame == NULL)		/* No regs saved if want current frame */
    return 0;

#ifdef HAVE_REGISTER_WINDOWS
  /* We assume that a register in a register window will only be saved
     in one place (since the name changes and/or disappears as you go
     towards inner frames), so we only call get_frame_saved_regs on
     the current frame.  This is directly in contradiction to the
     usage below, which assumes that registers used in a frame must be
     saved in a lower (more interior) frame.  This change is a result
     of working on a register window machine; get_frame_saved_regs
     always returns the registers saved within a frame, within the
     context (register namespace) of that frame. */

  /* However, note that we don't want this to return anything if
     nothing is saved (if there's a frame inside of this one).  Also,
     callers to this routine asking for the stack pointer want the
     stack pointer saved for *this* frame; this is returned from the
     next frame.  */

  if (REGISTER_IN_WINDOW_P (regnum))
    {
      frame1 = get_next_frame (frame);
      if (!frame1)
	return 0;		/* Registers of this frame are active.  */

      /* Get the SP from the next frame in; it will be this
         current frame.  */
      if (regnum != SP_REGNUM)
	frame1 = frame;

      FRAME_INIT_SAVED_REGS (frame1);
      return frame1->saved_regs[regnum];	/* ... which might be zero */
    }
#endif /* HAVE_REGISTER_WINDOWS */

  /* Note that this next routine assumes that registers used in
     frame x will be saved only in the frame that x calls and
     frames interior to it.  This is not true on the sparc, but the
     above macro takes care of it, so we should be all right. */
  while (1)
    {
      QUIT;
      frame1 = get_prev_frame (frame1);
      if (frame1 == 0 || frame1 == frame)
	break;
      FRAME_INIT_SAVED_REGS (frame1);
      if (frame1->saved_regs[regnum])
	addr = frame1->saved_regs[regnum];
    }

  return addr;
}

/* DEFAULT_GET_SAVED_REGISTER ()

   Find register number REGNUM relative to FRAME and put its (raw,
   target format) contents in *RAW_BUFFER.  Set *OPTIMIZED if the
   variable was optimized out (and thus can't be fetched).  Set *LVAL
   to lval_memory, lval_register, or not_lval, depending on whether
   the value was fetched from memory, from a register, or in a strange
   and non-modifiable way (e.g. a frame pointer which was calculated
   rather than fetched).  Set *ADDRP to the address, either in memory
   on as a REGISTER_BYTE offset into the registers array.

   Note that this implementation never sets *LVAL to not_lval.  But
   it can be replaced by defining GET_SAVED_REGISTER and supplying
   your own.

   The argument RAW_BUFFER must point to aligned memory.  */

static void
default_get_saved_register (char *raw_buffer,
			    int *optimized,
			    CORE_ADDR *addrp,
			    struct frame_info *frame,
			    int regnum,
			    enum lval_type *lval)
{
  CORE_ADDR addr;

  if (!target_has_registers)
    error ("No registers.");

  /* Normal systems don't optimize out things with register numbers.  */
  if (optimized != NULL)
    *optimized = 0;
  addr = find_saved_register (frame, regnum);
  if (addr != 0)
    {
      if (lval != NULL)
	*lval = lval_memory;
      if (regnum == SP_REGNUM)
	{
	  if (raw_buffer != NULL)
	    {
	      /* Put it back in target format.  */
	      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum),
			     (LONGEST) addr);
	    }
	  if (addrp != NULL)
	    *addrp = 0;
	  return;
	}
      if (raw_buffer != NULL)
	target_read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
    }
  else
    {
      if (lval != NULL)
	*lval = lval_register;
      addr = REGISTER_BYTE (regnum);
      if (raw_buffer != NULL)
	read_register_gen (regnum, raw_buffer);
    }
  if (addrp != NULL)
    *addrp = addr;
}

#if !defined (GET_SAVED_REGISTER)
#define GET_SAVED_REGISTER(raw_buffer, optimized, addrp, frame, regnum, lval) \
  default_get_saved_register(raw_buffer, optimized, addrp, frame, regnum, lval)
#endif

void
get_saved_register (char *raw_buffer,
		    int *optimized,
		    CORE_ADDR *addrp,
		    struct frame_info *frame,
		    int regnum,
		    enum lval_type *lval)
{
  GET_SAVED_REGISTER (raw_buffer, optimized, addrp, frame, regnum, lval);
}

/* READ_RELATIVE_REGISTER_RAW_BYTES_FOR_FRAME

   Copy the bytes of register REGNUM, relative to the input stack frame,
   into our memory at MYADDR, in target byte order.
   The number of bytes copied is REGISTER_RAW_SIZE (REGNUM).

   Returns 1 if could not be read, 0 if could.  */

/* FIXME: This function increases the confusion between FP_REGNUM
   and the virtual/pseudo-frame pointer.  */

static int
read_relative_register_raw_bytes_for_frame (int regnum,
					    char *myaddr,
					    struct frame_info *frame)
{
  int optim;
  if (regnum == FP_REGNUM && frame)
    {
      /* Put it back in target format. */
      store_address (myaddr, REGISTER_RAW_SIZE (FP_REGNUM),
		     (LONGEST) FRAME_FP (frame));

      return 0;
    }

  get_saved_register (myaddr, &optim, (CORE_ADDR *) NULL, frame,
		      regnum, (enum lval_type *) NULL);

  if (register_cached (regnum) < 0)
    return 1;			/* register value not available */

  return optim;
}

/* READ_RELATIVE_REGISTER_RAW_BYTES

   Copy the bytes of register REGNUM, relative to the current stack
   frame, into our memory at MYADDR, in target byte order.  
   The number of bytes copied is REGISTER_RAW_SIZE (REGNUM).

   Returns 1 if could not be read, 0 if could.  */

int
read_relative_register_raw_bytes (int regnum, char *myaddr)
{
  return read_relative_register_raw_bytes_for_frame (regnum, myaddr,
						     selected_frame);
}
