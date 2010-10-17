/* tc-c30.c -- Assembly code for the Texas Instruments TMS320C30
   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Steven Haworth (steve@pm.cse.rmit.edu.au)

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Texas Instruments TMS320C30 machine specific gas.
   Written by Steven Haworth (steve@pm.cse.rmit.edu.au).
   Bugs & suggestions are completely welcome.  This is free software.
   Please help us make it better.  */

#include "as.h"
#include "safe-ctype.h"
#include "opcode/tic30.h"
#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/* Put here all non-digit non-letter characters that may occur in an
   operand.  */
static char operand_special_chars[] = "%$-+(,)*._~/<>&^!:[@]";
static char *ordinal_names[] = {
  "first", "second", "third", "fourth", "fifth"
};

const int md_reloc_size = 0;

const char comment_chars[] = ";";
const char line_comment_chars[] = "*";
const char line_separator_chars[] = "";

const char *md_shortopts = "";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

/* Chars that mean this number is a floating point constant.  */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "fFdDxX";

/* Chars that can be used to separate mant from exp in floating point
   nums.  */
const char EXP_CHARS[] = "eE";

/* tables for lexical analysis */
static char opcode_chars[256];
static char register_chars[256];
static char operand_chars[256];
static char space_chars[256];
static char identifier_chars[256];
static char digit_chars[256];

/* lexical macros */
#define is_opcode_char(x) (opcode_chars[(unsigned char) x])
#define is_operand_char(x) (operand_chars[(unsigned char) x])
#define is_register_char(x) (register_chars[(unsigned char) x])
#define is_space_char(x) (space_chars[(unsigned char) x])
#define is_identifier_char(x) (identifier_chars[(unsigned char) x])
#define is_digit_char(x) (digit_chars[(unsigned char) x])

const pseudo_typeS md_pseudo_table[] = {
  {0, 0, 0}
};

int debug PARAMS ((const char *string, ...));

int
debug VPARAMS ((const char *string, ...))
{
  if (flag_debug)
    {
      char str[100];

      VA_OPEN (argptr, string);
      VA_FIXEDARG (argptr, const char *, string);
      vsprintf (str, string, argptr);
      VA_CLOSE (argptr);
      if (str[0] == '\0')
	return (0);
      fputs (str, USE_STDOUT ? stdout : stderr);
      return strlen (str);
    }
  else
    return 0;
}

/* hash table for opcode lookup */
static struct hash_control *op_hash;
/* hash table for parallel opcode lookup */
static struct hash_control *parop_hash;
/* hash table for register lookup */
static struct hash_control *reg_hash;
/* hash table for indirect addressing lookup */
static struct hash_control *ind_hash;

void
md_begin ()
{
  const char *hash_err;
  debug ("In md_begin()\n");
  op_hash = hash_new ();
  {
    const template *current_optab = tic30_optab;
    for (; current_optab < tic30_optab_end; current_optab++)
      {
	hash_err = hash_insert (op_hash, current_optab->name, (char *) current_optab);
	if (hash_err)
	  as_fatal ("Internal Error: Can't Hash %s: %s", current_optab->name, hash_err);
      }
  }
  parop_hash = hash_new ();
  {
    const partemplate *current_parop = tic30_paroptab;
    for (; current_parop < tic30_paroptab_end; current_parop++)
      {
	hash_err = hash_insert (parop_hash, current_parop->name, (char *) current_parop);
	if (hash_err)
	  as_fatal ("Internal Error: Can't Hash %s: %s", current_parop->name, hash_err);
      }
  }
  reg_hash = hash_new ();
  {
    const reg *current_reg = tic30_regtab;
    for (; current_reg < tic30_regtab_end; current_reg++)
      {
	hash_err = hash_insert (reg_hash, current_reg->name, (char *) current_reg);
	if (hash_err)
	  as_fatal ("Internal Error: Can't Hash %s: %s", current_reg->name, hash_err);
      }
  }
  ind_hash = hash_new ();
  {
    const ind_addr_type *current_ind = tic30_indaddr_tab;
    for (; current_ind < tic30_indaddrtab_end; current_ind++)
      {
	hash_err = hash_insert (ind_hash, current_ind->syntax, (char *) current_ind);
	if (hash_err)
	  as_fatal ("Internal Error: Can't Hash %s: %s", current_ind->syntax, hash_err);
      }
  }
  /* fill in lexical tables:  opcode_chars, operand_chars, space_chars */
  {
    register int c;
    register char *p;

    for (c = 0; c < 256; c++)
      {
	if (ISLOWER (c) || ISDIGIT (c))
	  {
	    opcode_chars[c] = c;
	    register_chars[c] = c;
	  }
	else if (ISUPPER (c))
	  {
	    opcode_chars[c] = TOLOWER (c);
	    register_chars[c] = opcode_chars[c];
	  }
	else if (c == ')' || c == '(')
	  {
	    register_chars[c] = c;
	  }
	if (ISUPPER (c) || ISLOWER (c) || ISDIGIT (c))
	  operand_chars[c] = c;
	if (ISDIGIT (c) || c == '-')
	  digit_chars[c] = c;
	if (ISALPHA (c) || c == '_' || c == '.' || ISDIGIT (c))
	  identifier_chars[c] = c;
	if (c == ' ' || c == '\t')
	  space_chars[c] = c;
	if (c == '_')
	  opcode_chars[c] = c;
      }
    for (p = operand_special_chars; *p != '\0'; p++)
      operand_chars[(unsigned char) *p] = *p;
  }
}

/* Address Mode OR values */
#define AM_Register  0x00000000
#define AM_Direct    0x00200000
#define AM_Indirect  0x00400000
#define AM_Immediate 0x00600000
#define AM_NotReq    0xFFFFFFFF

/* PC Relative OR values */
#define PC_Register 0x00000000
#define PC_Relative 0x02000000

typedef struct {
  unsigned op_type;
  struct {
    int resolved;
    unsigned address;
    char *label;
    expressionS direct_expr;
  } direct;
  struct {
    unsigned mod;
    int ARnum;
    unsigned char disp;
  } indirect;
  struct {
    unsigned opcode;
  } reg;
  struct {
    int resolved;
    int decimal_found;
    float f_number;
    int s_number;
    unsigned int u_number;
    char *label;
    expressionS imm_expr;
  } immediate;
} operand;

int tic30_parallel_insn PARAMS ((char *));
operand *tic30_operand PARAMS ((char *));
char *tic30_find_parallel_insn PARAMS ((char *, char *));

template *opcode;

struct tic30_insn {
  template *tm;			/* Template of current instruction */
  unsigned opcode;		/* Final opcode */
  unsigned int operands;	/* Number of given operands */
  /* Type of operand given in instruction */
  operand *operand_type[MAX_OPERANDS];
  unsigned addressing_mode;	/* Final addressing mode of instruction */
};

struct tic30_insn insn;
static int found_parallel_insn;

