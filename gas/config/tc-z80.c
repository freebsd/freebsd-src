/* tc-z80.c -- Assemble code for the Zilog Z80 and ASCII R800
   Copyright 2005, 2006 Free Software Foundation, Inc.
   Contributed by Arnold Metselaar <arnold_m@operamail.com>

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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"

/* Exported constants.  */
const char comment_chars[] = ";\0";
const char line_comment_chars[] = "#;\0";
const char line_separator_chars[] = "\0";
const char EXP_CHARS[] = "eE\0";
const char FLT_CHARS[] = "RrFf\0";

/* For machine specific options.  */
const char * md_shortopts = ""; /* None yet.  */

enum options
{
  OPTION_MACH_Z80 = OPTION_MD_BASE,
  OPTION_MACH_R800,
  OPTION_MACH_IUD,
  OPTION_MACH_WUD,
  OPTION_MACH_FUD,
  OPTION_MACH_IUP,
  OPTION_MACH_WUP,
  OPTION_MACH_FUP
};

#define INS_Z80    1
#define INS_UNDOC  2
#define INS_UNPORT 4
#define INS_R800   8

struct option md_longopts[] =
{
  { "z80",       no_argument, NULL, OPTION_MACH_Z80},
  { "r800",      no_argument, NULL, OPTION_MACH_R800},
  { "ignore-undocumented-instructions", no_argument, NULL, OPTION_MACH_IUD },
  { "Wnud",  no_argument, NULL, OPTION_MACH_IUD },
  { "warn-undocumented-instructions",  no_argument, NULL, OPTION_MACH_WUD },
  { "Wud",  no_argument, NULL, OPTION_MACH_WUD },
  { "forbid-undocumented-instructions", no_argument, NULL, OPTION_MACH_FUD },
  { "Fud",  no_argument, NULL, OPTION_MACH_FUD },
  { "ignore-unportable-instructions", no_argument, NULL, OPTION_MACH_IUP },
  { "Wnup",  no_argument, NULL, OPTION_MACH_IUP },
  { "warn-unportable-instructions",  no_argument, NULL, OPTION_MACH_WUP },
  { "Wup",  no_argument, NULL, OPTION_MACH_WUP },
  { "forbid-unportable-instructions", no_argument, NULL, OPTION_MACH_FUP },
  { "Fup",  no_argument, NULL, OPTION_MACH_FUP },

  { NULL, no_argument, NULL, 0 }
} ;

size_t md_longopts_size = sizeof (md_longopts);

extern int coff_flags;
/* Instruction classes that silently assembled.  */
static int ins_ok = INS_Z80 | INS_UNDOC;
/* Instruction classes that generate errors.  */
static int ins_err = INS_R800;
/* Instruction classes actually used, determines machine type.  */
static int ins_used = INS_Z80;

int
md_parse_option (int c, char* arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    default:
      return 0;
    case OPTION_MACH_Z80:
      ins_ok &= ~INS_R800;
      ins_err |= INS_R800;
      break;
    case OPTION_MACH_R800:
      ins_ok = INS_Z80 | INS_UNDOC | INS_R800;
      ins_err = INS_UNPORT;
      break;
    case OPTION_MACH_IUD:
      ins_ok |= INS_UNDOC;
      ins_err &= ~INS_UNDOC;
      break;
    case OPTION_MACH_IUP:
      ins_ok |= INS_UNDOC | INS_UNPORT;
      ins_err &= ~(INS_UNDOC | INS_UNPORT);
      break;
    case OPTION_MACH_WUD:
      if ((ins_ok & INS_R800) == 0)
	{
	  ins_ok &= ~(INS_UNDOC|INS_UNPORT);
	  ins_err &= ~INS_UNDOC;
	}
      break;
    case OPTION_MACH_WUP:
      ins_ok &= ~INS_UNPORT;
      ins_err &= ~(INS_UNDOC|INS_UNPORT);
      break;
    case OPTION_MACH_FUD:
      if ((ins_ok & INS_R800) == 0)
	{
	  ins_ok &= (INS_UNDOC | INS_UNPORT);
	  ins_err |= INS_UNDOC | INS_UNPORT;
	}
      break;
    case OPTION_MACH_FUP:
      ins_ok &= ~INS_UNPORT;
      ins_err |= INS_UNPORT;
      break;
    }

  return 1;
}

void
md_show_usage (FILE * f)
{
  fprintf (f, "\n\
CPU model/instruction set options:\n\
\n\
  -z80\t\t  assemble for Z80\n\
  -ignore-undocumented-instructions\n\
  -Wnud\n\
\tsilently assemble undocumented Z80-instructions that work on R800\n\
  -ignore-unportable-instructions\n\
  -Wnup\n\
\tsilently assemble all undocumented Z80-instructions\n\
  -warn-undocumented-instructions\n\
  -Wud\n\
\tissue warnings for undocumented Z80-instructions that work on R800\n\
  -warn-unportable-instructions\n\
  -Wup\n\
\tissue warnings for other undocumented Z80-instructions\n\
  -forbid-undocumented-instructions\n\
  -Fud\n\
\ttreat all undocumented z80-instructions as errors\n\
  -forbid-unportable-instructions\n\
  -Fup\n\
\ttreat undocumented z80-instructions that do not work on R800 as errors\n\
  -r800\t  assemble for R800\n\n\
Default: -z80 -ignore-undocument-instructions -warn-unportable-instructions.\n");
}

static symbolS * zero;

void
md_begin (void)
{
  expressionS nul;
  char * p;

  p = input_line_pointer;
  input_line_pointer = "0";
  nul.X_md=0;
  expression (& nul);
  input_line_pointer = p;
  zero = make_expr_symbol (& nul);
  /* We do not use relaxation (yet).  */
  linkrelax = 0;
}

void
z80_md_end (void)
{
  int mach_type;

  if (ins_used & (INS_UNPORT | INS_R800))
    ins_used |= INS_UNDOC;

  switch (ins_used)
    {
    case INS_Z80:
      mach_type = bfd_mach_z80strict;
      break;
    case INS_Z80|INS_UNDOC:
      mach_type = bfd_mach_z80;
      break;
    case INS_Z80|INS_UNDOC|INS_UNPORT:
      mach_type = bfd_mach_z80full;
      break;
    case INS_Z80|INS_UNDOC|INS_R800:
      mach_type = bfd_mach_r800;
      break;
    default:
      mach_type = 0;
    }

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, mach_type);
}

static const char *
skip_space (const char *s)
{
  while (*s == ' ' || *s == '\t')
    ++s;
  return s;
}

/* A non-zero return-value causes a continue in the
   function read_a_source_file () in ../read.c.  */
