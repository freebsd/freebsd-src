/* Target-dependent code for the Fujitsu FR30.
   Copyright 1999, Free Software Foundation, Inc.

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
#include "inferior.h"
#include "obstack.h"
#include "target.h"
#include "value.h"
#include "bfd.h"
#include "gdb_string.h"
#include "gdbcore.h"
#include "symfile.h"

/* An expression that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  */
int
fr30_frameless_function_invocation (fi)
     struct frame_info *fi;
{
  int frameless;
  CORE_ADDR func_start, after_prologue;
  func_start = (get_pc_function_start ((fi)->pc) +
		FUNCTION_START_OFFSET);
  after_prologue = func_start;
  after_prologue = SKIP_PROLOGUE (after_prologue);
  frameless = (after_prologue == func_start);
  return frameless;
}

/* Function: pop_frame
   This routine gets called when either the user uses the `return'
   command, or the call dummy breakpoint gets hit.  */

void
fr30_pop_frame ()
{
  struct frame_info *frame = get_current_frame ();
  int regnum;
  CORE_ADDR sp = read_register (SP_REGNUM);

  if (PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame))
    generic_pop_dummy_frame ();
  else
    {
      write_register (PC_REGNUM, FRAME_SAVED_PC (frame));

      for (regnum = 0; regnum < NUM_REGS; regnum++)
	if (frame->fsr.regs[regnum] != 0)
	  {
	    write_register (regnum,
		      read_memory_unsigned_integer (frame->fsr.regs[regnum],
					       REGISTER_RAW_SIZE (regnum)));
	  }
      write_register (SP_REGNUM, sp + frame->framesize);
    }
  flush_cached_frames ();
}


/* Function: fr30_store_return_value
   Put a value where a caller expects to see it.  Used by the 'return'
   command.  */
void
fr30_store_return_value (struct type *type,
			 char *valbuf)
{
  /* Here's how the FR30 returns values (gleaned from gcc/config/
     fr30/fr30.h):

     If the return value is 32 bits long or less, it goes in r4.

     If the return value is 64 bits long or less, it goes in r4 (most
     significant word) and r5 (least significant word.

     If the function returns a structure, of any size, the caller
     passes the function an invisible first argument where the callee
     should store the value.  But GDB doesn't let you do that anyway.

     If you're returning a value smaller than a word, it's not really
     necessary to zero the upper bytes of the register; the caller is
     supposed to ignore them.  However, the FR30 typically keeps its
     values extended to the full register width, so we should emulate
     that.  */

  /* The FR30 is big-endian, so if we return a small value (like a
     short or a char), we need to position it correctly within the
     register.  We round the size up to a register boundary, and then
     adjust the offset so as to place the value at the right end.  */
  int value_size = TYPE_LENGTH (type);
  int returned_size = (value_size + FR30_REGSIZE - 1) & ~(FR30_REGSIZE - 1);
  int offset = (REGISTER_BYTE (RETVAL_REG)
		+ (returned_size - value_size));
  char *zeros = alloca (returned_size);
  memset (zeros, 0, returned_size);

  write_register_bytes (REGISTER_BYTE (RETVAL_REG), zeros, returned_size);
  write_register_bytes (offset, valbuf, value_size);
}


/* Function: skip_prologue
   Return the address of the first code past the prologue of the function.  */

CORE_ADDR
fr30_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;

  /* See what the symbol table says */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      struct symtab_and_line sal;

      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end < func_end)
	{
	  return sal.end;
	}
    }

/* Either we didn't find the start of this function (nothing we can do),
   or there's no line info, or the line after the prologue is after
   the end of the function (there probably isn't a prologue). */

  return pc;
}


