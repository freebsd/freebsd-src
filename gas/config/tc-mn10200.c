/* tc-mn10200.c -- Assembler code for the Matsushita 10200
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
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
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/mn10200.h"

/* Structure to hold information about predefined registers.  */
struct reg_name
{
  const char *name;
  int value;
};

/* Generic assembler global variables which must be defined by all
   targets.  */

/* Characters which always start a comment.  */
const char comment_chars[] = "#";

/* Characters which start a comment at the beginning of a line.  */
const char line_comment_chars[] = ";#";

/* Characters which may be used to separate multiple commands on a
   single line.  */
const char line_separator_chars[] = ";";

/* Characters which are used to indicate an exponent in a floating
   point number.  */
const char EXP_CHARS[] = "eE";

/* Characters which mean that a number is a floating point constant,
   as in 0d1.0.  */
const char FLT_CHARS[] = "dD";

const relax_typeS md_relax_table[] = {
  /* bCC relaxing  */
  {0x81, -0x7e, 2, 1},
  {0x8004, -0x7ffb, 5, 2},
  {0x800006, -0x7ffff9, 7, 0},
  /* bCCx relaxing  */
  {0x81, -0x7e, 3, 4},
  {0x8004, -0x7ffb, 6, 5},
  {0x800006, -0x7ffff9, 8, 0},
  /* jsr relaxing  */
  {0x8004, -0x7ffb, 3, 7},
  {0x800006, -0x7ffff9, 5, 0},
  /* jmp relaxing  */
  {0x81, -0x7e, 2, 9},
  {0x8004, -0x7ffb, 3, 10},
  {0x800006, -0x7ffff9, 5, 0},

};

/* Local functions.  */
static void mn10200_insert_operand PARAMS ((unsigned long *, unsigned long *,
					    const struct mn10200_operand *,
					    offsetT, char *, unsigned,
					    unsigned));
static unsigned long check_operand PARAMS ((unsigned long,
					    const struct mn10200_operand *,
					    offsetT));
static int reg_name_search PARAMS ((const struct reg_name *, int, const char *));
static bfd_boolean data_register_name PARAMS ((expressionS *expressionP));
static bfd_boolean address_register_name PARAMS ((expressionS *expressionP));
static bfd_boolean other_register_name PARAMS ((expressionS *expressionP));

/* Fixups.  */
#define MAX_INSN_FIXUPS (5)
struct mn10200_fixup
{
  expressionS exp;
  int opindex;
  bfd_reloc_code_real_type reloc;
};
struct mn10200_fixup fixups[MAX_INSN_FIXUPS];
static int fc;

const char *md_shortopts = "";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { NULL,       NULL,           0 }
};

/* Opcode hash table.  */
static struct hash_control *mn10200_hash;

/* This table is sorted. Suitable for searching by a binary search.  */
static const struct reg_name data_registers[] =
{
  { "d0", 0 },
  { "d1", 1 },
  { "d2", 2 },
  { "d3", 3 },
};
#define DATA_REG_NAME_CNT				\
  (sizeof (data_registers) / sizeof (struct reg_name))

static const struct reg_name address_registers[] =
{
  { "a0", 0 },
  { "a1", 1 },
  { "a2", 2 },
  { "a3", 3 },
};
#define ADDRESS_REG_NAME_CNT					\
  (sizeof (address_registers) / sizeof (struct reg_name))

static const struct reg_name other_registers[] =
{
  { "mdr", 0 },
  { "psw", 0 },
};
#define OTHER_REG_NAME_CNT				\
  (sizeof (other_registers) / sizeof (struct reg_name))

/* reg_name_search does a binary search of the given register table
   to see if "name" is a valid regiter name.  Returns the register
   number from the array on success, or -1 on failure.  */

static int
reg_name_search (regs, regcount, name)
     const struct reg_name *regs;
     int regcount;
     const char *name;
{
  int middle, low, high;
  int cmp;

  low = 0;
  high = regcount - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, regs[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else
	return regs[middle].value;
    }
  while (low <= high);
  return -1;
}

/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: An expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */

