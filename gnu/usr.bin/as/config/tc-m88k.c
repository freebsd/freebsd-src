/* m88k.c -- Assembler for the Motorola 88000
   Contributed by Devon Bowen of Buffalo University
   and Torbjorn Granlund of the Swedish Institute of Computer Science.
   Copyright (C) 1989-1992 Free Software Foundation, Inc.

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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "as.h"
#include "opcode/m88k.h"

struct m88k_insn
{
  unsigned long opcode;
  expressionS exp;
  enum reloc_type reloc;
};

#if __STDC__ == 1

static int calcop(struct m88k_opcode *format, char *param, struct m88k_insn *insn);

#else /* not __STDC__ */

static int calcop();

#endif /* not __STDC__ */

char *getval ();
char *get_reg ();
char *get_imm16 ();
char *get_bf ();
char *get_pcr ();
char *get_cmp ();
char *get_cnd ();
char *get_cr ();
char *get_fcr ();
char *get_vec9 ();

struct field_val_assoc
{
  char *name;
  unsigned val;
};

struct field_val_assoc cr_regs[] =
{
  {"PID", 0},
  {"PSR", 1},
  {"EPSR", 2},
  {"SSBR", 3},
  {"SXIP", 4},
  {"SNIP", 5},
  {"SFIP", 6},
  {"VBR", 7},
  {"DMT0", 8},
  {"DMD0", 9},
  {"DMA0", 10},
  {"DMT1", 11},
  {"DMD1", 12},
  {"DMA1", 13},
  {"DMT2", 14},
  {"DMD2", 15},
  {"DMA2", 16},
  {"SR0", 17},
  {"SR1", 18},
  {"SR2", 19},
  {"SR3", 20},

  {NULL, 0},
};

struct field_val_assoc fcr_regs[] =
{
  {"FPECR", 0},
  {"FPHS1", 1},
  {"FPLS1", 2},
  {"FPHS2", 3},
  {"FPLS2", 4},
  {"FPPT", 5},
  {"FPRH", 6},
  {"FPRL", 7},
  {"FPIT", 8},

  {"FPSR", 62},
  {"FPCR", 63},

  {NULL, 0},
};

struct field_val_assoc cmpslot[] =
{
/* Integer  Floating point */
		{"nc", 0},
		{"cp", 1},
  {"eq", 2},
  {"ne", 3},
  {"gt", 4},
  {"le", 5},
  {"lt", 6},
  {"ge", 7},
  {"hi", 8},	{"ou", 8},
  {"ls", 9},	{"ib", 9},
  {"lo", 10},	{"in", 10},
  {"hs", 11},	{"ob", 11},

  {NULL, 0},
};

struct field_val_assoc cndmsk[] =
{
  {"gt0", 1},
  {"eq0", 2},
  {"ge0", 3},
  {"lt0", 12},
  {"ne0", 13},
  {"le0", 14},

  {NULL, 0},
};

extern char *myname;
static struct hash_control *op_hash = NULL;

/* These bits should be turned off in the first address of every segment */
int md_seg_align = 7;

/* This is the number to put at the beginning of the a.out file */
long omagic = OMAGIC;

/* These chars start a comment anywhere in a source file (except inside
   another comment */
char comment_chars[] = ";";

/* These chars only start a comment at the beginning of a line. */
char line_comment_chars[] = "#";

/* Chars that can be used to separate mant from exp in floating point nums */
char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* as in 0f123.456 */
/* or    0H1.234E-12 (see exp chars above) */
char FLT_CHARS[] = "dDfF";

extern void float_cons (), cons (), s_globl (), s_line (),
  s_space (), s_set (), stringer (), s_lcomm ();
static void s_bss ();

