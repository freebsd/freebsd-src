/* Disassemble MN10200 instructions.
   Copyright 1996, 1997, 1998, 2000, 2005 Free Software Foundation, Inc.

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
#include "opcode/mn10200.h" 
#include "dis-asm.h"
#include "opintl.h"

static void
disassemble (bfd_vma memaddr,
	     struct disassemble_info *info,
	     unsigned long insn,
	     unsigned long extension,
	     unsigned int size)
{
  struct mn10200_opcode *op = (struct mn10200_opcode *)mn10200_opcodes;
  const struct mn10200_operand *operand;
  int match = 0;

  /* Find the opcode.  */
  while (op->name)
    {
      int mysize, extra_shift;

      if (op->format == FMT_1)
	mysize = 1;
      else if (op->format == FMT_2
	       || op->format == FMT_4)
	mysize = 2;
      else if (op->format == FMT_3
	       || op->format == FMT_5)
	mysize = 3;
      else if (op->format == FMT_6)
	mysize = 4;
      else if (op->format == FMT_7)
	mysize = 5;
      else
	abort ();
	
      if (op->format == FMT_2 || op->format == FMT_5)
	extra_shift = 8;
      else if (op->format == FMT_3
	       || op->format == FMT_6
	       || op->format == FMT_7)
	extra_shift = 16;
      else
	extra_shift = 0;

      if ((op->mask & insn) == op->opcode
	  && size == (unsigned int) mysize)
	{
	  const unsigned char *opindex_ptr;
	  unsigned int nocomma;
	  int paren = 0;
	  
	  match = 1;
	  (*info->fprintf_func) (info->stream, "%s\t", op->name);

	  /* Now print the operands.  */
	  for (opindex_ptr = op->operands, nocomma = 1;
	       *opindex_ptr != 0;
	       opindex_ptr++)
	    {
	      unsigned long value;

	      operand = &mn10200_operands[*opindex_ptr];

	      if ((operand->flags & MN10200_OPERAND_EXTENDED) != 0)
		{
		  value = (insn & 0xffff) << 8;
		  value |= extension;
		}
	      else
		{
		  value = ((insn >> (operand->shift))
			   & ((1L << operand->bits) - 1L));
		}

	      if ((operand->flags & MN10200_OPERAND_SIGNED) != 0)
		value = ((long)(value << (32 - operand->bits))
			  >> (32 - operand->bits));

	      if (!nocomma
		  && (!paren
		      || ((operand->flags & MN10200_OPERAND_PAREN) == 0)))
		(*info->fprintf_func) (info->stream, ",");

	      nocomma = 0;
		
	      if ((operand->flags & MN10200_OPERAND_DREG) != 0)
		{
		  value = ((insn >> (operand->shift + extra_shift))
			   & ((1 << operand->bits) - 1));
		  (*info->fprintf_func) (info->stream, "d%ld", value);
		}

	      else if ((operand->flags & MN10200_OPERAND_AREG) != 0)
		{
		  value = ((insn >> (operand->shift + extra_shift))
			   & ((1 << operand->bits) - 1));
		  (*info->fprintf_func) (info->stream, "a%ld", value);
		}

	      else if ((operand->flags & MN10200_OPERAND_PSW) != 0)
		(*info->fprintf_func) (info->stream, "psw");

	      else if ((operand->flags & MN10200_OPERAND_MDR) != 0)
		(*info->fprintf_func) (info->stream, "mdr");

	      else if ((operand->flags & MN10200_OPERAND_PAREN) != 0)
		{
		  if (paren)
		    (*info->fprintf_func) (info->stream, ")");
		  else
		    {
		      (*info->fprintf_func) (info->stream, "(");
		      nocomma = 1;
		    }
		  paren = !paren;
		}

	      else if ((operand->flags & MN10200_OPERAND_PCREL) != 0)
		(*info->print_address_func)
		  ((value + memaddr + mysize) & 0xffffff, info);

	      else if ((operand->flags & MN10200_OPERAND_MEMADDR) != 0)
		(*info->print_address_func) (value, info);

	      else 
		(*info->fprintf_func) (info->stream, "%ld", value);
	    }
	  /* All done. */
	  break;
	}
      op++;
    }

  if (!match)
    (*info->fprintf_func) (info->stream, _("unknown\t0x%04lx"), insn);
}

