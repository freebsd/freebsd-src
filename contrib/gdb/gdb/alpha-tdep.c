/* Target-dependent code for the ALPHA architecture, for GDB, the GNU Debugger.
   Copyright 1993, 1994, 1995 Free Software Foundation, Inc.

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
#include "symtab.h"
#include "value.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "dis-asm.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_string.h"

/* FIXME: Some of this code should perhaps be merged with mips-tdep.c.  */

/* FIXME: Put this declaration in frame.h.  */
extern struct obstack frame_cache_obstack;


/* Forward declarations.  */

static CORE_ADDR read_next_frame_reg PARAMS ((struct frame_info *, int));

static CORE_ADDR heuristic_proc_start PARAMS ((CORE_ADDR));

static alpha_extra_func_info_t heuristic_proc_desc PARAMS ((CORE_ADDR,
							    CORE_ADDR,
							    struct frame_info *));

static alpha_extra_func_info_t find_proc_desc PARAMS ((CORE_ADDR,
						       struct frame_info *));

#if 0
static int alpha_in_lenient_prologue PARAMS ((CORE_ADDR, CORE_ADDR));
#endif

static void reinit_frame_cache_sfunc PARAMS ((char *, int,
					      struct cmd_list_element *));

static CORE_ADDR after_prologue PARAMS ((CORE_ADDR pc,
					 alpha_extra_func_info_t proc_desc));

static int alpha_in_prologue PARAMS ((CORE_ADDR pc,
				alpha_extra_func_info_t proc_desc));

/* Heuristic_proc_start may hunt through the text section for a long
   time across a 2400 baud serial line.  Allows the user to limit this
   search.  */
static unsigned int heuristic_fence_post = 0;

/* Layout of a stack frame on the alpha:

                |				|
 pdr members:	|  7th ... nth arg,		|
                |  `pushed' by caller.		|
                |				|
----------------|-------------------------------|<--  old_sp == vfp
   ^  ^  ^  ^	|				|
   |  |  |  |	|				|
   |  |localoff	|  Copies of 1st .. 6th		|
   |  |  |  |	|  argument if necessary.	|
   |  |  |  v	|				|
   |  |  |  ---	|-------------------------------|<-- FRAME_LOCALS_ADDRESS
   |  |  |      |				|
   |  |  |      |  Locals and temporaries.	|
   |  |  |      |				|
   |  |  |      |-------------------------------|
   |  |  |      |				|
   |-fregoffset	|  Saved float registers.	|
   |  |  |      |  F9				|
   |  |  |      |   .				|
   |  |  |      |   .				|
   |  |  |      |  F2				|
   |  |  v      |				|
   |  |  -------|-------------------------------|
   |  |         |				|
   |  |         |  Saved registers.		|
   |  |         |  S6				|
   |-regoffset	|   .				|
   |  |         |   .				|
   |  |         |  S0				|
   |  |         |  pdr.pcreg			|
   |  v         |				|
   |  ----------|-------------------------------|
   |            |				|
 frameoffset    |  Argument build area, gets	|
   |            |  7th ... nth arg for any	|
   |            |  called procedure.		|
   v            |  				|
   -------------|-------------------------------|<-- sp
                |				|
*/

#define PROC_LOW_ADDR(proc) ((proc)->pdr.adr) /* least address */
#define PROC_HIGH_ADDR(proc) ((proc)->pdr.iline) /* upper address bound */
#define PROC_DUMMY_FRAME(proc) ((proc)->pdr.iopt) /* frame for CALL_DUMMY */
#define PROC_FRAME_OFFSET(proc) ((proc)->pdr.frameoffset)
#define PROC_FRAME_REG(proc) ((proc)->pdr.framereg)
#define PROC_REG_MASK(proc) ((proc)->pdr.regmask)
#define PROC_FREG_MASK(proc) ((proc)->pdr.fregmask)
#define PROC_REG_OFFSET(proc) ((proc)->pdr.regoffset)
#define PROC_FREG_OFFSET(proc) ((proc)->pdr.fregoffset)
#define PROC_PC_REG(proc) ((proc)->pdr.pcreg)
#define PROC_LOCALOFF(proc) ((proc)->pdr.localoff)
#define PROC_SYMBOL(proc) (*(struct symbol**)&(proc)->pdr.isym)
#define _PROC_MAGIC_ 0x0F0F0F0F
#define PROC_DESC_IS_DUMMY(proc) ((proc)->pdr.isym == _PROC_MAGIC_)
#define SET_PROC_DESC_IS_DUMMY(proc) ((proc)->pdr.isym = _PROC_MAGIC_)

struct linked_proc_info
{
  struct alpha_extra_func_info info;
  struct linked_proc_info *next;
} *linked_proc_desc_table = NULL;


/* Guaranteed to set frame->saved_regs to some values (it never leaves it
   NULL).  */

void
alpha_find_saved_regs (frame)
     struct frame_info *frame;
{
  int ireg;
  CORE_ADDR reg_position;
  unsigned long mask;
  alpha_extra_func_info_t proc_desc;
  int returnreg;

  frame->saved_regs = (struct frame_saved_regs *)
    obstack_alloc (&frame_cache_obstack, sizeof(struct frame_saved_regs));
  memset (frame->saved_regs, 0, sizeof (struct frame_saved_regs));

