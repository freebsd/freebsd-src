/* tc-tic54x.c -- Assembly code for the Texas Instruments TMS320C54X
   Copyright 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Timothy Wall (twall@cygnus.com)

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
   02111-1307, USA.  */

/* Texas Instruments TMS320C54X machine specific gas.
   Written by Timothy Wall (twall@alum.mit.edu).

   Valuable things to do:
   Pipeline conflict warnings
   We encode/decode "ld #_label, dp" differently in relocatable files
     This means we're not compatible with TI output containing those
     expressions.  We store the upper nine bits; TI stores the lower nine
     bits.  How they recover the original upper nine bits is beyond me.

   Tests to add to expect testsuite:
     '=' and '==' with .if, .elseif, and .break

   Incompatibilities (mostly trivial):
   We don't allow '''
   We fill text section with zeroes instead of "nop"s
   We don't convert '' or "" to a single instance
   We don't convert '' to '\0'
   We don't allow strings with .byte/.half/.short/.long
   Probably details of the subsym stuff are different
   TI sets labels to be data type 4 (T_INT); GAS uses T_NULL.

   COFF1 limits section names to 8 characters.
   Some of the default behavior changed from COFF1 to COFF2.  */

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include "as.h"
#include "safe-ctype.h"
#include "sb.h"
#include "macro.h"
#include "subsegs.h"
#include "struc-symbol.h"
#include "opcode/tic54x.h"
#include "obj-coff.h"
#include <math.h>


static struct stag
{
  symbolS *sym;		        /* Symbol for this stag; value is offset.  */
  const char *name;		/* Shortcut to symbol name.  */
  bfd_vma size;		        /* Size of struct/union.  */
  int current_bitfield_offset;  /* Temporary for tracking fields.  */
  int is_union;
  struct stag_field		/* List of fields.  */
  {
    const char *name;
    bfd_vma offset;		/* Of start of this field.  */
    int bitfield_offset;	/* Of start of this field.  */
    struct stag *stag;	        /* If field is struct/union.  */
    struct stag_field *next;
  } *field;
  /* For nesting; used only in stag construction.  */
  struct stag *inner;	        /* Enclosed .struct.  */
  struct stag *outer;	        /* Enclosing .struct.  */
} *current_stag = NULL;

#define MAX_LINE 256 /* Lines longer than this are truncated by TI's asm.  */

typedef struct _tic54x_insn
{
  const template *tm;		/* Opcode template.  */

  char mnemonic[MAX_LINE];	/* Opcode name/mnemonic.  */
  char parmnemonic[MAX_LINE];   /* 2nd mnemonic of parallel insn.  */

  int opcount;
  struct opstruct
  {
    char buf[MAX_LINE];
    enum optype type;
    expressionS exp;
  } operands[MAX_OPERANDS];

  int paropcount;
  struct opstruct paroperands[MAX_OPERANDS];

  int is_lkaddr;
  int lkoperand;
  int words;			/* Size of insn in 16-bit words.  */
  int using_default_dst;	/* Do we need to explicitly set an
				   omitted OP_DST operand?  */
  struct
  {
    unsigned short word;	     /* Final encoded opcode data.  */
    int unresolved;
    int r_nchars;		     /* Relocation size.  */
    bfd_reloc_code_real_type r_type; /* Relocation type.  */
    expressionS addr_expr;	     /* Storage for unresolved expressions.  */
  } opcode[3];
} tic54x_insn;

enum cpu_version
{
  VNONE = 0, V541 = 1, V542 = 2, V543 = 3, V545 = 5, V548 = 8, V549 = 9,
  V545LP = 15, V546LP = 16
};

enum address_mode
{
  c_mode,   /* 16-bit addresses.  */
  far_mode  /* >16-bit addresses.  */
};

static segT stag_saved_seg;
static subsegT stag_saved_subseg;

const char comment_chars[] = ";";
const char line_comment_chars[] = ";*#"; /* At column zero only.  */
const char line_separator_chars[] = ""; /* Not permitted.  */

int emitting_long = 0;

/* Characters which indicate that this is a floating point constant.  */
const char FLT_CHARS[] = "fF";

/* Characters that can be used to separate mantissa from exp in FP
   nums.  */
const char EXP_CHARS[] = "eE";

const char *md_shortopts = "";

#define OPTION_ADDRESS_MODE     (OPTION_MD_BASE)
#define OPTION_CPU_VERSION      (OPTION_ADDRESS_MODE + 1)
#define OPTION_COFF_VERSION     (OPTION_CPU_VERSION + 1)
#define OPTION_STDERR_TO_FILE   (OPTION_COFF_VERSION + 1)

struct option md_longopts[] =
{
  { "mfar-mode",       no_argument,	    NULL, OPTION_ADDRESS_MODE },
  { "mf",	       no_argument,	    NULL, OPTION_ADDRESS_MODE },
  { "mcpu",	       required_argument,   NULL, OPTION_CPU_VERSION },
#if 0
  { "mcoff-version",   required_argument,   NULL, OPTION_COFF_VERSION },
#endif
  { "merrors-to-file", required_argument,   NULL, OPTION_STDERR_TO_FILE },
  { "me",	       required_argument,   NULL, OPTION_STDERR_TO_FILE },
  { NULL,              no_argument,         NULL, 0},
};

size_t md_longopts_size = sizeof (md_longopts);

static int assembly_begun = 0;
/* Addressing mode is not entirely implemented; the latest rev of the Other
   assembler doesn't seem to make any distinction whatsoever; all relocations
   are stored as extended relocatiosn.  Older versions used REL16 vs RELEXT16,
   but now it seems all relocations are RELEXT16.  We use all RELEXT16.

   The cpu version is kind of a waste of time as well.  There is one
   instruction (RND) for LP devices only, and several for devices with
   extended addressing only.  We include it for compatibility.  */
static enum address_mode amode = c_mode;
static enum cpu_version cpu = VNONE;

/* Include string substitutions in listing?  */
static int listing_sslist = 0;

/* Did we do subsym substitutions on the line?  */
static int substitution_line = 0;

/* Last label seen.  */
static symbolS *last_label_seen = NULL;

/* This ensures that all new labels are unique.  */
static int local_label_id;

static struct hash_control *subsym_recurse_hash; /* Prevent infinite recurse.  */
static struct hash_control *math_hash; /* Built-in math functions.  */
/* Allow maximum levels of macro nesting; level 0 is the main substitution
   symbol table.  The other assembler only does 32 levels, so there!  */
static struct hash_control *subsym_hash[100];

/* Keep track of local labels so we can substitute them before GAS sees them
   since macros use their own 'namespace' for local labels, use a separate hash

   We do our own local label handling 'cuz it's subtly different from the
   stock GAS handling.

   We use our own macro nesting counter, since GAS overloads it when expanding
   other things (like conditionals and repeat loops).  */
static int macro_level = 0;
static struct hash_control *local_label_hash[100];
/* Keep track of struct/union tags.  */
static struct hash_control *stag_hash;
static struct hash_control *op_hash;
static struct hash_control *parop_hash;
static struct hash_control *reg_hash;
static struct hash_control *mmreg_hash;
static struct hash_control *cc_hash;
static struct hash_control *cc2_hash;
static struct hash_control *cc3_hash;
static struct hash_control *sbit_hash;
static struct hash_control *misc_symbol_hash;

/* Only word (et al.), align, or conditionals are allowed within
   .struct/.union.  */
#define ILLEGAL_WITHIN_STRUCT()					\
  do								\
    if (current_stag != NULL)					\
      { 							\
	as_bad (_("pseudo-op illegal within .struct/.union"));	\
	return;							\
      }								\
  while (0)

static void	tic54x_emit_char 	PARAMS ((char));
static fragS *	frag_prev 		PARAMS ((fragS *, segT));
static fragS *	bit_offset_frag 	PARAMS ((fragS *, segT));
static int	frag_bit_offset 	PARAMS ((fragS *, segT));
static char *	parse_expression 	PARAMS ((char *, expressionS *));
static void	tic54x_asg 		PARAMS ((int));
static void	tic54x_eval 		PARAMS ((int));
static void	tic54x_bss 		PARAMS ((int));
static void	stag_add_field_symbols	PARAMS ((struct stag *, const char *, bfd_vma, symbolS *, const char *));
static void	stag_add_field 		PARAMS ((struct stag *, const char *, bfd_vma, struct stag *));
static void	tic54x_struct 		PARAMS ((int));
static void	tic54x_endstruct 	PARAMS ((int));
static void	tic54x_tag 		PARAMS ((int));
static void	tic54x_struct_field 	PARAMS ((int));
static void	tic54x_cons 		PARAMS ((int));
static void	tic54x_remove_local_label PARAMS ((const char *, PTR));
static void	tic54x_clear_local_labels PARAMS ((int));
static void	tic54x_sect 		PARAMS ((int));
static void	tic54x_space 		PARAMS ((int));
static void	tic54x_usect 		PARAMS ((int));
static enum cpu_version lookup_version	PARAMS ((const char *));
static void	set_cpu 		PARAMS ((enum cpu_version));
static void	tic54x_version 		PARAMS ((int));
static void	tic54x_float_cons 	PARAMS ((int));
static void	tic54x_stringer 	PARAMS ((int));
static void	tic54x_p2align 		PARAMS ((int));
static void	tic54x_align_words 	PARAMS ((int));
static void	tic54x_field 		PARAMS ((int));
static int	tic54x_initialized_section PARAMS ((segT));
static void	tic54x_clink 		PARAMS ((int));
static void	tic54x_set_default_include PARAMS ((int));
static void	tic54x_include 		PARAMS ((int));
static void	tic54x_message 		PARAMS ((int));
static void	tic54x_label 		PARAMS ((int));
static void	tic54x_mmregs 		PARAMS ((int));
static void	tic54x_loop 		PARAMS ((int));
static void	tic54x_endloop 		PARAMS ((int));
static void	tic54x_break 		PARAMS ((int));
static void	set_address_mode 	PARAMS ((int));
static void	tic54x_address_mode 	PARAMS ((int));
static void	tic54x_sblock 		PARAMS ((int));
static void	tic54x_set 		PARAMS ((int));
static void	tic54x_fclist 		PARAMS ((int));
static void	tic54x_sslist 		PARAMS ((int));
static void	tic54x_var 		PARAMS ((int));
static void	tic54x_mlib 		PARAMS ((int));
static int 	subsym_symlen 		PARAMS ((char *, char *));
static int 	subsym_symcmp 		PARAMS ((char *, char *));
static int 	subsym_firstch 		PARAMS ((char *, char *));
static int 	subsym_lastch 		PARAMS ((char *, char *));
static int 	subsym_isdefed 		PARAMS ((char *, char *));
static int 	subsym_ismember 	PARAMS ((char *, char *));
static int 	subsym_iscons 		PARAMS ((char *, char *));
static int 	subsym_isname 		PARAMS ((char *, char *));
static int 	subsym_isreg 		PARAMS ((char *, char *));
static int 	subsym_structsz 	PARAMS ((char *, char *));
static int 	subsym_structacc 	PARAMS ((char *, char *));
static float 	math_ceil 		PARAMS ((float, float));
static float 	math_cvi 		PARAMS ((float, float));
static float 	math_floor 		PARAMS ((float, float));
static float 	math_fmod 		PARAMS ((float, float));
static float 	math_int 		PARAMS ((float, float));
static float 	math_round 		PARAMS ((float, float));
static float 	math_sgn 		PARAMS ((float, float));
static float 	math_trunc 		PARAMS ((float, float));
static float 	math_acos 		PARAMS ((float, float));
static float 	math_asin 		PARAMS ((float, float));
static float 	math_atan 		PARAMS ((float, float));
static float 	math_atan2 		PARAMS ((float, float));
static float 	math_cosh 		PARAMS ((float, float));
static float 	math_cos 		PARAMS ((float, float));
static float 	math_cvf 		PARAMS ((float, float));
static float 	math_exp 		PARAMS ((float, float));
static float 	math_fabs 		PARAMS ((float, float));
static float 	math_ldexp 		PARAMS ((float, float));
static float 	math_log10 		PARAMS ((float, float));
static float 	math_log 		PARAMS ((float, float));
static float 	math_max 		PARAMS ((float, float));
static float 	math_min 		PARAMS ((float, float));
static float 	math_pow 		PARAMS ((float, float));
static float 	math_sin 		PARAMS ((float, float));
static float 	math_sinh 		PARAMS ((float, float));
static float 	math_sqrt 		PARAMS ((float, float));
static float 	math_tan 		PARAMS ((float, float));
static float 	math_tanh 		PARAMS ((float, float));
static int 	is_accumulator 		PARAMS ((struct opstruct *));
static int 	get_operands 		PARAMS ((struct opstruct operands[], char *));
static int 	is_immediate 		PARAMS ((struct opstruct *));
static int 	is_absolute 		PARAMS ((struct opstruct *));
static int 	is_indirect 		PARAMS ((struct opstruct *));
static int 	is_dual 		PARAMS ((struct opstruct *));
static int 	is_mmreg 		PARAMS ((struct opstruct *));
static int 	is_type 		PARAMS ((struct opstruct *, enum optype));
static int 	operands_match 		PARAMS ((tic54x_insn *, struct opstruct *, int, const enum optype *, int, int));
static int 	encode_dmad 		PARAMS ((tic54x_insn *, struct opstruct *, int));
static int 	encode_address 		PARAMS ((tic54x_insn *, struct opstruct *));
static int 	encode_indirect 	PARAMS ((tic54x_insn *, struct opstruct *));
static int 	encode_integer 		PARAMS ((tic54x_insn *, struct opstruct *, int, int, int, unsigned short));
static int 	encode_condition 	PARAMS ((tic54x_insn *, struct opstruct *));
static int 	encode_cc3 		PARAMS ((tic54x_insn *, struct opstruct *));
static int 	encode_arx 		PARAMS ((tic54x_insn *, struct opstruct *));
static int 	encode_cc2 		PARAMS ((tic54x_insn *, struct opstruct *));
static int 	encode_operand 		PARAMS ((tic54x_insn *, enum optype, struct opstruct *));
static void 	emit_insn 		PARAMS ((tic54x_insn *));
static int 	build_insn 		PARAMS ((tic54x_insn *));
static int 	optimize_insn 		PARAMS ((tic54x_insn *));
static int 	tic54x_parse_insn 	PARAMS ((tic54x_insn *, char *));
static int 	next_line_shows_parallel PARAMS ((char *));
static int 	tic54x_parse_parallel_insn_firstline PARAMS ((tic54x_insn *, char *));
static int 	tic54x_parse_parallel_insn_lastline PARAMS ((tic54x_insn *, char *));
static char *	subsym_get_arg 		PARAMS ((char *, char *, char **, int));
static void	subsym_create_or_replace PARAMS ((char *, char *));
static char *	subsym_lookup 		PARAMS ((char *, int));
static char *	subsym_substitute 	PARAMS ((char *, int));


void
md_show_usage (stream)
     FILE *stream;
{
  fprintf (stream, _("C54x-specific command line  options:\n"));
  fprintf (stream, _("-mfar-mode | -mf          Use extended addressing\n"));
  fprintf (stream, _("-mcpu=<CPU version>       Specify the CPU version\n"));
#if 0
  fprintf (stream, _("-mcoff-version={0|1|2}    Select COFF version\n"));
#endif
  fprintf (stream, _("-merrors-to-file <filename>\n"));
  fprintf (stream, _("-me <filename>            Redirect errors to a file\n"));
}

/* Output a single character (upper octect is zero).  */

static void
tic54x_emit_char (c)
     char c;
{
  expressionS exp;

  exp.X_op = O_constant;
  exp.X_add_number = c;
  emit_expr (&exp, 2);
}

/* Walk backwards in the frag chain.  */

static fragS *
frag_prev (frag, seg)
     fragS *frag;
     segT seg;
{
  segment_info_type *seginfo = seg_info (seg);
  fragS *fragp;

  for (fragp = seginfo->frchainP->frch_root; fragp; fragp = fragp->fr_next)
    if (fragp->fr_next == frag)
      return fragp;

  return NULL;
}

static fragS *
bit_offset_frag (frag, seg)
     fragS *frag;
     segT seg;
{
  while (frag != NULL)
    {
      if (frag->fr_fix == 0
	  && frag->fr_opcode == NULL
	  && frag->tc_frag_data == 0)
	frag = frag_prev (frag, seg);
      else
	return frag;
    }
  return NULL;
}

/* Return the number of bits allocated in the most recent word, or zero if
   none. .field/.space/.bes may leave words partially allocated.  */

static int
frag_bit_offset (frag, seg)
     fragS *frag;
     segT seg;
{
  frag = bit_offset_frag (frag, seg);

  if (frag)
    return frag->fr_opcode != NULL ? -1 : frag->tc_frag_data;

  return 0;
}

/* Read an expression from a C string; returns a pointer past the end of the
   expression.  */

static char *
parse_expression (str, exp)
     char *str;
     expressionS * exp;
{
  char *s;
  char *tmp;

  tmp = input_line_pointer;	/* Save line pointer.  */
  input_line_pointer = str;
  expression (exp);
  s = input_line_pointer;
  input_line_pointer = tmp;	/* Restore line pointer.  */
  return s;			/* Return pointer to where parsing stopped.  */
}

/* .asg "character-string"|character-string, symbol

   .eval is the only pseudo-op allowed to perform arithmetic on substitution
   symbols.  all other use of symbols defined with .asg are currently
   unsupported.  */

static void
tic54x_asg (x)
     int x ATTRIBUTE_UNUSED;
{
  int c;
  char *name;
  char *str;
  char *tmp;
  int quoted = *input_line_pointer == '"';

  ILLEGAL_WITHIN_STRUCT ();

  if (quoted)
    {
      int len;
      str = demand_copy_C_string (&len);
      c = *input_line_pointer;
    }
  else
    {
      str = input_line_pointer;
      while ((c = *input_line_pointer) != ',')
	{
	  if (is_end_of_line[(int) *input_line_pointer])
	    break;
	  ++input_line_pointer;
	}
      *input_line_pointer = 0;
    }
  if (c != ',')
    {
      as_bad (_("Comma and symbol expected for '.asg STRING, SYMBOL'"));
      ignore_rest_of_line ();
      return;
    }

  name = ++input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  if (!ISALPHA (*name))
    {
      as_bad ("symbols assigned with .asg must begin with a letter");
      ignore_rest_of_line ();
      return;
    }

  tmp = xmalloc (strlen (str) + 1);
  strcpy (tmp, str);
  str = tmp;
  tmp = xmalloc (strlen (name) + 1);
  strcpy (tmp, name);
  name = tmp;
  subsym_create_or_replace (name, str);
  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* .eval expression, symbol
   There's something screwy about this.  The other assembler sometimes does and
   sometimes doesn't substitute symbols defined with .eval.
   We'll put the symbols into the subsym table as well as the normal symbol
   table, since that's what works best.  */

static void
tic54x_eval (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  int value;
  char *name;
  symbolS *symbolP;
  char valuestr[32], *tmp;
  int quoted;

  ILLEGAL_WITHIN_STRUCT ();

  SKIP_WHITESPACE ();

  quoted = *input_line_pointer == '"';
  if (quoted)
    ++input_line_pointer;
  value = get_absolute_expression ();
  if (quoted)
    {
      if (*input_line_pointer != '"')
	{
	  as_bad (_("Unterminated string after absolute expression"));
	  ignore_rest_of_line ();
	  return;
	}
      ++input_line_pointer;
    }
  if (*input_line_pointer++ != ',')
    {
      as_bad (_("Comma and symbol expected for '.eval EXPR, SYMBOL'"));
      ignore_rest_of_line ();
      return;
    }
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  tmp = xmalloc (strlen (name) + 1);
  name = strcpy (tmp, name);
  *input_line_pointer = c;

  if (!ISALPHA (*name))
    {
      as_bad (_("symbols assigned with .eval must begin with a letter"));
      ignore_rest_of_line ();
      return;
    }
  symbolP = symbol_new (name, absolute_section,
			(valueT) value, &zero_address_frag);
  SF_SET_LOCAL (symbolP);
  symbol_table_insert (symbolP);

  /* The "other" assembler sometimes doesn't put .eval's in the subsym table
     But since there's not written rule as to when, don't even bother trying
     to match their behavior.  */
  sprintf (valuestr, "%d", value);
  tmp = xmalloc (strlen (valuestr) + 1);
  strcpy (tmp, valuestr);
  subsym_create_or_replace (name, tmp);

  demand_empty_rest_of_line ();
}

/* .bss symbol, size [, [blocking flag] [, alignment flag]

   alignment is to a longword boundary; blocking is to 128-word boundary.

   1) if there is a hole in memory, this directive should attempt to fill it
      (not yet implemented).

   2) if the blocking flag is not set, allocate at the current SPC
      otherwise, check to see if the current SPC plus the space to be
      allocated crosses the page boundary (128 words).
      if there's not enough space, create a hole and align with the next page
      boundary.
      (not yet implemented).  */

static void
tic54x_bss (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *p;
  int words;
  segT current_seg;
  subsegT current_subseg;
  symbolS *symbolP;
  int block = 0;
  int align = 0;

  ILLEGAL_WITHIN_STRUCT ();

  current_seg = now_seg;	/* Save current seg.  */
  current_subseg = now_subseg;	/* Save current subseg.  */

  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  if (c != ',')
    {
      as_bad (".bss size argument missing\n");
      ignore_rest_of_line ();
      return;
    }

  ++input_line_pointer;
  words = get_absolute_expression ();
  if (words < 0)
    {
      as_bad (".bss size %d < 0!", words);
      ignore_rest_of_line ();
      return;
    }

  if (*input_line_pointer == ',')
    {
      /* The blocking flag may be missing.  */
      ++input_line_pointer;
      if (*input_line_pointer != ',')
	block = get_absolute_expression ();
      else
	block = 0;

      if (*input_line_pointer == ',')
	{
	  ++input_line_pointer;
	  align = get_absolute_expression ();
	}
      else
	align = 0;
    }
  else
    block = align = 0;

  subseg_set (bss_section, 0);
  symbolP = symbol_find_or_make (name);

  if (S_GET_SEGMENT (symbolP) == bss_section)
    symbolP->sy_frag->fr_symbol = (symbolS *) NULL;

  symbol_set_frag (symbolP, frag_now);
  p = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP,
		(offsetT) (words * OCTETS_PER_BYTE), (char *) 0);
  *p = 0;			/* Fill char.  */

  S_SET_SEGMENT (symbolP, bss_section);

  /* The symbol may already have been created with a preceding
     ".globl" directive -- be careful not to step on storage class
     in that case.  Otherwise, set it to static.  */
  if (S_GET_STORAGE_CLASS (symbolP) != C_EXT)
    S_SET_STORAGE_CLASS (symbolP, C_STAT);

  if (align)
    {
      /* s_align eats end of line; restore it */
      s_align_bytes (4);
      --input_line_pointer;
    }

  if (block)
    bss_section->flags |= SEC_BLOCK;

  subseg_set (current_seg, current_subseg);	/* Restore current seg.  */
  demand_empty_rest_of_line ();
}

