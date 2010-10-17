/* tc-msp430.c -- Assembler code for the Texas Instruments MSP430

  Copyright (C) 2002, 2003 Free Software Foundation, Inc.
  Contributed by Dmitry Diky <diwil@mail.ru>

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
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define PUSH_1X_WORKAROUND
#include "as.h"
#include "subsegs.h"
#include "opcode/msp430.h"
#include "safe-ctype.h"

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "";
const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "dD";

/* Handle  long expressions.  */
extern LITTLENUM_TYPE generic_bignum[];

static struct hash_control *msp430_hash;

static unsigned int msp430_operands
  PARAMS ((struct msp430_opcode_s *, char *));
static int msp430_srcoperand
  PARAMS ((struct msp430_operand_s *, char *, int, int *));
static int msp430_dstoperand
  PARAMS ((struct msp430_operand_s *, char *, int));
static char *parse_exp
  PARAMS ((char *, expressionS *));
static inline char *skip_space
  PARAMS ((char *));
static int check_reg
  PARAMS ((char *));
static void msp430_set_arch
  PARAMS ((int));
static void show_mcu_list
  PARAMS ((FILE *));
static void del_spaces
  PARAMS ((char *));

#define MAX_OP_LEN	256

struct mcu_type_s
{
  char *name;
  int isa;
  int mach;
};

#define MSP430_ISA_11   11
#define MSP430_ISA_110	110
#define MSP430_ISA_12   12
#define MSP430_ISA_13   13
#define MSP430_ISA_14   14
#define MSP430_ISA_15   15
#define MSP430_ISA_16   16
#define MSP430_ISA_31   31
#define MSP430_ISA_32   32
#define MSP430_ISA_33   33
#define MSP430_ISA_41   41
#define MSP430_ISA_42   42
#define MSP430_ISA_43   43
#define MSP430_ISA_44   44

#define CHECK_RELOC_MSP430 		((imm_op || byte_op)?BFD_RELOC_MSP430_16_BYTE:BFD_RELOC_MSP430_16)
#define CHECK_RELOC_MSP430_PCREL	((imm_op || byte_op)?BFD_RELOC_MSP430_16_PCREL_BYTE:BFD_RELOC_MSP430_16_PCREL)

static struct mcu_type_s mcu_types[] =
{
  {"msp1",       MSP430_ISA_11, bfd_mach_msp11},
  {"msp2",       MSP430_ISA_14, bfd_mach_msp14},
  {"msp430x110", MSP430_ISA_11, bfd_mach_msp11},
  {"msp430x112", MSP430_ISA_11, bfd_mach_msp11},
  {"msp430x1101",MSP430_ISA_110, bfd_mach_msp110},
  {"msp430x1111",MSP430_ISA_110, bfd_mach_msp110},
  {"msp430x1121",MSP430_ISA_110, bfd_mach_msp110},
  {"msp430x1122",MSP430_ISA_11, bfd_mach_msp110},
  {"msp430x1132",MSP430_ISA_11, bfd_mach_msp110},

  {"msp430x122", MSP430_ISA_12, bfd_mach_msp12},
  {"msp430x123", MSP430_ISA_12, bfd_mach_msp12},
  {"msp430x1222",MSP430_ISA_12, bfd_mach_msp12},
  {"msp430x1232",MSP430_ISA_12, bfd_mach_msp12},

  {"msp430x133", MSP430_ISA_13, bfd_mach_msp13},
  {"msp430x135", MSP430_ISA_13, bfd_mach_msp13},
  {"msp430x1331",MSP430_ISA_13, bfd_mach_msp13},
  {"msp430x1351",MSP430_ISA_13, bfd_mach_msp13},
  {"msp430x147", MSP430_ISA_14, bfd_mach_msp14},
  {"msp430x148", MSP430_ISA_14, bfd_mach_msp14},
  {"msp430x149", MSP430_ISA_14, bfd_mach_msp14},

  {"msp430x155", MSP430_ISA_15, bfd_mach_msp15},
  {"msp430x156", MSP430_ISA_15, bfd_mach_msp15},
  {"msp430x157", MSP430_ISA_15, bfd_mach_msp15},
  {"msp430x167", MSP430_ISA_16, bfd_mach_msp16},
  {"msp430x168", MSP430_ISA_16, bfd_mach_msp16},
  {"msp430x169", MSP430_ISA_16, bfd_mach_msp16},

  {"msp430x311", MSP430_ISA_31, bfd_mach_msp31},
  {"msp430x312", MSP430_ISA_31, bfd_mach_msp31},
  {"msp430x313", MSP430_ISA_31, bfd_mach_msp31},
  {"msp430x314", MSP430_ISA_31, bfd_mach_msp31},
  {"msp430x315", MSP430_ISA_31, bfd_mach_msp31},
  {"msp430x323", MSP430_ISA_32, bfd_mach_msp32},
  {"msp430x325", MSP430_ISA_32, bfd_mach_msp32},
  {"msp430x336", MSP430_ISA_33, bfd_mach_msp33},
  {"msp430x337", MSP430_ISA_33, bfd_mach_msp33},