  /* If it is the frame for __sigtramp, the saved registers are located
     in a sigcontext structure somewhere on the stack. __sigtramp
     passes a pointer to the sigcontext structure on the stack.
     If the stack layout for __sigtramp changes, or if sigcontext offsets
     change, we might have to update this code.  */
#ifndef SIGFRAME_PC_OFF
#define SIGFRAME_PC_OFF		(2 * 8)
#define SIGFRAME_REGSAVE_OFF	(4 * 8)
#define SIGFRAME_FPREGSAVE_OFF	(SIGFRAME_REGSAVE_OFF + 32 * 8 + 8)
#endif
  if (frame->signal_handler_caller)
    {
      CORE_ADDR sigcontext_pointer_addr;
      CORE_ADDR sigcontext_addr;

      if (frame->next)
	sigcontext_pointer_addr = frame->next->frame;
      else
	sigcontext_pointer_addr = frame->frame;
      sigcontext_addr = read_memory_integer(sigcontext_pointer_addr, 8);
      for (ireg = 0; ireg < 32; ireg++)
	{
 	  reg_position = sigcontext_addr + SIGFRAME_REGSAVE_OFF + ireg * 8;
 	  frame->saved_regs->regs[ireg] = reg_position;
	}
      for (ireg = 0; ireg < 32; ireg++)
	{
 	  reg_position = sigcontext_addr + SIGFRAME_FPREGSAVE_OFF + ireg * 8;
 	  frame->saved_regs->regs[FP0_REGNUM + ireg] = reg_position;
	}
      frame->saved_regs->regs[PC_REGNUM] = sigcontext_addr + SIGFRAME_PC_OFF;
      return;
    }

  proc_desc = frame->proc_desc;
  if (proc_desc == NULL)
    /* I'm not sure how/whether this can happen.  Normally when we can't
       find a proc_desc, we "synthesize" one using heuristic_proc_desc
       and set the saved_regs right away.  */
    return;

  /* Fill in the offsets for the registers which gen_mask says
     were saved.  */

  reg_position = frame->frame + PROC_REG_OFFSET (proc_desc);
  mask = PROC_REG_MASK (proc_desc);

  returnreg = PROC_PC_REG (proc_desc);

  /* Note that RA is always saved first, regardless of its actual
     register number.  */
  if (mask & (1 << returnreg))
    {
      frame->saved_regs->regs[returnreg] = reg_position;
      reg_position += 8;
      mask &= ~(1 << returnreg); /* Clear bit for RA so we
				    don't save again later. */
    }

  for (ireg = 0; ireg <= 31 ; ++ireg)
    if (mask & (1 << ireg))
      {
	frame->saved_regs->regs[ireg] = reg_position;
	reg_position += 8;
      }

  /* Fill in the offsets for the registers which float_mask says
     were saved.  */

  reg_position = frame->frame + PROC_FREG_OFFSET (proc_desc);
  mask = PROC_FREG_MASK (proc_desc);

  for (ireg = 0; ireg <= 31 ; ++ireg)
    if (mask & (1 << ireg))
      {
	frame->saved_regs->regs[FP0_REGNUM+ireg] = reg_position;
	reg_position += 8;
      }

  frame->saved_regs->regs[PC_REGNUM] = frame->saved_regs->regs[returnreg];
}

static CORE_ADDR
read_next_frame_reg(fi, regno)
     struct frame_info *fi;
     int regno;
{
  for (; fi; fi = fi->next)
    {
      /* We have to get the saved sp from the sigcontext
	 if it is a signal handler frame.  */
      if (regno == SP_REGNUM && !fi->signal_handler_caller)
	return fi->frame;
      else
	{
	  if (fi->saved_regs == NULL)
	    alpha_find_saved_regs (fi);
	  if (fi->saved_regs->regs[regno])
	    return read_memory_integer(fi->saved_regs->regs[regno], 8);
	}
    }
  return read_register(regno);
}

CORE_ADDR
alpha_frame_saved_pc(frame)
     struct frame_info *frame;
{
  alpha_extra_func_info_t proc_desc = frame->proc_desc;
  /* We have to get the saved pc from the sigcontext
     if it is a signal handler frame.  */
  int pcreg = frame->signal_handler_caller ? PC_REGNUM : frame->pc_reg;

  if (proc_desc && PROC_DESC_IS_DUMMY(proc_desc))
      return read_memory_integer(frame->frame - 8, 8);

  return read_next_frame_reg(frame, pcreg);
}

CORE_ADDR
alpha_saved_pc_after_call (frame)
     struct frame_info *frame;
{
  CORE_ADDR pc = frame->pc;
  CORE_ADDR tmp;
  alpha_extra_func_info_t proc_desc;
  int pcreg;

  /* Skip over shared library trampoline if necessary.  */
  tmp = SKIP_TRAMPOLINE_CODE (pc);
  if (tmp != 0)
    pc = tmp;

  proc_desc = find_proc_desc (pc, frame->next);
  pcreg = proc_desc ? PROC_PC_REG (proc_desc) : RA_REGNUM;

  return read_register (pcreg);
}


static struct alpha_extra_func_info temp_proc_desc;
static struct frame_saved_regs temp_saved_regs;

/* This fencepost looks highly suspicious to me.  Removing it also
   seems suspicious as it could affect remote debugging across serial
   lines.  */

