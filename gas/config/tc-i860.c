/* tc-i860.c -- Assembler for the Intel i860 architecture.
   Copyright 1989, 1992, 1993, 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

   Brought back from the dead and completely reworked
   by Jason Eckhardt <jle@cygnus.com>.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with GAS; see the file COPYING.  If not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <stdio.h>
#include <string.h>
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/i860.h"
#include "elf/i860.h"


/* The opcode hash table.  */
static struct hash_control *op_hash = NULL;

/* These characters always start a comment.  */
const char comment_chars[] = "#!/";

/* These characters start a comment at the beginning of a line.  */
const char line_comment_chars[] = "#/";

const char line_separator_chars[] = ";";

/* Characters that can be used to separate the mantissa from the exponent
   in floating point numbers.  */
const char EXP_CHARS[] = "eE";

/* Characters that indicate this number is a floating point constant.
   As in 0f12.456 or 0d1.2345e12.  */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Register prefix (depends on syntax).  */
static char reg_prefix;

#define MAX_FIXUPS 2

struct i860_it
{
  char *error;
  unsigned long opcode;
  enum expand_type expand;
  struct i860_fi
  {
    expressionS exp;
    bfd_reloc_code_real_type reloc;
    int pcrel;
    valueT fup;
  } fi[MAX_FIXUPS];
} the_insn;

/* The current fixup count.  */
static int fc;

static char *expr_end;

/* Indicates error if a pseudo operation was expanded after a branch.  */
static char last_expand;

/* If true, then warn if any pseudo operations were expanded.  */
static int target_warn_expand = 0;

/* If true, then XP support is enabled.  */
static int target_xp = 0;

/* If true, then Intel syntax is enabled (default to AT&T/SVR4 syntax).  */
static int target_intel_syntax = 0;


/* Prototypes.  */
static void i860_process_insn (char *);
static void s_dual (int);
static void s_enddual (int);
static void s_atmp (int);
static void s_align_wrapper (int);
static int i860_get_expression (char *);
static bfd_reloc_code_real_type obtain_reloc_for_imm16 (fixS *, long *); 
#ifdef DEBUG_I860
static void print_insn (struct i860_it *);
#endif

const pseudo_typeS md_pseudo_table[] =
{
  {"align",   s_align_wrapper, 0},
  {"dual",    s_dual,          0},
  {"enddual", s_enddual,       0},
  {"atmp",    s_atmp,          0},
  {NULL,      0,               0},
};

/* Dual-instruction mode handling.  */
enum dual
{
  DUAL_OFF = 0, DUAL_ON, DUAL_DDOT, DUAL_ONDDOT,
};
static enum dual dual_mode = DUAL_OFF;

/* Handle ".dual" directive.  */
static void
s_dual (int ignore ATTRIBUTE_UNUSED)
{
  if (target_intel_syntax)
    dual_mode = DUAL_ON;
  else
    as_bad (_("Directive .dual available only with -mintel-syntax option"));
}

/* Handle ".enddual" directive.  */
static void
s_enddual (int ignore ATTRIBUTE_UNUSED)
{
  if (target_intel_syntax)
    dual_mode = DUAL_OFF;
  else
    as_bad (_("Directive .enddual available only with -mintel-syntax option"));
}

/* Temporary register used when expanding assembler pseudo operations.  */
static int atmp = 31;

static void
s_atmp (int ignore ATTRIBUTE_UNUSED)
{
  int temp;

  if (! target_intel_syntax)
    {
      as_bad (_("Directive .atmp available only with -mintel-syntax option"));
      demand_empty_rest_of_line ();
      return;
    }

  if (strncmp (input_line_pointer, "sp", 2) == 0)
    {
      input_line_pointer += 2;
      atmp = 2;
    }
  else if (strncmp (input_line_pointer, "fp", 2) == 0)
    {
      input_line_pointer += 2;
      atmp = 3;
    }
  else if (strncmp (input_line_pointer, "r", 1) == 0)
    {
      input_line_pointer += 1;
      temp = get_absolute_expression ();
      if (temp >= 0 && temp <= 31)
	atmp = temp;
      else
	as_bad (_("Unknown temporary pseudo register"));
    }
  else
    {
      as_bad (_("Unknown temporary pseudo register"));
    }
  demand_empty_rest_of_line ();
}

/* Handle ".align" directive depending on syntax mode.
   AT&T/SVR4 syntax uses the standard align directive.  However, 
   the Intel syntax additionally allows keywords for the alignment
   parameter: ".align type", where type is one of {.short, .long,
   .quad, .single, .double} representing alignments of 2, 4,
   16, 4, and 8, respectively.  */