  {"msp430x412", MSP430_ISA_41, bfd_mach_msp41},
  {"msp430x413", MSP430_ISA_41, bfd_mach_msp41},

  {"msp430xE423", MSP430_ISA_42, bfd_mach_msp42},
  {"msp430xE425", MSP430_ISA_42, bfd_mach_msp42},
  {"msp430xE427", MSP430_ISA_42, bfd_mach_msp42},
  {"msp430xW423", MSP430_ISA_42, bfd_mach_msp42},
  {"msp430xW425", MSP430_ISA_42, bfd_mach_msp42},
  {"msp430xW427", MSP430_ISA_42, bfd_mach_msp42},

  {"msp430x435", MSP430_ISA_43, bfd_mach_msp43},
  {"msp430x436", MSP430_ISA_43, bfd_mach_msp43},
  {"msp430x437", MSP430_ISA_43, bfd_mach_msp43},
  {"msp430x447", MSP430_ISA_44, bfd_mach_msp44},
  {"msp430x448", MSP430_ISA_44, bfd_mach_msp44},
  {"msp430x449", MSP430_ISA_44, bfd_mach_msp44},

  {NULL, 0, 0}
};


static struct mcu_type_s default_mcu =
    { "msp430x11", MSP430_ISA_11, bfd_mach_msp11 };

static struct mcu_type_s *msp430_mcu = &default_mcu;

const pseudo_typeS md_pseudo_table[] =
{
  {"arch", msp430_set_arch, 0},
  {NULL, NULL, 0}
};

#define OPTION_MMCU 'm'

const char *md_shortopts = "m:";

struct option md_longopts[] =
{
  {"mmcu", required_argument, NULL, OPTION_MMCU},
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

static void
show_mcu_list (stream)
     FILE *stream;
{
  int i;

  fprintf (stream, _("Known MCU names:\n"));

  for (i = 0; mcu_types[i].name; i++)
    fprintf (stream, _("\t %s\n"), mcu_types[i].name);

  fprintf (stream, "\n");
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream,
	   _("MSP430 options:\n"
	     "  -mmcu=[msp430-name] select microcontroller type\n"
	     "                  msp430x110  msp430x112\n"
	     "                  msp430x1101 msp430x1111\n"
	     "                  msp430x1121 msp430x1122 msp430x1132\n"
	     "                  msp430x122  msp430x123\n"
	     "                  msp430x1222 msp430x1232\n"
	     "                  msp430x133  msp430x135\n"
	     "                  msp430x1331 msp430x1351\n"
	     "                  msp430x147  msp430x148  msp430x149\n"
	     "                  msp430x155  msp430x156  msp430x157\n"
	     "                  msp430x167  msp430x168  msp430x169\n"
	     "                  msp430x311  msp430x312  msp430x313  msp430x314  msp430x315\n"
	     "                  msp430x323  msp430x325\n"
	     "                  msp430x336  msp430x337\n"
	     "                  msp430x412  msp430x413\n"
	     "                  msp430xE423 msp430xE425 msp430E427\n"
	     "                  msp430xW423 msp430xW425 msp430W427\n"
	     "                  msp430x435  msp430x436  msp430x437\n"
	     "                  msp430x447  msp430x448  msp430x449\n"));

  show_mcu_list (stream);
}

static char *
extract_word (char *from, char *to, int limit)
{
  char *op_start;
  char *op_end;
  int size = 0;

  /* Drop leading whitespace.  */
  from = skip_space (from);
  *to = 0;

  /* Find the op code end.  */
  for (op_start = op_end = from; *op_end != 0 && is_part_of_name (*op_end);)
    {
      to[size++] = *op_end++;
      if (size + 1 >= limit)
	break;
    }

  to[size] = 0;
  return op_end;
}

static void
msp430_set_arch (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  char *str = (char *) alloca (32);	/* 32 for good measure.  */

  input_line_pointer = extract_word (input_line_pointer, str, 32);

  md_parse_option (OPTION_MMCU, str);
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, msp430_mcu->mach);
}

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  int i;

  switch (c)
    {
    case OPTION_MMCU:
      for (i = 0; mcu_types[i].name; ++i)
	if (strcmp (mcu_types[i].name, arg) == 0)
	  break;

      if (!mcu_types[i].name)
	{
	  show_mcu_list (stderr);
	  as_fatal (_("unknown MCU: %s\n"), arg);
	}

      if (msp430_mcu == &default_mcu || msp430_mcu->mach == mcu_types[i].mach)
	msp430_mcu = &mcu_types[i];
      else
	as_fatal (_("redefinition of mcu type %s' to %s'"),
		  msp430_mcu->name, mcu_types[i].name);
      return 1;
    }

  return 0;
}

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

static inline char *
skip_space (s)
     char *s;
{
  while (ISSPACE (*s))
    ++s;
  return s;
}

