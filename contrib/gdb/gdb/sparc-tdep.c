/* Target-dependent code for the SPARC for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.

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

/* ??? Support for calling functions from gdb in sparc64 is unfinished.  */

#include "defs.h"
#include "arch-utils.h"
#include "frame.h"
#include "inferior.h"
#include "obstack.h"
#include "target.h"
#include "value.h"
#include "bfd.h"
#include "gdb_string.h"
#include "regcache.h"

#ifdef	USE_PROC_FS
#include <sys/procfs.h>
/* Prototypes for supply_gregset etc. */
#include "gregset.h"
#endif

#include "gdbcore.h"

#include "symfile.h" 	/* for 'entry_point_address' */

/*
 * Some local macros that have multi-arch and non-multi-arch versions:
 */

#if (GDB_MULTI_ARCH > 0)

/* Does the target have Floating Point registers?  */
#define SPARC_HAS_FPU     (gdbarch_tdep (current_gdbarch)->has_fpu)
/* Number of bytes devoted to Floating Point registers: */
#define FP_REGISTER_BYTES (gdbarch_tdep (current_gdbarch)->fp_register_bytes)
/* Highest numbered Floating Point register.  */
#define FP_MAX_REGNUM     (gdbarch_tdep (current_gdbarch)->fp_max_regnum)
/* Size of a general (integer) register: */
#define SPARC_INTREG_SIZE (gdbarch_tdep (current_gdbarch)->intreg_size)
/* Offset within the call dummy stack of the saved registers.  */
#define DUMMY_REG_SAVE_OFFSET (gdbarch_tdep (current_gdbarch)->reg_save_offset)

#else /* non-multi-arch */


/* Does the target have Floating Point registers?  */
#if defined(TARGET_SPARCLET) || defined(TARGET_SPARCLITE)
#define SPARC_HAS_FPU 0
#else
#define SPARC_HAS_FPU 1
#endif

/* Number of bytes devoted to Floating Point registers: */
#if (GDB_TARGET_IS_SPARC64)
#define FP_REGISTER_BYTES (64 * 4)
#else
#if (SPARC_HAS_FPU)
#define FP_REGISTER_BYTES (32 * 4)
#else
#define FP_REGISTER_BYTES 0
#endif
#endif

/* Highest numbered Floating Point register.  */
#if (GDB_TARGET_IS_SPARC64)
#define FP_MAX_REGNUM (FP0_REGNUM + 48)
#else
#define FP_MAX_REGNUM (FP0_REGNUM + 32)
#endif

/* Size of a general (integer) register: */
#define SPARC_INTREG_SIZE (REGISTER_RAW_SIZE (G0_REGNUM))

/* Offset within the call dummy stack of the saved registers.  */
#if (GDB_TARGET_IS_SPARC64)
#define DUMMY_REG_SAVE_OFFSET (128 + 16)
#else
#define DUMMY_REG_SAVE_OFFSET 0x60
#endif

#endif /* GDB_MULTI_ARCH */

struct gdbarch_tdep
  {
    int has_fpu;
    int fp_register_bytes;
    int y_regnum;
    int fp_max_regnum;
    int intreg_size;
    int reg_save_offset;
    int call_dummy_call_offset;
    int print_insn_mach;
  };

/* Now make GDB_TARGET_IS_SPARC64 a runtime test.  */
/* FIXME MVS: or try testing bfd_arch_info.arch and bfd_arch_info.mach ... 
 * define GDB_TARGET_IS_SPARC64 \
 *      (TARGET_ARCHITECTURE->arch == bfd_arch_sparc &&    \
 *      (TARGET_ARCHITECTURE->mach == bfd_mach_sparc_v9 || \
 *       TARGET_ARCHITECTURE->mach == bfd_mach_sparc_v9a))
 */

/* From infrun.c */
extern int stop_after_trap;

/* We don't store all registers immediately when requested, since they
   get sent over in large chunks anyway.  Instead, we accumulate most
   of the changes and send them over once.  "deferred_stores" keeps
   track of which sets of registers we have locally-changed copies of,
   so we only need send the groups that have changed.  */

int deferred_stores = 0;    /* Accumulated stores we want to do eventually. */


/* Some machines, such as Fujitsu SPARClite 86x, have a bi-endian mode
   where instructions are big-endian and data are little-endian.
   This flag is set when we detect that the target is of this type. */

int bi_endian = 0;


/* Fetch a single instruction.  Even on bi-endian machines
   such as sparc86x, instructions are always big-endian.  */

static unsigned long
fetch_instruction (CORE_ADDR pc)
{
  unsigned long retval;
  int i;
  unsigned char buf[4];

  read_memory (pc, buf, sizeof (buf));

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;
  for (i = 0; i < sizeof (buf); ++i)
    retval = (retval << 8) | buf[i];
  return retval;
}


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
#define X_CC(i) (((i) >> 20) & 3)
#define X_P(i) (((i) >> 19) & 1)
#define X_DISP19(i) ((((i) & 0x7ffff) ^ 0x40000) - 0x40000)
#define X_RCOND(i) (((i) >> 25) & 7)
#define X_DISP16(i) ((((((i) >> 6) && 0xc000) | ((i) & 0x3fff)) ^ 0x8000) - 0x8000)
#define X_FCN(i) (((i) >> 25) & 31)

typedef enum
{
  Error, not_branch, bicc, bicca, ba, baa, ticc, ta, done_retry
} branch_type;

/* Simulate single-step ptrace call for sun4.  Code written by Gary
   Beihl (beihl@mcc.com).  */

/* npc4 and next_pc describe the situation at the time that the
   step-breakpoint was set, not necessary the current value of NPC_REGNUM.  */
static CORE_ADDR next_pc, npc4, target;
static int brknpc4, brktrg;
typedef char binsn_quantum[BREAKPOINT_MAX];
static binsn_quantum break_mem[3];

static branch_type isbranch (long, CORE_ADDR, CORE_ADDR *);

/* single_step() is called just before we want to resume the inferior,
   if we want to single-step it but there is no hardware or kernel single-step
   support (as on all SPARCs).  We find all the possible targets of the
   coming instruction and breakpoint them.

   single_step is also called just after the inferior stops.  If we had
   set up a simulated single-step, we undo our damage.  */

void
sparc_software_single_step (enum target_signal ignore,	/* pid, but we don't need it */
			    int insert_breakpoints_p)
{
  branch_type br;
  CORE_ADDR pc;
  long pc_instruction;

  if (insert_breakpoints_p)
    {
      /* Always set breakpoint for NPC.  */
      next_pc = read_register (NPC_REGNUM);
      npc4 = next_pc + 4;	/* branch not taken */

      target_insert_breakpoint (next_pc, break_mem[0]);
      /* printf_unfiltered ("set break at %x\n",next_pc); */

      pc = read_register (PC_REGNUM);
      pc_instruction = fetch_instruction (pc);
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
      else if (GDB_TARGET_IS_SPARC64 && br == done_retry)
	{
	  brktrg = 1;
	  target_insert_breakpoint (target, break_mem[2]);
	}
    }
  else
    {
      /* Remove breakpoints */
      target_remove_breakpoint (next_pc, break_mem[0]);

      if (brknpc4)
	target_remove_breakpoint (npc4, break_mem[1]);

      if (brktrg)
	target_remove_breakpoint (target, break_mem[2]);
    }
}

struct frame_extra_info 
{
  CORE_ADDR bottom;
  int in_prologue;
  int flat;
  /* Following fields only relevant for flat frames.  */
  CORE_ADDR pc_addr;
  CORE_ADDR fp_addr;
  /* Add this to ->frame to get the value of the stack pointer at the 
     time of the register saves.  */
  int sp_offset;
};

/* Call this for each newly created frame.  For SPARC, we need to
   calculate the bottom of the frame, and do some extra work if the
   prologue has been generated via the -mflat option to GCC.  In
   particular, we need to know where the previous fp and the pc have
   been stashed, since their exact position within the frame may vary.  */

