/* Target-dependent code for the Acorn Risc Machine, for GDB, the GNU Debugger.
   Copyright (C) 1988, 1989, 1991, 1992, 1993, 1995, 1996, 1998, 1999
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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "symfile.h"
#include "gdb_string.h"
#include "coff/internal.h"	/* Internal format of COFF symbols in BFD */

/*
  The following macros are actually wrong.  Neither arm nor thumb can
  or should set the lsb on addr.
  The thumb addresses are mod 2, so (addr & 2) would be a good heuristic
  to use when checking for thumb (see arm_pc_is_thumb() below).
  Unfortunately, something else depends on these (incorrect) macros, so
  fixing them actually breaks gdb.  I didn't have time to investigate. Z.R.
*/
/* Thumb function addresses are odd (bit 0 is set).  Here are some
   macros to test, set, or clear bit 0 of addresses.  */
#define IS_THUMB_ADDR(addr)	((addr) & 1)
#define MAKE_THUMB_ADDR(addr)	((addr) | 1)
#define UNMAKE_THUMB_ADDR(addr) ((addr) & ~1)

/* Macros to round N up or down to the next A boundary; A must be
   a power of two. */
#define ROUND_DOWN(n,a) 	((n) & ~((a) - 1))
#define ROUND_UP(n,a) 		(((n) + (a) - 1) & ~((a) - 1))
  
/* Should call_function allocate stack space for a struct return?  */
/* The system C compiler uses a similar structure return convention to gcc */
int
arm_use_struct_convention (gcc_p, type)
     int gcc_p;
     struct type *type;
{
  return (TYPE_LENGTH (type) > 4);
}

int
arm_frame_chain_valid (chain, thisframe)
     CORE_ADDR chain;
     struct frame_info *thisframe;
{
#define LOWEST_PC 0x20  /* the first 0x20 bytes are the trap vectors. */
  return (chain != 0 && (FRAME_SAVED_PC (thisframe) >= LOWEST_PC));
}

/* Set to true if the 32-bit mode is in use. */

int arm_apcs_32 = 1;

/* Flag set by arm_fix_call_dummy that tells whether the target function
   is a Thumb function.  This flag is checked by arm_push_arguments.
   FIXME: Change the PUSH_ARGUMENTS macro (and its use in valops.c) to
   pass the function address as an additional parameter.  */

static int target_is_thumb;

/* Flag set by arm_fix_call_dummy that tells whether the calling function
   is a Thumb function.  This flag is checked by arm_pc_is_thumb
   and arm_call_dummy_breakpoint_offset.  */

static int caller_is_thumb;

/* Tell if the program counter value in MEMADDR is in a Thumb function.  */

int
arm_pc_is_thumb (memaddr)
     bfd_vma memaddr;
{
  struct minimal_symbol * sym;
  CORE_ADDR sp;

  /* If bit 0 of the address is set, assume this is a Thumb address. */
  if (IS_THUMB_ADDR (memaddr))
    return 1;

  /* Thumb function have a "special" bit set in minimal symbols */
  sym = lookup_minimal_symbol_by_pc (memaddr);
  if (sym)
    {
      return (MSYMBOL_IS_SPECIAL(sym));
    }
  else
    return 0;
}

/* Tell if the program counter value in MEMADDR is in a call dummy that
   is being called from a Thumb function.  */

int
arm_pc_is_thumb_dummy (memaddr)
     bfd_vma memaddr;
{
  CORE_ADDR sp = read_sp();

  if (PC_IN_CALL_DUMMY (memaddr, sp, sp+64))
    return caller_is_thumb;
  else
    return 0;
}

CORE_ADDR
arm_addr_bits_remove (val)
     CORE_ADDR val;
{
  if (arm_pc_is_thumb (val))
    return (val & (arm_apcs_32 ? 0xfffffffe : 0x03fffffe));
  else
    return (val & (arm_apcs_32 ? 0xfffffffc : 0x03fffffc));
}

CORE_ADDR
arm_saved_pc_after_call (frame)
     struct frame_info *frame;
{
  return ADDR_BITS_REMOVE (read_register (LR_REGNUM));
}

/* A typical Thumb prologue looks like this:
        push    {r7, lr}
        add     sp, sp, #-28
        add     r7, sp, #12
   Sometimes the latter instruction may be replaced by:
        mov     r7, sp 
*/

static CORE_ADDR
thumb_skip_prologue (pc)
     CORE_ADDR pc;
{
  CORE_ADDR current_pc;

  for (current_pc = pc; current_pc < pc + 20; current_pc += 2)
    {
      unsigned short insn = read_memory_unsigned_integer (current_pc, 2);

      if (   (insn & 0xfe00) != 0xb400		/* push {..., r7, lr} */
	  && (insn & 0xff00) != 0xb000		/* add sp, #simm */
	  && (insn & 0xff00) != 0xaf00		/* add r7, sp, #imm */
	  && insn != 0x466f			/* mov r7, sp */
	  && (insn & 0xffc0) != 0x4640)		/* mov r0-r7, r8-r15 */
	break;
    }

  return current_pc;
}

/* APCS (ARM procedure call standard) defines the following prologue:

   mov		ip, sp
  [stmfd	sp!, {a1,a2,a3,a4}]
   stmfd	sp!, {...,fp,ip,lr,pc}
  [stfe		f7, [sp, #-12]!]
  [stfe		f6, [sp, #-12]!]
  [stfe		f5, [sp, #-12]!]
  [stfe		f4, [sp, #-12]!]
   sub		fp, ip, #nn	// nn == 20 or 4 depending on second ins
*/

