/* Instruction printing code for the MAXQ

   Copyright 2004, 2005 Free Software Foundation, Inc.

   Written by Vineet Sharma(vineets@noida.hcltech.com) Inderpreet
   S.(inderpreetb@noida.hcltech.com)

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free 
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc., 
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/maxq.h"

struct _group_info
{
  unsigned char group_no;
  unsigned char sub_opcode;
  unsigned char src;
  unsigned char dst;
  unsigned char fbit;
  unsigned char bit_no;
  unsigned char flag;

};

typedef struct _group_info group_info;

#define SRC	0x01
#define DST	0x02
#define FORMAT	0x04
#define BIT_NO	0x08
#define SUB_OP	0x10

#define MASK_LOW_BYTE 0x0f
#define MASK_HIGH_BYTE 0xf0

/* Flags for retrieving the bits from the op-code.  */
#define _DECODE_LOWNIB_LOWBYTE  0x000f
#define _DECODE_HIGHNIB_LOWBYTE 0x00f0
#define _DECODE_LOWNIB_HIGHBYTE 0x0f00
#define _DECODE_HIGHNIB_HIGHBYTE 0xf000
#define _DECODE_HIGHBYTE 0xff00
#define _DECODE_LOWBYTE  0x00ff
#define _DECODE_4TO6_HIGHBYTE 0x7000
#define _DECODE_4TO6_LOWBYTE 0x0070
#define _DECODE_0TO6_HIGHBYTE 0x7f00
#define _DECODE_0TO2_HIGHBYTE 0x0700
#define _DECODE_GET_F_HIGHBYTE 0x8000
#define _DECODE_BIT7_HIGHBYTE 0x8000
#define _DECODE_BIT7_LOWBYTE 0x0080
#define _DECODE_GET_CARRY 0x10000
#define _DECODE_BIT0_LOWBYTE 0x1
#define _DECODE_BIT6AND7_HIGHBYTE 0xc000

/* Module and Register Indexed of System Registers.  */
#define _CURR_ACC_MODINDEX 0xa
#define _CURR_ACC_REGINDEX 0x0
#define _PSF_REG_MODINDEX  0x8
#define _PSF_REG_REGINDEX  0x4
#define _PFX_REG_MODINDEX  0xb
#define _PFX0_REG_REGINDEX 0x0
#define _PFX2_REG_REGINDEX 0x2
#define _DP_REG_MODINDEX   0xf
#define _DP0_REG_REGINDEX  0x3
#define _DP1_REG_REGINDEX  0x7
#define _IP_REG_MODINDEX   0xc
#define _IP_REG_REGINDEX   0x0
#define _IIR_REG_MODINDEX  0x8
#define _IIR_REG_REGINDEX  0xb
#define _SP_REG_MODINDEX   0xd
#define _SP_REG_REGINDEX   0x1
#define _IC_REG_MODINDEX   0x8
#define _IC_REG_REGINDEX   0x5
#define _LC_REG_MODINDEX   0xe
#define _LC0_REG_REGINDEX  0x0
#define _LC1_REG_REGINDEX  0x1
#define _LC2_REG_REGINDEX  0x2
#define _LC3_REG_REGINDEX  0x3

/* Flags for finding the bits in PSF Register.  */
#define SIM_ALU_DECODE_CARRY_BIT_POS  0x2
#define SIM_ALU_DECODE_SIGN_BIT_POS   0x40
#define SIM_ALU_DECODE_ZERO_BIT_POS   0x80
#define SIM_ALU_DECODE_EQUAL_BIT_POS  0x1
#define SIM_ALU_DECODE_IGE_BIT_POS    0x1

/* Number Of Op-code Groups.  */
unsigned char const SIM_ALU_DECODE_OPCODE_GROUPS = 11;

/* Op-code Groups.  */
unsigned char const SIM_ALU_DECODE_LOGICAL_XCHG_OP_GROUP = 1;

/* Group1: AND/OR/XOR/ADD/SUB Operations: fxxx 1010 ssss ssss.  */
unsigned char const SIM_ALU_DECODE_AND_OR_ADD_SUB_OP_GROUP = 2;