int
z80_start_line_hook (void)
{
  char *p, quote;
  char buf[4];

  /* Convert one character constants.  */
  for (p = input_line_pointer; *p && *p != '\n'; ++p)
    {
      switch (*p)
	{
	case '\'':
	  if (p[1] != 0 && p[1] != '\'' && p[2] == '\'')
	    {
	      snprintf (buf, 4, "%3d", (unsigned char)p[1]);
	      *p++ = buf[0];
	      *p++ = buf[1];
	      *p++ = buf[2];
	      break;
	    }
	case '"':
	  for (quote = *p++; quote != *p && '\n' != *p; ++p)
	    /* No escapes.  */ ;
	  if (quote != *p)
	    {
	      as_bad (_("-- unterminated string"));
	      ignore_rest_of_line ();
	      return 1;
	    }
	  break;
	}
    }
  /* Check for <label>[:] [.](EQU|DEFL) <value>.  */
  if (is_name_beginner (*input_line_pointer))
    {
      char c, *rest, *line_start;
      int len;
      symbolS * symbolP;

      line_start = input_line_pointer;
      LISTING_NEWLINE ();
      if (ignore_input ())
	return 0;

      c = get_symbol_end ();
      rest = input_line_pointer + 1;

      if (*rest == ':')
	++rest;
      if (*rest == ' ' || *rest == '\t')
	++rest;
      if (*rest == '.')
	++rest;
      if (strncasecmp (rest, "EQU", 3) == 0)
	len = 3;
      else if (strncasecmp (rest, "DEFL", 4) == 0)
	len = 4;
      else
	len = 0;
      if (len && (rest[len] == ' ' || rest[len] == '\t'))
	{
	  /* Handle assignment here.  */
	  input_line_pointer = rest + len;
	  if (line_start[-1] == '\n')
	    bump_line_counters ();
	  /* Most Z80 assemblers require the first definition of a
             label to use "EQU" and redefinitions to have "DEFL".  */
	  if (len == 3 && (symbolP = symbol_find (line_start)) != NULL) 
	    {
	      if (S_IS_DEFINED (symbolP) || symbol_equated_p (symbolP))
		as_bad (_("symbol `%s' is already defined"), line_start);
	    }
	  equals (line_start, 1);
	  return 1;
	}
      else
	{
	  /* Restore line and pointer.  */
	  *input_line_pointer = c;
	  input_line_pointer = line_start;
	}
    }
  return 0;
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return NULL;
}

char *
md_atof (int type ATTRIBUTE_UNUSED, char *litP ATTRIBUTE_UNUSED,
	 int *sizeP ATTRIBUTE_UNUSED)
{
  return _("floating point numbers are not implemented");
}

valueT
md_section_align (segT seg ATTRIBUTE_UNUSED, valueT size)
{
  return size;
}

long
md_pcrel_from (fixS * fixp)
{
  return fixp->fx_where +
    fixp->fx_frag->fr_address + 1;
}

typedef const char * (asfunc)(char, char, const char*);

typedef struct _table_t
{
  char* name;
  char prefix;
  char opcode;
  asfunc * fp;
} table_t;

/* Compares the key for structs that start with a char * to the key.  */
static int
key_cmp (const void * a, const void * b)
{
  const char *str_a, *str_b;

  str_a = *((const char**)a);
  str_b = *((const char**)b);
  return strcmp (str_a, str_b);
}

#define BUFLEN 8 /* Large enough for any keyword.  */

char buf[BUFLEN];
const char *key = buf;

#define R_STACKABLE (0x80)
#define R_ARITH     (0x40)
#define R_IX        (0x20)
#define R_IY        (0x10)
#define R_INDEX     (R_IX | R_IY)

#define REG_A (7)
#define REG_B (0)
#define REG_C (1)
#define REG_D (2)
#define REG_E (3)
#define REG_H (4)
#define REG_L (5)
#define REG_F (6 | 8)
#define REG_I (9)
#define REG_R (10)

#define REG_AF (3 | R_STACKABLE)
#define REG_BC (0 | R_STACKABLE | R_ARITH)
#define REG_DE (1 | R_STACKABLE | R_ARITH)
#define REG_HL (2 | R_STACKABLE | R_ARITH)
#define REG_SP (3 | R_ARITH)

static const struct reg_entry
{
  char* name;
  int number;
} regtable[] =
{
  {"a",  REG_A },
  {"af", REG_AF },
  {"b",  REG_B },
  {"bc", REG_BC },
  {"c",  REG_C },
  {"d",  REG_D },
  {"de", REG_DE },
  {"e",  REG_E },
  {"f",  REG_F },
  {"h",  REG_H },
  {"hl", REG_HL },
  {"i",  REG_I },
  {"ix", REG_HL | R_IX },
  {"ixh",REG_H | R_IX },
  {"ixl",REG_L | R_IX },
  {"iy", REG_HL | R_IY },
  {"iyh",REG_H | R_IY },
  {"iyl",REG_L | R_IY },
  {"l",  REG_L },
  {"r",  REG_R },
  {"sp", REG_SP },
} ;

/* Prevent an error on a line from also generating
   a "junk at end of line" error message.  */
static char err_flag;

static void
error (const char * message)
{
  as_bad (message);
  err_flag = 1;
}

static void
ill_op (void)
{
  error (_("illegal operand"));
}

static void
wrong_mach (int ins_type)
{
  const char *p;

  switch (ins_type)
    {
    case INS_UNDOC:
      p = "undocumented instruction";
      break;
    case INS_UNPORT:
      p = "instruction does not work on R800";
      break;
    case INS_R800:
      p = "instruction only works R800";
      break;
    default:
      p = 0; /* Not reachable.  */
    }

  if (ins_type & ins_err)
    error (_(p));
  else
    as_warn (_(p));
}

static void
check_mach (int ins_type)
{
  if ((ins_type & ins_ok) == 0)
    wrong_mach (ins_type);
  ins_used |= ins_type;
}

/* Check whether an expression is indirect.  */
static int
is_indir (const char *s)
{
  char quote;
  const char *p;
  int indir, depth;

  /* Indirection is indicated with parentheses.  */
  indir = (*s == '(');

  for (p = s, depth = 0; *p && *p != ','; ++p)
    {
      switch (*p)
	{
	case '"':
	case '\'':
	  for (quote = *p++; quote != *p && *p != '\n'; ++p)
	    if (*p == '\\' && p[1])
	      ++p;
	  break;
	case '(':
	  ++ depth;
	  break;
	case ')':
	  -- depth;
	  if (depth == 0)
	    {
	      p = skip_space (p + 1);
	      if (*p && *p != ',')
		indir = 0;
	      --p;
	    }
	  if (depth < 0)
	    error (_("mismatched parentheses"));
	  break;
	}
    }

  if (depth != 0)
    error (_("mismatched parentheses"));

  return indir;
}

