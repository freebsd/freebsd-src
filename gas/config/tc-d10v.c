/* tc-d10v.c -- Assembler code for the Mitsubishi D10V
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005
   Free Software Foundation, Inc.

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
   the Free Software Foundation, 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include <stdio.h>
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/d10v.h"
#include "elf/ppc.h"

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = "";
const char *md_shortopts          = "O";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

int Optimizing = 0;

#define AT_WORD_P(X) ((X)->X_op == O_right_shift \
		      && (X)->X_op_symbol != NULL \
		      && symbol_constant_p ((X)->X_op_symbol) \
		      && S_GET_VALUE ((X)->X_op_symbol) == AT_WORD_RIGHT_SHIFT)
#define AT_WORD_RIGHT_SHIFT 2

/* Fixups.  */
#define MAX_INSN_FIXUPS  5

struct d10v_fixup
{
  expressionS exp;
  int operand;
  int pcrel;
  int size;
  bfd_reloc_code_real_type reloc;
};

typedef struct _fixups
{
  int fc;
  struct d10v_fixup fix[MAX_INSN_FIXUPS];
  struct _fixups *next;
} Fixups;

static Fixups FixUps[2];
static Fixups *fixups;

static int do_not_ignore_hash = 0;

typedef int packing_type;
#define PACK_UNSPEC 	(0)	/* Packing order not specified.  */
#define PACK_PARALLEL	(1)	/* "||"  */
#define PACK_LEFT_RIGHT (2)	/* "->"  */
#define PACK_RIGHT_LEFT (3)	/* "<-"  */
static packing_type etype = PACK_UNSPEC; /* Used by d10v_cleanup.  */

/* TRUE if instruction swapping warnings should be inhibited.
   --nowarnswap.  */
static bfd_boolean flag_warn_suppress_instructionswap;

/* TRUE if instruction packing should be performed when --gstabs is specified.
   --gstabs-packing, --no-gstabs-packing.  */
static bfd_boolean flag_allow_gstabs_packing = 1;

/* Local functions.  */

enum options
{
  OPTION_NOWARNSWAP = OPTION_MD_BASE,
  OPTION_GSTABSPACKING,
  OPTION_NOGSTABSPACKING
};

struct option md_longopts[] =
{
  {"nowarnswap", no_argument, NULL, OPTION_NOWARNSWAP},
  {"gstabspacking",  no_argument, NULL, OPTION_GSTABSPACKING},
  {"gstabs-packing", no_argument, NULL, OPTION_GSTABSPACKING},
  {"nogstabspacking",   no_argument, NULL, OPTION_NOGSTABSPACKING},
  {"no-gstabs-packing", no_argument, NULL, OPTION_NOGSTABSPACKING},
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

/* Opcode hash table.  */
static struct hash_control *d10v_hash;

/* Do a binary search of the d10v_predefined_registers array to see if
   NAME is a valid regiter name.  Return the register number from the
   array on success, or -1 on failure.  */

static int
reg_name_search (char *name)
{
  int middle, low, high;
  int cmp;

  low = 0;
  high = d10v_reg_name_cnt () - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, d10v_predefined_registers[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else
	return d10v_predefined_registers[middle].value;
    }
  while (low <= high);
  return -1;
}

/* Check the string at input_line_pointer
   to see if it is a valid register name.  */

static int
register_name (expressionS *expressionP)
{
  int reg_number;
  char c, *p = input_line_pointer;

  while (*p
	 && *p != '\n' && *p != '\r' && *p != ',' && *p != ' ' && *p != ')')
    p++;

  c = *p;
  if (c)
    *p++ = 0;

  /* Look to see if it's in the register table.  */
  reg_number = reg_name_search (input_line_pointer);
  if (reg_number >= 0)
    {
      expressionP->X_op = O_register;
      /* Temporarily store a pointer to the string here.  */
      expressionP->X_op_symbol = (symbolS *) input_line_pointer;
      expressionP->X_add_number = reg_number;
      input_line_pointer = p;
      return 1;
    }
  if (c)
    *(p - 1) = c;
  return 0;
}

static int
check_range (unsigned long num, int bits, int flags)
{
  long min, max;
  int retval = 0;

  /* Don't bother checking 16-bit values.  */
  if (bits == 16)
    return 0;

  if (flags & OPERAND_SHIFT)
    {
      /* All special shift operands are unsigned and <= 16.
	 We allow 0 for now.  */
      if (num > 16)
	return 1;
      else
	return 0;
    }

  if (flags & OPERAND_SIGNED)
    {
      /* Signed 3-bit integers are restricted to the (-2, 3) range.  */
      if (flags & RESTRICTED_NUM3)
	{
	  if ((long) num < -2 || (long) num > 3)
	    retval = 1;
	}
      else
	{
	  max = (1 << (bits - 1)) - 1;
	  min = - (1 << (bits - 1));
	  if (((long) num > max) || ((long) num < min))
	    retval = 1;
	}
    }
  else
    {
      max = (1 << bits) - 1;
      min = 0;
      if (((long) num > max) || ((long) num < min))
	retval = 1;
    }
  return retval;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("D10V options:\n\
-O                      Optimize.  Will do some operations in parallel.\n\
--gstabs-packing        Pack adjacent short instructions together even\n\
                        when --gstabs is specified.  On by default.\n\
--no-gstabs-packing     If --gstabs is specified, do not pack adjacent\n\
                        instructions together.\n"));
}

int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case 'O':
      /* Optimize. Will attempt to parallelize operations.  */
      Optimizing = 1;
      break;
    case OPTION_NOWARNSWAP:
      flag_warn_suppress_instructionswap = 1;
      break;
    case OPTION_GSTABSPACKING:
      flag_allow_gstabs_packing = 1;
      break;
    case OPTION_NOGSTABSPACKING:
      flag_allow_gstabs_packing = 0;
      break;
    default:
      return 0;
    }
  return 1;
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;
    case 'd':
      prec = 4;
      break;
    default:
      *sizeP = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  for (i = 0; i < prec; i++)
    {
      md_number_to_chars (litP, (valueT) words[i], 2);
      litP += 2;
    }
  return NULL;
}

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED,
		 asection *sec ATTRIBUTE_UNUSED,
		 fragS *fragP ATTRIBUTE_UNUSED)
{
  abort ();
}

valueT
md_section_align (asection *seg, valueT addr)
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

