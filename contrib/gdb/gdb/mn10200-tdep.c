/* Target-dependent code for the Matsushita MN10200 for GDB, the GNU debugger.
   Copyright 1997 Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "obstack.h"
#include "target.h"
#include "value.h"
#include "bfd.h"
#include "gdb_string.h"
#include "gdbcore.h"
#include "symfile.h"

  	
/* Should call_function allocate stack space for a struct return?  */
int
mn10200_use_struct_convention (gcc_p, type)
     int gcc_p;
     struct type *type;
{
  return (TYPE_NFIELDS (type) > 1 || TYPE_LENGTH (type) > 8);
}



/* The main purpose of this file is dealing with prologues to extract
   information about stack frames and saved registers.

   For reference here's how prologues look on the mn10200:

     With frame pointer:
	mov fp,a0
	mov sp,fp
	add <size>,sp
	Register saves for d2, d3, a1, a2 as needed.  Saves start
	at fp - <size> + <outgoing_args_size> and work towards higher
	addresses.  Note that the saves are actually done off the stack
	pointer in the prologue!  This makes for smaller code and easier
	prologue scanning as the displacement fields will unlikely
        be more than 8 bits!

     Without frame pointer:
        add <size>,sp
	Register saves for d2, d3, a1, a2 as needed.  Saves start
	at sp + <outgoing_args_size> and work towards higher addresses.

     Out of line prologue:
	add <local size>,sp  -- optional
	jsr __prologue
	add <outgoing_size>,sp -- optional

   The stack pointer remains constant throughout the life of most
   functions.  As a result the compiler will usually omit the
   frame pointer, so we must handle frame pointerless functions.  */

/* Analyze the prologue to determine where registers are saved,
   the end of the prologue, etc etc.  Return the end of the prologue
   scanned.

   We store into FI (if non-null) several tidbits of information:

    * stack_size -- size of this stack frame.  Note that if we stop in
    certain parts of the prologue/epilogue we may claim the size of the
    current frame is zero.  This happens when the current frame has
    not been allocated yet or has already been deallocated.

    * fsr -- Addresses of registers saved in the stack by this frame.

    * status -- A (relatively) generic status indicator.  It's a bitmask
    with the following bits: 

      MY_FRAME_IN_SP: The base of the current frame is actually in
      the stack pointer.  This can happen for frame pointerless
      functions, or cases where we're stopped in the prologue/epilogue
      itself.  For these cases mn10200_analyze_prologue will need up
      update fi->frame before returning or analyzing the register
      save instructions.

      MY_FRAME_IN_FP: The base of the current frame is in the
      frame pointer register ($a2).

      CALLER_A2_IN_A0: $a2 from the caller's frame is temporarily
      in $a0.  This can happen if we're stopped in the prologue.

      NO_MORE_FRAMES: Set this if the current frame is "start" or
      if the first instruction looks like mov <imm>,sp.  This tells
      frame chain to not bother trying to unwind past this frame.  */

#define MY_FRAME_IN_SP 0x1
#define MY_FRAME_IN_FP 0x2
#define CALLER_A2_IN_A0 0x4
#define NO_MORE_FRAMES 0x8
 