/* Function: push_arguments
   Setup arguments and RP for a call to the target.  First four args
   go in FIRST_ARGREG -> LAST_ARGREG, subsequent args go on stack...
   Structs are passed by reference.  XXX not right now Z.R.
   64 bit quantities (doubles and long longs) may be split between
   the regs and the stack.
   When calling a function that returns a struct, a pointer to the struct
   is passed in as a secret first argument (always in FIRST_ARGREG).

   Stack space for the args has NOT been allocated: that job is up to us.
 */

CORE_ADDR
fr30_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  int argreg;
  int argnum;
  int stack_offset;
  struct stack_arg
    {
      char *val;
      int len;
      int offset;
    };
  struct stack_arg *stack_args =
  (struct stack_arg *) alloca (nargs * sizeof (struct stack_arg));
  int nstack_args = 0;

  argreg = FIRST_ARGREG;

  /* the struct_return pointer occupies the first parameter-passing reg */
  if (struct_return)
    write_register (argreg++, struct_addr);

  stack_offset = 0;

  /* Process args from left to right.  Store as many as allowed in
     registers, save the rest to be pushed on the stack */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      char *val;
      value_ptr arg = args[argnum];
      struct type *arg_type = check_typedef (VALUE_TYPE (arg));
      struct type *target_type = TYPE_TARGET_TYPE (arg_type);
      int len = TYPE_LENGTH (arg_type);
      enum type_code typecode = TYPE_CODE (arg_type);
      CORE_ADDR regval;
      int newarg;

      val = (char *) VALUE_CONTENTS (arg);

      {
	/* Copy the argument to general registers or the stack in
	   register-sized pieces.  Large arguments are split between
	   registers and stack.  */
	while (len > 0)
	  {
	    if (argreg <= LAST_ARGREG)
	      {
		int partial_len = len < REGISTER_SIZE ? len : REGISTER_SIZE;
		regval = extract_address (val, partial_len);

		/* It's a simple argument being passed in a general
		   register.  */
		write_register (argreg, regval);
		argreg++;
		len -= partial_len;
		val += partial_len;
	      }
	    else
	      {
		/* keep for later pushing */
		stack_args[nstack_args].val = val;
		stack_args[nstack_args++].len = len;
		break;
	      }
	  }
      }
    }
  /* now do the real stack pushing, process args right to left */
  while (nstack_args--)
    {
      sp -= stack_args[nstack_args].len;
      write_memory (sp, stack_args[nstack_args].val,
		    stack_args[nstack_args].len);
    }

  /* Return adjusted stack pointer.  */
  return sp;
}

void _initialize_fr30_tdep PARAMS ((void));

void
_initialize_fr30_tdep ()
{
  extern int print_insn_fr30 (bfd_vma, disassemble_info *);
  tm_print_insn = print_insn_fr30;
}

/* Function: check_prologue_cache
   Check if prologue for this frame's PC has already been scanned.
   If it has, copy the relevant information about that prologue and
   return non-zero.  Otherwise do not copy anything and return zero.

   The information saved in the cache includes:
   * the frame register number;
   * the size of the stack frame;
   * the offsets of saved regs (relative to the old SP); and
   * the offset from the stack pointer to the frame pointer

   The cache contains only one entry, since this is adequate
   for the typical sequence of prologue scan requests we get.
   When performing a backtrace, GDB will usually ask to scan
   the same function twice in a row (once to get the frame chain,
   and once to fill in the extra frame information).
 */

static struct frame_info prologue_cache;

static int
check_prologue_cache (fi)
     struct frame_info *fi;
{
  int i;

  if (fi->pc == prologue_cache.pc)
    {
      fi->framereg = prologue_cache.framereg;
      fi->framesize = prologue_cache.framesize;
      fi->frameoffset = prologue_cache.frameoffset;
      for (i = 0; i <= NUM_REGS; i++)
	fi->fsr.regs[i] = prologue_cache.fsr.regs[i];
      return 1;
    }
  else
    return 0;
}


/* Function: save_prologue_cache
   Copy the prologue information from fi to the prologue cache.
 */

