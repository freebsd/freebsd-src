/* Disassemble D10V instructions.
   Copyright 1996, 1997, 1998, 2000, 2001, 2005 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include <stdio.h>

#include "sysdep.h"
#include "opcode/d10v.h"
#include "dis-asm.h"

/* The PC wraps at 18 bits, except for the segment number,
   so use this mask to keep the parts we want.  */
#define PC_MASK	0x0303FFFF

static void
print_operand (struct d10v_operand *oper,
	       unsigned long insn,
	       struct d10v_opcode *op,
	       bfd_vma memaddr,
	       struct disassemble_info *info)
{
  int num, shift;

  if (oper->flags == OPERAND_ATMINUS)
    {
      (*info->fprintf_func) (info->stream, "@-");
      return;
    }
  if (oper->flags == OPERAND_MINUS)
    {
      (*info->fprintf_func) (info->stream, "-");
      return;
    }
  if (oper->flags == OPERAND_PLUS)
    {
      (*info->fprintf_func) (info->stream, "+");
      return;
    }
  if (oper->flags == OPERAND_ATSIGN)
    {
      (*info->fprintf_func) (info->stream, "@");
      return;
    }
  if (oper->flags == OPERAND_ATPAR)
    {
      (*info->fprintf_func) (info->stream, "@(");
      return;
    }

  shift = oper->shift;

  /* The LONG_L format shifts registers over by 15.  */
  if (op->format == LONG_L && (oper->flags & OPERAND_REG))
    shift += 15;

  num = (insn >> shift) & (0x7FFFFFFF >> (31 - oper->bits));

  if (oper->flags & OPERAND_REG)
    {
      int i;
      int match = 0;

      num += (oper->flags
	      & (OPERAND_GPR | OPERAND_FFLAG | OPERAND_CFLAG | OPERAND_CONTROL));
      if (oper->flags & (OPERAND_ACC0 | OPERAND_ACC1))
	num += num ? OPERAND_ACC1 : OPERAND_ACC0;
      for (i = 0; i < d10v_reg_name_cnt (); i++)
	{
	  if (num == (d10v_predefined_registers[i].value & ~ OPERAND_SP))
	    {
	      if (d10v_predefined_registers[i].pname)
		(*info->fprintf_func) (info->stream, "%s",
				       d10v_predefined_registers[i].pname);
	      else
		(*info->fprintf_func) (info->stream, "%s",
				       d10v_predefined_registers[i].name);
	      match = 1;
	      break;
	    }
	}
      if (match == 0)
	{
	  /* This would only get executed if a register was not in the
	     register table.  */
	  if (oper->flags & (OPERAND_ACC0 | OPERAND_ACC1))
	    (*info->fprintf_func) (info->stream, "a");
	  else if (oper->flags & OPERAND_CONTROL)
	    (*info->fprintf_func) (info->stream, "cr");
	  else if (oper->flags & OPERAND_REG)
	    (*info->fprintf_func) (info->stream, "r");
	  (*info->fprintf_func) (info->stream, "%d", num & REGISTER_MASK);
	}
    }
  else
    {
      /* Addresses are right-shifted by 2.  */
      if (oper->flags & OPERAND_ADDR)
	{
	  long max;
	  int neg = 0;

	  max = (1 << (oper->bits - 1));
	  if (num & max)
	    {
	      num = -num & ((1 << oper->bits) - 1);
	      neg = 1;
	    }
	  num = num << 2;
	  if (info->flags & INSN_HAS_RELOC)
	    (*info->print_address_func) (num & PC_MASK, info);
	  else
	    {
	      if (neg)
		(*info->print_address_func) ((memaddr - num) & PC_MASK, info);
	      else
		(*info->print_address_func) ((memaddr + num) & PC_MASK, info);
	    }
	}
      else
	{
	  if (oper->flags & OPERAND_SIGNED)
	    {
	      int max = (1 << (oper->bits - 1));
	      if (num & max)
		{
		  num = -num & ((1 << oper->bits) - 1);
		  (*info->fprintf_func) (info->stream, "-");
		}
	    }
	  (*info->fprintf_func) (info->stream, "0x%x", num);
	}
    }
}

