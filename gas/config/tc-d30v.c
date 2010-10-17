/* tc-d30v.c -- Assembler code for the Mitsubishi D30V
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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
#include "opcode/d30v.h"

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "";
const char *md_shortopts = "OnNcC";
const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "dD";

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#define NOP_MULTIPLY 1
#define NOP_ALL 2
static int warn_nops = 0;
static int Optimizing = 0;
static int warn_register_name_conflicts = 1;

#define FORCE_SHORT	1
#define FORCE_LONG	2

/* EXEC types.  */
typedef enum _exec_type
{
  EXEC_UNKNOWN,			/* no order specified */
  EXEC_PARALLEL,		/* done in parallel (FM=00) */
  EXEC_SEQ,			/* sequential (FM=01) */
  EXEC_REVSEQ			/* reverse sequential (FM=10) */
} exec_type_enum;

/* Fixups.  */
#define MAX_INSN_FIXUPS (5)
struct d30v_fixup
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
  struct d30v_fixup fix[MAX_INSN_FIXUPS];
  struct _fixups *next;
} Fixups;

static Fixups FixUps[2];
static Fixups *fixups;

/* Whether current and previous instruction are word multiply insns.  */
static int cur_mul32_p = 0;
static int prev_mul32_p = 0;

/*  The flag_explicitly_parallel is true iff the instruction being assembled
    has been explicitly written as a parallel short-instruction pair by the
    human programmer.  It is used in parallel_ok () to distinguish between
    those dangerous parallelizations attempted by the human, which are to be
    allowed, and those attempted by the assembler, which are not.  It is set
    from md_assemble ().  */
static int flag_explicitly_parallel = 0;
static int flag_xp_state = 0;

/* Whether current and previous left sub-instruction disables
   execution of right sub-instruction.  */
static int cur_left_kills_right_p = 0;
static int prev_left_kills_right_p = 0;

/* The known current alignment of the current section.  */
static int d30v_current_align;
static segT d30v_current_align_seg;

/* The last seen label in the current section.  This is used to auto-align
   labels preceding instructions.  */
static symbolS *d30v_last_label;

/* Two nops.  */
#define NOP_LEFT   ((long long) NOP << 32)
#define NOP_RIGHT  ((long long) NOP)
#define NOP2 (FM00 | NOP_LEFT | NOP_RIGHT)

/* Local functions.  */
static int reg_name_search PARAMS ((char *name));
static int register_name PARAMS ((expressionS *expressionP));
static int check_range PARAMS ((unsigned long num, int bits, int flags));
static int postfix PARAMS ((char *p));
static bfd_reloc_code_real_type get_reloc PARAMS ((struct d30v_operand *op, int rel_flag));
static int get_operands PARAMS ((expressionS exp[], int cmp_hack));
static struct d30v_format *find_format PARAMS ((struct d30v_opcode *opcode,
			expressionS ops[],int fsize, int cmp_hack));
static long long build_insn PARAMS ((struct d30v_insn *opcode, expressionS *opers));
static void write_long PARAMS ((struct d30v_insn *opcode, long long insn, Fixups *fx));
static void write_1_short PARAMS ((struct d30v_insn *opcode, long long insn,
				   Fixups *fx, int use_sequential));
static int write_2_short PARAMS ((struct d30v_insn *opcode1, long long insn1,
		   struct d30v_insn *opcode2, long long insn2, exec_type_enum exec_type, Fixups *fx));
static long long do_assemble PARAMS ((char *str, struct d30v_insn *opcode,
				      int shortp, int is_parallel));
static int parallel_ok PARAMS ((struct d30v_insn *opcode1, unsigned long insn1,
				struct d30v_insn *opcode2, unsigned long insn2,
				exec_type_enum exec_type));
static void d30v_number_to_chars PARAMS ((char *buf, long long value, int nbytes));
static void check_size PARAMS ((long value, int bits, char *file, int line));
static void d30v_align PARAMS ((int, char *, symbolS *));
static void s_d30v_align PARAMS ((int));
static void s_d30v_text PARAMS ((int));
static void s_d30v_data PARAMS ((int));
static void s_d30v_section PARAMS ((int));

struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word", cons, 4 },
  { "hword", cons, 2 },
  { "align", s_d30v_align, 0 },
  { "text", s_d30v_text, 0 },
  { "data", s_d30v_data, 0 },
  { "section", s_d30v_section, 0 },
  { "section.s", s_d30v_section, 0 },
  { "sect", s_d30v_section, 0 },
  { "sect.s", s_d30v_section, 0 },
  { NULL, NULL, 0 }
};

/* Opcode hash table.  */
static struct hash_control *d30v_hash;

/* Do a binary search of the pre_defined_registers array to see if
   NAME is a valid regiter name.  Return the register number from the
   array on success, or -1 on failure.  */

static int
reg_name_search (name)
     char *name;
{
  int middle, low, high;
  int cmp;

  low = 0;
  high = reg_name_cnt () - 1;

  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, pre_defined_registers[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else
	{
	  if (symbol_find (name) != NULL)
	    {
	      if (warn_register_name_conflicts)
		as_warn (_("Register name %s conflicts with symbol of the same name"),
			 name);
	    }

	  return pre_defined_registers[middle].value;
	}
    }
  while (low <= high);

  return -1;
}

/* Check the string at input_line_pointer to see if it is a valid
   register name.  */

static int
register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char c, *p = input_line_pointer;

  while (*p && *p != '\n' && *p != '\r' && *p != ',' && *p != ' ' && *p != ')')
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
check_range (num, bits, flags)
     unsigned long num;
     int bits;
     int flags;
{
  long min, max;

  /* Don't bother checking 32-bit values.  */
  if (bits == 32)
    {
      if (sizeof (unsigned long) * CHAR_BIT == 32)
	return 0;

      /* We don't record signed or unsigned for 32-bit quantities.
	 Allow either.  */
      min = -((unsigned long) 1 << (bits - 1));
      max = ((unsigned long) 1 << bits) - 1;
      return (long) num < min || (long) num > max;
    }

  if (flags & OPERAND_SHIFT)
    {
      /* We know that all shifts are right by three bits.  */
      num >>= 3;

      if (flags & OPERAND_SIGNED)
	{
	  unsigned long sign_bit = ((unsigned long) -1L >> 4) + 1;
	  num = (num ^ sign_bit) - sign_bit;
	}
    }

  if (flags & OPERAND_SIGNED)
    {
      max = ((unsigned long) 1 << (bits - 1)) - 1;
      min = - ((unsigned long) 1 << (bits - 1));
      return (long) num > max || (long) num < min;
    }
  else
    {
      max = ((unsigned long) 1 << bits) - 1;
      return num > (unsigned long) max;
    }
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\nD30V options:\n\
-O                      Make adjacent short instructions parallel if possible.\n\
-n                      Warn about all NOPs inserted by the assembler.\n\
-N			Warn about NOPs inserted after word multiplies.\n\
-c                      Warn about symbols whoes names match register names.\n\
-C                      Opposite of -C.  -c is the default.\n"));
}

int
md_parse_option (c, arg)
     int c;
     char *arg ATTRIBUTE_UNUSED;
{
  switch (c)
    {
      /* Optimize.  Will attempt to parallelize operations.  */
    case 'O':
      Optimizing = 1;
      break;

      /* Warn about all NOPS that the assembler inserts.  */
    case 'n':
      warn_nops = NOP_ALL;
      break;

      /* Warn about the NOPS that the assembler inserts because of the
	 multiply hazard.  */
    case 'N':
      warn_nops = NOP_MULTIPLY;
      break;

    case 'c':
      warn_register_name_conflicts = 1;
      break;

    case 'C':
      warn_register_name_conflicts = 0;
      break;

    default:
      return 0;
    }
  return 1;
}

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
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
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
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
  struct d30v_opcode *opcode;
  d30v_hash = hash_new ();

  /* Insert opcode names into a hash table.  */
  for (opcode = (struct d30v_opcode *) d30v_opcode_table; opcode->name; opcode++)
      hash_insert (d30v_hash, opcode->name, (char *) opcode);

  fixups = &FixUps[0];
  FixUps[0].next = &FixUps[1];
  FixUps[1].next = &FixUps[0];

  d30v_current_align_seg = now_seg;
}