const pseudo_typeS md_pseudo_table[] = {
	{"align", s_align_bytes, 0 },
	{"def", s_set, 0},
	{"dfloat", float_cons, 'd'},
	{"ffloat", float_cons, 'f'},
	{"global", s_globl, 0},
	{"half", cons, 2 },
	{"bss", s_bss, 0},
	{"string", stringer, 0},
	{"word", cons, 4 },
	{"zero", s_space, 0},
	{0}
};

const int md_reloc_size = 12; /* Size of relocation record */

void
md_begin ()
{
  char *retval = NULL;
  unsigned int i = 0;

  /* initialize hash table */

  op_hash = hash_new ();
  if (op_hash == NULL)
    as_fatal ("Could not initialize hash table");

  /* loop until you see the end of the list */

  while (*m88k_opcodes[i].name)
    {
      char *name = m88k_opcodes[i].name;

      /* hash each mnemonic and record its position */

      retval = hash_insert (op_hash, name, &m88k_opcodes[i]);

      if (retval != NULL && *retval != '\0')
	as_fatal ("Can't hash instruction '%s':%s",
		 m88k_opcodes[i].name, retval);

      /* skip to next unique mnemonic or end of list */

      for (i++; !strcmp (m88k_opcodes[i].name, name); i++)
	;
    }
}

int
md_parse_option (argP, cntP, vecP)
     char **argP;
     int *cntP;
     char ***vecP;
{
	as_warn ("unknown option: -%s", *argP);
	return(0);
}

void
md_assemble (op)
     char *op;
{
  char *param, *thisfrag;
  struct m88k_opcode *format;
  struct m88k_insn insn;

  assert (op);

  /* skip over instruction to find parameters */

  for (param = op; *param != 0 && !isspace (*param); param++)
    ;
  if (*param != 0)
    *param++ = 0;

  /* try to find the instruction in the hash table */

  if ((format = (struct m88k_opcode *) hash_find (op_hash, op)) == NULL)
    {
      as_fatal ("Invalid mnemonic '%s'", op);
      return;
    }

  /* try parsing this instruction into insn */

  insn.exp.X_add_symbol = 0;
  insn.exp.X_subtract_symbol = 0;
  insn.exp.X_add_number = 0;
  insn.exp.X_seg = 0;
  insn.reloc = NO_RELOC;

  while (!calcop(format, param, &insn))
    {
      /* if it doesn't parse try the next instruction */

      if (!strcmp (format[0].name, format[1].name))
	format++;
      else
	{
	  as_fatal ("Parameter syntax error");
	  return;
	}
    }

  /* grow the current frag and plop in the opcode */

  thisfrag = frag_more (4);
  md_number_to_chars (thisfrag, insn.opcode, 4);

  /* if this instruction requires labels mark it for later */

  switch (insn.reloc)
    {
    case NO_RELOC:
      break;

    case RELOC_LO16:
    case RELOC_HI16:
      fix_new (frag_now,
	       thisfrag - frag_now->fr_literal + 2,
	       2,
	       insn.exp.X_add_symbol,
	       insn.exp.X_subtract_symbol,
	       insn.exp.X_add_number,
	       0,
	       insn.reloc);
      break;

    case RELOC_IW16:
      fix_new (frag_now,
	       thisfrag - frag_now->fr_literal,
	       4,
	       insn.exp.X_add_symbol,
	       insn.exp.X_subtract_symbol,
	       insn.exp.X_add_number,
	       0,
	       insn.reloc);
      break;

    case RELOC_PC16:
      fix_new (frag_now,
	       thisfrag - frag_now->fr_literal + 2,
	       2,
	       insn.exp.X_add_symbol,
	       insn.exp.X_subtract_symbol,
	       insn.exp.X_add_number,
	       1,
	       insn.reloc);
      break;

    case RELOC_PC26:
      fix_new (frag_now,
	       thisfrag - frag_now->fr_literal,
	       4,
	       insn.exp.X_add_symbol,
	       insn.exp.X_subtract_symbol,
	       insn.exp.X_add_number,
	       1,
	       insn.reloc);
      break;

    default:
      as_fatal ("Unknown relocation type");
      break;
    }
}