void
md_begin (void)
{
  char *prev_name = "";
  struct d10v_opcode *opcode;
  d10v_hash = hash_new ();

  /* Insert unique names into hash table.  The D10v instruction set
     has many identical opcode names that have different opcodes based
     on the operands.  This hash table then provides a quick index to
     the first opcode with a particular name in the opcode table.  */

  for (opcode = (struct d10v_opcode *) d10v_opcodes; opcode->name; opcode++)
    {
      if (strcmp (prev_name, opcode->name))
	{
	  prev_name = (char *) opcode->name;
	  hash_insert (d10v_hash, opcode->name, (char *) opcode);
	}
    }

  fixups = &FixUps[0];
  FixUps[0].next = &FixUps[1];
  FixUps[1].next = &FixUps[0];
}

/* Remove the postincrement or postdecrement operator ( '+' or '-' )
   from an expression.  */

static int
postfix (char *p)
{
  while (*p != '-' && *p != '+')
    {
      if (*p == 0 || *p == '\n' || *p == '\r')
	break;
      p++;
    }

  if (*p == '-')
    {
      *p = ' ';
      return -1;
    }
  if (*p == '+')
    {
      *p = ' ';
      return 1;
    }

  return 0;
}

static bfd_reloc_code_real_type
get_reloc (struct d10v_operand *op)
{
  int bits = op->bits;

  if (bits <= 4)
    return 0;

  if (op->flags & OPERAND_ADDR)
    {
      if (bits == 8)
	return BFD_RELOC_D10V_10_PCREL_R;
      else
	return BFD_RELOC_D10V_18_PCREL;
    }

  return BFD_RELOC_16;
}

/* Parse a string of operands.  Return an array of expressions.  */

static int
get_operands (expressionS exp[])
{
  char *p = input_line_pointer;
  int numops = 0;
  int post = 0;
  int uses_at = 0;

  while (*p)
    {
      while (*p == ' ' || *p == '\t' || *p == ',')
	p++;
      if (*p == 0 || *p == '\n' || *p == '\r')
	break;

      if (*p == '@')
	{
	  uses_at = 1;

	  p++;
	  exp[numops].X_op = O_absent;
	  if (*p == '(')
	    {
	      p++;
	      exp[numops].X_add_number = OPERAND_ATPAR;
	    }
	  else if (*p == '-')
	    {
	      p++;
	      exp[numops].X_add_number = OPERAND_ATMINUS;
	    }
	  else
	    {
	      exp[numops].X_add_number = OPERAND_ATSIGN;
	      if (*p == '+')
		{
		  numops++;
		  exp[numops].X_op = O_absent;
		  exp[numops].X_add_number = OPERAND_PLUS;
		  p++;
		}
	      post = postfix (p);
	    }
	  numops++;
	  continue;
	}

      if (*p == ')')
	{
	  /* Just skip the trailing paren.  */
	  p++;
	  continue;
	}

      input_line_pointer = p;

      /* Check to see if it might be a register name.  */
      if (!register_name (&exp[numops]))
	{
	  /* Parse as an expression.  */
	  if (uses_at)
	    {
	      /* Any expression that involves the indirect addressing
		 cannot also involve immediate addressing.  Therefore
		 the use of the hash character is illegal.  */
	      int save = do_not_ignore_hash;
	      do_not_ignore_hash = 1;

	      expression (&exp[numops]);

	      do_not_ignore_hash = save;
	    }
	  else
	    expression (&exp[numops]);
	}

      if (strncasecmp (input_line_pointer, "@word", 5) == 0)
	{
	  input_line_pointer += 5;
	  if (exp[numops].X_op == O_register)
	    {
	      /* If it looked like a register name but was followed by
                 "@word" then it was really a symbol, so change it to
                 one.  */
	      exp[numops].X_op = O_symbol;
	      exp[numops].X_add_symbol =
		symbol_find_or_make ((char *) exp[numops].X_op_symbol);
	    }

	  /* Check for identifier@word+constant.  */
	  if (*input_line_pointer == '-' || *input_line_pointer == '+')
	    {
	      expressionS new_exp;
	      expression (&new_exp);
	      exp[numops].X_add_number = new_exp.X_add_number;
	    }

	  /* Convert expr into a right shift by AT_WORD_RIGHT_SHIFT.  */
	  {
	    expressionS new_exp;
	    memset (&new_exp, 0, sizeof new_exp);
	    new_exp.X_add_number = AT_WORD_RIGHT_SHIFT;
	    new_exp.X_op = O_constant;
	    new_exp.X_unsigned = 1;
	    exp[numops].X_op_symbol = make_expr_symbol (&new_exp);
	    exp[numops].X_op = O_right_shift;
	  }

	  know (AT_WORD_P (&exp[numops]));
	}

      if (exp[numops].X_op == O_illegal)
	as_bad (_("illegal operand"));
      else if (exp[numops].X_op == O_absent)
	as_bad (_("missing operand"));

      numops++;
      p = input_line_pointer;
    }

  switch (post)
    {
    case -1:	/* Postdecrement mode.  */
      exp[numops].X_op = O_absent;
      exp[numops++].X_add_number = OPERAND_MINUS;
      break;
    case 1:	/* Postincrement mode.  */
      exp[numops].X_op = O_absent;
      exp[numops++].X_add_number = OPERAND_PLUS;
      break;
    }

  exp[numops].X_op = 0;
  return numops;
}

static unsigned long
d10v_insert_operand (unsigned long insn,
		     int op_type,
		     offsetT value,
		     int left,
		     fixS *fix)
{
  int shift, bits;

  shift = d10v_operands[op_type].shift;
  if (left)
    shift += 15;

  bits = d10v_operands[op_type].bits;

  /* Truncate to the proper number of bits.  */
  if (check_range (value, bits, d10v_operands[op_type].flags))
    as_bad_where (fix->fx_file, fix->fx_line,
		  _("operand out of range: %ld"), (long) value);

  value &= 0x7FFFFFFF >> (31 - bits);
  insn |= (value << shift);

  return insn;
}

/* Take a pointer to the opcode entry in the opcode table and the
   array of operand expressions.  Return the instruction.  */