void
sparc_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  char *name;
  CORE_ADDR prologue_start, prologue_end;
  int insn;

  fi->extra_info = (struct frame_extra_info *)
    frame_obstack_alloc (sizeof (struct frame_extra_info));
  frame_saved_regs_zalloc (fi);

  fi->extra_info->bottom =
    (fi->next ?
     (fi->frame == fi->next->frame ? fi->next->extra_info->bottom : 
      fi->next->frame) : read_sp ());

  /* If fi->next is NULL, then we already set ->frame by passing read_fp()
     to create_new_frame.  */
  if (fi->next)
    {
      char *buf;

      buf = alloca (MAX_REGISTER_RAW_SIZE);

      /* Compute ->frame as if not flat.  If it is flat, we'll change
         it later.  */
      if (fi->next->next != NULL
	  && (fi->next->next->signal_handler_caller
	      || frame_in_dummy (fi->next->next))
	  && frameless_look_for_prologue (fi->next))
	{
	  /* A frameless function interrupted by a signal did not change
	     the frame pointer, fix up frame pointer accordingly.  */
	  fi->frame = FRAME_FP (fi->next);
	  fi->extra_info->bottom = fi->next->extra_info->bottom;
	}
      else
	{
	  /* Should we adjust for stack bias here? */
	  get_saved_register (buf, 0, 0, fi, FP_REGNUM, 0);
	  fi->frame = extract_address (buf, REGISTER_RAW_SIZE (FP_REGNUM));

	  if (GDB_TARGET_IS_SPARC64 && (fi->frame & 1))
	    fi->frame += 2047;
	}
    }

  /* Decide whether this is a function with a ``flat register window''
     frame.  For such functions, the frame pointer is actually in %i7.  */
  fi->extra_info->flat = 0;
  fi->extra_info->in_prologue = 0;
  if (find_pc_partial_function (fi->pc, &name, &prologue_start, &prologue_end))
    {
      /* See if the function starts with an add (which will be of a
         negative number if a flat frame) to the sp.  FIXME: Does not
         handle large frames which will need more than one instruction
         to adjust the sp.  */
      insn = fetch_instruction (prologue_start);
      if (X_OP (insn) == 2 && X_RD (insn) == 14 && X_OP3 (insn) == 0
	  && X_I (insn) && X_SIMM13 (insn) < 0)
	{
	  int offset = X_SIMM13 (insn);

	  /* Then look for a save of %i7 into the frame.  */
	  insn = fetch_instruction (prologue_start + 4);
	  if (X_OP (insn) == 3
	      && X_RD (insn) == 31
	      && X_OP3 (insn) == 4
	      && X_RS1 (insn) == 14)
	    {
	      char *buf;
	      
	      buf = alloca (MAX_REGISTER_RAW_SIZE);

	      /* We definitely have a flat frame now.  */
	      fi->extra_info->flat = 1;

	      fi->extra_info->sp_offset = offset;

	      /* Overwrite the frame's address with the value in %i7.  */
	      get_saved_register (buf, 0, 0, fi, I7_REGNUM, 0);
	      fi->frame = extract_address (buf, REGISTER_RAW_SIZE (I7_REGNUM));

	      if (GDB_TARGET_IS_SPARC64 && (fi->frame & 1))
		fi->frame += 2047;

	      /* Record where the fp got saved.  */
	      fi->extra_info->fp_addr = 
		fi->frame + fi->extra_info->sp_offset + X_SIMM13 (insn);

	      /* Also try to collect where the pc got saved to.  */
	      fi->extra_info->pc_addr = 0;
	      insn = fetch_instruction (prologue_start + 12);
	      if (X_OP (insn) == 3
		  && X_RD (insn) == 15
		  && X_OP3 (insn) == 4
		  && X_RS1 (insn) == 14)
		fi->extra_info->pc_addr = 
		  fi->frame + fi->extra_info->sp_offset + X_SIMM13 (insn);
	    }
	}
      else
	{
	  /* Check if the PC is in the function prologue before a SAVE
	     instruction has been executed yet.  If so, set the frame
	     to the current value of the stack pointer and set
	     the in_prologue flag.  */
	  CORE_ADDR addr;
	  struct symtab_and_line sal;

	  sal = find_pc_line (prologue_start, 0);
	  if (sal.line == 0)	/* no line info, use PC */
	    prologue_end = fi->pc;
	  else if (sal.end < prologue_end)
	    prologue_end = sal.end;
	  if (fi->pc < prologue_end)
	    {
	      for (addr = prologue_start; addr < fi->pc; addr += 4)
		{
		  insn = read_memory_integer (addr, 4);
		  if (X_OP (insn) == 2 && X_OP3 (insn) == 0x3c)
		    break;	/* SAVE seen, stop searching */
		}
	      if (addr >= fi->pc)
		{
		  fi->extra_info->in_prologue = 1;
		  fi->frame = read_register (SP_REGNUM);
		}
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
sparc_frame_chain (struct frame_info *frame)
{
  /* Value that will cause FRAME_CHAIN_VALID to not worry about the chain
     value.  If it really is zero, we detect it later in
     sparc_init_prev_frame.  */
  return (CORE_ADDR) 1;
}

CORE_ADDR
sparc_extract_struct_value_address (char *regbuf)
{
  return extract_address (regbuf + REGISTER_BYTE (O0_REGNUM),
			  REGISTER_RAW_SIZE (O0_REGNUM));
}

/* Find the pc saved in frame FRAME.  */

CORE_ADDR
sparc_frame_saved_pc (struct frame_info *frame)
{
  char *buf;
  CORE_ADDR addr;

  buf = alloca (MAX_REGISTER_RAW_SIZE);
  if (frame->signal_handler_caller)
    {
      /* This is the signal trampoline frame.
         Get the saved PC from the sigcontext structure.  */

#ifndef SIGCONTEXT_PC_OFFSET
#define SIGCONTEXT_PC_OFFSET 12
#endif

      CORE_ADDR sigcontext_addr;
      char *scbuf;
      int saved_pc_offset = SIGCONTEXT_PC_OFFSET;
      char *name = NULL;

      scbuf = alloca (TARGET_PTR_BIT / HOST_CHAR_BIT);

      /* Solaris2 ucbsigvechandler passes a pointer to a sigcontext
         as the third parameter.  The offset to the saved pc is 12.  */
      find_pc_partial_function (frame->pc, &name,
				(CORE_ADDR *) NULL, (CORE_ADDR *) NULL);
      if (name && STREQ (name, "ucbsigvechandler"))
	saved_pc_offset = 12;

      /* The sigcontext address is contained in register O2.  */
      get_saved_register (buf, (int *) NULL, (CORE_ADDR *) NULL,
			  frame, O0_REGNUM + 2, (enum lval_type *) NULL);
      sigcontext_addr = extract_address (buf, REGISTER_RAW_SIZE (O0_REGNUM + 2));

      /* Don't cause a memory_error when accessing sigcontext in case the
         stack layout has changed or the stack is corrupt.  */
      target_read_memory (sigcontext_addr + saved_pc_offset,
			  scbuf, sizeof (scbuf));
      return extract_address (scbuf, sizeof (scbuf));
    }
  else if (frame->extra_info->in_prologue ||
	   (frame->next != NULL &&
	    (frame->next->signal_handler_caller ||
	     frame_in_dummy (frame->next)) &&
	    frameless_look_for_prologue (frame)))
    {
      /* A frameless function interrupted by a signal did not save
         the PC, it is still in %o7.  */
      get_saved_register (buf, (int *) NULL, (CORE_ADDR *) NULL,
			  frame, O7_REGNUM, (enum lval_type *) NULL);
      return PC_ADJUST (extract_address (buf, SPARC_INTREG_SIZE));
    }
  if (frame->extra_info->flat)
    addr = frame->extra_info->pc_addr;
  else
    addr = frame->extra_info->bottom + FRAME_SAVED_I0 +
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
setup_arbitrary_frame (int argc, CORE_ADDR *argv)
{
  struct frame_info *frame;

  if (argc != 2)
    error ("Sparc frame specifications require two arguments: fp and sp");

  frame = create_new_frame (argv[0], 0);

  if (!frame)
    internal_error (__FILE__, __LINE__,
		    "create_new_frame returned invalid frame");

  frame->extra_info->bottom = argv[1];
  frame->pc = FRAME_SAVED_PC (frame);
  return frame;
}

/* Given a pc value, skip it forward past the function prologue by
   disassembling instructions that appear to be a prologue.

   If FRAMELESS_P is set, we are only testing to see if the function
   is frameless.  This allows a quicker answer.

   This routine should be more specific in its actions; making sure
   that it uses the same register in the initial prologue section.  */

static CORE_ADDR examine_prologue (CORE_ADDR, int, struct frame_info *,
				   CORE_ADDR *);

static CORE_ADDR
examine_prologue (CORE_ADDR start_pc, int frameless_p, struct frame_info *fi,
		  CORE_ADDR *saved_regs)
{
  int insn;
  int dest = -1;
  CORE_ADDR pc = start_pc;
  int is_flat = 0;

  insn = fetch_instruction (pc);

  /* Recognize the `sethi' insn and record its destination.  */
  if (X_OP (insn) == 0 && X_OP2 (insn) == 4)
    {
      dest = X_RD (insn);
      pc += 4;
      insn = fetch_instruction (pc);
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
      insn = fetch_instruction (pc);
    }

  /* Recognize any SAVE insn.  */
  if (X_OP (insn) == 2 && X_OP3 (insn) == 60)
    {
      pc += 4;
      if (frameless_p)		/* If the save is all we care about, */
	return pc;		/* return before doing more work */
      insn = fetch_instruction (pc);
    }
  /* Recognize add to %sp.  */
  else if (X_OP (insn) == 2 && X_RD (insn) == 14 && X_OP3 (insn) == 0)
    {
      pc += 4;
      if (frameless_p)		/* If the add is all we care about, */
	return pc;		/* return before doing more work */
      is_flat = 1;
      insn = fetch_instruction (pc);
      /* Recognize store of frame pointer (i7).  */
      if (X_OP (insn) == 3
	  && X_RD (insn) == 31
	  && X_OP3 (insn) == 4
	  && X_RS1 (insn) == 14)
	{
	  pc += 4;
	  insn = fetch_instruction (pc);

	  /* Recognize sub %sp, <anything>, %i7.  */
	  if (X_OP (insn) == 2
	      && X_OP3 (insn) == 4
	      && X_RS1 (insn) == 14
	      && X_RD (insn) == 31)
	    {
	      pc += 4;
	      insn = fetch_instruction (pc);
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
         This recognizes all non alternate stores of an input register,
         into a location offset from the frame pointer between
	 +68 and +92.  */

      /* The above will fail for arguments that are promoted 
	 (eg. shorts to ints or floats to doubles), because the compiler
	 will pass them in positive-offset frame space, but the prologue
	 will save them (after conversion) in negative frame space at an
	 unpredictable offset.  Therefore I am going to remove the 
	 restriction on the target-address of the save, on the theory
	 that any unbroken sequence of saves from input registers must
	 be part of the prologue.  In un-optimized code (at least), I'm
	 fairly sure that the compiler would emit SOME other instruction
	 (eg. a move or add) before emitting another save that is actually
	 a part of the function body.

	 Besides, the reserved stack space is different for SPARC64 anyway.

	 MVS  4/23/2000  */

      if (X_OP (insn) == 3
	  && (X_OP3 (insn) & 0x3c)	 == 4	/* Store, non-alternate.  */
	  && (X_RD (insn) & 0x18) == 0x18	/* Input register.  */
	  && X_I (insn)				/* Immediate mode.  */
	  && X_RS1 (insn) == 30)		/* Off of frame pointer.  */
	; /* empty statement -- fall thru to end of loop */
      else if (GDB_TARGET_IS_SPARC64
	       && X_OP (insn) == 3
	       && (X_OP3 (insn) & 0x3c) == 12	/* store, extended (64-bit) */
	       && (X_RD (insn) & 0x18) == 0x18	/* input register */
	       && X_I (insn)			/* immediate mode */
	       && X_RS1 (insn) == 30)		/* off of frame pointer */
	; /* empty statement -- fall thru to end of loop */
      else if (X_OP (insn) == 3
	       && (X_OP3 (insn) & 0x3c) == 36	/* store, floating-point */
	       && X_I (insn)			/* immediate mode */
	       && X_RS1 (insn) == 30)		/* off of frame pointer */
	; /* empty statement -- fall thru to end of loop */
      else if (is_flat
	       && X_OP (insn) == 3
	       && X_OP3 (insn) == 4		/* store? */
	       && X_RS1 (insn) == 14)		/* off of frame pointer */
	{
	  if (saved_regs && X_I (insn))
	    saved_regs[X_RD (insn)] =
	      fi->frame + fi->extra_info->sp_offset + X_SIMM13 (insn);
	}
      else
	break;
      pc += 4;
      insn = fetch_instruction (pc);
    }

  return pc;
}

CORE_ADDR
sparc_skip_prologue (CORE_ADDR start_pc, int frameless_p)
{
  return examine_prologue (start_pc, frameless_p, NULL, NULL);
}

/* Check instruction at ADDR to see if it is a branch.
   All non-annulled instructions will go to NPC or will trap.
   Set *TARGET if we find a candidate branch; set to zero if not.

   This isn't static as it's used by remote-sa.sparc.c.  */

static branch_type
isbranch (long instruction, CORE_ADDR addr, CORE_ADDR *target)
{
  branch_type val = not_branch;
  long int offset = 0;		/* Must be signed for sign-extend.  */

  *target = 0;

  if (X_OP (instruction) == 0
      && (X_OP2 (instruction) == 2
	  || X_OP2 (instruction) == 6
	  || X_OP2 (instruction) == 1
	  || X_OP2 (instruction) == 3
	  || X_OP2 (instruction) == 5
	  || (GDB_TARGET_IS_SPARC64 && X_OP2 (instruction) == 7)))
    {
      if (X_COND (instruction) == 8)
	val = X_A (instruction) ? baa : ba;
      else
	val = X_A (instruction) ? bicca : bicc;
      switch (X_OP2 (instruction))
	{
	case 7:
	if (!GDB_TARGET_IS_SPARC64)
	  break;
	/* else fall thru */
	case 2:
	case 6:
	  offset = 4 * X_DISP22 (instruction);
	  break;
	case 1:
	case 5:
	  offset = 4 * X_DISP19 (instruction);
	  break;
	case 3:
	  offset = 4 * X_DISP16 (instruction);
	  break;
	}
      *target = addr + offset;
    }
  else if (GDB_TARGET_IS_SPARC64
	   && X_OP (instruction) == 2
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

  return val;
}

/* Find register number REGNUM relative to FRAME and put its
   (raw) contents in *RAW_BUFFER.  Set *OPTIMIZED if the variable
   was optimized out (and thus can't be fetched).  If the variable
   was fetched from memory, set *ADDRP to where it was fetched from,
   otherwise it was fetched from a register.

   The argument RAW_BUFFER must point to aligned memory.  */

void
sparc_get_saved_register (char *raw_buffer, int *optimized, CORE_ADDR *addrp,
			  struct frame_info *frame, int regnum,
			  enum lval_type *lval)
{
  struct frame_info *frame1;
  CORE_ADDR addr;

  if (!target_has_registers)
    error ("No registers.");

  if (optimized)
    *optimized = 0;

  addr = 0;

  /* FIXME This code extracted from infcmd.c; should put elsewhere! */
  if (frame == NULL)
    {
      /* error ("No selected frame."); */
      if (!target_has_registers)
	error ("The program has no registers now.");
      if (selected_frame == NULL)
	error ("No selected frame.");
      /* Try to use selected frame */
      frame = get_prev_frame (selected_frame);
      if (frame == 0)
	error ("Cmd not meaningful in the outermost frame.");
    }


  frame1 = frame->next;

  /* Get saved PC from the frame info if not in innermost frame.  */
  if (regnum == PC_REGNUM && frame1 != NULL)
    {
      if (lval != NULL)
	*lval = not_lval;
      if (raw_buffer != NULL)
	{
	  /* Put it back in target format.  */
	  store_address (raw_buffer, REGISTER_RAW_SIZE (regnum), frame->pc);
	}
      if (addrp != NULL)
	*addrp = 0;
      return;
    }

  while (frame1 != NULL)
    {
      /* FIXME MVS: wrong test for dummy frame at entry.  */

      if (frame1->pc >= (frame1->extra_info->bottom ? 
			 frame1->extra_info->bottom : read_sp ())
	  && frame1->pc <= FRAME_FP (frame1))
	{
	  /* Dummy frame.  All but the window regs are in there somewhere.
	     The window registers are saved on the stack, just like in a
	     normal frame.  */
	  if (regnum >= G1_REGNUM && regnum < G1_REGNUM + 7)
	    addr = frame1->frame + (regnum - G0_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 8 * SPARC_INTREG_SIZE);
	  else if (regnum >= I0_REGNUM && regnum < I0_REGNUM + 8)
	    addr = (frame1->prev->extra_info->bottom
		    + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_I0);
	  else if (regnum >= L0_REGNUM && regnum < L0_REGNUM + 8)
	    addr = (frame1->prev->extra_info->bottom
		    + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_L0);
	  else if (regnum >= O0_REGNUM && regnum < O0_REGNUM + 8)
	    addr = frame1->frame + (regnum - O0_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 16 * SPARC_INTREG_SIZE);
	  else if (SPARC_HAS_FPU &&
		   regnum >= FP0_REGNUM && regnum < FP0_REGNUM + 32)
	    addr = frame1->frame + (regnum - FP0_REGNUM) * 4
	      - (FP_REGISTER_BYTES);
	  else if (GDB_TARGET_IS_SPARC64 && SPARC_HAS_FPU && 
		   regnum >= FP0_REGNUM + 32 && regnum < FP_MAX_REGNUM)
	    addr = frame1->frame + 32 * 4 + (regnum - FP0_REGNUM - 32) * 8
	      - (FP_REGISTER_BYTES);
	  else if (regnum >= Y_REGNUM && regnum < NUM_REGS)
	    addr = frame1->frame + (regnum - Y_REGNUM) * SPARC_INTREG_SIZE
	      - (FP_REGISTER_BYTES + 24 * SPARC_INTREG_SIZE);
	}
      else if (frame1->extra_info->flat)
	{

	  if (regnum == RP_REGNUM)
	    addr = frame1->extra_info->pc_addr;
	  else if (regnum == I7_REGNUM)
	    addr = frame1->extra_info->fp_addr;
	  else
	    {
	      CORE_ADDR func_start;
	      CORE_ADDR *regs;

	      regs = alloca (NUM_REGS * sizeof (CORE_ADDR)); 
	      memset (regs, 0, NUM_REGS * sizeof (CORE_ADDR));

	      find_pc_partial_function (frame1->pc, NULL, &func_start, NULL);
	      examine_prologue (func_start, 0, frame1, regs);
	      addr = regs[regnum];
	    }
	}
      else
	{
	  /* Normal frame.  Local and In registers are saved on stack.  */
	  if (regnum >= I0_REGNUM && regnum < I0_REGNUM + 8)
	    addr = (frame1->prev->extra_info->bottom
		    + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
		    + FRAME_SAVED_I0);
	  else if (regnum >= L0_REGNUM && regnum < L0_REGNUM + 8)
	    addr = (frame1->prev->extra_info->bottom
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

/* See tm-sparc.h for how this is calculated.  */

#define DUMMY_STACK_REG_BUF_SIZE \
     (((8+8+8) * SPARC_INTREG_SIZE) + FP_REGISTER_BYTES)
#define DUMMY_STACK_SIZE \
     (DUMMY_STACK_REG_BUF_SIZE + DUMMY_REG_SAVE_OFFSET)

void
sparc_push_dummy_frame (void)
{
  CORE_ADDR sp, old_sp;
  char *register_temp;

  register_temp = alloca (DUMMY_STACK_SIZE);

  old_sp = sp = read_sp ();

  if (GDB_TARGET_IS_SPARC64)
    {
      /* PC, NPC, CCR, FSR, FPRS, Y, ASI */
      read_register_bytes (REGISTER_BYTE (PC_REGNUM), &register_temp[0],
			   REGISTER_RAW_SIZE (PC_REGNUM) * 7);
      read_register_bytes (REGISTER_BYTE (PSTATE_REGNUM), 
			   &register_temp[7 * SPARC_INTREG_SIZE],
			   REGISTER_RAW_SIZE (PSTATE_REGNUM));
      /* FIXME: not sure what needs to be saved here.  */
    }
  else
    {
      /* Y, PS, WIM, TBR, PC, NPC, FPS, CPS regs */
      read_register_bytes (REGISTER_BYTE (Y_REGNUM), &register_temp[0],
			   REGISTER_RAW_SIZE (Y_REGNUM) * 8);
    }

  read_register_bytes (REGISTER_BYTE (O0_REGNUM),
		       &register_temp[8 * SPARC_INTREG_SIZE],
		       SPARC_INTREG_SIZE * 8);

  read_register_bytes (REGISTER_BYTE (G0_REGNUM),
		       &register_temp[16 * SPARC_INTREG_SIZE],
		       SPARC_INTREG_SIZE * 8);

  if (SPARC_HAS_FPU)
    read_register_bytes (REGISTER_BYTE (FP0_REGNUM),
			 &register_temp[24 * SPARC_INTREG_SIZE],
			 FP_REGISTER_BYTES);

  sp -= DUMMY_STACK_SIZE;

  write_sp (sp);

  write_memory (sp + DUMMY_REG_SAVE_OFFSET, &register_temp[0],
		DUMMY_STACK_REG_BUF_SIZE);

  if (strcmp (target_shortname, "sim") != 0)
    {
      write_fp (old_sp);

      /* Set return address register for the call dummy to the current PC.  */
      write_register (I7_REGNUM, read_pc () - 8);
    }
  else
    {
      /* The call dummy will write this value to FP before executing
         the 'save'.  This ensures that register window flushes work
         correctly in the simulator.  */
      write_register (G0_REGNUM + 1, read_register (FP_REGNUM));

      /* The call dummy will write this value to FP after executing
         the 'save'. */
      write_register (G0_REGNUM + 2, old_sp);

      /* The call dummy will write this value to the return address (%i7) after
         executing the 'save'. */
      write_register (G0_REGNUM + 3, read_pc () - 8);

      /* Set the FP that the call dummy will be using after the 'save'.
         This makes backtraces from an inferior function call work properly.  */
      write_register (FP_REGNUM, old_sp);
    }
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

   Stores, into an array of CORE_ADDR, 
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

   See tm-sparc.h (PUSH_DUMMY_FRAME and friends) for CRITICAL information
   about how this works.  */

static void sparc_frame_find_saved_regs (struct frame_info *, CORE_ADDR *);

static void
sparc_frame_find_saved_regs (struct frame_info *fi, CORE_ADDR *saved_regs_addr)
{
  register int regnum;
  CORE_ADDR frame_addr = FRAME_FP (fi);

  if (!fi)
    internal_error (__FILE__, __LINE__,
		    "Bad frame info struct in FRAME_FIND_SAVED_REGS");

  memset (saved_regs_addr, 0, NUM_REGS * sizeof (CORE_ADDR));

  if (fi->pc >= (fi->extra_info->bottom ? 
		 fi->extra_info->bottom : read_sp ())
      && fi->pc <= FRAME_FP (fi))
    {
      /* Dummy frame.  All but the window regs are in there somewhere. */
      for (regnum = G1_REGNUM; regnum < G1_REGNUM + 7; regnum++)
	saved_regs_addr[regnum] =
	  frame_addr + (regnum - G0_REGNUM) * SPARC_INTREG_SIZE
	  - DUMMY_STACK_REG_BUF_SIZE + 16 * SPARC_INTREG_SIZE;

      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; regnum++)
	saved_regs_addr[regnum] =
	  frame_addr + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	  - DUMMY_STACK_REG_BUF_SIZE + 8 * SPARC_INTREG_SIZE;

      if (SPARC_HAS_FPU)
	for (regnum = FP0_REGNUM; regnum < FP_MAX_REGNUM; regnum++)
	  saved_regs_addr[regnum] = frame_addr + (regnum - FP0_REGNUM) * 4
	    - DUMMY_STACK_REG_BUF_SIZE + 24 * SPARC_INTREG_SIZE;

      if (GDB_TARGET_IS_SPARC64)
	{
	  for (regnum = PC_REGNUM; regnum < PC_REGNUM + 7; regnum++)
	    {
	      saved_regs_addr[regnum] =
		frame_addr + (regnum - PC_REGNUM) * SPARC_INTREG_SIZE
		- DUMMY_STACK_REG_BUF_SIZE;
	    }
	  saved_regs_addr[PSTATE_REGNUM] =
	    frame_addr + 8 * SPARC_INTREG_SIZE - DUMMY_STACK_REG_BUF_SIZE;
	}
      else
	for (regnum = Y_REGNUM; regnum < NUM_REGS; regnum++)
	  saved_regs_addr[regnum] =
	    frame_addr + (regnum - Y_REGNUM) * SPARC_INTREG_SIZE
	    - DUMMY_STACK_REG_BUF_SIZE;

      frame_addr = fi->extra_info->bottom ?
	fi->extra_info->bottom : read_sp ();
    }
  else if (fi->extra_info->flat)
    {
      CORE_ADDR func_start;
      find_pc_partial_function (fi->pc, NULL, &func_start, NULL);
      examine_prologue (func_start, 0, fi, saved_regs_addr);

      /* Flat register window frame.  */
      saved_regs_addr[RP_REGNUM] = fi->extra_info->pc_addr;
      saved_regs_addr[I7_REGNUM] = fi->extra_info->fp_addr;
    }
  else
    {
      /* Normal frame.  Just Local and In registers */
      frame_addr = fi->extra_info->bottom ?
	fi->extra_info->bottom : read_sp ();
      for (regnum = L0_REGNUM; regnum < L0_REGNUM + 8; regnum++)
	saved_regs_addr[regnum] =
	  (frame_addr + (regnum - L0_REGNUM) * SPARC_INTREG_SIZE
	   + FRAME_SAVED_L0);
      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; regnum++)
	saved_regs_addr[regnum] =
	  (frame_addr + (regnum - I0_REGNUM) * SPARC_INTREG_SIZE
	   + FRAME_SAVED_I0);
    }
  if (fi->next)
    {
      if (fi->extra_info->flat)
	{
	  saved_regs_addr[O7_REGNUM] = fi->extra_info->pc_addr;
	}
      else
	{
	  /* Pull off either the next frame pointer or the stack pointer */
	  CORE_ADDR next_next_frame_addr =
	  (fi->next->extra_info->bottom ?
	   fi->next->extra_info->bottom : read_sp ());
	  for (regnum = O0_REGNUM; regnum < O0_REGNUM + 8; regnum++)
	    saved_regs_addr[regnum] =
	      (next_next_frame_addr
	       + (regnum - O0_REGNUM) * SPARC_INTREG_SIZE
	       + FRAME_SAVED_I0);
	}
    }
  /* Otherwise, whatever we would get from ptrace(GETREGS) is accurate */
  /* FIXME -- should this adjust for the sparc64 offset? */
  saved_regs_addr[SP_REGNUM] = FRAME_FP (fi);
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
sparc_pop_frame (void)
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR pc;
  CORE_ADDR *fsr;
  char *raw_buffer;
  int regnum;

  fsr = alloca (NUM_REGS * sizeof (CORE_ADDR));
  raw_buffer = alloca (REGISTER_BYTES);
  sparc_frame_find_saved_regs (frame, &fsr[0]);
  if (SPARC_HAS_FPU)
    {
      if (fsr[FP0_REGNUM])
	{
	  read_memory (fsr[FP0_REGNUM], raw_buffer, FP_REGISTER_BYTES);
	  write_register_bytes (REGISTER_BYTE (FP0_REGNUM),
				raw_buffer, FP_REGISTER_BYTES);
	}
      if (!(GDB_TARGET_IS_SPARC64))
	{
	  if (fsr[FPS_REGNUM])
	    {
	      read_memory (fsr[FPS_REGNUM], raw_buffer, SPARC_INTREG_SIZE);
	      write_register_gen (FPS_REGNUM, raw_buffer);
	    }
	  if (fsr[CPS_REGNUM])
	    {
	      read_memory (fsr[CPS_REGNUM], raw_buffer, SPARC_INTREG_SIZE);
	      write_register_gen (CPS_REGNUM, raw_buffer);
	    }
	}
    }
  if (fsr[G1_REGNUM])
    {
      read_memory (fsr[G1_REGNUM], raw_buffer, 7 * SPARC_INTREG_SIZE);
      write_register_bytes (REGISTER_BYTE (G1_REGNUM), raw_buffer,
			    7 * SPARC_INTREG_SIZE);
    }

  if (frame->extra_info->flat)
    {
      /* Each register might or might not have been saved, need to test
         individually.  */
      for (regnum = L0_REGNUM; regnum < L0_REGNUM + 8; ++regnum)
	if (fsr[regnum])
	  write_register (regnum, read_memory_integer (fsr[regnum],
						       SPARC_INTREG_SIZE));
      for (regnum = I0_REGNUM; regnum < I0_REGNUM + 8; ++regnum)
	if (fsr[regnum])
	  write_register (regnum, read_memory_integer (fsr[regnum],
						       SPARC_INTREG_SIZE));

      /* Handle all outs except stack pointer (o0-o5; o7).  */
      for (regnum = O0_REGNUM; regnum < O0_REGNUM + 6; ++regnum)
	if (fsr[regnum])
	  write_register (regnum, read_memory_integer (fsr[regnum],
						       SPARC_INTREG_SIZE));
      if (fsr[O0_REGNUM + 7])
	write_register (O0_REGNUM + 7,
			read_memory_integer (fsr[O0_REGNUM + 7],
					     SPARC_INTREG_SIZE));

      write_sp (frame->frame);
    }
  else if (fsr[I0_REGNUM])
    {
      CORE_ADDR sp;

      char *reg_temp;

      reg_temp = alloca (REGISTER_BYTES);

      read_memory (fsr[I0_REGNUM], raw_buffer, 8 * SPARC_INTREG_SIZE);

      /* Get the ins and locals which we are about to restore.  Just
         moving the stack pointer is all that is really needed, except
         store_inferior_registers is then going to write the ins and
         locals from the registers array, so we need to muck with the
         registers array.  */
      sp = fsr[SP_REGNUM];
 
      if (GDB_TARGET_IS_SPARC64 && (sp & 1))
	sp += 2047;

      read_memory (sp, reg_temp, SPARC_INTREG_SIZE * 16);

      /* Restore the out registers.
         Among other things this writes the new stack pointer.  */
      write_register_bytes (REGISTER_BYTE (O0_REGNUM), raw_buffer,
			    SPARC_INTREG_SIZE * 8);

      write_register_bytes (REGISTER_BYTE (L0_REGNUM), reg_temp,
			    SPARC_INTREG_SIZE * 16);
    }

  if (!(GDB_TARGET_IS_SPARC64))
    if (fsr[PS_REGNUM])
      write_register (PS_REGNUM, 
		      read_memory_integer (fsr[PS_REGNUM], 
					   REGISTER_RAW_SIZE (PS_REGNUM)));

  if (fsr[Y_REGNUM])
    write_register (Y_REGNUM, 
		    read_memory_integer (fsr[Y_REGNUM], 
					 REGISTER_RAW_SIZE (Y_REGNUM)));
  if (fsr[PC_REGNUM])
    {
      /* Explicitly specified PC (and maybe NPC) -- just restore them. */
      write_register (PC_REGNUM, 
		      read_memory_integer (fsr[PC_REGNUM],
					   REGISTER_RAW_SIZE (PC_REGNUM)));
      if (fsr[NPC_REGNUM])
	write_register (NPC_REGNUM,
			read_memory_integer (fsr[NPC_REGNUM],
					     REGISTER_RAW_SIZE (NPC_REGNUM)));
    }
  else if (frame->extra_info->flat)
    {
      if (frame->extra_info->pc_addr)
	pc = PC_ADJUST ((CORE_ADDR)
			read_memory_integer (frame->extra_info->pc_addr,
					     REGISTER_RAW_SIZE (PC_REGNUM)));
      else
	{
	  /* I think this happens only in the innermost frame, if so then
	     it is a complicated way of saying
	     "pc = read_register (O7_REGNUM);".  */
	  char *buf;

	  buf = alloca (MAX_REGISTER_RAW_SIZE);
	  get_saved_register (buf, 0, 0, frame, O7_REGNUM, 0);
	  pc = PC_ADJUST (extract_address
			  (buf, REGISTER_RAW_SIZE (O7_REGNUM)));
	}

      write_register (PC_REGNUM, pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  else if (fsr[I7_REGNUM])
    {
      /* Return address in %i7 -- adjust it, then restore PC and NPC from it */
      pc = PC_ADJUST ((CORE_ADDR) read_memory_integer (fsr[I7_REGNUM],
						       SPARC_INTREG_SIZE));
      write_register (PC_REGNUM, pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  flush_cached_frames ();
}

/* On the Sun 4 under SunOS, the compile will leave a fake insn which
   encodes the structure size being returned.  If we detect such
   a fake insn, step past it.  */

CORE_ADDR
sparc_pc_adjust (CORE_ADDR pc)
{
  unsigned long insn;
  char buf[4];
  int err;

  err = target_read_memory (pc + 8, buf, 4);
  insn = extract_unsigned_integer (buf, 4);
  if ((err == 0) && (insn & 0xffc00000) == 0)
    return pc + 12;
  else
    return pc + 8;
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
sunos4_skip_trampoline_code (CORE_ADDR pc)
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

#ifdef USE_PROC_FS		/* Target dependent support for /proc */
/* *INDENT-OFF* */
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
/* *INDENT-ON* */

/* Given a pointer to a general register set in /proc format (gregset_t *),
   unpack the register contents and supply them as gdb's idea of the current
   register values. */

void
supply_gregset (gdb_gregset_t *gregsetp)
{
  prgreg_t *regp = (prgreg_t *) gregsetp;
  int regi, offset = 0;

  /* If the host is 64-bit sparc, but the target is 32-bit sparc, 
     then the gregset may contain 64-bit ints while supply_register
     is expecting 32-bit ints.  Compensate.  */
  if (sizeof (regp[0]) == 8 && SPARC_INTREG_SIZE == 4)
    offset = 4;

  /* GDB register numbers for Gn, On, Ln, In all match /proc reg numbers.  */
  /* FIXME MVS: assumes the order of the first 32 elements... */
  for (regi = G0_REGNUM; regi <= I7_REGNUM; regi++)
    {
      supply_register (regi, ((char *) (regp + regi)) + offset);
    }

  /* These require a bit more care.  */
  supply_register (PC_REGNUM, ((char *) (regp + R_PC)) + offset);
  supply_register (NPC_REGNUM, ((char *) (regp + R_nPC)) + offset);
  supply_register (Y_REGNUM, ((char *) (regp + R_Y)) + offset);

  if (GDB_TARGET_IS_SPARC64)
    {
#ifdef R_CCR
      supply_register (CCR_REGNUM, ((char *) (regp + R_CCR)) + offset);
#else
      supply_register (CCR_REGNUM, NULL);
#endif
#ifdef R_FPRS
      supply_register (FPRS_REGNUM, ((char *) (regp + R_FPRS)) + offset);
#else
      supply_register (FPRS_REGNUM, NULL);
#endif
#ifdef R_ASI
      supply_register (ASI_REGNUM, ((char *) (regp + R_ASI)) + offset);
#else
      supply_register (ASI_REGNUM, NULL);
#endif
    }
  else	/* sparc32 */
    {
#ifdef R_PS
      supply_register (PS_REGNUM, ((char *) (regp + R_PS)) + offset);
#else
      supply_register (PS_REGNUM, NULL);
#endif

      /* For 64-bit hosts, R_WIM and R_TBR may not be defined.
	 Steal R_ASI and R_FPRS, and hope for the best!  */

#if !defined (R_WIM) && defined (R_ASI)
#define R_WIM R_ASI
#endif

#if !defined (R_TBR) && defined (R_FPRS)
#define R_TBR R_FPRS
#endif

#if defined (R_WIM)
      supply_register (WIM_REGNUM, ((char *) (regp + R_WIM)) + offset);
#else
      supply_register (WIM_REGNUM, NULL);
#endif

#if defined (R_TBR)
      supply_register (TBR_REGNUM, ((char *) (regp + R_TBR)) + offset);
#else
      supply_register (TBR_REGNUM, NULL);
#endif
    }

  /* Fill inaccessible registers with zero.  */
  if (GDB_TARGET_IS_SPARC64)
    {
      /*
       * don't know how to get value of any of the following:
       */
      supply_register (VER_REGNUM, NULL);
      supply_register (TICK_REGNUM, NULL);
      supply_register (PIL_REGNUM, NULL);
      supply_register (PSTATE_REGNUM, NULL);
      supply_register (TSTATE_REGNUM, NULL);
      supply_register (TBA_REGNUM, NULL);
      supply_register (TL_REGNUM, NULL);
      supply_register (TT_REGNUM, NULL);
      supply_register (TPC_REGNUM, NULL);
      supply_register (TNPC_REGNUM, NULL);
      supply_register (WSTATE_REGNUM, NULL);
      supply_register (CWP_REGNUM, NULL);
      supply_register (CANSAVE_REGNUM, NULL);
      supply_register (CANRESTORE_REGNUM, NULL);
      supply_register (CLEANWIN_REGNUM, NULL);
      supply_register (OTHERWIN_REGNUM, NULL);
      supply_register (ASR16_REGNUM, NULL);
      supply_register (ASR17_REGNUM, NULL);
      supply_register (ASR18_REGNUM, NULL);
      supply_register (ASR19_REGNUM, NULL);
      supply_register (ASR20_REGNUM, NULL);
      supply_register (ASR21_REGNUM, NULL);
      supply_register (ASR22_REGNUM, NULL);
      supply_register (ASR23_REGNUM, NULL);
      supply_register (ASR24_REGNUM, NULL);
      supply_register (ASR25_REGNUM, NULL);
      supply_register (ASR26_REGNUM, NULL);
      supply_register (ASR27_REGNUM, NULL);
      supply_register (ASR28_REGNUM, NULL);
      supply_register (ASR29_REGNUM, NULL);
      supply_register (ASR30_REGNUM, NULL);
      supply_register (ASR31_REGNUM, NULL);
      supply_register (ICC_REGNUM, NULL);
      supply_register (XCC_REGNUM, NULL);
    }
  else
    {
      supply_register (CPS_REGNUM, NULL);
    }
}

void
fill_gregset (gdb_gregset_t *gregsetp, int regno)
{
  prgreg_t *regp = (prgreg_t *) gregsetp;
  int regi, offset = 0;

  /* If the host is 64-bit sparc, but the target is 32-bit sparc, 
     then the gregset may contain 64-bit ints while supply_register
     is expecting 32-bit ints.  Compensate.  */
  if (sizeof (regp[0]) == 8 && SPARC_INTREG_SIZE == 4)
    offset = 4;

  for (regi = 0; regi <= R_I7; regi++)
    if ((regno == -1) || (regno == regi))
      read_register_gen (regi, (char *) (regp + regi) + offset);

  if ((regno == -1) || (regno == PC_REGNUM))
    read_register_gen (PC_REGNUM, (char *) (regp + R_PC) + offset);

  if ((regno == -1) || (regno == NPC_REGNUM))
    read_register_gen (NPC_REGNUM, (char *) (regp + R_nPC) + offset);

  if ((regno == -1) || (regno == Y_REGNUM))
    read_register_gen (Y_REGNUM, (char *) (regp + R_Y) + offset);

  if (GDB_TARGET_IS_SPARC64)
    {
#ifdef R_CCR
      if (regno == -1 || regno == CCR_REGNUM)
	read_register_gen (CCR_REGNUM, ((char *) (regp + R_CCR)) + offset);
#endif
#ifdef R_FPRS
      if (regno == -1 || regno == FPRS_REGNUM)
	read_register_gen (FPRS_REGNUM, ((char *) (regp + R_FPRS)) + offset);
#endif
#ifdef R_ASI
      if (regno == -1 || regno == ASI_REGNUM)
	read_register_gen (ASI_REGNUM, ((char *) (regp + R_ASI)) + offset);
#endif
    }
  else /* sparc32 */
    {
#ifdef R_PS
      if (regno == -1 || regno == PS_REGNUM)
	read_register_gen (PS_REGNUM, ((char *) (regp + R_PS)) + offset);
#endif

      /* For 64-bit hosts, R_WIM and R_TBR may not be defined.
	 Steal R_ASI and R_FPRS, and hope for the best!  */

#if !defined (R_WIM) && defined (R_ASI)
#define R_WIM R_ASI
#endif

#if !defined (R_TBR) && defined (R_FPRS)
#define R_TBR R_FPRS
#endif

#if defined (R_WIM)
      if (regno == -1 || regno == WIM_REGNUM)
	read_register_gen (WIM_REGNUM, ((char *) (regp + R_WIM)) + offset);
#else
      if (regno == -1 || regno == WIM_REGNUM)
	read_register_gen (WIM_REGNUM, NULL);
#endif

#if defined (R_TBR)
      if (regno == -1 || regno == TBR_REGNUM)
	read_register_gen (TBR_REGNUM, ((char *) (regp + R_TBR)) + offset);
#else
      if (regno == -1 || regno == TBR_REGNUM)
	read_register_gen (TBR_REGNUM, NULL);
#endif
    }
}

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), unpack the register contents and supply them as gdb's
   idea of the current floating point register values. */

void
supply_fpregset (gdb_fpregset_t *fpregsetp)
{
  register int regi;
  char *from;

  if (!SPARC_HAS_FPU)
    return;

  for (regi = FP0_REGNUM; regi < FP_MAX_REGNUM; regi++)
    {
      from = (char *) &fpregsetp->pr_fr.pr_regs[regi - FP0_REGNUM];
      supply_register (regi, from);
    }

  if (GDB_TARGET_IS_SPARC64)
    {
      /*
       * don't know how to get value of the following.  
       */
      supply_register (FSR_REGNUM, NULL);	/* zero it out for now */
      supply_register (FCC0_REGNUM, NULL);
      supply_register (FCC1_REGNUM, NULL); /* don't know how to get value */
      supply_register (FCC2_REGNUM, NULL); /* don't know how to get value */
      supply_register (FCC3_REGNUM, NULL); /* don't know how to get value */
    }
  else
    {
      supply_register (FPS_REGNUM, (char *) &(fpregsetp->pr_fsr));
    }
}

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), update the register specified by REGNO from gdb's idea
   of the current floating point register set.  If REGNO is -1, update
   them all. */
/* This will probably need some changes for sparc64.  */

void
fill_fpregset (gdb_fpregset_t *fpregsetp, int regno)
{
  int regi;
  char *to;
  char *from;

  if (!SPARC_HAS_FPU)
    return;

  for (regi = FP0_REGNUM; regi < FP_MAX_REGNUM; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &fpregsetp->pr_fr.pr_regs[regi - FP0_REGNUM];
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }

  if (!(GDB_TARGET_IS_SPARC64)) /* FIXME: does Sparc64 have this register? */
    if ((regno == -1) || (regno == FPS_REGNUM))
      {
	from = (char *)&registers[REGISTER_BYTE (FPS_REGNUM)];
	to = (char *) &fpregsetp->pr_fsr;
	memcpy (to, from, REGISTER_RAW_SIZE (FPS_REGNUM));
      }
}

#endif /* USE_PROC_FS */

/* Because of Multi-arch, GET_LONGJMP_TARGET is always defined.  So test
   for a definition of JB_PC.  */
#ifdef JB_PC

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

int
get_longjmp_target (CORE_ADDR *pc)
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
sunpro_static_transform_name (char *name)
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


/* Utilities for printing registers.
   Page numbers refer to the SPARC Architecture Manual.  */

static void dump_ccreg (char *, int);

static void
dump_ccreg (char *reg, int val)
{
  /* page 41 */
  printf_unfiltered ("%s:%s,%s,%s,%s", reg,
		     val & 8 ? "N" : "NN",
		     val & 4 ? "Z" : "NZ",
		     val & 2 ? "O" : "NO",
		     val & 1 ? "C" : "NC");
}

static char *
decode_asi (int val)
{
  /* page 72 */
  switch (val)
    {
    case 4:
      return "ASI_NUCLEUS";
    case 0x0c:
      return "ASI_NUCLEUS_LITTLE";
    case 0x10:
      return "ASI_AS_IF_USER_PRIMARY";
    case 0x11:
      return "ASI_AS_IF_USER_SECONDARY";
    case 0x18:
      return "ASI_AS_IF_USER_PRIMARY_LITTLE";
    case 0x19:
      return "ASI_AS_IF_USER_SECONDARY_LITTLE";
    case 0x80:
      return "ASI_PRIMARY";
    case 0x81:
      return "ASI_SECONDARY";
    case 0x82:
      return "ASI_PRIMARY_NOFAULT";
    case 0x83:
      return "ASI_SECONDARY_NOFAULT";
    case 0x88:
      return "ASI_PRIMARY_LITTLE";
    case 0x89:
      return "ASI_SECONDARY_LITTLE";
    case 0x8a:
      return "ASI_PRIMARY_NOFAULT_LITTLE";
    case 0x8b:
      return "ASI_SECONDARY_NOFAULT_LITTLE";
    default:
      return NULL;
    }
}

/* PRINT_REGISTER_HOOK routine.
   Pretty print various registers.  */
/* FIXME: Would be nice if this did some fancy things for 32 bit sparc.  */

void
sparc_print_register_hook (int regno)
{
  ULONGEST val;

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
#if 0				/* FIXME: gdb doesn't handle long doubles */
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

#if 0				/* FIXME: gdb doesn't handle long doubles */
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
  if (GDB_TARGET_IS_SPARC64)
    switch (regno)
      {
      case CCR_REGNUM:
	printf_unfiltered ("\t");
	dump_ccreg ("xcc", val >> 4);
	printf_unfiltered (", ");
	dump_ccreg ("icc", val & 15);
	break;
      case FPRS_REGNUM:
	printf ("\tfef:%d, du:%d, dl:%d",
		BITS (2, 1), BITS (1, 1), BITS (0, 1));
	break;
      case FSR_REGNUM:
	{
	  static char *fcc[4] =
	  {"=", "<", ">", "?"};
	  static char *rd[4] =
	  {"N", "0", "+", "-"};
	  /* Long, but I'd rather leave it as is and use a wide screen.  */
	  printf_filtered ("\t0:%s, 1:%s, 2:%s, 3:%s, rd:%s, tem:%d, ",
			   fcc[BITS (10, 3)], fcc[BITS (32, 3)],
			   fcc[BITS (34, 3)], fcc[BITS (36, 3)],
			   rd[BITS (30, 3)], BITS (23, 31));
	  printf_filtered ("ns:%d, ver:%d, ftt:%d, qne:%d, aexc:%d, cexc:%d",
			   BITS (22, 1), BITS (17, 7), BITS (14, 7), 
			   BITS (13, 1), BITS (5, 31), BITS (0, 31));
	  break;
	}
      case ASI_REGNUM:
	{
	  char *asi = decode_asi (val);
	  if (asi != NULL)
	    printf ("\t%s", asi);
	  break;
	}
      case VER_REGNUM:
	printf ("\tmanuf:%d, impl:%d, mask:%d, maxtl:%d, maxwin:%d",
		BITS (48, 0xffff), BITS (32, 0xffff),
		BITS (24, 0xff), BITS (8, 0xff), BITS (0, 31));
	break;
      case PSTATE_REGNUM:
	{
	  static char *mm[4] =
	  {"tso", "pso", "rso", "?"};
	  printf_filtered ("\tcle:%d, tle:%d, mm:%s, red:%d, ",
			   BITS (9, 1), BITS (8, 1), 
			   mm[BITS (6, 3)], BITS (5, 1));
	  printf_filtered ("pef:%d, am:%d, priv:%d, ie:%d, ag:%d",
			   BITS (4, 1), BITS (3, 1), BITS (2, 1), 
			   BITS (1, 1), BITS (0, 1));
	  break;
	}
      case TSTATE_REGNUM:
	/* FIXME: print all 4? */
	break;
      case TT_REGNUM:
	/* FIXME: print all 4? */
	break;
      case TPC_REGNUM:
	/* FIXME: print all 4? */
	break;
      case TNPC_REGNUM:
	/* FIXME: print all 4? */
	break;
      case WSTATE_REGNUM:
	printf ("\tother:%d, normal:%d", BITS (3, 7), BITS (0, 7));
	break;
      case CWP_REGNUM:
	printf ("\t%d", BITS (0, 31));
	break;
      case CANSAVE_REGNUM:
	printf ("\t%-2d before spill", BITS (0, 31));
	break;
      case CANRESTORE_REGNUM:
	printf ("\t%-2d before fill", BITS (0, 31));
	break;
      case CLEANWIN_REGNUM:
	printf ("\t%-2d before clean", BITS (0, 31));
	break;
      case OTHERWIN_REGNUM:
	printf ("\t%d", BITS (0, 31));
	break;
      }
  else	/* Sparc32 */
    switch (regno) 
      {
      case PS_REGNUM:
	printf ("\ticc:%c%c%c%c, pil:%d, s:%d, ps:%d, et:%d, cwp:%d",
		BITS (23, 1) ? 'N' : '-', BITS (22, 1) ? 'Z' : '-',
		BITS (21, 1) ? 'V' : '-', BITS (20, 1) ? 'C' : '-',
		BITS (8, 15), BITS (7, 1), BITS (6, 1), BITS (5, 1),
		BITS (0, 31));
	break;
      case FPS_REGNUM:
	{
	  static char *fcc[4] =
	  {"=", "<", ">", "?"};
	  static char *rd[4] =
	  {"N", "0", "+", "-"};
	  /* Long, but I'd rather leave it as is and use a wide screen.  */
	  printf ("\trd:%s, tem:%d, ns:%d, ver:%d, ftt:%d, qne:%d, "
		  "fcc:%s, aexc:%d, cexc:%d",
		  rd[BITS (30, 3)], BITS (23, 31), BITS (22, 1), BITS (17, 7),
		  BITS (14, 7), BITS (13, 1), fcc[BITS (10, 3)], BITS (5, 31),
		  BITS (0, 31));
	  break;
	}
      }

#undef BITS
}

int
gdb_print_insn_sparc (bfd_vma memaddr, disassemble_info *info)
{
  /* It's necessary to override mach again because print_insn messes it up. */
  info->mach = TARGET_ARCHITECTURE->mach;
  return print_insn_sparc (memaddr, info);
}

/* The SPARC passes the arguments on the stack; arguments smaller
   than an int are promoted to an int.  The first 6 words worth of 
   args are also passed in registers o0 - o5.  */

CORE_ADDR
sparc32_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_addr)
{
  int i, j, oregnum;
  int accumulate_size = 0;
  struct sparc_arg
    {
      char *contents;
      int len;
      int offset;
    };
  struct sparc_arg *sparc_args =
    (struct sparc_arg *) alloca (nargs * sizeof (struct sparc_arg));
  struct sparc_arg *m_arg;

  /* Promote arguments if necessary, and calculate their stack offsets
     and sizes. */
  for (i = 0, m_arg = sparc_args; i < nargs; i++, m_arg++)
    {
      struct value *arg = args[i];
      struct type *arg_type = check_typedef (VALUE_TYPE (arg));
      /* Cast argument to long if necessary as the compiler does it too.  */
      switch (TYPE_CODE (arg_type))
	{
	case TYPE_CODE_INT:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_ENUM:
	  if (TYPE_LENGTH (arg_type) < TYPE_LENGTH (builtin_type_long))
	    {
	      arg_type = builtin_type_long;
	      arg = value_cast (arg_type, arg);
	    }
	  break;
	default:
	  break;
	}
      m_arg->len = TYPE_LENGTH (arg_type);
      m_arg->offset = accumulate_size;
      accumulate_size = (accumulate_size + m_arg->len + 3) & ~3;
      m_arg->contents = VALUE_CONTENTS (arg);
    }

  /* Make room for the arguments on the stack.  */
  accumulate_size += CALL_DUMMY_STACK_ADJUST;
  sp = ((sp - accumulate_size) & ~7) + CALL_DUMMY_STACK_ADJUST;

  /* `Push' arguments on the stack.  */
  for (i = 0, oregnum = 0, m_arg = sparc_args; 
       i < nargs;
       i++, m_arg++)
    {
      write_memory (sp + m_arg->offset, m_arg->contents, m_arg->len);
      for (j = 0; 
	   j < m_arg->len && oregnum < 6; 
	   j += SPARC_INTREG_SIZE, oregnum++)
	write_register_gen (O0_REGNUM + oregnum, m_arg->contents + j);
    }

  return sp;
}


/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

void
sparc32_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  int typelen = TYPE_LENGTH (type);
  int regsize = REGISTER_RAW_SIZE (O0_REGNUM);

  if (TYPE_CODE (type) == TYPE_CODE_FLT && SPARC_HAS_FPU)
    memcpy (valbuf, &regbuf[REGISTER_BYTE (FP0_REGNUM)], typelen);
  else
    memcpy (valbuf,
	    &regbuf[O0_REGNUM * regsize +
		    (typelen >= regsize
		     || TARGET_BYTE_ORDER == BFD_ENDIAN_LITTLE ? 0
		     : regsize - typelen)],
	    typelen);
}


/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  On SPARCs with FPUs,
   float values are returned in %f0 (and %f1).  In all other cases,
   values are returned in register %o0.  */

void
sparc_store_return_value (struct type *type, char *valbuf)
{
  int regno;
  char *buffer;

  buffer = alloca (MAX_REGISTER_RAW_SIZE);

  if (TYPE_CODE (type) == TYPE_CODE_FLT && SPARC_HAS_FPU)
    /* Floating-point values are returned in the register pair */
    /* formed by %f0 and %f1 (doubles are, anyway).  */
    regno = FP0_REGNUM;
  else
    /* Other values are returned in register %o0.  */
    regno = O0_REGNUM;

  /* Add leading zeros to the value. */
  if (TYPE_LENGTH (type) < REGISTER_RAW_SIZE (regno))
    {
      memset (buffer, 0, REGISTER_RAW_SIZE (regno));
      memcpy (buffer + REGISTER_RAW_SIZE (regno) - TYPE_LENGTH (type), valbuf,
	      TYPE_LENGTH (type));
      write_register_gen (regno, buffer);
    }
  else
    write_register_bytes (REGISTER_BYTE (regno), valbuf, TYPE_LENGTH (type));
}

extern void
sparclet_store_return_value (struct type *type, char *valbuf)
{
  /* Other values are returned in register %o0.  */
  write_register_bytes (REGISTER_BYTE (O0_REGNUM), valbuf,
			TYPE_LENGTH (type));
}


#ifndef CALL_DUMMY_CALL_OFFSET
#define CALL_DUMMY_CALL_OFFSET \
     (gdbarch_tdep (current_gdbarch)->call_dummy_call_offset)
#endif /* CALL_DUMMY_CALL_OFFSET */

/* Insert the function address into a call dummy instruction sequence
   stored at DUMMY.

   For structs and unions, if the function was compiled with Sun cc,
   it expects 'unimp' after the call.  But gcc doesn't use that
   (twisted) convention.  So leave a nop there for gcc (FIX_CALL_DUMMY
   can assume it is operating on a pristine CALL_DUMMY, not one that
   has already been customized for a different function).  */

void
sparc_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun,
		      struct type *value_type, int using_gcc)
{
  int i;

  /* Store the relative adddress of the target function into the
     'call' instruction. */
  store_unsigned_integer (dummy + CALL_DUMMY_CALL_OFFSET, 4,
			  (0x40000000
			   | (((fun - (pc + CALL_DUMMY_CALL_OFFSET)) >> 2)
			      & 0x3fffffff)));

  /* If the called function returns an aggregate value, fill in the UNIMP
     instruction containing the size of the returned aggregate return value,
     which follows the call instruction.
     For details see the SPARC Architecture Manual Version 8, Appendix D.3.

     Adjust the call_dummy_breakpoint_offset for the bp_call_dummy breakpoint
     to the proper address in the call dummy, so that `finish' after a stop
     in a call dummy works.
     Tweeking current_gdbarch is not an optimal solution, but the call to
     sparc_fix_call_dummy is immediately followed by a call to run_stack_dummy,
     which is the only function where dummy_breakpoint_offset is actually
     used, if it is non-zero.  */
  if (TYPE_CODE (value_type) == TYPE_CODE_STRUCT
       || TYPE_CODE (value_type) == TYPE_CODE_UNION)
    {
      store_unsigned_integer (dummy + CALL_DUMMY_CALL_OFFSET + 8, 4,
			      TYPE_LENGTH (value_type) & 0x1fff);
      set_gdbarch_call_dummy_breakpoint_offset (current_gdbarch, 0x30);
    }
  else
    set_gdbarch_call_dummy_breakpoint_offset (current_gdbarch, 0x2c);

  if (!(GDB_TARGET_IS_SPARC64))
    {
      /* If this is not a simulator target, change the first four
	 instructions of the call dummy to NOPs.  Those instructions
	 include a 'save' instruction and are designed to work around
	 problems with register window flushing in the simulator. */
      
      if (strcmp (target_shortname, "sim") != 0)
	{
	  for (i = 0; i < 4; i++)
	    store_unsigned_integer (dummy + (i * 4), 4, 0x01000000);
	}
    }

  /* If this is a bi-endian target, GDB has written the call dummy
     in little-endian order.  We must byte-swap it back to big-endian. */
  if (bi_endian)
    {
      for (i = 0; i < CALL_DUMMY_LENGTH; i += 4)
	{
	  char tmp = dummy[i];
	  dummy[i] = dummy[i + 3];
	  dummy[i + 3] = tmp;
	  tmp = dummy[i + 1];
	  dummy[i + 1] = dummy[i + 2];
	  dummy[i + 2] = tmp;
	}
    }
}


/* Set target byte order based on machine type. */

static int
sparc_target_architecture_hook (const bfd_arch_info_type *ap)
{
  int i, j;

  if (ap->mach == bfd_mach_sparc_sparclite_le)
    {
      target_byte_order = BFD_ENDIAN_LITTLE;
      bi_endian = 1;
    }
  else
    bi_endian = 0;
  return 1;
}


/*
 * Module "constructor" function. 
 */

static struct gdbarch * sparc_gdbarch_init (struct gdbarch_info info,
					    struct gdbarch_list *arches);

void
_initialize_sparc_tdep (void)
{
  /* Hook us into the gdbarch mechanism.  */
  register_gdbarch_init (bfd_arch_sparc, sparc_gdbarch_init);

  tm_print_insn = gdb_print_insn_sparc;
  tm_print_insn_info.mach = TM_PRINT_INSN_MACH;		/* Selects sparc/sparclite */
  target_architecture_hook = sparc_target_architecture_hook;
}

/* Compensate for stack bias. Note that we currently don't handle
   mixed 32/64 bit code. */

CORE_ADDR
sparc64_read_sp (void)
{
  CORE_ADDR sp = read_register (SP_REGNUM);

  if (sp & 1)
    sp += 2047;
  return sp;
}

CORE_ADDR
sparc64_read_fp (void)
{
  CORE_ADDR fp = read_register (FP_REGNUM);

  if (fp & 1)
    fp += 2047;
  return fp;
}

void
sparc64_write_sp (CORE_ADDR val)
{
  CORE_ADDR oldsp = read_register (SP_REGNUM);
  if (oldsp & 1)
    write_register (SP_REGNUM, val - 2047);
  else
    write_register (SP_REGNUM, val);
}

void
sparc64_write_fp (CORE_ADDR val)
{
  CORE_ADDR oldfp = read_register (FP_REGNUM);
  if (oldfp & 1)
    write_register (FP_REGNUM, val - 2047);
  else
    write_register (FP_REGNUM, val);
}

/* The SPARC 64 ABI passes floating-point arguments in FP0 to FP31,
   and all other arguments in O0 to O5.  They are also copied onto
   the stack in the correct places.  Apparently (empirically), 
   structs of less than 16 bytes are passed member-by-member in
   separate registers, but I am unable to figure out the algorithm.
   Some members go in floating point regs, but I don't know which.

   FIXME: Handle small structs (less than 16 bytes containing floats).

   The counting regimen for using both integer and FP registers
   for argument passing is rather odd -- a single counter is used
   for both; this means that if the arguments alternate between
   int and float, we will waste every other register of both types.  */

CORE_ADDR
sparc64_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_retaddr)
{
  int i, j, register_counter = 0;
  CORE_ADDR tempsp;
  struct type *sparc_intreg_type = 
    TYPE_LENGTH (builtin_type_long) == SPARC_INTREG_SIZE ?
    builtin_type_long : builtin_type_long_long;

  sp = (sp & ~(((unsigned long) SPARC_INTREG_SIZE) - 1UL));

  /* Figure out how much space we'll need. */
  for (i = nargs - 1; i >= 0; i--)
    {
      int len = TYPE_LENGTH (check_typedef (VALUE_TYPE (args[i])));
      struct value *copyarg = args[i];
      int copylen = len;

      if (copylen < SPARC_INTREG_SIZE)
	{
	  copyarg = value_cast (sparc_intreg_type, copyarg);
	  copylen = SPARC_INTREG_SIZE;
	}
      sp -= copylen;
    }

  /* Round down. */
  sp = sp & ~7;
  tempsp = sp;

  /* if STRUCT_RETURN, then first argument is the struct return location. */
  if (struct_return)
    write_register (O0_REGNUM + register_counter++, struct_retaddr);

  /* Now write the arguments onto the stack, while writing FP
     arguments into the FP registers, and other arguments into the
     first six 'O' registers.  */

  for (i = 0; i < nargs; i++)
    {
      int len = TYPE_LENGTH (check_typedef (VALUE_TYPE (args[i])));
      struct value *copyarg = args[i];
      enum type_code typecode = TYPE_CODE (VALUE_TYPE (args[i]));
      int copylen = len;

      if (typecode == TYPE_CODE_INT   ||
	  typecode == TYPE_CODE_BOOL  ||
	  typecode == TYPE_CODE_CHAR  ||
	  typecode == TYPE_CODE_RANGE ||
	  typecode == TYPE_CODE_ENUM)
	if (len < SPARC_INTREG_SIZE)
	  {
	    /* Small ints will all take up the size of one intreg on
	       the stack.  */
	    copyarg = value_cast (sparc_intreg_type, copyarg);
	    copylen = SPARC_INTREG_SIZE;
	  }

      write_memory (tempsp, VALUE_CONTENTS (copyarg), copylen);
      tempsp += copylen;

      /* Corner case: Structs consisting of a single float member are floats.
       * FIXME!  I don't know about structs containing multiple floats!
       * Structs containing mixed floats and ints are even more weird.
       */



      /* Separate float args from all other args.  */
      if (typecode == TYPE_CODE_FLT && SPARC_HAS_FPU)
	{
	  if (register_counter < 16)
	    {
	      /* This arg gets copied into a FP register. */
	      int fpreg;

	      switch (len) {
	      case 4:	/* Single-precision (float) */
		fpreg = FP0_REGNUM + 2 * register_counter + 1;
		register_counter += 1;
		break;
	      case 8:	/* Double-precision (double) */
		fpreg = FP0_REGNUM + 2 * register_counter;
		register_counter += 1;
		break;
	      case 16:	/* Quad-precision (long double) */
		fpreg = FP0_REGNUM + 2 * register_counter;
		register_counter += 2;
		break;
	      default:
		internal_error (__FILE__, __LINE__, "bad switch");
	      }
	      write_register_bytes (REGISTER_BYTE (fpreg),
				    VALUE_CONTENTS (args[i]),
				    len);
	    }
	}
      else /* all other args go into the first six 'o' registers */
        {
          for (j = 0; 
	       j < len && register_counter < 6; 
	       j += SPARC_INTREG_SIZE)
	    {
	      int oreg = O0_REGNUM + register_counter;

	      write_register_gen (oreg, VALUE_CONTENTS (copyarg) + j);
	      register_counter += 1;
	    }
        }
    }
  return sp;
}

/* Values <= 32 bytes are returned in o0-o3 (floating-point values are
   returned in f0-f3). */

void
sp64_extract_return_value (struct type *type, char *regbuf, char *valbuf,
			   int bitoffset)
{
  int typelen = TYPE_LENGTH (type);
  int regsize = REGISTER_RAW_SIZE (O0_REGNUM);

  if (TYPE_CODE (type) == TYPE_CODE_FLT && SPARC_HAS_FPU)
    {
      memcpy (valbuf, &regbuf[REGISTER_BYTE (FP0_REGNUM)], typelen);
      return;
    }

  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
      || (TYPE_LENGTH (type) > 32))
    {
      memcpy (valbuf,
	      &regbuf[O0_REGNUM * regsize +
		      (typelen >= regsize ? 0 : regsize - typelen)],
	      typelen);
      return;
    }
  else
    {
      char *o0 = &regbuf[O0_REGNUM * regsize];
      char *f0 = &regbuf[FP0_REGNUM * regsize];
      int x;

      for (x = 0; x < TYPE_NFIELDS (type); x++)
	{
	  struct field *f = &TYPE_FIELDS (type)[x];
	  /* FIXME: We may need to handle static fields here. */
	  int whichreg = (f->loc.bitpos + bitoffset) / 32;
	  int remainder = ((f->loc.bitpos + bitoffset) % 32) / 8;
	  int where = (f->loc.bitpos + bitoffset) / 8;
	  int size = TYPE_LENGTH (f->type);
	  int typecode = TYPE_CODE (f->type);

	  if (typecode == TYPE_CODE_STRUCT)
	    {
	      sp64_extract_return_value (f->type,
					 regbuf,
					 valbuf,
					 bitoffset + f->loc.bitpos);
	    }
	  else if (typecode == TYPE_CODE_FLT && SPARC_HAS_FPU)
	    {
	      memcpy (valbuf + where, &f0[whichreg * 4] + remainder, size);
	    }
	  else
	    {
	      memcpy (valbuf + where, &o0[whichreg * 4] + remainder, size);
	    }
	}
    }
}

extern void
sparc64_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  sp64_extract_return_value (type, regbuf, valbuf, 0);
}

extern void 
sparclet_extract_return_value (struct type *type,
			       char *regbuf, 
			       char *valbuf)
{
  regbuf += REGISTER_RAW_SIZE (O0_REGNUM) * 8;
  if (TYPE_LENGTH (type) < REGISTER_RAW_SIZE (O0_REGNUM))
    regbuf += REGISTER_RAW_SIZE (O0_REGNUM) - TYPE_LENGTH (type);

  memcpy ((void *) valbuf, regbuf, TYPE_LENGTH (type));
}


extern CORE_ADDR
sparc32_stack_align (CORE_ADDR addr)
{
  return ((addr + 7) & -8);
}

extern CORE_ADDR
sparc64_stack_align (CORE_ADDR addr)
{
  return ((addr + 15) & -16);
}

extern void
sparc_print_extra_frame_info (struct frame_info *fi)
{
  if (fi && fi->extra_info && fi->extra_info->flat)
    printf_filtered (" flat, pc saved at 0x%s, fp saved at 0x%s\n",
		     paddr_nz (fi->extra_info->pc_addr), 
		     paddr_nz (fi->extra_info->fp_addr));
}

/* MULTI_ARCH support */

static char *
sparc32_register_name (int regno)
{
  static char *register_names[] = 
  { "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
    "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
    "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
    "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",

    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",

    "y", "psr", "wim", "tbr", "pc", "npc", "fpsr", "cpsr"
  };

  if (regno < 0 ||
      regno >= (sizeof (register_names) / sizeof (register_names[0])))
    return NULL;
  else
    return register_names[regno];
}

static char *
sparc64_register_name (int regno)
{
  static char *register_names[] = 
  { "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
    "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
    "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
    "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",

    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
    "f32", "f34", "f36", "f38", "f40", "f42", "f44", "f46",
    "f48", "f50", "f52", "f54", "f56", "f58", "f60", "f62",

    "pc", "npc", "ccr", "fsr", "fprs", "y", "asi", "ver", 
    "tick", "pil", "pstate", "tstate", "tba", "tl", "tt", "tpc", 
    "tnpc", "wstate", "cwp", "cansave", "canrestore", "cleanwin", "otherwin",
    "asr16", "asr17", "asr18", "asr19", "asr20", "asr21", "asr22", "asr23", 
    "asr24", "asr25", "asr26", "asr27", "asr28", "asr29", "asr30", "asr31",
    /* These are here at the end to simplify removing them if we have to.  */
    "icc", "xcc", "fcc0", "fcc1", "fcc2", "fcc3"
  };

  if (regno < 0 ||
      regno >= (sizeof (register_names) / sizeof (register_names[0])))
    return NULL;
  else
    return register_names[regno];
}

static char *
sparclite_register_name (int regno)
{
  static char *register_names[] = 
  { "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
    "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
    "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
    "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",

    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",

    "y", "psr", "wim", "tbr", "pc", "npc", "fpsr", "cpsr",
    "dia1", "dia2", "dda1", "dda2", "ddv1", "ddv2", "dcr", "dsr" 
  };

  if (regno < 0 ||
      regno >= (sizeof (register_names) / sizeof (register_names[0])))
    return NULL;
  else
    return register_names[regno];
}

static char *
sparclet_register_name (int regno)
{
  static char *register_names[] = 
  { "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
    "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
    "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
    "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",

    "", "", "", "", "", "", "", "", /* no floating point registers */
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",

    "y", "psr", "wim", "tbr", "pc", "npc", "", "", /* no FPSR or CPSR */
    "ccsr", "ccpr", "cccrcr", "ccor", "ccobr", "ccibr", "ccir", "", 

    /*       ASR15                 ASR19 (don't display them) */    
    "asr1",  "", "asr17", "asr18", "", "asr20", "asr21", "asr22"
    /* None of the rest get displayed */
#if 0
    "awr0",  "awr1",  "awr2",  "awr3",  "awr4",  "awr5",  "awr6",  "awr7",  
    "awr8",  "awr9",  "awr10", "awr11", "awr12", "awr13", "awr14", "awr15", 
    "awr16", "awr17", "awr18", "awr19", "awr20", "awr21", "awr22", "awr23", 
    "awr24", "awr25", "awr26", "awr27", "awr28", "awr29", "awr30", "awr31", 
    "apsr"
#endif /* 0 */
  };

  if (regno < 0 ||
      regno >= (sizeof (register_names) / sizeof (register_names[0])))
    return NULL;
  else
    return register_names[regno];
}

CORE_ADDR
sparc_push_return_address (CORE_ADDR pc_unused, CORE_ADDR sp)
{
  if (CALL_DUMMY_LOCATION == AT_ENTRY_POINT)
    {
      /* The return PC of the dummy_frame is the former 'current' PC
	 (where we were before we made the target function call).
	 This is saved in %i7 by push_dummy_frame.

	 We will save the 'call dummy location' (ie. the address
	 to which the target function will return) in %o7.  
	 This address will actually be the program's entry point.  
	 There will be a special call_dummy breakpoint there.  */

      write_register (O7_REGNUM, 
		      CALL_DUMMY_ADDRESS () - 8);
    }

  return sp;
}

/* Should call_function allocate stack space for a struct return?  */

static int
sparc64_use_struct_convention (int gcc_p, struct type *type)
{
  return (TYPE_LENGTH (type) > 32);
}

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function_by_hand.
   The ultimate mystery is, tho, what is the value "16"?

   MVS: That's the offset from where the sp is now, to where the
   subroutine is gonna expect to find the struct return address.  */

static void
sparc32_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  char *val;
  CORE_ADDR o7;

  val = alloca (SPARC_INTREG_SIZE); 
  store_unsigned_integer (val, SPARC_INTREG_SIZE, addr);
  write_memory (sp + (16 * SPARC_INTREG_SIZE), val, SPARC_INTREG_SIZE); 

  if (CALL_DUMMY_LOCATION == AT_ENTRY_POINT)
    {
      /* Now adjust the value of the link register, which was previously
	 stored by push_return_address.  Functions that return structs are
	 peculiar in that they return to link register + 12, rather than
	 link register + 8.  */

      o7 = read_register (O7_REGNUM);
      write_register (O7_REGNUM, o7 - 4);
    }
}

static void
sparc64_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  /* FIXME: V9 uses %o0 for this.  */
  /* FIXME MVS: Only for small enough structs!!! */

  target_write_memory (sp + (16 * SPARC_INTREG_SIZE), 
		       (char *) &addr, SPARC_INTREG_SIZE); 
#if 0
  if (CALL_DUMMY_LOCATION == AT_ENTRY_POINT)
    {
      /* Now adjust the value of the link register, which was previously
	 stored by push_return_address.  Functions that return structs are
	 peculiar in that they return to link register + 12, rather than
	 link register + 8.  */

      write_register (O7_REGNUM, read_register (O7_REGNUM) - 4);
    }
#endif
}

/* Default target data type for register REGNO.  */

static struct type *
sparc32_register_virtual_type (int regno)
{
  if (regno == PC_REGNUM ||
      regno == FP_REGNUM ||
      regno == SP_REGNUM)
    return builtin_type_unsigned_int;
  if (regno < 32)
    return builtin_type_int;
  if (regno < 64)
    return builtin_type_float;
  return builtin_type_int;
}

static struct type *
sparc64_register_virtual_type (int regno)
{
  if (regno == PC_REGNUM ||
      regno == FP_REGNUM ||
      regno == SP_REGNUM)
    return builtin_type_unsigned_long_long;
  if (regno < 32)
    return builtin_type_long_long;
  if (regno < 64)
    return builtin_type_float;
  if (regno < 80)
    return builtin_type_double;
  return builtin_type_long_long;
}

/* Number of bytes of storage in the actual machine representation for
   register REGNO.  */

static int
sparc32_register_size (int regno)
{
  return 4;
}

static int
sparc64_register_size (int regno)
{
  return (regno < 32 ? 8 : regno < 64 ? 4 : 8);
}

/* Index within the `registers' buffer of the first byte of the space
   for register REGNO.  */

static int
sparc32_register_byte (int regno)
{
  return (regno * 4);
}

static int
sparc64_register_byte (int regno)
{
  if (regno < 32)
    return regno * 8;
  else if (regno < 64)
    return 32 * 8 + (regno - 32) * 4;
  else if (regno < 80)
    return 32 * 8 + 32 * 4 + (regno - 64) * 8;
  else
    return 64 * 8 + (regno - 80) * 8;
}

/* Advance PC across any function entry prologue instructions to reach
   some "real" code.  SKIP_PROLOGUE_FRAMELESS_P advances the PC past
   some of the prologue, but stops as soon as it knows that the
   function has a frame.  Its result is equal to its input PC if the
   function is frameless, unequal otherwise.  */

static CORE_ADDR
sparc_gdbarch_skip_prologue (CORE_ADDR ip)
{
  return examine_prologue (ip, 0, NULL, NULL);
}

/* Immediately after a function call, return the saved pc.
   Can't go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

static CORE_ADDR
sparc_saved_pc_after_call (struct frame_info *fi)
{
  return sparc_pc_adjust (read_register (RP_REGNUM));
}

/* Convert registers between 'raw' and 'virtual' formats.
   They are the same on sparc, so there's nothing to do.  */

static void
sparc_convert_to_virtual (int regnum, struct type *type, char *from, char *to)
{	/* do nothing (should never be called) */
}

static void
sparc_convert_to_raw (struct type *type, int regnum, char *from, char *to)
{	/* do nothing (should never be called) */
}

/* Init saved regs: nothing to do, just a place-holder function.  */

static void
sparc_frame_init_saved_regs (struct frame_info *fi_ignored)
{	/* no-op */
}

/* gdbarch fix call dummy:
   All this function does is rearrange the arguments before calling
   sparc_fix_call_dummy (which does the real work).  */

static void
sparc_gdbarch_fix_call_dummy (char *dummy, 
			      CORE_ADDR pc, 
			      CORE_ADDR fun, 
			      int nargs, 
			      struct value **args, 
			      struct type *type, 
			      int gcc_p)
{
  if (CALL_DUMMY_LOCATION == ON_STACK)
    sparc_fix_call_dummy (dummy, pc, fun, type, gcc_p);
}

/* Coerce float to double: a no-op.  */

static int
sparc_coerce_float_to_double (struct type *formal, struct type *actual)
{
  return 1;
}

/* CALL_DUMMY_ADDRESS: fetch the breakpoint address for a call dummy.  */

static CORE_ADDR
sparc_call_dummy_address (void)
{
  return (CALL_DUMMY_START_OFFSET) + CALL_DUMMY_BREAKPOINT_OFFSET;
}

/* Supply the Y register number to those that need it.  */

int
sparc_y_regnum (void)
{
  return gdbarch_tdep (current_gdbarch)->y_regnum;
}

int
sparc_reg_struct_has_addr (int gcc_p, struct type *type)
{
  if (GDB_TARGET_IS_SPARC64)
    return (TYPE_LENGTH (type) > 32);
  else
    return (gcc_p != 1);
}

int
sparc_intreg_size (void)
{
  return SPARC_INTREG_SIZE;
}

static int
sparc_return_value_on_stack (struct type *type)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT &&
      TYPE_LENGTH (type) > 8)
    return 1;
  else
    return 0;
}

/*
 * Gdbarch "constructor" function.
 */

#define SPARC32_CALL_DUMMY_ON_STACK

#define SPARC_SP_REGNUM    14
#define SPARC_FP_REGNUM    30
#define SPARC_FP0_REGNUM   32
#define SPARC32_NPC_REGNUM 69
#define SPARC32_PC_REGNUM  68
#define SPARC32_Y_REGNUM   64
#define SPARC64_PC_REGNUM  80
#define SPARC64_NPC_REGNUM 81
#define SPARC64_Y_REGNUM   85

static struct gdbarch *
sparc_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  static LONGEST call_dummy_32[] = 
    { 0xbc100001, 0x9de38000, 0xbc100002, 0xbe100003,
      0xda03a058, 0xd803a054, 0xd603a050, 0xd403a04c,
      0xd203a048, 0x40000000, 0xd003a044, 0x01000000,
      0x91d02001, 0x01000000
    };
  static LONGEST call_dummy_64[] = 
    { 0x9de3bec0fd3fa7f7LL, 0xf93fa7eff53fa7e7LL,
      0xf13fa7dfed3fa7d7LL, 0xe93fa7cfe53fa7c7LL,
      0xe13fa7bfdd3fa7b7LL, 0xd93fa7afd53fa7a7LL,
      0xd13fa79fcd3fa797LL, 0xc93fa78fc53fa787LL,
      0xc13fa77fcc3fa777LL, 0xc83fa76fc43fa767LL,
      0xc03fa75ffc3fa757LL, 0xf83fa74ff43fa747LL,
      0xf03fa73f01000000LL, 0x0100000001000000LL,
      0x0100000091580000LL, 0xd027a72b93500000LL,
      0xd027a72791480000LL, 0xd027a72391400000LL,
      0xd027a71fda5ba8a7LL, 0xd85ba89fd65ba897LL,
      0xd45ba88fd25ba887LL, 0x9fc02000d05ba87fLL,
      0x0100000091d02001LL, 0x0100000001000000LL 
    };
  static LONGEST call_dummy_nil[] = {0};

  /* First see if there is already a gdbarch that can satisfy the request.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* None found: is the request for a sparc architecture? */
  if (info.bfd_arch_info->arch != bfd_arch_sparc)
    return NULL;	/* No; then it's not for us.  */

  /* Yes: create a new gdbarch for the specified machine type.  */
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  /* First set settings that are common for all sparc architectures.  */
  set_gdbarch_believe_pcc_promotion (gdbarch, 1);
  set_gdbarch_breakpoint_from_pc (gdbarch, memory_breakpoint_from_pc);
  set_gdbarch_coerce_float_to_double (gdbarch, 
				      sparc_coerce_float_to_double);
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 1);
  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 1);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_extract_struct_value_address (gdbarch, 
					    sparc_extract_struct_value_address);
  set_gdbarch_fix_call_dummy (gdbarch, sparc_gdbarch_fix_call_dummy);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_fp_regnum (gdbarch, SPARC_FP_REGNUM);
  set_gdbarch_fp0_regnum (gdbarch, SPARC_FP0_REGNUM);
  set_gdbarch_frame_args_address (gdbarch, default_frame_address);
  set_gdbarch_frame_chain (gdbarch, sparc_frame_chain);
  set_gdbarch_frame_init_saved_regs (gdbarch, sparc_frame_init_saved_regs);
  set_gdbarch_frame_locals_address (gdbarch, default_frame_address);
  set_gdbarch_frame_num_args (gdbarch, frame_num_args_unknown);
  set_gdbarch_frame_saved_pc (gdbarch, sparc_frame_saved_pc);
  set_gdbarch_frameless_function_invocation (gdbarch, 
					     frameless_look_for_prologue);
  set_gdbarch_get_saved_register (gdbarch, sparc_get_saved_register);
  set_gdbarch_init_extra_frame_info (gdbarch, sparc_init_extra_frame_info);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 16 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_max_register_raw_size (gdbarch, 8);
  set_gdbarch_max_register_virtual_size (gdbarch, 8);
  set_gdbarch_pop_frame (gdbarch, sparc_pop_frame);
  set_gdbarch_push_return_address (gdbarch, sparc_push_return_address);
  set_gdbarch_push_dummy_frame (gdbarch, sparc_push_dummy_frame);
  set_gdbarch_read_pc (gdbarch, generic_target_read_pc);
  set_gdbarch_register_convert_to_raw (gdbarch, sparc_convert_to_raw);
  set_gdbarch_register_convert_to_virtual (gdbarch, 
					   sparc_convert_to_virtual);
  set_gdbarch_register_convertible (gdbarch, 
				    generic_register_convertible_not);
  set_gdbarch_reg_struct_has_addr (gdbarch, sparc_reg_struct_has_addr);
  set_gdbarch_return_value_on_stack (gdbarch, sparc_return_value_on_stack);
  set_gdbarch_saved_pc_after_call (gdbarch, sparc_saved_pc_after_call);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_skip_prologue (gdbarch, sparc_gdbarch_skip_prologue);
  set_gdbarch_sp_regnum (gdbarch, SPARC_SP_REGNUM);
  set_gdbarch_use_generic_dummy_frames (gdbarch, 0);
  set_gdbarch_write_pc (gdbarch, generic_target_write_pc);

  /*
   * Settings that depend only on 32/64 bit word size 
   */

  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_sparc:
    case bfd_mach_sparc_sparclet:
    case bfd_mach_sparc_sparclite:
    case bfd_mach_sparc_v8plus:
    case bfd_mach_sparc_v8plusa:
    case bfd_mach_sparc_sparclite_le:
      /* 32-bit machine types: */

#ifdef SPARC32_CALL_DUMMY_ON_STACK
      set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_on_stack);
      set_gdbarch_call_dummy_address (gdbarch, sparc_call_dummy_address);
      set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 0x30);
      set_gdbarch_call_dummy_length (gdbarch, 0x38);
      set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
      set_gdbarch_call_dummy_words (gdbarch, call_dummy_32);