/* Group2: Logical Operations: 1000 1010 xxxx 1010.  */
unsigned char const SIM_ALU_DECODE_BIT_OP_GROUP = 3;

/* XCHG/Bit Operations: 1xxx 1010 xxxx 1010.  */
unsigned char const SIM_ALU_DECODE_SET_DEST_BIT_GROUP = 4;

/* Move value in bit of destination register: 1ddd dddd xbbb 0111.  */
unsigned char const SIM_ALU_DECODE_JUMP_OP_GROUP = 5;

#define JUMP_CHECK(insn)   \
   (   ((insn & _DECODE_4TO6_HIGHBYTE) == 0x0000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x2000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x6000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x1000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x5000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x3000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x7000) \
    || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x4000) )

/* JUMP operations: fxxx 1100 ssss ssss */
unsigned char const SIM_ALU_DECODE_RET_OP_GROUP = 6;

/* RET Operations: 1xxx 1100 0000 1101 */
unsigned char const SIM_ALU_DECODE_MOVE_SRC_DST_GROUP = 7;

/* Move src into dest register: fddd dddd ssss ssss */
unsigned char const SIM_ALU_DECODE_SET_SRC_BIT_GROUP = 8;

/* Move value in bit of source register: fbbb 0111 ssss ssss */
unsigned char const SIM_ALU_DECODE_DJNZ_CALL_PUSH_OP_GROUP = 9;

/* PUSH, DJNZ and CALL operations: fxxx 1101 ssss ssss */
unsigned char const SIM_ALU_DECODE_POP_OP_GROUP = 10;

/* POP operation: 1ddd dddd 0000 1101 */
unsigned char const SIM_ALU_DECODE_CMP_SRC_OP_GROUP = 11;

/* GLOBAL */
char unres_reg_name[20];

static char *
get_reg_name (unsigned char reg_code, type1 arg_pos)
{
  unsigned char module;
  unsigned char index;
  int ix = 0;
  reg_entry const *reg_x;
  mem_access_syntax const *syntax;
  mem_access *mem_acc;

  module = 0;
  index = 0;
  module = (reg_code & MASK_LOW_BYTE);
  index = (reg_code & MASK_HIGH_BYTE);
  index = index >> 4;

  /* Search the system register table.  */
  for (reg_x = &system_reg_table[0]; reg_x->reg_name != NULL; ++reg_x)
    if ((reg_x->Mod_name == module) && (reg_x->Mod_index == index))
      return reg_x->reg_name;

  /* Serch pheripheral table.  */
  for (ix = 0; ix < num_of_reg; ix++)
    {
      reg_x = &new_reg_table[ix];

      if ((reg_x->Mod_name == module) && (reg_x->Mod_index == index))
	return reg_x->reg_name;
    }

  for (mem_acc = &mem_table[0]; mem_acc->name != NULL || !mem_acc; ++mem_acc)
    {
      if (reg_code == mem_acc->opcode)
	{
	  for (syntax = mem_access_syntax_table;
	       mem_access_syntax_table != NULL || mem_access_syntax_table->name;
	       ++syntax)
	    if (!strcmp (mem_acc->name, syntax->name))
	      {
		if ((arg_pos == syntax->type) || (syntax->type == BOTH))
		  return mem_acc->name;

		break;
	      }
	}
    }

  memset (unres_reg_name, 0, 20);
  sprintf (unres_reg_name, "%01x%01xh", index, module);

  return unres_reg_name;
}