static unsigned long
build_insn (struct d10v_opcode *opcode,
	    expressionS *opers,
	    unsigned long insn)
{
  int i, bits, shift, flags, format;
  unsigned long number;

  /* The insn argument is only used for the DIVS kludge.  */
  if (insn)
    format = LONG_R;
  else
    {
      insn = opcode->opcode;
      format = opcode->format;
    }

  for (i = 0; opcode->operands[i]; i++)
    {
      flags = d10v_operands[opcode->operands[i]].flags;
      bits = d10v_operands[opcode->operands[i]].bits;
      shift = d10v_operands[opcode->operands[i]].shift;
      number = opers[i].X_add_number;

      if (flags & OPERAND_REG)
	{
	  number &= REGISTER_MASK;
	  if (format == LONG_L)
	    shift += 15;
	}

      if (opers[i].X_op != O_register && opers[i].X_op != O_constant)
	{
	  /* Now create a fixup.  */

	  if (fixups->fc >= MAX_INSN_FIXUPS)
	    as_fatal (_("too many fixups"));

	  if (AT_WORD_P (&opers[i]))
	    {
	      /* Recognize XXX>>1+N aka XXX@word+N as special (AT_WORD).  */
	      fixups->fix[fixups->fc].reloc = BFD_RELOC_D10V_18;
	      opers[i].X_op = O_symbol;
	      opers[i].X_op_symbol = NULL; /* Should free it.  */
	      /* number is left shifted by AT_WORD_RIGHT_SHIFT so
                 that, it is aligned with the symbol's value.  Later,
                 BFD_RELOC_D10V_18 will right shift (symbol_value +
                 X_add_number).  */
	      number <<= AT_WORD_RIGHT_SHIFT;
	      opers[i].X_add_number = number;
	    }
	  else
	    {
	      fixups->fix[fixups->fc].reloc =
		get_reloc ((struct d10v_operand *) &d10v_operands[opcode->operands[i]]);

	      /* Check that an immediate was passed to ops that expect one.  */
	      if ((flags & OPERAND_NUM)
		  && (fixups->fix[fixups->fc].reloc == 0))
		as_bad (_("operand is not an immediate"));
	    }

	  if (fixups->fix[fixups->fc].reloc == BFD_RELOC_16 ||
	      fixups->fix[fixups->fc].reloc == BFD_RELOC_D10V_18)
	    fixups->fix[fixups->fc].size = 2;
	  else
	    fixups->fix[fixups->fc].size = 4;

	  fixups->fix[fixups->fc].exp = opers[i];
	  fixups->fix[fixups->fc].operand = opcode->operands[i];
	  fixups->fix[fixups->fc].pcrel =
	    (flags & OPERAND_ADDR) ? TRUE : FALSE;
	  (fixups->fc)++;
	}

      /* Truncate to the proper number of bits.  */
      if ((opers[i].X_op == O_constant) && check_range (number, bits, flags))
	as_bad (_("operand out of range: %lu"), number);
      number &= 0x7FFFFFFF >> (31 - bits);
      insn = insn | (number << shift);
    }

  /* kludge: for DIVS, we need to put the operands in twice on the second
     pass, format is changed to LONG_R to force the second set of operands
     to not be shifted over 15.  */
  if ((opcode->opcode == OPCODE_DIVS) && (format == LONG_L))
    insn = build_insn (opcode, opers, insn);

  return insn;
}

/* Write out a long form instruction.  */

static void
write_long (unsigned long insn, Fixups *fx)
{
  int i, where;
  char *f = frag_more (4);

  insn |= FM11;
  number_to_chars_bigendian (f, insn, 4);

  for (i = 0; i < fx->fc; i++)
    {
      if (fx->fix[i].reloc)
	{
	  where = f - frag_now->fr_literal;
	  if (fx->fix[i].size == 2)
	    where += 2;

	  if (fx->fix[i].reloc == BFD_RELOC_D10V_18)
	    fx->fix[i].operand |= 4096;

	  fix_new_exp (frag_now,
		       where,
		       fx->fix[i].size,
		       &(fx->fix[i].exp),
		       fx->fix[i].pcrel,
		       fx->fix[i].operand|2048);
	}
    }
  fx->fc = 0;
}

/* Write out a short form instruction by itself.  */

static void
write_1_short (struct d10v_opcode *opcode,
	       unsigned long insn,
	       Fixups *fx)
{
  char *f = frag_more (4);
  int i, where;

  if (opcode->exec_type & PARONLY)
    as_fatal (_("Instruction must be executed in parallel with another instruction."));

  /* The other container needs to be NOP.
     According to 4.3.1: for FM=00, sub-instructions performed only by IU
     cannot be encoded in L-container.  */
  if (opcode->unit == IU)
    insn |= FM00 | (NOP << 15);		/* Right container.  */
  else
    insn = FM00 | (insn << 15) | NOP;	/* Left container.  */

  number_to_chars_bigendian (f, insn, 4);
  for (i = 0; i < fx->fc; i++)
    {
      if (fx->fix[i].reloc)
	{
	  where = f - frag_now->fr_literal;
	  if (fx->fix[i].size == 2)
	    where += 2;

	  if (fx->fix[i].reloc == BFD_RELOC_D10V_18)
	    fx->fix[i].operand |= 4096;

	  /* If it's an R reloc, we may have to switch it to L.  */
	  if ((fx->fix[i].reloc == BFD_RELOC_D10V_10_PCREL_R)
	      && (opcode->unit != IU))
	    fx->fix[i].operand |= 1024;

	  fix_new_exp (frag_now,
		       where,
		       fx->fix[i].size,
		       &(fx->fix[i].exp),
		       fx->fix[i].pcrel,
		       fx->fix[i].operand|2048);
	}
    }
  fx->fc = 0;
}

/* Determine if there are any resource conflicts among two manually
   parallelized instructions.  Some of this was lifted from parallel_ok.  */

