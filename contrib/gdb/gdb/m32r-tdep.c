/* Target-dependent code for the Mitsubishi m32r for GDB, the GNU debugger.
   Copyright 1996, Free Software Foundation, Inc.

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

/* Function: m32r_use_struct_convention
   Return nonzero if call_function should allocate stack space for a
   struct return? */
int
m32r_use_struct_convention (gcc_p, type)
     int gcc_p;
     struct type *type;
{
  return (TYPE_LENGTH (type) > 8);
}

/* Function: frame_find_saved_regs
   Return the frame_saved_regs structure for the frame.
   Doesn't really work for dummy frames, but it does pass back
   an empty frame_saved_regs, so I guess that's better than total failure */

void 
m32r_frame_find_saved_regs (fi, regaddr)
     struct frame_info *fi;
     struct frame_saved_regs *regaddr;
{
  memcpy(regaddr, &fi->fsr, sizeof(struct frame_saved_regs));
}

/* Turn this on if you want to see just how much instruction decoding
   if being done, its quite a lot
   */
#if 0
static void dump_insn(char * commnt,CORE_ADDR pc, int insn)
{
  printf_filtered("  %s %08x %08x ",
		  commnt,(unsigned int)pc,(unsigned int) insn);
  (*tm_print_insn)(pc,&tm_print_insn_info);
  printf_filtered("\n");
}
#define insn_debug(args) { printf_filtered args; }
#else
#define dump_insn(a,b,c) {}
#define insn_debug(args) {}
#endif

#define DEFAULT_SEARCH_LIMIT 44 

/* Function: scan_prologue
   This function decodes the target function prologue to determine
   1) the size of the stack frame, and 2) which registers are saved on it.
   It saves the offsets of saved regs in the frame_saved_regs argument,
   and returns the frame size.  */

/*
  The sequence it currently generates is:
  
	if (varargs function) { ddi sp,#n }
	push registers
	if (additional stack <= 256) {	addi sp,#-stack	}
	else if (additional stack < 65k) { add3 sp,sp,#-stack

	} else if (additional stack) {
	seth sp,#(stack & 0xffff0000)
	or3 sp,sp,#(stack & 0x0000ffff)
	sub sp,r4
	}
	if (frame pointer) {
		mv sp,fp
	}

These instructions are scheduled like everything else, so you should stop at
the first branch instruction.
 
*/

/* This is required by skip prologue and by m32r_init_extra_frame_info. 
   The results of decoding a prologue should be cached because this
   thrashing is getting nuts.
   I am thinking of making a container class with two indexes, name and
   address. It may be better to extend the symbol table.
   */

static void decode_prologue (start_pc, scan_limit, 
			     pl_endptr, framelength, 
			     fi, fsr)
     CORE_ADDR start_pc;
     CORE_ADDR scan_limit;
     CORE_ADDR * pl_endptr;  /* var parameter */
     unsigned long * framelength;
     struct frame_info * fi;
     struct frame_saved_regs * fsr;
{
  unsigned long framesize;
  int insn;
  int op1;
  int maybe_one_more = 0;
  CORE_ADDR after_prologue = 0;
  CORE_ADDR after_stack_adjust = 0;
  CORE_ADDR current_pc;


  framesize = 0;
  after_prologue = 0;
  insn_debug(("rd prolog l(%d)\n",scan_limit - current_pc));