static void
stag_add_field_symbols (stag, path, base_offset, rootsym, root_stag_name)
     struct stag *stag;
     const char *path;
     bfd_vma base_offset;
     symbolS *rootsym;
     const char *root_stag_name;
{
  char prefix[strlen (path) + 2];
  struct stag_field *field = stag->field;

  /* Construct a symbol for every field contained within this structure
     including fields within structure fields.  */
  strcpy (prefix, path);
  if (*path)
    strcat (prefix, ".");

  while (field != NULL)
    {
      int len = strlen (prefix) + strlen (field->name) + 2;
      char *name = xmalloc (len);
      strcpy (name, prefix);
      strcat (name, field->name);

      if (rootsym == NULL)
	{
	  symbolS *sym;
	  sym = symbol_new (name, absolute_section,
			    (field->stag ? field->offset :
			     (valueT) (base_offset + field->offset)),
			    &zero_address_frag);
	  SF_SET_LOCAL (sym);
	  symbol_table_insert (sym);
	}
      else
	{
	  char *replacement = xmalloc (strlen (name)
				       + strlen (stag->name) + 2);
	  strcpy (replacement, S_GET_NAME (rootsym));
	  strcat (replacement, "+");
	  strcat (replacement, root_stag_name);
	  strcat (replacement, name + strlen (S_GET_NAME (rootsym)));
	  hash_insert (subsym_hash[0], name, replacement);
	}

      /* Recurse if the field is a structure.
	 Note the field offset is relative to the outermost struct.  */
      if (field->stag != NULL)
	stag_add_field_symbols (field->stag, name,
				field->offset,
				rootsym, root_stag_name);
      field = field->next;
    }
}

/* Keep track of stag fields so that when structures are nested we can add the
   complete dereferencing symbols to the symbol table.  */

static void
stag_add_field (parent, name, offset, stag)
     struct stag *parent;
     const char *name;
     bfd_vma offset;
     struct stag *stag;
{
  struct stag_field *sfield = xmalloc (sizeof (struct stag_field));

  memset (sfield, 0, sizeof (*sfield));
  sfield->name = strcpy (xmalloc (strlen (name) + 1), name);
  sfield->offset = offset;
  sfield->bitfield_offset = parent->current_bitfield_offset;
  sfield->stag = stag;
  if (parent->field == NULL)
    parent->field = sfield;
  else
    {
      struct stag_field *sf = parent->field;
      while (sf->next != NULL)
	sf = sf->next;
      sf->next = sfield;
    }
  /* Only create a symbol for this field if the parent has no name.  */
  if (!strncmp (".fake", parent->name, 5))
    {
      symbolS *sym = symbol_new (name, absolute_section,
				 (valueT) offset, &zero_address_frag);
      SF_SET_LOCAL (sym);
      symbol_table_insert (sym);
    }
}

/* [STAG] .struct       [OFFSET]
   Start defining structure offsets (symbols in absolute section).  */

static void
tic54x_struct (arg)
     int arg;
{
  int start_offset = 0;
  int is_union = arg;

  if (!current_stag)
    {
      /* Starting a new struct, switch to absolute section.  */
      stag_saved_seg = now_seg;
      stag_saved_subseg = now_subseg;
      subseg_set (absolute_section, 0);
    }
  /* Align the current pointer.  */
  else if (current_stag->current_bitfield_offset != 0)
    {
      ++abs_section_offset;
      current_stag->current_bitfield_offset = 0;
    }

  /* Offset expression is only meaningful for global .structs.  */
  if (!is_union)
    {
      /* Offset is ignored in inner structs.  */
      SKIP_WHITESPACE ();
      if (!is_end_of_line[(int) *input_line_pointer])
	start_offset = get_absolute_expression ();
      else
	start_offset = 0;
    }

  if (current_stag)
    {
      /* Nesting, link to outer one.  */
      current_stag->inner = (struct stag *) xmalloc (sizeof (struct stag));
      memset (current_stag->inner, 0, sizeof (struct stag));
      current_stag->inner->outer = current_stag;
      current_stag = current_stag->inner;
      if (start_offset)
	as_warn (_("Offset on nested structures is ignored"));
      start_offset = abs_section_offset;
    }
  else
    {
      current_stag = (struct stag *) xmalloc (sizeof (struct stag));
      memset (current_stag, 0, sizeof (struct stag));
      abs_section_offset = start_offset;
    }
  current_stag->is_union = is_union;

  if (line_label == NULL)
    {
      static int struct_count = 0;
      char fake[] = ".fake_stagNNNNNNN";
      sprintf (fake, ".fake_stag%d", struct_count++);
      current_stag->sym = symbol_new (fake, absolute_section,
				      (valueT) abs_section_offset,
				      &zero_address_frag);
    }
  else
    {
      char label[strlen (S_GET_NAME (line_label)) + 1];
      strcpy (label, S_GET_NAME (line_label));
      current_stag->sym = symbol_new (label, absolute_section,
				      (valueT) abs_section_offset,
				      &zero_address_frag);
    }
  current_stag->name = S_GET_NAME (current_stag->sym);
  SF_SET_LOCAL (current_stag->sym);
  /* Nested .structs don't go into the symbol table.  */
  if (current_stag->outer == NULL)
    symbol_table_insert (current_stag->sym);

  line_label = NULL;
}

/* [LABEL] .endstruct
   finish defining structure offsets; optional LABEL's value will be the size
   of the structure.  */

static void
tic54x_endstruct (is_union)
     int is_union;
{
  int size;
  const char *path =
    !strncmp (current_stag->name, ".fake", 5) ? "" : current_stag->name;

  if (!current_stag || current_stag->is_union != is_union)
    {
      as_bad (_(".end%s without preceding .%s"),
	      is_union ? "union" : "struct",
	      is_union ? "union" : "struct");
      ignore_rest_of_line ();
      return;
    }

  /* Align end of structures.  */
  if (current_stag->current_bitfield_offset)
    {
      ++abs_section_offset;
      current_stag->current_bitfield_offset = 0;
    }

  if (current_stag->is_union)
    size = current_stag->size;
  else
    size = abs_section_offset - S_GET_VALUE (current_stag->sym);
  if (line_label != NULL)
    {
      S_SET_VALUE (line_label, size);
      symbol_table_insert (line_label);
      line_label = NULL;
    }

  /* Union size has already been calculated.  */
  if (!current_stag->is_union)
    current_stag->size = size;
  /* Nested .structs don't get put in the stag table.  */
  if (current_stag->outer == NULL)
    {
      hash_insert (stag_hash, current_stag->name, current_stag);
      stag_add_field_symbols (current_stag, path,
			      S_GET_VALUE (current_stag->sym),
			      NULL, NULL);
    }
  current_stag = current_stag->outer;

  /* If this is a nested .struct/.union, add it as a field to the enclosing
     one.  otherwise, restore the section we were in.  */
  if (current_stag != NULL)
    {
      stag_add_field (current_stag, current_stag->inner->name,
		      S_GET_VALUE (current_stag->inner->sym),
		      current_stag->inner);
    }
  else
    subseg_set (stag_saved_seg, stag_saved_subseg);
}

/* [LABEL]      .tag    STAG
   Reference a structure within a structure, as a sized field with an optional
   label.
   If used outside of a .struct/.endstruct, overlays the given structure
   format on the existing allocated space.  */

static void
tic54x_tag (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name = input_line_pointer;
  int c = get_symbol_end ();
  struct stag *stag = (struct stag *) hash_find (stag_hash, name);

  if (!stag)
    {
      if (*name)
	as_bad (_("Unrecognized struct/union tag '%s'"), name);
      else
	as_bad (_(".tag requires a structure tag"));
      ignore_rest_of_line ();
      return;
    }
  if (line_label == NULL)
    {
      as_bad (_("Label required for .tag"));
      ignore_rest_of_line ();
      return;
    }
  else
    {
      char label[strlen (S_GET_NAME (line_label)) + 1];

      strcpy (label, S_GET_NAME (line_label));
      if (current_stag != NULL)
	stag_add_field (current_stag, label,
			abs_section_offset - S_GET_VALUE (current_stag->sym),
			stag);
      else
	{
	  symbolS *sym = symbol_find (label);

	  if (!sym)
	    {
	      as_bad (_(".tag target '%s' undefined"), label);
	      ignore_rest_of_line ();
	      return;
	    }
	  stag_add_field_symbols (stag, S_GET_NAME (sym),
				  S_GET_VALUE (stag->sym), sym, stag->name);
	}
    }

  /* Bump by the struct size, but only if we're within a .struct section.  */
  if (current_stag != NULL && !current_stag->is_union)
    abs_section_offset += stag->size;

  *input_line_pointer = c;
  demand_empty_rest_of_line ();
  line_label = NULL;
}

/* Handle all .byte, .char, .double, .field, .float, .half, .int, .long,
   .short, .string, .ubyte, .uchar, .uhalf, .uint, .ulong, .ushort, .uword,
   and .word.  */

static void
tic54x_struct_field (type)
     int type;
{
  int size;
  int count = 1;
  int new_bitfield_offset = 0;
  int field_align = current_stag->current_bitfield_offset != 0;
  int longword_align = 0;

  SKIP_WHITESPACE ();
  if (!is_end_of_line[(int) *input_line_pointer])
    count = get_absolute_expression ();

  switch (type)
    {
    case 'b':
    case 'B':
    case 'c':
    case 'C':
    case 'h':
    case 'H':
    case 'i':
    case 'I':
    case 's':
    case 'S':
    case 'w':
    case 'W':
    case '*': /* String.  */
      size = 1;
      break;
    case 'f':
    case 'l':
    case 'L':
      longword_align = 1;
      size = 2;
      break;
    case '.': /* Bitfield.  */
      size = 0;
      if (count < 1 || count > 32)
	{
	  as_bad (_(".field count '%d' out of range (1 <= X <= 32)"), count);
	  ignore_rest_of_line ();
	  return;
	}
      if (current_stag->current_bitfield_offset + count > 16)
	{
	  /* Set the appropriate size and new field offset.  */
	  if (count == 32)
	    {
	      size = 2;
	      count = 1;
	    }
	  else if (count > 16)
	    {
	      size = 1;
	      count = 1;
	      new_bitfield_offset = count - 16;
	    }
	  else
	    new_bitfield_offset = count;
	}
      else
	{
	  field_align = 0;
	  new_bitfield_offset = current_stag->current_bitfield_offset + count;
	}
      break;
    default:
      as_bad (_("Unrecognized field type '%c'"), type);
      ignore_rest_of_line ();
      return;
    }

  if (field_align)
    {
      /* Align to the actual starting position of the field.  */
      current_stag->current_bitfield_offset = 0;
      ++abs_section_offset;
    }
  /* Align to longword boundary.  */
  if (longword_align && (abs_section_offset & 0x1))
    ++abs_section_offset;

  if (line_label == NULL)
    {
      static int fieldno = 0;
      char fake[] = ".fake_fieldNNNNN";

      sprintf (fake, ".fake_field%d", fieldno++);
      stag_add_field (current_stag, fake,
		      abs_section_offset - S_GET_VALUE (current_stag->sym),
		      NULL);
    }
  else
    {
      char label[strlen (S_GET_NAME (line_label) + 1)];

      strcpy (label, S_GET_NAME (line_label));
      stag_add_field (current_stag, label,
		      abs_section_offset - S_GET_VALUE (current_stag->sym),
		      NULL);
    }

  if (current_stag->is_union)
    {
      /* Note we treat the element as if it were an array of COUNT.  */
      if (current_stag->size < (unsigned) size * count)
	current_stag->size = size * count;
    }
  else
    {
      abs_section_offset += (unsigned) size * count;
      current_stag->current_bitfield_offset = new_bitfield_offset;
    }
  line_label = NULL;
}

/* Handle .byte, .word. .int, .long and all variants.  */