static void
check_resource_conflict (struct d10v_opcode *op1,
			 unsigned long insn1,
			 struct d10v_opcode *op2,
			 unsigned long insn2)
{
  int i, j, flags, mask, shift, regno;
  unsigned long ins, mod[2];
  struct d10v_opcode *op;

  if ((op1->exec_type & SEQ)
      || ! ((op1->exec_type & PAR) || (op1->exec_type & PARONLY)))
    {
      as_warn (_("packing conflict: %s must dispatch sequentially"),
	      op1->name);
      return;
    }

  if ((op2->exec_type & SEQ)
      || ! ((op2->exec_type & PAR) || (op2->exec_type & PARONLY)))
    {
      as_warn (_("packing conflict: %s must dispatch sequentially"),
	      op2->name);
      return;
    }

   /* See if both instructions write to the same resource.

      The idea here is to create two sets of bitmasks (mod and used) which
      indicate which registers are modified or used by each instruction.
      The operation can only be done in parallel if neither instruction
      modifies the same register. Accesses to control registers and memory
      are treated as accesses to a single register. So if both instructions
      write memory or if the first instruction writes memory and the second
      reads, then they cannot be done in parallel. We treat reads to the PSW
      (which includes C, F0, and F1) in isolation. So simultaneously writing
      C and F0 in two different sub-instructions is permitted.  */

  /* The bitmasks (mod and used) look like this (bit 31 = MSB).
     r0-r15	  0-15
     a0-a1	  16-17
     cr (not psw) 18
     psw(other)   19
     mem	  20
     psw(C flag)  21
     psw(F0 flag) 22  */

  for (j = 0; j < 2; j++)
    {
      if (j == 0)
	{
	  op = op1;
	  ins = insn1;
	}
      else
	{
	  op = op2;
	  ins = insn2;
	}
      mod[j] = 0;
      if (op->exec_type & BRANCH_LINK)
	mod[j] |= 1 << 13;

      for (i = 0; op->operands[i]; i++)
	{
	  flags = d10v_operands[op->operands[i]].flags;
	  shift = d10v_operands[op->operands[i]].shift;
	  mask = 0x7FFFFFFF >> (31 - d10v_operands[op->operands[i]].bits);
	  if (flags & OPERAND_REG)
	    {
	      regno = (ins >> shift) & mask;
	      if (flags & (OPERAND_ACC0 | OPERAND_ACC1))
		regno += 16;
	      else if (flags & OPERAND_CONTROL)	/* mvtc or mvfc */
		{
		  if (regno == 0)
		    regno = 19;
		  else
		    regno = 18;
		}
	      else if (flags & OPERAND_FFLAG)
		regno = 22;
	      else if (flags & OPERAND_CFLAG)
		regno = 21;

	      if (flags & OPERAND_DEST
		  /* Auto inc/dec also modifies the register.  */
		  || (op->operands[i + 1] != 0
		      && (d10v_operands[op->operands[i + 1]].flags
			  & (OPERAND_PLUS | OPERAND_MINUS)) != 0))
		{
		  mod[j] |= 1 << regno;
		  if (flags & OPERAND_EVEN)
		    mod[j] |= 1 << (regno + 1);
		}
	    }
	  else if (flags & OPERAND_ATMINUS)
	    {
	      /* SP implicitly used/modified.  */
	      mod[j] |= 1 << 15;
	    }
	}

      if (op->exec_type & WMEM)
	mod[j] |= 1 << 20;
      else if (op->exec_type & WF0)
	mod[j] |= 1 << 22;
      else if (op->exec_type & WCAR)
	mod[j] |= 1 << 21;
    }

  if ((mod[0] & mod[1]) == 0)
    return;
  else
    {
      unsigned long x;
      x = mod[0] & mod[1];

      for (j = 0; j <= 15; j++)
	if (x & (1 << j))
	  as_warn (_("resource conflict (R%d)"), j);
      for (j = 16; j <= 17; j++)
	if (x & (1 << j))
	  as_warn (_("resource conflict (A%d)"), j - 16);
      if (x & (1 << 19))
	as_warn (_("resource conflict (PSW)"));
      if (x & (1 << 21))
	as_warn (_("resource conflict (C flag)"));
      if (x & (1 << 22))
	as_warn (_("resource conflict (F flag)"));
    }
}

/* Check 2 instructions and determine if they can be safely
   executed in parallel.  Return 1 if they can be.  */

static int
parallel_ok (struct d10v_opcode *op1,
	     unsigned long insn1,
	     struct d10v_opcode *op2,
	     unsigned long insn2,
	     packing_type exec_type)
{
  int i, j, flags, mask, shift, regno;
  unsigned long ins, mod[2], used[2];
  struct d10v_opcode *op;

  if ((op1->exec_type & SEQ) != 0 || (op2->exec_type & SEQ) != 0
      || (op1->exec_type & PAR) == 0 || (op2->exec_type & PAR) == 0
      || (op1->unit == BOTH) || (op2->unit == BOTH)
      || (op1->unit == IU && op2->unit == IU)
      || (op1->unit == MU && op2->unit == MU))
    return 0;

  /* If this is auto parallelization, and the first instruction is a
     branch or should not be packed, then don't parallelize.  */
  if (exec_type == PACK_UNSPEC
      && (op1->exec_type & (ALONE | BRANCH)))
    return 0;

  /* The idea here is to create two sets of bitmasks (mod and used)
     which indicate which registers are modified or used by each
     instruction.  The operation can only be done in parallel if
     instruction 1 and instruction 2 modify different registers, and
     the first instruction does not modify registers that the second
     is using (The second instruction can modify registers that the
     first is using as they are only written back after the first
     instruction has completed).  Accesses to control registers, PSW,
     and memory are treated as accesses to a single register.  So if
     both instructions write memory or if the first instruction writes
     memory and the second reads, then they cannot be done in
     parallel.  Likewise, if the first instruction mucks with the psw
     and the second reads the PSW (which includes C, F0, and F1), then
     they cannot operate safely in parallel.  */

  /* The bitmasks (mod and used) look like this (bit 31 = MSB).
     r0-r15	  0-15
     a0-a1	  16-17
     cr (not psw) 18
     psw	  19
     mem	  20  */

  for (j = 0; j < 2; j++)
    {
      if (j == 0)
	{
	  op = op1;
	  ins = insn1;
	}
      else
	{
	  op = op2;
	  ins = insn2;
	}
      mod[j] = used[j] = 0;
      if (op->exec_type & BRANCH_LINK)
	mod[j] |= 1 << 13;

      for (i = 0; op->operands[i]; i++)
	{
	  flags = d10v_operands[op->operands[i]].flags;
	  shift = d10v_operands[op->operands[i]].shift;
	  mask = 0x7FFFFFFF >> (31 - d10v_operands[op->operands[i]].bits);
	  if (flags & OPERAND_REG)
	    {
	      regno = (ins >> shift) & mask;
	      if (flags & (OPERAND_ACC0 | OPERAND_ACC1))
		regno += 16;
	      else if (flags & OPERAND_CONTROL)	/* mvtc or mvfc.  */
		{
		  if (regno == 0)
		    regno = 19;
		  else
		    regno = 18;
		}
	      else if (flags & (OPERAND_FFLAG | OPERAND_CFLAG))
		regno = 19;

	      if (flags & OPERAND_DEST)
		{
		  mod[j] |= 1 << regno;
		  if (flags & OPERAND_EVEN)
		    mod[j] |= 1 << (regno + 1);
		}
	      else
		{
		  used[j] |= 1 << regno;
		  if (flags & OPERAND_EVEN)
		    used[j] |= 1 << (regno + 1);

		  /* Auto inc/dec also modifies the register.  */
		  if (op->operands[i + 1] != 0
		      && (d10v_operands[op->operands[i + 1]].flags
			  & (OPERAND_PLUS | OPERAND_MINUS)) != 0)
		    mod[j] |= 1 << regno;
		}
	    }
	  else if (flags & OPERAND_ATMINUS)
	    {
	      /* SP implicitly used/modified.  */
	      mod[j] |= 1 << 15;
	      used[j] |= 1 << 15;
	    }
	}
      if (op->exec_type & RMEM)
	used[j] |= 1 << 20;
      else if (op->exec_type & WMEM)
	mod[j] |= 1 << 20;
      else if (op->exec_type & RF0)
	used[j] |= 1 << 19;
      else if (op->exec_type & WF0)
	mod[j] |= 1 << 19;
      else if (op->exec_type & WCAR)
	mod[j] |= 1 << 19;
    }
  if ((mod[0] & mod[1]) == 0 && (mod[0] & used[1]) == 0)
    return 1;
  return 0;
}