  for (current_pc = start_pc; current_pc < scan_limit; current_pc += 2)
    {

      insn = read_memory_unsigned_integer (current_pc, 2);
      dump_insn("insn-1",current_pc,insn);    /* MTZ */
      
       /* If this is a 32 bit instruction, we dont want to examine its
	 immediate data as though it were an instruction */
      if (current_pc & 0x02)
	{ /* Clear the parallel execution bit from 16 bit instruction */
	  if (maybe_one_more)
	    { /* The last instruction was a branch, usually terminates
		 the series, but if this is a parallel instruction,
		 it may be a stack framing instruction */
	      if (! (insn & 0x8000))
		{ insn_debug(("Really done"));
		  break; /* nope, we are really done */
		}
	    }
	  insn &= 0x7fff;        /* decode this instruction further */
	}
      else
	{
	  if (maybe_one_more) 
	    break; /* This isnt the one more */
	  if (insn & 0x8000)
	    {
	      insn_debug(("32 bit insn\n"));
	      if (current_pc == scan_limit)
		scan_limit += 2; /* extend the search */
	      current_pc += 2;   /* skip the immediate data */
	      if (insn == 0x8faf)			/* add3 sp, sp, xxxx */
		/* add 16 bit sign-extended offset */
		{ insn_debug(("stack increment\n"));
		framesize += -((short) read_memory_unsigned_integer (current_pc, 2));
		}
	      else
		{
		  if (((insn >> 8) == 0xe4) && /* ld24 r4, xxxxxx; sub sp, r4 */
		      read_memory_unsigned_integer (current_pc + 2, 2) == 0x0f24)
		    { /* subtract 24 bit sign-extended negative-offset */
		      dump_insn("insn-2",current_pc+2,insn);
		      insn = read_memory_unsigned_integer (current_pc - 2, 4);
		      dump_insn("insn-3(l4)",current_pc -2,insn);
		      if (insn & 0x00800000)	/* sign extend */
			insn  |= 0xff000000;	/* negative */
		      else
			insn  &= 0x00ffffff;	/* positive */
		      framesize += insn;
		    }
		}
	      after_prologue = current_pc;
	      continue;
	    }
	}
      op1 = insn & 0xf000; /* isolate just the first nibble */
      
      if ((insn & 0xf0ff) == 0x207f)
	{		/* st reg, @-sp */
	  int regno;
	  insn_debug(("push\n"));
#if 0	/* No, PUSH FP is not an indication that we will use a frame pointer. */
	  if (((insn & 0xffff) == 0x2d7f) && fi) 
	    fi->using_frame_pointer = 1;
#endif
	  framesize  += 4;
#if 0 
/* Why should we increase the scan limit, just because we did a push? 
   And if there is a reason, surely we would only want to do it if we
   had already reached the scan limit... */
	  if (current_pc == scan_limit)
	    scan_limit += 2;
#endif
	  regno = ((insn >> 8) & 0xf);
	  if (fsr)				/* save_regs offset */
	    fsr->regs[regno] = framesize;
	  after_prologue = 0;
	    continue;
	}
      if ((insn >> 8) == 0x4f)  		/* addi sp, xx */
	/* add 8 bit sign-extended offset */
	{
	  int stack_adjust = (char) (insn & 0xff);

	  /* there are probably two of these stack adjustments:
	     1) A negative one in the prologue, and
	     2) A positive one in the epilogue.
	     We are only interested in the first one.  */

	  if (stack_adjust < 0)
	    {
	      framesize -= stack_adjust;
	      after_prologue = 0;
	      /* A frameless function may have no "mv fp, sp".
		 In that case, this is the end of the prologue.  */
	      after_stack_adjust = current_pc + 2;
	    }
	  continue;
	}
      if (insn == 0x1d8f) {	/* mv fp, sp */
	if (fi) 
	  fi->using_frame_pointer = 1;	/* fp is now valid */
	insn_debug(("done fp found\n"));
	after_prologue = current_pc + 2;
	break;				/* end of stack adjustments */
      }
      if (insn ==  0x7000)  /* Nop looks like a branch, continue explicitly */
	{ insn_debug(("nop\n"));
	after_prologue = current_pc + 2;
	continue; /* nop occurs between pushes */
	}
      /* End of prolog if any of these are branch instructions */
      if ((op1 == 0x7000)
	  || ( op1 == 0xb000)
	  || (op1 == 0x7000))
	{
	  after_prologue = current_pc;
	  insn_debug(("Done: branch\n"));
	  maybe_one_more = 1;
	  continue;
	}
      /* Some of the branch instructions are mixed with other types */
      if (op1 == 0x1000)
	{int subop = insn & 0x0ff0;
	  if ((subop == 0x0ec0) || (subop == 0x0fc0))
	    { insn_debug(("done: jmp\n"));
	      after_prologue = current_pc;
	      maybe_one_more = 1;
	      continue; /* jmp , jl */
	    }
	}
    }

  if (current_pc >= scan_limit)
    {
      if (pl_endptr) 
#if 1
	if (after_stack_adjust != 0)
	  /* We did not find a "mv fp,sp", but we DID find
	     a stack_adjust.  Is it safe to use that as the
	     end of the prologue?  I just don't know. */
	  {
	    *pl_endptr = after_stack_adjust;
	    if (framelength)
	      *framelength = framesize;
	  }
	else
#endif
      /* We reached the end of the loop without finding the end
	 of the prologue.  No way to win -- we should report failure.  
	 The way we do that is to return the original start_pc.
	 GDB will set a breakpoint at the start of the function (etc.) */

	*pl_endptr = start_pc;
	
      return;
    }
  if (after_prologue == 0) 
    after_prologue = current_pc;

  insn_debug((" framesize %d, firstline %08x\n",framesize,after_prologue));
  if (framelength) 
    *framelength = framesize;
  if (pl_endptr) 
    *pl_endptr = after_prologue;
} /*  decode_prologue */