static void
s_align_wrapper (int arg)
{
  char *parm = input_line_pointer;

  if (target_intel_syntax)
    {
      /* Replace a keyword with the equivalent integer so the
         standard align routine can parse the directive.  */
      if (strncmp (parm, ".short", 6) == 0)
        strncpy (parm, "     2", 6);
      else if (strncmp (parm, ".long", 5) == 0)
        strncpy (parm, "    4", 5);
      else if (strncmp (parm, ".quad", 5) == 0)
        strncpy (parm, "   16", 5);
      else if (strncmp (parm, ".single", 7) == 0)
        strncpy (parm, "      4", 7);
      else if (strncmp (parm, ".double", 7) == 0)
        strncpy (parm, "      8", 7);
     
      while (*input_line_pointer == ' ')
        ++input_line_pointer;
    }

  s_align_bytes (arg);
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables and data structures that the MD part of the
   assembler will need.  */
void
md_begin (void)
{
  const char *retval = NULL;
  int lose = 0;
  unsigned int i = 0;

  op_hash = hash_new ();

  while (i860_opcodes[i].name != NULL)
    {
      const char *name = i860_opcodes[i].name;
      retval = hash_insert (op_hash, name, (PTR)&i860_opcodes[i]);
      if (retval != NULL)
	{
	  fprintf (stderr, _("internal error: can't hash `%s': %s\n"),
		   i860_opcodes[i].name, retval);
	  lose = 1;
	}
      do
	{
	  if (i860_opcodes[i].match & i860_opcodes[i].lose)
	    {
	      fprintf (stderr,
		       _("internal error: losing opcode: `%s' \"%s\"\n"),
		       i860_opcodes[i].name, i860_opcodes[i].args);
	      lose = 1;
	    }
	  ++i;
	}
      while (i860_opcodes[i].name != NULL
	     && strcmp (i860_opcodes[i].name, name) == 0);
    }

  if (lose)
    as_fatal (_("Defective assembler.  No assembly attempted."));

  /* Set the register prefix for either Intel or AT&T/SVR4 syntax.  */
  reg_prefix = target_intel_syntax ? 0 : '%';
}

/* This is the core of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function emits the frags/bytes
   it assembles to.  */
void
md_assemble (char *str)
{
  char *destp;
  int num_opcodes = 1;
  int i;
  struct i860_it pseudo[3];

  assert (str);
  fc = 0;

  /* Assemble the instruction.  */
  i860_process_insn (str);

  /* Check for expandable flag to produce pseudo-instructions.  This
     is an undesirable feature that should be avoided.  */
  if (the_insn.expand != 0 && the_insn.expand != XP_ONLY
      && ! (the_insn.fi[0].fup & (OP_SEL_HA | OP_SEL_H | OP_SEL_L | OP_SEL_GOT
			    | OP_SEL_GOTOFF | OP_SEL_PLT)))
    {
      for (i = 0; i < 3; i++)
	pseudo[i] = the_insn;

      fc = 1;
      switch (the_insn.expand)
	{

	case E_DELAY:
	  num_opcodes = 1;
	  break;

	case E_MOV:
	  if (the_insn.fi[0].exp.X_add_symbol == NULL
	      && the_insn.fi[0].exp.X_op_symbol == NULL
	      && (the_insn.fi[0].exp.X_add_number < (1 << 15)
		  && the_insn.fi[0].exp.X_add_number >= -(1 << 15)))
	    break;

	  /* Emit "or l%const,r0,ireg_dest".  */
	  pseudo[0].opcode = (the_insn.opcode & 0x001f0000) | 0xe4000000;
	  pseudo[0].fi[0].fup = (OP_IMM_S16 | OP_SEL_L);

	  /* Emit "orh h%const,ireg_dest,ireg_dest".  */
	  pseudo[1].opcode = (the_insn.opcode & 0x03ffffff) | 0xec000000
			      | ((the_insn.opcode & 0x001f0000) << 5);
	  pseudo[1].fi[0].fup = (OP_IMM_S16 | OP_SEL_H);

	  num_opcodes = 2;
	  break;

	case E_ADDR:
	  if (the_insn.fi[0].exp.X_add_symbol == NULL
	      && the_insn.fi[0].exp.X_op_symbol == NULL
	      && (the_insn.fi[0].exp.X_add_number < (1 << 15)
		  && the_insn.fi[0].exp.X_add_number >= -(1 << 15)))
	    break;

	  /* Emit "orh ha%addr_expr,ireg_src2,r31".  */
	  pseudo[0].opcode = 0xec000000 | (the_insn.opcode & 0x03e00000)
			     | (atmp << 16);
	  pseudo[0].fi[0].fup = (OP_IMM_S16 | OP_SEL_HA);

	  /* Emit "l%addr_expr(r31),ireg_dest".  We pick up the fixup
	     information from the original instruction.   */
	  pseudo[1].opcode = (the_insn.opcode & ~0x03e00000) | (atmp << 21);
	  pseudo[1].fi[0].fup = the_insn.fi[0].fup | OP_SEL_L;

	  num_opcodes = 2;
	  break;

	case E_U32:
	  if (the_insn.fi[0].exp.X_add_symbol == NULL
	      && the_insn.fi[0].exp.X_op_symbol == NULL
	      && (the_insn.fi[0].exp.X_add_number < (1 << 16)
		  && the_insn.fi[0].exp.X_add_number >= 0))
	    break;

	  /* Emit "$(opcode)h h%const,ireg_src2,r31".  */
	  pseudo[0].opcode = (the_insn.opcode & 0xf3e0ffff) | 0x0c000000
			      | (atmp << 16);
	  pseudo[0].fi[0].fup = (OP_IMM_S16 | OP_SEL_H);

	  /* Emit "$(opcode) l%const,r31,ireg_dest".  */
	  pseudo[1].opcode = (the_insn.opcode & 0xf01f0000) | 0x04000000
			      | (atmp << 21);
	  pseudo[1].fi[0].fup = (OP_IMM_S16 | OP_SEL_L);

	  num_opcodes = 2;
	  break;

	case E_AND:
	  if (the_insn.fi[0].exp.X_add_symbol == NULL
	      && the_insn.fi[0].exp.X_op_symbol == NULL
	      && (the_insn.fi[0].exp.X_add_number < (1 << 16)
		  && the_insn.fi[0].exp.X_add_number >= 0))
	    break;

	  /* Emit "andnot h%const,ireg_src2,r31".  */
	  pseudo[0].opcode = (the_insn.opcode & 0x03e0ffff) | 0xd4000000
			      | (atmp << 16);
	  pseudo[0].fi[0].fup = (OP_IMM_S16 | OP_SEL_H);
	  pseudo[0].fi[0].exp.X_add_number =
            -1 - the_insn.fi[0].exp.X_add_number;

	  /* Emit "andnot l%const,r31,ireg_dest".  */
	  pseudo[1].opcode = (the_insn.opcode & 0x001f0000) | 0xd4000000
			      | (atmp << 21);
	  pseudo[1].fi[0].fup = (OP_IMM_S16 | OP_SEL_L);
	  pseudo[1].fi[0].exp.X_add_number =
            -1 - the_insn.fi[0].exp.X_add_number;

	  num_opcodes = 2;
	  break;

	case E_S32:
	  if (the_insn.fi[0].exp.X_add_symbol == NULL
	      && the_insn.fi[0].exp.X_op_symbol == NULL
	      && (the_insn.fi[0].exp.X_add_number < (1 << 15)
		  && the_insn.fi[0].exp.X_add_number >= -(1 << 15)))
	    break;

	  /* Emit "orh h%const,r0,r31".  */
	  pseudo[0].opcode = 0xec000000 | (atmp << 16);
	  pseudo[0].fi[0].fup = (OP_IMM_S16 | OP_SEL_H);

	  /* Emit "or l%const,r31,r31".  */
	  pseudo[1].opcode = 0xe4000000 | (atmp << 21) | (atmp << 16);
	  pseudo[1].fi[0].fup = (OP_IMM_S16 | OP_SEL_L);

	  /* Emit "r31,ireg_src2,ireg_dest".  */
	  pseudo[2].opcode = (the_insn.opcode & ~0x0400ffff) | (atmp << 11);
	  pseudo[2].fi[0].fup = OP_IMM_S16;

	  num_opcodes = 3;
	  break;

	default:
	  as_fatal (_("failed sanity check."));
	}

      the_insn = pseudo[0];

      /* Warn if an opcode is expanded after a delayed branch.  */
      if (num_opcodes > 1 && last_expand == 1)
	as_warn (_("Expanded opcode after delayed branch: `%s'"), str);

      /* Warn if an opcode is expanded in dual mode.  */
      if (num_opcodes > 1 && dual_mode != DUAL_OFF)
	as_warn (_("Expanded opcode in dual mode: `%s'"), str);

      /* Notify if any expansions happen.  */
      if (target_warn_expand && num_opcodes > 1)
	as_warn (_("An instruction was expanded (%s)"), str);
    }

  i = 0;
  do
    {
      int tmp;

      /* Output the opcode.  Note that the i860 always reads instructions
	 as little-endian data.  */
      destp = frag_more (4);
      number_to_chars_littleendian (destp, the_insn.opcode, 4);

      /* Check for expanded opcode after branch or in dual mode.  */
      last_expand = the_insn.fi[0].pcrel;

      /* Output the symbol-dependent stuff.  Only btne and bte will ever
         loop more than once here, since only they (possibly) have more
         than one fixup.  */
      for (tmp = 0; tmp < fc; tmp++)
        {
          if (the_insn.fi[tmp].fup != OP_NONE)
	    {
	      fixS *fix;
	      fix = fix_new_exp (frag_now,
			         destp - frag_now->fr_literal,
			         4,
			         &the_insn.fi[tmp].exp,
			         the_insn.fi[tmp].pcrel,
			         the_insn.fi[tmp].reloc);

	     /* Despite the odd name, this is a scratch field.  We use
	        it to encode operand type information.  */
	     fix->fx_addnumber = the_insn.fi[tmp].fup;
	   }
        }
      the_insn = pseudo[++i];
    }
  while (--num_opcodes > 0);

}

/* Assemble the instruction pointed to by STR.  */
static void
i860_process_insn (char *str)
{
  char *s;
  const char *args;
  char c;
  struct i860_opcode *insn;
  char *args_start;
  unsigned long opcode;
  unsigned int mask;
  int match = 0;
  int comma = 0;

#if 1 /* For compiler warnings.  */
  args = 0;
  insn = 0;
  args_start = 0;
  opcode = 0;
#endif

  for (s = str; ISLOWER (*s) || *s == '.' || *s == '3'
       || *s == '2' || *s == '1'; ++s)
    ;

  switch (*s)
    {
    case '\0':
      break;

    case ',':
      comma = 1;

      /*FALLTHROUGH*/

    case ' ':
      *s++ = '\0';
      break;

    default:
      as_fatal (_("Unknown opcode: `%s'"), str);
    }

  /* Check for dual mode ("d.") opcode prefix.  */
  if (strncmp (str, "d.", 2) == 0)
    {
      if (dual_mode == DUAL_ON)
	dual_mode = DUAL_ONDDOT;
      else
	dual_mode = DUAL_DDOT;
      str += 2;
    }

  if ((insn = (struct i860_opcode *) hash_find (op_hash, str)) == NULL)
    {
      if (dual_mode == DUAL_DDOT || dual_mode == DUAL_ONDDOT)
	str -= 2;
      as_bad (_("Unknown opcode: `%s'"), str);
      return;
    }

  if (comma)
    *--s = ',';

  args_start = s;
  for (;;)
    {
      int t;
      opcode = insn->match;
      memset (&the_insn, '\0', sizeof (the_insn));
      fc = 0;
      for (t = 0; t < MAX_FIXUPS; t++)
        {
          the_insn.fi[t].reloc = BFD_RELOC_NONE;
          the_insn.fi[t].pcrel = 0;
          the_insn.fi[t].fup = OP_NONE;
        }

      /* Build the opcode, checking as we go that the operands match.  */
      for (args = insn->args; ; ++args)
	{
          if (fc > MAX_FIXUPS)
            abort ();

	  switch (*args)
	    {

	    /* End of args.  */
	    case '\0':
	      if (*s == '\0')
		match = 1;
	      break;

	    /* These must match exactly.  */
	    case '+':
	    case '(':
	    case ')':
	    case ',':
	    case ' ':
	      if (*s++ == *args)
		continue;
	      break;

	    /* Must be at least one digit.  */
	    case '#':
	      if (ISDIGIT (*s++))
		{
		  while (ISDIGIT (*s))
		    ++s;
		  continue;
		}
	      break;

	    /* Next operand must be a register.  */
	    case '1':
	    case '2':
	    case 'd':
	      /* Check for register prefix if necessary.  */
	      if (reg_prefix && *s != reg_prefix)
		goto error;
	      else if (reg_prefix)
		s++;

	      switch (*s)
		{
		/* Frame pointer.  */
		case 'f':
		  s++;
		  if (*s++ == 'p')
		    {
		      mask = 0x3;
		      break;
		    }
		  goto error;

		/* Stack pointer.  */
		case 's':
		  s++;
		  if (*s++ == 'p')
		    {
		      mask = 0x2;
		      break;
		    }
		  goto error;

		/* Any register r0..r31.  */
		case 'r':
		  s++;
		  if (!ISDIGIT (c = *s++))
		    {
		      goto error;
		    }
		  if (ISDIGIT (*s))
		    {
		      if ((c = 10 * (c - '0') + (*s++ - '0')) >= 32)
			goto error;
		    }
		  else
		    c -= '0';
		  mask = c;
		  break;

		/* Not this opcode.  */
		default:
		  goto error;
		}

	      /* Obtained the register, now place it in the opcode.  */
	      switch (*args)
		{
		case '1':
		  opcode |= mask << 11;
		  continue;

		case '2':
		  opcode |= mask << 21;
		  continue;

		case 'd':
		  opcode |= mask << 16;
		  continue;

		}
	      break;

	    /* Next operand is a floating point register.  */
	    case 'e':
	    case 'f':
	    case 'g':
	      /* Check for register prefix if necessary.  */
	      if (reg_prefix && *s != reg_prefix)
		goto error;
	      else if (reg_prefix)
		s++;

	      if (*s++ == 'f' && ISDIGIT (*s))
		{
		  mask = *s++;
		  if (ISDIGIT (*s))
		    {
		      mask = 10 * (mask - '0') + (*s++ - '0');
		      if (mask >= 32)
			{
			  break;
			}
		    }
		  else
		    mask -= '0';

		  switch (*args)
		    {

		    case 'e':
		      opcode |= mask << 11;
		      continue;

		    case 'f':
		      opcode |= mask << 21;
		      continue;

		    case 'g':
		      opcode |= mask << 16;
		      if ((opcode & (1 << 10)) && mask != 0
			  && (mask == ((opcode >> 11) & 0x1f)))
			as_warn (_("Pipelined instruction: fsrc1 = fdest"));
		      continue;
		    }
		}
	      break;

	    /* Next operand must be a control register.  */
	    case 'c':
	      /* Check for register prefix if necessary.  */
	      if (reg_prefix && *s != reg_prefix)
		goto error;
	      else if (reg_prefix)
		s++;

	      if (strncmp (s, "fir", 3) == 0)
		{
		  opcode |= 0x0 << 21;
		  s += 3;
		  continue;
		}
	      if (strncmp (s, "psr", 3) == 0)
		{
		  opcode |= 0x1 << 21;
		  s += 3;
		  continue;
		}
	      if (strncmp (s, "dirbase", 7) == 0)
		{
		  opcode |= 0x2 << 21;
		  s += 7;
		  continue;
		}
	      if (strncmp (s, "db", 2) == 0)
		{
		  opcode |= 0x3 << 21;
		  s += 2;
		  continue;
		}
	      if (strncmp (s, "fsr", 3) == 0)
		{
		  opcode |= 0x4 << 21;
		  s += 3;
		  continue;
		}
	      if (strncmp (s, "epsr", 4) == 0)
		{
		  opcode |= 0x5 << 21;
		  s += 4;
		  continue;
		}
	      /* The remaining control registers are XP only.  */
	      if (target_xp && strncmp (s, "bear", 4) == 0)
		{
		  opcode |= 0x6 << 21;
		  s += 4;
		  continue;
		}
	      if (target_xp && strncmp (s, "ccr", 3) == 0)
		{
		  opcode |= 0x7 << 21;
		  s += 3;
		  continue;
		}
	      if (target_xp && strncmp (s, "p0", 2) == 0)
		{
		  opcode |= 0x8 << 21;
		  s += 2;
		  continue;
		}
	      if (target_xp && strncmp (s, "p1", 2) == 0)
		{
		  opcode |= 0x9 << 21;
		  s += 2;
		  continue;
		}
	      if (target_xp && strncmp (s, "p2", 2) == 0)
		{
		  opcode |= 0xa << 21;
		  s += 2;
		  continue;
		}
	      if (target_xp && strncmp (s, "p3", 2) == 0)
		{
		  opcode |= 0xb << 21;
		  s += 2;
		  continue;
		}
	      break;

	    /* 5-bit immediate in src1.  */
	    case '5':
	      if (! i860_get_expression (s))
		{
		  s = expr_end;
		  the_insn.fi[fc].fup |= OP_IMM_U5;
		  fc++;
		  continue;
		}
	      break;

	    /* 26-bit immediate, relative branch (lbroff).  */
	    case 'l':
	      the_insn.fi[fc].pcrel = 1;
	      the_insn.fi[fc].fup |= OP_IMM_BR26;
	      goto immediate;

	    /* 16-bit split immediate, relative branch (sbroff).  */
	    case 'r':
	      the_insn.fi[fc].pcrel = 1;
	      the_insn.fi[fc].fup |= OP_IMM_BR16;
	      goto immediate;

	    /* 16-bit split immediate.  */
	    case 's':
	      the_insn.fi[fc].fup |= OP_IMM_SPLIT16;
	      goto immediate;

	    /* 16-bit split immediate, byte aligned (st.b).  */
	    case 'S':
	      the_insn.fi[fc].fup |= OP_IMM_SPLIT16;
	      goto immediate;

	    /* 16-bit split immediate, half-word aligned (st.s).  */
	    case 'T':
	      the_insn.fi[fc].fup |= (OP_IMM_SPLIT16 | OP_ENCODE1 | OP_ALIGN2);
	      goto immediate;

	    /* 16-bit split immediate, word aligned (st.l).  */
	    case 'U':
	      the_insn.fi[fc].fup |= (OP_IMM_SPLIT16 | OP_ENCODE1 | OP_ALIGN4);
	      goto immediate;

	    /* 16-bit immediate.  */
	    case 'i':
	      the_insn.fi[fc].fup |= OP_IMM_S16;
	      goto immediate;

	    /* 16-bit immediate, byte aligned (ld.b).  */
	    case 'I':
	      the_insn.fi[fc].fup |= OP_IMM_S16;
	      goto immediate;

	    /* 16-bit immediate, half-word aligned (ld.s).  */
	    case 'J':
	      the_insn.fi[fc].fup |= (OP_IMM_S16 | OP_ENCODE1 | OP_ALIGN2);
	      goto immediate;

	    /* 16-bit immediate, word aligned (ld.l, {p}fld.l, fst.l).  */
	    case 'K':
	      if (insn->name[0] == 'l')
		the_insn.fi[fc].fup |= (OP_IMM_S16 | OP_ENCODE1 | OP_ALIGN4);
	      else
		the_insn.fi[fc].fup |= (OP_IMM_S16 | OP_ENCODE2 | OP_ALIGN4);
	      goto immediate;

	    /* 16-bit immediate, double-word aligned ({p}fld.d, fst.d).  */
	    case 'L':
	      the_insn.fi[fc].fup |= (OP_IMM_S16 | OP_ENCODE3 | OP_ALIGN8);
	      goto immediate;

	    /* 16-bit immediate, quad-word aligned (fld.q, fst.q).  */
	    case 'M':
	      the_insn.fi[fc].fup |= (OP_IMM_S16 | OP_ENCODE3 | OP_ALIGN16);

	      /*FALLTHROUGH*/

	      /* Handle the immediate for either the Intel syntax or
		 SVR4 syntax. The Intel syntax is "ha%immediate"
		 whereas SVR4 syntax is "[immediate]@ha".  */
	    immediate:
	      if (target_intel_syntax == 0)
		{
		  /* AT&T/SVR4 syntax.  */
	          if (*s == ' ')
		    s++;

	          /* Note that if i860_get_expression() fails, we will still
	  	     have created U entries in the symbol table for the
		     'symbols' in the input string.  Try not to create U
		     symbols for registers, etc.  */
	          if (! i860_get_expression (s))
		    s = expr_end;
	          else
		    goto error;

	          if (strncmp (s, "@ha", 3) == 0)
		    {
		      the_insn.fi[fc].fup |= OP_SEL_HA;
		      s += 3;
		    }
	          else if (strncmp (s, "@h", 2) == 0)
		    {
		      the_insn.fi[fc].fup |= OP_SEL_H;
		      s += 2;
		    }
	          else if (strncmp (s, "@l", 2) == 0)
		    {
		      the_insn.fi[fc].fup |= OP_SEL_L;
		      s += 2;
		    }
	          else if (strncmp (s, "@gotoff", 7) == 0
		           || strncmp (s, "@GOTOFF", 7) == 0)
		    {
		      as_bad (_("Assembler does not yet support PIC"));
		      the_insn.fi[fc].fup |= OP_SEL_GOTOFF;
		      s += 7;
		    }
	          else if (strncmp (s, "@got", 4) == 0
		           || strncmp (s, "@GOT", 4) == 0)
		    {
		      as_bad (_("Assembler does not yet support PIC"));
		      the_insn.fi[fc].fup |= OP_SEL_GOT;
		      s += 4;
		    }
	          else if (strncmp (s, "@plt", 4) == 0
		           || strncmp (s, "@PLT", 4) == 0)
		    {
		      as_bad (_("Assembler does not yet support PIC"));
		      the_insn.fi[fc].fup |= OP_SEL_PLT;
		      s += 4;
		    }

	          the_insn.expand = insn->expand;
                  fc++;
              
	          continue;
		}
	      else
		{
		  /* Intel syntax.  */
	          if (*s == ' ')
		    s++;
	          if (strncmp (s, "ha%", 3) == 0)
		    {
		      the_insn.fi[fc].fup |= OP_SEL_HA;
		      s += 3;
		    }
	          else if (strncmp (s, "h%", 2) == 0)
		    {
		      the_insn.fi[fc].fup |= OP_SEL_H;
		      s += 2;
		    }
	          else if (strncmp (s, "l%", 2) == 0)
		    {
		      the_insn.fi[fc].fup |= OP_SEL_L;
		      s += 2;
		    }
	          the_insn.expand = insn->expand;

	          /* Note that if i860_get_expression() fails, we will still
		     have created U entries in the symbol table for the
		     'symbols' in the input string.  Try not to create U
		     symbols for registers, etc.  */
	          if (! i860_get_expression (s))
		    s = expr_end;
	          else
		    goto error;

                  fc++;
	          continue;
		}
	      break;

	    default:
	      as_fatal (_("failed sanity check."));
	    }
	  break;
	}
    error:
      if (match == 0)
	{
	  /* Args don't match.  */
	  if (insn[1].name != NULL
	      && ! strcmp (insn->name, insn[1].name))
	    {
	      ++insn;
	      s = args_start;
	      continue;
	    }
	  else
	    {
	      as_bad (_("Illegal operands for %s"), insn->name);
	      return;
	    }
	}
      break;
    }

  /* Set the dual bit on this instruction if necessary.  */
  if (dual_mode != DUAL_OFF)
    {
      if ((opcode & 0xfc000000) == 0x48000000 || opcode == 0xb0000000)
        {
	  /* The instruction is a flop or a fnop, so set its dual bit
	     (but check that it is 8-byte aligned).  */
	  if (((frag_now->fr_address + frag_now_fix_octets ()) & 7) == 0)
	    opcode |= (1 << 9);
	  else
            as_bad (_("'d.%s' must be 8-byte aligned"), insn->name);

          if (dual_mode == DUAL_DDOT)
	    dual_mode = DUAL_OFF;
          else if (dual_mode == DUAL_ONDDOT)
	    dual_mode = DUAL_ON;
        }
      else if (dual_mode == DUAL_DDOT || dual_mode == DUAL_ONDDOT)
        as_bad (_("Prefix 'd.' invalid for instruction `%s'"), insn->name);
    }

  the_insn.opcode = opcode;

  /* Only recognize XP instructions when the user has requested it.  */
  if (insn->expand == XP_ONLY && ! target_xp)
    as_bad (_("Unknown opcode: `%s'"), insn->name);
}

static int
i860_get_expression (char *str)
{
  char *save_in;
  segT seg;

  save_in = input_line_pointer;
  input_line_pointer = str;
  seg = expression (&the_insn.fi[fc].exp);
  if (seg != absolute_section
      && seg != undefined_section
      && ! SEG_NORMAL (seg))
    {
      the_insn.error = _("bad segment");
      expr_end = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
  expr_end = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* Turn a string in input_line_pointer into a floating point constant of
   type TYPE, and store the appropriate bytes in *LITP.  The number of
   LITTLENUMS emitted is stored in *SIZEP.  An error message is returned,
   or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

char *
md_atof (int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  char *t;

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
      *sizeP = 0;
      return _("Bad call to MD_ATOF()");
    }
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

/* Write out in current endian mode.  */
void
md_number_to_chars (char *buf, valueT val, int n)
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* This should never be called for i860.  */
int
md_estimate_size_before_relax (register fragS *fragP ATTRIBUTE_UNUSED,
			       segT segtype ATTRIBUTE_UNUSED)
{
  as_fatal (_("i860_estimate_size_before_relax\n"));
}

#ifdef DEBUG_I860
static void
print_insn (struct i860_it *insn)
{
  if (insn->error)
    fprintf (stderr, "ERROR: %s\n", insn->error);

  fprintf (stderr, "opcode = 0x%08lx\t", insn->opcode);
  fprintf (stderr, "expand = 0x%x\t", insn->expand);
  fprintf (stderr, "reloc = %s\t\n",
	   bfd_get_reloc_code_name (insn->reloc));
  fprintf (stderr, "exp =  {\n");
  fprintf (stderr, "\t\tX_add_symbol = %s\n",
	   insn->exp.X_add_symbol ?
	   (S_GET_NAME (insn->exp.X_add_symbol) ?
	    S_GET_NAME (insn->exp.X_add_symbol) : "???") : "0");
  fprintf (stderr, "\t\tX_op_symbol = %s\n",
	   insn->exp.X_op_symbol ?
	   (S_GET_NAME (insn->exp.X_op_symbol) ?
	    S_GET_NAME (insn->exp.X_op_symbol) : "???") : "0");
  fprintf (stderr, "\t\tX_add_number = %lx\n",
	   insn->exp.X_add_number);
  fprintf (stderr, "}\n");
}
#endif /* DEBUG_I860 */


#ifdef OBJ_ELF
const char *md_shortopts = "VQ:";
#else
const char *md_shortopts = "";
#endif

#define OPTION_EB		(OPTION_MD_BASE + 0)
#define OPTION_EL		(OPTION_MD_BASE + 1)
#define OPTION_WARN_EXPAND	(OPTION_MD_BASE + 2)
#define OPTION_XP		(OPTION_MD_BASE + 3)
#define OPTION_INTEL_SYNTAX	(OPTION_MD_BASE + 4)

struct option md_longopts[] = {
  { "EB",	    no_argument, NULL, OPTION_EB },
  { "EL",	    no_argument, NULL, OPTION_EL },
  { "mwarn-expand", no_argument, NULL, OPTION_WARN_EXPAND },
  { "mxp",	    no_argument, NULL, OPTION_XP },
  { "mintel-syntax",no_argument, NULL, OPTION_INTEL_SYNTAX },
  { NULL,	    no_argument, NULL, 0 }
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case OPTION_EB:
      target_big_endian = 1;
      break;

    case OPTION_EL:
      target_big_endian = 0;
      break;

    case OPTION_WARN_EXPAND:
      target_warn_expand = 1;
      break;

    case OPTION_XP:
      target_xp = 1;
      break;

    case OPTION_INTEL_SYNTAX:
      target_intel_syntax = 1;
      break;

#ifdef OBJ_ELF
    /* SVR4 argument compatibility (-V): print version ID.  */
    case 'V':
      print_version_id ();
      break;

    /* SVR4 argument compatibility (-Qy, -Qn): controls whether
       a .comment section should be emitted or not (ignored).  */
    case 'Q':
      break;
#endif

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (FILE *stream)
{
  fprintf (stream, _("\
  -EL			  generate code for little endian mode (default)\n\
  -EB			  generate code for big endian mode\n\
  -mwarn-expand		  warn if pseudo operations are expanded\n\
  -mxp			  enable i860XP support (disabled by default)\n\
  -mintel-syntax	  enable Intel syntax (default to AT&T/SVR4)\n"));
#ifdef OBJ_ELF
  /* SVR4 compatibility flags.  */
  fprintf (stream, _("\
  -V			  print assembler version number\n\
  -Qy, -Qn		  ignored\n"));
#endif
}


/* We have no need to default values of symbols.  */
symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

/* The i860 denotes auto-increment with '++'.  */
void
md_operand (expressionS *exp)
{
  char *s;

  for (s = input_line_pointer; *s; s++)
    {
      if (s[0] == '+' && s[1] == '+')
	{
	  input_line_pointer += 2;
	  exp->X_op = O_register;
	  break;
	}
    }
}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segT segment ATTRIBUTE_UNUSED,
		  valueT size ATTRIBUTE_UNUSED)
{
  /* Byte alignment is fine.  */
  return size;
}

/* On the i860, a PC-relative offset is relative to the address of the
   offset plus its size.  */
long
md_pcrel_from (fixS *fixP)
{
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* Determine the relocation needed for non PC-relative 16-bit immediates.
   Also adjust the given immediate as necessary.  Finally, check that
   all constraints (such as alignment) are satisfied.   */
static bfd_reloc_code_real_type
obtain_reloc_for_imm16 (fixS *fix, long *val)
{
  valueT fup = fix->fx_addnumber;
  bfd_reloc_code_real_type reloc;

  if (fix->fx_pcrel)
    abort ();

  /* Check alignment restrictions.  */
  if ((fup & OP_ALIGN2) && (*val & 0x1))
    as_bad_where (fix->fx_file, fix->fx_line,
		  _("This immediate requires 0 MOD 2 alignment"));
  else if ((fup & OP_ALIGN4) && (*val & 0x3))
    as_bad_where (fix->fx_file, fix->fx_line,
		  _("This immediate requires 0 MOD 4 alignment"));
  else if ((fup & OP_ALIGN8) && (*val & 0x7))
    as_bad_where (fix->fx_file, fix->fx_line,
		  _("This immediate requires 0 MOD 8 alignment"));
  else if ((fup & OP_ALIGN16) && (*val & 0xf))
    as_bad_where (fix->fx_file, fix->fx_line,
		  _("This immediate requires 0 MOD 16 alignment"));

  if (fup & OP_SEL_HA)
    {
      *val = (*val >> 16) + (*val & 0x8000 ? 1 : 0);
      reloc = BFD_RELOC_860_HIGHADJ;
    }
  else if (fup & OP_SEL_H)
    {
      *val >>= 16;
      reloc = BFD_RELOC_860_HIGH;
    }
  else if (fup & OP_SEL_L)
    {
      int num_encode;
      if (fup & OP_IMM_SPLIT16)
	{
	  if (fup & OP_ENCODE1)
	    {
	      num_encode = 1;
	      reloc = BFD_RELOC_860_SPLIT1;
	    }
	  else if (fup & OP_ENCODE2)
	    {
	      num_encode = 2;
	      reloc = BFD_RELOC_860_SPLIT2;
	    }
	  else
	    {
	      num_encode = 0;
	      reloc = BFD_RELOC_860_SPLIT0;
	    }
	}
      else
	{
	  if (fup & OP_ENCODE1)
	    {
	      num_encode = 1;
	      reloc = BFD_RELOC_860_LOW1;
	    }
	  else if (fup & OP_ENCODE2)
	    {
	      num_encode = 2;
	      reloc = BFD_RELOC_860_LOW2;
	    }
	  else if (fup & OP_ENCODE3)
	    {
	      num_encode = 3;
	      reloc = BFD_RELOC_860_LOW3;
	    }
	  else
	    {
	      num_encode = 0;
	      reloc = BFD_RELOC_860_LOW0;
	    }
	}

      /* Preserve size encode bits.  */
      *val &= ~((1 << num_encode) - 1);
    }
  else
    {
      /* No selector.  What reloc do we generate (???)?  */
      reloc = BFD_RELOC_32;
    }

  return reloc;
}

/* Attempt to simplify or eliminate a fixup. To indicate that a fixup
   has been eliminated, set fix->fx_done. If fix->fx_addsy is non-NULL,
   we will have to generate a reloc entry.  */

void
md_apply_fix (fixS *fix, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  char *buf;
  long val = *valP;
  unsigned long insn;
  valueT fup;

  buf = fix->fx_frag->fr_literal + fix->fx_where;

  /* Recall that earlier we stored the opcode little-endian.  */
  insn = bfd_getl32 (buf);

  /* We stored a fix-up in this oddly-named scratch field.  */
  fup = fix->fx_addnumber;

  /* Determine the necessary relocations as well as inserting an
     immediate into the instruction.   */
  if (fup & OP_IMM_U5)
    {
      if (val & ~0x1f)
	as_bad_where (fix->fx_file, fix->fx_line,
		      _("5-bit immediate too large"));
      if (fix->fx_addsy)
	as_bad_where (fix->fx_file, fix->fx_line,
		      _("5-bit field must be absolute"));

      insn |= (val & 0x1f) << 11;
      bfd_putl32 (insn, buf);
      fix->fx_r_type = BFD_RELOC_NONE;
      fix->fx_done = 1;
    }
  else if (fup & OP_IMM_S16)
    {
      fix->fx_r_type = obtain_reloc_for_imm16 (fix, &val);

      /* Insert the immediate.  */
      if (fix->fx_addsy)
	fix->fx_done = 0;
      else
	{
	  insn |= val & 0xffff;
	  bfd_putl32 (insn, buf);
	  fix->fx_r_type = BFD_RELOC_NONE;
	  fix->fx_done = 1;
	}
    }
  else if (fup & OP_IMM_U16)
    abort ();

  else if (fup & OP_IMM_SPLIT16)
    {
      fix->fx_r_type = obtain_reloc_for_imm16 (fix, &val);

      /* Insert the immediate.  */
      if (fix->fx_addsy)
	fix->fx_done = 0;
      else
	{
	  insn |= val & 0x7ff;
	  insn |= (val & 0xf800) << 5;
	  bfd_putl32 (insn, buf);
	  fix->fx_r_type = BFD_RELOC_NONE;
	  fix->fx_done = 1;
	}
    }
  else if (fup & OP_IMM_BR16)
    {
      if (val & 0x3)
	as_bad_where (fix->fx_file, fix->fx_line,
		      _("A branch offset requires 0 MOD 4 alignment"));

      val = val >> 2;

      /* Insert the immediate.  */
      if (fix->fx_addsy)
	{
	  fix->fx_done = 0;
	  fix->fx_r_type = BFD_RELOC_860_PC16;
	}
      else
	{
	  insn |= (val & 0x7ff);
	  insn |= ((val & 0xf800) << 5);
	  bfd_putl32 (insn, buf);
	  fix->fx_r_type = BFD_RELOC_NONE;
	  fix->fx_done = 1;
	}
    }
  else if (fup & OP_IMM_BR26)
    {
      if (val & 0x3)
	as_bad_where (fix->fx_file, fix->fx_line,
		      _("A branch offset requires 0 MOD 4 alignment"));

      val >>= 2;

      /* Insert the immediate.  */
      if (fix->fx_addsy)
	{
	  fix->fx_r_type = BFD_RELOC_860_PC26;
	  fix->fx_done = 0;
	}
      else
	{
	  insn |= (val & 0x3ffffff);
	  bfd_putl32 (insn, buf);
	  fix->fx_r_type = BFD_RELOC_NONE;
	  fix->fx_done = 1;
	}
    }
  else if (fup != OP_NONE)
    {
      as_bad_where (fix->fx_file, fix->fx_line,
		    _("Unrecognized fix-up (0x%08lx)"), (unsigned long) fup);
      abort ();
    }
  else
    {
      /* I believe only fix-ups such as ".long .ep.main-main+0xc8000000"
 	 reach here (???).  */
      if (fix->fx_addsy)
	{
	  fix->fx_r_type = BFD_RELOC_32;
	  fix->fx_done = 0;
	}
      else
	{
	  insn |= (val & 0xffffffff);
	  bfd_putl32 (insn, buf);
	  fix->fx_r_type = BFD_RELOC_NONE;
	  fix->fx_done = 1;
	}
    }
}

/* Generate a machine dependent reloc from a fixup.  */
arelent*
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED,
	      fixS *fixp)
{
  arelent *reloc;

  reloc = xmalloc (sizeof (*reloc));
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->addend = fixp->fx_offset;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);

  if (! reloc->howto)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
                    "Cannot represent %s relocation in object file",
                    bfd_get_reloc_code_name (fixp->fx_r_type));
    }
  return reloc;
}

