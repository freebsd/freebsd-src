/* Target-dependent code for the NEC V850 for GDB, the GNU debugger.
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


static char *v850_generic_reg_names[] = REGISTER_NAMES;

static char *v850e_reg_names[] =
{
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
  "eipc", "eipsw", "fepc", "fepsw", "ecr", "psw", "sr6", "sr7",
  "sr8", "sr9", "sr10", "sr11", "sr12", "sr13", "sr14", "sr15",
  "ctpc", "ctpsw", "dbpc", "dbpsw", "ctbp", "sr21", "sr22", "sr23",
  "sr24", "sr25", "sr26", "sr27", "sr28", "sr29", "sr30", "sr31",
  "pc", "fp"
};

char **v850_register_names = v850_generic_reg_names;

struct
{
  char **regnames;
  int mach;
} v850_processor_type_table[] =
{
  { v850_generic_reg_names, bfd_mach_v850 },
  { v850e_reg_names, bfd_mach_v850e },
  { v850e_reg_names, bfd_mach_v850ea },
  { NULL, 0 }
};

/* Info gleaned from scanning a function's prologue.  */

struct pifsr			/* Info about one saved reg */
{
  int framereg;			/* Frame reg (SP or FP) */
  int offset;			/* Offset from framereg */
  int cur_frameoffset;		/* Current frameoffset */
  int reg;			/* Saved register number */
};

struct prologue_info
{
  int framereg;
  int frameoffset;
  int start_function;
  struct pifsr *pifsrs;
};

static CORE_ADDR v850_scan_prologue PARAMS ((CORE_ADDR pc, 
					     struct prologue_info *fs));


/* Should call_function allocate stack space for a struct return?  */
int
v850_use_struct_convention (gcc_p, type)
     int gcc_p;
     struct type *type;
{
  return (TYPE_NFIELDS (type) > 1 || TYPE_LENGTH (type) > 4);
}



/* Structure for mapping bits in register lists to register numbers. */
struct reg_list 
{
  long mask;
  int regno;
};

/* Helper function for v850_scan_prologue to handle prepare instruction. */

static void
handle_prepare (int insn, int insn2, CORE_ADDR *current_pc_ptr,
		struct prologue_info *pi, struct pifsr **pifsr_ptr)

{
  CORE_ADDR current_pc = *current_pc_ptr;
  struct pifsr *pifsr = *pifsr_ptr;
  long next = insn2 & 0xffff;
  long list12 = ((insn & 1) << 16) + (next & 0xffe0);
  long offset = (insn & 0x3e) << 1;
  static struct reg_list reg_table [] =
  {
    { 0x00800, 20 },	/* r20 */
    { 0x00400, 21 },	/* r21 */
    { 0x00200, 22 },	/* r22 */
    { 0x00100, 23 },	/* r23 */
    { 0x08000, 24 },	/* r24 */
    { 0x04000, 25 },	/* r25 */
    { 0x02000, 26 },	/* r26 */
    { 0x01000, 27 },	/* r27 */
    { 0x00080, 28 },	/* r28 */
    { 0x00040, 29 },	/* r29 */
    { 0x10000, 30 },	/* ep */
    { 0x00020, 31 },	/* lp */
    { 0, 0 }		/* end of table */
  };
  int i;

  if ((next & 0x1f) == 0x0b)		/* skip imm16 argument */
    current_pc += 2;
  else if ((next & 0x1f) == 0x13)	/* skip imm16 argument */
    current_pc += 2;
  else if ((next & 0x1f) == 0x1b)	/* skip imm32 argument */
    current_pc += 4;

  /* Calculate the total size of the saved registers, and add it
     it to the immediate value used to adjust SP. */
  for (i = 0; reg_table[i].mask != 0; i++)
    if (list12 & reg_table[i].mask)
      offset += REGISTER_RAW_SIZE (regtable[i].regno);
  pi->frameoffset -= offset;

