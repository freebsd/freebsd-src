/* tc-pdp11.c - pdp11-specific -
   Copyright 2001, 2002 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
  Apparently unused functions:
    md_convert_frag
    md_estimate_size_before_relax
    md_create_short_jump
    md_create_long_jump
*/

#include "as.h"
#include "safe-ctype.h"
#include "opcode/pdp11.h"

static int set_option PARAMS ((char *arg));
static int set_cpu_model PARAMS ((char *arg));
static int set_machine_model PARAMS ((char *arg));

extern int flonum_gen2vax PARAMS ((char format_letter, FLONUM_TYPE * f,
                                   LITTLENUM_TYPE * words));

#define TRUE 1
#define FALSE 0

/*
 * A representation for PDP-11 machine code.
 */
struct pdp11_code
{
  char *error;
  int code;
  int additional;	/* is there an additional word? */
  int word;		/* additional word, if any */
  struct
  {
    bfd_reloc_code_real_type type;
    expressionS exp;
    int pc_rel;
  } reloc;
};

/*
 * Instruction set extensions.
 *
 * If you change this from an array to something else, please update
 * the "PDP-11 instruction set extensions" comment in pdp11.h.
 */
int pdp11_extension[PDP11_EXT_NUM];

/*
 * Assembly options.
 */

#define ASM_OPT_PIC 1
#define ASM_OPT_NUM 2

int asm_option[ASM_OPT_NUM];

/* These chars start a comment anywhere in a source file (except inside
   another comment */
const char comment_chars[] = "#/";

/* These chars only start a comment at the beginning of a line.  */
const char line_comment_chars[] = "#/";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* as in 0f123.456 */
/* or    0H1.234E-12 (see exp chars above) */
const char FLT_CHARS[] = "dDfF";

void pseudo_even (int);
void pseudo_bss (int);

const pseudo_typeS md_pseudo_table[] =
{
  { "bss", pseudo_bss, 0 },
  { "even", pseudo_even, 0 },
  { 0, 0, 0 },
};

static void
init_defaults ()
{
  static int first = 1;

  if (first)
    {
      set_option ("all-extensions");
      set_option ("pic");
      first = 0;
    }
}

static struct hash_control *insn_hash = NULL;

void
md_begin ()
{
  int i;

  init_defaults ();

  insn_hash = hash_new ();
  if (insn_hash == NULL)
    as_fatal ("Virtual memory exhausted");

  for (i = 0; i < pdp11_num_opcodes; i++)
    hash_insert (insn_hash, pdp11_opcodes[i].name, (PTR)(pdp11_opcodes + i));
  for (i = 0; i < pdp11_num_aliases; i++)
    hash_insert (insn_hash, pdp11_aliases[i].name, (PTR)(pdp11_aliases + i));
}

void
md_number_to_chars (con, value, nbytes)
     char con[];
     valueT value;
     int nbytes;
{
  /* On a PDP-11, 0x1234 is stored as "\x12\x34", and
   * 0x12345678 is stored as "\x56\x78\x12\x34". It's
   * anyones guess what 0x123456 would be stored like.
   */

  switch (nbytes)
    {
    case 0:
      break;
    case 1:
      con[0] =  value       & 0xff;
      break;
    case 2:
      con[0] =  value       & 0xff;
      con[1] = (value >>  8) & 0xff;
      break;
    case 4:
      con[0] = (value >> 16) & 0xff;
      con[1] = (value >> 24) & 0xff;
      con[2] =  value       & 0xff;
      con[3] = (value >>  8) & 0xff;
      break;
    default:
      BAD_CASE (nbytes);
    }
}

/* Fix up some data or instructions after we find out the value of a symbol
   that they reference.  Knows about order of bytes in address.  */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT * valP;
     segT seg ATTRIBUTE_UNUSED;
{
  valueT code;
  valueT mask;
  valueT val = * valP;
  char *buf;
  int shift;
  int size;

  buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  size = fixP->fx_size;
  code = md_chars_to_number (buf, size);

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_16:
    case BFD_RELOC_16_PCREL:
      mask = 0xffff;
      shift = 0;
      break;
    case BFD_RELOC_PDP11_DISP_8_PCREL:
      mask = 0x00ff;
      shift = 1;
      break;
    case BFD_RELOC_PDP11_DISP_6_PCREL:
      mask = 0x003f;
      shift = 1;
      break;
    default:
      BAD_CASE (fixP->fx_r_type);
    }

  if (fixP->fx_addsy != NULL)
    val += symbol_get_bfdsym (fixP->fx_addsy)->section->vma;
    /* *value += fixP->fx_addsy->bsym->section->vma; */

  code &= ~mask;
  code |= (val >> shift) & mask;
  number_to_chars_littleendian (buf, code, size);

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

long
md_chars_to_number (con, nbytes)
     unsigned char con[];	/* Low order byte 1st.  */
     int nbytes;		/* Number of bytes in the input.  */
{
  /* On a PDP-11, 0x1234 is stored as "\x12\x34", and
   * 0x12345678 is stored as "\x56\x78\x12\x34". It's
   * anyones guess what 0x123456 would be stored like.
   */

  switch (nbytes)
    {
    case 0:
      return 0;
    case 1:
      return con[0];
    case 2:
      return (con[1] << BITS_PER_CHAR) | con[0];
    case 4:
      return
	(((con[1] << BITS_PER_CHAR) | con[0]) << (2 * BITS_PER_CHAR)) |
	 ((con[3] << BITS_PER_CHAR) | con[2]);
    default:
      BAD_CASE (nbytes);
      return 0;
    }
}

static char *
skip_whitespace (char *str)
{
  while (*str == ' ' || *str == '\t')
    str++;
  return str;
}

