/* tc-arc.c -- Assembler for the ARC
   Copyright (C) 1994, 1995, 1997, 1998, 1999 Free Software Foundation, Inc.
   Contributed by Doug Evans (dje@cygnus.com).

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
   02111-1307, USA. */

#include <stdio.h>
#include <ctype.h>
#include "as.h"
#include "subsegs.h"
#include "opcode/arc.h"
#include "elf/arc.h"

extern int arc_get_mach PARAMS ((char *));

static arc_insn arc_insert_operand PARAMS ((arc_insn,
					    const struct arc_operand *, int,
					    const struct arc_operand_value *,
					    offsetT, char *, unsigned int));
static void arc_common PARAMS ((int));
static void arc_cpu PARAMS ((int));
/*static void arc_rename PARAMS ((int));*/
static int get_arc_exp_reloc_type PARAMS ((int, int, expressionS *,
					   expressionS *));

const pseudo_typeS md_pseudo_table[] =
{
  { "align", s_align_bytes, 0 },	/* Defaulting is invalid (0) */
  { "common", arc_common, 0 },
/*{ "hword", cons, 2 }, - already exists */
  { "word", cons, 4 },
/*{ "xword", cons, 8 },*/
  { "cpu", arc_cpu, 0 },
/*{ "rename", arc_rename, 0 },*/
  { NULL, 0, 0 },
};

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "#;";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments started like this one will always
   work if '/' isn't otherwise defined. */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = "";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "rRsSfFdD";

/* Byte order.  */
extern int target_big_endian;
const char *arc_target_format = DEFAULT_TARGET_FORMAT;
static int byte_order = DEFAULT_BYTE_ORDER;

/* One of bfd_mach_arc_xxx.  */
static int arc_mach_type = bfd_mach_arc_base;

/* Non-zero if the cpu type has been explicitly specified.  */
static int mach_type_specified_p = 0;

/* Non-zero if opcode tables have been initialized.
   A .cpu command must appear before any instructions.  */
static int cpu_tables_init_p = 0;

static struct hash_control *arc_suffix_hash = NULL;

const char *md_shortopts = "";
struct option md_longopts[] =
{
#define OPTION_EB (OPTION_MD_BASE + 0)
  {"EB", no_argument, NULL, OPTION_EB},
#define OPTION_EL (OPTION_MD_BASE + 1)
  {"EL", no_argument, NULL, OPTION_EL},
  { NULL, no_argument, NULL, 0 }
};
size_t md_longopts_size = sizeof (md_longopts);

/*
 * md_parse_option
 *
 * Invocation line includes a switch not recognized by the base assembler.
 * See if it's a processor-specific option.
 */

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case OPTION_EB:
      byte_order = BIG_ENDIAN;
      arc_target_format = "elf32-bigarc";
      break;
    case OPTION_EL:
      byte_order = LITTLE_ENDIAN;
      arc_target_format = "elf32-littlearc";
      break;
    default:
      return 0;
    }
  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("\
ARC options:\n\
-EB			generate big endian output\n\
-EL			generate little endian output\n"));
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need.
   Opcode selection is defered until later because we might see a .cpu
   command.  */

void
md_begin ()
{
  /* The endianness can be chosen "at the factory".  */
  target_big_endian = byte_order == BIG_ENDIAN;

  if (!bfd_set_arch_mach (stdoutput, bfd_arch_arc, arc_mach_type))
    as_warn (_("could not set architecture and machine"));

  /* Assume the base cpu.  This call is necessary because we need to
     initialize `arc_operand_map' which may be needed before we see the
     first insn.  */
  arc_opcode_init_tables (arc_get_opcode_mach (bfd_mach_arc_base,
					       target_big_endian));
}

/* Initialize the various opcode and operand tables.
   MACH is one of bfd_mach_arc_xxx.  */

static void
init_opcode_tables (mach)
     int mach;
{
  register unsigned int i;
  char *last;

  if ((arc_suffix_hash = hash_new ()) == NULL)
    as_fatal (_("virtual memory exhausted"));

  if (!bfd_set_arch_mach (stdoutput, bfd_arch_arc, mach))
    as_warn (_("could not set architecture and machine"));

  /* This initializes a few things in arc-opc.c that we need.
     This must be called before the various arc_xxx_supported fns.  */
  arc_opcode_init_tables (arc_get_opcode_mach (mach, target_big_endian));

  /* Only put the first entry of each equivalently named suffix in the
     table.  */
  last = "";
  for (i = 0; i < arc_suffixes_count; i++)
    {
      if (! arc_opval_supported (&arc_suffixes[i]))
	continue;
      if (strcmp (arc_suffixes[i].name, last) != 0)
	hash_insert (arc_suffix_hash, arc_suffixes[i].name, (PTR) (arc_suffixes + i));
      last = arc_suffixes[i].name;
    }

  /* Since registers don't have a prefix, we put them in the symbol table so
     they can't be used as symbols.  This also simplifies argument parsing as
     we can let gas parse registers for us.  The recorded register number is
     the index in `arc_reg_names'.  */
  for (i = 0; i < arc_reg_names_count; i++)
    {
      if (! arc_opval_supported (&arc_reg_names[i]))
	continue;
      /* Use symbol_create here instead of symbol_new so we don't try to
	 output registers into the object file's symbol table.  */
      symbol_table_insert (symbol_create (arc_reg_names[i].name, reg_section,
					  i, &zero_address_frag));
    }

  /* Tell `s_cpu' it's too late.  */
  cpu_tables_init_p = 1;
}