static CORE_ADDR
heuristic_proc_start(pc)
    CORE_ADDR pc;
{
    CORE_ADDR start_pc = pc;
    CORE_ADDR fence = start_pc - heuristic_fence_post;

    if (start_pc == 0)	return 0;

    if (heuristic_fence_post == UINT_MAX
	|| fence < VM_MIN_ADDRESS)
      fence = VM_MIN_ADDRESS;

    /* search back for previous return */
    for (start_pc -= 4; ; start_pc -= 4)
	if (start_pc < fence)
	  {
	    /* It's not clear to me why we reach this point when
	       stop_soon_quietly, but with this test, at least we
	       don't print out warnings for every child forked (eg, on
	       decstation).  22apr93 rich@cygnus.com.  */
	    if (!stop_soon_quietly)
	      {
		static int blurb_printed = 0;

		if (fence == VM_MIN_ADDRESS)
		  warning("Hit beginning of text section without finding");
		else
		  warning("Hit heuristic-fence-post without finding");
		
		warning("enclosing function for address 0x%lx", pc);
		if (!blurb_printed)
		  {
		    printf_filtered ("\
This warning occurs if you are debugging a function without any symbols\n\
(for example, in a stripped executable).  In that case, you may wish to\n\
increase the size of the search with the `set heuristic-fence-post' command.\n\
\n\
Otherwise, you told GDB there was a function where there isn't one, or\n\
(more likely) you have encountered a bug in GDB.\n");
		    blurb_printed = 1;
		  }
	      }

	    return 0; 
	  }
	else if (ABOUT_TO_RETURN(start_pc))
	    break;

    start_pc += 4; /* skip return */
    return start_pc;
}

static alpha_extra_func_info_t
heuristic_proc_desc(start_pc, limit_pc, next_frame)
    CORE_ADDR start_pc, limit_pc;
    struct frame_info *next_frame;
{
    CORE_ADDR sp = read_next_frame_reg (next_frame, SP_REGNUM);
    CORE_ADDR cur_pc;
    int frame_size;
    int has_frame_reg = 0;
    unsigned long reg_mask = 0;
    int pcreg = -1;

    if (start_pc == 0)
      return NULL;
    memset (&temp_proc_desc, '\0', sizeof(temp_proc_desc));
    memset (&temp_saved_regs, '\0', sizeof(struct frame_saved_regs));
    PROC_LOW_ADDR (&temp_proc_desc) = start_pc;

    if (start_pc + 200 < limit_pc)
      limit_pc = start_pc + 200;
    frame_size = 0;
    for (cur_pc = start_pc; cur_pc < limit_pc; cur_pc += 4)
      {
        char buf[4];
	unsigned long word;
	int status;

	status = read_memory_nobpt (cur_pc, buf, 4); 
	if (status)
	  memory_error (status, cur_pc);
	word = extract_unsigned_integer (buf, 4);

	if ((word & 0xffff0000) == 0x23de0000)		/* lda $sp,n($sp) */
	  frame_size += (-word) & 0xffff;
	else if ((word & 0xfc1f0000) == 0xb41e0000	/* stq reg,n($sp) */
		 && (word & 0xffff0000) != 0xb7fe0000)	/* reg != $zero */
	  {
	    int reg = (word & 0x03e00000) >> 21;
	    reg_mask |= 1 << reg;
	    temp_saved_regs.regs[reg] = sp + (short)word;

	    /* Starting with OSF/1-3.2C, the system libraries are shipped
	       without local symbols, but they still contain procedure
	       descriptors without a symbol reference. GDB is currently
	       unable to find these procedure descriptors and uses
	       heuristic_proc_desc instead.
	       As some low level compiler support routines (__div*, __add*)
	       use a non-standard return address register, we have to
	       add some heuristics to determine the return address register,
	       or stepping over these routines will fail.
	       Usually the return address register is the first register
	       saved on the stack, but assembler optimization might
	       rearrange the register saves.
	       So we recognize only a few registers (t7, t9, ra) within
	       the procedure prologue as valid return address registers.

	       FIXME: Rewriting GDB to access the procedure descriptors,
	       e.g. via the minimal symbol table, might obviate this hack.  */
	    if (pcreg == -1
		&& cur_pc < (start_pc + 20)
		&& (reg == T7_REGNUM || reg == T9_REGNUM || reg == RA_REGNUM))
	      pcreg = reg;
	  }
	else if (word == 0x47de040f)			/* bis sp,sp fp */
	  has_frame_reg = 1;
      }
    if (pcreg == -1)
      {
	/* If we haven't found a valid return address register yet,
	   keep searching in the procedure prologue.  */
	while (cur_pc < (limit_pc + 20) && cur_pc < (start_pc + 20))
	  {
	    char buf[4];
	    unsigned long word;
	    int status;

	    status = read_memory_nobpt (cur_pc, buf, 4); 
	    if (status)
	      memory_error (status, cur_pc);
	    cur_pc += 4;
	    word = extract_unsigned_integer (buf, 4);

	    if ((word & 0xfc1f0000) == 0xb41e0000	/* stq reg,n($sp) */
		&& (word & 0xffff0000) != 0xb7fe0000)	/* reg != $zero */
	      {
		int reg = (word & 0x03e00000) >> 21;
		if (reg == T7_REGNUM || reg == T9_REGNUM || reg == RA_REGNUM)
		  {
		    pcreg = reg;
		    break;
		  }
	      }
	  }
      }

    if (has_frame_reg)
      PROC_FRAME_REG(&temp_proc_desc) = GCC_FP_REGNUM;
    else
      PROC_FRAME_REG(&temp_proc_desc) = SP_REGNUM;
    PROC_FRAME_OFFSET(&temp_proc_desc) = frame_size;
    PROC_REG_MASK(&temp_proc_desc) = reg_mask;
    PROC_PC_REG(&temp_proc_desc) = (pcreg == -1) ? RA_REGNUM : pcreg;
    PROC_LOCALOFF(&temp_proc_desc) = 0;	/* XXX - bogus */
    return &temp_proc_desc;
}

/* This returns the PC of the first inst after the prologue.  If we can't
   find the prologue, then return 0.  */