  /* Calculate the offsets of the registers relative to the value
     the SP will have after the registers have been pushed and the
     imm5 value has been subtracted from it. */
  if (pifsr)
    {
      for (i = 0; reg_table[i].mask != 0; i++)
	{
	  if (list12 & reg_table[i].mask)
	    {
	      int reg = reg_table[i].regno;
	      offset -= REGISTER_RAW_SIZE (reg);
	      pifsr->reg = reg;
	      pifsr->offset = offset;
	      pifsr->cur_frameoffset = pi->frameoffset;
    #ifdef DEBUG
	      printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
    #endif
	      pifsr++;
	    }
	}
    }
#ifdef DEBUG
  printf_filtered ("\tfound ctret after regsave func");
#endif

  /* Set result parameters. */
  *current_pc_ptr = current_pc;
  *pifsr_ptr = pifsr;
}


/* Helper function for v850_scan_prologue to handle pushm/pushl instructions.
   FIXME: the SR bit of the register list is not supported; must check
   that the compiler does not ever generate this bit. */

static void
handle_pushm (int insn, int insn2, struct prologue_info *pi,
	     struct pifsr **pifsr_ptr)

{
  struct pifsr *pifsr = *pifsr_ptr;
  long list12 = ((insn & 0x0f) << 16) + (insn2 & 0xfff0);
  long offset = 0;
  static struct reg_list pushml_reg_table [] =
  {
    { 0x80000, PS_REGNUM },	/* PSW */
    { 0x40000, 1 },		/* r1 */
    { 0x20000, 2 },		/* r2 */
    { 0x10000, 3 },		/* r3 */
    { 0x00800, 4 },		/* r4 */
    { 0x00400, 5 },		/* r5 */
    { 0x00200, 6 },		/* r6 */
    { 0x00100, 7 },		/* r7 */
    { 0x08000, 8 },		/* r8 */
    { 0x04000, 9 },		/* r9 */
    { 0x02000, 10 },		/* r10 */
    { 0x01000, 11 },		/* r11 */
    { 0x00080, 12 },		/* r12 */
    { 0x00040, 13 },		/* r13 */
    { 0x00020, 14 },		/* r14 */
    { 0x00010, 15 },		/* r15 */
    { 0, 0 }		/* end of table */
  };
  static struct reg_list pushmh_reg_table [] =
  {
    { 0x80000, 16 },		/* r16 */
    { 0x40000, 17 },		/* r17 */
    { 0x20000, 18 },		/* r18 */
    { 0x10000, 19 },		/* r19 */
    { 0x00800, 20 },		/* r20 */
    { 0x00400, 21 },		/* r21 */
    { 0x00200, 22 },		/* r22 */
    { 0x00100, 23 },		/* r23 */
    { 0x08000, 24 },		/* r24 */
    { 0x04000, 25 },		/* r25 */
    { 0x02000, 26 },		/* r26 */
    { 0x01000, 27 },		/* r27 */
    { 0x00080, 28 },		/* r28 */
    { 0x00040, 29 },		/* r29 */
    { 0x00010, 30 },		/* r30 */
    { 0x00020, 31 },		/* r31 */
    { 0, 0 }		/* end of table */
  };
  struct reg_list *reg_table;
  int i;

  /* Is this a pushml or a pushmh? */
  if ((insn2 & 7) == 1)
    reg_table = pushml_reg_table;
  else
    reg_table = pushmh_reg_table;

  /* Calculate the total size of the saved registers, and add it
     it to the immediate value used to adjust SP. */
  for (i = 0; reg_table[i].mask != 0; i++)
    if (list12 & reg_table[i].mask)
      offset += REGISTER_RAW_SIZE (regtable[i].regno);
  pi->frameoffset -= offset;

  /* Calculate the offsets of the registers relative to the value
     the SP will have after the registers have been pushed and the
     imm5 value is subtracted from it. */
  if (pifsr)
    {
      for (i = 0; reg_table[i].mask != 0; i++)
	{
	  if (list12 & reg_table[i].mask)
	    {
	      int reg = reg_table[i].regno;
	      offset -= REGISTER_RAW_SIZE (reg);
	      pifsr->reg = reg;
	      pifsr->offset = offset;
	      pifsr->cur_frameoffset = pi->frameoffset;
    #ifdef DEBUG
	      printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
    #endif
	      pifsr++;
	    }
	}
    }
#ifdef DEBUG
  printf_filtered ("\tfound ctret after regsave func");
#endif

