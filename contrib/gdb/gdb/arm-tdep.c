/* Common target dependent code for GDB on ARM systems.
   Copyright 1988, 1989, 1991, 1992, 1993, 1995, 1996, 1998, 1999, 2000,
   2001, 2002 Free Software Foundation, Inc.

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

#include <ctype.h>		/* XXX for isupper () */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "symfile.h"
#include "gdb_string.h"
#include "dis-asm.h"		/* For register flavors. */
#include "regcache.h"
#include "doublest.h"
#include "value.h"
#include "arch-utils.h"
#include "solib-svr4.h"

#include "arm-tdep.h"

#include "elf-bfd.h"
#include "coff/internal.h"
#include "elf/arm.h"

/* Each OS has a different mechanism for accessing the various
   registers stored in the sigcontext structure.

   SIGCONTEXT_REGISTER_ADDRESS should be defined to the name (or
   function pointer) which may be used to determine the addresses
   of the various saved registers in the sigcontext structure.

   For the ARM target, there are three parameters to this function. 
   The first is the pc value of the frame under consideration, the
   second the stack pointer of this frame, and the last is the
   register number to fetch.  

   If the tm.h file does not define this macro, then it's assumed that
   no mechanism is needed and we define SIGCONTEXT_REGISTER_ADDRESS to
   be 0. 
   
   When it comes time to multi-arching this code, see the identically
   named machinery in ia64-tdep.c for an example of how it could be
   done.  It should not be necessary to modify the code below where
   this macro is used.  */

#ifdef SIGCONTEXT_REGISTER_ADDRESS
#ifndef SIGCONTEXT_REGISTER_ADDRESS_P
#define SIGCONTEXT_REGISTER_ADDRESS_P() 1
#endif
#else
#define SIGCONTEXT_REGISTER_ADDRESS(SP,PC,REG) 0
#define SIGCONTEXT_REGISTER_ADDRESS_P() 0
#endif

/* Macros for setting and testing a bit in a minimal symbol that marks
   it as Thumb function.  The MSB of the minimal symbol's "info" field
   is used for this purpose. This field is already being used to store
   the symbol size, so the assumption is that the symbol size cannot
   exceed 2^31.

   MSYMBOL_SET_SPECIAL	Actually sets the "special" bit.
   MSYMBOL_IS_SPECIAL   Tests the "special" bit in a minimal symbol.
   MSYMBOL_SIZE         Returns the size of the minimal symbol,
   			i.e. the "info" field with the "special" bit
   			masked out.  */

#define MSYMBOL_SET_SPECIAL(msym)					\
	MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym))	\
					| 0x80000000)

#define MSYMBOL_IS_SPECIAL(msym)				\
	(((long) MSYMBOL_INFO (msym) & 0x80000000) != 0)

#define MSYMBOL_SIZE(msym)				\
	((long) MSYMBOL_INFO (msym) & 0x7fffffff)

/* This table matches the indicees assigned to enum arm_abi.  Keep
   them in sync.  */

static const char * const arm_abi_names[] =
{
  "<unknown>",
  "ARM EABI (version 1)",
  "ARM EABI (version 2)",
  "GNU/Linux",
  "NetBSD (a.out)",
  "NetBSD (ELF)",
  "APCS",
  "FreeBSD",
  "Windows CE",
  NULL
};

/* Number of different reg name sets (options). */
static int num_flavor_options;

/* We have more registers than the disassembler as gdb can print the value
   of special registers as well.
   The general register names are overwritten by whatever is being used by
   the disassembler at the moment. We also adjust the case of cpsr and fps. */

/* Initial value: Register names used in ARM's ISA documentation. */
static char * arm_register_name_strings[] =
{"r0",  "r1",  "r2",  "r3",	/*  0  1  2  3 */
 "r4",  "r5",  "r6",  "r7",	/*  4  5  6  7 */
 "r8",  "r9",  "r10", "r11",	/*  8  9 10 11 */
 "r12", "sp",  "lr",  "pc",	/* 12 13 14 15 */
 "f0",  "f1",  "f2",  "f3",	/* 16 17 18 19 */
 "f4",  "f5",  "f6",  "f7",	/* 20 21 22 23 */
 "fps", "cpsr" }; 		/* 24 25       */
static char **arm_register_names = arm_register_name_strings;

/* Valid register name flavors.  */
static const char **valid_flavors;

/* Disassembly flavor to use. Default to "std" register names. */
static const char *disassembly_flavor;
static int current_option;	/* Index to that option in the opcodes table. */

/* This is used to keep the bfd arch_info in sync with the disassembly
   flavor.  */
static void set_disassembly_flavor_sfunc(char *, int,
					 struct cmd_list_element *);
static void set_disassembly_flavor (void);

static void convert_from_extended (void *ptr, void *dbl);

/* Define other aspects of the stack frame.  We keep the offsets of
   all saved registers, 'cause we need 'em a lot!  We also keep the
   current size of the stack frame, and the offset of the frame
   pointer from the stack pointer (for frameless functions, and when
   we're still in the prologue of a function with a frame) */

struct frame_extra_info
{
  int framesize;
  int frameoffset;
  int framereg;
};

/* Addresses for calling Thumb functions have the bit 0 set.
   Here are some macros to test, set, or clear bit 0 of addresses.  */
#define IS_THUMB_ADDR(addr)	((addr) & 1)
#define MAKE_THUMB_ADDR(addr)	((addr) | 1)
#define UNMAKE_THUMB_ADDR(addr) ((addr) & ~1)

static int
arm_frame_chain_valid (CORE_ADDR chain, struct frame_info *thisframe)
{
  return (chain != 0 && (FRAME_SAVED_PC (thisframe) >= LOWEST_PC));
}

/* Set to true if the 32-bit mode is in use. */

int arm_apcs_32 = 1;

/* Flag set by arm_fix_call_dummy that tells whether the target
   function is a Thumb function.  This flag is checked by
   arm_push_arguments.  FIXME: Change the PUSH_ARGUMENTS macro (and
   its use in valops.c) to pass the function address as an additional
   parameter.  */

static int target_is_thumb;

/* Flag set by arm_fix_call_dummy that tells whether the calling
   function is a Thumb function.  This flag is checked by
   arm_pc_is_thumb and arm_call_dummy_breakpoint_offset.  */

static int caller_is_thumb;

/* Determine if the program counter specified in MEMADDR is in a Thumb
   function.  */

int
arm_pc_is_thumb (CORE_ADDR memaddr)
{
  struct minimal_symbol *sym;

  /* If bit 0 of the address is set, assume this is a Thumb address.  */
  if (IS_THUMB_ADDR (memaddr))
    return 1;

  /* Thumb functions have a "special" bit set in minimal symbols.  */
  sym = lookup_minimal_symbol_by_pc (memaddr);
  if (sym)
    {
      return (MSYMBOL_IS_SPECIAL (sym));
    }
  else
    {
      return 0;
    }
}

/* Determine if the program counter specified in MEMADDR is in a call
   dummy being called from a Thumb function.  */

int
arm_pc_is_thumb_dummy (CORE_ADDR memaddr)
{
  CORE_ADDR sp = read_sp ();

  /* FIXME: Until we switch for the new call dummy macros, this heuristic
     is the best we can do.  We are trying to determine if the pc is on
     the stack, which (hopefully) will only happen in a call dummy.
     We hope the current stack pointer is not so far alway from the dummy
     frame location (true if we have not pushed large data structures or
     gone too many levels deep) and that our 1024 is not enough to consider
     code regions as part of the stack (true for most practical purposes) */
  if (PC_IN_CALL_DUMMY (memaddr, sp, sp + 1024))
    return caller_is_thumb;
  else
    return 0;
}

/* Remove useless bits from addresses in a running program.  */
static CORE_ADDR
arm_addr_bits_remove (CORE_ADDR val)
{
  if (arm_pc_is_thumb (val))
    return (val & (arm_apcs_32 ? 0xfffffffe : 0x03fffffe));
  else
    return (val & (arm_apcs_32 ? 0xfffffffc : 0x03fffffc));
}

/* When reading symbols, we need to zap the low bit of the address,
   which may be set to 1 for Thumb functions.  */
static CORE_ADDR
arm_smash_text_address (CORE_ADDR val)
{
  return val & ~1;
}

/* Immediately after a function call, return the saved pc.  Can't
   always go through the frames for this because on some machines the
   new frame is not set up until the new function executes some
   instructions.  */

static CORE_ADDR
arm_saved_pc_after_call (struct frame_info *frame)
{
  return ADDR_BITS_REMOVE (read_register (ARM_LR_REGNUM));
}

/* Determine whether the function invocation represented by FI has a
   frame on the stack associated with it.  If it does return zero,
   otherwise return 1.  */

static int
arm_frameless_function_invocation (struct frame_info *fi)
{
  CORE_ADDR func_start, after_prologue;
  int frameless;

  /* Sometimes we have functions that do a little setup (like saving the
     vN registers with the stmdb instruction, but DO NOT set up a frame.
     The symbol table will report this as a prologue.  However, it is
     important not to try to parse these partial frames as frames, or we
     will get really confused.

     So I will demand 3 instructions between the start & end of the
     prologue before I call it a real prologue, i.e. at least
	mov ip, sp,
	stmdb sp!, {}
	sub sp, ip, #4.  */

  func_start = (get_pc_function_start ((fi)->pc) + FUNCTION_START_OFFSET);
  after_prologue = SKIP_PROLOGUE (func_start);

  /* There are some frameless functions whose first two instructions
     follow the standard APCS form, in which case after_prologue will
     be func_start + 8. */

  frameless = (after_prologue < func_start + 12);
  return frameless;
}

/* The address of the arguments in the frame.  */
static CORE_ADDR
arm_frame_args_address (struct frame_info *fi)
{
  return fi->frame;
}

/* The address of the local variables in the frame.  */
static CORE_ADDR
arm_frame_locals_address (struct frame_info *fi)
{
  return fi->frame;
}

/* The number of arguments being passed in the frame.  */
static int
arm_frame_num_args (struct frame_info *fi)
{
  /* We have no way of knowing.  */
  return -1;
}

/* A typical Thumb prologue looks like this:
   push    {r7, lr}
   add     sp, sp, #-28
   add     r7, sp, #12
   Sometimes the latter instruction may be replaced by:
   mov     r7, sp
   
   or like this:
   push    {r7, lr}
   mov     r7, sp
   sub	   sp, #12
   
   or, on tpcs, like this:
   sub     sp,#16
   push    {r7, lr}
   (many instructions)
   mov     r7, sp
   sub	   sp, #12

   There is always one instruction of three classes:
   1 - push
   2 - setting of r7
   3 - adjusting of sp
   
   When we have found at least one of each class we are done with the prolog.
   Note that the "sub sp, #NN" before the push does not count.
   */

static CORE_ADDR
thumb_skip_prologue (CORE_ADDR pc, CORE_ADDR func_end)
{
  CORE_ADDR current_pc;
  int findmask = 0;  	/* findmask:
      			   bit 0 - push { rlist }
			   bit 1 - mov r7, sp  OR  add r7, sp, #imm  (setting of r7)
      			   bit 2 - sub sp, #simm  OR  add sp, #simm  (adjusting of sp)
			*/

  for (current_pc = pc; current_pc + 2 < func_end && current_pc < pc + 40; current_pc += 2)
    {
      unsigned short insn = read_memory_unsigned_integer (current_pc, 2);

      if ((insn & 0xfe00) == 0xb400)	/* push { rlist } */
	{
	  findmask |= 1;  /* push found */
	}
      else if ((insn & 0xff00) == 0xb000)	/* add sp, #simm  OR  sub sp, #simm */
	{
	  if ((findmask & 1) == 0)  /* before push ? */
	    continue;
	  else
	    findmask |= 4;  /* add/sub sp found */
	}
      else if ((insn & 0xff00) == 0xaf00)	/* add r7, sp, #imm */
	{
	  findmask |= 2;  /* setting of r7 found */
	}
      else if (insn == 0x466f)			/* mov r7, sp */
	{
	  findmask |= 2;  /* setting of r7 found */
	}
      else if (findmask == (4+2+1))
	{
	  break;	/* We have found one of each type of prologue instruction */
	}
      else
	continue;	/* something in the prolog that we don't care about or some
	  		   instruction from outside the prolog scheduled here for optimization */
    }

  return current_pc;
}