/* Parse general expression.  */
static const char *
parse_exp2 (const char *s, expressionS *op, segT *pseg)
{
  const char *p;
  int indir;
  int i;
  const struct reg_entry * regp;
  expressionS offset;

  p = skip_space (s);
  op->X_md = indir = is_indir (p);
  if (indir)
    p = skip_space (p + 1);

  for (i = 0; i < BUFLEN; ++i)
    {
      if (!ISALPHA (p[i])) /* Register names consist of letters only.  */
	break;
      buf[i] = TOLOWER (p[i]);
    }

  if ((i < BUFLEN) && ((p[i] == 0) || (strchr (")+-, \t", p[i]))))
    {
      buf[i] = 0;
      regp = bsearch (& key, regtable, ARRAY_SIZE (regtable),
		      sizeof (regtable[0]), key_cmp);
      if (regp)
	{
	  *pseg = reg_section;
	  op->X_add_symbol = op->X_op_symbol = 0;
	  op->X_add_number = regp->number;
	  op->X_op = O_register;
	  p += strlen (regp->name);
	  p = skip_space (p);
	  if (indir)
	    {
	      if (*p == ')')
		++p;
	      if ((regp->number & R_INDEX) && (regp->number & R_ARITH))
		{
		  op->X_op = O_md1;

		  if  ((*p == '+') || (*p == '-'))
		    {
		      input_line_pointer = (char*) p;
		      expression (& offset);
		      p = skip_space (input_line_pointer);
		      if (*p != ')')
			error (_("bad offset expression syntax"));
		      else
			++ p;
		      op->X_add_symbol = make_expr_symbol (& offset);
		      return p;
		    }

		  /* We treat (i[xy]) as (i[xy]+0), which is how it will
		     end up anyway, unless we're processing jp (i[xy]).  */
		  op->X_add_symbol = zero;
		}
	    }
	  p = skip_space (p);

	  if ((*p == 0) || (*p == ','))
	    return p;
	}
    }
  /* Not an argument involving a register; use the generic parser.  */
  input_line_pointer = (char*) s ;
  *pseg = expression (op);
  if (op->X_op == O_absent)
    error (_("missing operand"));
  if (op->X_op == O_illegal)
    error (_("bad expression syntax"));
  return input_line_pointer;
}

static const char *
parse_exp (const char *s, expressionS *op)
{
  segT dummy;
  return parse_exp2 (s, op, & dummy);
}

/* Condition codes, including some synonyms provided by HiTech zas.  */
static const struct reg_entry cc_tab[] =
{
  { "age", 6 << 3 },
  { "alt", 7 << 3 },
  { "c",   3 << 3 },
  { "di",  4 << 3 },
  { "ei",  5 << 3 },
  { "lge", 2 << 3 },
  { "llt", 3 << 3 },
  { "m",   7 << 3 },
  { "nc",  2 << 3 },
  { "nz",  0 << 3 },
  { "p",   6 << 3 },
  { "pe",  5 << 3 },
  { "po",  4 << 3 },
  { "z",   1 << 3 },
} ;

/* Parse condition code.  */
static const char *
parse_cc (const char *s, char * op)
{
  const char *p;
  int i;
  struct reg_entry * cc_p;

  for (i = 0; i < BUFLEN; ++i)
    {
      if (!ISALPHA (s[i])) /* Condition codes consist of letters only.  */
	break;
      buf[i] = TOLOWER (s[i]);
    }

  if ((i < BUFLEN)
      && ((s[i] == 0) || (s[i] == ',')))
    {
      buf[i] = 0;
      cc_p = bsearch (&key, cc_tab, ARRAY_SIZE (cc_tab),
		      sizeof (cc_tab[0]), key_cmp);
    }
  else
    cc_p = NULL;

  if (cc_p)
    {
      *op = cc_p->number;
      p = s + i;
    }
  else
    p = NULL;

  return p;
}

static const char *
emit_insn (char prefix, char opcode, const char * args)
{
  char *p;

  if (prefix)
    {
      p = frag_more (2);
      *p++ = prefix;
    }
  else
    p = frag_more (1);
  *p = opcode;
  return args;
}

void z80_cons_fix_new (fragS *frag_p, int offset, int nbytes, expressionS *exp)
{
  bfd_reloc_code_real_type r[4] =
    {
      BFD_RELOC_8,
      BFD_RELOC_16,
      BFD_RELOC_24,
      BFD_RELOC_32
    };

  if (nbytes < 1 || nbytes > 4) 
    {
      as_bad (_("unsupported BFD relocation size %u"), nbytes);
    }
  else
    {
      fix_new_exp (frag_p, offset, nbytes, exp, 0, r[nbytes-1]);
    }
}

static void
emit_byte (expressionS * val, bfd_reloc_code_real_type r_type)
{
  char *p;
  int lo, hi;
  fixS * fixp;

  p = frag_more (1);
  *p = val->X_add_number;
  if ((r_type == BFD_RELOC_8_PCREL) && (val->X_op == O_constant))
    {
      as_bad(_("cannot make a relative jump to an absolute location"));
    }
  else if (val->X_op == O_constant)
    {
      lo = -128;
      hi = (BFD_RELOC_8 == r_type) ? 255 : 127;

      if ((val->X_add_number < lo) || (val->X_add_number > hi))
	{
	  if (r_type == BFD_RELOC_Z80_DISP8)
	    as_bad (_("offset too large"));
	  else
	    as_warn (_("overflow"));
	}
    }
  else
    {
      fixp = fix_new_exp (frag_now, p - frag_now->fr_literal, 1, val,
			  (r_type == BFD_RELOC_8_PCREL) ? TRUE : FALSE, r_type);
      /* FIXME : Process constant offsets immediately.  */
    }
}

static void
emit_word (expressionS * val)
{
  char *p;

  p = frag_more (2);
  if (   (val->X_op == O_register)
      || (val->X_op == O_md1))
    ill_op ();
  else
    {
      *p = val->X_add_number;
      p[1] = (val->X_add_number>>8);
      if (val->X_op != O_constant)
	fix_new_exp (frag_now, p - frag_now->fr_literal, 2,
		     val, FALSE, BFD_RELOC_16);
    }
}

static void
emit_mx (char prefix, char opcode, int shift, expressionS * arg)
     /* The operand m may be r, (hl), (ix+d), (iy+d),
	if 0 == prefix m may also be ixl, ixh, iyl, iyh.  */
{
  char *q;
  int rnum;

  rnum = arg->X_add_number;
  switch (arg->X_op)
    {
    case O_register:
      if (arg->X_md)
	{
	  if (rnum != REG_HL)
	    {
	      ill_op ();
	      break;
	    }
	  else
	    rnum = 6;
	}
      else
	{
	  if ((prefix == 0) && (rnum & R_INDEX))
	    {
	      prefix = (rnum & R_IX) ? 0xDD : 0xFD;
	      check_mach (INS_UNDOC);
	      rnum &= ~R_INDEX;
	    }
	  if (rnum > 7)
	    {
	      ill_op ();
	      break;
	    }
	}
      q = frag_more (prefix ? 2 : 1);
      if (prefix)
	* q ++ = prefix;
      * q ++ = opcode + (rnum << shift);
      break;
    case O_md1:
      q = frag_more (2);
      *q++ = (rnum & R_IX) ? 0xDD : 0xFD;
      *q = (prefix) ? prefix : (opcode + (6 << shift));
      emit_byte (symbol_get_value_expression (arg->X_add_symbol),
		 BFD_RELOC_Z80_DISP8);
      if (prefix)
	{
	  q = frag_more (1);
	  *q = opcode+(6<<shift);
	}
      break;
    default:
      abort ();
    }
}