CORE_ADDR
arm_skip_prologue (pc)
     CORE_ADDR pc;
{
  unsigned long inst;
  CORE_ADDR skip_pc;
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;

  /* See what the symbol table says. */
  if (find_pc_partial_function (pc, NULL, & func_addr, & func_end))
    {
      sal = find_pc_line (func_addr, 0);
      if (sal.line != 0 && sal.end < func_end)
	return sal.end;
    }

  /* Check if this is Thumb code.  */
  if (arm_pc_is_thumb (pc))
    return thumb_skip_prologue (pc);

  /* Can't find the prologue end in the symbol table, try it the hard way
     by disassembling the instructions. */
  skip_pc = pc;
  inst = read_memory_integer (skip_pc, 4);
  if (inst != 0xe1a0c00d)  /* mov ip, sp */
    return pc;

  skip_pc += 4;
  inst = read_memory_integer (skip_pc, 4);
  if ((inst & 0xfffffff0) == 0xe92d0000)  /* stmfd sp!,{a1,a2,a3,a4}  */
    {
      skip_pc += 4;
      inst = read_memory_integer (skip_pc, 4);
    }

  if ((inst & 0xfffff800) != 0xe92dd800)  /* stmfd sp!,{...,fp,ip,lr,pc} */
    return pc;

  skip_pc += 4;
  inst = read_memory_integer (skip_pc, 4);

  /* Any insns after this point may float into the code, if it makes
     for better instruction scheduling, so we skip them only if
     we find them, but still consdier the function to be frame-ful  */

  /* We may have either one sfmfd instruction here, or several stfe insns,
     depending on the version of floating point code we support.  */
  if ((inst & 0xffbf0fff) == 0xec2d0200)  /* sfmfd fn, <cnt>, [sp]! */
    {
      skip_pc += 4;
      inst = read_memory_integer (skip_pc, 4);
    }
  else
    {
      while ((inst & 0xffff8fff) == 0xed6d0103) /* stfe fn, [sp, #-12]! */
        {
          skip_pc += 4;
          inst = read_memory_integer (skip_pc, 4);
        }
    }

  if ((inst & 0xfffff000) == 0xe24cb000) /* sub fp, ip, #nn */
    skip_pc += 4;

  return skip_pc;
}



/* Function: thumb_scan_prologue (helper function for arm_scan_prologue)
   This function decodes a Thumb function prologue to determine:
     1) the size of the stack frame
     2) which registers are saved on it
     3) the offsets of saved regs
     4) the offset from the stack pointer to the frame pointer
   This information is stored in the "extra" fields of the frame_info.

   A typical Thumb function prologue might look like this:
	push {r7, lr}
	sub  sp, #28,
	add  r7, sp, #12
   Which would create this stack frame (offsets relative to FP)
     old SP ->	24  stack parameters
		20  LR
		16  R7
     R7 ->       0  local variables (16 bytes)
     SP ->     -12  additional stack space (12 bytes)
   The frame size would thus be 36 bytes, and the frame offset would be
   12 bytes.  The frame register is R7.  */
	
static void
thumb_scan_prologue (fi)
     struct frame_info * fi;
{
  CORE_ADDR prologue_start;
  CORE_ADDR prologue_end;
  CORE_ADDR current_pc;
  int       saved_reg[16];	/* which register has been copied to register n? */
  int       i;

  if (find_pc_partial_function (fi->pc, NULL, & prologue_start, & prologue_end))
    {
      struct symtab_and_line sal = find_pc_line (prologue_start, 0);

      if (sal.line == 0)		/* no line info, use current PC */
	prologue_end = fi->pc;
      else if (sal.end < prologue_end)	/* next line begins after fn end */
	prologue_end = sal.end;		/* (probably means no prologue)  */
    }
  else
    prologue_end = prologue_start + 40; /* We're in the boondocks: allow for */
					/* 16 pushes, an add, and "mv fp,sp" */

  prologue_end = min (prologue_end, fi->pc);

  /* Initialize the saved register map.  When register H is copied to
     register L, we will put H in saved_reg[L].  */
  for (i = 0; i < 16; i++)
    saved_reg[i] = i;

  /* Search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.  */

  fi->framesize = 0;
  for (current_pc = prologue_start; current_pc < prologue_end; current_pc += 2)
    {
      unsigned short insn;
      int regno;
      int offset;

      insn = read_memory_unsigned_integer (current_pc, 2);

      if ((insn & 0xfe00) == 0xb400)		/* push { rlist } */
	{
	  /* Bits 0-7 contain a mask for registers R0-R7.  Bit 8 says
	     whether to save LR (R14).  */
	  int mask = (insn & 0xff) | ((insn & 0x100) << 6);

	  /* Calculate offsets of saved R0-R7 and LR. */
	  for (regno = LR_REGNUM; regno >= 0; regno--)
	    if (mask & (1 << regno))
 	      {
		fi->framesize += 4;
		fi->fsr.regs[saved_reg[regno]] = -(fi->framesize);
		saved_reg[regno] = regno;	/* reset saved register map */
	      }
	}
      else if ((insn & 0xff00) == 0xb000)	/* add sp, #simm */
	{
	  offset = (insn & 0x7f) << 2;		/* get scaled offset */
	  if (insn & 0x80)			/* is it signed? */
	     offset = -offset;
	  fi->framesize -= offset;
	}
      else if ((insn & 0xff00) == 0xaf00)	/* add r7, sp, #imm */
	{
	  fi->framereg = THUMB_FP_REGNUM;
	  fi->frameoffset = (insn & 0xff) << 2;	/* get scaled offset */
	}
      else if (insn == 0x466f)			/* mov r7, sp */
	{
	  fi->framereg = THUMB_FP_REGNUM;
	  fi->frameoffset = 0;
	  saved_reg[THUMB_FP_REGNUM] = SP_REGNUM;
	}
      else if ((insn & 0xffc0) == 0x4640)	/* mov r0-r7, r8-r15 */
	{
	  int lo_reg = insn & 7;		/* dest. register (r0-r7) */
	  int hi_reg = ((insn >> 3) & 7) + 8;	/* source register (r8-15) */
	  saved_reg[lo_reg] = hi_reg;		/* remember hi reg was saved */
	}
      else
	break;				/* anything else isn't prologue */
    }
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
     struct frame_info * fi;
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
     struct frame_info * fi;
{
  int i;

  prologue_cache.pc          = fi->pc;
  prologue_cache.framereg    = fi->framereg;
  prologue_cache.framesize   = fi->framesize;
  prologue_cache.frameoffset = fi->frameoffset;
  