void
md_assemble (line)
     char *line;
{
  template *opcode;
  char *current_posn;
  char *token_start;
  char save_char;
  unsigned int count;

  debug ("In md_assemble() with argument %s\n", line);
  memset (&insn, '\0', sizeof (insn));
  if (found_parallel_insn)
    {
      debug ("Line is second part of parallel instruction\n\n");
      found_parallel_insn = 0;
      return;
    }
  if ((current_posn = tic30_find_parallel_insn (line, input_line_pointer + 1)) == NULL)
    current_posn = line;
  else
    found_parallel_insn = 1;
  while (is_space_char (*current_posn))
    current_posn++;
  token_start = current_posn;
  if (!is_opcode_char (*current_posn))
    {
      as_bad ("Invalid character %s in opcode", output_invalid (*current_posn));
      return;
    }
  /* Check if instruction is a parallel instruction by seeing if the first
     character is a q.  */
  if (*token_start == 'q')
    {
      if (tic30_parallel_insn (token_start))
	{
	  if (found_parallel_insn)
	    free (token_start);
	  return;
	}
    }
  while (is_opcode_char (*current_posn))
    current_posn++;
  {				/* Find instruction */
    save_char = *current_posn;
    *current_posn = '\0';
    opcode = (template *) hash_find (op_hash, token_start);
    if (opcode)
      {
	debug ("Found instruction %s\n", opcode->name);
	insn.tm = opcode;
      }
    else
      {
	debug ("Didn't find insn\n");
	as_bad ("Unknown TMS320C30 instruction: %s", token_start);
	return;
      }
    *current_posn = save_char;
  }
  if (*current_posn != END_OF_INSN)
    {				/* Find operands */
      int paren_not_balanced;
      int expecting_operand = 0;
      int this_operand;
      do
	{
	  /* skip optional white space before operand */
	  while (!is_operand_char (*current_posn) && *current_posn != END_OF_INSN)
	    {
	      if (!is_space_char (*current_posn))
		{
		  as_bad ("Invalid character %s before %s operand",
			  output_invalid (*current_posn),
			  ordinal_names[insn.operands]);
		  return;
		}
	      current_posn++;
	    }
	  token_start = current_posn;	/* after white space */
	  paren_not_balanced = 0;
	  while (paren_not_balanced || *current_posn != ',')
	    {
	      if (*current_posn == END_OF_INSN)
		{
		  if (paren_not_balanced)
		    {
		      as_bad ("Unbalanced parenthesis in %s operand.",
			      ordinal_names[insn.operands]);
		      return;
		    }
		  else
		    break;	/* we are done */
		}
	      else if (!is_operand_char (*current_posn) && !is_space_char (*current_posn))
		{
		  as_bad ("Invalid character %s in %s operand",
			  output_invalid (*current_posn),
			  ordinal_names[insn.operands]);
		  return;
		}
	      if (*current_posn == '(')
		++paren_not_balanced;
	      if (*current_posn == ')')
		--paren_not_balanced;
	      current_posn++;
	    }
	  if (current_posn != token_start)
	    {			/* yes, we've read in another operand */
	      this_operand = insn.operands++;
	      if (insn.operands > MAX_OPERANDS)
		{
		  as_bad ("Spurious operands; (%d operands/instruction max)",
			  MAX_OPERANDS);
		  return;
		}
	      /* now parse operand adding info to 'insn' as we go along */
	      save_char = *current_posn;
	      *current_posn = '\0';
	      insn.operand_type[this_operand] = tic30_operand (token_start);
	      *current_posn = save_char;
	      if (insn.operand_type[this_operand] == NULL)
		return;
	    }
	  else
	    {
	      if (expecting_operand)
		{
		  as_bad ("Expecting operand after ','; got nothing");
		  return;
		}
	      if (*current_posn == ',')
		{
		  as_bad ("Expecting operand before ','; got nothing");
		  return;
		}
	    }
	  /* now *current_posn must be either ',' or END_OF_INSN */
	  if (*current_posn == ',')
	    {
	      if (*++current_posn == END_OF_INSN)
		{		/* just skip it, if it's \n complain */
		  as_bad ("Expecting operand after ','; got nothing");
		  return;
		}
	      expecting_operand = 1;
	    }
	}
      while (*current_posn != END_OF_INSN);	/* until we get end of insn */
    }
  debug ("Number of operands found: %d\n", insn.operands);
  /* Check that number of operands is correct */
  if (insn.operands != insn.tm->operands)
    {
      unsigned int i;
      unsigned int numops = insn.tm->operands;
      /* If operands are not the same, then see if any of the operands are not
         required.  Then recheck with number of given operands.  If they are still not
         the same, then give an error, otherwise carry on.  */
      for (i = 0; i < insn.tm->operands; i++)
	if (insn.tm->operand_types[i] & NotReq)
	  numops--;
      if (insn.operands != numops)
	{
	  as_bad ("Incorrect number of operands given");
	  return;
	}
    }
  insn.addressing_mode = AM_NotReq;
  for (count = 0; count < insn.operands; count++)
    {
      if (insn.operand_type[count]->op_type & insn.tm->operand_types[count])
	{
	  debug ("Operand %d matches\n", count + 1);
	  /* If instruction has two operands and has an AddressMode modifier then set
	     addressing mode type for instruction */
	  if (insn.tm->opcode_modifier == AddressMode)
	    {
	      int addr_insn = 0;
	      /* Store instruction uses the second operand for the address mode.  */
	      if ((insn.tm->operand_types[1] & (Indirect | Direct)) == (Indirect | Direct))
		addr_insn = 1;
	      if (insn.operand_type[addr_insn]->op_type & (AllReg))
		insn.addressing_mode = AM_Register;
	      else if (insn.operand_type[addr_insn]->op_type & Direct)
		insn.addressing_mode = AM_Direct;
	      else if (insn.operand_type[addr_insn]->op_type & Indirect)
		insn.addressing_mode = AM_Indirect;
	      else
		insn.addressing_mode = AM_Immediate;
	    }
	}
      else
	{
	  as_bad ("The %s operand doesn't match", ordinal_names[count]);
	  return;
	}
    }
  /* Now set the addressing mode for 3 operand instructions.  */
  if ((insn.tm->operand_types[0] & op3T1) && (insn.tm->operand_types[1] & op3T2))
    {
      /* Set the addressing mode to the values used for 2 operand instructions in the
         G addressing field of the opcode.  */
      char *p;
      switch (insn.operand_type[0]->op_type)
	{
	case Rn:
	case ARn:
	case DPReg:
	case OtherReg:
	  if (insn.operand_type[1]->op_type & (AllReg))
	    insn.addressing_mode = AM_Register;
	  else if (insn.operand_type[1]->op_type & Indirect)
	    insn.addressing_mode = AM_Direct;
	  else
	    {
	      /* Shouldn't make it to this stage */
	      as_bad ("Incompatible first and second operands in instruction");
	      return;
	    }
	  break;
	case Indirect:
	  if (insn.operand_type[1]->op_type & (AllReg))
	    insn.addressing_mode = AM_Indirect;
	  else if (insn.operand_type[1]->op_type & Indirect)
	    insn.addressing_mode = AM_Immediate;
	  else
	    {
	      /* Shouldn't make it to this stage */
	      as_bad ("Incompatible first and second operands in instruction");
	      return;
	    }
	  break;
	}
      /* Now make up the opcode for the 3 operand instructions.  As in parallel
         instructions, there will be no unresolved values, so they can be fully formed
         and added to the frag table.  */
      insn.opcode = insn.tm->base_opcode;
      if (insn.operand_type[0]->op_type & Indirect)
	{
	  insn.opcode |= (insn.operand_type[0]->indirect.ARnum);
	  insn.opcode |= (insn.operand_type[0]->indirect.mod << 3);
	}
      else
	insn.opcode |= (insn.operand_type[0]->reg.opcode);
      if (insn.operand_type[1]->op_type & Indirect)
	{
	  insn.opcode |= (insn.operand_type[1]->indirect.ARnum << 8);
	  insn.opcode |= (insn.operand_type[1]->indirect.mod << 11);
	}
      else
	insn.opcode |= (insn.operand_type[1]->reg.opcode << 8);
      if (insn.operands == 3)
	insn.opcode |= (insn.operand_type[2]->reg.opcode << 16);
      insn.opcode |= insn.addressing_mode;
      p = frag_more (INSN_SIZE);
      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
    }
  else
    {				/* Not a three operand instruction */
      char *p;
      int am_insn = -1;
      insn.opcode = insn.tm->base_opcode;
      /* Create frag for instruction - all instructions are 4 bytes long.  */
      p = frag_more (INSN_SIZE);
      if ((insn.operands > 0) && (insn.tm->opcode_modifier == AddressMode))
	{
	  insn.opcode |= insn.addressing_mode;
	  if (insn.addressing_mode == AM_Indirect)
	    {
	      /* Determine which operand gives the addressing mode */
	      if (insn.operand_type[0]->op_type & Indirect)
		am_insn = 0;
	      if ((insn.operands > 1) && (insn.operand_type[1]->op_type & Indirect))
		am_insn = 1;
	      insn.opcode |= (insn.operand_type[am_insn]->indirect.disp);
	      insn.opcode |= (insn.operand_type[am_insn]->indirect.ARnum << 8);
	      insn.opcode |= (insn.operand_type[am_insn]->indirect.mod << 11);
	      if (insn.operands > 1)
		insn.opcode |= (insn.operand_type[!am_insn]->reg.opcode << 16);
	      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	    }
	  else if (insn.addressing_mode == AM_Register)
	    {
	      insn.opcode |= (insn.operand_type[0]->reg.opcode);
	      if (insn.operands > 1)
		insn.opcode |= (insn.operand_type[1]->reg.opcode << 16);
	      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	    }
	  else if (insn.addressing_mode == AM_Direct)
	    {
	      if (insn.operand_type[0]->op_type & Direct)
		am_insn = 0;
	      if ((insn.operands > 1) && (insn.operand_type[1]->op_type & Direct))
		am_insn = 1;
	      if (insn.operands > 1)
		insn.opcode |= (insn.operand_type[!am_insn]->reg.opcode << 16);
	      if (insn.operand_type[am_insn]->direct.resolved == 1)
		{
		  /* Resolved values can be placed straight into instruction word, and output */
		  insn.opcode |= (insn.operand_type[am_insn]->direct.address & 0x0000FFFF);
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		}
	      else
		{		/* Unresolved direct addressing mode instruction */
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		  fix_new_exp (frag_now, p + 2 - (frag_now->fr_literal), 2, &insn.operand_type[am_insn]->direct.direct_expr, 0, 0);
		}
	    }
	  else if (insn.addressing_mode == AM_Immediate)
	    {
	      if (insn.operand_type[0]->immediate.resolved == 1)
		{
		  char *keeploc;
		  int size;
		  if (insn.operands > 1)
		    insn.opcode |= (insn.operand_type[1]->reg.opcode << 16);
		  switch (insn.tm->imm_arg_type)
		    {
		    case Imm_Float:
		      debug ("Floating point first operand\n");
		      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		      keeploc = input_line_pointer;
		      input_line_pointer = insn.operand_type[0]->immediate.label;
		      if (md_atof ('f', p + 2, &size) != 0)
			{
			  as_bad ("invalid short form floating point immediate operand");
			  return;
			}
		      input_line_pointer = keeploc;
		      break;
		    case Imm_UInt:
		      debug ("Unsigned int first operand\n");
		      if (insn.operand_type[0]->immediate.decimal_found)
			as_warn ("rounding down first operand float to unsigned int");
		      if (insn.operand_type[0]->immediate.u_number > 0xFFFF)
			as_warn ("only lower 16-bits of first operand are used");
		      insn.opcode |= (insn.operand_type[0]->immediate.u_number & 0x0000FFFFL);
		      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		      break;
		    case Imm_SInt:
		      debug ("Int first operand\n");
		      if (insn.operand_type[0]->immediate.decimal_found)
			as_warn ("rounding down first operand float to signed int");
		      if (insn.operand_type[0]->immediate.s_number < -32768 ||
			  insn.operand_type[0]->immediate.s_number > 32767)
			{
			  as_bad ("first operand is too large for 16-bit signed int");
			  return;
			}
		      insn.opcode |= (insn.operand_type[0]->immediate.s_number & 0x0000FFFFL);
		      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		      break;
		    }
		}
	      else
		{		/* Unresolved immediate label */
		  if (insn.operands > 1)
		    insn.opcode |= (insn.operand_type[1]->reg.opcode << 16);
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		  fix_new_exp (frag_now, p + 2 - (frag_now->fr_literal), 2, &insn.operand_type[0]->immediate.imm_expr, 0, 0);
		}
	    }
	}
      else if (insn.tm->opcode_modifier == PCRel)
	{
	  /* Conditional Branch and Call instructions */
	  if ((insn.tm->operand_types[0] & (AllReg | Disp)) == (AllReg | Disp))
	    {
	      if (insn.operand_type[0]->op_type & (AllReg))
		{
		  insn.opcode |= (insn.operand_type[0]->reg.opcode);
		  insn.opcode |= PC_Register;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		}
	      else
		{
		  insn.opcode |= PC_Relative;
		  if (insn.operand_type[0]->immediate.resolved == 1)
		    {
		      insn.opcode |= (insn.operand_type[0]->immediate.s_number & 0x0000FFFF);
		      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		    }
		  else
		    {
		      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		      fix_new_exp (frag_now, p + 2 - (frag_now->fr_literal), 2, &insn.operand_type[0]->immediate.imm_expr, 1, 0);
		    }
		}
	    }
	  else if ((insn.tm->operand_types[0] & ARn) == ARn)
	    {
	      /* Decrement and Branch instructions */
	      insn.opcode |= ((insn.operand_type[0]->reg.opcode - 0x08) << 22);
	      if (insn.operand_type[1]->op_type & (AllReg))
		{
		  insn.opcode |= (insn.operand_type[1]->reg.opcode);
		  insn.opcode |= PC_Register;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		}
	      else if (insn.operand_type[1]->immediate.resolved == 1)
		{
		  if (insn.operand_type[0]->immediate.decimal_found)
		    {
		      as_bad ("first operand is floating point");
		      return;
		    }
		  if (insn.operand_type[0]->immediate.s_number < -32768 ||
		      insn.operand_type[0]->immediate.s_number > 32767)
		    {
		      as_bad ("first operand is too large for 16-bit signed int");
		      return;
		    }
		  insn.opcode |= (insn.operand_type[1]->immediate.s_number);
		  insn.opcode |= PC_Relative;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		}
	      else
		{
		  insn.opcode |= PC_Relative;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		  fix_new_exp (frag_now, p + 2 - frag_now->fr_literal, 2, &insn.operand_type[1]->immediate.imm_expr, 1, 0);
		}
	    }
	}
      else if (insn.tm->operand_types[0] == IVector)
	{
	  /* Trap instructions */
	  if (insn.operand_type[0]->op_type & IVector)
	    insn.opcode |= (insn.operand_type[0]->immediate.u_number);
	  else
	    {			/* Shouldn't get here */
	      as_bad ("interrupt vector for trap instruction out of range");
	      return;
	    }
	  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	}
      else if (insn.tm->opcode_modifier == StackOp || insn.tm->opcode_modifier == Rotate)
	{
	  /* Push, Pop and Rotate instructions */
	  insn.opcode |= (insn.operand_type[0]->reg.opcode << 16);
	  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	}
      else if ((insn.tm->operand_types[0] & (Abs24 | Direct)) == (Abs24 | Direct))
	{
	  /* LDP Instruction needs to be tested for before the next section */
	  if (insn.operand_type[0]->op_type & Direct)
	    {
	      if (insn.operand_type[0]->direct.resolved == 1)
		{
		  /* Direct addressing uses lower 8 bits of direct address */
		  insn.opcode |= (insn.operand_type[0]->direct.address & 0x00FF0000) >> 16;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		}
	      else
		{
		  fixS *fix;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		  fix = fix_new_exp (frag_now, p + 3 - (frag_now->fr_literal), 1, &insn.operand_type[0]->direct.direct_expr, 0, 0);
		  /* Ensure that the assembler doesn't complain about fitting a 24-bit
		     address into 8 bits.  */
		  fix->fx_no_overflow = 1;
		}
	    }
	  else
	    {
	      if (insn.operand_type[0]->immediate.resolved == 1)
		{
		  /* Immediate addressing uses upper 8 bits of address */
		  if (insn.operand_type[0]->immediate.u_number > 0x00FFFFFF)
		    {
		      as_bad ("LDP instruction needs a 24-bit operand");
		      return;
		    }
		  insn.opcode |= ((insn.operand_type[0]->immediate.u_number & 0x00FF0000) >> 16);
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		}
	      else
		{
		  fixS *fix;
		  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
		  fix = fix_new_exp (frag_now, p + 3 - (frag_now->fr_literal), 1, &insn.operand_type[0]->immediate.imm_expr, 0, 0);
		  fix->fx_no_overflow = 1;
		}
	    }
	}
      else if (insn.tm->operand_types[0] & (Imm24))
	{
	  /* Unconditional Branch and Call instructions */
	  if (insn.operand_type[0]->immediate.resolved == 1)
	    {
	      if (insn.operand_type[0]->immediate.u_number > 0x00FFFFFF)
		as_warn ("first operand is too large for a 24-bit displacement");
	      insn.opcode |= (insn.operand_type[0]->immediate.u_number & 0x00FFFFFF);
	      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	    }
	  else
	    {
	      md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	      fix_new_exp (frag_now, p + 1 - (frag_now->fr_literal), 3, &insn.operand_type[0]->immediate.imm_expr, 0, 0);
	    }
	}
      else if (insn.tm->operand_types[0] & NotReq)
	{
	  /* Check for NOP instruction without arguments.  */
	  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	}
      else if (insn.tm->operands == 0)
	{
	  /* Check for instructions without operands.  */
	  md_number_to_chars (p, (valueT) insn.opcode, INSN_SIZE);
	}
    }
  debug ("Addressing mode: %08X\n", insn.addressing_mode);
  {
    unsigned int i;
    for (i = 0; i < insn.operands; i++)
      {
	if (insn.operand_type[i]->immediate.label)
	  free (insn.operand_type[i]->immediate.label);
	free (insn.operand_type[i]);
      }
  }
  debug ("Final opcode: %08X\n", insn.opcode);
  debug ("\n");
}