static CORE_ADDR
after_prologue (pc, proc_desc)
     CORE_ADDR pc;
     alpha_extra_func_info_t proc_desc;
{
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;

  if (!proc_desc)
    proc_desc = find_proc_desc (pc, NULL);

  if (proc_desc)
    {
      /* If function is frameless, then we need to do it the hard way.  I
	 strongly suspect that frameless always means prologueless... */
      if (PROC_FRAME_REG (proc_desc) == SP_REGNUM
	  && PROC_FRAME_OFFSET (proc_desc) == 0)
	return 0;
    }

  if (!find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    return 0;			/* Unknown */

  sal = find_pc_line (func_addr, 0);

  if (sal.end < func_end)
    return sal.end;

  /* The line after the prologue is after the end of the function.  In this
     case, tell the caller to find the prologue the hard way.  */

  return 0;
}

/* Return non-zero if we *might* be in a function prologue.  Return zero if we
   are definatly *not* in a function prologue.  */

static int
alpha_in_prologue (pc, proc_desc)
     CORE_ADDR pc;
     alpha_extra_func_info_t proc_desc;
{
  CORE_ADDR after_prologue_pc;

  after_prologue_pc = after_prologue (pc, proc_desc);

  if (after_prologue_pc == 0
      || pc < after_prologue_pc)
    return 1;
  else
    return 0;
}

static alpha_extra_func_info_t
find_proc_desc (pc, next_frame)
    CORE_ADDR pc;
    struct frame_info *next_frame;
{
  alpha_extra_func_info_t proc_desc;
  struct block *b;
  struct symbol *sym;
  CORE_ADDR startaddr;

  /* Try to get the proc_desc from the linked call dummy proc_descs
     if the pc is in the call dummy.
     This is hairy. In the case of nested dummy calls we have to find the
     right proc_desc, but we might not yet know the frame for the dummy
     as it will be contained in the proc_desc we are searching for.
     So we have to find the proc_desc whose frame is closest to the current
     stack pointer.  */

  if (PC_IN_CALL_DUMMY (pc, 0, 0))
    {
      struct linked_proc_info *link;
      CORE_ADDR sp = read_next_frame_reg (next_frame, SP_REGNUM);
      alpha_extra_func_info_t found_proc_desc = NULL;
      long min_distance = LONG_MAX;

      for (link = linked_proc_desc_table; link; link = link->next)
	{
	  long distance = (CORE_ADDR) PROC_DUMMY_FRAME (&link->info) - sp;
	  if (distance > 0 && distance < min_distance)
	    {
	      min_distance = distance;
	      found_proc_desc = &link->info;
	    }
	}
      if (found_proc_desc != NULL)
	return found_proc_desc;
    }

  b = block_for_pc(pc);

  find_pc_partial_function (pc, NULL, &startaddr, NULL);
  if (b == NULL)
    sym = NULL;
  else
    {
      if (startaddr > BLOCK_START (b))
	/* This is the "pathological" case referred to in a comment in
	   print_frame_info.  It might be better to move this check into
	   symbol reading.  */
	sym = NULL;
      else
	sym = lookup_symbol (MIPS_EFI_SYMBOL_NAME, b, LABEL_NAMESPACE,
			     0, NULL);
    }

  /* If we never found a PDR for this function in symbol reading, then
     examine prologues to find the information.  */
  if (sym && ((mips_extra_func_info_t) SYMBOL_VALUE (sym))->pdr.framereg == -1)
    sym = NULL;

  if (sym)
    {
	/* IF this is the topmost frame AND
	 * (this proc does not have debugging information OR
	 * the PC is in the procedure prologue)
	 * THEN create a "heuristic" proc_desc (by analyzing
	 * the actual code) to replace the "official" proc_desc.
	 */
	proc_desc = (alpha_extra_func_info_t)SYMBOL_VALUE(sym);
	if (next_frame == NULL)
	  {
	    if (PROC_DESC_IS_DUMMY (proc_desc) || alpha_in_prologue (pc, proc_desc))
	      {
		alpha_extra_func_info_t found_heuristic =
		  heuristic_proc_desc (PROC_LOW_ADDR (proc_desc),
				       pc, next_frame);
		if (found_heuristic)
		  {
		    PROC_LOCALOFF (found_heuristic) =
		      PROC_LOCALOFF (proc_desc);
		    PROC_PC_REG (found_heuristic) = PROC_PC_REG (proc_desc);
		    proc_desc = found_heuristic;
		  }
	      }
	  }
    }
  else
    {
      /* Is linked_proc_desc_table really necessary?  It only seems to be used
	 by procedure call dummys.  However, the procedures being called ought
	 to have their own proc_descs, and even if they don't,
	 heuristic_proc_desc knows how to create them! */

      register struct linked_proc_info *link;
      for (link = linked_proc_desc_table; link; link = link->next)
	  if (PROC_LOW_ADDR(&link->info) <= pc
	      && PROC_HIGH_ADDR(&link->info) > pc)
	      return &link->info;

      if (startaddr == 0)
	startaddr = heuristic_proc_start (pc);

      proc_desc =
	heuristic_proc_desc (startaddr, pc, next_frame);
    }
  return proc_desc;
}

alpha_extra_func_info_t cached_proc_desc;

CORE_ADDR
alpha_frame_chain(frame)
    struct frame_info *frame;
{
    alpha_extra_func_info_t proc_desc;
    CORE_ADDR saved_pc = FRAME_SAVED_PC(frame);

    if (saved_pc == 0 || inside_entry_file (saved_pc))
      return 0;

    proc_desc = find_proc_desc(saved_pc, frame);
    if (!proc_desc)
      return 0;

    cached_proc_desc = proc_desc;

    /* Fetch the frame pointer for a dummy frame from the procedure
       descriptor.  */
    if (PROC_DESC_IS_DUMMY(proc_desc))
      return (CORE_ADDR) PROC_DUMMY_FRAME(proc_desc);

    /* If no frame pointer and frame size is zero, we must be at end
       of stack (or otherwise hosed).  If we don't check frame size,
       we loop forever if we see a zero size frame.  */
    if (PROC_FRAME_REG (proc_desc) == SP_REGNUM
	&& PROC_FRAME_OFFSET (proc_desc) == 0
	/* The previous frame from a sigtramp frame might be frameless
	   and have frame size zero.  */
	&& !frame->signal_handler_caller)
      {
	/* The alpha __sigtramp routine is frameless and has a frame size
	   of zero, but we are able to backtrace through it. */
	char *name;
	find_pc_partial_function (saved_pc, &name,
				  (CORE_ADDR *)NULL, (CORE_ADDR *)NULL);
	if (IN_SIGTRAMP (saved_pc, name))
	  return frame->frame;
	else
	  return 0;
      }
    else
      return read_next_frame_reg(frame, PROC_FRAME_REG(proc_desc))
	     + PROC_FRAME_OFFSET(proc_desc);
}

void
init_extra_frame_info (frame)
     struct frame_info *frame;
{
  /* Use proc_desc calculated in frame_chain */
  alpha_extra_func_info_t proc_desc =
    frame->next ? cached_proc_desc : find_proc_desc(frame->pc, frame->next);

  frame->saved_regs = NULL;
  frame->localoff = 0;
  frame->pc_reg = RA_REGNUM;
  frame->proc_desc = proc_desc == &temp_proc_desc ? 0 : proc_desc;
  if (proc_desc)
    {
      /* Get the locals offset and the saved pc register from the
	 procedure descriptor, they are valid even if we are in the
	 middle of the prologue.  */
      frame->localoff = PROC_LOCALOFF(proc_desc);
      frame->pc_reg = PROC_PC_REG(proc_desc);

      /* Fixup frame-pointer - only needed for top frame */

      /* Fetch the frame pointer for a dummy frame from the procedure
	 descriptor.  */
      if (PROC_DESC_IS_DUMMY(proc_desc))
	frame->frame = (CORE_ADDR) PROC_DUMMY_FRAME(proc_desc);

      /* This may not be quite right, if proc has a real frame register.
	 Get the value of the frame relative sp, procedure might have been
	 interrupted by a signal at it's very start.  */
      else if (frame->pc == PROC_LOW_ADDR (proc_desc) && !PROC_DESC_IS_DUMMY (proc_desc))
	frame->frame = read_next_frame_reg (frame->next, SP_REGNUM);
      else
	frame->frame = read_next_frame_reg (frame->next, PROC_FRAME_REG (proc_desc))
	  + PROC_FRAME_OFFSET (proc_desc);

      if (proc_desc == &temp_proc_desc)
	{
	  char *name;

	  /* Do not set the saved registers for a sigtramp frame,
	     alpha_find_saved_registers will do that for us.
	     We can't use frame->signal_handler_caller, it is not yet set.  */
	  find_pc_partial_function (frame->pc, &name,
				    (CORE_ADDR *)NULL,(CORE_ADDR *)NULL);
	  if (!IN_SIGTRAMP (frame->pc, name))
	    {
	      frame->saved_regs = (struct frame_saved_regs*)
		obstack_alloc (&frame_cache_obstack,
			       sizeof (struct frame_saved_regs));
	      *frame->saved_regs = temp_saved_regs;
	      frame->saved_regs->regs[PC_REGNUM]
		= frame->saved_regs->regs[RA_REGNUM];
	    }
	}
    }
}

/* ALPHA stack frames are almost impenetrable.  When execution stops,
   we basically have to look at symbol information for the function
   that we stopped in, which tells us *which* register (if any) is
   the base of the frame pointer, and what offset from that register
   the frame itself is at.  

   This presents a problem when trying to examine a stack in memory
   (that isn't executing at the moment), using the "frame" command.  We
   don't have a PC, nor do we have any registers except SP.

   This routine takes two arguments, SP and PC, and tries to make the
   cached frames look as if these two arguments defined a frame on the
   cache.  This allows the rest of info frame to extract the important
   arguments without difficulty.  */

struct frame_info *
setup_arbitrary_frame (argc, argv)
     int argc;
     CORE_ADDR *argv;
{
  if (argc != 2)
    error ("ALPHA frame specifications require two arguments: sp and pc");

  return create_new_frame (argv[0], argv[1]);
}

/* The alpha passes the first six arguments in the registers, the rest on
   the stack. The register arguments are eventually transferred to the
   argument transfer area immediately below the stack by the called function
   anyway. So we `push' at least six arguments on the stack, `reload' the
   argument registers and then adjust the stack pointer to point past the
   sixth argument. This algorithm simplifies the passing of a large struct
   which extends from the registers to the stack.
   If the called function is returning a structure, the address of the
   structure to be returned is passed as a hidden first argument.  */

CORE_ADDR
alpha_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  register i;
  int accumulate_size = struct_return ? 8 : 0;
  int arg_regs_size = ALPHA_NUM_ARG_REGS * 8;
  struct alpha_arg { char *contents; int len; int offset; };
  struct alpha_arg *alpha_args =
      (struct alpha_arg*)alloca (nargs * sizeof (struct alpha_arg));
  register struct alpha_arg *m_arg;
  char raw_buffer[sizeof (CORE_ADDR)];
  int required_arg_regs;

  for (i = 0, m_arg = alpha_args; i < nargs; i++, m_arg++)
    {
      value_ptr arg = args[i];
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
      accumulate_size = (accumulate_size + m_arg->len + 7) & ~7;
      m_arg->contents = VALUE_CONTENTS(arg);
    }

  /* Determine required argument register loads, loading an argument register
     is expensive as it uses three ptrace calls.  */
  required_arg_regs = accumulate_size / 8;
  if (required_arg_regs > ALPHA_NUM_ARG_REGS)
    required_arg_regs = ALPHA_NUM_ARG_REGS;

  /* Make room for the arguments on the stack.  */
  if (accumulate_size < arg_regs_size)
    accumulate_size = arg_regs_size; 
  sp -= accumulate_size;

  /* Keep sp aligned to a multiple of 16 as the compiler does it too.  */
  sp &= ~15;

  /* `Push' arguments on the stack.  */
  for (i = nargs; m_arg--, --i >= 0; )
    write_memory(sp + m_arg->offset, m_arg->contents, m_arg->len);
  if (struct_return)
    {
      store_address (raw_buffer, sizeof (CORE_ADDR), struct_addr);
      write_memory (sp, raw_buffer, sizeof (CORE_ADDR));
    }

  /* Load the argument registers.  */
  for (i = 0; i < required_arg_regs; i++)
    {
      LONGEST val;

      val = read_memory_integer (sp + i * 8, 8);
      write_register (A0_REGNUM + i, val);
      write_register (FPA0_REGNUM + i, val);
    }

  return sp + arg_regs_size;
}

