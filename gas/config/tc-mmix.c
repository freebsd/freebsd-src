/* tc-mmix.c -- Assembler for Don Knuth's MMIX.
   Copyright (C) 2001, 2002, 2003 Free Software Foundation.

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

/* Knuth's assembler mmixal does not provide a relocatable format; mmo is
   to be considered a final link-format.  In the final link, we make mmo,
   but for relocatable files, we use ELF.

   One goal is to provide a superset of what mmixal does, including
   compatible syntax, but the main purpose is to serve GCC.  */


#include <stdio.h>
#include <limits.h>
#include "as.h"
#include "subsegs.h"
#include "bfd.h"
#include "elf/mmix.h"
#include "opcode/mmix.h"
#include "safe-ctype.h"
#include "dwarf2dbg.h"
#include "obstack.h"

/* Something to describe what we need to do with a fixup before output,
   for example assert something of what it became or make a relocation.  */

enum mmix_fixup_action
 {
   mmix_fixup_byte,
   mmix_fixup_register,
   mmix_fixup_register_or_adjust_for_byte
 };

static int get_spec_regno PARAMS ((char *));
static int get_operands PARAMS ((int, char *, expressionS[]));
static int get_putget_operands
  PARAMS ((struct mmix_opcode *, char *, expressionS[]));
static void s_prefix PARAMS ((int));
static void s_greg PARAMS ((int));
static void s_loc PARAMS ((int));
static void s_bspec PARAMS ((int));
static void s_espec PARAMS ((int));
static void mmix_s_local PARAMS ((int));
static void mmix_greg_internal PARAMS ((char *));
static void mmix_set_geta_branch_offset PARAMS ((char *, offsetT value));
static void mmix_set_jmp_offset PARAMS ((char *, offsetT));
static void mmix_fill_nops PARAMS ((char *, int));
static int cmp_greg_symbol_fixes PARAMS ((const PTR, const PTR));
static int cmp_greg_val_greg_symbol_fixes
  PARAMS ((const PTR p1, const PTR p2));
static void mmix_handle_rest_of_empty_line PARAMS ((void));
static void mmix_discard_rest_of_line PARAMS ((void));
static void mmix_byte PARAMS ((void));
static void mmix_cons PARAMS ((int));

/* Continue the tradition of symbols.c; use control characters to enforce
   magic.  These are used when replacing e.g. 8F and 8B so we can handle
   such labels correctly with the common parser hooks.  */
#define MAGIC_FB_BACKWARD_CHAR '\003'
#define MAGIC_FB_FORWARD_CHAR '\004'

/* Copy the location of a frag to a fix.  */
#define COPY_FR_WHERE_TO_FX(FRAG, FIX)		\
 do						\
   {						\
     (FIX)->fx_file = (FRAG)->fr_file;		\
     (FIX)->fx_line = (FRAG)->fr_line;		\
   }						\
 while (0)

const char *md_shortopts = "x";
static int current_fb_label = -1;
static char *pending_label = NULL;

static bfd_vma lowest_text_loc = (bfd_vma) -1;
static int text_has_contents = 0;

/* The alignment of the previous instruction, and a boolean for whether we
   want to avoid aligning the next WYDE, TETRA, OCTA or insn.  */
static int last_alignment = 0;
static int want_unaligned = 0;

static bfd_vma lowest_data_loc = (bfd_vma) -1;
static int data_has_contents = 0;

/* The fragS of the instruction being assembled.  Only valid from within
   md_assemble.  */
fragS *mmix_opcode_frag = NULL;

/* Raw GREGs as appearing in input.  These may be fewer than the number
   after relaxing.  */
static int n_of_raw_gregs = 0;
static struct
 {
   char *label;
   expressionS exp;
 } mmix_raw_gregs[MAX_GREGS];

/* Fixups for all unique GREG registers.  We store the fixups here in
   md_convert_frag, then we use the array to convert
   BFD_RELOC_MMIX_BASE_PLUS_OFFSET fixups in tc_gen_reloc.  The index is
   just a running number and is not supposed to be correlated to a
   register number.  */
static fixS *mmix_gregs[MAX_GREGS];
static int n_of_cooked_gregs = 0;

/* Pointing to the register section we use for output.  */
static asection *real_reg_section;

/* For each symbol; unknown or section symbol, we keep a list of GREG
   definitions sorted on increasing offset.  It seems no use keeping count
   to allocate less room than the maximum number of gregs when we've found
   one for a section or symbol.  */
struct mmix_symbol_gregs
 {
   int n_gregs;
   struct mmix_symbol_greg_fixes
   {
     fixS *fix;

     /* A signed type, since we may have GREGs pointing slightly before the
	contents of a section.  */
     offsetT offs;
   } greg_fixes[MAX_GREGS];
 };

/* Should read insert a colon on something that starts in column 0 on
   this line?  */
static int label_without_colon_this_line = 1;

/* Should we automatically expand instructions into multiple insns in
   order to generate working code?  */
static int expand_op = 1;

/* Should we warn when expanding operands?  FIXME: test-cases for when -x
   is absent.  */
static int warn_on_expansion = 1;

/* Should we merge non-zero GREG register definitions?  */
static int merge_gregs = 1;

/* Should we pass on undefined BFD_RELOC_MMIX_BASE_PLUS_OFFSET relocs
   (missing suitable GREG definitions) to the linker?  */
static int allocate_undefined_gregs_in_linker = 0;

/* Should we emit built-in symbols?  */
static int predefined_syms = 1;

/* Should we allow anything but the listed special register name
   (e.g. equated symbols)?  */
static int equated_spec_regs = 1;

/* Do we require standard GNU syntax?  */
int mmix_gnu_syntax = 0;

/* Do we globalize all symbols?  */
int mmix_globalize_symbols = 0;

/* When expanding insns, do we want to expand PUSHJ as a call to a stub
   (or else as a series of insns)?  */
int pushj_stubs = 1;

/* Do we know that the next semicolon is at the end of the operands field
   (in mmixal mode; constant 1 in GNU mode)?  */
int mmix_next_semicolon_is_eoln = 1;

/* Do we have a BSPEC in progress?  */
static int doing_bspec = 0;
static char *bspec_file;
static unsigned int bspec_line;

struct option md_longopts[] =
 {
#define OPTION_RELAX  (OPTION_MD_BASE)
#define OPTION_NOEXPAND  (OPTION_RELAX + 1)
#define OPTION_NOMERGEGREG  (OPTION_NOEXPAND + 1)
#define OPTION_NOSYMS  (OPTION_NOMERGEGREG + 1)
#define OPTION_GNU_SYNTAX  (OPTION_NOSYMS + 1)
#define OPTION_GLOBALIZE_SYMBOLS  (OPTION_GNU_SYNTAX + 1)
#define OPTION_FIXED_SPEC_REGS  (OPTION_GLOBALIZE_SYMBOLS + 1)
#define OPTION_LINKER_ALLOCATED_GREGS  (OPTION_FIXED_SPEC_REGS + 1)
#define OPTION_NOPUSHJSTUBS  (OPTION_LINKER_ALLOCATED_GREGS + 1)
   {"linkrelax", no_argument, NULL, OPTION_RELAX},
   {"no-expand", no_argument, NULL, OPTION_NOEXPAND},
   {"no-merge-gregs", no_argument, NULL, OPTION_NOMERGEGREG},
   {"no-predefined-syms", no_argument, NULL, OPTION_NOSYMS},
   {"gnu-syntax", no_argument, NULL, OPTION_GNU_SYNTAX},
   {"globalize-symbols", no_argument, NULL, OPTION_GLOBALIZE_SYMBOLS},
   {"fixed-special-register-names", no_argument, NULL,
    OPTION_FIXED_SPEC_REGS},
   {"linker-allocated-gregs", no_argument, NULL,
    OPTION_LINKER_ALLOCATED_GREGS},
   {"no-pushj-stubs", no_argument, NULL, OPTION_NOPUSHJSTUBS},
   {"no-stubs", no_argument, NULL, OPTION_NOPUSHJSTUBS},
   {NULL, no_argument, NULL, 0}
 };

size_t md_longopts_size = sizeof (md_longopts);

static struct hash_control *mmix_opcode_hash;

/* We use these when implementing the PREFIX pseudo.  */
char *mmix_current_prefix;
struct obstack mmix_sym_obstack;


/* For MMIX, we encode the relax_substateT:s (in e.g. fr_substate) as one
   bit length, and the relax-type shifted on top of that.  There seems to
   be no point in making the relaxation more fine-grained; the linker does
   that better and we might interfere by changing non-optimal relaxations
   into other insns that cannot be relaxed as easily.

   Groups for MMIX relaxing:

   1. GETA
      extra length: zero or three insns.

   2. Bcc
      extra length: zero or five insns.

   3. PUSHJ
      extra length: zero or four insns.
      Special handling to deal with transition to PUSHJSTUB.

   4. JMP
      extra length: zero or four insns.

   5. GREG
      special handling, allocates a named global register unless another
      is within reach for all uses.

   6. PUSHJSTUB
      special handling (mostly) for external references; assumes the
      linker will generate a stub if target is no longer than 256k from
      the end of the section plus max size of previous stubs.  Zero or
      four insns.  */

#define STATE_GETA	(1)
#define STATE_BCC	(2)
#define STATE_PUSHJ	(3)
#define STATE_JMP	(4)
#define STATE_GREG	(5)
#define STATE_PUSHJSTUB	(6)

/* No fine-grainedness here.  */
#define STATE_LENGTH_MASK	    (1)

#define STATE_ZERO		    (0)
#define STATE_MAX		    (1)

/* More descriptive name for convenience.  */
/* FIXME: We should start on something different, not MAX.  */
#define STATE_UNDF		    STATE_MAX

/* FIXME: For GREG, we must have other definitions; UNDF == MAX isn't
   appropriate; we need it the other way round.  This value together with
   fragP->tc_frag_data shows what state the frag is in: tc_frag_data
   non-NULL means 0, NULL means 8 bytes.  */
#define STATE_GREG_UNDF ENCODE_RELAX (STATE_GREG, STATE_ZERO)
#define STATE_GREG_DEF ENCODE_RELAX (STATE_GREG, STATE_MAX)

/* These displacements are relative to the address following the opcode
   word of the instruction.  The catch-all states have zero for "reach"
   and "next" entries.  */

#define GETA_0F (65536 * 4 - 8)
#define GETA_0B (-65536 * 4 - 4)

#define GETA_MAX_LEN 4 * 4
#define GETA_3F 0
#define GETA_3B 0

#define BCC_0F GETA_0F
#define BCC_0B GETA_0B

#define BCC_MAX_LEN 6 * 4
#define BCC_5F GETA_3F
#define BCC_5B GETA_3B

#define PUSHJ_0F GETA_0F
#define PUSHJ_0B GETA_0B

#define PUSHJ_MAX_LEN 5 * 4
#define PUSHJ_4F GETA_3F
#define PUSHJ_4B GETA_3B

/* We'll very rarely have sections longer than LONG_MAX, but we'll make a
   feeble attempt at getting 64-bit C99 or gcc-specific values (assuming
   long long is 64 bits on the host).  */
#ifdef LLONG_MIN
#define PUSHJSTUB_MIN LLONG_MIN
#elsif defined (LONG_LONG_MIN)
#define PUSHJSTUB_MIN LONG_LONG_MIN
#else
#define PUSHJSTUB_MIN LONG_MIN
#endif
#ifdef LLONG_MAX
#define PUSHJSTUB_MAX LLONG_MAX
#elsif defined (LONG_LONG_MAX)
#define PUSHJSTUB_MAX LONG_LONG_MAX
#else
#define PUSHJSTUB_MAX LONG_MAX
#endif

#define JMP_0F (65536 * 256 * 4 - 8)
#define JMP_0B (-65536 * 256 * 4 - 4)

#define JMP_MAX_LEN 5 * 4
#define JMP_4F 0
#define JMP_4B 0

#define RELAX_ENCODE_SHIFT 1
#define ENCODE_RELAX(what, length) (((what) << RELAX_ENCODE_SHIFT) + (length))

const relax_typeS mmix_relax_table[] =
 {
   /* Error sentinel (0, 0).  */
   {1,		1,		0,	0},

   /* Unused (0, 1).  */
   {1,		1,		0,	0},

   /* GETA (1, 0).  */
   {GETA_0F,	GETA_0B,	0,	ENCODE_RELAX (STATE_GETA, STATE_MAX)},

   /* GETA (1, 1).  */
   {GETA_3F,	GETA_3B,
		GETA_MAX_LEN - 4,	0},

   /* BCC (2, 0).  */
   {BCC_0F,	BCC_0B,		0,	ENCODE_RELAX (STATE_BCC, STATE_MAX)},

   /* BCC (2, 1).  */
   {BCC_5F,	BCC_5B,
		BCC_MAX_LEN - 4,	0},

   /* PUSHJ (3, 0).  Next state is actually PUSHJSTUB (6, 0).  */
   {PUSHJ_0F,	PUSHJ_0B,	0,	ENCODE_RELAX (STATE_PUSHJSTUB, STATE_ZERO)},

   /* PUSHJ (3, 1).  */
   {PUSHJ_4F,	PUSHJ_4B,
		PUSHJ_MAX_LEN - 4,	0},

   /* JMP (4, 0).  */
   {JMP_0F,	JMP_0B,		0,	ENCODE_RELAX (STATE_JMP, STATE_MAX)},

   /* JMP (4, 1).  */
   {JMP_4F,	JMP_4B,
		JMP_MAX_LEN - 4,	0},

   /* GREG (5, 0), (5, 1), though the table entry isn't used.  */
   {0, 0, 0, 0}, {0, 0, 0, 0},

   /* PUSHJSTUB (6, 0).  PUSHJ (3, 0) uses the range, so we set it to infinite.  */
   {PUSHJSTUB_MAX, PUSHJSTUB_MIN,
    		0,			ENCODE_RELAX (STATE_PUSHJ, STATE_MAX)},
   /* PUSHJSTUB (6, 1) isn't used.  */
   {0, 0,	PUSHJ_MAX_LEN, 		0}
};

const pseudo_typeS md_pseudo_table[] =
 {
   /* Support " .greg sym,expr" syntax.  */
   {"greg", s_greg, 0},

   /* Support " .bspec expr" syntax.  */
   {"bspec", s_bspec, 1},

   /* Support " .espec" syntax.  */
   {"espec", s_espec, 1},

   /* Support " .local $45" syntax.  */
   {"local", mmix_s_local, 1},

   {NULL, 0, 0}
 };

const char mmix_comment_chars[] = "%!";

/* A ':' is a valid symbol character in mmixal.  It's the prefix
   delimiter, but other than that, it works like a symbol character,
   except that we strip one off at the beginning of symbols.  An '@' is a
   symbol by itself (for the current location); space around it must not
   be stripped.  */
const char mmix_symbol_chars[] = ":@";

const char line_comment_chars[] = "*#";

const char line_separator_chars[] = ";";

const char mmix_exp_chars[] = "eE";

const char mmix_flt_chars[] = "rf";


/* Fill in the offset-related part of GETA or Bcc.  */

static void
mmix_set_geta_branch_offset (opcodep, value)
     char *opcodep;
     offsetT value;
{
  if (value < 0)
    {
      value += 65536 * 4;
      opcodep[0] |= 1;
    }

  value /= 4;
  md_number_to_chars (opcodep + 2, value, 2);
}

/* Fill in the offset-related part of JMP.  */

static void
mmix_set_jmp_offset (opcodep, value)
     char *opcodep;
     offsetT value;
{
  if (value < 0)
    {
      value += 65536 * 256 * 4;
      opcodep[0] |= 1;
    }

  value /= 4;
  md_number_to_chars (opcodep + 1, value, 3);
}

/* Fill in NOP:s for the expanded part of GETA/JMP/Bcc/PUSHJ.  */

static void
mmix_fill_nops (opcodep, n)
     char *opcodep;
     int n;
{
  int i;

  for (i = 0; i < n; i++)
    md_number_to_chars (opcodep + i * 4, SWYM_INSN_BYTE << 24, 4);
}

/* See macro md_parse_name in tc-mmix.h.  */

int
mmix_current_location (fn, exp)
     void (*fn) PARAMS ((expressionS *));
     expressionS *exp;
{
  (*fn) (exp);

  return 1;
}

/* Get up to three operands, filling them into the exp array.
   General idea and code stolen from the tic80 port.  */