int
calcop (format, param, insn)
     struct m88k_opcode *format;
     char *param;
     struct m88k_insn *insn;
{
  char *fmt = format->op_spec;
  int f;
  unsigned val;
  unsigned opcode;

  insn->opcode = format->opcode;
  opcode = 0;

  for (;;)
    {
      if (param == 0)
	return 0;
      f = *fmt++;
      switch (f)
	{
	case 0:
	  insn->opcode |= opcode;
	  return *param == 0;

	default:
	  if (f != *param++)
	    return 0;
	  break;

	case 'd':
	  param = get_reg (param, &val);
	  opcode |= val << 21;
	  break;

	case '1':
	  param = get_reg (param, &val);
	  opcode |= val << 16;
	  break;

	case '2':
	  param = get_reg (param, &val);
	  opcode |= val;
	  break;

	case '3':
	  param = get_reg (param, &val);
	  opcode |= (val << 16) | val;
	  break;

	case 'I':
	  param = get_imm16 (param, insn);
	  break;

	case 'b':
	  param = get_bf (param, &val);
	  opcode |= val;
	  break;

	case 'p':
	  param = get_pcr (param, insn, RELOC_PC16);
	  break;

	case 'P':
	  param = get_pcr (param, insn, RELOC_PC26);
	  break;

	case 'B':
	  param = get_cmp (param, &val);
	  opcode |= val;
	  break;

	case 'M':
	  param = get_cnd (param, &val);
	  opcode |= val;
	  break;

	case 'c':
	  param = get_cr (param, &val);
	  opcode |= val << 5;
	  break;

	case 'f':
	  param = get_fcr (param, &val);
	  opcode |= val << 5;
	  break;

	case 'V':
	  param = get_vec9 (param, &val);
	  opcode |= val;
	  break;

	case '?':
	  /* Having this here repeats the warning somtimes.
	   But can't we stand that?  */
	  as_warn ("Use of obsolete instruction");
	  break;
	}
    }
}

char *
match_name (param, assoc_tab, valp)
     char *param;
     struct field_val_assoc *assoc_tab;
     unsigned *valp;
{
  int i;
  char *name;
  int name_len;

  for (i = 0;; i++)
    {
      name = assoc_tab[i].name;
      if (name == NULL)
	return NULL;
      name_len = strlen (name);
      if (!strncmp (param, name, name_len))
	{
	  *valp = assoc_tab[i].val;
	  return param + name_len;
	}
    }
}

char *
get_reg (param, regnop)
     char *param;
     unsigned *regnop;
{
  unsigned c;
  unsigned regno;

  c = *param++;
  if (c == 'r')
    {
      regno = *param++ - '0';
      if (regno < 10)
	{
	  if (regno == 0)
	    {
	      *regnop = 0;
	      return param;
	    }
	  c = *param - '0';
	  if (c < 10)
	    {
	      regno = regno * 10 + c;
	      if (c < 32)
		{
		  *regnop = regno;
		  return param + 1;
		}
	    }
	  else
	    {
	      *regnop = regno;
	      return param;
	    }
	}
      return NULL;
    }
  else if (c == 's' && param[0] == 'p')
    {
      *regnop = 31;
      return param + 1;
    }

  return 0;
}

char *
get_imm16 (param, insn)
     char *param;
     struct m88k_insn *insn;
{
  enum reloc_type reloc = NO_RELOC;
  unsigned int val;
  segT seg;
  char *save_ptr;

  if (!strncmp (param, "hi16", 4) && !isalnum (param[4]))
    {
      reloc = RELOC_HI16;
      param += 4;
    }
  else if (!strncmp (param, "lo16", 4) && !isalnum (param[4]))
    {
      reloc = RELOC_LO16;
      param += 4;
    }
  else if (!strncmp (param, "iw16", 4) && !isalnum (param[4]))
    {
      reloc = RELOC_IW16;
      param += 4;
    }

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  seg = expression (&insn->exp);
  param = input_line_pointer;
  input_line_pointer = save_ptr;