void
alpha_push_dummy_frame()
{
  int ireg;
  struct linked_proc_info *link;
  alpha_extra_func_info_t proc_desc;
  CORE_ADDR sp = read_register (SP_REGNUM);
  CORE_ADDR save_address;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  unsigned long mask;

  link = (struct linked_proc_info *) xmalloc(sizeof (struct linked_proc_info));
  link->next = linked_proc_desc_table;
  linked_proc_desc_table = link;
 
  proc_desc = &link->info;

  /*
   * The registers we must save are all those not preserved across
   * procedure calls.
   * In addition, we must save the PC and RA.
   *
   * Dummy frame layout:
   *  (high memory)
   * 	Saved PC
   *    Saved F30
   *    ...
   *    Saved F0
   *	Saved R29
   *	...
   *	Saved R0
   *	Saved R26 (RA)
   *	Parameter build area
   *  (low memory)
   */

/* MASK(i,j) == (1<<i) + (1<<(i+1)) + ... + (1<<j)). Assume i<=j<31. */
#define MASK(i,j) (((1L << ((j)+1)) - 1) ^ ((1L << (i)) - 1))
#define GEN_REG_SAVE_MASK (MASK(0,8) | MASK(16,29))
#define GEN_REG_SAVE_COUNT 24
#define FLOAT_REG_SAVE_MASK (MASK(0,1) | MASK(10,30))
#define FLOAT_REG_SAVE_COUNT 23
  /* The special register is the PC as we have no bit for it in the save masks.
     alpha_frame_saved_pc knows where the pc is saved in a dummy frame.  */
#define SPECIAL_REG_SAVE_COUNT 1

  PROC_REG_MASK(proc_desc) = GEN_REG_SAVE_MASK;
  PROC_FREG_MASK(proc_desc) = FLOAT_REG_SAVE_MASK;
  /* PROC_REG_OFFSET is the offset from the dummy frame to the saved RA,
     but keep SP aligned to a multiple of 16.  */
  PROC_REG_OFFSET(proc_desc) =
    - ((8 * (SPECIAL_REG_SAVE_COUNT
	    + GEN_REG_SAVE_COUNT
	    + FLOAT_REG_SAVE_COUNT)
	+ 15) & ~15);
  PROC_FREG_OFFSET(proc_desc) =
    PROC_REG_OFFSET(proc_desc) + 8 * GEN_REG_SAVE_COUNT;

  /* Save general registers.
     The return address register is the first saved register, all other
     registers follow in ascending order.
     The PC is saved immediately below the SP.  */
  save_address = sp + PROC_REG_OFFSET(proc_desc);
  store_address (raw_buffer, 8, read_register (RA_REGNUM));
  write_memory (save_address, raw_buffer, 8);
  save_address += 8;
  mask = PROC_REG_MASK(proc_desc) & 0xffffffffL;
  for (ireg = 0; mask; ireg++, mask >>= 1)
    if (mask & 1)
      {
	if (ireg == RA_REGNUM)
	  continue;
	store_address (raw_buffer, 8, read_register (ireg));
	write_memory (save_address, raw_buffer, 8);
	save_address += 8;
      }

  store_address (raw_buffer, 8, read_register (PC_REGNUM));
  write_memory (sp - 8, raw_buffer, 8);

  /* Save floating point registers.  */
  save_address = sp + PROC_FREG_OFFSET(proc_desc);
  mask = PROC_FREG_MASK(proc_desc) & 0xffffffffL;
  for (ireg = 0; mask; ireg++, mask >>= 1)
    if (mask & 1)
      {
	store_address (raw_buffer, 8, read_register (ireg + FP0_REGNUM));
	write_memory (save_address, raw_buffer, 8);
	save_address += 8;
      }

  /* Set and save the frame address for the dummy.  
     This is tricky. The only registers that are suitable for a frame save
     are those that are preserved across procedure calls (s0-s6). But if
     a read system call is interrupted and then a dummy call is made
     (see testsuite/gdb.t17/interrupt.exp) the dummy call hangs till the read
     is satisfied. Then it returns with the s0-s6 registers set to the values
     on entry to the read system call and our dummy frame pointer would be
     destroyed. So we save the dummy frame in the proc_desc and handle the
     retrieval of the frame pointer of a dummy specifically. The frame register
     is set to the virtual frame (pseudo) register, it's value will always
     be read as zero and will help us to catch any errors in the dummy frame
     retrieval code.  */
  PROC_DUMMY_FRAME(proc_desc) = sp;
  PROC_FRAME_REG(proc_desc) = FP_REGNUM;
  PROC_FRAME_OFFSET(proc_desc) = 0;
  sp += PROC_REG_OFFSET(proc_desc);
  write_register (SP_REGNUM, sp);

  PROC_LOW_ADDR(proc_desc) = CALL_DUMMY_ADDRESS ();
  PROC_HIGH_ADDR(proc_desc) = PROC_LOW_ADDR(proc_desc) + 4;

  SET_PROC_DESC_IS_DUMMY(proc_desc);
  PROC_PC_REG(proc_desc) = RA_REGNUM;
}