static char *
find_whitespace (char *str)
{
  while (*str != ' ' && *str != '\t' && *str != 0)
    str++;
  return str;
}

static char *
parse_reg (char *str, struct pdp11_code *operand)
{
  str = skip_whitespace (str);
  if (TOLOWER (*str) == 'r')
    {
      str++;
      switch (*str)
	{
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	  operand->code = *str - '0';
	  str++;
	  break;
	default:
	  operand->error = "Bad register name";
	  return str - 1;
	}
    }
  else if (strncmp (str, "sp", 2) == 0 ||
	   strncmp (str, "SP", 2) == 0)
    {
      operand->code = 6;
      str += 2;
    }
  else if (strncmp (str, "pc", 2) == 0 ||
	   strncmp (str, "PC", 2) == 0)
    {
      operand->code = 7;
      str += 2;
    }
  else
    {
      operand->error = "Bad register name";
      return str;
    }

  return str;
}

static char *
parse_ac5 (char *str, struct pdp11_code *operand)
{
  str = skip_whitespace (str);
  if (strncmp (str, "fr", 2) == 0 ||
      strncmp (str, "FR", 2) == 0 ||
      strncmp (str, "ac", 2) == 0 ||
      strncmp (str, "AC", 2) == 0)
    {
      str += 2;
      switch (*str)
	{
	case '0': case '1': case '2': case '3':
        case '4': case '5':
	  operand->code = *str - '0';
	  str++;
	  break;
	default:
	  operand->error = "Bad register name";
	  return str - 2;
	}
    }
  else
    {
      operand->error = "Bad register name";
      return str;
    }

  return str;
}

static char *
parse_ac (char *str, struct pdp11_code *operand)
{
  str = parse_ac5 (str, operand);
  if (!operand->error && operand->code > 3)
    {
	  operand->error = "Bad register name";
	  return str - 3;
    }

  return str;
}

static char *
parse_expression (char *str, struct pdp11_code *operand)
{
  char *save_input_line_pointer;
  segT seg;

  save_input_line_pointer = input_line_pointer;
  input_line_pointer = str;
  seg = expression (&operand->reloc.exp);
  if (seg == NULL)
    {
      input_line_pointer = save_input_line_pointer;
      operand->error = "Error in expression";
      return str;
    }

  str = input_line_pointer;
  input_line_pointer = save_input_line_pointer;

  operand->reloc.pc_rel = 0;

#if 0
  /* FIXME: what follows is broken badly.  You can't deal with differences
     in radix conventions this way, because of symbolic constants, constant
     expressions made up of pieces of differing radix, etc.  The only
     choices are to change ../expr.c to know about pdp11 conventions, or
     to accept the fact that gas will use consistent conventions that differ
     from those of traditional pdp11 assemblers.  For now, I've
     chosen the latter.   paul koning, 12/23/2001
  */
  if (operand->reloc.exp.X_op == O_constant)
    {
      if (*str == '.')
	str++;
      else
	{
	  /* FIXME: buffer overflow! */
	  char buf[100];
	  char *end;

	  sprintf (buf, "%ld", operand->reloc.exp.X_add_number);
	  operand->reloc.exp.X_add_number = strtol (buf, &end, 8);
	}
    }
#endif
  return str;
}

static char *
parse_op_no_deferred (char *str, struct pdp11_code *operand)
{
  LITTLENUM_TYPE literal_float[2];

  str = skip_whitespace (str);

  switch (*str)
    {
    case '(':				/* (rn) and (rn)+ */
      str = parse_reg (str + 1, operand);
      if (operand->error)
	return str;
      str = skip_whitespace (str);
      if (*str != ')')
	{
	  operand->error = "Missing ')'";
	  return str;
	}
      str++;
      if (*str == '+')
	{
	  operand->code |= 020;
	  str++;
	}
      else
	{
	  operand->code |= 010;
	}
      break;

    case '#':				/* immediate */
    case '$':
      str = parse_expression (str + 1, operand);
      if (operand->error)
	return str;
      operand->additional = TRUE;
      operand->word = operand->reloc.exp.X_add_number;
      switch (operand->reloc.exp.X_op)
	{
	case O_constant:
	  break;
	case O_symbol:
	case O_add:
	case O_subtract:
	  operand->reloc.type = BFD_RELOC_16;
	  operand->reloc.pc_rel = 0;
	  break;
        case O_big:
          if (operand->reloc.exp.X_add_number > 0)
            {
              operand->error = "Error in expression";
              break;
            }
          /* it's a floating literal...  */
          know (operand->reloc.exp.X_add_number < 0);
          flonum_gen2vax ('f', &generic_floating_point_number, literal_float);
          operand->word = literal_float[0];
          if (literal_float[1] != 0)
            as_warn (_("Low order bits truncated in immediate float operand"));
          break;
	default:
	  operand->error = "Error in expression";
	  break;
	}
      operand->code = 027;
      break;

    default:				/* label, d(rn), -(rn) */
      {
	char *old = str;

	if (strncmp (str, "-(", 2) == 0)	/* -(rn) */
	  {
	    str = parse_reg (str + 2, operand);
	    if (operand->error)
	      return str;
	    str = skip_whitespace (str);
	    if (*str != ')')
	      {
		operand->error = "Missing ')'";
		return str;
	      }
	    operand->code |= 040;
	    str++;
	    break;
	  }

	str = parse_expression (str, operand);
	if (operand->error)
	  return str;

	str = skip_whitespace (str);

	if (*str != '(')		/* label */
	  {
	    if (operand->reloc.exp.X_op != O_symbol)
	      {
		operand->error = "Label expected";
		return old;
	      }
	    operand->code = 067;
	    operand->additional = 1;
	    operand->word = 0;
	    operand->reloc.type = BFD_RELOC_16_PCREL;
	    operand->reloc.pc_rel = 1;
	    break;
	  }

	str++;				/* d(rn) */
	str = parse_reg (str, operand);
	if (operand->error)
	  return str;

	str = skip_whitespace (str);

	if (*str != ')')
	  {
	    operand->error = "Missing ')'";
	    return str;
	  }

	str++;
	operand->additional = TRUE;
	operand->code |= 060;
	switch (operand->reloc.exp.X_op)
	  {
	  case O_symbol:
	    operand->word = 0;
	    operand->reloc.pc_rel = 1;
	    break;
	  case O_constant:
	    if ((operand->code & 7) == 7)
	      {
		operand->reloc.pc_rel = 1;
		operand->word = operand->reloc.exp.X_add_number;
	      }
	    else
	      {
		operand->word = operand->reloc.exp.X_add_number;
	      }
	    break;
	  default:
	    BAD_CASE (operand->reloc.exp.X_op);
	  }
	break;
      }
    }

  return str;
}