static bfd_boolean
check_move (unsigned char insn0, unsigned char insn8)
{
  bfd_boolean first = FALSE;
  bfd_boolean second = FALSE;
  char *first_reg;
  char *second_reg;
  reg_entry const *reg_x;
  const unsigned char module1 = insn0 & MASK_LOW_BYTE;
  const unsigned char index1 = ((insn0 & 0x70) >> 4);
  const unsigned char module2 = insn8 & MASK_LOW_BYTE;
  const unsigned char index2 = ((insn8 & MASK_HIGH_BYTE) >> 4);

  /* DST */
  if (((insn0 & MASK_LOW_BYTE) == MASK_LOW_BYTE)
      && ((index1 == 0) || (index1 == 1) || (index1 == 2) || (index1 == 5)
	  || (index1 == 4) || (index1 == 6)))
    first = TRUE;

  else if (((insn0 & MASK_LOW_BYTE) == 0x0D) && (index1 == 0))
    first = TRUE;

  else if ((module1 == 0x0E)
	   && ((index1 == 0) || (index1 == 1) || (index1 == 2)))
    first = TRUE;

  else
    {
      for (reg_x = &system_reg_table[0]; reg_x->reg_name != NULL && reg_x;
	   ++reg_x)
	{
	  if ((reg_x->Mod_name == module1) && (reg_x->Mod_index == index1)
	      && ((reg_x->rtype == Reg_16W) || (reg_x->rtype == Reg_8W)))
	    {
	      /* IP not allowed.  */
	      if ((reg_x->Mod_name == 0x0C) && (reg_x->Mod_index == 0x00))
		continue;

	      /* A[AP] not allowed.  */
	      if ((reg_x->Mod_name == 0x0A) && (reg_x->Mod_index == 0x01))
		continue;
	      first_reg = reg_x->reg_name;
	      first = TRUE;
	      break;
	    }
	}
    }

  if (!first)
    /* No need to check further.  */
    return FALSE;

  if (insn0 & 0x80)
    {
      /* SRC */
      if (((insn8 & MASK_LOW_BYTE) == MASK_LOW_BYTE)
	  && ((index2 == 0) || (index2 == 1) || (index2 == 2) || (index2 == 4)
	      || (index2 == 5) || (index2 == 6)))
	second = TRUE;

      else if (((insn8 & MASK_LOW_BYTE) == 0x0D) && (index2 == 0))
	second = TRUE;

      else if ((module2 == 0x0E)
	       && ((index2 == 0) || (index2 == 1) || (index2 == 2)))
	second = TRUE;

      else
	{
	  for (reg_x = &system_reg_table[0];
	       reg_x->reg_name != NULL && reg_x;
	       ++reg_x)
	    {
	      if ((reg_x->Mod_name == (insn8 & MASK_LOW_BYTE))
		  && (reg_x->Mod_index == (((insn8 & 0xf0) >> 4))))
		{
		  second = TRUE;
		  second_reg = reg_x->reg_name;
		  break;
		}
	    }
	}	

      if (second)
	{
	  if ((module1 == 0x0A && index1 == 0x0)
	      && (module2 == 0x0A && index2 == 0x01))
	    return FALSE;

	  return TRUE;
	}

      return FALSE;
    }

  return first;
}

static void
maxq_print_arg (MAX_ARG_TYPE              arg,
		struct disassemble_info * info,
		group_info                grp)
{
  switch (arg)
    {
    case FLAG_C:
      info->fprintf_func (info->stream, "C");
      break;
    case FLAG_NC:
      info->fprintf_func (info->stream, "NC");
      break;

    case FLAG_Z:
      info->fprintf_func (info->stream, "Z");
      break;

    case FLAG_NZ:
      info->fprintf_func (info->stream, "NZ");
      break;

    case FLAG_S:
      info->fprintf_func (info->stream, "S");
      break;

    case FLAG_E:
      info->fprintf_func (info->stream, "E");
      break;

    case FLAG_NE:
      info->fprintf_func (info->stream, "NE");
      break;

    case ACC_BIT:
      info->fprintf_func (info->stream, "Acc");
      if ((grp.flag & BIT_NO) == BIT_NO)
	info->fprintf_func (info->stream, ".%d", grp.bit_no);
      break;

    case A_BIT_0:
      info->fprintf_func (info->stream, "#0");
      break;
    case A_BIT_1:
      info->fprintf_func (info->stream, "#1");
      break;

    default:
      break;
    }
}

