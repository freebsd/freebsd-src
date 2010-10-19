/*-
   tc-pj.c -- Assemble code for Pico Java
   Copyright 1999, 2000, 2001, 2002, 2003, 2005
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

/* Contributed by Steve Chamberlain of Transmeta <sac@pobox.com>.  */

#include "as.h"
#include "safe-ctype.h"
#include "opcode/pj.h"

extern const pj_opc_info_t pj_opc_info[512];

const char comment_chars[]        = "!/";
const char line_separator_chars[] = ";";
const char line_comment_chars[]   = "/!#";

static int pending_reloc;
static struct hash_control *opcode_hash_control;

static void
little (int ignore ATTRIBUTE_UNUSED)
{
  target_big_endian = 0;
}

static void
big (int ignore ATTRIBUTE_UNUSED)
{
  target_big_endian = 1;
}

const pseudo_typeS md_pseudo_table[] =
{
  {"ml",    little, 0},
  {"mb",    big,    0},
  {0, 0, 0}
};

const char FLT_CHARS[] = "rRsSfFdDxXpP";
const char EXP_CHARS[] = "eE";

void
md_operand (expressionS *op)
{
  if (strncmp (input_line_pointer, "%hi16", 5) == 0)
    {
      if (pending_reloc)
	as_bad (_("confusing relocation expressions"));
      pending_reloc = BFD_RELOC_PJ_CODE_HI16;
      input_line_pointer += 5;
      expression (op);
    }

  if (strncmp (input_line_pointer, "%lo16", 5) == 0)
    {
      if (pending_reloc)
	as_bad (_("confusing relocation expressions"));
      pending_reloc = BFD_RELOC_PJ_CODE_LO16;
      input_line_pointer += 5;
      expression (op);
    }
}

/* Parse an expression and then restore the input line pointer.  */

static char *
parse_exp_save_ilp (char *s, expressionS *op)
{
  char *save = input_line_pointer;

  input_line_pointer = s;
  expression (op);
  s = input_line_pointer;
  input_line_pointer = save;
  return s;
}

/* This is called by emit_expr via TC_CONS_FIX_NEW when creating a
   reloc for a cons.  We could use the definition there, except that
   we want to handle magic pending reloc expressions specially.  */

void
pj_cons_fix_new_pj (fragS *frag, int where, int nbytes, expressionS *exp)
{
  static int rv[5][2] =
  { { 0, 0 },
    { BFD_RELOC_8, BFD_RELOC_8 },
    { BFD_RELOC_PJ_CODE_DIR16, BFD_RELOC_16 },
    { 0, 0 },
    { BFD_RELOC_PJ_CODE_DIR32, BFD_RELOC_32 }};

  fix_new_exp (frag, where, nbytes, exp, 0,
	       pending_reloc ? pending_reloc
	       : rv[nbytes][(now_seg->flags & SEC_CODE) ? 0 : 1]);

  pending_reloc = 0;
}

/* Turn a reloc description character from the pj-opc.h table into
   code which BFD can handle.  */

static int
c_to_r (int x)
{
  switch (x)
    {
    case O_R8:
      return BFD_RELOC_8_PCREL;
    case O_U8:
    case O_8:
      return BFD_RELOC_8;
    case O_R16:
      return BFD_RELOC_PJ_CODE_REL16;
    case O_U16:
    case O_16:
      return BFD_RELOC_PJ_CODE_DIR16;
    case O_R32:
      return BFD_RELOC_PJ_CODE_REL32;
    case O_32:
      return BFD_RELOC_PJ_CODE_DIR32;
    }
  abort ();
  return 0;
}

/* Handler for the ipush fake opcode,
   turns ipush <foo> into sipush lo16<foo>, sethi hi16<foo>.  */