  val = insn->exp.X_add_number;

  if (seg == SEG_ABSOLUTE)
    {
      /* Insert the value now, and reset reloc to NO_RELOC.  */
      if (reloc == NO_RELOC)
	{
	  /* Warn about too big expressions if not surrounded by xx16.  */
	  if (val > 0xffff)
	    as_warn ("Expression truncated to 16 bits");
	}

      if (reloc == RELOC_HI16)
	val >>= 16;

      insn->opcode |= val & 0xffff;
      reloc = NO_RELOC;
    }
  else if (reloc == NO_RELOC)
    /* We accept a symbol even without lo16, hi16, etc, and assume
       lo16 was intended.  */
    reloc = RELOC_LO16;

  insn->reloc = reloc;

  return param;
}

char *
get_pcr (param, insn, reloc)
     char *param;
     struct m88k_insn *insn;
     enum reloc_type reloc;
{
  char *saveptr, *saveparam;
  segT seg;

  saveptr = input_line_pointer;
  input_line_pointer = param;

  seg = expression (&insn->exp);

  saveparam = input_line_pointer;
  input_line_pointer = saveptr;

  /* Botch: We should relocate now if SEG_ABSOLUTE.  */
  insn->reloc = reloc;

  return saveparam;
}

char *
get_cmp (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned int val;
  char *save_ptr;

  save_ptr = param;

  param = match_name (param, cmpslot, valp);
  val = *valp;

  if (param == NULL)
    {
      param = save_ptr;

      save_ptr = input_line_pointer;
      input_line_pointer = param;
      val = get_absolute_expression ();
      param = input_line_pointer;
      input_line_pointer = save_ptr;

      if (val >= 32)
	{
	  as_warn ("Expression truncated to 5 bits");
	  val %= 32;
	}
    }

  *valp = val << 21;
  return param;
}

char *
get_cnd (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned int val;

  if (isdigit (*param))
    {
      param = getval (param, &val);

      if (val >= 32)
	{
	  as_warn ("Expression truncated to 5 bits");
	  val %= 32;
	}
    }
  else
    {
      if (isupper (*param))
	*param = tolower (*param);

      if (isupper (param[1]))
	param[1] = tolower (param[1]);

      param = match_name (param, cndmsk, valp);

      if (param == NULL)
	return NULL;

      val = *valp;
    }

  *valp = val << 21;
  return param;
}

char *
get_bf2 (param, bc)
     char *param;
     int bc;
{
  int depth = 0;
  int c;

  for (;;)
    {
      c = *param;
      if (c == 0)
	return param;
      else if (c == '(')
	depth++;
      else if (c == ')')
	depth--;
      else if (c == bc && depth <= 0)
	return param;
      param++;
    }
}

char *
get_bf_offset_expression (param, offsetp)
     char *param;
     unsigned *offsetp;
{
  unsigned offset;

  if (isalpha (param[0]))
    {
      if (isupper (param[0]))
	param[0] = tolower (param[0]);
      if (isupper (param[1]))
	param[1] = tolower (param[1]);

      param = match_name (param, cmpslot, offsetp);

      return param;
    }
  else
    {
      input_line_pointer = param;
      offset = get_absolute_expression ();
      param = input_line_pointer;
    }

  *offsetp = offset;
  return param;
}

