/* Target-dependent code for Mitsubishi D30V, for GDB.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

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

/*  Contributed by Martin Hunt, hunt@cygnus.com */

#include "defs.h"
#include "frame.h"
#include "obstack.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "value.h"
#include "inferior.h"
#include "dis-asm.h"  
#include "symfile.h"
#include "objfiles.h"

void d30v_frame_find_saved_regs PARAMS ((struct frame_info *fi,
					 struct frame_saved_regs *fsr));
void d30v_frame_find_saved_regs_offsets PARAMS ((struct frame_info *fi,
						 struct frame_saved_regs *fsr));
static void d30v_pop_dummy_frame PARAMS ((struct frame_info *fi));
static void d30v_print_flags PARAMS ((void));
static void print_flags_command PARAMS ((char *, int));

/* the following defines assume:
   fp is r61, lr is r62, sp is r63, and ?? is r22
   if that changes, they will need to be updated */

#define OP_MASK_ALL_BUT_RA	0x0ffc0fff /* throw away Ra, keep the rest */

#define OP_STW_SPM		0x054c0fc0 /* stw Ra, @(sp-) */
#define OP_STW_SP_R0		0x05400fc0 /* stw Ra, @(sp,r0) */
#define OP_STW_SP_IMM0		0x05480fc0 /* st Ra, @(sp, 0x0) */
#define OP_STW_R22P_R0		0x05440580 /* stw Ra, @(r22+,r0) */

#define OP_ST2W_SPM		0x056c0fc0 /* st2w Ra, @(sp-) */
#define OP_ST2W_SP_R0		0x05600fc0 /* st2w Ra, @(sp, r0) */
#define OP_ST2W_SP_IMM0		0x05680fc0 /* st2w Ra, @(sp, 0x0) */
#define OP_ST2W_R22P_R0		0x05640580 /* st2w Ra, @(r22+, r0) */

#define OP_MASK_OPCODE		0x0ffc0000 /* just the opcode, ign operands */
#define OP_NOP			0x00f00000 /* nop */

#define OP_MASK_ALL_BUT_IMM	0x0fffffc0 /* throw away imm, keep the rest */
#define OP_SUB_SP_IMM		0x082bffc0 /* sub sp,sp,imm */
#define OP_ADD_SP_IMM		0x080bffc0 /* add sp,sp,imm */
#define OP_ADD_R22_SP_IMM	0x08096fc0 /* add r22,sp,imm */
#define OP_STW_FP_SP_IMM	0x054bdfc0 /* stw fp,@(sp,imm) */
#define OP_OR_SP_R0_IMM		0x03abf000 /* or sp,r0,imm */

/* no mask */
#define OP_OR_FP_R0_SP		0x03a3d03f /* or fp,r0,sp */
#define OP_OR_FP_SP_R0		0x03a3dfc0 /* or fp,sp,r0 */
#define OP_OR_FP_IMM0_SP	0x03abd03f /* or fp,0x0,sp */
#define OP_STW_FP_R22P_R0	0x0547d580 /* stw fp,@(r22+,r0) */
#define OP_STW_LR_R22P_R0	0x0547e580 /* stw lr,@(r22+,r0) */

#define OP_MASK_OP_AND_RB	0x0ff80fc0 /* keep op and rb,throw away rest */
#define OP_STW_SP_IMM		0x05480fc0 /* stw Ra,@(sp,imm) */
#define OP_ST2W_SP_IMM		0x05680fc0 /* st2w Ra,@(sp,imm) */
#define OP_STW_FP_IMM		0x05480f40 /* stw Ra,@(fp,imm) */
#define OP_STW_FP_R0		0x05400f40 /* stw Ra,@(fp,r0) */

#define OP_MASK_FM_BIT		0x80000000
#define OP_MASK_CC_BITS		0x70000000
#define OP_MASK_SUB_INST	0x0fffffff

#define EXTRACT_RA(op)		(((op) >> 12) & 0x3f)
#define EXTRACT_RB(op)		(((op) >> 6) & 0x3f)
#define EXTRACT_RC(op)		(((op) & 0x3f)
#define EXTRACT_UIMM6(op)	((op) & 0x3f)
#define EXTRACT_IMM6(op)	((((int)EXTRACT_UIMM6(op)) << 26) >> 26)
#define EXTRACT_IMM26(op)	((((op)&0x0ff00000) >> 2) | ((op)&0x0003ffff))
#define EXTRACT_IMM32(opl, opr)	((EXTRACT_UIMM6(opl) << 26)|EXTRACT_IMM26(opr))


int
d30v_frame_chain_valid (chain, fi)
     CORE_ADDR chain;
     struct frame_info *fi;      /* not used here */
{
#if 0
  return ((chain) != 0 && (fi) != 0 && (fi)->return_pc != 0);
#else
  return ((chain) != 0 && (fi) != 0 && (fi)->frame <= chain);
#endif
}

/* Discard from the stack the innermost frame, restoring all saved
   registers.  */

