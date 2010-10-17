/* Disassemble MSP430 instructions.
   Copyright (C) 2002, 2004 Free Software Foundation, Inc.
   
   Contributed by Dmitry Diky <diwil@mail.ru>
        
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
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include "dis-asm.h"
#include "opintl.h"
#include "libiberty.h"

#define DASM_SECTION
#include "opcode/msp430.h"
#undef DASM_SECTION


static unsigned short msp430dis_opcode
  PARAMS ((bfd_vma, disassemble_info *));
int print_insn_msp430
  PARAMS ((bfd_vma, disassemble_info *));
int msp430_nooperands
  PARAMS ((struct msp430_opcode_s *, bfd_vma, unsigned short, char *, int *));
int msp430_singleoperand
  PARAMS ((disassemble_info *, struct msp430_opcode_s *, bfd_vma, unsigned short,
	   char *, char *, int *));
int msp430_doubleoperand
  PARAMS ((disassemble_info *, struct msp430_opcode_s *, bfd_vma, unsigned short,
	   char *, char *, char *, char *, int *));
int msp430_branchinstr
  PARAMS ((disassemble_info *, struct msp430_opcode_s *, bfd_vma, unsigned short,
	   char *, char *, int *));

#define PS(x)   (0xffff & (x))

static unsigned short
msp430dis_opcode (addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  bfd_byte buffer[2];
  int status;

  status = info->read_memory_func (addr, buffer, 2, info);
  if (status != 0)
    {
      info->memory_error_func (status, addr, info);
      return -1;
    }
  return bfd_getl16 (buffer);
}

int
print_insn_msp430 (addr, info)
     bfd_vma addr;
     disassemble_info *info;
{
  void *stream = info->stream;
  fprintf_ftype prin = info->fprintf_func;
  struct msp430_opcode_s *opcode;
  char op1[32], op2[32], comm1[64], comm2[64];
  int cmd_len = 0;
  unsigned short insn;
  int cycles = 0;
  char *bc = "";
  char dinfo[32];		/* Debug purposes.  */

  insn = msp430dis_opcode (addr, info);
  sprintf (dinfo, "0x%04x", insn);

  if (((int) addr & 0xffff) > 0xffdf)
    {
      (*prin) (stream, "interrupt service routine at 0x%04x", 0xffff & insn);
      return 2;
    }

  *comm1 = 0;
  *comm2 = 0;

  for (opcode = msp430_opcodes; opcode->name; opcode++)
    {
      if ((insn & opcode->bin_mask) == opcode->bin_opcode
	  && opcode->bin_opcode != 0x9300)
	{
	  *op1 = 0;
	  *op2 = 0;
	  *comm1 = 0;
	  *comm2 = 0;

	  /* r0 as destination. Ad should be zero.  */
	  if (opcode->insn_opnumb == 3 && (insn & 0x000f) == 0
	      && (0x0080 & insn) == 0)
	    {
	      cmd_len =
		msp430_branchinstr (info, opcode, addr, insn, op1, comm1,
				    &cycles);
	      if (cmd_len)
		break;
	    }

	  switch (opcode->insn_opnumb)
	    {
	    case 0:
	      cmd_len = msp430_nooperands (opcode, addr, insn, comm1, &cycles);
	      break;
	    case 2:
	      cmd_len =
		msp430_doubleoperand (info, opcode, addr, insn, op1, op2,
				      comm1, comm2, &cycles);
	      if (insn & BYTE_OPERATION)
		bc = ".b";
	      break;
	    case 1:
	      cmd_len =
		msp430_singleoperand (info, opcode, addr, insn, op1, comm1,
				      &cycles);
	      if (insn & BYTE_OPERATION && opcode->fmt != 3)
		bc = ".b";
	      break;
	    default:
	      break;
	    }
	}

      if (cmd_len)
	break;
    }

  dinfo[5] = 0;

  if (cmd_len < 1)
    {
      /* Unknown opcode, or invalid combination of operands.  */
      (*prin) (stream, ".word	0x%04x;	????", PS (insn));
      return 2;
    }

  (*prin) (stream, "%s%s", opcode->name, bc);

  if (*op1)
    (*prin) (stream, "\t%s", op1);
  if (*op2)
    (*prin) (stream, ",");

  if (strlen (op1) < 7)
    (*prin) (stream, "\t");
  if (!strlen (op1))
    (*prin) (stream, "\t");

  if (*op2)
    (*prin) (stream, "%s", op2);
  if (strlen (op2) < 8)
    (*prin) (stream, "\t");

  if (*comm1 || *comm2)
    (*prin) (stream, ";");
  else if (cycles)
    {
      if (*op2)
	(*prin) (stream, ";");
      else
	{
	  if (strlen (op1) < 7)
	    (*prin) (stream, ";");
	  else
	    (*prin) (stream, "\t;");
	}
    }
  if (*comm1)
    (*prin) (stream, "%s", comm1);
  if (*comm1 && *comm2)
    (*prin) (stream, ",");
  if (*comm2)
    (*prin) (stream, " %s", comm2);
  return cmd_len;
}