struct tic30_par_insn {
  partemplate *tm;		/* Template of current parallel instruction */
  unsigned operands[2];		/* Number of given operands for each insn */
  /* Type of operand given in instruction */
  operand *operand_type[2][MAX_OPERANDS];
  int swap_operands;		/* Whether to swap operands around.  */
  unsigned p_field;		/* Value of p field in multiply add/sub instructions */
  unsigned opcode;		/* Final opcode */
};

struct tic30_par_insn p_insn;

int
tic30_parallel_insn (char *token)
{
  static partemplate *p_opcode;
  char *current_posn = token;
  char *token_start;
  char save_char;

  debug ("In tic30_parallel_insn with %s\n", token);
  memset (&p_insn, '\0', sizeof (p_insn));
  while (is_opcode_char (*current_posn))
    current_posn++;
  {				/* Find instruction */
    save_char = *current_posn;
    *current_posn = '\0';
    p_opcode = (partemplate *) hash_find (parop_hash, token);
    if (p_opcode)
      {
	debug ("Found instruction %s\n", p_opcode->name);
	p_insn.tm = p_opcode;
      }
    else
      {
	char first_opcode[6] =
	{0};
	char second_opcode[6] =
	{0};
	unsigned int i;
	int current_opcode = -1;
	int char_ptr = 0;

	for (i = 0; i < strlen (token); i++)
	  {
	    char ch = *(token + i);
	    if (ch == '_' && current_opcode == -1)
	      {
		current_opcode = 0;
		continue;
	      }
	    if (ch == '_' && current_opcode == 0)
	      {
		current_opcode = 1;
		char_ptr = 0;
		continue;
	      }
	    switch (current_opcode)
	      {
	      case 0:
		first_opcode[char_ptr++] = ch;
		break;
	      case 1:
		second_opcode[char_ptr++] = ch;
		break;
	      }
	  }
	debug ("first_opcode = %s\n", first_opcode);
	debug ("second_opcode = %s\n", second_opcode);
	sprintf (token, "q_%s_%s", second_opcode, first_opcode);
	p_opcode = (partemplate *) hash_find (parop_hash, token);
	if (p_opcode)
	  {
	    debug ("Found instruction %s\n", p_opcode->name);
	    p_insn.tm = p_opcode;
	    p_insn.swap_operands = 1;
	  }
	else
	  return 0;
      }
    *current_posn = save_char;
  }
  {				/* Find operands */
    int paren_not_balanced;
    int expecting_operand = 0;
    int found_separator = 0;
    do
      {
	/* skip optional white space before operand */
	while (!is_operand_char (*current_posn) && *current_posn != END_OF_INSN)
	  {
	    if (!is_space_char (*current_posn) && *current_posn != PARALLEL_SEPARATOR)
	      {
		as_bad ("Invalid character %s before %s operand",
			output_invalid (*current_posn),
			ordinal_names[insn.operands]);
		return 1;
	      }
	    if (*current_posn == PARALLEL_SEPARATOR)
	      found_separator = 1;
	    current_posn++;
	  }
	token_start = current_posn;	/* after white space */
	paren_not_balanced = 0;
	while (paren_not_balanced || *current_posn != ',')
	  {
	    if (*current_posn == END_OF_INSN)
	      {
		if (paren_not_balanced)
		  {
		    as_bad ("Unbalanced parenthesis in %s operand.",
			    ordinal_names[insn.operands]);
		    return 1;
		  }
		else
		  break;	/* we are done */
	      }
	    else if (*current_posn == PARALLEL_SEPARATOR)
	      {
		while (is_space_char (*(current_posn - 1)))
		  current_posn--;
		break;
	      }
	    else if (!is_operand_char (*current_posn) && !is_space_char (*current_posn))
	      {
		as_bad ("Invalid character %s in %s operand",
			output_invalid (*current_posn),
			ordinal_names[insn.operands]);
		return 1;
	      }
	    if (*current_posn == '(')
	      ++paren_not_balanced;
	    if (*current_posn == ')')
	      --paren_not_balanced;
	    current_posn++;
	  }
	if (current_posn != token_start)
	  {			/* yes, we've read in another operand */
	    p_insn.operands[found_separator]++;
	    if (p_insn.operands[found_separator] > MAX_OPERANDS)
	      {
		as_bad ("Spurious operands; (%d operands/instruction max)",
			MAX_OPERANDS);
		return 1;
	      }
	    /* now parse operand adding info to 'insn' as we go along */
	    save_char = *current_posn;
	    *current_posn = '\0';
	    p_insn.operand_type[found_separator][p_insn.operands[found_separator] - 1] =
	      tic30_operand (token_start);
	    *current_posn = save_char;
	    if (!p_insn.operand_type[found_separator][p_insn.operands[found_separator] - 1])
	      return 1;
	  }
	else
	  {
	    if (expecting_operand)
	      {
		as_bad ("Expecting operand after ','; got nothing");
		return 1;
	      }
	    if (*current_posn == ',')
	      {
		as_bad ("Expecting operand before ','; got nothing");
		return 1;
	      }
	  }
	/* now *current_posn must be either ',' or END_OF_INSN */
	if (*current_posn == ',')
	  {
	    if (*++current_posn == END_OF_INSN)
	      {			/* just skip it, if it's \n complain */
		as_bad ("Expecting operand after ','; got nothing");
		return 1;
	      }
	    expecting_operand = 1;
	  }
      }
    while (*current_posn != END_OF_INSN);	/* until we get end of insn */
  }
  if (p_insn.swap_operands)
    {
      int temp_num, i;
      operand *temp_op;

      temp_num = p_insn.operands[0];
      p_insn.operands[0] = p_insn.operands[1];
      p_insn.operands[1] = temp_num;
      for (i = 0; i < MAX_OPERANDS; i++)
	{
	  temp_op = p_insn.operand_type[0][i];
	  p_insn.operand_type[0][i] = p_insn.operand_type[1][i];
	  p_insn.operand_type[1][i] = temp_op;
	}
    }
  if (p_insn.operands[0] != p_insn.tm->operands_1)
    {
      as_bad ("incorrect number of operands given in the first instruction");
      return 1;
    }
  if (p_insn.operands[1] != p_insn.tm->operands_2)
    {
      as_bad ("incorrect number of operands given in the second instruction");
      return 1;
    }
  debug ("Number of operands in first insn: %d\n", p_insn.operands[0]);
  debug ("Number of operands in second insn: %d\n", p_insn.operands[1]);
  {				/* Now check if operands are correct */
    int count;
    int num_rn = 0;
    int num_ind = 0;
    for (count = 0; count < 2; count++)
      {
	unsigned int i;
	for (i = 0; i < p_insn.operands[count]; i++)
	  {
	    if ((p_insn.operand_type[count][i]->op_type &
		 p_insn.tm->operand_types[count][i]) == 0)
	      {
		as_bad ("%s instruction, operand %d doesn't match", ordinal_names[count], i + 1);
		return 1;
	      }
	    /* Get number of R register and indirect reference contained within the first
	       two operands of each instruction.  This is required for the multiply
	       parallel instructions which require two R registers and two indirect
	       references, but not in any particular place.  */
	    if ((p_insn.operand_type[count][i]->op_type & Rn) && i < 2)
	      num_rn++;
	    else if ((p_insn.operand_type[count][i]->op_type & Indirect) && i < 2)
	      num_ind++;
	  }
      }
    if ((p_insn.tm->operand_types[0][0] & (Indirect | Rn)) == (Indirect | Rn))
      {
	/* Check for the multiply instructions */
	if (num_rn != 2)
	  {
	    as_bad ("incorrect format for multiply parallel instruction");
	    return 1;
	  }
	if (num_ind != 2)
	  {			/* Shouldn't get here */
	    as_bad ("incorrect format for multiply parallel instruction");
	    return 1;
	  }
	if ((p_insn.operand_type[0][2]->reg.opcode != 0x00) &&
	    (p_insn.operand_type[0][2]->reg.opcode != 0x01))
	  {
	    as_bad ("destination for multiply can only be R0 or R1");
	    return 1;
	  }
	if ((p_insn.operand_type[1][2]->reg.opcode != 0x02) &&
	    (p_insn.operand_type[1][2]->reg.opcode != 0x03))
	  {
	    as_bad ("destination for add/subtract can only be R2 or R3");
	    return 1;
	  }
	/* Now determine the P field for the instruction */
	if (p_insn.operand_type[0][0]->op_type & Indirect)
	  {
	    if (p_insn.operand_type[0][1]->op_type & Indirect)
	      p_insn.p_field = 0x00000000;	/* Ind * Ind, Rn  +/- Rn  */
	    else if (p_insn.operand_type[1][0]->op_type & Indirect)
	      p_insn.p_field = 0x01000000;	/* Ind * Rn,  Ind +/- Rn  */
	    else
	      p_insn.p_field = 0x03000000;	/* Ind * Rn,  Rn  +/- Ind */
	  }
	else
	  {
	    if (p_insn.operand_type[0][1]->op_type & Rn)
	      p_insn.p_field = 0x02000000;	/* Rn  * Rn,  Ind +/- Ind */
	    else if (p_insn.operand_type[1][0]->op_type & Indirect)
	      {
		operand *temp;
		p_insn.p_field = 0x01000000;	/* Rn  * Ind, Ind +/- Rn  */
		/* Need to swap the two multiply operands around so that everything is in
		   its place for the opcode makeup ie so Ind * Rn, Ind +/- Rn */
		temp = p_insn.operand_type[0][0];
		p_insn.operand_type[0][0] = p_insn.operand_type[0][1];
		p_insn.operand_type[0][1] = temp;
	      }
	    else
	      {
		operand *temp;
		p_insn.p_field = 0x03000000;	/* Rn  * Ind, Rn  +/- Ind */
		temp = p_insn.operand_type[0][0];
		p_insn.operand_type[0][0] = p_insn.operand_type[0][1];
		p_insn.operand_type[0][1] = temp;
	      }
	  }
      }
  }
  debug ("P field: %08X\n", p_insn.p_field);
  /* Finalise opcode.  This is easier for parallel instructions as they have to be
     fully resolved, there are no memory addresses allowed, except through indirect
     addressing, so there are no labels to resolve.  */
  {
    p_insn.opcode = p_insn.tm->base_opcode;
    switch (p_insn.tm->oporder)
      {
      case OO_4op1:
	p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.ARnum);
	p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.mod << 3);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.ARnum << 8);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.mod << 11);
	p_insn.opcode |= (p_insn.operand_type[1][0]->reg.opcode << 16);
	p_insn.opcode |= (p_insn.operand_type[0][1]->reg.opcode << 22);
	break;
      case OO_4op2:
	p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.ARnum);
	p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.mod << 3);
	p_insn.opcode |= (p_insn.operand_type[1][0]->indirect.ARnum << 8);
	p_insn.opcode |= (p_insn.operand_type[1][0]->indirect.mod << 11);
	p_insn.opcode |= (p_insn.operand_type[1][1]->reg.opcode << 19);
	p_insn.opcode |= (p_insn.operand_type[0][1]->reg.opcode << 22);
	if (p_insn.operand_type[1][1]->reg.opcode == p_insn.operand_type[0][1]->reg.opcode)
	  as_warn ("loading the same register in parallel operation");
	break;
      case OO_4op3:
	p_insn.opcode |= (p_insn.operand_type[0][1]->indirect.ARnum);
	p_insn.opcode |= (p_insn.operand_type[0][1]->indirect.mod << 3);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.ARnum << 8);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.mod << 11);
	p_insn.opcode |= (p_insn.operand_type[1][0]->reg.opcode << 16);
	p_insn.opcode |= (p_insn.operand_type[0][0]->reg.opcode << 22);
	break;
      case OO_5op1:
	p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.ARnum);
	p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.mod << 3);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.ARnum << 8);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.mod << 11);
	p_insn.opcode |= (p_insn.operand_type[1][0]->reg.opcode << 16);
	p_insn.opcode |= (p_insn.operand_type[0][1]->reg.opcode << 19);
	p_insn.opcode |= (p_insn.operand_type[0][2]->reg.opcode << 22);
	break;
      case OO_5op2:
	p_insn.opcode |= (p_insn.operand_type[0][1]->indirect.ARnum);
	p_insn.opcode |= (p_insn.operand_type[0][1]->indirect.mod << 3);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.ARnum << 8);
	p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.mod << 11);
	p_insn.opcode |= (p_insn.operand_type[1][0]->reg.opcode << 16);
	p_insn.opcode |= (p_insn.operand_type[0][0]->reg.opcode << 19);
	p_insn.opcode |= (p_insn.operand_type[0][2]->reg.opcode << 22);
	break;
      case OO_PField:
	p_insn.opcode |= p_insn.p_field;
	if (p_insn.operand_type[0][2]->reg.opcode == 0x01)
	  p_insn.opcode |= 0x00800000;
	if (p_insn.operand_type[1][2]->reg.opcode == 0x03)
	  p_insn.opcode |= 0x00400000;
	switch (p_insn.p_field)
	  {
	  case 0x00000000:
	    p_insn.opcode |= (p_insn.operand_type[0][1]->indirect.ARnum);
	    p_insn.opcode |= (p_insn.operand_type[0][1]->indirect.mod << 3);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.ARnum << 8);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.mod << 11);
	    p_insn.opcode |= (p_insn.operand_type[1][1]->reg.opcode << 16);
	    p_insn.opcode |= (p_insn.operand_type[1][0]->reg.opcode << 19);
	    break;
	  case 0x01000000:
	    p_insn.opcode |= (p_insn.operand_type[1][0]->indirect.ARnum);
	    p_insn.opcode |= (p_insn.operand_type[1][0]->indirect.mod << 3);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.ARnum << 8);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.mod << 11);
	    p_insn.opcode |= (p_insn.operand_type[1][1]->reg.opcode << 16);
	    p_insn.opcode |= (p_insn.operand_type[0][1]->reg.opcode << 19);
	    break;
	  case 0x02000000:
	    p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.ARnum);
	    p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.mod << 3);
	    p_insn.opcode |= (p_insn.operand_type[1][0]->indirect.ARnum << 8);
	    p_insn.opcode |= (p_insn.operand_type[1][0]->indirect.mod << 11);
	    p_insn.opcode |= (p_insn.operand_type[0][1]->reg.opcode << 16);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->reg.opcode << 19);
	    break;
	  case 0x03000000:
	    p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.ARnum);
	    p_insn.opcode |= (p_insn.operand_type[1][1]->indirect.mod << 3);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.ARnum << 8);
	    p_insn.opcode |= (p_insn.operand_type[0][0]->indirect.mod << 11);
	    p_insn.opcode |= (p_insn.operand_type[1][0]->reg.opcode << 16);
	    p_insn.opcode |= (p_insn.operand_type[0][1]->reg.opcode << 19);
	    break;
	  }
	break;
      }
  }				/* Opcode is finalised at this point for all parallel instructions.  */
  {				/* Output opcode */
    char *p;
    p = frag_more (INSN_SIZE);
    md_number_to_chars (p, (valueT) p_insn.opcode, INSN_SIZE);
  }
  {
    unsigned int i, j;
    for (i = 0; i < 2; i++)
      for (j = 0; j < p_insn.operands[i]; j++)
	free (p_insn.operand_type[i][j]);
  }
  debug ("Final opcode: %08X\n", p_insn.opcode);
  debug ("\n");
  return 1;
}