static void
save_prologue_cache (fi)
     struct frame_info *fi;
{
  int i;

  prologue_cache.pc = fi->pc;
  prologue_cache.framereg = fi->framereg;
  prologue_cache.framesize = fi->framesize;
  prologue_cache.frameoffset = fi->frameoffset;

  for (i = 0; i <= NUM_REGS; i++)
    {
      prologue_cache.fsr.regs[i] = fi->fsr.regs[i];
    }
}


/* Function: scan_prologue
   Scan the prologue of the function that contains PC, and record what
   we find in PI.  PI->fsr must be zeroed by the called.  Returns the
   pc after the prologue.  Note that the addresses saved in pi->fsr
   are actually just frame relative (negative offsets from the frame
   pointer).  This is because we don't know the actual value of the
   frame pointer yet.  In some circumstances, the frame pointer can't
   be determined till after we have scanned the prologue.  */

static void
fr30_scan_prologue (fi)
     struct frame_info *fi;
{
  int sp_offset, fp_offset;
  CORE_ADDR prologue_start, prologue_end, current_pc;

  /* Check if this function is already in the cache of frame information. */
  if (check_prologue_cache (fi))
    return;

  /* Assume there is no frame until proven otherwise.  */
  fi->framereg = SP_REGNUM;
  fi->framesize = 0;
  fi->frameoffset = 0;

  /* Find the function prologue.  If we can't find the function in
     the symbol table, peek in the stack frame to find the PC.  */
  if (find_pc_partial_function (fi->pc, NULL, &prologue_start, &prologue_end))
    {
      /* Assume the prologue is everything between the first instruction
         in the function and the first source line.  */
      struct symtab_and_line sal = find_pc_line (prologue_start, 0);

      if (sal.line == 0)	/* no line info, use current PC */
	prologue_end = fi->pc;
      else if (sal.end < prologue_end)	/* next line begins after fn end */
	prologue_end = sal.end;	/* (probably means no prologue)  */
    }
  else
    {
      /* XXX Z.R. What now??? The following is entirely bogus */
      prologue_start = (read_memory_integer (fi->frame, 4) & 0x03fffffc) - 12;
      prologue_end = prologue_start + 40;
    }

  /* Now search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.  */

  sp_offset = fp_offset = 0;
  for (current_pc = prologue_start; current_pc < prologue_end; current_pc += 2)
    {
      unsigned int insn;

      insn = read_memory_unsigned_integer (current_pc, 2);

      if ((insn & 0xfe00) == 0x8e00)	/* stm0 or stm1 */
	{
	  int reg, mask = insn & 0xff;

	  /* scan in one sweep - create virtual 16-bit mask from either insn's mask */
	  if ((insn & 0x0100) == 0)
	    {
	      mask <<= 8;	/* stm0 - move to upper byte in virtual mask */
	    }

	  /* Calculate offsets of saved registers (to be turned later into addresses). */
	  for (reg = R4_REGNUM; reg <= R11_REGNUM; reg++)
	    if (mask & (1 << (15 - reg)))
	      {
		sp_offset -= 4;
		fi->fsr.regs[reg] = sp_offset;
	      }
	}
      else if ((insn & 0xfff0) == 0x1700)	/* st rx,@-r15 */
	{
	  int reg = insn & 0xf;

	  sp_offset -= 4;
	  fi->fsr.regs[reg] = sp_offset;
	}
      else if ((insn & 0xff00) == 0x0f00)	/* enter */
	{
	  fp_offset = fi->fsr.regs[FP_REGNUM] = sp_offset - 4;
	  sp_offset -= 4 * (insn & 0xff);
	  fi->framereg = FP_REGNUM;
	}
      else if (insn == 0x1781)	/* st rp,@-sp */
	{
	  sp_offset -= 4;
	  fi->fsr.regs[RP_REGNUM] = sp_offset;
	}
      else if (insn == 0x170e)	/* st fp,@-sp */
	{
	  sp_offset -= 4;
	  fi->fsr.regs[FP_REGNUM] = sp_offset;
	}
      else if (insn == 0x8bfe)	/* mov sp,fp */
	{
	  fi->framereg = FP_REGNUM;
	}
      else if ((insn & 0xff00) == 0xa300)	/* addsp xx */
	{
	  sp_offset += 4 * (signed char) (insn & 0xff);
	}
      else if ((insn & 0xff0f) == 0x9b00 &&	/* ldi:20 xx,r0 */
	       read_memory_unsigned_integer (current_pc + 4, 2)
	       == 0xac0f)	/* sub r0,sp */
	{
	  /* large stack adjustment */
	  sp_offset -= (((insn & 0xf0) << 12) | read_memory_unsigned_integer (current_pc + 2, 2));
	  current_pc += 4;
	}
      else if (insn == 0x9f80 &&	/* ldi:32 xx,r0 */
	       read_memory_unsigned_integer (current_pc + 6, 2)
	       == 0xac0f)	/* sub r0,sp */
	{
	  /* large stack adjustment */
	  sp_offset -=
	    (read_memory_unsigned_integer (current_pc + 2, 2) << 16 |
	     read_memory_unsigned_integer (current_pc + 4, 2));
	  current_pc += 6;
	}
    }