/* Insert an operand value into an instruction.
   If REG is non-NULL, it is a register number and ignore VAL.  */

static arc_insn
arc_insert_operand (insn, operand, mods, reg, val, file, line)
     arc_insn insn;
     const struct arc_operand *operand;
     int mods;
     const struct arc_operand_value *reg;
     offsetT val;
     char *file;
     unsigned int line;
{
  if (operand->bits != 32)
    {
      long min, max;
      offsetT test;

      if ((operand->flags & ARC_OPERAND_SIGNED) != 0)
	{
	  if ((operand->flags & ARC_OPERAND_SIGNOPT) != 0)
	    max = (1 << operand->bits) - 1;
	  else
	    max = (1 << (operand->bits - 1)) - 1;
	  min = - (1 << (operand->bits - 1));
	}
      else
	{
	  max = (1 << operand->bits) - 1;
	  min = 0;
	}

      if ((operand->flags & ARC_OPERAND_NEGATIVE) != 0)
	test = - val;
      else
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

  if (operand->insert)
    {
      const char *errmsg;

      errmsg = NULL;
      insn = (*operand->insert) (insn, operand, mods, reg, (long) val, &errmsg);
      if (errmsg != (const char *) NULL)
	as_warn (errmsg);
    }
  else
    insn |= (((long) val & ((1 << operand->bits) - 1))
	     << operand->shift);

  return insn;
}

/* We need to keep a list of fixups.  We can't simply generate them as
   we go, because that would require us to first create the frag, and
   that would screw up references to ``.''.  */

struct arc_fixup
{
  /* index into `arc_operands' */
  int opindex;
  expressionS exp;
};

#define MAX_FIXUPS 5

#define MAX_SUFFIXES 5

/* This routine is called for each instruction to be assembled.  */

void
md_assemble (str)
     char *str;
{
  const struct arc_opcode *opcode;
  char *start;
  arc_insn insn;
  static int init_tables_p = 0;

  /* Opcode table initialization is deferred until here because we have to
     wait for a possible .cpu command.  */
  if (!init_tables_p)
    {
      init_opcode_tables (arc_mach_type);
      init_tables_p = 1;
    }

  /* Skip leading white space.  */
  while (isspace (*str))
    str++;

  /* The instructions are stored in lists hashed by the first letter (though
     we needn't care how they're hashed).  Get the first in the list.  */

  opcode = arc_opcode_lookup_asm (str);

  /* Keep looking until we find a match.  */

  start = str;
  for ( ; opcode != NULL; opcode = ARC_OPCODE_NEXT_ASM (opcode))
    {
      int past_opcode_p, fc, num_suffixes;
      char *syn;
      struct arc_fixup fixups[MAX_FIXUPS];
      /* Used as a sanity check.  If we need a limm reloc, make sure we ask
	 for an extra 4 bytes from frag_more.  */
      int limm_reloc_p;
      const struct arc_operand_value *insn_suffixes[MAX_SUFFIXES];

      /* Is this opcode supported by the selected cpu?  */
      if (! arc_opcode_supported (opcode))
	continue;

      /* Scan the syntax string.  If it doesn't match, try the next one.  */

      arc_opcode_init_insert ();
      insn = opcode->value;
      fc = 0;
      past_opcode_p = 0;
      num_suffixes = 0;
      limm_reloc_p = 0;

      /* We don't check for (*str != '\0') here because we want to parse
	 any trailing fake arguments in the syntax string.  */
      for (str = start, syn = opcode->syntax; *syn != '\0'; )
	{
	  int mods;
	  const struct arc_operand *operand;

	  /* Non operand chars must match exactly.  */
	  if (*syn != '%' || *++syn == '%')
	    {
	      /* Handle '+' specially as we want to allow "ld r0,[sp-4]".  */
	      /* ??? The syntax has changed to [sp,-4].  */
	      if (0 && *syn == '+' && *str == '-')
		{
		  /* Skip over syn's +, but leave str's - alone.
		     That makes the case identical to "ld r0,[sp+-4]".  */
		  ++syn;
		}
	      else if (*str == *syn)
		{
		  if (*syn == ' ')
		    past_opcode_p = 1;
		  ++syn;
		  ++str;
		}
	      else
		break;
	      continue;
	    }

	  /* We have an operand.  Pick out any modifiers.  */
	  mods = 0;
	  while (ARC_MOD_P (arc_operands[arc_operand_map[*syn]].flags))
	    {
	      mods |= arc_operands[arc_operand_map[*syn]].flags & ARC_MOD_BITS;
	      ++syn;
	    }
	  operand = arc_operands + arc_operand_map[*syn];
	  if (operand->fmt == 0)
	    as_fatal (_("unknown syntax format character `%c'"), *syn);

	  if (operand->flags & ARC_OPERAND_FAKE)
	    {
	      const char *errmsg = NULL;
	      if (operand->insert)
		{
		  insn = (*operand->insert) (insn, operand, mods, NULL, 0, &errmsg);
		  /* If we get an error, go on to try the next insn.  */
		  if (errmsg)
		    break;
		}
	      ++syn;
	    }
	  /* Are we finished with suffixes?  */
	  else if (!past_opcode_p)
	    {
	      int found;
	      char c;
	      char *s,*t;
	      const struct arc_operand_value *suf,*suffix,*suffix_end;

	      if (!(operand->flags & ARC_OPERAND_SUFFIX))
		abort ();

	      /* If we're at a space in the input string, we want to skip the
		 remaining suffixes.  There may be some fake ones though, so
		 just go on to try the next one.  */
	      if (*str == ' ')
		{
		  ++syn;
		  continue;
		}

	      s = str;
	      if (mods & ARC_MOD_DOT)
		{
		  if (*s != '.')
		    break;
		  ++s;
		}
	      else
		{
		  /* This can happen in "b.nd foo" and we're currently looking
		     for "%q" (ie: a condition code suffix).  */
		  if (*s == '.')
		    {
		      ++syn;
		      continue;
		    }
		}

	      /* Pick the suffix out and look it up via the hash table.  */
	      for (t = s; *t && isalpha (*t); ++t)
		continue;
	      c = *t;
	      *t = '\0';
	      suf = hash_find (arc_suffix_hash, s);
	      *t = c;
	      if (!suf)
		{
		  /* This can happen in "blle foo" and we're currently using
		     the template "b%q%.n %j".  The "bl" insn occurs later in
		     the table so "lle" isn't an illegal suffix.  */
		  break;
		}

	      /* Is it the right type?  Note that the same character is used
	         several times, so we have to examine all of them.  This is
		 relatively efficient as equivalent entries are kept
		 together.  If it's not the right type, don't increment `str'
		 so we try the next one in the series.  */
	      found = 0;
	      suffix_end = arc_suffixes + arc_suffixes_count;
	      for (suffix = suf;
		   suffix < suffix_end && strcmp (suffix->name, suf->name) == 0;
		   ++suffix)
		{
		  if (arc_operands[suffix->type].fmt == *syn)
		    {
		      /* Insert the suffix's value into the insn.  */
		      if (operand->insert)
			insn = (*operand->insert) (insn, operand,
						   mods, NULL, suffix->value,
						   NULL);
		      else
			insn |= suffix->value << operand->shift;

		      str = t;
		      found = 1;
		      break;
		    }
		}
	      ++syn;
	      if (!found)
		; /* Wrong type.  Just go on to try next insn entry.  */
	      else
		{
		  if (num_suffixes == MAX_SUFFIXES)
		    as_bad (_("too many suffixes"));
		  else
		    insn_suffixes[num_suffixes++] = suffix;
		}
	    }
	  else
	    /* This is either a register or an expression of some kind.  */
	    {
	      char c;
	      char *hold;
	      const struct arc_operand_value *reg = NULL;
	      long value = 0;
	      expressionS exp;

	      if (operand->flags & ARC_OPERAND_SUFFIX)
		abort ();

	      /* Is there anything left to parse?
		 We don't check for this at the top because we want to parse
		 any trailing fake arguments in the syntax string.  */
	      if (*str == '\0')
		break;
#if 0
	      /* Is this a syntax character?  Eg: is there a '[' present when
		 there shouldn't be?  */
	      if (!isalnum (*str)
		  /* '.' as in ".LLC0" */
		  && *str != '.'
		  /* '_' as in "_print" */
		  && *str != '_'
		  /* '-' as in "[fp,-4]" */
		  && *str != '-'
		  /* '%' as in "%ia(_func)" */
		  && *str != '%')
		break;
#endif

	      /* Parse the operand.  */
	      hold = input_line_pointer;
	      input_line_pointer = str;
	      expression (&exp);
	      str = input_line_pointer;
	      input_line_pointer = hold;

	      if (exp.X_op == O_illegal)
		as_bad (_("illegal operand"));
	      else if (exp.X_op == O_absent)
		as_bad (_("missing operand"));
	      else if (exp.X_op == O_constant)
		{
		  value = exp.X_add_number;
		}
	      else if (exp.X_op == O_register)
		{
		  reg = arc_reg_names + exp.X_add_number;
		}
	      else
		{
		  /* We need to generate a fixup for this expression.  */
		  if (fc >= MAX_FIXUPS)
		    as_fatal (_("too many fixups"));
		  fixups[fc].exp = exp;

		  /* If this is a register constant (IE: one whose
		     register value gets stored as 61-63) then this
		     must be a limm.  We don't support shimm relocs.  */
		  /* ??? This bit could use some cleaning up.
		     Referencing the format chars like this goes
		     against style.  */
#define IS_REG_OPERAND(o) ((o) == 'a' || (o) == 'b' || (o) == 'c')
		  if (IS_REG_OPERAND (*syn))
		    {
		      const char *junk;

		      fixups[fc].opindex = arc_operand_map['L'];
		      limm_reloc_p = 1;
		      /* Tell insert_reg we need a limm.  This is
			 needed because the value at this point is
			 zero, a shimm.  */
		      /* ??? We need a cleaner interface than this.  */
		      (*arc_operands[arc_operand_map['Q']].insert)
			(insn, operand, mods, reg, 0L, &junk);
		    }
		  else
		    fixups[fc].opindex = arc_operand_map[*syn];
		  ++fc;
		  value = 0;
		}

	      /* Insert the register or expression into the instruction.  */
	      if (operand->insert)
		{
		  const char *errmsg = NULL;
		  insn = (*operand->insert) (insn, operand, mods,
					     reg, (long) value, &errmsg);
#if 0
		  if (errmsg != (const char *) NULL)
		    as_warn (errmsg);
#endif
		  /* FIXME: We want to try shimm insns for limm ones.  But if
		     the constant won't fit, we must go on to try the next
		     possibility.  Where do we issue warnings for constants
		     that are too big then?  At present, we'll flag the insn
		     as unrecognizable!  Maybe have the "bad instruction"
		     error message include our `errmsg'?  */
		  if (errmsg != (const char *) NULL)
		    break;
		}
	      else
		insn |= (value & ((1 << operand->bits) - 1)) << operand->shift;

	      ++syn;
	    }
	}

      /* If we're at the end of the syntax string, we're done.  */
      /* FIXME: try to move this to a separate function.  */
      if (*syn == '\0')
	{
	  int i;
	  char *f;
	  long limm, limm_p;

	  /* For the moment we assume a valid `str' can only contain blanks
	     now.  IE: We needn't try again with a longer version of the
	     insn and it is assumed that longer versions of insns appear
	     before shorter ones (eg: lsr r2,r3,1 vs lsr r2,r3).  */

	  while (isspace (*str))
	    ++str;

	  if (*str != '\0')
	    as_bad (_("junk at end of line: `%s'"), str);

	  /* Is there a limm value?  */
	  limm_p = arc_opcode_limm_p (&limm);

	  /* Perform various error and warning tests.  */

	  {
	    static int in_delay_slot_p = 0;
	    static int prev_insn_needs_cc_nop_p = 0;
	    /* delay slot type seen */
	    int delay_slot_type = ARC_DELAY_NONE;
	    /* conditional execution flag seen */
	    int conditional = 0;
	    /* 1 if condition codes are being set */
	    int cc_set_p = 0;
	    /* 1 if conditional branch, including `b' "branch always" */
	    int cond_branch_p = opcode->flags & ARC_OPCODE_COND_BRANCH;
	    int need_cc_nop_p = 0;

	    for (i = 0; i < num_suffixes; ++i)
	      {
		switch (arc_operands[insn_suffixes[i]->type].fmt)
		  {
		  case 'n' :
		    delay_slot_type = insn_suffixes[i]->value;
		    break;
		  case 'q' :
		    conditional = insn_suffixes[i]->value;
		    break;
		  case 'f' :
		    cc_set_p = 1;
		    break;
		  }
	      }

	    /* Putting an insn with a limm value in a delay slot is supposed to
	       be legal, but let's warn the user anyway.  Ditto for 8 byte
	       jumps with delay slots.  */
	    if (in_delay_slot_p && limm_p)
	      as_warn (_("8 byte instruction in delay slot"));
	    if (delay_slot_type != ARC_DELAY_NONE && limm_p)
	      as_warn (_("8 byte jump instruction with delay slot"));
	    in_delay_slot_p = (delay_slot_type != ARC_DELAY_NONE) && !limm_p;

	    /* Warn when a conditional branch immediately follows a set of
	       the condition codes.  Note that this needn't be done if the
	       insn that sets the condition codes uses a limm.  */
	    if (cond_branch_p && conditional != 0 /* 0 = "always" */
		&& prev_insn_needs_cc_nop_p)
	      as_warn (_("conditional branch follows set of flags"));
	    prev_insn_needs_cc_nop_p = cc_set_p && !limm_p;
	  }

	  /* Write out the instruction.
	     It is important to fetch enough space in one call to `frag_more'.
	     We use (f - frag_now->fr_literal) to compute where we are and we
	     don't want frag_now to change between calls.  */
	  if (limm_p)
	    {
	      f = frag_more (8);
	      md_number_to_chars (f, insn, 4);
	      md_number_to_chars (f + 4, limm, 4);
	    }
	  else if (limm_reloc_p)
	    {
	      /* We need a limm reloc, but the tables think we don't.  */
	      abort ();
	    }
	  else
	    {
	      f = frag_more (4);
	      md_number_to_chars (f, insn, 4);
	    }

	  /* Create any fixups.  */
	  for (i = 0; i < fc; ++i)
	    {
	      int op_type, reloc_type;
	      expressionS exptmp;
	      const struct arc_operand *operand;

	      /* Create a fixup for this operand.
		 At this point we do not use a bfd_reloc_code_real_type for
		 operands residing in the insn, but instead just use the
		 operand index.  This lets us easily handle fixups for any
		 operand type, although that is admittedly not a very exciting
		 feature.  We pick a BFD reloc type in md_apply_fix.

		 Limm values (4 byte immediate "constants") must be treated
		 normally because they're not part of the actual insn word
		 and thus the insertion routines don't handle them.  */

	      if (arc_operands[fixups[i].opindex].flags & ARC_OPERAND_LIMM)
		{
		  op_type = fixups[i].opindex;
		  /* FIXME: can we add this data to the operand table?  */
		  if (op_type == arc_operand_map['L'])
		    reloc_type = BFD_RELOC_32;
		  else if (op_type == arc_operand_map['J'])
		    reloc_type = BFD_RELOC_ARC_B26;
		  else
		    abort ();
		  reloc_type = get_arc_exp_reloc_type (1, reloc_type,
						       &fixups[i].exp,
						       &exptmp);
		}
	      else
		{
		  op_type = get_arc_exp_reloc_type (0, fixups[i].opindex,
						    &fixups[i].exp, &exptmp);
		  reloc_type = op_type + (int) BFD_RELOC_UNUSED;
		}
	      operand = &arc_operands[op_type];
	      fix_new_exp (frag_now,
			   ((f - frag_now->fr_literal)
			    + (operand->flags & ARC_OPERAND_LIMM ? 4 : 0)), 4,
			   &exptmp,
			   (operand->flags & ARC_OPERAND_RELATIVE_BRANCH) != 0,
			   (bfd_reloc_code_real_type) reloc_type);
	    }

	  /* All done.  */
	  return;
	}

      /* Try the next entry.  */
    }

  as_bad (_("bad instruction `%s'"), start);
}

/* ??? This was copied from tc-sparc.c, I think.  Is it necessary?  */

static void
arc_common (ignore)
     int ignore;
{
  char *name;
  char c;
  char *p;
  int temp, size;
  symbolS *symbolP;

  name = input_line_pointer;
  c = get_symbol_end ();
  /* just after name is now '\0' */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("expected comma after symbol-name"));
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;		/* skip ',' */
  if ((temp = get_absolute_expression ()) < 0)
    {
      as_bad (_(".COMMon length (%d.) <0! Ignored."), temp);
      ignore_rest_of_line ();
      return;
    }
  size = temp;
  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;
  if (S_IS_DEFINED (symbolP) && ! S_IS_COMMON (symbolP))
    {
      as_bad (_("ignoring attempt to re-define symbol"));
      ignore_rest_of_line ();
      return;
    }
  if (S_GET_VALUE (symbolP) != 0)
    {
      if (S_GET_VALUE (symbolP) != size)
	{
	  as_warn (_("Length of .comm \"%s\" is already %ld. Not changed to %d."),
		   S_GET_NAME (symbolP), (long) S_GET_VALUE (symbolP), size);
	}
    }
  assert (symbol_get_frag (symbolP) == &zero_address_frag);
  if (*input_line_pointer != ',')
    {
      as_bad (_("expected comma after common length"));
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != '"')
    {
      temp = get_absolute_expression ();
      if (temp < 0)
	{
	  temp = 0;
	  as_warn (_("Common alignment negative; 0 assumed"));
	}
      if (symbolP->local)
	{
	  segT old_sec;
	  int old_subsec;
	  char *p;
	  int align;

	allocate_bss:
	  old_sec = now_seg;
	  old_subsec = now_subseg;
	  align = temp;
	  record_alignment (bss_section, align);
	  subseg_set (bss_section, 0);
	  if (align)
	    frag_align (align, 0, 0);
	  if (S_GET_SEGMENT (symbolP) == bss_section)
	    symbol_get_frag (symbolP)->fr_symbol = 0;
	  symbol_set_frag (symbolP, frag_now);
	  p = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP,
			(offsetT) size, (char *) 0);
	  *p = 0;
	  S_SET_SEGMENT (symbolP, bss_section);
	  S_CLEAR_EXTERNAL (symbolP);
	  subseg_set (old_sec, old_subsec);
	}
      else
	{
	allocate_common:
	  S_SET_VALUE (symbolP, (valueT) size);
	  S_SET_ALIGN (symbolP, temp);
	  S_SET_EXTERNAL (symbolP);
	  S_SET_SEGMENT (symbolP, bfd_com_section_ptr);
	}
    }
  else
    {
      input_line_pointer++;
      /* ??? Some say data, some say bss.  */
      if (strncmp (input_line_pointer, ".bss\"", 5)
	  && strncmp (input_line_pointer, ".data\"", 6))
	{
	  input_line_pointer--;
	  goto bad_common_segment;
	}
      while (*input_line_pointer++ != '"')
	;
      goto allocate_common;
    }
  demand_empty_rest_of_line ();
  return;

  {
  bad_common_segment:
    p = input_line_pointer;
    while (*p && *p != '\n')
      p++;
    c = *p;
    *p = '\0';
    as_bad (_("bad .common segment %s"), input_line_pointer + 1);
    *p = c;
    input_line_pointer = p;
    ignore_rest_of_line ();
    return;
  }
}