static CORE_ADDR
mn10200_analyze_prologue (fi, pc)
    struct frame_info *fi;
    CORE_ADDR pc;
{
  CORE_ADDR func_addr, func_end, addr, stop;
  CORE_ADDR stack_size;
  unsigned char buf[4];
  int status;
  char *name;
  int out_of_line_prologue = 0;

  /* Use the PC in the frame if it's provided to look up the
     start of this function.  */
  pc = (fi ? fi->pc : pc);

  /* Find the start of this function.  */
  status = find_pc_partial_function (pc, &name, &func_addr, &func_end);

  /* Do nothing if we couldn't find the start of this function or if we're
     stopped at the first instruction in the prologue.  */
  if (status == 0)
    return pc;

  /* If we're in start, then give up.  */
  if (strcmp (name, "start") == 0)
    {
      if (fi)
        fi->status = NO_MORE_FRAMES;
      return pc;
    }

  /* At the start of a function our frame is in the stack pointer.  */
  if (fi)
    fi->status = MY_FRAME_IN_SP;

  /* If we're physically on an RTS instruction, then our frame has already
     been deallocated.

     fi->frame is bogus, we need to fix it.  */
  if (fi && fi->pc + 1 == func_end)
    {
      status = target_read_memory (fi->pc, buf, 1);
      if (status != 0)
	{
	  if (fi->next == NULL)
	    fi->frame = read_sp ();
	  return fi->pc;
	}

      if (buf[0] == 0xfe)
	{
	  if (fi->next == NULL)
	    fi->frame = read_sp ();
	  return fi->pc;
	}
    }

  /* Similarly if we're stopped on the first insn of a prologue as our
     frame hasn't been allocated yet.  */
  if (fi && fi->pc == func_addr)
    {
      if (fi->next == NULL)
	fi->frame = read_sp ();
      return fi->pc;
    }

  /* Figure out where to stop scanning.  */
  stop = fi ? fi->pc : func_end;

  /* Don't walk off the end of the function.  */
  stop = stop > func_end ? func_end : stop;

  /* Start scanning on the first instruction of this function.  */
  addr = func_addr;

  status = target_read_memory (addr, buf, 2);
  if (status != 0)
    {
      if (fi && fi->next == NULL && fi->status & MY_FRAME_IN_SP)
	fi->frame = read_sp ();
      return addr;
    }

  /* First see if this insn sets the stack pointer; if so, it's something
     we won't understand, so quit now.   */
  if (buf[0] == 0xdf
      || (buf[0] == 0xf4 && buf[1] == 0x77))
    {
      if (fi)
	fi->status = NO_MORE_FRAMES;
      return addr;
    }

  /* Now see if we have a frame pointer.
       
     Search for mov a2,a0 (0xf278)
        then	mov a3,a2 (0xf27e).  */

  if (buf[0] == 0xf2 && buf[1] == 0x78)
    {
      /* Our caller's $a2 will be found in $a0 now.  Note it for
	 our callers.  */
      if (fi)
	fi->status |= CALLER_A2_IN_A0;
      addr += 2;
      if (addr >= stop)
	{
	  /* We still haven't allocated our local stack.  Handle this
	     as if we stopped on the first or last insn of a function.   */
	  if (fi && fi->next == NULL)
	    fi->frame = read_sp ();
	  return addr;
	}

      status = target_read_memory (addr, buf, 2);
      if (status != 0)
	{
	  if (fi && fi->next == NULL)
	    fi->frame = read_sp ();
	  return addr;
	}
      if (buf[0] == 0xf2 && buf[1] == 0x7e)
	{
	  addr += 2;

	  /* Our frame pointer is valid now.  */
	  if (fi)
	    {
	      fi->status |= MY_FRAME_IN_FP;
	      fi->status &= ~MY_FRAME_IN_SP;
	    }
	  if (addr >= stop)
	    return addr;
	}
      else
	{
	  if (fi && fi->next == NULL)
	    fi->frame = read_sp ();
	  return addr;
	}
    }

  /* Next we should allocate the local frame.
       
     Search for add imm8,a3 (0xd3XX)
        or	add imm16,a3 (0xf70bXXXX)
        or	add imm24,a3 (0xf467XXXXXX).
       
     If none of the above was found, then this prologue has
     no stack, and therefore can't have any register saves,
     so quit now.  */
  status = target_read_memory (addr, buf, 2);
  if (status != 0)
    {
      if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	fi->frame = read_sp ();
      return addr;
    }
  if (buf[0] == 0xd3)
    {
      stack_size = extract_signed_integer (&buf[1], 1);
      if (fi)
	fi->stack_size = stack_size;
      addr += 2;
      if (addr >= stop)
	{
	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	    fi->frame = read_sp () - stack_size;
	  return addr;
	}
    }
  else if (buf[0] == 0xf7 && buf[1] == 0x0b)
    {
      status = target_read_memory (addr + 2, buf, 2);
      if (status != 0)
	{
	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	    fi->frame = read_sp ();
	  return addr;
	}
      stack_size = extract_signed_integer (buf, 2);
      if (fi)
	fi->stack_size = stack_size;
      addr += 4;
      if (addr >= stop)
	{
	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	    fi->frame = read_sp () - stack_size;
	  return addr;
	}
    }
  else if (buf[0] == 0xf4 && buf[1] == 0x67)
    {
      status = target_read_memory (addr + 2, buf, 3);
      if (status != 0)
	{
	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	    fi->frame = read_sp ();
	  return addr;
	}
      stack_size = extract_signed_integer (buf, 3);
      if (fi)
	fi->stack_size = stack_size;
      addr += 5;
      if (addr >= stop)
	{
	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	    fi->frame = read_sp () - stack_size;
	  return addr;
	}
    }

  /* Now see if we have a call to __prologue for an out of line
     prologue.  */
  status = target_read_memory (addr, buf, 2);
  if (status != 0)
    return addr;

  /* First check for 16bit pc-relative call to __prologue.  */
  if (buf[0] == 0xfd)
    {
      CORE_ADDR temp;
      status = target_read_memory (addr + 1, buf, 2);
      if (status != 0)
	{
  	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
    	    fi->frame = read_sp ();
	  return addr;
	}
      
      /* Get the PC this instruction will branch to.  */
      temp = (extract_signed_integer (buf, 2) + addr + 3) & 0xffffff;

      /* Get the name of the function at the target address.  */
      status = find_pc_partial_function (temp, &name, NULL, NULL);
      if (status == 0)
	{
  	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
    	    fi->frame = read_sp ();
	  return addr;
	}

      /* Note if it is an out of line prologue.  */
      out_of_line_prologue = (strcmp (name, "__prologue") == 0);

      /* This sucks up 3 bytes of instruction space.  */
      if (out_of_line_prologue)
	addr += 3;

      if (addr >= stop)
	{
	  if (fi && fi->next == NULL)
	    {
	      fi->stack_size -= 16;
	      fi->frame = read_sp () - fi->stack_size;
	    }
	  return addr;
	}
    }
  /* Now check for the 24bit pc-relative call to __prologue.  */
  else if (buf[0] == 0xf4 && buf[1] == 0xe1)
    {
      CORE_ADDR temp;
      status = target_read_memory (addr + 2, buf, 3);
      if (status != 0)
	{
  	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
    	    fi->frame = read_sp ();
	  return addr;
	}
      
      /* Get the PC this instruction will branch to.  */
      temp = (extract_signed_integer (buf, 3) + addr + 5) & 0xffffff;

      /* Get the name of the function at the target address.  */
      status = find_pc_partial_function (temp, &name, NULL, NULL);
      if (status == 0)
	{
  	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
    	    fi->frame = read_sp ();
	  return addr;
	}

      /* Note if it is an out of line prologue.  */
      out_of_line_prologue = (strcmp (name, "__prologue") == 0);

      /* This sucks up 5 bytes of instruction space.  */
      if (out_of_line_prologue)
	addr += 5;

      if (addr >= stop)
	{
	  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP))
	    {
	      fi->stack_size -= 16;
	      fi->frame = read_sp () - fi->stack_size;
	    }
	  return addr;
	}
    }

  /* Now actually handle the out of line prologue.  */
  if (out_of_line_prologue)
    {
      int outgoing_args_size = 0;

      /* First adjust the stack size for this function.  The out of
	 line prologue saves 4 registers (16bytes of data).  */
      if (fi)
	fi->stack_size -= 16;

      /* Update fi->frame if necessary.  */
      if (fi && fi->next == NULL)
	fi->frame = read_sp () - fi->stack_size;

      /* After the out of line prologue, there may be another
	 stack adjustment for the outgoing arguments.

	 Search for add imm8,a3 (0xd3XX)
	    or	add imm16,a3 (0xf70bXXXX)
	    or	add imm24,a3 (0xf467XXXXXX).  */
       
      status = target_read_memory (addr, buf, 2);
      if (status != 0)
	{
	  if (fi)
	    {
	      fi->fsr.regs[2] = fi->frame + fi->stack_size + 4;
	      fi->fsr.regs[3] = fi->frame + fi->stack_size + 8;
	      fi->fsr.regs[5] = fi->frame + fi->stack_size + 12;
	      fi->fsr.regs[6] = fi->frame + fi->stack_size + 16;
	    }
	  return addr;
	}

      if (buf[0] == 0xd3)
	{
	  outgoing_args_size = extract_signed_integer (&buf[1], 1);
	  addr += 2;
	}
      else if (buf[0] == 0xf7 && buf[1] == 0x0b)
	{
	  status = target_read_memory (addr + 2, buf, 2);
	  if (status != 0)
	    {
	      if (fi)
		{
		  fi->fsr.regs[2] = fi->frame + fi->stack_size + 4;
		  fi->fsr.regs[3] = fi->frame + fi->stack_size + 8;
		  fi->fsr.regs[5] = fi->frame + fi->stack_size + 12;
		  fi->fsr.regs[6] = fi->frame + fi->stack_size + 16;
		}
	      return addr;
	    }
	  outgoing_args_size = extract_signed_integer (buf, 2);
	  addr += 4;
	}
      else if (buf[0] == 0xf4 && buf[1] == 0x67)
	{
	  status = target_read_memory (addr + 2, buf, 3);
	  if (status != 0)
	    {
	      if (fi && fi->next == NULL)
		{
		  fi->fsr.regs[2] = fi->frame + fi->stack_size + 4;
		  fi->fsr.regs[3] = fi->frame + fi->stack_size + 8;
		  fi->fsr.regs[5] = fi->frame + fi->stack_size + 12;
		  fi->fsr.regs[6] = fi->frame + fi->stack_size + 16;
		}
	      return addr;
	    }
	  outgoing_args_size = extract_signed_integer (buf, 3);
	  addr += 5;
	}
      else
	outgoing_args_size = 0;

      /* Now that we know the size of the outgoing arguments, fix
	 fi->frame again if this is the innermost frame.  */
      if (fi && fi->next == NULL)
	fi->frame -= outgoing_args_size;

      /* Note the register save information and update the stack
	 size for this frame too.  */
      if (fi)
	{
	  fi->fsr.regs[2] = fi->frame + fi->stack_size + 4;
	  fi->fsr.regs[3] = fi->frame + fi->stack_size + 8;
	  fi->fsr.regs[5] = fi->frame + fi->stack_size + 12;
	  fi->fsr.regs[6] = fi->frame + fi->stack_size + 16;
	  fi->stack_size += outgoing_args_size;
	}
      /* There can be no more prologue insns, so return now.  */
      return addr;
    }

  /* At this point fi->frame needs to be correct.

     If MY_FRAME_IN_SP is set and we're the innermost frame, then we
     need to fix fi->frame so that backtracing, find_frame_saved_regs,
     etc work correctly.  */
  if (fi && fi->next == NULL && (fi->status & MY_FRAME_IN_SP) != 0)
    fi->frame = read_sp () - fi->stack_size;

  /* And last we have the register saves.  These are relatively
     simple because they're physically done off the stack pointer,
     and thus the number of different instructions we need to
     check is greatly reduced because we know the displacements
     will be small.
       
     Search for movx d2,(X,a3) (0xf55eXX)
        then	movx d3,(X,a3) (0xf55fXX)
        then	mov  a1,(X,a3) (0x5dXX)	   No frame pointer case
        then	mov  a2,(X,a3) (0x5eXX)	   No frame pointer case
        or  mov  a0,(X,a3) (0x5cXX)	   Frame pointer case.  */

  status = target_read_memory (addr, buf, 2);
  if (status != 0)
    return addr;
  if (buf[0] == 0xf5 && buf[1] == 0x5e)
    {
      if (fi)
	{
	  status = target_read_memory (addr + 2, buf, 1);
	  if (status != 0)
	    return addr;
	  fi->fsr.regs[2] = (fi->frame + stack_size
			     + extract_signed_integer (buf, 1));
	}
      addr += 3;
      if (addr >= stop)
	return addr;
      status = target_read_memory (addr, buf, 2);
      if (status != 0)
	return addr;
    }
  if (buf[0] == 0xf5 && buf[1] == 0x5f)
    {
      if (fi)
	{
	  status = target_read_memory (addr + 2, buf, 1);
	  if (status != 0)
	    return addr;
	  fi->fsr.regs[3] = (fi->frame + stack_size
			     + extract_signed_integer (buf, 1));
	}
      addr += 3;
      if (addr >= stop)
	return addr;
      status = target_read_memory (addr, buf, 2);
      if (status != 0)
	return addr;
    }
  if (buf[0] == 0x5d)
    {
      if (fi)
	{
	  status = target_read_memory (addr + 1, buf, 1);
	  if (status != 0)
	    return addr;
	  fi->fsr.regs[5] = (fi->frame + stack_size
			     + extract_signed_integer (buf, 1));
	}
      addr += 2;
      if (addr >= stop)
	return addr;
      status = target_read_memory (addr, buf, 2);
      if (status != 0)
	return addr;
    }
  if (buf[0] == 0x5e || buf[0] == 0x5c)
    {
      if (fi)
	{
	  status = target_read_memory (addr + 1, buf, 1);
	  if (status != 0)
	    return addr;
	  fi->fsr.regs[6] = (fi->frame + stack_size
			     + extract_signed_integer (buf, 1));
	  fi->status &= ~CALLER_A2_IN_A0;
	}
      addr += 2;
      if (addr >= stop)
	return addr;
      return addr;
    }
  return addr;
}
  