  /* Set result parameters. */
  *pifsr_ptr = pifsr;
}




/* Function: scan_prologue
   Scan the prologue of the function that contains PC, and record what
   we find in PI.  PI->fsr must be zeroed by the called.  Returns the
   pc after the prologue.  Note that the addresses saved in pi->fsr
   are actually just frame relative (negative offsets from the frame
   pointer).  This is because we don't know the actual value of the
   frame pointer yet.  In some circumstances, the frame pointer can't
   be determined till after we have scanned the prologue.  */

static CORE_ADDR
v850_scan_prologue (pc, pi)
     CORE_ADDR pc;
     struct prologue_info *pi;
{
  CORE_ADDR func_addr, prologue_end, current_pc;
  struct pifsr *pifsr, *pifsr_tmp;
  int fp_used;
  int ep_used;
  int reg;
  CORE_ADDR save_pc, save_end;
  int regsave_func_p;
  int r12_tmp;

  /* First, figure out the bounds of the prologue so that we can limit the
     search to something reasonable.  */

  if (find_pc_partial_function (pc, NULL, &func_addr, NULL))
    {
      struct symtab_and_line sal;

      sal = find_pc_line (func_addr, 0);

      if (func_addr == entry_point_address ())
	pi->start_function = 1;
      else
	pi->start_function = 0;

#if 0
      if (sal.line == 0)
	prologue_end = pc;
      else
	prologue_end = sal.end;
#else
      prologue_end = pc;
#endif
    }
  else
    {				/* We're in the boondocks */
      func_addr = pc - 100;
      prologue_end = pc;
    }

  prologue_end = min (prologue_end, pc);

  /* Now, search the prologue looking for instructions that setup fp, save
     rp, adjust sp and such.  We also record the frame offset of any saved
     registers. */ 

  pi->frameoffset = 0;
  pi->framereg = SP_REGNUM;
  fp_used = 0;
  ep_used = 0;
  pifsr = pi->pifsrs;
  regsave_func_p = 0;
  save_pc = 0;
  save_end = 0;
  r12_tmp = 0;

#ifdef DEBUG
  printf_filtered ("Current_pc = 0x%.8lx, prologue_end = 0x%.8lx\n",
		   (long)func_addr, (long)prologue_end);
#endif