static void
tic54x_cons (type)
     int type;
{
  unsigned int c;
  int octets;

  /* If we're within a .struct construct, don't actually allocate space.  */
  if (current_stag != NULL)
    {
      tic54x_struct_field (type);
      return;
    }

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  generate_lineno_debug ();

  /* Align long words to long word boundaries (4 octets).  */
  if (type == 'l' || type == 'L')
    {
      frag_align (2, 0, 2);
      /* If there's a label, assign it to the first allocated word.  */
      if (line_label != NULL)
	{
	  symbol_set_frag (line_label, frag_now);
	  S_SET_VALUE (line_label, frag_now_fix ());
	}
    }

  switch (type)
    {
    case 'l':
    case 'L':
    case 'x':
      octets = 4;
      break;
    case 'b':
    case 'B':
    case 'c':
    case 'C':
      octets = 1;
      break;
    default:
      octets = 2;
      break;
    }

  do
    {
      if (*input_line_pointer == '"')
	{
	  input_line_pointer++;
	  while (is_a_char (c = next_char_of_string ()))
	    tic54x_emit_char (c);
	  know (input_line_pointer[-1] == '\"');
	}
      else
	{
	  expressionS exp;

	  input_line_pointer = parse_expression (input_line_pointer, &exp);
	  if (exp.X_op == O_constant)
	    {
	      offsetT value = exp.X_add_number;
	      /* Truncate overflows.  */
	      switch (octets)
		{
		case 1:
		  if ((value > 0 && value > 0xFF)
		      || (value < 0 && value < - 0x100))
		    as_warn ("Overflow in expression, truncated to 8 bits");
		  break;
		case 2:
		  if ((value > 0 && value > 0xFFFF)
		      || (value < 0 && value < - 0x10000))
		    as_warn ("Overflow in expression, truncated to 16 bits");
		  break;
		}
	    }
	  if (exp.X_op != O_constant && octets < 2)
	    {
	      /* Disallow .byte with a non constant expression that will
		 require relocation.  */
	      as_bad (_("Relocatable values require at least WORD storage"));
	      ignore_rest_of_line ();
	      return;
	    }

	  if (exp.X_op != O_constant
	      && amode == c_mode
	      && octets == 4)
	    {
	      /* FIXME -- at one point TI tools used to output REL16
		 relocations, but I don't think the latest tools do at all
		 The current tools output extended relocations regardless of
		 the addressing mode (I actually think that ".c_mode" is
		 totally ignored in the latest tools).  */
	      amode = far_mode;
	      emitting_long = 1;
	      emit_expr (&exp, 4);
	      emitting_long = 0;
	      amode = c_mode;
	    }
	  else
	    {
	      emitting_long = octets == 4;
	      emit_expr (&exp, (octets == 1) ? 2 : octets);
	      emitting_long = 0;
	    }
	}
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */
  demand_empty_rest_of_line ();
}

/* .global <symbol>[,...,<symbolN>]
   .def    <symbol>[,...,<symbolN>]
   .ref    <symbol>[,...,<symbolN>]

   These all identify global symbols.

   .def means the symbol is defined in the current module and can be accessed
   by other files.  The symbol should be placed in the symbol table.

   .ref means the symbol is used in the current module but defined in another
   module.  The linker is to resolve this symbol's definition at link time.

   .global should act as a .ref or .def, as needed.

   global, def and ref all have symbol storage classes of C_EXT.

   I can't identify any difference in how the "other" c54x assembler treats
   these, so we ignore the type here.  */

void
tic54x_global (type)
     int type;
{
  char *name;
  int c;
  symbolS *symbolP;

  if (type == 'r')
    as_warn (_("Use of .def/.ref is deprecated.  Use .global instead"));

  ILLEGAL_WITHIN_STRUCT ();

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);

      *input_line_pointer = c;
      S_SET_STORAGE_CLASS (symbolP, C_EXT);
      if (c == ',')
	{
	  input_line_pointer++;
	  if (is_end_of_line[(int) *input_line_pointer])
	    c = *input_line_pointer;
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

/* Remove the symbol from the local label hash lookup.  */

static void
tic54x_remove_local_label (key, value)
     const char *key;
     PTR value ATTRIBUTE_UNUSED;
{
  PTR *elem = hash_delete (local_label_hash[macro_level], key);
  free (elem);
}

/* Reset all local labels.  */

static void
tic54x_clear_local_labels (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  hash_traverse (local_label_hash[macro_level], tic54x_remove_local_label);
}

/* .text
   .data
   .sect "section name"

   Initialized section
   make sure local labels get cleared when changing sections

   ARG is 't' for text, 'd' for data, or '*' for a named section

   For compatibility, '*' sections are SEC_CODE if instructions are
   encountered, or SEC_DATA if not.
*/

static void
tic54x_sect (arg)
     int arg;
{
  ILLEGAL_WITHIN_STRUCT ();

  /* Local labels are cleared when changing sections.  */
  tic54x_clear_local_labels (0);

  if (arg == 't')
    s_text (0);
  else if (arg == 'd')
    s_data (0);
  else
    {
      char *name = NULL;
      int len;

      /* If there are quotes, remove them.  */
      if (*input_line_pointer == '"')
	{
	  name = demand_copy_C_string (&len);
	  demand_empty_rest_of_line ();
	  name = strcpy (xmalloc (len + 10), name);
	}
      else
	{
	  int c;
	  name = input_line_pointer;
	  c = get_symbol_end ();
          len = strlen(name);
	  name = strcpy (xmalloc (len + 10), name);
	  *input_line_pointer = c;
	  demand_empty_rest_of_line ();
	}
      /* Make sure all named initialized sections flagged properly.  If we
         encounter instructions, we'll flag it with SEC_CODE as well.  */
      strcat (name, ",\"w\"\n");
      input_scrub_insert_line (name);
      obj_coff_section (0);

      /* If there was a line label, make sure that it gets assigned the proper
	 section.  This is for compatibility, even though the actual behavior
	 is not explicitly defined.  For consistency, we make .sect behave
	 like .usect, since that is probably what people expect.  */
      if (line_label != NULL)
	{
	  S_SET_SEGMENT (line_label, now_seg);
	  symbol_set_frag (line_label, frag_now);
	  S_SET_VALUE (line_label, frag_now_fix ());
	  if (S_GET_STORAGE_CLASS (line_label) != C_EXT)
	    S_SET_STORAGE_CLASS (line_label, C_LABEL);
	}
    }
}

/* [symbol] .space space_in_bits
   [symbol] .bes space_in_bits
   BES puts the symbol at the *last* word allocated

   cribbed from s_space.  */

static void
tic54x_space (arg)
     int arg;
{
  expressionS exp;
  char *p = 0;
  int octets = 0;
  long words;
  int bits_per_byte = (OCTETS_PER_BYTE * 8);
  int bit_offset = 0;
  symbolS *label = line_label;
  int bes = arg;

  ILLEGAL_WITHIN_STRUCT ();

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  /* Read the bit count.  */
  expression (&exp);

  /* Some expressions are unresolvable until later in the assembly pass;
     postpone until relaxation/fixup.  we also have to postpone if a previous
     partial allocation has not been completed yet.  */
  if (exp.X_op != O_constant || frag_bit_offset (frag_now, now_seg) == -1)
    {
      struct bit_info *bi = xmalloc (sizeof (struct bit_info));
      char *p;

      bi->seg = now_seg;
      bi->type = bes;
      bi->sym = label;
      p = frag_var (rs_machine_dependent,
		    65536 * 2, 1, (relax_substateT) 0,
		    make_expr_symbol (&exp), (offsetT) 0,
		    (char *) bi);
      if (p)
	*p = 0;

      return;
    }

  /* Reduce the required size by any bit offsets currently left over
     from a previous .space/.bes/.field directive.  */
  bit_offset = frag_now->tc_frag_data;
  if (bit_offset != 0 && bit_offset < 16)
    {
      int spare_bits = bits_per_byte - bit_offset;

      if (spare_bits >= exp.X_add_number)
	{
	  /* Don't have to do anything; sufficient bits have already been
	     allocated; just point the label to the right place.  */
	  if (label != NULL)
	    {
	      symbol_set_frag (label, frag_now);
	      S_SET_VALUE (label, frag_now_fix () - 1);
	      label = NULL;
	    }
	  frag_now->tc_frag_data += exp.X_add_number;
	  goto getout;
	}
      exp.X_add_number -= spare_bits;
      /* Set the label to point to the first word allocated, which in this
	 case is the previous word, which was only partially filled.  */
      if (!bes && label != NULL)
	{
	  symbol_set_frag (label, frag_now);
	  S_SET_VALUE (label, frag_now_fix () - 1);
	  label = NULL;
	}
    }
  /* Convert bits to bytes/words and octets, rounding up.  */
  words = ((exp.X_add_number + bits_per_byte - 1) / bits_per_byte);
  /* How many do we have left over?  */
  bit_offset = exp.X_add_number % bits_per_byte;
  octets = words * OCTETS_PER_BYTE;
  if (octets < 0)
    {
      as_warn (_(".space/.bes repeat count is negative, ignored"));
      goto getout;
    }
  else if (octets == 0)
    {
      as_warn (_(".space/.bes repeat count is zero, ignored"));
      goto getout;
    }

  /* If we are in the absolute section, just bump the offset.  */
  if (now_seg == absolute_section)
    {
      abs_section_offset += words;
      if (bes && label != NULL)
	S_SET_VALUE (label, abs_section_offset - 1);
      frag_now->tc_frag_data = bit_offset;
      goto getout;
    }

  if (!need_pass_2)
    p = frag_var (rs_fill, 1, 1,
		  (relax_substateT) 0, (symbolS *) 0,
		  (offsetT) octets, (char *) 0);

  /* Make note of how many bits of this word we've allocated so far.  */
  frag_now->tc_frag_data = bit_offset;

  /* .bes puts label at *last* word allocated.  */
  if (bes && label != NULL)
    {
      symbol_set_frag (label, frag_now);
      S_SET_VALUE (label, frag_now_fix () - 1);
    }

  if (p)
    *p = 0;

 getout:

  demand_empty_rest_of_line ();
}

/* [symbol] .usect "section-name", size-in-words
		   [, [blocking-flag] [, alignment-flag]]

   Uninitialized section.
   Non-zero blocking means that if the section would cross a page (128-word)
   boundary, it will be page-aligned.
   Non-zero alignment aligns on a longword boundary.

   Has no effect on the current section.  */

static void
tic54x_usect (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *section_name;
  char *p;
  segT seg;
  int size, blocking_flag, alignment_flag;
  segT current_seg;
  subsegT current_subseg;
  flagword flags;

  ILLEGAL_WITHIN_STRUCT ();

  current_seg = now_seg;	/* Save current seg.  */
  current_subseg = now_subseg;	/* Save current subseg.  */

  if (*input_line_pointer == '"')
    input_line_pointer++;
  section_name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  input_line_pointer++;		/* Skip null symbol terminator.  */
  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  if (*input_line_pointer == ',')
    ++input_line_pointer;
  else if (c != ',')
    {
      as_bad (_("Missing size argument"));
      ignore_rest_of_line ();
      return;
    }

  size = get_absolute_expression ();

  /* Read a possibly present third argument (blocking flag).  */
  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      if (*input_line_pointer != ',')
	blocking_flag = get_absolute_expression ();
      else
	blocking_flag = 0;

      /* Read a possibly present fourth argument (alignment flag).  */
      if (*input_line_pointer == ',')
	{
	  ++input_line_pointer;
	  alignment_flag = get_absolute_expression ();
	}
      else
	alignment_flag = 0;
    }
  else
    blocking_flag = alignment_flag = 0;

  seg = subseg_new (name, 0);
  flags = bfd_get_section_flags (stdoutput, seg) | SEC_ALLOC;

  if (alignment_flag)
    {
      /* s_align eats end of line; restore it.  */
      s_align_bytes (4);
      --input_line_pointer;
    }

  if (line_label != NULL)
    {
      S_SET_SEGMENT (line_label, seg);
      symbol_set_frag (line_label, frag_now);
      S_SET_VALUE (line_label, frag_now_fix ());
      /* Set scl to label, since that's what TI does.  */
      if (S_GET_STORAGE_CLASS (line_label) != C_EXT)
	S_SET_STORAGE_CLASS (line_label, C_LABEL);
    }

  seg_info (seg)->bss = 1;	/* Uninitialized data.  */

  p = frag_var (rs_fill, 1, 1,
		(relax_substateT) 0, (symbolS *) line_label,
		size * OCTETS_PER_BYTE, (char *) 0);
  *p = 0;

  if (blocking_flag)
    flags |= SEC_BLOCK;

  if (!bfd_set_section_flags (stdoutput, seg, flags))
    as_warn ("Error setting flags for \"%s\": %s", name,
	     bfd_errmsg (bfd_get_error ()));

  subseg_set (current_seg, current_subseg);	/* Restore current seg.  */
  demand_empty_rest_of_line ();
}

static enum cpu_version
lookup_version (ver)
     const char *ver;
{
  enum cpu_version version = VNONE;

  if (ver[0] == '5' && ver[1] == '4')
    {
      if (strlen (ver) == 3
	  && (ver[2] == '1' || ver[2] == '2' || ver[2] == '3'
	      || ver[2] == '5' || ver[2] == '8' || ver[2] == '9'))
	version = ver[2] - '0';
      else if (strlen (ver) == 5
	       && TOUPPER (ver[3]) == 'L'
	       && TOUPPER (ver[4]) == 'P'
	       && (ver[2] == '5' || ver[2] == '6'))
	version = ver[2] - '0' + 10;
    }

  return version;
}

static void
set_cpu (version)
     enum cpu_version version;
{
  cpu = version;
  if (version == V545LP || version == V546LP)
    {
      symbolS *symbolP = symbol_new ("__allow_lp", absolute_section,
				     (valueT) 1, &zero_address_frag);
      SF_SET_LOCAL (symbolP);
      symbol_table_insert (symbolP);
    }
}

/* .version cpu-version
   cpu-version may be one of the following:
   541
   542
   543
   545
   545LP
   546LP
   548
   549

   This is for compatibility only.  It currently has no affect on assembly.  */
static int cpu_needs_set = 1;

static void
tic54x_version (x)
     int x ATTRIBUTE_UNUSED;
{
  enum cpu_version version = VNONE;
  enum cpu_version old_version = cpu;
  int c;
  char *ver;

  ILLEGAL_WITHIN_STRUCT ();

  SKIP_WHITESPACE ();
  ver = input_line_pointer;
  while (!is_end_of_line[(int) *input_line_pointer])
    ++input_line_pointer;
  c = *input_line_pointer;
  *input_line_pointer = 0;

  version = lookup_version (ver);

  if (cpu != VNONE && cpu != version)
    as_warn (_("CPU version has already been set"));

  if (version == VNONE)
    {
      as_bad (_("Unrecognized version '%s'"), ver);
      ignore_rest_of_line ();
      return;
    }
  else if (assembly_begun && version != old_version)
    {
      as_bad (_("Changing of CPU version on the fly not supported"));
      ignore_rest_of_line ();
      return;
    }

  set_cpu (version);

  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* 'f' = float, 'x' = xfloat, 'd' = double, 'l' = ldouble.  */

static void
tic54x_float_cons (type)
     int type;
{
  if (current_stag != 0)
    tic54x_struct_field ('f');

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  /* Align to long word boundary (4 octets) unless it's ".xfloat".  */
  if (type != 'x')
    {
      frag_align (2, 0, 2);
      /* If there's a label, assign it to the first allocated word.  */
      if (line_label != NULL)
	{
	  symbol_set_frag (line_label, frag_now);
	  S_SET_VALUE (line_label, frag_now_fix ());
	}
    }

  float_cons ('f');
}

/* The argument is capitalized if it should be zero-terminated
   's' is normal string with upper 8-bits zero-filled, 'p' is packed.
   Code copied from stringer, and slightly modified so that strings are packed
   and encoded into the correct octets.  */

static void
tic54x_stringer (type)
     int type;
{
  unsigned int c;
  char *start;
  int append_zero = type == 'S' || type == 'P';
  int packed = type == 'p' || type == 'P';
  int last_char = -1; /* Packed strings need two bytes at a time to encode.  */

  if (current_stag != NULL)
    {
      tic54x_struct_field ('*');
      return;
    }

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  c = ',';			/* Do loop.  */
  while (c == ',')
    {
      SKIP_WHITESPACE ();
      switch (*input_line_pointer)
	{
	default:
	  {
	    unsigned short value = get_absolute_expression ();
	    FRAG_APPEND_1_CHAR ( value       & 0xFF);
	    FRAG_APPEND_1_CHAR ((value >> 8) & 0xFF);
	    break;
	  }
	case '\"':
	  ++input_line_pointer;	/* -> 1st char of string.  */
	  start = input_line_pointer;
	  while (is_a_char (c = next_char_of_string ()))
	    {
	      if (!packed)
		{
		  FRAG_APPEND_1_CHAR (c);
		  FRAG_APPEND_1_CHAR (0);
		}
	      else
		{
		  /* Packed strings are filled MS octet first.  */
		  if (last_char == -1)
		    last_char = c;
		  else
		    {
		      FRAG_APPEND_1_CHAR (c);
		      FRAG_APPEND_1_CHAR (last_char);
		      last_char = -1;
		    }
		}
	    }
	  if (append_zero)
	    {
	      if (packed && last_char != -1)
		{
		  FRAG_APPEND_1_CHAR (0);
		  FRAG_APPEND_1_CHAR (last_char);
		  last_char = -1;
		}
	      else
		{
		  FRAG_APPEND_1_CHAR (0);
		  FRAG_APPEND_1_CHAR (0);
		}
	    }
	  know (input_line_pointer[-1] == '\"');
	  break;
	}
      SKIP_WHITESPACE ();
      c = *input_line_pointer;
      if (!is_end_of_line[c])
	++input_line_pointer;
    }

  /* Finish up any leftover packed string.  */
  if (packed && last_char != -1)
    {
      FRAG_APPEND_1_CHAR (0);
      FRAG_APPEND_1_CHAR (last_char);
    }
  demand_empty_rest_of_line ();
}

static void
tic54x_p2align (arg)
     int arg ATTRIBUTE_UNUSED;
{
  as_bad (_("p2align not supported on this target"));
}

static void
tic54x_align_words (arg)
     int arg;
{
  /* Only ".align" with no argument is allowed within .struct/.union.  */
  int count = arg;

  if (!is_end_of_line[(int) *input_line_pointer])
    {
      if (arg == 2)
	as_warn (_("Argument to .even ignored"));
      else
	count = get_absolute_expression ();
    }

  if (current_stag != NULL && arg == 128)
    {
      if (current_stag->current_bitfield_offset != 0)
	{
	  current_stag->current_bitfield_offset = 0;
	  ++abs_section_offset;
	}
      demand_empty_rest_of_line ();
      return;
    }

  ILLEGAL_WITHIN_STRUCT ();

  s_align_bytes (count << 1);
}

/* Initialize multiple-bit fields withing a single word of memory.  */

static void
tic54x_field (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  expressionS exp;
  int size = 16;
  char *p;
  valueT value;
  symbolS *label = line_label;

  if (current_stag != NULL)
    {
      tic54x_struct_field ('.');
      return;
    }

  input_line_pointer = parse_expression (input_line_pointer, &exp);

  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      size = get_absolute_expression ();
      if (size < 1 || size > 32)
	{
	  as_bad (_("Invalid field size, must be from 1 to 32"));
	  ignore_rest_of_line ();
	  return;
	}
    }

  /* Truncate values to the field width.  */
  if (exp.X_op != O_constant)
    {
      /* If the expression value is relocatable, the field size *must*
         be 16.  */
      if (size != 16)
	{
	  as_bad (_("field size must be 16 when value is relocatable"));
	  ignore_rest_of_line ();
	  return;
	}

      frag_now->tc_frag_data = 0;
      emit_expr (&exp, 2);
    }
  else
    {
      unsigned long fmask = (size == 32) ? 0xFFFFFFFF : (1ul << size) - 1;

      value = exp.X_add_number;
      exp.X_add_number &= fmask;
      if (value != (valueT) exp.X_add_number)
	as_warn (_("field value truncated"));
      value = exp.X_add_number;
      /* Bits are stored MS first.  */
      while (size >= 16)
	{
	  frag_now->tc_frag_data = 0;
	  p = frag_more (2);
	  md_number_to_chars (p, (value >> (size - 16)) & 0xFFFF, 2);
	  size -= 16;
	}
      if (size > 0)
	{
	  int bit_offset = frag_bit_offset (frag_now, now_seg);

	  fragS *alloc_frag = bit_offset_frag (frag_now, now_seg);
	  if (bit_offset == -1)
	    {
	      struct bit_info *bi = xmalloc (sizeof (struct bit_info));
	      /* We don't know the previous offset at this time, so store the
		 info we need and figure it out later.  */
	      expressionS size_exp;

	      size_exp.X_op = O_constant;
	      size_exp.X_add_number = size;
	      bi->seg = now_seg;
	      bi->type = TYPE_FIELD;
	      bi->value = value;
	      p = frag_var (rs_machine_dependent,
			    4, 1, (relax_substateT) 0,
			    make_expr_symbol (&size_exp), (offsetT) 0,
			    (char *) bi);
	      goto getout;
	    }
	  else if (bit_offset == 0 || bit_offset + size > 16)
	    {
	      /* Align a new field.  */
	      p = frag_more (2);
	      frag_now->tc_frag_data = 0;
	      alloc_frag = frag_now;
	    }
	  else
	    {
	      /* Put the new value entirely within the existing one.  */
	      p = alloc_frag == frag_now ?
		frag_now->fr_literal + frag_now_fix_octets () - 2 :
		alloc_frag->fr_literal;
	      if (label != NULL)
		{
		  symbol_set_frag (label, alloc_frag);
		  if (alloc_frag == frag_now)
		    S_SET_VALUE (label, frag_now_fix () - 1);
		  label = NULL;
		}
	    }
	  value <<= 16 - alloc_frag->tc_frag_data - size;

	  /* OR in existing value.  */
	  if (alloc_frag->tc_frag_data)
	    value |= ((unsigned short) p[1] << 8) | p[0];
	  md_number_to_chars (p, value, 2);
	  alloc_frag->tc_frag_data += size;
	  if (alloc_frag->tc_frag_data == 16)
	    alloc_frag->tc_frag_data = 0;
	}
    }
 getout:
  demand_empty_rest_of_line ();
}

/* Ideally, we want to check SEC_LOAD and SEC_HAS_CONTENTS, but those aren't
   available yet.  seg_info ()->bss is the next best thing.  */

static int
tic54x_initialized_section (seg)
     segT seg;
{
  return !seg_info (seg)->bss;
}

/* .clink ["section name"]

   Marks the section as conditionally linked (link only if contents are
   referenced elsewhere.
   Without a name, refers to the current initialized section.
   Name is required for uninitialized sections.  */

static void
tic54x_clink (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  segT seg = now_seg;

  ILLEGAL_WITHIN_STRUCT ();

  if (*input_line_pointer == '\"')
    {
      char *section_name = ++input_line_pointer;
      char *name;

      while (is_a_char (next_char_of_string ()))
	;
      know (input_line_pointer[-1] == '\"');
      input_line_pointer[-1] = 0;
      name = xmalloc (input_line_pointer - section_name + 1);
      strcpy (name, section_name);

      seg = bfd_get_section_by_name (stdoutput, name);
      if (seg == NULL)
	{
	  as_bad (_("Unrecognized section '%s'"), section_name);
	  ignore_rest_of_line ();
	  return;
	}
    }
  else
    {
      if (!tic54x_initialized_section (seg))
	{
	  as_bad (_("Current section is unitialized, "
		    "section name required for .clink"));
	  ignore_rest_of_line ();
	  return;
	}
    }

  seg->flags |= SEC_CLINK;

  demand_empty_rest_of_line ();
}

/* Change the default include directory to be the current source file's
   directory, instead of the current working directory.  If DOT is non-zero,
   set to "." instead.  */

static void
tic54x_set_default_include (dot)
     int dot;
{
  char *dir = ".";
  char *tmp = NULL;

  if (!dot)
    {
      char *curfile;
      unsigned lineno;

      as_where (&curfile, &lineno);
      dir = strcpy (xmalloc (strlen (curfile) + 1), curfile);
      tmp = strrchr (dir, '/');
    }
  if (tmp != NULL)
    {
      int len;

      *tmp = '\0';
      len = strlen (dir);
      if (include_dir_count == 0)
	{
	  include_dirs = (char **) xmalloc (sizeof (*include_dirs));
	  include_dir_count = 1;
	}
      include_dirs[0] = dir;
      if (len > include_dir_maxlen)
	include_dir_maxlen = len;
    }
  else if (include_dirs != NULL)
    include_dirs[0] = ".";
}

/* .include "filename" | filename
   .copy    "filename" | filename

   FIXME 'include' file should be omitted from any output listing,
     'copy' should be included in any output listing
   FIXME -- prevent any included files from changing listing (compat only)
   FIXME -- need to include source file directory in search path; what's a
      good way to do this?

   Entering/exiting included/copied file clears all local labels.  */

static void
tic54x_include (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  char newblock[] = " .newblock\n";
  char *filename;
  char *input;
  int len, c = -1;

  ILLEGAL_WITHIN_STRUCT ();

  SKIP_WHITESPACE ();

  if (*input_line_pointer == '"')
    {
      filename = demand_copy_C_string (&len);
      demand_empty_rest_of_line ();
    }
  else
    {
      filename = input_line_pointer;
      while (!is_end_of_line[(int) *input_line_pointer])
	++input_line_pointer;
      c = *input_line_pointer;
      *input_line_pointer = '\0';
      filename = strcpy (xmalloc (strlen (filename) + 1), filename);
      *input_line_pointer = c;
      demand_empty_rest_of_line ();
    }
  /* Insert a partial line with the filename (for the sake of s_include)
     and a .newblock.
     The included file will be inserted before the newblock, so that the
     newblock is executed after the included file is processed.  */
  input = xmalloc (sizeof (newblock) + strlen (filename) + 4);
  sprintf (input, "\"%s\"\n%s", filename, newblock);
  input_scrub_insert_line (input);

  tic54x_clear_local_labels (0);

  tic54x_set_default_include (0);

  s_include (0);
}

static void
tic54x_message (type)
     int type;
{
  char *msg;
  char c;
  int len;

  ILLEGAL_WITHIN_STRUCT ();

  if (*input_line_pointer == '"')
    msg = demand_copy_C_string (&len);
  else
    {
      msg = input_line_pointer;
      while (!is_end_of_line[(int) *input_line_pointer])
	++input_line_pointer;
      c = *input_line_pointer;
      *input_line_pointer = 0;
      msg = strcpy (xmalloc (strlen (msg) + 1), msg);
      *input_line_pointer = c;
    }

  switch (type)
    {
    case 'm':
      as_tsktsk ("%s", msg);
      break;
    case 'w':
      as_warn ("%s", msg);
      break;
    case 'e':
      as_bad ("%s", msg);
      break;
    }

  demand_empty_rest_of_line ();
}

/* .label <symbol>
   Define a special symbol that refers to the loadtime address rather than the
   runtime address within the current section.

   This symbol gets a special storage class so that when it is resolved, it is
   resolved relative to the load address (lma) of the section rather than the
   run address (vma).  */

static void
tic54x_label (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  char *name = input_line_pointer;
  symbolS *symbolP;
  int c;

  ILLEGAL_WITHIN_STRUCT ();

  c = get_symbol_end ();
  symbolP = colon (name);
  S_SET_STORAGE_CLASS (symbolP, C_STATLAB);

  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* .mmregs
   Install all memory-mapped register names into the symbol table as
   absolute local symbols.  */

static void
tic54x_mmregs (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  symbol *sym;

  ILLEGAL_WITHIN_STRUCT ();

  for (sym = (symbol *) mmregs; sym->name; sym++)
    {
      symbolS *symbolP = symbol_new (sym->name, absolute_section,
				     (valueT) sym->value, &zero_address_frag);
      SF_SET_LOCAL (symbolP);
      symbol_table_insert (symbolP);
    }
}

/* .loop [count]
   Count defaults to 1024.  */

static void
tic54x_loop (count)
     int count;
{
  ILLEGAL_WITHIN_STRUCT ();

  SKIP_WHITESPACE ();
  if (!is_end_of_line[(int) *input_line_pointer])
    count = get_absolute_expression ();

  do_repeat (count, "LOOP", "ENDLOOP");
}

/* Normally, endloop gets eaten by the preceding loop.  */

static void
tic54x_endloop (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  as_bad (_("ENDLOOP without corresponding LOOP"));
  ignore_rest_of_line ();
}

/* .break [condition].  */

static void
tic54x_break (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int cond = 1;

  ILLEGAL_WITHIN_STRUCT ();

  SKIP_WHITESPACE ();
  if (!is_end_of_line[(int) *input_line_pointer])
    cond = get_absolute_expression ();

  if (cond)
    end_repeat (substitution_line ? 1 : 0);
}

static void
set_address_mode (mode)
     int mode;
{
  amode = mode;
  if (mode == far_mode)
    {
      symbolS *symbolP = symbol_new ("__allow_far", absolute_section,
				     (valueT) 1, &zero_address_frag);
      SF_SET_LOCAL (symbolP);
      symbol_table_insert (symbolP);
    }
}

static int address_mode_needs_set = 1;

static void
tic54x_address_mode (mode)
     int mode;
{
  if (assembly_begun && amode != (unsigned) mode)
    {
      as_bad (_("Mixing of normal and extended addressing not supported"));
      ignore_rest_of_line ();
      return;
    }
  if (mode == far_mode && cpu != VNONE && cpu != V548 && cpu != V549)
    {
      as_bad (_("Extended addressing not supported on the specified CPU"));
      ignore_rest_of_line ();
      return;
    }

  set_address_mode (mode);
  demand_empty_rest_of_line ();
}

/* .sblock "section"|section [,...,"section"|section]
   Designate initialized sections for blocking.  */

static void
tic54x_sblock (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int c = ',';

  ILLEGAL_WITHIN_STRUCT ();

  while (c == ',')
    {
      segT seg;
      char *name;

      if (*input_line_pointer == '"')
	{
	  int len;

	  name = demand_copy_C_string (&len);
	}
      else
	{
	  char *section_name = input_line_pointer;

	  c = get_symbol_end ();
	  name = xmalloc (strlen (section_name) + 1);
	  strcpy (name, section_name);
	  *input_line_pointer = c;
	}

      seg = bfd_get_section_by_name (stdoutput, name);
      if (seg == NULL)
	{
	  as_bad (_("Unrecognized section '%s'"), name);
	  ignore_rest_of_line ();
	  return;
	}
      else if (!tic54x_initialized_section (seg))
	{
	  as_bad (_(".sblock may be used for initialized sections only"));
	  ignore_rest_of_line ();
	  return;
	}
      seg->flags |= SEC_BLOCK;

      c = *input_line_pointer;
      if (!is_end_of_line[(int) c])
	++input_line_pointer;
    }

  demand_empty_rest_of_line ();
}

/* symbol .set value
   symbol .equ value

   value must be defined externals; no forward-referencing allowed
   symbols assigned with .set/.equ may not be redefined.  */

static void
tic54x_set (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  symbolS *symbolP;
  char *name;

  ILLEGAL_WITHIN_STRUCT ();

  if (!line_label)
    {
      as_bad (_("Symbol missing for .set/.equ"));
      ignore_rest_of_line ();
      return;
    }
  name = xstrdup (S_GET_NAME (line_label));
  line_label = NULL;
  if ((symbolP = symbol_find (name)) == NULL
      && (symbolP = md_undefined_symbol (name)) == NULL)
    {
      symbolP = symbol_new (name, absolute_section, 0, &zero_address_frag);
      S_SET_STORAGE_CLASS (symbolP, C_STAT);
    }
  free (name);
  S_SET_DATA_TYPE (symbolP, T_INT);
  S_SET_SEGMENT (symbolP, absolute_section);
  symbol_table_insert (symbolP);
  pseudo_set (symbolP);
  demand_empty_rest_of_line ();
}

/* .fclist
   .fcnolist
   List false conditional blocks.  */

static void
tic54x_fclist (show)
     int show;
{
  if (show)
    listing &= ~LISTING_NOCOND;
  else
    listing |= LISTING_NOCOND;
  demand_empty_rest_of_line ();
}

static void
tic54x_sslist (show)
     int show;
{
  ILLEGAL_WITHIN_STRUCT ();

  listing_sslist = show;
}

/* .var SYM[,...,SYMN]
   Define a substitution string to be local to a macro.  */

static void
tic54x_var (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  static char empty[] = "";
  char *name;
  int c;

  ILLEGAL_WITHIN_STRUCT ();

  if (macro_level == 0)
    {
      as_bad (_(".var may only be used within a macro definition"));
      ignore_rest_of_line ();
      return;
    }
  do
    {
      if (!ISALPHA (*input_line_pointer))
	{
	  as_bad (_("Substitution symbols must begin with a letter"));
	  ignore_rest_of_line ();
	  return;
	}
      name = input_line_pointer;
      c = get_symbol_end ();
      /* .var symbols start out with a null string.  */
      name = strcpy (xmalloc (strlen (name) + 1), name);
      hash_insert (subsym_hash[macro_level], name, empty);
      *input_line_pointer = c;
      if (c == ',')
	{
	  ++input_line_pointer;
	  if (is_end_of_line[(int) *input_line_pointer])
	    c = *input_line_pointer;
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

/* .mlib <macro library filename>

   Macro libraries are archived (standard AR-format) text macro definitions
   Expand the file and include it.

   FIXME need to try the source file directory as well.  */

static void
tic54x_mlib (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *filename;
  char *path;
  int len, i;
  bfd *abfd, *mbfd;

  ILLEGAL_WITHIN_STRUCT ();

  /* Parse the filename.  */
  if (*input_line_pointer == '"')
    {
      if ((filename = demand_copy_C_string (&len)) == NULL)
	return;
    }
  else
    {
      SKIP_WHITESPACE ();
      len = 0;
      while (!is_end_of_line[(int) *input_line_pointer]
	     && !ISSPACE (*input_line_pointer))
	{
	  obstack_1grow (&notes, *input_line_pointer);
	  ++input_line_pointer;
	  ++len;
	}
      obstack_1grow (&notes, '\0');
      filename = obstack_finish (&notes);
    }
  demand_empty_rest_of_line ();

  tic54x_set_default_include (0);
  path = xmalloc ((unsigned long) len + include_dir_maxlen + 5);

  for (i = 0; i < include_dir_count; i++)
    {
      FILE *try;

      strcpy (path, include_dirs[i]);
      strcat (path, "/");
      strcat (path, filename);
      if ((try = fopen (path, "r")) != NULL)
	{
	  fclose (try);
	  break;
	}
    }

  if (i >= include_dir_count)
    {
      free (path);
      path = filename;
    }

  /* FIXME: if path is found, malloc'd storage is not freed.  Of course, this
     happens all over the place, and since the assembler doesn't usually keep
     running for a very long time, it really doesn't matter.  */
  register_dependency (path);

  /* Expand all archive entries to temporary files and include them.  */
  abfd = bfd_openr (path, NULL);
  if (!abfd)
    {
      as_bad (_("Can't open macro library file '%s' for reading."), path);
      as_perror ("%s", path);
      ignore_rest_of_line ();
      return;
    }
  if (!bfd_check_format (abfd, bfd_archive))
    {
      as_bad (_("File '%s' not in macro archive format"), path);
      ignore_rest_of_line ();
      return;
    }

  /* Open each BFD as binary (it should be straight ASCII text).  */
  for (mbfd = bfd_openr_next_archived_file (abfd, NULL);
       mbfd != NULL; mbfd = bfd_openr_next_archived_file (abfd, mbfd))
    {
      /* Get a size at least as big as the archive member.  */
      bfd_size_type size = bfd_get_size (mbfd);
      char *buf = xmalloc (size);
      char *fname = tmpnam (NULL);
      FILE *ftmp;

      /* We're not sure how big it is, but it will be smaller than "size".  */
      bfd_bread (buf, size, mbfd);

      /* Write to a temporary file, then use s_include to include it
	 a bit of a hack.  */
      ftmp = fopen (fname, "w+b");
      fwrite ((void *) buf, size, 1, ftmp);
      if (buf[size - 1] != '\n')
	fwrite ("\n", 1, 1, ftmp);
      fclose (ftmp);
      free (buf);
      input_scrub_insert_file (fname);
      unlink (fname);
    }
}

const pseudo_typeS md_pseudo_table[] =
{
  { "algebraic", s_ignore                 ,          0 },
  { "align"    , tic54x_align_words       ,        128 },
  { "ascii"    , tic54x_stringer          ,        'p' },
  { "asciz"    , tic54x_stringer          ,        'P' },
  { "even"     , tic54x_align_words       ,          2 },
  { "asg"      , tic54x_asg               ,          0 },
  { "eval"     , tic54x_eval              ,          0 },
  { "bss"      , tic54x_bss               ,          0 },
  { "byte"     , tic54x_cons              ,        'b' },
  { "ubyte"    , tic54x_cons              ,        'B' },
  { "char"     , tic54x_cons              ,        'c' },
  { "uchar"    , tic54x_cons              ,        'C' },
  { "clink"    , tic54x_clink             ,          0 },
  { "c_mode"   , tic54x_address_mode      ,     c_mode },
  { "copy"     , tic54x_include           ,        'c' },
  { "include"  , tic54x_include           ,        'i' },
  { "data"     , tic54x_sect              ,        'd' },
  { "double"   , tic54x_float_cons        ,        'd' },
  { "ldouble"  , tic54x_float_cons        ,        'l' },
  { "drlist"   , s_ignore                 ,          0 },
  { "drnolist" , s_ignore                 ,          0 },
  { "emsg"     , tic54x_message           ,        'e' },
  { "mmsg"     , tic54x_message           ,        'm' },
  { "wmsg"     , tic54x_message           ,        'w' },
#if 0
  { "end"      , s_end                    ,          0 },
#endif
  { "far_mode" , tic54x_address_mode      ,   far_mode },
  { "fclist"   , tic54x_fclist            ,          1 },
  { "fcnolist" , tic54x_fclist            ,          0 },
  { "field"    , tic54x_field             ,         -1 },
  { "float"    , tic54x_float_cons        ,        'f' },
  { "xfloat"   , tic54x_float_cons        ,        'x' },
  { "global"   , tic54x_global            ,        'g' },
  { "def"      , tic54x_global            ,        'd' },
  { "ref"      , tic54x_global            ,        'r' },
  { "half"     , tic54x_cons              ,        'h' },
  { "uhalf"    , tic54x_cons              ,        'H' },
  { "short"    , tic54x_cons              ,        's' },
  { "ushort"   , tic54x_cons              ,        'S' },
  { "if"       , s_if                     , (int) O_ne },
  { "elseif"   , s_elseif                 , (int) O_ne },
  { "else"     , s_else                   ,          0 },
  { "endif"    , s_endif                  ,          0 },
  { "int"      , tic54x_cons              ,        'i' },
  { "uint"     , tic54x_cons              ,        'I' },
  { "word"     , tic54x_cons              ,        'w' },
  { "uword"    , tic54x_cons              ,        'W' },
  { "label"    , tic54x_label             ,          0 }, /* Loadtime
                                                             address.  */
  { "length"   , s_ignore                 ,          0 },
  { "width"    , s_ignore                 ,          0 },
#if 0
  { "list"     , listing_list             ,          1 },
  { "nolist"   , listing_list             ,          0 },
#endif
  { "long"     , tic54x_cons              ,        'l' },
  { "ulong"    , tic54x_cons              ,        'L' },
  { "xlong"    , tic54x_cons              ,        'x' },
  { "loop"     , tic54x_loop              ,       1024 },
  { "break"    , tic54x_break             ,          0 },
  { "endloop"  , tic54x_endloop           ,          0 },
  { "mlib"     , tic54x_mlib              ,          0 },
  { "mlist"    , s_ignore                 ,          0 },
  { "mnolist"  , s_ignore                 ,          0 },
  { "mmregs"   , tic54x_mmregs            ,          0 },
  { "newblock" , tic54x_clear_local_labels,          0 },
  { "option"   , s_ignore                 ,          0 },
  { "p2align"  , tic54x_p2align           ,          0 },
#if 0
  { "page"     , listing_eject            ,          0 },
#endif
  { "sblock"   , tic54x_sblock            ,          0 },
  { "sect"     , tic54x_sect              ,        '*' },
  { "set"      , tic54x_set               ,          0 },
  { "equ"      , tic54x_set               ,          0 },
  { "space"    , tic54x_space             ,          0 },
  { "bes"      , tic54x_space             ,          1 },
  { "sslist"   , tic54x_sslist            ,          1 },
  { "ssnolist" , tic54x_sslist            ,          0 },
  { "string"   , tic54x_stringer          ,        's' },
  { "pstring"  , tic54x_stringer          ,        'p' },
  { "struct"   , tic54x_struct            ,          0 },
  { "tag"      , tic54x_tag               ,          0 },
  { "endstruct", tic54x_endstruct         ,          0 },
  { "tab"      , s_ignore                 ,          0 },
  { "text"     , tic54x_sect              ,        't' },
#if 0
  { "title"    , listing_title            ,          0 },
#endif
  { "union"    , tic54x_struct            ,          1 },
  { "endunion" , tic54x_endstruct         ,          1 },
  { "usect"    , tic54x_usect             ,          0 },
  { "var"      , tic54x_var               ,          0 },
  { "version"  , tic54x_version           ,          0 },
  {0           , 0                        ,          0 }
};

#if 0
/* For debugging, strings for each operand type.  */
static const char *optypes[] =
{
  "none", "Xmem", "Ymem", "pmad", "dmad", "Smem", "Lmem", "MMR", "PA",
  "Sind", "xpmad", "xpmad+", "MMRX", "MMRY",
  "SRC1", "SRC", "RND", "DST",
  "ARX",
  "SHIFT", "SHFT",
  "B", "A", "lk", "TS", "k8", "16", "BITC", "CC", "CC2", "CC3", "123", "031",
  "k5", "k8u", "ASM", "T", "DP", "ARP", "k3", "lku", "N", "SBIT", "12",
  "k9", "TRN",
};
#endif

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    default:
      return 0;
    case OPTION_COFF_VERSION:
      {
	int version = atoi (arg);

	if (version != 0 && version != 1 && version != 2)
	  as_fatal (_("Bad COFF version '%s'"), arg);
	/* FIXME -- not yet implemented.  */
	break;
      }
    case OPTION_CPU_VERSION:
      {
	cpu = lookup_version (arg);
	cpu_needs_set = 1;
	if (cpu == VNONE)
	  as_fatal (_("Bad CPU version '%s'"), arg);
	break;
      }
    case OPTION_ADDRESS_MODE:
      amode = far_mode;
      address_mode_needs_set = 1;
      break;
    case OPTION_STDERR_TO_FILE:
      {
	char *filename = arg;
	FILE *fp = fopen (filename, "w+");

	if (fp == NULL)
	  as_fatal (_("Can't redirect stderr to the file '%s'"), filename);
	fclose (fp);
	if ((fp = freopen (filename, "w+", stderr)) == NULL)
	  as_fatal (_("Can't redirect stderr to the file '%s'"), filename);
	break;
      }
    }

  return 1;
}

/* Create a "local" substitution string hash table for a new macro level
   Some docs imply that macros have to use .newblock in order to be able
   to re-use a local label.  We effectively do an automatic .newblock by
   deleting the local label hash between macro invocations.  */

void
tic54x_macro_start ()
{
  ++macro_level;
  subsym_hash[macro_level] = hash_new ();
  local_label_hash[macro_level] = hash_new ();
}

void
tic54x_macro_info (info)
     void *info;
{
  struct formal_struct
  {
    struct formal_struct *next;	/* Next formal in list  */
    sb name;			/* Name of the formal  */
    sb def;			/* The default value  */
    sb actual;			/* The actual argument (changed on
                                   each expansion) */
    int index;			/* The index of the formal
                                   0 .. formal_count - 1 */
  } *entry;
  struct macro_struct
  {
    sb sub;			/* Substitution text.  */
    int formal_count;		/* Number of formal args.  */
    struct formal_struct *formals;	/* Pointer to list of
                                           formal_structs.  */
    struct hash_control *formal_hash; /* Hash table of formals.  */
  } *macro;

  macro = (struct macro_struct *) info;

  /* Put the formal args into the substitution symbol table.  */
  for (entry = macro->formals; entry; entry = entry->next)
    {
      char *name = strncpy (xmalloc (entry->name.len + 1),
			    entry->name.ptr, entry->name.len);
      char *value = strncpy (xmalloc (entry->actual.len + 1),
			     entry->actual.ptr, entry->actual.len);

      name[entry->name.len] = '\0';
      value[entry->actual.len] = '\0';
      hash_insert (subsym_hash[macro_level], name, value);
    }
}

/* Get rid of this macro's .var's, arguments, and local labels.  */

void
tic54x_macro_end ()
{
  hash_die (subsym_hash[macro_level]);
  subsym_hash[macro_level] = NULL;
  hash_die (local_label_hash[macro_level]);
  local_label_hash[macro_level] = NULL;
  --macro_level;
}

static int
subsym_symlen (a, ignore)
     char *a;
     char *ignore ATTRIBUTE_UNUSED;
{
  return strlen (a);
}

/* Compare symbol A to string B.  */

static int
subsym_symcmp (a, b)
     char *a;
     char *b;
{
  return strcmp (a, b);
}

/* Return the index of the first occurrence of B in A, or zero if none
   assumes b is an integer char value as a string.  Index is one-based.  */

static int
subsym_firstch (a, b)
     char *a;
     char *b;
{
  int val = atoi (b);
  char *tmp = strchr (a, val);

  return tmp ? tmp - a + 1 : 0;
}

/* Similar to firstch, but returns index of last occurrence of B in A.  */

static int
subsym_lastch (a, b)
     char *a;
     char *b;
{
  int val = atoi (b);
  char *tmp = strrchr (a, val);

  return tmp ? tmp - a + 1 : 0;
}

/* Returns 1 if string A is defined in the symbol table (NOT the substitution
   symbol table).  */

static int
subsym_isdefed (a, ignore)
     char *a;
     char *ignore ATTRIBUTE_UNUSED;
{
  symbolS *symbolP = symbol_find (a);

  return symbolP != NULL;
}

/* Assign first member of comma-separated list B (e.g. "1,2,3") to the symbol
   A, or zero if B is a null string.  Both arguments *must* be substitution
   symbols, unsubstituted.  */

static int
subsym_ismember (sym, list)
     char *sym;
     char *list;
{
  char *elem, *ptr, *listv;

  if (!list)
    return 0;

  listv = subsym_lookup (list, macro_level);
  if (!listv)
    {
      as_bad (_("Undefined substitution symbol '%s'"), list);
      ignore_rest_of_line ();
      return 0;
    }

  ptr = elem = xmalloc (strlen (listv) + 1);
  strcpy (elem, listv);
  while (*ptr && *ptr != ',')
    ++ptr;
  *ptr++ = 0;

  subsym_create_or_replace (sym, elem);

  /* Reassign the list.  */
  subsym_create_or_replace (list, ptr);

  /* Assume this value, docs aren't clear.  */
  return *list != 0;
}

/* Return zero if not a constant; otherwise:
   1 if binary
   2 if octal
   3 if hexadecimal
   4 if character
   5 if decimal.  */

static int
subsym_iscons (a, ignore)
     char *a;
     char *ignore ATTRIBUTE_UNUSED;
{
  expressionS exp;

  parse_expression (a, &exp);

  if (exp.X_op == O_constant)
    {
      int len = strlen (a);

      switch (TOUPPER (a[len - 1]))
	{
	case 'B':
	  return 1;
	case 'Q':
	  return 2;
	case 'H':
	  return 3;
	case '\'':
	  return 4;
	default:
	  break;
	}
      /* No suffix; either octal, hex, or decimal.  */
      if (*a == '0' && len > 1)
	{
	  if (TOUPPER (a[1]) == 'X')
	    return 3;
	  return 2;
	}
      return 5;
    }

  return 0;
}

/* Return 1 if A is a valid symbol name.  Expects string input.   */

static int
subsym_isname (a, ignore)
     char *a;
     char *ignore ATTRIBUTE_UNUSED;
{
  if (!is_name_beginner (*a))
    return 0;
  while (*a)
    {
      if (!is_part_of_name (*a))
	return 0;
      ++a;
    }
  return 1;
}

/* Return whether the string is a register; accepts ar0-7, unless .mmregs has
   been seen; if so, recognize any memory-mapped register.
   Note this does not recognize "A" or "B" accumulators.  */

static int
subsym_isreg (a, ignore)
     char *a;
     char *ignore ATTRIBUTE_UNUSED;
{
  if (hash_find (reg_hash, a))
    return 1;
  if (hash_find (mmreg_hash, a))
    return 1;
  return 0;
}

/* Return the structure size, given the stag.  */

static int
subsym_structsz (name, ignore)
     char *name;
     char *ignore ATTRIBUTE_UNUSED;
{
  struct stag *stag = (struct stag *) hash_find (stag_hash, name);

  if (stag)
    return stag->size;

  return 0;
}

/* If anybody actually uses this, they can fix it :)
   FIXME I'm not sure what the "reference point" of a structure is.  It might
   be either the initial offset given .struct, or it may be the offset of the
   structure within another structure, or it might be something else
   altogether.  since the TI assembler doesn't seem to ever do anything but
   return zero, we punt and return zero.  */

static int
subsym_structacc (stag_name, ignore)
     char *stag_name ATTRIBUTE_UNUSED;
     char *ignore ATTRIBUTE_UNUSED;
{
  return 0;
}

static float
math_ceil (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) ceil (arg1);
}

static float
math_cvi (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (int) arg1;
}

static float
math_floor (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) floor (arg1);
}

static float
math_fmod (arg1, arg2)
     float arg1;
     float arg2;
{
  return (int) arg1 % (int) arg2;
}

static float
math_int (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return ((float) ((int) arg1)) == arg1;
}

static float
math_round (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return arg1 > 0 ? (int) (arg1 + 0.5) : (int) (arg1 - 0.5);
}

static float
math_sgn (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (arg1 < 0) ? -1 : (arg1 ? 1 : 0);
}

static float
math_trunc (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (int) arg1;
}

static float
math_acos (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) acos (arg1);
}

