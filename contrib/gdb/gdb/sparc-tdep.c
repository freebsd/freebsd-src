/* Target-dependent code for the SPARC for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* ??? Support for calling functions from gdb in sparc64 is unfinished.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "obstack.h"
#include "target.h"
#include "value.h"

#ifdef	USE_PROC_FS
#include <sys/procfs.h>
#endif

#include "gdbcore.h"

#ifdef GDB_TARGET_IS_SPARC64
#define FP_REGISTER_BYTES (64 * 4)
#else
#define FP_REGISTER_BYTES (32 * 4)
#endif

/* If not defined, assume 32 bit sparc.  */
#ifndef FP_MAX_REGNUM
#define FP_MAX_REGNUM (FP0_REGNUM + 32)
#endif

#define SPARC_INTREG_SIZE (REGISTER_RAW_SIZE (G0_REGNUM))

/* From infrun.c */
extern int stop_after_trap;

/* We don't store all registers immediately when requested, since they
   get sent over in large chunks anyway.  Instead, we accumulate most
   of the changes and send them over once.  "deferred_stores" keeps
   track of which sets of registers we have locally-changed copies of,
   so we only need send the groups that have changed.  */

int deferred_stores = 0;	/* Cumulates stores we want to do eventually. */

/* Branches with prediction are treated like their non-predicting cousins.  */
/* FIXME: What about floating point branches?  */

/* Macros to extract fields from sparc instructions.  */
#define X_OP(i) (((i) >> 30) & 0x3)
#define X_RD(i) (((i) >> 25) & 0x1f)
#define X_A(i) (((i) >> 29) & 1)
#define X_COND(i) (((i) >> 25) & 0xf)
#define X_OP2(i) (((i) >> 22) & 0x7)
#define X_IMM22(i) ((i) & 0x3fffff)
#define X_OP3(i) (((i) >> 19) & 0x3f)
#define X_RS1(i) (((i) >> 14) & 0x1f)
#define X_I(i) (((i) >> 13) & 1)
#define X_IMM13(i) ((i) & 0x1fff)
/* Sign extension macros.  */
#define X_SIMM13(i) ((X_IMM13 (i) ^ 0x1000) - 0x1000)
#define X_DISP22(i) ((X_IMM22 (i) ^ 0x200000) - 0x200000)
#ifdef GDB_TARGET_IS_SPARC64
#define X_CC(i) (((i) >> 20) & 3)
#define X_P(i) (((i) >> 19) & 1)
#define X_DISP19(i) ((((i) & 0x7ffff) ^ 0x40000) - 0x40000)
#define X_RCOND(i) (((i) >> 25) & 7)
#define X_DISP16(i) ((((((i) >> 6) && 0xc000) | ((i) & 0x3fff)) ^ 0x8000) - 0x8000)
#define X_FCN(i) (((i) >> 25) & 31)
#endif

typedef enum
{
  Error, not_branch, bicc, bicca, ba, baa, ticc, ta,
#ifdef GDB_TARGET_IS_SPARC64
  done_retry
#endif
} branch_type;

/* Simulate single-step ptrace call for sun4.  Code written by Gary
   Beihl (beihl@mcc.com).  */

/* npc4 and next_pc describe the situation at the time that the
   step-breakpoint was set, not necessary the current value of NPC_REGNUM.  */
static CORE_ADDR next_pc, npc4, target;
static int brknpc4, brktrg;
typedef char binsn_quantum[BREAKPOINT_MAX];
static binsn_quantum break_mem[3];

/* Non-zero if we just simulated a single-step ptrace call.  This is
   needed because we cannot remove the breakpoints in the inferior
   process until after the `wait' in `wait_for_inferior'.  Used for
   sun4. */

int one_stepped;

/* single_step() is called just before we want to resume the inferior,
   if we want to single-step it but there is no hardware or kernel single-step
   support (as on all SPARCs).  We find all the possible targets of the
   coming instruction and breakpoint them.

   single_step is also called just after the inferior stops.  If we had
   set up a simulated single-step, we undo our damage.  */

void
single_step (ignore)
     int ignore; /* pid, but we don't need it */
{
  branch_type br, isbranch();
  CORE_ADDR pc;
  long pc_instruction;

  if (!one_stepped)
    {
      /* Always set breakpoint for NPC.  */
      next_pc = read_register (NPC_REGNUM);
      npc4 = next_pc + 4; /* branch not taken */

      target_insert_breakpoint (next_pc, break_mem[0]);
      /* printf_unfiltered ("set break at %x\n",next_pc); */

      pc = read_register (PC_REGNUM);
      pc_instruction = read_memory_integer (pc, 4);
      br = isbranch (pc_instruction, pc, &target);
      brknpc4 = brktrg = 0;

      if (br == bicca)
	{
	  /* Conditional annulled branch will either end up at
	     npc (if taken) or at npc+4 (if not taken).
	     Trap npc+4.  */
	  brknpc4 = 1;
	  target_insert_breakpoint (npc4, break_mem[1]);
	}
      else if (br == baa && target != next_pc)
	{
	  /* Unconditional annulled branch will always end up at
	     the target.  */
	  brktrg = 1;
	  target_insert_breakpoint (target, break_mem[2]);
	}
#ifdef GDB_TARGET_IS_SPARC64
      else if (br == done_retry)
	{
	  brktrg = 1;
	  target_insert_breakpoint (target, break_mem[2]);
	}
#endif

      /* We are ready to let it go */
      one_stepped = 1;
      return;
    }
  else
    {
      /* Remove breakpoints */
      target_remove_breakpoint (next_pc, break_mem[0]);

      if (brknpc4)
	target_remove_breakpoint (npc4, break_mem[1]);

      if (brktrg)
	target_remove_breakpoint (target, break_mem[2]);

      one_stepped = 0;
    }
}

/* Call this for each newly created frame.  For SPARC, we need to calculate
   the bottom of the frame, and do some extra work if the prologue
   has been generated via the -mflat option to GCC.  In particular,
   we need to know where the previous fp and the pc have been stashed,
   since their exact position within the frame may vary.  */

void
sparc_init_extra_frame_info (fromleaf, fi)
     int fromleaf;
     struct frame_info *fi;
{
  char *name;
  CORE_ADDR addr;
  int insn;

  fi->bottom =
    (fi->next ?
     (fi->frame == fi->next->frame ? fi->next->bottom : fi->next->frame) :
     read_register (SP_REGNUM));

