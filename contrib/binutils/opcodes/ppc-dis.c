/* ppc-dis.c -- Disassemble PowerPC instructions
   Copyright 1994, 1995, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
2, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/ppc.h"

/* This file provides several disassembler functions, all of which use
   the disassembler interface defined in dis-asm.h.  Several functions
   are provided because this file handles disassembly for the PowerPC
   in both big and little endian mode and also for the POWER (RS/6000)
   chip.  */

static int print_insn_powerpc (bfd_vma, struct disassemble_info *, int, int);

struct dis_private {
  /* Stash the result of parsing disassembler_options here.  */
  int dialect;
};

/* Determine which set of machines to disassemble for.  PPC403/601 or
   BookE.  For convenience, also disassemble instructions supported
   by the AltiVec vector unit.  */

static int
powerpc_dialect (struct disassemble_info *info)
{
  int dialect = PPC_OPCODE_PPC | PPC_OPCODE_ALTIVEC;

  if (BFD_DEFAULT_TARGET_SIZE == 64)
    dialect |= PPC_OPCODE_64;

  if (info->disassembler_options
      && strstr (info->disassembler_options, "booke") != NULL)
    dialect |= PPC_OPCODE_BOOKE | PPC_OPCODE_BOOKE64;
  else if ((info->mach == bfd_mach_ppc_e500)
	   || (info->disassembler_options
	       && strstr (info->disassembler_options, "e500") != NULL))
    {
      dialect |= PPC_OPCODE_BOOKE
	| PPC_OPCODE_SPE | PPC_OPCODE_ISEL
	| PPC_OPCODE_EFS | PPC_OPCODE_BRLOCK
	| PPC_OPCODE_PMR | PPC_OPCODE_CACHELCK
	| PPC_OPCODE_RFMCI;
      /* efs* and AltiVec conflict.  */
      dialect &= ~PPC_OPCODE_ALTIVEC;
    }
  else if (info->disassembler_options
	   && strstr (info->disassembler_options, "efs") != NULL)
    {
      dialect |= PPC_OPCODE_EFS;
      /* efs* and AltiVec conflict.  */
      dialect &= ~PPC_OPCODE_ALTIVEC;
    }
  else
    dialect |= (PPC_OPCODE_403 | PPC_OPCODE_601 | PPC_OPCODE_CLASSIC
		| PPC_OPCODE_COMMON);

  if (info->disassembler_options
      && strstr (info->disassembler_options, "power4") != NULL)
    dialect |= PPC_OPCODE_POWER4;

  if (info->disassembler_options
      && strstr (info->disassembler_options, "any") != NULL)
    dialect |= PPC_OPCODE_ANY;

  if (info->disassembler_options)
    {
      if (strstr (info->disassembler_options, "32") != NULL)
	dialect &= ~PPC_OPCODE_64;
      else if (strstr (info->disassembler_options, "64") != NULL)
	dialect |= PPC_OPCODE_64;
    }

  ((struct dis_private *) &info->private_data)->dialect = dialect;
  return dialect;
}

/* Print a big endian PowerPC instruction.  */

int
print_insn_big_powerpc (bfd_vma memaddr, struct disassemble_info *info)
{
  int dialect = ((struct dis_private *) &info->private_data)->dialect;
  return print_insn_powerpc (memaddr, info, 1, dialect);
}

/* Print a little endian PowerPC instruction.  */

int
print_insn_little_powerpc (bfd_vma memaddr, struct disassemble_info *info)
{
  int dialect = ((struct dis_private *) &info->private_data)->dialect;
  return print_insn_powerpc (memaddr, info, 0, dialect);
}

/* Print a POWER (RS/6000) instruction.  */

int
print_insn_rs6000 (bfd_vma memaddr, struct disassemble_info *info)
{
  return print_insn_powerpc (memaddr, info, 1, PPC_OPCODE_POWER);
}

/* Print a PowerPC or POWER instruction.  */