#else
      set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_at_entry_point);
      set_gdbarch_call_dummy_address (gdbarch, entry_point_address);
      set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 0);
      set_gdbarch_call_dummy_length (gdbarch, 0);
      set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
      set_gdbarch_call_dummy_words (gdbarch, call_dummy_nil);
#endif
      set_gdbarch_call_dummy_stack_adjust (gdbarch, 68);
      set_gdbarch_call_dummy_start_offset (gdbarch, 0);
      set_gdbarch_frame_args_skip (gdbarch, 68);
      set_gdbarch_function_start_offset (gdbarch, 0);
      set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
      set_gdbarch_npc_regnum (gdbarch, SPARC32_NPC_REGNUM);
      set_gdbarch_pc_regnum (gdbarch, SPARC32_PC_REGNUM);
      set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
      set_gdbarch_push_arguments (gdbarch, sparc32_push_arguments);
      set_gdbarch_read_fp (gdbarch, generic_target_read_fp);
      set_gdbarch_read_sp (gdbarch, generic_target_read_sp);

      set_gdbarch_register_byte (gdbarch, sparc32_register_byte);
      set_gdbarch_register_raw_size (gdbarch, sparc32_register_size);
      set_gdbarch_register_size (gdbarch, 4);
      set_gdbarch_register_virtual_size (gdbarch, sparc32_register_size);
      set_gdbarch_register_virtual_type (gdbarch, 
					 sparc32_register_virtual_type);