  /* If fi->next is NULL, then we already set ->frame by passing read_fp()
     to create_new_frame.  */
  if (fi->next)
    {
      char buf[MAX_REGISTER_RAW_SIZE];
      int err;

      /* Compute ->frame as if not flat.  If it is flat, we'll change
	 it later.  */
      /* FIXME: If error reading memory, should just stop backtracing, rather
	 than error().  */
      get_saved_register (buf, 0, 0, fi, FP_REGNUM, 0);
      fi->frame = extract_address (buf, REGISTER_RAW_SIZE (FP_REGNUM));
    }

  /* Decide whether this is a function with a ``flat register window''
     frame.  For such functions, the frame pointer is actually in %i7.  */
  fi->flat = 0;
  if (find_pc_partial_function (fi->pc, &name, &addr, NULL))
    {
      /* See if the function starts with an add (which will be of a
	 negative number if a flat frame) to the sp.  FIXME: Does not
	 handle large frames which will need more than one instruction
	 to adjust the sp.  */
      insn = read_memory_integer (addr, 4);
      if (X_OP (insn) == 2 && X_RD (insn) == 14 && X_OP3 (insn) == 0
	  && X_I (insn) && X_SIMM13 (insn) < 0)
	{
	  int offset = X_SIMM13 (insn);

	  /* Then look for a save of %i7 into the frame.  */
	  insn = read_memory_integer (addr + 4, 4);
	  if (X_OP (insn) == 3
	      && X_RD (insn) == 31
	      && X_OP3 (insn) == 4
	      && X_RS1 (insn) == 14)
	    {
	      char buf[MAX_REGISTER_RAW_SIZE];

	      /* We definitely have a flat frame now.  */
	      fi->flat = 1;

	      fi->sp_offset = offset;

	      /* Overwrite the frame's address with the value in %i7.  */
	      get_saved_register (buf, 0, 0, fi, I7_REGNUM, 0);
	      fi->frame = extract_address (buf, REGISTER_RAW_SIZE (I7_REGNUM));

	      /* Record where the fp got saved.  */
	      fi->fp_addr = fi->frame + fi->sp_offset + X_SIMM13 (insn);

	      /* Also try to collect where the pc got saved to.  */
	      fi->pc_addr = 0;
	      insn = read_memory_integer (addr + 12, 4);
	      if (X_OP (insn) == 3
		  && X_RD (insn) == 15
		  && X_OP3 (insn) == 4
		  && X_RS1 (insn) == 14)
		fi->pc_addr = fi->frame + fi->sp_offset + X_SIMM13 (insn);
	    }
	}
    }
  if (fi->next && fi->frame == 0)
    {
      /* Kludge to cause init_prev_frame_info to destroy the new frame.  */
      fi->frame = fi->next->frame;
      fi->pc = fi->next->pc;
    }
}

CORE_ADDR
sparc_frame_chain (frame)
     struct frame_info *frame;
{
  /* Value that will cause FRAME_CHAIN_VALID to not worry about the chain
     value.  If it realy is zero, we detect it later in
     sparc_init_prev_frame.  */
  return (CORE_ADDR)1;
}

CORE_ADDR
sparc_extract_struct_value_address (regbuf)
     char regbuf[REGISTER_BYTES];
{
#ifdef GDB_TARGET_IS_SPARC64
  return extract_address (regbuf + REGISTER_BYTE (O0_REGNUM),
			  REGISTER_RAW_SIZE (O0_REGNUM));
#else
  return read_memory_integer (((int *)(regbuf)) [SP_REGNUM] + (16 * SPARC_INTREG_SIZE), 
	       		      TARGET_PTR_BIT / TARGET_CHAR_BIT);
#endif
}

/* Find the pc saved in frame FRAME.  */

CORE_ADDR
sparc_frame_saved_pc (frame)
     struct frame_info *frame;
{
  char buf[MAX_REGISTER_RAW_SIZE];
  CORE_ADDR addr;

  if (frame->signal_handler_caller)
    {
      /* This is the signal trampoline frame.
	 Get the saved PC from the sigcontext structure.  */

#ifndef SIGCONTEXT_PC_OFFSET
#define SIGCONTEXT_PC_OFFSET 12
#endif

      CORE_ADDR sigcontext_addr;
      char scbuf[TARGET_PTR_BIT / HOST_CHAR_BIT];
      int saved_pc_offset = SIGCONTEXT_PC_OFFSET;
      char *name = NULL;

      /* Solaris2 ucbsigvechandler passes a pointer to a sigcontext
	 as the third parameter.  The offset to the saved pc is 12.  */
      find_pc_partial_function (frame->pc, &name,
				(CORE_ADDR *)NULL,(CORE_ADDR *)NULL);
      if (name && STREQ (name, "ucbsigvechandler"))
	saved_pc_offset = 12;

      /* The sigcontext address is contained in register O2.  */
      get_saved_register (buf, (int *)NULL, (CORE_ADDR *)NULL,
			  frame, O0_REGNUM + 2, (enum lval_type *)NULL);
      sigcontext_addr = extract_address (buf, REGISTER_RAW_SIZE (O0_REGNUM + 2));

      /* Don't cause a memory_error when accessing sigcontext in case the
	 stack layout has changed or the stack is corrupt.  */
      target_read_memory (sigcontext_addr + saved_pc_offset,
			  scbuf, sizeof (scbuf));
      return extract_address (scbuf, sizeof (scbuf));
    }
  if (frame->flat)
    addr = frame->pc_addr;
  else
    addr = frame->bottom + FRAME_SAVED_I0 +
      SPARC_INTREG_SIZE * (I7_REGNUM - I0_REGNUM);

  if (addr == 0)
    /* A flat frame leaf function might not save the PC anywhere,
       just leave it in %o7.  */
    return PC_ADJUST (read_register (O7_REGNUM));

  read_memory (addr, buf, SPARC_INTREG_SIZE);
  return PC_ADJUST (extract_address (buf, SPARC_INTREG_SIZE));
}

/* Since an individual frame in the frame cache is defined by two
   arguments (a frame pointer and a stack pointer), we need two
   arguments to get info for an arbitrary stack frame.  This routine
   takes two arguments and makes the cached frames look as if these
   two arguments defined a frame on the cache.  This allows the rest
   of info frame to extract the important arguments without
   difficulty.  */

struct frame_info *
setup_arbitrary_frame (argc, argv)
     int argc;
     CORE_ADDR *argv;
{
  struct frame_info *frame;

  if (argc != 2)
    error ("Sparc frame specifications require two arguments: fp and sp");

  frame = create_new_frame (argv[0], 0);

  if (!frame)
    fatal ("internal: create_new_frame returned invalid frame");
  
  frame->bottom = argv[1];
  frame->pc = FRAME_SAVED_PC (frame);
  return frame;
}