/* Delete spaces from s: X ( r 1  2)  => X(r12).  */

static void
del_spaces (s)
     char *s;
{
  while (*s)
    {
      if (ISSPACE (*s))
	{
	  char *m = s + 1;

	  while (ISSPACE (*m) && *m)
	    m++;
	  memmove (s, m, strlen (m) + 1);
	}
      else
	s++;
    }
}

/* Extract one word from FROM and copy it to TO. Delimeters are ",;\n"  */

static char *
extract_operand (char *from, char *to, int limit)
{
  int size = 0;

  /* Drop leading whitespace.  */
  from = skip_space (from);

  while (size < limit && *from)
    {
      *(to + size) = *from;
      if (*from == ',' || *from == ';' || *from == '\n')
	break;
      from++;
      size++;
    }

  *(to + size) = 0;
  del_spaces (to);

  from++;

  return from;
}

static char *
extract_cmd (char *from, char *to, int limit)
{
  int size = 0;

  while (*from && ! ISSPACE (*from) && *from != '.' && limit > size)
    {
      *(to + size) = *from;
      from++;
      size++;
    }

  *(to + size) = 0;

  return from;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[4];
  LITTLENUM_TYPE *wordP;
  char *t;

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

  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  /* This loop outputs the LITTLENUMs in REVERSE order.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return NULL;
}

void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
  abort ();
}

void
md_begin ()
{
  struct msp430_opcode_s *opcode;
  msp430_hash = hash_new ();

  for (opcode = msp430_opcodes; opcode->name; opcode++)
    hash_insert (msp430_hash, opcode->name, (char *) opcode);

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, msp430_mcu->mach);
}

void
md_assemble (str)
     char *str;
{
  struct msp430_opcode_s *opcode;
  char cmd[32];
  unsigned int i = 0;

  str = skip_space (str);	/* Skip leading spaces.  */
  str = extract_cmd (str, cmd, sizeof (cmd));

  while (cmd[i] && i < sizeof (cmd))
    {
      char a = TOLOWER (cmd[i]);
      cmd[i] = a;
      i++;
    }

  if (!cmd[0])
    {
      as_bad (_("can't find opcode "));
      return;
    }

  opcode = (struct msp430_opcode_s *) hash_find (msp430_hash, cmd);

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode `%s'"), cmd);
      return;
    }

  {
    char *__t = input_line_pointer;
    msp430_operands (opcode, str);
    input_line_pointer = __t;
  }
}

/* Parse instruction operands.
   Return binary opcode.  */

static unsigned int
msp430_operands (opcode, line)
     struct msp430_opcode_s *opcode;
     char *line;
{
  int bin = opcode->bin_opcode;	/* opcode mask.  */
  int __is;
  char l1[MAX_OP_LEN], l2[MAX_OP_LEN];
  char *frag;
  int where;
  struct msp430_operand_s op1, op2;
  int res = 0;
  static short ZEROS = 0;
  int byte_op, imm_op;

  /* opcode is the one from opcodes table
     line contains something like
     [.w] @r2+, 5(R1)
     or
     .b @r2+, 5(R1).  */

  /* Check if byte or word operation.  */
  if (*line == '.' && TOLOWER (*(line + 1)) == 'b')
    {
      bin |= BYTE_OPERATION;
      byte_op = 1;
    }
  else
    byte_op = 0;

  /* skip .[bwBW].  */
  while (! ISSPACE (*line) && *line)
    line++;

  if (opcode->insn_opnumb && (!*line || *line == '\n'))
    {
      as_bad (_("instruction %s requires %d operand(s)"),
	      opcode->name, opcode->insn_opnumb);
      return 0;
    }

  memset (l1, 0, sizeof (l1));
  memset (l2, 0, sizeof (l2));
  memset (&op1, 0, sizeof (op1));
  memset (&op2, 0, sizeof (op2));

  imm_op = 0;

  switch (opcode->fmt)
    {
    case 0:			/* Emulated.  */
      switch (opcode->insn_opnumb)
	{
	case 0:
	  /* Set/clear bits instructions.  */
	  __is = 2;
	  frag = frag_more (__is);
	  bfd_putl16 ((bfd_vma) bin, frag);
	  break;
	case 1:
	  /* Something which works with destination operand.  */
	  line = extract_operand (line, l1, sizeof (l1));
	  res = msp430_dstoperand (&op1, l1, opcode->bin_opcode);
	  if (res)
	    break;

	  bin |= (op1.reg | (op1.am << 7));
	  __is = 1 + op1.ol;
	  frag = frag_more (2 * __is);
	  where = frag - frag_now->fr_literal;
	  bfd_putl16 ((bfd_vma) bin, frag);

	  if (op1.mode == OP_EXP)
	    {
	      where += 2;
	      bfd_putl16 ((bfd_vma) ZEROS, frag + 2);

	      if (op1.reg)
		fix_new_exp (frag_now, where, 2,
			     &(op1.exp), FALSE, CHECK_RELOC_MSP430);
	      else
		fix_new_exp (frag_now, where, 2,
			     &(op1.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	    }
	  break;

	case 2:
	  {
	    /* Shift instruction.  */
	    line = extract_operand (line, l1, sizeof (l1));
	    strncpy (l2, l1, sizeof (l2));
	    l2[sizeof (l2) - 1] = '\0';
	    res = msp430_srcoperand (&op1, l1, opcode->bin_opcode, &imm_op);
	    res += msp430_dstoperand (&op2, l2, opcode->bin_opcode);

	    if (res)
	      break;	/* An error occurred.  All warnings were done before.  */

	    bin |= (op2.reg | (op1.reg << 8) | (op1.am << 4) | (op2.am << 7));

	    __is = 1 + op1.ol + op2.ol;	/* insn size in words.  */
	    frag = frag_more (2 * __is);
	    where = frag - frag_now->fr_literal;
	    bfd_putl16 ((bfd_vma) bin, frag);

	    if (op1.mode == OP_EXP)
	      {
		where += 2;	/* Advance 'where' as we do not know _where_.  */
		bfd_putl16 ((bfd_vma) ZEROS, frag + 2);

		if (op1.reg || (op1.reg == 0 && op1.am == 3))	/* Not PC relative.  */
		  fix_new_exp (frag_now, where, 2,
			       &(op1.exp), FALSE, CHECK_RELOC_MSP430);
		else
		  fix_new_exp (frag_now, where, 2,
			       &(op1.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	      }

	    if (op2.mode == OP_EXP)
	      {
		imm_op = 0;
		bfd_putl16 ((bfd_vma) ZEROS, frag + 2 + ((__is == 3) ? 2 : 0));

		if (op2.reg)	/* Not PC relative.  */
		  fix_new_exp (frag_now, where + 2, 2,
			       &(op2.exp), FALSE, CHECK_RELOC_MSP430);
		else
		  fix_new_exp (frag_now, where + 2, 2,
			       &(op2.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	      }
	    break;
	  }
	case 3:
	  /* Branch instruction => mov dst, r0.  */
	  line = extract_operand (line, l1, sizeof (l1));

	  res = msp430_srcoperand (&op1, l1, opcode->bin_opcode, &imm_op);
	  if (res)
	    break;

	  byte_op = 0;
	  imm_op = 0;

	  bin |= ((op1.reg << 8) | (op1.am << 4));
	  __is = 1 + op1.ol;
	  frag = frag_more (2 * __is);
	  where = frag - frag_now->fr_literal;
	  bfd_putl16 ((bfd_vma) bin, frag);

	  if (op1.mode == OP_EXP)
	    {
	      where += 2;
	      bfd_putl16 ((bfd_vma) ZEROS, frag + 2);

	      if (op1.reg || (op1.reg == 0 && op1.am == 3))
		fix_new_exp (frag_now, where, 2,
			     &(op1.exp), FALSE, CHECK_RELOC_MSP430);
	      else
		fix_new_exp (frag_now, where, 2,
			     &(op1.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	    }
	  break;
	}
      break;

    case 1:			/* Format 1, double operand.  */
      line = extract_operand (line, l1, sizeof (l1));
      line = extract_operand (line, l2, sizeof (l2));
      res = msp430_srcoperand (&op1, l1, opcode->bin_opcode, &imm_op);
      res += msp430_dstoperand (&op2, l2, opcode->bin_opcode);

      if (res)
	break;			/* Error occurred.  All warnings were done before.  */

      bin |= (op2.reg | (op1.reg << 8) | (op1.am << 4) | (op2.am << 7));

      __is = 1 + op1.ol + op2.ol;	/* insn size in words.  */
      frag = frag_more (2 * __is);
      where = frag - frag_now->fr_literal;
      bfd_putl16 ((bfd_vma) bin, frag);

      if (op1.mode == OP_EXP)
	{
	  where += 2;		/* Advance where as we do not know _where_.  */
	  bfd_putl16 ((bfd_vma) ZEROS, frag + 2);

	  if (op1.reg || (op1.reg == 0 && op1.am == 3))	/* Not PC relative.  */
	    fix_new_exp (frag_now, where, 2,
			 &(op1.exp), FALSE, CHECK_RELOC_MSP430);
	  else
	    fix_new_exp (frag_now, where, 2,
			 &(op1.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	}

      if (op2.mode == OP_EXP)
	{
	  imm_op = 0;
	  bfd_putl16 ((bfd_vma) ZEROS, frag + 2 + ((__is == 3) ? 2 : 0));

	  if (op2.reg)		/* Not PC relative.  */
	    fix_new_exp (frag_now, where + 2, 2,
			 &(op2.exp), FALSE, CHECK_RELOC_MSP430);
	  else
	    fix_new_exp (frag_now, where + 2, 2,
			 &(op2.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	}
      break;

    case 2:			/* Single-operand mostly instr.  */
      if (opcode->insn_opnumb == 0)
	{
	  /* reti instruction.  */
	  frag = frag_more (2);
	  bfd_putl16 ((bfd_vma) bin, frag);
	  break;
	}

      line = extract_operand (line, l1, sizeof (l1));
      res = msp430_srcoperand (&op1, l1, opcode->bin_opcode, &imm_op);
      if (res)
	break;		/* Error in operand.  */

      bin |= op1.reg | (op1.am << 4);
      __is = 1 + op1.ol;
      frag = frag_more (2 * __is);
      where = frag - frag_now->fr_literal;
      bfd_putl16 ((bfd_vma) bin, frag);

      if (op1.mode == OP_EXP)
	{
	  bfd_putl16 ((bfd_vma) ZEROS, frag + 2);

	  if (op1.reg || (op1.reg == 0 && op1.am == 3))	/* Not PC relative.  */
	    fix_new_exp (frag_now, where + 2, 2,
			 &(op1.exp), FALSE, CHECK_RELOC_MSP430);
	  else
	    fix_new_exp (frag_now, where + 2, 2,
			 &(op1.exp), TRUE, CHECK_RELOC_MSP430_PCREL);
	}
      break;

    case 3:			/* Conditional jumps instructions.  */
      line = extract_operand (line, l1, sizeof (l1));
      /* l1 is a label.  */
      if (l1[0])
	{
	  char *m = l1;
	  expressionS exp;

	  if (*m == '$')
	    m++;

	  parse_exp (m, &exp);
	  frag = frag_more (2);	/* Instr size is 1 word.  */

	  /* In order to handle something like:

	     and #0x8000, r5
	     tst r5
	     jz   4     ;       skip next 4 bytes
	     inv r5
	     inc r5
	     nop        ;       will jump here if r5 positive or zero

	     jCOND      -n      ;assumes jump n bytes backward:

	     mov r5,r6
	     jmp -2

	     is equal to:
	     lab:
	     mov r5,r6
	     jmp lab

	     jCOND      $n      ; jump from PC in either direction.  */

	  if (exp.X_op == O_constant)
	    {
	      int x = exp.X_add_number;

	      if (x & 1)
		{
		  as_warn (_("Even number required. Rounded to %d"), x + 1);
		  x++;
		}

	      if ((*l1 == '$' && x > 0) || x < 0)
		x -= 2;

	      x >>= 1;

	      if (x > 512 || x < -511)
		{
		  as_bad (_("Wrong displacement  %d"), x << 1);
		  break;
		}

	      bin |= x & 0x3ff;
	      bfd_putl16 ((bfd_vma) bin, frag);
	    }
	  else if (exp.X_op == O_symbol && *l1 != '$')
	    {
	      where = frag - frag_now->fr_literal;
	      fix_new_exp (frag_now, where, 2,
			   &exp, TRUE, BFD_RELOC_MSP430_10_PCREL);

	      bfd_putl16 ((bfd_vma) bin, frag);
	    }
	  else if (*l1 == '$')
	    {
	      as_bad (_("instruction requires label sans '$'"));
	      break;
	    }
	  else
	    {
	      as_bad (_
		      ("instruction requires label or value in range -511:512"));
	      break;
	    }
	}
      else
	{
	  as_bad (_("instruction requires label"));
	  break;
	}
      break;

    default:
      as_bad (_("Ilegal instruction or not implmented opcode."));
    }

  input_line_pointer = line;
  return 0;
}

static int
msp430_dstoperand (op, l, bin)
     struct msp430_operand_s *op;
     char *l;
     int bin;
{
  int dummy;
  int ret = msp430_srcoperand (op, l, bin, &dummy);
  if (ret)
    return ret;

  if (op->am == 2)
    {
      char *__tl = "0";

      op->mode = OP_EXP;
      op->am = 1;
      op->ol = 1;
      parse_exp (__tl, &(op->exp));
      if (op->exp.X_op != O_constant || op->exp.X_add_number != 0)
	{
	  as_bad (_("Internal bug. Try to use 0(r%d) instead of @r%d"),
		  op->reg, op->reg);
	  return 1;
	}
      return 0;
    }

  if (op->am > 1)
    {
      as_bad (_
	      ("this addressing mode is not applicable for destination operand"));
      return 1;
    }
  return 0;
}


static int
check_reg (t)
     char *t;
{
  /* If this is a reg numb, str 't' must be a number from 0 - 15.  */

  if (strlen (t) > 2 && *(t + 2) != '+')
    return 1;

  while (*t)
    {
      if ((*t < '0' || *t > '9') && *t != '+')
	break;
      t++;
    }

  if (*t)
    return 1;

  return 0;
}


static int
msp430_srcoperand (op, l, bin, imm_op)
     struct msp430_operand_s *op;
     char *l;
     int bin;
     int *imm_op;
{
  char *__tl = l;

  /* Check if an immediate #VALUE.  The hash sign should be only at the beginning!  */
  if (*l == '#')
    {
      char *h = l;
      int vshift = -1;
      int rval = 0;

      /* Check if there is:
	 llo(x) - least significant 16 bits, x &= 0xffff
	 lhi(x) - x = (x >> 16) & 0xffff,
	 hlo(x) - x = (x >> 32) & 0xffff,
	 hhi(x) - x = (x >> 48) & 0xffff
	 The value _MUST_ be constant expression: #hlo(1231231231).  */

      *imm_op = 1;

      if (strncasecmp (h, "#llo(", 5) == 0)
	{
	  vshift = 0;
	  rval = 3;
	}
      else if (strncasecmp (h, "#lhi(", 5) == 0)
	{
	  vshift = 1;
	  rval = 3;
	}
      else if (strncasecmp (h, "#hlo(", 5) == 0)
	{
	  vshift = 2;
	  rval = 3;
	}
      else if (strncasecmp (h, "#hhi(", 5) == 0)
	{
	  vshift = 3;
	  rval = 3;
	}
      else if (strncasecmp (h, "#lo(", 4) == 0)
	{
	  vshift = 0;
	  rval = 2;
	}
      else if (strncasecmp (h, "#hi(", 4) == 0)
	{
	  vshift = 1;
	  rval = 2;
	}

      op->reg = 0;		/* Reg PC.  */
      op->am = 3;
      op->ol = 1;		/* Immediate  will follow an instruction.  */
      __tl = h + 1 + rval;
      op->mode = OP_EXP;
      parse_exp (__tl, &(op->exp));
      if (op->exp.X_op == O_constant)
	{
	  int x = op->exp.X_add_number;

	  if (vshift == 0)
	    {
	      x = x & 0xffff;
	      op->exp.X_add_number = x;
	    }
	  else if (vshift == 1)
	    {
	      x = (x >> 16) & 0xffff;
	      op->exp.X_add_number = x;
	    }
	  else if (vshift > 1)
	    {
	      if (x < 0)
		op->exp.X_add_number = -1;
	      else
		op->exp.X_add_number = 0;	/* Nothing left.  */
	      x = op->exp.X_add_number;
	    }

	  if (op->exp.X_add_number > 65535 || op->exp.X_add_number < -32768)
	    {
	      as_bad (_("value %ld out of range. Use #lo() or #hi()"), x);
	      return 1;
	    }

	  /* Now check constants.  */
	  /* Substitute register mode with a constant generator if applicable.  */

	  x = (short) x;	/* Extend sign.  */

	  if (x == 0)
	    {
	      op->reg = 3;
	      op->am = 0;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 1)
	    {
	      op->reg = 3;
	      op->am = 1;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 2)
	    {
	      op->reg = 3;
	      op->am = 2;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == -1)
	    {
	      op->reg = 3;
	      op->am = 3;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 4)
	    {
#ifdef PUSH_1X_WORKAROUND
	      if (bin == 0x1200)
		{
		  /* Remove warning as confusing.
		     as_warn(_("Hardware push bug workaround")); */
		}
	      else
#endif
		{
		  op->reg = 2;
		  op->am = 2;
		  op->ol = 0;
		  op->mode = OP_REG;
		}
	    }
	  else if (x == 8)
	    {
#ifdef PUSH_1X_WORKAROUND
	      if (bin == 0x1200)
		{
		  /* Remove warning as confusing.
		     as_warn(_("Hardware push bug workaround")); */
		}
	      else
#endif
		{
		  op->reg = 2;
		  op->am = 3;
		  op->ol = 0;
		  op->mode = OP_REG;
		}
	    }
	}
      else if (op->exp.X_op == O_symbol)
	{
	  op->mode = OP_EXP;
	}
      else if (op->exp.X_op == O_big)
	{
	  short x;
	  if (vshift != -1)
	    {
	      op->exp.X_op = O_constant;
	      op->exp.X_add_number = 0xffff & generic_bignum[vshift];
	      x = op->exp.X_add_number;
	    }
	  else
	    {
	      as_bad (_
		      ("unknown expression in operand %s. use #llo() #lhi() #hlo() #hhi() "),
		      l);
	      return 1;
	    }

	  if (x == 0)
	    {
	      op->reg = 3;
	      op->am = 0;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 1)
	    {
	      op->reg = 3;
	      op->am = 1;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 2)
	    {
	      op->reg = 3;
	      op->am = 2;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == -1)
	    {
	      op->reg = 3;
	      op->am = 3;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 4)
	    {
	      op->reg = 2;
	      op->am = 2;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	  else if (x == 8)
	    {
	      op->reg = 2;
	      op->am = 3;
	      op->ol = 0;
	      op->mode = OP_REG;
	    }
	}
      else
	{
	  as_bad (_("unknown operand %s"), l);
	}
      return 0;
    }

  /* Check if absolute &VALUE (assume that we can construct something like ((a&b)<<7 + 25).  */
  if (*l == '&')
    {
      char *h = l;

      op->reg = 2;		/* reg 2 in absolute addr mode.  */
      op->am = 1;		/* mode As == 01 bin.  */
      op->ol = 1;		/* Immediate value followed by instruction.  */
      __tl = h + 1;
      parse_exp (__tl, &(op->exp));
      op->mode = OP_EXP;
      if (op->exp.X_op == O_constant)
	{
	  int x = op->exp.X_add_number;
	  if (x > 65535 || x < -32768)
	    {
	      as_bad (_("value out of range: %d"), x);
	      return 1;
	    }
	}
      else if (op->exp.X_op == O_symbol)
	{
	}
      else
	{
	  as_bad (_("unknown expression in operand %s"), l);
	  return 1;
	}
      return 0;
    }

  /* Check if indirect register mode @Rn / postincrement @Rn+.  */
  if (*l == '@')
    {
      char *t = l;
      char *m = strchr (l, '+');

      if (t != l)
	{
	  as_bad (_("unknown addressing mode %s"), l);
	  return 1;
	}

      t++;
      if (*t != 'r' && *t != 'R')
	{
	  as_bad (_("unknown addressing mode %s"), l);
	  return 1;
	}

      t++;	/* Points to the reg value.  */

      if (check_reg (t))
	{
	  as_bad (_("Bad register name r%s"), t);
	  return 1;
	}

      op->mode = OP_REG;
      op->am = m ? 3 : 2;
      op->ol = 0;
      if (m)
	*m = 0;			/* strip '+' */
      op->reg = atoi (t);
      if (op->reg < 0 || op->reg > 15)
	{
	  as_bad (_("MSP430 does not have %d registers"), op->reg);
	  return 1;
	}

      return 0;
    }

  /* Check if register indexed X(Rn).  */
  do
    {
      char *h = strrchr (l, '(');
      char *m = strrchr (l, ')');
      char *t;

      *imm_op = 1;

      if (!h)
	break;
      if (!m)
	{
	  as_bad (_("')' required"));
	  return 1;
	}

      t = h;
      op->am = 1;
      op->ol = 1;
      /* Extract a register.  */
      t++;	/* Advance pointer.  */

      if (*t != 'r' && *t != 'R')
	{
	  as_bad (_
		  ("unknown operator %s. Did you mean X(Rn) or #[hl][hl][oi](CONST) ?"),
		  l);
	  return 1;
	}
      t++;

      op->reg = *t - '0';
      if (op->reg > 9 || op->reg < 0)
	{
	  as_bad (_("unknown operator (r%s substituded as a register name"),
		  t);
	  return 1;
	}
      t++;
      if (*t != ')')
	{
	  op->reg = op->reg * 10;
	  op->reg += *t - '0';

	  if (op->reg > 15)
	    {
	      as_bad (_("unknown operator %s"), l);
	      return 1;
	    }
	  if (op->reg == 2)
	    {
	      as_bad (_("r2 should not be used in indexed addressing mode"));
	      return 1;
	    }

	  if (*(t + 1) != ')')
	    {
	      as_bad (_("unknown operator %s"), l);
	      return 1;
	    }
	}

      /* Extract constant.  */
      __tl = l;
      *h = 0;
      op->mode = OP_EXP;
      parse_exp (__tl, &(op->exp));
      if (op->exp.X_op == O_constant)
	{
	  int x = op->exp.X_add_number;

	  if (x > 65535 || x < -32768)
	    {
	      as_bad (_("value out of range: %d"), x);
	      return 1;
	    }

	  if (x == 0)
	    {
	      op->mode = OP_REG;
	      op->am = 2;
	      op->ol = 0;
	      return 0;
	    }
	}
      else if (op->exp.X_op == O_symbol)
	{
	}
      else
	{
	  as_bad (_("unknown expression in operand %s"), l);
	  return 1;
	}

      return 0;
    }
  while (0);

  /* Register mode 'mov r1,r2'.  */
  do
    {
      char *t = l;

      /* Operand should be a register.  */
      if (*t == 'r' || *t == 'R')
	{
	  int x = atoi (t + 1);

	  if (check_reg (t + 1))
	    break;

	  if (x < 0 || x > 15)
	    break;		/* Symbolic mode.  */

	  op->mode = OP_REG;
	  op->am = 0;
	  op->ol = 0;
	  op->reg = x;
	  return 0;
	}
    }
  while (0);

  /* Symbolic mode 'mov a, b' == 'mov x(pc), y(pc)'.  */
  do
    {
      char *t = l;

      __tl = l;

      while (*t)
	{
	  /* alpha/number    underline     dot for labels.  */
	  if (! ISALNUM (*t) && *t != '_' && *t != '.')
	    {
	      as_bad (_("unknown operand %s"), l);
	      return 1;
	    }
	  t++;
	}

      op->mode = OP_EXP;
      op->reg = 0;		/* PC relative... be careful.  */
      op->am = 1;
      op->ol = 1;
      __tl = l;
      parse_exp (__tl, &(op->exp));
      return 0;
    }
  while (0);

  /* Unreachable.  */
  as_bad (_("unknown addressing mode for operand %s"), l);
  return 1;
}


/* GAS will call this function for each section at the end of the assembly,
   to permit the CPU backend to adjust the alignment of a section.  */

valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);

  return ((addr + (1 << align) - 1) & (-1 << align));
}

/* If you define this macro, it should return the offset between the
   address of a PC relative fixup and the position from which the PC
   relative adjustment should be made.  On many processors, the base
   of a PC relative instruction is the next instruction, so this
   macro would return the length of an instruction.  */

long
md_pcrel_from_section (fixp, sec)
     fixS *fixp;
     segT sec;
{
  if (fixp->fx_addsy != (symbolS *) NULL
      && (!S_IS_DEFINED (fixp->fx_addsy)
	  || (S_GET_SEGMENT (fixp->fx_addsy) != sec)))
    return 0;

  return fixp->fx_frag->fr_address + fixp->fx_where;
}

/* GAS will call this for each fixup.  It should store the correct
   value in the object file.  */

void
md_apply_fix3 (fixp, valuep, seg)
     fixS *fixp;
     valueT *valuep;
     segT seg;
{
  unsigned char *where;
  unsigned long insn;
  long value;

  if (fixp->fx_addsy == (symbolS *) NULL)
    {
      value = *valuep;
      fixp->fx_done = 1;
    }
  else if (fixp->fx_pcrel)
    {
      segT s = S_GET_SEGMENT (fixp->fx_addsy);

      if (fixp->fx_addsy && (s == seg || s == absolute_section))
	{
	  value = S_GET_VALUE (fixp->fx_addsy) + *valuep;
	  fixp->fx_done = 1;
	}
      else
	value = *valuep;
    }
  else
    {
      value = fixp->fx_offset;

      if (fixp->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixp->fx_subsy) == absolute_section)
	    {
	      value -= S_GET_VALUE (fixp->fx_subsy);
	      fixp->fx_done = 1;
	    }
	  else
	    {
	      /* We don't actually support subtracting a symbol.  */
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    _("expression too complex"));
	    }
	}
    }

  switch (fixp->fx_r_type)
    {
    default:
      fixp->fx_no_overflow = 1;
      break;
    case BFD_RELOC_MSP430_10_PCREL:
      break;
    }

  if (fixp->fx_done)
    {
      /* Fetch the instruction, insert the fully resolved operand
	 value, and stuff the instruction back again.  */

      where = fixp->fx_frag->fr_literal + fixp->fx_where;

      insn = bfd_getl16 (where);

      switch (fixp->fx_r_type)
	{
	case BFD_RELOC_MSP430_10_PCREL:
	  if (value & 1)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("odd address operand: %ld"), value);

	  /* Jumps are in words.  */
	  value >>= 1;
	  --value;		/* Correct PC.  */

	  if (value < -512 || value > 511)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("operand out of range: %ld"), value);

	  value &= 0x3ff;	/* get rid of extended sign */
	  bfd_putl16 ((bfd_vma) (value | insn), where);
	  break;

	case BFD_RELOC_MSP430_16_PCREL:
	  if (value & 1)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("odd address operand: %ld"), value);

	  /* Nothing to be corrected here.  */
	  if (value < -32768 || value > 65536)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("operand out of range: %ld"), value);

	  value &= 0xffff;	/* Get rid of extended sign.  */
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_MSP430_16_PCREL_BYTE:
	  /* Nothing to be corrected here.  */
	  if (value < -32768 || value > 65536)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("operand out of range: %ld"), value);

	  value &= 0xffff;	/* Get rid of extended sign.  */
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_32:
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_MSP430_16:
	case BFD_RELOC_16:
	case BFD_RELOC_MSP430_16_BYTE:
	  value &= 0xffff;
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	default:
	  as_fatal (_("line %d: unknown relocation type: 0x%x"),
		    fixp->fx_line, fixp->fx_r_type);
	  break;
	}
    }
  else
    {
      fixp->fx_addnumber = value;
    }
}

/* A `BFD_ASSEMBLER' GAS will call this to generate a reloc.  GAS
   will pass the resulting reloc to `bfd_install_relocation'.  This
   currently works poorly, as `bfd_install_relocation' often does the
   wrong thing, and instances of `tc_gen_reloc' have been written to
   work around the problems, which in turns makes it difficult to fix
   `bfd_install_relocation'.  */

/* If while processing a fixup, a reloc really needs to be created
   then it is done here.  */

arelent *
tc_gen_reloc (seg, fixp)
     asection *seg ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
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

  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    reloc->address = fixp->fx_offset;

  reloc->addend = fixp->fx_offset;

  return reloc;
}

/* Parse ordinary expression.  */

static char *
parse_exp (s, op)
     char *s;
     expressionS *op;
{
  input_line_pointer = s;
  expression (op);
  if (op->X_op == O_absent)
    as_bad (_("missing operand"));
  return input_line_pointer;
}


int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp ATTRIBUTE_UNUSED;
     asection *seg ATTRIBUTE_UNUSED;
{
  abort ();
  return 0;
}