static int
get_operands (max_operands, s, exp)
     int max_operands;
     char *s;
     expressionS exp[];
{
  char *p = s;
  int numexp = 0;
  int nextchar = ',';

  while (nextchar == ',')
    {
      /* Skip leading whitespace */
      while (*p == ' ' || *p == '\t')
	p++;

      /* Check to see if we have any operands left to parse */
      if (*p == 0 || *p == '\n' || *p == '\r')
	{
	  break;
	}
      else if (numexp == max_operands)
	{
	  /* This seems more sane than saying "too many operands".  We'll
	     get here only if the trailing trash starts with a comma.  */
	  as_bad (_("invalid operands"));
	  mmix_discard_rest_of_line ();
	  return 0;
	}

      /* Begin operand parsing at the current scan point.  */

      input_line_pointer = p;
      expression (&exp[numexp]);

      if (exp[numexp].X_op == O_illegal)
	{
	  as_bad (_("invalid operands"));
	}
      else if (exp[numexp].X_op == O_absent)
	{
	  as_bad (_("missing operand"));
	}

      numexp++;
      p = input_line_pointer;

      /* Skip leading whitespace */
      while (*p == ' ' || *p == '\t')
	p++;
      nextchar = *p++;
    }

  /* If we allow "naked" comments, ignore the rest of the line.  */
  if (nextchar != ',')
    {
      mmix_handle_rest_of_empty_line ();
      input_line_pointer--;
    }

  /* Mark the end of the valid operands with an illegal expression.  */
  exp[numexp].X_op = O_illegal;

  return (numexp);
}

/* Get the value of a special register, or -1 if the name does not match
   one.  NAME is a null-terminated string.  */

static int
get_spec_regno (name)
     char *name;
{
  int i;

  if (name == NULL)
    return -1;

  if (*name == ':')
    name++;

  /* Well, it's a short array and we'll most often just match the first
     entry, rJ.  */
  for (i = 0; mmix_spec_regs[i].name != NULL; i++)
    if (strcmp (name, mmix_spec_regs[i].name) == 0)
      return mmix_spec_regs[i].number;

  return -1;
}

/* For GET and PUT, parse the register names "manually", so we don't use
   user labels.  */
static int
get_putget_operands (insn, operands, exp)
     struct mmix_opcode *insn;
     char *operands;
     expressionS exp[];
{
  expressionS *expp_reg;
  expressionS *expp_sreg;
  char *sregp = NULL;
  char *sregend = operands;
  char *p = operands;
  char c = *sregend;
  int regno;

  /* Skip leading whitespace */
  while (*p == ' ' || *p == '\t')
    p++;

  input_line_pointer = p;

  /* Initialize both possible operands to error state, in case we never
     get further.  */
  exp[0].X_op = O_illegal;
  exp[1].X_op = O_illegal;

  if (insn->operands == mmix_operands_get)
    {
      expp_reg = &exp[0];
      expp_sreg = &exp[1];

      expression (expp_reg);

      p = input_line_pointer;

      /* Skip whitespace */
      while (*p == ' ' || *p == '\t')
	p++;

      if (*p == ',')
	{
	  p++;

	  /* Skip whitespace */
	  while (*p == ' ' || *p == '\t')
	    p++;
	  sregp = p;
	  input_line_pointer = sregp;
	  c = get_symbol_end ();
	  sregend = input_line_pointer;
	}
    }
  else
    {
      expp_sreg = &exp[0];
      expp_reg = &exp[1];

      sregp = p;
      c = get_symbol_end ();
      sregend = p = input_line_pointer;
      *p = c;

      /* Skip whitespace */
      while (*p == ' ' || *p == '\t')
	p++;

      if (*p == ',')
	{
	  p++;

	  /* Skip whitespace */
	  while (*p == ' ' || *p == '\t')
	    p++;

	  input_line_pointer = p;
	  expression (expp_reg);
	}
      *sregend = 0;
    }

  regno = get_spec_regno (sregp);
  *sregend = c;

  /* Let the caller issue errors; we've made sure the operands are
     invalid.  */
  if (expp_reg->X_op != O_illegal
      && expp_reg->X_op != O_absent
      && regno != -1)
    {
      expp_sreg->X_op = O_register;
      expp_sreg->X_add_number = regno + 256;
    }

  return 2;
}

/* Handle MMIX-specific option.  */

int
md_parse_option (c, arg)
     int c;
     char *arg ATTRIBUTE_UNUSED;
{
  switch (c)
    {
    case 'x':
      warn_on_expansion = 0;
      allocate_undefined_gregs_in_linker = 1;
      break;

    case OPTION_RELAX:
      linkrelax = 1;
      break;

    case OPTION_NOEXPAND:
      expand_op = 0;
      break;

    case OPTION_NOMERGEGREG:
      merge_gregs = 0;
      break;

    case OPTION_NOSYMS:
      predefined_syms = 0;
      equated_spec_regs = 0;
      break;

    case OPTION_GNU_SYNTAX:
      mmix_gnu_syntax = 1;
      label_without_colon_this_line = 0;
      break;

    case OPTION_GLOBALIZE_SYMBOLS:
      mmix_globalize_symbols = 1;
      break;

    case OPTION_FIXED_SPEC_REGS:
      equated_spec_regs = 0;
      break;

    case OPTION_LINKER_ALLOCATED_GREGS:
      allocate_undefined_gregs_in_linker = 1;
      break;

    case OPTION_NOPUSHJSTUBS:
      pushj_stubs = 0;
      break;

    default:
      return 0;
    }

  return 1;
}

/* Display MMIX-specific help text.  */

void
md_show_usage (stream)
     FILE * stream;
{
  fprintf (stream, _(" MMIX-specific command line options:\n"));
  fprintf (stream, _("\
  -fixed-special-register-names\n\
                          Allow only the original special register names.\n"));
  fprintf (stream, _("\
  -globalize-symbols      Make all symbols global.\n"));
  fprintf (stream, _("\
  -gnu-syntax             Turn off mmixal syntax compatibility.\n"));
  fprintf (stream, _("\
  -relax                  Create linker relaxable code.\n"));
  fprintf (stream, _("\
  -no-predefined-syms     Do not provide mmixal built-in constants.\n\
                          Implies -fixed-special-register-names.\n"));
  fprintf (stream, _("\
  -no-expand              Do not expand GETA, branches, PUSHJ or JUMP\n\
                          into multiple instructions.\n"));
  fprintf (stream, _("\
  -no-merge-gregs         Do not merge GREG definitions with nearby values.\n"));
  fprintf (stream, _("\
  -linker-allocated-gregs If there's no suitable GREG definition for the\
                          operands of an instruction, let the linker resolve.\n"));
  fprintf (stream, _("\
  -x                      Do not warn when an operand to GETA, a branch,\n\
                          PUSHJ or JUMP is not known to be within range.\n\
                          The linker will catch any errors.  Implies\n\
                          -linker-allocated-gregs."));
}

/* Step to end of line, but don't step over the end of the line.  */

static void
mmix_discard_rest_of_line ()
{
  while (*input_line_pointer
	 && (! is_end_of_line[(unsigned char) *input_line_pointer]
	     || TC_EOL_IN_INSN (input_line_pointer)))
    input_line_pointer++;
}

/* Act as demand_empty_rest_of_line if we're in strict GNU syntax mode,
   otherwise just ignore the rest of the line (and skip the end-of-line
   delimiter).  */

static void
mmix_handle_rest_of_empty_line ()
{
  if (mmix_gnu_syntax)
    demand_empty_rest_of_line ();
  else
    {
      mmix_discard_rest_of_line ();
      input_line_pointer++;
    }
}

/* Initialize GAS MMIX specifics.  */

void
mmix_md_begin ()
{
  int i;
  const struct mmix_opcode *opcode;

  /* We assume nobody will use this, so don't allocate any room.  */
  obstack_begin (&mmix_sym_obstack, 0);

  /* This will break the day the "lex" thingy changes.  For now, it's the
     only way to make ':' part of a name, and a name beginner.  */
  lex_type[':'] = (LEX_NAME | LEX_BEGIN_NAME);

  mmix_opcode_hash = hash_new ();

  real_reg_section
    = bfd_make_section_old_way (stdoutput, MMIX_REG_SECTION_NAME);

  for (opcode = mmix_opcodes; opcode->name; opcode++)
    hash_insert (mmix_opcode_hash, opcode->name, (char *) opcode);

  /* We always insert the ordinary registers 0..255 as registers.  */
  for (i = 0; i < 256; i++)
    {
      char buf[5];

      /* Alternatively, we could diddle with '$' and the following number,
	 but keeping the registers as symbols helps keep parsing simple.  */
      sprintf (buf, "$%d", i);
      symbol_table_insert (symbol_new (buf, reg_section, i,
				       &zero_address_frag));
    }

  /* Insert mmixal built-in names if allowed.  */
  if (predefined_syms)
    {
      for (i = 0; mmix_spec_regs[i].name != NULL; i++)
	symbol_table_insert (symbol_new (mmix_spec_regs[i].name,
					 reg_section,
					 mmix_spec_regs[i].number + 256,
					 &zero_address_frag));

      /* FIXME: Perhaps these should be recognized as specials; as field
	 names for those instructions.  */
      symbol_table_insert (symbol_new ("ROUND_CURRENT", reg_section, 512,
				       &zero_address_frag));
      symbol_table_insert (symbol_new ("ROUND_OFF", reg_section, 512 + 1,
				       &zero_address_frag));
      symbol_table_insert (symbol_new ("ROUND_UP", reg_section, 512 + 2,
				       &zero_address_frag));
      symbol_table_insert (symbol_new ("ROUND_DOWN", reg_section, 512 + 3,
				       &zero_address_frag));
      symbol_table_insert (symbol_new ("ROUND_NEAR", reg_section, 512 + 4,
				       &zero_address_frag));
    }
}

/* Assemble one insn in STR.  */