char *
get_bf (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned offset = 0;
  unsigned width = 0;
  char *xp;
  char *save_ptr;

  xp = get_bf2 (param, '<');

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  if (*xp == 0)
    {
      /* We did not find '<'.  We have an offset (width implicitly 32).  */
      param = get_bf_offset_expression (param, &offset);
      if (param == NULL)
	return NULL;
      input_line_pointer = save_ptr;
    }
  else
    {
      *xp++ = 0;		/* Overwrite the '<' */
      param = get_bf2 (xp, '>');
      if (*param == 0)
	return NULL;
      *param++ = 0;		/* Overwrite the '>' */

      width = get_absolute_expression ();
      xp =  get_bf_offset_expression (xp, &offset);
      input_line_pointer = save_ptr;

      if (xp + 1 != param)
	return NULL;
    }

  *valp = ((width % 32) << 5) | (offset % 32);

  return param;
}

char *
get_cr (param, regnop)
     char *param;
     unsigned *regnop;
{
  unsigned regno;
  unsigned c;
/*  int i; FIXME remove this */
/*  int name_len; FIXME remove this */

  if (!strncmp (param, "cr", 2))
    {
      param += 2;

      regno = *param++ - '0';
      if (regno < 10)
	{
	  if (regno == 0)
	    {
	      *regnop = 0;
	      return param;
	    }
	  c = *param - '0';
	  if (c < 10)
	    {
	      regno = regno * 10 + c;
	      if (c < 64)
		{
		  *regnop = regno;
		  return param + 1;
		}
	    }
	  else
	    {
	      *regnop = regno;
	      return param;
	    }
	}
      return NULL;
    }

  param = match_name (param, cr_regs, regnop);

  return param;
}

char *
get_fcr (param, regnop)
     char *param;
     unsigned *regnop;
{
  unsigned regno;
  unsigned c;
/*  int i; FIXME remove this */
/*  int name_len; FIXME: remove this */

  if (!strncmp (param, "fcr", 3))
    {
      param += 3;

      regno = *param++ - '0';
      if (regno < 10)
	{
	  if (regno == 0)
	    {
	      *regnop = 0;
	      return param;
	    }
	  c = *param - '0';
	  if (c < 10)
	    {
	      regno = regno * 10 + c;
	      if (c < 64)
		{
		  *regnop = regno;
		  return param + 1;
		}
	    }
	  else
	    {
	      *regnop = regno;
	      return param;
	    }
	}
      return NULL;
    }

  param = match_name (param, fcr_regs, regnop);

  return param;
}

char *
get_vec9 (param, valp)
     char *param;
     unsigned *valp;
{
  unsigned val;
  char *save_ptr;

  save_ptr = input_line_pointer;
  input_line_pointer = param;
  val = get_absolute_expression ();
  param = input_line_pointer;
  input_line_pointer = save_ptr;

  if (val >= 1 << 9)
    as_warn ("Expression truncated to 9 bits");

  *valp = val % (1 << 9);

  return param;
}

#define hexval(z) \
  (isdigit (z) ? (z) - '0' :						\
   islower (z) ? (z) - 'a' + 10 : 					\
   isupper (z) ? (z) - 'A' + 10 : -1)

char *
getval (param, valp)
     char *param;
     unsigned int *valp;
{
  unsigned int val = 0;
  unsigned int c;

  c = *param++;
  if (c == '0')
    {
      c = *param++;
      if (c == 'x' || c == 'X')
	{
	  c = *param++;
	  c = hexval (c);
	  while (c < 16)
	    {
	      val = val * 16 + c;
	      c = *param++;
	      c = hexval (c);
	    }
	}
      else
	{
	  c -= '0';
	  while (c < 8)
	    {
	      val = val * 8 + c;
	      c = *param++ - '0';
	    }
	}
    }
  else
    {
      c -= '0';
      while (c < 10)
	{
	  val = val * 10 + c;
	  c = *param++ - '0';
	}
    }

  *valp = val;
  return param - 1;
}

void
md_number_to_chars (buf, val, nbytes)
char *buf;
long val;
int nbytes;
{
  switch (nbytes)
    {
    case 4:
      *buf++ = val >> 24;
      *buf++ = val >> 16;
    case 2:
      *buf++ = val >> 8;
    case 1:
      *buf = val;
      break;

    default:
      abort ();
    }
}