static float
math_asin (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) asin (arg1);
}

static float
math_atan (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) atan (arg1);
}

static float
math_atan2 (arg1, arg2)
     float arg1;
     float arg2;
{
  return (float) atan2 (arg1, arg2);
}

static float
math_cosh (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) cosh (arg1);
}

static float
math_cos (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) cos (arg1);
}

static float
math_cvf (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) arg1;
}

static float
math_exp (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) exp (arg1);
}

static float
math_fabs (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) fabs (arg1);
}

/* expr1 * 2^expr2.  */

static float
math_ldexp (arg1, arg2)
     float arg1;
     float arg2;
{
  return arg1 * (float) pow (2.0, arg2);
}

static float
math_log10 (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) log10 (arg1);
}

static float
math_log (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) log (arg1);
}

static float
math_max (arg1, arg2)
     float arg1;
     float arg2;
{
  return (arg1 > arg2) ? arg1 : arg2;
}

static float
math_min (arg1, arg2)
     float arg1;
     float arg2;
{
  return (arg1 < arg2) ? arg1 : arg2;
}

static float
math_pow (arg1, arg2)
     float arg1;
     float arg2;
{
  return (float) pow (arg1, arg2);
}

static float
math_sin (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) sin (arg1);
}

static float
math_sinh (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) sinh (arg1);
}