static unsigned char
get_group (const unsigned int insn)
{
  if (check_move ((insn >> 8), (insn & _DECODE_LOWBYTE)))
    return 8;

  if ((insn & _DECODE_LOWNIB_HIGHBYTE) == 0x0A00)
    {
      /* && condition with sec part added on 26 May for resolving 2 & 3 grp
	 conflict.  */
      if (((insn & _DECODE_LOWNIB_LOWBYTE) == 0x000A)
	  && ((insn & _DECODE_GET_F_HIGHBYTE) == 0x8000))
	{
	  if ((insn & _DECODE_HIGHNIB_HIGHBYTE) == 0x8000)
	    return 2;
	  else
	    return 3;
	}

      return 1;
    }
  else if ((insn & _DECODE_LOWNIB_HIGHBYTE) == 0x0C00)
    {
      if (((insn & _DECODE_LOWBYTE) == 0x000D) && JUMP_CHECK (insn)
	  && ((insn & _DECODE_GET_F_HIGHBYTE) == 0x8000))
	return 6;
      else if ((insn & _DECODE_LOWBYTE) == 0x008D)
	return 7;

      return 5;
    }
  else if (((insn & _DECODE_LOWNIB_HIGHBYTE) == 0x0D00)
	   && (((insn & _DECODE_4TO6_HIGHBYTE) == 0x3000)
	       || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x4000)
	       || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x5000)
	       || ((insn & _DECODE_4TO6_HIGHBYTE) == 0x0000)))
    return 10;

  else if ((insn & _DECODE_LOWBYTE) == 0x000D)
    return 11;

  else if ((insn & _DECODE_LOWBYTE) == 0x008D)
    return 12;

  else if ((insn & _DECODE_0TO6_HIGHBYTE) == 0x7800)
    return 13;

  else if ((insn & _DECODE_LOWNIB_HIGHBYTE) == 0x0700)
    return 9;

  else if (((insn & _DECODE_LOWNIB_LOWBYTE) == 0x0007)
	   && ((insn & _DECODE_GET_F_HIGHBYTE) == 0x8000))
    return 4;

  return 8;
}

