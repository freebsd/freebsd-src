/* Target-machine dependent code for WDC-65816, for GDB.
   Copyright (C) 1995 Free Software Foundation, Inc.

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
#include "gdbcmd.h"
#include "gdbtypes.h"
#include "dis-asm.h"


/* Return the saved PC from this frame. */


CORE_ADDR
w65_frame_saved_pc (frame)
     struct frame_info *frame;
{
  return (read_memory_integer (frame->frame + 2, 4) & 0xffffff);
}

CORE_ADDR
addr_bits_remove (x)
     CORE_ADDR x;
{
  return x;
}

read_memory_pointer (x)
     CORE_ADDR x;
{
  return read_memory_integer (ADDR_BITS_REMOVE (x), 4);
}

init_frame_pc ()
{
  abort ();
}

void
w65_push_dummy_frame ()
{
  abort ();
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.

   We cache the result of doing this in the frame_cache_obstack, since
   it is fairly expensive.  */

void
frame_find_saved_regs (fip, fsrp)
     struct frame_info *fip;
     struct frame_saved_regs *fsrp;
{
  int locals;
  CORE_ADDR pc;
  CORE_ADDR adr;
  int i;

  memset (fsrp, 0, sizeof *fsrp);
}

int
saved_pc_after_call ()
{
  int sp = read_register (SP_REGNUM);
  int val = read_memory_integer (sp + 1, 4);
  return ADDR_BITS_REMOVE (val);
}


extract_return_value (type, regbuf, valbuf)
     struct type *type;
     char *regbuf;
     char *valbuf;
{
  int b;
  int len = TYPE_LENGTH (type);

  for (b = 0; b < len; b += 2)
    {
      int todo = len - b;
      if (todo > 2)
	todo = 2;
      memcpy (valbuf + b, regbuf + b, todo);
    }
}

void
write_return_value (type, valbuf)
     struct type *type;
     char *valbuf;
{
  int reg;
  int len;
  for (len = 0; len < TYPE_LENGTH (type); len += 2)
    {
      write_register_bytes (REGISTER_BYTE (len / 2 + 2), valbuf + len, 2);
    }
}

void
store_struct_return (addr, sp)
     CORE_ADDR addr;
     CORE_ADDR sp;
{
  write_register (2, addr);
}

void
w65_pop_frame ()
{
}

init_extra_frame_info ()
{
}

pop_frame ()
{
}

w65_frame_chain (thisframe)
     struct frame_info *thisframe;
{
  return 0xffff & read_memory_integer ((thisframe)->frame, 2);
}

static int
gb (x)
{
  return read_memory_integer (x, 1) & 0xff;
}

extern CORE_ADDR 
w65_skip_prologue (pc)
     CORE_ADDR pc;
{
  CORE_ADDR too_far = pc + 20;

  /* looking for bits of the prologue, we can expect to
     see this in a frameful function:

     stack adjust:

     3B                 tsc
     1A                 inc a
     18                 clc
     69E2FF             adc     #0xffe2
     3A                 dec a
     1B                 tcs
     1A                 inc a

     link:

     A500               lda     <r15
     48                 pha
     3B                 tsc
     1a                 inc     a
     8500               sta     <r15

   */

#define TSC  0x3b
#define TCS  0x1b
#define INCA 0x1a
#define PHA  0x48
#define LDADIR 0xa5
#define STADIR 0x85

  /* Skip a stack adjust - any area between a tsc and tcs */
  if (gb (pc) == TSC)
    {
      while (pc < too_far && gb (pc) != TCS)
	{
	  pc++;
	}
      pc++;
      /* Skip a stupid inc a */
      if (gb (pc) == INCA)
	pc++;

    }
  /* Stack adjust can also be done with n pha's */
  while (gb (pc) == PHA)
    pc++;

  /* Skip a link - that's a ld/ph/tsc/inc/sta */

  if (gb (pc) == LDADIR
      && gb (pc + 5) == STADIR
      && gb (pc + 1) == gb (pc + 6)
      && gb (pc + 2) == PHA
      && gb (pc + 3) == TSC
      && gb (pc + 4) == INCA)
    {
      pc += 7;
    }

  return pc;
}


register_raw_size (n)
{
  return sim_reg_size (n);
}


void
print_register_hook (regno)
{
  if (regno == P_REGNUM)
    {
      /* CCR register */

      int C, Z, N, V, I, D, X, M;
      unsigned char b[1];
      unsigned char l;

      read_relative_register_raw_bytes (regno, b);
      l = b[0];
      printf_unfiltered ("\t");
      C = (l & 0x1) != 0;
      Z = (l & 0x2) != 0;
      I = (l & 0x4) != 0;
      D = (l & 0x8) != 0;
      X = (l & 0x10) != 0;
      M = (l & 0x20) != 0;
      V = (l & 0x40) != 0;
      N = (l & 0x80) != 0;

      printf_unfiltered ("N-%d ", N);
      printf_unfiltered ("V-%d ", V);
      printf_unfiltered ("M-%d ", M);
      printf_unfiltered ("X-%d ", X);
      printf_unfiltered ("D-%d ", D);
      printf_unfiltered ("I-%d ", I);
      printf_unfiltered ("Z-%d ", Z);
      printf_unfiltered ("C-%d ", C);
      if ((C | Z) == 0)
	printf_unfiltered ("u> ");
      if ((C | Z) == 1)
	printf_unfiltered ("u<= ");
      if ((C == 0))
	printf_unfiltered ("u>= ");
      if (C == 1)
	printf_unfiltered ("u< ");
      if (Z == 0)
	printf_unfiltered ("!= ");
      if (Z == 1)
	printf_unfiltered ("== ");
      if ((N ^ V) == 0)
	printf_unfiltered (">= ");
      if ((N ^ V) == 1)
	printf_unfiltered ("< ");
      if ((Z | (N ^ V)) == 0)
	printf_unfiltered ("> ");
      if ((Z | (N ^ V)) == 1)
	printf_unfiltered ("<= ");
    }
}

void
_initialize_w65_tdep ()
{
  tm_print_insn = print_insn_w65;
}