static char *
parse_op_noreg (char *str, struct pdp11_code *operand)
{
  str = skip_whitespace (str);
  operand->error = NULL;

  if (*str == '@' || *str == '*')
    {
      str = parse_op_no_deferred (str + 1, operand);
      if (operand->error)
	return str;
      operand->code |= 010;
    }
  else
    str = parse_op_no_deferred (str, operand);

  return str;
}

static char *
parse_op (char *str, struct pdp11_code *operand)
{
  str = skip_whitespace (str);

  str = parse_reg (str, operand);
  if (!operand->error)
    return str;

  operand->error = NULL;
  parse_ac5 (str, operand);
  if (!operand->error)
    {
      operand->error = "Float AC not legal as integer operand";
      return str;
    }

  return parse_op_noreg (str, operand);
}

static char *
parse_fop (char *str, struct pdp11_code *operand)
{
  str = skip_whitespace (str);

  str = parse_ac5 (str, operand);
  if (!operand->error)
    return str;

  operand->error = NULL;
  parse_reg (str, operand);
  if (!operand->error)
    {
      operand->error = "General register not legal as float operand";
      return str;
    }

  return parse_op_noreg (str, operand);
}

static char *
parse_separator (char *str, int *error)
{
  str = skip_whitespace (str);
  *error = (*str != ',');
  if (!*error)
    str++;
  return str;
}

