/* Disassemble WDC 65816 instructions.
   Copyright 1995, 1998, 2000, 2001, 2002 Free Software Foundation, Inc.

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

#include <stdio.h>
#include "sysdep.h"
#define STATIC_TABLE
#define DEFINE_TABLE

#include "w65-opc.h"
#include "dis-asm.h"

static fprintf_ftype fpr;
static void *stream;
static struct disassemble_info *local_info;

static void print_operand PARAMS ((int, char *, unsigned int *));

#if 0
static char *lname[] = { "r0","r1","r2","r3","r4","r5","r6","r7","s0" };

static char *
findname (val)
     unsigned int val;
{
  if (val >= 0x10 && val <= 0x20)
    return lname[(val - 0x10) / 2];
  return 0;
}
#endif
static void
print_operand (lookup, format, args)
     int lookup;
     char *format;
     unsigned int *args;
{
  int val;
  int c;

  while (*format)
    {
      switch (c = *format++)
	{
	case '$':
	  val = args[(*format++) - '0'];
	  if (lookup)
	    {
#if 0
	      name = findname (val);
	      if (name)
		fpr (stream, "%s", name);
	      else
#endif
		local_info->print_address_func (val, local_info);
	    }
	  else
	    fpr (stream, "0x%x", val);

	  break;
	default:
	  fpr (stream, "%c", c);
	  break;
	}
    }
}

int
print_insn_w65 (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int status = 0;
  unsigned char insn[4];
  const struct opinfo *op;
  int i;
  int X = 0;
  int M = 0;
  int args[2];
  stream = info->stream;
  fpr = info->fprintf_func;
  local_info = info;
  for (i = 0; i < 4 && status == 0; i++)
    {
      status = info->read_memory_func (memaddr + i, insn + i, 1, info);
    }

  for (op = optable; op->val != insn[0]; op++)
    ;

  fpr (stream, "%s", op->name);

  /* Prepare all the posible operand values.  */
  {
    int size = 1;
    int asR_W65_ABS8 = insn[1];
    int asR_W65_ABS16 = (insn[2] << 8) + asR_W65_ABS8;
    int asR_W65_ABS24 = (insn[3] << 16) + asR_W65_ABS16;
    int asR_W65_PCR8 = ((char) (asR_W65_ABS8)) + memaddr + 2;
    int asR_W65_PCR16 = ((short) (asR_W65_ABS16)) + memaddr + 3;

    switch (op->amode)
      {
	DISASM ();
      }

    return size;
  }
}