  for (current_pc = func_addr; current_pc < prologue_end; )
    {
      int insn, insn2;

#ifdef DEBUG
      printf_filtered ("0x%.8lx ", (long)current_pc);
      (*tm_print_insn) (current_pc, &tm_print_insn_info);
#endif

      insn = read_memory_unsigned_integer (current_pc, 2);
      current_pc += 2;
      if ((insn & 0x0780) >= 0x0600) /* Four byte instruction? */
	{
	  insn2 = read_memory_unsigned_integer (current_pc, 2);
	  current_pc += 2;
	}

      if ((insn & 0xffc0) == ((10 << 11) | 0x0780) && !regsave_func_p)
	{			/* jarl <func>,10 */
	  long low_disp = insn2 & ~ (long) 1;
	  long disp = (((((insn & 0x3f) << 16) + low_disp)
			& ~ (long) 1) ^ 0x00200000) - 0x00200000;

	  save_pc = current_pc;
	  save_end = prologue_end;
	  regsave_func_p = 1;
	  current_pc += disp - 4;
	  prologue_end = (current_pc
			  + (2 * 3)	/* moves to/from ep */
			  + 4		/* addi <const>,sp,sp */
			  + 2		/* jmp [r10] */
			  + (2 * 12)	/* sst.w to save r2, r20-r29, r31 */
			  + 20);	/* slop area */

#ifdef DEBUG
	  printf_filtered ("\tfound jarl <func>,r10, disp = %ld, low_disp = %ld, new pc = 0x%.8lx\n",
			   disp, low_disp, (long)current_pc + 2);
#endif
	  continue;
	}
      else if ((insn & 0xffc0) == 0x0200 && !regsave_func_p)
	{			/* callt <imm6> */
	  long ctbp = read_register (CTBP_REGNUM);
	  long adr = ctbp + ((insn & 0x3f) << 1);

	  save_pc = current_pc;
	  save_end = prologue_end;
	  regsave_func_p = 1;
	  current_pc = ctbp + (read_memory_unsigned_integer (adr, 2) & 0xffff);
	  prologue_end = (current_pc
			  + (2 * 3)	/* prepare list2,imm5,sp/imm */
			  + 4		/* ctret */
			  + 20);	/* slop area */

#ifdef DEBUG
	  printf_filtered ("\tfound callt,  ctbp = 0x%.8lx, adr = %.8lx, new pc = 0x%.8lx\n",
			   ctbp, adr, (long)current_pc);
#endif
	  continue;
	}
      else if ((insn & 0xffc0) == 0x0780)	/* prepare list2,imm5 */
	{
	  handle_prepare (insn, insn2, &current_pc, pi, &pifsr);
	  continue;
	}
      else if (insn == 0x07e0 && regsave_func_p && insn2 == 0x0144)
	{			/* ctret after processing register save function */
	  current_pc = save_pc;
	  prologue_end = save_end;
	  regsave_func_p = 0;
#ifdef DEBUG
	  printf_filtered ("\tfound ctret after regsave func");
#endif
	  continue;
	}
      else if ((insn & 0xfff0) == 0x07e0 && (insn2 & 5) == 1)
	{			/* pushml, pushmh */
	  handle_pushm (insn, insn2, pi, &pifsr);
	  continue;
	}
      else if ((insn & 0xffe0) == 0x0060 && regsave_func_p)
	{			/* jmp after processing register save function */
	  current_pc = save_pc;
	  prologue_end = save_end;
	  regsave_func_p = 0;
#ifdef DEBUG
	  printf_filtered ("\tfound jmp after regsave func");
#endif
	  continue;
	}
      else if ((insn & 0x07c0) == 0x0780	/* jarl or jr */
	       || (insn & 0xffe0) == 0x0060	/* jmp */
	       || (insn & 0x0780) == 0x0580)	/* branch */
	{
#ifdef DEBUG
	  printf_filtered ("\n");
#endif
	  break;				/* Ran into end of prologue */
	}

      else if ((insn & 0xffe0) == ((SP_REGNUM << 11) | 0x0240))		/* add <imm>,sp */
	pi->frameoffset += ((insn & 0x1f) ^ 0x10) - 0x10;
      else if (insn == ((SP_REGNUM << 11) | 0x0600 | SP_REGNUM))	/* addi <imm>,sp,sp */
	pi->frameoffset += insn2;
      else if (insn == ((FP_RAW_REGNUM << 11) | 0x0000 | SP_REGNUM))	/* mov sp,fp */
	{
	  fp_used = 1;
	  pi->framereg = FP_RAW_REGNUM;
	}

      else if (insn == ((R12_REGNUM << 11) | 0x0640 | R0_REGNUM))	/* movhi hi(const),r0,r12 */
	r12_tmp = insn2 << 16;
      else if (insn == ((R12_REGNUM << 11) | 0x0620 | R12_REGNUM))	/* movea lo(const),r12,r12 */
	r12_tmp += insn2;
      else if (insn == ((SP_REGNUM << 11) | 0x01c0 | R12_REGNUM) && r12_tmp) /* add r12,sp */
	pi->frameoffset = r12_tmp;
      else if (insn == ((EP_REGNUM << 11) | 0x0000 | SP_REGNUM))	/* mov sp,ep */
	ep_used = 1;
      else if (insn == ((EP_REGNUM << 11) | 0x0000 | R1_REGNUM))	/* mov r1,ep */
	ep_used = 0;
      else if (((insn & 0x07ff) == (0x0760 | SP_REGNUM)			/* st.w <reg>,<offset>[sp] */
		|| (fp_used
		    && (insn & 0x07ff) == (0x0760 | FP_RAW_REGNUM)))	/* st.w <reg>,<offset>[fp] */
	       && pifsr
	       && (((reg = (insn >> 11) & 0x1f) >= SAVE1_START_REGNUM && reg <= SAVE1_END_REGNUM)
		   || (reg >= SAVE2_START_REGNUM && reg <= SAVE2_END_REGNUM)
		   || (reg >= SAVE3_START_REGNUM && reg <= SAVE3_END_REGNUM)))
	{
	  pifsr->reg = reg;
	  pifsr->offset = insn2 & ~1;
	  pifsr->cur_frameoffset = pi->frameoffset;
#ifdef DEBUG
	  printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
#endif
	  pifsr++;
	}

      else if (ep_used						/* sst.w <reg>,<offset>[ep] */
	       && ((insn & 0x0781) == 0x0501)
	       && pifsr
	       && (((reg = (insn >> 11) & 0x1f) >= SAVE1_START_REGNUM && reg <= SAVE1_END_REGNUM)
		   || (reg >= SAVE2_START_REGNUM && reg <= SAVE2_END_REGNUM)
		   || (reg >= SAVE3_START_REGNUM && reg <= SAVE3_END_REGNUM)))
	{
	  pifsr->reg = reg;
	  pifsr->offset = (insn & 0x007e) << 1;
	  pifsr->cur_frameoffset = pi->frameoffset;
#ifdef DEBUG
	  printf_filtered ("\tSaved register r%d, offset %d", reg, pifsr->offset);
#endif
	  pifsr++;
	}

#ifdef DEBUG
      printf_filtered ("\n");
#endif
    }