static float
math_sqrt (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) sqrt (arg1);
}

static float
math_tan (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) tan (arg1);
}

static float
math_tanh (arg1, ignore)
     float arg1;
     float ignore ATTRIBUTE_UNUSED;
{
  return (float) tanh (arg1);
}

/* Built-in substitution symbol functions and math functions.  */
typedef struct
{
  char *name;
  int (*proc) PARAMS ((char *, char *));
  int nargs;
} subsym_proc_entry;

static const subsym_proc_entry subsym_procs[] =
{
  /* Assembler built-in string substitution functions.  */
  { "$symlen", subsym_symlen, 1,  },
  { "$symcmp", subsym_symcmp, 2,  },
  { "$firstch", subsym_firstch, 2,  },
  { "$lastch", subsym_lastch, 2,  },
  { "$isdefed", subsym_isdefed, 1,  },
  { "$ismember", subsym_ismember, 2,  },
  { "$iscons", subsym_iscons, 1,  },
  { "$isname", subsym_isname, 1,  },
  { "$isreg", subsym_isreg, 1,  },
  { "$structsz", subsym_structsz, 1,  },
  { "$structacc", subsym_structacc, 1,  },
  { NULL, NULL, 0 },
};

typedef struct
{
  char *name;
  float (*proc) PARAMS ((float, float));
  int nargs;
  int int_return;
} math_proc_entry;

static const math_proc_entry math_procs[] =
{
  /* Integer-returning built-in math functions.  */
  { "$cvi", math_cvi, 1, 1 },
  { "$int", math_int, 1, 1 },
  { "$sgn", math_sgn, 1, 1 },

  /* Float-returning built-in math functions.  */
  { "$acos", math_acos, 1, 0 },
  { "$asin", math_asin, 1, 0 },
  { "$atan", math_atan, 1, 0 },
  { "$atan2", math_atan2, 2, 0 },
  { "$ceil", math_ceil, 1, 0 },
  { "$cosh", math_cosh, 1, 0 },
  { "$cos", math_cos, 1, 0 },
  { "$cvf", math_cvf, 1, 0 },
  { "$exp", math_exp, 1, 0 },
  { "$fabs", math_fabs, 1, 0 },
  { "$floor", math_floor, 1, 0 },
  { "$fmod", math_fmod, 2, 0 },
  { "$ldexp", math_ldexp, 2, 0 },
  { "$log10", math_log10, 1, 0 },
  { "$log", math_log, 1, 0 },
  { "$max", math_max, 2, 0 },
  { "$min", math_min, 2, 0 },
  { "$pow", math_pow, 2, 0 },
  { "$round", math_round, 1, 0 },
  { "$sin", math_sin, 1, 0 },
  { "$sinh", math_sinh, 1, 0 },
  { "$sqrt", math_sqrt, 1, 0 },
  { "$tan", math_tan, 1, 0 },
  { "$tanh", math_tanh, 1, 0 },
  { "$trunc", math_trunc, 1, 0 },
  { NULL, NULL, 0, 0 },
};

void
md_begin ()
{
  template *tm;
  symbol *sym;
  const subsym_proc_entry *subsym_proc;
  const math_proc_entry *math_proc;
  const char *hash_err;
  char **symname;
  char *TIC54X_DIR = getenv ("TIC54X_DIR");
  char *A_DIR = TIC54X_DIR ? TIC54X_DIR : getenv ("A_DIR");

  local_label_id = 0;

  /* Look for A_DIR and add it to the include list.  */
  if (A_DIR != NULL)
    {
      char *tmp = xstrdup (A_DIR);

      do
	{
	  char *next = strchr (tmp, ';');

	  if (next)
	    *next++ = '\0';
	  add_include_dir (tmp);
	  tmp = next;
	}
      while (tmp != NULL);
    }

  op_hash = hash_new ();
  for (tm = (template *) tic54x_optab; tm->name; tm++)
    {
      if (hash_find (op_hash, tm->name))
	continue;
      hash_err = hash_insert (op_hash, tm->name, (char *) tm);
      if (hash_err)
	as_fatal ("Internal Error: Can't hash %s: %s",
		  tm->name, hash_err);
    }
  parop_hash = hash_new ();
  for (tm = (template *) tic54x_paroptab; tm->name; tm++)
    {
      if (hash_find (parop_hash, tm->name))
	continue;
      hash_err = hash_insert (parop_hash, tm->name, (char *) tm);
      if (hash_err)
	as_fatal ("Internal Error: Can't hash %s: %s",
		  tm->name, hash_err);
    }
  reg_hash = hash_new ();
  for (sym = (symbol *) regs; sym->name; sym++)
    {
      /* Add basic registers to the symbol table.  */
      symbolS *symbolP = symbol_new (sym->name, absolute_section,
				     (valueT) sym->value, &zero_address_frag);
      SF_SET_LOCAL (symbolP);
      symbol_table_insert (symbolP);
      hash_err = hash_insert (reg_hash, sym->name, (char *) sym);
    }
  for (sym = (symbol *) mmregs; sym->name; sym++)
    hash_err = hash_insert (reg_hash, sym->name, (char *) sym);
  mmreg_hash = hash_new ();
  for (sym = (symbol *) mmregs; sym->name; sym++)
    hash_err = hash_insert (mmreg_hash, sym->name, (char *) sym);

  cc_hash = hash_new ();
  for (sym = (symbol *) condition_codes; sym->name; sym++)
    hash_err = hash_insert (cc_hash, sym->name, (char *) sym);

  cc2_hash = hash_new ();
  for (sym = (symbol *) cc2_codes; sym->name; sym++)
    hash_err = hash_insert (cc2_hash, sym->name, (char *) sym);

  cc3_hash = hash_new ();
  for (sym = (symbol *) cc3_codes; sym->name; sym++)
    hash_err = hash_insert (cc3_hash, sym->name, (char *) sym);

  sbit_hash = hash_new ();
  for (sym = (symbol *) status_bits; sym->name; sym++)
    hash_err = hash_insert (sbit_hash, sym->name, (char *) sym);

  misc_symbol_hash = hash_new ();
  for (symname = (char **) misc_symbols; *symname; symname++)
    hash_err = hash_insert (misc_symbol_hash, *symname, *symname);

  /* Only the base substitution table and local label table are initialized;
     the others (for local macro substitution) get instantiated as needed.  */
  local_label_hash[0] = hash_new ();
  subsym_hash[0] = hash_new ();
  for (subsym_proc = subsym_procs; subsym_proc->name; subsym_proc++)
    hash_err = hash_insert (subsym_hash[0], subsym_proc->name,
			    (char *) subsym_proc);

  math_hash = hash_new ();
  for (math_proc = math_procs; math_proc->name; math_proc++)
    {
      /* Insert into the main subsym hash for recognition; insert into
	 the math hash to actually store information.  */
      hash_err = hash_insert (subsym_hash[0], math_proc->name,
			      (char *) math_proc);
      hash_err = hash_insert (math_hash, math_proc->name,
			      (char *) math_proc);
    }
  subsym_recurse_hash = hash_new ();
  stag_hash = hash_new ();
}

static int
is_accumulator (operand)
     struct opstruct *operand;
{
  return strcasecmp (operand->buf, "a") == 0
    || strcasecmp (operand->buf, "b") == 0;
}

/* Return the number of operands found, or -1 on error, copying the
   operands into the given array and the accompanying expressions into
   the next array.  */

static int
get_operands (operands, line)
     struct opstruct operands[];
     char *line;
{
  char *lptr = line;
  int numexp = 0;
  int expecting_operand = 0;
  int i;

  while (numexp < MAX_OPERANDS && !is_end_of_line[(int) *lptr])
    {
      int paren_not_balanced = 0;
      char *op_start, *op_end;

      while (*lptr && ISSPACE (*lptr))
	++lptr;
      op_start = lptr;
      while (paren_not_balanced || *lptr != ',')
	{
	  if (*lptr == '\0')
	    {
	      if (paren_not_balanced)
		{
		  as_bad ("Unbalanced parenthesis in operand %d", numexp);
		  return -1;
		}
	      else
		break;
	    }
	  if (*lptr == '(')
	    ++paren_not_balanced;
	  else if (*lptr == ')')
	    --paren_not_balanced;
	  ++lptr;
	}
      op_end = lptr;
      if (op_end != op_start)
	{
	  int len = op_end - op_start;

	  strncpy (operands[numexp].buf, op_start, len);
	  operands[numexp].buf[len] = 0;
	  /* Trim trailing spaces; while the preprocessor gets rid of most,
	     there are weird usage patterns that can introduce them
	     (i.e. using strings for macro args).  */
	  while (len > 0 && ISSPACE (operands[numexp].buf[len - 1]))
	    operands[numexp].buf[--len] = 0;
	  lptr = op_end;
	  ++numexp;
	}
      else
	{
	  if (expecting_operand || *lptr == ',')
	    {
	      as_bad ("Expecting operand after ','");
	      return -1;
	    }
	}
      if (*lptr == ',')
	{
	  if (*++lptr == '\0')
	    {
	      as_bad ("Expecting operand after ','");
	      return -1;
	    }
	  expecting_operand = 1;
	}
    }

  while (*lptr && ISSPACE (*lptr++))
    ;
  if (!is_end_of_line[(int) *lptr])
    {
      as_bad ("Extra junk on line");
      return -1;
    }

  /* OK, now parse them into expressions.  */
  for (i = 0; i < numexp; i++)
    {
      memset (&operands[i].exp, 0, sizeof (operands[i].exp));
      if (operands[i].buf[0] == '#')
	{
	  /* Immediate.  */
	  parse_expression (operands[i].buf + 1, &operands[i].exp);
	}
      else if (operands[i].buf[0] == '@')
	{
	  /* Direct notation.  */
	  parse_expression (operands[i].buf + 1, &operands[i].exp);
	}
      else if (operands[i].buf[0] == '*')
	{
	  /* Indirect.  */
	  char *paren = strchr (operands[i].buf, '(');

	  /* Allow immediate syntax in the inner expression.  */
	  if (paren && paren[1] == '#')
	    *++paren = '(';

	  /* Pull out the lk expression or SP offset, if present.  */
	  if (paren != NULL)
	    {
	      int len = strlen (paren);
	      char *end = paren + len;
	      int c;

	      while (end[-1] != ')')
		if (--end <= paren)
		  {
		    as_bad (_("Badly formed address expression"));
		    return -1;
		  }
	      c = *end;
	      *end = '\0';
	      parse_expression (paren, &operands[i].exp);
	      *end = c;
	    }
	  else
	    operands[i].exp.X_op = O_absent;
	}
      else
	parse_expression (operands[i].buf, &operands[i].exp);
    }

  return numexp;
}

/* Predicates for different operand types.  */

static int
is_immediate (operand)
     struct opstruct *operand;
{
  return *operand->buf == '#';
}

/* This is distinguished from immediate because some numbers must be constants
   and must *not* have the '#' prefix.  */

static int
is_absolute (operand)
     struct opstruct *operand;
{
  return operand->exp.X_op == O_constant && !is_immediate (operand);
}

/* Is this an indirect operand?  */

static int
is_indirect (operand)
     struct opstruct *operand;
{
  return operand->buf[0] == '*';
}

/* Is this a valid dual-memory operand?  */

static int
is_dual (operand)
     struct opstruct *operand;
{
  if (is_indirect (operand) && strncasecmp (operand->buf, "*ar", 3) == 0)
    {
      char *tmp = operand->buf + 3;
      int arf;
      int valid_mod;

      arf = *tmp++ - '0';
      /* Only allow *ARx, *ARx-, *ARx+, or *ARx+0%.  */
      valid_mod = *tmp == '\0' ||
	strcasecmp (tmp, "-") == 0 ||
	strcasecmp (tmp, "+") == 0 ||
	strcasecmp (tmp, "+0%") == 0;
      return arf >= 2 && arf <= 5 && valid_mod;
    }
  return 0;
}

static int
is_mmreg (operand)
     struct opstruct *operand;
{
  return (is_absolute (operand)
	  || is_immediate (operand)
	  || hash_find (mmreg_hash, operand->buf) != 0);
}

static int
is_type (operand, type)
     struct opstruct *operand;
     enum optype type;
{
  switch (type)
    {
    case OP_None:
      return operand->buf[0] == 0;
    case OP_Xmem:
    case OP_Ymem:
      return is_dual (operand);
    case OP_Sind:
      return is_indirect (operand);
    case OP_xpmad_ms7:
      /* This one *must* be immediate.  */
      return is_immediate (operand);
    case OP_xpmad:
    case OP_pmad:
    case OP_PA:
    case OP_dmad:
    case OP_Lmem:
    case OP_MMR:
      return 1;
    case OP_Smem:
      /* Address may be a numeric, indirect, or an expression.  */
      return !is_immediate (operand);
    case OP_MMRY:
    case OP_MMRX:
      return is_mmreg (operand);
    case OP_SRC:
    case OP_SRC1:
    case OP_RND:
    case OP_DST:
      return is_accumulator (operand);
    case OP_B:
      return is_accumulator (operand) && TOUPPER (operand->buf[0]) == 'B';
    case OP_A:
      return is_accumulator (operand) && TOUPPER (operand->buf[0]) == 'A';
    case OP_ARX:
      return strncasecmp ("ar", operand->buf, 2) == 0
	&& ISDIGIT (operand->buf[2]);
    case OP_SBIT:
      return hash_find (sbit_hash, operand->buf) != 0 || is_absolute (operand);
    case OP_CC:
      return hash_find (cc_hash, operand->buf) != 0;
    case OP_CC2:
      return hash_find (cc2_hash, operand->buf) != 0;
    case OP_CC3:
      return hash_find (cc3_hash, operand->buf) != 0
	|| is_immediate (operand) || is_absolute (operand);
    case OP_16:
      return (is_immediate (operand) || is_absolute (operand))
	&& operand->exp.X_add_number == 16;
    case OP_N:
      /* Allow st0 or st1 instead of a numeric.  */
      return is_absolute (operand) || is_immediate (operand) ||
	strcasecmp ("st0", operand->buf) == 0 ||
	strcasecmp ("st1", operand->buf) == 0;
    case OP_12:
    case OP_123:
      return is_absolute (operand) || is_immediate (operand);
    case OP_SHFT:
      return (is_immediate (operand) || is_absolute (operand))
	&& operand->exp.X_add_number >= 0 && operand->exp.X_add_number < 16;
    case OP_SHIFT:
      /* Let this one catch out-of-range values.  */
      return (is_immediate (operand) || is_absolute (operand))
	&& operand->exp.X_add_number != 16;
    case OP_BITC:
    case OP_031:
    case OP_k8:
      return is_absolute (operand) || is_immediate (operand);
    case OP_k8u:
      return is_immediate (operand)
	&& operand->exp.X_op == O_constant
	&& operand->exp.X_add_number >= 0
	&& operand->exp.X_add_number < 256;
    case OP_lk:
    case OP_lku:
      /* Allow anything; assumes opcodes are ordered with Smem operands
	 versions first.  */
      return 1;
    case OP_k5:
    case OP_k3:
    case OP_k9:
      /* Just make sure it's an integer; check range later.  */
      return is_immediate (operand);
    case OP_T:
      return strcasecmp ("t", operand->buf) == 0 ||
	strcasecmp ("treg", operand->buf) == 0;
    case OP_TS:
      return strcasecmp ("ts", operand->buf) == 0;
    case OP_ASM:
      return strcasecmp ("asm", operand->buf) == 0;
    case OP_TRN:
      return strcasecmp ("trn", operand->buf) == 0;
    case OP_DP:
      return strcasecmp ("dp", operand->buf) == 0;
    case OP_ARP:
      return strcasecmp ("arp", operand->buf) == 0;
    default:
      return 0;
    }
}

static int
operands_match (insn, operands, opcount, refoptype, minops, maxops)
     tic54x_insn *insn;
     struct opstruct *operands;
     int opcount;
     const enum optype *refoptype;
     int minops;
     int maxops;
{
  int op = 0, refop = 0;

  if (opcount == 0 && minops == 0)
    return 1;

  while (op <= maxops && refop <= maxops)
    {
      while (!is_type (&operands[op], OPTYPE (refoptype[refop])))
	{
	  /* Skip an optional template operand if it doesn't agree
	     with the current operand.  */
	  if (refoptype[refop] & OPT)
	    {
	      ++refop;
	      --maxops;
	      if (refop > maxops)
		return 0;
	    }
	  else
	    return 0;
	}

      /* Save the actual operand type for later use.  */
      operands[op].type = OPTYPE (refoptype[refop]);
      ++refop;
      ++op;
      /* Have we matched them all yet?  */
      if (op == opcount)
	{
	  while (op < maxops)
	    {
	      /* If a later operand is *not* optional, no match.  */
	      if ((refoptype[refop] & OPT) == 0)
		return 0;
	      /* Flag any implicit default OP_DST operands so we know to add
		 them explicitly when encoding the operand later.  */
	      if (OPTYPE (refoptype[refop]) == OP_DST)
		insn->using_default_dst = 1;
	      ++refop;
	      ++op;
	    }

	  return 1;
	}
    }

  return 0;
}

/* 16-bit direct memory address
   Explicit dmad operands are always in last word of insn (usually second
   word, but bumped to third if lk addressing is used)

   We allow *(dmad) notation because the TI assembler allows it.

   XPC_CODE:
   0 for 16-bit addresses
   1 for full 23-bit addresses
   2 for the upper 7 bits of a 23-bit address (LDX).  */