/* Advance the PC across any function entry prologue instructions to reach
   some "real" code.

   The APCS (ARM Procedure Call Standard) defines the following
   prologue:

   mov          ip, sp
   [stmfd       sp!, {a1,a2,a3,a4}]
   stmfd        sp!, {...,fp,ip,lr,pc}
   [stfe        f7, [sp, #-12]!]
   [stfe        f6, [sp, #-12]!]
   [stfe        f5, [sp, #-12]!]
   [stfe        f4, [sp, #-12]!]
   sub fp, ip, #nn @@ nn == 20 or 4 depending on second insn */

static CORE_ADDR
arm_skip_prologue (CORE_ADDR pc)
{
  unsigned long inst;
  CORE_ADDR skip_pc;
  CORE_ADDR func_addr, func_end;
  char *func_name;
  struct symtab_and_line sal;

  /* See what the symbol table says.  */

  if (find_pc_partial_function (pc, &func_name, &func_addr, &func_end))
    {
      struct symbol *sym;

      /* Found a function.  */
      sym = lookup_symbol (func_name, NULL, VAR_NAMESPACE, NULL, NULL);
      if (sym && SYMBOL_LANGUAGE (sym) != language_asm)
        {
	  /* Don't use this trick for assembly source files. */
	  sal = find_pc_line (func_addr, 0);
	  if ((sal.line != 0) && (sal.end < func_end))
	    return sal.end;
        }
    }

  /* Check if this is Thumb code.  */
  if (arm_pc_is_thumb (pc))
    return thumb_skip_prologue (pc, func_end);

  /* Can't find the prologue end in the symbol table, try it the hard way
     by disassembling the instructions. */
  skip_pc = pc;
  inst = read_memory_integer (skip_pc, 4);
  if (inst != 0xe1a0c00d)	/* mov ip, sp */
    return pc;

  skip_pc += 4;
  inst = read_memory_integer (skip_pc, 4);
  if ((inst & 0xfffffff0) == 0xe92d0000)	/* stmfd sp!,{a1,a2,a3,a4}  */
    {
      skip_pc += 4;
      inst = read_memory_integer (skip_pc, 4);
    }

  if ((inst & 0xfffff800) != 0xe92dd800)	/* stmfd sp!,{...,fp,ip,lr,pc} */
    return pc;

  skip_pc += 4;
  inst = read_memory_integer (skip_pc, 4);

  /* Any insns after this point may float into the code, if it makes
     for better instruction scheduling, so we skip them only if we
     find them, but still consdier the function to be frame-ful.  */

  /* We may have either one sfmfd instruction here, or several stfe
     insns, depending on the version of floating point code we
     support.  */
  if ((inst & 0xffbf0fff) == 0xec2d0200)	/* sfmfd fn, <cnt>, [sp]! */
    {
      skip_pc += 4;
      inst = read_memory_integer (skip_pc, 4);
    }
  else
    {
      while ((inst & 0xffff8fff) == 0xed6d0103)		/* stfe fn, [sp, #-12]! */
	{
	  skip_pc += 4;
	  inst = read_memory_integer (skip_pc, 4);
	}
    }

  if ((inst & 0xfffff000) == 0xe24cb000)	/* sub fp, ip, #nn */
    skip_pc += 4;

  return skip_pc;
}
/* *INDENT-OFF* */
/* Function: thumb_scan_prologue (helper function for arm_scan_prologue)
   This function decodes a Thumb function prologue to determine:
     1) the size of the stack frame
     2) which registers are saved on it
     3) the offsets of saved regs
     4) the offset from the stack pointer to the frame pointer
   This information is stored in the "extra" fields of the frame_info.

   A typical Thumb function prologue would create this stack frame
   (offsets relative to FP)
     old SP ->	24  stack parameters
		20  LR
		16  R7
     R7 ->       0  local variables (16 bytes)
     SP ->     -12  additional stack space (12 bytes)
   The frame size would thus be 36 bytes, and the frame offset would be
   12 bytes.  The frame register is R7. 
   
   The comments for thumb_skip_prolog() describe the algorithm we use to detect
   the end of the prolog */
/* *INDENT-ON* */

static void
thumb_scan_prologue (struct frame_info *fi)
{
  CORE_ADDR prologue_start;
  CORE_ADDR prologue_end;
  CORE_ADDR current_pc;
  int saved_reg[16];		/* which register has been copied to register n? */
  int findmask = 0;  	/* findmask:
      			   bit 0 - push { rlist }
			   bit 1 - mov r7, sp  OR  add r7, sp, #imm  (setting of r7)
      			   bit 2 - sub sp, #simm  OR  add sp, #simm  (adjusting of sp)
			*/
  int i;

  if (find_pc_partial_function (fi->pc, NULL, &prologue_start, &prologue_end))
    {
      struct symtab_and_line sal = find_pc_line (prologue_start, 0);

      if (sal.line == 0)	/* no line info, use current PC */
	prologue_end = fi->pc;
      else if (sal.end < prologue_end)	/* next line begins after fn end */
	prologue_end = sal.end;	/* (probably means no prologue)  */
    }
  else
    prologue_end = prologue_start + 40;		/* We're in the boondocks: allow for */
  /* 16 pushes, an add, and "mv fp,sp" */

  prologue_end = min (prologue_end, fi->pc);

  /* Initialize the saved register map.  When register H is copied to
     register L, we will put H in saved_reg[L].  */
  for (i = 0; i < 16; i++)
    saved_reg[i] = i;

  /* Search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.
     Do this until all basic prolog instructions are found.  */

  fi->extra_info->framesize = 0;
  for (current_pc = prologue_start;
       (current_pc < prologue_end) && ((findmask & 7) != 7);
       current_pc += 2)
    {
      unsigned short insn;
      int regno;
      int offset;

      insn = read_memory_unsigned_integer (current_pc, 2);

      if ((insn & 0xfe00) == 0xb400)	/* push { rlist } */
	{
	  int mask;
	  findmask |= 1;  /* push found */
	  /* Bits 0-7 contain a mask for registers R0-R7.  Bit 8 says
	     whether to save LR (R14).  */
	  mask = (insn & 0xff) | ((insn & 0x100) << 6);

	  /* Calculate offsets of saved R0-R7 and LR. */
	  for (regno = ARM_LR_REGNUM; regno >= 0; regno--)
	    if (mask & (1 << regno))
	      {
		fi->extra_info->framesize += 4;
		fi->saved_regs[saved_reg[regno]] =
		  -(fi->extra_info->framesize);
		saved_reg[regno] = regno;	/* reset saved register map */
	      }
	}
      else if ((insn & 0xff00) == 0xb000)	/* add sp, #simm  OR  sub sp, #simm */
	{
	  if ((findmask & 1) == 0)  /* before push ? */
	    continue;
	  else
	    findmask |= 4;  /* add/sub sp found */
	  
	  offset = (insn & 0x7f) << 2;	/* get scaled offset */
	  if (insn & 0x80)	/* is it signed? (==subtracting) */
	    {
	      fi->extra_info->frameoffset += offset;
	      offset = -offset;
	    }
	  fi->extra_info->framesize -= offset;
	}
      else if ((insn & 0xff00) == 0xaf00)	/* add r7, sp, #imm */
	{
	  findmask |= 2;  /* setting of r7 found */
	  fi->extra_info->framereg = THUMB_FP_REGNUM;
	  /* get scaled offset */
	  fi->extra_info->frameoffset = (insn & 0xff) << 2;
	}
      else if (insn == 0x466f)			/* mov r7, sp */
	{
	  findmask |= 2;  /* setting of r7 found */
	  fi->extra_info->framereg = THUMB_FP_REGNUM;
	  fi->extra_info->frameoffset = 0;
	  saved_reg[THUMB_FP_REGNUM] = ARM_SP_REGNUM;
	}
      else if ((insn & 0xffc0) == 0x4640)	/* mov r0-r7, r8-r15 */
	{
	  int lo_reg = insn & 7;	/* dest. register (r0-r7) */
	  int hi_reg = ((insn >> 3) & 7) + 8;	/* source register (r8-15) */
	  saved_reg[lo_reg] = hi_reg;	/* remember hi reg was saved */
	}
      else
	continue;	/* something in the prolog that we don't care about or some
	  		   instruction from outside the prolog scheduled here for optimization */
    }
}

/* Check if prologue for this frame's PC has already been scanned.  If
   it has, copy the relevant information about that prologue and
   return non-zero.  Otherwise do not copy anything and return zero.

   The information saved in the cache includes:
   * the frame register number;
   * the size of the stack frame;
   * the offsets of saved regs (relative to the old SP); and
   * the offset from the stack pointer to the frame pointer

   The cache contains only one entry, since this is adequate for the
   typical sequence of prologue scan requests we get.  When performing
   a backtrace, GDB will usually ask to scan the same function twice
   in a row (once to get the frame chain, and once to fill in the
   extra frame information).  */

static struct frame_info prologue_cache;

static int
check_prologue_cache (struct frame_info *fi)
{
  int i;

  if (fi->pc == prologue_cache.pc)
    {
      fi->extra_info->framereg = prologue_cache.extra_info->framereg;
      fi->extra_info->framesize = prologue_cache.extra_info->framesize;
      fi->extra_info->frameoffset = prologue_cache.extra_info->frameoffset;
      for (i = 0; i < NUM_REGS + NUM_PSEUDO_REGS; i++)
	fi->saved_regs[i] = prologue_cache.saved_regs[i];
      return 1;
    }
  else
    return 0;
}


/* Copy the prologue information from fi to the prologue cache.  */

static void
save_prologue_cache (struct frame_info *fi)
{
  int i;

  prologue_cache.pc = fi->pc;
  prologue_cache.extra_info->framereg = fi->extra_info->framereg;
  prologue_cache.extra_info->framesize = fi->extra_info->framesize;
  prologue_cache.extra_info->frameoffset = fi->extra_info->frameoffset;

  for (i = 0; i < NUM_REGS + NUM_PSEUDO_REGS; i++)
    prologue_cache.saved_regs[i] = fi->saved_regs[i];
}


/* This function decodes an ARM function prologue to determine:
   1) the size of the stack frame
   2) which registers are saved on it
   3) the offsets of saved regs
   4) the offset from the stack pointer to the frame pointer
   This information is stored in the "extra" fields of the frame_info.

   There are two basic forms for the ARM prologue.  The fixed argument
   function call will look like:

   mov    ip, sp
   stmfd  sp!, {fp, ip, lr, pc}
   sub    fp, ip, #4
   [sub sp, sp, #4]

   Which would create this stack frame (offsets relative to FP):
   IP ->   4    (caller's stack)
   FP ->   0    PC (points to address of stmfd instruction + 8 in callee)
   -4   LR (return address in caller)
   -8   IP (copy of caller's SP)
   -12  FP (caller's FP)
   SP -> -28    Local variables

   The frame size would thus be 32 bytes, and the frame offset would be
   28 bytes.  The stmfd call can also save any of the vN registers it
   plans to use, which increases the frame size accordingly.

   Note: The stored PC is 8 off of the STMFD instruction that stored it
   because the ARM Store instructions always store PC + 8 when you read
   the PC register.

   A variable argument function call will look like:

   mov    ip, sp
   stmfd  sp!, {a1, a2, a3, a4}
   stmfd  sp!, {fp, ip, lr, pc}
   sub    fp, ip, #20

   Which would create this stack frame (offsets relative to FP):
   IP ->  20    (caller's stack)
   16  A4
   12  A3
   8  A2
   4  A1
   FP ->   0    PC (points to address of stmfd instruction + 8 in callee)
   -4   LR (return address in caller)
   -8   IP (copy of caller's SP)
   -12  FP (caller's FP)
   SP -> -28    Local variables

   The frame size would thus be 48 bytes, and the frame offset would be
   28 bytes.

   There is another potential complication, which is that the optimizer
   will try to separate the store of fp in the "stmfd" instruction from
   the "sub fp, ip, #NN" instruction.  Almost anything can be there, so
   we just key on the stmfd, and then scan for the "sub fp, ip, #NN"...

   Also, note, the original version of the ARM toolchain claimed that there
   should be an

   instruction at the end of the prologue.  I have never seen GCC produce
   this, and the ARM docs don't mention it.  We still test for it below in
   case it happens...

 */