/* Function: frame_chain
   Figure out and return the caller's frame pointer given current
   frame_info struct.

   We don't handle dummy frames yet but we would probably just return the
   stack pointer that was in use at the time the function call was made?  */

CORE_ADDR
mn10200_frame_chain (fi)
     struct frame_info *fi;
{
  struct frame_info dummy_frame;

  /* Walk through the prologue to determine the stack size,
     location of saved registers, end of the prologue, etc.  */
  if (fi->status == 0)
    mn10200_analyze_prologue (fi, (CORE_ADDR)0);

  /* Quit now if mn10200_analyze_prologue set NO_MORE_FRAMES.  */
  if (fi->status & NO_MORE_FRAMES)
    return 0;

  /* Now that we've analyzed our prologue, determine the frame
     pointer for our caller.

       If our caller has a frame pointer, then we need to
       find the entry value of $a2 to our function.

	 If CALLER_A2_IN_A0, then the chain is in $a0.

	 If fsr.regs[6] is nonzero, then it's at the memory
	 location pointed to by fsr.regs[6].

	 Else it's still in $a2.

       If our caller does not have a frame pointer, then his
       frame base is fi->frame + -caller's stack size + 4.  */
       
  /* The easiest way to get that info is to analyze our caller's frame.

     So we set up a dummy frame and call mn10200_analyze_prologue to
     find stuff for us.  */
  dummy_frame.pc = FRAME_SAVED_PC (fi);
  dummy_frame.frame = fi->frame;
  memset (dummy_frame.fsr.regs, '\000', sizeof dummy_frame.fsr.regs);
  dummy_frame.status = 0;
  dummy_frame.stack_size = 0;
  mn10200_analyze_prologue (&dummy_frame);