  if (pifsr)
    pifsr->framereg = 0;	/* Tie off last entry */

  /* Fix up any offsets to the final offset.  If a frame pointer was created, use it
     instead of the stack pointer.  */
  for (pifsr_tmp = pi->pifsrs; pifsr_tmp && pifsr_tmp != pifsr; pifsr_tmp++)
    {
      pifsr_tmp->offset -= pi->frameoffset - pifsr_tmp->cur_frameoffset;
      pifsr_tmp->framereg = pi->framereg;

#ifdef DEBUG
      printf_filtered ("Saved register r%d, offset = %d, framereg = r%d\n",
		       pifsr_tmp->reg, pifsr_tmp->offset, pifsr_tmp->framereg);
#endif
    }

#ifdef DEBUG
  printf_filtered ("Framereg = r%d, frameoffset = %d\n", pi->framereg, pi->frameoffset);
#endif

  return current_pc;
}

/* Function: init_extra_frame_info
   Setup the frame's frame pointer, pc, and frame addresses for saved
   registers.  Most of the work is done in scan_prologue().

   Note that when we are called for the last frame (currently active frame),
   that fi->pc and fi->frame will already be setup.  However, fi->frame will
   be valid only if this routine uses FP.  For previous frames, fi-frame will
   always be correct (since that is derived from v850_frame_chain ()).

   We can be called with the PC in the call dummy under two circumstances.
   First, during normal backtracing, second, while figuring out the frame
   pointer just prior to calling the target function (see run_stack_dummy).  */

void
v850_init_extra_frame_info (fi)
     struct frame_info *fi;
{
  struct prologue_info pi;
  struct pifsr pifsrs[NUM_REGS + 1], *pifsr;

  if (fi->next)
    fi->pc = FRAME_SAVED_PC (fi->next);

  memset (fi->fsr.regs, '\000', sizeof fi->fsr.regs);

  /* The call dummy doesn't save any registers on the stack, so we can return
     now.  */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
      return;

  pi.pifsrs = pifsrs;

  v850_scan_prologue (fi->pc, &pi);

  if (!fi->next && pi.framereg == SP_REGNUM)
    fi->frame = read_register (pi.framereg) - pi.frameoffset;

  for (pifsr = pifsrs; pifsr->framereg; pifsr++)
    {
      fi->fsr.regs[pifsr->reg] = pifsr->offset + fi->frame;

      if (pifsr->framereg == SP_REGNUM)
	fi->fsr.regs[pifsr->reg] += pi.frameoffset;
    }
}

/* Function: frame_chain
   Figure out the frame prior to FI.  Unfortunately, this involves
   scanning the prologue of the caller, which will also be done
   shortly by v850_init_extra_frame_info.  For the dummy frame, we
   just return the stack pointer that was in use at the time the
   function call was made.  */