  for (i = 0; i <= NUM_REGS; i++)
    prologue_cache.fsr.regs[i] = fi->fsr.regs[i];
}


/* Function: arm_scan_prologue
   This function decodes an ARM function prologue to determine:
     1) the size of the stack frame
     2) which registers are saved on it
     3) the offsets of saved regs
     4) the offset from the stack pointer to the frame pointer
   This information is stored in the "extra" fields of the frame_info.

   A typical Arm function prologue might look like this:
	mov    ip, sp
	stmfd  sp!, {fp, ip, lr, pc}
	sub    fp, ip, #4
	sub    sp, sp, #16
   Which would create this stack frame (offsets relative to FP):
     IP ->   4	(caller's stack)
     FP ->   0	PC (points to address of stmfd instruction + 12 in callee)
	    -4	LR (return address in caller)
	    -8	IP (copy of caller's SP)
     	   -12	FP (caller's FP)
     SP -> -28	Local variables
   The frame size would thus be 32 bytes, and the frame offset would be
   28 bytes.  */

static void
arm_scan_prologue (fi)
     struct frame_info * fi;
{
  int regno, sp_offset, fp_offset;
  CORE_ADDR prologue_start, prologue_end, current_pc;

  /* Check if this function is already in the cache of frame information. */
  if (check_prologue_cache (fi))
    return;

  /* Assume there is no frame until proven otherwise.  */
  fi->framereg    = SP_REGNUM;
  fi->framesize   = 0;
  fi->frameoffset = 0;

  /* Check for Thumb prologue.  */
  if (arm_pc_is_thumb (fi->pc))
    {
      thumb_scan_prologue (fi);
      save_prologue_cache (fi);
      return;
    }

  /* Find the function prologue.  If we can't find the function in
     the symbol table, peek in the stack frame to find the PC.  */
  if (find_pc_partial_function (fi->pc, NULL, &prologue_start, &prologue_end))
    {
      /* Assume the prologue is everything between the first instruction
         in the function and the first source line.  */
      struct symtab_and_line sal = find_pc_line (prologue_start, 0);

      if (sal.line == 0)		/* no line info, use current PC */
	prologue_end = fi->pc;
      else if (sal.end < prologue_end)	/* next line begins after fn end */
	prologue_end = sal.end;		/* (probably means no prologue)  */
    }
  else
    {
      /* Get address of the stmfd in the prologue of the callee; the saved
         PC is the address of the stmfd + 12.  */
      prologue_start = (read_memory_integer (fi->frame, 4) & 0x03fffffc) - 12;
      prologue_end = prologue_start + 40; /* FIXME: should be big enough */
    }

  /* Now search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.  */

  sp_offset = fp_offset = 0;
  for (current_pc = prologue_start; current_pc < prologue_end; current_pc += 4)
    {
      unsigned int insn = read_memory_unsigned_integer (current_pc, 4);

      if ((insn & 0xffff0000) == 0xe92d0000)	/* stmfd sp!, {..., r7, lr} */
	{
	  int mask = insn & 0xffff;

	  /* Calculate offsets of saved registers. */
	  for (regno = PC_REGNUM; regno >= 0; regno--)
	    if (mask & (1 << regno))
	      {
		sp_offset -= 4;
		fi->fsr.regs[regno] = sp_offset;
	      }
	}
      else if ((insn & 0xfffff000) == 0xe24cb000)	/* sub fp, ip #n */
	{
	  unsigned imm = insn & 0xff;			/* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;		/* rotate amount */
	  imm = (imm >> rot) | (imm << (32-rot));
	  fp_offset = -imm;
	  fi->framereg = FP_REGNUM;
	}
      else if ((insn & 0xfffff000) == 0xe24dd000)	/* sub sp, sp #n */
	{
	  unsigned imm = insn & 0xff;			/* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;		/* rotate amount */
	  imm = (imm >> rot) | (imm << (32-rot));
	  sp_offset -= imm;
	}
      else if ((insn & 0xffff7fff) == 0xed6d0103) /* stfe f?, [sp, -#c]! */
	{
	  sp_offset -= 12;
	  regno = F0_REGNUM + ((insn >> 12) & 0x07);
	  fi->fsr.regs[regno] = sp_offset;
	}
      else if (insn == 0xe1a0c00d)			/* mov ip, sp */
	continue;
      else
	break;	/* not a recognized prologue instruction */
    }

  /* The frame size is just the negative of the offset (from the original SP)
     of the last thing thing we pushed on the stack.  The frame offset is
     [new FP] - [new SP].  */
  fi->framesize = -sp_offset;
  fi->frameoffset = fp_offset - sp_offset;
  
  save_prologue_cache (fi);
}


/* Function: find_callers_reg
   Find REGNUM on the stack.  Otherwise, it's in an active register.  One thing
   we might want to do here is to check REGNUM against the clobber mask, and
   somehow flag it as invalid if it isn't saved on the stack somewhere.  This
   would provide a graceful failure mode when trying to get the value of
   caller-saves registers for an inner frame.  */

static CORE_ADDR
arm_find_callers_reg (fi, regnum)
     struct frame_info * fi;
     int regnum;
{
  for (; fi; fi = fi->next)
    
#if 0	/* FIXME: enable this code if we convert to new call dummy scheme.  */
    if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
      return generic_read_register_dummy (fi->pc, fi->frame, regnum);
    else
#endif
      if (fi->fsr.regs[regnum] != 0)
	return read_memory_integer (fi->fsr.regs[regnum], 
				  REGISTER_RAW_SIZE(regnum));
  return read_register (regnum);
}


/* Function: frame_chain
   Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.
   For ARM, we save the frame size when we initialize the frame_info.

   The original definition of this function was a macro in tm-arm.h:
      { In the case of the ARM, the frame's nominal address is the FP value,
	 and 12 bytes before comes the saved previous FP value as a 4-byte word.  }

      #define FRAME_CHAIN(thisframe)  \
	((thisframe)->pc >= LOWEST_PC ?    \
	 read_memory_integer ((thisframe)->frame - 12, 4) :\
	 0)
*/

CORE_ADDR
arm_frame_chain (fi)
     struct frame_info * fi;
{
#if 0	/* FIXME: enable this code if we convert to new call dummy scheme.  */
  CORE_ADDR fn_start, callers_pc, fp;

  /* is this a dummy frame? */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    return fi->frame;	/* dummy frame same as caller's frame */