void
d30v_pop_frame ()
{
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR fp;
  int regnum;
  struct frame_saved_regs fsr;
  char raw_buffer[8];

  fp = FRAME_FP (frame);
  if (frame->dummy)
    {
      d30v_pop_dummy_frame(frame);
      return;
    }

  /* fill out fsr with the address of where each */
  /* register was stored in the frame */
  get_frame_saved_regs (frame, &fsr);
  
  /* now update the current registers with the old values */
  for (regnum = A0_REGNUM; regnum < A0_REGNUM+2 ; regnum++)
    {
      if (fsr.regs[regnum])
	{
	  read_memory (fsr.regs[regnum], raw_buffer, 8);
	  write_register_bytes (REGISTER_BYTE (regnum), raw_buffer, 8);
	}
    }
  for (regnum = 0; regnum < SP_REGNUM; regnum++)
    {
      if (fsr.regs[regnum])
	{
	  write_register (regnum, read_memory_unsigned_integer (fsr.regs[regnum], 4));
	}
    }
  if (fsr.regs[PSW_REGNUM])
    {
      write_register (PSW_REGNUM, read_memory_unsigned_integer (fsr.regs[PSW_REGNUM], 4));
    }

  write_register (PC_REGNUM, read_register(LR_REGNUM));
  write_register (SP_REGNUM, fp + frame->size);
  target_store_registers (-1);
  flush_cached_frames ();
}

static int 
check_prologue (op)
     unsigned long op;
{
  /* add sp,sp,imm -- observed */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_ADD_SP_IMM)
    return 1;

  /* add r22,sp,imm -- observed */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_ADD_R22_SP_IMM)
    return 1;

  /* or  fp,r0,sp -- observed */
  if (op == OP_OR_FP_R0_SP)
    return 1;

  /* nop */
  if ((op & OP_MASK_OPCODE) == OP_NOP)
    return 1;

  /* stw  Ra,@(sp,r0) */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_SP_R0)
    return 1;

  /* stw  Ra,@(sp,0x0) */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_SP_IMM0)
    return 1;

  /* st2w  Ra,@(sp,r0) */
 if ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_SP_R0)
   return 1;

  /* st2w  Ra,@(sp,0x0) */
 if ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_SP_IMM0)
   return 1;

 /* stw fp, @(r22+,r0) -- observed */
 if (op == OP_STW_FP_R22P_R0)
   return 1;

 /* stw r62, @(r22+,r0) -- observed */
 if (op == OP_STW_LR_R22P_R0)
   return 1;

 /* stw Ra, @(fp,r0) -- observed */
 if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_FP_R0)
   return 1;			/* first arg */

 /* stw Ra, @(fp,imm) -- observed */
 if ((op & OP_MASK_OP_AND_RB) == OP_STW_FP_IMM)
   return 1;			/* second and subsequent args */

 /* stw fp,@(sp,imm) -- observed */
 if ((op & OP_MASK_ALL_BUT_IMM) == OP_STW_FP_SP_IMM)
   return 1;

 /* st2w Ra,@(r22+,r0) */
 if ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_R22P_R0)
   return 1;

  /* stw  Ra, @(sp-) */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_SPM)
    return 1;

  /* st2w  Ra, @(sp-) */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_SPM)
    return 1;

  /* sub.?  sp,sp,imm */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_SUB_SP_IMM)
    return 1;

  return 0;
}

CORE_ADDR
d30v_skip_prologue (pc)
     CORE_ADDR pc;
{
  unsigned long op[2];
  unsigned long opl, opr;	/* left / right sub operations */
  unsigned long fm0, fm1;	/* left / right mode bits */
  unsigned long cc0, cc1;
  unsigned long op1, op2;
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;

  /* If we have line debugging information, then the end of the */
  /* prologue should the first assembly instruction of  the first source line */
  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);
      if ( sal.end && sal.end < func_end)
	return sal.end;
    }
  
  if (target_read_memory (pc, (char *)&op[0], 8))
    return pc;			/* Can't access it -- assume no prologue. */

  while (1)
    {
      opl = (unsigned long)read_memory_integer (pc, 4);
      opr = (unsigned long)read_memory_integer (pc+4, 4);

      fm0 = (opl & OP_MASK_FM_BIT);
      fm1 = (opr & OP_MASK_FM_BIT);

      cc0 = (opl & OP_MASK_CC_BITS);
      cc1 = (opr & OP_MASK_CC_BITS);

      opl = (opl & OP_MASK_SUB_INST);
      opr = (opr & OP_MASK_SUB_INST);

      if (fm0 && fm1)
	{
	  /* long instruction (opl contains the opcode) */
	  if (((opl & OP_MASK_ALL_BUT_IMM) != OP_ADD_SP_IMM) && /* add sp,sp,imm */
	      ((opl & OP_MASK_ALL_BUT_IMM) != OP_ADD_R22_SP_IMM) && /* add r22,sp,imm */
	      ((opl & OP_MASK_OP_AND_RB) != OP_STW_SP_IMM) && /* stw Ra, @(sp,imm) */
	      ((opl & OP_MASK_OP_AND_RB) != OP_ST2W_SP_IMM)) /* st2w Ra, @(sp,imm) */
	    break;
	}
      else
	{
	  /* short instructions */
	  if (fm0 && !fm1)
	    {
	      op1 = opr;
	      op2 = opl;
	    } 
	  else 
	    {
	      op1 = opl;
	      op2 = opr;
	    }
	  if (check_prologue(op1))
	    {
	      if (!check_prologue(op2))
		{
		  /* if the previous opcode was really part of the prologue */
		  /* and not just a NOP, then we want to break after both instructions */
		  if ((op1 & OP_MASK_OPCODE) != OP_NOP)
		    pc += 8;
		  break;
		}
	    }
	  else
	    break;
	}
      pc += 8;
    }
  return pc;
}