/* Expects two short instructions.
   If possible, writes out both as a single packed instruction.
   Otherwise, writes out the first one, packed with a NOP.
   Returns number of instructions not written out.  */

static int
write_2_short (struct d10v_opcode *opcode1,
	       unsigned long insn1,
	       struct d10v_opcode *opcode2,
	       unsigned long insn2,
	       packing_type exec_type,
	       Fixups *fx)
{
  unsigned long insn;
  char *f;
  int i, j, where;

  if ((exec_type != PACK_PARALLEL)
      && ((opcode1->exec_type & PARONLY) || (opcode2->exec_type & PARONLY)))
    as_fatal (_("Instruction must be executed in parallel"));

  if ((opcode1->format & LONG_OPCODE) || (opcode2->format & LONG_OPCODE))
    as_fatal (_("Long instructions may not be combined."));

  switch (exec_type)
    {
    case PACK_UNSPEC:	/* Order not specified.  */
      if (opcode1->exec_type & ALONE)
	{
	  /* Case of a short branch on a separate GAS line.  Pack with NOP.  */
	  write_1_short (opcode1, insn1, fx->next);
	  return 1;
	}
      if (Optimizing
	  && parallel_ok (opcode1, insn1, opcode2, insn2, exec_type))
	{
	  /* Parallel.  */
	  if (opcode1->unit == IU)
	    insn = FM00 | (insn2 << 15) | insn1;
	  else if (opcode2->unit == MU)
	    insn = FM00 | (insn2 << 15) | insn1;
	  else
	    insn = FM00 | (insn1 << 15) | insn2;
	}
      else if (opcode1->unit == IU)
	/* Reverse sequential with IU opcode1 on right and done first.  */
	insn = FM10 | (insn2 << 15) | insn1;
      else
	/* Sequential with non-IU opcode1 on left and done first.  */
	insn = FM01 | (insn1 << 15) | insn2;
      break;

    case PACK_PARALLEL:
      if (opcode1->exec_type & SEQ || opcode2->exec_type & SEQ)
	as_fatal
	  (_("One of these instructions may not be executed in parallel."));
      if (opcode1->unit == IU)
	{
	  if (opcode2->unit == IU)
	    as_fatal (_("Two IU instructions may not be executed in parallel"));
	  if (!flag_warn_suppress_instructionswap)
	    as_warn (_("Swapping instruction order"));
	  insn = FM00 | (insn2 << 15) | insn1;
	}
      else if (opcode2->unit == MU)
	{
	  if (opcode1->unit == MU)
	    as_fatal (_("Two MU instructions may not be executed in parallel"));
	  if (!flag_warn_suppress_instructionswap)
	    as_warn (_("Swapping instruction order"));
	  insn = FM00 | (insn2 << 15) | insn1;
	}
      else
	insn = FM00 | (insn1 << 15) | insn2;
      check_resource_conflict (opcode1, insn1, opcode2, insn2);
      break;

    case PACK_LEFT_RIGHT:
      if (opcode1->unit != IU)
	insn = FM01 | (insn1 << 15) | insn2;
      else if (opcode2->unit == MU || opcode2->unit == EITHER)
	{
	  if (!flag_warn_suppress_instructionswap)
	    as_warn (_("Swapping instruction order"));
	  insn = FM10 | (insn2 << 15) | insn1;
	}
      else
	as_fatal (_("IU instruction may not be in the left container"));
      if (opcode1->exec_type & ALONE)
	as_warn (_("Instruction in R container is squashed by flow control instruction in L container."));
      break;

    case PACK_RIGHT_LEFT:
      if (opcode2->unit != MU)
	insn = FM10 | (insn1 << 15) | insn2;
      else if (opcode1->unit == IU || opcode1->unit == EITHER)
	{
	  if (!flag_warn_suppress_instructionswap)
	    as_warn (_("Swapping instruction order"));
	  insn = FM01 | (insn2 << 15) | insn1;
	}
      else
	as_fatal (_("MU instruction may not be in the right container"));
      if (opcode2->exec_type & ALONE)
	as_warn (_("Instruction in R container is squashed by flow control instruction in L container."));
      break;

    default:
      as_fatal (_("unknown execution type passed to write_2_short()"));
    }

  f = frag_more (4);
  number_to_chars_bigendian (f, insn, 4);

  /* Process fixup chains.  fx refers to insn2 when j == 0, and to
     insn1 when j == 1.  Yes, it's reversed.  */

  for (j = 0; j < 2; j++)
    {
      for (i = 0; i < fx->fc; i++)
	{
	  if (fx->fix[i].reloc)
	    {
	      where = f - frag_now->fr_literal;
	      if (fx->fix[i].size == 2)
		where += 2;

	      if (fx->fix[i].reloc == BFD_RELOC_D10V_10_PCREL_R
		  /* A BFD_RELOC_D10V_10_PCREL_R relocation applied to
		     the instruction in the L container has to be
		     adjusted to BDF_RELOC_D10V_10_PCREL_L.  When
		     j==0, we're processing insn2's operands, so we
		     want to mark the operand if insn2 is *not* in the
		     R container.  When j==1, we're processing insn1's
		     operands, so we want to mark the operand if insn2
		     *is* in the R container.  Note that, if two
		     instructions are identical, we're never going to
		     swap them, so the test is safe.  */
		  && j == ((insn & 0x7fff) == insn2))
		fx->fix[i].operand |= 1024;

	      if (fx->fix[i].reloc == BFD_RELOC_D10V_18)
		fx->fix[i].operand |= 4096;

	      fix_new_exp (frag_now,
			   where,
			   fx->fix[i].size,
			   &(fx->fix[i].exp),
			   fx->fix[i].pcrel,
			   fx->fix[i].operand|2048);
	    }
	}
      fx->fc = 0;
      fx = fx->next;
    }
  return 0;
}

/* This is the main entry point for the machine-dependent assembler.
   str points to a machine-dependent instruction.  This function is
   supposed to emit the frags/bytes it assembles to.  For the D10V, it
   mostly handles the special VLIW parsing and packing and leaves the
   difficult stuff to do_assemble().  */

static unsigned long prev_insn;
static struct d10v_opcode *prev_opcode = 0;
static subsegT prev_subseg;
static segT prev_seg = 0;;

/* Find the symbol which has the same name as the register in exp.  */