static void
arm_scan_prologue (struct frame_info *fi)
{
  int regno, sp_offset, fp_offset;
  LONGEST return_value;
  CORE_ADDR prologue_start, prologue_end, current_pc;

  /* Check if this function is already in the cache of frame information. */
  if (check_prologue_cache (fi))
    return;

  /* Assume there is no frame until proven otherwise.  */
  fi->extra_info->framereg = ARM_SP_REGNUM;
  fi->extra_info->framesize = 0;
  fi->extra_info->frameoffset = 0;

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
      /* One way to find the end of the prologue (which works well
         for unoptimized code) is to do the following:

	    struct symtab_and_line sal = find_pc_line (prologue_start, 0);

	    if (sal.line == 0)
	      prologue_end = fi->pc;
	    else if (sal.end < prologue_end)
	      prologue_end = sal.end;

	 This mechanism is very accurate so long as the optimizer
	 doesn't move any instructions from the function body into the
	 prologue.  If this happens, sal.end will be the last
	 instruction in the first hunk of prologue code just before
	 the first instruction that the scheduler has moved from
	 the body to the prologue.

	 In order to make sure that we scan all of the prologue
	 instructions, we use a slightly less accurate mechanism which
	 may scan more than necessary.  To help compensate for this
	 lack of accuracy, the prologue scanning loop below contains
	 several clauses which'll cause the loop to terminate early if
	 an implausible prologue instruction is encountered.  
	 
	 The expression
	 
	      prologue_start + 64
	    
	 is a suitable endpoint since it accounts for the largest
	 possible prologue plus up to five instructions inserted by
	 the scheduler. */
         
      if (prologue_end > prologue_start + 64)
	{
	  prologue_end = prologue_start + 64;	/* See above. */
	}
    }
  else
    {
      /* Get address of the stmfd in the prologue of the callee; the saved
         PC is the address of the stmfd + 8.  */
      if (!safe_read_memory_integer (fi->frame, 4,  &return_value))
        return;
      else
        {
          prologue_start = ADDR_BITS_REMOVE (return_value) - 8;
          prologue_end = prologue_start + 64;   /* See above. */
        }
    }

  /* Now search the prologue looking for instructions that set up the
     frame pointer, adjust the stack pointer, and save registers.

     Be careful, however, and if it doesn't look like a prologue,
     don't try to scan it.  If, for instance, a frameless function
     begins with stmfd sp!, then we will tell ourselves there is
     a frame, which will confuse stack traceback, as well ad"finish" 
     and other operations that rely on a knowledge of the stack
     traceback.

     In the APCS, the prologue should start with  "mov ip, sp" so
     if we don't see this as the first insn, we will stop.  [Note:
     This doesn't seem to be true any longer, so it's now an optional
     part of the prologue.  - Kevin Buettner, 2001-11-20]  */

  sp_offset = fp_offset = 0;

  if (read_memory_unsigned_integer (prologue_start, 4)
      == 0xe1a0c00d)		/* mov ip, sp */
    current_pc = prologue_start + 4;
  else
    current_pc = prologue_start;

  for (; current_pc < prologue_end; current_pc += 4)
    {
      unsigned int insn = read_memory_unsigned_integer (current_pc, 4);

      if ((insn & 0xffff0000) == 0xe92d0000)
	/* stmfd sp!, {..., fp, ip, lr, pc}
	   or
	   stmfd sp!, {a1, a2, a3, a4}  */
	{
	  int mask = insn & 0xffff;

	  /* Calculate offsets of saved registers. */
	  for (regno = ARM_PC_REGNUM; regno >= 0; regno--)
	    if (mask & (1 << regno))
	      {
		sp_offset -= 4;
		fi->saved_regs[regno] = sp_offset;
	      }
	}
      else if ((insn & 0xfffff000) == 0xe24cb000)	/* sub fp, ip #n */
	{
	  unsigned imm = insn & 0xff;	/* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;	/* rotate amount */
	  imm = (imm >> rot) | (imm << (32 - rot));
	  fp_offset = -imm;
	  fi->extra_info->framereg = ARM_FP_REGNUM;
	}
      else if ((insn & 0xfffff000) == 0xe24dd000)	/* sub sp, sp #n */
	{
	  unsigned imm = insn & 0xff;	/* immediate value */
	  unsigned rot = (insn & 0xf00) >> 7;	/* rotate amount */
	  imm = (imm >> rot) | (imm << (32 - rot));
	  sp_offset -= imm;
	}
      else if ((insn & 0xffff7fff) == 0xed6d0103)	/* stfe f?, [sp, -#c]! */
	{
	  sp_offset -= 12;
	  regno = ARM_F0_REGNUM + ((insn >> 12) & 0x07);
	  fi->saved_regs[regno] = sp_offset;
	}
      else if ((insn & 0xffbf0fff) == 0xec2d0200)	/* sfmfd f0, 4, [sp!] */
	{
	  int n_saved_fp_regs;
	  unsigned int fp_start_reg, fp_bound_reg;

	  if ((insn & 0x800) == 0x800)	/* N0 is set */
	    {
	      if ((insn & 0x40000) == 0x40000)	/* N1 is set */
		n_saved_fp_regs = 3;
	      else
		n_saved_fp_regs = 1;
	    }
	  else
	    {
	      if ((insn & 0x40000) == 0x40000)	/* N1 is set */
		n_saved_fp_regs = 2;
	      else
		n_saved_fp_regs = 4;
	    }

	  fp_start_reg = ARM_F0_REGNUM + ((insn >> 12) & 0x7);
	  fp_bound_reg = fp_start_reg + n_saved_fp_regs;
	  for (; fp_start_reg < fp_bound_reg; fp_start_reg++)
	    {
	      sp_offset -= 12;
	      fi->saved_regs[fp_start_reg++] = sp_offset;
	    }
	}
      else if ((insn & 0xf0000000) != 0xe0000000)
	break;	/* Condition not true, exit early */
      else if ((insn & 0xfe200000) == 0xe8200000) /* ldm? */
	break;	/* Don't scan past a block load */
      else
	/* The optimizer might shove anything into the prologue,
	   so we just skip what we don't recognize. */
	continue;
    }

  /* The frame size is just the negative of the offset (from the original SP)
     of the last thing thing we pushed on the stack.  The frame offset is
     [new FP] - [new SP].  */
  fi->extra_info->framesize = -sp_offset;
  if (fi->extra_info->framereg == ARM_FP_REGNUM)
    fi->extra_info->frameoffset = fp_offset - sp_offset;
  else
    fi->extra_info->frameoffset = 0;

  save_prologue_cache (fi);
}

/* Find REGNUM on the stack.  Otherwise, it's in an active register.
   One thing we might want to do here is to check REGNUM against the
   clobber mask, and somehow flag it as invalid if it isn't saved on
   the stack somewhere.  This would provide a graceful failure mode
   when trying to get the value of caller-saves registers for an inner
   frame.  */

static CORE_ADDR
arm_find_callers_reg (struct frame_info *fi, int regnum)
{
  for (; fi; fi = fi->next)

#if 0				/* FIXME: enable this code if we convert to new call dummy scheme.  */
    if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
      return generic_read_register_dummy (fi->pc, fi->frame, regnum);
    else
#endif
    if (fi->saved_regs[regnum] != 0)
      return read_memory_integer (fi->saved_regs[regnum],
				  REGISTER_RAW_SIZE (regnum));
  return read_register (regnum);
}
/* Function: frame_chain Given a GDB frame, determine the address of
   the calling function's frame.  This will be used to create a new
   GDB frame struct, and then INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC
   will be called for the new frame.  For ARM, we save the frame size
   when we initialize the frame_info.  */

static CORE_ADDR
arm_frame_chain (struct frame_info *fi)
{
#if 0				/* FIXME: enable this code if we convert to new call dummy scheme.  */
  CORE_ADDR fn_start, callers_pc, fp;

  /* is this a dummy frame? */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    return fi->frame;		/* dummy frame same as caller's frame */

  /* is caller-of-this a dummy frame? */
  callers_pc = FRAME_SAVED_PC (fi);	/* find out who called us: */
  fp = arm_find_callers_reg (fi, ARM_FP_REGNUM);
  if (PC_IN_CALL_DUMMY (callers_pc, fp, fp))
    return fp;			/* dummy frame's frame may bear no relation to ours */

  if (find_pc_partial_function (fi->pc, 0, &fn_start, 0))
    if (fn_start == entry_point_address ())
      return 0;			/* in _start fn, don't chain further */
#endif
  CORE_ADDR caller_pc, fn_start;
  int framereg = fi->extra_info->framereg;

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
  /* XXX Fixme, we should try to do this without creating a temporary
     caller_fi.  */
  if (arm_pc_is_thumb (caller_pc) != arm_pc_is_thumb (fi->pc))
    {
      struct frame_info caller_fi;
      struct cleanup *old_chain;

      /* Create a temporary frame suitable for scanning the caller's
	 prologue.  (Ugh.)  */
      memset (&caller_fi, 0, sizeof (caller_fi));
      caller_fi.extra_info = (struct frame_extra_info *)
	xcalloc (1, sizeof (struct frame_extra_info));
      old_chain = make_cleanup (xfree, caller_fi.extra_info);
      caller_fi.saved_regs = (CORE_ADDR *)
	xcalloc (1, SIZEOF_FRAME_SAVED_REGS);
      make_cleanup (xfree, caller_fi.saved_regs);

      /* Now, scan the prologue and obtain the frame register.  */
      caller_fi.pc = caller_pc;
      arm_scan_prologue (&caller_fi);
      framereg = caller_fi.extra_info->framereg;

      /* Deallocate the storage associated with the temporary frame
	 created above.  */
      do_cleanups (old_chain);
    }

  /* If the caller used a frame register, return its value.
     Otherwise, return the caller's stack pointer.  */
  if (framereg == ARM_FP_REGNUM || framereg == THUMB_FP_REGNUM)
    return arm_find_callers_reg (fi, framereg);
  else
    return fi->frame + fi->extra_info->framesize;
}

/* This function actually figures out the frame address for a given pc
   and sp.  This is tricky because we sometimes don't use an explicit
   frame pointer, and the previous stack pointer isn't necessarily
   recorded on the stack.  The only reliable way to get this info is
   to examine the prologue.  FROMLEAF is a little confusing, it means
   this is the next frame up the chain AFTER a frameless function.  If
   this is true, then the frame value for this frame is still in the
   fp register.  */

static void
arm_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  int reg;
  CORE_ADDR sp;

  if (fi->saved_regs == NULL)
    frame_saved_regs_zalloc (fi);

  fi->extra_info = (struct frame_extra_info *)
    frame_obstack_alloc (sizeof (struct frame_extra_info));

  fi->extra_info->framesize = 0;
  fi->extra_info->frameoffset = 0;
  fi->extra_info->framereg = 0;

  if (fi->next)
    fi->pc = FRAME_SAVED_PC (fi->next);

  memset (fi->saved_regs, '\000', sizeof fi->saved_regs);

#if 0				/* FIXME: enable this code if we convert to new call dummy scheme.  */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    {
      /* We need to setup fi->frame here because run_stack_dummy gets it wrong
         by assuming it's always FP.  */
      fi->frame = generic_read_register_dummy (fi->pc, fi->frame,
					       ARM_SP_REGNUM);
      fi->extra_info->framesize = 0;
      fi->extra_info->frameoffset = 0;
      return;
    }
  else