static int end_of_stack;

/* Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.
*/

CORE_ADDR
d30v_frame_chain (frame)
     struct frame_info *frame;
{
  struct frame_saved_regs fsr;

  d30v_frame_find_saved_regs (frame, &fsr);

  if (end_of_stack)
    return (CORE_ADDR)0;

  if (frame->return_pc == IMEM_START)
    return (CORE_ADDR)0;

  if (!fsr.regs[FP_REGNUM])
    {
      if (!fsr.regs[SP_REGNUM] || fsr.regs[SP_REGNUM] == STACK_START)
	return (CORE_ADDR)0;
      
      return fsr.regs[SP_REGNUM];
    }

  if (!read_memory_unsigned_integer(fsr.regs[FP_REGNUM],4))
    return (CORE_ADDR)0;

  return read_memory_unsigned_integer(fsr.regs[FP_REGNUM],4);
}  

static int next_addr, uses_frame;
static int frame_size;

static int 
prologue_find_regs (op, fsr, addr)
     unsigned long op;
     struct frame_saved_regs *fsr;
     CORE_ADDR addr;
{
  int n;
  int offset;

  /* add sp,sp,imm -- observed */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_ADD_SP_IMM)
    {
      offset = EXTRACT_IMM6(op);
      /*next_addr += offset;*/
      frame_size += -offset;
      return 1;
    }

  /* add r22,sp,imm -- observed */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_ADD_R22_SP_IMM)
    {
      offset = EXTRACT_IMM6(op);
      next_addr = (offset - frame_size);
      return 1;
    }

  /* stw Ra, @(fp, offset) -- observed */
  if ((op & OP_MASK_OP_AND_RB) == OP_STW_FP_IMM)
    {
      n = EXTRACT_RA(op);
      offset = EXTRACT_IMM6(op);
      fsr->regs[n] = (offset - frame_size);
      return 1;
    }

  /* stw Ra, @(fp, r0) -- observed */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_FP_R0)
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = (- frame_size);
      return 1;
    }

  /* or  fp,0,sp -- observed */
  if ((op == OP_OR_FP_R0_SP) ||
      (op == OP_OR_FP_SP_R0) ||
      (op == OP_OR_FP_IMM0_SP))
    {
      uses_frame = 1;
      return 1;
    }

  /* nop */
  if ((op & OP_MASK_OPCODE) == OP_NOP)
    return 1;

  /* stw Ra,@(r22+,r0) -- observed */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_R22P_R0)
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = next_addr;
      next_addr += 4;
      return 1;
    }
#if 0				/* subsumed in pattern above */
  /* stw fp,@(r22+,r0) -- observed */
  if (op == OP_STW_FP_R22P_R0)
    {
      fsr->regs[FP_REGNUM] = next_addr;	/* XXX */
      next_addr += 4;
      return 1;
    }

  /* stw r62,@(r22+,r0) -- observed */
  if (op == OP_STW_LR_R22P_R0)
    {
      fsr->regs[LR_REGNUM] = next_addr;
      next_addr += 4;
      return 1;
    }
#endif
  /* st2w Ra,@(r22+,r0) -- observed */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_R22P_R0)
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = next_addr;
      fsr->regs[n+1] = next_addr + 4;
      next_addr += 8;
      return 1;
    }

  /* stw  rn, @(sp-) */
  if ((op & OP_MASK_ALL_BUT_RA) == OP_STW_SPM)
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = next_addr;
      next_addr -= 4;
      return 1;
    }

  /* st2w  Ra, @(sp-) */
  else if ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_SPM)
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = next_addr;
      fsr->regs[n+1] = next_addr+4;
      next_addr -= 8;
      return 1;
    }

  /* sub  sp,sp,imm */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_SUB_SP_IMM)
    {
      offset = EXTRACT_IMM6(op);
      frame_size += -offset;
      return 1;
    }

  /* st  rn, @(sp,0) -- observed */
  if (((op & OP_MASK_ALL_BUT_RA) == OP_STW_SP_R0) ||
      ((op & OP_MASK_ALL_BUT_RA) == OP_STW_SP_IMM0))
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = (- frame_size);
      return 1;
    }

  /* st2w  rn, @(sp,0) */
  if (((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_SP_R0) ||
      ((op & OP_MASK_ALL_BUT_RA) == OP_ST2W_SP_IMM0))
    {
      n = EXTRACT_RA(op);
      fsr->regs[n] = (- frame_size);
      fsr->regs[n+1] = (- frame_size) + 4;
      return 1;
    }

  /* stw fp,@(sp,imm) -- observed */
  if ((op & OP_MASK_ALL_BUT_IMM) == OP_STW_FP_SP_IMM)
    {
      offset = EXTRACT_IMM6(op);
      fsr->regs[FP_REGNUM] = (offset - frame_size);
      return 1;
    }
  return 0;
}

/* Put here the code to store, into a struct frame_saved_regs, the
   addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special: the address we
   return for it IS the sp for the next frame. */
void
d30v_frame_find_saved_regs (fi, fsr)
     struct frame_info *fi;
     struct frame_saved_regs *fsr;
{
  CORE_ADDR fp, pc;
  unsigned long opl, opr;
  unsigned long op1, op2;
  unsigned long fm0, fm1;
  int i;