/* Select the cpu we're assembling for.  */

static void
arc_cpu (ignore)
     int ignore;
{
  int mach;
  char c;
  char *cpu;

  /* If an instruction has already been seen, it's too late.  */
  if (cpu_tables_init_p)
    {
      as_bad (_(".cpu command must appear before any instructions"));
      ignore_rest_of_line ();
      return;
    }

  cpu = input_line_pointer;
  c = get_symbol_end ();
  mach = arc_get_mach (cpu);
  *input_line_pointer = c;
  if (mach == -1)
    goto bad_cpu;

  demand_empty_rest_of_line ();

  /* The cpu may have been selected on the command line.
     The choices must match.  */
  /* ??? This was a command line option early on.  It's gone now, but
     leave this in.  */
  if (mach_type_specified_p && mach != arc_mach_type)
    as_bad (_(".cpu conflicts with previous value"));
  else
    {
      arc_mach_type = mach;
      mach_type_specified_p = 1;
      if (!bfd_set_arch_mach (stdoutput, bfd_arch_arc, mach))
	as_warn (_("could not set architecture and machine"));
    }
  return;

 bad_cpu:
  as_bad (_("bad .cpu op"));
  ignore_rest_of_line ();
}

#if 0
/* The .rename pseudo-op.  This is used by gcc to implement
   -mmangle-cpu-libgcc.  */