/* Given a pc value, skip it forward past the function prologue by
   disassembling instructions that appear to be a prologue.

   If FRAMELESS_P is set, we are only testing to see if the function
   is frameless.  This allows a quicker answer.

   This routine should be more specific in its actions; making sure
   that it uses the same register in the initial prologue section.  */

static CORE_ADDR examine_prologue PARAMS ((CORE_ADDR, int, struct frame_info *,
					   struct frame_saved_regs *));

static CORE_ADDR 
examine_prologue (start_pc, frameless_p, fi, saved_regs)
     CORE_ADDR start_pc;
     int frameless_p;
     struct frame_info *fi;
     struct frame_saved_regs *saved_regs;
{
  int insn;
  int dest = -1;
  CORE_ADDR pc = start_pc;
  int is_flat = 0;

  insn = read_memory_integer (pc, 4);

  /* Recognize the `sethi' insn and record its destination.  */
  if (X_OP (insn) == 0 && X_OP2 (insn) == 4)
    {
      dest = X_RD (insn);
      pc += 4;
      insn = read_memory_integer (pc, 4);
    }

  /* Recognize an add immediate value to register to either %g1 or
     the destination register recorded above.  Actually, this might
     well recognize several different arithmetic operations.
     It doesn't check that rs1 == rd because in theory "sub %g0, 5, %g1"
     followed by "save %sp, %g1, %sp" is a valid prologue (Not that
     I imagine any compiler really does that, however).  */
  if (X_OP (insn) == 2
      && X_I (insn)
      && (X_RD (insn) == 1 || X_RD (insn) == dest))
    {
      pc += 4;
      insn = read_memory_integer (pc, 4);
    }

  /* Recognize any SAVE insn.  */
  if (X_OP (insn) == 2 && X_OP3 (insn) == 60)
    {
      pc += 4;
      if (frameless_p)			/* If the save is all we care about, */
	return pc;			/* return before doing more work */
      insn = read_memory_integer (pc, 4);
    }
  /* Recognize add to %sp.  */
  else if (X_OP (insn) == 2 && X_RD (insn) == 14 && X_OP3 (insn) == 0)
    {
      pc += 4;
      if (frameless_p)			/* If the add is all we care about, */
	return pc;			/* return before doing more work */
      is_flat = 1;
      insn = read_memory_integer (pc, 4);
      /* Recognize store of frame pointer (i7).  */
      if (X_OP (insn) == 3
	  && X_RD (insn) == 31
	  && X_OP3 (insn) == 4
	  && X_RS1 (insn) == 14)
	{
	  pc += 4;
	  insn = read_memory_integer (pc, 4);

	  /* Recognize sub %sp, <anything>, %i7.  */
	  if (X_OP (insn) ==  2
	      && X_OP3 (insn) == 4
	      && X_RS1 (insn) == 14
	      && X_RD (insn) == 31)
	    {
	      pc += 4;
	      insn = read_memory_integer (pc, 4);
	    }
	  else
	    return pc;
	}
      else
	return pc;
    }
  else
    /* Without a save or add instruction, it's not a prologue.  */
    return start_pc;

  while (1)
    {
      /* Recognize stores into the frame from the input registers.
	 This recognizes all non alternate stores of input register,
	 into a location offset from the frame pointer.  */
      if ((X_OP (insn) == 3
	   && (X_OP3 (insn) & 0x3c) == 4 /* Store, non-alternate.  */
	   && (X_RD (insn) & 0x18) == 0x18 /* Input register.  */
	   && X_I (insn)		/* Immediate mode.  */
	   && X_RS1 (insn) == 30	/* Off of frame pointer.  */
	   /* Into reserved stack space.  */
	   && X_SIMM13 (insn) >= 0x44
	   && X_SIMM13 (insn) < 0x5b))
	;
      else if (is_flat
	       && X_OP (insn) == 3
	       && X_OP3 (insn) == 4
	       && X_RS1 (insn) == 14
	       )
	{
	  if (saved_regs && X_I (insn))
	    saved_regs->regs[X_RD (insn)] =
	      fi->frame + fi->sp_offset + X_SIMM13 (insn);
	}
      else
	break;
      pc += 4;
      insn = read_memory_integer (pc, 4);
    }

  return pc;
}

CORE_ADDR 
skip_prologue (start_pc, frameless_p)
     CORE_ADDR start_pc;
     int frameless_p;
{
  return examine_prologue (start_pc, frameless_p, NULL, NULL);
}

/* Check instruction at ADDR to see if it is a branch.
   All non-annulled instructions will go to NPC or will trap.
   Set *TARGET if we find a candidate branch; set to zero if not.

   This isn't static as it's used by remote-sa.sparc.c.  */

branch_type
isbranch (instruction, addr, target)
     long instruction;
     CORE_ADDR addr, *target;
{
  branch_type val = not_branch;
  long int offset;		/* Must be signed for sign-extend.  */

  *target = 0;

  if (X_OP (instruction) == 0
      && (X_OP2 (instruction) == 2
	  || X_OP2 (instruction) == 6
#ifdef GDB_TARGET_IS_SPARC64
	  || X_OP2 (instruction) == 1
	  || X_OP2 (instruction) == 3
	  || X_OP2 (instruction) == 5
#else
	  || X_OP2 (instruction) == 7
#endif
	  ))
    {
      if (X_COND (instruction) == 8)
	val = X_A (instruction) ? baa : ba;
      else
	val = X_A (instruction) ? bicca : bicc;
      switch (X_OP2 (instruction))
	{
	case 2:
	case 6:
#ifndef GDB_TARGET_IS_SPARC64
	case 7:
#endif
	  offset = 4 * X_DISP22 (instruction);
	  break;
#ifdef GDB_TARGET_IS_SPARC64
	case 1:
	case 5:
	  offset = 4 * X_DISP19 (instruction);
	  break;
	case 3:
	  offset = 4 * X_DISP16 (instruction);
	  break;
#endif
	}
      *target = addr + offset;
    }
#ifdef GDB_TARGET_IS_SPARC64
  else if (X_OP (instruction) == 2
	   && X_OP3 (instruction) == 62)
    {
      if (X_FCN (instruction) == 0)
	{
	  /* done */
	  *target = read_register (TNPC_REGNUM);
	  val = done_retry;
	}
      else if (X_FCN (instruction) == 1)
	{
	  /* retry */
	  *target = read_register (TPC_REGNUM);
	  val = done_retry;
	}
    }
#endif

  return val;
}