  fp = fi->frame;
  memset (fsr, 0, sizeof (*fsr));
  next_addr = 0;
  frame_size = 0;
  end_of_stack = 0;

  uses_frame = 0;

  d30v_frame_find_saved_regs_offsets (fi, fsr);
  
  fi->size = frame_size;

  if (!fp)
    fp = read_register(SP_REGNUM);

  for (i=0; i<NUM_REGS-1; i++)
    if (fsr->regs[i])
      {
	fsr->regs[i] = fsr->regs[i] + fp + frame_size;
      }

  if (fsr->regs[LR_REGNUM])
    fi->return_pc = read_memory_unsigned_integer(fsr->regs[LR_REGNUM],4);
  else
    fi->return_pc = read_register(LR_REGNUM);
  
  /* the SP is not normally (ever?) saved, but check anyway */
  if (!fsr->regs[SP_REGNUM])
    {
      /* if the FP was saved, that means the current FP is valid, */
      /* otherwise, it isn't being used, so we use the SP instead */
      if (uses_frame)
	fsr->regs[SP_REGNUM] = read_register(FP_REGNUM) + fi->size;
      else
	{
	  fsr->regs[SP_REGNUM] = fp + fi->size;
	  fi->frameless = 1;
	  fsr->regs[FP_REGNUM] = 0;
	}
    }
}

void
d30v_frame_find_saved_regs_offsets (fi, fsr)
     struct frame_info *fi;
     struct frame_saved_regs *fsr;
{
  CORE_ADDR fp, pc;
  unsigned long opl, opr;
  unsigned long op1, op2;
  unsigned long fm0, fm1;
  int i;

  fp = fi->frame;
  memset (fsr, 0, sizeof (*fsr));
  next_addr = 0;
  frame_size = 0;
  end_of_stack = 0;

  pc = get_pc_function_start (fi->pc);

  uses_frame = 0;
  while (pc < fi->pc)
    {
      opl = (unsigned long)read_memory_integer (pc, 4);
      opr = (unsigned long)read_memory_integer (pc+4, 4);

      fm0 = (opl & OP_MASK_FM_BIT);
      fm1 = (opr & OP_MASK_FM_BIT);

      opl = (opl & OP_MASK_SUB_INST);
      opr = (opr & OP_MASK_SUB_INST);

      if (fm0 && fm1)
	{
	  /* long instruction */
	  if ((opl & OP_MASK_ALL_BUT_IMM) == OP_ADD_SP_IMM)
	    {
	      /* add sp,sp,n */
	      long offset = EXTRACT_IMM32(opl, opr);
	      frame_size += -offset;
	    }
	  else if ((opl & OP_MASK_ALL_BUT_IMM) == OP_ADD_R22_SP_IMM)
	    {
	      /* add r22,sp,offset */
	      long offset = EXTRACT_IMM32(opl,opr);
	      next_addr = (offset - frame_size);
	    }
	  else if ((opl & OP_MASK_OP_AND_RB) == OP_STW_SP_IMM)
	    {
	      /* st Ra, @(sp,imm) */
	      long offset = EXTRACT_IMM32(opl, opr);
	      short n = EXTRACT_RA(opl);
	      fsr->regs[n] = (offset - frame_size);
	    }
	  else if ((opl & OP_MASK_OP_AND_RB) == OP_ST2W_SP_IMM)
	    {
	      /* st2w Ra, @(sp,offset) */
	      long offset = EXTRACT_IMM32(opl, opr);
	      short n = EXTRACT_RA(opl);
	      fsr->regs[n] = (offset - frame_size);
	      fsr->regs[n+1] = (offset - frame_size) + 4;
	    }
	  else if ((opl & OP_MASK_ALL_BUT_IMM) == OP_OR_SP_R0_IMM)
	    {
	      end_of_stack = 1;
	    }
	  else
	    break;
	}
      else
	{
	  /* short instructions */
	  if (fm0 && !fm1)
	    {
	      op2 = opl;
	      op1 = opr;
	    } 
	  else 
	    {
	      op1 = opl;
	      op2 = opr;
	    }
	  if (!prologue_find_regs(op1,fsr,pc) || !prologue_find_regs(op2,fsr,pc))
	    break;
	}
      pc += 8;
    }
  
#if 0
  fi->size = frame_size;

  if (!fp)
    fp = read_register(SP_REGNUM);

  for (i=0; i<NUM_REGS-1; i++)
    if (fsr->regs[i])
      {
	fsr->regs[i] = fsr->regs[i] + fp + frame_size;
      }

  if (fsr->regs[LR_REGNUM])
    fi->return_pc = read_memory_unsigned_integer(fsr->regs[LR_REGNUM],4);
  else
    fi->return_pc = read_register(LR_REGNUM);
  
  /* the SP is not normally (ever?) saved, but check anyway */
  if (!fsr->regs[SP_REGNUM])
    {
      /* if the FP was saved, that means the current FP is valid, */
      /* otherwise, it isn't being used, so we use the SP instead */
      if (uses_frame)
	fsr->regs[SP_REGNUM] = read_register(FP_REGNUM) + fi->size;
      else
	{
	  fsr->regs[SP_REGNUM] = fp + fi->size;
	  fi->frameless = 1;
	  fsr->regs[FP_REGNUM] = 0;
	}
    }
#endif
}