  /* is caller-of-this a dummy frame? */
  callers_pc = FRAME_SAVED_PC(fi);  /* find out who called us: */
  fp = arm_find_callers_reg (fi, FP_REGNUM);
  if (PC_IN_CALL_DUMMY (callers_pc, fp, fp))	
    return fp;		/* dummy frame's frame may bear no relation to ours */

  if (find_pc_partial_function (fi->pc, 0, &fn_start, 0))
    if (fn_start == entry_point_address ())
      return 0;		/* in _start fn, don't chain further */
#endif
  CORE_ADDR caller_pc, fn_start;
  struct frame_info caller_fi;
  int framereg = fi->framereg;

  if (fi->pc < LOWEST_PC)
    return 0;

  /* If the caller is the startup code, we're at the end of the chain.  */
  caller_pc = FRAME_SAVED_PC (fi);
  if (find_pc_partial_function (caller_pc, 0, &fn_start, 0))
    if (fn_start == entry_point_address ())
      return 0;

  /* If the caller is Thumb and the caller is ARM, or vice versa,
     the frame register of the caller is different from ours.
     So we must scan the prologue of the caller to determine its
     frame register number. */
  if (arm_pc_is_thumb (caller_pc) != arm_pc_is_thumb (fi->pc))
    {
      memset (& caller_fi, 0, sizeof (caller_fi));
      caller_fi.pc = caller_pc;
      arm_scan_prologue (& caller_fi);
      framereg = caller_fi.framereg;
    }

  /* If the caller used a frame register, return its value.
     Otherwise, return the caller's stack pointer.  */
  if (framereg == FP_REGNUM || framereg == THUMB_FP_REGNUM)
    return arm_find_callers_reg (fi, framereg);
  else
    return fi->frame + fi->framesize;
}

/* Function: init_extra_frame_info
   This function actually figures out the frame address for a given pc and
   sp.  This is tricky  because we sometimes don't use an explicit
   frame pointer, and the previous stack pointer isn't necessarily recorded
   on the stack.  The only reliable way to get this info is to
   examine the prologue.  */

void
arm_init_extra_frame_info (fi)
     struct frame_info * fi;
{
  int reg;

  if (fi->next)
    fi->pc = FRAME_SAVED_PC (fi->next);

  memset (fi->fsr.regs, '\000', sizeof fi->fsr.regs);

#if 0	/* FIXME: enable this code if we convert to new call dummy scheme.  */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    {
      /* We need to setup fi->frame here because run_stack_dummy gets it wrong
	 by assuming it's always FP.  */
      fi->frame       = generic_read_register_dummy (fi->pc, fi->frame, SP_REGNUM);
      fi->framesize   = 0;
      fi->frameoffset = 0;
      return;
    }
  else 
#endif
    {
      arm_scan_prologue (fi);

      if (!fi->next)			/* this is the innermost frame? */
	fi->frame = read_register (fi->framereg);
      else			 	/* not the innermost frame */
	/* If we have an FP,  the callee saved it. */
	if (fi->framereg == FP_REGNUM || fi->framereg == THUMB_FP_REGNUM)
	  if (fi->next->fsr.regs[fi->framereg] != 0)
	    fi->frame = read_memory_integer (fi->next->fsr.regs[fi->framereg],
					     4);

      /* Calculate actual addresses of saved registers using offsets determined
         by arm_scan_prologue.  */
      for (reg = 0; reg < NUM_REGS; reg++)
	if (fi->fsr.regs[reg] != 0)
	  fi->fsr.regs[reg] += fi->frame + fi->framesize - fi->frameoffset;
    }
}


/* Function: frame_saved_pc
   Find the caller of this frame.  We do this by seeing if LR_REGNUM is saved
   in the stack anywhere, otherwise we get it from the registers.

   The old definition of this function was a macro:
     #define FRAME_SAVED_PC(FRAME) \
	ADDR_BITS_REMOVE (read_memory_integer ((FRAME)->frame - 4, 4))
*/

CORE_ADDR
arm_frame_saved_pc (fi)
     struct frame_info * fi;
{
#if 0	/* FIXME: enable this code if we convert to new call dummy scheme.  */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    return generic_read_register_dummy (fi->pc, fi->frame, PC_REGNUM);
  else
#endif
    {
      CORE_ADDR pc = arm_find_callers_reg (fi, LR_REGNUM);
      return IS_THUMB_ADDR (pc) ? UNMAKE_THUMB_ADDR (pc) : pc;
    }
}


/* Return the frame address.  On ARM, it is R11; on Thumb it is R7.
   Examine the Program Status Register to decide which state we're in.  */

CORE_ADDR
arm_target_read_fp ()
{
  if (read_register (PS_REGNUM) & 0x20)		/* Bit 5 is Thumb state bit */
    return read_register (THUMB_FP_REGNUM);	/* R7 if Thumb */
  else
    return read_register (FP_REGNUM);		/* R11 if ARM */
}


/* Calculate the frame offsets of the saved registers (ARM version). */
void
arm_frame_find_saved_regs (fi, regaddr)
     struct frame_info *fi;
     struct frame_saved_regs *regaddr;
{
  memcpy (regaddr, &fi->fsr, sizeof (struct frame_saved_regs));
}


void
arm_push_dummy_frame ()
{
  CORE_ADDR old_sp = read_register (SP_REGNUM);
  CORE_ADDR sp = old_sp;
  CORE_ADDR fp, prologue_start;
  int regnum;

  /* Push the two dummy prologue instructions in reverse order,
     so that they'll be in the correct low-to-high order in memory.  */
  /* sub     fp, ip, #4 */
  sp = push_word (sp, 0xe24cb004);
  /*  stmdb   sp!, {r0-r10, fp, ip, lr, pc} */
  prologue_start = sp = push_word (sp, 0xe92ddfff);

  /* push a pointer to the dummy prologue + 12, because when
     stm instruction stores the PC, it stores the address of the stm
     instruction itself plus 12.  */
  fp = sp = push_word (sp, prologue_start + 12);
  sp = push_word (sp, read_register (PC_REGNUM)); /* FIXME: was PS_REGNUM */
  sp = push_word (sp, old_sp);
  sp = push_word (sp, read_register (FP_REGNUM));
  
  for (regnum = 10; regnum >= 0; regnum --)
    sp = push_word (sp, read_register (regnum));
  
  write_register (FP_REGNUM, fp);
  write_register (THUMB_FP_REGNUM, fp);
  write_register (SP_REGNUM, sp);
}