/* Find register number REGNUM relative to FRAME and put its
   (raw) contents in *RAW_BUFFER.  Set *OPTIMIZED if the variable
   was optimized out (and thus can't be fetched).  If the variable
   was fetched from memory, set *ADDRP to where it was fetched from,
   otherwise it was fetched from a register.

   The argument RAW_BUFFER must point to aligned memory.  */

void
get_saved_register (raw_buffer, optimized, addrp, frame, regnum, lval)
     char *raw_buffer;
     int *optimized;
     CORE_ADDR *addrp;
     struct frame_info *frame;
     int regnum;
     enum lval_type *lval;
{
  struct frame_info *frame1;
  CORE_ADDR addr;

  if (!target_has_registers)
    error ("No registers.");

  if (optimized)
    *optimized = 0;

  addr = 0;
  frame1 = frame->next;
  while (frame1 != NULL)
    {
      if (frame1->pc >= (frame1->bottom ? frame1->bottom :
			 read_register (SP_REGNUM))
	  && frame1->pc <= FRAME_FP (frame1))
	{
	  /* Dummy frame.  All but the window regs are in there somewhere. */
	  if (regnum >= G1_REGNUM && regnum < G1_REGNUM + 7)
	    addr = frame1->frame + (regnum - G0_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 8 * SPARC_INTREG_SIZE);
	  else if (regnum >= I0_REGNUM && regnum < I0_REGNUM + 8)
	    addr = frame1->frame + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 16 * SPARC_INTREG_SIZE);
	  else if (regnum >= FP0_REGNUM && regnum < FP0_REGNUM + 32)
	    addr = frame1->frame + (regnum - FP0_REGNUM) * 4
	      - (FP_REGISTER_BYTES);
#ifdef GDB_TARGET_IS_SPARC64
	  else if (regnum >= FP0_REGNUM + 32 && regnum < FP_MAX_REGNUM)
	    addr = frame1->frame + 32 * 4 + (regnum - FP0_REGNUM - 32) * 8
	      - (FP_REGISTER_BYTES);
#endif
	  else if (regnum >= Y_REGNUM && regnum < NUM_REGS)
	    addr = frame1->frame + (regnum - Y_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 24 * SPARC_INTREG_SIZE);
	}
      else if (frame1->flat)
	{

	  if (regnum == RP_REGNUM)
	    addr = frame1->pc_addr;
	  else if (regnum == I7_REGNUM)
	    addr = frame1->fp_addr;
	  else
	    {
	      CORE_ADDR func_start;
	      struct frame_saved_regs regs;
	      memset (&regs, 0, sizeof (regs));

	      find_pc_partial_function (frame1->pc, NULL, &func_start, NULL);
	      examine_prologue (func_start, 0, frame1, &regs);
	      addr = regs.regs[regnum];
	    }
	}
      else
	{
	  /* Normal frame.  Local and In registers are saved on stack.  */
	  if (regnum >= I0_REGNUM && regnum < I0_REGNUM + 8)
	    addr = (frame1->prev->bottom
		    + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_I0);
	  else if (regnum >= L0_REGNUM && regnum < L0_REGNUM + 8)
	    addr = (frame1->prev->bottom
		    + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_L0);
	  else if (regnum >= O0_REGNUM && regnum < O0_REGNUM + 8)
	    {
	      /* Outs become ins.  */
	      get_saved_register (raw_buffer, optimized, addrp, frame1,
				  (regnum - O0_REGNUM + I0_REGNUM), lval);
	      return;
	    }
	}
      if (addr != 0)
	break;
      frame1 = frame1->next;
    }
  if (addr != 0)
    {
      if (lval != NULL)
	*lval = lval_memory;
      if (regnum == SP_REGNUM)
	{
	  if (raw_buffer != NULL)
	    {
	      /* Put it back in target format.  */
	      store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), addr);
	    }
	  if (addrp != NULL)
	    *addrp = 0;
	  return;
	}
      if (raw_buffer != NULL)
	read_memory (addr, raw_buffer, REGISTER_RAW_SIZE (regnum));
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

/* Push an empty stack frame, and record in it the current PC, regs, etc.

   We save the non-windowed registers and the ins.  The locals and outs
   are new; they don't need to be saved. The i's and l's of
   the last frame were already saved on the stack.  */

/* Definitely see tm-sparc.h for more doc of the frame format here.  */

#ifdef GDB_TARGET_IS_SPARC64
#define DUMMY_REG_SAVE_OFFSET (128 + 16)
#else
#define DUMMY_REG_SAVE_OFFSET 0x60
#endif

/* See tm-sparc.h for how this is calculated.  */
#define DUMMY_STACK_REG_BUF_SIZE \
(((8+8+8) * SPARC_INTREG_SIZE) + (32 * REGISTER_RAW_SIZE (FP0_REGNUM)))
#define DUMMY_STACK_SIZE (DUMMY_STACK_REG_BUF_SIZE + DUMMY_REG_SAVE_OFFSET)

void
sparc_push_dummy_frame ()
{
  CORE_ADDR sp, old_sp;
  char register_temp[DUMMY_STACK_SIZE];

  old_sp = sp = read_register (SP_REGNUM);

#ifdef GDB_TARGET_IS_SPARC64
  /* FIXME: not sure what needs to be saved here.  */
#else
  /* Y, PS, WIM, TBR, PC, NPC, FPS, CPS regs */
  read_register_bytes (REGISTER_BYTE (Y_REGNUM), &register_temp[0],
		       REGISTER_RAW_SIZE (Y_REGNUM) * 8);
#endif

  read_register_bytes (REGISTER_BYTE (O0_REGNUM),
		       &register_temp[8 * SPARC_INTREG_SIZE],
		       SPARC_INTREG_SIZE * 8);

  read_register_bytes (REGISTER_BYTE (G0_REGNUM),
		       &register_temp[16 * SPARC_INTREG_SIZE],
		       SPARC_INTREG_SIZE * 8);

  read_register_bytes (REGISTER_BYTE (FP0_REGNUM),
		       &register_temp[24 * SPARC_INTREG_SIZE],
		       FP_REGISTER_BYTES);

  sp -= DUMMY_STACK_SIZE;

  write_register (SP_REGNUM, sp);

  write_memory (sp + DUMMY_REG_SAVE_OFFSET, &register_temp[0],
		DUMMY_STACK_REG_BUF_SIZE);

  write_register (FP_REGNUM, old_sp);

  /* Set return address register for the call dummy to the current PC.  */
  write_register (I7_REGNUM, read_pc() - 8);
}