void
alpha_pop_frame()
{
  register int regnum;
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR new_sp = frame->frame;

  alpha_extra_func_info_t proc_desc = frame->proc_desc;

  write_register (PC_REGNUM, FRAME_SAVED_PC(frame));
  if (frame->saved_regs == NULL)
    alpha_find_saved_regs (frame);
  if (proc_desc)
    {
      for (regnum = 32; --regnum >= 0; )
	if (PROC_REG_MASK(proc_desc) & (1 << regnum))
	  write_register (regnum,
			  read_memory_integer (frame->saved_regs->regs[regnum],
					       8));
      for (regnum = 32; --regnum >= 0; )
	if (PROC_FREG_MASK(proc_desc) & (1 << regnum))
	  write_register (regnum + FP0_REGNUM,
			  read_memory_integer (frame->saved_regs->regs[regnum + FP0_REGNUM], 8));
    }
  write_register (SP_REGNUM, new_sp);
  flush_cached_frames ();

  if (proc_desc && PROC_DESC_IS_DUMMY(proc_desc))
    {
      struct linked_proc_info *pi_ptr, *prev_ptr;

      for (pi_ptr = linked_proc_desc_table, prev_ptr = NULL;
	   pi_ptr != NULL;
	   prev_ptr = pi_ptr, pi_ptr = pi_ptr->next)
	{
	  if (&pi_ptr->info == proc_desc)
	    break;
	}

      if (pi_ptr == NULL)
	error ("Can't locate dummy extra frame info\n");

      if (prev_ptr != NULL)
	prev_ptr->next = pi_ptr->next;
      else
	linked_proc_desc_table = pi_ptr->next;

      free (pi_ptr);
    }
}