#ifdef comment

void
md_number_to_imm (buf, val, nbytes, fixP, seg_type)
unsigned char *buf;
unsigned int val;
int nbytes;
fixS *fixP;
int seg_type;
{
  if (seg_type != N_TEXT || fixP->fx_r_type == NO_RELOC)
    {
      switch (nbytes)
	{
	case 4:
	  *buf++ = val >> 24;
	  *buf++ = val >> 16;
	case 2:
	  *buf++ = val >> 8;
	case 1:
	  *buf = val;
	  break;

	default:
	  abort ();
	}
      return;
    }

  switch (fixP->fx_r_type)
    {
    case RELOC_IW16:
      buf[2] = val >> 8;
      buf[3] = val;
      break;

    case RELOC_LO16:
      buf[0] = val >> 8;
      buf[1] = val;
      break;

    case RELOC_HI16:
      buf[0] = val >> 24;
      buf[1] = val >> 16;
      break;

    case RELOC_PC16:
      val += 4;
      buf[0] = val >> 10;
      buf[1] = val >> 2;
      break;

    case RELOC_PC26:
      val += 4;
      buf[0] |= (val >> 26) & 0x03;
      buf[1] = val >> 18;
      buf[2] = val >> 10;
      buf[3] = val >> 2;
      break;

    case RELOC_32:
      buf[0] = val >> 24;
      buf[1] = val >> 16;
      buf[2] = val >> 8;
      buf[3] = val;
      break;

    default:
      as_fatal ("Bad relocation type");
      break;
    }
}
#endif /* comment */

/* Apply a fixS to the frags, now that we know the value it ought to
   hold. */

void md_apply_fix(fixP, val)
fixS *fixP;
long val;
{
	char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

	fixP->fx_addnumber = val;


	switch (fixP->fx_r_type) {

	case RELOC_IW16:
		buf[2] = val >> 8;
		buf[3] = val;
		break;

	case RELOC_LO16:
		buf[0] = val >> 8;
		buf[1] = val;
		break;

	case RELOC_HI16:
		buf[0] = val >> 24;
		buf[1] = val >> 16;
		break;

	case RELOC_PC16:
		val += 4;
		buf[0] = val >> 10;
		buf[1] = val >> 2;
		break;

	case RELOC_PC26:
		val += 4;
		buf[0] |= (val >> 26) & 0x03;
		buf[1] = val >> 18;
		buf[2] = val >> 10;
		buf[3] = val >> 2;
		break;

	case RELOC_32:
		buf[0] = val >> 24;
		buf[1] = val >> 16;
		buf[2] = val >> 8;
		buf[3] = val;
		break;

	case NO_RELOC:
		switch (fixP->fx_size) {
		case 4:
			*buf++ = val >> 24;
			*buf++ = val >> 16;
		case 2:
			*buf++ = val >> 8;
		case 1:
			*buf = val;
			break;

		default:
			abort ();
		}

	default:
		as_bad("bad relocation type: 0x%02x", fixP->fx_r_type);
		break;
	}

	return;
} /* md_apply_fix() */

void
md_number_to_disp (buf, val, nbytes)
char *buf;
int val;
int nbytes;
{
  as_fatal ("md_number_to_disp not defined");
  md_number_to_chars (buf, val, nbytes);
}

void
md_number_to_field (buf, val, nbytes)
char *buf;
int val;
int nbytes;
{
  as_fatal ("md_number_to_field not defined");
  md_number_to_chars (buf, val, nbytes);
}

#define MAX_LITTLENUMS 6

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP. An error message is returned, or NULL on OK.
 */