#ifdef SPARC32_CALL_DUMMY_ON_STACK
      set_gdbarch_sizeof_call_dummy_words (gdbarch, sizeof (call_dummy_32));
#else
      set_gdbarch_sizeof_call_dummy_words (gdbarch, 0);
#endif
      set_gdbarch_stack_align (gdbarch, sparc32_stack_align);
      set_gdbarch_store_struct_return (gdbarch, sparc32_store_struct_return);
      set_gdbarch_use_struct_convention (gdbarch, 
					 generic_use_struct_convention);
      set_gdbarch_write_fp (gdbarch, generic_target_write_fp);
      set_gdbarch_write_sp (gdbarch, generic_target_write_sp);
      tdep->y_regnum = SPARC32_Y_REGNUM;
      tdep->fp_max_regnum = SPARC_FP0_REGNUM + 32;
      tdep->intreg_size = 4;
      tdep->reg_save_offset = 0x60;
      tdep->call_dummy_call_offset = 0x24;
      break;

    case bfd_mach_sparc_v9:
    case bfd_mach_sparc_v9a:
      /* 64-bit machine types: */
    default:	/* Any new machine type is likely to be 64-bit.  */

#ifdef SPARC64_CALL_DUMMY_ON_STACK
      set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_on_stack);
      set_gdbarch_call_dummy_address (gdbarch, sparc_call_dummy_address);
      set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 8 * 4);
      set_gdbarch_call_dummy_length (gdbarch, 192);
      set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
      set_gdbarch_call_dummy_start_offset (gdbarch, 148);
      set_gdbarch_call_dummy_words (gdbarch, call_dummy_64);