operand *
tic30_operand (token)
     char *token;
{
  unsigned int count;
  char ind_buffer[strlen (token)];
  operand *current_op;

  debug ("In tic30_operand with %s\n", token);
  current_op = (operand *) malloc (sizeof (operand));
  memset (current_op, '\0', sizeof (operand));
  if (*token == DIRECT_REFERENCE)
    {
      char *token_posn = token + 1;
      int direct_label = 0;
      debug ("Found direct reference\n");
      while (*token_posn)
	{
	  if (!is_digit_char (*token_posn))
	    direct_label = 1;
	  token_posn++;
	}
      if (direct_label)
	{
	  char *save_input_line_pointer;
	  segT retval;
	  debug ("Direct reference is a label\n");
	  current_op->direct.label = token + 1;
	  save_input_line_pointer = input_line_pointer;
	  input_line_pointer = token + 1;
	  debug ("Current input_line_pointer: %s\n", input_line_pointer);
	  retval = expression (&current_op->direct.direct_expr);
	  debug ("Expression type: %d\n", current_op->direct.direct_expr.X_op);
	  debug ("Expression addnum: %d\n", current_op->direct.direct_expr.X_add_number);
	  debug ("Segment: %d\n", retval);
	  input_line_pointer = save_input_line_pointer;
	  if (current_op->direct.direct_expr.X_op == O_constant)
	    {
	      current_op->direct.address = current_op->direct.direct_expr.X_add_number;
	      current_op->direct.resolved = 1;
	    }
	}
      else
	{
	  debug ("Direct reference is a number\n");
	  current_op->direct.address = atoi (token + 1);
	  current_op->direct.resolved = 1;
	}
      current_op->op_type = Direct;
    }
  else if (*token == INDIRECT_REFERENCE)
    {				/* Indirect reference operand */
      int found_ar = 0;
      int found_disp = 0;
      int ar_number = -1;
      int disp_number = 0;
      int buffer_posn = 1;
      ind_addr_type *ind_addr_op;
      debug ("Found indirect reference\n");
      ind_buffer[0] = *token;
      for (count = 1; count < strlen (token); count++)
	{			/* Strip operand */
	  ind_buffer[buffer_posn] = TOLOWER (*(token + count));
	  if ((*(token + count - 1) == 'a' || *(token + count - 1) == 'A') &&
	      (*(token + count) == 'r' || *(token + count) == 'R'))
	    {
	      /* AR reference is found, so get its number and remove it from the buffer
	         so it can pass through hash_find() */
	      if (found_ar)
		{
		  as_bad ("More than one AR register found in indirect reference");
		  return NULL;
		}
	      if (*(token + count + 1) < '0' || *(token + count + 1) > '7')
		{
		  as_bad ("Illegal AR register in indirect reference");
		  return NULL;
		}
	      ar_number = *(token + count + 1) - '0';
	      found_ar = 1;
	      count++;
	    }
	  if (*(token + count) == '(')
	    {
	      /* Parenthesis found, so check if a displacement value is inside.  If so, get
	         the value and remove it from the buffer.  */
	      if (is_digit_char (*(token + count + 1)))
		{
		  char disp[10];
		  int disp_posn = 0;

		  if (found_disp)
		    {
		      as_bad ("More than one displacement found in indirect reference");
		      return NULL;
		    }
		  count++;
		  while (*(token + count) != ')')
		    {
		      if (!is_digit_char (*(token + count)))
			{
			  as_bad ("Invalid displacement in indirect reference");
			  return NULL;
			}
		      disp[disp_posn++] = *(token + (count++));
		    }
		  disp[disp_posn] = '\0';
		  disp_number = atoi (disp);
		  count--;
		  found_disp = 1;
		}
	    }
	  buffer_posn++;
	}
      ind_buffer[buffer_posn] = '\0';
      if (!found_ar)
	{
	  as_bad ("AR register not found in indirect reference");
	  return NULL;
	}
      ind_addr_op = (ind_addr_type *) hash_find (ind_hash, ind_buffer);
      if (ind_addr_op)
	{
	  debug ("Found indirect reference: %s\n", ind_addr_op->syntax);
	  if (ind_addr_op->displacement == IMPLIED_DISP)
	    {
	      found_disp = 1;
	      disp_number = 1;
	    }
	  else if ((ind_addr_op->displacement == DISP_REQUIRED) && !found_disp)
	    {
	      /* Maybe an implied displacement of 1 again */
	      as_bad ("required displacement wasn't given in indirect reference");
	      return 0;
	    }
	}
      else
	{
	  as_bad ("illegal indirect reference");
	  return NULL;
	}
      if (found_disp && (disp_number < 0 || disp_number > 255))
	{
	  as_bad ("displacement must be an unsigned 8-bit number");
	  return NULL;
	}
      current_op->indirect.mod = ind_addr_op->modfield;
      current_op->indirect.disp = disp_number;
      current_op->indirect.ARnum = ar_number;
      current_op->op_type = Indirect;
    }
  else
    {
      reg *regop = (reg *) hash_find (reg_hash, token);
      if (regop)
	{
	  debug ("Found register operand: %s\n", regop->name);
	  if (regop->regtype == REG_ARn)
	    current_op->op_type = ARn;
	  else if (regop->regtype == REG_Rn)
	    current_op->op_type = Rn;
	  else if (regop->regtype == REG_DP)
	    current_op->op_type = DPReg;
	  else
	    current_op->op_type = OtherReg;
	  current_op->reg.opcode = regop->opcode;
	}
      else
	{
	  if (!is_digit_char (*token) || *(token + 1) == 'x' || strchr (token, 'h'))
	    {
	      char *save_input_line_pointer;
	      segT retval;
	      debug ("Probably a label: %s\n", token);
	      current_op->immediate.label = (char *) malloc (strlen (token) + 1);
	      strcpy (current_op->immediate.label, token);
	      current_op->immediate.label[strlen (token)] = '\0';
	      save_input_line_pointer = input_line_pointer;
	      input_line_pointer = token;
	      debug ("Current input_line_pointer: %s\n", input_line_pointer);
	      retval = expression (&current_op->immediate.imm_expr);
	      debug ("Expression type: %d\n", current_op->immediate.imm_expr.X_op);
	      debug ("Expression addnum: %d\n", current_op->immediate.imm_expr.X_add_number);
	      debug ("Segment: %d\n", retval);
	      input_line_pointer = save_input_line_pointer;
	      if (current_op->immediate.imm_expr.X_op == O_constant)
		{
		  current_op->immediate.s_number = current_op->immediate.imm_expr.X_add_number;
		  current_op->immediate.u_number = (unsigned int) current_op->immediate.imm_expr.X_add_number;
		  current_op->immediate.resolved = 1;
		}
	    }
	  else
	    {
	      unsigned count;
	      debug ("Found a number or displacement\n");
	      for (count = 0; count < strlen (token); count++)
		if (*(token + count) == '.')
		  current_op->immediate.decimal_found = 1;
	      current_op->immediate.label = (char *) malloc (strlen (token) + 1);
	      strcpy (current_op->immediate.label, token);
	      current_op->immediate.label[strlen (token)] = '\0';
	      current_op->immediate.f_number = (float) atof (token);
	      current_op->immediate.s_number = (int) atoi (token);
	      current_op->immediate.u_number = (unsigned int) atoi (token);
	      current_op->immediate.resolved = 1;
	    }
	  current_op->op_type = Disp | Abs24 | Imm16 | Imm24;
	  if (current_op->immediate.u_number <= 31)
	    current_op->op_type |= IVector;
	}
    }
  return current_op;
}