  if (dummy_frame.status & MY_FRAME_IN_FP)
    {
      /* Our caller has a frame pointer.  So find the frame in $a2, $a0,
	 or in the stack.  */
      if (fi->fsr.regs[6])
	return (read_memory_integer (fi->fsr.regs[FP_REGNUM], REGISTER_SIZE)
		& 0xffffff);
      else if (fi->status & CALLER_A2_IN_A0)
	return read_register (4);
      else
	return read_register (FP_REGNUM);
    }
  else
    {
      /* Our caller does not have a frame pointer.  So his frame starts
	 at the base of our frame (fi->frame) + <his size> + 4 (saved pc).  */
      return fi->frame + -dummy_frame.stack_size + 4;
    }
}

/* Function: skip_prologue
   Return the address of the first inst past the prologue of the function.  */

CORE_ADDR
mn10200_skip_prologue (pc)
     CORE_ADDR pc;
{
  /* We used to check the debug symbols, but that can lose if
     we have a null prologue.  */
  return mn10200_analyze_prologue (NULL, pc);
}

/* Function: pop_frame
   This routine gets called when either the user uses the `return'
   command, or the call dummy breakpoint gets hit.  */

void
mn10200_pop_frame (frame)
     struct frame_info *frame;
{
  int regnum;

  if (PC_IN_CALL_DUMMY(frame->pc, frame->frame, frame->frame))
    generic_pop_dummy_frame ();
  else
    {
      write_register (PC_REGNUM, FRAME_SAVED_PC (frame));

      /* Restore any saved registers.  */
      for (regnum = 0; regnum < NUM_REGS; regnum++)
	if (frame->fsr.regs[regnum] != 0)
	  {
	    ULONGEST value;

	    value = read_memory_unsigned_integer (frame->fsr.regs[regnum],
						  REGISTER_RAW_SIZE (regnum));
	    write_register (regnum, value);
	  }

      /* Actually cut back the stack.  */
      write_register (SP_REGNUM, FRAME_FP (frame));

      /* Don't we need to set the PC?!?  XXX FIXME.  */
    }