char *
md_atof (type, litP, sizeP)
     char type;
     char *litP;
     int *sizeP;
{
  int	prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char	*t;
  char	*atof_ieee ();

  switch (type)
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

    case 'x':
    case 'X':
      prec = 6;
      break;

    case 'p':
    case 'P':
      prec = 6;
      break;

    default:
      *sizeP=0;
      return "Bad call to MD_ATOF()";
    }
  t=atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer=t;

  *sizeP=prec * sizeof (LITTLENUM_TYPE);
  for (wordP=words;prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP+=sizeof (LITTLENUM_TYPE);
    }
  return "";	/* Someone should teach Dean about null pointers */
}

int md_short_jump_size = 4;

void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     long from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  ptr[0] = 0xc0; ptr[1] = 0x00; ptr[2] = 0x00; ptr[3] = 0x00;
  fix_new (frag,
	   ptr - frag->fr_literal,
	   4,
	   to_symbol,
	   (symbolS *) 0,
	   (long int) 0,
	   0,
	   RELOC_PC26);		/* Botch: Shouldn't this be RELOC_PC16? */
}

int md_long_jump_size = 4;

void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr;
     long from_addr, to_addr;
     fragS *frag;
     symbolS *to_symbol;
{
  ptr[0] = 0xc0; ptr[1] = 0x00; ptr[2] = 0x00; ptr[3] = 0x00;
  fix_new (frag,
	   ptr - frag->fr_literal,
	   4,
	   to_symbol,
	   (symbolS *) 0,
	   (long int) 0,
	   0,
	   RELOC_PC26);
}

int
md_estimate_size_before_relax (fragP, segment_type)
     fragS *fragP;
     segT segment_type;
{
	as_fatal("Relaxation should never occur");
	return(0);
}

const relax_typeS md_relax_table[] = {0};

void
md_convert_frag (headers, fragP)
object_headers *headers;
     fragS *fragP;
{
  as_fatal ("Relaxation should never occur");
}

void
md_end ()
{
}

#ifdef comment

/*
 * Risc relocations are completely different, so it needs
 * this machine dependent routine to emit them.
 */
void
emit_relocations (fixP, segment_address_in_file)
    fixS *fixP;
    relax_addressT segment_address_in_file;
{
    struct reloc_info_m88k ri;
    symbolS *symbolP;
    extern char *next_object_file_charP;

    bzero ((char *) &ri, sizeof (ri));
    for (; fixP; fixP = fixP->fx_next) {

	if (fixP->fx_r_type >= NO_RELOC) {
	    fprintf (stderr, "fixP->fx_r_type = %d\n", fixP->fx_r_type);
	    abort ();
	}

	if ((symbolP = fixP->fx_addsy) != NULL) {
	    ri.r_address = fixP->fx_frag->fr_address +
	        fixP->fx_where - segment_address_in_file;
	    if ((symbolP->sy_type & N_TYPE) == N_UNDF) {
		ri.r_extern = 1;
		ri.r_symbolnum = symbolP->sy_number;
	    } else {
		ri.r_extern = 0;
		ri.r_symbolnum = symbolP->sy_type & N_TYPE;
	    }
	    if (symbolP && symbolP->sy_frag) {
		ri.r_addend = symbolP->sy_frag->fr_address;
	    }
	    ri.r_type = fixP->fx_r_type;
	    if (fixP->fx_pcrel) {
/*		ri.r_addend -= fixP->fx_where;          */
		ri.r_addend -= ri.r_address;
	    } else {
		ri.r_addend = fixP->fx_addnumber;
	    }

/*	    md_ri_to_chars ((char *) &ri, ri);        */
	    append (&next_object_file_charP, (char *)& ri, sizeof (ri));
	}
    }
    return;
}
#endif /* comment */

/* Translate internal representation of relocation info to target format.

   On m88k: first 4 bytes are normal unsigned long address,
   next three bytes are index, most sig. byte first.
   Byte 7 is broken up with bit 7 as external,
   	bits 6, 5, & 4 unused, and the lower four bits as relocation
	type.
   Next 4 bytes are long addend. */

