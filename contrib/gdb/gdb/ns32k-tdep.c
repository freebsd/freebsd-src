/* Print NS 32000 instructions for GDB, the GNU debugger.
   Copyright 1986, 1988, 1991, 1992, 1994, 1995, 1998, 1999, 2000, 2001
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "gdbcore.h"

static int sign_extend (int value, int bits);

void
_initialize_ns32k_tdep (void)
{
  tm_print_insn = print_insn_ns32k;
}

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

CORE_ADDR
umax_skip_prologue (CORE_ADDR pc)
{
  register unsigned char op = read_memory_integer (pc, 1);
  if (op == 0x82)
    {
      op = read_memory_integer (pc + 2, 1);
      if ((op & 0x80) == 0)
	pc += 3;
      else if ((op & 0xc0) == 0x80)
	pc += 4;
      else
	pc += 6;
    }
  return pc;
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.
   Encore's C compiler often reuses same area on stack for args,
   so this will often not work properly.  If the arg names
   are known, it's likely most of them will be printed. */

int
umax_frame_num_args (struct frame_info *fi)
{
  int numargs;
  CORE_ADDR pc;
  CORE_ADDR enter_addr;
  unsigned int insn;
  unsigned int addr_mode;
  int width;

  numargs = -1;
  enter_addr = ns32k_get_enter_addr ((fi)->pc);
  if (enter_addr > 0)
    {
      pc = ((enter_addr == 1)
	    ? SAVED_PC_AFTER_CALL (fi)
	    : FRAME_SAVED_PC (fi));
      insn = read_memory_integer (pc, 2);
      addr_mode = (insn >> 11) & 0x1f;
      insn = insn & 0x7ff;
      if ((insn & 0x7fc) == 0x57c
	  && addr_mode == 0x14)	/* immediate */
	{
	  if (insn == 0x57c)	/* adjspb */
	    width = 1;
	  else if (insn == 0x57d)	/* adjspw */
	    width = 2;
	  else if (insn == 0x57f)	/* adjspd */
	    width = 4;
	  else
	    internal_error (__FILE__, __LINE__, "bad else");
	  numargs = read_memory_integer (pc + 2, width);
	  if (width > 1)
	    flip_bytes (&numargs, width);
	  numargs = -sign_extend (numargs, width * 8) / 4;
	}
    }
  return numargs;
}

static int
sign_extend (int value, int bits)
{
  value = value & ((1 << bits) - 1);
  return (value & (1 << (bits - 1))
	  ? value | (~((1 << bits) - 1))
	  : value);
}

void
flip_bytes (void *p, int count)
{
  char tmp;
  char *ptr = 0;

  while (count > 0)
    {
      tmp = *ptr;
      ptr[0] = ptr[count - 1];
      ptr[count - 1] = tmp;
      ptr++;
      count -= 2;
    }
}

/* Return the number of locals in the current frame given a pc
   pointing to the enter instruction.  This is used in the macro
   FRAME_FIND_SAVED_REGS.  */

int
ns32k_localcount (CORE_ADDR enter_pc)
{
  unsigned char localtype;
  int localcount;

  localtype = read_memory_integer (enter_pc + 2, 1);
  if ((localtype & 0x80) == 0)
    localcount = localtype;
  else if ((localtype & 0xc0) == 0x80)
    localcount = (((localtype & 0x3f) << 8)
		  | (read_memory_integer (enter_pc + 3, 1) & 0xff));
  else
    localcount = (((localtype & 0x3f) << 24)
		  | ((read_memory_integer (enter_pc + 3, 1) & 0xff) << 16)
		  | ((read_memory_integer (enter_pc + 4, 1) & 0xff) << 8)
		  | (read_memory_integer (enter_pc + 5, 1) & 0xff));
  return localcount;
}


/* Nonzero if instruction at PC is a return instruction.  */

static int
ns32k_about_to_return (CORE_ADDR pc)
{
  return (read_memory_integer (pc, 1) == 0x12);
}


/*
 * Get the address of the enter opcode for the function
 * containing PC, if there is an enter for the function,
 * and if the pc is between the enter and exit.
 * Returns positive address if pc is between enter/exit,
 * 1 if pc before enter or after exit, 0 otherwise.
 */

CORE_ADDR
ns32k_get_enter_addr (CORE_ADDR pc)
{
  CORE_ADDR enter_addr;
  unsigned char op;

  if (pc == 0)
    return 0;

  if (ns32k_about_to_return (pc))
    return 1;			/* after exit */

  enter_addr = get_pc_function_start (pc);

  if (pc == enter_addr)
    return 1;			/* before enter */

  op = read_memory_integer (enter_addr, 1);

  if (op != 0x82)
    return 0;			/* function has no enter/exit */

  return enter_addr;		/* pc is between enter and exit */
}