/* The operand m may be r, (hl), (ix+d), (iy+d),
   if 0 = prefix m may also be ixl, ixh, iyl, iyh.  */
static const char *
emit_m (char prefix, char opcode, const char *args)
{
  expressionS arg_m;
  const char *p;

  p = parse_exp (args, &arg_m);
  switch (arg_m.X_op)
    {
    case O_md1:
    case O_register:
      emit_mx (prefix, opcode, 0, &arg_m);
      break;
    default:
      ill_op ();
    }
  return p;
}

/* The operand m may be as above or one of the undocumented
   combinations (ix+d),r and (iy+d),r (if unportable instructions
   are allowed).  */
static const char *
emit_mr (char prefix, char opcode, const char *args)
{
  expressionS arg_m, arg_r;
  const char *p;

  p = parse_exp (args, & arg_m);

  switch (arg_m.X_op)
    {
    case O_md1:
      if (*p == ',')
	{
	  p = parse_exp (p + 1, & arg_r);

	  if ((arg_r.X_md == 0)
	      && (arg_r.X_op == O_register)
	      && (arg_r.X_add_number < 8))
	    opcode += arg_r.X_add_number-6; /* Emit_mx () will add 6.  */
	  else
	    {
	      ill_op ();
	      break;
	    }
	  check_mach (INS_UNPORT);
	}
    case O_register:
      emit_mx (prefix, opcode, 0, & arg_m);
      break;
    default:
      ill_op ();
    }
  return p;
}

static void
emit_sx (char prefix, char opcode, expressionS * arg_p)
{
  char *q;

  switch (arg_p->X_op)
    {
    case O_register:
    case O_md1:
      emit_mx (prefix, opcode, 0, arg_p);
      break;
    default:
      if (arg_p->X_md)
	ill_op ();
      else
	{
	  q = frag_more (prefix ? 2 : 1);
	  if (prefix)
	    *q++ = prefix;
	  *q = opcode ^ 0x46;
	  emit_byte (arg_p, BFD_RELOC_8);
	}
    }
}

/* The operand s may be r, (hl), (ix+d), (iy+d), n.  */
static const char *
emit_s (char prefix, char opcode, const char *args)
{
  expressionS arg_s;
  const char *p;

  p = parse_exp (args, & arg_s);
  emit_sx (prefix, opcode, & arg_s);
  return p;
}

static const char *
emit_call (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  expressionS addr;
  const char *p;  char *q;

  p = parse_exp (args, &addr);
  if (addr.X_md)
    ill_op ();
  else
    {
      q = frag_more (1);
      *q = opcode;
      emit_word (& addr);
    }
  return p;
}

/* Operand may be rr, r, (hl), (ix+d), (iy+d).  */
static const char *
emit_incdec (char prefix, char opcode, const char * args)
{
  expressionS operand;
  int rnum;
  const char *p;  char *q;

  p = parse_exp (args, &operand);
  rnum = operand.X_add_number;
  if ((! operand.X_md)
      && (operand.X_op == O_register)
      && (R_ARITH&rnum))
    {
      q = frag_more ((rnum & R_INDEX) ? 2 : 1);
      if (rnum & R_INDEX)
	*q++ = (rnum & R_IX) ? 0xDD : 0xFD;
      *q = prefix + ((rnum & 3) << 4);
    }
  else
    {
      if ((operand.X_op == O_md1) || (operand.X_op == O_register))
	emit_mx (0, opcode, 3, & operand);
      else
	ill_op ();
    }
  return p;
}

static const char *
emit_jr (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  expressionS addr;
  const char *p;
  char *q;

  p = parse_exp (args, &addr);
  if (addr.X_md)
    ill_op ();
  else
    {
      q = frag_more (1);
      *q = opcode;
      emit_byte (&addr, BFD_RELOC_8_PCREL);
    }
  return p;
}

static const char *
emit_jp (char prefix, char opcode, const char * args)
{
  expressionS addr;
  const char *p;
  char *q;
  int rnum;

  p = parse_exp (args, & addr);
  if (addr.X_md)
    {
      rnum = addr.X_add_number;
      if ((addr.X_op == O_register && (rnum & ~R_INDEX) == REG_HL)
	 /* An operand (i[xy]) would have been rewritten to (i[xy]+0)
            in parse_exp ().  */
	  || (addr.X_op == O_md1 && addr.X_add_symbol == zero))
	{
	  q = frag_more ((rnum & R_INDEX) ? 2 : 1);
	  if (rnum & R_INDEX)
	    *q++ = (rnum & R_IX) ? 0xDD : 0xFD;
	  *q = prefix;
	}
      else
	ill_op ();
    }
  else
    {
      q = frag_more (1);
      *q = opcode;
      emit_word (& addr);
    }
  return p;
}

static const char *
emit_im (char prefix, char opcode, const char * args)
{
  expressionS mode;
  const char *p;
  char *q;

  p = parse_exp (args, & mode);
  if (mode.X_md || (mode.X_op != O_constant))
    ill_op ();
  else
    switch (mode.X_add_number)
      {
      case 1:
      case 2:
	++mode.X_add_number;
	/* Fall through.  */
      case 0:
	q = frag_more (2);
	*q++ = prefix;
	*q = opcode + 8*mode.X_add_number;
	break;
      default:
	ill_op ();
      }
  return p;
}

static const char *
emit_pop (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  expressionS regp;
  const char *p;
  char *q;

  p = parse_exp (args, & regp);
  if ((!regp.X_md)
      && (regp.X_op == O_register)
      && (regp.X_add_number & R_STACKABLE))
    {
      int rnum;

      rnum = regp.X_add_number;
      if (rnum&R_INDEX)
	{
	  q = frag_more (2);
	  *q++ = (rnum&R_IX)?0xDD:0xFD;
	}
      else
	q = frag_more (1);
      *q = opcode + ((rnum & 3) << 4);
    }
  else
    ill_op ();

  return p;
}

static const char *
emit_retcc (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  char cc, *q;
  const char *p;

  p = parse_cc (args, &cc);
  q = frag_more (1);
  if (p)
    *q = opcode + cc;
  else
    *q = prefix;
  return p ? p : args;
}