static int
encode_dmad (insn, operand, xpc_code)
     tic54x_insn *insn;
     struct opstruct *operand;
     int xpc_code;
{
  int op = 1 + insn->is_lkaddr;

  /* Only allow *(dmad) expressions; all others are invalid.  */
  if (is_indirect (operand) && operand->buf[strlen (operand->buf) - 1] != ')')
    {
      as_bad (_("Invalid dmad syntax '%s'"), operand->buf);
      return 0;
    }

  insn->opcode[op].addr_expr = operand->exp;

  if (insn->opcode[op].addr_expr.X_op == O_constant)
    {
      valueT value = insn->opcode[op].addr_expr.X_add_number;

      if (xpc_code == 1)
	{
	  insn->opcode[0].word &= 0xFF80;
	  insn->opcode[0].word |= (value >> 16) & 0x7F;
	  insn->opcode[1].word = value & 0xFFFF;
	}
      else if (xpc_code == 2)
	insn->opcode[op].word = (value >> 16) & 0xFFFF;
      else
	insn->opcode[op].word = value;
    }
  else
    {
      /* Do the fixup later; just store the expression.  */
      insn->opcode[op].word = 0;
      insn->opcode[op].r_nchars = 2;

      if (amode == c_mode)
	insn->opcode[op].r_type = BFD_RELOC_TIC54X_16_OF_23;
      else if (xpc_code == 1)
	{
	  /* This relocation spans two words, so adjust accordingly.  */
	  insn->opcode[0].addr_expr = operand->exp;
	  insn->opcode[0].r_type = BFD_RELOC_TIC54X_23;
	  insn->opcode[0].r_nchars = 4;
	  insn->opcode[0].unresolved = 1;
	  /* It's really 2 words, but we want to stop encoding after the
	     first, since we must encode both words at once.  */
	  insn->words = 1;
	}
      else if (xpc_code == 2)
	insn->opcode[op].r_type = BFD_RELOC_TIC54X_MS7_OF_23;
      else
	insn->opcode[op].r_type = BFD_RELOC_TIC54X_16_OF_23;

      insn->opcode[op].unresolved = 1;
    }

  return 1;
}

/* 7-bit direct address encoding.  */

static int
encode_address (insn, operand)
     tic54x_insn *insn;
     struct opstruct *operand;
{
  /* Assumes that dma addresses are *always* in word 0 of the opcode.  */
  insn->opcode[0].addr_expr = operand->exp;

  if (operand->exp.X_op == O_constant)
    insn->opcode[0].word |= (operand->exp.X_add_number & 0x7F);
  else
    {
      if (operand->exp.X_op == O_register)
        as_bad (_("Use the .mmregs directive to use memory-mapped register names such as '%s'"), operand->buf);
      /* Do the fixup later; just store the expression.  */
      insn->opcode[0].r_nchars = 1;
      insn->opcode[0].r_type = BFD_RELOC_TIC54X_PARTLS7;
      insn->opcode[0].unresolved = 1;
    }

  return 1;
}

static int
encode_indirect (insn, operand)
     tic54x_insn *insn;
     struct opstruct *operand;
{
  int arf;
  int mod;

  if (insn->is_lkaddr)
    {
      /* lk addresses always go in the second insn word.  */
      mod = ((TOUPPER (operand->buf[1]) == 'A') ? 12 :
	     (operand->buf[1] == '(') ? 15 :
	     (strchr (operand->buf, '%') != NULL) ? 14 : 13);
      arf = ((mod == 12) ? operand->buf[3] - '0' :
	     (mod == 15) ? 0 : operand->buf[4] - '0');

      insn->opcode[1].addr_expr = operand->exp;

      if (operand->exp.X_op == O_constant)
	insn->opcode[1].word = operand->exp.X_add_number;
      else
	{
	  insn->opcode[1].word = 0;
	  insn->opcode[1].r_nchars = 2;
	  insn->opcode[1].r_type = BFD_RELOC_TIC54X_16_OF_23;
	  insn->opcode[1].unresolved = 1;
	}
    }
  else if (strncasecmp (operand->buf, "*sp (", 4) == 0)
    {
      /* Stack offsets look the same as 7-bit direct addressing.  */
      return encode_address (insn, operand);
    }
  else
    {
      arf = (TOUPPER (operand->buf[1]) == 'A' ?
	     operand->buf[3] : operand->buf[4]) - '0';

      if (operand->buf[1] == '+')
	{
	  mod = 3;		    /* *+ARx  */
	  if (insn->tm->flags & FL_SMR)
	    as_warn (_("Address mode *+ARx is write-only. "
		       "Results of reading are undefined."));
	}
      else if (operand->buf[4] == '\0')
	mod = 0;		    /* *ARx  */
      else if (operand->buf[5] == '\0')
	mod = (operand->buf[4] == '-' ? 1 : 2); /* *ARx+ / *ARx-  */
      else if (operand->buf[6] == '\0')
	{
	  if (operand->buf[5] == '0')
	    mod = (operand->buf[4] == '-' ? 5 : 6); /* *ARx+0 / *ARx-0  */
	  else
	    mod = (operand->buf[4] == '-' ? 8 : 10);/* *ARx+% / *ARx-%  */
	}
      else if (TOUPPER (operand->buf[6]) == 'B')
	mod = (operand->buf[4] == '-' ? 4 : 7); /* ARx+0B / *ARx-0B  */
      else if (TOUPPER (operand->buf[6]) == '%')
	mod = (operand->buf[4] == '-' ? 9 : 11); /* ARx+0% / *ARx - 0%  */
      else
	{
	  as_bad (_("Unrecognized indirect address format \"%s\""),
		  operand->buf);
	  return 0;
	}
    }

  insn->opcode[0].word |= 0x80 | (mod << 3) | arf;

  return 1;
}

static int
encode_integer (insn, operand, which, min, max, mask)
     tic54x_insn *insn;
     struct opstruct *operand;
     int which;
     int min;
     int max;
     unsigned short mask;
{
  long parse, integer;

  insn->opcode[which].addr_expr = operand->exp;

  if (operand->exp.X_op == O_constant)
    {
      parse = operand->exp.X_add_number;
      /* Hack -- fixup for 16-bit hex quantities that get converted positive
	 instead of negative.  */
      if ((parse & 0x8000) && min == -32768 && max == 32767)
	integer = (short) parse;
      else
	integer = parse;

      if (integer >= min && integer <= max)
	{
	  insn->opcode[which].word |= (integer & mask);
	  return 1;
	}
      as_bad (_("Operand '%s' out of range (%d <= x <= %d)"),
	      operand->buf, min, max);
    }
  else
    {
      if (insn->opcode[which].addr_expr.X_op == O_constant)
	{
	  insn->opcode[which].word |=
	    insn->opcode[which].addr_expr.X_add_number & mask;
	}
      else
	{
	  /* Do the fixup later; just store the expression.  */
	  bfd_reloc_code_real_type rtype =
	    (mask == 0x1FF ? BFD_RELOC_TIC54X_PARTMS9 :
	     mask == 0xFFFF ? BFD_RELOC_TIC54X_16_OF_23 :
	     mask == 0x7F ? BFD_RELOC_TIC54X_PARTLS7 : BFD_RELOC_8);
	  int size = (mask == 0x1FF || mask == 0xFFFF) ? 2 : 1;

	  if (rtype == BFD_RELOC_8)
	    as_bad (_("Error in relocation handling"));

	  insn->opcode[which].r_nchars = size;
	  insn->opcode[which].r_type = rtype;
	  insn->opcode[which].unresolved = 1;
	}

      return 1;
    }

  return 0;
}

static int
encode_condition (insn, operand)
     tic54x_insn *insn;
     struct opstruct *operand;
{
  symbol *cc = (symbol *) hash_find (cc_hash, operand->buf);
  if (!cc)
    {
      as_bad (_("Unrecognized condition code \"%s\""), operand->buf);
      return 0;
    }
#define CC_GROUP 0x40
#define CC_ACC   0x08
#define CATG_A1  0x07
#define CATG_B1  0x30
#define CATG_A2  0x30
#define CATG_B2  0x0C
#define CATG_C2  0x03
  /* Disallow group 1 conditions mixed with group 2 conditions
     if group 1, allow only one category A and one category B
     if group 2, allow only one each of category A, B, and C.  */
  if (((insn->opcode[0].word & 0xFF) != 0))
    {
      if ((insn->opcode[0].word & CC_GROUP) != (cc->value & CC_GROUP))
	{
	  as_bad (_("Condition \"%s\" does not match preceding group"),
		  operand->buf);
	  return 0;
	}
      if (insn->opcode[0].word & CC_GROUP)
	{
	  if ((insn->opcode[0].word & CC_ACC) != (cc->value & CC_ACC))
	    {
	      as_bad (_("Condition \"%s\" uses a different accumulator from "
			"a preceding condition"),
		      operand->buf);
	      return 0;
	    }
	  if ((insn->opcode[0].word & CATG_A1) && (cc->value & CATG_A1))
	    {
	      as_bad (_("Only one comparison conditional allowed"));
	      return 0;
	    }
	  if ((insn->opcode[0].word & CATG_B1) && (cc->value & CATG_B1))
	    {
	      as_bad (_("Only one overflow conditional allowed"));
	      return 0;
	    }
	}
      else if (   ((insn->opcode[0].word & CATG_A2) && (cc->value & CATG_A2))
	       || ((insn->opcode[0].word & CATG_B2) && (cc->value & CATG_B2))
	       || ((insn->opcode[0].word & CATG_C2) && (cc->value & CATG_C2)))
	{
	  as_bad (_("Duplicate %s conditional"), operand->buf);
	  return 0;
	}
    }

  insn->opcode[0].word |= cc->value;
  return 1;
}

static int
encode_cc3 (insn, operand)
     tic54x_insn *insn;
     struct opstruct *operand;
{
  symbol *cc3 = (symbol *) hash_find (cc3_hash, operand->buf);
  int value = cc3 ? cc3->value : operand->exp.X_add_number << 8;

  if ((value & 0x0300) != value)
    {
      as_bad (_("Unrecognized condition code \"%s\""), operand->buf);
      return 0;
    }
  insn->opcode[0].word |= value;
  return 1;
}

static int
encode_arx (insn, operand)
     tic54x_insn *insn;
     struct opstruct *operand;
{
  int arf = strlen (operand->buf) >= 3 ? operand->buf[2] - '0' : -1;

  if (strncasecmp ("ar", operand->buf, 2) || arf < 0 || arf > 7)
    {
      as_bad (_("Invalid auxiliary register (use AR0-AR7)"));
      return 0;
    }
  insn->opcode[0].word |= arf;
  return 1;
}

static int
encode_cc2 (insn, operand)
     tic54x_insn *insn;
     struct opstruct *operand;
{
  symbol *cc2 = (symbol *) hash_find (cc2_hash, operand->buf);

  if (!cc2)
    {
      as_bad (_("Unrecognized condition code \"%s\""), operand->buf);
      return 0;
    }
  insn->opcode[0].word |= cc2->value;
  return 1;
}

static int
encode_operand (insn, type, operand)
     tic54x_insn *insn;
     enum optype type;
     struct opstruct *operand;
{
  int ext = (insn->tm->flags & FL_EXT) != 0;

  if (type == OP_MMR && operand->exp.X_op != O_constant)
    {
      /* Disallow long-constant addressing for memory-mapped addressing.  */
      if (insn->is_lkaddr)
	{
	  as_bad (_("lk addressing modes are invalid for memory-mapped "
		    "register addressing"));
	  return 0;
	}
      type = OP_Smem;
      /* Warn about *+ARx when used with MMR operands.  */
      if (strncasecmp (operand->buf, "*+ar", 4) == 0)
	{
	  as_warn (_("Address mode *+ARx is not allowed in memory-mapped "
		     "register addressing.  Resulting behavior is "
		     "undefined."));
	}
    }

  switch (type)
    {
    case OP_None:
      return 1;
    case OP_dmad:
      /* 16-bit immediate value.  */
      return encode_dmad (insn, operand, 0);
    case OP_SRC:
      if (TOUPPER (*operand->buf) == 'B')
	{
	  insn->opcode[ext ? (1 + insn->is_lkaddr) : 0].word |= (1 << 9);
	  if (insn->using_default_dst)
	    insn->opcode[ext ? (1 + insn->is_lkaddr) : 0].word |= (1 << 8);
	}
      return 1;
    case OP_RND:
      /* Make sure this agrees with the OP_DST operand.  */
      if (!((TOUPPER (operand->buf[0]) == 'B') ^
	    ((insn->opcode[0].word & (1 << 8)) != 0)))
	{
	  as_bad (_("Destination accumulator for each part of this parallel "
		    "instruction must be different"));
	  return 0;
	}
      return 1;
    case OP_SRC1:
    case OP_DST:
      if (TOUPPER (operand->buf[0]) == 'B')
	insn->opcode[ext ? (1 + insn->is_lkaddr) : 0].word |= (1 << 8);
      return 1;
    case OP_Xmem:
    case OP_Ymem:
      {
	int mod = (operand->buf[4] == '\0' ? 0 : /* *arx  */
		   operand->buf[4] == '-' ? 1 : /* *arx-  */
		   operand->buf[5] == '\0' ? 2 : 3); /* *arx+, *arx+0%  */
	int arf = operand->buf[3] - '0' - 2;
	int code = (mod << 2) | arf;
	insn->opcode[0].word |= (code << (type == OP_Xmem ? 4 : 0));
	return 1;
      }
    case OP_Lmem:
    case OP_Smem:
      if (!is_indirect (operand))
	return encode_address (insn, operand);
      /* Fall through.  */
    case OP_Sind:
      return encode_indirect (insn, operand);
    case OP_xpmad_ms7:
      return encode_dmad (insn, operand, 2);
    case OP_xpmad:
      return encode_dmad (insn, operand, 1);
    case OP_PA:
    case OP_pmad:
      return encode_dmad (insn, operand, 0);
    case OP_ARX:
      return encode_arx (insn, operand);
    case OP_MMRX:
    case OP_MMRY:
    case OP_MMR:
      {
	int value = operand->exp.X_add_number;

	if (type == OP_MMR)
	  insn->opcode[0].word |= value;
	else
	  {
	    if (value < 16 || value > 24)
	      {
		as_bad (_("Memory mapped register \"%s\" out of range"),
			operand->buf);
		return 0;
	      }
	    if (type == OP_MMRX)
	      insn->opcode[0].word |= (value - 16) << 4;
	    else
	      insn->opcode[0].word |= (value - 16);
	  }
	return 1;
      }
    case OP_B:
    case OP_A:
      return 1;
    case OP_SHFT:
      return encode_integer (insn, operand, ext + insn->is_lkaddr,
			     0, 15, 0xF);
    case OP_SHIFT:
      return encode_integer (insn, operand, ext + insn->is_lkaddr,
			     -16, 15, 0x1F);
    case OP_lk:
      return encode_integer (insn, operand, 1 + insn->is_lkaddr,
			     -32768, 32767, 0xFFFF);
    case OP_CC:
      return encode_condition (insn, operand);
    case OP_CC2:
      return encode_cc2 (insn, operand);
    case OP_CC3:
      return encode_cc3 (insn, operand);
    case OP_BITC:
      return encode_integer (insn, operand, 0, 0, 15, 0xF);
    case OP_k8:
      return encode_integer (insn, operand, 0, -128, 127, 0xFF);
    case OP_123:
      {
	int value = operand->exp.X_add_number;
	int code;
	if (value < 1 || value > 3)
	  {
	    as_bad (_("Invalid operand (use 1, 2, or 3)"));
	    return 0;
	  }
	code = value == 1 ? 0 : value == 2 ? 0x2 : 0x1;
	insn->opcode[0].word |= (code << 8);
	return 1;
      }
    case OP_031:
      return encode_integer (insn, operand, 0, 0, 31, 0x1F);
    case OP_k8u:
      return encode_integer (insn, operand, 0, 0, 255, 0xFF);
    case OP_lku:
      return encode_integer (insn, operand, 1 + insn->is_lkaddr,
			     0, 65535, 0xFFFF);
    case OP_SBIT:
      {
	symbol *sbit = (symbol *) hash_find (sbit_hash, operand->buf);
	int value = is_absolute (operand) ?
	  operand->exp.X_add_number : (sbit ? sbit->value : -1);
	int reg = 0;

	if (insn->opcount == 1)
	  {
	    if (!sbit)
	      {
		as_bad (_("A status register or status bit name is required"));
		return 0;
	      }
	    /* Guess the register based on the status bit; "ovb" is the last
	       status bit defined for st0.  */
	    if (sbit > (symbol *) hash_find (sbit_hash, "ovb"))
	      reg = 1;
	  }
	if (value == -1)
	  {
	    as_bad (_("Unrecognized status bit \"%s\""), operand->buf);
	    return 0;
	  }
	insn->opcode[0].word |= value;
	insn->opcode[0].word |= (reg << 9);
	return 1;
      }
    case OP_N:
      if (strcasecmp (operand->buf, "st0") == 0
	  || strcasecmp (operand->buf, "st1") == 0)
	{
	  insn->opcode[0].word |=
	    ((unsigned short) (operand->buf[2] - '0')) << 9;
	  return 1;
	}
      else if (operand->exp.X_op == O_constant
	       && (operand->exp.X_add_number == 0
		   || operand->exp.X_add_number == 1))
	{
	  insn->opcode[0].word |=
	    ((unsigned short) (operand->exp.X_add_number)) << 9;
	  return 1;
	}
      as_bad (_("Invalid status register \"%s\""), operand->buf);
      return 0;
    case OP_k5:
      return encode_integer (insn, operand, 0, -16, 15, 0x1F);
    case OP_k3:
      return encode_integer (insn, operand, 0, 0, 7, 0x7);
    case OP_k9:
      return encode_integer (insn, operand, 0, 0, 0x1FF, 0x1FF);
    case OP_12:
      if (operand->exp.X_add_number != 1
	  && operand->exp.X_add_number != 2)
	{
	  as_bad (_("Operand \"%s\" out of range (use 1 or 2)"), operand->buf);
	  return 0;
	}
      insn->opcode[0].word |= (operand->exp.X_add_number - 1) << 9;
      return 1;
    case OP_16:
    case OP_T:
    case OP_TS:
    case OP_ASM:
    case OP_TRN:
    case OP_DP:
    case OP_ARP:
      /* No encoding necessary.  */
      return 1;
    default:
      return 0;
    }

  return 1;
}

static void
emit_insn (insn)
     tic54x_insn *insn;
{
  int i;
  flagword oldflags = bfd_get_section_flags (stdoutput, now_seg);
  flagword flags = oldflags | SEC_CODE;

  if (! bfd_set_section_flags (stdoutput, now_seg, flags))
        as_warn (_("error setting flags for \"%s\": %s"),
                 bfd_section_name (stdoutput, now_seg),
                 bfd_errmsg (bfd_get_error ()));

  for (i = 0; i < insn->words; i++)
    {
      int size = (insn->opcode[i].unresolved
		  && insn->opcode[i].r_type == BFD_RELOC_TIC54X_23) ? 4 : 2;
      char *p = frag_more (size);

      if (size == 2)
	md_number_to_chars (p, (valueT) insn->opcode[i].word, 2);
      else
	md_number_to_chars (p, (valueT) insn->opcode[i].word << 16, 4);

      if (insn->opcode[i].unresolved)
	fix_new_exp (frag_now, p - frag_now->fr_literal,
		     insn->opcode[i].r_nchars, &insn->opcode[i].addr_expr,
		     FALSE, insn->opcode[i].r_type);
    }
}

/* Convert the operand strings into appropriate opcode values
   return the total number of words used by the instruction.  */

static int
build_insn (insn)
     tic54x_insn *insn;
{
  int i;

  /* Only non-parallel instructions support lk addressing.  */
  if (!(insn->tm->flags & FL_PAR))
    {
      for (i = 0; i < insn->opcount; i++)
	{
	  if ((OPTYPE (insn->operands[i].type) == OP_Smem
	       || OPTYPE (insn->operands[i].type) == OP_Lmem
	       || OPTYPE (insn->operands[i].type) == OP_Sind)
	      && strchr (insn->operands[i].buf, '(')
	      /* Don't mistake stack-relative addressing for lk addressing.  */
	      && strncasecmp (insn->operands[i].buf, "*sp (", 4) != 0)
	    {
	      insn->is_lkaddr = 1;
	      insn->lkoperand = i;
	      break;
	    }
	}
    }
  insn->words = insn->tm->words + insn->is_lkaddr;

  insn->opcode[0].word = insn->tm->opcode;
  if (insn->tm->flags & FL_EXT)
    insn->opcode[1 + insn->is_lkaddr].word = insn->tm->opcode2;

  for (i = 0; i < insn->opcount; i++)
    {
      enum optype type = insn->operands[i].type;

      if (!encode_operand (insn, type, &insn->operands[i]))
	return 0;
    }
  if (insn->tm->flags & FL_PAR)
    for (i = 0; i < insn->paropcount; i++)
      {
	enum optype partype = insn->paroperands[i].type;

	if (!encode_operand (insn, partype, &insn->paroperands[i]))
	  return 0;
      }

  emit_insn (insn);

  return insn->words;
}