static void
get_insn_opcode (const unsigned int insn, group_info *i)
{
  static unsigned char pfx_flag = 0;
  static unsigned char count_for_pfx = 0;

  i->flag ^= i->flag;
  i->bit_no ^= i->bit_no;
  i->dst ^= i->dst;
  i->fbit ^= i->fbit;
  i->group_no ^= i->group_no;
  i->src ^= i->src;
  i->sub_opcode ^= i->sub_opcode;

  if (count_for_pfx > 0)
    count_for_pfx++;

  if (((insn >> 8) == 0x0b) || ((insn >> 8) == 0x2b))
    {
      pfx_flag = 1;
      count_for_pfx = 1;
    }

  i->group_no = get_group (insn);

  if (pfx_flag && (i->group_no == 0x0D) && (count_for_pfx == 2)
      && ((insn & _DECODE_0TO6_HIGHBYTE) == 0x7800))
    {
      i->group_no = 0x08;
      count_for_pfx = 0;
      pfx_flag ^= pfx_flag;
    }

  switch (i->group_no)
    {
    case 1:
      i->sub_opcode = ((insn & _DECODE_4TO6_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      i->src = ((insn & _DECODE_LOWBYTE));
      i->flag |= SRC;
      i->fbit = ((insn & _DECODE_GET_F_HIGHBYTE) >> 15);
      i->flag |= FORMAT;
      break;

    case 2:
      i->sub_opcode = ((insn & _DECODE_HIGHNIB_LOWBYTE) >> 4);
      i->flag |= SUB_OP;
      break;

    case 3:
      i->sub_opcode = ((insn & _DECODE_HIGHNIB_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      i->bit_no = ((insn & _DECODE_HIGHNIB_LOWBYTE) >> 4);
      i->flag |= BIT_NO;
      break;

    case 4:
      i->sub_opcode = ((insn & _DECODE_BIT7_LOWBYTE) >> 7);
      i->flag |= SUB_OP;
      i->dst = ((insn & _DECODE_0TO6_HIGHBYTE) >> 8);
      i->flag |= DST;
      i->bit_no = ((insn & _DECODE_4TO6_LOWBYTE) >> 4);
      i->flag |= BIT_NO;
      break;

    case 5:
      i->sub_opcode = ((insn & _DECODE_4TO6_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      i->src = ((insn & _DECODE_LOWBYTE));
      i->flag |= SRC;
      i->fbit = ((insn & _DECODE_GET_F_HIGHBYTE) >> 15);
      i->flag |= FORMAT;
      break;

    case 6:
      i->sub_opcode = ((insn & _DECODE_HIGHNIB_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      break;

    case 7:
      i->sub_opcode = ((insn & _DECODE_HIGHNIB_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      break;

    case 8:
      i->dst = ((insn & _DECODE_0TO6_HIGHBYTE) >> 8);
      i->flag |= DST;
      i->src = ((insn & _DECODE_LOWBYTE));
      i->flag |= SRC;
      i->fbit = ((insn & _DECODE_GET_F_HIGHBYTE) >> 15);
      i->flag |= FORMAT;
      break;

    case 9:
      i->sub_opcode = ((insn & _DECODE_0TO2_HIGHBYTE) >> 8);
      i->flag |= SUB_OP;
      i->bit_no = ((insn & _DECODE_4TO6_HIGHBYTE) >> 12);
      i->flag |= BIT_NO;
      i->fbit = ((insn & _DECODE_GET_F_HIGHBYTE) >> 15);
      i->flag |= FORMAT;
      i->src = ((insn & _DECODE_LOWBYTE));
      i->flag |= SRC;
      break;

    case 10:
      i->sub_opcode = ((insn & _DECODE_4TO6_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      i->src = ((insn & _DECODE_LOWBYTE));
      i->flag |= SRC;
      i->fbit = ((insn & _DECODE_GET_F_HIGHBYTE) >> 15);
      i->flag |= FORMAT;
      break;

    case 11:
      i->dst = ((insn & _DECODE_0TO6_HIGHBYTE) >> 8);
      i->flag |= DST;
      break;

    case 12:
      i->dst = ((insn & _DECODE_0TO6_HIGHBYTE) >> 8);
      i->flag |= DST;
      break;

    case 13:
      i->sub_opcode = ((insn & _DECODE_4TO6_HIGHBYTE) >> 12);
      i->flag |= SUB_OP;
      i->src = ((insn & _DECODE_LOWBYTE));
      i->flag |= SRC;
      i->fbit = ((insn & _DECODE_GET_F_HIGHBYTE) >> 15);
      i->flag |= FORMAT;
      break;

    }
  return;
}


/* Print one instruction from MEMADDR on INFO->STREAM. Return the size of the 
   instruction (always 2 on MAXQ20).  */

static int
print_insn (bfd_vma memaddr, struct disassemble_info *info,
	    enum bfd_endian endianess)
{
  /* The raw instruction.  */
  unsigned char insn[2], insn0, insn8, derived_code;
  unsigned int format;
  unsigned int actual_operands;
  unsigned int i;
  /* The group_info collected/decoded.  */
  group_info grp;
  MAXQ20_OPCODE_INFO const *opcode;
  int status;

  format = 0;

  status = info->read_memory_func (memaddr, (bfd_byte *) & insn[0], 2, info);

  if (status != 0)
    {
      info->memory_error_func (status, memaddr, info);
      return -1;
    }

  insn8 = insn[1];
  insn0 = insn[0];

  /* FIXME: Endianness always little.  */
  if (endianess == BFD_ENDIAN_BIG)
    get_insn_opcode (((insn[0] << 8) | (insn[1])), &grp);
  else
    get_insn_opcode (((insn[1] << 8) | (insn[0])), &grp);

  derived_code = ((grp.group_no << 4) | grp.sub_opcode);

  if (insn[0] == 0 && insn[1] == 0)
    {
      info->fprintf_func (info->stream, "00 00");
      return 2;
    }

  /* The opcode is always in insn0.  */
  for (opcode = &op_table[0]; opcode->name != NULL; ++opcode)
    {
      if (opcode->instr_id == derived_code)
	{
	  if (opcode->instr_id == 0x3D)
	    {
	      if ((grp.bit_no == 0) && (opcode->arg[1] != A_BIT_0))
		continue;
	      if ((grp.bit_no == 1) && (opcode->arg[1] != A_BIT_1))
		continue;
	      if ((grp.bit_no == 3) && (opcode->arg[0] != 0))
		continue;
	    }

	  info->fprintf_func (info->stream, "%s ", opcode->name);

	  actual_operands = 0;

	  if ((grp.flag & SRC) == SRC)
	    actual_operands++;

	  if ((grp.flag & DST) == DST)
	    actual_operands++;

	  /* If Implict FLAG in the Instruction.  */
	  if ((opcode->op_number > actual_operands)
	      && !((grp.flag & SRC) == SRC) && !((grp.flag & DST) == DST))
	    {
	      for (i = 0; i < opcode->op_number; i++)
		{
		  if (i == 1 && (opcode->arg[1] != NO_ARG))
		    info->fprintf_func (info->stream, ",");
		  maxq_print_arg (opcode->arg[i], info, grp);
		}
	    }

	  /* DST is ABSENT in the grp.  */
	  if ((opcode->op_number > actual_operands)
	      && ((grp.flag & SRC) == SRC))
	    {
	      maxq_print_arg (opcode->arg[0], info, grp);
	      info->fprintf_func (info->stream, " ");

	      if (opcode->instr_id == 0xA4)
		info->fprintf_func (info->stream, "LC[0]");

	      if (opcode->instr_id == 0xA5)
		info->fprintf_func (info->stream, "LC[1]");

	      if ((grp.flag & SRC) == SRC)
		info->fprintf_func (info->stream, ",");
	    }

	  if ((grp.flag & DST) == DST)
	    {
	      if ((grp.flag & BIT_NO) == BIT_NO)
		{
		  info->fprintf_func (info->stream, " %s.%d",
				      get_reg_name (grp.dst,
						    (type1) 0 /*DST*/),
				      grp.bit_no);
		}
	      else
		info->fprintf_func (info->stream, " %s",
				    get_reg_name (grp.dst, (type1) 0));
	    }

	  /* SRC is ABSENT in the grp.  */
	  if ((opcode->op_number > actual_operands)
	      && ((grp.flag & DST) == DST))
	    {
	      info->fprintf_func (info->stream, ",");
	      maxq_print_arg (opcode->arg[1], info, grp);
	      info->fprintf_func (info->stream, " ");
	    }

	  if ((grp.flag & SRC) == SRC)
	    {
	      if ((grp.flag & DST) == DST)
		info->fprintf_func (info->stream, ",");

	      if ((grp.flag & BIT_NO) == BIT_NO)
		{
		  format = opcode->format;

		  if ((grp.flag & FORMAT) == FORMAT)
		    format = grp.fbit;
		  if (format == 1)
		    info->fprintf_func (info->stream, " %s.%d",
					get_reg_name (grp.src,
						      (type1) 1 /*SRC*/),
					grp.bit_no);
		  if (format == 0)
		    info->fprintf_func (info->stream, " #%02xh.%d",
					grp.src, grp.bit_no);
		}
	      else
		{
		  format = opcode->format;

		  if ((grp.flag & FORMAT) == FORMAT)
		    format = grp.fbit;
		  if (format == 1)
		    info->fprintf_func (info->stream, " %s",
					get_reg_name (grp.src,
						      (type1) 1 /*SRC*/));
		  if (format == 0)
		    info->fprintf_func (info->stream, " #%02xh",
					(grp.src));
		}
	    }

	  return 2;
	}
    }

  info->fprintf_func (info->stream, "Unable to Decode :  %02x %02x",
		      insn[0], insn[1]);
  return 2;			
}

int
print_insn_maxq_little (bfd_vma memaddr, struct disassemble_info *info)
{
  return print_insn (memaddr, info, BFD_ENDIAN_LITTLE);
}