static int
print_insn_powerpc (bfd_vma memaddr,
		    struct disassemble_info *info,
		    int bigendian,
		    int dialect)
{
  bfd_byte buffer[4];
  int status;
  unsigned long insn;
  const struct powerpc_opcode *opcode;
  const struct powerpc_opcode *opcode_end;
  unsigned long op;

  if (dialect == 0)
    dialect = powerpc_dialect (info);

  status = (*info->read_memory_func) (memaddr, buffer, 4, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  if (bigendian)
    insn = bfd_getb32 (buffer);
  else
    insn = bfd_getl32 (buffer);

  /* Get the major opcode of the instruction.  */
  op = PPC_OP (insn);

  /* Find the first match in the opcode table.  We could speed this up
     a bit by doing a binary search on the major opcode.  */
  opcode_end = powerpc_opcodes + powerpc_num_opcodes;
 again:
  for (opcode = powerpc_opcodes; opcode < opcode_end; opcode++)
    {
      unsigned long table_op;
      const unsigned char *opindex;
      const struct powerpc_operand *operand;
      int invalid;
      int need_comma;
      int need_paren;

      table_op = PPC_OP (opcode->opcode);
      if (op < table_op)
	break;
      if (op > table_op)
	continue;

      if ((insn & opcode->mask) != opcode->opcode
	  || (opcode->flags & dialect) == 0)
	continue;

      /* Make two passes over the operands.  First see if any of them
	 have extraction functions, and, if they do, make sure the
	 instruction is valid.  */
      invalid = 0;
      for (opindex = opcode->operands; *opindex != 0; opindex++)
	{
	  operand = powerpc_operands + *opindex;
	  if (operand->extract)
	    (*operand->extract) (insn, dialect, &invalid);
	}
      if (invalid)
	continue;

      /* The instruction is valid.  */
      if (opcode->operands[0] != 0)
	(*info->fprintf_func) (info->stream, "%-7s ", opcode->name);
      else
	(*info->fprintf_func) (info->stream, "%s", opcode->name);

      /* Now extract and print the operands.  */
      need_comma = 0;
      need_paren = 0;
      for (opindex = opcode->operands; *opindex != 0; opindex++)
	{
	  long value;

	  operand = powerpc_operands + *opindex;

	  /* Operands that are marked FAKE are simply ignored.  We
	     already made sure that the extract function considered
	     the instruction to be valid.  */
	  if ((operand->flags & PPC_OPERAND_FAKE) != 0)
	    continue;

	  /* Extract the value from the instruction.  */
	  if (operand->extract)
	    value = (*operand->extract) (insn, dialect, &invalid);
	  else
	    {
	      value = (insn >> operand->shift) & ((1 << operand->bits) - 1);
	      if ((operand->flags & PPC_OPERAND_SIGNED) != 0
		  && (value & (1 << (operand->bits - 1))) != 0)
		value -= 1 << operand->bits;
	    }

	  /* If the operand is optional, and the value is zero, don't
	     print anything.  */
	  if ((operand->flags & PPC_OPERAND_OPTIONAL) != 0
	      && (operand->flags & PPC_OPERAND_NEXT) == 0
	      && value == 0)
	    continue;

	  if (need_comma)
	    {
	      (*info->fprintf_func) (info->stream, ",");
	      need_comma = 0;
	    }

	  /* Print the operand as directed by the flags.  */
	  if ((operand->flags & PPC_OPERAND_GPR) != 0
	      || ((operand->flags & PPC_OPERAND_GPR_0) != 0 && value != 0))
	    (*info->fprintf_func) (info->stream, "r%ld", value);
	  else if ((operand->flags & PPC_OPERAND_FPR) != 0)
	    (*info->fprintf_func) (info->stream, "f%ld", value);
	  else if ((operand->flags & PPC_OPERAND_VR) != 0)
	    (*info->fprintf_func) (info->stream, "v%ld", value);
	  else if ((operand->flags & PPC_OPERAND_RELATIVE) != 0)
	    (*info->print_address_func) (memaddr + value, info);
	  else if ((operand->flags & PPC_OPERAND_ABSOLUTE) != 0)
	    (*info->print_address_func) ((bfd_vma) value & 0xffffffff, info);
	  else if ((operand->flags & PPC_OPERAND_CR) == 0
		   || (dialect & PPC_OPCODE_PPC) == 0)
	    (*info->fprintf_func) (info->stream, "%ld", value);
	  else
	    {
	      if (operand->bits == 3)
		(*info->fprintf_func) (info->stream, "cr%d", value);
	      else
		{
		  static const char *cbnames[4] = { "lt", "gt", "eq", "so" };
		  int cr;
		  int cc;

		  cr = value >> 2;
		  if (cr != 0)
		    (*info->fprintf_func) (info->stream, "4*cr%d+", cr);
		  cc = value & 3;
		  (*info->fprintf_func) (info->stream, "%s", cbnames[cc]);
		}
	    }

	  if (need_paren)
	    {
	      (*info->fprintf_func) (info->stream, ")");
	      need_paren = 0;
	    }

	  if ((operand->flags & PPC_OPERAND_PARENS) == 0)
	    need_comma = 1;
	  else
	    {
	      (*info->fprintf_func) (info->stream, "(");
	      need_paren = 1;
	    }
	}

      /* We have found and printed an instruction; return.  */
      return 4;
    }

  if ((dialect & PPC_OPCODE_ANY) != 0)
    {
      dialect = ~PPC_OPCODE_ANY;
      goto again;
    }

  /* We could not find a match.  */
  (*info->fprintf_func) (info->stream, ".long 0x%lx", insn);

  return 4;
}

void
print_ppc_disassembler_options (FILE *stream)
{
  fprintf (stream, "\n\
The following PPC specific disassembler options are supported for use with\n\
the -M switch:\n");

  fprintf (stream, "  booke|booke32|booke64    Disassemble the BookE instructions\n");
  fprintf (stream, "  e500|e500x2              Disassemble the e500 instructions\n");
  fprintf (stream, "  efs                      Disassemble the EFS instructions\n");
  fprintf (stream, "  power4                   Disassemble the Power4 instructions\n");
  fprintf (stream, "  32                       Do not disassemble 64-bit instructions\n");
  fprintf (stream, "  64                       Allow disassembly of 64-bit instructions\n");
}