static bfd_boolean
data_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand.  */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (data_registers, DATA_REG_NAME_CNT, name);

  /* Put back the delimiting char.  */
  *input_line_pointer = c;

  /* Look to see if it's in the register table.  */
  if (reg_number >= 0)
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* Make the rest nice.  */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;

      return TRUE;
    }

  /* Reset the line as if we had not done anything.  */
  input_line_pointer = start;
  return FALSE;
}

/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: An expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */

static bfd_boolean
address_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand.  */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (address_registers, ADDRESS_REG_NAME_CNT, name);

  /* Put back the delimiting char.  */
  *input_line_pointer = c;

  /* Look to see if it's in the register table.  */
  if (reg_number >= 0)
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* Make the rest nice.  */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;

      return TRUE;
    }

  /* Reset the line as if we had not done anything.  */
  input_line_pointer = start;
  return FALSE;
}

/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: An expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */

static bfd_boolean
other_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand.  */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (other_registers, OTHER_REG_NAME_CNT, name);

  /* Put back the delimiting char.  */
  *input_line_pointer = c;

  /* Look to see if it's in the register table.  */
  if (reg_number >= 0)
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* Make the rest nice.  */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;

      return TRUE;
    }

  /* Reset the line as if we had not done anything.  */
  input_line_pointer = start;
  return FALSE;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("MN10200 options:\n\
none yet\n"));
}

int
md_parse_option (c, arg)
     int c ATTRIBUTE_UNUSED;
     char *arg ATTRIBUTE_UNUSED;
{
  return 0;
}

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

char *
md_atof (type, litp, sizep)
     int type;
     char *litp;
     int *sizep;
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
      *sizep = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizep = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      md_number_to_chars (litp, (valueT) words[i], 2);
      litp += 2;
    }

  return NULL;
}