static symbolS *
find_symbol_matching_register (expressionS *exp)
{
  int i;

  if (exp->X_op != O_register)
    return NULL;

  /* Find the name of the register.  */
  for (i = d10v_reg_name_cnt (); i--;)
    if (d10v_predefined_registers[i].value == exp->X_add_number)
      break;

  if (i < 0)
    abort ();

  /* Now see if a symbol has been defined with the same name.  */
  return symbol_find (d10v_predefined_registers[i].name);
}

/* Get a pointer to an entry in the opcode table.
   The function must look at all opcodes with the same name and use
   the operands to choose the correct opcode.  */

static struct d10v_opcode *
find_opcode (struct d10v_opcode *opcode, expressionS myops[])
{
  int i, match;
  struct d10v_opcode *next_opcode;

  /* Get all the operands and save them as expressions.  */
  get_operands (myops);

  /* Now see if the operand is a fake.  If so, find the correct size
     instruction, if possible.  */
  if (opcode->format == OPCODE_FAKE)
    {
      int opnum = opcode->operands[0];
      int flags;

      if (myops[opnum].X_op == O_register)
	{
	  myops[opnum].X_op = O_symbol;
	  myops[opnum].X_add_symbol =
	    symbol_find_or_make ((char *) myops[opnum].X_op_symbol);
	  myops[opnum].X_add_number = 0;
	  myops[opnum].X_op_symbol = NULL;
	}

      next_opcode = opcode + 1;

      /* If the first operand is supposed to be a register, make sure
	 we got a valid one.  */
      flags = d10v_operands[next_opcode->operands[0]].flags;
      if (flags & OPERAND_REG)
	{
	  int X_op = myops[0].X_op;
	  int num = myops[0].X_add_number;

	  if (X_op != O_register
	      || (num & ~flags
		  & (OPERAND_GPR | OPERAND_ACC0 | OPERAND_ACC1
		     | OPERAND_FFLAG | OPERAND_CFLAG | OPERAND_CONTROL))
	      || ((flags & OPERAND_SP) && ! (num & OPERAND_SP)))
	    {
	      as_bad (_("bad opcode or operands"));
	      return 0;
	    }
	}

      if (myops[opnum].X_op == O_constant
	  || (myops[opnum].X_op == O_symbol
	      && S_IS_DEFINED (myops[opnum].X_add_symbol)
	      && (S_GET_SEGMENT (myops[opnum].X_add_symbol) == now_seg)))
	{
	  for (i = 0; opcode->operands[i + 1]; i++)
	    {
	      int bits = d10v_operands[next_opcode->operands[opnum]].bits;
	      int flags = d10v_operands[next_opcode->operands[opnum]].flags;
	      if (flags & OPERAND_ADDR)
		bits += 2;

	      if (myops[opnum].X_op == O_constant)
		{
		  if (!check_range (myops[opnum].X_add_number, bits, flags))
		    break;
		}
	      else
		{
		  fragS *sym_frag;
		  fragS *f;
		  unsigned long current_position;
		  unsigned long symbol_position;
		  unsigned long value;
		  bfd_boolean found_symbol;

		  /* Calculate the address of the current instruction
		     and the address of the symbol.  Do this by summing
		     the offsets of previous frags until we reach the
		     frag containing the symbol, and the current frag.  */
		  sym_frag = symbol_get_frag (myops[opnum].X_add_symbol);
		  found_symbol = FALSE;

		  current_position =
		    obstack_next_free (&frchain_now->frch_obstack)
		    - frag_now->fr_literal;
		  symbol_position = S_GET_VALUE (myops[opnum].X_add_symbol);

		  for (f = frchain_now->frch_root; f; f = f->fr_next)
		    {
		      current_position += f->fr_fix + f->fr_offset;

		      if (f == sym_frag)
			found_symbol = TRUE;

		      if (! found_symbol)
			symbol_position += f->fr_fix + f->fr_offset;
		    }

		  value = symbol_position;

		  if (flags & OPERAND_ADDR)
		    value -= current_position;

		  if (AT_WORD_P (&myops[opnum]))
		    {
		      if (bits > 4)
			{
			  bits += 2;
			  if (!check_range (value, bits, flags))
			    break;
			}
		    }
		  else if (!check_range (value, bits, flags))
		    break;
		}
	      next_opcode++;
	    }

	  if (opcode->operands [i + 1] == 0)
	    as_fatal (_("value out of range"));
	  else
	    opcode = next_opcode;
	}
      else
	/* Not a constant, so use a long instruction.  */
	opcode += 2;
    }

  match = 0;

  /* Now search the opcode table table for one with operands
     that matches what we've got.  */
  while (!match)
    {
      match = 1;
      for (i = 0; opcode->operands[i]; i++)
	{
	  int flags = d10v_operands[opcode->operands[i]].flags;
	  int X_op = myops[i].X_op;
	  int num = myops[i].X_add_number;

	  if (X_op == 0)
	    {
	      match = 0;
	      break;
	    }

	  if (flags & OPERAND_REG)
	    {
	      if ((X_op != O_register)
		  || (num & ~flags
		      & (OPERAND_GPR | OPERAND_ACC0 | OPERAND_ACC1
			 | OPERAND_FFLAG | OPERAND_CFLAG
			 | OPERAND_CONTROL))
		  || ((flags & OPERAND_SP) && ! (num & OPERAND_SP)))
		{
		  match = 0;
		  break;
		}
	    }

	  if (((flags & OPERAND_MINUS)   && ((X_op != O_absent) || (num != OPERAND_MINUS))) ||
	      ((flags & OPERAND_PLUS)    && ((X_op != O_absent) || (num != OPERAND_PLUS))) ||
	      ((flags & OPERAND_ATMINUS) && ((X_op != O_absent) || (num != OPERAND_ATMINUS))) ||
	      ((flags & OPERAND_ATPAR)   && ((X_op != O_absent) || (num != OPERAND_ATPAR))) ||
	      ((flags & OPERAND_ATSIGN)  && ((X_op != O_absent) || ((num != OPERAND_ATSIGN) && (num != OPERAND_ATPAR)))))
	    {
	      match = 0;
	      break;
	    }

	  /* Unfortunately, for the indirect operand in instructions such
	     as ``ldb r1, @(c,r14)'' this function can be passed
	     X_op == O_register (because 'c' is a valid register name).
	     However we cannot just ignore the case when X_op == O_register
	     but flags & OPERAND_REG is null, so we check to see if a symbol
	     of the same name as the register exists.  If the symbol does
	     exist, then the parser was unable to distinguish the two cases
	     and we fix things here. (Ref: PR14826)  */

	  if (!(flags & OPERAND_REG) && (X_op == O_register))
	    {
	      symbolS * sym;

	      sym = find_symbol_matching_register (& myops[i]);

	      if (sym != NULL)
		{
		  myops[i].X_op = X_op = O_symbol;
		  myops[i].X_add_symbol = sym;
		}
	      else
		as_bad
		  (_("illegal operand - register name found where none expected"));
	    }
	}

      /* We're only done if the operands matched so far AND there
	     are no more to check.  */
      if (match && myops[i].X_op == 0)
	break;
      else
	match = 0;

      next_opcode = opcode + 1;

      if (next_opcode->opcode == 0)
	break;

      if (strcmp (next_opcode->name, opcode->name))
	break;

      opcode = next_opcode;
    }

  if (!match)
    {
      as_bad (_("bad opcode or operands"));
      return 0;
    }

  /* Check that all registers that are required to be even are.
     Also, if any operands were marked as registers, but were really symbols,
     fix that here.  */
  for (i = 0; opcode->operands[i]; i++)
    {
      if ((d10v_operands[opcode->operands[i]].flags & OPERAND_EVEN) &&
	  (myops[i].X_add_number & 1))
	as_fatal (_("Register number must be EVEN"));
      if ((d10v_operands[opcode->operands[i]].flags & OPERAND_NOSP)
	  && (myops[i].X_add_number & OPERAND_SP))
	as_bad (_("Unsupported use of sp"));
      if (myops[i].X_op == O_register)
	{
	  if (!(d10v_operands[opcode->operands[i]].flags & OPERAND_REG))
	    {
	      myops[i].X_op = O_symbol;
	      myops[i].X_add_symbol =
		symbol_find_or_make ((char *) myops[i].X_op_symbol);
	      myops[i].X_add_number = 0;
	      myops[i].X_op_symbol = NULL;
	    }
	}
      if ((d10v_operands[opcode->operands[i]].flags & OPERAND_CONTROL)
	  && (myops[i].X_add_number == OPERAND_CONTROL + 4
	      || myops[i].X_add_number == OPERAND_CONTROL + 5
	      || myops[i].X_add_number == OPERAND_CONTROL + 6
	      || myops[i].X_add_number == OPERAND_CONTROL + 12
	      || myops[i].X_add_number == OPERAND_CONTROL + 13
	      || myops[i].X_add_number == OPERAND_CONTROL + 15))
	as_warn (_("cr%ld is a reserved control register"),
		 myops[i].X_add_number - OPERAND_CONTROL);
    }
  return opcode;
}

