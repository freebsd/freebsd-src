/* Disassemble V850 instructions.
   Copyright 1996, 1997, 1998, 2000, 2001 Free Software Foundation, Inc.

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
#include "opcode/v850.h" 
#include "dis-asm.h"
#include "opintl.h"

static const char *const v850_reg_names[] =
{ "r0", "r1", "r2", "sp", "gp", "r5", "r6", "r7", 
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", 
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", 
  "r24", "r25", "r26", "r27", "r28", "r29", "ep", "lp" };

static const char *const v850_sreg_names[] =
{ "eipc", "eipsw", "fepc", "fepsw", "ecr", "psw", "sr6", "sr7", 
  "sr8", "sr9", "sr10", "sr11", "sr12", "sr13", "sr14", "sr15",
  "ctpc", "ctpsw", "dbpc", "dbpsw", "ctbp", "sr21", "sr22", "sr23", 
  "sr24", "sr25", "sr26", "sr27", "sr28", "sr29", "sr30", "sr31",
  "sr16", "sr17", "sr18", "sr19", "sr20", "sr21", "sr22", "sr23", 
  "sr24", "sr25", "sr26", "sr27", "sr28", "sr29", "sr30", "sr31" };

static const char *const v850_cc_names[] =
{ "v", "c/l", "z", "nh", "s/n", "t", "lt", "le", 
  "nv", "nc/nl", "nz", "h", "ns/p", "sa", "ge", "gt" };

static int disassemble
  PARAMS ((bfd_vma, struct disassemble_info *, unsigned long));

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
  
  /* Special case: 32 bit MOV */
  if ((insn & 0xffe0) == 0x0620)
    short_op = true;
  
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

    case bfd_mach_v850e:
      target_processor = PROCESSOR_V850E;
      break;

    case bfd_mach_v850ea: 
      target_processor = PROCESSOR_V850EA;
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
		    
		case V850E_PUSH_POP:
		  {
		    static int list12_regs[32]   = { 30,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0, 31, 29, 28, 23, 22, 21, 20, 27, 26, 25, 24 };
		    static int list18_h_regs[32] = { 19, 18, 17, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 30, 31, 29, 28, 23, 22, 21, 20, 27, 26, 25, 24 };
		    static int list18_l_regs[32] = {  3,  2,  1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 14, 15, 13, 12,  7,  6,  5,  4, 11, 10,  9,  8 };
		    int *             regs;
		    int               i;
		    unsigned long int mask = 0;
		    int               pc   = false;
		    int               sr   = false;
		    
		    
		    switch (operand->shift)
		      {
		      case 0xffe00001: regs = list12_regs; break;
		      case 0xfff8000f: regs = list18_h_regs; break;
		      case 0xfff8001f: regs = list18_l_regs; value &= ~0x10; break;  /* Do not include magic bit */
		      default:
			/* xgettext:c-format */
			fprintf (stderr, _("unknown operand shift: %x\n"), operand->shift );
			abort();
		      }

		    for (i = 0; i < 32; i++)
		      {
			if (value & (1 << i))
			  {
			    switch (regs[ i ])
			      {
			      default: mask |= (1 << regs[ i ]); break;
				/* xgettext:c-format */
			      case 0:  fprintf (stderr, _("unknown pop reg: %d\n"), i ); abort();
			      case -1: pc = true; break;
			      case -2: sr = true; break;
			      }
			  }
		      }

		    info->fprintf_func (info->stream, "{");
		    
		    if (mask || pc || sr)
		      {
			if (mask)
			  {
			    unsigned int bit;
			    int          shown_one = false;
			    
			    for (bit = 0; bit < 32; bit++)
			      if (mask & (1 << bit))
				{
				  unsigned long int first = bit;
				  unsigned long int last;

				  if (shown_one)
				    info->fprintf_func (info->stream, ", ");
				  else
				    shown_one = true;
				  
				  info->fprintf_func (info->stream, v850_reg_names[first]);
				  
				  for (bit++; bit < 32; bit++)
				    if ((mask & (1 << bit)) == 0)
				      break;

				  last = bit;

				  if (last > first + 1)
				    {
				      info->fprintf_func (info->stream, " - %s", v850_reg_names[ last - 1 ]);
				    }
				}
			  }
			
			if (pc)
			  info->fprintf_func (info->stream, "%sPC", mask ? ", " : "");
			if (sr)
			  info->fprintf_func (info->stream, "%sSR", (mask || pc) ? ", " : "");
		      }
		    
		    info->fprintf_func (info->stream, "}");
		  }
		break;
		  
		case V850E_IMMEDIATE16:
		  status = info->read_memory_func (memaddr + bytes_read, buffer, 2, info);
		  if (status == 0)
		    {
		      bytes_read += 2;
		      value = bfd_getl16 (buffer);

		      /* If this is a DISPOSE instruction with ff set to 0x10, then shift value up by 16.  */
		      if ((insn & 0x001fffc0) == 0x00130780)
			value <<= 16;

		      info->fprintf_func (info->stream, "0x%x", value);
		    }
		  else
		    {
		      info->memory_error_func (status, memaddr + bytes_read, info);
		    }
		  break;
		  
		case V850E_IMMEDIATE32:
		  status = info->read_memory_func (memaddr + bytes_read, buffer, 4, info);
		  if (status == 0)
		    {
		      bytes_read += 4;
		      value = bfd_getl32 (buffer);
		      info->fprintf_func (info->stream, "0x%lx", value);
		    }
		  else
		    {
		      info->memory_error_func (status, memaddr + bytes_read, info);
		    }
		  break;
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