static void
arc_rename (ignore)
     int ignore;
{
  char *name,*new;
  char c;
  symbolS *sym;
  int len;

  name = input_line_pointer;
  c = get_symbol_end ();
  sym = symbol_find_or_make (name);
  *input_line_pointer = c;

  if (*input_line_pointer != ',')
    {
      as_bad (_("missing rename string"));
      ignore_rest_of_line ();
      return;
    }
  ++input_line_pointer;
  SKIP_WHITESPACE ();

  name = input_line_pointer;
  c = get_symbol_end ();
  if (*name == '\0')
    {
      *input_line_pointer = c;
      as_bad (_("invalid symbol to rename to"));
      ignore_rest_of_line ();
      return;
    }
  new = (char *) xmalloc (strlen (name) + 1);
  strcpy (new, name);
  *input_line_pointer = c;
  symbol_get_tc (sym)->real_name = new;

  demand_empty_rest_of_line ();
}
#endif

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP.
   An error message is returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;
  char *atof_ieee ();

  switch (type)
    {
    case 'f':
    case 'F':
      prec = 2;
      break;

    case 'd':
    case 'D':
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
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return NULL;
}

/* Write a value out to the object file, using the appropriate
   endianness.  */

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* Round up a section size to the appropriate boundary. */

valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
  int align = bfd_get_section_alignment (stdoutput, segment);

  return ((size + (1 << align) - 1) & (-1 << align));
}