void
d30v_init_extra_frame_info (fromleaf, fi)
     int fromleaf;
     struct frame_info *fi;
{
  struct frame_saved_regs dummy;

  if (fi->next && (fi->pc == 0))
    fi->pc = fi->next->return_pc; 

  d30v_frame_find_saved_regs_offsets (fi, &dummy);

  if (uses_frame == 0)
    fi->frameless = 1;
  else
    fi->frameless = 0;

  if ((fi->next == 0) && (uses_frame == 0))
    /* innermost frame and it's "frameless",
       so the fi->frame field is wrong, fix it! */
    fi->frame = read_sp ();

  if (dummy.regs[LR_REGNUM])
    {
      /* it was saved, grab it! */
      dummy.regs[LR_REGNUM] += (fi->frame + frame_size);
      fi->return_pc = read_memory_unsigned_integer(dummy.regs[LR_REGNUM],4);
    }
  else
    fi->return_pc = read_register(LR_REGNUM);
}

void
d30v_init_frame_pc (fromleaf, prev)
     int fromleaf;
     struct frame_info *prev;
{
  /* default value, put here so we can breakpoint on it and
     see if the default value is really the right thing to use */
  prev->pc = (fromleaf ? SAVED_PC_AFTER_CALL (prev->next) : \
	      prev->next ? FRAME_SAVED_PC (prev->next) : read_pc ());
}

static void d30v_print_register PARAMS ((int regnum, int tabular));

static void
d30v_print_register (regnum, tabular)
     int regnum;
     int tabular;
{
  if (regnum < A0_REGNUM)
    {
      if (tabular)
	printf_filtered ("%08x", read_register (regnum));
      else
	printf_filtered ("0x%x	%d", read_register (regnum),
			 read_register (regnum));
    }
  else
    {
      char regbuf[MAX_REGISTER_RAW_SIZE];

      read_relative_register_raw_bytes (regnum, regbuf);

      val_print (REGISTER_VIRTUAL_TYPE (regnum), regbuf, 0, 0,
		 gdb_stdout, 'x', 1, 0, Val_pretty_default);

      if (!tabular)
	{
	  printf_filtered ("	");
	  val_print (REGISTER_VIRTUAL_TYPE (regnum), regbuf, 0, 0,
		 gdb_stdout, 'd', 1, 0, Val_pretty_default);
	}
    }
}

static void
d30v_print_flags ()
{
  long psw = read_register (PSW_REGNUM);
  printf_filtered ("flags #1");
  printf_filtered ("   (sm) %d", (psw & PSW_SM) != 0);
  printf_filtered ("   (ea) %d", (psw & PSW_EA) != 0);
  printf_filtered ("   (db) %d", (psw & PSW_DB) != 0);
  printf_filtered ("   (ds) %d", (psw & PSW_DS) != 0);
  printf_filtered ("   (ie) %d", (psw & PSW_IE) != 0);
  printf_filtered ("   (rp) %d", (psw & PSW_RP) != 0);
  printf_filtered ("   (md) %d\n", (psw & PSW_MD) != 0);

  printf_filtered ("flags #2");
  printf_filtered ("   (f0) %d", (psw & PSW_F0) != 0);
  printf_filtered ("   (f1) %d", (psw & PSW_F1) != 0);
  printf_filtered ("   (f2) %d", (psw & PSW_F2) != 0);
  printf_filtered ("   (f3) %d", (psw & PSW_F3) != 0);
  printf_filtered ("    (s) %d", (psw & PSW_S) != 0);
  printf_filtered ("    (v) %d", (psw & PSW_V) != 0);
  printf_filtered ("   (va) %d", (psw & PSW_VA) != 0);
  printf_filtered ("    (c) %d\n", (psw & PSW_C) != 0);
}

static void
print_flags_command (args, from_tty)
     char *args;
     int from_tty;
{
  d30v_print_flags ();
}