CORE_ADDR
v850_frame_chain (fi)
     struct frame_info *fi;
{
  struct prologue_info pi;
  CORE_ADDR callers_pc, fp;

  /* First, find out who called us */
  callers_pc = FRAME_SAVED_PC (fi);
  /* If caller is a call-dummy, then our FP bears no relation to his FP! */
  fp = v850_find_callers_reg (fi, FP_RAW_REGNUM);
  if (PC_IN_CALL_DUMMY(callers_pc, fp, fp))
    return fp;	/* caller is call-dummy: return oldest value of FP */

  /* Caller is NOT a call-dummy, so everything else should just work.
     Even if THIS frame is a call-dummy! */
  pi.pifsrs = NULL;

  v850_scan_prologue (callers_pc, &pi);

  if (pi.start_function)
    return 0;			/* Don't chain beyond the start function */

  if (pi.framereg == FP_RAW_REGNUM)
    return v850_find_callers_reg (fi, pi.framereg);

  return fi->frame - pi.frameoffset;
}

/* Function: find_callers_reg
   Find REGNUM on the stack.  Otherwise, it's in an active register.
   One thing we might want to do here is to check REGNUM against the
   clobber mask, and somehow flag it as invalid if it isn't saved on
   the stack somewhere.  This would provide a graceful failure mode
   when trying to get the value of caller-saves registers for an inner
   frame.  */

CORE_ADDR
v850_find_callers_reg (fi, regnum)
     struct frame_info *fi;
     int regnum;
{
  for (; fi; fi = fi->next)
    if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
      return generic_read_register_dummy (fi->pc, fi->frame, regnum);
    else if (fi->fsr.regs[regnum] != 0)
      return read_memory_unsigned_integer (fi->fsr.regs[regnum], 
					   REGISTER_RAW_SIZE(regnum));

  return read_register (regnum);
}

/* Function: skip_prologue
   Return the address of the first code past the prologue of the function.  */

CORE_ADDR
v850_skip_prologue (pc)
     CORE_ADDR pc;
{
  CORE_ADDR func_addr, func_end;

  /* See what the symbol table says */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      struct symtab_and_line sal;

      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end < func_end)
	return sal.end;
      else
	/* Either there's no line info, or the line after the prologue is after
	   the end of the function.  In this case, there probably isn't a
	   prologue.  */
	return pc;
    }

/* We can't find the start of this function, so there's nothing we can do. */
  return pc;
}

/* Function: pop_frame
   This routine gets called when either the user uses the `return'
   command, or the call dummy breakpoint gets hit.  */

void
v850_pop_frame (frame)
     struct frame_info *frame;
{
  int regnum;

  if (PC_IN_CALL_DUMMY(frame->pc, frame->frame, frame->frame))
    generic_pop_dummy_frame ();
  else
    {
      write_register (PC_REGNUM, FRAME_SAVED_PC (frame));

      for (regnum = 0; regnum < NUM_REGS; regnum++)
	if (frame->fsr.regs[regnum] != 0)
	  write_register (regnum,
			  read_memory_unsigned_integer (frame->fsr.regs[regnum],
							REGISTER_RAW_SIZE(regnum)));

      write_register (SP_REGNUM, FRAME_FP (frame));
    }

  flush_cached_frames ();
}

/* Function: push_arguments
   Setup arguments and RP for a call to the target.  First four args
   go in R6->R9, subsequent args go into sp + 16 -> sp + ...  Structs
   are passed by reference.  64 bit quantities (doubles and long
   longs) may be split between the regs and the stack.  When calling a
   function that returns a struct, a pointer to the struct is passed
   in as a secret first argument (always in R6).

   Stack space for the args has NOT been allocated: that job is up to us.
   */