#else
      set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_at_entry_point);
      set_gdbarch_call_dummy_address (gdbarch, entry_point_address);
      set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 0);
      set_gdbarch_call_dummy_length (gdbarch, 0);
      set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
      set_gdbarch_call_dummy_start_offset (gdbarch, 0);
      set_gdbarch_call_dummy_words (gdbarch, call_dummy_nil);
#endif
      set_gdbarch_call_dummy_stack_adjust (gdbarch, 128);
      set_gdbarch_frame_args_skip (gdbarch, 136);
      set_gdbarch_function_start_offset (gdbarch, 0);
      set_gdbarch_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
      set_gdbarch_npc_regnum (gdbarch, SPARC64_NPC_REGNUM);
      set_gdbarch_pc_regnum (gdbarch, SPARC64_PC_REGNUM);
      set_gdbarch_ptr_bit (gdbarch, 8 * TARGET_CHAR_BIT);
      set_gdbarch_push_arguments (gdbarch, sparc64_push_arguments);
      /* NOTE different for at_entry */
      set_gdbarch_read_fp (gdbarch, sparc64_read_fp);
      set_gdbarch_read_sp (gdbarch, sparc64_read_sp);
      /* Some of the registers aren't 64 bits, but it's a lot simpler just
	 to assume they all are (since most of them are).  */
      set_gdbarch_register_byte (gdbarch, sparc64_register_byte);
      set_gdbarch_register_raw_size (gdbarch, sparc64_register_size);
      set_gdbarch_register_size (gdbarch, 8);
      set_gdbarch_register_virtual_size (gdbarch, sparc64_register_size);
      set_gdbarch_register_virtual_type (gdbarch, 
					 sparc64_register_virtual_type);