void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     fragS *fragP;
{
  static unsigned long label_count = 0;
  char buf[40];

  subseg_change (sec, 0);
  if (fragP->fr_subtype == 0)
    {
      fix_new (fragP, fragP->fr_fix + 1, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 2;
    }
  else if (fragP->fr_subtype == 1)
    {
      /* Reverse the condition of the first branch.  */
      int offset = fragP->fr_fix;
      int opcode = fragP->fr_literal[offset] & 0xff;

      switch (opcode)
	{
	case 0xe8:
	  opcode = 0xe9;
	  break;
	case 0xe9:
	  opcode = 0xe8;
	  break;
	case 0xe0:
	  opcode = 0xe2;
	  break;
	case 0xe2:
	  opcode = 0xe0;
	  break;
	case 0xe3:
	  opcode = 0xe1;
	  break;
	case 0xe1:
	  opcode = 0xe3;
	  break;
	case 0xe4:
	  opcode = 0xe6;
	  break;
	case 0xe6:
	  opcode = 0xe4;
	  break;
	case 0xe7:
	  opcode = 0xe5;
	  break;
	case 0xe5:
	  opcode = 0xe7;
	  break;
	default:
	  abort ();
	}
      fragP->fr_literal[offset] = opcode;

      /* Create a fixup for the reversed conditional branch.  */
      sprintf (buf, ".%s_%ld", FAKE_LABEL_NAME, label_count++);
      fix_new (fragP, fragP->fr_fix + 1, 1,
	       symbol_new (buf, sec, 0, fragP->fr_next),
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);

      /* Now create the unconditional branch + fixup to the
	 final target.  */
      fragP->fr_literal[offset + 2] = 0xfc;
      fix_new (fragP, fragP->fr_fix + 3, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_16_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 5;
    }
  else if (fragP->fr_subtype == 2)
    {
      /* Reverse the condition of the first branch.  */
      int offset = fragP->fr_fix;
      int opcode = fragP->fr_literal[offset] & 0xff;

      switch (opcode)
	{
	case 0xe8:
	  opcode = 0xe9;
	  break;
	case 0xe9:
	  opcode = 0xe8;
	  break;
	case 0xe0:
	  opcode = 0xe2;
	  break;
	case 0xe2:
	  opcode = 0xe0;
	  break;
	case 0xe3:
	  opcode = 0xe1;
	  break;
	case 0xe1:
	  opcode = 0xe3;
	  break;
	case 0xe4:
	  opcode = 0xe6;
	  break;
	case 0xe6:
	  opcode = 0xe4;
	  break;
	case 0xe7:
	  opcode = 0xe5;
	  break;
	case 0xe5:
	  opcode = 0xe7;
	  break;
	default:
	  abort ();
	}
      fragP->fr_literal[offset] = opcode;

      /* Create a fixup for the reversed conditional branch.  */
      sprintf (buf, ".%s_%ld", FAKE_LABEL_NAME, label_count++);
      fix_new (fragP, fragP->fr_fix + 1, 1,
	       symbol_new (buf, sec, 0, fragP->fr_next),
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);

      /* Now create the unconditional branch + fixup to the
	 final target.  */
      fragP->fr_literal[offset + 2] = 0xf4;
      fragP->fr_literal[offset + 3] = 0xe0;
      fix_new (fragP, fragP->fr_fix + 4, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_24_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 7;
    }
  else if (fragP->fr_subtype == 3)
    {
      fix_new (fragP, fragP->fr_fix + 2, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 3;
    }
  else if (fragP->fr_subtype == 4)
    {
      /* Reverse the condition of the first branch.  */
      int offset = fragP->fr_fix;
      int opcode = fragP->fr_literal[offset + 1] & 0xff;

      switch (opcode)
	{
	case 0xfc:
	  opcode = 0xfd;
	  break;
	case 0xfd:
	  opcode = 0xfc;
	  break;
	case 0xfe:
	  opcode = 0xff;
	  break;
	case 0xff:
	  opcode = 0xfe;
	case 0xe8:
	  opcode = 0xe9;
	  break;
	case 0xe9:
	  opcode = 0xe8;
	  break;
	case 0xe0:
	  opcode = 0xe2;
	  break;
	case 0xe2:
	  opcode = 0xe0;
	  break;
	case 0xe3:
	  opcode = 0xe1;
	  break;
	case 0xe1:
	  opcode = 0xe3;
	  break;
	case 0xe4:
	  opcode = 0xe6;
	  break;
	case 0xe6:
	  opcode = 0xe4;
	  break;
	case 0xe7:
	  opcode = 0xe5;
	  break;
	case 0xe5:
	  opcode = 0xe7;
	  break;
	case 0xec:
	  opcode = 0xed;
	  break;
	case 0xed:
	  opcode = 0xec;
	  break;
	case 0xee:
	  opcode = 0xef;
	  break;
	case 0xef:
	  opcode = 0xee;
	  break;
	default:
	  abort ();
	}
      fragP->fr_literal[offset + 1] = opcode;

      /* Create a fixup for the reversed conditional branch.  */
      sprintf (buf, ".%s_%ld", FAKE_LABEL_NAME, label_count++);
      fix_new (fragP, fragP->fr_fix + 2, 1,
	       symbol_new (buf, sec, 0, fragP->fr_next),
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);

      /* Now create the unconditional branch + fixup to the
	 final target.  */
      fragP->fr_literal[offset + 3] = 0xfc;
      fix_new (fragP, fragP->fr_fix + 4, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_16_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 6;
    }
  else if (fragP->fr_subtype == 5)
    {
      /* Reverse the condition of the first branch.  */
      int offset = fragP->fr_fix;
      int opcode = fragP->fr_literal[offset + 1] & 0xff;

      switch (opcode)
	{
	case 0xfc:
	  opcode = 0xfd;
	  break;
	case 0xfd:
	  opcode = 0xfc;
	  break;
	case 0xfe:
	  opcode = 0xff;
	  break;
	case 0xff:
	  opcode = 0xfe;
	case 0xe8:
	  opcode = 0xe9;
	  break;
	case 0xe9:
	  opcode = 0xe8;
	  break;
	case 0xe0:
	  opcode = 0xe2;
	  break;
	case 0xe2:
	  opcode = 0xe0;
	  break;
	case 0xe3:
	  opcode = 0xe1;
	  break;
	case 0xe1:
	  opcode = 0xe3;
	  break;
	case 0xe4:
	  opcode = 0xe6;
	  break;
	case 0xe6:
	  opcode = 0xe4;
	  break;
	case 0xe7:
	  opcode = 0xe5;
	  break;
	case 0xe5:
	  opcode = 0xe7;
	  break;
	case 0xec:
	  opcode = 0xed;
	  break;
	case 0xed:
	  opcode = 0xec;
	  break;
	case 0xee:
	  opcode = 0xef;
	  break;
	case 0xef:
	  opcode = 0xee;
	  break;
	default:
	  abort ();
	}
      fragP->fr_literal[offset + 1] = opcode;

      /* Create a fixup for the reversed conditional branch.  */
      sprintf (buf, ".%s_%ld", FAKE_LABEL_NAME, label_count++);
      fix_new (fragP, fragP->fr_fix + 2, 1,
	       symbol_new (buf, sec, 0, fragP->fr_next),
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);

      /* Now create the unconditional branch + fixup to the
	 final target.  */
      fragP->fr_literal[offset + 3] = 0xf4;
      fragP->fr_literal[offset + 4] = 0xe0;
      fix_new (fragP, fragP->fr_fix + 5, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_24_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 8;
    }
  else if (fragP->fr_subtype == 6)
    {
      fix_new (fragP, fragP->fr_fix + 1, 2, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_16_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 3;
    }
  else if (fragP->fr_subtype == 7)
    {
      int offset = fragP->fr_fix;
      fragP->fr_literal[offset] = 0xf4;
      fragP->fr_literal[offset + 1] = 0xe1;

      fix_new (fragP, fragP->fr_fix + 2, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_24_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 5;
    }
  else if (fragP->fr_subtype == 8)
    {
      fragP->fr_literal[fragP->fr_fix] = 0xea;
      fix_new (fragP, fragP->fr_fix + 1, 1, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_8_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 2;
    }
  else if (fragP->fr_subtype == 9)
    {
      int offset = fragP->fr_fix;
      fragP->fr_literal[offset] = 0xfc;

      fix_new (fragP, fragP->fr_fix + 1, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_16_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 3;
    }
  else if (fragP->fr_subtype == 10)
    {
      int offset = fragP->fr_fix;
      fragP->fr_literal[offset] = 0xf4;
      fragP->fr_literal[offset + 1] = 0xe0;

      fix_new (fragP, fragP->fr_fix + 2, 4, fragP->fr_symbol,
	       fragP->fr_offset, 1, BFD_RELOC_24_PCREL);
      fragP->fr_var = 0;
      fragP->fr_fix += 5;
    }
  else
    abort ();
}

valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

void
md_begin ()
{
  char *prev_name = "";
  register const struct mn10200_opcode *op;

  mn10200_hash = hash_new ();

  /* Insert unique names into hash table.  The MN10200 instruction set
     has many identical opcode names that have different opcodes based
     on the operands.  This hash table then provides a quick index to
     the first opcode with a particular name in the opcode table.  */

  op = mn10200_opcodes;
  while (op->name)
    {
      if (strcmp (prev_name, op->name))
	{
	  prev_name = (char *) op->name;
	  hash_insert (mn10200_hash, op->name, (char *) op);
	}
      op++;
    }

  /* This is both a simplification (we don't have to write md_apply_fix3)
     and support for future optimizations (branch shortening and similar
     stuff in the linker.  */
  linkrelax = 1;
}

void
md_assemble (str)
     char *str;
{
  char *s;
  struct mn10200_opcode *opcode;
  struct mn10200_opcode *next_opcode;
  const unsigned char *opindex_ptr;
  int next_opindex, relaxable;
  unsigned long insn, extension, size = 0;
  char *f;
  int i;
  int match;

  /* Get the opcode.  */
  for (s = str; *s != '\0' && !ISSPACE (*s); s++)
    ;
  if (*s != '\0')
    *s++ = '\0';

  /* Find the first opcode with the proper name.  */
  opcode = (struct mn10200_opcode *) hash_find (mn10200_hash, str);
  if (opcode == NULL)
    {
      as_bad (_("Unrecognized opcode: `%s'"), str);
      return;
    }

  str = s;
  while (ISSPACE (*str))
    ++str;

  input_line_pointer = str;

  for (;;)
    {
      const char *errmsg = NULL;
      int op_idx;
      char *hold;
      int extra_shift = 0;

      relaxable = 0;
      fc = 0;
      match = 0;
      next_opindex = 0;
      insn = opcode->opcode;
      extension = 0;
      for (op_idx = 1, opindex_ptr = opcode->operands;
	   *opindex_ptr != 0;
	   opindex_ptr++, op_idx++)
	{
	  const struct mn10200_operand *operand;
	  expressionS ex;

	  if (next_opindex == 0)
	    {
	      operand = &mn10200_operands[*opindex_ptr];
	    }
	  else
	    {
	      operand = &mn10200_operands[next_opindex];
	      next_opindex = 0;
	    }

	  errmsg = NULL;

	  while (*str == ' ' || *str == ',')
	    ++str;

	  if (operand->flags & MN10200_OPERAND_RELAX)
	    relaxable = 1;

	  /* Gather the operand.  */
	  hold = input_line_pointer;
	  input_line_pointer = str;

	  if (operand->flags & MN10200_OPERAND_PAREN)
	    {
	      if (*input_line_pointer != ')' && *input_line_pointer != '(')
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      input_line_pointer++;
	      goto keep_going;
	    }
	  /* See if we can match the operands.  */
	  else if (operand->flags & MN10200_OPERAND_DREG)
	    {
	      if (!data_register_name (&ex))
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	    }
	  else if (operand->flags & MN10200_OPERAND_AREG)
	    {
	      if (!address_register_name (&ex))
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	    }
	  else if (operand->flags & MN10200_OPERAND_PSW)
	    {
	      char *start = input_line_pointer;
	      char c = get_symbol_end ();

	      if (strcmp (start, "psw") != 0)
		{
		  *input_line_pointer = c;
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      *input_line_pointer = c;
	      goto keep_going;
	    }
	  else if (operand->flags & MN10200_OPERAND_MDR)
	    {
	      char *start = input_line_pointer;
	      char c = get_symbol_end ();

	      if (strcmp (start, "mdr") != 0)
		{
		  *input_line_pointer = c;
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}
	      *input_line_pointer = c;
	      goto keep_going;
	    }
	  else if (data_register_name (&ex))
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else if (address_register_name (&ex))
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else if (other_register_name (&ex))
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else if (*str == ')' || *str == '(')
	    {
	      input_line_pointer = hold;
	      str = hold;
	      goto error;
	    }
	  else
	    {
	      expression (&ex);
	    }

	  switch (ex.X_op)
	    {
	    case O_illegal:
	      errmsg = _("illegal operand");
	      goto error;
	    case O_absent:
	      errmsg = _("missing operand");
	      goto error;
	    case O_register:
	      if ((operand->flags
		   & (MN10200_OPERAND_DREG | MN10200_OPERAND_AREG)) == 0)
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}

	      if (opcode->format == FMT_2 || opcode->format == FMT_5)
		extra_shift = 8;
	      else if (opcode->format == FMT_3 || opcode->format == FMT_6
		       || opcode->format == FMT_7)
		extra_shift = 16;
	      else
		extra_shift = 0;

	      mn10200_insert_operand (&insn, &extension, operand,
				      ex.X_add_number, (char *) NULL,
				      0, extra_shift);

	      break;

	    case O_constant:
	      /* If this operand can be promoted, and it doesn't
		 fit into the allocated bitfield for this insn,
		 then promote it (ie this opcode does not match).  */
	      if (operand->flags
		  & (MN10200_OPERAND_PROMOTE | MN10200_OPERAND_RELAX)
		  && !check_operand (insn, operand, ex.X_add_number))
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}

	      mn10200_insert_operand (&insn, &extension, operand,
				      ex.X_add_number, (char *) NULL,
				      0, 0);
	      break;

	    default:
	      /* If this operand can be promoted, then this opcode didn't
		 match since we can't know if it needed promotion!  */
	      if (operand->flags & MN10200_OPERAND_PROMOTE)
		{
		  input_line_pointer = hold;
		  str = hold;
		  goto error;
		}

	      /* We need to generate a fixup for this expression.  */
	      if (fc >= MAX_INSN_FIXUPS)
		as_fatal (_("too many fixups"));
	      fixups[fc].exp = ex;
	      fixups[fc].opindex = *opindex_ptr;
	      fixups[fc].reloc = BFD_RELOC_UNUSED;
	      ++fc;
	      break;
	    }

keep_going:
	  str = input_line_pointer;
	  input_line_pointer = hold;

	  while (*str == ' ' || *str == ',')
	    ++str;

	}

      /* Make sure we used all the operands!  */
      if (*str != ',')
	match = 1;

    error:
      if (match == 0)
	{
	  next_opcode = opcode + 1;
	  if (!strcmp (next_opcode->name, opcode->name))
	    {
	      opcode = next_opcode;
	      continue;
	    }

	  as_bad ("%s", errmsg);
	  return;
	}
      break;
    }

  while (ISSPACE (*str))
    ++str;

  if (*str != '\0')
    as_bad (_("junk at end of line: `%s'"), str);

  input_line_pointer = str;

  if (opcode->format == FMT_1)
    size = 1;
  else if (opcode->format == FMT_2 || opcode->format == FMT_4)
    size = 2;
  else if (opcode->format == FMT_3 || opcode->format == FMT_5)
    size = 3;
  else if (opcode->format == FMT_6)
    size = 4;
  else if (opcode->format == FMT_7)
    size = 5;
  else
    abort ();

  /* Write out the instruction.  */

  if (relaxable && fc > 0)
    {
      int type;

      /* bCC  */
      if (size == 2 && opcode->opcode != 0xfc0000)
	{
	  /* Handle bra specially.  Basically treat it like jmp so
	     that we automatically handle 8, 16 and 32 bit offsets
	     correctly as well as jumps to an undefined address.

	     It is also important to not treat it like other bCC
	     instructions since the long forms of bra is different
	     from other bCC instructions.  */
	  if (opcode->opcode == 0xea00)
	    type = 8;
	  else
	    type = 0;
	}
      /* jsr  */
      else if (size == 3 && opcode->opcode == 0xfd0000)
	type = 6;
      /* jmp  */
      else if (size == 3 && opcode->opcode == 0xfc0000)
	type = 8;
      /* bCCx  */
      else
	type = 3;

      f = frag_var (rs_machine_dependent, 8, 8 - size, type,
		    fixups[0].exp.X_add_symbol,
		    fixups[0].exp.X_add_number,
		    (char *)fixups[0].opindex);
      number_to_chars_bigendian (f, insn, size);
      if (8 - size > 4)
	{
	  number_to_chars_bigendian (f + size, 0, 4);
	  number_to_chars_bigendian (f + size + 4, 0, 8 - size - 4);
	}
      else
	number_to_chars_bigendian (f + size, 0, 8 - size);
    }

  else
    {
      f = frag_more (size);

      /* Oh, what a mess.  The instruction is in big endian format, but
	 16 and 24bit immediates are little endian!  */
      if (opcode->format == FMT_3)
	{
	  number_to_chars_bigendian (f, (insn >> 16) & 0xff, 1);
	  number_to_chars_littleendian (f + 1, insn & 0xffff, 2);
	}
      else if (opcode->format == FMT_6)
	{
	  number_to_chars_bigendian (f, (insn >> 16) & 0xffff, 2);
	  number_to_chars_littleendian (f + 2, insn & 0xffff, 2);
	}
      else if (opcode->format == FMT_7)
	{
	  number_to_chars_bigendian (f, (insn >> 16) & 0xffff, 2);
	  number_to_chars_littleendian (f + 2, insn & 0xffff, 2);
	  number_to_chars_littleendian (f + 4, extension & 0xff, 1);
	}
      else
	{
	  number_to_chars_bigendian (f, insn, size > 4 ? 4 : size);
	}

      /* Create any fixups.  */
      for (i = 0; i < fc; i++)
	{
	  const struct mn10200_operand *operand;

	  operand = &mn10200_operands[fixups[i].opindex];
	  if (fixups[i].reloc != BFD_RELOC_UNUSED)
	    {
	      reloc_howto_type *reloc_howto;
	      int size;
	      int offset;
	      fixS *fixP;

	      reloc_howto = bfd_reloc_type_lookup (stdoutput,
						   fixups[i].reloc);

	      if (!reloc_howto)
		abort ();

	      size = bfd_get_reloc_size (reloc_howto);

	      if (size < 1 || size > 4)
		abort ();

	      offset = 4 - size;
	      fixP = fix_new_exp (frag_now, f - frag_now->fr_literal + offset,
				  size,
				  &fixups[i].exp,
				  reloc_howto->pc_relative,
				  fixups[i].reloc);

	      /* PC-relative offsets are from the first byte of the
		 next instruction, not from the start of the current
		 instruction.  */
	      if (reloc_howto->pc_relative)
		fixP->fx_offset += size;
	    }
	  else
	    {
	      int reloc, pcrel, reloc_size, offset;
	      fixS *fixP;

	      reloc = BFD_RELOC_NONE;
	      /* How big is the reloc?  Remember SPLIT relocs are
		 implicitly 32bits.  */
	      reloc_size = operand->bits;

	      offset = size - reloc_size / 8;

	      /* Is the reloc pc-relative?  */
	      pcrel = (operand->flags & MN10200_OPERAND_PCREL) != 0;

	      /* Choose a proper BFD relocation type.  */
	      if (pcrel)
		{
		  if (reloc_size == 8)
		    reloc = BFD_RELOC_8_PCREL;
		  else if (reloc_size == 24)
		    reloc = BFD_RELOC_24_PCREL;
		  else
		    abort ();
		}
	      else
		{
		  if (reloc_size == 32)
		    reloc = BFD_RELOC_32;
		  else if (reloc_size == 16)
		    reloc = BFD_RELOC_16;
		  else if (reloc_size == 8)
		    reloc = BFD_RELOC_8;
		  else if (reloc_size == 24)
		    reloc = BFD_RELOC_24;
		  else
		    abort ();
		}

	      /* Convert the size of the reloc into what fix_new_exp
                 wants.  */
	      reloc_size = reloc_size / 8;
	      if (reloc_size == 8)
		reloc_size = 0;
	      else if (reloc_size == 16)
		reloc_size = 1;
	      else if (reloc_size == 32 || reloc_size == 24)
		reloc_size = 2;

	      fixP = fix_new_exp (frag_now, f - frag_now->fr_literal + offset,
				  reloc_size, &fixups[i].exp, pcrel,
				  ((bfd_reloc_code_real_type) reloc));

	      /* PC-relative offsets are from the first byte of the
		 next instruction, not from the start of the current
		 instruction.  */
	      if (pcrel)
		fixP->fx_offset += size;
	    }
	}
    }
}

/* If while processing a fixup, a reloc really needs to be created
   Then it is done here.  */

arelent *
tc_gen_reloc (seg, fixp)
     asection *seg ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;
  reloc = (arelent *) xmalloc (sizeof (arelent));

  if (fixp->fx_subsy != NULL)
    {
      /* FIXME: We should resolve difference expressions if possible
	 here.  At least this is better than silently ignoring the
	 subtrahend.  */
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("can't resolve `%s' {%s section} - `%s' {%s section}"),
		    fixp->fx_addsy ? S_GET_NAME (fixp->fx_addsy) : "0",
		    segment_name (fixp->fx_addsy
				  ? S_GET_SEGMENT (fixp->fx_addsy)
				  : absolute_section),
		    S_GET_NAME (fixp->fx_subsy),
		    segment_name (S_GET_SEGMENT (fixp->fx_addsy)));
    }

  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("reloc %d not supported by object file format"),
		    (int) fixp->fx_r_type);
      return NULL;
    }
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->addend = fixp->fx_offset;
  return reloc;
}

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp;
     asection *seg;
{
  if (fragp->fr_subtype == 6
      && (!S_IS_DEFINED (fragp->fr_symbol)
	  || seg != S_GET_SEGMENT (fragp->fr_symbol)))
    fragp->fr_subtype = 7;
  else if (fragp->fr_subtype == 8
	   && (!S_IS_DEFINED (fragp->fr_symbol)
	       || seg != S_GET_SEGMENT (fragp->fr_symbol)))
    fragp->fr_subtype = 10;

  if (fragp->fr_subtype >= sizeof (md_relax_table) / sizeof (md_relax_table[0]))
    abort ();

  return md_relax_table[fragp->fr_subtype].rlx_length;
}

long
md_pcrel_from (fixp)
     fixS *fixp;
{
  return fixp->fx_frag->fr_address;
#if 0
  if (fixp->fx_addsy != (symbolS *) NULL && !S_IS_DEFINED (fixp->fx_addsy))
    {
      /* The symbol is undefined.  Let the linker figure it out.  */
      return 0;
    }
  return fixp->fx_frag->fr_address + fixp->fx_where;
#endif
}

void
md_apply_fix3 (fixP, valP, seg)
     fixS * fixP;
     valueT * valP ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
{
  /* We shouldn't ever get here because linkrelax is nonzero.  */
  abort ();
  fixP->fx_done = 1;
}

/* Insert an operand value into an instruction.  */

static void
mn10200_insert_operand (insnp, extensionp, operand, val, file, line, shift)
     unsigned long *insnp;
     unsigned long *extensionp;
     const struct mn10200_operand *operand;
     offsetT val;
     char *file;
     unsigned int line;
     unsigned int shift;
{
  /* No need to check 24 or 32bit operands for a bit.  */
  if (operand->bits < 24
      && (operand->flags & MN10200_OPERAND_NOCHECK) == 0)
    {
      long min, max;
      offsetT test;

      if ((operand->flags & MN10200_OPERAND_SIGNED) != 0)
	{
	  max = (1 << (operand->bits - 1)) - 1;
	  min = - (1 << (operand->bits - 1));
	}
      else
	{
	  max = (1 << operand->bits) - 1;
	  min = 0;
	}

      test = val;

      if (test < (offsetT) min || test > (offsetT) max)
        {
          const char *err =
            _("operand out of range (%s not between %ld and %ld)");
          char buf[100];

          sprint_value (buf, test);
          if (file == (char *) NULL)
            as_warn (err, buf, min, max);
          else
            as_warn_where (file, line, err, buf, min, max);
        }
    }

  if ((operand->flags & MN10200_OPERAND_EXTENDED) == 0)
    {
      *insnp |= (((long) val & ((1 << operand->bits) - 1))
		 << (operand->shift + shift));

      if ((operand->flags & MN10200_OPERAND_REPEATED) != 0)
	*insnp |= (((long) val & ((1 << operand->bits) - 1))
		   << (operand->shift + shift + 2));
    }
  else
    {
      *extensionp |= (val >> 16) & 0xff;
      *insnp |= val & 0xffff;
    }
}

static unsigned long
check_operand (insn, operand, val)
     unsigned long insn ATTRIBUTE_UNUSED;
     const struct mn10200_operand *operand;
     offsetT val;
{
  /* No need to check 24bit or 32bit operands for a bit.  */
  if (operand->bits < 24
      && (operand->flags & MN10200_OPERAND_NOCHECK) == 0)
    {
      long min, max;
      offsetT test;

      if ((operand->flags & MN10200_OPERAND_SIGNED) != 0)
	{
	  max = (1 << (operand->bits - 1)) - 1;
	  min = - (1 << (operand->bits - 1));
	}
      else
	{
	  max = (1 << operand->bits) - 1;
	  min = 0;
	}

      test = val;

      if (test < (offsetT) min || test > (offsetT) max)
	return 0;
      else
	return 1;
    }
  return 1;
}