  /* Throw away any cached frame information.  */
  flush_cached_frames ();
}

/* Function: push_arguments
   Setup arguments for a call to the target.  Arguments go in
   order on the stack.  */

CORE_ADDR
mn10200_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     unsigned char struct_return;
     CORE_ADDR struct_addr;
{
  int argnum = 0;
  int len = 0;
  int stack_offset = 0;
  int regsused = struct_return ? 1 : 0;

  /* This should be a nop, but align the stack just in case something
     went wrong.  Stacks are two byte aligned on the mn10200.  */
  sp &= ~1;

  /* Now make space on the stack for the args.

     XXX This doesn't appear to handle pass-by-invisible reference
     arguments.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int arg_length = (TYPE_LENGTH (VALUE_TYPE (args[argnum])) + 1) & ~1;

      /* If we've used all argument registers, then this argument is
	 pushed.  */
      if (regsused >= 2 || arg_length > 4)
	{
	  regsused = 2;
	  len += arg_length;
	}
      /* We know we've got some arg register space left.  If this argument
	 will fit entirely in regs, then put it there.  */
      else if (arg_length <= 2
	       || TYPE_CODE (VALUE_TYPE (args[argnum])) == TYPE_CODE_PTR) 
	{
	  regsused++;
	}
      else if (regsused == 0)
	{
	  regsused = 2;
	}
      else
	{
	  regsused = 2;
	  len += arg_length;
	}
    }

  /* Allocate stack space.  */
  sp -= len;

  regsused = struct_return ? 1 : 0;
  /* Push all arguments onto the stack. */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      char *val;

      /* XXX Check this.  What about UNIONS?  */
      if (TYPE_CODE (VALUE_TYPE (*args)) == TYPE_CODE_STRUCT
	  && TYPE_LENGTH (VALUE_TYPE (*args)) > 8)
	{
	  /* XXX Wrong, we want a pointer to this argument.  */
          len = TYPE_LENGTH (VALUE_TYPE (*args));
          val = (char *)VALUE_CONTENTS (*args);
	}
      else
	{
	  len = TYPE_LENGTH (VALUE_TYPE (*args));
	  val = (char *)VALUE_CONTENTS (*args);
	}

      if (regsused < 2
	  && (len <= 2
	      || TYPE_CODE (VALUE_TYPE (*args)) == TYPE_CODE_PTR))
	{
	  write_register (regsused, extract_unsigned_integer (val, 4));
	  regsused++;
	}
      else if (regsused == 0 && len == 4)
	{
	  write_register (regsused, extract_unsigned_integer (val, 2));
	  write_register (regsused + 1, extract_unsigned_integer (val + 2, 2));
	  regsused = 2;
	}
      else
	{
	  regsused = 2;
	  while (len > 0)
	    {
	      write_memory (sp + stack_offset, val, 2);

	      len -= 2;
	      val += 2;
	      stack_offset += 2;
	    }
	}
      args++;
    }

  return sp;
}