/* To skip prologues, I use this predicate.  Returns either PC itself
   if the code at PC does not look like a function prologue; otherwise
   returns an address that (if we're lucky) follows the prologue.  If
   LENIENT, then we must skip everything which is involved in setting
   up the frame (it's OK to skip more, just so long as we don't skip
   anything which might clobber the registers which are being saved.
   Currently we must not skip more on the alpha, but we might the lenient
   stuff some day.  */

CORE_ADDR
alpha_skip_prologue (pc, lenient)
     CORE_ADDR pc;
     int lenient;
{
    unsigned long inst;
    int offset;
    CORE_ADDR post_prologue_pc;
    char buf[4];

#ifdef GDB_TARGET_HAS_SHARED_LIBS
    /* Silently return the unaltered pc upon memory errors.
       This could happen on OSF/1 if decode_line_1 tries to skip the
       prologue for quickstarted shared library functions when the
       shared library is not yet mapped in.
       Reading target memory is slow over serial lines, so we perform
       this check only if the target has shared libraries.  */
    if (target_read_memory (pc, buf, 4))
      return pc;
#endif

    /* See if we can determine the end of the prologue via the symbol table.
       If so, then return either PC, or the PC after the prologue, whichever
       is greater.  */

    post_prologue_pc = after_prologue (pc, NULL);

    if (post_prologue_pc != 0)
      return max (pc, post_prologue_pc);

    /* Can't determine prologue from the symbol table, need to examine
       instructions.  */

    /* Skip the typical prologue instructions. These are the stack adjustment
       instruction and the instructions that save registers on the stack
       or in the gcc frame.  */
    for (offset = 0; offset < 100; offset += 4)
      {
	int status;

	status = read_memory_nobpt (pc + offset, buf, 4);
	if (status)
	  memory_error (status, pc + offset);
	inst = extract_unsigned_integer (buf, 4);

	/* The alpha has no delay slots. But let's keep the lenient stuff,
	   we might need it for something else in the future.  */
	if (lenient && 0)
	  continue;

	if ((inst & 0xffff0000) == 0x27bb0000)	/* ldah $gp,n($t12) */
	    continue;
	if ((inst & 0xffff0000) == 0x23bd0000)	/* lda $gp,n($gp) */
	    continue;
	if ((inst & 0xffff0000) == 0x23de0000)	/* lda $sp,n($sp) */
	    continue;
	else if ((inst & 0xfc1f0000) == 0xb41e0000
		 && (inst & 0xffff0000) != 0xb7fe0000)
	    continue;				/* stq reg,n($sp) */
						/* reg != $zero */
	else if ((inst & 0xfc1f0000) == 0x9c1e0000
		 && (inst & 0xffff0000) != 0x9ffe0000)
	    continue;				/* stt reg,n($sp) */
						/* reg != $zero */
	else if (inst == 0x47de040f)		/* bis sp,sp,fp */
	    continue;
	else
	    break;
    }
    return pc + offset;
}

#if 0
/* Is address PC in the prologue (loosely defined) for function at
   STARTADDR?  */

static int
alpha_in_lenient_prologue (startaddr, pc)
     CORE_ADDR startaddr;
     CORE_ADDR pc;
{
  CORE_ADDR end_prologue = alpha_skip_prologue (startaddr, 1);
  return pc >= startaddr && pc < end_prologue;
}
#endif

/* The alpha needs a conversion between register and memory format if
   the register is a floating point register and
      memory format is float, as the register format must be double
   or
      memory format is an integer with 4 bytes or less, as the representation
      of integers in floating point registers is different. */