void tc_aout_fix_to_chars(where, fixP, segment_address_in_file)
char *where;
fixS *fixP;
relax_addressT segment_address_in_file;
{
	long r_index;
	long r_extern;
	long r_addend = 0;
	long r_address;

	know(fixP->fx_addsy);

	if (!S_IS_DEFINED(fixP->fx_addsy)) {
		r_extern = 1;
		r_index = fixP->fx_addsy->sy_number;
	} else {
		r_extern = 0;
		r_index = S_GET_TYPE(fixP->fx_addsy);
	}

	/* this is easy */
	md_number_to_chars(where,
			   r_address = fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file,
			   4);

	/* now the fun stuff */
	where[4] = (r_index >> 16) & 0x0ff;
	where[5] = (r_index >> 8) & 0x0ff;
	where[6] = r_index & 0x0ff;
	where[7] = ((r_extern << 7)  & 0x80) | (0 & 0x70) | (fixP->fx_r_type & 0xf);

	/* Also easy */
	if (fixP->fx_addsy->sy_frag) {
		r_addend = fixP->fx_addsy->sy_frag->fr_address;
	}

	if (fixP->fx_pcrel) {
		r_addend -= r_address;
	} else {
		r_addend = fixP->fx_addnumber;
	}

	md_number_to_chars(&where[8], r_addend, 4);

	return;
} /* tc_aout_fix_to_chars() */


static void
s_bss()
{
  char *name;
  char c;
  char *p;
  int temp, bss_align = 1;
  symbolS *symbolP;
  extern char is_end_of_line[256];

  name = input_line_pointer;
  c = get_symbol_end();
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE();
  if ( * input_line_pointer != ',' )
    {
      as_warn("Expected comma after name");
      ignore_rest_of_line();
      return;
    }
  input_line_pointer ++;
  if ((temp = get_absolute_expression()) < 0)
    {
      as_warn("BSS length (%d.) <0! Ignored.", temp);
      ignore_rest_of_line();
      return;
    }
  *p = 0;
  symbolP = symbol_find_or_make(name);
  *p = c;
  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      bss_align = get_absolute_expression();
      while (local_bss_counter % bss_align != 0)
	local_bss_counter++;
    }

  if (!S_IS_DEFINED(symbolP)
      || (S_GET_SEGMENT(symbolP) == SEG_BSS
	  && S_GET_VALUE(symbolP) == local_bss_counter)) {
	  S_SET_VALUE(symbolP, local_bss_counter);
	  S_SET_SEGMENT(symbolP, SEG_BSS);
	  symbolP->sy_frag  = &bss_address_frag;
	  local_bss_counter += temp;
  } else {
	  as_warn( "Ignoring attempt to re-define symbol from %d. to %d.",
		  S_GET_VALUE(symbolP), local_bss_counter );
  }
  while (!is_end_of_line[*input_line_pointer])
    {
      input_line_pointer++;
    }

  return;
}

/* We have no need to default values of symbols. */

/* ARGSUSED */
symbolS *md_undefined_symbol(name)
char *name;
{
	return 0;
} /* md_undefined_symbol() */

/* Parse an operand that is machine-specific.
   We just return without modifying the expression if we have nothing
   to do. */

/* ARGSUSED */
void md_operand(expressionP)
expressionS *expressionP;
{
} /* md_operand() */

/* Round up a section size to the appropriate boundary. */
long md_section_align(segment, size)
segT segment;
long size;
{
	return((size + 7) & ~7); /* Round all sects to multiple of 8 */
} /* md_section_align() */

/* Exactly what point is a PC-relative offset relative TO?
   On the sparc, they're relative to the address of the offset, plus
   its size.  This gets us to the following instruction.
   (??? Is this right?  FIXME-SOON) */
long md_pcrel_from(fixP)
fixS *fixP;
{
	return(fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address);
} /* md_pcrel_from() */

 /* end of tc-m88k.c */
