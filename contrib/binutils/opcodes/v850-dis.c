/* Disassemble V850 instructions.
   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.

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

#include "ansidecl.h"
#include "opcode/v850.h" 
#include "dis-asm.h"

static const char *const v850_reg_names[] =
{ "r0", "r1", "r2", "sp", "gp", "r5", "r6", "r7", 
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", 
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", 
  "r24", "r25", "r26", "r27", "r28", "r29", "ep", "lp" };

static const char *const v850_sreg_names[] =
{ "eipc", "eipsw", "fepc", "fepsw", "ecr", "psw", "sr6", "sr7", 
  "sr8", "sr9", "sr10", "sr11", "sr12", "sr13", "sr14", "sr15",
  "sr16", "sr17", "sr18", "sr19", "sr20", "sr21", "sr22", "sr23", 
  "sr24", "sr25", "sr26", "sr27", "sr28", "sr29", "sr30", "sr31" };

static const char *const v850_cc_names[] =
{ "v", "c/l", "z", "nh", "s/n", "t", "lt", "le", 
  "nv", "nc/nl", "nz", "h", "ns/p", "sa", "ge", "gt" };

static int
disassemble (memaddr, info, insn)
     bfd_vma memaddr;
     struct disassemble_info *info;
     unsigned long insn;
{
  struct v850_opcode *          op = (struct v850_opcode *)v850_opcodes;
  const struct v850_operand *   operand;
  int                           match = 0;
  int                         	short_op = ((insn & 0x0600) != 0x0600);
  int				bytes_read;
  int				target_processor;
  
  
  bytes_read = short_op ? 2 : 4;
  
  /* If this is a two byte insn, then mask off the high bits. */
  if (short_op)
    insn &= 0xffff;

  switch (info->mach)
    {
    case 0:
    default:
      target_processor = PROCESSOR_V850;
      break;

    }
  
  /* Find the opcode.  */
  while (op->name)
    {
      if ((op->mask & insn) == op->opcode
	  && (op->processors & target_processor))
	{
	  const unsigned char * opindex_ptr;
	  unsigned int          opnum;
	  unsigned int          memop;

	  match = 1;
	  (*info->fprintf_func) (info->stream, "%s\t", op->name);
/*fprintf (stderr, "match: mask: %x insn: %x, opcode: %x, name: %s\n", op->mask, insn, op->opcode, op->name );*/

	  memop = op->memop;
	  /* Now print the operands.

	     MEMOP is the operand number at which a memory
	     address specification starts, or zero if this
	     instruction has no memory addresses.

	     A memory address is always two arguments.

	     This information allows us to determine when to
	     insert commas into the output stream as well as
	     when to insert disp[reg] expressions onto the
	     output stream.  */
	  
	  for (opindex_ptr = op->operands, opnum = 1;
	       *opindex_ptr != 0;
	       opindex_ptr++, opnum++)
	    {
	      long 	value;
	      int  	flag;
	      int       status;
	      bfd_byte	buffer[ 4 ];
	      
	      operand = &v850_operands[*opindex_ptr];
	      
	      if (operand->extract)
		value = (operand->extract) (insn, 0);
	      else
		{
		  if (operand->bits == -1)
		    value = (insn & operand->shift);
		  else
		    value = (insn >> operand->shift) & ((1 << operand->bits) - 1);

		  if (operand->flags & V850_OPERAND_SIGNED)
		    value = ((long)(value << (32 - operand->bits))
			     >> (32 - operand->bits));
		}

	      /* The first operand is always output without any
		 special handling.

		 For the following arguments:

		   If memop && opnum == memop + 1, then we need '[' since
		   we're about to output the register used in a memory
		   reference.

		   If memop && opnum == memop + 2, then we need ']' since
		   we just finished the register in a memory reference.  We
		   also need a ',' before this operand.

		   Else we just need a comma.

		   We may need to output a trailing ']' if the last operand
		   in an instruction is the register for a memory address. 

		   The exception (and there's always an exception) is the
		   "jmp" insn which needs square brackets around it's only
		   register argument.  */

	           if (memop && opnum == memop + 1) info->fprintf_func (info->stream, "[");
	      else if (memop && opnum == memop + 2) info->fprintf_func (info->stream, "],");
	      else if (memop == 1 && opnum == 1
		       && (operand->flags & V850_OPERAND_REG))
		                                    info->fprintf_func (info->stream, "[");
	      else if (opnum > 1)	            info->fprintf_func (info->stream, ", ");

	      /* extract the flags, ignorng ones which do not effect disassembly output. */
	      flag = operand->flags;
	      flag &= ~ V850_OPERAND_SIGNED;
	      flag &= ~ V850_OPERAND_RELAX;
	      flag &= - flag;
	      
	      switch (flag)
		{
		case V850_OPERAND_REG:  info->fprintf_func (info->stream, "%s", v850_reg_names[value]); break;
		case V850_OPERAND_SRG:  info->fprintf_func (info->stream, "%s", v850_sreg_names[value]); break;
		case V850_OPERAND_CC:   info->fprintf_func (info->stream, "%s", v850_cc_names[value]); break;
		case V850_OPERAND_EP:   info->fprintf_func (info->stream, "ep"); break;
		default:                info->fprintf_func (info->stream, "%d", value); break;
		case V850_OPERAND_DISP:
		  {
		    bfd_vma addr = value + memaddr;
		    
		    /* On the v850 the top 8 bits of an address are used by an overlay manager.
		       Thus it may happen that when we are looking for a symbol to match
		       against an address with some of its top bits set, the search fails to
		       turn up an exact match.  In this case we try to find an exact match
		       against a symbol in the lower address space, and if we find one, we
		       use that address.   We only do this for JARL instructions however, as
		       we do not want to misinterpret branch instructions.  */
		    if (operand->bits == 22)
		      {
			if ( ! info->symbol_at_address_func (addr, info)
			    && ((addr & 0xFF000000) != 0)
			    && info->symbol_at_address_func (addr & 0x00FFFFFF, info))
			  {
			    addr &= 0x00FFFFFF;
			  }
		      }
		    info->print_address_func (addr, info);
		    break;
		  }
		    
		}		  

	      /* Handle jmp correctly.  */
	      if (memop == 1 && opnum == 1
		  && ((operand->flags & V850_OPERAND_REG) != 0))
		(*info->fprintf_func) (info->stream, "]");
	    }

	  /* Close any square bracket we left open.  */
	  if (memop && opnum == memop + 2)
	    (*info->fprintf_func) (info->stream, "]");

	  /* All done. */
	  break;
	}
      op++;
    }

  if (!match)
    {
      if (short_op)
	info->fprintf_func (info->stream, ".short\t0x%04x", insn);
      else
	info->fprintf_func (info->stream, ".long\t0x%08x", insn);
    }

  return bytes_read;
}

int 
print_insn_v850 (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info * info;
{
  int           status;
  bfd_byte      buffer[ 4 ];
  unsigned long insn = 0;

  /* First figure out how big the opcode is.  */
  
  status = info->read_memory_func (memaddr, buffer, 2, info);
  if (status == 0)
    {
      insn = bfd_getl16 (buffer);
      
      if (   (insn & 0x0600) == 0x0600
	  && (insn & 0xffe0) != 0x0620)
	{
	  /* If this is a 4 byte insn, read 4 bytes of stuff.  */
	  status = info->read_memory_func (memaddr, buffer, 4, info);

	  if (status == 0)
	    insn = bfd_getl32 (buffer);
	}
    }
  
  if (status != 0)
    {
      info->memory_error_func (status, memaddr, info);
      return -1;
    }

  /* Make sure we tell our caller how many bytes we consumed.  */
  return disassemble (memaddr, info, insn);
}
