/* Disassembly routines for TMS320C30 architecture
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.
   Contributed by Steven Haworth (steve@pm.cse.rmit.edu.au)

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <errno.h>
#include <math.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opcode/tic30.h"

#define NORMAL_INSN   1
#define PARALLEL_INSN 2

/* Gets the type of instruction based on the top 2 or 3 bits of the
   instruction word. */
#define GET_TYPE(insn) (insn & 0x80000000 ? insn & 0xC0000000 : insn & 0xE0000000)

/* Instruction types. */
#define TWO_OPERAND_1 0x00000000
#define TWO_OPERAND_2 0x40000000
#define THREE_OPERAND 0x20000000
#define PAR_STORE     0xC0000000
#define MUL_ADDS      0x80000000
#define BRANCHES      0x60000000

/* Specific instruction id bits. */
#define NORMAL_IDEN    0x1F800000
#define PAR_STORE_IDEN 0x3E000000
#define MUL_ADD_IDEN   0x2C000000
#define BR_IMM_IDEN    0x1F000000
#define BR_COND_IDEN   0x1C3F0000

/* Addressing modes. */
#define AM_REGISTER 0x00000000
#define AM_DIRECT   0x00200000
#define AM_INDIRECT 0x00400000
#define AM_IMM      0x00600000

#define P_FIELD 0x03000000

#define REG_AR0 0x08
#define LDP_INSN 0x08700000

/* TMS320C30 program counter for current instruction. */
static unsigned int _pc;

struct instruction
  {
    int type;
    template *tm;
    partemplate *ptm;
  };

int get_tic30_instruction PARAMS ((unsigned long, struct instruction *));
int print_two_operand
  PARAMS ((disassemble_info *, unsigned long, struct instruction *));
int print_three_operand
  PARAMS ((disassemble_info *, unsigned long, struct instruction *));
int print_par_insn
  PARAMS ((disassemble_info *, unsigned long, struct instruction *));
int print_branch
  PARAMS ((disassemble_info *, unsigned long, struct instruction *));
int get_indirect_operand PARAMS ((unsigned short, int, char *));
int get_register_operand PARAMS ((unsigned char, char *));
int cnvt_tmsfloat_ieee PARAMS ((unsigned long, int, float *));

int
print_insn_tic30 (pc, info)
     bfd_vma pc;
     disassemble_info *info;
{
  unsigned long insn_word;
  struct instruction insn =
  {0, NULL, NULL};
  bfd_vma bufaddr = pc - info->buffer_vma;
  /* Obtain the current instruction word from the buffer. */
  insn_word = (*(info->buffer + bufaddr) << 24) | (*(info->buffer + bufaddr + 1) << 16) |
    (*(info->buffer + bufaddr + 2) << 8) | *(info->buffer + bufaddr + 3);
  _pc = pc / 4;
  /* Get the instruction refered to by the current instruction word
     and print it out based on its type. */
  if (!get_tic30_instruction (insn_word, &insn))
    return -1;
  switch (GET_TYPE (insn_word))
    {
    case TWO_OPERAND_1:
    case TWO_OPERAND_2:
      if (!print_two_operand (info, insn_word, &insn))
	return -1;
      break;
    case THREE_OPERAND:
      if (!print_three_operand (info, insn_word, &insn))
	return -1;
      break;
    case PAR_STORE:
    case MUL_ADDS:
      if (!print_par_insn (info, insn_word, &insn))
	return -1;
      break;
    case BRANCHES:
      if (!print_branch (info, insn_word, &insn))
	return -1;
      break;
    }
  return 4;
}