#endif

  /* Compute stack pointer for this frame.  We use this value for both the
     sigtramp and call dummy cases.  */
  if (!fi->next)
    sp = read_sp();
  else
    sp = (fi->next->frame - fi->next->extra_info->frameoffset
	  + fi->next->extra_info->framesize);

  /* Determine whether or not we're in a sigtramp frame. 
     Unfortunately, it isn't sufficient to test
     fi->signal_handler_caller because this value is sometimes set
     after invoking INIT_EXTRA_FRAME_INFO.  So we test *both*
     fi->signal_handler_caller and IN_SIGTRAMP to determine if we need
     to use the sigcontext addresses for the saved registers.

     Note: If an ARM IN_SIGTRAMP method ever needs to compare against
     the name of the function, the code below will have to be changed
     to first fetch the name of the function and then pass this name
     to IN_SIGTRAMP.  */

  if (SIGCONTEXT_REGISTER_ADDRESS_P () 
      && (fi->signal_handler_caller || IN_SIGTRAMP (fi->pc, (char *)0)))
    {
      for (reg = 0; reg < NUM_REGS; reg++)
	fi->saved_regs[reg] = SIGCONTEXT_REGISTER_ADDRESS (sp, fi->pc, reg);

      /* FIXME: What about thumb mode? */
      fi->extra_info->framereg = ARM_SP_REGNUM;
      fi->frame =
	read_memory_integer (fi->saved_regs[fi->extra_info->framereg],
			     REGISTER_RAW_SIZE (fi->extra_info->framereg));
      fi->extra_info->framesize = 0;
      fi->extra_info->frameoffset = 0;

    }
  else if (PC_IN_CALL_DUMMY (fi->pc, sp, fi->frame))
    {
      CORE_ADDR rp;
      CORE_ADDR callers_sp;

      /* Set rp point at the high end of the saved registers.  */
      rp = fi->frame - REGISTER_SIZE;

      /* Fill in addresses of saved registers.  */
      fi->saved_regs[ARM_PS_REGNUM] = rp;
      rp -= REGISTER_RAW_SIZE (ARM_PS_REGNUM);
      for (reg = ARM_PC_REGNUM; reg >= 0; reg--)
	{
	  fi->saved_regs[reg] = rp;
	  rp -= REGISTER_RAW_SIZE (reg);
	}

      callers_sp = read_memory_integer (fi->saved_regs[ARM_SP_REGNUM],
                                        REGISTER_RAW_SIZE (ARM_SP_REGNUM));
      fi->extra_info->framereg = ARM_FP_REGNUM;
      fi->extra_info->framesize = callers_sp - sp;
      fi->extra_info->frameoffset = fi->frame - sp;
    }
  else
    {
      arm_scan_prologue (fi);

      if (!fi->next)
	/* this is the innermost frame? */
	fi->frame = read_register (fi->extra_info->framereg);
      else if (fi->extra_info->framereg == ARM_FP_REGNUM
	       || fi->extra_info->framereg == THUMB_FP_REGNUM)
	{
	  /* not the innermost frame */
	  /* If we have an FP, the callee saved it. */
	  if (fi->next->saved_regs[fi->extra_info->framereg] != 0)
	    fi->frame =
	      read_memory_integer (fi->next
				   ->saved_regs[fi->extra_info->framereg], 4);
	  else if (fromleaf)
	    /* If we were called by a frameless fn.  then our frame is
	       still in the frame pointer register on the board... */
	    fi->frame = read_fp ();
	}

      /* Calculate actual addresses of saved registers using offsets
         determined by arm_scan_prologue.  */
      for (reg = 0; reg < NUM_REGS; reg++)
	if (fi->saved_regs[reg] != 0)
	  fi->saved_regs[reg] += (fi->frame + fi->extra_info->framesize
				  - fi->extra_info->frameoffset);
    }
}


/* Find the caller of this frame.  We do this by seeing if ARM_LR_REGNUM
   is saved in the stack anywhere, otherwise we get it from the
   registers.

   The old definition of this function was a macro:
   #define FRAME_SAVED_PC(FRAME) \
   ADDR_BITS_REMOVE (read_memory_integer ((FRAME)->frame - 4, 4)) */

static CORE_ADDR
arm_frame_saved_pc (struct frame_info *fi)
{
#if 0				/* FIXME: enable this code if we convert to new call dummy scheme.  */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    return generic_read_register_dummy (fi->pc, fi->frame, ARM_PC_REGNUM);
  else
#endif
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame - fi->extra_info->frameoffset,
			fi->frame))
    {
      return read_memory_integer (fi->saved_regs[ARM_PC_REGNUM],
				  REGISTER_RAW_SIZE (ARM_PC_REGNUM));
    }
  else
    {
      CORE_ADDR pc = arm_find_callers_reg (fi, ARM_LR_REGNUM);
      return IS_THUMB_ADDR (pc) ? UNMAKE_THUMB_ADDR (pc) : pc;
    }
}

/* Return the frame address.  On ARM, it is R11; on Thumb it is R7.
   Examine the Program Status Register to decide which state we're in.  */

static CORE_ADDR
arm_read_fp (void)
{
  if (read_register (ARM_PS_REGNUM) & 0x20)	/* Bit 5 is Thumb state bit */
    return read_register (THUMB_FP_REGNUM);	/* R7 if Thumb */
  else
    return read_register (ARM_FP_REGNUM);	/* R11 if ARM */
}

/* Store into a struct frame_saved_regs the addresses of the saved
   registers of frame described by FRAME_INFO.  This includes special
   registers such as PC and FP saved in special ways in the stack
   frame.  SP is even more special: the address we return for it IS
   the sp for the next frame.  */

static void
arm_frame_init_saved_regs (struct frame_info *fip)
{

  if (fip->saved_regs)
    return;

  arm_init_extra_frame_info (0, fip);
}

/* Push an empty stack frame, to record the current PC, etc.  */

static void
arm_push_dummy_frame (void)
{
  CORE_ADDR old_sp = read_register (ARM_SP_REGNUM);
  CORE_ADDR sp = old_sp;
  CORE_ADDR fp, prologue_start;
  int regnum;

  /* Push the two dummy prologue instructions in reverse order,
     so that they'll be in the correct low-to-high order in memory.  */
  /* sub     fp, ip, #4 */
  sp = push_word (sp, 0xe24cb004);
  /*  stmdb   sp!, {r0-r10, fp, ip, lr, pc} */
  prologue_start = sp = push_word (sp, 0xe92ddfff);

  /* Push a pointer to the dummy prologue + 12, because when stm
     instruction stores the PC, it stores the address of the stm
     instruction itself plus 12.  */
  fp = sp = push_word (sp, prologue_start + 12);

  /* Push the processor status.  */
  sp = push_word (sp, read_register (ARM_PS_REGNUM));

  /* Push all 16 registers starting with r15.  */
  for (regnum = ARM_PC_REGNUM; regnum >= 0; regnum--)
    sp = push_word (sp, read_register (regnum));

  /* Update fp (for both Thumb and ARM) and sp.  */
  write_register (ARM_FP_REGNUM, fp);
  write_register (THUMB_FP_REGNUM, fp);
  write_register (ARM_SP_REGNUM, sp);
}

/* CALL_DUMMY_WORDS:
   This sequence of words is the instructions

   mov  lr,pc
   mov  pc,r4
   illegal

   Note this is 12 bytes.  */

static LONGEST arm_call_dummy_words[] =
{
  0xe1a0e00f, 0xe1a0f004, 0xe7ffdefe
};

/* Adjust the call_dummy_breakpoint_offset for the bp_call_dummy
   breakpoint to the proper address in the call dummy, so that
   `finish' after a stop in a call dummy works.

   FIXME rearnsha 2002-02018: Tweeking current_gdbarch is not an
   optimal solution, but the call to arm_fix_call_dummy is immediately
   followed by a call to run_stack_dummy, which is the only function
   where call_dummy_breakpoint_offset is actually used.  */


static void
arm_set_call_dummy_breakpoint_offset (void)
{
  if (caller_is_thumb)
    set_gdbarch_call_dummy_breakpoint_offset (current_gdbarch, 4);
  else
    set_gdbarch_call_dummy_breakpoint_offset (current_gdbarch, 8);
}

/* Fix up the call dummy, based on whether the processor is currently
   in Thumb or ARM mode, and whether the target function is Thumb or
   ARM.  There are three different situations requiring three
   different dummies:

   * ARM calling ARM: uses the call dummy in tm-arm.h, which has already
   been copied into the dummy parameter to this function.
   * ARM calling Thumb: uses the call dummy in tm-arm.h, but with the
   "mov pc,r4" instruction patched to be a "bx r4" instead.
   * Thumb calling anything: uses the Thumb dummy defined below, which
   works for calling both ARM and Thumb functions.

   All three call dummies expect to receive the target function
   address in R4, with the low bit set if it's a Thumb function.  */

static void
arm_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs,
		    struct value **args, struct type *type, int gcc_p)
{
  static short thumb_dummy[4] =
  {
    0xf000, 0xf801,		/*        bl      label */
    0xdf18,			/*        swi     24 */
    0x4720,			/* label: bx      r4 */
  };
  static unsigned long arm_bx_r4 = 0xe12fff14;	/* bx r4 instruction */

  /* Set flag indicating whether the current PC is in a Thumb function. */
  caller_is_thumb = arm_pc_is_thumb (read_pc ());
  arm_set_call_dummy_breakpoint_offset ();

  /* If the target function is Thumb, set the low bit of the function
     address.  And if the CPU is currently in ARM mode, patch the
     second instruction of call dummy to use a BX instruction to
     switch to Thumb mode.  */
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

  /* Put the target address in r4; the call dummy will copy this to
     the PC. */
  write_register (4, fun);
}

/* Note: ScottB

   This function does not support passing parameters using the FPA
   variant of the APCS.  It passes any floating point arguments in the
   general registers and/or on the stack.  */

static CORE_ADDR
arm_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		    int struct_return, CORE_ADDR struct_addr)
{
  char *fp;
  int argnum, argreg, nstack_size;

  /* Walk through the list of args and determine how large a temporary
     stack is required.  Need to take care here as structs may be
     passed on the stack, and we have to to push them.  */
  nstack_size = -4 * REGISTER_SIZE;	/* Some arguments go into A1-A4.  */
  if (struct_return)		/* The struct address goes in A1.  */
    nstack_size += REGISTER_SIZE;

  /* Walk through the arguments and add their size to nstack_size.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      struct type *arg_type;

      arg_type = check_typedef (VALUE_TYPE (args[argnum]));
      len = TYPE_LENGTH (arg_type);

      nstack_size += len;
    }

  /* Allocate room on the stack, and initialize our stack frame
     pointer.  */
  fp = NULL;
  if (nstack_size > 0)
    {
      sp -= nstack_size;
      fp = (char *) sp;
    }

  /* Initialize the integer argument register pointer.  */
  argreg = ARM_A1_REGNUM;

  /* The struct_return pointer occupies the first parameter passing
     register.  */
  if (struct_return)
    write_register (argreg++, struct_addr);

  /* Process arguments from left to right.  Store as many as allowed
     in the parameter passing registers (A1-A4), and save the rest on
     the temporary stack.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      char *val;
      CORE_ADDR regval;
      enum type_code typecode;
      struct type *arg_type, *target_type;

      arg_type = check_typedef (VALUE_TYPE (args[argnum]));
      target_type = TYPE_TARGET_TYPE (arg_type);
      len = TYPE_LENGTH (arg_type);
      typecode = TYPE_CODE (arg_type);
      val = (char *) VALUE_CONTENTS (args[argnum]);

#if 1
      /* I don't know why this code was disable. The only logical use
         for a function pointer is to call that function, so setting
         the mode bit is perfectly fine. FN */
      /* If the argument is a pointer to a function, and it is a Thumb
         function, set the low bit of the pointer.  */
      if (TYPE_CODE_PTR == typecode
	  && NULL != target_type
	  && TYPE_CODE_FUNC == TYPE_CODE (target_type))
	{
	  CORE_ADDR regval = extract_address (val, len);
	  if (arm_pc_is_thumb (regval))
	    store_address (val, len, MAKE_THUMB_ADDR (regval));
	}
#endif
      /* Copy the argument to general registers or the stack in
         register-sized pieces.  Large arguments are split between
         registers and stack.  */
      while (len > 0)
	{
	  int partial_len = len < REGISTER_SIZE ? len : REGISTER_SIZE;

	  if (argreg <= ARM_LAST_ARG_REGNUM)
	    {
	      /* It's an argument being passed in a general register.  */
	      regval = extract_address (val, partial_len);
	      write_register (argreg++, regval);
	    }
	  else
	    {
	      /* Push the arguments onto the stack.  */
	      write_memory ((CORE_ADDR) fp, val, REGISTER_SIZE);
	      fp += REGISTER_SIZE;
	    }

	  len -= partial_len;
	  val += partial_len;
	}
    }

  /* Return adjusted stack pointer.  */
  return sp;
}

/* Pop the current frame.  So long as the frame info has been initialized
   properly (see arm_init_extra_frame_info), this code works for dummy frames
   as well as regular frames.  I.e, there's no need to have a special case
   for dummy frames.  */