static const char *
emit_adc (char prefix, char opcode, const char * args)
{
  expressionS term;
  int rnum;
  const char *p;
  char *q;

  p = parse_exp (args, &term);
  if (*p++ != ',')
    {
      error (_("bad intruction syntax"));
      return p;
    }

  if ((term.X_md) || (term.X_op != O_register))
    ill_op ();
  else
    switch (term.X_add_number)
      {
      case REG_A:
	p = emit_s (0, prefix, p);
	break;
      case REG_HL:
	p = parse_exp (p, &term);
	if ((!term.X_md) && (term.X_op == O_register))
	  {
	    rnum = term.X_add_number;
	    if (R_ARITH == (rnum & (R_ARITH | R_INDEX)))
	      {
		q = frag_more (2);
		*q++ = 0xED;
		*q = opcode + ((rnum & 3) << 4);
		break;
	      }
	  }
	/* Fall through.  */
      default:
	ill_op ();
      }
  return p;
}

static const char *
emit_add (char prefix, char opcode, const char * args)
{
  expressionS term;
  int lhs, rhs;
  const char *p;
  char *q;

  p = parse_exp (args, &term);
  if (*p++ != ',')
    {
      error (_("bad intruction syntax"));
      return p;
    }

  if ((term.X_md) || (term.X_op != O_register))
    ill_op ();
  else
    switch (term.X_add_number & ~R_INDEX)
      {
      case REG_A:
	p = emit_s (0, prefix, p);
	break;
      case REG_HL:
	lhs = term.X_add_number;
	p = parse_exp (p, &term);
	if ((!term.X_md) && (term.X_op == O_register))
	  {
	    rhs = term.X_add_number;
	    if ((rhs & R_ARITH)
		&& ((rhs == lhs) || ((rhs & ~R_INDEX) != REG_HL)))
	      {
		q = frag_more ((lhs & R_INDEX) ? 2 : 1);
		if (lhs & R_INDEX)
		  *q++ = (lhs & R_IX) ? 0xDD : 0xFD;
		*q = opcode + ((rhs & 3) << 4);
		break;
	      }
	  }
	/* Fall through.  */
      default:
	ill_op ();
      }
  return p;
}

static const char *
emit_bit (char prefix, char opcode, const char * args)
{
  expressionS b;
  int bn;
  const char *p;

  p = parse_exp (args, &b);
  if (*p++ != ',')
    error (_("bad intruction syntax"));

  bn = b.X_add_number;
  if ((!b.X_md)
      && (b.X_op == O_constant)
      && (0 <= bn)
      && (bn < 8))
    {
      if (opcode == 0x40)
	/* Bit : no optional third operand.  */
	p = emit_m (prefix, opcode + (bn << 3), p);
      else
	/* Set, res : resulting byte can be copied to register.  */
	p = emit_mr (prefix, opcode + (bn << 3), p);
    }
  else
    ill_op ();
  return p;
}

static const char *
emit_jpcc (char prefix, char opcode, const char * args)
{
  char cc;
  const char *p;

  p = parse_cc (args, & cc);
  if (p && *p++ == ',')
    p = emit_call (0, opcode + cc, p);
  else
    p = (prefix == (char)0xC3)
      ? emit_jp (0xE9, prefix, args)
      : emit_call (0, prefix, args);
  return p;
}

static const char *
emit_jrcc (char prefix, char opcode, const char * args)
{
  char cc;
  const char *p;

  p = parse_cc (args, &cc);
  if (p && *p++ == ',')
    {
      if (cc > (3 << 3))
	error (_("condition code invalid for jr"));
      else
	p = emit_jr (0, opcode + cc, p);
    }
  else
    p = emit_jr (0, prefix, args);

  return p;
}

static const char *
emit_ex (char prefix_in ATTRIBUTE_UNUSED,
	 char opcode_in ATTRIBUTE_UNUSED, const char * args)
{
  expressionS op;
  const char * p;
  char prefix, opcode;

  p = parse_exp (args, &op);
  p = skip_space (p);
  if (*p++ != ',')
    {
      error (_("bad instruction syntax"));
      return p;
    }

  prefix = opcode = 0;
  if (op.X_op == O_register)
    switch (op.X_add_number | (op.X_md ? 0x8000 : 0))
      {
      case REG_AF:
	if (TOLOWER (*p++) == 'a' && TOLOWER (*p++) == 'f')
	  {
	    /* The scrubber changes '\'' to '`' in this context.  */
	    if (*p == '`')
	      ++p;
	    opcode = 0x08;
	  }
	break;
      case REG_DE:
	if (TOLOWER (*p++) == 'h' && TOLOWER (*p++) == 'l')
	  opcode = 0xEB;
	break;
      case REG_SP|0x8000:
	p = parse_exp (p, & op);
	if (op.X_op == O_register
	    && op.X_md == 0
	    && (op.X_add_number & ~R_INDEX) == REG_HL)
	  {
	    opcode = 0xE3;
	    if (R_INDEX & op.X_add_number)
	      prefix = (R_IX & op.X_add_number) ? 0xDD : 0xFD;
	  }
	break;
      }
  if (opcode)
    emit_insn (prefix, opcode, p);
  else
    ill_op ();

  return p;
}

static const char *
emit_in (char prefix ATTRIBUTE_UNUSED, char opcode ATTRIBUTE_UNUSED,
	const char * args)
{
  expressionS reg, port;
  const char *p;
  char *q;

  p = parse_exp (args, &reg);
  if (*p++ != ',')
    {
      error (_("bad intruction syntax"));
      return p;
    }

  p = parse_exp (p, &port);
  if (reg.X_md == 0
      && reg.X_op == O_register
      && (reg.X_add_number <= 7 || reg.X_add_number == REG_F)
      && (port.X_md))
    {
      if (port.X_op != O_md1 && port.X_op != O_register)
	{
	  if (REG_A == reg.X_add_number)
	    {
	      q = frag_more (1);
	      *q = 0xDB;
	      emit_byte (&port, BFD_RELOC_8);
	    }
	  else
	    ill_op ();
	}
      else
	{
	  if (port.X_add_number == REG_C)
	    {
	      if (reg.X_add_number == REG_F)
		check_mach (INS_UNDOC);
	      else
		{
		  q = frag_more (2);
		  *q++ = 0xED;
		  *q = 0x40|((reg.X_add_number&7)<<3);
		}
	    }
	  else
	    ill_op ();
	}
    }
  else
    ill_op ();
  return p;
}