void
d30v_do_registers_info (regnum, fpregs)
     int regnum;
     int fpregs;
{
  long long num1, num2;
  long psw;

  if (regnum != -1)
    {
      if (REGISTER_NAME (0) == NULL || REGISTER_NAME (0)[0] == '\000')
	return;

      printf_filtered ("%s ", REGISTER_NAME (regnum));
      d30v_print_register (regnum, 0);

      printf_filtered ("\n");
      return;
    }

  /* Have to print all the registers.  Format them nicely.  */

  printf_filtered ("PC=");
  print_address (read_pc (), gdb_stdout);

  printf_filtered (" PSW=");
  d30v_print_register (PSW_REGNUM, 1);

  printf_filtered (" BPC=");
  print_address (read_register (BPC_REGNUM), gdb_stdout);

  printf_filtered (" BPSW=");
  d30v_print_register (BPSW_REGNUM, 1);
  printf_filtered ("\n");

  printf_filtered ("DPC=");
  print_address (read_register (DPC_REGNUM), gdb_stdout);

  printf_filtered (" DPSW=");
  d30v_print_register (DPSW_REGNUM, 1);

  printf_filtered (" IBA=");
  print_address (read_register (IBA_REGNUM), gdb_stdout);
  printf_filtered ("\n");

  printf_filtered ("RPT_C=");
  d30v_print_register (RPT_C_REGNUM, 1);

  printf_filtered (" RPT_S=");
  print_address (read_register (RPT_S_REGNUM), gdb_stdout);

  printf_filtered (" RPT_E=");
  print_address (read_register (RPT_E_REGNUM), gdb_stdout);
  printf_filtered ("\n");

  printf_filtered ("MOD_S=");
  print_address (read_register (MOD_S_REGNUM), gdb_stdout);

  printf_filtered (" MOD_E=");
  print_address (read_register (MOD_E_REGNUM), gdb_stdout);
  printf_filtered ("\n");

  printf_filtered ("EIT_VB=");
  print_address (read_register (EIT_VB_REGNUM), gdb_stdout);

  printf_filtered (" INT_S=");
  d30v_print_register (INT_S_REGNUM, 1);

  printf_filtered (" INT_M=");
  d30v_print_register (INT_M_REGNUM, 1);
  printf_filtered ("\n");

  d30v_print_flags ();
  for (regnum = 0; regnum <= 63;)
    {
      int i;

      printf_filtered ("R%d-R%d ", regnum, regnum + 7);
      if (regnum < 10)
	printf_filtered (" ");
      if (regnum + 7 < 10)
	printf_filtered (" ");

      for (i = 0; i < 8; i++)
	{
	  printf_filtered (" ");
	  d30v_print_register (regnum++, 1);
	}

      printf_filtered ("\n");
    }

  printf_filtered ("A0-A1    ");

  d30v_print_register (A0_REGNUM, 1);
  printf_filtered ("    ");
  d30v_print_register (A1_REGNUM, 1);
  printf_filtered ("\n");
}

CORE_ADDR
d30v_fix_call_dummy (dummyname, start_sp, fun, nargs, args, type, gcc_p)
     char *dummyname;
     CORE_ADDR start_sp;
     CORE_ADDR fun;
     int nargs;
     value_ptr *args;
     struct type *type;
     int gcc_p;
{
  int regnum;
  CORE_ADDR sp;
  char buffer[MAX_REGISTER_RAW_SIZE];
  struct frame_info *frame = get_current_frame ();
  frame->dummy = start_sp;
  /*start_sp |= DMEM_START;*/

  sp = start_sp;
  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      sp -= REGISTER_RAW_SIZE(regnum);
      store_address (buffer, REGISTER_RAW_SIZE(regnum), read_register(regnum));
      write_memory (sp, buffer, REGISTER_RAW_SIZE(regnum));
    }
  write_register (SP_REGNUM, (LONGEST)sp);
  /* now we need to load LR with the return address */
  write_register (LR_REGNUM, (LONGEST)d30v_call_dummy_address());  
  return sp;
}

static void
d30v_pop_dummy_frame (fi)
     struct frame_info *fi;
{
  CORE_ADDR sp = fi->dummy;
  int regnum;

  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      sp -= REGISTER_RAW_SIZE(regnum);
      write_register(regnum, read_memory_unsigned_integer (sp, REGISTER_RAW_SIZE(regnum)));
    }
  flush_cached_frames (); /* needed? */
}


CORE_ADDR
d30v_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  int i, len, index=0, regnum=2;
  char buffer[4], *contents;
  LONGEST val;
  CORE_ADDR ptrs[10];

#if 0
  /* Pass 1. Put all large args on stack */
  for (i = 0; i < nargs; i++)
    {
      value_ptr arg = args[i];
      struct type *arg_type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (arg_type);
      contents = VALUE_CONTENTS(arg);
      val = extract_signed_integer (contents, len);
      if (len > 4)
	{
	  /* put on stack and pass pointers */
	  sp -= len;
	  write_memory (sp, contents, len);
	  ptrs[index++] = sp;
	}
    }
#endif
  index = 0;

  for (i = 0; i < nargs; i++)
    {
      value_ptr arg = args[i];
      struct type *arg_type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (arg_type);
      contents = VALUE_CONTENTS(arg);
      if (len > 4)
	{
	  /* we need multiple registers */
	  int ndx;

	  for (ndx = 0; len > 0; ndx += 8, len -= 8)
	    {
	      if (regnum & 1)
		regnum++;	/* all args > 4 bytes start in even register */

	      if (regnum < 18)
		{
		  val = extract_signed_integer (&contents[ndx], 4);
		  write_register (regnum++, val);

		  if (len >= 8)
		    val = extract_signed_integer (&contents[ndx+4], 4);
		  else
		    val = extract_signed_integer (&contents[ndx+4], len-4);
		  write_register (regnum++, val);
		}
	      else
		{
		  /* no more registers available.  put it on the stack */

		  /* all args > 4 bytes are padded to a multiple of 8 bytes
		   and start on an 8 byte boundary */
		  if (sp & 7)
		    sp -= (sp & 7); /* align it */

		  sp -= ((len + 7) & ~7); /* allocate space */
		  write_memory (sp, &contents[ndx], len);
		  break;
		}
	    }
	}
      else
	{
	  if (regnum < 18 )
	    {
	      val = extract_signed_integer (contents, len);
	      write_register (regnum++, val);
	    }
	  else
	    {
	      /* all args are padded to a multiple of 4 bytes (at least) */
	      sp -= ((len + 3) & ~3);
	      write_memory (sp, contents, len);
	    }
	}
    }
  if (sp & 7)
    /* stack pointer is not on an 8 byte boundary -- align it */
    sp -= (sp & 7);
  return sp;
}