void
md_assemble (str)
     char *str;
{
  char *operands = str;
  char modified_char = 0;
  struct mmix_opcode *instruction;
  fragS *opc_fragP = NULL;
  int max_operands = 3;

  /* Note that the struct frag member fr_literal in frags.h is char[], so
     I have to make this a plain char *.  */
  /* unsigned */ char *opcodep = NULL;

  expressionS exp[4];
  int n_operands = 0;

  /* Move to end of opcode.  */
  for (operands = str;
       is_part_of_name (*operands);
       ++operands)
    ;

  if (ISSPACE (*operands))
    {
      modified_char = *operands;
      *operands++ = '\0';
    }

  instruction = (struct mmix_opcode *) hash_find (mmix_opcode_hash, str);
  if (instruction == NULL)
    {
      as_bad (_("unknown opcode: `%s'"), str);

      /* Avoid "unhandled label" errors.  */
      pending_label = NULL;
      return;
    }

  /* Put back the character after the opcode.  */
  if (modified_char != 0)
    operands[-1] = modified_char;

  input_line_pointer = operands;

  /* Is this a mmixal pseudodirective?  */
  if (instruction->type == mmix_type_pseudo)
    {
      /* For mmixal compatibility, a label for an instruction (and
	 emitting pseudo) refers to the _aligned_ address.  We emit the
	 label here for the pseudos that don't handle it themselves.  When
	 having an fb-label, emit it here, and increment the counter after
	 the pseudo.  */
      switch (instruction->operands)
	{
	case mmix_operands_loc:
	case mmix_operands_byte:
	case mmix_operands_prefix:
	case mmix_operands_local:
	case mmix_operands_bspec:
	case mmix_operands_espec:
	  if (current_fb_label >= 0)
	    colon (fb_label_name (current_fb_label, 1));
	  else if (pending_label != NULL)
	    {
	      colon (pending_label);
	      pending_label = NULL;
	    }
	  break;

	default:
	  break;
	}

      /* Some of the pseudos emit contents, others don't.  Set a
	 contents-emitted flag when we emit something into .text   */
      switch (instruction->operands)
	{
	case mmix_operands_loc:
	  /* LOC */
	  s_loc (0);
	  break;

	case mmix_operands_byte:
	  /* BYTE */
	  mmix_byte ();
	  break;

	case mmix_operands_wyde:
	  /* WYDE */
	  mmix_cons (2);
	  break;

	case mmix_operands_tetra:
	  /* TETRA */
	  mmix_cons (4);
	  break;

	case mmix_operands_octa:
	  /* OCTA */
	  mmix_cons (8);
	  break;

	case mmix_operands_prefix:
	  /* PREFIX */
	  s_prefix (0);
	  break;

	case mmix_operands_local:
	  /* LOCAL */
	  mmix_s_local (0);
	  break;

	case mmix_operands_bspec:
	  /* BSPEC */
	  s_bspec (0);
	  break;

	case mmix_operands_espec:
	  /* ESPEC */
	  s_espec (0);
	  break;

	default:
	  BAD_CASE (instruction->operands);
	}

      /* These are all working like the pseudo functions in read.c:s_...,
	 in that they step over the end-of-line marker at the end of the
	 line.  We don't want that here.  */
      input_line_pointer--;

      /* Step up the fb-label counter if there was a definition on this
	 line.  */
      if (current_fb_label >= 0)
	{
	  fb_label_instance_inc (current_fb_label);
	  current_fb_label = -1;
	}

      /* Reset any don't-align-next-datum request, unless this was a LOC
         directive.  */
      if (instruction->operands != mmix_operands_loc)
	want_unaligned = 0;

      return;
    }

  /* Not a pseudo; we *will* emit contents.  */
  if (now_seg == data_section)
    {
      if (lowest_data_loc != (bfd_vma) -1 && (lowest_data_loc & 3) != 0)
	{
	  if (data_has_contents)
	    as_bad (_("specified location wasn't TETRA-aligned"));
	  else if (want_unaligned)
	    as_bad (_("unaligned data at an absolute location is not supported"));

	  lowest_data_loc &= ~(bfd_vma) 3;
	  lowest_data_loc += 4;
	}

      data_has_contents = 1;
    }
  else if (now_seg == text_section)
    {
      if (lowest_text_loc != (bfd_vma) -1 && (lowest_text_loc & 3) != 0)
	{
	  if (text_has_contents)
	    as_bad (_("specified location wasn't TETRA-aligned"));
	  else if (want_unaligned)
	    as_bad (_("unaligned data at an absolute location is not supported"));

	  lowest_text_loc &= ~(bfd_vma) 3;
	  lowest_text_loc += 4;
	}

      text_has_contents = 1;
    }

  /* After a sequence of BYTEs or WYDEs, we need to get to instruction
     alignment.  For other pseudos, a ".p2align 2" is supposed to be
     inserted by the user.  */
  if (last_alignment < 2 && ! want_unaligned)
    {
      frag_align (2, 0, 0);
      record_alignment (now_seg, 2);
      last_alignment = 2;
    }
  else
    /* Reset any don't-align-next-datum request.  */
    want_unaligned = 0;

  /* For mmixal compatibility, a label for an instruction (and emitting
     pseudo) refers to the _aligned_ address.  So we have to emit the
     label here.  */
  if (pending_label != NULL)
    {
      colon (pending_label);
      pending_label = NULL;
    }

  /* We assume that mmix_opcodes keeps having unique mnemonics for each
     opcode, so we don't have to iterate over more than one opcode; if the
     syntax does not match, then there's a syntax error.  */

  /* Operands have little or no context and are all comma-separated; it is
     easier to parse each expression first.   */
  switch (instruction->operands)
    {
    case mmix_operands_reg_yz:
    case mmix_operands_pop:
    case mmix_operands_regaddr:
    case mmix_operands_pushj:
    case mmix_operands_get:
    case mmix_operands_put:
    case mmix_operands_set:
    case mmix_operands_save:
    case mmix_operands_unsave:
      max_operands = 2;
      break;

    case mmix_operands_sync:
    case mmix_operands_jmp:
    case mmix_operands_resume:
      max_operands = 1;
      break;

      /* The original 3 is fine for the rest.  */
    default:
      break;
    }

  /* If this is GET or PUT, and we don't do allow those names to be
     equated, we need to parse the names ourselves, so we don't pick up a
     user label instead of the special register.  */
  if (! equated_spec_regs
      && (instruction->operands == mmix_operands_get
	  || instruction->operands == mmix_operands_put))
    n_operands = get_putget_operands (instruction, operands, exp);
  else
    n_operands = get_operands (max_operands, operands, exp);

  /* If there's a fb-label on the current line, set that label.  This must
     be done *after* evaluating expressions of operands, since neither a
     "1B" nor a "1F" refers to "1H" on the same line.  */
  if (current_fb_label >= 0)
    {
      fb_label_instance_inc (current_fb_label);
      colon (fb_label_name (current_fb_label, 0));
      current_fb_label = -1;
    }

  /* We also assume that the length of the instruction is at least 4, the
     size of an unexpanded instruction.  We need a self-contained frag
     since we want the relocation to point to the instruction, not the
     variant part.  */

  opcodep = frag_more (4);
  mmix_opcode_frag = opc_fragP = frag_now;
  frag_now->fr_opcode = opcodep;

  /* Mark start of insn for DWARF2 debug features.  */
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
    dwarf2_emit_insn (4);

  md_number_to_chars (opcodep, instruction->match, 4);

  switch (instruction->operands)
    {
    case mmix_operands_jmp:
      if (n_operands == 0 && ! mmix_gnu_syntax)
	/* Zeros are in place - nothing needs to be done when we have no
	   operands.  */
	break;

      /* Add a frag for a JMP relaxation; we need room for max four
	 extra instructions.  We don't do any work around here to check if
	 we can determine the offset right away.  */
      if (n_operands != 1 || exp[0].X_op == O_register)
	{
	  as_bad (_("invalid operand to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (expand_op)
	frag_var (rs_machine_dependent, 4 * 4, 0,
		  ENCODE_RELAX (STATE_JMP, STATE_UNDF),
		  exp[0].X_add_symbol,
		  exp[0].X_add_number,
		  opcodep);
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		     exp + 0, 1, BFD_RELOC_MMIX_ADDR27);
      break;

    case mmix_operands_pushj:
      /* We take care of PUSHJ in full here.  */
      if (n_operands != 2
	  || ((exp[0].X_op == O_constant || exp[0].X_op == O_register)
	      && (exp[0].X_add_number > 255 || exp[0].X_add_number < 0)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[0].X_op == O_register || exp[0].X_op == O_constant)
	opcodep[1] = exp[0].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
		     1, exp + 0, 0, BFD_RELOC_MMIX_REG_OR_BYTE);

      if (expand_op)
	frag_var (rs_machine_dependent, PUSHJ_MAX_LEN - 4, 0,
		  ENCODE_RELAX (STATE_PUSHJ, STATE_UNDF),
		  exp[1].X_add_symbol,
		  exp[1].X_add_number,
		  opcodep);
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		     exp + 1, 1, BFD_RELOC_MMIX_ADDR19);
      break;

    case mmix_operands_regaddr:
      /* GETA/branch: Add a frag for relaxation.  We don't do any work
	 around here to check if we can determine the offset right away.  */
      if (n_operands != 2 || exp[1].X_op == O_register)
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (! expand_op)
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		     exp + 1, 1, BFD_RELOC_MMIX_ADDR19);
      else if (instruction->type == mmix_type_condbranch)
	frag_var (rs_machine_dependent, BCC_MAX_LEN - 4, 0,
		  ENCODE_RELAX (STATE_BCC, STATE_UNDF),
		  exp[1].X_add_symbol,
		  exp[1].X_add_number,
		  opcodep);
      else
	frag_var (rs_machine_dependent, GETA_MAX_LEN - 4, 0,
		  ENCODE_RELAX (STATE_GETA, STATE_UNDF),
		  exp[1].X_add_symbol,
		  exp[1].X_add_number,
		  opcodep);
      break;

    default:
      break;
    }

  switch (instruction->operands)
    {
    case mmix_operands_regs:
      /* We check the number of operands here, since we're in a
	 FALLTHROUGH sequence in the next switch.  */
      if (n_operands != 3 || exp[2].X_op == O_constant)
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}
      /* FALLTHROUGH.  */
    case mmix_operands_regs_z:
      if (n_operands != 3)
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}
      /* FALLTHROUGH.  */
    case mmix_operands_reg_yz:
    case mmix_operands_roundregs_z:
    case mmix_operands_roundregs:
    case mmix_operands_regs_z_opt:
    case mmix_operands_neg:
    case mmix_operands_regaddr:
    case mmix_operands_get:
    case mmix_operands_set:
    case mmix_operands_save:
      if (n_operands < 1
	  || (exp[0].X_op == O_register && exp[0].X_add_number > 255))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[0].X_op == O_register)
	opcodep[1] = exp[0].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
		     1, exp + 0, 0, BFD_RELOC_MMIX_REG);
      break;

    default:
      ;
    }

  /* A corresponding once-over for those who take an 8-bit constant as
     their first operand.  */
  switch (instruction->operands)
    {
    case mmix_operands_pushgo:
      /* PUSHGO: X is a constant, but can be expressed as a register.
	 We handle X here and use the common machinery of T,X,3,$ for
	 the rest of the operands.  */
      if (n_operands < 2
	  || ((exp[0].X_op == O_constant || exp[0].X_op == O_register)
	      && (exp[0].X_add_number > 255 || exp[0].X_add_number < 0)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}
      else if (exp[0].X_op == O_constant || exp[0].X_op == O_register)
	opcodep[1] = exp[0].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
		     1, exp + 0, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
      break;

    case mmix_operands_pop:
      if ((n_operands == 0 || n_operands == 1) && ! mmix_gnu_syntax)
	break;
      /* FALLTHROUGH.  */
    case mmix_operands_x_regs_z:
      if (n_operands < 1
	  || (exp[0].X_op == O_constant
	      && (exp[0].X_add_number > 255
		  || exp[0].X_add_number < 0)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[0].X_op == O_constant)
	opcodep[1] = exp[0].X_add_number;
      else
	/* FIXME: This doesn't bring us unsignedness checking.  */
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
		     1, exp + 0, 0, BFD_RELOC_8);
    default:
      ;
    }

  /* Handle the rest.  */
  switch (instruction->operands)
    {
    case mmix_operands_set:
      /* SET: Either two registers, "$X,$Y", with Z field as zero, or
	 "$X,YZ", meaning change the opcode to SETL.  */
      if (n_operands != 2
	  || (exp[1].X_op == O_constant
	      && (exp[1].X_add_number > 0xffff || exp[1].X_add_number < 0)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[1].X_op == O_constant)
	{
	  /* There's an ambiguity with "SET $0,Y" when Y isn't defined
	     yet.  To keep things simple, we assume that Y is then a
	     register, and only change the opcode if Y is defined at this
	     point.

	     There's no compatibility problem with mmixal, since it emits
	     errors if the field is not defined at this point.  */
	  md_number_to_chars (opcodep, SETL_INSN_BYTE, 1);

	  opcodep[2] = (exp[1].X_add_number >> 8) & 255;
	  opcodep[3] = exp[1].X_add_number & 255;
	  break;
	}
      /* FALLTHROUGH.  */
    case mmix_operands_x_regs_z:
      /* SYNCD: "X,$Y,$Z|Z".  */
      /* FALLTHROUGH.  */
    case mmix_operands_regs:
      /* Three registers, $X,$Y,$Z.  */
      /* FALLTHROUGH.  */
    case mmix_operands_regs_z:
      /* Operands "$X,$Y,$Z|Z", number of arguments checked above.  */
      /* FALLTHROUGH.  */
    case mmix_operands_pushgo:
      /* Operands "$X|X,$Y,$Z|Z", optional Z.  */
      /* FALLTHROUGH.  */
    case mmix_operands_regs_z_opt:
      /* Operands "$X,$Y,$Z|Z", with $Z|Z being optional, default 0.  Any
	 operands not completely decided yet are postponed to later in
	 assembly (but not until link-time yet).  */

      if ((n_operands != 2 && n_operands != 3)
	  || (exp[1].X_op == O_register && exp[1].X_add_number > 255)
	  || (n_operands == 3
	      && ((exp[2].X_op == O_register
		   && exp[2].X_add_number > 255
		   && mmix_gnu_syntax)
		  || (exp[2].X_op == O_constant
		      && (exp[2].X_add_number > 255
			  || exp[2].X_add_number < 0)))))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (n_operands == 2)
	{
	  symbolS *sym;

	  /* The last operand is immediate whenever we see just two
	     operands.  */
	  opcodep[0] |= IMM_OFFSET_BIT;

	  /* Now, we could either have an implied "0" as the Z operand, or
	     it could be the constant of a "base address plus offset".  It
	     depends on whether it is allowed; only memory operations, as
	     signified by instruction->type and "T" and "X" operand types,
	     and it depends on whether we find a register in the second
	     operand, exp[1].  */
	  if (exp[1].X_op == O_register && exp[1].X_add_number <= 255)
	    {
	      /* A zero then; all done.  */
	      opcodep[2] = exp[1].X_add_number;
	      break;
	    }

	  /* Not known as a register.  Is base address plus offset
	     allowed, or can we assume that it is a register anyway?  */
	  if ((instruction->operands != mmix_operands_regs_z_opt
	       && instruction->operands != mmix_operands_x_regs_z
	       && instruction->operands != mmix_operands_pushgo)
	      || (instruction->type != mmix_type_memaccess_octa
		  && instruction->type != mmix_type_memaccess_tetra
		  && instruction->type != mmix_type_memaccess_wyde
		  && instruction->type != mmix_type_memaccess_byte
		  && instruction->type != mmix_type_memaccess_block
		  && instruction->type != mmix_type_jsr
		  && instruction->type != mmix_type_branch))
	    {
	      fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
			   1, exp + 1, 0, BFD_RELOC_MMIX_REG);
	      break;
	    }

	  /* To avoid getting a NULL add_symbol for constants and then
	     catching a SEGV in write_relocs since it doesn't handle
	     constants well for relocs other than PC-relative, we need to
	     pass expressions as symbols and use fix_new, not fix_new_exp.  */
	  sym = make_expr_symbol (exp + 1);

	  /* Now we know it can be a "base address plus offset".  Add
	     proper fixup types so we can handle this later, when we've
	     parsed everything.  */
	  fix_new (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
		   8, sym, 0, 0, BFD_RELOC_MMIX_BASE_PLUS_OFFSET);
	  break;
	}

      if (exp[1].X_op == O_register)
	opcodep[2] = exp[1].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
		     1, exp + 1, 0, BFD_RELOC_MMIX_REG);

      /* In mmixal compatibility mode, we allow special registers as
	 constants for the Z operand.  They have 256 added to their
	 register numbers, so the right thing will happen if we just treat
	 those as constants.  */
      if (exp[2].X_op == O_register && exp[2].X_add_number <= 255)
	opcodep[3] = exp[2].X_add_number;
      else if (exp[2].X_op == O_constant
	       || (exp[2].X_op == O_register && exp[2].X_add_number > 255))
	{
	  opcodep[3] = exp[2].X_add_number;
	  opcodep[0] |= IMM_OFFSET_BIT;
	}
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
		     1, exp + 2, 0,
		     (instruction->operands == mmix_operands_set
		      || instruction->operands == mmix_operands_regs)
		     ? BFD_RELOC_MMIX_REG : BFD_RELOC_MMIX_REG_OR_BYTE);
      break;

    case mmix_operands_pop:
      /* POP, one eight and one 16-bit operand.  */
      if (n_operands == 0 && ! mmix_gnu_syntax)
	break;
      if (n_operands == 1 && ! mmix_gnu_syntax)
	goto a_single_24_bit_number_operand;
      /* FALLTHROUGH.  */
    case mmix_operands_reg_yz:
      /* A register and a 16-bit unsigned number.  */
      if (n_operands != 2
	  || exp[1].X_op == O_register
	  || (exp[1].X_op == O_constant
	      && (exp[1].X_add_number > 0xffff || exp[1].X_add_number < 0)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[1].X_op == O_constant)
	{
	  opcodep[2] = (exp[1].X_add_number >> 8) & 255;
	  opcodep[3] = exp[1].X_add_number & 255;
	}
      else
	/* FIXME: This doesn't bring us unsignedness checking.  */
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
		     2, exp + 1, 0, BFD_RELOC_16);
      break;

    case mmix_operands_jmp:
      /* A JMP.  Everything is already done.  */
      break;

    case mmix_operands_roundregs:
      /* Two registers with optional rounding mode or constant in between.  */
      if ((n_operands == 3 && exp[2].X_op == O_constant)
	  || (n_operands == 2 && exp[1].X_op == O_constant))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}
      /* FALLTHROUGH.  */
    case mmix_operands_roundregs_z:
      /* Like FLOT, "$X,ROUND_MODE,$Z|Z", but the rounding mode is
	 optional and can be the corresponding constant.  */
      {
	/* Which exp index holds the second operand (not the rounding
	   mode).  */
	int op2no = n_operands - 1;

	if ((n_operands != 2 && n_operands != 3)
	    || ((exp[op2no].X_op == O_register
		 && exp[op2no].X_add_number > 255)
		|| (exp[op2no].X_op == O_constant
		    && (exp[op2no].X_add_number > 255
			|| exp[op2no].X_add_number < 0)))
	    || (n_operands == 3
		/* We don't allow for the rounding mode to be deferred; it
		   must be determined in the "first pass".  It cannot be a
		   symbol equated to a rounding mode, but defined after
		   the first use.  */
		&& ((exp[1].X_op == O_register
		     && exp[1].X_add_number < 512)
		    || (exp[1].X_op == O_constant
			&& exp[1].X_add_number < 0
			&& exp[1].X_add_number > 4)
		    || (exp[1].X_op != O_register
			&& exp[1].X_op != O_constant))))
	  {
	    as_bad (_("invalid operands to opcode %s: `%s'"),
		    instruction->name, operands);
	    return;
	  }

	/* Add rounding mode if present.  */
	if (n_operands == 3)
	  opcodep[2] = exp[1].X_add_number & 255;

	if (exp[op2no].X_op == O_register)
	  opcodep[3] = exp[op2no].X_add_number;
	else if (exp[op2no].X_op == O_constant)
	  {
	    opcodep[3] = exp[op2no].X_add_number;
	    opcodep[0] |= IMM_OFFSET_BIT;
	  }
	else
	  fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
		       1, exp + op2no, 0,
		       instruction->operands == mmix_operands_roundregs
		       ? BFD_RELOC_MMIX_REG
		       : BFD_RELOC_MMIX_REG_OR_BYTE);
	break;
      }

    case mmix_operands_sync:
    a_single_24_bit_number_operand:
      if (n_operands != 1
	  || exp[0].X_op == O_register
	  || (exp[0].X_op == O_constant
	      && (exp[0].X_add_number > 0xffffff || exp[0].X_add_number < 0)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[0].X_op == O_constant)
	{
	  opcodep[1] = (exp[0].X_add_number >> 16) & 255;
	  opcodep[2] = (exp[0].X_add_number >> 8) & 255;
	  opcodep[3] = exp[0].X_add_number & 255;
	}
      else
	/* FIXME: This doesn't bring us unsignedness checking.  */
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
		     3, exp + 0, 0, BFD_RELOC_24);
      break;

    case mmix_operands_neg:
      /* Operands "$X,Y,$Z|Z"; NEG or NEGU.  Y is optional, 0 is default.  */

      if ((n_operands != 3 && n_operands != 2)
	  || (n_operands == 3 && exp[1].X_op == O_register)
	  || ((exp[1].X_op == O_constant || exp[1].X_op == O_register)
	      && (exp[1].X_add_number > 255 || exp[1].X_add_number < 0))
	  || (n_operands == 3
	      && ((exp[2].X_op == O_register && exp[2].X_add_number > 255)
		  || (exp[2].X_op == O_constant
		      && (exp[2].X_add_number > 255
			  || exp[2].X_add_number < 0)))))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (n_operands == 2)
	{
	  if (exp[1].X_op == O_register)
	    opcodep[3] = exp[1].X_add_number;
	  else if (exp[1].X_op == O_constant)
	    {
	      opcodep[3] = exp[1].X_add_number;
	      opcodep[0] |= IMM_OFFSET_BIT;
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
			 1, exp + 1, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
	  break;
	}

      if (exp[1].X_op == O_constant)
	opcodep[2] = exp[1].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
		     1, exp + 1, 0, BFD_RELOC_8);

      if (exp[2].X_op == O_register)
	opcodep[3] = exp[2].X_add_number;
      else if (exp[2].X_op == O_constant)
	{
	  opcodep[3] = exp[2].X_add_number;
	  opcodep[0] |= IMM_OFFSET_BIT;
	}
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
		     1, exp + 2, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
      break;

    case mmix_operands_regaddr:
      /* A GETA/branch-type.  */
      break;

    case mmix_operands_get:
      /* "$X,spec_reg"; GET.
	 Like with rounding modes, we demand that the special register or
	 symbol is already defined when we get here at the point of use.  */
      if (n_operands != 2
	  || (exp[1].X_op == O_register
	      && (exp[1].X_add_number < 256 || exp[1].X_add_number >= 512))
	  || (exp[1].X_op == O_constant
	      && (exp[1].X_add_number < 0 || exp[1].X_add_number > 256))
	  || (exp[1].X_op != O_constant && exp[1].X_op != O_register))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      opcodep[3] = exp[1].X_add_number - 256;
      break;

    case mmix_operands_put:
      /* "spec_reg,$Z|Z"; PUT.  */
      if (n_operands != 2
	  || (exp[0].X_op == O_register
	      && (exp[0].X_add_number < 256 || exp[0].X_add_number >= 512))
	  || (exp[0].X_op == O_constant
	      && (exp[0].X_add_number < 0 || exp[0].X_add_number > 256))
	  || (exp[0].X_op != O_constant && exp[0].X_op != O_register))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      opcodep[1] = exp[0].X_add_number - 256;

      /* Note that the Y field is zero.  */

      if (exp[1].X_op == O_register)
	opcodep[3] = exp[1].X_add_number;
      else if (exp[1].X_op == O_constant)
	{
	  opcodep[3] = exp[1].X_add_number;
	  opcodep[0] |= IMM_OFFSET_BIT;
	}
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
		     1, exp + 1, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
      break;

    case mmix_operands_save:
      /* "$X,0"; SAVE.  */
      if (n_operands != 2
	  || exp[1].X_op != O_constant
	  || exp[1].X_add_number != 0)
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}
      break;

    case mmix_operands_unsave:
      if (n_operands < 2 && ! mmix_gnu_syntax)
	{
	  if (n_operands == 1)
	    {
	      if (exp[0].X_op == O_register)
		opcodep[3] = exp[0].X_add_number;
	      else
		fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
			     1, exp, 0, BFD_RELOC_MMIX_REG);
	    }
	  break;
	}

      /* "0,$Z"; UNSAVE.  */
      if (n_operands != 2
	  || exp[0].X_op != O_constant
	  || exp[0].X_add_number != 0
	  || exp[1].X_op == O_constant
	  || (exp[1].X_op == O_register
	      && exp[1].X_add_number > 255))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[1].X_op == O_register)
	opcodep[3] = exp[1].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
		     1, exp + 1, 0, BFD_RELOC_MMIX_REG);
      break;

    case mmix_operands_xyz_opt:
      /* SWYM, TRIP, TRAP: zero, one, two or three operands.  */
      if (n_operands == 0 && ! mmix_gnu_syntax)
	/* Zeros are in place - nothing needs to be done for zero
	   operands.  We don't allow this in GNU syntax mode, because it
	   was believed that the risk of missing to supply an operand is
	   higher than the benefit of not having to specify a zero.  */
	;
      else if (n_operands == 1 && exp[0].X_op != O_register)
	{
	  if (exp[0].X_op == O_constant)
	    {
	      if (exp[0].X_add_number > 255*255*255
		  || exp[0].X_add_number < 0)
		{
		  as_bad (_("invalid operands to opcode %s: `%s'"),
			  instruction->name, operands);
		  return;
		}
	      else
		{
		  opcodep[1] = (exp[0].X_add_number >> 16) & 255;
		  opcodep[2] = (exp[0].X_add_number >> 8) & 255;
		  opcodep[3] = exp[0].X_add_number & 255;
		}
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
			 3, exp, 0, BFD_RELOC_24);
	}
      else if (n_operands == 2
	       && exp[0].X_op != O_register
	       && exp[1].X_op != O_register)
	{
	  /* Two operands.  */

	  if (exp[0].X_op == O_constant)
	    {
	      if (exp[0].X_add_number > 255
		  || exp[0].X_add_number < 0)
		{
		  as_bad (_("invalid operands to opcode %s: `%s'"),
			  instruction->name, operands);
		  return;
		}
	      else
		opcodep[1] = exp[0].X_add_number & 255;
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
			 1, exp, 0, BFD_RELOC_8);

	  if (exp[1].X_op == O_constant)
	    {
	      if (exp[1].X_add_number > 255*255
		  || exp[1].X_add_number < 0)
		{
		  as_bad (_("invalid operands to opcode %s: `%s'"),
			  instruction->name, operands);
		  return;
		}
	      else
		{
		  opcodep[2] = (exp[1].X_add_number >> 8) & 255;
		  opcodep[3] = exp[1].X_add_number & 255;
		}
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
			 2, exp + 1, 0, BFD_RELOC_16);
	}
      else if (n_operands == 3
	       && exp[0].X_op != O_register
	       && exp[1].X_op != O_register
	       && exp[2].X_op != O_register)
	{
	  /* Three operands.  */

	  if (exp[0].X_op == O_constant)
	    {
	      if (exp[0].X_add_number > 255
		  || exp[0].X_add_number < 0)
		{
		  as_bad (_("invalid operands to opcode %s: `%s'"),
			  instruction->name, operands);
		  return;
		}
	      else
		opcodep[1] = exp[0].X_add_number & 255;
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
			 1, exp, 0, BFD_RELOC_8);

	  if (exp[1].X_op == O_constant)
	    {
	      if (exp[1].X_add_number > 255
		  || exp[1].X_add_number < 0)
		{
		  as_bad (_("invalid operands to opcode %s: `%s'"),
			  instruction->name, operands);
		  return;
		}
	      else
		opcodep[2] = exp[1].X_add_number & 255;
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
			 1, exp + 1, 0, BFD_RELOC_8);

	  if (exp[2].X_op == O_constant)
	    {
	      if (exp[2].X_add_number > 255
		  || exp[2].X_add_number < 0)
		{
		  as_bad (_("invalid operands to opcode %s: `%s'"),
			  instruction->name, operands);
		  return;
		}
	      else
		opcodep[3] = exp[2].X_add_number & 255;
	    }
	  else
	    fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
			 1, exp + 2, 0, BFD_RELOC_8);
	}
      else if (n_operands <= 3
	       && (strcmp (instruction->name, "trip") == 0
		   || strcmp (instruction->name, "trap") == 0))
	{
	  /* The meaning of operands to TRIP and TRAP are not defined, so
	     we add combinations not handled above here as we find them.  */
	  if (n_operands == 3)
	    {
	      /* Don't require non-register operands.  Always generate
		 fixups, so we don't have to copy lots of code and create
		 maintenance problems.  TRIP is supposed to be a rare
		 instruction, so the overhead should not matter.  We
		 aren't allowed to fix_new_exp for an expression which is
		 an  O_register at this point, however.  */
	      if (exp[0].X_op == O_register)
		opcodep[1] = exp[0].X_add_number;
	      else
		fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 1,
			     1, exp, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
	      if (exp[1].X_op == O_register)
		opcodep[2] = exp[1].X_add_number;
	      else
		fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
			     1, exp + 1, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
	      if (exp[2].X_op == O_register)
		opcodep[3] = exp[2].X_add_number;
	      else
		fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
			     1, exp + 2, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
	    }
	  else if (n_operands == 2)
	    {
	      if (exp[0].X_op == O_register)
		opcodep[2] = exp[0].X_add_number;
	      else
		fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 2,
			     1, exp, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
	      if (exp[1].X_op == O_register)
		opcodep[3] = exp[1].X_add_number;
	      else
		fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
			     1, exp + 1, 0, BFD_RELOC_MMIX_REG_OR_BYTE);
	    }
	  else
	    {
	      as_bad (_("unsupported operands to %s: `%s'"),
		      instruction->name, operands);
	      return;
	    }
	}
      else
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}
      break;

    case mmix_operands_resume:
      if (n_operands == 0 && ! mmix_gnu_syntax)
	break;

      if (n_operands != 1
	  || exp[0].X_op == O_register
	  || (exp[0].X_op == O_constant
	      && (exp[0].X_add_number < 0
		  || exp[0].X_add_number > 255)))
	{
	  as_bad (_("invalid operands to opcode %s: `%s'"),
		  instruction->name, operands);
	  return;
	}

      if (exp[0].X_op == O_constant)
	opcodep[3] = exp[0].X_add_number;
      else
	fix_new_exp (opc_fragP, opcodep - opc_fragP->fr_literal + 3,
		     1, exp + 0, 0, BFD_RELOC_8);
      break;

    case mmix_operands_pushj:
      /* All is done for PUSHJ already.  */
      break;

    default:
      BAD_CASE (instruction->operands);
    }
}