static const char *
emit_out (char prefix ATTRIBUTE_UNUSED, char opcode ATTRIBUTE_UNUSED,
	 const char * args)
{
  expressionS reg, port;
  const char *p;
  char *q;

  p = parse_exp (args, & port);
  if (*p++ != ',')
    {
      error (_("bad intruction syntax"));
      return p;
    }
  p = parse_exp (p, &reg);
  if (!port.X_md)
    { ill_op (); return p; }
  /* Allow "out (c), 0" as unportable instruction.  */
  if (reg.X_op == O_constant && reg.X_add_number == 0)
    {
      check_mach (INS_UNPORT);
      reg.X_op = O_register;
      reg.X_add_number = 6;
    }
  if (reg.X_md
      || reg.X_op != O_register
      || reg.X_add_number > 7)
    ill_op ();
  else
    if (port.X_op != O_register && port.X_op != O_md1)
      {
	if (REG_A == reg.X_add_number)
	  {
	    q = frag_more (1);
	    *q = 0xD3;
	    emit_byte (&port, BFD_RELOC_8);
	  }
	else
	  ill_op ();
      }
    else
      {
	if (REG_C == port.X_add_number)
	  {
	    q = frag_more (2);
	    *q++ = 0xED;
	    *q = 0x41 | (reg.X_add_number << 3);
	  }
	else
	  ill_op ();
      }
  return p;
}

static const char *
emit_rst (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  expressionS addr;
  const char *p;
  char *q;

  p = parse_exp (args, &addr);
  if (addr.X_op != O_constant)
    {
      error ("rst needs constant address");
      return p;
    }

  if (addr.X_add_number & ~(7 << 3))
    ill_op ();
  else
    {
      q = frag_more (1);
      *q = opcode + (addr.X_add_number & (7 << 3));
    }
  return p;
}

static void
emit_ldxhl (char prefix, char opcode, expressionS *src, expressionS *d)
{
  char *q;

  if (src->X_md)
    ill_op ();
  else
    {
      if (src->X_op == O_register)
	{
	  if (src->X_add_number>7)
	    ill_op ();
	  if (prefix)
	    {
	      q = frag_more (2);
	      *q++ = prefix;
	    }
	  else
	q = frag_more (1);
	  *q = opcode + src->X_add_number;
	  if (d)
	    emit_byte (d, BFD_RELOC_Z80_DISP8);
	}
      else
	{
	  if (prefix)
	    {
	      q = frag_more (2);
	      *q++ = prefix;
	    }
	  else
	    q = frag_more (1);
	  *q = opcode^0x46;
	  if (d)
	    emit_byte (d, BFD_RELOC_Z80_DISP8);
	  emit_byte (src, BFD_RELOC_8);
	}
    }
}

static void
emit_ldreg (int dest, expressionS * src)
{
  char *q;
  int rnum;

  switch (dest)
    {
      /* 8 Bit ld group:  */
    case REG_I:
    case REG_R:
      if (src->X_md == 0 && src->X_op == O_register && src->X_add_number == REG_A)
	{
	  q = frag_more (2);
	  *q++ = 0xED;
	  *q = (dest == REG_I) ? 0x47 : 0x4F;
	}
      else
	ill_op ();
      break;

    case REG_A:
      if ((src->X_md) && src->X_op != O_register && src->X_op != O_md1)
	{
	  q = frag_more (1);
	  *q = 0x3A;
	  emit_word (src);
	  break;
	}

      if ((src->X_md)
	  && src->X_op == O_register
	  && (src->X_add_number == REG_BC || src->X_add_number == REG_DE))
	{
	  q = frag_more (1);
	  *q = 0x0A + ((dest & 1) << 4);
	  break;
	}

      if ((!src->X_md)
	  && src->X_op == O_register
	  && (src->X_add_number == REG_R || src->X_add_number == REG_I))
	{
	  q = frag_more (2);
	  *q++ = 0xED;
	  *q = (src->X_add_number == REG_I) ? 0x57 : 0x5F;
	  break;
	}
      /* Fall through.  */
    case REG_B:
    case REG_C:
    case REG_D:
    case REG_E:
      emit_sx (0, 0x40 + (dest << 3), src);
      break;

    case REG_H:
    case REG_L:
      if ((src->X_md == 0)
	  && (src->X_op == O_register)
	  && (src->X_add_number & R_INDEX))
	ill_op ();
      else
	emit_sx (0, 0x40 + (dest << 3), src);
      break;

    case R_IX | REG_H:
    case R_IX | REG_L:
    case R_IY | REG_H:
    case R_IY | REG_L:
      if (src->X_md)
	{
	  ill_op ();
	  break;
	}
      check_mach (INS_UNDOC);
      if (src-> X_op == O_register)
	{
	  rnum = src->X_add_number;
	  if ((rnum & ~R_INDEX) < 8
	      && ((rnum & R_INDEX) == (dest & R_INDEX)
		   || (   (rnum & ~R_INDEX) != REG_H
		       && (rnum & ~R_INDEX) != REG_L)))
	    {
	      q = frag_more (2);
	      *q++ = (dest & R_IX) ? 0xDD : 0xFD;
	      *q = 0x40 + ((dest & 0x07) << 3) + (rnum & 7);
	    }
	  else
	    ill_op ();
	}
      else
	{
	  q = frag_more (2);
	  *q++ = (dest & R_IX) ? 0xDD : 0xFD;
	  *q = 0x06 + ((dest & 0x07) << 3);
	  emit_byte (src, BFD_RELOC_8);
	}
      break;

      /* 16 Bit ld group:  */
    case REG_SP:
      if (src->X_md == 0
	  && src->X_op == O_register
	  && REG_HL == (src->X_add_number &~ R_INDEX))
	{
	  q = frag_more ((src->X_add_number & R_INDEX) ? 2 : 1);
	  if (src->X_add_number & R_INDEX)
	    *q++ = (src->X_add_number & R_IX) ? 0xDD : 0xFD;
	  *q = 0xF9;
	  break;
	}
      /* Fall through.  */
    case REG_BC:
    case REG_DE:
      if (src->X_op == O_register || src->X_op == O_md1)
	ill_op ();
      q = frag_more (src->X_md ? 2 : 1);
      if (src->X_md)
	{
	  *q++ = 0xED;
	  *q = 0x4B + ((dest & 3) << 4);
	}
      else
	*q = 0x01 + ((dest & 3) << 4);
      emit_word (src);
      break;

    case REG_HL:
    case REG_HL | R_IX:
    case REG_HL | R_IY:
      if (src->X_op == O_register || src->X_op == O_md1)
	ill_op ();
      q = frag_more ((dest & R_INDEX) ? 2 : 1);
      if (dest & R_INDEX)
	* q ++ = (dest & R_IX) ? 0xDD : 0xFD;
      *q = (src->X_md) ? 0x2A : 0x21;
      emit_word (src);
      break;

    case REG_AF:
    case REG_F:
      ill_op ();
      break;

    default:
      abort ();
    }
}