/* Function: push_return_address (pc)
   Set up the return address for the inferior function call.
   Needed for targets where we don't actually execute a JSR/BSR instruction */
 
CORE_ADDR
mn10200_push_return_address (pc, sp)
     CORE_ADDR pc;
     CORE_ADDR sp;
{
  unsigned char buf[4];

  store_unsigned_integer (buf, 4, CALL_DUMMY_ADDRESS ());
  write_memory (sp - 4, buf, 4);
  return sp - 4;
}

/* Function: store_struct_return (addr,sp)
   Store the structure value return address for an inferior function
   call.  */
 
CORE_ADDR
mn10200_store_struct_return (addr, sp)
     CORE_ADDR addr;
     CORE_ADDR sp;
{
  /* The structure return address is passed as the first argument.  */
  write_register (0, addr);
  return sp;
}
 
/* Function: frame_saved_pc 
   Find the caller of this frame.  We do this by seeing if RP_REGNUM
   is saved in the stack anywhere, otherwise we get it from the
   registers.  If the inner frame is a dummy frame, return its PC
   instead of RP, because that's where "caller" of the dummy-frame
   will be found.  */

CORE_ADDR
mn10200_frame_saved_pc (fi)
     struct frame_info *fi;
{
  /* The saved PC will always be at the base of the current frame.  */
  return (read_memory_integer (fi->frame, REGISTER_SIZE) & 0xffffff);
}