/* Assemble a single instruction.
   Return an opcode, or -1 (an invalid opcode) on error.  */

static unsigned long
do_assemble (char *str, struct d10v_opcode **opcode)
{
  unsigned char *op_start, *op_end;
  char *save;
  char name[20];
  int nlen = 0;
  expressionS myops[6];
  unsigned long insn;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the opcode end.  */
  for (op_start = op_end = (unsigned char *) str;
       *op_end && nlen < 20 && !is_end_of_line[*op_end] && *op_end != ' ';
       op_end++)
    {
      name[nlen] = TOLOWER (op_start[nlen]);
      nlen++;
    }
  name[nlen] = 0;

  if (nlen == 0)
    return -1;

  /* Find the first opcode with the proper name.  */
  *opcode = (struct d10v_opcode *) hash_find (d10v_hash, name);
  if (*opcode == NULL)
    as_fatal (_("unknown opcode: %s"), name);

  save = input_line_pointer;
  input_line_pointer = (char *) op_end;
  *opcode = find_opcode (*opcode, myops);
  if (*opcode == 0)
    return -1;
  input_line_pointer = save;

  insn = build_insn ((*opcode), myops, 0);
  return insn;
}

/* If while processing a fixup, a reloc really needs to be created.
   Then it is done here.  */

arelent *
tc_gen_reloc (asection *seg ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *reloc;
  reloc = xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("reloc %d not supported by object file format"),
		    (int) fixp->fx_r_type);
      return NULL;
    }

  if (fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    reloc->address = fixp->fx_offset;

  reloc->addend = 0;

  return reloc;
}

int
md_estimate_size_before_relax (fragS *fragp ATTRIBUTE_UNUSED,
			       asection *seg ATTRIBUTE_UNUSED)
{
  abort ();
  return 0;
}

long
md_pcrel_from_section (fixS *fixp, segT sec)
{
  if (fixp->fx_addsy != (symbolS *) NULL
      && (!S_IS_DEFINED (fixp->fx_addsy)
	  || (S_GET_SEGMENT (fixp->fx_addsy) != sec)))
    return 0;
  return fixp->fx_frag->fr_address + fixp->fx_where;
}

void
md_apply_fix (fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  char *where;
  unsigned long insn;
  long value = *valP;
  int op_type;
  int left = 0;

  if (fixP->fx_addsy == (symbolS *) NULL)
    fixP->fx_done = 1;

  /* We don't actually support subtracting a symbol.  */
  if (fixP->fx_subsy != (symbolS *) NULL)
    as_bad_where (fixP->fx_file, fixP->fx_line, _("expression too complex"));

  op_type = fixP->fx_r_type;
  if (op_type & 2048)
    {
      op_type -= 2048;
      if (op_type & 1024)
	{
	  op_type -= 1024;
	  fixP->fx_r_type = BFD_RELOC_D10V_10_PCREL_L;
	  left = 1;
	}
      else if (op_type & 4096)
	{
	  op_type -= 4096;
	  fixP->fx_r_type = BFD_RELOC_D10V_18;
	}
      else
	fixP->fx_r_type =
	  get_reloc ((struct d10v_operand *) &d10v_operands[op_type]);
    }

  /* Fetch the instruction, insert the fully resolved operand
     value, and stuff the instruction back again.  */
  where = fixP->fx_frag->fr_literal + fixP->fx_where;
  insn = bfd_getb32 ((unsigned char *) where);

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_D10V_10_PCREL_L:
    case BFD_RELOC_D10V_10_PCREL_R:
    case BFD_RELOC_D10V_18_PCREL:
      /* If the fix is relative to a global symbol, not a section
	 symbol, then ignore the offset.
         XXX - Do we have to worry about branches to a symbol + offset ?  */
      if (fixP->fx_addsy != NULL
	  && S_IS_EXTERNAL (fixP->fx_addsy) )
        {
          segT fseg = S_GET_SEGMENT (fixP->fx_addsy);
          segment_info_type *segf = seg_info(fseg);

	  if ( segf && segf->sym != fixP->fx_addsy)
	    value = 0;
        }
      /* Drop through.  */
    case BFD_RELOC_D10V_18:
      /* Instruction addresses are always right-shifted by 2.  */
      value >>= AT_WORD_RIGHT_SHIFT;
      if (fixP->fx_size == 2)
	bfd_putb16 ((bfd_vma) value, (unsigned char *) where);
      else
	{
	  struct d10v_opcode *rep, *repi;

	  rep = (struct d10v_opcode *) hash_find (d10v_hash, "rep");
	  repi = (struct d10v_opcode *) hash_find (d10v_hash, "repi");
	  if ((insn & FM11) == FM11
	      && ((repi != NULL
		   && (insn & repi->mask) == (unsigned) repi->opcode)
		  || (rep != NULL
		      && (insn & rep->mask) == (unsigned) rep->opcode))
	      && value < 4)
	    as_fatal
	      (_("line %d: rep or repi must include at least 4 instructions"),
	       fixP->fx_line);
	  insn =
	    d10v_insert_operand (insn, op_type, (offsetT) value, left, fixP);
	  bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
	}
      break;
    case BFD_RELOC_32:
      bfd_putb32 ((bfd_vma) value, (unsigned char *) where);
      break;
    case BFD_RELOC_16:
      bfd_putb16 ((bfd_vma) value, (unsigned char *) where);
      break;

    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return;

    default:
      as_fatal (_("line %d: unknown relocation type: 0x%x"),
		fixP->fx_line, fixP->fx_r_type);
    }
}