/* sparc_frame_find_saved_regs ().  This function is here only because
   pop_frame uses it.  Note there is an interesting corner case which
   I think few ports of GDB get right--if you are popping a frame
   which does not save some register that *is* saved by a more inner
   frame (such a frame will never be a dummy frame because dummy
   frames save all registers).  Rewriting pop_frame to use
   get_saved_register would solve this problem and also get rid of the
   ugly duplication between sparc_frame_find_saved_regs and
   get_saved_register.

   Stores, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.

   Note that on register window machines, we are currently making the
   assumption that window registers are being saved somewhere in the
   frame in which they are being used.  If they are stored in an
   inferior frame, find_saved_register will break.

   On the Sun 4, the only time all registers are saved is when
   a dummy frame is involved.  Otherwise, the only saved registers
   are the LOCAL and IN registers which are saved as a result
   of the "save/restore" opcodes.  This condition is determined
   by address rather than by value.

   The "pc" is not stored in a frame on the SPARC.  (What is stored
   is a return address minus 8.)  sparc_pop_frame knows how to
   deal with that.  Other routines might or might not.

   See tm-sparc.h (PUSH_FRAME and friends) for CRITICAL information
   about how this works.  */

static void sparc_frame_find_saved_regs PARAMS ((struct frame_info *,
						 struct frame_saved_regs *));

static void
sparc_frame_find_saved_regs (fi, saved_regs_addr)
     struct frame_info *fi;
     struct frame_saved_regs *saved_regs_addr;
{
  register int regnum;
  CORE_ADDR frame_addr = FRAME_FP (fi);

  if (!fi)
    fatal ("Bad frame info struct in FRAME_FIND_SAVED_REGS");

  memset (saved_regs_addr, 0, sizeof (*saved_regs_addr));