static int
optimize_insn (insn)
     tic54x_insn *insn;
{
  /* Optimize some instructions, helping out the brain-dead programmer.  */
#define is_zero(op) ((op).exp.X_op == O_constant && (op).exp.X_add_number == 0)
  if (strcasecmp (insn->tm->name, "add") == 0)
    {
      if (insn->opcount > 1
	  && is_accumulator (&insn->operands[insn->opcount - 2])
	  && is_accumulator (&insn->operands[insn->opcount - 1])
	  && strcasecmp (insn->operands[insn->opcount - 2].buf,
			 insn->operands[insn->opcount - 1].buf) == 0)
	{
	  --insn->opcount;
	  insn->using_default_dst = 1;
	  return 1;
	}

      /* Try to collapse if Xmem and shift count is zero.  */
      if ((OPTYPE (insn->tm->operand_types[0]) == OP_Xmem
	   && OPTYPE (insn->tm->operand_types[1]) == OP_SHFT
	   && is_zero (insn->operands[1]))
	  /* Or if Smem, shift is zero or absent, and SRC == DST.  */
	  || (OPTYPE (insn->tm->operand_types[0]) == OP_Smem
	      && OPTYPE (insn->tm->operand_types[1]) == OP_SHIFT
	      && is_type (&insn->operands[1], OP_SHIFT)
	      && is_zero (insn->operands[1]) && insn->opcount == 3))
	{
	  insn->operands[1] = insn->operands[2];
	  insn->opcount = 2;
	  return 1;
	}
    }
  else if (strcasecmp (insn->tm->name, "ld") == 0)
    {
      if (insn->opcount == 3 && insn->operands[0].type != OP_SRC)
	{
	  if ((OPTYPE (insn->tm->operand_types[1]) == OP_SHIFT
	       || OPTYPE (insn->tm->operand_types[1]) == OP_SHFT)
	      && is_zero (insn->operands[1])
	      && (OPTYPE (insn->tm->operand_types[0]) != OP_lk
		  || (insn->operands[0].exp.X_op == O_constant
		      && insn->operands[0].exp.X_add_number <= 255
		      && insn->operands[0].exp.X_add_number >= 0)))
	    {
	      insn->operands[1] = insn->operands[2];
	      insn->opcount = 2;
	      return 1;
	    }
	}
    }
  else if (strcasecmp (insn->tm->name, "sth") == 0
	   || strcasecmp (insn->tm->name, "stl") == 0)
    {
      if ((OPTYPE (insn->tm->operand_types[1]) == OP_SHIFT
	   || OPTYPE (insn->tm->operand_types[1]) == OP_SHFT)
	  && is_zero (insn->operands[1]))
	{
	  insn->operands[1] = insn->operands[2];
	  insn->opcount = 2;
	  return 1;
	}
    }
  else if (strcasecmp (insn->tm->name, "sub") == 0)
    {
      if (insn->opcount > 1
	  && is_accumulator (&insn->operands[insn->opcount - 2])
	  && is_accumulator (&insn->operands[insn->opcount - 1])
	  && strcasecmp (insn->operands[insn->opcount - 2].buf,
			 insn->operands[insn->opcount - 1].buf) == 0)
	{
	  --insn->opcount;
	  insn->using_default_dst = 1;
	  return 1;
	}

      if (   ((OPTYPE (insn->tm->operand_types[0]) == OP_Smem
	    && OPTYPE (insn->tm->operand_types[1]) == OP_SHIFT)
	   || (OPTYPE (insn->tm->operand_types[0]) == OP_Xmem
	    && OPTYPE (insn->tm->operand_types[1]) == OP_SHFT))
	  && is_zero (insn->operands[1])
	  && insn->opcount == 3)
	{
	  insn->operands[1] = insn->operands[2];
	  insn->opcount = 2;
	  return 1;
	}
    }
  return 0;
}

/* Find a matching template if possible, and get the operand strings.  */

static int
tic54x_parse_insn (insn, line)
     tic54x_insn *insn;
     char *line;
{
  insn->tm = (template *) hash_find (op_hash, insn->mnemonic);
  if (!insn->tm)
    {
      as_bad (_("Unrecognized instruction \"%s\""), insn->mnemonic);
      return 0;
    }

  insn->opcount = get_operands (insn->operands, line);
  if (insn->opcount < 0)
    return 0;

  /* Check each variation of operands for this mnemonic.  */
  while (insn->tm->name && strcasecmp (insn->tm->name, insn->mnemonic) == 0)
    {
      if (insn->opcount >= insn->tm->minops
	  && insn->opcount <= insn->tm->maxops
	  && operands_match (insn, &insn->operands[0], insn->opcount,
			     insn->tm->operand_types,
			     insn->tm->minops, insn->tm->maxops))
	{
	  /* SUCCESS! now try some optimizations.  */
	  if (optimize_insn (insn))
	    {
	      insn->tm = (template *) hash_find (op_hash,
						 insn->mnemonic);
	      continue;
	    }

	  return 1;
	}
      ++(insn->tm);
    }
  as_bad (_("Unrecognized operand list '%s' for instruction '%s'"),
	  line, insn->mnemonic);
  return 0;
}

/* We set this in start_line_hook, 'cause if we do a line replacement, we
   won't be able to see the next line.  */
static int parallel_on_next_line_hint = 0;

/* See if this is part of a parallel instruction
   Look for a subsequent line starting with "||".  */

static int
next_line_shows_parallel (next_line)
     char *next_line;
{
  /* Look for the second half.  */
  while (ISSPACE (*next_line))
    ++next_line;

  return (next_line[0] == PARALLEL_SEPARATOR
	  && next_line[1] == PARALLEL_SEPARATOR);
}

static int
tic54x_parse_parallel_insn_firstline (insn, line)
     tic54x_insn *insn;
     char *line;
{
  insn->tm = (template *) hash_find (parop_hash, insn->mnemonic);
  if (!insn->tm)
    {
      as_bad (_("Unrecognized parallel instruction \"%s\""),
	      insn->mnemonic);
      return 0;
    }

  while (insn->tm->name && strcasecmp (insn->tm->name,
                                       insn->mnemonic) == 0)
    {
      insn->opcount = get_operands (insn->operands, line);
      if (insn->opcount < 0)
	return 0;
      if (insn->opcount == 2
	  && operands_match (insn, &insn->operands[0], insn->opcount,
			     insn->tm->operand_types, 2, 2))
	{
	  return 1;
	}
      ++(insn->tm);
    }
  /* Didn't find a matching parallel; try for a normal insn.  */
  return 0;
}

/* Parse the second line of a two-line parallel instruction.  */

static int
tic54x_parse_parallel_insn_lastline (insn, line)
     tic54x_insn *insn;
     char *line;
{
  int valid_mnemonic = 0;

  insn->paropcount = get_operands (insn->paroperands, line);
  while (insn->tm->name && strcasecmp (insn->tm->name,
				       insn->mnemonic) == 0)
    {
      if (strcasecmp (insn->tm->parname, insn->parmnemonic) == 0)
	{
	  valid_mnemonic = 1;

	  if (insn->paropcount >= insn->tm->minops
	      && insn->paropcount <= insn->tm->maxops
	      && operands_match (insn, insn->paroperands,
				 insn->paropcount,
				 insn->tm->paroperand_types,
				 insn->tm->minops, insn->tm->maxops))
	    return 1;
	}
      ++(insn->tm);
    }
  if (valid_mnemonic)
    as_bad (_("Invalid operand (s) for parallel instruction \"%s\""),
	    insn->parmnemonic);
  else
    as_bad (_("Unrecognized parallel instruction combination \"%s || %s\""),
	    insn->mnemonic, insn->parmnemonic);

  return 0;
}

/* If quotes found, return copy of line up to closing quote;
   otherwise up until terminator.
   If it's a string, pass as-is; otherwise attempt substitution symbol
   replacement on the value.  */

static char *
subsym_get_arg (line, terminators, str, nosub)
     char *line;
     char *terminators;
     char **str;
     int nosub;
{
  char *ptr = line;
  char *endp;
  int is_string = *line == '"';
  int is_char = ISDIGIT (*line);

  if (is_char)
    {
      while (ISDIGIT (*ptr))
	++ptr;
      endp = ptr;
      *str = xmalloc (ptr - line + 1);
      strncpy (*str, line, ptr - line);
      (*str)[ptr - line] = 0;
    }
  else if (is_string)
    {
      char *savedp = input_line_pointer;
      int len;

      input_line_pointer = ptr;
      *str = demand_copy_C_string (&len);
      endp = input_line_pointer;
      input_line_pointer = savedp;

      /* Do forced substitutions if requested.  */
      if (!nosub && **str == ':')
	*str = subsym_substitute (*str, 1);
    }
  else
    {
      char *term = terminators;
      char *value = NULL;

      while (*ptr && *ptr != *term)
	{
	  if (!*term)
	    {
	      term = terminators;
	      ++ptr;
	    }
	  else
	    ++term;
	}
      endp = ptr;
      *str = xmalloc (ptr - line + 1);
      strncpy (*str, line, ptr - line);
      (*str)[ptr - line] = 0;
      /* Do simple substitution, if available.  */
      if (!nosub && (value = subsym_lookup (*str, macro_level)) != NULL)
	*str = value;
    }

  return endp;
}

/* Replace the given substitution string.
   We start at the innermost macro level, so that existing locals remain local
   Note: we're treating macro args identically to .var's; I don't know if
   that's compatible w/TI's assembler.  */

static void
subsym_create_or_replace (name, value)
     char *name;
     char *value;
{
  int i;

  for (i = macro_level; i > 0; i--)
    {
      if (hash_find (subsym_hash[i], name))
	{
	  hash_replace (subsym_hash[i], name, value);
	  return;
	}
    }
  if (hash_find (subsym_hash[0], name))
    hash_replace (subsym_hash[0], name, value);
  else
    hash_insert (subsym_hash[0], name, value);
}

/* Look up the substitution string replacement for the given symbol.
   Start with the innermost macro substitution table given and work
   outwards.  */

static char *
subsym_lookup (name, nest_level)
     char *name;
     int nest_level;
{
  char *value = hash_find (subsym_hash[nest_level], name);

  if (value || nest_level == 0)
    return value;

  return subsym_lookup (name, nest_level - 1);
}

/* Do substitution-symbol replacement on the given line (recursively).
   return the argument if no substitution was done

   Also look for built-in functions ($func (arg)) and local labels.

   If FORCED is set, look for forced substitutions of the form ':SYMBOL:'.  */

static char *
subsym_substitute (line, forced)
     char * line;
     int forced;
{
  /* For each apparent symbol, see if it's a substitution symbol, and if so,
     replace it in the input.  */
  char *replacement; /* current replacement for LINE.  */
  char *head; /* Start of line.  */
  char *ptr; /* Current examination point.  */
  int changed = 0; /* Did we make a substitution?  */
  int eval_line = 0; /* Is this line a .eval/.asg statement?  */
  int eval_symbol = 0; /* Are we in the middle of the symbol for
                          .eval/.asg?  */
  char *eval_end = NULL;
  int recurse = 1;
  int line_conditional = 0;
  char *tmp;

  /* Work with a copy of the input line.  */
  replacement = xmalloc (strlen (line) + 1);
  strcpy (replacement, line);

  ptr = head = replacement;

  /* Flag lines where we might need to replace a single '=' with two;
     GAS uses single '=' to assign macro args values, and possibly other
     places, so limit what we replace.  */
  if (strstr (line, ".if")
      || strstr (line, ".elseif")
      || strstr (line, ".break"))
    line_conditional = 1;

  /* Watch out for .eval, so that we avoid doing substitution on the
     symbol being assigned a value.  */
  if (strstr (line, ".eval") || strstr (line, ".asg"))
    eval_line = 1;

  /* If it's a macro definition, don't do substitution on the argument
     names.  */
  if (strstr (line, ".macro"))
    return line;

  while (!is_end_of_line[(int) *ptr])
    {
      int current_char = *ptr;

      /* Need to update this since LINE may have been modified.  */
      if (eval_line)
	eval_end = strrchr (ptr, ',');

      /* Replace triple double quotes with bounding quote/escapes.  */
      if (current_char == '"' && ptr[1] == '"' && ptr[2] == '"')
	{
	  ptr[1] = '\\';
	  tmp = strstr (ptr + 2, "\"\"\"");
	  if (tmp)
	    tmp[0] = '\\';
	  changed = 1;
	}

      /* Replace a single '=' with a '==';
	 for compatibility with older code only.  */
      if (line_conditional && current_char == '=')
	{
	  if (ptr[1] == '=')
	    {
	      ptr += 2;
	      continue;
	    }
	  *ptr++ = '\0';
	  tmp = xmalloc (strlen (head) + 2 + strlen (ptr) + 1);
	  sprintf (tmp, "%s==%s", head, ptr);
	  /* Continue examining after the '=='.  */
	  ptr = tmp + strlen (head) + 2;
	  free (replacement);
	  head = replacement = tmp;
	  changed = 1;
	}

      /* Flag when we've reached the symbol part of .eval/.asg.  */
      if (eval_line && ptr >= eval_end)
	eval_symbol = 1;

      /* For each apparent symbol, see if it's a substitution symbol, and if
	 so, replace it in the input.  */
      if ((forced && current_char == ':')
	  || (!forced && is_name_beginner (current_char)))
	{
	  char *name; /* Symbol to be replaced.  */
	  char *savedp = input_line_pointer;
	  int c;
	  char *value = NULL;
	  char *tail; /* Rest of line after symbol.  */

	  /* Skip the colon.  */
	  if (forced)
	    ++ptr;

	  name = input_line_pointer = ptr;
	  c = get_symbol_end ();
	  /* '?' is not normally part of a symbol, but it IS part of a local
	     label.  */
	  if (c == '?')
	    {
	      *input_line_pointer++ = c;
	      c = *input_line_pointer;
	      *input_line_pointer = '\0';
	    }
	  /* Avoid infinite recursion; if a symbol shows up a second time for
	     substitution, leave it as is.  */
	  if (hash_find (subsym_recurse_hash, name) == NULL)
	    value = subsym_lookup (name, macro_level);
	  else
	    as_warn (_("%s symbol recursion stopped at "
		       "second appearance of '%s'"),
		     forced ? "Forced substitution" : "Substitution", name);
	  ptr = tail = input_line_pointer;
	  input_line_pointer = savedp;

	  /* Check for local labels; replace them with the appropriate
	     substitution.  */
	  if ((*name == '$' && ISDIGIT (name[1]) && name[2] == '\0')
	      || name[strlen (name) - 1] == '?')
	    {
	      /* Use an existing identifier for that label if, available, or
		 create a new, unique identifier.  */
	      value = hash_find (local_label_hash[macro_level], name);
	      if (value == NULL)
		{
		  char digit[11];
		  char *namecopy = strcpy (xmalloc (strlen (name) + 1), name);

		  value = strcpy (xmalloc (strlen (name) + sizeof (digit) + 1),
				  name);
		  if (*value != '$')
		    value[strlen (value) - 1] = '\0';
		  sprintf (digit, ".%d", local_label_id++);
		  strcat (value, digit);
		  hash_insert (local_label_hash[macro_level], namecopy, value);
		}
	      /* Indicate where to continue looking for substitutions.  */
	      ptr = tail;
	    }
	  /* Check for built-in subsym and math functions.  */
	  else if (value != NULL && *name == '$')
	    {
	      subsym_proc_entry *entry = (subsym_proc_entry *) value;
	      math_proc_entry *math_entry = hash_find (math_hash, name);
	      char *arg1, *arg2 = NULL;

	      *ptr = c;
	      if (entry == NULL)
		{
		  as_bad (_("Unrecognized substitution symbol function"));
		  break;
		}
	      else if (*ptr != '(')
		{
		  as_bad (_("Missing '(' after substitution symbol function"));
		  break;
		}
	      ++ptr;
	      if (math_entry != NULL)
		{
		  float arg1, arg2 = 0;
		  volatile float fresult;

		  arg1 = (float) strtod (ptr, &ptr);
		  if (math_entry->nargs == 2)
		    {
		      if (*ptr++ != ',')
			{
			  as_bad (_("Expecting second argument"));
			  break;
			}
		      arg2 = (float) strtod (ptr, &ptr);
		    }
		  fresult = (*math_entry->proc) (arg1, arg2);
		  value = xmalloc (128);
		  if (math_entry->int_return)
		    sprintf (value, "%d", (int) fresult);
		  else
		    sprintf (value, "%f", fresult);
		  if (*ptr++ != ')')
		    {
		      as_bad (_("Extra junk in function call, expecting ')'"));
		      break;
		    }
		  /* Don't bother recursing; the replacement isn't a
                     symbol.  */
		  recurse = 0;
		}
	      else
		{
		  int val;
		  int arg_type[2] = { *ptr == '"' , 0 };
		  int ismember = !strcmp (entry->name, "$ismember");

		  /* Parse one or two args, which must be a substitution
		     symbol, string or a character-string constant.  */
		  /* For all functions, a string or substitution symbol may be
		     used, with the following exceptions:
		     firstch/lastch: 2nd arg must be character constant
		     ismember: both args must be substitution symbols.  */
		  ptr = subsym_get_arg (ptr, ",)", &arg1, ismember);
		  if (!arg1)
		    break;
		  if (entry->nargs == 2)
		    {
		      if (*ptr++ != ',')
			{
			  as_bad (_("Function expects two arguments"));
			  break;
			}
		      /* Character constants are converted to numerics
			 by the preprocessor.  */
		      arg_type[1] = (ISDIGIT (*ptr)) ? 2 : (*ptr == '"');
		      ptr = subsym_get_arg (ptr, ")", &arg2, ismember);
		    }
		  /* Args checking.  */
		  if ((!strcmp (entry->name, "$firstch")
		       || !strcmp (entry->name, "$lastch"))
		      && arg_type[1] != 2)
		    {
		      as_bad (_("Expecting character constant argument"));
		      break;
		    }
		  if (ismember
		      && (arg_type[0] != 0 || arg_type[1] != 0))
		    {
		      as_bad (_("Both arguments must be substitution symbols"));
		      break;
		    }
		  if (*ptr++ != ')')
		    {
		      as_bad (_("Extra junk in function call, expecting ')'"));
		      break;
		    }
		  val = (*entry->proc) (arg1, arg2);
		  value = xmalloc (64);
		  sprintf (value, "%d", val);
		}
	      /* Fix things up to replace the entire expression, not just the
		 function name.  */
	      tail = ptr;
	      c = *tail;
	    }

	  if (value != NULL && !eval_symbol)
	    {
	      /* Replace the symbol with its string replacement and
		 continue.  Recursively replace VALUE until either no
		 substitutions are performed, or a substitution that has been
		 previously made is encountered again.

		 put the symbol into the recursion hash table so we only
		 try to replace a symbol once.  */
	      if (recurse)
		{
		  hash_insert (subsym_recurse_hash, name, name);
		  value = subsym_substitute (value, macro_level > 0);
		  hash_delete (subsym_recurse_hash, name);
		}

	      /* Temporarily zero-terminate where the symbol started.  */
	      *name = 0;
	      if (forced)
		{
		  if (c == '(')
		    {
		      /* Subscripted substitution symbol -- use just the
			 indicated portion of the string; the description
			 kinda indicates that forced substitution is not
			 supposed to be recursive, but I'm not sure.  */
		      unsigned beg, len = 1; /* default to a single char */
		      char *newval = strcpy (xmalloc (strlen (value) + 1),
					     value);

		      savedp = input_line_pointer;
		      input_line_pointer = tail + 1;
		      beg = get_absolute_expression ();
		      if (beg < 1)
			{
			  as_bad (_("Invalid subscript (use 1 to %d)"),
				  strlen (value));
			  break;
			}
		      if (*input_line_pointer == ',')
			{
			  ++input_line_pointer;
			  len = get_absolute_expression ();
			  if (beg + len > strlen (value))
			    {
			      as_bad (_("Invalid length (use 0 to %d"),
				      strlen (value) - beg);
			      break;
			    }
			}
		      newval += beg - 1;
		      newval[len] = 0;
		      tail = input_line_pointer;
		      if (*tail++ != ')')
			{
			  as_bad (_("Missing ')' in subscripted substitution "
				    "symbol expression"));
			  break;
			}
		      c = *tail;
		      input_line_pointer = savedp;

		      value = newval;
		    }
		  name[-1] = 0;
		}
	      tmp = xmalloc (strlen (head) + strlen (value) +
			     strlen (tail + 1) + 2);
	      strcpy (tmp, head);
	      strcat (tmp, value);
	      /* Make sure forced substitutions are properly terminated.  */
	      if (forced)
		{
		  if (c != ':')
		    {
		      as_bad (_("Missing forced substitution terminator ':'"));
		      break;
		    }
		  ++tail;
#if 0
		  /* Try to replace required whitespace
		     eliminated by the preprocessor; technically, a forced
		     substitution could come anywhere, even mid-symbol,
		     e.g. if x is "0", 'sym:x:end' should result in 'sym0end',
		     but 'sym:x: end' should result in 'sym0 end'.
		     FIXME -- this should really be fixed in the preprocessor,
		     but would require several new states;
		     KEEP_WHITE_AROUND_COLON does part of the job, but isn't
		     complete.  */
		  if ((is_part_of_name (tail[1])
		       && tail[1] != '.'
		       && tail[1] != '$')
		      || tail[1] == '\0' || tail[1] == ',' || tail[1] == '"')
		    ++tail;
		  else
		    *tail = ' ';
#endif
		}
	      else
		/* Restore the character after the symbol end.  */
		*tail = c;
	      strcat (tmp, tail);
	      /* Continue examining after the replacement value.  */
	      ptr = tmp + strlen (head) + strlen (value);
	      free (replacement);
	      head = replacement = tmp;
	      changed = 1;
	    }
	  else
	    *ptr = c;
	}
      else
	{
	  ++ptr;
	}
    }

  if (changed)
    return replacement;
  else
    return line;
}