/* d10v_cleanup() is called after the assembler has finished parsing
   the input file, when a label is read from the input file, or when a
   stab directive is output.  Because the D10V assembler sometimes
   saves short instructions to see if it can package them with the
   next instruction, there may be a short instruction that still needs
   to be written.

   NOTE: accesses a global, etype.
   NOTE: invoked by various macros such as md_cleanup: see.  */

int
d10v_cleanup (void)
{
  segT seg;
  subsegT subseg;

  /* If cleanup was invoked because the assembler encountered, e.g., a
     user label, we write out the pending instruction, if any.  If it
     was invoked because the assembler is outputting a piece of line
     debugging information, though, we write out the pending
     instruction only if the --no-gstabs-packing command line switch
     has been specified.  */
  if (prev_opcode
      && etype == PACK_UNSPEC
      && (! outputting_stabs_line_debug || ! flag_allow_gstabs_packing))
    {
      seg = now_seg;
      subseg = now_subseg;

      if (prev_seg)
	subseg_set (prev_seg, prev_subseg);

      write_1_short (prev_opcode, prev_insn, fixups->next);
      subseg_set (seg, subseg);
      prev_opcode = NULL;
    }
  return 1;
}

/* Like normal .word, except support @word.
   Clobbers input_line_pointer, checks end-of-line.  */

static void
d10v_dot_word (int dummy ATTRIBUTE_UNUSED)
{
  expressionS exp;
  char *p;

  if (is_it_end_of_statement ())
    {
      demand_empty_rest_of_line ();
      return;
    }

  do
    {
      expression (&exp);
      if (!strncasecmp (input_line_pointer, "@word", 5))
	{
	  exp.X_add_number = 0;
	  input_line_pointer += 5;

	  p = frag_more (2);
	  fix_new_exp (frag_now, p - frag_now->fr_literal, 2,
		       &exp, 0, BFD_RELOC_D10V_18);
	}
      else
	emit_expr (&exp, 2);
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */
  demand_empty_rest_of_line ();
}

/* Mitsubishi asked that we support some old syntax that apparently
   had immediate operands starting with '#'.  This is in some of their
   sample code but is not documented (although it appears in some
   examples in their assembler manual). For now, we'll solve this
   compatibility problem by simply ignoring any '#' at the beginning
   of an operand.  */

/* Operands that begin with '#' should fall through to here.
   From expr.c.  */

void
md_operand (expressionS *expressionP)
{
  if (*input_line_pointer == '#' && ! do_not_ignore_hash)
    {
      input_line_pointer++;
      expression (expressionP);
    }
}

bfd_boolean
d10v_fix_adjustable (fixS *fixP)
{
  /* We need the symbol name for the VTABLE entries.  */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word",	d10v_dot_word,	2 },
  { NULL,       NULL,           0 }
};

void
md_assemble (char *str)
{
  /* etype is saved extype.  For multi-line instructions.  */
  packing_type extype = PACK_UNSPEC;		/* Parallel, etc.  */
  struct d10v_opcode *opcode;
  unsigned long insn;
  char *str2;

  if (etype == PACK_UNSPEC)
    {
      /* Look for the special multiple instruction separators.  */
      str2 = strstr (str, "||");
      if (str2)
	extype = PACK_PARALLEL;
      else
	{
	  str2 = strstr (str, "->");
	  if (str2)
	    extype = PACK_LEFT_RIGHT;
	  else
	    {
	      str2 = strstr (str, "<-");
	      if (str2)
		extype = PACK_RIGHT_LEFT;
	    }
	}

      /* str2 points to the separator, if there is one.  */
      if (str2)
	{
	  *str2 = 0;

	  /* If two instructions are present and we already have one saved,
	     then first write out the saved one.  */
	  d10v_cleanup ();

	  /* Assemble first instruction and save it.  */
	  prev_insn = do_assemble (str, &prev_opcode);
	  prev_seg = now_seg;
	  prev_subseg = now_subseg;
	  if (prev_insn == (unsigned long) -1)
	    as_fatal (_("can't find opcode "));
	  fixups = fixups->next;
	  str = str2 + 2;
	}
    }

  insn = do_assemble (str, &opcode);
  if (insn == (unsigned long) -1)
    {
      if (extype != PACK_UNSPEC)
	{
	  etype = extype;
	  return;
	}
      as_fatal (_("can't find opcode "));
    }

  if (etype != PACK_UNSPEC)
    {
      extype = etype;
      etype = PACK_UNSPEC;
    }

  /* If this is a long instruction, write it and any previous short
     instruction.  */
  if (opcode->format & LONG_OPCODE)
    {
      if (extype != PACK_UNSPEC)
	as_fatal (_("Unable to mix instructions as specified"));
      d10v_cleanup ();
      write_long (insn, fixups);
      prev_opcode = NULL;
      return;
    }

  if (prev_opcode
      && prev_seg
      && ((prev_seg != now_seg) || (prev_subseg != now_subseg)))
    d10v_cleanup ();

  if (prev_opcode
      && (0 == write_2_short (prev_opcode, prev_insn, opcode, insn, extype,
			      fixups)))
    {
      /* No instructions saved.  */
      prev_opcode = NULL;
    }
  else
    {
      if (extype != PACK_UNSPEC)
	as_fatal (_("Unable to mix instructions as specified"));
      /* Save last instruction so it may be packed on next pass.  */
      prev_opcode = opcode;
      prev_insn = insn;
      prev_seg = now_seg;
      prev_subseg = now_subseg;
      fixups = fixups->next;
    }
}