/* We don't have any form of relaxing.  */

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp;
     asection *seg;
{
  abort ();
}

/* Convert a machine dependent frag.  We never generate these.  */

void
md_convert_frag (abfd, sec, fragp)
     bfd *abfd;
     asection *sec;
     fragS *fragp;
{
  abort ();
}

/* Parse an operand that is machine-specific.

   The ARC has a special %-op to adjust addresses so they're usable in
   branches.  The "st" is short for the STatus register.
   ??? Later expand this to take a flags value too.

   ??? We can't create new expression types so we map the %-op's onto the
   existing syntax.  This means that the user could use the chosen syntax
   to achieve the same effect.  Perhaps put a special cookie in X_add_number
   to mark the expression as special.  */

void 
md_operand (expressionP)
     expressionS *expressionP;
{
  char *p = input_line_pointer;

  if (*p == '%' && strncmp (p, "%st(", 4) == 0)
    {
      input_line_pointer += 4;
      expression (expressionP);
      if (*input_line_pointer != ')')
	{
	  as_bad (_("missing ')' in %-op"));
	  return;
	}
      ++input_line_pointer;
      if (expressionP->X_op == O_symbol
	  && expressionP->X_add_number == 0
	  /* I think this test is unnecessary but just as a sanity check... */
	  && expressionP->X_op_symbol == NULL)
	{
	  expressionS two;

	  expressionP->X_op = O_right_shift;
	  two.X_op = O_constant;
	  two.X_add_symbol = two.X_op_symbol = NULL;
	  two.X_add_number = 2;
	  expressionP->X_op_symbol = make_expr_symbol (&two);
	}
      /* allow %st(sym1-sym2) */
      else if (expressionP->X_op == O_subtract
	       && expressionP->X_add_symbol != NULL
	       && expressionP->X_op_symbol != NULL
	       && expressionP->X_add_number == 0)
	{
	  expressionS two;

	  expressionP->X_add_symbol = make_expr_symbol (expressionP);
	  expressionP->X_op = O_right_shift;
	  two.X_op = O_constant;
	  two.X_add_symbol = two.X_op_symbol = NULL;
	  two.X_add_number = 2;
	  expressionP->X_op_symbol = make_expr_symbol (&two);
	}
      else
	{
	  as_bad (_("expression too complex for %%st"));
	  return;
	}
    }
}