/* Function: skip_prologue
   Find end of function prologue */

CORE_ADDR
m32r_skip_prologue (pc)
     CORE_ADDR pc;
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;

  /* See what the symbol table says */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end <= func_end)
	{
	  
	  insn_debug(("BP after prologue %08x\n",sal.end));
	  func_end = sal.end;
	}
      else
	/* Either there's no line info, or the line after the prologue is after
	   the end of the function.  In this case, there probably isn't a
	   prologue.  */
	{
	  insn_debug(("No line info, line(%x) sal_end(%x) funcend(%x)\n",
		      sal.line,sal.end,func_end));
	  func_end = min(func_end,func_addr + DEFAULT_SEARCH_LIMIT);
	}
    }
  else 
    func_end = pc + DEFAULT_SEARCH_LIMIT;
  decode_prologue (pc, func_end, &sal.end, 0, 0, 0);
  return sal.end;
}

static unsigned long
m32r_scan_prologue (fi, fsr)
     struct frame_info *fi;
     struct frame_saved_regs *fsr;
{
  struct symtab_and_line sal;
  CORE_ADDR prologue_start, prologue_end, current_pc;
  unsigned long framesize;

  /* this code essentially duplicates skip_prologue, 
     but we need the start address below.  */

  if (find_pc_partial_function (fi->pc, NULL, &prologue_start, &prologue_end))
    {
      sal = find_pc_line (prologue_start, 0);

      if (sal.line == 0)		/* no line info, use current PC */
	if (prologue_start == entry_point_address ())
	  return 0;
    }
  else
    {
      prologue_start = fi->pc;
      prologue_end = prologue_start + 48; /* We're in the boondocks: 
					      allow for 16 pushes, an add, 
					      and "mv fp,sp" */
    }
#if 0
  prologue_end = min (prologue_end, fi->pc);
#endif
  insn_debug(("fipc(%08x) start(%08x) end(%08x)\n",
	      fi->pc,prologue_start,prologue_end));
  prologue_end = min(prologue_end, prologue_start + DEFAULT_SEARCH_LIMIT);
  decode_prologue (prologue_start,prologue_end,&prologue_end,&framesize,
		   fi,fsr);
  return framesize;
}

/* Function: init_extra_frame_info
   This function actually figures out the frame address for a given pc and
   sp.  This is tricky on the m32r because we sometimes don't use an explicit
   frame pointer, and the previous stack pointer isn't necessarily recorded
   on the stack.  The only reliable way to get this info is to
   examine the prologue.  */

void
m32r_init_extra_frame_info (fi)
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
      return;
    }
  else 
    {
      fi->using_frame_pointer = 0;
      fi->framesize = m32r_scan_prologue (fi, &fi->fsr);

      if (!fi->next)
	if (fi->using_frame_pointer)
	  {
	    fi->frame = read_register (FP_REGNUM);
	  }
	else
	  fi->frame = read_register (SP_REGNUM);
      else 	/* fi->next means this is not the innermost frame */
	if (fi->using_frame_pointer)		    /* we have an FP */
	  if (fi->next->fsr.regs[FP_REGNUM] != 0)   /* caller saved our FP */
	    fi->frame = read_memory_integer (fi->next->fsr.regs[FP_REGNUM], 4);
      for (reg = 0; reg < NUM_REGS; reg++)
	if (fi->fsr.regs[reg] != 0)
	  fi->fsr.regs[reg] = fi->frame + fi->framesize - fi->fsr.regs[reg];
    }
}

/* Function: mn10300_virtual_frame_pointer
   Return the register that the function uses for a frame pointer, 
   plus any necessary offset to be applied to the register before
   any frame pointer offsets.  */

void
m32r_virtual_frame_pointer (pc, reg, offset)
     CORE_ADDR pc;
     long *reg;
     long *offset;
{
  struct frame_info fi;

  /* Set up a dummy frame_info. */
  fi.next = NULL;
  fi.prev = NULL;
  fi.frame = 0;
  fi.pc = pc;

  /* Analyze the prolog and fill in the extra info.  */
  m32r_init_extra_frame_info (&fi);


  /* Results will tell us which type of frame it uses.  */
  if (fi.using_frame_pointer)
    {
      *reg    = FP_REGNUM;
      *offset = 0;
    }
  else
    {
      *reg    = SP_REGNUM;
      *offset = 0;
    }
}