/* pick an out-of-the-way place to set the return value */
/* for an inferior function call.  The link register is set to this  */
/* value and a momentary breakpoint is set there.  When the breakpoint */
/* is hit, the dummy frame is popped and the previous environment is */
/* restored. */

CORE_ADDR
d30v_call_dummy_address ()
{
  CORE_ADDR entry;
  struct minimal_symbol *sym;

  entry = entry_point_address ();

  if (entry != 0)
    return entry;

  sym = lookup_minimal_symbol ("_start", NULL, symfile_objfile);

  if (!sym || MSYMBOL_TYPE (sym) != mst_text)
    return 0;
  else
    return SYMBOL_VALUE_ADDRESS (sym);
}

/* Given a return value in `regbuf' with a type `valtype', 
   extract and copy its value into `valbuf'.  */

void
d30v_extract_return_value (valtype, regbuf, valbuf)
     struct type *valtype;
     char regbuf[REGISTER_BYTES];
     char *valbuf;
{
  memcpy (valbuf, regbuf + REGISTER_BYTE (2), TYPE_LENGTH (valtype));
}

/* The following code implements access to, and display of, the D30V's
   instruction trace buffer.  The buffer consists of 64K or more
   4-byte words of data, of which each words includes an 8-bit count,
   an 8-bit segment number, and a 16-bit instruction address.

   In theory, the trace buffer is continuously capturing instruction
   data that the CPU presents on its "debug bus", but in practice, the
   ROMified GDB stub only enables tracing when it continues or steps
   the program, and stops tracing when the program stops; so it
   actually works for GDB to read the buffer counter out of memory and
   then read each trace word.  The counter records where the tracing
   stops, but there is no record of where it started, so we remember
   the PC when we resumed and then search backwards in the trace
   buffer for a word that includes that address.  This is not perfect,
   because you will miss trace data if the resumption PC is the target
   of a branch.  (The value of the buffer counter is semi-random, any
   trace data from a previous program stop is gone.)  */

/* The address of the last word recorded in the trace buffer.  */

#define DBBC_ADDR (0xd80000)

/* The base of the trace buffer, at least for the "Board_0".  */

#define TRACE_BUFFER_BASE (0xf40000)

static void trace_command PARAMS ((char *, int));

static void untrace_command PARAMS ((char *, int));

static void trace_info PARAMS ((char *, int));

static void tdisassemble_command PARAMS ((char *, int));

static void display_trace PARAMS ((int, int));

/* True when instruction traces are being collected.  */

static int tracing;

/* Remembered PC.  */

static CORE_ADDR last_pc;

/* True when trace output should be displayed whenever program stops.  */

static int trace_display;

/* True when trace listing should include source lines.  */

static int default_trace_show_source = 1;

struct trace_buffer {
  int size;
  short *counts;
  CORE_ADDR *addrs;
} trace_data;

static void
trace_command (args, from_tty)
     char *args;
     int from_tty;
{
  /* Clear the host-side trace buffer, allocating space if needed.  */
  trace_data.size = 0;
  if (trace_data.counts == NULL)
    trace_data.counts = (short *) xmalloc (65536 * sizeof(short));
  if (trace_data.addrs == NULL)
    trace_data.addrs = (CORE_ADDR *) xmalloc (65536 * sizeof(CORE_ADDR));

  tracing = 1;

  printf_filtered ("Tracing is now on.\n");
}

static void
untrace_command (args, from_tty)
     char *args;
     int from_tty;
{
  tracing = 0;

  printf_filtered ("Tracing is now off.\n");
}

static void
trace_info (args, from_tty)
     char *args;
     int from_tty;
{
  int i;

  if (trace_data.size)
    {
      printf_filtered ("%d entries in trace buffer:\n", trace_data.size);

      for (i = 0; i < trace_data.size; ++i)
	{
	  printf_filtered ("%d: %d instruction%s at 0x%x\n",
			   i, trace_data.counts[i],
			   (trace_data.counts[i] == 1 ? "" : "s"),
			   trace_data.addrs[i]);
	}
    }
  else
    printf_filtered ("No entries in trace buffer.\n");

  printf_filtered ("Tracing is currently %s.\n", (tracing ? "on" : "off"));
}

/* Print the instruction at address MEMADDR in debugged memory,
   on STREAM.  Returns length of the instruction, in bytes.  */

static int
print_insn (memaddr, stream)
     CORE_ADDR memaddr;
     GDB_FILE *stream;
{
  /* If there's no disassembler, something is very wrong.  */
  if (tm_print_insn == NULL)
    abort ();

  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    tm_print_insn_info.endian = BFD_ENDIAN_BIG;
  else
    tm_print_insn_info.endian = BFD_ENDIAN_LITTLE;
  return (*tm_print_insn) (memaddr, &tm_print_insn_info);
}

void
d30v_eva_prepare_to_trace ()
{
  if (!tracing)
    return;

  last_pc = read_register (PC_REGNUM);
}

/* Collect trace data from the target board and format it into a form
   more useful for display.  */

