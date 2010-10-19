/* i370-dis.c -- Disassemble Instruction 370 (ESA/390) instructions
   Copyright 1994, 2000, 2003, 2005 Free Software Foundation, Inc.
   PowerPC version written by Ian Lance Taylor, Cygnus Support
   Rewritten for i370 ESA/390 support by Linas Vepstas <linas@linas.org>

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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include <stdio.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/i370.h"

/* This file provides several disassembler functions, all of which use
   the disassembler interface defined in dis-asm.h.  */

int
print_insn_i370 (bfd_vma memaddr, struct disassemble_info *info)
{
  bfd_byte buffer[8];
  int status;
  i370_insn_t insn;
  const struct i370_opcode *opcode;
  const struct i370_opcode *opcode_end;

  status = (*info->read_memory_func) (memaddr, buffer, 6, info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, memaddr, info);
      return -1;
    }

  /* Cast the bytes into the insn (in a host-endian indep way).  */
  insn.i[0] = (buffer[0] << 24) & 0xff000000;
  insn.i[0] |= (buffer[1] << 16) & 0xff0000;
  insn.i[0] |= (buffer[2] << 8) & 0xff00;
  insn.i[0] |= buffer[3]  & 0xff;
  insn.i[1] = (buffer[4] << 24) & 0xff000000;
  insn.i[1] |= (buffer[5] << 16) & 0xff0000;

  /* Find the first match in the opcode table.  We could speed this up
     a bit by doing a binary search on the major opcode.  */
  opcode_end = i370_opcodes + i370_num_opcodes;
  for (opcode = i370_opcodes; opcode < opcode_end; opcode++)
    {
      const unsigned char *opindex;
      const struct i370_operand *operand;
      i370_insn_t masked;
      int invalid;

      /* Mask off operands, and look for a match ... */
      masked = insn;
      if (2 == opcode->len)
        {
          masked.i[0] >>= 16;
          masked.i[0] &= 0xffff;
        }
      masked.i[0] &= opcode->mask.i[0];
      if (masked.i[0] != opcode->opcode.i[0])
	continue;

      if (6 == opcode->len)
        {
          masked.i[1] &= opcode->mask.i[1];
          if (masked.i[1] != opcode->opcode.i[1])
	    continue;
        }

      /* Found a match.  adjust a tad.  */
      if (2 == opcode->len)
        {
          insn.i[0] >>= 16;
          insn.i[0] &= 0xffff;
        }

      /* Make two passes over the operands.  First see if any of them
         have extraction functions, and, if they do, make sure the
         instruction is valid.  */
      invalid = 0;
      for (opindex = opcode->operands; *opindex != 0; opindex++)
        {
          operand = i370_operands + *opindex;
          if (operand->extract)
            (*operand->extract) (insn, &invalid);
        }
      if (invalid)
	continue;

      /* The instruction is valid.  */
      (*info->fprintf_func) (info->stream, "%s", opcode->name);
      if (opcode->operands[0] != 0)
        (*info->fprintf_func) (info->stream, "\t");

      /* Now extract and print the operands.  */
      for (opindex = opcode->operands; *opindex != 0; opindex++)
        {
          long value;

          operand = i370_operands + *opindex;

          /* Extract the value from the instruction.  */
          if (operand->extract)
            value = (*operand->extract) (insn, (int *) NULL);
          else
	    value = (insn.i[0] >> operand->shift) & ((1 << operand->bits) - 1);

          /* Print the operand as directed by the flags.  */
          if ((operand->flags & I370_OPERAND_OPTIONAL) != 0)
            {
              if (value)
                (*info->fprintf_func) (info->stream, "(r%ld)", value);
            }
          else if ((operand->flags & I370_OPERAND_SBASE) != 0)
            {
              (*info->fprintf_func) (info->stream, "(r%ld)", value);
            }
          else if ((operand->flags & I370_OPERAND_INDEX) != 0)
            {
              if (value)
                (*info->fprintf_func) (info->stream, "(r%ld,", value);
              else
                (*info->fprintf_func) (info->stream, "(,");
            }
          else if ((operand->flags & I370_OPERAND_LENGTH) != 0)
            {
              (*info->fprintf_func) (info->stream, "(%ld,", value);
            }
          else if ((operand->flags & I370_OPERAND_BASE) != 0)
            (*info->fprintf_func) (info->stream, "r%ld)", value);
          else if ((operand->flags & I370_OPERAND_GPR) != 0)
            (*info->fprintf_func) (info->stream, "r%ld,", value);
          else if ((operand->flags & I370_OPERAND_FPR) != 0)
            (*info->fprintf_func) (info->stream, "f%ld,", value);
          else if ((operand->flags & I370_OPERAND_RELATIVE) != 0)
            (*info->fprintf_func) (info->stream, "%ld", value);
          else
            (*info->fprintf_func) (info->stream, " %ld, ", value);
        }

      return opcode->len;
    }

  /* We could not find a match.  */
  (*info->fprintf_func) (info->stream, ".short 0x%02x%02x", buffer[0], buffer[1]);

  return 2;
}