void
alpha_register_convert_to_virtual (regnum, valtype, raw_buffer, virtual_buffer)
    int regnum;
    struct type *valtype;
    char *raw_buffer;
    char *virtual_buffer;
{
  if (TYPE_LENGTH (valtype) >= REGISTER_RAW_SIZE (regnum))
    {
      memcpy (virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE (regnum));
      return;
    }

  if (TYPE_CODE (valtype) == TYPE_CODE_FLT)
    {
      double d = extract_floating (raw_buffer, REGISTER_RAW_SIZE (regnum));
      store_floating (virtual_buffer, TYPE_LENGTH (valtype), d);
    }
  else if (TYPE_CODE (valtype) == TYPE_CODE_INT && TYPE_LENGTH (valtype) <= 4)
    {
      unsigned LONGEST l;
      l = extract_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum));
      l = ((l >> 32) & 0xc0000000) | ((l >> 29) & 0x3fffffff);
      store_unsigned_integer (virtual_buffer, TYPE_LENGTH (valtype), l);
    }
  else
    error ("Cannot retrieve value from floating point register");
}

void
alpha_register_convert_to_raw (valtype, regnum, virtual_buffer, raw_buffer)
    struct type *valtype;
    int regnum;
    char *virtual_buffer;
    char *raw_buffer;
{
  if (TYPE_LENGTH (valtype) >= REGISTER_RAW_SIZE (regnum))
    {
      memcpy (raw_buffer, virtual_buffer, REGISTER_RAW_SIZE (regnum));
      return;
    }

  if (TYPE_CODE (valtype) == TYPE_CODE_FLT)
    {
      double d = extract_floating (virtual_buffer, TYPE_LENGTH (valtype));
      store_floating (raw_buffer, REGISTER_RAW_SIZE (regnum), d);
    }
  else if (TYPE_CODE (valtype) == TYPE_CODE_INT && TYPE_LENGTH (valtype) <= 4)
    {
      unsigned LONGEST l;
      if (TYPE_UNSIGNED (valtype))
	l = extract_unsigned_integer (virtual_buffer, TYPE_LENGTH (valtype));
      else
	l = extract_signed_integer (virtual_buffer, TYPE_LENGTH (valtype));
      l = ((l & 0xc0000000) << 32) | ((l & 0x3fffffff) << 29);
      store_unsigned_integer (raw_buffer, REGISTER_RAW_SIZE (regnum), l);
    }
  else
    error ("Cannot store value in floating point register");
}

/* Given a return value in `regbuf' with a type `valtype', 
   extract and copy its value into `valbuf'.  */

void
alpha_extract_return_value (valtype, regbuf, valbuf)
    struct type *valtype;
    char regbuf[REGISTER_BYTES];
    char *valbuf;
{
  if (TYPE_CODE (valtype) == TYPE_CODE_FLT)
    alpha_register_convert_to_virtual (FP0_REGNUM, valtype,
				       regbuf + REGISTER_BYTE (FP0_REGNUM),
				       valbuf);
  else
    memcpy (valbuf, regbuf + REGISTER_BYTE (V0_REGNUM), TYPE_LENGTH (valtype));
}

/* Given a return value in `regbuf' with a type `valtype', 
   write its value into the appropriate register.  */

void
alpha_store_return_value (valtype, valbuf)
    struct type *valtype;
    char *valbuf;
{
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  int regnum = V0_REGNUM;
  int length = TYPE_LENGTH (valtype);
  
  if (TYPE_CODE (valtype) == TYPE_CODE_FLT)
    {
      regnum = FP0_REGNUM;
      length = REGISTER_RAW_SIZE (regnum);
      alpha_register_convert_to_raw (valtype, regnum, valbuf, raw_buffer);
    }
  else
    memcpy (raw_buffer, valbuf, length);

  write_register_bytes (REGISTER_BYTE (regnum), raw_buffer, length);
}

/* Just like reinit_frame_cache, but with the right arguments to be
   callable as an sfunc.  */

static void
reinit_frame_cache_sfunc (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  reinit_frame_cache ();
}

/* This is the definition of CALL_DUMMY_ADDRESS.  It's a heuristic that is used
   to find a convenient place in the text segment to stick a breakpoint to
   detect the completion of a target function call (ala call_function_by_hand).
 */

CORE_ADDR
alpha_call_dummy_address ()
{
  CORE_ADDR entry;
  struct minimal_symbol *sym;

  entry = entry_point_address ();

  if (entry != 0)
    return entry;

  sym = lookup_minimal_symbol ("_Prelude", NULL, symfile_objfile);

  if (!sym || MSYMBOL_TYPE (sym) != mst_text)
    return 0;
  else
    return SYMBOL_VALUE_ADDRESS (sym) + 4;
}

void
_initialize_alpha_tdep ()
{
  struct cmd_list_element *c;

  tm_print_insn = print_insn_alpha;

  /* Let the user set the fence post for heuristic_proc_start.  */

  /* We really would like to have both "0" and "unlimited" work, but
     command.c doesn't deal with that.  So make it a var_zinteger
     because the user can always use "999999" or some such for unlimited.  */
  c = add_set_cmd ("heuristic-fence-post", class_support, var_zinteger,
		   (char *) &heuristic_fence_post,
		   "\
Set the distance searched for the start of a function.\n\
If you are debugging a stripped executable, GDB needs to search through the\n\
program for the start of a function.  This command sets the distance of the\n\
search.  The only need to set it is when debugging a stripped executable.",
		   &setlist);
  /* We need to throw away the frame cache when we set this, since it
     might change our ability to get backtraces.  */
  c->function.sfunc = reinit_frame_cache_sfunc;
  add_show_from_set (c, &showlist);
}