void
d30v_eva_get_trace_data ()
{
  int count, i, j, oldsize;
  int trace_addr, trace_seg, trace_cnt, next_cnt;
  unsigned int last_trace, trace_word, next_word;
  unsigned int *tmpspace;

  if (!tracing)
    return;

  tmpspace = xmalloc (65536 * sizeof(unsigned int));

  last_trace = read_memory_unsigned_integer (DBBC_ADDR, 2) << 2;

  /* Collect buffer contents from the target, stopping when we reach
     the word recorded when execution resumed.  */

  count = 0;
  while (last_trace > 0)
    {
      QUIT;
      trace_word =
	read_memory_unsigned_integer (TRACE_BUFFER_BASE + last_trace, 4);
      trace_addr = trace_word & 0xffff;
      last_trace -= 4;
      /* Ignore an apparently nonsensical entry.  */
      if (trace_addr == 0xffd5)
	continue;
      tmpspace[count++] = trace_word;
      if (trace_addr == last_pc)
	break;
      if (count > 65535)
	break;
    }

  /* Move the data to the host-side trace buffer, adjusting counts to
     include the last instruction executed and transforming the address
     into something that GDB likes.  */

  for (i = 0; i < count; ++i)
    {
      trace_word = tmpspace[i];
      next_word = ((i == 0) ? 0 : tmpspace[i - 1]);
      trace_addr = trace_word & 0xffff;
      next_cnt = (next_word >> 24) & 0xff;
      j = trace_data.size + count - i - 1;
      trace_data.addrs[j] = (trace_addr << 2) + 0x1000000;
      trace_data.counts[j] = next_cnt + 1;
    }

  oldsize = trace_data.size;
  trace_data.size += count;

  free (tmpspace);

  if (trace_display)
    display_trace (oldsize, trace_data.size);
}

static void
tdisassemble_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  int i, count;
  CORE_ADDR low, high;
  char *space_index;

  if (!arg)
    {
      low = 0;
      high = trace_data.size;
    }
  else if (!(space_index = (char *) strchr (arg, ' ')))
    {
      low = parse_and_eval_address (arg);
      high = low + 5;
    }
  else
    {
      /* Two arguments.  */
      *space_index = '\0';
      low = parse_and_eval_address (arg);
      high = parse_and_eval_address (space_index + 1);
      if (high < low)
	high = low;
    }

  printf_filtered ("Dump of trace from %d to %d:\n", low, high);

  display_trace (low, high);

  printf_filtered ("End of trace dump.\n");
  gdb_flush (gdb_stdout);
}

static void
display_trace (low, high)
     int low, high;
{
  int i, count, trace_show_source, first, suppress;
  CORE_ADDR next_address;

  trace_show_source = default_trace_show_source;
  if (!have_full_symbols () && !have_partial_symbols())
    {
      trace_show_source = 0;
      printf_filtered ("No symbol table is loaded.  Use the \"file\" command.\n");
      printf_filtered ("Trace will not display any source.\n");
    }

  first = 1;
  suppress = 0;
  for (i = low; i < high; ++i)
    {
      next_address = trace_data.addrs[i];
      count = trace_data.counts[i]; 
      while (count-- > 0)
	{
	  QUIT;
	  if (trace_show_source)
	    {
	      struct symtab_and_line sal, sal_prev;

	      sal_prev = find_pc_line (next_address - 4, 0);
	      sal = find_pc_line (next_address, 0);

	      if (sal.symtab)
		{
		  if (first || sal.line != sal_prev.line)
		    print_source_lines (sal.symtab, sal.line, sal.line + 1, 0);
		  suppress = 0;
		}
	      else
		{
		  if (!suppress)
		    /* FIXME-32x64--assumes sal.pc fits in long.  */
		    printf_filtered ("No source file for address %s.\n",
				     local_hex_string((unsigned long) sal.pc));
		  suppress = 1;
		}
	    }
	  first = 0;
	  print_address (next_address, gdb_stdout);
	  printf_filtered (":");
	  printf_filtered ("\t");
	  wrap_here ("    ");
	  next_address = next_address + print_insn (next_address, gdb_stdout);
	  printf_filtered ("\n");
	  gdb_flush (gdb_stdout);
	}
    }
}

extern void (*target_resume_hook) PARAMS ((void));
extern void (*target_wait_loop_hook) PARAMS ((void));

void
_initialize_d30v_tdep ()
{
  tm_print_insn = print_insn_d30v;

  target_resume_hook = d30v_eva_prepare_to_trace;
  target_wait_loop_hook = d30v_eva_get_trace_data;

  add_info ("flags", print_flags_command, "Print d30v flags.");

  add_com ("trace", class_support, trace_command,
	   "Enable tracing of instruction execution.");

  add_com ("untrace", class_support, untrace_command,
 	   "Disable tracing of instruction execution.");

  add_com ("tdisassemble", class_vars, tdisassemble_command,
	   "Disassemble the trace buffer.\n\
Two optional arguments specify a range of trace buffer entries\n\
as reported by info trace (NOT addresses!).");

  add_info ("trace", trace_info,
	    "Display info about the trace data buffer.");

  add_show_from_set (add_set_cmd ("tracedisplay", no_class,
				  var_integer, (char *)&trace_display,
				  "Set automatic display of trace.\n", &setlist),
		     &showlist);
  add_show_from_set (add_set_cmd ("tracesource", no_class,
				  var_integer, (char *)&default_trace_show_source,
				  "Set display of source code with trace.\n", &setlist),
		     &showlist);

} 