static void
ipush_code (pj_opc_info_t *opcode ATTRIBUTE_UNUSED, char *str)
{
  char *b = frag_more (6);
  expressionS arg;

  b[0] = 0x11;
  b[3] = 0xed;
  parse_exp_save_ilp (str + 1, &arg);
  if (pending_reloc)
    {
      as_bad (_("can't have relocation for ipush"));
      pending_reloc = 0;
    }

  fix_new_exp (frag_now, b - frag_now->fr_literal + 1, 2,
	       &arg, 0, BFD_RELOC_PJ_CODE_DIR16);
  fix_new_exp (frag_now, b - frag_now->fr_literal + 4, 2,
	       &arg, 0, BFD_RELOC_PJ_CODE_HI16);
}

/* Insert names into the opcode table which are really mini macros,
   not opcodes.  The fakeness is indicated with an opcode of -1.  */

static void
fake_opcode (const char *name,
	     void (*func) (struct pj_opc_info_t *, char *))
{
  pj_opc_info_t * fake = xmalloc (sizeof (pj_opc_info_t));

  fake->opcode = -1;
  fake->opcode_next = -1;
  fake->u.func = func;
  hash_insert (opcode_hash_control, name, (char *) fake);
}

/* Enter another entry into the opcode hash table so the same opcode
   can have another name.  */

static void
alias (const char *new, const char *old)
{
  hash_insert (opcode_hash_control, new,
	       (char *) hash_find (opcode_hash_control, old));
}

/* This function is called once, at assembler startup time.  It sets
   up the hash table with all the opcodes in it, and also initializes
   some aliases for compatibility with other assemblers.  */

void
md_begin (void)
{
  const pj_opc_info_t *opcode;
  opcode_hash_control = hash_new ();

  /* Insert names into hash table.  */
  for (opcode = pj_opc_info; opcode->u.name; opcode++)
    hash_insert (opcode_hash_control, opcode->u.name, (char *) opcode);

  /* Insert the only fake opcode.  */
  fake_opcode ("ipush", ipush_code);

  /* Add some aliases for opcode names.  */
  alias ("ifeq_s", "ifeq");
  alias ("ifne_s", "ifne");
  alias ("if_icmpge_s", "if_icmpge");
  alias ("if_icmpne_s", "if_icmpne");
  alias ("if_icmpeq_s", "if_icmpeq");
  alias ("if_icmpgt_s", "if_icmpgt");
  alias ("goto_s", "goto");

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, 0);
}

/* This is the guts of the machine-dependent assembler.  STR points to
   a machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */

void
md_assemble (char *str)
{
  char *op_start;
  char *op_end;

  pj_opc_info_t *opcode;
  char *output;
  int idx = 0;
  char pend;

  int nlen = 0;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the op code end.  */
  op_start = str;
  for (op_end = str;
       *op_end && !is_end_of_line[*op_end & 0xff] && *op_end != ' ';
       op_end++)
    nlen++;

  pend = *op_end;
  *op_end = 0;

  if (nlen == 0)
    as_bad (_("can't find opcode "));

  opcode = (pj_opc_info_t *) hash_find (opcode_hash_control, op_start);
  *op_end = pend;

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode %s"), op_start);
      return;
    }

  if (opcode->opcode == -1)
    {
      /* It's a fake opcode.  Dig out the args and pretend that was
         what we were passed.  */
      (*opcode->u.func) (opcode, op_end);
    }
  else
    {
      int an;

      output = frag_more (opcode->len);
      output[idx++] = opcode->opcode;

      if (opcode->opcode_next != -1)
	output[idx++] = opcode->opcode_next;

      for (an = 0; opcode->arg[an]; an++)
	{
	  expressionS arg;

	  if (*op_end == ',' && an != 0)
	    op_end++;

	  if (*op_end == 0)
	    as_bad ("expected expresssion");

	  op_end = parse_exp_save_ilp (op_end, &arg);

	  fix_new_exp (frag_now,
		       output - frag_now->fr_literal + idx,
		       ASIZE (opcode->arg[an]),
		       &arg,
		       PCREL (opcode->arg[an]),
		       pending_reloc ? pending_reloc : c_to_r (opcode->arg[an]));

	  idx += ASIZE (opcode->arg[an]);
	  pending_reloc = 0;
	}

      while (ISSPACE (*op_end))
	op_end++;

      if (*op_end != 0)
	as_warn ("extra stuff on line ignored");

    }

  if (pending_reloc)
    as_bad ("Something forgot to clean up\n");

}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP .  An error message is
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

  if (!target_big_endian)
    {
      for (i = prec - 1; i >= 0; i--)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }
  else
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }

  return NULL;
}