/* For the benefit of insns that start with a digit, we assemble by way of
   tc_unrecognized_line too, through this function.  */

int
mmix_assemble_return_nonzero (str)
     char *str;
{
  int last_error_count = had_errors ();
  char *s2 = str;
  char c;

  /* Normal instruction handling downcases, so we must too.  */
  while (ISALNUM (*s2))
    {
      if (ISUPPER ((unsigned char) *s2))
	*s2 = TOLOWER (*s2);
      s2++;
    }

  /* Cut the line for sake of the assembly.  */
  for (s2 = str; *s2 && *s2 != '\n'; s2++)
    ;

  c = *s2;
  *s2 = 0;
  md_assemble (str);
  *s2 = c;

  return had_errors () == last_error_count;
}

/* The PREFIX pseudo.  */

static void
s_prefix (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *p;
  int c;

  SKIP_WHITESPACE ();

  p = input_line_pointer;

  c = get_symbol_end ();

  /* Reseting prefix?  */
  if (*p == ':' && p[1] == 0)
    mmix_current_prefix = NULL;
  else
    {
      /* Put this prefix on the mmix symbols obstack.  We could malloc and
	 free it separately, but then we'd have to worry about that.
	 People using up memory on prefixes have other problems.  */
      obstack_grow (&mmix_sym_obstack, p, strlen (p) + 1);
      p = obstack_finish (&mmix_sym_obstack);

      /* Accumulate prefixes, and strip a leading ':'.  */
      if (mmix_current_prefix != NULL || *p == ':')
	p = mmix_prefix_name (p);

      mmix_current_prefix = p;
    }

  *input_line_pointer = c;

  mmix_handle_rest_of_empty_line ();
}

/* We implement prefixes by using the tc_canonicalize_symbol_name hook,
   and store each prefixed name on a (separate) obstack.  This means that
   the name is on the "notes" obstack in non-prefixed form and on the
   mmix_sym_obstack in prefixed form, but currently it is not worth
   rewriting the whole GAS symbol handling to improve "hooking" to avoid
   that.  (It might be worth a rewrite for other reasons, though).  */

char *
mmix_prefix_name (shortname)
     char *shortname;
{
  if (*shortname == ':')
    return shortname + 1;

  if (mmix_current_prefix == NULL)
    as_fatal (_("internal: mmix_prefix_name but empty prefix"));

  if (*shortname == '$')
    return shortname;

  obstack_grow (&mmix_sym_obstack, mmix_current_prefix,
		strlen (mmix_current_prefix));
  obstack_grow (&mmix_sym_obstack, shortname, strlen (shortname) + 1);
  return obstack_finish (&mmix_sym_obstack);
}

/* The GREG pseudo.  At LABEL, we have the name of a symbol that we
   want to make a register symbol, and which should be initialized with
   the value in the expression at INPUT_LINE_POINTER (defaulting to 0).
   Either and (perhaps less meaningful) both may be missing.  LABEL must
   be persistent, perhaps allocated on an obstack.  */

static void
mmix_greg_internal (label)
     char *label;
{
  expressionS *expP = &mmix_raw_gregs[n_of_raw_gregs].exp;

  /* Don't set the section to register contents section before the
     expression has been parsed; it may refer to the current position.  */
  expression (expP);

  /* FIXME: Check that no expression refers to the register contents
     section.  May need to be done in elf64-mmix.c.  */
  if (expP->X_op == O_absent)
    {
      /* Default to zero if the expression was absent.  */
      expP->X_op = O_constant;
      expP->X_add_number = 0;
      expP->X_unsigned = 0;
      expP->X_add_symbol = NULL;
      expP->X_op_symbol = NULL;
    }

  /* We must handle prefixes here, as we save the labels and expressions
     to be output later.  */
  mmix_raw_gregs[n_of_raw_gregs].label
    = mmix_current_prefix == NULL ? label : mmix_prefix_name (label);

  if (n_of_raw_gregs == MAX_GREGS - 1)
    as_bad (_("too many GREG registers allocated (max %d)"), MAX_GREGS);
  else
    n_of_raw_gregs++;

  mmix_handle_rest_of_empty_line ();
}

/* The ".greg label,expr" worker.  */

static void
s_greg (unused)
     int unused ATTRIBUTE_UNUSED;
{
  char *p;
  char c;
  p = input_line_pointer;

  /* This will skip over what can be a symbol and zero out the next
     character, which we assume is a ',' or other meaningful delimiter.
     What comes after that is the initializer expression for the
     register.  */
  c = get_symbol_end ();

  if (! is_end_of_line[(unsigned char) c])
    input_line_pointer++;

  if (*p)
    {
      /* The label must be persistent; it's not used until after all input
	 has been seen.  */
      obstack_grow (&mmix_sym_obstack, p, strlen (p) + 1);
      mmix_greg_internal (obstack_finish (&mmix_sym_obstack));
    }
  else
    mmix_greg_internal (NULL);
}

/* The "BSPEC expr" worker.  */

static void
s_bspec (unused)
     int unused ATTRIBUTE_UNUSED;
{
  asection *expsec;
  asection *sec;
  char secname[sizeof (MMIX_OTHER_SPEC_SECTION_PREFIX) + 20]
    = MMIX_OTHER_SPEC_SECTION_PREFIX;
  expressionS exp;
  int n;

  /* Get a constant expression which we can evaluate *now*.  Supporting
     more complex (though assembly-time computable) expressions is
     feasible but Too Much Work for something of unknown usefulness like
     BSPEC-ESPEC.  */
  expsec = expression (&exp);
  mmix_handle_rest_of_empty_line ();

  /* Check that we don't have another BSPEC in progress.  */
  if (doing_bspec)
    {
      as_bad (_("BSPEC already active.  Nesting is not supported."));
      return;
    }

  if (exp.X_op != O_constant
      || expsec != absolute_section
      || exp.X_add_number < 0
      || exp.X_add_number > 65535)
    {
      as_bad (_("invalid BSPEC expression"));
      exp.X_add_number = 0;
    }

  n = (int) exp.X_add_number;

  sprintf (secname + strlen (MMIX_OTHER_SPEC_SECTION_PREFIX), "%d", n);
  sec = bfd_get_section_by_name (stdoutput, secname);
  if (sec == NULL)
    {
      /* We need a non-volatile name as it will be stored in the section
         struct.  */
      char *newsecname = xstrdup (secname);
      sec = bfd_make_section (stdoutput, newsecname);

      if (sec == NULL)
	as_fatal (_("can't create section %s"), newsecname);

      if (!bfd_set_section_flags (stdoutput, sec,
				  bfd_get_section_flags (stdoutput, sec)
				  | SEC_READONLY))
	as_fatal (_("can't set section flags for section %s"), newsecname);
    }

  /* Tell ELF about the pending section change.  */
  obj_elf_section_change_hook ();
  subseg_set (sec, 0);

  /* Save position for missing ESPEC.  */
  as_where (&bspec_file, &bspec_line);

  doing_bspec = 1;
}

/* The "ESPEC" worker.  */

static void
s_espec (unused)
     int unused ATTRIBUTE_UNUSED;
{
  /* First, check that we *do* have a BSPEC in progress.  */
  if (! doing_bspec)
    {
      as_bad (_("ESPEC without preceding BSPEC"));
      return;
    }

  mmix_handle_rest_of_empty_line ();
  doing_bspec = 0;

  /* When we told ELF about the section change in s_bspec, it stored the
     previous section for us so we can get at it with the equivalent of a
     .previous pseudo.  */
  obj_elf_previous (0);
}

/* The " .local expr" and " local expr" worker.  We make a BFD_MMIX_LOCAL
   relocation against the current position against the expression.
   Implementing this by means of contents in a section lost.  */

static void
mmix_s_local (unused)
     int unused ATTRIBUTE_UNUSED;
{
  expressionS exp;

  /* Don't set the section to register contents section before the
     expression has been parsed; it may refer to the current position in
     some contorted way.  */
  expression (&exp);

  if (exp.X_op == O_absent)
    {
      as_bad (_("missing local expression"));
      return;
    }
  else if (exp.X_op == O_register)
    {
      /* fix_new_exp doesn't like O_register.  Should be configurable.
	 We're fine with a constant here, though.  */
      exp.X_op = O_constant;
    }

  fix_new_exp (frag_now, 0, 0, &exp, 0, BFD_RELOC_MMIX_LOCAL);
  mmix_handle_rest_of_empty_line ();
}

/* Set fragP->fr_var to the initial guess of the size of a relaxable insn
   and return it.  Sizes of other instructions are not known.  This
   function may be called multiple times.  */