static const char *
emit_ld (char prefix_in ATTRIBUTE_UNUSED, char opcode_in ATTRIBUTE_UNUSED,
	const char * args)
{
  expressionS dst, src;
  const char *p;
  char *q;
  char prefix, opcode;

  p = parse_exp (args, &dst);
  if (*p++ != ',')
    error (_("bad intruction syntax"));
  p = parse_exp (p, &src);

  switch (dst.X_op)
    {
    case O_md1:
      emit_ldxhl ((dst.X_add_number & R_IX) ? 0xDD : 0xFD, 0x70,
		  &src, symbol_get_value_expression (dst.X_add_symbol));
      break;

    case O_register:
      if (dst.X_md)
	{
	  switch (dst.X_add_number)
	    {
	    case REG_BC:
	    case REG_DE:
	      if (src.X_md == 0 && src.X_op == O_register && src.X_add_number == REG_A)
		{
		  q = frag_more (1);
		  *q = 0x02 + ( (dst.X_add_number & 1) << 4);
		}
	      else
		ill_op ();
	      break;
	    case REG_HL:
	      emit_ldxhl (0, 0x70, &src, NULL);
	      break;
	    default:
	      ill_op ();
	    }
	}
      else
	emit_ldreg (dst.X_add_number, &src);
      break;

    default:
      if (src.X_md != 0 || src.X_op != O_register)
	ill_op ();
      prefix = opcode = 0;
      switch (src.X_add_number)
	{
	case REG_A:
	  opcode = 0x32; break;
	case REG_BC: case REG_DE: case REG_SP:
	  prefix = 0xED; opcode = 0x43 + ((src.X_add_number&3)<<4); break;
	case REG_HL:
	  opcode = 0x22; break;
	case REG_HL|R_IX:
	  prefix = 0xDD; opcode = 0x22; break;
	case REG_HL|R_IY:
	  prefix = 0xFD; opcode = 0x22; break;
	}
      if (opcode)
	{
	  q = frag_more (prefix?2:1);
	  if (prefix)
	    *q++ = prefix;
	  *q = opcode;
	  emit_word (&dst);
	}
      else
	ill_op ();
    }
  return p;
}

static void
emit_data (int size ATTRIBUTE_UNUSED)
{
  const char *p, *q;
  char *u, quote;
  int cnt;
  expressionS exp;

  if (is_it_end_of_statement ())
    {
      demand_empty_rest_of_line ();
      return;
    }
  p = skip_space (input_line_pointer);

  do
    {
      if (*p == '\"' || *p == '\'')
	{
	    for (quote = *p, q = ++p, cnt = 0; *p && quote != *p; ++p, ++cnt)
	      ;
	    u = frag_more (cnt);
	    memcpy (u, q, cnt);
	    if (!*p)
	      as_warn (_("unterminated string"));
	    else
	      p = skip_space (p+1);
	}
      else
	{
	  p = parse_exp (p, &exp);
	  if (exp.X_op == O_md1 || exp.X_op == O_register)
	    {
	      ill_op ();
	      break;
	    }
	  if (exp.X_md)
	    as_warn (_("parentheses ignored"));
	  emit_byte (&exp, BFD_RELOC_8);
	  p = skip_space (p);
	}
    }
  while (*p++ == ',') ;
  input_line_pointer = (char *)(p-1);
}

static const char *
emit_mulub (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  const char *p;

  p = skip_space (args);
  if (TOLOWER (*p++) != 'a' || *p++ != ',')
    ill_op ();
  else
    {
      char *q, reg;

      reg = TOLOWER (*p++);
      switch (reg)
	{
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	  check_mach (INS_R800);
	  if (!*skip_space (p))
	    {
	      q = frag_more (2);
	      *q++ = prefix;
	      *q = opcode + ((reg - 'b') << 3);
	      break;
	    }
	default:
	  ill_op ();
	}
    }
  return p;
}

static const char *
emit_muluw (char prefix ATTRIBUTE_UNUSED, char opcode, const char * args)
{
  const char *p;

  p = skip_space (args);
  if (TOLOWER (*p++) != 'h' || TOLOWER (*p++) != 'l' || *p++ != ',')
    ill_op ();
  else
    {
      expressionS reg;
      char *q;

      p = parse_exp (p, & reg);

      if ((!reg.X_md) && reg.X_op == O_register)
	switch (reg.X_add_number)
	  {
	  case REG_BC:
	  case REG_SP:
	    check_mach (INS_R800);
	    q = frag_more (2);
	    *q++ = prefix;
	    *q = opcode + ((reg.X_add_number & 3) << 4);
	    break;
	  default:
	    ill_op ();
	  }
    }
  return p;
}

/* Port specific pseudo ops.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "db" , emit_data, 1},
  { "d24", cons, 3},
  { "d32", cons, 4},
  { "def24", cons, 3},
  { "def32", cons, 4},
  { "defb", emit_data, 1},  
  { "defs", s_space, 1}, /* Synonym for ds on some assemblers.  */
  { "defw", cons, 2},
  { "ds",   s_space, 1}, /* Fill with bytes rather than words.  */
  { "dw", cons, 2},
  { "psect", obj_coff_section, 0}, /* TODO: Translate attributes.  */
  { "set", 0, 0}, 		/* Real instruction on z80.  */
  { NULL, 0, 0 }
} ;