/* We have no need to default values of symbols.
   We could catch register names here, but that is handled by inserting
   them all in the symbol table to begin with.  */

symbolS *
md_undefined_symbol (name)
     char *name;
{
  return 0;
}

/* Functions concerning expressions.  */

/* Parse a .byte, .word, etc. expression.

   Values for the status register are specified with %st(label).
   `label' will be right shifted by 2.  */

void
arc_parse_cons_expression (exp, nbytes)
     expressionS *exp;
     int nbytes;
{
  expr (0, exp);
}

/* Record a fixup for a cons expression.  */

void
arc_cons_fix_new (frag, where, nbytes, exp)
     fragS *frag;
     int where;
     int nbytes;
     expressionS *exp;
{
  if (nbytes == 4)
    {
      int reloc_type;
      expressionS exptmp;

      /* This may be a special ARC reloc (eg: %st()).  */
      reloc_type = get_arc_exp_reloc_type (1, BFD_RELOC_32, exp, &exptmp);
      fix_new_exp (frag, where, nbytes, &exptmp, 0, reloc_type);
    }
  else
    {
      fix_new_exp (frag, where, nbytes, exp, 0,
		   nbytes == 2 ? BFD_RELOC_16
		   : nbytes == 8 ? BFD_RELOC_64
		   : BFD_RELOC_32);
    }
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long 
md_pcrel_from (fixP)
     fixS *fixP;
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && ! S_IS_DEFINED (fixP->fx_addsy))
    {
      /* The symbol is undefined.  Let the linker figure it out.  */
      return 0;
    }

  /* Return the address of the delay slot.  */
  return fixP->fx_frag->fr_address + fixP->fx_where + fixP->fx_size;
}