/* Fix up the call dummy, based on whether the processor is currently
   in Thumb or ARM mode, and whether the target function is Thumb
   or ARM.  There are three different situations requiring three
   different dummies:

   * ARM calling ARM: uses the call dummy in tm-arm.h, which has already
     been copied into the dummy parameter to this function.
   * ARM calling Thumb: uses the call dummy in tm-arm.h, but with the
     "mov pc,r4" instruction patched to be a "bx r4" instead.
   * Thumb calling anything: uses the Thumb dummy defined below, which
     works for calling both ARM and Thumb functions.

   All three call dummies expect to receive the target function address
   in R4, with the low bit set if it's a Thumb function.
*/

void
arm_fix_call_dummy (dummy, pc, fun, nargs, args, type, gcc_p)
     char *        dummy;
     CORE_ADDR     pc;
     CORE_ADDR     fun;
     int           nargs;
     value_ptr *   args;
     struct type * type;
     int           gcc_p;
{
  static short thumb_dummy[4] =
  {
    0xf000, 0xf801,	/*	  bl      label */
    0xdf18,		/*	  swi     24 */
    0x4720,		/* label: bx	  r4 */
  };
  static unsigned long arm_bx_r4 = 0xe12fff14;	/* bx r4 instruction */

  /* Set flag indicating whether the current PC is in a Thumb function. */
  caller_is_thumb = arm_pc_is_thumb (read_pc());

  /* If the target function is Thumb, set the low bit of the function address.
     And if the CPU is currently in ARM mode, patch the second instruction
     of call dummy to use a BX instruction to switch to Thumb mode.  */
  target_is_thumb = arm_pc_is_thumb (fun);
  if (target_is_thumb)
    {
      fun |= 1;
      if (!caller_is_thumb)
	store_unsigned_integer (dummy + 4, sizeof (arm_bx_r4), arm_bx_r4);
    }

  /* If the CPU is currently in Thumb mode, use the Thumb call dummy
     instead of the ARM one that's already been copied.  This will
     work for both Thumb and ARM target functions.  */
  if (caller_is_thumb)
    {
      int i;
      char *p = dummy;
      int len = sizeof (thumb_dummy) / sizeof (thumb_dummy[0]);

      for (i = 0; i < len; i++)
	{
	  store_unsigned_integer (p, sizeof (thumb_dummy[0]), thumb_dummy[i]);
	  p += sizeof (thumb_dummy[0]);
	}
    }

  /* Put the target address in r4; the call dummy will copy this to the PC. */
  write_register (4, fun);
}


/* Return the offset in the call dummy of the instruction that needs
   to have a breakpoint placed on it.  This is the offset of the 'swi 24'
   instruction, which is no longer actually used, but simply acts
   as a place-holder now.

   This implements the CALL_DUMMY_BREAK_OFFSET macro.
*/

int
arm_call_dummy_breakpoint_offset ()
{
  if (caller_is_thumb)
    return 4;
  else
    return 8;
}