/* next_line points to the next line after the current instruction (current_line).
   Search for the parallel bars, and if found, merge two lines into internal syntax
   for a parallel instruction:
   q_[INSN1]_[INSN2] [OPERANDS1] | [OPERANDS2]
   By this stage, all comments are scrubbed, and only the bare lines are given.
 */

#define NONE           0
#define START_OPCODE   1
#define END_OPCODE     2
#define START_OPERANDS 3
#define END_OPERANDS   4

char *
tic30_find_parallel_insn (current_line, next_line)
     char *current_line;
     char *next_line;
{
  int found_parallel = 0;
  char first_opcode[256];
  char second_opcode[256];
  char first_operands[256];
  char second_operands[256];
  char *parallel_insn;

  debug ("In tic30_find_parallel_insn()\n");
  while (!is_end_of_line[(unsigned char) *next_line])
    {
      if (*next_line == PARALLEL_SEPARATOR && *(next_line + 1) == PARALLEL_SEPARATOR)
	{
	  found_parallel = 1;
	  next_line++;
	  break;
	}
      next_line++;
    }
  if (!found_parallel)
    return NULL;
  debug ("Found a parallel instruction\n");
  {
    int i;
    char *opcode, *operands, *line;

    for (i = 0; i < 2; i++)
      {
	if (i == 0)
	  {
	    opcode = &first_opcode[0];
	    operands = &first_operands[0];
	    line = current_line;
	  }
	else
	  {
	    opcode = &second_opcode[0];
	    operands = &second_operands[0];
	    line = next_line;
	  }
	{
	  int search_status = NONE;
	  int char_ptr = 0;
	  char c;

	  while (!is_end_of_line[(unsigned char) (c = *line)])
	    {
	      if (is_opcode_char (c) && search_status == NONE)
		{
		  opcode[char_ptr++] = TOLOWER (c);
		  search_status = START_OPCODE;
		}
	      else if (is_opcode_char (c) && search_status == START_OPCODE)
		{
		  opcode[char_ptr++] = TOLOWER (c);
		}
	      else if (!is_opcode_char (c) && search_status == START_OPCODE)
		{
		  opcode[char_ptr] = '\0';
		  char_ptr = 0;
		  search_status = END_OPCODE;
		}
	      else if (is_operand_char (c) && search_status == START_OPERANDS)
		{
		  operands[char_ptr++] = c;
		}
	      if (is_operand_char (c) && search_status == END_OPCODE)
		{
		  operands[char_ptr++] = c;
		  search_status = START_OPERANDS;
		}
	      line++;
	    }
	  if (search_status != START_OPERANDS)
	    return NULL;
	  operands[char_ptr] = '\0';
	}
      }
  }
  parallel_insn = (char *) malloc (strlen (first_opcode) + strlen (first_operands) +
		     strlen (second_opcode) + strlen (second_operands) + 8);
  sprintf (parallel_insn, "q_%s_%s %s | %s", first_opcode, second_opcode, first_operands, second_operands);
  debug ("parallel insn = %s\n", parallel_insn);
  return parallel_insn;
}