void
get_saved_register (raw_buffer, optimized, addrp, frame, regnum, lval)
     char *raw_buffer;
     int *optimized;
     CORE_ADDR *addrp;
     struct frame_info *frame;
     int regnum;
     enum lval_type *lval;
{
  generic_get_saved_register (raw_buffer, optimized, addrp, 
			      frame, regnum, lval);
}

/* Function: init_extra_frame_info
   Setup the frame's frame pointer, pc, and frame addresses for saved
   registers.  Most of the work is done in mn10200_analyze_prologue().

   Note that when we are called for the last frame (currently active frame),
   that fi->pc and fi->frame will already be setup.  However, fi->frame will
   be valid only if this routine uses FP.  For previous frames, fi-frame will
   always be correct.  mn10200_analyze_prologue will fix fi->frame if
   it's not valid.

   We can be called with the PC in the call dummy under two circumstances.
   First, during normal backtracing, second, while figuring out the frame
   pointer just prior to calling the target function (see run_stack_dummy).  */

void
mn10200_init_extra_frame_info (fi)
     struct frame_info *fi;
{
  if (fi->next)
    fi->pc = FRAME_SAVED_PC (fi->next);

  memset (fi->fsr.regs, '\000', sizeof fi->fsr.regs);
  fi->status = 0;
  fi->stack_size = 0;

  mn10200_analyze_prologue (fi, 0);
}

void
_initialize_mn10200_tdep ()
{
  tm_print_insn = print_insn_mn10200;
}