int
get_tic30_instruction (insn_word, insn)
     unsigned long insn_word;
     struct instruction *insn;
{
  switch (GET_TYPE (insn_word))
    {
    case TWO_OPERAND_1:
    case TWO_OPERAND_2:
    case THREE_OPERAND:
      insn->type = NORMAL_INSN;
      {
	template *current_optab = (template *) tic30_optab;
	for (; current_optab < tic30_optab_end; current_optab++)
	  {
	    if (GET_TYPE (current_optab->base_opcode) == GET_TYPE (insn_word))
	      {
		if (current_optab->operands == 0)
		  {
		    if (current_optab->base_opcode == insn_word)
		      {
			insn->tm = current_optab;
			break;
		      }
		  }
		else if ((current_optab->base_opcode & NORMAL_IDEN) == (insn_word & NORMAL_IDEN))
		  {
		    insn->tm = current_optab;
		    break;
		  }
	      }
	  }
      }
      break;
    case PAR_STORE:
      insn->type = PARALLEL_INSN;
      {
	partemplate *current_optab = (partemplate *) tic30_paroptab;
	for (; current_optab < tic30_paroptab_end; current_optab++)
	  {
	    if (GET_TYPE (current_optab->base_opcode) == GET_TYPE (insn_word))
	      {
		if ((current_optab->base_opcode & PAR_STORE_IDEN) == (insn_word & PAR_STORE_IDEN))
		  {
		    insn->ptm = current_optab;
		    break;
		  }
	      }
	  }
      }
      break;
    case MUL_ADDS:
      insn->type = PARALLEL_INSN;
      {
	partemplate *current_optab = (partemplate *) tic30_paroptab;
	for (; current_optab < tic30_paroptab_end; current_optab++)
	  {
	    if (GET_TYPE (current_optab->base_opcode) == GET_TYPE (insn_word))
	      {
		if ((current_optab->base_opcode & MUL_ADD_IDEN) == (insn_word & MUL_ADD_IDEN))
		  {
		    insn->ptm = current_optab;
		    break;
		  }
	      }
	  }
      }
      break;
    case BRANCHES:
      insn->type = NORMAL_INSN;
      {
	template *current_optab = (template *) tic30_optab;
	for (; current_optab < tic30_optab_end; current_optab++)
	  {
	    if (GET_TYPE (current_optab->base_opcode) == GET_TYPE (insn_word))
	      {
		if (current_optab->operand_types[0] & Imm24)
		  {
		    if ((current_optab->base_opcode & BR_IMM_IDEN) == (insn_word & BR_IMM_IDEN))
		      {
			insn->tm = current_optab;
			break;
		      }
		  }
		else if (current_optab->operands > 0)
		  {
		    if ((current_optab->base_opcode & BR_COND_IDEN) == (insn_word & BR_COND_IDEN))
		      {
			insn->tm = current_optab;
			break;
		      }
		  }
		else
		  {
		    if ((current_optab->base_opcode & (BR_COND_IDEN | 0x00800000)) == (insn_word & (BR_COND_IDEN | 0x00800000)))
		      {
			insn->tm = current_optab;
			break;
		      }
		  }
	      }
	  }
      }
      break;
    default:
      return 0;
    }
  return 1;
}

int
print_two_operand (info, insn_word, insn)
     disassemble_info *info;
     unsigned long insn_word;
     struct instruction *insn;
{
  char name[12];
  char operand[2][13] =
  {
    {0},
    {0}};
  float f_number;

  if (insn->tm == NULL)
    return 0;
  strcpy (name, insn->tm->name);
  if (insn->tm->opcode_modifier == AddressMode)
    {
      int src_op, dest_op;
      /* Determine whether instruction is a store or a normal instruction. */
      if ((insn->tm->operand_types[1] & (Direct | Indirect)) == (Direct | Indirect))
	{
	  src_op = 1;
	  dest_op = 0;
	}
      else
	{
	  src_op = 0;
	  dest_op = 1;
	}
      /* Get the destination register. */
      if (insn->tm->operands == 2)
	get_register_operand ((insn_word & 0x001F0000) >> 16, operand[dest_op]);
      /* Get the source operand based on addressing mode. */
      switch (insn_word & AddressMode)
	{
	case AM_REGISTER:
	  /* Check for the NOP instruction before getting the operand. */
	  if ((insn->tm->operand_types[0] & NotReq) == 0)
	    get_register_operand ((insn_word & 0x0000001F), operand[src_op]);
	  break;
	case AM_DIRECT:
	  sprintf (operand[src_op], "@0x%lX", (insn_word & 0x0000FFFF));
	  break;
	case AM_INDIRECT:
	  get_indirect_operand ((insn_word & 0x0000FFFF), 2, operand[src_op]);
	  break;
	case AM_IMM:
	  /* Get the value of the immediate operand based on variable type. */
	  switch (insn->tm->imm_arg_type)
	    {
	    case Imm_Float:
	      cnvt_tmsfloat_ieee ((insn_word & 0x0000FFFF), 2, &f_number);
	      sprintf (operand[src_op], "%2.2f", f_number);
	      break;
	    case Imm_SInt:
	      sprintf (operand[src_op], "%d", (short) (insn_word & 0x0000FFFF));
	      break;
	    case Imm_UInt:
	      sprintf (operand[src_op], "%lu", (insn_word & 0x0000FFFF));
	      break;
	    default:
	      return 0;
	    }
	  /* Handle special case for LDP instruction. */
	  if ((insn_word & 0xFFFFFF00) == LDP_INSN)
	    {
	      strcpy (name, "ldp");
	      sprintf (operand[0], "0x%06lX", (insn_word & 0x000000FF) << 16);
	      operand[1][0] = '\0';
	    }
	}
    }
  /* Handle case for stack and rotate instructions. */
  else if (insn->tm->operands == 1)
    {
      if (insn->tm->opcode_modifier == StackOp)
	{
	  get_register_operand ((insn_word & 0x001F0000) >> 16, operand[0]);
	}
    }
  /* Output instruction to stream. */
  info->fprintf_func (info->stream, "   %s %s%c%s", name,
		      operand[0][0] ? operand[0] : "",
		      operand[1][0] ? ',' : ' ',
		      operand[1][0] ? operand[1] : "");
  return 1;
}