/* Function: find_callers_reg
   Find REGNUM on the stack.  Otherwise, it's in an active register.  One thing
   we might want to do here is to check REGNUM against the clobber mask, and
   somehow flag it as invalid if it isn't saved on the stack somewhere.  This
   would provide a graceful failure mode when trying to get the value of
   caller-saves registers for an inner frame.  */

CORE_ADDR
m32r_find_callers_reg (fi, regnum)
     struct frame_info *fi;
     int regnum;
{
  for (; fi; fi = fi->next)
    if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
      return generic_read_register_dummy (fi->pc, fi->frame, regnum);
    else if (fi->fsr.regs[regnum] != 0)
      return read_memory_integer (fi->fsr.regs[regnum], 
				  REGISTER_RAW_SIZE(regnum));
  return read_register (regnum);
}

/* Function: frame_chain
   Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.
   For m32r, we save the frame size when we initialize the frame_info.  */

CORE_ADDR
m32r_frame_chain (fi)
     struct frame_info *fi;
{
  CORE_ADDR fn_start, callers_pc, fp;

  /* is this a dummy frame? */
  if (PC_IN_CALL_DUMMY(fi->pc, fi->frame, fi->frame))
    return fi->frame;	/* dummy frame same as caller's frame */

  /* is caller-of-this a dummy frame? */
  callers_pc = FRAME_SAVED_PC(fi);  /* find out who called us: */
  fp = m32r_find_callers_reg (fi, FP_REGNUM);
  if (PC_IN_CALL_DUMMY(callers_pc, fp, fp))	
    return fp;		/* dummy frame's frame may bear no relation to ours */

  if (find_pc_partial_function (fi->pc, 0, &fn_start, 0))
    if (fn_start == entry_point_address ())
      return 0;		/* in _start fn, don't chain further */
  if (fi->framesize == 0)
    {
      printf_filtered("cannot determine frame size @ %08x , pc(%08x)\n",
		      (unsigned long) fi->frame,
		      (unsigned long) fi->pc );
      return 0;
    }
  insn_debug(("m32rx frame %08x\n",fi->frame+fi->framesize));
  return fi->frame + fi->framesize;
}

/* Function: push_return_address (pc)
   Set up the return address for the inferior function call.
   Necessary for targets that don't actually execute a JSR/BSR instruction 
   (ie. when using an empty CALL_DUMMY) */

CORE_ADDR
m32r_push_return_address (pc, sp)
     CORE_ADDR pc;
     CORE_ADDR sp;
{
  write_register (RP_REGNUM, CALL_DUMMY_ADDRESS ());
  return sp;
}


/* Function: pop_frame
   Discard from the stack the innermost frame,
   restoring all saved registers.  */

struct frame_info *
m32r_pop_frame (frame)
     struct frame_info *frame;
{
  int regnum;

  if (PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame))
    generic_pop_dummy_frame ();
  else
    {
      for (regnum = 0; regnum < NUM_REGS; regnum++)
	if (frame->fsr.regs[regnum] != 0)
	  write_register (regnum, 
			  read_memory_integer (frame->fsr.regs[regnum], 4));

      write_register (PC_REGNUM, FRAME_SAVED_PC (frame));
      write_register (SP_REGNUM, read_register (FP_REGNUM));
      if (read_register (PSW_REGNUM) & 0x80)
	write_register (SPU_REGNUM, read_register (SP_REGNUM));
      else
	write_register (SPI_REGNUM, read_register (SP_REGNUM));
    }
  flush_cached_frames ();
  return NULL;
}

/* Function: frame_saved_pc
   Find the caller of this frame.  We do this by seeing if RP_REGNUM is saved
   in the stack anywhere, otherwise we get it from the registers. */

CORE_ADDR
m32r_frame_saved_pc (fi)
     struct frame_info *fi;
{
  if (PC_IN_CALL_DUMMY(fi->pc, fi->frame, fi->frame))
    return generic_read_register_dummy(fi->pc, fi->frame, PC_REGNUM);
  else
    return m32r_find_callers_reg (fi, RP_REGNUM);
}