int
md_estimate_size_before_relax (fragP, segment)
     fragS *fragP;
     segT    segment;
{
  int length;

#define HANDLE_RELAXABLE(state)						\
 case ENCODE_RELAX (state, STATE_UNDF):					\
   if (fragP->fr_symbol != NULL						\
       && S_GET_SEGMENT (fragP->fr_symbol) == segment			\
       && !S_IS_WEAK (fragP->fr_symbol))				\
     {									\
       /* The symbol lies in the same segment - a relaxable case.  */	\
       fragP->fr_subtype						\
	 = ENCODE_RELAX (state, STATE_ZERO);				\
     }									\
   break;

  switch (fragP->fr_subtype)
    {
      HANDLE_RELAXABLE (STATE_GETA);
      HANDLE_RELAXABLE (STATE_BCC);
      HANDLE_RELAXABLE (STATE_JMP);

    case ENCODE_RELAX (STATE_PUSHJ, STATE_UNDF):
      if (fragP->fr_symbol != NULL
	  && S_GET_SEGMENT (fragP->fr_symbol) == segment
	  && !S_IS_WEAK (fragP->fr_symbol))
	/* The symbol lies in the same segment - a relaxable case.  */
	fragP->fr_subtype = ENCODE_RELAX (STATE_PUSHJ, STATE_ZERO);
      else if (pushj_stubs)
	/* If we're to generate stubs, assume we can reach a stub after
           the section.  */
	fragP->fr_subtype = ENCODE_RELAX (STATE_PUSHJSTUB, STATE_ZERO);
      /* FALLTHROUGH.  */
    case ENCODE_RELAX (STATE_PUSHJ, STATE_ZERO):
    case ENCODE_RELAX (STATE_PUSHJSTUB, STATE_ZERO):
      /* We need to distinguish different relaxation rounds.  */
      seg_info (segment)->tc_segment_info_data.last_stubfrag = fragP;
      break;

    case ENCODE_RELAX (STATE_GETA, STATE_ZERO):
    case ENCODE_RELAX (STATE_BCC, STATE_ZERO):
    case ENCODE_RELAX (STATE_JMP, STATE_ZERO):
      /* When relaxing a section for the second time, we don't need to do
	 anything except making sure that fr_var is set right.  */
      break;

    case STATE_GREG_DEF:
      length = fragP->tc_frag_data != NULL ? 0 : 8;
      fragP->fr_var = length;

      /* Don't consult the relax_table; it isn't valid for this
	 relaxation.  */
      return length;
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
    }

  length = mmix_relax_table[fragP->fr_subtype].rlx_length;
  fragP->fr_var = length;

  return length;
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on
   OK.  */

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
      /* FIXME: Having 'f' in mmix_flt_chars (and here) makes it
	 problematic to also have a forward reference in an expression.
	 The testsuite wants it, and it's customary.
	 We'll deal with the real problems when they come; we share the
	 problem with most other ports.  */
    case 'f':
    case 'r':
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

/* Convert variable-sized frags into one or more fixups.  */

void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     fragS *fragP;
{
  /* Pointer to first byte in variable-sized part of the frag.  */
  char *var_partp;

  /* Pointer to first opcode byte in frag.  */
  char *opcodep;

  /* Size in bytes of variable-sized part of frag.  */
  int var_part_size = 0;

  /* This is part of *fragP.  It contains all information about addresses
     and offsets to varying parts.  */
  symbolS *symbolP;
  unsigned long var_part_offset;

  /* This is the frag for the opcode.  It, rather than fragP, must be used
     when emitting a frag for the opcode.  */
  fragS *opc_fragP = fragP->tc_frag_data;
  fixS *tmpfixP;

  /* Where, in file space, does addr point?  */
  bfd_vma target_address;
  bfd_vma opcode_address;

  know (fragP->fr_type == rs_machine_dependent);

  var_part_offset = fragP->fr_fix;
  var_partp = fragP->fr_literal + var_part_offset;
  opcodep = fragP->fr_opcode;

  symbolP = fragP->fr_symbol;

  target_address
    = ((symbolP ? S_GET_VALUE (symbolP) : 0) + fragP->fr_offset);

  /* The opcode that would be extended is the last four "fixed" bytes.  */
  opcode_address = fragP->fr_address + fragP->fr_fix - 4;

  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_PUSHJSTUB, STATE_ZERO):
      /* Setting the unknown bits to 0 seems the most appropriate.  */
      mmix_set_geta_branch_offset (opcodep, 0);
      tmpfixP = fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 8,
			 fragP->fr_symbol, fragP->fr_offset, 1,
			 BFD_RELOC_MMIX_PUSHJ_STUBBABLE);
      COPY_FR_WHERE_TO_FX (fragP, tmpfixP);
      var_part_size = 0;
      break;

    case ENCODE_RELAX (STATE_GETA, STATE_ZERO):
    case ENCODE_RELAX (STATE_BCC, STATE_ZERO):
    case ENCODE_RELAX (STATE_PUSHJ, STATE_ZERO):
      mmix_set_geta_branch_offset (opcodep, target_address - opcode_address);
      if (linkrelax)
	{
	  tmpfixP
	    = fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		       fragP->fr_symbol, fragP->fr_offset, 1,
		       BFD_RELOC_MMIX_ADDR19);
	  COPY_FR_WHERE_TO_FX (fragP, tmpfixP);
	}
      var_part_size = 0;
      break;

    case ENCODE_RELAX (STATE_JMP, STATE_ZERO):
      mmix_set_jmp_offset (opcodep, target_address - opcode_address);
      if (linkrelax)
	{
	  tmpfixP
	    = fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		       fragP->fr_symbol, fragP->fr_offset, 1,
		       BFD_RELOC_MMIX_ADDR27);
	  COPY_FR_WHERE_TO_FX (fragP, tmpfixP);
	}
      var_part_size = 0;
      break;

    case STATE_GREG_DEF:
      if (fragP->tc_frag_data == NULL)
	{
	  /* We must initialize data that's supposed to be "fixed up" to
	     avoid emitting garbage, because md_apply_fix3 won't do
	     anything for undefined symbols.  */
	  md_number_to_chars (var_partp, 0, 8);
	  tmpfixP
	    = fix_new (fragP, var_partp - fragP->fr_literal, 8,
		       fragP->fr_symbol, fragP->fr_offset, 0, BFD_RELOC_64);
	  COPY_FR_WHERE_TO_FX (fragP, tmpfixP);
	  mmix_gregs[n_of_cooked_gregs++] = tmpfixP;
	  var_part_size = 8;
	}
      else
	var_part_size = 0;
      break;

#define HANDLE_MAX_RELOC(state, reloc)					\
  case ENCODE_RELAX (state, STATE_MAX):					\
    var_part_size							\
      = mmix_relax_table[ENCODE_RELAX (state, STATE_MAX)].rlx_length;	\
    mmix_fill_nops (var_partp, var_part_size / 4);			\
    if (warn_on_expansion)						\
      as_warn_where (fragP->fr_file, fragP->fr_line,			\
		     _("operand out of range, instruction expanded"));	\
    tmpfixP = fix_new (fragP, var_partp - fragP->fr_literal - 4, 8,	\
		       fragP->fr_symbol, fragP->fr_offset, 1, reloc);	\
    COPY_FR_WHERE_TO_FX (fragP, tmpfixP);				\
    break

      HANDLE_MAX_RELOC (STATE_GETA, BFD_RELOC_MMIX_GETA);
      HANDLE_MAX_RELOC (STATE_BCC, BFD_RELOC_MMIX_CBRANCH);
      HANDLE_MAX_RELOC (STATE_PUSHJ, BFD_RELOC_MMIX_PUSHJ);
      HANDLE_MAX_RELOC (STATE_JMP, BFD_RELOC_MMIX_JMP);

    default:
      BAD_CASE (fragP->fr_subtype);
      break;
    }

  fragP->fr_fix += var_part_size;
  fragP->fr_var = 0;
}

/* Applies the desired value to the specified location.
   Also sets up addends for RELA type relocations.
   Stolen from tc-mcore.c.

   Note that this function isn't called when linkrelax != 0.  */

void
md_apply_fix3 (fixP, valP, segment)
     fixS *   fixP;
     valueT * valP;
     segT     segment;
{
  char *buf  = fixP->fx_where + fixP->fx_frag->fr_literal;
  /* Note: use offsetT because it is signed, valueT is unsigned.  */
  offsetT val  = (offsetT) * valP;
  segT symsec
    = (fixP->fx_addsy == NULL
       ? absolute_section : S_GET_SEGMENT (fixP->fx_addsy));

  /* If the fix is relative to a symbol which is not defined, or, (if
     pcrel), not in the same segment as the fix, we cannot resolve it
     here.  */
  if (fixP->fx_addsy != NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_IS_WEAK (fixP->fx_addsy)
	  || (fixP->fx_pcrel && symsec != segment)
	  || (! fixP->fx_pcrel
	      && symsec != absolute_section
	      && ((fixP->fx_r_type != BFD_RELOC_MMIX_REG
		   && fixP->fx_r_type != BFD_RELOC_MMIX_REG_OR_BYTE)
		  || symsec != reg_section))))
    {
      fixP->fx_done = 0;
      return;
    }
  else if (fixP->fx_r_type == BFD_RELOC_MMIX_LOCAL
	   || fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
	   || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    {
      /* These are never "fixed".  */
      fixP->fx_done = 0;
      return;
    }
  else
    /* We assume every other relocation is "fixed".  */
    fixP->fx_done = 1;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_64:
    case BFD_RELOC_32:
    case BFD_RELOC_24:
    case BFD_RELOC_16:
    case BFD_RELOC_8:
    case BFD_RELOC_64_PCREL:
    case BFD_RELOC_32_PCREL:
    case BFD_RELOC_24_PCREL:
    case BFD_RELOC_16_PCREL:
    case BFD_RELOC_8_PCREL:
      md_number_to_chars (buf, val, fixP->fx_size);
      break;

    case BFD_RELOC_MMIX_ADDR19:
      if (expand_op)
	{
	  /* This shouldn't happen.  */
	  BAD_CASE (fixP->fx_r_type);
	  break;
	}
      /* FALLTHROUGH.  */
    case BFD_RELOC_MMIX_GETA:
    case BFD_RELOC_MMIX_CBRANCH:
    case BFD_RELOC_MMIX_PUSHJ:
    case BFD_RELOC_MMIX_PUSHJ_STUBBABLE:
      /* If this fixup is out of range, punt to the linker to emit an
	 error.  This should only happen with -no-expand.  */
      if (val < -(((offsetT) 1 << 19)/2)
	  || val >= ((offsetT) 1 << 19)/2 - 1
	  || (val & 3) != 0)
	{
	  if (warn_on_expansion)
	    as_warn_where (fixP->fx_file, fixP->fx_line,
			   _("operand out of range"));
	  fixP->fx_done = 0;
	  val = 0;
	}
      mmix_set_geta_branch_offset (buf, val);
      break;

    case BFD_RELOC_MMIX_ADDR27:
      if (expand_op)
	{
	  /* This shouldn't happen.  */
	  BAD_CASE (fixP->fx_r_type);
	  break;
	}
      /* FALLTHROUGH.  */
    case BFD_RELOC_MMIX_JMP:
      /* If this fixup is out of range, punt to the linker to emit an
	 error.  This should only happen with -no-expand.  */
      if (val < -(((offsetT) 1 << 27)/2)
	  || val >= ((offsetT) 1 << 27)/2 - 1
	  || (val & 3) != 0)
	{
	  if (warn_on_expansion)
	    as_warn_where (fixP->fx_file, fixP->fx_line,
			   _("operand out of range"));
	  fixP->fx_done = 0;
	  val = 0;
	}
      mmix_set_jmp_offset (buf, val);
      break;

    case BFD_RELOC_MMIX_REG_OR_BYTE:
      if (fixP->fx_addsy != NULL
	  && (S_GET_SEGMENT (fixP->fx_addsy) != reg_section
	      || S_GET_VALUE (fixP->fx_addsy) > 255)
	  && S_GET_SEGMENT (fixP->fx_addsy) != absolute_section)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("invalid operands"));
	  /* We don't want this "symbol" appearing in output, because
	     that will fail.  */
	  fixP->fx_done = 1;
	}

      buf[0] = val;

      /* If this reloc is for a Z field, we need to adjust
	 the opcode if we got a constant here.
	 FIXME: Can we make this more robust?  */

      if ((fixP->fx_where & 3) == 3
	  && (fixP->fx_addsy == NULL
	      || S_GET_SEGMENT (fixP->fx_addsy) == absolute_section))
	buf[-3] |= IMM_OFFSET_BIT;
      break;

    case BFD_RELOC_MMIX_REG:
      if (fixP->fx_addsy == NULL
	  || S_GET_SEGMENT (fixP->fx_addsy) != reg_section
	  || S_GET_VALUE (fixP->fx_addsy) > 255)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("invalid operands"));
	  fixP->fx_done = 1;
	}

      *buf = val;
      break;

    case BFD_RELOC_MMIX_BASE_PLUS_OFFSET:
      /* These are never "fixed".  */
      fixP->fx_done = 0;
      return;

    case BFD_RELOC_MMIX_PUSHJ_1:
    case BFD_RELOC_MMIX_PUSHJ_2:
    case BFD_RELOC_MMIX_PUSHJ_3:
    case BFD_RELOC_MMIX_CBRANCH_J:
    case BFD_RELOC_MMIX_CBRANCH_1:
    case BFD_RELOC_MMIX_CBRANCH_2:
    case BFD_RELOC_MMIX_CBRANCH_3:
    case BFD_RELOC_MMIX_GETA_1:
    case BFD_RELOC_MMIX_GETA_2:
    case BFD_RELOC_MMIX_GETA_3:
    case BFD_RELOC_MMIX_JMP_1:
    case BFD_RELOC_MMIX_JMP_2:
    case BFD_RELOC_MMIX_JMP_3:
    default:
      BAD_CASE (fixP->fx_r_type);
      break;
    }

  if (fixP->fx_done)
    /* Make sure that for completed fixups we have the value around for
       use by e.g. mmix_frob_file.  */
    fixP->fx_offset = val;
}

/* A bsearch function for looking up a value against offsets for GREG
   definitions.  */

static int
cmp_greg_val_greg_symbol_fixes (p1, p2)
     const PTR p1;
     const PTR p2;
{
  offsetT val1 = *(offsetT *) p1;
  offsetT val2 = ((struct mmix_symbol_greg_fixes *) p2)->offs;

  if (val1 >= val2 && val1 < val2 + 255)
    return 0;

  if (val1 > val2)
    return 1;

  return -1;
}

/* Generate a machine-dependent relocation.  */