/* Compute the reloc type of an expression.
   The possibly modified expression is stored in EXPNEW.

   This is used to convert the expressions generated by the %-op's into
   the appropriate operand type.  It is called for both data in instructions
   (operands) and data outside instructions (variables, debugging info, etc.).

   Currently supported %-ops:

   %st(symbol): represented as "symbol >> 2"
                "st" is short for STatus as in the status register (pc)

   DEFAULT_TYPE is the type to use if no special processing is required.

   DATA_P is non-zero for data or limm values, zero for insn operands.
   Remember that the opcode "insertion fns" cannot be used on data, they're
   only for inserting operands into insns.  They also can't be used for limm
   values as the insertion routines don't handle limm values.  When called for
   insns we return fudged reloc types (real_value - BFD_RELOC_UNUSED).  When
   called for data or limm values we use real reloc types.  */

static int
get_arc_exp_reloc_type (data_p, default_type, exp, expnew)
     int data_p;
     int default_type;
     expressionS *exp;
     expressionS *expnew;
{
  /* If the expression is "symbol >> 2" we must change it to just "symbol",
     as fix_new_exp can't handle it.  Similarily for (symbol - symbol) >> 2.
     That's ok though.  What's really going on here is that we're using
     ">> 2" as a special syntax for specifying BFD_RELOC_ARC_B26.  */

  if (exp->X_op == O_right_shift
      && exp->X_op_symbol != NULL
      && symbol_constant_p (exp->X_op_symbol)
      && S_GET_VALUE (exp->X_op_symbol) == 2
      && exp->X_add_number == 0)
    {
      if (exp->X_add_symbol != NULL
	  && (symbol_constant_p (exp->X_add_symbol)
	      || symbol_equated_p (exp->X_add_symbol)))
	{
	  *expnew = *exp;
	  expnew->X_op = O_symbol;
	  expnew->X_op_symbol = NULL;
	  return data_p ? BFD_RELOC_ARC_B26 : arc_operand_map['J'];
	}
      else if (exp->X_add_symbol != NULL
	       && (symbol_get_value_expression (exp->X_add_symbol)->X_op
		   == O_subtract))
	{
	  *expnew = *symbol_get_value_expression (exp->X_add_symbol);
	  return data_p ? BFD_RELOC_ARC_B26 : arc_operand_map['J'];
	}
    }

  *expnew = *exp;
  return default_type;
}

/* Apply a fixup to the object code.  This is called for all the
   fixups we generated by the call to fix_new_exp, above.  In the call
   above we used a reloc code which was the largest legal reloc code
   plus the operand index.  Here we undo that to recover the operand
   index.  At this point all symbol values should be fully resolved,
   and we attempt to completely resolve the reloc.  If we can not do
   that, we determine the correct reloc code and put it back in the fixup.  */