/* Remove the postincrement or postdecrement operator ( '+' or '-' )
   from an expression.  */

static int
postfix (p)
     char *p;
{
  while (*p != '-' && *p != '+')
    {
      if (*p == 0 || *p == '\n' || *p == '\r' || *p == ' ' || *p == ',')
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
get_reloc (op, rel_flag)
     struct d30v_operand *op;
     int rel_flag;
{
  switch (op->bits)
    {
    case 6:
      if (op->flags & OPERAND_SHIFT)
	return BFD_RELOC_D30V_9_PCREL;
      else
	return BFD_RELOC_D30V_6;
      break;
    case 12:
      if (!(op->flags & OPERAND_SHIFT))
	as_warn (_("unexpected 12-bit reloc type"));
      if (rel_flag == RELOC_PCREL)
	return BFD_RELOC_D30V_15_PCREL;
      else
	return BFD_RELOC_D30V_15;
    case 18:
      if (!(op->flags & OPERAND_SHIFT))
	as_warn (_("unexpected 18-bit reloc type"));
      if (rel_flag == RELOC_PCREL)
	return BFD_RELOC_D30V_21_PCREL;
      else
	return BFD_RELOC_D30V_21;
    case 32:
      if (rel_flag == RELOC_PCREL)
	return BFD_RELOC_D30V_32_PCREL;
      else
	return BFD_RELOC_D30V_32;
    default:
      return 0;
    }
}

/* Parse a string of operands and return an array of expressions.  */

static int
get_operands (exp, cmp_hack)
     expressionS exp[];
     int cmp_hack;
{
  char *p = input_line_pointer;
  int numops = 0;
  int post = 0;

  if (cmp_hack)
    {
      exp[numops].X_op = O_absent;
      exp[numops++].X_add_number = cmp_hack - 1;
    }

  while (*p)
    {
      while (*p == ' ' || *p == '\t' || *p == ',')
	p++;

      if (*p == 0 || *p == '\n' || *p == '\r')
	break;

      if (*p == '@')
	{
	  p++;
	  exp[numops].X_op = O_absent;
	  if (*p == '(')
	    {
	      p++;
	      exp[numops].X_add_number = OPERAND_ATPAR;
	      post = postfix (p);
	    }
	  else if (*p == '-')
	    {
	      p++;
	      exp[numops].X_add_number = OPERAND_ATMINUS;
	    }
	  else
	    {
	      exp[numops].X_add_number = OPERAND_ATSIGN;
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
	  expression (&exp[numops]);
	}

      if (exp[numops].X_op == O_illegal)
	as_bad (_("illegal operand"));
      else if (exp[numops].X_op == O_absent)
	as_bad (_("missing operand"));

      numops++;
      p = input_line_pointer;

      switch (post)
	{
	case -1:
	  /* Postdecrement mode.  */
	  exp[numops].X_op = O_absent;
	  exp[numops++].X_add_number = OPERAND_MINUS;
	  break;
	case 1:
	  /* Postincrement mode.  */
	  exp[numops].X_op = O_absent;
	  exp[numops++].X_add_number = OPERAND_PLUS;
	  break;
	}
      post = 0;
    }

  exp[numops].X_op = 0;

  return numops;
}

/* Generate the instruction.
   It does everything but write the FM bits.  */

static long long
build_insn (opcode, opers)
     struct d30v_insn *opcode;
     expressionS *opers;
{
  int i, length, bits, shift, flags;
  unsigned long number, id = 0;
  long long insn;
  struct d30v_opcode *op = opcode->op;
  struct d30v_format *form = opcode->form;

  insn =
    opcode->ecc << 28 | op->op1 << 25 | op->op2 << 20 | form->modifier << 18;

  for (i = 0; form->operands[i]; i++)
    {
      flags = d30v_operand_table[form->operands[i]].flags;

      /* Must be a register or number.  */
      if (!(flags & OPERAND_REG) && !(flags & OPERAND_NUM)
	  && !(flags & OPERAND_NAME) && !(flags & OPERAND_SPECIAL))
	continue;

      bits = d30v_operand_table[form->operands[i]].bits;
      if (flags & OPERAND_SHIFT)
	bits += 3;

      length = d30v_operand_table[form->operands[i]].length;
      shift = 12 - d30v_operand_table[form->operands[i]].position;
      if (opers[i].X_op != O_symbol)
	number = opers[i].X_add_number;
      else
	number = 0;
      if (flags & OPERAND_REG)
	{
	  /* Check for mvfsys or mvtsys control registers.  */
	  if (flags & OPERAND_CONTROL && (number & 0x7f) > MAX_CONTROL_REG)
	    {
	      /* PSWL or PSWH.  */
	      id = (number & 0x7f) - MAX_CONTROL_REG;
	      number = 0;
	    }
	  else if (number & OPERAND_FLAG)
	    {
	      /* NUMBER is a flag register.  */
	      id = 3;
	    }
	  number &= 0x7F;
	}
      else if (flags & OPERAND_SPECIAL)
	{
	  number = id;
	}

      if (opers[i].X_op != O_register && opers[i].X_op != O_constant
	  && !(flags & OPERAND_NAME))
	{
	  /* Now create a fixup.  */
	  if (fixups->fc >= MAX_INSN_FIXUPS)
	    as_fatal (_("too many fixups"));

	  fixups->fix[fixups->fc].reloc =
	    get_reloc ((struct d30v_operand *) &d30v_operand_table[form->operands[i]], op->reloc_flag);
	  fixups->fix[fixups->fc].size = 4;
	  fixups->fix[fixups->fc].exp = opers[i];
	  fixups->fix[fixups->fc].operand = form->operands[i];
	  if (fixups->fix[fixups->fc].reloc == BFD_RELOC_D30V_9_PCREL)
	    fixups->fix[fixups->fc].pcrel = RELOC_PCREL;
	  else
	    fixups->fix[fixups->fc].pcrel = op->reloc_flag;
	  (fixups->fc)++;
	}

      /* Truncate to the proper number of bits.  */
      if ((opers[i].X_op == O_constant) && check_range (number, bits, flags))
	as_bad (_("operand out of range: %ld"), number);
      if (bits < 31)
	number &= 0x7FFFFFFF >> (31 - bits);
      if (flags & OPERAND_SHIFT)
	number >>= 3;
      if (bits == 32)
	{
	  /* It's a LONG instruction.  */
	  insn |= ((number & 0xffffffff) >> 26);	/* top 6 bits */
	  insn <<= 32;			/* shift the first word over */
	  insn |= ((number & 0x03FC0000) << 2);		/* next 8 bits */
	  insn |= number & 0x0003FFFF;			/* bottom 18 bits */
	}
      else
	insn |= number << shift;
    }

  return insn;
}

/* Write out a long form instruction.  */

static void
write_long (opcode, insn, fx)
     struct d30v_insn *opcode ATTRIBUTE_UNUSED;
     long long insn;
     Fixups *fx;
{
  int i, where;
  char *f = frag_more (8);

  insn |= FM11;
  d30v_number_to_chars (f, insn, 8);

  for (i = 0; i < fx->fc; i++)
    {
      if (fx->fix[i].reloc)
	{
	  where = f - frag_now->fr_literal;
	  fix_new_exp (frag_now,
		       where,
		       fx->fix[i].size,
		       &(fx->fix[i].exp),
		       fx->fix[i].pcrel,
		       fx->fix[i].reloc);
	}
    }

  fx->fc = 0;
}

/* Write out a short form instruction by itself.  */

static void
write_1_short (opcode, insn, fx, use_sequential)
     struct d30v_insn *opcode;
     long long insn;
     Fixups *fx;
     int use_sequential;
{
  char *f = frag_more (8);
  int i, where;

  if (warn_nops == NOP_ALL)
    as_warn (_("%s NOP inserted"), use_sequential ?
	     _("sequential") : _("parallel"));

  /* The other container needs to be NOP.  */
  if (use_sequential)
    {
      /* Use a sequential NOP rather than a parallel one,
	 as the current instruction is a FLAG_MUL32 type one
	 and the next instruction is a load.  */

      /* According to 4.3.1: for FM=01, sub-instructions performed
	 only by IU cannot be encoded in L-container.  */

      if (opcode->op->unit == IU)
	/* Right then left.  */
	insn |= FM10 | NOP_LEFT;
      else
	/* Left then right.  */
	insn = FM01 | (insn << 32) | NOP_RIGHT;
    }
  else
    {
      /* According to 4.3.1: for FM=00, sub-instructions performed
	 only by IU cannot be encoded in L-container.  */

      if (opcode->op->unit == IU)
	/* Right container.  */
	insn |= FM00 | NOP_LEFT;
      else
	/* Left container.  */
	insn = FM00 | (insn << 32) | NOP_RIGHT;
    }

  d30v_number_to_chars (f, insn, 8);

  for (i = 0; i < fx->fc; i++)
    {
      if (fx->fix[i].reloc)
	{
	  where = f - frag_now->fr_literal;
	  fix_new_exp (frag_now,
		       where,
		       fx->fix[i].size,
		       &(fx->fix[i].exp),
		       fx->fix[i].pcrel,
		       fx->fix[i].reloc);
	}
    }

  fx->fc = 0;
}

/* Write out a short form instruction if possible.
   Return number of instructions not written out.  */

static int
write_2_short (opcode1, insn1, opcode2, insn2, exec_type, fx)
     struct d30v_insn *opcode1, *opcode2;
     long long insn1, insn2;
     exec_type_enum exec_type;
     Fixups *fx;
{
  long long insn = NOP2;
  char *f;
  int i, j, where;

  if (exec_type == EXEC_SEQ
      && (opcode1->op->flags_used & (FLAG_JMP | FLAG_JSR))
      && ((opcode1->op->flags_used & FLAG_DELAY) == 0)
      && ((opcode1->ecc == ECC_AL) || ! Optimizing))
    {
      /* Unconditional, non-delayed branches kill instructions in
	 the right bin.  Conditional branches don't always but if
	 we are not optimizing, then we have been asked to produce
	 an error about such constructs.  For the purposes of this
	 test, subroutine calls are considered to be branches.  */
      write_1_short (opcode1, insn1, fx->next, FALSE);
      return 1;
    }

  /* Note: we do not have to worry about subroutine calls occurring
     in the right hand container.  The return address is always
     aligned to the next 64 bit boundary, be that 64 or 32 bit away.  */
  switch (exec_type)
    {
    case EXEC_UNKNOWN:	/* Order not specified.  */
      if (Optimizing
	  && parallel_ok (opcode1, insn1, opcode2, insn2, exec_type)
	  && ! (   (opcode1->op->unit == EITHER_BUT_PREFER_MU
		 || opcode1->op->unit == MU)
		&&
		(   opcode2->op->unit == EITHER_BUT_PREFER_MU
		 || opcode2->op->unit == MU)))
	{
	  /* Parallel.  */
	  exec_type = EXEC_PARALLEL;

	  if (opcode1->op->unit == IU
	      || opcode2->op->unit == MU
	      || opcode2->op->unit == EITHER_BUT_PREFER_MU)
	    insn = FM00 | (insn2 << 32) | insn1;
	  else
	    {
	      insn = FM00 | (insn1 << 32) | insn2;
	      fx = fx->next;
	    }
	}
      else if ((opcode1->op->flags_used & (FLAG_JMP | FLAG_JSR)
		&& ((opcode1->op->flags_used & FLAG_DELAY) == 0))
	       || opcode1->op->flags_used & FLAG_RP)
	{
	  /* We must emit (non-delayed) branch type instructions
	     on their own with nothing in the right container.  */
	  /* We must treat repeat instructions likewise, since the
	     following instruction has to be separate from the repeat
	     in order to be repeated.  */
	  write_1_short (opcode1, insn1, fx->next, FALSE);
	  return 1;
	}
      else if (prev_left_kills_right_p)
	{
	  /* The left instruction kils the right slot, so we
	     must leave it empty.  */
	  write_1_short (opcode1, insn1, fx->next, FALSE);
	  return 1;
	}
      else if (opcode1->op->unit == IU)
	{
	  if (opcode2->op->unit == EITHER_BUT_PREFER_MU)
	    {
	      /* Case 103810 is a request from Mitsubishi that opcodes
		 with EITHER_BUT_PREFER_MU should not be executed in
		 reverse sequential order.  */
	      write_1_short (opcode1, insn1, fx->next, FALSE);
	      return 1;
	    }

	  /* Reverse sequential.  */
	  insn = FM10 | (insn2 << 32) | insn1;
	  exec_type = EXEC_REVSEQ;
	}
      else
	{
	  /* Sequential.  */
	  insn = FM01 | (insn1 << 32) | insn2;
	  fx = fx->next;
	  exec_type = EXEC_SEQ;
	}
      break;

    case EXEC_PARALLEL:	/* Parallel.  */
      flag_explicitly_parallel = flag_xp_state;
      if (! parallel_ok (opcode1, insn1, opcode2, insn2, exec_type))
	as_bad (_("Instructions may not be executed in parallel"));
      else if (opcode1->op->unit == IU)
	{
	  if (opcode2->op->unit == IU)
	    as_bad (_("Two IU instructions may not be executed in parallel"));
	  as_warn (_("Swapping instruction order"));
	  insn = FM00 | (insn2 << 32) | insn1;
	}
      else if (opcode2->op->unit == MU)
	{
	  if (opcode1->op->unit == MU)
	    as_bad (_("Two MU instructions may not be executed in parallel"));
	  else if (opcode1->op->unit == EITHER_BUT_PREFER_MU)
	    as_warn (_("Executing %s in IU may not work"), opcode1->op->name);
	  as_warn (_("Swapping instruction order"));
	  insn = FM00 | (insn2 << 32) | insn1;
	}
      else
	{
	  if (opcode2->op->unit == EITHER_BUT_PREFER_MU)
	    as_warn (_("Executing %s in IU may not work in parallel execution"),
		     opcode2->op->name);

	  insn = FM00 | (insn1 << 32) | insn2;
	  fx = fx->next;
	}
      flag_explicitly_parallel = 0;
      break;

    case EXEC_SEQ:	/* Sequential.  */
      if (opcode1->op->unit == IU)
	as_bad (_("IU instruction may not be in the left container"));
      if (prev_left_kills_right_p)
	as_bad (_("special left instruction `%s' kills instruction "
		  "`%s' in right container"),
		opcode1->op->name, opcode2->op->name);
      insn = FM01 | (insn1 << 32) | insn2;
      fx = fx->next;
      break;

    case EXEC_REVSEQ:	/* Reverse sequential.  */
      if (opcode2->op->unit == MU)
	as_bad (_("MU instruction may not be in the right container"));
      if (opcode1->op->unit == EITHER_BUT_PREFER_MU)
	as_warn (_("Executing %s in reverse serial with %s may not work"),
		 opcode1->op->name, opcode2->op->name);
      else if (opcode2->op->unit == EITHER_BUT_PREFER_MU)
	as_warn (_("Executing %s in IU in reverse serial may not work"),
		 opcode2->op->name);
      insn = FM10 | (insn1 << 32) | insn2;
      fx = fx->next;
      break;

    default:
      as_fatal (_("unknown execution type passed to write_2_short()"));
    }

#if 0
  printf ("writing out %llx\n", insn);
#endif
  f = frag_more (8);
  d30v_number_to_chars (f, insn, 8);

  /* If the previous instruction was a 32-bit multiply but it is put into a
     parallel container, mark the current instruction as being a 32-bit
     multiply.  */
  if (prev_mul32_p && exec_type == EXEC_PARALLEL)
    cur_mul32_p = 1;

  for (j = 0; j < 2; j++)
    {
      for (i = 0; i < fx->fc; i++)
	{
	  if (fx->fix[i].reloc)
	    {
	      where = (f - frag_now->fr_literal) + 4 * j;

	      fix_new_exp (frag_now,
			   where,
			   fx->fix[i].size,
			   &(fx->fix[i].exp),
			   fx->fix[i].pcrel,
			   fx->fix[i].reloc);
	    }
	}

      fx->fc = 0;
      fx = fx->next;
    }

  return 0;
}

/* Check 2 instructions and determine if they can be safely
   executed in parallel.  Return 1 if they can be.  */

static int
parallel_ok (op1, insn1, op2, insn2, exec_type)
     struct d30v_insn *op1, *op2;
     unsigned long insn1, insn2;
     exec_type_enum exec_type;
{
  int i, j, shift, regno, bits, ecc;
  unsigned long flags, mask, flags_set1, flags_set2, flags_used1, flags_used2;
  unsigned long ins, mod_reg[2][3], used_reg[2][3], flag_reg[2];
  struct d30v_format *f;
  struct d30v_opcode *op;

  /* Section 4.3: Both instructions must not be IU or MU only.  */
  if ((op1->op->unit == IU && op2->op->unit == IU)
      || (op1->op->unit == MU && op2->op->unit == MU))
    return 0;

  /* First instruction must not be a jump to safely optimize, unless this
     is an explicit parallel operation.  */
  if (exec_type != EXEC_PARALLEL
      && (op1->op->flags_used & (FLAG_JMP | FLAG_JSR)))
    return 0;

  /* If one instruction is /TX or /XT and the other is /FX or /XF respectively,
     then it is safe to allow the two to be done as parallel ops, since only
     one will ever be executed at a time.  */
  if ((op1->ecc == ECC_TX && op2->ecc == ECC_FX)
      || (op1->ecc == ECC_FX && op2->ecc == ECC_TX)
      || (op1->ecc == ECC_XT && op2->ecc == ECC_XF)
      || (op1->ecc == ECC_XF && op2->ecc == ECC_XT))
    return 1;

  /* [0] r0-r31
     [1] r32-r63
     [2] a0, a1, flag registers.  */
  for (j = 0; j < 2; j++)
    {
      if (j == 0)
	{
	  f = op1->form;
	  op = op1->op;
	  ecc = op1->ecc;
	  ins = insn1;
	}
      else
	{
	  f = op2->form;
	  op = op2->op;
	  ecc = op2->ecc;
	  ins = insn2;
	}

      flag_reg[j] = 0;
      mod_reg[j][0] = mod_reg[j][1] = 0;
      used_reg[j][0] = used_reg[j][1] = 0;

      if (flag_explicitly_parallel)
	{
	  /* For human specified parallel instructions we have been asked
	     to ignore the possibility that both instructions could modify
	     bits in the PSW, so we initialise the mod & used arrays to 0.
	     We have been asked, however, to refuse to allow parallel
	     instructions which explicitly set the same flag register,
	     eg "cmpne f0,r1,0x10 || cmpeq f0, r5, 0x2", so further on we test
	     for the use of a flag register and set a bit in the mod or used
	     array appropriately.  */
	  mod_reg[j][2]  = 0;
	  used_reg[j][2] = 0;
	}
      else
	{
	  mod_reg[j][2] = (op->flags_set & FLAG_ALL);
	  used_reg[j][2] = (op->flags_used & FLAG_ALL);
	}

      /* BSR/JSR always sets R62.  */
      if (op->flags_used & FLAG_JSR)
	mod_reg[j][1] = (1L << (62 - 32));

      /* Conditional execution affects the flags_used.  */
      switch (ecc)
	{
	case ECC_TX:
	case ECC_FX:
	  used_reg[j][2] |= flag_reg[j] = FLAG_0;
	  break;

	case ECC_XT:
	case ECC_XF:
	  used_reg[j][2] |= flag_reg[j] = FLAG_1;
	  break;

	case ECC_TT:
	case ECC_TF:
	  used_reg[j][2] |= flag_reg[j] = (FLAG_0 | FLAG_1);
	  break;
	}

      for (i = 0; f->operands[i]; i++)
	{
	  flags = d30v_operand_table[f->operands[i]].flags;
	  shift = 12 - d30v_operand_table[f->operands[i]].position;
	  bits = d30v_operand_table[f->operands[i]].bits;
	  if (bits == 32)
	    mask = 0xffffffff;
	  else
	    mask = 0x7FFFFFFF >> (31 - bits);

	  if ((flags & OPERAND_PLUS) || (flags & OPERAND_MINUS))
	    {
	      /* This is a post-increment or post-decrement.
		 The previous register needs to be marked as modified.  */
	      shift = 12 - d30v_operand_table[f->operands[i - 1]].position;
	      regno = (ins >> shift) & 0x3f;
	      if (regno >= 32)
		mod_reg[j][1] |= 1L << (regno - 32);
	      else
		mod_reg[j][0] |= 1L << regno;
	    }
	  else if (flags & OPERAND_REG)
	    {
	      regno = (ins >> shift) & mask;
	      /* The memory write functions don't have a destination
                 register.  */
	      if ((flags & OPERAND_DEST) && !(op->flags_set & FLAG_MEM))
		{
		  /* MODIFIED registers and flags.  */
		  if (flags & OPERAND_ACC)
		    {
		      if (regno == 0)
			mod_reg[j][2] |= FLAG_A0;
		      else if (regno == 1)
			mod_reg[j][2] |= FLAG_A1;
		      else
			abort ();
		    }
		  else if (flags & OPERAND_FLAG)
		    mod_reg[j][2] |= 1L << regno;
		  else if (!(flags & OPERAND_CONTROL))
		    {
		      int r, z;

		      /* Need to check if there are two destination
			 registers, for example ld2w.  */
		      if (flags & OPERAND_2REG)
			z = 1;
		      else
			z = 0;

		      for (r = regno; r <= regno + z; r++)
			{
			  if (r >= 32)
			    mod_reg[j][1] |= 1L << (r - 32);
			  else
			    mod_reg[j][0] |= 1L << r;
			}
		    }
		}
	      else
		{
		  /* USED, but not modified registers and flags.  */
		  if (flags & OPERAND_ACC)
		    {
		      if (regno == 0)
			used_reg[j][2] |= FLAG_A0;
		      else if (regno == 1)
			used_reg[j][2] |= FLAG_A1;
		      else
			abort ();
		    }
		  else if (flags & OPERAND_FLAG)
		    used_reg[j][2] |= 1L << regno;
		  else if (!(flags & OPERAND_CONTROL))
		    {
		      int r, z;

		      /* Need to check if there are two source
			 registers, for example st2w.  */
		      if (flags & OPERAND_2REG)
			z = 1;
		      else
			z = 0;

		      for (r = regno; r <= regno + z; r++)
			{
			  if (r >= 32)
			    used_reg[j][1] |= 1L << (r - 32);
			  else
			    used_reg[j][0] |= 1L << r;
			}
		    }
		}
	    }
	}
    }

  flags_set1 = op1->op->flags_set;
  flags_set2 = op2->op->flags_set;
  flags_used1 = op1->op->flags_used;
  flags_used2 = op2->op->flags_used;

  /* Check for illegal combinations with ADDppp/SUBppp.  */
  if (((flags_set1 & FLAG_NOT_WITH_ADDSUBppp) != 0
       && (flags_used2 & FLAG_ADDSUBppp) != 0)
      || ((flags_set2 & FLAG_NOT_WITH_ADDSUBppp) != 0
	  && (flags_used1 & FLAG_ADDSUBppp) != 0))
    return 0;

  /* Load instruction combined with half-word multiply is illegal.  */
  if (((flags_used1 & FLAG_MEM) != 0 && (flags_used2 & FLAG_MUL16))
      || ((flags_used2 & FLAG_MEM) != 0 && (flags_used1 & FLAG_MUL16)))
    return 0;

  /* Specifically allow add || add by removing carry, overflow bits dependency.
     This is safe, even if an addc follows since the IU takes the argument in
     the right container, and it writes its results last.
     However, don't paralellize add followed by addc or sub followed by
     subb.  */
  if (mod_reg[0][2] == FLAG_CVVA && mod_reg[1][2] == FLAG_CVVA
      && (used_reg[0][2] & ~flag_reg[0]) == 0
      && (used_reg[1][2] & ~flag_reg[1]) == 0
      && op1->op->unit == EITHER && op2->op->unit == EITHER)
    {
      mod_reg[0][2] = mod_reg[1][2] = 0;
    }

  for (j = 0; j < 3; j++)
    {
      /* If the second instruction depends on the first, we obviously
	 cannot parallelize.  Note, the mod flag implies use, so
	 check that as well.  */
      /* If flag_explicitly_parallel is set, then the case of the
	 second instruction using a register the first instruction
	 modifies is assumed to be okay; we trust the human.  We
	 don't trust the human if both instructions modify the same
	 register but we do trust the human if they modify the same
	 flags.  */
      /* We have now been requested not to trust the human if the
	 instructions modify the same flag registers either.  */
      if (flag_explicitly_parallel)
	{
	  if ((mod_reg[0][j] & mod_reg[1][j]) != 0)
	    return 0;
	}
      else
	if ((mod_reg[0][j] & (mod_reg[1][j] | used_reg[1][j])) != 0)
	  return 0;
    }

  return 1;
}

/* This is the main entry point for the machine-dependent assembler.
   STR points to a machine-dependent instruction.  This function is
   supposed to emit the frags/bytes it assembles to.  For the D30V, it
   mostly handles the special VLIW parsing and packing and leaves the
   difficult stuff to do_assemble ().  */

static long long prev_insn = -1;
static struct d30v_insn prev_opcode;
static subsegT prev_subseg;
static segT prev_seg = 0;

void
md_assemble (str)
     char *str;
{
  struct d30v_insn opcode;
  long long insn;
  /* Execution type; parallel, etc.  */
  exec_type_enum extype = EXEC_UNKNOWN;
  /* Saved extype.  Used for multiline instructions.  */
  static exec_type_enum etype = EXEC_UNKNOWN;
  char *str2;

  if ((prev_insn != -1) && prev_seg
      && ((prev_seg != now_seg) || (prev_subseg != now_subseg)))
    d30v_cleanup (FALSE);

  if (d30v_current_align < 3)
    d30v_align (3, NULL, d30v_last_label);
  else if (d30v_current_align > 3)
    d30v_current_align = 3;
  d30v_last_label = NULL;

  flag_explicitly_parallel = 0;
  flag_xp_state = 0;
  if (etype == EXEC_UNKNOWN)
    {
      /* Look for the special multiple instruction separators.  */
      str2 = strstr (str, "||");
      if (str2)
	{
	  extype = EXEC_PARALLEL;
	  flag_xp_state = 1;
	}
      else
	{
	  str2 = strstr (str, "->");
	  if (str2)
	    extype = EXEC_SEQ;
	  else
	    {
	      str2 = strstr (str, "<-");
	      if (str2)
		extype = EXEC_REVSEQ;
	    }
	}

      /* STR2 points to the separator, if one.  */
      if (str2)
	{
	  *str2 = 0;

	  /* If two instructions are present and we already have one saved,
	     then first write it out.  */
	  d30v_cleanup (FALSE);

	  /* Assemble first instruction and save it.  */
	  prev_insn = do_assemble (str, &prev_opcode, 1, 0);
	  if (prev_insn == -1)
	    as_bad (_("Cannot assemble instruction"));
	  if (prev_opcode.form != NULL && prev_opcode.form->form >= LONG)
	    as_bad (_("First opcode is long.  Unable to mix instructions as specified."));
	  fixups = fixups->next;
	  str = str2 + 2;
	  prev_seg = now_seg;
	  prev_subseg = now_subseg;
	}
    }

  insn = do_assemble (str, &opcode,
		      (extype != EXEC_UNKNOWN || etype != EXEC_UNKNOWN),
		      extype == EXEC_PARALLEL);
  if (insn == -1)
    {
      if (extype != EXEC_UNKNOWN)
	etype = extype;
      as_bad (_("Cannot assemble instruction"));
      return;
    }

  if (etype != EXEC_UNKNOWN)
    {
      extype = etype;
      etype = EXEC_UNKNOWN;
    }

  /* Word multiply instructions must not be followed by either a load or a
     16-bit multiply instruction in the next cycle.  */
  if (   (extype != EXEC_REVSEQ)
      && prev_mul32_p
      && (opcode.op->flags_used & (FLAG_MEM | FLAG_MUL16)))
    {
      /* However, load and multiply should able to be combined in a parallel
	 operation, so check for that first.  */
      if (prev_insn != -1
	  && (opcode.op->flags_used & FLAG_MEM)
	  && opcode.form->form < LONG
	  && (extype == EXEC_PARALLEL || (Optimizing && extype == EXEC_UNKNOWN))
	  && parallel_ok (&prev_opcode, (long) prev_insn,
			  &opcode, (long) insn, extype)
	  && write_2_short (&prev_opcode, (long) prev_insn,
			    &opcode, (long) insn, extype, fixups) == 0)
	{
	  /* No instructions saved.  */
	  prev_insn = -1;
	  return;
	}
      else
	{
	  /* Can't parallelize, flush previous instruction and emit a
	     word of NOPS, unless the previous instruction is a NOP,
	     in which case just flush it, as this will generate a word
	     of NOPs for us.  */

	  if (prev_insn != -1 && (strcmp (prev_opcode.op->name, "nop") == 0))
	    d30v_cleanup (FALSE);
	  else
	    {
	      char *f;

	      if (prev_insn != -1)
		d30v_cleanup (TRUE);
	      else
		{
		  f = frag_more (8);
		  d30v_number_to_chars (f, NOP2, 8);

		  if (warn_nops == NOP_ALL || warn_nops == NOP_MULTIPLY)
		    {
		      if (opcode.op->flags_used & FLAG_MEM)
			as_warn (_("word of NOPs added between word multiply and load"));
		      else
			as_warn (_("word of NOPs added between word multiply and 16-bit multiply"));
		    }
		}
	    }

	  extype = EXEC_UNKNOWN;
	}
    }
  else if (   (extype == EXEC_REVSEQ)
	   && cur_mul32_p
	   && (prev_opcode.op->flags_used & (FLAG_MEM | FLAG_MUL16)))
    {
      /* Can't parallelize, flush current instruction and add a
         sequential NOP.  */
      write_1_short (&opcode, (long) insn, fixups->next->next, TRUE);

      /* Make the previous instruction the current one.  */
      extype = EXEC_UNKNOWN;
      insn = prev_insn;
      now_seg = prev_seg;
      now_subseg = prev_subseg;
      prev_insn = -1;
      cur_mul32_p = prev_mul32_p;
      prev_mul32_p = 0;
      memcpy (&opcode, &prev_opcode, sizeof (prev_opcode));
    }

  /* If this is a long instruction, write it and any previous short
     instruction.  */
  if (opcode.form->form >= LONG)
    {
      if (extype != EXEC_UNKNOWN)
	as_bad (_("Instruction uses long version, so it cannot be mixed as specified"));
      d30v_cleanup (FALSE);
      write_long (&opcode, insn, fixups);
      prev_insn = -1;
    }
  else if ((prev_insn != -1)
	   && (write_2_short
	       (&prev_opcode, (long) prev_insn, &opcode,
		(long) insn, extype, fixups) == 0))
    {
      /* No instructions saved.  */
      prev_insn = -1;
    }
  else
    {
      if (extype != EXEC_UNKNOWN)
	as_bad (_("Unable to mix instructions as specified"));

      /* Save off last instruction so it may be packed on next pass.  */
      memcpy (&prev_opcode, &opcode, sizeof (prev_opcode));
      prev_insn = insn;
      prev_seg = now_seg;
      prev_subseg = now_subseg;
      fixups = fixups->next;
      prev_mul32_p = cur_mul32_p;
    }
}

/* Assemble a single instruction and return an opcode.
   Return -1 (an invalid opcode) on error.  */

#define NAME_BUF_LEN	20

static long long
do_assemble (str, opcode, shortp, is_parallel)
     char *str;
     struct d30v_insn *opcode;
     int shortp;
     int is_parallel;
{
  unsigned char *op_start;
  unsigned char *save;
  unsigned char *op_end;
  char           name[NAME_BUF_LEN];
  int            cmp_hack;
  int            nlen = 0;
  int            fsize = (shortp ? FORCE_SHORT : 0);
  expressionS    myops[6];
  long long      insn;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the opcode end.  */
  for (op_start = op_end = (unsigned char *) (str);
       *op_end
       && nlen < (NAME_BUF_LEN - 1)
       && *op_end != '/'
       && !is_end_of_line[*op_end] && *op_end != ' ';
       op_end++)
    {
      name[nlen] = TOLOWER (op_start[nlen]);
      nlen++;
    }

  if (nlen == 0)
    return -1;

  name[nlen] = 0;

  /* If there is an execution condition code, handle it.  */
  if (*op_end == '/')
    {
      int i = 0;
      while ((i < ECC_MAX) && strncasecmp (d30v_ecc_names[i], op_end + 1, 2))
	i++;

      if (i == ECC_MAX)
	{
	  char tmp[4];
	  strncpy (tmp, op_end + 1, 2);
	  tmp[2] = 0;
	  as_bad (_("unknown condition code: %s"), tmp);
	  return -1;
	}
#if 0
      printf ("condition code=%d\n", i);
#endif
      opcode->ecc = i;
      op_end += 3;
    }
  else
    opcode->ecc = ECC_AL;

  /* CMP and CMPU change their name based on condition codes.  */
  if (!strncmp (name, "cmp", 3))
    {
      int p, i;
      char **str = (char **) d30v_cc_names;
      if (name[3] == 'u')
	p = 4;
      else
	p = 3;

      for (i = 1; *str && strncmp (*str, &name[p], 2); i++, str++)
	;

      /* cmpu only supports some condition codes.  */
      if (p == 4)
	{
	  if (i < 3 || i > 6)
	    {
	      name[p + 2] = 0;
	      as_bad (_("cmpu doesn't support condition code %s"), &name[p]);
	    }
	}

      if (!*str)
	{
	  name[p + 2] = 0;
	  as_bad (_("unknown condition code: %s"), &name[p]);
	}

      cmp_hack = i;
      name[p] = 0;
    }
  else
    cmp_hack = 0;

#if 0
  printf ("cmp_hack=%d\n", cmp_hack);
#endif

  /* Need to look for .s or .l.  */
  if (name[nlen - 2] == '.')
    {
      switch (name[nlen - 1])
	{
	case 's':
	  fsize = FORCE_SHORT;
	  break;
	case 'l':
	  fsize = FORCE_LONG;
	  break;
	}
      name[nlen - 2] = 0;
    }

  /* Find the first opcode with the proper name.  */
  opcode->op = (struct d30v_opcode *) hash_find (d30v_hash, name);
  if (opcode->op == NULL)
    {
      as_bad (_("unknown opcode: %s"), name);
      return -1;
    }

  save = input_line_pointer;
  input_line_pointer = op_end;
  while (!(opcode->form = find_format (opcode->op, myops, fsize, cmp_hack)))
    {
      opcode->op++;
      if (opcode->op->name == NULL || strcmp (opcode->op->name, name))
	{
	  as_bad (_("operands for opcode `%s' do not match any valid format"),
		  name);
	  return -1;
	}
    }
  input_line_pointer = save;

  insn = build_insn (opcode, myops);

  /* Propagate multiply status.  */
  if (insn != -1)
    {
      if (is_parallel && prev_mul32_p)
	cur_mul32_p = 1;
      else
	{
	  prev_mul32_p = cur_mul32_p;
	  cur_mul32_p  = (opcode->op->flags_used & FLAG_MUL32) != 0;
	}
    }

  /* Propagate left_kills_right status.  */
  if (insn != -1)
    {
      prev_left_kills_right_p = cur_left_kills_right_p;

      if (opcode->op->flags_set & FLAG_LKR)
	{
	  cur_left_kills_right_p = 1;

	  if (strcmp (opcode->op->name, "mvtsys") == 0)
	    {
	      /* Left kills right for only mvtsys only for
                 PSW/PSWH/PSWL/flags target.  */
	      if ((myops[0].X_op == O_register) &&
		  ((myops[0].X_add_number == OPERAND_CONTROL) || /* psw */
		   (myops[0].X_add_number == OPERAND_CONTROL+MAX_CONTROL_REG+2) || /* pswh */
		   (myops[0].X_add_number == OPERAND_CONTROL+MAX_CONTROL_REG+1) || /* pswl */
		   (myops[0].X_add_number == OPERAND_FLAG+0) || /* f0 */
		   (myops[0].X_add_number == OPERAND_FLAG+1) || /* f1 */
		   (myops[0].X_add_number == OPERAND_FLAG+2) || /* f2 */
		   (myops[0].X_add_number == OPERAND_FLAG+3) || /* f3 */
		   (myops[0].X_add_number == OPERAND_FLAG+4) || /* f4 */
		   (myops[0].X_add_number == OPERAND_FLAG+5) || /* f5 */
		   (myops[0].X_add_number == OPERAND_FLAG+6) || /* f6 */
		   (myops[0].X_add_number == OPERAND_FLAG+7))) /* f7 */
		{
		  cur_left_kills_right_p = 1;
		}
	      else
		{
		  /* Other mvtsys target registers don't kill right
                     instruction.  */
		  cur_left_kills_right_p = 0;
		}
	    } /* mvtsys */
	}
      else
	cur_left_kills_right_p = 0;
    }

  return insn;
}

/* Get a pointer to an entry in the format table.
   It must look at all formats for an opcode and use the operands
   to choose the correct one.  Return NULL on error.  */

static struct d30v_format *
find_format (opcode, myops, fsize, cmp_hack)
     struct d30v_opcode *opcode;
     expressionS myops[];
     int fsize;
     int cmp_hack;
{
  int numops, match, index, i = 0, j, k;
  struct d30v_format *fm;

  if (opcode == NULL)
    return NULL;

  /* Get all the operands and save them as expressions.  */
  numops = get_operands (myops, cmp_hack);

  while ((index = opcode->format[i++]) != 0)
    {
      if (fsize == FORCE_SHORT && index >= LONG)
	continue;

      if (fsize == FORCE_LONG && index < LONG)
	continue;

      fm = (struct d30v_format *) &d30v_format_table[index];
      k = index;
      while (fm->form == index)
	{
	  match = 1;
	  /* Now check the operands for compatibility.  */
	  for (j = 0; match && fm->operands[j]; j++)
	    {
	      int flags = d30v_operand_table[fm->operands[j]].flags;
	      int bits = d30v_operand_table[fm->operands[j]].bits;
	      int X_op = myops[j].X_op;
	      int num = myops[j].X_add_number;

	      if (flags & OPERAND_SPECIAL)
		break;
	      else if (X_op == O_illegal)
		match = 0;
	      else if (flags & OPERAND_REG)
		{
		  if (X_op != O_register
		      || ((flags & OPERAND_ACC) && !(num & OPERAND_ACC))
		      || (!(flags & OPERAND_ACC) && (num & OPERAND_ACC))
		      || ((flags & OPERAND_FLAG) && !(num & OPERAND_FLAG))
		      || (!(flags & (OPERAND_FLAG | OPERAND_CONTROL)) && (num & OPERAND_FLAG))
		      || ((flags & OPERAND_CONTROL)
			  && !(num & (OPERAND_CONTROL | OPERAND_FLAG))))
		    {
		      match = 0;
		    }
		}
	      else if (((flags & OPERAND_MINUS)
			&& (X_op != O_absent || num != OPERAND_MINUS))
		       || ((flags & OPERAND_PLUS)
			   && (X_op != O_absent || num != OPERAND_PLUS))
		       || ((flags & OPERAND_ATMINUS)
			   && (X_op != O_absent || num != OPERAND_ATMINUS))
		       || ((flags & OPERAND_ATPAR)
			   && (X_op != O_absent || num != OPERAND_ATPAR))
		       || ((flags & OPERAND_ATSIGN)
			   && (X_op != O_absent || num != OPERAND_ATSIGN)))
		{
		  match = 0;
		}
	      else if (flags & OPERAND_NUM)
		{
		  /* A number can be a constant or symbol expression.  */

		  /* If we have found a register name, but that name
		     also matches a symbol, then re-parse the name as
		     an expression.  */
		  if (X_op == O_register
		      && symbol_find ((char *) myops[j].X_op_symbol))
		    {
		      input_line_pointer = (char *) myops[j].X_op_symbol;
		      expression (&myops[j]);
		    }

		  /* Turn an expression into a symbol for later resolution.  */
		  if (X_op != O_absent && X_op != O_constant
		      && X_op != O_symbol && X_op != O_register
		      && X_op != O_big)
		    {
		      symbolS *sym = make_expr_symbol (&myops[j]);
		      myops[j].X_op = X_op = O_symbol;
		      myops[j].X_add_symbol = sym;
		      myops[j].X_add_number = num = 0;
		    }

		  if (fm->form >= LONG)
		    {
		      /* If we're testing for a LONG format, either fits.  */
		      if (X_op != O_constant && X_op != O_symbol)
			match = 0;
		    }
		  else if (fm->form < LONG
			   && ((fsize == FORCE_SHORT && X_op == O_symbol)
			       || (fm->form == SHORT_D2 && j == 0)))
		    match = 1;

		  /* This is the tricky part.  Will the constant or symbol
		     fit into the space in the current format?  */
		  else if (X_op == O_constant)
		    {
		      if (check_range (num, bits, flags))
			match = 0;
		    }
		  else if (X_op == O_symbol
			   && S_IS_DEFINED (myops[j].X_add_symbol)
			   && S_GET_SEGMENT (myops[j].X_add_symbol) == now_seg
			   && opcode->reloc_flag == RELOC_PCREL)
		    {
		      /* If the symbol is defined, see if the value will fit
			 into the form we're considering.  */
		      fragS *f;
		      long value;

		      /* Calculate the current address by running through the
			 previous frags and adding our current offset.  */
		      value = 0;
		      for (f = frchain_now->frch_root; f; f = f->fr_next)
			value += f->fr_fix + f->fr_offset;
		      value = (S_GET_VALUE (myops[j].X_add_symbol) - value
			       - (obstack_next_free (&frchain_now->frch_obstack)
				  - frag_now->fr_literal));
		      if (check_range (value, bits, flags))
			match = 0;
		    }
		  else
		    match = 0;
		}
	    }
#if 0
	  printf ("through the loop: match=%d\n", match);
#endif
	  /* We're only done if the operands matched so far AND there
	     are no more to check.  */
	  if (match && myops[j].X_op == 0)
	    {
	      /* Final check - issue a warning if an odd numbered register
		 is used as the first register in an instruction that reads
		 or writes 2 registers.  */

	      for (j = 0; fm->operands[j]; j++)
		if (myops[j].X_op == O_register
		    && (myops[j].X_add_number & 1)
		    && (d30v_operand_table[fm->operands[j]].flags & OPERAND_2REG))
		  as_warn (_("Odd numbered register used as target of multi-register instruction"));

	      return fm;
	    }
	  fm = (struct d30v_format *) &d30v_format_table[++k];
	}
#if 0
      printf ("trying another format: i=%d\n", i);
#endif
    }
  return NULL;
}

/* If while processing a fixup, a reloc really needs to be created,
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

  reloc->addend = 0;
  return reloc;
}

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp ATTRIBUTE_UNUSED;
     asection *seg ATTRIBUTE_UNUSED;
{
  abort ();
  return 0;
}

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

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  char *where;
  unsigned long insn, insn2;
  long value = *valP;

  if (fixP->fx_addsy == (symbolS *) NULL)
    fixP->fx_done = 1;

  /* We don't support subtracting a symbol.  */
  if (fixP->fx_subsy != (symbolS *) NULL)
    as_bad_where (fixP->fx_file, fixP->fx_line, _("expression too complex"));

  /* Fetch the instruction, insert the fully resolved operand
     value, and stuff the instruction back again.  */
  where = fixP->fx_frag->fr_literal + fixP->fx_where;
  insn = bfd_getb32 ((unsigned char *) where);

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_8:  /* Check for a bad .byte directive.  */
      if (fixP->fx_addsy != NULL)
	as_bad (_("line %d: unable to place address of symbol '%s' into a byte"),
		fixP->fx_line, S_GET_NAME (fixP->fx_addsy));
      else if (((unsigned)value) > 0xff)
	as_bad (_("line %d: unable to place value %lx into a byte"),
		fixP->fx_line, value);
      else
	*(unsigned char *) where = value;
      break;

    case BFD_RELOC_16:  /* Check for a bad .short directive.  */
      if (fixP->fx_addsy != NULL)
	as_bad (_("line %d: unable to place address of symbol '%s' into a short"),
		fixP->fx_line, S_GET_NAME (fixP->fx_addsy));
      else if (((unsigned)value) > 0xffff)
	as_bad (_("line %d: unable to place value %lx into a short"),
		fixP->fx_line, value);
      else
	bfd_putb16 ((bfd_vma) value, (unsigned char *) where);
      break;

    case BFD_RELOC_64:  /* Check for a bad .quad directive.  */
      if (fixP->fx_addsy != NULL)
	as_bad (_("line %d: unable to place address of symbol '%s' into a quad"),
		fixP->fx_line, S_GET_NAME (fixP->fx_addsy));
      else
	{
	  bfd_putb32 ((bfd_vma) value, (unsigned char *) where);
	  bfd_putb32 (0, ((unsigned char *) where) + 4);
	}
      break;

    case BFD_RELOC_D30V_6:
      check_size (value, 6, fixP->fx_file, fixP->fx_line);
      insn |= value & 0x3F;
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      break;

    case BFD_RELOC_D30V_9_PCREL:
      if (fixP->fx_where & 0x7)
	{
	  if (fixP->fx_done)
	    value += 4;
	  else
	    fixP->fx_r_type = BFD_RELOC_D30V_9_PCREL_R;
	}
      check_size (value, 9, fixP->fx_file, fixP->fx_line);
      insn |= ((value >> 3) & 0x3F) << 12;
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      break;

    case BFD_RELOC_D30V_15:
      check_size (value, 15, fixP->fx_file, fixP->fx_line);
      insn |= (value >> 3) & 0xFFF;
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      break;

    case BFD_RELOC_D30V_15_PCREL:
      if (fixP->fx_where & 0x7)
	{
	  if (fixP->fx_done)
	    value += 4;
	  else
	    fixP->fx_r_type = BFD_RELOC_D30V_15_PCREL_R;
	}
      check_size (value, 15, fixP->fx_file, fixP->fx_line);
      insn |= (value >> 3) & 0xFFF;
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      break;

    case BFD_RELOC_D30V_21:
      check_size (value, 21, fixP->fx_file, fixP->fx_line);
      insn |= (value >> 3) & 0x3FFFF;
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      break;

    case BFD_RELOC_D30V_21_PCREL:
      if (fixP->fx_where & 0x7)
	{
	  if (fixP->fx_done)
	    value += 4;
	  else
	    fixP->fx_r_type = BFD_RELOC_D30V_21_PCREL_R;
	}
      check_size (value, 21, fixP->fx_file, fixP->fx_line);
      insn |= (value >> 3) & 0x3FFFF;
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      break;

    case BFD_RELOC_D30V_32:
      insn2 = bfd_getb32 ((unsigned char *) where + 4);
      insn |= (value >> 26) & 0x3F;		/* Top 6 bits.  */
      insn2 |= ((value & 0x03FC0000) << 2);	/* Next 8 bits.  */
      insn2 |= value & 0x0003FFFF;		/* Bottom 18 bits.  */
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      bfd_putb32 ((bfd_vma) insn2, (unsigned char *) where + 4);
      break;

    case BFD_RELOC_D30V_32_PCREL:
      insn2 = bfd_getb32 ((unsigned char *) where + 4);
      insn |= (value >> 26) & 0x3F;		/* Top 6 bits.  */
      insn2 |= ((value & 0x03FC0000) << 2);	/* Next 8 bits.  */
      insn2 |= value & 0x0003FFFF;		/* Bottom 18 bits.  */
      bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      bfd_putb32 ((bfd_vma) insn2, (unsigned char *) where + 4);
      break;

    case BFD_RELOC_32:
      bfd_putb32 ((bfd_vma) value, (unsigned char *) where);
      break;

    default:
      as_bad (_("line %d: unknown relocation type: 0x%x"),
	      fixP->fx_line, fixP->fx_r_type);
    }
}

/* Called after the assembler has finished parsing the input file or
   after a label is defined.  Because the D30V assembler sometimes
   saves short instructions to see if it can package them with the
   next instruction, there may be a short instruction that still needs
   written.  */

int
d30v_cleanup (use_sequential)
     int use_sequential;
{
  segT seg;
  subsegT subseg;

  if (prev_insn != -1)
    {
      seg = now_seg;
      subseg = now_subseg;
      subseg_set (prev_seg, prev_subseg);
      write_1_short (&prev_opcode, (long) prev_insn, fixups->next,
		     use_sequential);
      subseg_set (seg, subseg);
      prev_insn = -1;
      if (use_sequential)
	prev_mul32_p = FALSE;
    }

  return 1;
}

static void
d30v_number_to_chars (buf, value, n)
     char *buf;			/* Return 'nbytes' of chars here.  */
     long long value;		/* The value of the bits.  */
     int n;			/* Number of bytes in the output.  */
{
  while (n--)
    {
      buf[n] = value & 0xff;
      value >>= 8;
    }
}

/* This function is called at the start of every line.  It checks to
   see if the first character is a '.', which indicates the start of a
   pseudo-op.  If it is, then write out any unwritten instructions.  */

void
d30v_start_line ()
{
  char *c = input_line_pointer;

  while (ISSPACE (*c))
    c++;

  if (*c == '.')
    d30v_cleanup (FALSE);
}

static void
check_size (value, bits, file, line)
     long value;
     int bits;
     char *file;
     int line;
{
  int tmp, max;

  if (value < 0)
    tmp = ~value;
  else
    tmp = value;

  max = (1 << (bits - 1)) - 1;

  if (tmp > max)
    as_bad_where (file, line, _("value too large to fit in %d bits"), bits);
}

/* d30v_frob_label() is called when after a label is recognized.  */

void
d30v_frob_label (lab)
     symbolS *lab;
{
  /* Emit any pending instructions.  */
  d30v_cleanup (FALSE);

  /* Update the label's address with the current output pointer.  */
  symbol_set_frag (lab, frag_now);
  S_SET_VALUE (lab, (valueT) frag_now_fix ());

  /* Record this label for future adjustment after we find out what
     kind of data it references, and the required alignment therewith.  */
  d30v_last_label = lab;
}

/* Hook into cons for capturing alignment changes.  */

void
d30v_cons_align (size)
     int size;
{
  int log_size;

  log_size = 0;
  while ((size >>= 1) != 0)
    ++log_size;

  if (d30v_current_align < log_size)
    d30v_align (log_size, (char *) NULL, NULL);
  else if (d30v_current_align > log_size)
    d30v_current_align = log_size;
  d30v_last_label = NULL;
}

/* Called internally to handle all alignment needs.  This takes care
   of eliding calls to frag_align if'n the cached current alignment
   says we've already got it, as well as taking care of the auto-aligning
   labels wrt code.  */

static void
d30v_align (n, pfill, label)
     int n;
     char *pfill;
     symbolS *label;
{
  /* The front end is prone to changing segments out from under us
     temporarily when -g is in effect.  */
  int switched_seg_p = (d30v_current_align_seg != now_seg);

  /* Do not assume that if 'd30v_current_align >= n' and
     '! switched_seg_p' that it is safe to avoid performing
     this alignment request.  The alignment of the current frag
     can be changed under our feet, for example by a .ascii
     directive in the source code.  cf testsuite/gas/d30v/reloc.s  */
  d30v_cleanup (FALSE);

  if (pfill == NULL)
    {
      if (n > 2
	  && (bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
	{
	  static char const nop[4] = { 0x00, 0xf0, 0x00, 0x00 };

	  /* First, make sure we're on a four-byte boundary, in case
	     someone has been putting .byte values the text section.  */
	  if (d30v_current_align < 2 || switched_seg_p)
	    frag_align (2, 0, 0);
	  frag_align_pattern (n, nop, sizeof nop, 0);
	}
      else
	frag_align (n, 0, 0);
    }
  else
    frag_align (n, *pfill, 0);

  if (!switched_seg_p)
    d30v_current_align = n;

  if (label != NULL)
    {
      symbolS     *sym;
      int          label_seen = FALSE;
      struct frag *old_frag;
      valueT       old_value;
      valueT       new_value;

      assert (S_GET_SEGMENT (label) == now_seg);

      old_frag  = symbol_get_frag (label);
      old_value = S_GET_VALUE (label);
      new_value = (valueT) frag_now_fix ();

      /* It is possible to have more than one label at a particular
	 address, especially if debugging is enabled, so we must
	 take care to adjust all the labels at this address in this
	 fragment.  To save time we search from the end of the symbol
	 list, backwards, since the symbols we are interested in are
	 almost certainly the ones that were most recently added.
	 Also to save time we stop searching once we have seen at least
	 one matching label, and we encounter a label that is no longer
	 in the target fragment.  Note, this search is guaranteed to
	 find at least one match when sym == label, so no special case
	 code is necessary.  */
      for (sym = symbol_lastP; sym != NULL; sym = symbol_previous (sym))
	{
	  if (symbol_get_frag (sym) == old_frag
	      && S_GET_VALUE (sym) == old_value)
	    {
	      label_seen = TRUE;
	      symbol_set_frag (sym, frag_now);
	      S_SET_VALUE (sym, new_value);
	    }
	  else if (label_seen && symbol_get_frag (sym) != old_frag)
	    break;
	}
    }

  record_alignment (now_seg, n);
}

/* Handle the .align pseudo-op.  This aligns to a power of two.  We
   hook here to latch the current alignment.  */

static void
s_d30v_align (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int align;
  char fill, *pfill = NULL;
  long max_alignment = 15;

  align = get_absolute_expression ();
  if (align > max_alignment)
    {
      align = max_alignment;
      as_warn (_("Alignment too large: %d assumed"), align);
    }
  else if (align < 0)
    {
      as_warn (_("Alignment negative: 0 assumed"));
      align = 0;
    }

  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      fill = get_absolute_expression ();
      pfill = &fill;
    }

  d30v_last_label = NULL;
  d30v_align (align, pfill, NULL);

  demand_empty_rest_of_line ();
}

/* Handle the .text pseudo-op.  This is like the usual one, but it
   clears the saved last label and resets known alignment.  */

static void
s_d30v_text (i)
     int i;

{
  s_text (i);
  d30v_last_label = NULL;
  d30v_current_align = 0;
  d30v_current_align_seg = now_seg;
}

/* Handle the .data pseudo-op.  This is like the usual one, but it
   clears the saved last label and resets known alignment.  */

static void
s_d30v_data (i)
     int i;
{
  s_data (i);
  d30v_last_label = NULL;
  d30v_current_align = 0;
  d30v_current_align_seg = now_seg;
}

/* Handle the .section pseudo-op.  This is like the usual one, but it
   clears the saved last label and resets known alignment.  */

static void
s_d30v_section (ignore)
     int ignore;
{
  obj_elf_section (ignore);
  d30v_last_label = NULL;
  d30v_current_align = 0;
  d30v_current_align_seg = now_seg;
}