#ifdef SPARC64_CALL_DUMMY_ON_STACK
      set_gdbarch_sizeof_call_dummy_words (gdbarch, sizeof (call_dummy_64));
#else
      set_gdbarch_sizeof_call_dummy_words (gdbarch, 0);
#endif
      set_gdbarch_stack_align (gdbarch, sparc64_stack_align);
      set_gdbarch_store_struct_return (gdbarch, sparc64_store_struct_return);
      set_gdbarch_use_struct_convention (gdbarch, 
					 sparc64_use_struct_convention);
      set_gdbarch_write_fp (gdbarch, sparc64_write_fp);
      set_gdbarch_write_sp (gdbarch, sparc64_write_sp);
      tdep->y_regnum = SPARC64_Y_REGNUM;
      tdep->fp_max_regnum = SPARC_FP0_REGNUM + 48;
      tdep->intreg_size = 8;
      tdep->reg_save_offset = 0x90;
      tdep->call_dummy_call_offset = 148 + 4 * 5;
      break;
    }

  /* 
   * Settings that vary per-architecture:
   */

  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_sparc:
      set_gdbarch_extract_return_value (gdbarch, sparc32_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 72);
      set_gdbarch_register_bytes (gdbarch, 32*4 + 32*4 + 8*4);
      set_gdbarch_register_name (gdbarch, sparc32_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->has_fpu = 1;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 32 * 4;
      tdep->print_insn_mach = bfd_mach_sparc;
      break;
    case bfd_mach_sparc_sparclet:
      set_gdbarch_extract_return_value (gdbarch, 
					sparclet_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 32 + 32 + 8 + 8 + 8);
      set_gdbarch_register_bytes (gdbarch, 32*4 + 32*4 + 8*4 + 8*4 + 8*4);
      set_gdbarch_register_name (gdbarch, sparclet_register_name);
      set_gdbarch_store_return_value (gdbarch, sparclet_store_return_value);
      tdep->has_fpu = 0;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 0;
      tdep->print_insn_mach = bfd_mach_sparc_sparclet;
      break;
    case bfd_mach_sparc_sparclite:
      set_gdbarch_extract_return_value (gdbarch, sparc32_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, func_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 80);
      set_gdbarch_register_bytes (gdbarch, 32*4 + 32*4 + 8*4 + 8*4);
      set_gdbarch_register_name (gdbarch, sparclite_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->has_fpu = 0;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 0;
      tdep->print_insn_mach = bfd_mach_sparc_sparclite;
      break;
    case bfd_mach_sparc_v8plus:
      set_gdbarch_extract_return_value (gdbarch, sparc32_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 72);
      set_gdbarch_register_bytes (gdbarch, 32*4 + 32*4 + 8*4);
      set_gdbarch_register_name (gdbarch, sparc32_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->print_insn_mach = bfd_mach_sparc;
      tdep->fp_register_bytes = 32 * 4;
      tdep->has_fpu = 1;	/* (all but sparclet and sparclite) */
      break;
    case bfd_mach_sparc_v8plusa:
      set_gdbarch_extract_return_value (gdbarch, sparc32_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 72);
      set_gdbarch_register_bytes (gdbarch, 32*4 + 32*4 + 8*4);
      set_gdbarch_register_name (gdbarch, sparc32_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->has_fpu = 1;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 32 * 4;
      tdep->print_insn_mach = bfd_mach_sparc;
      break;
    case bfd_mach_sparc_sparclite_le:
      set_gdbarch_extract_return_value (gdbarch, sparc32_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, func_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 80);
      set_gdbarch_register_bytes (gdbarch, 32*4 + 32*4 + 8*4 + 8*4);
      set_gdbarch_register_name (gdbarch, sparclite_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->has_fpu = 0;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 0;
      tdep->print_insn_mach = bfd_mach_sparc_sparclite;
      break;
    case bfd_mach_sparc_v9:
      set_gdbarch_extract_return_value (gdbarch, sparc64_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 125);
      set_gdbarch_register_bytes (gdbarch, 32*8 + 32*8 + 45*8);
      set_gdbarch_register_name (gdbarch, sparc64_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->has_fpu = 1;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 64 * 4;
      tdep->print_insn_mach = bfd_mach_sparc_v9a;
      break;
    case bfd_mach_sparc_v9a:
      set_gdbarch_extract_return_value (gdbarch, sparc64_extract_return_value);
      set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);
      set_gdbarch_num_regs (gdbarch, 125);
      set_gdbarch_register_bytes (gdbarch, 32*8 + 32*8 + 45*8);
      set_gdbarch_register_name (gdbarch, sparc64_register_name);
      set_gdbarch_store_return_value (gdbarch, sparc_store_return_value);
      tdep->has_fpu = 1;	/* (all but sparclet and sparclite) */
      tdep->fp_register_bytes = 64 * 4;
      tdep->print_insn_mach = bfd_mach_sparc_v9a;
      break;
    }

  return gdbarch;
}