int
msp430_nooperands (opcode, addr, insn, comm, cycles)
     struct msp430_opcode_s *opcode;
     bfd_vma addr ATTRIBUTE_UNUSED;
     unsigned short insn ATTRIBUTE_UNUSED;
     char *comm;
     int *cycles;
{
  /* Pop with constant.  */
  if (insn == 0x43b2)
    return 0;
  if (insn == opcode->bin_opcode)
    return 2;

  if (opcode->fmt == 0)
    {
      if ((insn & 0x0f00) != 3 || (insn & 0x0f00) != 2)
	return 0;

      strcpy (comm, "emulated...");
      *cycles = 1;
    }
  else
    {
      strcpy (comm, "return from interupt");
      *cycles = 5;
    }

  return 2;
}


int
msp430_singleoperand (info, opcode, addr, insn, op, comm, cycles)
     disassemble_info *info;
     struct msp430_opcode_s *opcode;
     bfd_vma addr;
     unsigned short insn;
     char *op;
     char *comm;
     int *cycles;
{
  int regs = 0, regd = 0;
  int ad = 0, as = 0;
  int where = 0;
  int cmd_len = 2;
  short dst = 0;

  regd = insn & 0x0f;
  regs = (insn & 0x0f00) >> 8;
  as = (insn & 0x0030) >> 4;
  ad = (insn & 0x0080) >> 7;

  switch (opcode->fmt)
    {
    case 0:			/* Emulated work with dst register.  */
      if (regs != 2 && regs != 3 && regs != 1)
	return 0;

      /* Check if not clr insn.  */
      if (opcode->bin_opcode == 0x4300 && (ad || as))
	return 0;

      /* Check if really inc, incd insns.  */
      if ((opcode->bin_opcode & 0xff00) == 0x5300 && as == 3)
	return 0;

      if (ad == 0)
	{
	  *cycles = 1;

	  /* Register.  */
	  if (regd == 0)
	    {
	      *cycles += 1;
	      sprintf (op, "r0");
	    }
	  else if (regd == 1)
	    sprintf (op, "r1");

	  else if (regd == 2)
	    sprintf (op, "r2");

	  else
	    sprintf (op, "r%d", regd);
	}
      else			/* ad == 1 msp430dis_opcode.  */
	{
	  if (regd == 0)
	    {
	      /* PC relative.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      *cycles = 4;
	      sprintf (op, "0x%04x", dst);
	      sprintf (comm, "PC rel. abs addr 0x%04x",
		       PS ((short) (addr + 2) + dst));
	    }
	  else if (regd == 2)
	    {
	      /* Absolute.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      *cycles = 4;
	      sprintf (op, "&0x%04x", PS (dst));
	    }
	  else
	    {
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      *cycles = 4;
	      sprintf (op, "%d(r%d)", dst, regd);
	    }
	}
      break;

    case 2:	/* rrc, push, call, swpb, rra, sxt, push, call, reti etc...  */

      if (as == 0)
	{
	  if (regd == 3)
	    {
	      /* Constsnts.  */
	      sprintf (op, "#0");
	      sprintf (comm, "r3 As==00");
	    }
	  else
	    {
	      /* Register.  */
	      sprintf (op, "r%d", regd);
	    }
	  *cycles = 1;
	}
      else if (as == 2)
	{
	  *cycles = 1;
	  if (regd == 2)
	    {
	      sprintf (op, "#4");
	      sprintf (comm, "r2 As==10");
	    }
	  else if (regd == 3)
	    {
	      sprintf (op, "#2");
	      sprintf (comm, "r3 As==10");
	    }
	  else
	    {
	      *cycles = 3;
	      /* Indexed register mode @Rn.  */
	      sprintf (op, "@r%d", regd);
	    }
	}
      else if (as == 3)
	{
	  *cycles = 1;
	  if (regd == 2)
	    {
	      sprintf (op, "#8");
	      sprintf (comm, "r2 As==11");
	    }
	  else if (regd == 3)
	    {
	      sprintf (op, "#-1");
	      sprintf (comm, "r3 As==11");
	    }
	  else if (regd == 0)
	    {
	      *cycles = 3;
	      /* absolute. @pc+ */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      sprintf (op, "#%d", dst);
	      sprintf (comm, "#0x%04x", PS (dst));
	    }
	  else
	    {
	      *cycles = 3;
	      sprintf (op, "@r%d+", regd);
	    }
	}
      else if (as == 1)
	{
	  *cycles = 4;
	  if (regd == 0)
	    {
	      /* PC relative.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      sprintf (op, "0x%04x", PS (dst));
	      sprintf (comm, "PC rel. 0x%04x",
		       PS ((short) addr + 2 + dst));
	    }
	  else if (regd == 2)
	    {
	      /* Absolute.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      sprintf (op, "&0x%04x", PS (dst));
	    }
	  else if (regd == 3)
	    {
	      *cycles = 1;
	      sprintf (op, "#1");
	      sprintf (comm, "r3 As==01");
	    }
	  else
	    {
	      /* Indexd.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 2;
	      sprintf (op, "%d(r%d)", dst, regd);
	    }
	}
      break;

    case 3:			/* Jumps.  */
      where = insn & 0x03ff;
      if (where & 0x200)
	where |= ~0x03ff;
      if (where > 512 || where < -511)
	return 0;

      where *= 2;
      sprintf (op, "$%+-8d", where + 2);
      sprintf (comm, "abs 0x%x", PS ((short) (addr) + 2 + where));
      *cycles = 2;
      return 2;
      break;
    default:
      cmd_len = 0;
    }

  return cmd_len;
}

int
msp430_doubleoperand (info, opcode, addr, insn, op1, op2, comm1, comm2, cycles)
     disassemble_info *info;
     struct msp430_opcode_s *opcode;
     bfd_vma addr;
     unsigned short insn;
     char *op1, *op2;
     char *comm1, *comm2;
     int *cycles;
{
  int regs = 0, regd = 0;
  int ad = 0, as = 0;
  int cmd_len = 2;
  short dst = 0;

  regd = insn & 0x0f;
  regs = (insn & 0x0f00) >> 8;
  as = (insn & 0x0030) >> 4;
  ad = (insn & 0x0080) >> 7;

  if (opcode->fmt == 0)
    {
      /* Special case: rla and rlc are the only 2 emulated instructions that
	 fall into two operand instructions.  */
      /* With dst, there are only:
	 Rm       	Register,
         x(Rm)     	Indexed,
         0xXXXX    	Relative,
         &0xXXXX    	Absolute 
         emulated_ins   dst
         basic_ins      dst, dst.  */

      if (regd != regs || as != ad)
	return 0;		/* May be 'data' section.  */

      if (ad == 0)
	{
	  /* Register mode.  */
	  if (regd == 3)
	    {
	      strcpy (comm1, "Illegal as emulation instr");
	      return -1;
	    }

	  sprintf (op1, "r%d", regd);
	  *cycles = 1;
	}
      else			/* ad == 1 */
	{
	  if (regd == 0)
	    {
	      /* PC relative, Symbolic.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 4;
	      *cycles = 6;
	      sprintf (op1, "0x%04x", PS (dst));
	      sprintf (comm1, "PC rel. 0x%04x",
		       PS ((short) addr + 2 + dst));

	    }
	  else if (regd == 2)
	    {
	      /* Absolute.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      /* If the 'src' field is not the same as the dst
		 then this is not an rla instruction.  */
	      if (dst != msp430dis_opcode (addr + 4, info))
		return 0;
	      cmd_len += 4;
	      *cycles = 6;
	      sprintf (op1, "&0x%04x", PS (dst));
	    }
	  else
	    {
	      /* Indexed.  */
	      dst = msp430dis_opcode (addr + 2, info);
	      cmd_len += 4;
	      *cycles = 6;
	      sprintf (op1, "%d(r%d)", dst, regd);
	    }
	}

      *op2 = 0;
      *comm2 = 0;
      return cmd_len;
    }

  /* Two operands exactly.  */
  if (ad == 0 && regd == 3)
    {
      /* R2/R3 are illegal as dest: may be data section.  */
      strcpy (comm1, "Illegal as 2-op instr");
      return -1;
    }

  /* Source.  */
  if (as == 0)
    {
      *cycles = 1;
      if (regs == 3)
	{
	  /* Constsnts.  */
	  sprintf (op1, "#0");
	  sprintf (comm1, "r3 As==00");
	}
      else
	{
	  /* Register.  */
	  sprintf (op1, "r%d", regs);
	}
    }
  else if (as == 2)
    {
      *cycles = 1;

      if (regs == 2)
	{
	  sprintf (op1, "#4");
	  sprintf (comm1, "r2 As==10");
	}
      else if (regs == 3)
	{
	  sprintf (op1, "#2");
	  sprintf (comm1, "r3 As==10");
	}
      else
	{
	  *cycles = 2;

	  /* Indexed register mode @Rn.  */
	  sprintf (op1, "@r%d", regs);
	}
      if (!regs)
	*cycles = 3;
    }
  else if (as == 3)
    {
      if (regs == 2)
	{
	  sprintf (op1, "#8");
	  sprintf (comm1, "r2 As==11");
	  *cycles = 1;
	}
      else if (regs == 3)
	{
	  sprintf (op1, "#-1");
	  sprintf (comm1, "r3 As==11");
	  *cycles = 1;
	}
      else if (regs == 0)
	{
	  *cycles = 3;
	  /* Absolute. @pc+  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "#%d", dst);
	  sprintf (comm1, "#0x%04x", PS (dst));
	}
      else
	{
	  *cycles = 2;
	  sprintf (op1, "@r%d+", regs);
	}
    }
  else if (as == 1)
    {
      if (regs == 0)
	{
	  *cycles = 4;
	  /* PC relative.  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "0x%04x", PS (dst));
	  sprintf (comm1, "PC rel. 0x%04x",
		   PS ((short) addr + 2 + dst));
	}
      else if (regs == 2)
	{
	  *cycles = 2;
	  /* Absolute.  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "&0x%04x", PS (dst));
	  sprintf (comm1, "0x%04x", PS (dst));
	}
      else if (regs == 3)
	{
	  *cycles = 1;
	  sprintf (op1, "#1");
	  sprintf (comm1, "r3 As==01");
	}
      else
	{
	  *cycles = 3;
	  /* Indexed.  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "%d(r%d)", dst, regs);
	}
    }

  /* Destination. Special care needed on addr + XXXX.  */

  if (ad == 0)
    {
      /* Register.  */
      if (regd == 0)
	{
	  *cycles += 1;
	  sprintf (op2, "r0");
	}
      else if (regd == 1)
	sprintf (op2, "r1");

      else if (regd == 2)
	sprintf (op2, "r2");

      else
	sprintf (op2, "r%d", regd);
    }
  else				/* ad == 1.  */
    {
      * cycles += 3;

      if (regd == 0)
	{
	  /* PC relative.  */
	  *cycles += 1;
	  dst = msp430dis_opcode (addr + cmd_len, info);
	  sprintf (op2, "0x%04x", PS (dst));
	  sprintf (comm2, "PC rel. 0x%04x",
		   PS ((short) addr + cmd_len + dst));
	  cmd_len += 2;
	}
      else if (regd == 2)
	{
	  /* Absolute.  */
	  dst = msp430dis_opcode (addr + cmd_len, info);
	  cmd_len += 2;
	  sprintf (op2, "&0x%04x", PS (dst));
	}
      else
	{
	  dst = msp430dis_opcode (addr + cmd_len, info);
	  cmd_len += 2;
	  sprintf (op2, "%d(r%d)", dst, regd);
	}
    }

  return cmd_len;
}


int
msp430_branchinstr (info, opcode, addr, insn, op1, comm1, cycles)
     disassemble_info *info;
     struct msp430_opcode_s *opcode ATTRIBUTE_UNUSED;
     bfd_vma addr ATTRIBUTE_UNUSED;
     unsigned short insn;
     char *op1;
     char *comm1;
     int *cycles;
{
  int regs = 0, regd = 0;
  int ad = 0, as = 0;
  int cmd_len = 2;
  short dst = 0;

  regd = insn & 0x0f;
  regs = (insn & 0x0f00) >> 8;
  as = (insn & 0x0030) >> 4;
  ad = (insn & 0x0080) >> 7;

  if (regd != 0)	/* Destination register is not a PC.  */
    return 0;

  /* dst is a source register.  */
  if (as == 0)
    {
      /* Constants.  */
      if (regs == 3)
	{
	  *cycles = 1;
	  sprintf (op1, "#0");
	  sprintf (comm1, "r3 As==00");
	}
      else
	{
	  /* Register.  */
	  *cycles = 1;
	  sprintf (op1, "r%d", regs);
	}
    }
  else if (as == 2)
    {
      if (regs == 2)
	{
	  *cycles = 2;
	  sprintf (op1, "#4");
	  sprintf (comm1, "r2 As==10");
	}
      else if (regs == 3)
	{
	  *cycles = 1;
	  sprintf (op1, "#2");
	  sprintf (comm1, "r3 As==10");
	}
      else
	{
	  /* Indexed register mode @Rn.  */
	  *cycles = 2;
	  sprintf (op1, "@r%d", regs);
	}
    }
  else if (as == 3)
    {
      if (regs == 2)
	{
	  *cycles = 1;
	  sprintf (op1, "#8");
	  sprintf (comm1, "r2 As==11");
	}
      else if (regs == 3)
	{
	  *cycles = 1;
	  sprintf (op1, "#-1");
	  sprintf (comm1, "r3 As==11");
	}
      else if (regs == 0)
	{
	  /* Absolute. @pc+  */
	  *cycles = 3;
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "#0x%04x", PS (dst));
	}
      else
	{
	  *cycles = 2;
	  sprintf (op1, "@r%d+", regs);
	}
    }
  else if (as == 1)
    {
      * cycles = 3;

      if (regs == 0)
	{
	  /* PC relative.  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  (*cycles)++;
	  sprintf (op1, "0x%04x", PS (dst));
	  sprintf (comm1, "PC rel. 0x%04x",
		   PS ((short) addr + 2 + dst));
	}
      else if (regs == 2)
	{
	  /* Absolute.  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "&0x%04x", PS (dst));
	}
      else if (regs == 3)
	{
	  (*cycles)--;
	  sprintf (op1, "#1");
	  sprintf (comm1, "r3 As==01");
	}
      else
	{
	  /* Indexd.  */
	  dst = msp430dis_opcode (addr + 2, info);
	  cmd_len += 2;
	  sprintf (op1, "%d(r%d)", dst, regs);
	}
    }

  return cmd_len;
}