int 
print_insn_mn10200 (bfd_vma memaddr, struct disassemble_info *info)
{
  int status;
  bfd_byte buffer[4];
  unsigned long insn;
  unsigned long extension = 0;
  unsigned int consume;

  /* First figure out how big the opcode is.  */
  status = (*info->read_memory_func) (memaddr, buffer, 1, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  insn = *(unsigned char *) buffer;

  /* These are one byte insns.  */
  if ((insn & 0xf0) == 0x00
      || (insn & 0xf0) == 0x10
      || (insn & 0xf0) == 0x20
      || (insn & 0xf0) == 0x30
      || ((insn & 0xf0) == 0x80
	  && (insn & 0x0c) >> 2 != (insn & 0x03))
      || (insn & 0xf0) == 0x90
      || (insn & 0xf0) == 0xa0
      || (insn & 0xf0) == 0xb0
      || (insn & 0xff) == 0xeb
      || (insn & 0xff) == 0xf6
      || (insn & 0xff) == 0xfe
      || (insn & 0xff) == 0xff)
    {
      extension = 0;
      consume = 1;
    }

  /* These are two byte insns.  */
  else if ((insn & 0xf0) == 0x40
	   || (insn & 0xf0) == 0x50
	   || (insn & 0xf0) == 0x60
	   || (insn & 0xf0) == 0x70
	   || (insn & 0xf0) == 0x80
	   || (insn & 0xfc) == 0xd0
	   || (insn & 0xfc) == 0xd4
	   || (insn & 0xfc) == 0xd8
	   || (insn & 0xfc) == 0xe0
	   || (insn & 0xfc) == 0xe4
	   || (insn & 0xff) == 0xe8
	   || (insn & 0xff) == 0xe9
	   || (insn & 0xff) == 0xea
	   || (insn & 0xff) == 0xf0
	   || (insn & 0xff) == 0xf1
	   || (insn & 0xff) == 0xf2
	   || (insn & 0xff) == 0xf3)
    {
      status = (*info->read_memory_func) (memaddr, buffer, 2, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	   return -1;
	}
      insn = bfd_getb16 (buffer);
      consume = 2;
    }

  /* These are three byte insns with a 16bit operand in little
     endian form.  */
  else if ((insn & 0xf0) == 0xc0
	   || (insn & 0xfc) == 0xdc
	   || (insn & 0xfc) == 0xec
	   || (insn & 0xff) == 0xf8
	   || (insn & 0xff) == 0xf9
	   || (insn & 0xff) == 0xfa
	   || (insn & 0xff) == 0xfb
	   || (insn & 0xff) == 0xfc
	   || (insn & 0xff) == 0xfd)
    {
      status = (*info->read_memory_func) (memaddr + 1, buffer, 2, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn <<= 16;
      insn |= bfd_getl16 (buffer);
      extension = 0;
      consume = 3;
    }
  /* These are three byte insns too, but we don't have to mess with
     endianness stuff.  */
  else if ((insn & 0xff) == 0xf5)
    {
      status = (*info->read_memory_func) (memaddr + 1, buffer, 2, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn <<= 16;
      insn |= bfd_getb16 (buffer);
      extension = 0;
      consume = 3;
    }

  /* These are four byte insns.  */
  else if ((insn & 0xff) == 0xf7)
    {
      status = (*info->read_memory_func) (memaddr, buffer, 2, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn = bfd_getb16 (buffer);
      insn <<= 16;
      status = (*info->read_memory_func) (memaddr + 2, buffer, 2, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn |= bfd_getl16 (buffer);
      extension = 0;
      consume = 4;
    }

  /* These are five byte insns.  */
  else if ((insn & 0xff) == 0xf4)
    {
      status = (*info->read_memory_func) (memaddr, buffer, 2, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn = bfd_getb16 (buffer);
      insn <<= 16;

      status = (*info->read_memory_func) (memaddr + 4, buffer, 1, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn |= (*(unsigned char *)buffer << 8) & 0xff00;

      status = (*info->read_memory_func) (memaddr + 3, buffer, 1, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      insn |= (*(unsigned char *)buffer) & 0xff;

      status = (*info->read_memory_func) (memaddr + 2, buffer, 1, info);
      if (status != 0)
	{
	  (*info->memory_error_func) (status, memaddr, info);
	  return -1;
	}
      extension = (*(unsigned char *)buffer) & 0xff;
      consume = 5;
    }
  else
    {
      (*info->fprintf_func) (info->stream, _("unknown\t0x%02lx"), insn);
      return 1;
    }

  disassemble (memaddr, info, insn, extension, consume);

  return consume;
}