static void
arm_pop_frame (void)
{
  int regnum;
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR old_SP = (frame->frame - frame->extra_info->frameoffset
		      + frame->extra_info->framesize);

  for (regnum = 0; regnum < NUM_REGS; regnum++)
    if (frame->saved_regs[regnum] != 0)
      write_register (regnum,
		  read_memory_integer (frame->saved_regs[regnum],
				       REGISTER_RAW_SIZE (regnum)));

  write_register (ARM_PC_REGNUM, FRAME_SAVED_PC (frame));
  write_register (ARM_SP_REGNUM, old_SP);

  flush_cached_frames ();
}

static void
print_fpu_flags (int flags)
{
  if (flags & (1 << 0))
    fputs ("IVO ", stdout);
  if (flags & (1 << 1))
    fputs ("DVZ ", stdout);
  if (flags & (1 << 2))
    fputs ("OFL ", stdout);
  if (flags & (1 << 3))
    fputs ("UFL ", stdout);
  if (flags & (1 << 4))
    fputs ("INX ", stdout);
  putchar ('\n');
}

/* Print interesting information about the floating point processor
   (if present) or emulator.  */
static void
arm_print_float_info (void)
{
  register unsigned long status = read_register (ARM_FPS_REGNUM);
  int type;

  type = (status >> 24) & 127;
  printf ("%s FPU type %d\n",
	  (status & (1 << 31)) ? "Hardware" : "Software",
	  type);
  fputs ("mask: ", stdout);
  print_fpu_flags (status >> 16);
  fputs ("flags: ", stdout);
  print_fpu_flags (status);
}

/* Return the GDB type object for the "standard" data type of data in
   register N.  */

static struct type *
arm_register_type (int regnum)
{
  if (regnum >= ARM_F0_REGNUM && regnum < ARM_F0_REGNUM + NUM_FREGS)
    {
      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
	return builtin_type_arm_ext_big;
      else
	return builtin_type_arm_ext_littlebyte_bigword;
    }
  else
    return builtin_type_int32;
}

/* Index within `registers' of the first byte of the space for
   register N.  */

static int
arm_register_byte (int regnum)
{
  if (regnum < ARM_F0_REGNUM)
    return regnum * INT_REGISTER_RAW_SIZE;
  else if (regnum < ARM_PS_REGNUM)
    return (NUM_GREGS * INT_REGISTER_RAW_SIZE
	    + (regnum - ARM_F0_REGNUM) * FP_REGISTER_RAW_SIZE);
  else
    return (NUM_GREGS * INT_REGISTER_RAW_SIZE
	    + NUM_FREGS * FP_REGISTER_RAW_SIZE
	    + (regnum - ARM_FPS_REGNUM) * STATUS_REGISTER_SIZE);
}

/* Number of bytes of storage in the actual machine representation for
   register N.  All registers are 4 bytes, except fp0 - fp7, which are
   12 bytes in length.  */

static int
arm_register_raw_size (int regnum)
{
  if (regnum < ARM_F0_REGNUM)
    return INT_REGISTER_RAW_SIZE;
  else if (regnum < ARM_FPS_REGNUM)
    return FP_REGISTER_RAW_SIZE;
  else
    return STATUS_REGISTER_SIZE;
}

/* Number of bytes of storage in a program's representation
   for register N.  */
static int
arm_register_virtual_size (int regnum)
{
  if (regnum < ARM_F0_REGNUM)
    return INT_REGISTER_VIRTUAL_SIZE;
  else if (regnum < ARM_FPS_REGNUM)
    return FP_REGISTER_VIRTUAL_SIZE;
  else
    return STATUS_REGISTER_SIZE;
}


/* NOTE: cagney/2001-08-20: Both convert_from_extended() and
   convert_to_extended() use floatformat_arm_ext_littlebyte_bigword.
   It is thought that this is is the floating-point register format on
   little-endian systems.  */

static void
convert_from_extended (void *ptr, void *dbl)
{
  DOUBLEST d;
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    floatformat_to_doublest (&floatformat_arm_ext_big, ptr, &d);
  else
    floatformat_to_doublest (&floatformat_arm_ext_littlebyte_bigword,
			     ptr, &d);
  floatformat_from_doublest (TARGET_DOUBLE_FORMAT, &d, dbl);
}

static void
convert_to_extended (void *dbl, void *ptr)
{
  DOUBLEST d;
  floatformat_to_doublest (TARGET_DOUBLE_FORMAT, ptr, &d);
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    floatformat_from_doublest (&floatformat_arm_ext_big, &d, dbl);
  else
    floatformat_from_doublest (&floatformat_arm_ext_littlebyte_bigword,
			       &d, dbl);
}

static int
condition_true (unsigned long cond, unsigned long status_reg)
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

/* Support routines for single stepping.  Calculate the next PC value.  */
#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1

static unsigned long
shifted_reg_val (unsigned long inst, int carry, unsigned long pc_val,
		 unsigned long status_reg)
{
  unsigned long res, shift;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);

  if (bit (inst, 4))
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
    case 0:			/* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;

    case 1:			/* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2:			/* ASR */
      if (shift >= 32)
	shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3:			/* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32 - shift));
      break;
    }

  return res & 0xffffffff;
}

/* Return number of 1-bits in VAL.  */

static int
bitcount (unsigned long val)
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;		/* delete rightmost 1-bit in val */
  return nbits;
}

CORE_ADDR
thumb_get_next_pc (CORE_ADDR pc)
{
  unsigned long pc_val = ((unsigned long) pc) + 4;	/* PC after prefetch */
  unsigned short inst1 = read_memory_integer (pc, 2);
  CORE_ADDR nextpc = pc + 2;	/* default is next instruction */
  unsigned long offset;

  if ((inst1 & 0xff00) == 0xbd00)	/* pop {rlist, pc} */
    {
      CORE_ADDR sp;

      /* Fetch the saved PC from the stack.  It's stored above
         all of the other registers.  */
      offset = bitcount (bits (inst1, 0, 7)) * REGISTER_SIZE;
      sp = read_register (ARM_SP_REGNUM);
      nextpc = (CORE_ADDR) read_memory_integer (sp + offset, 4);
      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	error ("Infinite loop detected");
    }
  else if ((inst1 & 0xf000) == 0xd000)	/* conditional branch */
    {
      unsigned long status = read_register (ARM_PS_REGNUM);
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
      offset = (sbits (inst1, 0, 10) << 12) + (bits (inst2, 0, 10) << 1);
      nextpc = pc_val + offset;
    }

  return nextpc;
}

CORE_ADDR
arm_get_next_pc (CORE_ADDR pc)
{
  unsigned long pc_val;
  unsigned long this_instr;
  unsigned long status;
  CORE_ADDR nextpc;

  if (arm_pc_is_thumb (pc))
    return thumb_get_next_pc (pc);

  pc_val = (unsigned long) pc;
  this_instr = read_memory_integer (pc, 4);
  status = read_register (ARM_PS_REGNUM);
  nextpc = (CORE_ADDR) (pc_val + 4);	/* Default case */

  if (condition_true (bits (this_instr, 28, 31), status))
    {
      switch (bits (this_instr, 24, 27))
	{
	case 0x0:
	case 0x1:		/* data processing */
	case 0x2:
	case 0x3:
	  {
	    unsigned long operand1, operand2, result = 0;
	    unsigned long rn;
	    int c;

	    if (bits (this_instr, 12, 15) != 15)
	      break;

	    if (bits (this_instr, 22, 25) == 0
		&& bits (this_instr, 4, 7) == 9)	/* multiply */
	      error ("Illegal update to pc in instruction");

	    /* Multiply into PC */
	    c = (status & FLAG_C) ? 1 : 0;
	    rn = bits (this_instr, 16, 19);
	    operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);

	    if (bit (this_instr, 25))
	      {
		unsigned long immval = bits (this_instr, 0, 7);
		unsigned long rotate = 2 * bits (this_instr, 8, 11);
		operand2 = ((immval >> rotate) | (immval << (32 - rotate)))
		  & 0xffffffff;
	      }
	    else		/* operand 2 is a shifted register */
	      operand2 = shifted_reg_val (this_instr, c, pc_val, status);

	    switch (bits (this_instr, 21, 24))
	      {
	      case 0x0:	/*and */
		result = operand1 & operand2;
		break;

	      case 0x1:	/*eor */
		result = operand1 ^ operand2;
		break;

	      case 0x2:	/*sub */
		result = operand1 - operand2;
		break;

	      case 0x3:	/*rsb */
		result = operand2 - operand1;
		break;

	      case 0x4:	/*add */
		result = operand1 + operand2;
		break;

	      case 0x5:	/*adc */
		result = operand1 + operand2 + c;
		break;

	      case 0x6:	/*sbc */
		result = operand1 - operand2 + c;
		break;

	      case 0x7:	/*rsc */
		result = operand2 - operand1 + c;
		break;

	      case 0x8:
	      case 0x9:
	      case 0xa:
	      case 0xb:	/* tst, teq, cmp, cmn */
		result = (unsigned long) nextpc;
		break;

	      case 0xc:	/*orr */
		result = operand1 | operand2;
		break;

	      case 0xd:	/*mov */
		/* Always step into a function.  */
		result = operand2;
		break;

	      case 0xe:	/*bic */
		result = operand1 & ~operand2;
		break;

	      case 0xf:	/*mvn */
		result = ~operand2;
		break;
	      }
	    nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }

	case 0x4:
	case 0x5:		/* data transfer */
	case 0x6:
	case 0x7:
	  if (bit (this_instr, 20))
	    {
	      /* load */
	      if (bits (this_instr, 12, 15) == 15)
		{
		  /* rd == pc */
		  unsigned long rn;
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
		       ? shifted_reg_val (this_instr, c, pc_val, status)
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

	case 0x8:
	case 0x9:		/* block transfer */
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
		      if (bit (this_instr, 24))		/* pre */
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

	case 0xb:		/* branch & link */
	case 0xa:		/* branch */
	  {
	    nextpc = BranchDest (pc, this_instr);

	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }

	case 0xc:
	case 0xd:
	case 0xe:		/* coproc ops */
	case 0xf:		/* SWI */
	  break;

	default:
	  fprintf_filtered (gdb_stderr, "Bad bit-field extraction\n");
	  return (pc);
	}
    }

  return nextpc;
}

/* single_step() is called just before we want to resume the inferior,
   if we want to single-step it but there is no hardware or kernel
   single-step support.  We find the target of the coming instruction
   and breakpoint it.

   single_step is also called just after the inferior stops.  If we had
   set up a simulated single-step, we undo our damage.  */

static void
arm_software_single_step (enum target_signal sig, int insert_bpt)
{
  static int next_pc; /* State between setting and unsetting. */
  static char break_mem[BREAKPOINT_MAX]; /* Temporary storage for mem@bpt */

  if (insert_bpt)
    {
      next_pc = arm_get_next_pc (read_register (ARM_PC_REGNUM));
      target_insert_breakpoint (next_pc, break_mem);
    }
  else
    target_remove_breakpoint (next_pc, break_mem);
}

#include "bfd-in2.h"
#include "libcoff.h"

static int
gdb_print_insn_arm (bfd_vma memaddr, disassemble_info *info)
{
  if (arm_pc_is_thumb (memaddr))
    {
      static asymbol *asym;
      static combined_entry_type ce;
      static struct coff_symbol_struct csym;
      static struct _bfd fake_bfd;
      static bfd_target fake_target;

      if (csym.native == NULL)
	{
	  /* Create a fake symbol vector containing a Thumb symbol.  This is
	     solely so that the code in print_insn_little_arm() and
	     print_insn_big_arm() in opcodes/arm-dis.c will detect the presence
	     of a Thumb symbol and switch to decoding Thumb instructions.  */

	  fake_target.flavour = bfd_target_coff_flavour;
	  fake_bfd.xvec = &fake_target;
	  ce.u.syment.n_sclass = C_THUMBEXTFUNC;
	  csym.native = &ce;
	  csym.symbol.the_bfd = &fake_bfd;
	  csym.symbol.name = "fake";
	  asym = (asymbol *) & csym;
	}

      memaddr = UNMAKE_THUMB_ADDR (memaddr);
      info->symbols = &asym;
    }
  else
    info->symbols = NULL;

  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    return print_insn_big_arm (memaddr, info);
  else
    return print_insn_little_arm (memaddr, info);
}

/* The following define instruction sequences that will cause ARM
   cpu's to take an undefined instruction trap.  These are used to
   signal a breakpoint to GDB.
   
   The newer ARMv4T cpu's are capable of operating in ARM or Thumb
   modes.  A different instruction is required for each mode.  The ARM
   cpu's can also be big or little endian.  Thus four different
   instructions are needed to support all cases.
   
   Note: ARMv4 defines several new instructions that will take the
   undefined instruction trap.  ARM7TDMI is nominally ARMv4T, but does
   not in fact add the new instructions.  The new undefined
   instructions in ARMv4 are all instructions that had no defined
   behaviour in earlier chips.  There is no guarantee that they will
   raise an exception, but may be treated as NOP's.  In practice, it
   may only safe to rely on instructions matching:
   
   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 
   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   C C C C 0 1 1 x x x x x x x x x x x x x x x x x x x x 1 x x x x
   
   Even this may only true if the condition predicate is true. The
   following use a condition predicate of ALWAYS so it is always TRUE.
   
   There are other ways of forcing a breakpoint.  GNU/Linux, RISC iX,
   and NetBSD all use a software interrupt rather than an undefined
   instruction to force a trap.  This can be handled by by the
   abi-specific code during establishment of the gdbarch vector.  */


/* NOTE rearnsha 2002-02-18: for now we allow a non-multi-arch gdb to
   override these definitions.  */
#ifndef ARM_LE_BREAKPOINT
#define ARM_LE_BREAKPOINT {0xFE,0xDE,0xFF,0xE7}
#endif
#ifndef ARM_BE_BREAKPOINT
#define ARM_BE_BREAKPOINT {0xE7,0xFF,0xDE,0xFE}
#endif
#ifndef THUMB_LE_BREAKPOINT
#define THUMB_LE_BREAKPOINT {0xfe,0xdf}
#endif
#ifndef THUMB_BE_BREAKPOINT
#define THUMB_BE_BREAKPOINT {0xdf,0xfe}
#endif

static const char arm_default_arm_le_breakpoint[] = ARM_LE_BREAKPOINT;
static const char arm_default_arm_be_breakpoint[] = ARM_BE_BREAKPOINT;
static const char arm_default_thumb_le_breakpoint[] = THUMB_LE_BREAKPOINT;
static const char arm_default_thumb_be_breakpoint[] = THUMB_BE_BREAKPOINT;

/* Determine the type and size of breakpoint to insert at PCPTR.  Uses
   the program counter value to determine whether a 16-bit or 32-bit
   breakpoint should be used.  It returns a pointer to a string of
   bytes that encode a breakpoint instruction, stores the length of
   the string to *lenptr, and adjusts the program counter (if
   necessary) to point to the actual memory location where the
   breakpoint should be inserted.  */

/* XXX ??? from old tm-arm.h: if we're using RDP, then we're inserting
   breakpoints and storing their handles instread of what was in
   memory.  It is nice that this is the same size as a handle -
   otherwise remote-rdp will have to change. */

unsigned char *
arm_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (arm_pc_is_thumb (*pcptr) || arm_pc_is_thumb_dummy (*pcptr))
    {
      *pcptr = UNMAKE_THUMB_ADDR (*pcptr);
      *lenptr = tdep->thumb_breakpoint_size;
      return tdep->thumb_breakpoint;
    }
  else
    {
      *lenptr = tdep->arm_breakpoint_size;
      return tdep->arm_breakpoint;
    }
}