const char *md_shortopts = "";

struct option md_longopts[] =
{
#define OPTION_LITTLE (OPTION_MD_BASE)
#define OPTION_BIG    (OPTION_LITTLE + 1)

  {"little", no_argument, NULL, OPTION_LITTLE},
  {"big", no_argument, NULL, OPTION_BIG},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case OPTION_LITTLE:
      little (0);
      break;
    case OPTION_BIG:
      big (0);
      break;
    default:
      return 0;
    }
  return 1;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("\
PJ options:\n\
-little			generate little endian code\n\
-big			generate big endian code\n"));
}

/* Apply a fixup to the object file.  */

void
md_apply_fix (fixS *fixP, valueT * valP, segT seg ATTRIBUTE_UNUSED)
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  long val = *valP;
  long max, min;
  int shift;

  max = min = 0;
  shift = 0;
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      return;

    case BFD_RELOC_PJ_CODE_REL16:
      if (val < -0x8000 || val >= 0x7fff)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("pcrel too far"));
      buf[0] |= (val >> 8) & 0xff;
      buf[1] = val & 0xff;
      break;

    case BFD_RELOC_PJ_CODE_HI16:
      *buf++ = val >> 24;
      *buf++ = val >> 16;
      fixP->fx_addnumber = val & 0xffff;
      break;

    case BFD_RELOC_PJ_CODE_DIR16:
    case BFD_RELOC_PJ_CODE_LO16:
      *buf++ = val >> 8;
      *buf++ = val >> 0;

      max = 0xffff;
      min = -0xffff;
      break;

    case BFD_RELOC_8:
      max = 0xff;
      min = -0xff;
      *buf++ = val;
      break;

    case BFD_RELOC_PJ_CODE_DIR32:
      *buf++ = val >> 24;
      *buf++ = val >> 16;
      *buf++ = val >> 8;
      *buf++ = val >> 0;
      break;

    case BFD_RELOC_32:
      if (target_big_endian)
	{
	  *buf++ = val >> 24;
	  *buf++ = val >> 16;
	  *buf++ = val >> 8;
	  *buf++ = val >> 0;
	}
      else
	{
	  *buf++ = val >> 0;
	  *buf++ = val >> 8;
	  *buf++ = val >> 16;
	  *buf++ = val >> 24;
	}
      break;

    case BFD_RELOC_16:
      if (target_big_endian)
	{
	  *buf++ = val >> 8;
	  *buf++ = val >> 0;
	}
      else
	{
	  *buf++ = val >> 0;
	  *buf++ = val >> 8;
	}
      break;

    default:
      abort ();
    }

  if (max != 0 && (val < min || val > max))
    as_bad_where (fixP->fx_file, fixP->fx_line, _("offset out of range"));

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* Put number into target byte order.  Always put values in an
   executable section into big endian order.  */

void
md_number_to_chars (char *ptr, valueT use, int nbytes)
{
  if (target_big_endian || now_seg->flags & SEC_CODE)
    number_to_chars_bigendian (ptr, use, nbytes);
  else
    number_to_chars_littleendian (ptr, use, nbytes);
}

/* Translate internal representation of relocation info to BFD target
   format.  */

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  rel = xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;

  r_type = fixp->fx_r_type;
  rel->addend = fixp->fx_addnumber;
  rel->howto = bfd_reloc_type_lookup (stdoutput, r_type);

  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Cannot represent relocation type %s"),
		    bfd_get_reloc_code_name (r_type));
      /* Set howto to a garbage value so that we can keep going.  */
      rel->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_32);
      assert (rel->howto != NULL);
    }

  return rel;
}
