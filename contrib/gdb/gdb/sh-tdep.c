/* Target-dependent code for Hitachi Super-H, for GDB.
   Copyright (C) 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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

/*
 Contributed by Steve Chamberlain
                sac@cygnus.com
 */

#include "defs.h"
#include "frame.h"
#include "obstack.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "value.h"
#include "dis-asm.h"

/* Default to the original SH.  */

#define DEFAULT_SH_TYPE "sh"

/* This value is the model of SH in use.  */

char *sh_processor_type;

char *tmp_sh_processor_type;

/* A set of original names, to be used when restoring back to generic
   registers from a specific set.  */

char *sh_generic_reg_names[] = REGISTER_NAMES;

char *sh_reg_names[] = {
  "r0", "r1", "r2",  "r3",  "r4",  "r5",   "r6",  "r7",
  "r8", "r9", "r10", "r11", "r12", "r13",  "r14", "r15",
  "pc", "pr", "gbr", "vbr", "mach","macl", "sr",
  "fpul", "fpscr",
  "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", ""
};

char *sh3_reg_names[] = {
  "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
  "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
  "pc",  "pr",  "gbr", "vbr", "mach","macl","sr",
  "fpul", "fpscr",
  "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
  "fr8", "fr9", "fr10","fr11","fr12","fr13","fr14","fr15",
  "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0",
  "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1"
};

struct {
  char *name;
  char **regnames;
} sh_processor_type_table[] = {
  { "sh", sh_reg_names },
  { "sh3", sh3_reg_names },
  { NULL, NULL }
};

/* Prologue looks like
   [mov.l	<regs>,@-r15]...
   [sts.l	pr,@-r15]
   [mov.l	r14,@-r15]
   [mov		r15,r14]
*/

#define IS_STS(x)  		((x) == 0x4f22)
#define IS_PUSH(x) 		(((x) & 0xff0f) == 0x2f06)
#define GET_PUSHED_REG(x)  	(((x) >> 4) & 0xf)
#define IS_MOV_SP_FP(x)  	((x) == 0x6ef3)
#define IS_ADD_SP(x) 		(((x) & 0xff00) == 0x7f00)
#define IS_MOV_R3(x) 		(((x) & 0xff00) == 0x1a00)
#define IS_SHLL_R3(x)		((x) == 0x4300)
#define IS_ADD_R3SP(x)		((x) == 0x3f3c)

/* Skip any prologue before the guts of a function */

CORE_ADDR
sh_skip_prologue (start_pc)
     CORE_ADDR start_pc;
{
  int w;

  w = read_memory_integer (start_pc, 2);
  while (IS_STS (w)
	 || IS_PUSH (w)
	 || IS_MOV_SP_FP (w)
	 || IS_MOV_R3 (w)
	 || IS_ADD_R3SP (w)
	 || IS_ADD_SP (w)
	 || IS_SHLL_R3 (w))
    {
      start_pc += 2;
      w = read_memory_integer (start_pc, 2);
    }

  return start_pc;
}

/* Disassemble an instruction.  */

int
gdb_print_insn_sh (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    return print_insn_sh (memaddr, info);
  else
    return print_insn_shl (memaddr, info);
}

/* Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.

   For us, the frame address is its stack pointer value, so we look up
   the function prologue to determine the caller's sp value, and return it.  */

CORE_ADDR
sh_frame_chain (frame)
     struct frame_info *frame;
{
  if (!inside_entry_file (frame->pc))
    return read_memory_integer (FRAME_FP (frame) + frame->f_offset, 4);
  else
    return 0;
}

/* Put here the code to store, into a struct frame_saved_regs, the
   addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special: the address we
   return for it IS the sp for the next frame. */

void
frame_find_saved_regs (fi, fsr)
     struct frame_info *fi;
     struct frame_saved_regs *fsr;
{
  int where[NUM_REGS];
  int rn;
  int have_fp = 0;
  int depth;
  int pc;
  int opc;
  int insn;
  int r3_val = 0;

  opc = pc = get_pc_function_start (fi->pc);

  insn = read_memory_integer (pc, 2);

  fi->leaf_function = 1;
  fi->f_offset = 0;

  for (rn = 0; rn < NUM_REGS; rn++)
    where[rn] = -1;

  depth = 0;

  /* Loop around examining the prologue insns, but give up
     after 15 of them, since we're getting silly then */
  while (pc < opc + 15 * 2)
    {
      /* See where the registers will be saved to */
      if (IS_PUSH (insn))
	{
	  pc += 2;
	  rn = GET_PUSHED_REG (insn);
	  where[rn] = depth;
	  insn = read_memory_integer (pc, 2);
	  depth += 4;
	}
      else if (IS_STS (insn))
	{
	  pc += 2;
	  where[PR_REGNUM] = depth;
	  insn = read_memory_integer (pc, 2);
	  /* If we're storing the pr then this isn't a leaf */
	  fi->leaf_function = 0;
	  depth += 4;
	}
      else if (IS_MOV_R3 (insn))
	{
	  r3_val = (char) (insn & 0xff);
	  pc += 2;
	  insn = read_memory_integer (pc, 2);
	}
      else if (IS_SHLL_R3 (insn))
	{
	  r3_val <<= 1;
	  pc += 2;
	  insn = read_memory_integer (pc, 2);
	}
      else if (IS_ADD_R3SP (insn))
	{
	  depth += -r3_val;
	  pc += 2;
	  insn = read_memory_integer (pc, 2);
	}
      else if (IS_ADD_SP (insn))
	{
	  pc += 2;
	  depth += -((char) (insn & 0xff));
	  insn = read_memory_integer (pc, 2);
	}
      else
	break;
    }