arelent *
tc_gen_reloc (section, fixP)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixP;
{
  bfd_signed_vma val
    = fixP->fx_offset
    + (fixP->fx_addsy != NULL
       && !S_IS_WEAK (fixP->fx_addsy)
       && !S_IS_COMMON (fixP->fx_addsy)
       ? S_GET_VALUE (fixP->fx_addsy) : 0);
  arelent *relP;
  bfd_reloc_code_real_type code = BFD_RELOC_NONE;
  char *buf  = fixP->fx_where + fixP->fx_frag->fr_literal;
  symbolS *addsy = fixP->fx_addsy;
  asection *addsec = addsy == NULL ? NULL : S_GET_SEGMENT (addsy);
  asymbol *baddsy = addsy != NULL ? symbol_get_bfdsym (addsy) : NULL;
  bfd_vma addend
    = val - (baddsy == NULL || S_IS_COMMON (addsy) || S_IS_WEAK (addsy)
	     ? 0 : bfd_asymbol_value (baddsy));

  /* A single " LOCAL expression" in the wrong section will not work when
     linking to MMO; relocations for zero-content sections are then
     ignored.  Normally, relocations would modify section contents, and
     you'd never think or be able to do something like that.  The
     relocation resulting from a LOCAL directive doesn't have an obvious
     and mandatory location.  I can't figure out a way to do this better
     than just helping the user around this limitation here; hopefully the
     code using the local expression is around.  Putting the LOCAL
     semantics in a relocation still seems right; a section didn't do.  */
  if (bfd_section_size (section->owner, section) == 0)
    as_bad_where
      (fixP->fx_file, fixP->fx_line,
       fixP->fx_r_type == BFD_RELOC_MMIX_LOCAL
       /* The BFD_RELOC_MMIX_LOCAL-specific message is supposed to be
	  user-friendly, though a little bit non-substantial.  */
       ? _("directive LOCAL must be placed in code or data")
       : _("internal confusion: relocation in a section without contents"));

  /* FIXME: Range tests for all these.  */
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_64:
    case BFD_RELOC_32:
    case BFD_RELOC_24:
    case BFD_RELOC_16:
    case BFD_RELOC_8:
      code = fixP->fx_r_type;

      if (addsy == NULL || bfd_is_abs_section (addsec))
	{
	  /* Resolve this reloc now, as md_apply_fix3 would have done (not
	     called if -linkrelax).  There is no point in keeping a reloc
	     to an absolute symbol.  No reloc that is subject to
	     relaxation must be to an absolute symbol; difference
	     involving symbols in a specific section must be signalled as
	     an error if the relaxing cannot be expressed; having a reloc
	     to the resolved (now absolute) value does not help.  */
	  md_number_to_chars (buf, val, fixP->fx_size);
	  return NULL;
	}
      break;

    case BFD_RELOC_64_PCREL:
    case BFD_RELOC_32_PCREL:
    case BFD_RELOC_24_PCREL:
    case BFD_RELOC_16_PCREL:
    case BFD_RELOC_8_PCREL:
    case BFD_RELOC_MMIX_LOCAL:
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_MMIX_GETA:
    case BFD_RELOC_MMIX_GETA_1:
    case BFD_RELOC_MMIX_GETA_2:
    case BFD_RELOC_MMIX_GETA_3:
    case BFD_RELOC_MMIX_CBRANCH:
    case BFD_RELOC_MMIX_CBRANCH_J:
    case BFD_RELOC_MMIX_CBRANCH_1:
    case BFD_RELOC_MMIX_CBRANCH_2:
    case BFD_RELOC_MMIX_CBRANCH_3:
    case BFD_RELOC_MMIX_PUSHJ:
    case BFD_RELOC_MMIX_PUSHJ_1:
    case BFD_RELOC_MMIX_PUSHJ_2:
    case BFD_RELOC_MMIX_PUSHJ_3:
    case BFD_RELOC_MMIX_PUSHJ_STUBBABLE:
    case BFD_RELOC_MMIX_JMP:
    case BFD_RELOC_MMIX_JMP_1:
    case BFD_RELOC_MMIX_JMP_2:
    case BFD_RELOC_MMIX_JMP_3:
    case BFD_RELOC_MMIX_ADDR19:
    case BFD_RELOC_MMIX_ADDR27:
      code = fixP->fx_r_type;
      break;

    case BFD_RELOC_MMIX_REG_OR_BYTE:
      /* If we have this kind of relocation to an unknown symbol or to the
	 register contents section (that is, to a register), then we can't
	 resolve the relocation here.  */
      if (addsy != NULL
	  && (bfd_is_und_section (addsec)
	      || strcmp (bfd_get_section_name (addsec->owner, addsec),
			 MMIX_REG_CONTENTS_SECTION_NAME) == 0))
	{
	  code = fixP->fx_r_type;
	  break;
	}

      /* If the relocation is not to the register section or to the
	 absolute section (a numeric value), then we have an error.  */
      if (addsy != NULL
	  && (S_GET_SEGMENT (addsy) != real_reg_section
	      || val > 255
	      || val < 0)
	  && ! bfd_is_abs_section (addsec))
	goto badop;

      /* Set the "immediate" bit of the insn if this relocation is to Z
	 field when the value is a numeric value, i.e. not a register.  */
      if ((fixP->fx_where & 3) == 3
	  && (addsy == NULL || bfd_is_abs_section (addsec)))
	buf[-3] |= IMM_OFFSET_BIT;

      buf[0] = val;
      return NULL;

    case BFD_RELOC_MMIX_BASE_PLUS_OFFSET:
      if (addsy != NULL
	  && strcmp (bfd_get_section_name (addsec->owner, addsec),
		     MMIX_REG_CONTENTS_SECTION_NAME) == 0)
	{
	  /* This changed into a register; the relocation is for the
	     register-contents section.  The constant part remains zero.  */
	  code = BFD_RELOC_MMIX_REG;
	  break;
	}

      /* If we've found out that this was indeed a register, then replace
	 with the register number.  The constant part is already zero.

	 If we encounter any other defined symbol, then we must find a
	 suitable register and emit a reloc.  */
      if (addsy == NULL || addsec != real_reg_section)
	{
	  struct mmix_symbol_gregs *gregs;
	  struct mmix_symbol_greg_fixes *fix;

	  if (S_IS_DEFINED (addsy)
	      && !bfd_is_com_section (addsec)
	      && !S_IS_WEAK (addsy))
	    {
	      if (! symbol_section_p (addsy) && ! bfd_is_abs_section (addsec))
		as_fatal (_("internal: BFD_RELOC_MMIX_BASE_PLUS_OFFSET not resolved to section"));

	      /* If this is an absolute symbol sufficiently near
		 lowest_data_loc, then we canonicalize on the data
		 section.  Note that val is signed here; we may subtract
		 lowest_data_loc which is unsigned.  Careful with those
		 comparisons.  */
	      if (lowest_data_loc != (bfd_vma) -1
		  && (bfd_vma) val + 256 > lowest_data_loc
		  && bfd_is_abs_section (addsec))
		{
		  val -= (offsetT) lowest_data_loc;
		  addsy = section_symbol (data_section);
		}
	      /* Likewise text section.  */
	      else if (lowest_text_loc != (bfd_vma) -1
		       && (bfd_vma) val + 256 > lowest_text_loc
		       && bfd_is_abs_section (addsec))
		{
		  val -= (offsetT) lowest_text_loc;
		  addsy = section_symbol (text_section);
		}
	    }

	  gregs = *symbol_get_tc (addsy);

	  /* If that symbol does not have any associated GREG definitions,
	     we can't do anything.  */
	  if (gregs == NULL
	      || (fix = bsearch (&val, gregs->greg_fixes, gregs->n_gregs,
				 sizeof (gregs->greg_fixes[0]),
				 cmp_greg_val_greg_symbol_fixes)) == NULL
	      /* The register must not point *after* the address we want.  */
	      || fix->offs > val
	      /* Neither must the register point more than 255 bytes
		 before the address we want.  */
	      || fix->offs + 255 < val)
	    {
	      /* We can either let the linker allocate GREGs
		 automatically, or emit an error.  */
	      if (allocate_undefined_gregs_in_linker)
		{
		  /* The values in baddsy and addend are right.  */
		  code = fixP->fx_r_type;
		  break;
		}
	      else
		as_bad_where (fixP->fx_file, fixP->fx_line,
			      _("no suitable GREG definition for operands"));
	      return NULL;
	    }
	  else
	    {
	      /* Transform the base-plus-offset reloc for the actual area
		 to a reloc for the register with the address of the area.
		 Put addend for register in Z operand.  */
	      buf[1] = val - fix->offs;
	      code = BFD_RELOC_MMIX_REG;
	      baddsy
		= (bfd_get_section_by_name (stdoutput,
					    MMIX_REG_CONTENTS_SECTION_NAME)
		   ->symbol);

	      addend = fix->fix->fx_frag->fr_address + fix->fix->fx_where;
	    }
	}
      else if (S_GET_VALUE (addsy) > 255)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("invalid operands"));
      else
	{
	  *buf = val;
	  return NULL;
	}
      break;

    case BFD_RELOC_MMIX_REG:
      if (addsy != NULL
	  && (bfd_is_und_section (addsec)
	      || strcmp (bfd_get_section_name (addsec->owner, addsec),
			 MMIX_REG_CONTENTS_SECTION_NAME) == 0))
	{
	  code = fixP->fx_r_type;
	  break;
	}

      if (addsy != NULL
	  && (addsec != real_reg_section
	      || val > 255
	      || val < 0)
	  && ! bfd_is_und_section (addsec))
	/* Drop through to error message.  */
	;
      else
	{
	  buf[0] = val;
	  return NULL;
	}
      /* FALLTHROUGH.  */

      /* The others are supposed to be handled by md_apply_fix3.
	 FIXME: ... which isn't called when -linkrelax.  Move over
	 md_apply_fix3 code here for everything reasonable.  */
    badop:
    default:
      as_bad_where
	(fixP->fx_file, fixP->fx_line,
	 _("operands were not reducible at assembly-time"));

      /* Unmark this symbol as used in a reloc, so we don't bump into a BFD
	 assert when trying to output reg_section.  FIXME: A gas bug.  */
      fixP->fx_addsy = NULL;
      return NULL;
    }

  relP = (arelent *) xmalloc (sizeof (arelent));
  assert (relP != 0);
  relP->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *relP->sym_ptr_ptr = baddsy;
  relP->address = fixP->fx_frag->fr_address + fixP->fx_where;

  relP->addend = addend;

  /* If this had been a.out, we would have had a kludge for weak symbols
     here.  */

  relP->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (! relP->howto)
    {
      const char *name;

      name = S_GET_NAME (addsy);
      if (name == NULL)
	name = _("<unknown>");
      as_fatal (_("cannot generate relocation type for symbol %s, code %s"),
		name, bfd_get_reloc_code_name (code));
    }

  return relP;
}

/* Do some reformatting of a line.  FIXME: We could transform a mmixal
   line into traditional (GNU?) format, unless #NO_APP, and get rid of all
   ugly labels_without_colons etc.  */

void
mmix_handle_mmixal ()
{
  char *s0 = input_line_pointer;
  char *s;
  char *label = NULL;
  char c;

  if (pending_label != NULL)
    as_fatal (_("internal: unhandled label %s"), pending_label);

  if (mmix_gnu_syntax)
    return;

  /* If the first character is a '.', then it's a pseudodirective, not a
     label.  Make GAS not handle label-without-colon on this line.  We
     also don't do mmixal-specific stuff on this line.  */
  if (input_line_pointer[0] == '.')
    {
      label_without_colon_this_line = 0;
      return;
    }

  /* Don't handle empty lines here.  */
  while (1)
    {
      if (*s0 == 0 || is_end_of_line[(unsigned int) *s0])
	return;

      if (! ISSPACE (*s0))
	break;

      s0++;
    }

  /* If we're on a line with a label, check if it's a mmixal fb-label.
     Save an indicator and skip the label; it must be set only after all
     fb-labels of expressions are evaluated.  */
  if (ISDIGIT (input_line_pointer[0])
      && input_line_pointer[1] == 'H'
      && ISSPACE (input_line_pointer[2]))
    {
      char *s;
      current_fb_label = input_line_pointer[0] - '0';

      /* We have to skip the label, but also preserve the newlineness of
	 the previous character, since the caller checks that.  It's a
	 mess we blame on the caller.  */
      input_line_pointer[1] = input_line_pointer[-1];
      input_line_pointer += 2;

      s = input_line_pointer;
      while (*s && ISSPACE (*s) && ! is_end_of_line[(unsigned int) *s])
	s++;

      /* For errors emitted here, the book-keeping is off by one; the
	 caller is about to bump the counters.  Adjust the error messages.  */
      if (is_end_of_line[(unsigned int) *s])
	{
	  char *name;
	  unsigned int line;
	  as_where (&name, &line);
	  as_bad_where (name, line + 1,
			_("[0-9]H labels may not appear alone on a line"));
	  current_fb_label = -1;
	}
      if (*s == '.')
	{
	  char *name;
	  unsigned int line;
	  as_where (&name, &line);
	  as_bad_where (name, line + 1,
			_("[0-9]H labels do not mix with dot-pseudos"));
	  current_fb_label = -1;
	}
    }
  else
    {
      current_fb_label = -1;
      if (is_name_beginner (input_line_pointer[0]))
	label = input_line_pointer;
    }

  s0 = input_line_pointer;
  /* Skip over label.  */
  while (*s0 && is_part_of_name (*s0))
    s0++;

  /* Remove trailing ":" off labels, as they'd otherwise be considered
     part of the name.  But don't do it for local labels.  */
  if (s0 != input_line_pointer && s0[-1] == ':'
      && (s0 - 2 != input_line_pointer
	  || ! ISDIGIT (s0[-2])))
    s0[-1] = ' ';
  else if (label != NULL)
    {
      /* For labels that don't end in ":", we save it so we can later give
	 it the same alignment and address as the associated instruction.  */

      /* Make room for the label including the ending nul.  */
      int len_0 = s0 - label + 1;

      /* Save this label on the MMIX symbol obstack.  Saving it on an
	 obstack is needless for "IS"-pseudos, but it's harmless and we
	 avoid a little code-cluttering.  */
      obstack_grow (&mmix_sym_obstack, label, len_0);
      pending_label = obstack_finish (&mmix_sym_obstack);
      pending_label[len_0 - 1] = 0;
    }

  while (*s0 && ISSPACE (*s0) && ! is_end_of_line[(unsigned int) *s0])
    s0++;

  if (pending_label != NULL && is_end_of_line[(unsigned int) *s0])
    /* Whoops, this was actually a lone label on a line.  Like :-ended
       labels, we don't attach such labels to the next instruction or
       pseudo.  */
    pending_label = NULL;

  /* Find local labels of operands.  Look for "[0-9][FB]" where the
     characters before and after are not part of words.  Break if a single
     or double quote is seen anywhere.  It means we can't have local
     labels as part of list with mixed quoted and unquoted members for
     mmixal compatibility but we can't have it all.  For the moment.
     Replace the '<N>B' or '<N>F' with MAGIC_FB_BACKWARD_CHAR<N> and
     MAGIC_FB_FORWARD_CHAR<N> respectively.  */

  /* First make sure we don't have any of the magic characters on the line
     appearing as input.  */
  s = s0;
  while (*s)
    {
      c = *s++;
      if (is_end_of_line[(unsigned int) c])
	break;
      if (c == MAGIC_FB_BACKWARD_CHAR || c == MAGIC_FB_FORWARD_CHAR)
	as_bad (_("invalid characters in input"));
    }

  /* Scan again, this time looking for ';' after operands.  */
  s = s0;

  /* Skip the insn.  */
  while (*s
	 && ! ISSPACE (*s)
	 && *s != ';'
	 && ! is_end_of_line[(unsigned int) *s])
    s++;

  /* Skip the spaces after the insn.  */
  while (*s
	 && ISSPACE (*s)
	 && *s != ';'
	 && ! is_end_of_line[(unsigned int) *s])
    s++;

  /* Skip the operands.  While doing this, replace [0-9][BF] with
     (MAGIC_FB_BACKWARD_CHAR|MAGIC_FB_FORWARD_CHAR)[0-9].  */
  while ((c = *s) != 0
	 && ! ISSPACE (c)
	 && c != ';'
	 && ! is_end_of_line[(unsigned int) c])
    {
      if (c == '"')
	{
	  s++;

	  /* FIXME: Test-case for semi-colon in string.  */
	  while (*s
		 && *s != '"'
		 && (! is_end_of_line[(unsigned int) *s] || *s == ';'))
	    s++;

	  if (*s == '"')
	    s++;
	}
      else if (ISDIGIT (c))
	{
	  if ((s[1] != 'B' && s[1] != 'F')
	      || is_part_of_name (s[-1])
	      || is_part_of_name (s[2]))
	    s++;
	  else
	    {
	      s[0] = (s[1] == 'B'
		      ? MAGIC_FB_BACKWARD_CHAR : MAGIC_FB_FORWARD_CHAR);
	      s[1] = c;
	    }
	}
      else
	s++;
    }

  /* Skip any spaces after the operands.  */
  while (*s
	 && ISSPACE (*s)
	 && *s != ';'
	 && !is_end_of_line[(unsigned int) *s])
    s++;

  /* If we're now looking at a semi-colon, then it's an end-of-line
     delimiter.  */
  mmix_next_semicolon_is_eoln = (*s == ';');

  /* Make IS into an EQU by replacing it with "= ".  Only match upper-case
     though; let lower-case be a syntax error.  */
  s = s0;
  if (s[0] == 'I' && s[1] == 'S' && ISSPACE (s[2]))
    {
      *s = '=';
      s[1] = ' ';

      /* Since labels can start without ":", we have to handle "X IS 42"
	 in full here, or "X" will be parsed as a label to be set at ".".  */
      input_line_pointer = s;

      /* Right after this function ends, line numbers will be bumped if
	 input_line_pointer[-1] = '\n'.  We want accurate line numbers for
	 the equals call, so we bump them before the call, and make sure
	 they aren't bumped afterwards.  */
      bump_line_counters ();

      /* A fb-label is valid as an IS-label.  */
      if (current_fb_label >= 0)
	{
	  char *fb_name;

	  /* We need to save this name on our symbol obstack, since the
	     string we got in fb_label_name is volatile and will change
	     with every call to fb_label_name, like those resulting from
	     parsing the IS-operand.  */
	  fb_name = fb_label_name (current_fb_label, 1);
	  obstack_grow (&mmix_sym_obstack, fb_name, strlen (fb_name) + 1);
	  equals (obstack_finish (&mmix_sym_obstack), 0);
	  fb_label_instance_inc (current_fb_label);
	  current_fb_label = -1;
	}
      else
	{
	  if (pending_label == NULL)
	    as_bad (_("empty label field for IS"));
	  else
	    equals (pending_label, 0);
	  pending_label = NULL;
	}

      /* For mmixal, we can have comments without a comment-start
	 character.   */
      mmix_handle_rest_of_empty_line ();
      input_line_pointer--;

      input_line_pointer[-1] = ' ';
    }
  else if (s[0] == 'G'
	   && s[1] == 'R'
	   && strncmp (s, "GREG", 4) == 0
	   && (ISSPACE (s[4]) || is_end_of_line[(unsigned char) s[4]]))
    {
      input_line_pointer = s + 4;

      /* Right after this function ends, line numbers will be bumped if
	 input_line_pointer[-1] = '\n'.  We want accurate line numbers for
	 the s_greg call, so we bump them before the call, and make sure
	 they aren't bumped afterwards.  */
      bump_line_counters ();

      /* A fb-label is valid as a GREG-label.  */
      if (current_fb_label >= 0)
	{
	  char *fb_name;

	  /* We need to save this name on our symbol obstack, since the
	     string we got in fb_label_name is volatile and will change
	     with every call to fb_label_name, like those resulting from
	     parsing the IS-operand.  */
	  fb_name = fb_label_name (current_fb_label, 1);

	  /* Make sure we save the canonical name and don't get bitten by
             prefixes.  */
	  obstack_1grow (&mmix_sym_obstack, ':');
	  obstack_grow (&mmix_sym_obstack, fb_name, strlen (fb_name) + 1);
	  mmix_greg_internal (obstack_finish (&mmix_sym_obstack));
	  fb_label_instance_inc (current_fb_label);
	  current_fb_label = -1;
	}
      else
	mmix_greg_internal (pending_label);

      /* Back up before the end-of-line marker that was skipped in
	 mmix_greg_internal.  */
      input_line_pointer--;
      input_line_pointer[-1] = ' ';

      pending_label = NULL;
    }
  else if (pending_label != NULL)
    {
      input_line_pointer += strlen (pending_label);

      /* See comment above about getting line numbers bumped.  */
      input_line_pointer[-1] = '\n';
    }
}