/* Extract from an array REGBUF containing the (raw) register state a
   function return value of type TYPE, and copy that, in virtual
   format, into VALBUF.  */

static void
arm_extract_return_value (struct type *type,
			  char regbuf[REGISTER_BYTES],
			  char *valbuf)
{
  if (TYPE_CODE_FLT == TYPE_CODE (type))
    {
      struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

      switch (tdep->fp_model)
	{
	case ARM_FLOAT_FPA:
	  convert_from_extended (&regbuf[REGISTER_BYTE (ARM_F0_REGNUM)],
				 valbuf);
	  break;

	case ARM_FLOAT_SOFT:
	case ARM_FLOAT_SOFT_VFP:
	  memcpy (valbuf, &regbuf[REGISTER_BYTE (ARM_A1_REGNUM)],
		  TYPE_LENGTH (type));
	  break;

	default:
	  internal_error
	    (__FILE__, __LINE__,
	     "arm_extract_return_value: Floating point model not supported");
	  break;
	}
    }
  else
    memcpy (valbuf, &regbuf[REGISTER_BYTE (ARM_A1_REGNUM)],
	    TYPE_LENGTH (type));
}

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value.  */

static CORE_ADDR
arm_extract_struct_value_address (char *regbuf)
{
  return extract_address (regbuf, REGISTER_RAW_SIZE(ARM_A1_REGNUM));
}

/* Will a function return an aggregate type in memory or in a
   register?  Return 0 if an aggregate type can be returned in a
   register, 1 if it must be returned in memory.  */

static int
arm_use_struct_convention (int gcc_p, struct type *type)
{
  int nRc;
  register enum type_code code;

  /* In the ARM ABI, "integer" like aggregate types are returned in
     registers.  For an aggregate type to be integer like, its size
     must be less than or equal to REGISTER_SIZE and the offset of
     each addressable subfield must be zero.  Note that bit fields are
     not addressable, and all addressable subfields of unions always
     start at offset zero.

     This function is based on the behaviour of GCC 2.95.1.
     See: gcc/arm.c: arm_return_in_memory() for details.

     Note: All versions of GCC before GCC 2.95.2 do not set up the
     parameters correctly for a function returning the following
     structure: struct { float f;}; This should be returned in memory,
     not a register.  Richard Earnshaw sent me a patch, but I do not
     know of any way to detect if a function like the above has been
     compiled with the correct calling convention.  */

  /* All aggregate types that won't fit in a register must be returned
     in memory.  */
  if (TYPE_LENGTH (type) > REGISTER_SIZE)
    {
      return 1;
    }

  /* The only aggregate types that can be returned in a register are
     structs and unions.  Arrays must be returned in memory.  */
  code = TYPE_CODE (type);
  if ((TYPE_CODE_STRUCT != code) && (TYPE_CODE_UNION != code))
    {
      return 1;
    }

  /* Assume all other aggregate types can be returned in a register.
     Run a check for structures, unions and arrays.  */
  nRc = 0;

  if ((TYPE_CODE_STRUCT == code) || (TYPE_CODE_UNION == code))
    {
      int i;
      /* Need to check if this struct/union is "integer" like.  For
         this to be true, its size must be less than or equal to
         REGISTER_SIZE and the offset of each addressable subfield
         must be zero.  Note that bit fields are not addressable, and
         unions always start at offset zero.  If any of the subfields
         is a floating point type, the struct/union cannot be an
         integer type.  */

      /* For each field in the object, check:
         1) Is it FP? --> yes, nRc = 1;
         2) Is it addressable (bitpos != 0) and
         not packed (bitsize == 0)?
         --> yes, nRc = 1  
       */

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  enum type_code field_type_code;
	  field_type_code = TYPE_CODE (TYPE_FIELD_TYPE (type, i));

	  /* Is it a floating point type field?  */
	  if (field_type_code == TYPE_CODE_FLT)
	    {
	      nRc = 1;
	      break;
	    }

	  /* If bitpos != 0, then we have to care about it.  */
	  if (TYPE_FIELD_BITPOS (type, i) != 0)
	    {
	      /* Bitfields are not addressable.  If the field bitsize is 
	         zero, then the field is not packed.  Hence it cannot be
	         a bitfield or any other packed type.  */
	      if (TYPE_FIELD_BITSIZE (type, i) == 0)
		{
		  nRc = 1;
		  break;
		}
	    }
	}
    }

  return nRc;
}

/* Write into appropriate registers a function return value of type
   TYPE, given in virtual format.  */

static void
arm_store_return_value (struct type *type, char *valbuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
      char buf[MAX_REGISTER_RAW_SIZE];

      switch (tdep->fp_model)
	{
	case ARM_FLOAT_FPA:

	  convert_to_extended (valbuf, buf);
	  write_register_bytes (REGISTER_BYTE (ARM_F0_REGNUM), buf,
				MAX_REGISTER_RAW_SIZE);
	  break;

	case ARM_FLOAT_SOFT:
	case ARM_FLOAT_SOFT_VFP:
	  write_register_bytes (ARM_A1_REGNUM, valbuf, TYPE_LENGTH (type));
	  break;

	default:
	  internal_error
	    (__FILE__, __LINE__,
	     "arm_store_return_value: Floating point model not supported");
	  break;
	}
    }
  else
    write_register_bytes (ARM_A1_REGNUM, valbuf, TYPE_LENGTH (type));
}

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

static void
arm_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (ARM_A1_REGNUM, addr);
}

static int
arm_get_longjmp_target (CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  char buf[INT_REGISTER_RAW_SIZE];
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  jb_addr = read_register (ARM_A1_REGNUM);

  if (target_read_memory (jb_addr + tdep->jb_pc * tdep->jb_elt_size, buf,
			  INT_REGISTER_RAW_SIZE))
    return 0;

  *pc = extract_address (buf, INT_REGISTER_RAW_SIZE);
  return 1;
}

/* Return non-zero if the PC is inside a thumb call thunk.  */

int
arm_in_call_stub (CORE_ADDR pc, char *name)
{
  CORE_ADDR start_addr;

  /* Find the starting address of the function containing the PC.  If
     the caller didn't give us a name, look it up at the same time.  */
  if (find_pc_partial_function (pc, name ? NULL : &name, &start_addr, NULL) == 0)
    return 0;

  return strncmp (name, "_call_via_r", 11) == 0;
}

/* If PC is in a Thumb call or return stub, return the address of the
   target PC, which is in a register.  The thunk functions are called
   _called_via_xx, where x is the register name.  The possible names
   are r0-r9, sl, fp, ip, sp, and lr.  */

CORE_ADDR
arm_skip_stub (CORE_ADDR pc)
{
  char *name;
  CORE_ADDR start_addr;

  /* Find the starting address and name of the function containing the PC.  */
  if (find_pc_partial_function (pc, &name, &start_addr, NULL) == 0)
    return 0;

  /* Call thunks always start with "_call_via_".  */
  if (strncmp (name, "_call_via_", 10) == 0)
    {
      /* Use the name suffix to determine which register contains the
         target PC.  */
      static char *table[15] =
      {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
       "r8", "r9", "sl", "fp", "ip", "sp", "lr"
      };
      int regno;

      for (regno = 0; regno <= 14; regno++)
	if (strcmp (&name[10], table[regno]) == 0)
	  return read_register (regno);
    }

  return 0;			/* not a stub */
}

/* If the user changes the register disassembly flavor used for info register
   and other commands, we have to also switch the flavor used in opcodes
   for disassembly output.
   This function is run in the set disassembly_flavor command, and does that. */

static void
set_disassembly_flavor_sfunc (char *args, int from_tty,
			      struct cmd_list_element *c)
{
  set_disassembly_flavor ();
}

/* Return the ARM register name corresponding to register I.  */
static char *
arm_register_name (int i)
{
  return arm_register_names[i];
}

static void
set_disassembly_flavor (void)
{
  const char *setname, *setdesc, **regnames;
  int numregs, j;

  /* Find the flavor that the user wants in the opcodes table. */
  int current = 0;
  numregs = get_arm_regnames (current, &setname, &setdesc, &regnames);
  while ((disassembly_flavor != setname)
	 && (current < num_flavor_options))
    get_arm_regnames (++current, &setname, &setdesc, &regnames);
  current_option = current;

  /* Fill our copy. */
  for (j = 0; j < numregs; j++)
    arm_register_names[j] = (char *) regnames[j];

  /* Adjust case. */
  if (isupper (*regnames[ARM_PC_REGNUM]))
    {
      arm_register_names[ARM_FPS_REGNUM] = "FPS";
      arm_register_names[ARM_PS_REGNUM] = "CPSR";
    }
  else
    {
      arm_register_names[ARM_FPS_REGNUM] = "fps";
      arm_register_names[ARM_PS_REGNUM] = "cpsr";
    }

  /* Synchronize the disassembler. */
  set_arm_regname_option (current);
}

/* arm_othernames implements the "othernames" command.  This is kind
   of hacky, and I prefer the set-show disassembly-flavor which is
   also used for the x86 gdb.  I will keep this around, however, in
   case anyone is actually using it. */