  if (fi->pc >= (fi->bottom ? fi->bottom :
		   read_register (SP_REGNUM))
      && fi->pc <= FRAME_FP(fi))
    {
      /* Dummy frame.  All but the window regs are in there somewhere. */
      for (regnum = G1_REGNUM; regnum < G1_REGNUM+7; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - G0_REGNUM) * SPARC_INTREG_SIZE
	    - (FP_REGISTER_BYTES + 8 * SPARC_INTREG_SIZE);
      for (regnum = I0_REGNUM; regnum < I0_REGNUM+8; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	    - (FP_REGISTER_BYTES + 16 * SPARC_INTREG_SIZE);
      for (regnum = FP0_REGNUM; regnum < FP0_REGNUM + 32; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - FP0_REGNUM) * 4
	    - (FP_REGISTER_BYTES);
#ifdef GDB_TARGET_IS_SPARC64
      for (regnum = FP0_REGNUM + 32; regnum < FP_MAX_REGNUM; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + 32 * 4 + (regnum - FP0_REGNUM - 32) * 8
	    - (FP_REGISTER_BYTES);
#endif
      for (regnum = Y_REGNUM; regnum < NUM_REGS; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame_addr + (regnum - Y_REGNUM) * SPARC_INTREG_SIZE - 0xe0;
	    - (FP_REGISTER_BYTES + 24 * SPARC_INTREG_SIZE);
      frame_addr = fi->bottom ?
	fi->bottom : read_register (SP_REGNUM);
    }
  else if (fi->flat)
    {
      CORE_ADDR func_start;
      find_pc_partial_function (fi->pc, NULL, &func_start, NULL);
      examine_prologue (func_start, 0, fi, saved_regs_addr);

      /* Flat register window frame.  */
      saved_regs_addr->regs[RP_REGNUM] = fi->pc_addr;
      saved_regs_addr->regs[I7_REGNUM] = fi->fp_addr;
    }
  else
    {
      /* Normal frame.  Just Local and In registers */
      frame_addr = fi->bottom ?
	fi->bottom : read_register (SP_REGNUM);
      for (regnum = L0_REGNUM; regnum < L0_REGNUM+8; regnum++)
	saved_regs_addr->regs[regnum] =
	  (frame_addr + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
	   + FRAME_SAVED_L0);
      for (regnum = I0_REGNUM; regnum < I0_REGNUM+8; regnum++)
	saved_regs_addr->regs[regnum] =
	  (frame_addr + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	   + FRAME_SAVED_I0);
    }
  if (fi->next)
    {
      if (fi->flat)
	{
	  saved_regs_addr->regs[O7_REGNUM] = fi->pc_addr;
	}
      else
	{
	  /* Pull off either the next frame pointer or the stack pointer */
	  CORE_ADDR next_next_frame_addr =
	    (fi->next->bottom ?
	     fi->next->bottom :
	     read_register (SP_REGNUM));
	  for (regnum = O0_REGNUM; regnum < O0_REGNUM+8; regnum++)
	    saved_regs_addr->regs[regnum] =
	      (next_next_frame_addr
	       + (regnum - O0_REGNUM) * SPARC_INTREG_SIZE
	       + FRAME_SAVED_I0);
	}
    }
  /* Otherwise, whatever we would get from ptrace(GETREGS) is accurate */
  saved_regs_addr->regs[SP_REGNUM] = FRAME_FP (fi);
}

/* Discard from the stack the innermost frame, restoring all saved registers.

   Note that the values stored in fsr by get_frame_saved_regs are *in
   the context of the called frame*.  What this means is that the i
   regs of fsr must be restored into the o regs of the (calling) frame that
   we pop into.  We don't care about the output regs of the calling frame,
   since unless it's a dummy frame, it won't have any output regs in it.

   We never have to bother with %l (local) regs, since the called routine's
   locals get tossed, and the calling routine's locals are already saved
   on its stack.  */

/* Definitely see tm-sparc.h for more doc of the frame format here.  */

void
sparc_pop_frame ()
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR pc;
  struct frame_saved_regs fsr;
  char raw_buffer[REGISTER_BYTES];
  int regnum;

  sparc_frame_find_saved_regs (frame, &fsr);
  if (fsr.regs[FP0_REGNUM])
    {
      read_memory (fsr.regs[FP0_REGNUM], raw_buffer, FP_REGISTER_BYTES);
      write_register_bytes (REGISTER_BYTE (FP0_REGNUM),
			    raw_buffer, FP_REGISTER_BYTES);
    }
#ifndef GDB_TARGET_IS_SPARC64
  if (fsr.regs[FPS_REGNUM])
    {
      read_memory (fsr.regs[FPS_REGNUM], raw_buffer, 4);
      write_register_bytes (REGISTER_BYTE (FPS_REGNUM), raw_buffer, 4);
    }
  if (fsr.regs[CPS_REGNUM])
    {
      read_memory (fsr.regs[CPS_REGNUM], raw_buffer, 4);
      write_register_bytes (REGISTER_BYTE (CPS_REGNUM), raw_buffer, 4);
    }
#endif
  if (fsr.regs[G1_REGNUM])
    {
      read_memory (fsr.regs[G1_REGNUM], raw_buffer, 7 * SPARC_INTREG_SIZE);
      write_register_bytes (REGISTER_BYTE (G1_REGNUM), raw_buffer,
			    7 * SPARC_INTREG_SIZE);
    }

  if (frame->flat)
    {
      /* Each register might or might not have been saved, need to test
	 individually.  */
      for (regnum = L0_REGNUM; regnum < L0_REGNUM + 8; ++regnum)
	if (fsr.regs[regnum])
	  write_register (regnum, read_memory_integer (fsr.regs[regnum],
						       SPARC_INTREG_SIZE));
      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; ++regnum)
	if (fsr.regs[regnum])
	  write_register (regnum, read_memory_integer (fsr.regs[regnum],
						       SPARC_INTREG_SIZE));

      /* Handle all outs except stack pointer (o0-o5; o7).  */
      for (regnum = O0_REGNUM; regnum < O0_REGNUM + 6; ++regnum)
	if (fsr.regs[regnum])
	  write_register (regnum, read_memory_integer (fsr.regs[regnum],
						       SPARC_INTREG_SIZE));
      if (fsr.regs[O0_REGNUM + 7])
	write_register (O0_REGNUM + 7,
			read_memory_integer (fsr.regs[O0_REGNUM + 7],
					     SPARC_INTREG_SIZE));

      write_register (SP_REGNUM, frame->frame);
    }
  else if (fsr.regs[I0_REGNUM])
    {
      CORE_ADDR sp;

      char reg_temp[REGISTER_BYTES];

      read_memory (fsr.regs[I0_REGNUM], raw_buffer, 8 * SPARC_INTREG_SIZE);

      /* Get the ins and locals which we are about to restore.  Just
	 moving the stack pointer is all that is really needed, except
	 store_inferior_registers is then going to write the ins and
	 locals from the registers array, so we need to muck with the
	 registers array.  */
      sp = fsr.regs[SP_REGNUM];
      read_memory (sp, reg_temp, SPARC_INTREG_SIZE * 16);

      /* Restore the out registers.
	 Among other things this writes the new stack pointer.  */
      write_register_bytes (REGISTER_BYTE (O0_REGNUM), raw_buffer,
			    SPARC_INTREG_SIZE * 8);

      write_register_bytes (REGISTER_BYTE (L0_REGNUM), reg_temp,
			    SPARC_INTREG_SIZE * 16);
    }
#ifndef GDB_TARGET_IS_SPARC64
  if (fsr.regs[PS_REGNUM])
    write_register (PS_REGNUM, read_memory_integer (fsr.regs[PS_REGNUM], 4));
#endif
  if (fsr.regs[Y_REGNUM])
    write_register (Y_REGNUM, read_memory_integer (fsr.regs[Y_REGNUM], REGISTER_RAW_SIZE (Y_REGNUM)));
  if (fsr.regs[PC_REGNUM])
    {
      /* Explicitly specified PC (and maybe NPC) -- just restore them. */
      write_register (PC_REGNUM, read_memory_integer (fsr.regs[PC_REGNUM],
						      REGISTER_RAW_SIZE (PC_REGNUM)));
      if (fsr.regs[NPC_REGNUM])
	write_register (NPC_REGNUM,
			read_memory_integer (fsr.regs[NPC_REGNUM],
					     REGISTER_RAW_SIZE (NPC_REGNUM)));
    }
  else if (frame->flat)
    {
      if (frame->pc_addr)
	pc = PC_ADJUST ((CORE_ADDR)
			read_memory_integer (frame->pc_addr,
					     REGISTER_RAW_SIZE (PC_REGNUM)));
      else
	{
	  /* I think this happens only in the innermost frame, if so then
	     it is a complicated way of saying
	     "pc = read_register (O7_REGNUM);".  */
	  char buf[MAX_REGISTER_RAW_SIZE];
	  get_saved_register (buf, 0, 0, frame, O7_REGNUM, 0);
	  pc = PC_ADJUST (extract_address
			  (buf, REGISTER_RAW_SIZE (O7_REGNUM)));
	}

      write_register (PC_REGNUM,  pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  else if (fsr.regs[I7_REGNUM])
    {
      /* Return address in %i7 -- adjust it, then restore PC and NPC from it */
      pc = PC_ADJUST ((CORE_ADDR) read_memory_integer (fsr.regs[I7_REGNUM],
						       SPARC_INTREG_SIZE));
      write_register (PC_REGNUM,  pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  flush_cached_frames ();
}

/* On the Sun 4 under SunOS, the compile will leave a fake insn which
   encodes the structure size being returned.  If we detect such
   a fake insn, step past it.  */

CORE_ADDR
sparc_pc_adjust(pc)
     CORE_ADDR pc;
{
  unsigned long insn;
  char buf[4];
  int err;

  err = target_read_memory (pc + 8, buf, sizeof(long));
  insn = extract_unsigned_integer (buf, 4);
  if ((err == 0) && (insn & 0xffc00000) == 0)
    return pc+12;
  else
    return pc+8;
}

/* If pc is in a shared library trampoline, return its target.
   The SunOs 4.x linker rewrites the jump table entries for PIC
   compiled modules in the main executable to bypass the dynamic linker
   with jumps of the form
	sethi %hi(addr),%g1
	jmp %g1+%lo(addr)
   and removes the corresponding jump table relocation entry in the
   dynamic relocations.
   find_solib_trampoline_target relies on the presence of the jump
   table relocation entry, so we have to detect these jump instructions
   by hand.  */

CORE_ADDR
sunos4_skip_trampoline_code (pc)
     CORE_ADDR pc;
{
  unsigned long insn1;
  char buf[4];
  int err;

  err = target_read_memory (pc, buf, 4);
  insn1 = extract_unsigned_integer (buf, 4);
  if (err == 0 && (insn1 & 0xffc00000) == 0x03000000)
    {
      unsigned long insn2;

      err = target_read_memory (pc + 4, buf, 4);
      insn2 = extract_unsigned_integer (buf, 4);
      if (err == 0 && (insn2 & 0xffffe000) == 0x81c06000)
	{
	  CORE_ADDR target_pc = (insn1 & 0x3fffff) << 10;
	  int delta = insn2 & 0x1fff;

	  /* Sign extend the displacement.  */
	  if (delta & 0x1000)
	    delta |= ~0x1fff;
	  return target_pc + delta;
	}
    }
  return find_solib_trampoline_target (pc);
}

#ifdef USE_PROC_FS	/* Target dependent support for /proc */

/*  The /proc interface divides the target machine's register set up into
    two different sets, the general register set (gregset) and the floating
    point register set (fpregset).  For each set, there is an ioctl to get
    the current register set and another ioctl to set the current values.

    The actual structure passed through the ioctl interface is, of course,
    naturally machine dependent, and is different for each set of registers.
    For the sparc for example, the general register set is typically defined
    by:

	typedef int gregset_t[38];

	#define	R_G0	0
	...
	#define	R_TBR	37

    and the floating point set by:

	typedef struct prfpregset {
		union { 
			u_long  pr_regs[32]; 
			double  pr_dregs[16];
		} pr_fr;
		void *  pr_filler;
		u_long  pr_fsr;
		u_char  pr_qcnt;
		u_char  pr_q_entrysize;
		u_char  pr_en;
		u_long  pr_q[64];
	} prfpregset_t;

    These routines provide the packing and unpacking of gregset_t and
    fpregset_t formatted data.

 */

/* Given a pointer to a general register set in /proc format (gregset_t *),
   unpack the register contents and supply them as gdb's idea of the current
   register values. */

void
supply_gregset (gregsetp)
prgregset_t *gregsetp;
{
  register int regi;
  register prgreg_t *regp = (prgreg_t *) gregsetp;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};

  /* GDB register numbers for Gn, On, Ln, In all match /proc reg numbers.  */
  for (regi = G0_REGNUM ; regi <= I7_REGNUM ; regi++)
    {
      supply_register (regi, (char *) (regp + regi));
    }

  /* These require a bit more care.  */
  supply_register (PS_REGNUM, (char *) (regp + R_PS));
  supply_register (PC_REGNUM, (char *) (regp + R_PC));
  supply_register (NPC_REGNUM,(char *) (regp + R_nPC));
  supply_register (Y_REGNUM,  (char *) (regp + R_Y));

  /* Fill inaccessible registers with zero.  */
  supply_register (WIM_REGNUM, zerobuf);
  supply_register (TBR_REGNUM, zerobuf);
  supply_register (CPS_REGNUM, zerobuf);
}

void
fill_gregset (gregsetp, regno)
prgregset_t *gregsetp;
int regno;
{
  int regi;
  register prgreg_t *regp = (prgreg_t *) gregsetp;

  for (regi = 0 ; regi <= R_I7 ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  *(regp + regi) = *(int *) &registers[REGISTER_BYTE (regi)];
	}
    }
  if ((regno == -1) || (regno == PS_REGNUM))
    {
      *(regp + R_PS) = *(int *) &registers[REGISTER_BYTE (PS_REGNUM)];
    }
  if ((regno == -1) || (regno == PC_REGNUM))
    {
      *(regp + R_PC) = *(int *) &registers[REGISTER_BYTE (PC_REGNUM)];
    }
  if ((regno == -1) || (regno == NPC_REGNUM))
    {
      *(regp + R_nPC) = *(int *) &registers[REGISTER_BYTE (NPC_REGNUM)];
    }
  if ((regno == -1) || (regno == Y_REGNUM))
    {
      *(regp + R_Y) = *(int *) &registers[REGISTER_BYTE (Y_REGNUM)];
    }
}

#if defined (FP0_REGNUM)

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), unpack the register contents and supply them as gdb's
    idea of the current floating point register values. */

void 
supply_fpregset (fpregsetp)
prfpregset_t *fpregsetp;
{
  register int regi;
  char *from;
  
  for (regi = FP0_REGNUM ; regi < FP_MAX_REGNUM ; regi++)
    {
      from = (char *) &fpregsetp->pr_fr.pr_regs[regi-FP0_REGNUM];
      supply_register (regi, from);
    }
  supply_register (FPS_REGNUM, (char *) &(fpregsetp->pr_fsr));
}

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), update the register specified by REGNO from gdb's idea
    of the current floating point register set.  If REGNO is -1, update
    them all. */
/* ??? This will probably need some changes for sparc64.  */

void
fill_fpregset (fpregsetp, regno)
prfpregset_t *fpregsetp;
int regno;
{
  int regi;
  char *to;
  char *from;

  for (regi = FP0_REGNUM ; regi < FP_MAX_REGNUM ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &fpregsetp->pr_fr.pr_regs[regi-FP0_REGNUM];
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }
  if ((regno == -1) || (regno == FPS_REGNUM))
    {
      fpregsetp->pr_fsr = *(int *) &registers[REGISTER_BYTE (FPS_REGNUM)];
    }
}

#endif	/* defined (FP0_REGNUM) */

#endif  /* USE_PROC_FS */


#ifdef GET_LONGJMP_TARGET

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

int
get_longjmp_target (pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
#define LONGJMP_TARGET_SIZE 4
  char buf[LONGJMP_TARGET_SIZE];

  jb_addr = read_register (O0_REGNUM);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  LONGJMP_TARGET_SIZE))
    return 0;

  *pc = extract_address (buf, LONGJMP_TARGET_SIZE);

  return 1;
}
#endif /* GET_LONGJMP_TARGET */