static void
dis_long (unsigned long insn,
	  bfd_vma memaddr,
	  struct disassemble_info *info)
{
  int i;
  struct d10v_opcode *op = (struct d10v_opcode *) d10v_opcodes;
  struct d10v_operand *oper;
  int need_paren = 0;
  int match = 0;

  while (op->name)
    {
      if ((op->format & LONG_OPCODE)
	  && ((op->mask & insn) == (unsigned long) op->opcode))
	{
	  match = 1;
	  (*info->fprintf_func) (info->stream, "%s\t", op->name);

	  for (i = 0; op->operands[i]; i++)
	    {
	      oper = (struct d10v_operand *) &d10v_operands[op->operands[i]];
	      if (oper->flags == OPERAND_ATPAR)
		need_paren = 1;
	      print_operand (oper, insn, op, memaddr, info);
	      if (op->operands[i + 1] && oper->bits
		  && d10v_operands[op->operands[i + 1]].flags != OPERAND_PLUS
		  && d10v_operands[op->operands[i + 1]].flags != OPERAND_MINUS)
		(*info->fprintf_func) (info->stream, ", ");
	    }
	  break;
	}
      op++;
    }

  if (!match)
    (*info->fprintf_func) (info->stream, ".long\t0x%08lx", insn);

  if (need_paren)
    (*info->fprintf_func) (info->stream, ")");
}

static void
dis_2_short (unsigned long insn,
	     bfd_vma memaddr,
	     struct disassemble_info *info,
	     int order)
{
  int i, j;
  unsigned int ins[2];
  struct d10v_opcode *op;
  int match, num_match = 0;
  struct d10v_operand *oper;
  int need_paren = 0;

  ins[0] = (insn & 0x3FFFFFFF) >> 15;
  ins[1] = insn & 0x00007FFF;

  for (j = 0; j < 2; j++)
    {
      op = (struct d10v_opcode *) d10v_opcodes;
      match = 0;
      while (op->name)
	{
	  if ((op->format & SHORT_OPCODE)
	      && ((((unsigned int) op->mask) & ins[j])
		  == (unsigned int) op->opcode))
	    {
	      (*info->fprintf_func) (info->stream, "%s\t", op->name);
	      for (i = 0; op->operands[i]; i++)
		{
		  oper = (struct d10v_operand *) &d10v_operands[op->operands[i]];
		  if (oper->flags == OPERAND_ATPAR)
		    need_paren = 1;
		  print_operand (oper, ins[j], op, memaddr, info);
		  if (op->operands[i + 1] && oper->bits
		      && d10v_operands[op->operands[i + 1]].flags != OPERAND_PLUS
		      && d10v_operands[op->operands[i + 1]].flags != OPERAND_MINUS)
		    (*info->fprintf_func) (info->stream, ", ");
		}
	      match = 1;
	      num_match++;
	      break;
	    }
	  op++;
	}
      if (!match)
	(*info->fprintf_func) (info->stream, "unknown");

      switch (order)
	{
	case 0:
	  (*info->fprintf_func) (info->stream, "\t->\t");
	  order = -1;
	  break;
	case 1:
	  (*info->fprintf_func) (info->stream, "\t<-\t");
	  order = -1;
	  break;
	case 2:
	  (*info->fprintf_func) (info->stream, "\t||\t");
	  order = -1;
	  break;
	default:
	  break;
	}
    }

  if (num_match == 0)
    (*info->fprintf_func) (info->stream, ".long\t0x%08lx", insn);

  if (need_paren)
    (*info->fprintf_func) (info->stream, ")");
}

int
print_insn_d10v (bfd_vma memaddr, struct disassemble_info *info)
{
  int status;
  bfd_byte buffer[4];
  unsigned long insn;

  status = (*info->read_memory_func) (memaddr, buffer, 4, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }
  insn = bfd_getb32 (buffer);

  status = insn & FM11;
  switch (status)
    {
    case 0:
      dis_2_short (insn, memaddr, info, 2);
      break;
    case FM01:
      dis_2_short (insn, memaddr, info, 0);
      break;
    case FM10:
      dis_2_short (insn, memaddr, info, 1);
      break;
    case FM11:
      dis_long (insn, memaddr, info);
      break;
    }
  return 4;
}