static void
arm_othernames (char *names, int n)
{
  /* Circle through the various flavors. */
  current_option = (current_option + 1) % num_flavor_options;

  disassembly_flavor = valid_flavors[current_option];
  set_disassembly_flavor (); 
}

/* Fetch, and possibly build, an appropriate link_map_offsets structure
   for ARM linux targets using the struct offsets defined in <link.h>.
   Note, however, that link.h is not actually referred to in this file.
   Instead, the relevant structs offsets were obtained from examining
   link.h.  (We can't refer to link.h from this file because the host
   system won't necessarily have it, or if it does, the structs which
   it defines will refer to the host system, not the target.)  */

struct link_map_offsets *
arm_linux_svr4_fetch_link_map_offsets (void)
{
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = 0;

  if (lmp == 0)
    {
      lmp = &lmo;

      lmo.r_debug_size = 8;	/* Actual size is 20, but this is all we
                                   need. */

      lmo.r_map_offset = 4;
      lmo.r_map_size   = 4;

      lmo.link_map_size = 20;	/* Actual size is 552, but this is all we
                                   need. */

      lmo.l_addr_offset = 0;
      lmo.l_addr_size   = 4;

      lmo.l_name_offset = 4;
      lmo.l_name_size   = 4;

      lmo.l_next_offset = 12;
      lmo.l_next_size   = 4;

      lmo.l_prev_offset = 16;
      lmo.l_prev_size   = 4;
    }

    return lmp;
}

/* Test whether the coff symbol specific value corresponds to a Thumb
   function.  */

static int
coff_sym_is_thumb (int val)
{
  return (val == C_THUMBEXT ||
	  val == C_THUMBSTAT ||
	  val == C_THUMBEXTFUNC ||
	  val == C_THUMBSTATFUNC ||
	  val == C_THUMBLABEL);
}

/* arm_coff_make_msymbol_special()
   arm_elf_make_msymbol_special()
   
   These functions test whether the COFF or ELF symbol corresponds to
   an address in thumb code, and set a "special" bit in a minimal
   symbol to indicate that it does.  */
   
static void
arm_elf_make_msymbol_special(asymbol *sym, struct minimal_symbol *msym)
{
  /* Thumb symbols are of type STT_LOPROC, (synonymous with
     STT_ARM_TFUNC).  */
  if (ELF_ST_TYPE (((elf_symbol_type *)sym)->internal_elf_sym.st_info)
      == STT_LOPROC)
    MSYMBOL_SET_SPECIAL (msym);
}

static void
arm_coff_make_msymbol_special(int val, struct minimal_symbol *msym)
{
  if (coff_sym_is_thumb (val))
    MSYMBOL_SET_SPECIAL (msym);
}


static void
process_note_abi_tag_sections (bfd *abfd, asection *sect, void *obj)
{
  enum arm_abi *os_ident_ptr = obj;
  const char *name;
  unsigned int sectsize;

  name = bfd_get_section_name (abfd, sect);
  sectsize = bfd_section_size (abfd, sect);

  if (strcmp (name, ".note.ABI-tag") == 0 && sectsize > 0)
    {
      unsigned int name_length, data_length, note_type;
      char *note;

      /* If the section is larger than this, it's probably not what we are
	 looking for.  */
      if (sectsize > 128)
	sectsize = 128;

      note = alloca (sectsize);

      bfd_get_section_contents (abfd, sect, note,
                                (file_ptr) 0, (bfd_size_type) sectsize);

      name_length = bfd_h_get_32 (abfd, note);
      data_length = bfd_h_get_32 (abfd, note + 4);
      note_type   = bfd_h_get_32 (abfd, note + 8);

      if (name_length == 4 && data_length == 16 && note_type == 1
          && strcmp (note + 12, "GNU") == 0)
	{
	  int os_number = bfd_h_get_32 (abfd, note + 16);

	  /* The case numbers are from abi-tags in glibc.  */
	  switch (os_number)
	    {
	    case 0 :
	      *os_ident_ptr = ARM_ABI_LINUX;
	      break;

	    case 1 :
	      internal_error
		(__FILE__, __LINE__,
		 "process_note_abi_sections: Hurd objects not supported");
	      break;

	    case 2 :
	      internal_error
		(__FILE__, __LINE__,
		 "process_note_abi_sections: Solaris objects not supported");
	      break;

	    default :
	      internal_error
		(__FILE__, __LINE__,
		 "process_note_abi_sections: unknown OS number %d",
		 os_number);
	      break;
	    }
	}
    }
  /* NetBSD uses a similar trick.  */
  else if (strcmp (name, ".note.netbsd.ident") == 0 && sectsize > 0)
    {
      unsigned int name_length, desc_length, note_type;
      char *note;

      /* If the section is larger than this, it's probably not what we are
	 looking for.  */
      if (sectsize > 128)
	sectsize = 128;

      note = alloca (sectsize);

      bfd_get_section_contents (abfd, sect, note,
                                (file_ptr) 0, (bfd_size_type) sectsize);

      name_length = bfd_h_get_32 (abfd, note);
      desc_length = bfd_h_get_32 (abfd, note + 4);
      note_type   = bfd_h_get_32 (abfd, note + 8);

      if (name_length == 7 && desc_length == 4 && note_type == 1
          && strcmp (note + 12, "NetBSD") == 0)
	/* XXX Should we check the version here?
	   Probably not necessary yet.  */
	*os_ident_ptr = ARM_ABI_NETBSD_ELF;
    }
}

/* Return one of the ELFOSABI_ constants for BFDs representing ELF
   executables.  If it's not an ELF executable or if the OS/ABI couldn't
   be determined, simply return -1.  */

static int
get_elfosabi (bfd *abfd)
{
  int elfosabi;
  enum arm_abi arm_abi = ARM_ABI_UNKNOWN;

  elfosabi = elf_elfheader (abfd)->e_ident[EI_OSABI];

  /* When elfosabi is 0 (ELFOSABI_NONE), this is supposed to indicate
     that we're on a SYSV system.  However, GNU/Linux uses a note section
     to record OS/ABI info, but leaves e_ident[EI_OSABI] zero.  So we
     have to check the note sections too.

     GNU/ARM tools set the EI_OSABI field to ELFOSABI_ARM, so handle that
     as well.  */
  if (elfosabi == 0 || elfosabi == ELFOSABI_ARM)
    {
      bfd_map_over_sections (abfd,
			     process_note_abi_tag_sections,
			     &arm_abi);
    }

  if (arm_abi != ARM_ABI_UNKNOWN)
    return arm_abi;

  switch (elfosabi)
    {
    case ELFOSABI_NONE:
      /* Existing ARM Tools don't set this field, so look at the EI_FLAGS
	 field for more information.  */

      switch (EF_ARM_EABI_VERSION(elf_elfheader(abfd)->e_flags))
	{
	case EF_ARM_EABI_VER1:
	  return ARM_ABI_EABI_V1;

	case EF_ARM_EABI_VER2:
	  return ARM_ABI_EABI_V2;

	case EF_ARM_EABI_UNKNOWN:
	  /* Assume GNU tools.  */
	  return ARM_ABI_APCS;

	default:
	  internal_error (__FILE__, __LINE__,
			  "get_elfosabi: Unknown ARM EABI version 0x%lx",
			  EF_ARM_EABI_VERSION(elf_elfheader(abfd)->e_flags));

	}
      break;

    case ELFOSABI_NETBSD:
      return ARM_ABI_NETBSD_ELF;

    case ELFOSABI_FREEBSD:
      return ARM_ABI_FREEBSD;

    case ELFOSABI_LINUX:
      return ARM_ABI_LINUX;

    case ELFOSABI_ARM:
      /* Assume GNU tools with the old APCS abi.  */
      return ARM_ABI_APCS;

    default:
    }

  return ARM_ABI_UNKNOWN;
}

struct arm_abi_handler
{
  struct arm_abi_handler *next;
  enum arm_abi abi;
  void (*init_abi)(struct gdbarch_info, struct gdbarch *);
};

struct arm_abi_handler *arm_abi_handler_list = NULL;

void
arm_gdbarch_register_os_abi (enum arm_abi abi,
			     void (*init_abi)(struct gdbarch_info,
					      struct gdbarch *))
{
  struct arm_abi_handler **handler_p;

  for (handler_p = &arm_abi_handler_list; *handler_p != NULL;
       handler_p = &(*handler_p)->next)
    {
      if ((*handler_p)->abi == abi)
	{
	  internal_error
	    (__FILE__, __LINE__,
	     "arm_gdbarch_register_os_abi: A handler for this ABI variant (%d)"
	     " has already been registered", (int)abi);
	  /* If user wants to continue, override previous definition.  */
	  (*handler_p)->init_abi = init_abi;
	  return;
	}
    }

  (*handler_p)
    = (struct arm_abi_handler *) xmalloc (sizeof (struct arm_abi_handler));
  (*handler_p)->next = NULL;
  (*handler_p)->abi = abi;
  (*handler_p)->init_abi = init_abi;
}

/* Initialize the current architecture based on INFO.  If possible, re-use an
   architecture from ARCHES, which is a list of architectures already created
   during this debugging session.

   Called e.g. at program startup, when reading a core file, and when reading
   a binary file.  */