static table_t instab[] =
{
  { "adc",  0x88, 0x4A, emit_adc },
  { "add",  0x80, 0x09, emit_add },
  { "and",  0x00, 0xA0, emit_s },
  { "bit",  0xCB, 0x40, emit_bit },
  { "call", 0xCD, 0xC4, emit_jpcc },
  { "ccf",  0x00, 0x3F, emit_insn },
  { "cp",   0x00, 0xB8, emit_s },
  { "cpd",  0xED, 0xA9, emit_insn },
  { "cpdr", 0xED, 0xB9, emit_insn },
  { "cpi",  0xED, 0xA1, emit_insn },
  { "cpir", 0xED, 0xB1, emit_insn },
  { "cpl",  0x00, 0x2F, emit_insn },
  { "daa",  0x00, 0x27, emit_insn },
  { "dec",  0x0B, 0x05, emit_incdec },
  { "di",   0x00, 0xF3, emit_insn },
  { "djnz", 0x00, 0x10, emit_jr },
  { "ei",   0x00, 0xFB, emit_insn },
  { "ex",   0x00, 0x00, emit_ex},
  { "exx",  0x00, 0xD9, emit_insn },
  { "halt", 0x00, 0x76, emit_insn },
  { "im",   0xED, 0x46, emit_im },
  { "in",   0x00, 0x00, emit_in },
  { "inc",  0x03, 0x04, emit_incdec },
  { "ind",  0xED, 0xAA, emit_insn },
  { "indr", 0xED, 0xBA, emit_insn },
  { "ini",  0xED, 0xA2, emit_insn },
  { "inir", 0xED, 0xB2, emit_insn },
  { "jp",   0xC3, 0xC2, emit_jpcc },
  { "jr",   0x18, 0x20, emit_jrcc },
  { "ld",   0x00, 0x00, emit_ld },
  { "ldd",  0xED, 0xA8, emit_insn },
  { "lddr", 0xED, 0xB8, emit_insn },
  { "ldi",  0xED, 0xA0, emit_insn },
  { "ldir", 0xED, 0xB0, emit_insn },
  { "mulub", 0xED, 0xC5, emit_mulub }, /* R800 only.  */
  { "muluw", 0xED, 0xC3, emit_muluw }, /* R800 only.  */
  { "neg",  0xed, 0x44, emit_insn },
  { "nop",  0x00, 0x00, emit_insn },
  { "or",   0x00, 0xB0, emit_s },
  { "otdr", 0xED, 0xBB, emit_insn },
  { "otir", 0xED, 0xB3, emit_insn },
  { "out",  0x00, 0x00, emit_out },
  { "outd", 0xED, 0xAB, emit_insn },
  { "outi", 0xED, 0xA3, emit_insn },
  { "pop",  0x00, 0xC1, emit_pop },
  { "push", 0x00, 0xC5, emit_pop },
  { "res",  0xCB, 0x80, emit_bit },
  { "ret",  0xC9, 0xC0, emit_retcc },
  { "reti", 0xED, 0x4D, emit_insn },
  { "retn", 0xED, 0x45, emit_insn },
  { "rl",   0xCB, 0x10, emit_mr },
  { "rla",  0x00, 0x17, emit_insn },
  { "rlc",  0xCB, 0x00, emit_mr },
  { "rlca", 0x00, 0x07, emit_insn },
  { "rld",  0xED, 0x6F, emit_insn },
  { "rr",   0xCB, 0x18, emit_mr },
  { "rra",  0x00, 0x1F, emit_insn },
  { "rrc",  0xCB, 0x08, emit_mr },
  { "rrca", 0x00, 0x0F, emit_insn },
  { "rrd",  0xED, 0x67, emit_insn },
  { "rst",  0x00, 0xC7, emit_rst},
  { "sbc",  0x98, 0x42, emit_adc },
  { "scf",  0x00, 0x37, emit_insn },
  { "set",  0xCB, 0xC0, emit_bit },
  { "sla",  0xCB, 0x20, emit_mr },
  { "sli",  0xCB, 0x30, emit_mr },
  { "sll",  0xCB, 0x30, emit_mr },
  { "sra",  0xCB, 0x28, emit_mr },
  { "srl",  0xCB, 0x38, emit_mr },
  { "sub",  0x00, 0x90, emit_s },
  { "xor",  0x00, 0xA8, emit_s },
} ;

void
md_assemble (char* str)
{
  const char *p;
  char * old_ptr;
  int i;
  table_t *insp;

  err_flag = 0;
  old_ptr = input_line_pointer;
  p = skip_space (str);
  for (i = 0; (i < BUFLEN) && (ISALPHA (*p));)
    buf[i++] = TOLOWER (*p++);

  if (i == BUFLEN)
    {
      buf[BUFLEN-3] = buf[BUFLEN-2] = '.'; /* Mark opcode as abbreviated.  */
      buf[BUFLEN-1] = 0;
      as_bad (_("Unknown instruction '%s'"), buf);
    }
  else if ((*p) && (!ISSPACE (*p)))
    as_bad (_("syntax error"));
  else 
    {
      buf[i] = 0;
      p = skip_space (p);
      key = buf;
      
      insp = bsearch (&key, instab, ARRAY_SIZE (instab),
		    sizeof (instab[0]), key_cmp);
      if (!insp)
	as_bad (_("Unknown instruction '%s'"), buf);
      else
	{
	  p = insp->fp (insp->prefix, insp->opcode, p);
	  p = skip_space (p);
	if ((!err_flag) && *p)
	  as_bad (_("junk at end of line, first unrecognized character is `%c'"),
		  *p);
	}
    }
  input_line_pointer = old_ptr;
}

void
md_apply_fix (fixS * fixP, valueT* valP, segT seg ATTRIBUTE_UNUSED)
{
  long val = * (long *) valP;
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_8_PCREL:
      if (fixP->fx_addsy)
        {
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 0;
        }
      else
        {
	  fixP->fx_no_overflow = (-128 <= val && val < 128);
	  if (!fixP->fx_no_overflow)
            as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("relative jump out of range"));
	  *buf++ = val;
          fixP->fx_done = 1;
        }
      break;

    case BFD_RELOC_Z80_DISP8:
      if (fixP->fx_addsy)
        {
          fixP->fx_no_overflow = 1;
          fixP->fx_done = 0;
        }
      else
        {
	  fixP->fx_no_overflow = (-128 <= val && val < 128);
	  if (!fixP->fx_no_overflow)
            as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("index offset  out of range"));
	  *buf++ = val;
          fixP->fx_done = 1;
        }
      break;

    case BFD_RELOC_8:
      if (val > 255 || val < -128)
	as_warn_where (fixP->fx_file, fixP->fx_line, _("overflow"));
      *buf++ = val;
      fixP->fx_no_overflow = 1; 
      if (fixP->fx_addsy == NULL)
	fixP->fx_done = 1;
      break;

    case BFD_RELOC_16:
      *buf++ = val;
      *buf++ = (val >> 8);
      fixP->fx_no_overflow = 1; 
      if (fixP->fx_addsy == NULL)
	fixP->fx_done = 1;
      break;

    case BFD_RELOC_24: /* Def24 may produce this.  */
      *buf++ = val;
      *buf++ = (val >> 8);
      *buf++ = (val >> 16);
      fixP->fx_no_overflow = 1; 
      if (fixP->fx_addsy == NULL)
	fixP->fx_done = 1;
      break;

    case BFD_RELOC_32: /* Def32 and .long may produce this.  */
      *buf++ = val;
      *buf++ = (val >> 8);
      *buf++ = (val >> 16);
      *buf++ = (val >> 24);
      if (fixP->fx_addsy == NULL)
	fixP->fx_done = 1;
      break;

    default:
      printf (_("md_apply_fix: unknown r_type 0x%x\n"), fixP->fx_r_type);
      abort ();
    }
}

/* GAS will call this to generate a reloc.  GAS will pass the
   resulting reloc to `bfd_install_relocation'.  This currently works
   poorly, as `bfd_install_relocation' often does the wrong thing, and
   instances of `tc_gen_reloc' have been written to work around the
   problems, which in turns makes it difficult to fix
   `bfd_install_relocation'.  */

/* If while processing a fixup, a reloc really
   needs to be created then it is done here.  */

arelent *
tc_gen_reloc (asection *seg ATTRIBUTE_UNUSED , fixS *fixp)
{
  arelent *reloc;

  if (! bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type))
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("reloc %d not supported by object file format"),
		    (int) fixp->fx_r_type);
      return NULL;
    }

  reloc               = xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr  = xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address      = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->howto        = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  reloc->addend       = fixp->fx_offset;

  return reloc;
}