void
md_assemble (instruction_string)
     char *instruction_string;
{
  const struct pdp11_opcode *op;
  struct pdp11_code insn, op1, op2;
  int error;
  int size;
  char *err = NULL;
  char *str;
  char *p;
  char c;

  str = skip_whitespace (instruction_string);
  p = find_whitespace (str);
  if (p - str == 0)
    {
      as_bad ("No instruction found");
      return;
    }

  c = *p;
  *p = '\0';
  op = (struct pdp11_opcode *)hash_find (insn_hash, str);
  *p = c;
  if (op == 0)
    {
#if 0
      op1.error = NULL;
      op1.additional = FALSE;
      op1.reloc.type = BFD_RELOC_NONE;
      op1.code = 0;
      op1.word = 0;
      str = parse_expression (str, &op1);
      if (op1.error)
	{
	  as_bad (op1.error);
	  return;
	}

      {
	char *to = frag_more (2);

	md_number_to_chars (to, op1.code, 2);
	if (insn.reloc.type != BFD_RELOC_NONE)
	  fix_new_exp (frag_now, to - frag_now->fr_literal, 2,
		       &insn.reloc.exp, insn.reloc.pc_rel, insn.reloc.type);
      }
#else
      as_bad (_("Unknown instruction '%s'"), str);
#endif

      return;
    }

  if (!pdp11_extension[op->extension])
    {
      as_warn ("Unsupported instruction set extension: %s", op->name);
      return;
    }

  insn.error = NULL;
  insn.code = op->opcode;
  insn.reloc.type = BFD_RELOC_NONE;
  op1.error = NULL;
  op1.additional = FALSE;
  op1.reloc.type = BFD_RELOC_NONE;
  op2.error = NULL;
  op2.additional = FALSE;
  op2.reloc.type = BFD_RELOC_NONE;

  str = p;
  size = 2;

  switch (op->type)
    {
    case PDP11_OPCODE_NO_OPS:
      str = skip_whitespace (str);
      if (*str == 0)
	str = "";
      break;

    case PDP11_OPCODE_IMM3:
    case PDP11_OPCODE_IMM6:
    case PDP11_OPCODE_IMM8:
      str = skip_whitespace (str);
      if (*str == '#' || *str == '$')
	str++;
      str = parse_expression (str, &op1);
      if (op1.error)
	break;
      if (op1.reloc.exp.X_op != O_constant || op1.reloc.type != BFD_RELOC_NONE)
	{
	  op1.error = "operand is not an absolute constant";
	  break;
	}
      switch (op->type)
	{
	case PDP11_OPCODE_IMM3:
	  if (op1.reloc.exp.X_add_number & ~7)
	    {
	      op1.error = "3-bit immediate out of range";
	      break;
	    }
	  break;
	case PDP11_OPCODE_IMM6:
	  if (op1.reloc.exp.X_add_number & ~0x3f)
	    {
	      op1.error = "6-bit immediate out of range";
	      break;
	    }
	  break;
	case PDP11_OPCODE_IMM8:
	  if (op1.reloc.exp.X_add_number & ~0xff)
	    {
	      op1.error = "8-bit immediate out of range";
	      break;
	    }
	  break;
	}
      insn.code |= op1.reloc.exp.X_add_number;
      break;

    case PDP11_OPCODE_DISPL:
      {
	char *new;
	new = parse_expression (str, &op1);
	op1.code = 0;
	op1.reloc.pc_rel = 1;
	op1.reloc.type = BFD_RELOC_PDP11_DISP_8_PCREL;
	if (op1.reloc.exp.X_op != O_symbol)
	  {
	    op1.error = "Symbol expected";
	    break;
	  }
	if (op1.code & ~0xff)
	  {
	    err = "8-bit displacement out of range";
	    break;
	  }
	str = new;
	insn.code |= op1.code;
	insn.reloc = op1.reloc;
      }
      break;

    case PDP11_OPCODE_REG:
      str = parse_reg (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      break;

    case PDP11_OPCODE_OP:
      str = parse_op (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      break;

    case PDP11_OPCODE_FOP:
      str = parse_fop (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      break;

    case PDP11_OPCODE_REG_OP:
      str = parse_reg (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code << 6;
      str = parse_separator (str, &error);
      if (error)
	{
	  op2.error = "Missing ','";
	  break;
	}
      str = parse_op (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      break;

    case PDP11_OPCODE_REG_OP_REV:
      str = parse_op (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      str = parse_separator (str, &error);
      if (error)
	{
	  op2.error = "Missing ','";
	  break;
	}
      str = parse_reg (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code << 6;
      break;

    case PDP11_OPCODE_AC_FOP:
      str = parse_ac (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code << 6;
      str = parse_separator (str, &error);
      if (error)
	{
	  op1.error = "Missing ','";
	  break;
	}
      str = parse_fop (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      break;

    case PDP11_OPCODE_FOP_AC:
      str = parse_fop (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      str = parse_separator (str, &error);
      if (error)
	{
	  op1.error = "Missing ','";
	  break;
	}
      str = parse_ac (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code << 6;
      break;

    case PDP11_OPCODE_AC_OP:
      str = parse_ac (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code << 6;
      str = parse_separator (str, &error);
      if (error)
	{
	  op1.error = "Missing ','";
	  break;
	}
      str = parse_op (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      break;

    case PDP11_OPCODE_OP_AC:
      str = parse_op (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code;
      if (op1.additional)
	size += 2;
      str = parse_separator (str, &error);
      if (error)
	{
	  op1.error = "Missing ','";
	  break;
	}
      str = parse_ac (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code << 6;
      break;

    case PDP11_OPCODE_OP_OP:
      str = parse_op (str, &op1);
      if (op1.error)
	break;
      insn.code |= op1.code << 6;
      if (op1.additional)
	size += 2;
      str = parse_separator (str, &error);
      if (error)
	{
	  op2.error = "Missing ','";
	  break;
	}
      str = parse_op (str, &op2);
      if (op2.error)
	break;
      insn.code |= op2.code;
      if (op2.additional)
	size += 2;
      break;

    case PDP11_OPCODE_REG_DISPL:
      {
	char *new;
	str = parse_reg (str, &op2);
	if (op2.error)
	  break;
	insn.code |= op2.code << 6;
	str = parse_separator (str, &error);
	if (error)
	  {
	    op1.error = "Missing ','";
	    break;
	  }
	new = parse_expression (str, &op1);
	op1.code = 0;
	op1.reloc.pc_rel = 1;
	op1.reloc.type = BFD_RELOC_PDP11_DISP_6_PCREL;
	if (op1.reloc.exp.X_op != O_symbol)
	  {
	    op1.error = "Symbol expected";
	    break;
	  }
	if (op1.code & ~0x3f)
	  {
	    err = "6-bit displacement out of range";
	    break;
	  }
	str = new;
	insn.code |= op1.code;
	insn.reloc = op1.reloc;
      }
      break;

    default:
      BAD_CASE (op->type);
    }

  if (op1.error)
    err = op1.error;
  else if (op2.error)
    err = op2.error;
  else
    {
      str = skip_whitespace (str);
      if (*str)
	err = "Too many operands";
    }

  {
    char *to = NULL;

    if (err)
      {
	as_bad (err);
	return;
      }

    to = frag_more (size);

    md_number_to_chars (to, insn.code, 2);
    if (insn.reloc.type != BFD_RELOC_NONE)
      fix_new_exp (frag_now, to - frag_now->fr_literal, 2,
		   &insn.reloc.exp, insn.reloc.pc_rel, insn.reloc.type);
    to += 2;

    if (op1.additional)
      {
	md_number_to_chars (to, op1.word, 2);
	if (op1.reloc.type != BFD_RELOC_NONE)
	  fix_new_exp (frag_now, to - frag_now->fr_literal, 2,
		       &op1.reloc.exp, op1.reloc.pc_rel, op1.reloc.type);
	to += 2;
      }

    if (op2.additional)
      {
	md_number_to_chars (to, op2.word, 2);
	if (op2.reloc.type != BFD_RELOC_NONE)
	  fix_new_exp (frag_now, to - frag_now->fr_literal, 2,
		       &op2.reloc.exp, op2.reloc.pc_rel, op2.reloc.type);
      }
  }
}

int
md_estimate_size_before_relax (fragP, segment)
     fragS *fragP ATTRIBUTE_UNUSED;
     segT segment ATTRIBUTE_UNUSED;
{
  return 0;
}

void
md_convert_frag (headers, seg, fragP)
     bfd *headers ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
}

const int md_short_jump_size = 2;
const int md_long_jump_size = 4;

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr ATTRIBUTE_UNUSED;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
}

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr ATTRIBUTE_UNUSED;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
}

static int
set_option (arg)
     char *arg;
{
  int yes = 1;

  if (strcmp (arg, "all-extensions") == 0 ||
      strcmp (arg, "all") == 0)
    {
      memset (pdp11_extension, ~0, sizeof pdp11_extension);
      pdp11_extension[PDP11_NONE] = 0;
      return 1;
    }
  else if (strcmp (arg, "no-extensions") == 0)
    {
      memset (pdp11_extension, 0, sizeof pdp11_extension);
      pdp11_extension[PDP11_BASIC] = 1;
      return 1;
    }

  if (strncmp (arg, "no-", 3) == 0)
    {
      yes = 0;
      arg += 3;
    }

  if (strcmp (arg, "cis") == 0)			/* commersial instructions */
    pdp11_extension[PDP11_CIS] = yes;
  else if (strcmp (arg, "csm") == 0)		/* call supervisor mode */
    pdp11_extension[PDP11_CSM] = yes;
  else if (strcmp (arg, "eis") == 0)		/* extended instruction set */
    pdp11_extension[PDP11_EIS] = pdp11_extension[PDP11_LEIS] = yes;
  else if (strcmp (arg, "fis") == 0 ||		/* KEV11 floating-point */
	   strcmp (arg, "kev11") == 0 ||
	   strcmp (arg, "kev-11") == 0)
    pdp11_extension[PDP11_FIS] = yes;
  else if (strcmp (arg, "fpp") == 0 ||		/* FP-11 floating-point */
	   strcmp (arg, "fpu") == 0 ||
	   strcmp (arg, "fp11") == 0 ||
	   strcmp (arg, "fp-11") == 0 ||
	   strcmp (arg, "fpj11") == 0 ||
	   strcmp (arg, "fp-j11") == 0 ||
	   strcmp (arg, "fpj-11") == 0)
    pdp11_extension[PDP11_FPP] = yes;
  else if (strcmp (arg, "limited-eis") == 0)	/* limited extended insns */
    {
      pdp11_extension[PDP11_LEIS] = yes;
      if (!pdp11_extension[PDP11_LEIS])
	pdp11_extension[PDP11_EIS] = 0;
    }
  else if (strcmp (arg, "mfpt") == 0)		/* move from processor type */
    pdp11_extension[PDP11_MFPT] = yes;
  else if (strncmp (arg, "mproc", 5) == 0 ||	/* multiprocessor insns: */
	   strncmp (arg, "multiproc", 9) == 0 )	/* TSTSET, WRTLCK */
    pdp11_extension[PDP11_MPROC] = yes;
  else if (strcmp (arg, "mxps") == 0)		/* move from/to proc status */
    pdp11_extension[PDP11_MXPS] = yes;
  else if (strcmp (arg, "pic") == 0)		/* position-independent code */
    asm_option[ASM_OPT_PIC] = yes;
  else if (strcmp (arg, "spl") == 0)		/* set priority level */
    pdp11_extension[PDP11_SPL] = yes;
  else if (strcmp (arg, "ucode") == 0 ||	/* microcode instructions: */
	   strcmp (arg, "microcode") == 0)	/* LDUB, MED, XFC */
    pdp11_extension[PDP11_UCODE] = yes;
  else
    return 0;

  return 1;
}

static int
set_cpu_model (arg)
     char *arg;
{
  char buf[4];
  char *model = buf;

  if (arg[0] == 'k')
    arg++;

  *model++ = *arg++;

  if (strchr ("abdx", model[-1]) == NULL)
    return 0;

  if (model[-1] == 'd')
    {
      if (arg[0] == 'f' ||
	  arg[0] == 'j')
	model[-1] = *arg++;
    }
  else if (model[-1] == 'x')
    {
      if (arg[0] == 't')
	model[-1] = *arg++;
    }

  if (arg[0] == '-')
    arg++;

  if (strncmp (arg, "11", 2) != 0)
    return 0;
  arg += 2;

  if (arg[0] == '-')
    {
      if (*++arg == 0)
	return 0;
    }

  /* allow up to two revision letters */
  if (arg[0] != 0)
    *model++ = *arg++;
  if (arg[0] != 0)
    *model++ = *arg++;

  *model++ = 0;

  set_option ("no-extensions");

  if (strncmp (buf, "a", 1) == 0)		/* KA11 (11/15/20) */
    return 1; /* no extensions */

  else if (strncmp (buf, "b", 1) == 0)		/* KB11 (11/45/50/55/70) */
    return set_option ("eis") &&
           set_option ("spl");

  else if (strncmp (buf, "da", 2) == 0)		/* KD11-A (11/35/40) */
    return set_option ("limited-eis");

  else if (strncmp (buf, "db", 2) == 0 ||	/* KD11-B (11/05/10) */
	   strncmp (buf, "dd", 2) == 0)		/* KD11-D (11/04) */
    return 1; /* no extensions */

  else if (strncmp (buf, "de", 2) == 0)		/* KD11-E (11/34) */
    return set_option ("eis") &&
           set_option ("mxps");

  else if (strncmp (buf, "df", 2) == 0 ||	/* KD11-F (11/03) */
	   strncmp (buf, "dh", 2) == 0 ||	/* KD11-H (11/03) */
	   strncmp (buf, "dq", 2) == 0)		/* KD11-Q (11/03) */
    return set_option ("limited-eis") &&
           set_option ("mxps");

  else if (strncmp (buf, "dk", 2) == 0)		/* KD11-K (11/60) */
    return set_option ("eis") &&
           set_option ("mxps") &&
           set_option ("ucode");

  else if (strncmp (buf, "dz", 2) == 0)		/* KD11-Z (11/44) */
    return set_option ("csm") &&
           set_option ("eis") &&
           set_option ("mfpt") &&
           set_option ("mxps") &&
           set_option ("spl");

  else if (strncmp (buf, "f", 1) == 0)		/* F11 (11/23/24) */
    return set_option ("eis") &&
           set_option ("mfpt") &&
           set_option ("mxps");

  else if (strncmp (buf, "j", 1) == 0)		/* J11 (11/53/73/83/84/93/94)*/
    return set_option ("csm") &&
           set_option ("eis") &&
           set_option ("mfpt") &&
           set_option ("multiproc") &&
           set_option ("mxps") &&
           set_option ("spl");

  else if (strncmp (buf, "t", 1) == 0)		/* T11 (11/21) */
    return set_option ("limited-eis") &&
           set_option ("mxps");

  else
    return 0;
}

static int
set_machine_model (arg)
     char *arg;
{
  if (strncmp (arg, "pdp-11/", 7) != 0 &&
      strncmp (arg, "pdp11/", 6) != 0 &&
      strncmp (arg, "11/", 3) != 0)
    return 0;

  if (strncmp (arg, "pdp", 3) == 0)
    arg += 3;
  if (arg[0] == '-')
    arg++;
  if (strncmp (arg, "11/", 3) == 0)
    arg += 3;

  if (strcmp (arg, "03") == 0)			/* 11/03 */
    return set_cpu_model ("kd11f");		/* KD11-F */

  else if (strcmp (arg, "04") == 0)		/* 11/04 */
    return set_cpu_model ("kd11d");		/* KD11-D */

  else if (strcmp (arg, "05") == 0 ||		/* 11/05 or 11/10 */
	   strcmp (arg, "10") == 0)
    return set_cpu_model ("kd11b");		/* KD11-B */

  else if (strcmp (arg, "15") == 0 ||		/* 11/15 or 11/20 */
	   strcmp (arg, "20") == 0)
    return set_cpu_model ("ka11");		/* KA11 */

  else if (strcmp (arg, "21") == 0)		/* 11/21 */
    return set_cpu_model ("t11");		/* T11 */

  else if (strcmp (arg, "23") == 0 ||		/* 11/23 or 11/24 */
	   strcmp (arg, "24") == 0)
    return set_cpu_model ("f11");		/* F11 */

  else if (strcmp (arg, "34") == 0 ||		/* 11/34 or 11/34a */
           strcmp (arg, "34a") == 0)
    return set_cpu_model ("kd11e");		/* KD11-E */

  else if (strcmp (arg, "35") == 0 ||		/* 11/35 or 11/40 */
	   strcmp (arg, "40") == 0)
    return set_cpu_model ("kd11da");		/* KD11-A */

  else if (strcmp (arg, "44") == 0)		/* 11/44 */
    return set_cpu_model ("kd11dz");		/* KD11-Z */

  else if (strcmp (arg, "45") == 0 ||		/* 11/45/50/55/70 */
	   strcmp (arg, "50") == 0 ||
	   strcmp (arg, "55") == 0 ||
	   strcmp (arg, "70") == 0)
    return set_cpu_model ("kb11");		/* KB11 */

  else if (strcmp (arg, "60") == 0)		/* 11/60 */
    return set_cpu_model ("kd11k");		/* KD11-K */ /* FPP? */

  else if (strcmp (arg, "53") == 0 ||		/* 11/53/73/83/84/93/94 */
	   strcmp (arg, "73") == 0 ||
	   strcmp (arg, "83") == 0 ||
	   strcmp (arg, "84") == 0 ||
	   strcmp (arg, "93") == 0 ||
	   strcmp (arg, "94") == 0)
    return set_cpu_model ("j11") &&		/* J11 */
           set_option ("fpp");			/* All J11 machines come */
						/* with FPP installed.  */
  else
    return 0;
}

const char *md_shortopts = "m:";

struct option md_longopts[] =
{
#define OPTION_CPU 257
  { "cpu", required_argument, NULL, OPTION_CPU },
#define OPTION_MACHINE 258
  { "machine", required_argument, NULL, OPTION_MACHINE },
#define OPTION_PIC 259
  { "pic", no_argument, NULL, OPTION_PIC },
  { NULL, no_argument, NULL, 0 }
};

size_t md_longopts_size = sizeof (md_longopts);

/*
 * md_parse_option
 *	Invocation line includes a switch not recognized by the base assembler.
 *	See if it's a processor-specific option.
 */

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  init_defaults ();

  switch (c)
    {
    case 'm':
      if (set_option (arg))
	return 1;
      if (set_cpu_model (arg))
	return 1;
      if (set_machine_model (arg))
	return 1;
      break;

    case OPTION_CPU:
      if (set_cpu_model (arg))
	return 1;
      break;

    case OPTION_MACHINE:
      if (set_machine_model (arg))
	return 1;
      break;

    case OPTION_PIC:
      if (set_option ("pic"))
	return 1;
      break;

    default:
      break;
    }

  as_bad ("unrecognized option `-%c%s'", c, arg ? arg : "");

  return 0;
}

/*
One possible way of parsing options.

enum
{
  OPTION_CSM,
  OPTION_CIS,
  ...
};

struct
{
  const char *pattern;
  int opt;
  const char *description;
} options;

static struct options extension_opts[] =
{
  { "Ncsm", OPTION_CSM,
			"allow (disallow) CSM instruction" },
  { "Ncis", OPTION_CIS,
			"allow (disallow) commersial instruction set" },
  { "Neis", OPTION_EIS,
			"allow (disallow) extended instruction set" },
  ...
  { "all-extensions", OPTION_ALL_EXTENSIONS,
			"allow all instruction set extensions\n\
			(this is the default)" },
  { "no-extensions", OPTION_NO_EXTENSIONS,
			"disallow all instruction set extensions" },
  { "pic", OPTION_PIC,
			"position-independent code" },
};

static struct options cpu_opts[] =
{
  { "Ka_11_*", OPTION_KA11, "KA11 CPU. ..." },
  { "Kb_11_*", OPTION_KB11, "KB11 CPU. ..." },
  { "Kd_11_a*", OPTION_KD11A, "KD11-A CPU. ..." },
  { "Kd_11_b*", OPTION_KD11B, "KD11-B CPU. ..." },
  { "Kd_11_d*", OPTION_KD11D, "KD11-D CPU. ..." },
  { "Kd_11_e*", OPTION_KD11E, "KD11-E CPU. ..." },
  { "Kd_11_f*", OPTION_KD11F, "KD11-F CPU. ..." },
  { "Kd_11_h*", OPTION_KD11H, "KD11-H CPU. ..." },
  { "Kd_11_q*", OPTION_KD11Q, "KD11-Q CPU. ..." },
  { "Kd_11_z*", OPTION_KD11Z, "KD11-Z CPU. ..." },
  { "Df_11_*", OPTION_F11, "F11 CPU. ..." },
  { "Dj_11_*", OPTION_J11, "J11 CPU. ..." },
  { "Dt_11_*", OPTION_T11, "T11 CPU. ..." },
};

static struct options model_opts[] =
{
  { "P03", OPTION_PDP11_03, "same as ..." },
  { "P04", OPTION_PDP11_04, "same as ..." },
  { "P05", OPTION_PDP11_05, "same as ..." },
  { "P10", OPTION_PDP11_10, "same as ..." },
  { "P15", OPTION_PDP11_15, "same as ..." },
  { "P20", OPTION_PDP11_20, "same as ..." },
  { "P21", OPTION_PDP11_21, "same as ..." },
  { "P24", OPTION_PDP11_24, "same as ..." },
  { "P34", OPTION_PDP11_34, "same as ..." },
  { "P34a", OPTION_PDP11_34A, "same as ..." },
  { "P40", OPTION_PDP11_40, "same as ..." },
  { "P44", OPTION_PDP11_44, "same as ..." },
  { "P45", OPTION_PDP11_45, "same as ..." },
  { "P50", OPTION_PDP11_50, "same as ..." },
  { "P53", OPTION_PDP11_53, "same as ..." },
  { "P55", OPTION_PDP11_55, "same as ..." },
  { "P60", OPTION_PDP11_60, "same as ..." },
  { "P70", OPTION_PDP11_70, "same as ..." },
  { "P73", OPTION_PDP11_73, "same as ..." },
  { "P83", OPTION_PDP11_83, "same as ..." },
  { "P84", OPTION_PDP11_84, "same as ..." },
  { "P93", OPTION_PDP11_93, "same as ..." },
  { "P94", OPTION_PDP11_94, "same as ..." },
};

struct
{
  const char *title;
  struct options *opts;
  int num;
} all_opts[] =
{
  { "PDP-11 instruction set extentions",
    extension_opts,
    sizeof extension_opts / sizeof extension_opts[0] },
  { "PDP-11 CPU model options",
    cpu_opts,
    sizeof cpu_opts / sizeof cpu_opts[0] },
  { "PDP-11 machine model options",
    model_opts,
    sizeof model_opts / sizeof model_opts[0] },
};

int
parse_match (char *arg, char *pattern)
{
  int yes = 1;

  while (*pattern)
    {
      switch (*pattern++)
	{
	case 'N':
	  if (strncmp (arg, "no-") == 0)
	    {
	      yes = 0;
	      arg += 3;
	    }
	  break;

	case 'K':
	  if (arg[0] == 'k')
	    arg++;
	  break;

	case 'D':
	  if (strncmp (arg, "kd", 2) == 0)
	    arg +=2;
	  break;

	case 'P':
	  if (strncmp (arg, "pdp-11/", 7) == 0)
	    arg += 7;
	  else if (strncmp (arg, "pdp11/", 6) == 0)
	    arg += 6;
	  else if (strncmp (arg, "11/", 3) == 0)
	    arg += 3;
	  break;

	case '_':
	  if (arg[0] == "-")
	    {
	      if (*++arg == 0)
		return 0;
	    }
	  break;

	case '*':
	  return 1;

	default:
	  if (*arg++ != pattern[-1])
	    return 0;
	}
    }

  return arg[0] == 0;
}

int
fprint_opt (stream, pattern)
     FILE *stream;
     const char *pattern;
{
  int n;

  while (*pattern)
    {
      switch (*pattern++)
	{
	case 'N':
	  n += fprintf (stream, "(no-)");
	  break;

	case 'K':
	  n += fprintf (stream, "k");
	  break;

	case 'P':
	  n += fprintf (stream "11/");
	  break;

	case 'D':
	case '_':
	case '*':
	  break;

	default:
	  fputc (pattern[-1], stream);
	  n++;
	}
    }

  return n;
}

int
parse_option (char *arg)
{
  int i, j;

  for (i = 0; i < sizeof all_opts / sizeof all_opts[0]; i++)
    {
      for (j = 0; j < all_opts[i].num; j++)
	{
	  if (parse_match (arg, all_opts[i].opts[j].pattern))
	    {
	      set_option (all_opts[i].opts[j].opt);
	      return 1;
	    }
	}
    }

  return 0;
}

static void
fprint_space (stream, n)
     FILE *stream;
     int n;
{
  while (n--)
    fputc (' ', stream);
}

void
md_show_usage (stream)
     FILE *stream;
{
  int i, j, n;

  for (i = 0; i < sizeof all_opts / sizeof all_opts[0]; i++)
    {
      fprintf (stream "\n%s:\n\n", all_opts[i].title);

      for (j = 0; j < all_opts[i].num; j++)
	{
	  fprintf (stream, "-m");
	  n = fprintf_opt (stream, all_opts[i].opts[j].pattern);
	  fprint_space (stream, 22 - n);
	  fprintf (stream, "%s\n", all_opts[i].opts[j].description);
	}
    }
}
*/

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, "\
\n\
PDP-11 instruction set extentions:\n\
\n\
-m(no-)cis		allow (disallow) commersial instruction set\n\
-m(no-)csm		allow (disallow) CSM instruction\n\
-m(no-)eis		allow (disallow) full extended instruction set\n\
-m(no-)fis		allow (disallow) KEV11 floating-point instructions\n\
-m(no-)fpp		allow (disallow) FP-11 floating-point instructions\n\
-m(no-)fpu		allow (disallow) FP-11 floating-point instructions\n\
-m(no-)limited-eis	allow (disallow) limited extended instruction set\n\
-m(no-)mfpt		allow (disallow) processor type instruction\n\
-m(no-)multiproc	allow (disallow) multiprocessor instructions\n\
-m(no-)mxps		allow (disallow) processor status instructions\n\
-m(no-)spl		allow (disallow) SPL instruction\n\
-m(no-)ucode		allow (disallow) microcode instructions\n\
-mall-extensions	allow all instruction set extensions\n\
			(this is the default)\n\
-mno-extentions		disallow all instruction set extensions\n\
-pic			generate position-indepenent code\n\
\n\
PDP-11 CPU model options:\n\
\n\
-mka11*			KA11 CPU.  base line instruction set only\n\
-mkb11*			KB11 CPU.  enable full EIS and SPL\n\
-mkd11a*		KD11-A CPU.  enable limited EIS\n\
-mkd11b*		KD11-B CPU.  base line instruction set only\n\
-mkd11d*		KD11-D CPU.  base line instruction set only\n\
-mkd11e*		KD11-E CPU.  enable full EIS, MTPS, and MFPS\n\
-mkd11f*		KD11-F CPU.  enable limited EIS, MTPS, and MFPS\n\
-mkd11h*		KD11-H CPU.  enable limited EIS, MTPS, and MFPS\n\
-mkd11q*		KD11-Q CPU.  enable limited EIS, MTPS, and MFPS\n\
-mkd11k*		KD11-K CPU.  enable full EIS, MTPS, MFPS, LDUB, MED,\n\
			XFC, and MFPT\n\
-mkd11z*		KD11-Z CPU.  enable full EIS, MTPS, MFPS, MFPT, SPL,\n\
			and CSM\n\
-mf11*			F11 CPU.  enable full EIS, MFPS, MTPS, and MFPT\n\
-mj11*			J11 CPU.  enable full EIS, MTPS, MFPS, MFPT, SPL,\n\
			CSM, TSTSET, and WRTLCK\n\
-mt11*			T11 CPU.  enable limited EIS, MTPS, and MFPS\n\
\n\
PDP-11 machine model options:\n\
\n\
-m11/03			same as -mkd11f\n\
-m11/04			same as -mkd11d\n\
-m11/05			same as -mkd11b\n\
-m11/10			same as -mkd11b\n\
-m11/15			same as -mka11\n\
-m11/20			same as -mka11\n\
-m11/21			same as -mt11\n\
-m11/23			same as -mf11\n\
-m11/24			same as -mf11\n\
-m11/34			same as -mkd11e\n\
-m11/34a		same as -mkd11e -mfpp\n\
-m11/35			same as -mkd11a\n\
-m11/40			same as -mkd11a\n\
-m11/44			same as -mkd11z\n\
-m11/45			same as -mkb11\n\
-m11/50			same as -mkb11\n\
-m11/53			same as -mj11\n\
-m11/55			same as -mkb11\n\
-m11/60			same as -mkd11k\n\
-m11/70			same as -mkb11\n\
-m11/73			same as -mj11\n\
-m11/83			same as -mj11\n\
-m11/84			same as -mj11\n\
-m11/93			same as -mj11\n\
-m11/94			same as -mj11\n\
");
}

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
  return (size + 1) & ~1;
}

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  return fixP->fx_frag->fr_address + fixP->fx_where + fixP->fx_size;
}

/* Translate internal representation of relocation info to BFD target
   format.  */
arelent *
tc_gen_reloc (section, fixp)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  /* This is taken account for in md_apply_fix3().  */
  reloc->addend = -symbol_get_bfdsym (fixp->fx_addsy)->section->vma;

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_16:
      if (fixp->fx_pcrel)
	code = BFD_RELOC_16_PCREL;
      else
	code = BFD_RELOC_16;
      break;

    case BFD_RELOC_16_PCREL:
      code = BFD_RELOC_16_PCREL;
      break;

    default:
      BAD_CASE (fixp->fx_r_type);
      return NULL;
    }

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);

  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    "Can not represent %s relocation in this object file format",
		    bfd_get_reloc_code_name (code));
      return NULL;
    }

  return reloc;
}

void
pseudo_bss (c)
     int c ATTRIBUTE_UNUSED;
{
  int temp;

  temp = get_absolute_expression ();
  subseg_set (bss_section, temp);
  demand_empty_rest_of_line ();
}

void
pseudo_even (c)
     int c ATTRIBUTE_UNUSED;
{
  int alignment = 1; /* 2^1 */
  frag_align (alignment, 0, 1);
  record_alignment (now_seg, alignment);
}

/* end of tc-pdp11.c */