/* Function: push_arguments
   Setup the function arguments for calling a function in the inferior.

   On the Mitsubishi M32R architecture, there are four registers (R0 to R3)
   which are dedicated for passing function arguments.  Up to the first 
   four arguments (depending on size) may go into these registers.
   The rest go on the stack.

   Arguments that are smaller than 4 bytes will still take up a whole
   register or a whole 32-bit word on the stack, and will be
   right-justified in the register or the stack word.  This includes
   chars, shorts, and small aggregate types.
 
   Arguments of 8 bytes size are split between two registers, if 
   available.  If only one register is available, the argument will 
   be split between the register and the stack.  Otherwise it is
   passed entirely on the stack.  Aggregate types with sizes between
   4 and 8 bytes are passed entirely on the stack, and are left-justified
   within the double-word (as opposed to aggregates smaller than 4 bytes
   which are right-justified).

   Aggregates of greater than 8 bytes are first copied onto the stack, 
   and then a pointer to the copy is passed in the place of the normal
   argument (either in a register if available, or on the stack).

   Functions that must return an aggregate type can return it in the 
   normal return value registers (R0 and R1) if its size is 8 bytes or
   less.  For larger return values, the caller must allocate space for 
   the callee to copy the return value to.  A pointer to this space is
   passed as an implicit first argument, always in R0. */

CORE_ADDR
m32r_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     unsigned char struct_return;
     CORE_ADDR struct_addr;
{
  int stack_offset, stack_alloc;
  int argreg;
  int argnum;
  struct type *type;
  CORE_ADDR regval;
  char *val;
  char valbuf[4];
  int len;
  int odd_sized_struct;

  /* first force sp to a 4-byte alignment */
  sp = sp & ~3;

  argreg = ARG0_REGNUM;  
  /* The "struct return pointer" pseudo-argument goes in R0 */
  if (struct_return)
      write_register (argreg++, struct_addr);
 
  /* Now make sure there's space on the stack */
  for (argnum = 0, stack_alloc = 0;
       argnum < nargs; argnum++)
    stack_alloc += ((TYPE_LENGTH(VALUE_TYPE(args[argnum])) + 3) & ~3);
  sp -= stack_alloc;    /* make room on stack for args */
 
 
  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  There are 16 bytes
     in four registers available.  Loop thru args from first to last.  */
 
  argreg = ARG0_REGNUM;
  for (argnum = 0, stack_offset = 0; argnum < nargs; argnum++)
    {
      type = VALUE_TYPE (args[argnum]);
      len  = TYPE_LENGTH (type);
      memset(valbuf, 0, sizeof(valbuf));
      if (len < 4)
        { /* value gets right-justified in the register or stack word */
          memcpy(valbuf + (4 - len),
                 (char *) VALUE_CONTENTS (args[argnum]), len);
          val = valbuf;
        }
      else
        val = (char *) VALUE_CONTENTS (args[argnum]);
 
      if (len > 4 && (len & 3) != 0)
        odd_sized_struct = 1;           /* such structs go entirely on stack */
      else
        odd_sized_struct = 0;
      while (len > 0)
        {
          if (argreg > ARGLAST_REGNUM || odd_sized_struct)
            {				/* must go on the stack */
              write_memory (sp + stack_offset, val, 4);
              stack_offset += 4;
            }
          /* NOTE WELL!!!!!  This is not an "else if" clause!!!
             That's because some *&^%$ things get passed on the stack
             AND in the registers!   */
          if (argreg <= ARGLAST_REGNUM)
            {				/* there's room in a register */
              regval = extract_address (val, REGISTER_RAW_SIZE(argreg));
              write_register (argreg++, regval);
            }
          /* Store the value 4 bytes at a time.  This means that things
             larger than 4 bytes may go partly in registers and partly
             on the stack.  */
          len -= REGISTER_RAW_SIZE(argreg);
          val += REGISTER_RAW_SIZE(argreg);
        }
    }
  return sp;
}

/* Function: fix_call_dummy 
   If there is real CALL_DUMMY code (eg. on the stack), this function
   has the responsability to insert the address of the actual code that
   is the target of the target function call.  */

void
m32r_fix_call_dummy (dummy, pc, fun, nargs, args, type, gcc_p)
     char *dummy;
     CORE_ADDR pc;
     CORE_ADDR fun;
     int nargs;
     value_ptr *args;
     struct type *type;
     int gcc_p;
{
  /* ld24 r8, <(imm24) fun> */
  *(unsigned long *) (dummy) = (fun & 0x00ffffff) | 0xe8000000;
}

/* Function: get_saved_register
   Just call the generic_get_saved_register function.  */

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


/* Function: m32r_write_sp
   Because SP is really a read-only register that mirrors either SPU or SPI,
   we must actually write one of those two as well, depending on PSW. */

void
m32r_write_sp (val)
     CORE_ADDR val;
{
  unsigned long psw = read_register (PSW_REGNUM);

  if (psw & 0x80)	/* stack mode: user or interrupt */
    write_register (SPU_REGNUM, val);
  else
    write_register (SPI_REGNUM, val);
  write_register (SP_REGNUM, val);
}

void
_initialize_m32r_tdep ()
{
  tm_print_insn = print_insn_m32r;
}