  /* The frame size is just the negative of the offset (from the original SP)
     of the last thing thing we pushed on the stack.  The frame offset is
     [new FP] - [new SP].  */
  fi->framesize = -sp_offset;
  fi->frameoffset = fp_offset - sp_offset;

  save_prologue_cache (fi);
}

/* Function: init_extra_frame_info
   Setup the frame's frame pointer, pc, and frame addresses for saved
   registers.  Most of the work is done in scan_prologue().

   Note that when we are called for the last frame (currently active frame),
   that fi->pc and fi->frame will already be setup.  However, fi->frame will
   be valid only if this routine uses FP.  For previous frames, fi-frame will
   always be correct (since that is derived from fr30_frame_chain ()).

   We can be called with the PC in the call dummy under two circumstances.
   First, during normal backtracing, second, while figuring out the frame
   pointer just prior to calling the target function (see run_stack_dummy).  */

void
fr30_init_extra_frame_info (fi)
     struct frame_info *fi;
{
  int reg;

  if (fi->next)
    fi->pc = FRAME_SAVED_PC (fi->next);

  memset (fi->fsr.regs, '\000', sizeof fi->fsr.regs);

  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    {
      /* We need to setup fi->frame here because run_stack_dummy gets it wrong
         by assuming it's always FP.  */
      fi->frame = generic_read_register_dummy (fi->pc, fi->frame, SP_REGNUM);
      fi->framesize = 0;
      fi->frameoffset = 0;
      return;
    }
  fr30_scan_prologue (fi);

  if (!fi->next)		/* this is the innermost frame? */
    fi->frame = read_register (fi->framereg);
  else
    /* not the innermost frame */
    /* If we have an FP,  the callee saved it. */ if (fi->framereg == FP_REGNUM)
    if (fi->next->fsr.regs[fi->framereg] != 0)
      fi->frame = read_memory_integer (fi->next->fsr.regs[fi->framereg],
				       4);
  /* Calculate actual addresses of saved registers using offsets determined
     by fr30_scan_prologue.  */
  for (reg = 0; reg < NUM_REGS; reg++)
    if (fi->fsr.regs[reg] != 0)
      {
	fi->fsr.regs[reg] += fi->frame + fi->framesize - fi->frameoffset;
      }
}

/* Function: find_callers_reg
   Find REGNUM on the stack.  Otherwise, it's in an active register.
   One thing we might want to do here is to check REGNUM against the
   clobber mask, and somehow flag it as invalid if it isn't saved on
   the stack somewhere.  This would provide a graceful failure mode
   when trying to get the value of caller-saves registers for an inner
   frame.  */