int
print_three_operand (info, insn_word, insn)
     disassemble_info *info;
     unsigned long insn_word;
     struct instruction *insn;
{
  char operand[3][13] =
  {
    {0},
    {0},
    {0}};

  if (insn->tm == NULL)
    return 0;
  switch (insn_word & AddressMode)
    {
    case AM_REGISTER:
      get_register_operand ((insn_word & 0x000000FF), operand[0]);
      get_register_operand ((insn_word & 0x0000FF00) >> 8, operand[1]);
      break;
    case AM_DIRECT:
      get_register_operand ((insn_word & 0x000000FF), operand[0]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1]);
      break;
    case AM_INDIRECT:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0]);
      get_register_operand ((insn_word & 0x0000FF00) >> 8, operand[1]);
      break;
    case AM_IMM:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1]);
      break;
    default:
      return 0;
    }
  if (insn->tm->operands == 3)
    get_register_operand ((insn_word & 0x001F0000) >> 16, operand[2]);
  info->fprintf_func (info->stream, "   %s %s,%s%c%s", insn->tm->name,
		      operand[0], operand[1],
		      operand[2][0] ? ',' : ' ',
		      operand[2][0] ? operand[2] : "");
  return 1;
}

int
print_par_insn (info, insn_word, insn)
     disassemble_info *info;
     unsigned long insn_word;
     struct instruction *insn;
{
  size_t i, len;
  char *name1, *name2;
  char operand[2][3][13] =
  {
    {
      {0},
      {0},
      {0}},
    {
      {0},
      {0},
      {0}}};