/* Give the value of an fb-label rewritten as in mmix_handle_mmixal, when
   parsing an expression.

   On valid calls, input_line_pointer points at a MAGIC_FB_BACKWARD_CHAR
   or MAGIC_FB_BACKWARD_CHAR, followed by an ascii digit for the label.
   We fill in the label as an expression.  */

void
mmix_fb_label (expP)
     expressionS *expP;
{
  symbolS *sym;
  char *fb_internal_name;

  /* This doesn't happen when not using mmixal syntax.  */
  if (mmix_gnu_syntax
      || (input_line_pointer[0] != MAGIC_FB_BACKWARD_CHAR
	  && input_line_pointer[0] != MAGIC_FB_FORWARD_CHAR))
    return;

  /* The current backward reference has augmentation 0.  A forward
     reference has augmentation 1, unless it's the same as a fb-label on
     _this_ line, in which case we add one more so we don't refer to it.
     This is the semantics of mmixal; it differs to that of common
     fb-labels which refer to a here-label on the current line as a
     backward reference.  */
  fb_internal_name
    = fb_label_name (input_line_pointer[1] - '0',
		     (input_line_pointer[0] == MAGIC_FB_FORWARD_CHAR ? 1 : 0)
		     + ((input_line_pointer[1] - '0' == current_fb_label
			 && input_line_pointer[0] == MAGIC_FB_FORWARD_CHAR)
			? 1 : 0));

  input_line_pointer += 2;
  sym = symbol_find_or_make (fb_internal_name);

  /* We don't have to clean up unrelated fields here; we just do what the
     expr machinery does, but *not* just what it does for [0-9][fb], since
     we need to treat those as ordinary symbols sometimes; see testcases
     err-byte2.s and fb-2.s.  */
  if (S_GET_SEGMENT (sym) == absolute_section)
    {
      expP->X_op = O_constant;
      expP->X_add_number = S_GET_VALUE (sym);
    }
  else
    {
      expP->X_op = O_symbol;
      expP->X_add_symbol = sym;
      expP->X_add_number = 0;
    }
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
mmix_force_relocation (fixP)
     fixS *fixP;
{
  if (fixP->fx_r_type == BFD_RELOC_MMIX_LOCAL
      || fixP->fx_r_type == BFD_RELOC_MMIX_BASE_PLUS_OFFSET)
    return 1;

  if (linkrelax)
    return 1;

  /* All our pcrel relocations are must-keep.  Note that md_apply_fix3 is
     called *after* this, and will handle getting rid of the presumed
     reloc; a relocation isn't *forced* other than to be handled by
     md_apply_fix3 (or tc_gen_reloc if linkrelax).  */
  if (fixP->fx_pcrel)
    return 1;

  return generic_force_reloc (fixP);
}

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixP, sec)
     fixS * fixP;
     segT   sec;
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* The symbol is undefined (or is defined but not in this section).
	 Let the linker figure it out.  */
      return 0;
    }

  return (fixP->fx_frag->fr_address + fixP->fx_where);
}

/* Adjust the symbol table.  We make reg_section relative to the real
   register section.  */

void
mmix_adjust_symtab ()
{
  symbolS *sym;
  symbolS *regsec = section_symbol (reg_section);

  for (sym = symbol_rootP; sym != NULL; sym = symbol_next (sym))
    if (S_GET_SEGMENT (sym) == reg_section)
      {
	if (sym == regsec)
	  {
	    if (S_IS_EXTERN (sym) || symbol_used_in_reloc_p (sym))
	      abort ();
	    symbol_remove (sym, &symbol_rootP, &symbol_lastP);
	  }
	else
	  /* Change section to the *real* register section, so it gets
	     proper treatment when writing it out.  Only do this for
	     global symbols.  This also means we don't have to check for
	     $0..$255.  */
	  S_SET_SEGMENT (sym, real_reg_section);
      }
}

/* This is the expansion of LABELS_WITHOUT_COLONS.
   We let md_start_line_hook tweak label_without_colon_this_line, and then
   this function returns the tweaked value, and sets it to 1 for the next
   line.  FIXME: Very, very brittle.  Not sure it works the way I
   thought at the time I first wrote this.  */

int
mmix_label_without_colon_this_line ()
{
  int retval = label_without_colon_this_line;

  if (! mmix_gnu_syntax)
    label_without_colon_this_line = 1;

  return retval;
}

/* This is the expansion of md_relax_frag.  We go through the ordinary
   relax table function except when the frag is for a GREG.  Then we have
   to check whether there's another GREG by the same value that we can
   join with.  */

long
mmix_md_relax_frag (seg, fragP, stretch)
     segT seg;
     fragS *fragP;
     long stretch;
{
  switch (fragP->fr_subtype)
    {
      /* Growth for this type has been handled by mmix_md_end and
	 correctly estimated, so there's nothing more to do here.  */
    case STATE_GREG_DEF:
      return 0;

    case ENCODE_RELAX (STATE_PUSHJ, STATE_ZERO):
      {
	/* We need to handle relaxation type ourselves, since relax_frag
	   doesn't update fr_subtype if there's no size increase in the
	   current section; when going from plain PUSHJ to a stub.  This
	   is otherwise functionally the same as relax_frag in write.c,
	   simplified for this case.  */
	offsetT aim;
	addressT target;
	addressT address;
	symbolS *symbolP;
	target = fragP->fr_offset;
	address = fragP->fr_address;
	symbolP = fragP->fr_symbol;

	if (symbolP)
	  {
	    fragS *sym_frag;

	    sym_frag = symbol_get_frag (symbolP);
	    know (S_GET_SEGMENT (symbolP) != absolute_section
		  || sym_frag == &zero_address_frag);
	    target += S_GET_VALUE (symbolP);

	    /* If frag has yet to be reached on this pass, assume it will
	       move by STRETCH just as we did.  If this is not so, it will
	       be because some frag between grows, and that will force
	       another pass.  */

	    if (stretch != 0
		&& sym_frag->relax_marker != fragP->relax_marker
		&& S_GET_SEGMENT (symbolP) == seg)
	      target += stretch;
	  }

	aim = target - address - fragP->fr_fix;
	if (aim >= PUSHJ_0B && aim <= PUSHJ_0F)
	  {
	    /* Target is reachable with a PUSHJ.  */
	    segment_info_type *seginfo = seg_info (seg);

	    /* If we're at the end of a relaxation round, clear the stub
	       counter as initialization for the next round.  */
	    if (fragP == seginfo->tc_segment_info_data.last_stubfrag)
	      seginfo->tc_segment_info_data.nstubs = 0;
	    return 0;
	  }

	/* Not reachable.  Try a stub.  */
	fragP->fr_subtype = ENCODE_RELAX (STATE_PUSHJSTUB, STATE_ZERO);
      }
      /* FALLTHROUGH.  */
    
      /* See if this PUSHJ is redirectable to a stub.  */
    case ENCODE_RELAX (STATE_PUSHJSTUB, STATE_ZERO):
      {
	segment_info_type *seginfo = seg_info (seg);
	fragS *lastfrag = seginfo->frchainP->frch_last;
	relax_substateT prev_type = fragP->fr_subtype;

	/* The last frag is always an empty frag, so it suffices to look
	   at its address to know the ending address of this section.  */
	know (lastfrag->fr_type == rs_fill
	      && lastfrag->fr_fix == 0
	      && lastfrag->fr_var == 0);

	/* For this PUSHJ to be relaxable into a call to a stub, the
	   distance must be no longer than 256k bytes from the PUSHJ to
	   the end of the section plus the maximum size of stubs so far.  */
	if ((lastfrag->fr_address
	     + stretch
	     + PUSHJ_MAX_LEN * seginfo->tc_segment_info_data.nstubs)
	    - (fragP->fr_address + fragP->fr_fix)
	    > GETA_0F
	    || !pushj_stubs)
	  fragP->fr_subtype = mmix_relax_table[prev_type].rlx_more;
	else
	  seginfo->tc_segment_info_data.nstubs++;

	/* If we're at the end of a relaxation round, clear the stub
	   counter as initialization for the next round.  */
	if (fragP == seginfo->tc_segment_info_data.last_stubfrag)
	  seginfo->tc_segment_info_data.nstubs = 0;

	return
	   (mmix_relax_table[fragP->fr_subtype].rlx_length
	    - mmix_relax_table[prev_type].rlx_length);
      }

    case ENCODE_RELAX (STATE_PUSHJ, STATE_MAX):
      {
	segment_info_type *seginfo = seg_info (seg);

	/* Need to cover all STATE_PUSHJ states to act on the last stub
	   frag (the end of this relax round; initialization for the
	   next).  */
	if (fragP == seginfo->tc_segment_info_data.last_stubfrag)
	  seginfo->tc_segment_info_data.nstubs = 0;

	return 0;
      }

    default:
      return relax_frag (seg, fragP, stretch);

    case STATE_GREG_UNDF:
      BAD_CASE (fragP->fr_subtype);
    }

  as_fatal (_("internal: unexpected relax type %d:%d"),
	    fragP->fr_type, fragP->fr_subtype);
  return 0;
}

/* Various things we punt until all input is seen.  */

void
mmix_md_end ()
{
  fragS *fragP;
  symbolS *mainsym;
  int i;

  /* The first frag of GREG:s going into the register contents section.  */
  fragS *mmix_reg_contents_frags = NULL;

  /* Reset prefix.  All labels reachable at this point must be
     canonicalized.  */
  mmix_current_prefix = NULL;

  if (doing_bspec)
    as_bad_where (bspec_file, bspec_line, _("BSPEC without ESPEC."));

  /* Emit the low LOC setting of .text.  */
  if (text_has_contents && lowest_text_loc != (bfd_vma) -1)
    {
      symbolS *symbolP;
      char locsymbol[sizeof (":") - 1
		    + sizeof (MMIX_LOC_SECTION_START_SYMBOL_PREFIX) - 1
		    + sizeof (".text")];

      /* An exercise in non-ISO-C-ness, this one.  */
      sprintf (locsymbol, ":%s%s", MMIX_LOC_SECTION_START_SYMBOL_PREFIX,
	       ".text");
      symbolP
	= symbol_new (locsymbol, absolute_section, lowest_text_loc,
		      &zero_address_frag);
      S_SET_EXTERNAL (symbolP);
    }

  /* Ditto .data.  */
  if (data_has_contents && lowest_data_loc != (bfd_vma) -1)
    {
      symbolS *symbolP;
      char locsymbol[sizeof (":") - 1
		     + sizeof (MMIX_LOC_SECTION_START_SYMBOL_PREFIX) - 1
		     + sizeof (".data")];

      sprintf (locsymbol, ":%s%s", MMIX_LOC_SECTION_START_SYMBOL_PREFIX,
	       ".data");
      symbolP
	= symbol_new (locsymbol, absolute_section, lowest_data_loc,
		      &zero_address_frag);
      S_SET_EXTERNAL (symbolP);
    }

  /* Unless GNU syntax mode, set "Main" to be a function, so the
     disassembler doesn't get confused when we write truly
     mmixal-compatible code (and don't use .type).  Similarly set it
     global (regardless of -globalize-symbols), so the linker sees it as
     the start symbol in ELF mode.  */
  mainsym = symbol_find (MMIX_START_SYMBOL_NAME);
  if (mainsym != NULL && ! mmix_gnu_syntax)
    {
      symbol_get_bfdsym (mainsym)->flags |= BSF_FUNCTION;
      S_SET_EXTERNAL (mainsym);
    }

  if (n_of_raw_gregs != 0)
    {
      /* Emit GREGs.  They are collected in order of appearance, but must
	 be emitted in opposite order to both have section address regno*8
	 and the same allocation order (within a file) as mmixal.  */
      segT this_segment = now_seg;
      subsegT this_subsegment = now_subseg;
      asection *regsec
	= bfd_make_section_old_way (stdoutput,
				    MMIX_REG_CONTENTS_SECTION_NAME);
      subseg_set (regsec, 0);

      /* Finally emit the initialization-value.  Emit a variable frag, which
	 we'll fix in md_estimate_size_before_relax.  We set the initializer
	 for the tc_frag_data field to NULL, so we can use that field for
	 relaxation purposes.  */
      mmix_opcode_frag = NULL;

      frag_grow (0);
      mmix_reg_contents_frags = frag_now;

      for (i = n_of_raw_gregs - 1; i >= 0; i--)
	{
	  if (mmix_raw_gregs[i].label != NULL)
	    /* There's a symbol.  Let it refer to this location in the
	       register contents section.  The symbol must be globalized
	       separately.  */
	    colon (mmix_raw_gregs[i].label);

	  frag_var (rs_machine_dependent, 8, 0, STATE_GREG_UNDF,
		    make_expr_symbol (&mmix_raw_gregs[i].exp), 0, NULL);
	}

      subseg_set (this_segment, this_subsegment);
    }

  /* Iterate over frags resulting from GREGs and move those that evidently
     have the same value together and point one to another.

     This works in time O(N^2) but since the upper bound for non-error use
     is 223, it's best to keep this simpler algorithm.  */
  for (fragP = mmix_reg_contents_frags; fragP != NULL; fragP = fragP->fr_next)
    {
      fragS **fpp;
      fragS *fp = NULL;
      fragS *osymfrag;
      offsetT osymval;
      expressionS *oexpP;
      symbolS *symbolP = fragP->fr_symbol;

      if (fragP->fr_type != rs_machine_dependent
	  || fragP->fr_subtype != STATE_GREG_UNDF)
	continue;

      /* Whatever the outcome, we will have this GREG judged merged or
	 non-merged.  Since the tc_frag_data is NULL at this point, we
	 default to non-merged.  */
      fragP->fr_subtype = STATE_GREG_DEF;

      /* If we're not supposed to merge GREG definitions, then just don't
	 look for equivalents.  */
      if (! merge_gregs)
	continue;

      osymval = (offsetT) S_GET_VALUE (symbolP);
      osymfrag = symbol_get_frag (symbolP);

      /* If the symbol isn't defined, we can't say that another symbol
	 equals this frag, then.  FIXME: We can look at the "deepest"
	 defined name; if a = c and b = c then obviously a == b.  */
      if (! S_IS_DEFINED (symbolP))
	continue;

      oexpP = symbol_get_value_expression (fragP->fr_symbol);

      /* If the initialization value is zero, then we must not merge them.  */
      if (oexpP->X_op == O_constant && osymval == 0)
	continue;

      /* Iterate through the frags downward this one.  If we find one that
	 has the same non-zero value, move it to after this one and point
	 to it as the equivalent.  */
      for (fpp = &fragP->fr_next; *fpp != NULL; fpp = &fpp[0]->fr_next)
	{
	  fp = *fpp;

	  if (fp->fr_type != rs_machine_dependent
	      || fp->fr_subtype != STATE_GREG_UNDF)
	    continue;

	  /* Calling S_GET_VALUE may simplify the symbol, changing from
	     expr_section etc. so call it first.  */
	  if ((offsetT) S_GET_VALUE (fp->fr_symbol) == osymval
	      && symbol_get_frag (fp->fr_symbol) == osymfrag)
	    {
	      /* Move the frag links so the one we found equivalent comes
		 after the current one, carefully considering that
		 sometimes fpp == &fragP->fr_next and the moves must be a
		 NOP then.  */
	      *fpp = fp->fr_next;
	      fp->fr_next = fragP->fr_next;
	      fragP->fr_next = fp;
	      break;
	    }
	}

      if (*fpp != NULL)
	fragP->tc_frag_data = fp;
    }
}

/* qsort function for mmix_symbol_gregs.  */

static int
cmp_greg_symbol_fixes (parg, qarg)
     const PTR parg;
     const PTR qarg;
{
  const struct mmix_symbol_greg_fixes *p
    = (const struct mmix_symbol_greg_fixes *) parg;
  const struct mmix_symbol_greg_fixes *q
    = (const struct mmix_symbol_greg_fixes *) qarg;

  return p->offs > q->offs ? 1 : p->offs < q->offs ? -1 : 0;
}

/* Collect GREG definitions from mmix_gregs and hang them as lists sorted
   on increasing offsets onto each section symbol or undefined symbol.

   Also, remove the register convenience section so it doesn't get output
   as an ELF section.  */