/* This is called from HANDLE_ALIGN in write.c.  Fill in the contents
   of an rs_align_code fragment.  */

void
i860_handle_align (fragS *fragp)
{
  /* Instructions are always stored little-endian on the i860.  */
  static const unsigned char le_nop[] = { 0x00, 0x00, 0x00, 0xA0 };

  int bytes;
  char *p;

  if (fragp->fr_type != rs_align_code)
    return;

  bytes = fragp->fr_next->fr_address - fragp->fr_address - fragp->fr_fix;
  p = fragp->fr_literal + fragp->fr_fix;

  /* Make sure we are on a 4-byte boundary, in case someone has been
     putting data into a text section.  */
  if (bytes & 3)
    {
      int fix = bytes & 3;
      memset (p, 0, fix);
      p += fix;
      fragp->fr_fix += fix;
    }

  memcpy (p, le_nop, 4);
  fragp->fr_var = 4;
}

/* This is called after a user-defined label is seen.  We check
   if the label has a double colon (valid in Intel syntax mode only),
   in which case it should be externalized.  */

void
i860_check_label (symbolS *labelsym)
{
  /* At this point, the current line pointer is sitting on the character
     just after the first colon on the label.  */ 
  if (target_intel_syntax && *input_line_pointer == ':')
    {
      S_SET_EXTERNAL (labelsym);
      input_line_pointer++;
    }
}