/* We use this to handle substitution symbols
   hijack input_line_pointer, replacing it with our substituted string.

   .sslist should enable listing the line after replacements are made...

   returns the new buffer limit.  */

void
tic54x_start_line_hook ()
{
  char *line, *endp;
  char *replacement = NULL;

  /* Work with a copy of the input line, including EOL char.  */
  endp = input_line_pointer;
  while (!is_end_of_line[(int) *endp++])
    ;
  line = xmalloc (endp - input_line_pointer + 1);
  strncpy (line, input_line_pointer, endp - input_line_pointer + 1);
  line[endp - input_line_pointer] = 0;

  /* Scan ahead for parallel insns.  */
  parallel_on_next_line_hint = next_line_shows_parallel (endp + 1);

  /* If within a macro, first process forced replacements.  */
  if (macro_level > 0)
    replacement = subsym_substitute (line, 1);
  else
    replacement = line;
  replacement = subsym_substitute (replacement, 0);

  if (replacement != line)
    {
      char *tmp = replacement;
      char *comment = strchr (replacement, ';');
      char endc = replacement[strlen (replacement) - 1];

      /* Clean up the replacement; we'd prefer to have this done by the
	 standard preprocessing equipment (maybe do_scrub_chars?)
	 but for now, do a quick-and-dirty.  */
      if (comment != NULL)
	{
	  comment[0] = endc;
	  comment[1] = 0;
	  --comment;
	}
      else
	comment = replacement + strlen (replacement) - 1;

      /* Trim trailing whitespace.  */
      while (ISSPACE (*comment))
	{
	  comment[0] = endc;
	  comment[1] = 0;
	  --comment;
	}

      /* Compact leading whitespace.  */
      while (ISSPACE (tmp[0]) && ISSPACE (tmp[1]))
	++tmp;

      input_line_pointer = endp;
      input_scrub_insert_line (tmp);
      free (replacement);
      free (line);
      /* Keep track of whether we've done a substitution.  */
      substitution_line = 1;
    }
  else
    {
      /* No change.  */
      free (line);
      substitution_line = 0;
    }
}

/* This is the guts of the machine-dependent assembler.  STR points to a
   machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */
void
md_assemble (line)
     char *line;
{
  static int repeat_slot = 0;
  static int delay_slots = 0; /* How many delay slots left to fill?  */
  static int is_parallel = 0;
  static tic54x_insn insn;
  char *lptr;
  char *savedp = input_line_pointer;
  int c;

  input_line_pointer = line;
  c = get_symbol_end ();

  if (cpu == VNONE)
    cpu = V542;
  if (address_mode_needs_set)
    {
      set_address_mode (amode);
      address_mode_needs_set = 0;
    }
  if (cpu_needs_set)
    {
      set_cpu (cpu);
      cpu_needs_set = 0;
    }
  assembly_begun = 1;

  if (is_parallel)
    {
      is_parallel = 0;

      strcpy (insn.parmnemonic, line);
      lptr = input_line_pointer;
      *lptr = c;
      input_line_pointer = savedp;

      if (tic54x_parse_parallel_insn_lastline (&insn, lptr))
	{
	  int words = build_insn (&insn);

	  if (delay_slots != 0)
	    {
	      if (words > delay_slots)
		{
		  as_bad (_("Instruction does not fit in available delay "
			    "slots (%d-word insn, %d slots left)"),
			  words, delay_slots);
		  delay_slots = 0;
		  return;
		}
	      delay_slots -= words;
	    }
	}
      return;
    }

  memset (&insn, 0, sizeof (insn));
  strcpy (insn.mnemonic, line);
  lptr = input_line_pointer;
  *lptr = c;
  input_line_pointer = savedp;

  /* See if this line is part of a parallel instruction; if so, either this
     line or the next line will have the "||" specifier preceding the
     mnemonic, and we look for it in the parallel insn hash table.  */
  if (strstr (line, "||") != NULL || parallel_on_next_line_hint)
    {
      char *tmp = strstr (line, "||");
      if (tmp != NULL)
	*tmp = '\0';

      if (tic54x_parse_parallel_insn_firstline (&insn, lptr))
	{
	  is_parallel = 1;
	  /* If the parallel part is on the same line, process it now,
	     otherwise let the assembler pick up the next line for us.  */
	  if (tmp != NULL)
	    {
	      while (ISSPACE (tmp[2]))
		++tmp;
	      md_assemble (tmp + 2);
	    }
	}
      else
	{
	  as_bad (_("Unrecognized parallel instruction '%s'"), line);
	}
      return;
    }

  if (tic54x_parse_insn (&insn, lptr))
    {
      int words;

      if ((insn.tm->flags & FL_LP)
	  && cpu != V545LP && cpu != V546LP)
	{
	  as_bad (_("Instruction '%s' requires an LP cpu version"),
		  insn.tm->name);
	  return;
	}
      if ((insn.tm->flags & FL_FAR)
	  && amode != far_mode)
	{
	  as_bad (_("Instruction '%s' requires far mode addressing"),
		  insn.tm->name);
	  return;
	}

      words = build_insn (&insn);

      /* Is this instruction in a delay slot?  */
      if (delay_slots)
	{
	  if (words > delay_slots)
	    {
	      as_warn (_("Instruction does not fit in available delay "
			 "slots (%d-word insn, %d slots left). "
			 "Resulting behavior is undefined."),
		       words, delay_slots);
	      delay_slots = 0;
	      return;
	    }
	  /* Branches in delay slots are not allowed.  */
	  if (insn.tm->flags & FL_BMASK)
	    {
	      as_warn (_("Instructions which cause PC discontinuity are not "
			 "allowed in a delay slot. "
			 "Resulting behavior is undefined."));
	    }
	  delay_slots -= words;
	}

      /* Is this instruction the target of a repeat?  */
      if (repeat_slot)
	{
	  if (insn.tm->flags & FL_NR)
	    as_warn (_("'%s' is not repeatable. "
		       "Resulting behavior is undefined."),
		     insn.tm->name);
	  else if (insn.is_lkaddr)
	    as_warn (_("Instructions using long offset modifiers or absolute "
		       "addresses are not repeatable. "
		       "Resulting behavior is undefined."));
	  repeat_slot = 0;
	}

      /* Make sure we check the target of a repeat instruction.  */
      if (insn.tm->flags & B_REPEAT)
	{
	  repeat_slot = 1;
	  /* FIXME -- warn if repeat_slot == 1 at EOF.  */
	}
      /* Make sure we check our delay slots for validity.  */
      if (insn.tm->flags & FL_DELAY)
	{
	  delay_slots = 2;
	  /* FIXME -- warn if delay_slots != 0 at EOF.  */
	}
    }
}

/* Do a final adjustment on the symbol table; in this case, make sure we have
   a ".file" symbol.  */

void
tic54x_adjust_symtab ()
{
  if (symbol_rootP == NULL
      || S_GET_STORAGE_CLASS (symbol_rootP) != C_FILE)
    {
      char *filename;
      unsigned lineno;
      as_where (&filename, &lineno);
      c_dot_file_symbol (filename);
    }
}

/* In order to get gas to ignore any | chars at the start of a line,
   this function returns true if a | is found in a line.
   This lets us process parallel instructions, which span two lines.  */

int
tic54x_unrecognized_line (int c)
{
  return c == PARALLEL_SEPARATOR;
}

/* Watch for local labels of the form $[0-9] and [_a-zA-Z][_a-zA-Z0-9]*?
   Encode their names so that only we see them and can map them to the
   appropriate places.
   FIXME -- obviously this isn't done yet.  These locals still show up in the
   symbol table.  */
void
tic54x_define_label (sym)
     symbolS *sym;
{
#if 0
  static int local_label_count = 0;
  const char *name = S_GET_NAME (sym);
#endif

  /* Just in case we need this later; note that this is not necessarily the
     same thing as line_label...
     When aligning or assigning labels to fields, sometimes the label is
     assigned other than the address at which the label appears.
     FIXME -- is this really needed? I think all the proper label assignment
     is done in tic54x_cons.  */
  last_label_seen = sym;
}

/* Try to parse something that normal parsing failed at.  */

symbolS *
tic54x_undefined_symbol (name)
     char *name;
{
  symbol *sym;

  /* Not sure how to handle predefined symbols.  */
  if ((sym = (symbol *) hash_find (cc_hash, name)) != NULL ||
      (sym = (symbol *) hash_find (cc2_hash, name)) != NULL ||
      (sym = (symbol *) hash_find (cc3_hash, name)) != NULL ||
      (sym = (symbol *) hash_find (misc_symbol_hash, name)) != NULL ||
      (sym = (symbol *) hash_find (sbit_hash, name)) != NULL)
    {
      return symbol_new (name, reg_section,
			 (valueT) sym->value,
			 &zero_address_frag);
    }

  if ((sym = (symbol *) hash_find (reg_hash, name)) != NULL ||
      (sym = (symbol *) hash_find (mmreg_hash, name)) != NULL ||
      !strcasecmp (name, "a") || !strcasecmp (name, "b"))
    {
      return symbol_new (name, reg_section,
			 (valueT) sym ? sym->value : 0,
			 &zero_address_frag);
    }

  return NULL;
}

/* Parse a name in an expression before the expression parser takes a stab at
   it.  */

int
tic54x_parse_name (name, exp)
     char *name ATTRIBUTE_UNUSED;
     expressionS *exp ATTRIBUTE_UNUSED;
{
#if 0
  symbol *sym = (symbol *) hash_find (mmreg_hash, name);

  /* If it's a MMREG, replace it with its constant value.  */
  if (sym)
    {
      exp->X_op = O_constant;
      exp->X_add_number = sym->value;
      return 1;
    }
#endif
  return 0;
}

char *
md_atof (type, literalP, sizeP)
     int type;
     char *literalP;
     int *sizeP;
{
#define MAX_LITTLENUMS 2
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *word;
  /* Only one precision on the c54x.  */
  int prec = 2;
  char *t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = 4;

  /* Target data is little-endian, but floats are stored
     big-"word"ian.  ugh.  */
  for (word = words; prec--;)
    {
      md_number_to_chars (literalP, (long) (*word++), sizeof (LITTLENUM_TYPE));
      literalP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}

arelent *
tc_gen_reloc (section, fixP)
     asection *section;
     fixS *fixP;
{
  arelent *rel;
  bfd_reloc_code_real_type code = fixP->fx_r_type;
  asymbol *sym = symbol_get_bfdsym (fixP->fx_addsy);

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = sym;
  /* We assume that all rel->address are host byte offsets.  */
  rel->address = fixP->fx_frag->fr_address + fixP->fx_where;
  rel->address /= OCTETS_PER_BYTE;
  rel->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (!strcmp (sym->name, section->name))
    rel->howto += HOWTO_BANK;

  if (!rel->howto)
    {
      const char *name = S_GET_NAME (fixP->fx_addsy);
      if (name == NULL)
	name = "<unknown>";
      as_fatal ("Cannot generate relocation type for symbol %s, code %s",
		name, bfd_get_reloc_code_name (code));
      return NULL;
    }
  return rel;
}

/* Handle cons expressions.  */

void
tic54x_cons_fix_new (frag, where, octets, exp)
     fragS *frag;
     int where;
     int octets;
     expressionS *exp;
{
  bfd_reloc_code_real_type r;

  switch (octets)
    {
    default:
      as_bad (_("Unsupported relocation size %d"), octets);
      r = BFD_RELOC_TIC54X_16_OF_23;
      break;
    case 2:
      r = BFD_RELOC_TIC54X_16_OF_23;
      break;
    case 4:
      /* TI assembler always uses this, regardless of addressing mode.  */
      if (emitting_long)
	r = BFD_RELOC_TIC54X_23;
      else
	/* We never want to directly generate this; this is provided for
	   stabs support only.  */
	r = BFD_RELOC_32;
      break;
    }
  fix_new_exp (frag, where, octets, exp, 0, r);
}

/* Attempt to simplify or even eliminate a fixup.
   To indicate that a fixup has been eliminated, set fixP->fx_done.

   If fixp->fx_addsy is non-NULL, we'll have to generate a reloc entry.   */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT * valP;
     segT seg ATTRIBUTE_UNUSED;
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  valueT val = * valP;

  switch (fixP->fx_r_type)
    {
    default:
      as_fatal ("Bad relocation type: 0x%02x", fixP->fx_r_type);
      return;
    case BFD_RELOC_TIC54X_MS7_OF_23:
      val = (val >> 16) & 0x7F;
      /* Fall through.  */
    case BFD_RELOC_TIC54X_16_OF_23:
    case BFD_RELOC_16:
      bfd_put_16 (stdoutput, val, buf);
      /* Indicate what we're actually writing, so that we don't get warnings
	 about exceeding available space.  */
      *valP = val & 0xFFFF;
      break;
    case BFD_RELOC_TIC54X_PARTLS7:
      bfd_put_16 (stdoutput,
		  (bfd_get_16 (stdoutput, buf) & 0xFF80) | (val & 0x7F),
		  buf);
      /* Indicate what we're actually writing, so that we don't get warnings
	 about exceeding available space.  */
      *valP = val & 0x7F;
      break;
    case BFD_RELOC_TIC54X_PARTMS9:
      /* TI assembler doesn't shift its encoding for relocatable files, and is
	 thus incompatible with this implementation's relocatable files.  */
      bfd_put_16 (stdoutput,
		  (bfd_get_16 (stdoutput, buf) & 0xFE00) | (val >> 7),
		  buf);
      break;
    case BFD_RELOC_32:
    case BFD_RELOC_TIC54X_23:
      bfd_put_32 (stdoutput,
		  (bfd_get_32 (stdoutput, buf) & 0xFF800000) | val,
		  buf);
      break;
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* This is our chance to record section alignment
   don't need to do anything here, since BFD does the proper encoding.  */

valueT
md_section_align (segment, section_size)
     segT segment ATTRIBUTE_UNUSED;
     valueT section_size;
{
  return section_size;
}

long
md_pcrel_from (fixP)
     fixS *fixP ATTRIBUTE_UNUSED;
{
  return 0;
}

#if defined OBJ_COFF

short
tc_coff_fix2rtype (fixP)
     fixS *fixP;
{
  return (fixP->fx_r_type);
}

#endif /* OBJ_COFF */

/* Mostly little-endian, but longwords (4 octets) get MS word stored
   first.  */

void
tic54x_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if (n != 4)
    number_to_chars_littleendian (buf, val, n);
  else
    {
      number_to_chars_littleendian (buf    , val >> 16   , 2);
      number_to_chars_littleendian (buf + 2, val & 0xFFFF, 2);
    }
}

int
tic54x_estimate_size_before_relax (frag, seg)
     fragS *frag ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
{
  return 0;
}

/* We use this to handle bit allocations which we couldn't handle before due
   to symbols being in different frags.  return number of octets added.  */

int
tic54x_relax_frag (frag, stretch)
     fragS *frag;
     long stretch ATTRIBUTE_UNUSED;
{
  symbolS *sym = frag->fr_symbol;
  int growth = 0;
  int i;

  if (sym != NULL)
    {
      struct bit_info *bi = (struct bit_info *) frag->fr_opcode;
      int bit_offset = frag_bit_offset (frag_prev (frag, bi->seg), bi->seg);
      int size = S_GET_VALUE (sym);
      fragS *prev_frag = bit_offset_frag (frag_prev (frag, bi->seg), bi->seg);
      int available = 16 - bit_offset;

      if (symbol_get_frag (sym) != &zero_address_frag
	  || S_IS_COMMON (sym)
	  || !S_IS_DEFINED (sym))
	as_bad_where (frag->fr_file, frag->fr_line,
		      _("non-absolute value used with .space/.bes"));

      if (size < 0)
	{
	  as_warn (_("negative value ignored in %s"),
		   bi->type == TYPE_SPACE ? ".space" :
		   bi->type == TYPE_BES ? ".bes" : ".field");
	  growth = 0;
	  frag->tc_frag_data = frag->fr_fix = 0;
	  return 0;
	}

      if (bi->type == TYPE_FIELD)
	{
	  /* Bit fields of 16 or larger will have already been handled.  */
	  if (bit_offset != 0 && available >= size)
	    {
	      char *p = prev_frag->fr_literal;

	      valueT value = bi->value;
	      value <<= available - size;
	      value |= ((unsigned short) p[1] << 8) | p[0];
	      md_number_to_chars (p, value, 2);
	      if ((prev_frag->tc_frag_data += size) == 16)
		prev_frag->tc_frag_data = 0;
	      if (bi->sym)
		symbol_set_frag (bi->sym, prev_frag);
	      /* This frag is no longer used.  */
	      growth = -frag->fr_fix;
	      frag->fr_fix = 0;
	      frag->tc_frag_data = 0;
	    }
	  else
	    {
	      char *p = frag->fr_literal;

	      valueT value = bi->value << (16 - size);
	      md_number_to_chars (p, value, 2);
	      if ((frag->tc_frag_data = size) == 16)
		frag->tc_frag_data = 0;
	      growth = 0;
	    }
	}
      else
	{
	  if (bit_offset != 0 && bit_offset < 16)
	    {
	      if (available >= size)
		{
		  if ((prev_frag->tc_frag_data += size) == 16)
		    prev_frag->tc_frag_data = 0;
		  if (bi->sym)
		    symbol_set_frag (bi->sym, prev_frag);
		  /* This frag is no longer used.  */
		  growth = -frag->fr_fix;
		  frag->fr_fix = 0;
		  frag->tc_frag_data = 0;
		  goto getout;
		}
	      if (bi->type == TYPE_SPACE && bi->sym)
		symbol_set_frag (bi->sym, prev_frag);
	      size -= available;
	    }
	  growth = (size + 15) / 16 * OCTETS_PER_BYTE - frag->fr_fix;
	  for (i = 0; i < growth; i++)
	    frag->fr_literal[i] = 0;
	  frag->fr_fix = growth;
	  frag->tc_frag_data = size % 16;
	  /* Make sure any BES label points to the LAST word allocated.  */
	  if (bi->type == TYPE_BES && bi->sym)
	    S_SET_VALUE (bi->sym, frag->fr_fix / OCTETS_PER_BYTE - 1);
	}
    getout:
      frag->fr_symbol = 0;
      frag->fr_opcode = 0;
      free ((void *) bi);
    }
  return growth;
}

void
tic54x_convert_frag (abfd, seg, frag)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
     fragS *frag;
{
  /* Offset is in bytes.  */
  frag->fr_offset = (frag->fr_next->fr_address
		     - frag->fr_address
		     - frag->fr_fix) / frag->fr_var;
  if (frag->fr_offset < 0)
    {
      as_bad_where (frag->fr_file, frag->fr_line,
		    _("attempt to .space/.bes backwards? (%ld)"),
		    (long) frag->fr_offset);
    }
  frag->fr_type = rs_space;
}

/* We need to avoid having labels defined for certain directives/pseudo-ops
   since once the label is defined, it's in the symbol table for good.  TI
   syntax puts the symbol *before* the pseudo (which is kinda like MRI syntax,
   I guess, except I've never seen a definition of MRI syntax).

   C is the character that used to be at *REST, which points to the end of the
   label.

   Don't allow labels to start with '.'  */

int
tic54x_start_label (c, rest)
     int c;
     char *rest;
{
  /* If within .struct/.union, no auto line labels, please.  */
  if (current_stag != NULL)
    return 0;

  /* Disallow labels starting with "."  */
  if (c != ':')
    {
      char *label = rest;

      while (!is_end_of_line[(int) label[-1]])
	--label;
      if (*label == '.')
	{
	  as_bad (_("Invalid label '%s'"), label);
	  return 0;
	}
    }

  if (is_end_of_line[(int) c])
    return 1;

  if (ISSPACE (c))
    while (ISSPACE (c = *++rest))
      ;
  if (c == '.')
    {
      /* Don't let colon () define a label for any of these...  */
      return (strncasecmp (rest, ".tag", 4) != 0 || !ISSPACE (rest[4]))
	&& (strncasecmp (rest, ".struct", 7) != 0 || !ISSPACE (rest[7]))
	&& (strncasecmp (rest, ".union", 6) != 0 || !ISSPACE (rest[6]))
	&& (strncasecmp (rest, ".macro", 6) != 0 || !ISSPACE (rest[6]))
	&& (strncasecmp (rest, ".set", 4) != 0 || !ISSPACE (rest[4]))
	&& (strncasecmp (rest, ".equ", 4) != 0 || !ISSPACE (rest[4]));
    }

  return 1;
}