void
mmix_frob_file ()
{
  int i;
  struct mmix_symbol_gregs *all_greg_symbols[MAX_GREGS];
  int n_greg_symbols = 0;

  /* Collect all greg fixups and decorate each corresponding symbol with
     the greg fixups for it.  */
  for (i = 0; i < n_of_cooked_gregs; i++)
    {
      offsetT offs;
      symbolS *sym;
      struct mmix_symbol_gregs *gregs;
      fixS *fixP;

      fixP = mmix_gregs[i];
      know (fixP->fx_r_type == BFD_RELOC_64);

      /* This case isn't doable in general anyway, methinks.  */
      if (fixP->fx_subsy != NULL)
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("GREG expression too complicated"));
	  continue;
	}

      sym = fixP->fx_addsy;
      offs = (offsetT) fixP->fx_offset;

      /* If the symbol is defined, then it must be resolved to a section
	 symbol at this time, or else we don't know how to handle it.  */
      if (S_IS_DEFINED (sym)
	  && !bfd_is_com_section (S_GET_SEGMENT (sym))
	  && !S_IS_WEAK (sym))
	{
	  if (! symbol_section_p (sym)
	      && ! bfd_is_abs_section (S_GET_SEGMENT (sym)))
	    as_fatal (_("internal: GREG expression not resolved to section"));

	  offs += S_GET_VALUE (sym);
	}

      /* If this is an absolute symbol sufficiently near lowest_data_loc,
	 then we canonicalize on the data section.  Note that offs is
	 signed here; we may subtract lowest_data_loc which is unsigned.
	 Careful with those comparisons.  */
      if (lowest_data_loc != (bfd_vma) -1
	  && (bfd_vma) offs + 256 > lowest_data_loc
	  && bfd_is_abs_section (S_GET_SEGMENT (sym)))
	{
	  offs -= (offsetT) lowest_data_loc;
	  sym = section_symbol (data_section);
	}
      /* Likewise text section.  */
      else if (lowest_text_loc != (bfd_vma) -1
	       && (bfd_vma) offs + 256 > lowest_text_loc
	       && bfd_is_abs_section (S_GET_SEGMENT (sym)))
	{
	  offs -= (offsetT) lowest_text_loc;
	  sym = section_symbol (text_section);
	}

      gregs = *symbol_get_tc (sym);

      if (gregs == NULL)
	{
	  gregs = xmalloc (sizeof (*gregs));
	  gregs->n_gregs = 0;
	  symbol_set_tc (sym, &gregs);
	  all_greg_symbols[n_greg_symbols++] = gregs;
	}

      gregs->greg_fixes[gregs->n_gregs].fix = fixP;
      gregs->greg_fixes[gregs->n_gregs++].offs = offs;
    }

  /* For each symbol having a GREG definition, sort those definitions on
     offset.  */
  for (i = 0; i < n_greg_symbols; i++)
    qsort (all_greg_symbols[i]->greg_fixes, all_greg_symbols[i]->n_gregs,
	   sizeof (all_greg_symbols[i]->greg_fixes[0]), cmp_greg_symbol_fixes);

  if (real_reg_section != NULL)
    {
      asection **secpp;

      /* FIXME: Pass error state gracefully.  */
      if (bfd_get_section_flags (stdoutput, real_reg_section) & SEC_HAS_CONTENTS)
	as_fatal (_("register section has contents\n"));

      /* Really remove the section.  */
      for (secpp = &stdoutput->sections;
	   *secpp != real_reg_section;
	   secpp = &(*secpp)->next)
	;
      bfd_section_list_remove (stdoutput, secpp);
      --stdoutput->section_count;
    }

}

/* Provide an expression for a built-in name provided when-used.
   Either a symbol that is a handler; living in 0x10*[1..8] and having
   name [DVWIOUZX]_Handler, or a mmixal built-in symbol.

   If the name isn't a built-in name and parsed into *EXPP, return zero.  */

int
mmix_parse_predefined_name (name, expP)
     char *name;
     expressionS *expP;
{
  char *canon_name;
  char *handler_charp;
  const char handler_chars[] = "DVWIOUZX";
  symbolS *symp;

  if (! predefined_syms)
    return 0;

  canon_name = tc_canonicalize_symbol_name (name);

  if (canon_name[1] == '_'
      && strcmp (canon_name + 2, "Handler") == 0
      && (handler_charp = strchr (handler_chars, *canon_name)) != NULL)
    {
      /* If the symbol doesn't exist, provide one relative to the .text
	 section.

	 FIXME: We should provide separate sections, mapped in the linker
	 script.  */
      symp = symbol_find (name);
      if (symp == NULL)
	symp = symbol_new (name, text_section,
			   0x10 * (handler_charp + 1 - handler_chars),
			   &zero_address_frag);
    }
  else
    {
      /* These symbols appear when referenced; needed for
         mmixal-compatible programs.  */
      unsigned int i;

      static const struct
      {
	const char *name;
	valueT val;
      } predefined_abs_syms[] =
	{
	  {"Data_Segment", (valueT) 0x20 << 56},
	  {"Pool_Segment", (valueT) 0x40 << 56},
	  {"Stack_Segment", (valueT) 0x60 << 56},
	  {"StdIn", 0},
	  {"StdOut", 1},
	  {"StdErr", 2},
	  {"TextRead", 0},
	  {"TextWrite", 1},
	  {"BinaryRead", 2},
	  {"BinaryWrite", 3},
	  {"BinaryReadWrite", 4},
	  {"Halt", 0},
	  {"Fopen", 1},
	  {"Fclose", 2},
	  {"Fread", 3},
	  {"Fgets", 4},
	  {"Fgetws", 5},
	  {"Fwrite", 6},
	  {"Fputs", 7},
	  {"Fputws", 8},
	  {"Fseek", 9},
	  {"Ftell", 10},
	  {"D_BIT", 0x80},
	  {"V_BIT", 0x40},
	  {"W_BIT", 0x20},
	  {"I_BIT", 0x10},
	  {"O_BIT", 0x08},
	  {"U_BIT", 0x04},
	  {"Z_BIT", 0x02},
	  {"X_BIT", 0x01},
	  {"Inf", 0x7ff00000}
	};

      /* If it's already in the symbol table, we shouldn't do anything.  */
      symp = symbol_find (name);
      if (symp != NULL)
	return 0;

      for (i = 0;
	   i < sizeof (predefined_abs_syms) / sizeof (predefined_abs_syms[0]);
	   i++)
	if (strcmp (canon_name, predefined_abs_syms[i].name) == 0)
	  {
	    symbol_table_insert (symbol_new (predefined_abs_syms[i].name,
					     absolute_section,
					     predefined_abs_syms[i].val,
					     &zero_address_frag));

	    /* Let gas find the symbol we just created, through its
               ordinary lookup.  */
	    return 0;
	  }

      /* Not one of those symbols.  Let gas handle it.  */
      return 0;
    }

  expP->X_op = O_symbol;
  expP->X_add_number = 0;
  expP->X_add_symbol = symp;
  expP->X_op_symbol = NULL;

  return 1;
}

/* Just check that we don't have a BSPEC/ESPEC pair active when changing
   sections "normally", and get knowledge about alignment from the new
   section.  */

void
mmix_md_elf_section_change_hook ()
{
  if (doing_bspec)
    as_bad (_("section change from within a BSPEC/ESPEC pair is not supported"));

  last_alignment = bfd_get_section_alignment (now_seg->owner, now_seg);
  want_unaligned = 0;
}

/* The LOC worker.  This is like s_org, but we have to support changing
   section too.   */

static void
s_loc (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  segT section;
  expressionS exp;
  char *p;
  symbolS *sym;
  offsetT off;

  /* Must not have a BSPEC in progress.  */
  if (doing_bspec)
    {
      as_bad (_("directive LOC from within a BSPEC/ESPEC pair is not supported"));
      return;
    }

  section = expression (&exp);

  if (exp.X_op == O_illegal
      || exp.X_op == O_absent
      || exp.X_op == O_big
      || section == undefined_section)
    {
      as_bad (_("invalid LOC expression"));
      return;
    }

  if (section == absolute_section)
    {
      /* Translate a constant into a suitable section.  */

      if (exp.X_add_number < ((offsetT) 0x20 << 56))
	{
	  /* Lower than Data_Segment - assume it's .text.  */
	  section = text_section;

	  /* Save the lowest seen location, so we can pass on this
	     information to the linker.  We don't actually org to this
	     location here, we just pass on information to the linker so
	     it can put the code there for us.  */

	  /* If there was already a loc (that has to be set lower than
	     this one), we org at (this - lower).  There's an implicit
	     "LOC 0" before any entered code.  FIXME: handled by spurious
	     settings of text_has_contents.  */
	  if (exp.X_add_number < 0
	      || exp.X_add_number < (offsetT) lowest_text_loc)
	    {
	      as_bad (_("LOC expression stepping backwards is not supported"));
	      exp.X_op = O_absent;
	    }
	  else
	    {
	      if (text_has_contents && lowest_text_loc == (bfd_vma) -1)
		lowest_text_loc = 0;

	      if (lowest_text_loc == (bfd_vma) -1)
		{
		  lowest_text_loc = exp.X_add_number;

		  /* We want only to change the section, not set an offset.  */
		  exp.X_op = O_absent;
		}
	      else
		exp.X_add_number -= lowest_text_loc;
	    }
	}
      else
	{
	  /* Do the same for the .data section.  */
	  section = data_section;

	  if (exp.X_add_number < (offsetT) lowest_data_loc)
	    {
	      as_bad (_("LOC expression stepping backwards is not supported"));
	      exp.X_op = O_absent;
	    }
	  else
	    {
	      if (data_has_contents && lowest_data_loc == (bfd_vma) -1)
		lowest_data_loc = (bfd_vma) 0x20 << 56;

	      if (lowest_data_loc == (bfd_vma) -1)
		{
		  lowest_data_loc = exp.X_add_number;

		  /* We want only to change the section, not set an offset.  */
		  exp.X_op = O_absent;
		}
	      else
		exp.X_add_number -= lowest_data_loc;
	    }
	}
    }

  if (section != now_seg)
    {
      obj_elf_section_change_hook ();
      subseg_set (section, 0);

      /* Call our section change hooks using the official hook.  */
      md_elf_section_change_hook ();
    }

  if (exp.X_op != O_absent)
    {
      if (exp.X_op != O_constant && exp.X_op != O_symbol)
	{
	  /* Handle complex expressions.  */
	  sym = make_expr_symbol (&exp);
	  off = 0;
	}
      else
	{
	  sym = exp.X_add_symbol;
	  off = exp.X_add_number;
	}

      p = frag_var (rs_org, 1, 1, (relax_substateT) 0, sym, off, (char *) 0);
      *p = 0;
    }

  mmix_handle_rest_of_empty_line ();
}

/* The BYTE worker.  We have to support sequences of mixed "strings",
   numbers and other constant "first-pass" reducible expressions separated
   by comma.  */

static void
mmix_byte ()
{
  unsigned int c;
  char *start;

  if (now_seg == text_section)
    text_has_contents = 1;
  else if (now_seg == data_section)
    data_has_contents = 1;

  do
    {
      SKIP_WHITESPACE ();
      switch (*input_line_pointer)
	{
	case '\"':
	  ++input_line_pointer;
	  start = input_line_pointer;
	  while (is_a_char (c = next_char_of_string ()))
	    {
	      FRAG_APPEND_1_CHAR (c);
	    }

	  if (input_line_pointer[-1] != '\"')
	    {
	      /* We will only get here in rare cases involving #NO_APP,
		 where the unterminated string is not recognized by the
		 preformatting pass.  */
	      as_bad (_("unterminated string"));
	      mmix_discard_rest_of_line ();
	      return;
	    }
	  break;

	default:
	  {
	    expressionS exp;
	    segT expseg = expression (&exp);

	    /* We have to allow special register names as constant numbers.  */
	    if ((expseg != absolute_section && expseg != reg_section)
		|| (exp.X_op != O_constant
		    && (exp.X_op != O_register
			|| exp.X_add_number <= 255)))
	      {
		as_bad (_("BYTE expression not a pure number"));
		mmix_discard_rest_of_line ();
		return;
	      }
	    else if ((exp.X_add_number > 255 && exp.X_op != O_register)
		     || exp.X_add_number < 0)
	      {
		/* Note that mmixal does not allow negative numbers in
		   BYTE sequences, so neither should we.  */
		as_bad (_("BYTE expression not in the range 0..255"));
		mmix_discard_rest_of_line ();
		return;
	      }

	    FRAG_APPEND_1_CHAR (exp.X_add_number);
	  }
	  break;
	}

      SKIP_WHITESPACE ();
      c = *input_line_pointer++;
    }
  while (c == ',');

  input_line_pointer--;

  if (mmix_gnu_syntax)
    demand_empty_rest_of_line ();
  else
    {
      mmix_discard_rest_of_line ();
      /* Do like demand_empty_rest_of_line and step over the end-of-line
         boundary.  */
      input_line_pointer++;
    }

  /* Make sure we align for the next instruction.  */
  last_alignment = 0;
}

/* Like cons_worker, but we have to ignore "naked comments", not barf on
   them.  Implements WYDE, TETRA and OCTA.  We're a little bit more
   lenient than mmix_byte but FIXME: they should eventually merge.  */

static void
mmix_cons (nbytes)
     int nbytes;
{
  expressionS exp;
  char *start;

  /* If we don't have any contents, then it's ok to have a specified start
     address that is not a multiple of the max data size.  We will then
     align it as necessary when we get here.  Otherwise, it's a fatal sin.  */
  if (now_seg == text_section)
    {
      if (lowest_text_loc != (bfd_vma) -1
	  && (lowest_text_loc & (nbytes - 1)) != 0)
	{
	  if (text_has_contents)
	    as_bad (_("data item with alignment larger than location"));
	  else if (want_unaligned)
	    as_bad (_("unaligned data at an absolute location is not supported"));

	  lowest_text_loc &= ~((bfd_vma) nbytes - 1);
	  lowest_text_loc += (bfd_vma) nbytes;
	}

      text_has_contents = 1;
    }
  else if (now_seg == data_section)
    {
      if (lowest_data_loc != (bfd_vma) -1
	  && (lowest_data_loc & (nbytes - 1)) != 0)
	{
	  if (data_has_contents)
	    as_bad (_("data item with alignment larger than location"));
	  else if (want_unaligned)
	    as_bad (_("unaligned data at an absolute location is not supported"));

	  lowest_data_loc &= ~((bfd_vma) nbytes - 1);
	  lowest_data_loc += (bfd_vma) nbytes;
	}

      data_has_contents = 1;
    }

  /* Always align these unless asked not to (valid for the current pseudo).  */
  if (! want_unaligned)
    {
      last_alignment = nbytes == 2 ? 1 : (nbytes == 4 ? 2 : 3);
      frag_align (last_alignment, 0, 0);
      record_alignment (now_seg, last_alignment);
    }

  /* For mmixal compatibility, a label for an instruction (and emitting
     pseudo) refers to the _aligned_ address.  So we have to emit the
     label here.  */
  if (current_fb_label >= 0)
    colon (fb_label_name (current_fb_label, 1));
  else if (pending_label != NULL)
    {
      colon (pending_label);
      pending_label = NULL;
    }

  SKIP_WHITESPACE ();

  if (is_end_of_line[(unsigned int) *input_line_pointer])
    {
      /* Default to zero if the expression was absent.  */

      exp.X_op = O_constant;
      exp.X_add_number = 0;
      exp.X_unsigned = 0;
      exp.X_add_symbol = NULL;
      exp.X_op_symbol = NULL;
      emit_expr (&exp, (unsigned int) nbytes);
    }
  else
    do
      {
	unsigned int c;

	switch (*input_line_pointer)
	  {
	    /* We support strings here too; each character takes up nbytes
	       bytes.  */
	  case '\"':
	    ++input_line_pointer;
	    start = input_line_pointer;
	    while (is_a_char (c = next_char_of_string ()))
	      {
		exp.X_op = O_constant;
		exp.X_add_number = c;
		exp.X_unsigned = 1;
		emit_expr (&exp, (unsigned int) nbytes);
	      }

	    if (input_line_pointer[-1] != '\"')
	      {
		/* We will only get here in rare cases involving #NO_APP,
		   where the unterminated string is not recognized by the
		   preformatting pass.  */
		as_bad (_("unterminated string"));
		mmix_discard_rest_of_line ();
		return;
	      }
	    break;

	  default:
	    {
	      expression (&exp);
	      emit_expr (&exp, (unsigned int) nbytes);
	      SKIP_WHITESPACE ();
	    }
	    break;
	  }
      }
    while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */

  mmix_handle_rest_of_empty_line ();

  /* We don't need to step up the counter for the current_fb_label here;
     that's handled by the caller.  */
}

/* The md_do_align worker.  At present, we just record an alignment to
   nullify the automatic alignment we do for WYDE, TETRA and OCTA, as gcc
   does not use the unaligned macros when attribute packed is used.
   Arguably this is a GCC bug.  */

void
mmix_md_do_align (n, fill, len, max)
     int n;
     char *fill ATTRIBUTE_UNUSED;
     int len ATTRIBUTE_UNUSED;
     int max ATTRIBUTE_UNUSED;
{
  last_alignment = n;
  want_unaligned = n == 0;
}