#undef NONE
#undef START_OPCODE
#undef END_OPCODE
#undef START_OPERANDS
#undef END_OPERANDS

/* In order to get gas to ignore any | chars at the start of a line,
   this function returns true if a | is found in a line.  */

int
tic30_unrecognized_line (c)
     int c;
{
  debug ("In tc_unrecognized_line\n");
  return (c == PARALLEL_SEPARATOR);
}

int
md_estimate_size_before_relax (fragP, segment)
     fragS *fragP ATTRIBUTE_UNUSED;
     segT segment ATTRIBUTE_UNUSED;
{
  debug ("In md_estimate_size_before_relax()\n");
  return 0;
}

void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     register fragS *fragP ATTRIBUTE_UNUSED;
{
  debug ("In md_convert_frag()\n");
}

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  valueT value = *valP;

  debug ("In md_apply_fix() with value = %ld\n", (long) value);
  debug ("Values in fixP\n");
  debug ("fx_size = %d\n", fixP->fx_size);
  debug ("fx_pcrel = %d\n", fixP->fx_pcrel);
  debug ("fx_where = %d\n", fixP->fx_where);
  debug ("fx_offset = %d\n", (int) fixP->fx_offset);
  {
    char *buf = fixP->fx_frag->fr_literal + fixP->fx_where;

    value /= INSN_SIZE;
    if (fixP->fx_size == 1)
      /* Special fix for LDP instruction.  */
      value = (value & 0x00FF0000) >> 16;

    debug ("new value = %ld\n", (long) value);
    md_number_to_chars (buf, value, fixP->fx_size);
  }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