static struct gdbarch *
arm_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  enum arm_abi arm_abi = ARM_ABI_UNKNOWN;
  struct arm_abi_handler *abi_handler;

  /* Try to deterimine the ABI of the object we are loading.  */

  if (info.abfd != NULL)
    {
      switch (bfd_get_flavour (info.abfd))
	{
	case bfd_target_elf_flavour:
	  arm_abi = get_elfosabi (info.abfd);
	  break;

	case bfd_target_aout_flavour:
	  if (strcmp (bfd_get_target(info.abfd), "a.out-arm-netbsd") == 0)
	    arm_abi = ARM_ABI_NETBSD_AOUT;
	  else
	    /* Assume it's an old APCS-style ABI.  */
	    arm_abi = ARM_ABI_APCS;
	  break;

	case bfd_target_coff_flavour:
	  /* Assume it's an old APCS-style ABI.  */
	  /* XXX WinCE?  */
	  arm_abi = ARM_ABI_APCS;
	  break;

	default:
	  /* Not sure what to do here, leave the ABI as unknown.  */
	  break;
	}
    }

  /* Find a candidate among extant architectures.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      /* Make sure the ABI selection matches.  */
      tdep = gdbarch_tdep (arches->gdbarch);
      if (tdep && tdep->arm_abi == arm_abi)
	return arches->gdbarch;
    }

  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  tdep->arm_abi = arm_abi;
  if (arm_abi < ARM_ABI_INVALID)
    tdep->abi_name = arm_abi_names[arm_abi];
  else
    {
      internal_error (__FILE__, __LINE__, "Invalid setting of arm_abi %d",
		      (int) arm_abi);
      tdep->abi_name = "<invalid>";
    }

  /* This is the way it has always defaulted.  */
  tdep->fp_model = ARM_FLOAT_FPA;

  /* Breakpoints.  */
  switch (info.byte_order)
    {
    case BFD_ENDIAN_BIG:
      tdep->arm_breakpoint = arm_default_arm_be_breakpoint;
      tdep->arm_breakpoint_size = sizeof (arm_default_arm_be_breakpoint);
      tdep->thumb_breakpoint = arm_default_thumb_be_breakpoint;
      tdep->thumb_breakpoint_size = sizeof (arm_default_thumb_be_breakpoint);

      break;

    case BFD_ENDIAN_LITTLE:
      tdep->arm_breakpoint = arm_default_arm_le_breakpoint;
      tdep->arm_breakpoint_size = sizeof (arm_default_arm_le_breakpoint);
      tdep->thumb_breakpoint = arm_default_thumb_le_breakpoint;
      tdep->thumb_breakpoint_size = sizeof (arm_default_thumb_le_breakpoint);

      break;

    default:
      internal_error (__FILE__, __LINE__,
		      "arm_gdbarch_init: bad byte order for float format");
    }

  /* On ARM targets char defaults to unsigned.  */
  set_gdbarch_char_signed (gdbarch, 0);

  /* This should be low enough for everything.  */
  tdep->lowest_pc = 0x20;
  tdep->jb_pc = -1; /* Longjump support not enabled by default.  */

  set_gdbarch_use_generic_dummy_frames (gdbarch, 0);

  /* Call dummy code.  */
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 1);
  /* We have to give this a value now, even though we will re-set it 
     during each call to arm_fix_call_dummy.  */
  set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 8);
  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);

  set_gdbarch_call_dummy_words (gdbarch, arm_call_dummy_words);
  set_gdbarch_sizeof_call_dummy_words (gdbarch, sizeof (arm_call_dummy_words));
  set_gdbarch_call_dummy_start_offset (gdbarch, 0);
  set_gdbarch_call_dummy_length (gdbarch, 0);

  set_gdbarch_fix_call_dummy (gdbarch, arm_fix_call_dummy);

  set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_on_stack);

  set_gdbarch_get_saved_register (gdbarch, generic_get_saved_register);
  set_gdbarch_push_arguments (gdbarch, arm_push_arguments);
  set_gdbarch_coerce_float_to_double (gdbarch,
				      standard_coerce_float_to_double);

  /* Frame handling.  */
  set_gdbarch_frame_chain_valid (gdbarch, arm_frame_chain_valid);
  set_gdbarch_init_extra_frame_info (gdbarch, arm_init_extra_frame_info);
  set_gdbarch_read_fp (gdbarch, arm_read_fp);
  set_gdbarch_frame_chain (gdbarch, arm_frame_chain);
  set_gdbarch_frameless_function_invocation
    (gdbarch, arm_frameless_function_invocation);
  set_gdbarch_frame_saved_pc (gdbarch, arm_frame_saved_pc);
  set_gdbarch_frame_args_address (gdbarch, arm_frame_args_address);
  set_gdbarch_frame_locals_address (gdbarch, arm_frame_locals_address);
  set_gdbarch_frame_num_args (gdbarch, arm_frame_num_args);
  set_gdbarch_frame_args_skip (gdbarch, 0);
  set_gdbarch_frame_init_saved_regs (gdbarch, arm_frame_init_saved_regs);
  set_gdbarch_push_dummy_frame (gdbarch, arm_push_dummy_frame);
  set_gdbarch_pop_frame (gdbarch, arm_pop_frame);

  /* Address manipulation.  */
  set_gdbarch_smash_text_address (gdbarch, arm_smash_text_address);
  set_gdbarch_addr_bits_remove (gdbarch, arm_addr_bits_remove);

  /* Offset from address of function to start of its code.  */
  set_gdbarch_function_start_offset (gdbarch, 0);

  /* Advance PC across function entry code.  */
  set_gdbarch_skip_prologue (gdbarch, arm_skip_prologue);

  /* Get the PC when a frame might not be available.  */
  set_gdbarch_saved_pc_after_call (gdbarch, arm_saved_pc_after_call);

  /* The stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Breakpoint manipulation.  */
  set_gdbarch_breakpoint_from_pc (gdbarch, arm_breakpoint_from_pc);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);

  /* Information about registers, etc.  */
  set_gdbarch_print_float_info (gdbarch, arm_print_float_info);
  set_gdbarch_fp_regnum (gdbarch, ARM_FP_REGNUM); /* ??? */
  set_gdbarch_sp_regnum (gdbarch, ARM_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, ARM_PC_REGNUM);
  set_gdbarch_register_byte (gdbarch, arm_register_byte);
  set_gdbarch_register_bytes (gdbarch,
			      (NUM_GREGS * INT_REGISTER_RAW_SIZE
			       + NUM_FREGS * FP_REGISTER_RAW_SIZE
			       + NUM_SREGS * STATUS_REGISTER_SIZE));
  set_gdbarch_num_regs (gdbarch, NUM_GREGS + NUM_FREGS + NUM_SREGS);
  set_gdbarch_register_raw_size (gdbarch, arm_register_raw_size);
  set_gdbarch_register_virtual_size (gdbarch, arm_register_virtual_size);
  set_gdbarch_max_register_raw_size (gdbarch, FP_REGISTER_RAW_SIZE);
  set_gdbarch_max_register_virtual_size (gdbarch, FP_REGISTER_VIRTUAL_SIZE);
  set_gdbarch_register_virtual_type (gdbarch, arm_register_type);

  /* Integer registers are 4 bytes.  */
  set_gdbarch_register_size (gdbarch, 4);
  set_gdbarch_register_name (gdbarch, arm_register_name);

  /* Returning results.  */
  set_gdbarch_extract_return_value (gdbarch, arm_extract_return_value);
  set_gdbarch_store_return_value (gdbarch, arm_store_return_value);
  set_gdbarch_store_struct_return (gdbarch, arm_store_struct_return);
  set_gdbarch_use_struct_convention (gdbarch, arm_use_struct_convention);
  set_gdbarch_extract_struct_value_address (gdbarch,
					    arm_extract_struct_value_address);

  /* Single stepping.  */
  /* XXX For an RDI target we should ask the target if it can single-step.  */
  set_gdbarch_software_single_step (gdbarch, arm_software_single_step);

  /* Minsymbol frobbing.  */
  set_gdbarch_elf_make_msymbol_special (gdbarch, arm_elf_make_msymbol_special);
  set_gdbarch_coff_make_msymbol_special (gdbarch,
					 arm_coff_make_msymbol_special);

  /* Hook in the ABI-specific overrides, if they have been registered.  */
  if (arm_abi == ARM_ABI_UNKNOWN)
    {
      /* Don't complain about not knowing the ABI variant if we don't 
	 have an inferior.  */
      if (info.abfd)
	fprintf_filtered
	  (gdb_stderr, "GDB doesn't recognize the ABI of the inferior.  "
	   "Attempting to continue with the default ARM settings");
    }
  else
    {
      for (abi_handler = arm_abi_handler_list; abi_handler != NULL;
	   abi_handler = abi_handler->next)
	if (abi_handler->abi == arm_abi)
	  break;

      if (abi_handler)
	abi_handler->init_abi (info, gdbarch);
      else
	{
	  /* We assume that if GDB_MULTI_ARCH is less than 
	     GDB_MULTI_ARCH_TM that an ABI variant can be supported by
	     overriding definitions in this file.  */
	  if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL)
	    fprintf_filtered
	      (gdb_stderr,
	       "A handler for the ABI variant \"%s\" is not built into this "
	       "configuration of GDB.  "
	       "Attempting to continue with the default ARM settings",
	       arm_abi_names[arm_abi]);
	}
    }

  /* Now we have tuned the configuration, set a few final things,
     based on what the OS ABI has told us.  */

  if (tdep->jb_pc >= 0)
    set_gdbarch_get_longjmp_target (gdbarch, arm_get_longjmp_target);

  /* Floating point sizes and format.  */
  switch (info.byte_order)
    {
    case BFD_ENDIAN_BIG:
      set_gdbarch_float_format (gdbarch, &floatformat_ieee_single_big);
      set_gdbarch_double_format (gdbarch, &floatformat_ieee_double_big);
      set_gdbarch_long_double_format (gdbarch, &floatformat_ieee_double_big);
      
      break;

    case BFD_ENDIAN_LITTLE:
      set_gdbarch_float_format (gdbarch, &floatformat_ieee_single_little);
      if (tdep->fp_model == ARM_FLOAT_VFP
	  || tdep->fp_model == ARM_FLOAT_SOFT_VFP)
	{
	  set_gdbarch_double_format (gdbarch, &floatformat_ieee_double_little);
	  set_gdbarch_long_double_format (gdbarch,
					  &floatformat_ieee_double_little);
	}
      else
	{
	  set_gdbarch_double_format
	    (gdbarch, &floatformat_ieee_double_littlebyte_bigword);
	  set_gdbarch_long_double_format
	    (gdbarch, &floatformat_ieee_double_littlebyte_bigword);
	}
      break;

    default:
      internal_error (__FILE__, __LINE__,
		      "arm_gdbarch_init: bad byte order for float format");
    }

  /* We can't use SIZEOF_FRAME_SAVED_REGS here, since that still
     references the old architecture vector, not the one we are
     building here.  */
  if (prologue_cache.saved_regs != NULL)
    xfree (prologue_cache.saved_regs);

  prologue_cache.saved_regs = (CORE_ADDR *)
    xcalloc (1, (sizeof (CORE_ADDR)
		 * (gdbarch_num_regs (gdbarch) + NUM_PSEUDO_REGS)));

  return gdbarch;
}

static void
arm_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep == NULL)
    return;

  if (tdep->abi_name != NULL)
    fprintf_unfiltered (file, "arm_dump_tdep: ABI = %s\n", tdep->abi_name);
  else
    internal_error (__FILE__, __LINE__,
		    "arm_dump_tdep: illegal setting of tdep->arm_abi (%d)",
		    (int) tdep->arm_abi);

  fprintf_unfiltered (file, "arm_dump_tdep: Lowest pc = 0x%lx",
		      (unsigned long) tdep->lowest_pc);
}

static void
arm_init_abi_eabi_v1 (struct gdbarch_info info,
		      struct gdbarch *gdbarch)
{
  /* Place-holder.  */
}

static void
arm_init_abi_eabi_v2 (struct gdbarch_info info,
		      struct gdbarch *gdbarch)
{
  /* Place-holder.  */
}

static void
arm_init_abi_apcs (struct gdbarch_info info,
		   struct gdbarch *gdbarch)
{
  /* Place-holder.  */
}

void
_initialize_arm_tdep (void)
{
  struct ui_file *stb;
  long length;
  struct cmd_list_element *new_cmd;
  const char *setname;
  const char *setdesc;
  const char **regnames;
  int numregs, i, j;
  static char *helptext;

  if (GDB_MULTI_ARCH)
    gdbarch_register (bfd_arch_arm, arm_gdbarch_init, arm_dump_tdep);

  /* Register some ABI variants for embedded systems.  */
  arm_gdbarch_register_os_abi (ARM_ABI_EABI_V1, arm_init_abi_eabi_v1);
  arm_gdbarch_register_os_abi (ARM_ABI_EABI_V2, arm_init_abi_eabi_v2);
  arm_gdbarch_register_os_abi (ARM_ABI_APCS, arm_init_abi_apcs);

  tm_print_insn = gdb_print_insn_arm;

  /* Get the number of possible sets of register names defined in opcodes. */
  num_flavor_options = get_arm_regname_num_options ();

  /* Sync the opcode insn printer with our register viewer: */
  parse_arm_disassembler_option ("reg-names-std");

  /* Begin creating the help text. */
  stb = mem_fileopen ();
  fprintf_unfiltered (stb, "Set the disassembly flavor.\n\
The valid values are:\n");

  /* Initialize the array that will be passed to add_set_enum_cmd(). */
  valid_flavors = xmalloc ((num_flavor_options + 1) * sizeof (char *));
  for (i = 0; i < num_flavor_options; i++)
    {
      numregs = get_arm_regnames (i, &setname, &setdesc, &regnames);
      valid_flavors[i] = setname;
      fprintf_unfiltered (stb, "%s - %s\n", setname,
			  setdesc);
      /* Copy the default names (if found) and synchronize disassembler. */
      if (!strcmp (setname, "std"))
	{
          disassembly_flavor = setname;
          current_option = i;
	  for (j = 0; j < numregs; j++)
            arm_register_names[j] = (char *) regnames[j];
          set_arm_regname_option (i);
	}
    }
  /* Mark the end of valid options. */
  valid_flavors[num_flavor_options] = NULL;

  /* Finish the creation of the help text. */
  fprintf_unfiltered (stb, "The default is \"std\".");
  helptext = ui_file_xstrdup (stb, &length);
  ui_file_delete (stb);

  /* Add the disassembly-flavor command */
  new_cmd = add_set_enum_cmd ("disassembly-flavor", no_class,
			      valid_flavors,
			      &disassembly_flavor,
			      helptext,
			      &setlist);
  set_cmd_sfunc (new_cmd, set_disassembly_flavor_sfunc);
  add_show_from_set (new_cmd, &showlist);

  /* ??? Maybe this should be a boolean.  */
  add_show_from_set (add_set_cmd ("apcs32", no_class,
				  var_zinteger, (char *) &arm_apcs_32,
				  "Set usage of ARM 32-bit mode.\n", &setlist),
		     &showlist);

  /* Add the deprecated "othernames" command */

  add_com ("othernames", class_obscure, arm_othernames,
	   "Switch to the next set of register names.");

  /* Fill in the prologue_cache fields.  */
  prologue_cache.saved_regs = NULL;
  prologue_cache.extra_info = (struct frame_extra_info *)
    xcalloc (1, sizeof (struct frame_extra_info));
}