#ifdef STATIC_TRANSFORM_NAME
/* SunPRO (3.0 at least), encodes the static variables.  This is not
   related to C++ mangling, it is done for C too.  */

char *
sunpro_static_transform_name (name)
     char *name;
{
  char *p;
  if (name[0] == '$')
    {
      /* For file-local statics there will be a dollar sign, a bunch
	 of junk (the contents of which match a string given in the
	 N_OPT), a period and the name.  For function-local statics
	 there will be a bunch of junk (which seems to change the
	 second character from 'A' to 'B'), a period, the name of the
	 function, and the name.  So just skip everything before the
	 last period.  */
      p = strrchr (name, '.');
      if (p != NULL)
	name = p + 1;
    }
  return name;
}
#endif /* STATIC_TRANSFORM_NAME */

#ifdef GDB_TARGET_IS_SPARC64

/* Utilities for printing registers.
   Page numbers refer to the SPARC Architecture Manual.  */

static void dump_ccreg PARAMS ((char *, int));

static void
dump_ccreg (reg, val)
     char *reg;
     int val;
{
  /* page 41 */
  printf_unfiltered ("%s:%s,%s,%s,%s", reg,
	  val & 8 ? "N" : "NN",
	  val & 4 ? "Z" : "NZ",
	  val & 2 ? "O" : "NO",
	  val & 1 ? "C" : "NC"
  );
}