int
md_parse_option (c, arg)
     int c ATTRIBUTE_UNUSED;
     char *arg ATTRIBUTE_UNUSED;
{
  debug ("In md_parse_option()\n");
  return 0;
}

void
md_show_usage (stream)
     FILE *stream ATTRIBUTE_UNUSED;
{
  debug ("In md_show_usage()\n");
}

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  debug ("In md_undefined_symbol()\n");
  return (symbolS *) 0;
}

valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  debug ("In md_section_align() segment = %d and size = %d\n", segment, size);
  size = (size + 3) / 4;
  size *= 4;
  debug ("New size value = %d\n", size);
  return size;
}

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  int offset;

  debug ("In md_pcrel_from()\n");
  debug ("fx_where = %d\n", fixP->fx_where);
  debug ("fx_size = %d\n", fixP->fx_size);
  /* Find the opcode that represents the current instruction in the fr_literal
     storage area, and check bit 21.  Bit 21 contains whether the current instruction
     is a delayed one or not, and then set the offset value appropriately.  */
  if (fixP->fx_frag->fr_literal[fixP->fx_where - fixP->fx_size + 1] & 0x20)
    offset = 3;
  else
    offset = 1;
  debug ("offset = %d\n", offset);
  /* PC Relative instructions have a format:
     displacement = Label - (PC + offset)
     This function returns PC + offset where:
     fx_where - fx_size = PC
     INSN_SIZE * offset = offset number of instructions
   */
  return fixP->fx_where - fixP->fx_size + (INSN_SIZE * offset);
}