int
md_apply_fix3 (fixP, valueP, seg)
     fixS *fixP;
     valueT *valueP;
     segT seg;
{
  /*char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;*/
  valueT value;

  /* FIXME FIXME FIXME: The value we are passed in *valueP includes
     the symbol values.  Since we are using BFD_ASSEMBLER, if we are
     doing this relocation the code in write.c is going to call
     bfd_perform_relocation, which is also going to use the symbol
     value.  That means that if the reloc is fully resolved we want to
     use *valueP since bfd_perform_relocation is not being used.
     However, if the reloc is not fully resolved we do not want to use
     *valueP, and must use fx_offset instead.  However, if the reloc
     is PC relative, we do want to use *valueP since it includes the
     result of md_pcrel_from.  This is confusing.  */

  if (fixP->fx_addsy == (symbolS *) NULL)
    {
      value = *valueP;
      fixP->fx_done = 1;
    }
  else if (fixP->fx_pcrel)
    {
      value = *valueP;
      /* ELF relocations are against symbols.
	 If this symbol is in a different section then we need to leave it for
	 the linker to deal with.  Unfortunately, md_pcrel_from can't tell,
	 so we have to undo it's effects here.  */
      if (S_IS_DEFINED (fixP->fx_addsy)
	  && S_GET_SEGMENT (fixP->fx_addsy) != seg)
	value += md_pcrel_from (fixP);
    }
  else
    {
      value = fixP->fx_offset;
      if (fixP->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixP->fx_subsy) == absolute_section)
	    value -= S_GET_VALUE (fixP->fx_subsy);
	  else
	    {
	      /* We can't actually support subtracting a symbol.  */
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("expression too complex"));
	    }
	}
    }

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      int opindex;
      const struct arc_operand *operand;
      char *where;
      arc_insn insn;

      opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;

      operand = &arc_operands[opindex];

      /* Fetch the instruction, insert the fully resolved operand
	 value, and stuff the instruction back again.  */
      where = fixP->fx_frag->fr_literal + fixP->fx_where;
      if (target_big_endian)
	insn = bfd_getb32 ((unsigned char *) where);
      else
	insn = bfd_getl32 ((unsigned char *) where);
      insn = arc_insert_operand (insn, operand, -1, NULL, (offsetT) value,
				 fixP->fx_file, fixP->fx_line);
      if (target_big_endian)
	bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
      else
	bfd_putl32 ((bfd_vma) insn, (unsigned char *) where);

      if (fixP->fx_done)
	{
	  /* Nothing else to do here.  */
	  return 1;
	}

      /* Determine a BFD reloc value based on the operand information.
	 We are only prepared to turn a few of the operands into relocs.
	 !!! Note that we can't handle limm values here.  Since we're using
	 implicit addends the addend must be inserted into the instruction,
	 however, the opcode insertion routines currently do nothing with
	 limm values.  */
      if (operand->fmt == 'B')
	{
	  assert ((operand->flags & ARC_OPERAND_RELATIVE_BRANCH) != 0
		  && operand->bits == 20
		  && operand->shift == 7);
	  fixP->fx_r_type = BFD_RELOC_ARC_B22_PCREL;
	}
      else if (0 && operand->fmt == 'J')
	{
	  assert ((operand->flags & ARC_OPERAND_ABSOLUTE_BRANCH) != 0
		  && operand->bits == 24
		  && operand->shift == 32);
	  fixP->fx_r_type = BFD_RELOC_ARC_B26;
	}
      else if (0 && operand->fmt == 'L')
	{
	  assert ((operand->flags & ARC_OPERAND_LIMM) != 0
		  && operand->bits == 32
		  && operand->shift == 32);
	  fixP->fx_r_type = BFD_RELOC_32;
	}
      else
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("unresolved expression that must be resolved"));
	  fixP->fx_done = 1;
	  return 1;
	}
    }
  else
    {
      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_8:
	  md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			      value, 1);
	  break;
	case BFD_RELOC_16:
	  md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			      value, 2);
	  break;
	case BFD_RELOC_32:
	  md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			      value, 4);
	  break;
#if 0
	case BFD_RELOC_64:
	  md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			      value, 8);
	  break;
#endif
	case BFD_RELOC_ARC_B26:
	  /* If !fixP->fx_done then `value' is an implicit addend.
	     We must shift it right by 2 in this case as well because the
	     linker performs the relocation and then adds this in (as opposed
	     to adding this in and then shifting right by 2).  */
	  value >>= 2;
	  md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			      value, 4);
	  break;
	default:
	  abort ();
	}
    }

  fixP->fx_addnumber = value;

  return 1;
}

/* Translate internal representation of relocation info to BFD target
   format.  */

arelent *
tc_gen_reloc (section, fixP)
     asection *section;
     fixS *fixP;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("internal error: can't export reloc type %d (`%s')"),
		    fixP->fx_r_type, bfd_get_reloc_code_name (fixP->fx_r_type));
      return NULL;
    }

  assert (!fixP->fx_pcrel == !reloc->howto->pc_relative);

  reloc->addend = fixP->fx_addnumber;

  return reloc;
}

/* Frobbers.  */

#if 0
/* Set the real name if the .rename pseudo-op was used.
   Return 1 if the symbol should not be included in the symbol table.  */

int
arc_frob_symbol (sym)
     symbolS *sym;
{
  if (symbol_get_tc (sym)->real_name != (char *) NULL)
    S_SET_NAME (sym, symbol_get_tc (sym)->real_name);

  return 0;
}
#endif