static char *
decode_asi (val)
     int val;
{
  /* page 72 */
  switch (val)
    {
    case 4 : return "ASI_NUCLEUS";
    case 0x0c : return "ASI_NUCLEUS_LITTLE";
    case 0x10 : return "ASI_AS_IF_USER_PRIMARY";
    case 0x11 : return "ASI_AS_IF_USER_SECONDARY";
    case 0x18 : return "ASI_AS_IF_USER_PRIMARY_LITTLE";
    case 0x19 : return "ASI_AS_IF_USER_SECONDARY_LITTLE";
    case 0x80 : return "ASI_PRIMARY";
    case 0x81 : return "ASI_SECONDARY";
    case 0x82 : return "ASI_PRIMARY_NOFAULT";
    case 0x83 : return "ASI_SECONDARY_NOFAULT";
    case 0x88 : return "ASI_PRIMARY_LITTLE";
    case 0x89 : return "ASI_SECONDARY_LITTLE";
    case 0x8a : return "ASI_PRIMARY_NOFAULT_LITTLE";
    case 0x8b : return "ASI_SECONDARY_NOFAULT_LITTLE";
    default : return NULL;
    }
}

/* PRINT_REGISTER_HOOK routine.
   Pretty print various registers.  */
/* FIXME: Would be nice if this did some fancy things for 32 bit sparc.  */

void
sparc_print_register_hook (regno)
     int regno;
{
  unsigned LONGEST val;

  /* Handle double/quad versions of lower 32 fp regs.  */
  if (regno >= FP0_REGNUM && regno < FP0_REGNUM + 32
      && (regno & 1) == 0)
    {
      char value[16];

      if (!read_relative_register_raw_bytes (regno, value)
	  && !read_relative_register_raw_bytes (regno + 1, value + 4))
	{
	  printf_unfiltered ("\t");
	  print_floating (value, builtin_type_double, gdb_stdout);
	}
#if 0 /* FIXME: gdb doesn't handle long doubles */
      if ((regno & 3) == 0)
	{
	  if (!read_relative_register_raw_bytes (regno + 2, value + 8)
	      && !read_relative_register_raw_bytes (regno + 3, value + 12))
	    {
	      printf_unfiltered ("\t");
	      print_floating (value, builtin_type_long_double, gdb_stdout);
	    }
	}
#endif
      return;
    }

#if 0 /* FIXME: gdb doesn't handle long doubles */
  /* Print upper fp regs as long double if appropriate.  */
  if (regno >= FP0_REGNUM + 32 && regno < FP_MAX_REGNUM
      /* We test for even numbered regs and not a multiple of 4 because
	 the upper fp regs are recorded as doubles.  */
      && (regno & 1) == 0)
    {
      char value[16];

      if (!read_relative_register_raw_bytes (regno, value)
	  && !read_relative_register_raw_bytes (regno + 1, value + 8))
	{
	  printf_unfiltered ("\t");
	  print_floating (value, builtin_type_long_double, gdb_stdout);
	}
      return;
    }
#endif

  /* FIXME: Some of these are priviledged registers.
     Not sure how they should be handled.  */

#define BITS(n, mask) ((int) (((val) >> (n)) & (mask)))

  val = read_register (regno);

  /* pages 40 - 60 */
  switch (regno)
    {
    case CCR_REGNUM :
      printf_unfiltered("\t");
      dump_ccreg ("xcc", val >> 4);
      printf_unfiltered(", ");
      dump_ccreg ("icc", val & 15);
      break;
    case FPRS_REGNUM :
      printf ("\tfef:%d, du:%d, dl:%d",
	      BITS (2, 1), BITS (1, 1), BITS (0, 1));
      break;
    case FSR_REGNUM :
      {
	static char *fcc[4] = { "=", "<", ">", "?" };
	static char *rd[4] = { "N", "0", "+", "-" };
	/* Long, yes, but I'd rather leave it as is and use a wide screen.  */
	printf ("\t0:%s, 1:%s, 2:%s, 3:%s, rd:%s, tem:%d, ns:%d, ver:%d, ftt:%d, qne:%d, aexc:%d, cexc:%d",
		fcc[BITS (10, 3)], fcc[BITS (32, 3)],
		fcc[BITS (34, 3)], fcc[BITS (36, 3)],
		rd[BITS (30, 3)], BITS (23, 31), BITS (22, 1), BITS (17, 7),
		BITS (14, 7), BITS (13, 1), BITS (5, 31), BITS (0, 31));
	break;
      }
    case ASI_REGNUM :
      {
	char *asi = decode_asi (val);
	if (asi != NULL)
	  printf ("\t%s", asi);
	break;
      }
    case VER_REGNUM :
      printf ("\tmanuf:%d, impl:%d, mask:%d, maxtl:%d, maxwin:%d",
	      BITS (48, 0xffff), BITS (32, 0xffff),
	      BITS (24, 0xff), BITS (8, 0xff), BITS (0, 31));
      break;
    case PSTATE_REGNUM :
      {
	static char *mm[4] = { "tso", "pso", "rso", "?" };
	printf ("\tcle:%d, tle:%d, mm:%s, red:%d, pef:%d, am:%d, priv:%d, ie:%d, ag:%d",
		BITS (9, 1), BITS (8, 1), mm[BITS (6, 3)], BITS (5, 1),
		BITS (4, 1), BITS (3, 1), BITS (2, 1), BITS (1, 1),
		BITS (0, 1));
	break;
      }
    case TSTATE_REGNUM :
      /* FIXME: print all 4? */
      break;
    case TT_REGNUM :
      /* FIXME: print all 4? */
      break;
    case TPC_REGNUM :
      /* FIXME: print all 4? */
      break;
    case TNPC_REGNUM :
      /* FIXME: print all 4? */
      break;
    case WSTATE_REGNUM :
      printf ("\tother:%d, normal:%d", BITS (3, 7), BITS (0, 7));
      break;
    case CWP_REGNUM :
      printf ("\t%d", BITS (0, 31));
      break;
    case CANSAVE_REGNUM :
      printf ("\t%-2d before spill", BITS (0, 31));
      break;
    case CANRESTORE_REGNUM :
      printf ("\t%-2d before fill", BITS (0, 31));
      break;
    case CLEANWIN_REGNUM :
      printf ("\t%-2d before clean", BITS (0, 31));
      break;
    case OTHERWIN_REGNUM :
      printf ("\t%d", BITS (0, 31));
      break;
    }

#undef BITS
}

#endif

void
_initialize_sparc_tdep ()
{
  tm_print_insn = print_insn_sparc;
}