  if (insn->ptm == NULL)
    return 0;
  /* Parse out the names of each of the parallel instructions from the
     q_insn1_insn2 format. */
  name1 = (char *) strdup (insn->ptm->name + 2);
  name2 = "";
  len = strlen (name1);
  for (i = 0; i < len; i++)
    {
      if (name1[i] == '_')
	{
	  name2 = &name1[i + 1];
	  name1[i] = '\0';
	  break;
	}
    }
  /* Get the operands of the instruction based on the operand order. */
  switch (insn->ptm->oporder)
    {
    case OO_4op1:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0][0]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1][1]);
      get_register_operand ((insn_word >> 16) & 0x07, operand[1][0]);
      get_register_operand ((insn_word >> 22) & 0x07, operand[0][1]);
      break;
    case OO_4op2:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0][0]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1][0]);
      get_register_operand ((insn_word >> 19) & 0x07, operand[1][1]);
      get_register_operand ((insn_word >> 22) & 0x07, operand[0][1]);
      break;
    case OO_4op3:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0][1]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1][1]);
      get_register_operand ((insn_word >> 16) & 0x07, operand[1][0]);
      get_register_operand ((insn_word >> 22) & 0x07, operand[0][0]);
      break;
    case OO_5op1:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0][0]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1][1]);
      get_register_operand ((insn_word >> 16) & 0x07, operand[1][0]);
      get_register_operand ((insn_word >> 19) & 0x07, operand[0][1]);
      get_register_operand ((insn_word >> 22) & 0x07, operand[0][2]);
      break;
    case OO_5op2:
      get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0][1]);
      get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1][1]);
      get_register_operand ((insn_word >> 16) & 0x07, operand[1][0]);
      get_register_operand ((insn_word >> 19) & 0x07, operand[0][0]);
      get_register_operand ((insn_word >> 22) & 0x07, operand[0][2]);
      break;
    case OO_PField:
      if (insn_word & 0x00800000)
	get_register_operand (0x01, operand[0][2]);
      else
	get_register_operand (0x00, operand[0][2]);
      if (insn_word & 0x00400000)
	get_register_operand (0x03, operand[1][2]);
      else
	get_register_operand (0x02, operand[1][2]);
      switch (insn_word & P_FIELD)
	{
	case 0x00000000:
	  get_indirect_operand ((insn_word & 0x000000FF), 1, operand[0][1]);
	  get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[0][0]);
	  get_register_operand ((insn_word >> 16) & 0x07, operand[1][1]);
	  get_register_operand ((insn_word >> 19) & 0x07, operand[1][0]);
	  break;
	case 0x01000000:
	  get_indirect_operand ((insn_word & 0x000000FF), 1, operand[1][0]);
	  get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[0][0]);
	  get_register_operand ((insn_word >> 16) & 0x07, operand[1][1]);
	  get_register_operand ((insn_word >> 19) & 0x07, operand[0][1]);
	  break;
	case 0x02000000:
	  get_indirect_operand ((insn_word & 0x000000FF), 1, operand[1][1]);
	  get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[1][0]);
	  get_register_operand ((insn_word >> 16) & 0x07, operand[0][1]);
	  get_register_operand ((insn_word >> 19) & 0x07, operand[0][0]);
	  break;
	case 0x03000000:
	  get_indirect_operand ((insn_word & 0x000000FF), 1, operand[1][1]);
	  get_indirect_operand ((insn_word & 0x0000FF00) >> 8, 1, operand[0][0]);
	  get_register_operand ((insn_word >> 16) & 0x07, operand[1][0]);
	  get_register_operand ((insn_word >> 19) & 0x07, operand[0][1]);
	  break;
	}
      break;
    default:
      return 0;
    }
  info->fprintf_func (info->stream, "   %s %s,%s%c%s", name1,
		      operand[0][0], operand[0][1],
		      operand[0][2][0] ? ',' : ' ',
		      operand[0][2][0] ? operand[0][2] : "");
  info->fprintf_func (info->stream, "\n\t\t\t|| %s %s,%s%c%s", name2,
		      operand[1][0], operand[1][1],
		      operand[1][2][0] ? ',' : ' ',
		      operand[1][2][0] ? operand[1][2] : "");
  free (name1);
  return 1;
}

int
print_branch (info, insn_word, insn)
     disassemble_info *info;
     unsigned long insn_word;
     struct instruction *insn;
{
  char operand[2][13] =
  {
    {0},
    {0}};
  unsigned long address;
  int print_label = 0;

  if (insn->tm == NULL)
    return 0;
  /* Get the operands for 24-bit immediate jumps. */
  if (insn->tm->operand_types[0] & Imm24)
    {
      address = insn_word & 0x00FFFFFF;
      sprintf (operand[0], "0x%lX", address);
      print_label = 1;
    }
  /* Get the operand for the trap instruction. */
  else if (insn->tm->operand_types[0] & IVector)
    {
      address = insn_word & 0x0000001F;
      sprintf (operand[0], "0x%lX", address);
    }
  else
    {
      address = insn_word & 0x0000FFFF;
      /* Get the operands for the DB instructions. */
      if (insn->tm->operands == 2)
	{
	  get_register_operand (((insn_word & 0x01C00000) >> 22) + REG_AR0, operand[0]);
	  if (insn_word & PCRel)
	    {
	      sprintf (operand[1], "%d", (short) address);
	      print_label = 1;
	    }
	  else
	    get_register_operand (insn_word & 0x0000001F, operand[1]);
	}
      /* Get the operands for the standard branches. */
      else if (insn->tm->operands == 1)
	{
	  if (insn_word & PCRel)
	    {
	      address = (short) address;
	      sprintf (operand[0], "%ld", address);
	      print_label = 1;
	    }
	  else
	    get_register_operand (insn_word & 0x0000001F, operand[0]);
	}
    }
  info->fprintf_func (info->stream, "   %s %s%c%s", insn->tm->name,
		      operand[0][0] ? operand[0] : "",
		      operand[1][0] ? ',' : ' ',
		      operand[1][0] ? operand[1] : "");
  /* Print destination of branch in relation to current symbol. */
  if (print_label && info->symbols)
    {
      asymbol *sym = *info->symbols;

      if ((insn->tm->opcode_modifier == PCRel) && (insn_word & PCRel))
	{
	  address = (_pc + 1 + (short) address) - ((sym->section->vma + sym->value) / 4);
	  /* Check for delayed instruction, if so adjust destination. */
	  if (insn_word & 0x00200000)
	    address += 2;
	}
      else
	{
	  address -= ((sym->section->vma + sym->value) / 4);
	}
      if (address == 0)
	info->fprintf_func (info->stream, " <%s>", sym->name);
      else
	info->fprintf_func (info->stream, " <%s %c %d>", sym->name,
			    ((short) address < 0) ? '-' : '+',
			    abs (address));
    }
  return 1;
}