CORE_ADDR
arm_push_arguments(nargs, args, sp, struct_return, struct_addr)
     int         nargs;
     value_ptr * args;
     CORE_ADDR   sp;
     int         struct_return;
     CORE_ADDR   struct_addr;
{
  int argreg;
  int float_argreg;
  int argnum;
  int stack_offset;
  struct stack_arg {
      char *val;
      int len;
      int offset;
    };
  struct stack_arg *stack_args =
      (struct stack_arg*)alloca (nargs * sizeof (struct stack_arg));
  int nstack_args = 0;


  /* Initialize the integer and float register pointers.  */
  argreg = A1_REGNUM;
  float_argreg = F0_REGNUM;

  /* the struct_return pointer occupies the first parameter-passing reg */
  if (struct_return)
      write_register (argreg++, struct_addr);

  /* The offset onto the stack at which we will start copying parameters
     (after the registers are used up) begins at 16 in the old ABI.
     This leaves room for the "home" area for register parameters.  */
  stack_offset = REGISTER_SIZE * 4;

  /* Process args from left to right.  Store as many as allowed in
	registers, save the rest to be pushed on the stack */
  for(argnum = 0; argnum < nargs; argnum++)
    {
      char *         val;
      value_ptr      arg = args[argnum];
      struct type *  arg_type = check_typedef (VALUE_TYPE (arg));
      struct type *  target_type = TYPE_TARGET_TYPE (arg_type);
      int            len = TYPE_LENGTH (arg_type);
      enum type_code typecode = TYPE_CODE (arg_type);
      CORE_ADDR      regval;
      int newarg;

      val = (char *) VALUE_CONTENTS (arg);

      /* If the argument is a pointer to a function, and it's a Thumb
         function, set the low bit of the pointer.  */
      if (typecode == TYPE_CODE_PTR
	  && target_type != NULL
	  && TYPE_CODE (target_type) == TYPE_CODE_FUNC)
	{
	  regval = extract_address (val, len);
	  if (arm_pc_is_thumb (regval))
	    store_address (val, len, MAKE_THUMB_ADDR (regval));
	}

#define MAPCS_FLOAT 0	/* --mapcs-float not implemented by the compiler yet */
#if MAPCS_FLOAT
      /* Up to four floating point arguments can be passed in floating
         point registers on ARM (not on Thumb).  */
      if (typecode == TYPE_CODE_FLT
	  && float_argreg <= ARM_LAST_FP_ARG_REGNUM
	  && !target_is_thumb)
	{
	  /* This is a floating point value that fits entirely
	     in a single register.  */
	  regval = extract_address (val, len);
	  write_register (float_argreg++, regval);
	}
      else
#endif
	{
	  /* Copy the argument to general registers or the stack in
	     register-sized pieces.  Large arguments are split between
	     registers and stack.  */
	  while (len > 0)
	    {
	      if (argreg <= ARM_LAST_ARG_REGNUM)
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
    while(nstack_args--)
      {
	sp -= stack_args[nstack_args].len;
	write_memory(sp, stack_args[nstack_args].val,
		stack_args[nstack_args].len);
      }

  /* Return adjusted stack pointer.  */
  return sp;
}

void
arm_pop_frame ()
{
  struct frame_info *frame = get_current_frame();
  int regnum;

  for (regnum = 0; regnum < NUM_REGS; regnum++)
    if (frame->fsr.regs[regnum] != 0)
      write_register (regnum, 
		      read_memory_integer (frame->fsr.regs[regnum], 4));

  write_register (PC_REGNUM, FRAME_SAVED_PC (frame));
  write_register (SP_REGNUM, read_register (frame->framereg));

  flush_cached_frames ();
}

static void
print_fpu_flags (flags)
     int flags;
{
    if (flags & (1 << 0)) fputs ("IVO ", stdout);
    if (flags & (1 << 1)) fputs ("DVZ ", stdout);
    if (flags & (1 << 2)) fputs ("OFL ", stdout);
    if (flags & (1 << 3)) fputs ("UFL ", stdout);
    if (flags & (1 << 4)) fputs ("INX ", stdout);
    putchar ('\n');
}

void
arm_float_info ()
{
    register unsigned long status = read_register (FPS_REGNUM);
    int type;

    type = (status >> 24) & 127;
    printf ("%s FPU type %d\n",
	    (status & (1<<31)) ? "Hardware" : "Software",
	    type);
    fputs ("mask: ", stdout);
    print_fpu_flags (status >> 16);
    fputs ("flags: ", stdout);
    print_fpu_flags (status);
}

static char *original_register_names[] =
{ "a1", "a2", "a3", "a4", /*  0  1  2  3 */
  "v1", "v2", "v3", "v4", /*  4  5  6  7 */
  "v5", "v6", "sl", "fp", /*  8  9 10 11 */
  "ip", "sp", "lr", "pc", /* 12 13 14 15 */
  "f0", "f1", "f2", "f3", /* 16 17 18 19 */
  "f4", "f5", "f6", "f7", /* 20 21 22 23 */
  "fps","ps" }            /* 24 25       */;

/* These names are the ones which gcc emits, and 
   I find them less confusing.  Toggle between them
   using the `othernames' command. */
static char *additional_register_names[] =
{ "r0", "r1", "r2", "r3", /*  0  1  2  3 */
  "r4", "r5", "r6", "r7", /*  4  5  6  7 */
  "r8", "r9", "sl", "fp", /*  8  9 10 11 */
  "ip", "sp", "lr", "pc", /* 12 13 14 15 */
  "f0", "f1", "f2", "f3", /* 16 17 18 19 */
  "f4", "f5", "f6", "f7", /* 20 21 22 23 */
  "fps","ps" }            /* 24 25       */;

char **arm_register_names = original_register_names;


static void
arm_othernames ()
{
  static int toggle;
  arm_register_names = (toggle
			? additional_register_names
			: original_register_names);
  toggle = !toggle;
}

/* FIXME:  Fill in with the 'right thing', see asm 
   template in arm-convert.s */

void 
convert_from_extended (ptr, dbl)
     void * ptr;
     double * dbl;
{
  *dbl = *(double*)ptr;
}

void 
convert_to_extended (dbl, ptr)
     void * ptr;
     double * dbl;
{
  *(double*)ptr = *dbl;
}

static int
condition_true (cond, status_reg)
     unsigned long cond;
     unsigned long status_reg;
{
  if (cond == INST_AL || cond == INST_NV)
    return 1;

  switch (cond)
    {
    case INST_EQ:
      return ((status_reg & FLAG_Z) != 0);
    case INST_NE:
      return ((status_reg & FLAG_Z) == 0);
    case INST_CS:
      return ((status_reg & FLAG_C) != 0);
    case INST_CC:
      return ((status_reg & FLAG_C) == 0);
    case INST_MI:
      return ((status_reg & FLAG_N) != 0);
    case INST_PL:
      return ((status_reg & FLAG_N) == 0);
    case INST_VS:
      return ((status_reg & FLAG_V) != 0);
    case INST_VC:
      return ((status_reg & FLAG_V) == 0);
    case INST_HI:
      return ((status_reg & (FLAG_C | FLAG_Z)) == FLAG_C);
    case INST_LS:
      return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
    case INST_GE:
      return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_LT:
      return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
    case INST_GT:
      return (((status_reg & FLAG_Z) == 0) &&
	      (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    case INST_LE:
      return (((status_reg & FLAG_Z) != 0) ||
	      (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
    }
  return 1;
}

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1

static unsigned long
shifted_reg_val (inst, carry, pc_val, status_reg)
     unsigned long inst;
     int carry;
     unsigned long pc_val;
     unsigned long status_reg;
{
  unsigned long res, shift;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);
 
  if (bit(inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);
 
  res = (rm == 15 
	 ? ((pc_val | (ARM_PC_32 ? 0 : status_reg))
	    + (bit (inst, 4) ? 12 : 8)) 
	 : read_register (rm));

  switch (shifttype)
    {
    case 0: /* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;
      
    case 1: /* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2: /* ASR */
      if (shift >= 32) shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3: /* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32-shift));
      break;
    }

  return res & 0xffffffff;
}


/* Return number of 1-bits in VAL.  */

static int
bitcount (val)
     unsigned long val;
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;	/* delete rightmost 1-bit in val */
  return nbits;
}


static CORE_ADDR
thumb_get_next_pc (pc)
     CORE_ADDR pc;
{
  unsigned long pc_val = ((unsigned long)pc) + 4;	/* PC after prefetch */
  unsigned short inst1 = read_memory_integer (pc, 2);
  CORE_ADDR nextpc = pc + 2;		/* default is next instruction */
  unsigned long offset;

  if ((inst1 & 0xff00) == 0xbd00)	/* pop {rlist, pc} */
    {
      CORE_ADDR sp;

      /* Fetch the saved PC from the stack.  It's stored above
         all of the other registers.  */
      offset = bitcount (bits (inst1, 0, 7)) * REGISTER_SIZE;
      sp = read_register (SP_REGNUM);
      nextpc = (CORE_ADDR) read_memory_integer (sp + offset, 4);
      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	error ("Infinite loop detected");
    }
  else if ((inst1 & 0xf000) == 0xd000)	/* conditional branch */
    {
      unsigned long status = read_register (PS_REGNUM);
      unsigned long cond = bits (inst1, 8, 11); 
      if (cond != 0x0f && condition_true (cond, status))	/* 0x0f = SWI */
	nextpc = pc_val + (sbits (inst1, 0, 7) << 1);
    }
  else if ((inst1 & 0xf800) == 0xe000)	/* unconditional branch */
    {
      nextpc = pc_val + (sbits (inst1, 0, 10) << 1);
    }
  else if ((inst1 & 0xf800) == 0xf000)	/* long branch with link */
    {
      unsigned short inst2 = read_memory_integer (pc + 2, 2);
      offset = (sbits (inst1, 0, 10) << 12) + (bits  (inst2, 0, 10) << 1);
      nextpc = pc_val + offset;
    }

  return nextpc;
}


CORE_ADDR
arm_get_next_pc (pc)
     CORE_ADDR pc;
{
  unsigned long pc_val;
  unsigned long this_instr;
  unsigned long status;
  CORE_ADDR nextpc;

  if (arm_pc_is_thumb (pc))
    return thumb_get_next_pc (pc);

  pc_val = (unsigned long) pc;
  this_instr = read_memory_integer (pc, 4);
  status = read_register (PS_REGNUM);
  nextpc = (CORE_ADDR) (pc_val + 4);  /* Default case */

  if (condition_true (bits (this_instr, 28, 31), status))
    {
      switch (bits (this_instr, 24, 27))
	{
	case 0x0: case 0x1: /* data processing */
	case 0x2: case 0x3:
	  {
	    unsigned long operand1, operand2, result = 0;
	    unsigned long rn;
	    int c;
 
	    if (bits (this_instr, 12, 15) != 15)
	      break;

	    if (bits (this_instr, 22, 25) == 0
		&& bits (this_instr, 4, 7) == 9)  /* multiply */
	      error ("Illegal update to pc in instruction");

	    /* Multiply into PC */
	    c = (status & FLAG_C) ? 1 : 0;
	    rn = bits (this_instr, 16, 19);
	    operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);
 
	    if (bit (this_instr, 25))
	      {
		unsigned long immval = bits (this_instr, 0, 7);
		unsigned long rotate = 2 * bits (this_instr, 8, 11);
		operand2 = ((immval >> rotate) | (immval << (32-rotate)))
			   & 0xffffffff;
	      }
	    else  /* operand 2 is a shifted register */
	      operand2 = shifted_reg_val (this_instr, c, pc_val, status);
 
	    switch (bits (this_instr, 21, 24))
	      {
	      case 0x0: /*and*/
		result = operand1 & operand2;
		break;

	      case 0x1: /*eor*/
		result = operand1 ^ operand2;
		break;

	      case 0x2: /*sub*/
		result = operand1 - operand2;
		break;

	      case 0x3: /*rsb*/
		result = operand2 - operand1;
		break;

	      case 0x4:  /*add*/
		result = operand1 + operand2;
		break;

	      case 0x5: /*adc*/
		result = operand1 + operand2 + c;
		break;

	      case 0x6: /*sbc*/
		result = operand1 - operand2 + c;
		break;

	      case 0x7: /*rsc*/
		result = operand2 - operand1 + c;
		break;

	      case 0x8: case 0x9: case 0xa: case 0xb: /* tst, teq, cmp, cmn */
		result = (unsigned long) nextpc;
		break;

	      case 0xc: /*orr*/
		result = operand1 | operand2;
		break;

	      case 0xd: /*mov*/
		/* Always step into a function.  */
		result = operand2;
                break;

	      case 0xe: /*bic*/
		result = operand1 & ~operand2;
		break;

	      case 0xf: /*mvn*/
		result = ~operand2;
		break;
	      }
	    nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }
 
	case 0x4: case 0x5: /* data transfer */
	case 0x6: case 0x7:
	  if (bit (this_instr, 20))
	    {
	      /* load */
	      if (bits (this_instr, 12, 15) == 15)
		{
		  /* rd == pc */
		  unsigned long  rn;
		  unsigned long base;
 
		  if (bit (this_instr, 22))
		    error ("Illegal update to pc in instruction");

		  /* byte write to PC */
		  rn = bits (this_instr, 16, 19);
		  base = (rn == 15) ? pc_val + 8 : read_register (rn);
		  if (bit (this_instr, 24))
		    {
		      /* pre-indexed */
		      int c = (status & FLAG_C) ? 1 : 0;
		      unsigned long offset =
			(bit (this_instr, 25)
			 ? shifted_reg_val (this_instr, c, pc_val)
			 : bits (this_instr, 0, 11));

		      if (bit (this_instr, 23))
			base += offset;
		      else
			base -= offset;
		    }
		  nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) base, 
							    4);
 
		  nextpc = ADDR_BITS_REMOVE (nextpc);

		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;
 
	case 0x8: case 0x9: /* block transfer */
	  if (bit (this_instr, 20))
	    {
	      /* LDM */
	      if (bit (this_instr, 15))
		{
		  /* loading pc */
		  int offset = 0;

		  if (bit (this_instr, 23))
		    {
		      /* up */
		      unsigned long reglist = bits (this_instr, 0, 14);
		      offset = bitcount (reglist) * 4;
		      if (bit (this_instr, 24)) /* pre */
			offset += 4;
		    }
		  else if (bit (this_instr, 24))
		    offset = -4;
 
		  {
		    unsigned long rn_val = 
		      read_register (bits (this_instr, 16, 19));
		    nextpc =
		      (CORE_ADDR) read_memory_integer ((CORE_ADDR) (rn_val
								    + offset),
						       4);
		  }
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;
 
	case 0xb:           /* branch & link */
	case 0xa:           /* branch */
	  {
	    nextpc = BranchDest (pc, this_instr);

	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }
 
	case 0xc: case 0xd:
	case 0xe:           /* coproc ops */
	case 0xf:           /* SWI */
	  break;

	default:
	  fprintf (stderr, "Bad bit-field extraction\n");
	  return (pc);
	}
    }

  return nextpc;
}

#include "bfd-in2.h"
#include "libcoff.h"

static int
gdb_print_insn_arm (memaddr, info)
     bfd_vma memaddr;
     disassemble_info * info;
{
  if (arm_pc_is_thumb (memaddr))
    {
      static asymbol *                  asym;
      static combined_entry_type        ce;
      static struct coff_symbol_struct  csym;
      static struct _bfd                fake_bfd;
      static bfd_target                 fake_target;

      if (csym.native == NULL)
	{
	  /* Create a fake symbol vector containing a Thumb symbol.  This is
	     solely so that the code in print_insn_little_arm() and
	     print_insn_big_arm() in opcodes/arm-dis.c will detect the presence
	     of a Thumb symbol and switch to decoding Thumb instructions.  */
	     
	  fake_target.flavour  = bfd_target_coff_flavour;
	  fake_bfd.xvec        = & fake_target;
	  ce.u.syment.n_sclass = C_THUMBEXTFUNC;
	  csym.native          = & ce;
	  csym.symbol.the_bfd  = & fake_bfd;
	  csym.symbol.name     = "fake";
	  asym                 = (asymbol *) & csym;
	}
      
      memaddr = UNMAKE_THUMB_ADDR (memaddr);
      info->symbols = & asym;
    }
  else
    info->symbols = NULL;
  
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    return print_insn_big_arm (memaddr, info);
  else
    return print_insn_little_arm (memaddr, info);
}

/* Sequence of bytes for breakpoint instruction.  */
#define ARM_LE_BREAKPOINT {0xFE,0xDE,0xFF,0xE7} /* Recognized illegal opcodes */
#define ARM_BE_BREAKPOINT {0xE7,0xFF,0xDE,0xFE} 
#define THUMB_LE_BREAKPOINT {0xfe,0xdf}       
#define THUMB_BE_BREAKPOINT {0xdf,0xfe}       

/* The following has been superseded by BREAKPOINT_FOR_PC, but
   is defined merely to keep mem-break.c happy.  */
#define LITTLE_BREAKPOINT ARM_LE_BREAKPOINT
#define BIG_BREAKPOINT    ARM_BE_BREAKPOINT

/* This function implements the BREAKPOINT_FROM_PC macro.  It uses the program
   counter value to determine whether a 16- or 32-bit breakpoint should be
   used.  It returns a pointer to a string of bytes that encode a breakpoint
   instruction, stores the length of the string to *lenptr, and adjusts pc
   (if necessary) to point to the actual memory location where the
   breakpoint should be inserted.  */

unsigned char *
arm_breakpoint_from_pc (pcptr, lenptr)
     CORE_ADDR * pcptr;
     int * lenptr;
{
  if (arm_pc_is_thumb (*pcptr) || arm_pc_is_thumb_dummy (*pcptr))
    {
      if (TARGET_BYTE_ORDER == BIG_ENDIAN)
        {
          static char thumb_breakpoint[] = THUMB_BE_BREAKPOINT;
          *pcptr = UNMAKE_THUMB_ADDR (*pcptr);
          *lenptr = sizeof (thumb_breakpoint);
          return thumb_breakpoint;
         }
      else
        {
          static char thumb_breakpoint[] = THUMB_LE_BREAKPOINT;
          *pcptr = UNMAKE_THUMB_ADDR (*pcptr);
          *lenptr = sizeof (thumb_breakpoint);
          return thumb_breakpoint;
        }
    }
  else
    {
      if (TARGET_BYTE_ORDER == BIG_ENDIAN)
        {
          static char arm_breakpoint[] = ARM_BE_BREAKPOINT;
          *lenptr = sizeof (arm_breakpoint);
          return arm_breakpoint;
        }
      else
        {
          static char arm_breakpoint[] = ARM_LE_BREAKPOINT;
          *lenptr = sizeof (arm_breakpoint);
          return arm_breakpoint;
        }
    }
}
/* Return non-zero if the PC is inside a call thunk (aka stub or trampoline).
   This implements the IN_SOLIB_CALL_TRAMPOLINE macro.  */

int
arm_in_call_stub (pc, name)
     CORE_ADDR pc;
     char * name;
{
  CORE_ADDR start_addr;

  /* Find the starting address of the function containing the PC.  If the
     caller didn't give us a name, look it up at the same time.  */
  if (find_pc_partial_function (pc, name ? NULL : &name, &start_addr, NULL) == 0)
    return 0;

  return strncmp (name, "_call_via_r", 11) == 0;
}


/* If PC is in a Thumb call or return stub, return the address of the target
   PC, which is in a register.  The thunk functions are called _called_via_xx,
   where x is the register name.  The possible names are r0-r9, sl, fp, ip,
   sp, and lr. */

CORE_ADDR
arm_skip_stub (pc)
     CORE_ADDR pc;
{
  char * name;
  CORE_ADDR start_addr;

  /* Find the starting address and name of the function containing the PC.  */
  if (find_pc_partial_function (pc, &name, &start_addr, NULL) == 0)
    return 0;

  /* Call thunks always start with "_call_via_".  */
  if (strncmp (name, "_call_via_", 10) == 0)
    {
      /* Use the name suffix to determine which register contains
         the target PC.  */
      static char *table[15] = 
	{ "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	  "r8", "r9", "sl", "fp", "ip", "sp", "lr"
	};
      int regno;

      for (regno = 0; regno <= 14; regno++)
	if (strcmp (&name[10], table[regno]) == 0)
	  return read_register (regno);
    }
  return 0;	/* not a stub */
}


void
_initialize_arm_tdep ()
{
  tm_print_insn = gdb_print_insn_arm;

  add_com ("othernames", class_obscure, arm_othernames,
	   "Switch to the other set of register names.");

  /* ??? Maybe this should be a boolean.  */
  add_show_from_set (add_set_cmd ("apcs32", no_class,
				  var_zinteger, (char *)&arm_apcs_32,
				  "Set usage of ARM 32-bit mode.\n", &setlist),
		     & showlist);

}

/* Test whether the coff symbol specific value corresponds to a Thumb function */
int
coff_sym_is_thumb(int val)
{
	return (val == C_THUMBEXT ||
      val == C_THUMBSTAT ||
      val == C_THUMBEXTFUNC ||
      val == C_THUMBSTATFUNC ||
      val == C_THUMBLABEL);
}