  /* Now we know how deep things are, we can work out their addresses */

  for (rn = 0; rn < NUM_REGS; rn++)
    {
      if (where[rn] >= 0)
	{
	  if (rn == FP_REGNUM)
	    have_fp = 1;

	  fsr->regs[rn] = fi->frame - where[rn] + depth - 4;
	}
      else
	{
	  fsr->regs[rn] = 0;
	}
    }

  if (have_fp)
    {
      fsr->regs[SP_REGNUM] = read_memory_integer (fsr->regs[FP_REGNUM], 4);
    }
  else
    {
      fsr->regs[SP_REGNUM] = fi->frame - 4;
    }

  fi->f_offset = depth - where[FP_REGNUM] - 4;
  /* Work out the return pc - either from the saved pr or the pr
     value */

  if (fsr->regs[PR_REGNUM])
    fi->return_pc = read_memory_integer (fsr->regs[PR_REGNUM], 4);
  else
    fi->return_pc = read_register (PR_REGNUM);
}

/* initialize the extra info saved in a FRAME */

void
init_extra_frame_info (fromleaf, fi)
     int fromleaf;
     struct frame_info *fi;
{
  struct frame_saved_regs dummy;

  if (fi->next)
    fi->pc = fi->next->return_pc;

  frame_find_saved_regs (fi, &dummy);
}


/* Discard from the stack the innermost frame,
   restoring all saved registers.  */

void
pop_frame ()
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR fp;
  register int regnum;
  struct frame_saved_regs fsr;

  fp = FRAME_FP (frame);
  get_frame_saved_regs (frame, &fsr);

  /* Copy regs from where they were saved in the frame */
  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      if (fsr.regs[regnum])
	{
	  write_register (regnum, read_memory_integer (fsr.regs[regnum], 4));
	}
    }

  write_register (PC_REGNUM, frame->return_pc);
  write_register (SP_REGNUM, fp + 4);
  flush_cached_frames ();
}

/* Command to set the processor type.  */

void
sh_set_processor_type_command (args, from_tty)
     char *args;
     int from_tty;
{
  int i;
  char *temp;

  /* The `set' commands work by setting the value, then calling the hook,
     so we let the general command modify a scratch location, then decide
     here if we really want to modify the processor type.  */
  if (tmp_sh_processor_type == NULL || *tmp_sh_processor_type == '\0')
    {
      printf_unfiltered ("The known SH processor types are as follows:\n\n");
      for (i = 0; sh_processor_type_table[i].name != NULL; ++i)
	printf_unfiltered ("%s\n", sh_processor_type_table[i].name);

      /* Restore the value.  */
      tmp_sh_processor_type = strsave (sh_processor_type);

      return;
    }
  
  if (!sh_set_processor_type (tmp_sh_processor_type))
    {
      /* Restore to a valid value before erroring out.  */
      temp = tmp_sh_processor_type;
      tmp_sh_processor_type = strsave (sh_processor_type);
      error ("Unknown processor type `%s'.", temp);
    }
}

static void
sh_show_processor_type_command (args, from_tty)
     char *args;
     int from_tty;
{
}

/* Modify the actual processor type. */

int
sh_set_processor_type (str)
     char *str;
{
  int i, j;

  if (str == NULL)
    return 0;

  for (i = 0; sh_processor_type_table[i].name != NULL; ++i)
    {
      if (strcasecmp (str, sh_processor_type_table[i].name) == 0)
	{
	  sh_processor_type = str;

	  for (j = 0; j < NUM_REGS; ++j)
	    reg_names[j] = sh_processor_type_table[i].regnames[j];

	  return 1;
	}
    }

  return 0;
}

/* Print the registers in a form similar to the E7000 */

static void
show_regs (args, from_tty)
     char *args;
     int from_tty;
{
  printf_filtered ("PC=%08x SR=%08x PR=%08x MACH=%08x MACHL=%08x\n",
		   read_register (PC_REGNUM),
		   read_register (SR_REGNUM),
		   read_register (PR_REGNUM),
		   read_register (MACH_REGNUM),
		   read_register (MACL_REGNUM));

  printf_filtered ("R0-R7  %08x %08x %08x %08x %08x %08x %08x %08x\n",
		   read_register (0),
		   read_register (1),
		   read_register (2),
		   read_register (3),
		   read_register (4),
		   read_register (5),
		   read_register (6),
		   read_register (7));
  printf_filtered ("R8-R15 %08x %08x %08x %08x %08x %08x %08x %08x\n",
		   read_register (8),
		   read_register (9),
		   read_register (10),
		   read_register (11),
		   read_register (12),
		   read_register (13),
		   read_register (14),
		   read_register (15));
}

void
_initialize_sh_tdep ()
{
  struct cmd_list_element *c;

  tm_print_insn = gdb_print_insn_sh;

  c = add_set_cmd ("processor", class_support, var_string_noescape,
		   (char *) &tmp_sh_processor_type,
		   "Set the type of SH processor in use.\n\
Set this to be able to access processor-type-specific registers.\n\
",
		   &setlist);
  c->function.cfunc = sh_set_processor_type_command;
  c = add_show_from_set (c, &showlist);
  c->function.cfunc = sh_show_processor_type_command;

  tmp_sh_processor_type = strsave (DEFAULT_SH_TYPE);
  sh_set_processor_type_command (strsave (DEFAULT_SH_TYPE), 0);

  add_com ("regs", class_vars, show_regs, "Print all registers");
}