char *
md_atof (what_statement_type, literalP, sizeP)
     int what_statement_type;
     char *literalP;
     int *sizeP;
{
  int prec;
  char *token;
  char keepval;
  unsigned long value;
  float float_value;
  debug ("In md_atof()\n");
  debug ("precision = %c\n", what_statement_type);
  debug ("literal = %s\n", literalP);
  debug ("line = ");
  token = input_line_pointer;
  while (!is_end_of_line[(unsigned char) *input_line_pointer]
	 && (*input_line_pointer != ','))
    {
      debug ("%c", *input_line_pointer);
      input_line_pointer++;
    }
  keepval = *input_line_pointer;
  *input_line_pointer = '\0';
  debug ("\n");
  float_value = (float) atof (token);
  *input_line_pointer = keepval;
  debug ("float_value = %f\n", float_value);
  switch (what_statement_type)
    {
    case 'f':
    case 'F':
    case 's':
    case 'S':
      prec = 2;
      break;

    case 'd':
    case 'D':
    case 'r':
    case 'R':
      prec = 4;
      break;

    default:
      *sizeP = 0;
      return "Bad call to MD_ATOF()";
    }
  if (float_value == 0.0)
    {
      value = (prec == 2) ? 0x00008000L : 0x80000000L;
    }
  else
    {
      unsigned long exp, sign, mant, tmsfloat;
      tmsfloat = *((long *) &float_value);
      sign = tmsfloat & 0x80000000;
      mant = tmsfloat & 0x007FFFFF;
      exp = tmsfloat & 0x7F800000;
      exp <<= 1;
      if (exp == 0xFF000000)
	{
	  if (mant == 0)
	    value = 0x7F7FFFFF;
	  else if (sign == 0)
	    value = 0x7F7FFFFF;
	  else
	    value = 0x7F800000;
	}
      else
	{
	  exp -= 0x7F000000;
	  if (sign)
	    {
	      mant = mant & 0x007FFFFF;
	      mant = -mant;
	      mant = mant & 0x00FFFFFF;
	      if (mant == 0)
		{
		  mant |= 0x00800000;
		  exp = (long) exp - 0x01000000;
		}
	    }
	  tmsfloat = exp | mant;
	  value = tmsfloat;
	}
      if (prec == 2)
	{
	  long exp, mant;

	  if (tmsfloat == 0x80000000)
	    {
	      value = 0x8000;
	    }
	  else
	    {
	      value = 0;
	      exp = (tmsfloat & 0xFF000000);
	      exp >>= 24;
	      mant = tmsfloat & 0x007FFFFF;
	      if (tmsfloat & 0x00800000)
		{
		  mant |= 0xFF000000;
		  mant += 0x00000800;
		  mant >>= 12;
		  mant |= 0x00000800;
		  mant &= 0x0FFF;
		  if (exp > 7)
		    value = 0x7800;
		}
	      else
		{
		  mant |= 0x00800000;
		  mant += 0x00000800;
		  exp += (mant >> 24);
		  mant >>= 12;
		  mant &= 0x07FF;
		  if (exp > 7)
		    value = 0x77FF;
		}
	      if (exp < -8)
		value = 0x8000;
	      if (value == 0)
		{
		  mant = (exp << 12) | mant;
		  value = mant & 0xFFFF;
		}
	    }
	}
    }
  md_number_to_chars (literalP, value, prec);
  *sizeP = prec;
  return 0;
}

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  debug ("In md_number_to_chars()\n");
  number_to_chars_bigendian (buf, val, n);
  /*  number_to_chars_littleendian(buf,val,n); */
}

#define F(SZ,PCREL)		(((SZ) << 1) + (PCREL))
#define MAP(SZ,PCREL,TYPE)	case F(SZ,PCREL): code = (TYPE); break

arelent *
tc_gen_reloc (section, fixP)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixP;
{
  arelent *rel;
  bfd_reloc_code_real_type code = 0;

  debug ("In tc_gen_reloc()\n");
  debug ("fixP.size = %d\n", fixP->fx_size);
  debug ("fixP.pcrel = %d\n", fixP->fx_pcrel);
  debug ("addsy.name = %s\n", S_GET_NAME (fixP->fx_addsy));
  switch (F (fixP->fx_size, fixP->fx_pcrel))
    {
      MAP (1, 0, BFD_RELOC_TIC30_LDP);
      MAP (2, 0, BFD_RELOC_16);
      MAP (3, 0, BFD_RELOC_24);
      MAP (2, 1, BFD_RELOC_16_PCREL);
      MAP (4, 0, BFD_RELOC_32);
    default:
      as_bad ("Can not do %d byte %srelocation", fixP->fx_size,
	      fixP->fx_pcrel ? "pc-relative " : "");
    }
#undef MAP
#undef F

  rel = (arelent *) xmalloc (sizeof (arelent));
  assert (rel != 0);
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  rel->address = fixP->fx_frag->fr_address + fixP->fx_where;
  rel->addend = 0;
  rel->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (!rel->howto)
    {
      const char *name;
      name = S_GET_NAME (fixP->fx_addsy);
      if (name == NULL)
	name = "<unknown>";
      as_fatal ("Cannot generate relocation type for symbol %s, code %s", name, bfd_get_reloc_code_name (code));
    }
  return rel;
}

void
md_operand (expressionP)
     expressionS *expressionP ATTRIBUTE_UNUSED;
{
  debug ("In md_operand()\n");
}

char output_invalid_buf[8];

char *
output_invalid (c)
     char c;
{
  if (ISPRINT (c))
    sprintf (output_invalid_buf, "'%c'", c);
  else
    sprintf (output_invalid_buf, "(0x%x)", (unsigned) c);
  return output_invalid_buf;
}