int
get_indirect_operand (fragment, size, buffer)
     unsigned short fragment;
     int size;
     char *buffer;
{
  unsigned char mod;
  unsigned arnum;
  unsigned char disp;

  if (buffer == NULL)
    return 0;
  /* Determine which bits identify the sections of the indirect operand based on the
     size in bytes. */
  switch (size)
    {
    case 1:
      mod = (fragment & 0x00F8) >> 3;
      arnum = (fragment & 0x0007);
      disp = 0;
      break;
    case 2:
      mod = (fragment & 0xF800) >> 11;
      arnum = (fragment & 0x0700) >> 8;
      disp = (fragment & 0x00FF);
      break;
    default:
      return 0;
    }
  {
    const ind_addr_type *current_ind = tic30_indaddr_tab;
    for (; current_ind < tic30_indaddrtab_end; current_ind++)
      {
	if (current_ind->modfield == mod)
	  {
	    if (current_ind->displacement == IMPLIED_DISP && size == 2)
	      {
		continue;
	      }
	    else
	      {
		size_t i, len;
		int bufcnt;
		
		len = strlen (current_ind->syntax);
		for (i = 0, bufcnt = 0; i < len; i++, bufcnt++)
		  {
		    buffer[bufcnt] = current_ind->syntax[i];
		    if (buffer[bufcnt - 1] == 'a' && buffer[bufcnt] == 'r')
		      buffer[++bufcnt] = arnum + '0';
		    if (buffer[bufcnt] == '(' && current_ind->displacement == DISP_REQUIRED)
		      {
			sprintf (&buffer[bufcnt + 1], "%u", disp);
			bufcnt += strlen (&buffer[bufcnt + 1]);
		      }
		  }
		buffer[bufcnt + 1] = '\0';
		break;
	      }
	  }
      }
  }
  return 1;
}

int
get_register_operand (fragment, buffer)
     unsigned char fragment;
     char *buffer;
{
  const reg *current_reg = tic30_regtab;

  if (buffer == NULL)
    return 0;
  for (; current_reg < tic30_regtab_end; current_reg++)
    {
      if ((fragment & 0x1F) == current_reg->opcode)
	{
	  strcpy (buffer, current_reg->name);
	  return 1;
	}
    }
  return 0;
}

int
cnvt_tmsfloat_ieee (tmsfloat, size, ieeefloat)
     unsigned long tmsfloat;
     int size;
     float *ieeefloat;
{
  unsigned long exp, sign, mant;

  if (size == 2)
    {
      if ((tmsfloat & 0x0000F000) == 0x00008000)
	tmsfloat = 0x80000000;
      else
	{
	  tmsfloat <<= 16;
	  tmsfloat = (long) tmsfloat >> 4;
	}
    }
  exp = tmsfloat & 0xFF000000;
  if (exp == 0x80000000)
    {
      *ieeefloat = 0.0;
      return 1;
    }
  exp += 0x7F000000;
  sign = (tmsfloat & 0x00800000) << 8;
  mant = tmsfloat & 0x007FFFFF;
  if (exp == 0xFF000000)
    {
      if (mant == 0)
	*ieeefloat = ERANGE;
      if (sign == 0)
	*ieeefloat = 1.0 / 0.0;
      else
	*ieeefloat = -1.0 / 0.0;
      return 1;
    }
  exp >>= 1;
  if (sign)
    {
      mant = (~mant) & 0x007FFFFF;
      mant += 1;
      exp += mant & 0x00800000;
      exp &= 0x7F800000;
      mant &= 0x007FFFFF;
    }
  if (tmsfloat == 0x80000000)
    sign = mant = exp = 0;
  tmsfloat = sign | exp | mant;
  *ieeefloat = *((float *) &tmsfloat);
  return 1;
}