CORE_ADDR
fr30_find_callers_reg (fi, regnum)
     struct frame_info *fi;
     int regnum;
{
  for (; fi; fi = fi->next)
    if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
      return generic_read_register_dummy (fi->pc, fi->frame, regnum);
    else if (fi->fsr.regs[regnum] != 0)
      return read_memory_unsigned_integer (fi->fsr.regs[regnum],
					   REGISTER_RAW_SIZE (regnum));

  return read_register (regnum);
}


/* Function: frame_chain
   Figure out the frame prior to FI.  Unfortunately, this involves
   scanning the prologue of the caller, which will also be done
   shortly by fr30_init_extra_frame_info.  For the dummy frame, we
   just return the stack pointer that was in use at the time the
   function call was made.  */


CORE_ADDR
fr30_frame_chain (fi)
     struct frame_info *fi;
{
  CORE_ADDR fn_start, callers_pc, fp;
  struct frame_info caller_fi;
  int framereg;

  /* is this a dummy frame? */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    return fi->frame;		/* dummy frame same as caller's frame */

  /* is caller-of-this a dummy frame? */
  callers_pc = FRAME_SAVED_PC (fi);	/* find out who called us: */
  fp = fr30_find_callers_reg (fi, FP_REGNUM);
  if (PC_IN_CALL_DUMMY (callers_pc, fp, fp))
    return fp;			/* dummy frame's frame may bear no relation to ours */

  if (find_pc_partial_function (fi->pc, 0, &fn_start, 0))
    if (fn_start == entry_point_address ())
      return 0;			/* in _start fn, don't chain further */

  framereg = fi->framereg;

  /* If the caller is the startup code, we're at the end of the chain.  */
  if (find_pc_partial_function (callers_pc, 0, &fn_start, 0))
    if (fn_start == entry_point_address ())
      return 0;

  memset (&caller_fi, 0, sizeof (caller_fi));
  caller_fi.pc = callers_pc;
  fr30_scan_prologue (&caller_fi);
  framereg = caller_fi.framereg;

  /* If the caller used a frame register, return its value.
     Otherwise, return the caller's stack pointer.  */
  if (framereg == FP_REGNUM)
    return fr30_find_callers_reg (fi, framereg);
  else
    return fi->frame + fi->framesize;
}

/* Function: frame_saved_pc 
   Find the caller of this frame.  We do this by seeing if RP_REGNUM
   is saved in the stack anywhere, otherwise we get it from the
   registers.  If the inner frame is a dummy frame, return its PC
   instead of RP, because that's where "caller" of the dummy-frame
   will be found.  */

CORE_ADDR
fr30_frame_saved_pc (fi)
     struct frame_info *fi;
{
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    return generic_read_register_dummy (fi->pc, fi->frame, PC_REGNUM);
  else
    return fr30_find_callers_reg (fi, RP_REGNUM);
}

/* Function: fix_call_dummy
   Pokes the callee function's address into the CALL_DUMMY assembly stub.
   Assumes that the CALL_DUMMY looks like this:
   jarl <offset24>, r31
   trap
 */

int
fr30_fix_call_dummy (dummy, sp, fun, nargs, args, type, gcc_p)
     char *dummy;
     CORE_ADDR sp;
     CORE_ADDR fun;
     int nargs;
     value_ptr *args;
     struct type *type;
     int gcc_p;
{
  long offset24;

  offset24 = (long) fun - (long) entry_point_address ();
  offset24 &= 0x3fffff;
  offset24 |= 0xff800000;	/* jarl <offset24>, r31 */

  store_unsigned_integer ((unsigned int *) &dummy[2], 2, offset24 & 0xffff);
  store_unsigned_integer ((unsigned int *) &dummy[0], 2, offset24 >> 16);
  return 0;
}