CORE_ADDR
v850_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     unsigned char struct_return;
     CORE_ADDR struct_addr;
{
  int argreg;
  int argnum;
  int len = 0;
  int stack_offset;

  /* First, just for safety, make sure stack is aligned */
  sp &= ~3;

  /* Now make space on the stack for the args. */
  for (argnum = 0; argnum < nargs; argnum++)
    len += ((TYPE_LENGTH(VALUE_TYPE(args[argnum])) + 3) & ~3);
  sp -= len;	/* possibly over-allocating, but it works... */
		/* (you might think we could allocate 16 bytes */
		/* less, but the ABI seems to use it all! )  */
  argreg = ARG0_REGNUM;

  /* the struct_return pointer occupies the first parameter-passing reg */
  if (struct_return)
      write_register (argreg++, struct_addr);

  stack_offset = 16;
  /* The offset onto the stack at which we will start copying parameters
     (after the registers are used up) begins at 16 rather than at zero.
     I don't really know why, that's just the way it seems to work.  */

  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  There are 16 bytes
     in four registers available.  Loop thru args from first to last.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      char *val;
      char valbuf[REGISTER_RAW_SIZE(ARG0_REGNUM)];

      if (TYPE_CODE (VALUE_TYPE (*args)) == TYPE_CODE_STRUCT
	  && TYPE_LENGTH (VALUE_TYPE (*args)) > 8)
	{
	  store_address (valbuf, 4, VALUE_ADDRESS (*args));
	  len = 4;
	  val = valbuf;
	}
      else
	{
	  len = TYPE_LENGTH (VALUE_TYPE (*args));
	  val = (char *)VALUE_CONTENTS (*args);
	}

      while (len > 0)
	if  (argreg <= ARGLAST_REGNUM)
	  {
	    CORE_ADDR regval;

	    regval = extract_address (val, REGISTER_RAW_SIZE (argreg));
	    write_register (argreg, regval);

	    len -= REGISTER_RAW_SIZE (argreg);
	    val += REGISTER_RAW_SIZE (argreg);
	    argreg++;
	  }
	else
	  {
	    write_memory (sp + stack_offset, val, 4);

	    len -= 4;
	    val += 4;
	    stack_offset += 4;
	  }
      args++;
    }
  return sp;
}

/* Function: push_return_address (pc)
   Set up the return address for the inferior function call.
   Needed for targets where we don't actually execute a JSR/BSR instruction */
 
CORE_ADDR
v850_push_return_address (pc, sp)
     CORE_ADDR pc;
     CORE_ADDR sp;
{
  write_register (RP_REGNUM, CALL_DUMMY_ADDRESS ());
  return sp;
}
 
/* Function: frame_saved_pc 
   Find the caller of this frame.  We do this by seeing if RP_REGNUM
   is saved in the stack anywhere, otherwise we get it from the
   registers.  If the inner frame is a dummy frame, return its PC
   instead of RP, because that's where "caller" of the dummy-frame
   will be found.  */

CORE_ADDR
v850_frame_saved_pc (fi)
     struct frame_info *fi;
{
  if (PC_IN_CALL_DUMMY(fi->pc, fi->frame, fi->frame))
    return generic_read_register_dummy(fi->pc, fi->frame, PC_REGNUM);
  else
    return v850_find_callers_reg (fi, RP_REGNUM);
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


/* Function: fix_call_dummy
   Pokes the callee function's address into the CALL_DUMMY assembly stub.
   Assumes that the CALL_DUMMY looks like this:
	jarl <offset24>, r31
	trap
   */

int
v850_fix_call_dummy (dummy, sp, fun, nargs, args, type, gcc_p)
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

  store_unsigned_integer ((unsigned int *)&dummy[2], 2, offset24 & 0xffff);
  store_unsigned_integer ((unsigned int *)&dummy[0], 2, offset24 >> 16);
  return 0;
}

/* Change the register names based on the current machine type. */

static int
v850_target_architecture_hook (ap)
     const bfd_arch_info_type *ap;
{
  int i, j;

  if (ap->arch != bfd_arch_v850)
    return 0;

  for (i = 0; v850_processor_type_table[i].regnames != NULL; i++)
    {
      if (v850_processor_type_table[i].mach == ap->mach)
	{
	  v850_register_names = v850_processor_type_table[i].regnames;
	  return 1;
	}
    }

  fatal ("Architecture `%s' unreconized", ap->printable_name);
}

void
_initialize_v850_tdep ()
{
  tm_print_insn = print_insn_v850;
  target_architecture_hook = v850_target_architecture_hook;
}
