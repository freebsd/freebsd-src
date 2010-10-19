/* tc-cris.c -- Assembler code for the CRIS CPU core.
   Copyright 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   Contributed by Axis Communications AB, Lund, Sweden.
   Originally written for GAS 1.38.1 by Mikael Asker.
   Updates, BFDizing, GNUifying and ELF support by Hans-Peter Nilsson.

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
   along with GAS; see the file COPYING.  If not, write to the
   Free Software Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include <stdio.h>
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "opcode/cris.h"
#include "dwarf2dbg.h"

/* Conventions used here:
   Generally speaking, pointers to binutils types such as "fragS" and
   "expressionS" get parameter and variable names ending in "P", such as
   "fragP", to harmonize with the rest of the binutils code.  Other
   pointers get a "p" suffix, such as "bufp".  Any function or type-name
   that could clash with a current or future binutils or GAS function get
   a "cris_" prefix.  */

#define SYNTAX_RELAX_REG_PREFIX "no_register_prefix"
#define SYNTAX_ENFORCE_REG_PREFIX "register_prefix"
#define SYNTAX_USER_SYM_LEADING_UNDERSCORE "leading_underscore"
#define SYNTAX_USER_SYM_NO_LEADING_UNDERSCORE "no_leading_underscore"
#define REGISTER_PREFIX_CHAR '$'

/* True for expressions where getting X_add_symbol and X_add_number is
   enough to get the "base" and "offset"; no need to make_expr_symbol.
   It's not enough to check if X_op_symbol is NULL; that misses unary
   operations like O_uminus.  */
#define SIMPLE_EXPR(EXP) \
 ((EXP)->X_op == O_constant || (EXP)->X_op == O_symbol)

/* Like in ":GOT", ":GOTOFF" etc.  Other ports use '@', but that's in
   line_separator_chars for CRIS, so we avoid it.  */
#define PIC_SUFFIX_CHAR ':'

/* This might be CRIS_INSN_NONE if we're assembling a prefix-insn only.
   Note that some prefix-insns might be assembled as CRIS_INSN_NORMAL.  */
enum cris_insn_kind
{
  CRIS_INSN_NORMAL, CRIS_INSN_NONE, CRIS_INSN_BRANCH, CRIS_INSN_MUL
};

/* An instruction will have one of these prefixes.
   Although the same bit-pattern, we handle BDAP with an immediate
   expression (eventually quick or [pc+]) different from when we only have
   register expressions.  */
enum prefix_kind
{
  PREFIX_NONE, PREFIX_BDAP_IMM, PREFIX_BDAP, PREFIX_BIAP, PREFIX_DIP,
  PREFIX_PUSH
};

/* The prefix for an instruction.  */
struct cris_prefix
{
  enum prefix_kind kind;
  int base_reg_number;
  unsigned int opcode;

  /* There might be an expression to be evaluated, like I in [rN+I].  */
  expressionS expr;

  /* If there's an expression, we might need a relocation.  Here's the
     type of what relocation to start relaxaton with.
     The relocation is assumed to start immediately after the prefix insn,
     so we don't provide an offset.  */
  enum bfd_reloc_code_real reloc;
};

/* The description of the instruction being assembled.  */
struct cris_instruction
{
  /* If CRIS_INSN_NONE, then this insn is of zero length.  */
  enum cris_insn_kind insn_type;

  /* If a special register was mentioned, this is its description, else
     it is NULL.  */
  const struct cris_spec_reg *spec_reg;

  unsigned int opcode;

  /* An insn may have at most one expression; theoretically there could be
     another in its prefix (but I don't see how that could happen).  */
  expressionS expr;

  /* The expression might need a relocation.  Here's one to start
     relaxation with.  */
  enum bfd_reloc_code_real reloc;

  /* The size in bytes of an immediate expression, or zero if
     nonapplicable.  */
  int imm_oprnd_size;
};

enum cris_archs
{
  arch_cris_unknown,
  arch_crisv0, arch_crisv3, arch_crisv8, arch_crisv10,
  arch_cris_any_v0_v10, arch_crisv32, arch_cris_common_v10_v32
};

static enum cris_archs cris_arch_from_string (char **);
static int cris_insn_ver_valid_for_arch (enum cris_insn_version_usage,
					 enum cris_archs);

static void cris_process_instruction (char *, struct cris_instruction *,
				      struct cris_prefix *);
static int get_bwd_size_modifier (char **, int *);
static int get_bw_size_modifier (char **, int *);
static int get_gen_reg (char **, int *);
static int get_spec_reg (char **, const struct cris_spec_reg **);
static int get_sup_reg (char **, int *);
static int get_autoinc_prefix_or_indir_op (char **, struct cris_prefix *,
					   int *, int *, int *,
					   expressionS *);
static int get_3op_or_dip_prefix_op (char **, struct cris_prefix *);
static int cris_get_expression (char **, expressionS *);
static int get_flags (char **, int *);
static void gen_bdap (int, expressionS *);
static int branch_disp (int);
static void gen_cond_branch_32 (char *, char *, fragS *, symbolS *, symbolS *,
				long int);
static void cris_number_to_imm (char *, long, int, fixS *, segT);
static void cris_create_short_jump (char *, addressT, addressT, fragS *,
				    symbolS *);
static void s_syntax (int);
static void s_cris_file (int);
static void s_cris_loc (int);
static void s_cris_arch (int);

/* Get ":GOT", ":GOTOFF", ":PLT" etc. suffixes.  */
static void cris_get_pic_suffix (char **, bfd_reloc_code_real_type *,
				 expressionS *);
static unsigned int cris_get_pic_reloc_size (bfd_reloc_code_real_type);

/* All the .syntax functions.  */
static void cris_force_reg_prefix (void);
static void cris_relax_reg_prefix (void);
static void cris_sym_leading_underscore (void);
static void cris_sym_no_leading_underscore (void);
static char *cris_insn_first_word_frag (void);

/* Handle to the opcode hash table.  */
static struct hash_control *op_hash = NULL;

/* If we target cris-axis-linux-gnu (as opposed to generic cris-axis-elf),
   we default to no underscore and required register-prefixes.  The
   difference is in the default values.  */
#ifdef TE_LINUX
#define DEFAULT_CRIS_AXIS_LINUX_GNU TRUE
#else
#define DEFAULT_CRIS_AXIS_LINUX_GNU FALSE
#endif

/* Whether we demand that registers have a `$' prefix.  Default here.  */
static bfd_boolean demand_register_prefix = DEFAULT_CRIS_AXIS_LINUX_GNU;

/* Whether global user symbols have a leading underscore.  Default here.  */
static bfd_boolean symbols_have_leading_underscore
  = !DEFAULT_CRIS_AXIS_LINUX_GNU;

/* Whether or not we allow PIC, and expand to PIC-friendly constructs.  */
static bfd_boolean pic = FALSE;

/* If we're configured for "cris", default to allow all v0..v10
   instructions and register names.  */
#ifndef DEFAULT_CRIS_ARCH
#define DEFAULT_CRIS_ARCH cris_any_v0_v10
#endif

/* No whitespace in the CONCAT2 parameter list.  */
static enum cris_archs cris_arch = XCONCAT2 (arch_,DEFAULT_CRIS_ARCH);

const pseudo_typeS md_pseudo_table[] =
{
  {"dword", cons, 4},
  {"syntax", s_syntax, 0},
  {"file", s_cris_file, 0},
  {"loc", s_cris_loc, 0},
  {"arch", s_cris_arch, 0},
  {NULL, 0, 0}
};

static int warn_for_branch_expansion = 0;

/* Whether to emit error when a MULS/MULU could be located last on a
   cache-line.  */
static int err_for_dangerous_mul_placement
 = (XCONCAT2 (arch_,DEFAULT_CRIS_ARCH) != arch_crisv32);

const char cris_comment_chars[] = ";";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.  */
/* Note that input_file.c hand-checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that slash-star will always start a comment.  */
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "@";

/* Now all floating point support is shut off.  See md_atof.  */
const char EXP_CHARS[] = "";
const char FLT_CHARS[] = "";

/* For CRIS, we encode the relax_substateTs (in e.g. fr_substate) as:
		       2		 1		   0
      ---/ /--+-----------------+-----------------+-----------------+
	      |	 what state ?	|	     how long ?		    |
      ---/ /--+-----------------+-----------------+-----------------+

   The "how long" bits are 00 = byte, 01 = word, 10 = dword (long).
   Not all lengths are legit for a given value of (what state).

   Groups for CRIS address relaxing:

   1. Bcc (pre-V32)
      length: byte, word, 10-byte expansion

   2. BDAP
      length: byte, word, dword

   3. MULS/MULU
      Not really a relaxation (no infrastructure to get delay-slots
      right), just an alignment and placement checker for the v10
      multiply/cache-bug.

   4. Bcc (V32 and later)
      length: byte, word, 14-byte expansion

   5. Bcc (V10+V32)
      length: byte, word, error

   6. BA (V32)
      length: byte, word, dword

   7. LAPC (V32)
      length: byte, dword
  */

#define STATE_COND_BRANCH           (1)
#define STATE_BASE_PLUS_DISP_PREFIX (2)
#define STATE_MUL		    (3)
#define STATE_COND_BRANCH_V32       (4)
#define STATE_COND_BRANCH_COMMON    (5)
#define STATE_ABS_BRANCH_V32	    (6)
#define STATE_LAPC		    (7)
#define STATE_COND_BRANCH_PIC       (8)

#define STATE_LENGTH_MASK	    (3)
#define STATE_BYTE		    (0)
#define STATE_WORD		    (1)
#define STATE_DWORD		    (2)
/* Symbol undefined.  */
#define STATE_UNDF		    (3)
#define STATE_MAX_LENGTH	    (3)

/* These displacements are relative to the address following the opcode
   word of the instruction.  The first letter is Byte, Word.  The 2nd
   letter is Forward, Backward.  */

#define BRANCH_BF ( 254)
#define BRANCH_BB (-256)
#define BRANCH_BF_V32 ( 252)
#define BRANCH_BB_V32 (-258)
#define BRANCH_WF (2 +  32767)
#define BRANCH_WB (2 + -32768)
#define BRANCH_WF_V32 (-2 + 32767)
#define BRANCH_WB_V32 (-2 + -32768)

#define BDAP_BF	  ( 127)
#define BDAP_BB	  (-128)
#define BDAP_WF	  ( 32767)
#define BDAP_WB	  (-32768)

#define ENCODE_RELAX(what, length) (((what) << 2) + (length))

const relax_typeS md_cris_relax_table[] =
{
  /* Error sentinel (0, 0).  */
  {1,	      1,	 0,  0},

  /* Unused (0, 1).  */
  {1,	      1,	 0,  0},

  /* Unused (0, 2).  */
  {1,	      1,	 0,  0},

  /* Unused (0, 3).  */
  {1,	      1,	 0,  0},

  /* Bcc o (1, 0).  */
  {BRANCH_BF, BRANCH_BB, 0,  ENCODE_RELAX (1, 1)},

  /* Bcc [PC+] (1, 1).  */
  {BRANCH_WF, BRANCH_WB, 2,  ENCODE_RELAX (1, 2)},

  /* BEXT/BWF, BA, JUMP (external), JUMP (always), Bnot_cc, JUMP (default)
     (1, 2).  */
  {0,	      0,	 10, 0},

  /* Unused (1, 3).  */
  {1,	      1,	 0,  0},

  /* BDAP o (2, 0).  */
  {BDAP_BF,   BDAP_BB,	 0,  ENCODE_RELAX (2, 1)},

  /* BDAP.[bw] [PC+] (2, 1).  */
  {BDAP_WF,   BDAP_WB,	 2,  ENCODE_RELAX (2, 2)},

  /* BDAP.d [PC+] (2, 2).  */
  {0,	      0,	 4,  0},

  /* Unused (2, 3).  */
  {1,	      1,	 0,  0},

  /* MULS/MULU (3, 0).  Positions (3, 1..3) are unused.  */
  {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},

  /* V32: Bcc o (4, 0).  */
  {BRANCH_BF_V32, BRANCH_BB_V32, 0,  ENCODE_RELAX (4, 1)},

  /* V32: Bcc [PC+] (4, 1).  */
  {BRANCH_WF_V32, BRANCH_WB_V32, 2,  ENCODE_RELAX (4, 2)},

  /* V32: BA .+12; NOP; BA32 target; NOP; Bcc .-6  (4, 2).  */
  {0,	      0,	 12, 0},

  /* Unused (4, 3).  */
  {1,	      1,	 0,  0},

  /* COMMON: Bcc o (5, 0).  The offsets are calculated as for v32.  Code
     should contain two nop insns (or four if offset size is large or
     unknown) after every label.  */
  {BRANCH_BF_V32, BRANCH_BB_V32, 0,  ENCODE_RELAX (5, 1)},

  /* COMMON: Bcc [PC+] (5, 1).  */
  {BRANCH_WF_V32, BRANCH_WB_V32, 2,  ENCODE_RELAX (5, 2)},

  /* COMMON: FIXME: ???.  Treat as error currently.  */
  {0,	      0,	 12, 0},

  /* Unused (5, 3).  */
  {1,	      1,	 0,  0},

  /* V32: BA o (6, 0).  */
  {BRANCH_BF_V32, BRANCH_BB_V32, 0,  ENCODE_RELAX (6, 1)},

  /* V32: BA.W (6, 1).  */
  {BRANCH_WF_V32, BRANCH_WB_V32, 2,  ENCODE_RELAX (6, 2)},

  /* V32: BA.D (6, 2).  */
  {0,	      0,	 4, 0},

  /* Unused (6, 3).  */
  {1,	      1,	 0,  0},

  /* LAPC: LAPCQ .+0..15*2,Rn  (7, 0).  */
  {14*2,   -1*2,	 0,  ENCODE_RELAX (7, 2)},

  /* Unused (7, 1).
     While there's a shorter sequence, e.g. LAPCQ + an ADDQ or SUBQ,
     that would affect flags, so we can't do that as it wouldn't be a
     proper insn expansion of LAPCQ.  This row is associated with a
     2-byte expansion, so it's unused rather than the next.  */
  {1,	      1,	 0,  0},

  /* LAPC: LAPC.D (7, 2).  */
  {0,	      0,	 4, 0},

  /* Unused (7, 3).  */
  {1,	      1,	 0,  0},

  /* PIC for pre-v32: Bcc o (8, 0).  */
  {BRANCH_BF, BRANCH_BB, 0,  ENCODE_RELAX (STATE_COND_BRANCH_PIC, 1)},

  /* Bcc [PC+] (8, 1).  */
  {BRANCH_WF, BRANCH_WB, 2,  ENCODE_RELAX (STATE_COND_BRANCH_PIC, 2)},

  /* 32-bit expansion, PIC (8, 2).  */
  {0,	      0,	 12, 0},

  /* Unused (8, 3).  */
  {1,	      1,	 0,  0}
};

#undef BDAP_BF
#undef BDAP_BB
#undef BDAP_WF
#undef BDAP_WB

/* Target-specific multicharacter options, not const-declared.  */
struct option md_longopts[] =
{
#define OPTION_NO_US (OPTION_MD_BASE + 0)
  {"no-underscore", no_argument, NULL, OPTION_NO_US},
#define OPTION_US (OPTION_MD_BASE + 1)
  {"underscore", no_argument, NULL, OPTION_US},
#define OPTION_PIC (OPTION_US + 1)
  {"pic", no_argument, NULL, OPTION_PIC},
#define OPTION_MULBUG_ABORT_ON (OPTION_PIC + 1)
  {"mul-bug-abort", no_argument, NULL, OPTION_MULBUG_ABORT_ON},
#define OPTION_MULBUG_ABORT_OFF (OPTION_MULBUG_ABORT_ON + 1)
  {"no-mul-bug-abort", no_argument, NULL, OPTION_MULBUG_ABORT_OFF},
#define OPTION_ARCH (OPTION_MULBUG_ABORT_OFF + 1)
  {"march", required_argument, NULL, OPTION_ARCH},
  {NULL, no_argument, NULL, 0}
};

/* Not const-declared.  */
size_t md_longopts_size = sizeof (md_longopts);
const char *md_shortopts = "hHN";

/* At first glance, this may seems wrong and should be 4 (ba + nop); but
   since a short_jump must skip a *number* of long jumps, it must also be
   a long jump.  Here, we hope to make it a "ba [16bit_offs]" and a "nop"
   for the delay slot and hope that the jump table at most needs
   32767/4=8191 long-jumps.  A branch is better than a jump, since it is
   relative; we will not have a reloc to fix up somewhere.

   Note that we can't add relocs, because relaxation uses these fixed
   numbers, and md_create_short_jump is called after relaxation.  */

int md_short_jump_size = 6;

/* The v32 version has a delay-slot, hence two bytes longer.
   The pre-v32 PIC version uses a prefixed insn.  */
#define cris_any_v0_v10_long_jump_size 6
#define cris_any_v0_v10_long_jump_size_pic 8
#define crisv32_long_jump_size 8

int md_long_jump_size = XCONCAT2 (DEFAULT_CRIS_ARCH,_long_jump_size);

/* Report output format.  Small changes in output format (like elf
   variants below) can happen until all options are parsed, but after
   that, the output format must remain fixed.  */

const char *
cris_target_format (void)
{
  switch (OUTPUT_FLAVOR)
    {
    case bfd_target_aout_flavour:
      return "a.out-cris";

    case bfd_target_elf_flavour:
      if (symbols_have_leading_underscore)
	return "elf32-us-cris";
      return "elf32-cris";

    default:
      abort ();
      return NULL;
    }
}

/* Return a bfd_mach_cris... value corresponding to the value of
   cris_arch.  */

unsigned int
cris_mach (void)
{
  unsigned int retval = 0;

  switch (cris_arch)
    {
    case arch_cris_common_v10_v32:
      retval = bfd_mach_cris_v10_v32;
      break;

    case arch_crisv32:
      retval = bfd_mach_cris_v32;
      break;

    case arch_crisv10:
    case arch_cris_any_v0_v10:
      retval = bfd_mach_cris_v0_v10;
      break;

    default:
      BAD_CASE (cris_arch);
    }

  return retval;
}

/* We need a port-specific relaxation function to cope with sym2 - sym1
   relative expressions with both symbols in the same segment (but not
   necessarily in the same frag as this insn), for example:
     move.d [pc+sym2-(sym1-2)],r10
    sym1:
   The offset can be 8, 16 or 32 bits long.  */

long
cris_relax_frag (segT seg ATTRIBUTE_UNUSED, fragS *fragP,
		 long stretch ATTRIBUTE_UNUSED)
{
  long growth;
  offsetT aim = 0;
  symbolS *symbolP;
  const relax_typeS *this_type;
  const relax_typeS *start_type;
  relax_substateT next_state;
  relax_substateT this_state;
  const relax_typeS *table = TC_GENERIC_RELAX_TABLE;

  /* We only have to cope with frags as prepared by
     md_estimate_size_before_relax.  The dword cases may get here
     because of the different reasons that they aren't relaxable.  */
  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_DWORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_DWORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_DWORD):
    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_DWORD):
    case ENCODE_RELAX (STATE_LAPC, STATE_DWORD):
    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_DWORD):
      /* When we get to these states, the frag won't grow any more.  */
      return 0;

    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_WORD):
    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_BYTE):
      if (fragP->fr_symbol == NULL
	  || S_GET_SEGMENT (fragP->fr_symbol) != absolute_section)
	as_fatal (_("internal inconsistency problem in %s: fr_symbol %lx"),
		  __FUNCTION__, (long) fragP->fr_symbol);
      symbolP = fragP->fr_symbol;
      if (symbol_resolved_p (symbolP))
	as_fatal (_("internal inconsistency problem in %s: resolved symbol"),
		  __FUNCTION__);
      aim = S_GET_VALUE (symbolP);
      break;

    case ENCODE_RELAX (STATE_MUL, STATE_BYTE):
      /* Nothing to do here.  */
      return 0;

    default:
      as_fatal (_("internal inconsistency problem in %s: fr_subtype %d"),
		  __FUNCTION__, fragP->fr_subtype);
    }

  /* The rest is stolen from relax_frag.  There's no obvious way to
     share the code, but fortunately no requirement to keep in sync as
     long as fragP->fr_symbol does not have its segment changed.  */

  this_state = fragP->fr_subtype;
  start_type = this_type = table + this_state;

  if (aim < 0)
    {
      /* Look backwards.  */
      for (next_state = this_type->rlx_more; next_state;)
	if (aim >= this_type->rlx_backward)
	  next_state = 0;
	else
	  {
	    /* Grow to next state.  */
	    this_state = next_state;
	    this_type = table + this_state;
	    next_state = this_type->rlx_more;
	  }
    }
  else
    {
      /* Look forwards.  */
      for (next_state = this_type->rlx_more; next_state;)
	if (aim <= this_type->rlx_forward)
	  next_state = 0;
	else
	  {
	    /* Grow to next state.  */
	    this_state = next_state;
	    this_type = table + this_state;
	    next_state = this_type->rlx_more;
	  }
    }

  growth = this_type->rlx_length - start_type->rlx_length;
  if (growth != 0)
    fragP->fr_subtype = this_state;
  return growth;
}

/* Prepare machine-dependent frags for relaxation.

   Called just before relaxation starts. Any symbol that is now undefined
   will not become defined.

   Return the correct fr_subtype in the frag.

   Return the initial "guess for fr_var" to caller.  The guess for fr_var
   is *actually* the growth beyond fr_fix. Whatever we do to grow fr_fix
   or fr_var contributes to our returned value.

   Although it may not be explicit in the frag, pretend
   fr_var starts with a value.  */

int
md_estimate_size_before_relax (fragS *fragP, segT segment_type)
{
  int old_fr_fix;
  symbolS *symbolP = fragP->fr_symbol;

#define HANDLE_RELAXABLE(state)						\
    case ENCODE_RELAX (state, STATE_UNDF):				\
      if (symbolP != NULL						\
	  && S_GET_SEGMENT (symbolP) == segment_type			\
	  && !S_IS_WEAK (symbolP))					\
	/* The symbol lies in the same segment - a relaxable		\
	   case.  */							\
	fragP->fr_subtype						\
	  = ENCODE_RELAX (state, STATE_BYTE);				\
      else								\
	/* Unknown or not the same segment, so not relaxable.  */	\
	fragP->fr_subtype						\
	  = ENCODE_RELAX (state, STATE_DWORD);				\
      fragP->fr_var							\
	= md_cris_relax_table[fragP->fr_subtype].rlx_length;		\
      break

  old_fr_fix = fragP->fr_fix;

  switch (fragP->fr_subtype)
    {
      HANDLE_RELAXABLE (STATE_COND_BRANCH);
      HANDLE_RELAXABLE (STATE_COND_BRANCH_V32);
      HANDLE_RELAXABLE (STATE_COND_BRANCH_COMMON);
      HANDLE_RELAXABLE (STATE_COND_BRANCH_PIC);
      HANDLE_RELAXABLE (STATE_ABS_BRANCH_V32);

    case ENCODE_RELAX (STATE_LAPC, STATE_UNDF):
      if (symbolP != NULL
	  && S_GET_SEGMENT (symbolP) == segment_type
	  && !S_IS_WEAK (symbolP))
	{
	  /* The symbol lies in the same segment - a relaxable case.
	     Check if we currently have an odd offset; we can't code
	     that into the instruction.  Relaxing presumably only cause
	     multiple-of-two changes, so we should only need to adjust
	     for that here.  */
	  bfd_vma target_address
	    = (symbolP
	       ? S_GET_VALUE (symbolP)
	       : 0) + fragP->fr_offset;
	  bfd_vma var_part_offset = fragP->fr_fix;
	  bfd_vma address_of_var_part = fragP->fr_address + var_part_offset;
	  long offset = target_address - (address_of_var_part - 2);

	  fragP->fr_subtype
	    = (offset & 1)
	    ? ENCODE_RELAX (STATE_LAPC, STATE_DWORD)
	    : ENCODE_RELAX (STATE_LAPC, STATE_BYTE);
	}
      else
	/* Unknown or not the same segment, so not relaxable.  */
	fragP->fr_subtype
	  = ENCODE_RELAX (STATE_LAPC, STATE_DWORD);
      fragP->fr_var
	= md_cris_relax_table[fragP->fr_subtype].rlx_length;
      break;

    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_UNDF):
      /* Note that we can not do anything sane with relaxing
	 [rX + a_known_symbol_in_text], it will have to be a 32-bit
	 value.

	 We could play tricks with managing a constant pool and make
	 a_known_symbol_in_text a "bdap [pc + offset]" pointing there
	 (like the GOT for ELF shared libraries), but that's no use, it
	 would in general be no shorter or faster code, only more
	 complicated.  */

      if (S_GET_SEGMENT (symbolP) != absolute_section)
	{
	  /* Go for dword if not absolute or same segment.  */
	  fragP->fr_subtype
	    = ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_DWORD);
	  fragP->fr_var = md_cris_relax_table[fragP->fr_subtype].rlx_length;
	}
      else if (!symbol_resolved_p (fragP->fr_symbol))
	{
	  /* The symbol will eventually be completely resolved as an
	     absolute expression, but right now it depends on the result
	     of relaxation and we don't know anything else about the
	     value.  We start relaxation with the assumption that it'll
	     fit in a byte.  */
	  fragP->fr_subtype
	    = ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_BYTE);
	  fragP->fr_var = md_cris_relax_table[fragP->fr_subtype].rlx_length;
	}
      else
	{
	  /* Absolute expression.  */
	  long int value;
	  value = (symbolP != NULL
		   ? S_GET_VALUE (symbolP) : 0) + fragP->fr_offset;

	  if (value >= -128 && value <= 127)
	    {
	      /* Byte displacement.  */
	      (fragP->fr_opcode)[0] = value;
	    }
	  else
	    {
	      /* Word or dword displacement.  */
	      int pow2_of_size = 1;
	      char *writep;

	      if (value < -32768 || value > 32767)
		{
		  /* Outside word range, make it a dword.  */
		  pow2_of_size = 2;
		}

	      /* Modify the byte-offset BDAP into a word or dword offset
		 BDAP.	Or really, a BDAP rX,8bit into a
		 BDAP.[wd] rX,[PC+] followed by a word or dword.  */
	      (fragP->fr_opcode)[0] = BDAP_PC_LOW + pow2_of_size * 16;

	      /* Keep the register number in the highest four bits.  */
	      (fragP->fr_opcode)[1] &= 0xF0;
	      (fragP->fr_opcode)[1] |= BDAP_INCR_HIGH;

	      /* It grew by two or four bytes.  */
	      fragP->fr_fix += 1 << pow2_of_size;
	      writep = fragP->fr_literal + old_fr_fix;
	      md_number_to_chars (writep, value, 1 << pow2_of_size);
	    }
	  frag_wane (fragP);
	}
      break;

    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_DWORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_PIC, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH_PIC, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_PIC, STATE_DWORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_DWORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_DWORD):
    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_BYTE):
    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_WORD):
    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_DWORD):
    case ENCODE_RELAX (STATE_LAPC, STATE_BYTE):
    case ENCODE_RELAX (STATE_LAPC, STATE_DWORD):
    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_BYTE):
    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_WORD):
    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_DWORD):
      /* When relaxing a section for the second time, we don't need to
	 do anything except making sure that fr_var is set right.  */
      fragP->fr_var = md_cris_relax_table[fragP->fr_subtype].rlx_length;
      break;

    case ENCODE_RELAX (STATE_MUL, STATE_BYTE):
      /* Nothing to do here.  */
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
    }

  return fragP->fr_var + (fragP->fr_fix - old_fr_fix);
}

/* Perform post-processing of machine-dependent frags after relaxation.
   Called after relaxation is finished.
   In:	Address of frag.
	fr_type == rs_machine_dependent.
	fr_subtype is what the address relaxed to.

   Out: Any fixS:s and constants are set up.

   The caller will turn the frag into a ".space 0".  */

void
md_convert_frag (bfd *abfd ATTRIBUTE_UNUSED, segT sec ATTRIBUTE_UNUSED,
		 fragS *fragP)
{
  /* Pointer to first byte in variable-sized part of the frag.  */
  char *var_partp;

  /* Pointer to first opcode byte in frag.  */
  char *opcodep;

  /* Used to check integrity of the relaxation.
     One of 2 = long, 1 = word, or 0 = byte.  */
  int length_code;

  /* Size in bytes of variable-sized part of frag.  */
  int var_part_size = 0;

  /* This is part of *fragP.  It contains all information about addresses
     and offsets to varying parts.  */
  symbolS *symbolP;
  unsigned long var_part_offset;

  /* Where, in file space, is _var of *fragP?  */
  unsigned long address_of_var_part = 0;

  /* Where, in file space, does addr point?  */
  unsigned long target_address;

  know (fragP->fr_type == rs_machine_dependent);

  length_code = fragP->fr_subtype & STATE_LENGTH_MASK;
  know (length_code >= 0 && length_code < STATE_MAX_LENGTH);

  var_part_offset = fragP->fr_fix;
  var_partp = fragP->fr_literal + var_part_offset;
  opcodep = fragP->fr_opcode;

  symbolP = fragP->fr_symbol;
  target_address = (symbolP ? S_GET_VALUE (symbolP) : 0) + fragP->fr_offset;
  address_of_var_part = fragP->fr_address + var_part_offset;

  switch (fragP->fr_subtype)
    {
    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH_PIC, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_BYTE):
    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_BYTE):
    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_BYTE):
      opcodep[0] = branch_disp ((target_address - address_of_var_part));
      var_part_size = 0;
      break;

    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_PIC, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_WORD):
    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_WORD):
    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_WORD):
      /* We had a quick immediate branch, now turn it into a word one i.e. a
	 PC autoincrement.  */
      opcodep[0] = BRANCH_PC_LOW;
      opcodep[1] &= 0xF0;
      opcodep[1] |= BRANCH_INCR_HIGH;
      md_number_to_chars (var_partp,
			  (long)
			  (target_address
			   - (address_of_var_part
			      + (cris_arch == arch_crisv32
				 || cris_arch == arch_cris_common_v10_v32
				 ? -2 : 2))),
			  2);
      var_part_size = 2;
      break;

    case ENCODE_RELAX (STATE_COND_BRANCH, STATE_DWORD):
      gen_cond_branch_32 (fragP->fr_opcode, var_partp, fragP,
			  fragP->fr_symbol, (symbolS *) NULL,
			  fragP->fr_offset);
      /* Ten bytes added: a branch, nop and a jump.  */
      var_part_size = 2 + 2 + 4 + 2;
      break;

    case ENCODE_RELAX (STATE_COND_BRANCH_PIC, STATE_DWORD):
      gen_cond_branch_32 (fragP->fr_opcode, var_partp, fragP,
			  fragP->fr_symbol, (symbolS *) NULL,
			  fragP->fr_offset);
      /* Twelve bytes added: a branch, nop and a pic-branch-32.  */
      var_part_size = 2 + 2 + 4 + 2 + 2;
      break;

    case ENCODE_RELAX (STATE_COND_BRANCH_V32, STATE_DWORD):
      gen_cond_branch_32 (fragP->fr_opcode, var_partp, fragP,
			  fragP->fr_symbol, (symbolS *) NULL,
			  fragP->fr_offset);
      /* Twelve bytes added: a branch, nop and another branch and nop.  */
      var_part_size = 2 + 2 + 2 + 4 + 2;
      break;

    case ENCODE_RELAX (STATE_COND_BRANCH_COMMON, STATE_DWORD):
      as_bad_where (fragP->fr_file, fragP->fr_line,
		    _("Relaxation to long branches for .arch common_v10_v32\
 not implemented"));
      /* Pretend we have twelve bytes for sake of quelling further
         errors.  */
      var_part_size = 2 + 2 + 2 + 4 + 2;
      break;

    case ENCODE_RELAX (STATE_ABS_BRANCH_V32, STATE_DWORD):
      /* We had a quick immediate branch or a word immediate ba.  Now
	 turn it into a dword one.  */
      opcodep[0] = BA_DWORD_OPCODE & 255;
      opcodep[1] = (BA_DWORD_OPCODE >> 8) & 255;
      fix_new (fragP, var_partp - fragP->fr_literal, 4, symbolP,
	       fragP->fr_offset + 6, 1, BFD_RELOC_32_PCREL);
      var_part_size = 4;
      break;

    case ENCODE_RELAX (STATE_LAPC, STATE_BYTE):
      {
	long offset = target_address - (address_of_var_part - 2);

	/* This is mostly a sanity check; useful occurrences (if there
	   really are any) should have been caught in
	   md_estimate_size_before_relax.  We can (at least
	   theoretically) stumble over invalid code with odd sizes and
	   .p2aligns within the code, so emit an error if that happens.
	   (The generic relaxation machinery is not fit to check this.)  */

	if (offset & 1)
	  as_bad_where (fragP->fr_file, fragP->fr_line,
		    _("Complicated LAPC target operand is not\
 a multiple of two.  Use LAPC.D"));

	/* FIXME: This *is* a sanity check.  Remove when done with.  */
	if (offset > 15*2 || offset < 0)
	  as_fatal (_("Internal error found in md_convert_frag: offset %ld.\
  Please report this."),
		    offset);

	opcodep[0] |= (offset / 2) & 0xf;
	var_part_size = 0;
      }
      break;

    case ENCODE_RELAX (STATE_LAPC, STATE_DWORD):
      {
	md_number_to_chars (opcodep,
			    LAPC_DWORD_OPCODE + (opcodep[1] & 0xf0) * 256,
			    2);
	/* Remember that the reloc is against the position *after* the
	   relocated contents, so we need to adjust to the start of
	   the insn.  */
	fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		 fragP->fr_offset + 6, 1, BFD_RELOC_32_PCREL);
	var_part_size = 4;
      }
      break;

    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_BYTE):
      if (symbolP == NULL)
	as_fatal (_("internal inconsistency in %s: bdapq no symbol"),
		    __FUNCTION__);
      opcodep[0] = S_GET_VALUE (symbolP);
      var_part_size = 0;
      break;

    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_WORD):
      /* We had a BDAP 8-bit "quick immediate", now turn it into a 16-bit
	 one that uses PC autoincrement.  */
      opcodep[0] = BDAP_PC_LOW + (1 << 4);
      opcodep[1] &= 0xF0;
      opcodep[1] |= BDAP_INCR_HIGH;
      if (symbolP == NULL)
	as_fatal (_("internal inconsistency in %s: bdap.w with no symbol"),
		  __FUNCTION__);
      md_number_to_chars (var_partp, S_GET_VALUE (symbolP), 2);
      var_part_size = 2;
      break;

    case ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_DWORD):
      /* We had a BDAP 16-bit "word", change the offset to a dword.  */
      opcodep[0] = BDAP_PC_LOW + (2 << 4);
      opcodep[1] &= 0xF0;
      opcodep[1] |= BDAP_INCR_HIGH;
      if (fragP->fr_symbol == NULL)
	md_number_to_chars (var_partp, fragP->fr_offset, 4);
      else
	fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		 fragP->fr_offset, 0, BFD_RELOC_32);
      var_part_size = 4;
      break;

    case ENCODE_RELAX (STATE_MUL, STATE_BYTE):
      /* This is the only time we check position and aligmnent of the
	 placement-tracking frag.  */
      if (sec->alignment_power < 2)
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("section alignment must be >= 4 bytes to check MULS/MULU safeness"));
      else
	{
	  /* If the address after the MULS/MULU has alignment which is
	     that of the section and may be that of a cache-size of the
	     buggy versions, then the MULS/MULU can be placed badly.  */
	  if ((address_of_var_part
	       & ((1 << sec->alignment_power) - 1) & 31) == 0)
	    as_bad_where (fragP->fr_file, fragP->fr_line,
			  _("dangerous MULS/MULU location; give it higher alignment"));
	}
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
      break;
    }

  fragP->fr_fix += var_part_size;
}

/* Generate a short jump around a secondary jump table.
   Used by md_create_long_jump.

   This used to be md_create_short_jump, but is now called from
   md_create_long_jump instead, when sufficient, since the sizes of the
   jumps are the same for pre-v32.  */

static void
cris_create_short_jump (char *storep, addressT from_addr, addressT to_addr,
			fragS *fragP ATTRIBUTE_UNUSED,
			symbolS *to_symbol ATTRIBUTE_UNUSED)
{
  long int distance;

  /* See md_create_long_jump about the comment on the "+ 2".  */
  long int max_minimal_minus_distance;
  long int max_minimal_plus_distance;
  int nop_opcode;

  if (cris_arch == arch_crisv32)
    {
      max_minimal_minus_distance = BRANCH_BB_V32 + 2;
      max_minimal_plus_distance = BRANCH_BF_V32 + 2;
      nop_opcode = NOP_OPCODE_V32;
    }
  else
    {
      max_minimal_minus_distance = BRANCH_BB + 2;
      max_minimal_plus_distance = BRANCH_BF + 2;
      nop_opcode = NOP_OPCODE;
    }

  distance = to_addr - from_addr;

  if (max_minimal_minus_distance <= distance
      && distance <= max_minimal_plus_distance)
    {
      /* Create a "short" short jump: "BA distance - 2".  */
      storep[0] = branch_disp (distance - 2);
      storep[1] = BA_QUICK_HIGH;

      /* A nop for the delay slot.  */
      md_number_to_chars (storep + 2, nop_opcode, 2);

      /* The extra word should be filled with something sane too.  Make it
	 a nop to keep disassembly sane.  */
      md_number_to_chars (storep + 4, nop_opcode, 2);
    }
  else
    {
      /* Make it a "long" short jump: "BA (PC+)".  */
      md_number_to_chars (storep, BA_PC_INCR_OPCODE, 2);

      /* ".WORD distance - 4".  */
      md_number_to_chars (storep + 2,
			  (long) (distance - 4
				  - (cris_arch == arch_crisv32
				     ? -4 : 0)),
			  2);

      /* A nop for the delay slot.  */
      md_number_to_chars (storep + 4, nop_opcode, 2);
    }
}

/* Generate a long jump in a secondary jump table.

   storep  Where to store the jump instruction.
   from_addr  Address of the jump instruction.
   to_addr    Destination address of the jump.
   fragP      Which frag the destination address operand
	      lies in.
   to_symbol  Destination symbol.  */

void
md_create_long_jump (char *storep, addressT from_addr, addressT to_addr,
		     fragS *fragP, symbolS *to_symbol)
{
  long int distance;

  /* FIXME: What's that "+ 3"?  It comes from the magic numbers that
     used to be here, it's just translated to the limit macros used in
     the relax table.  But why + 3?  */
  long int max_short_minus_distance
    = cris_arch != arch_crisv32 ? BRANCH_WB + 3 : BRANCH_WB_V32 + 3;

  long int max_short_plus_distance
    = cris_arch != arch_crisv32 ? BRANCH_WF + 3 : BRANCH_WF_V32 + 3;

  /* Bail out for compatibility mode.  (It seems it can be implemented,
     perhaps with a 10-byte sequence: "move.d NNNN,$pc/$acr", "jump
     $acr", "nop"; but doesn't seem worth it at the moment.)  */
  if (cris_arch == arch_cris_common_v10_v32)
    as_fatal (_("Out-of-range .word offset handling\
 is not implemented for .arch common_v10_v32"));

  distance = to_addr - from_addr;

  if (max_short_minus_distance <= distance
      && distance <= max_short_plus_distance)
    /* Then make it a "short" long jump.  */
    cris_create_short_jump (storep, from_addr, to_addr, fragP,
			    to_symbol);
  else
    {
      /* We have a "long" long jump: "JUMP [PC+]".  If CRISv32, always
	 make it a BA.  Else make it an "MOVE [PC=PC+N],P0" if we're supposed
	 to emit PIC code.  */
      md_number_to_chars (storep,
			  cris_arch == arch_crisv32
			  ? BA_DWORD_OPCODE
			  : (pic ? MOVE_PC_INCR_OPCODE_PREFIX
			     : JUMP_PC_INCR_OPCODE),
			  2);

      /* Follow with a ".DWORD to_addr", PC-relative for PIC.  */
      fix_new (fragP, storep + 2 - fragP->fr_literal, 4, to_symbol,
	       cris_arch == arch_crisv32 ? 6 : 0,
	       cris_arch == arch_crisv32 || pic ? 1 : 0,
	       cris_arch == arch_crisv32 || pic
	       ? BFD_RELOC_32_PCREL : BFD_RELOC_32);

      /* Follow it with a "NOP" for CRISv32.  */
      if (cris_arch == arch_crisv32)
	md_number_to_chars (storep + 6, NOP_OPCODE_V32, 2);
      else if (pic)
	/* ...and the rest of the move-opcode for pre-v32 PIC.  */
	md_number_to_chars (storep + 6, MOVE_PC_INCR_OPCODE_SUFFIX, 2);
    }
}

/* Allocate space for the first piece of an insn, and mark it as the
   start of the insn for debug-format use.  */

static char *
cris_insn_first_word_frag (void)
{
  char *insnp = frag_more (2);

  /* We need to mark the start of the insn by passing dwarf2_emit_insn
     the offset from the current fragment position.  This must be done
     after the first fragment is created but before any other fragments
     (fixed or varying) are created.  Note that the offset only
     corresponds to the "size" of the insn for a fixed-size,
     non-expanded insn.  */
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
    dwarf2_emit_insn (2);

  return insnp;
}

/* Port-specific assembler initialization.  */

void
md_begin (void)
{
  const char *hashret = NULL;
  int i = 0;

  /* Set up a hash table for the instructions.  */
  op_hash = hash_new ();
  if (op_hash == NULL)
    as_fatal (_("Virtual memory exhausted"));

  /* Enable use of ".if ..asm.arch.cris.v32"
     and ".if ..asm.arch.cris.common_v10_v32" and a few others.  */
  symbol_table_insert (symbol_new ("..asm.arch.cris.v32", absolute_section,
				   (cris_arch == arch_crisv32),
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("..asm.arch.cris.v10", absolute_section,
				   (cris_arch == arch_crisv10),
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("..asm.arch.cris.common_v10_v32",
				   absolute_section,
				   (cris_arch == arch_cris_common_v10_v32),
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("..asm.arch.cris.any_v0_v10",
				   absolute_section,
				   (cris_arch == arch_cris_any_v0_v10),
				   &zero_address_frag));

  while (cris_opcodes[i].name != NULL)
    {
      const char *name = cris_opcodes[i].name;

      if (! cris_insn_ver_valid_for_arch (cris_opcodes[i].applicable_version,
					  cris_arch))
	{
	  i++;
	  continue;
	}

      /* Need to cast to get rid of "const".  FIXME: Fix hash_insert instead.  */
      hashret = hash_insert (op_hash, name, (void *) &cris_opcodes[i]);

      if (hashret != NULL && *hashret != '\0')
	as_fatal (_("Can't hash `%s': %s\n"), cris_opcodes[i].name,
		  *hashret == 0 ? _("(unknown reason)") : hashret);
      do
	{
	  if (cris_opcodes[i].match & cris_opcodes[i].lose)
	    as_fatal (_("Buggy opcode: `%s' \"%s\"\n"), cris_opcodes[i].name,
		      cris_opcodes[i].args);

	  ++i;
	}
      while (cris_opcodes[i].name != NULL
	     && strcmp (cris_opcodes[i].name, name) == 0);
    }
}

/* Assemble a source line.  */

void
md_assemble (char *str)
{
  struct cris_instruction output_instruction;
  struct cris_prefix prefix;
  char *opcodep;
  char *p;

  know (str);

  /* Do the low-level grunt - assemble to bits and split up into a prefix
     and ordinary insn.  */
  cris_process_instruction (str, &output_instruction, &prefix);

  /* Handle any prefixes to the instruction.  */
  switch (prefix.kind)
    {
    case PREFIX_NONE:
      break;

      /* When the expression is unknown for a BDAP, it can need 0, 2 or 4
	 extra bytes, so we handle it separately.  */
    case PREFIX_BDAP_IMM:
      /* We only do it if the relocation is unspecified, i.e. not a PIC
	 relocation.  */
      if (prefix.reloc == BFD_RELOC_NONE)
	{
	  gen_bdap (prefix.base_reg_number, &prefix.expr);
	  break;
	}
      /* Fall through.  */
    case PREFIX_BDAP:
    case PREFIX_BIAP:
    case PREFIX_DIP:
      opcodep = cris_insn_first_word_frag ();

      /* Output the prefix opcode.  */
      md_number_to_chars (opcodep, (long) prefix.opcode, 2);

      /* Having a specified reloc only happens for DIP and for BDAP with
	 PIC operands, but it is ok to drop through here for the other
	 prefixes as they can have no relocs specified.  */
      if (prefix.reloc != BFD_RELOC_NONE)
	{
	  unsigned int relocsize
	    = (prefix.kind == PREFIX_DIP
	       ? 4 : cris_get_pic_reloc_size (prefix.reloc));

	  p = frag_more (relocsize);
	  fix_new_exp (frag_now, (p - frag_now->fr_literal), relocsize,
		       &prefix.expr, 0, prefix.reloc);
	}
      break;

    case PREFIX_PUSH:
      opcodep = cris_insn_first_word_frag ();

      /* Output the prefix opcode.  Being a "push", we add the negative
	 size of the register to "sp".  */
      if (output_instruction.spec_reg != NULL)
	{
	  /* Special register.  */
	  opcodep[0] = -output_instruction.spec_reg->reg_size;
	}
      else
	{
	  /* General register.  */
	  opcodep[0] = -4;
	}
      opcodep[1] = (REG_SP << 4) + (BDAP_QUICK_OPCODE >> 8);
      break;

    default:
      BAD_CASE (prefix.kind);
    }

  /* If we only had a prefix insn, we're done.  */
  if (output_instruction.insn_type == CRIS_INSN_NONE)
    return;

  /* Done with the prefix.  Continue with the main instruction.  */
  if (prefix.kind == PREFIX_NONE)
    opcodep = cris_insn_first_word_frag ();
  else
    opcodep = frag_more (2);

  /* Output the instruction opcode.  */
  md_number_to_chars (opcodep, (long) (output_instruction.opcode), 2);

  /* Output the symbol-dependent instruction stuff.  */
  if (output_instruction.insn_type == CRIS_INSN_BRANCH)
    {
      segT to_seg = absolute_section;
      int is_undefined = 0;
      int length_code;

      if (output_instruction.expr.X_op != O_constant)
	{
	  to_seg = S_GET_SEGMENT (output_instruction.expr.X_add_symbol);

	  if (to_seg == undefined_section)
	    is_undefined = 1;
	}

      if (to_seg == now_seg || is_undefined
	  /* In CRISv32, there *is* a 32-bit absolute branch, so don't
	     emit the 12-byte sequence for known symbols in other
	     segments.  */
	  || (cris_arch == arch_crisv32
	      && output_instruction.opcode == BA_QUICK_OPCODE))
	{
	  /* Handle complex expressions.  */
	  valueT addvalue
	    = (SIMPLE_EXPR (&output_instruction.expr)
	       ? output_instruction.expr.X_add_number
	       : 0);
	  symbolS *sym
	    = (SIMPLE_EXPR (&output_instruction.expr)
	       ? output_instruction.expr.X_add_symbol
	       : make_expr_symbol (&output_instruction.expr));

	  /* If is_undefined, the expression may still become now_seg.
	     That case is handled by md_estimate_size_before_relax.  */
	  length_code = to_seg == now_seg ? STATE_BYTE : STATE_UNDF;

	  /* Make room for max twelve bytes of variable length for v32 mode
	     or PIC, ten for v10 and older.  */
	  frag_var (rs_machine_dependent,
		    (cris_arch == arch_crisv32
		     || cris_arch == arch_cris_common_v10_v32
		     || pic) ? 12 : 10, 0,
		    ENCODE_RELAX (cris_arch == arch_crisv32
				  ? (output_instruction.opcode
				     == BA_QUICK_OPCODE
				     ? STATE_ABS_BRANCH_V32
				     : STATE_COND_BRANCH_V32)
				  : (cris_arch == arch_cris_common_v10_v32
				     ? STATE_COND_BRANCH_COMMON
				     : (pic ? STATE_COND_BRANCH_PIC
					: STATE_COND_BRANCH)),
				  length_code),
		    sym, addvalue, opcodep);
	}
      else
	{
	  /* We have: to_seg != now_seg && to_seg != undefined_section.
	     This means it is a branch to a known symbol in another
	     section, perhaps an absolute address.  Emit a 32-bit branch.  */
	  char *cond_jump
	    = frag_more ((cris_arch == arch_crisv32
			  || cris_arch == arch_cris_common_v10_v32
			  || pic)
			 ? 12 : 10);

	  gen_cond_branch_32 (opcodep, cond_jump, frag_now,
			      output_instruction.expr.X_add_symbol,
			      (symbolS *) NULL,
			      output_instruction.expr.X_add_number);
	}
    }
  else if (output_instruction.insn_type == CRIS_INSN_MUL
	   && err_for_dangerous_mul_placement)
    /* Create a frag which which we track the location of the mul insn
       (in the last two bytes before the mul-frag).  */
    frag_variant (rs_machine_dependent, 0, 0,
		  ENCODE_RELAX (STATE_MUL, STATE_BYTE),
		  NULL, 0, opcodep);
  else
    {
      if (output_instruction.imm_oprnd_size > 0)
	{
	  /* The instruction has an immediate operand.  */
	  enum bfd_reloc_code_real reloc = BFD_RELOC_NONE;

	  switch (output_instruction.imm_oprnd_size)
	    {
	      /* Any byte-size immediate constants are treated as
		 word-size.  FIXME: Thus overflow check does not work
		 correctly.  */

	    case 2:
	      /* Note that size-check for the explicit reloc has already
		 been done when we get here.  */
	      if (output_instruction.reloc != BFD_RELOC_NONE)
		reloc = output_instruction.reloc;
	      else
		reloc = BFD_RELOC_16;
	      break;

	    case 4:
	      /* Allow a relocation specified in the operand.  */
	      if (output_instruction.reloc != BFD_RELOC_NONE)
		reloc = output_instruction.reloc;
	      else
		reloc = BFD_RELOC_32;
	      break;

	    default:
	      BAD_CASE (output_instruction.imm_oprnd_size);
	    }

	  p = frag_more (output_instruction.imm_oprnd_size);
	  fix_new_exp (frag_now, (p - frag_now->fr_literal),
		       output_instruction.imm_oprnd_size,
		       &output_instruction.expr,
		       reloc == BFD_RELOC_32_PCREL
		       || reloc == BFD_RELOC_16_PCREL
		       || reloc == BFD_RELOC_8_PCREL, reloc);
	}
      else if (output_instruction.reloc == BFD_RELOC_CRIS_LAPCQ_OFFSET
	       && output_instruction.expr.X_md != 0)
	{
	  /* Handle complex expressions.  */
	  valueT addvalue
	    = (output_instruction.expr.X_op_symbol != NULL
	       ? 0 : output_instruction.expr.X_add_number);
	  symbolS *sym
	    = (output_instruction.expr.X_op_symbol != NULL
	       ? make_expr_symbol (&output_instruction.expr)
	       : output_instruction.expr.X_add_symbol);

	  /* This is a relaxing construct, so we need a frag_var rather
	     than the fix_new_exp call below.  */
	  frag_var (rs_machine_dependent,
		    4, 0,
		    ENCODE_RELAX (STATE_LAPC, STATE_UNDF),
		    sym, addvalue, opcodep);
	}
      else if (output_instruction.reloc != BFD_RELOC_NONE)
	{
	  /* An immediate operand that has a relocation and needs to be
	     processed further.  */

	  /* It is important to use fix_new_exp here and everywhere else
	     (and not fix_new), as fix_new_exp can handle "difference
	     expressions" - where the expression contains a difference of
	     two symbols in the same segment.  */
	  fix_new_exp (frag_now, (opcodep - frag_now->fr_literal), 2,
		       &output_instruction.expr,
		       output_instruction.reloc == BFD_RELOC_32_PCREL
		       || output_instruction.reloc == BFD_RELOC_16_PCREL
		       || output_instruction.reloc == BFD_RELOC_8_PCREL
		       || (output_instruction.reloc
			   == BFD_RELOC_CRIS_LAPCQ_OFFSET),
		       output_instruction.reloc);
	}
    }
}

/* Low level text-to-bits assembly.  */

static void
cris_process_instruction (char *insn_text, struct cris_instruction *out_insnp,
			  struct cris_prefix *prefixp)
{
  char *s;
  char modified_char = 0;
  const char *args;
  struct cris_opcode *instruction;
  char *operands;
  int match = 0;
  int mode;
  int regno;
  int size_bits;

  /* Reset these fields to a harmless state in case we need to return in
     error.  */
  prefixp->kind = PREFIX_NONE;
  prefixp->reloc = BFD_RELOC_NONE;
  out_insnp->insn_type = CRIS_INSN_NONE;
  out_insnp->imm_oprnd_size = 0;

  /* Find the end of the opcode mnemonic.  We assume (true in 2.9.1)
     that the caller has translated the opcode to lower-case, up to the
     first non-letter.  */
  for (operands = insn_text; ISLOWER (*operands); ++operands)
    ;

  /* Terminate the opcode after letters, but save the character there if
     it was of significance.  */
  switch (*operands)
    {
    case '\0':
      break;

    case '.':
      /* Put back the modified character later.  */
      modified_char = *operands;
      /* Fall through.  */

    case ' ':
      /* Consume the character after the mnemonic
	 and replace it with '\0'.  */
      *operands++ = '\0';
      break;

    default:
      as_bad (_("Unknown opcode: `%s'"), insn_text);
      return;
    }

  /* Find the instruction.  */
  instruction = (struct cris_opcode *) hash_find (op_hash, insn_text);
  if (instruction == NULL)
    {
      as_bad (_("Unknown opcode: `%s'"), insn_text);
      return;
    }

  /* Put back the modified character.  */
  switch (modified_char)
    {
    case 0:
      break;

    default:
      *--operands = modified_char;
    }

  /* Try to match an opcode table slot.  */
  for (s = operands;;)
    {
      int imm_expr_found;

      /* Initialize *prefixp, perhaps after being modified for a
	 "near match".  */
      prefixp->kind = PREFIX_NONE;
      prefixp->reloc = BFD_RELOC_NONE;

      /* Initialize *out_insnp.  */
      memset (out_insnp, 0, sizeof (*out_insnp));
      out_insnp->opcode = instruction->match;
      out_insnp->reloc = BFD_RELOC_NONE;
      out_insnp->insn_type = CRIS_INSN_NORMAL;
      out_insnp->imm_oprnd_size = 0;

      imm_expr_found = 0;

      /* Build the opcode, checking as we go to make sure that the
	 operands match.  */
      for (args = instruction->args;; ++args)
	{
	  switch (*args)
	    {
	    case '\0':
	      /* If we've come to the end of arguments, we're done.  */
	      if (*s == '\0')
		match = 1;
	      break;

	    case '!':
	      /* Non-matcher character for disassembly.
		 Ignore it here.  */
	      continue;

	    case '[':
	    case ']':
	    case ',':
	    case ' ':
	      /* These must match exactly.  */
	      if (*s++ == *args)
		continue;
	      break;

	    case 'A':
	      /* "ACR", case-insensitive.
		 Handle a sometimes-mandatory dollar sign as register
		 prefix.  */
	      if (*s == REGISTER_PREFIX_CHAR)
		s++;
	      else if (demand_register_prefix)
		break;

	      if ((*s++ != 'a' && s[-1] != 'A')
		  || (*s++ != 'c' && s[-1] != 'C')
		  || (*s++ != 'r' && s[-1] != 'R'))
		break;
	      continue;

	    case 'B':
	      /* This is not really an operand, but causes a "BDAP
		 -size,SP" prefix to be output, for PUSH instructions.  */
	      prefixp->kind = PREFIX_PUSH;
	      continue;

	    case 'b':
	      /* This letter marks an operand that should not be matched
		 in the assembler. It is a branch with 16-bit
		 displacement.  The assembler will create them from the
		 8-bit flavor when necessary.  The assembler does not
		 support the [rN+] operand, as the [r15+] that is
		 generated for 16-bit displacements.  */
	      break;

	    case 'c':
	      /* A 5-bit unsigned immediate in bits <4:0>.  */
	      if (! cris_get_expression (&s, &out_insnp->expr))
		break;
	      else
		{
		  if (out_insnp->expr.X_op == O_constant
		      && (out_insnp->expr.X_add_number < 0
			  || out_insnp->expr.X_add_number > 31))
		    as_bad (_("Immediate value not in 5 bit unsigned range: %ld"),
			    out_insnp->expr.X_add_number);

		  out_insnp->reloc = BFD_RELOC_CRIS_UNSIGNED_5;
		  continue;
		}

	    case 'C':
	      /* A 4-bit unsigned immediate in bits <3:0>.  */
	      if (! cris_get_expression (&s, &out_insnp->expr))
		break;
	      else
		{
		  if (out_insnp->expr.X_op == O_constant
		      && (out_insnp->expr.X_add_number < 0
			  || out_insnp->expr.X_add_number > 15))
		    as_bad (_("Immediate value not in 4 bit unsigned range: %ld"),
			    out_insnp->expr.X_add_number);

		  out_insnp->reloc = BFD_RELOC_CRIS_UNSIGNED_4;
		  continue;
		}

 	      /* For 'd', check for an optional ".d" or ".D" at the
                 start of the operands, followed by a space character.  */
	    case 'd':
	      if (modified_char == '.' && *s == '.')
		{
		  if ((s[1] != 'd' && s[1] == 'D')
		      || ! ISSPACE (s[2]))
		    break;
		  s += 2;
		  continue;
		}
	      continue;

	    case 'D':
	      /* General register in bits <15:12> and <3:0>.  */
	      if (! get_gen_reg (&s, &regno))
		break;
	      else
		{
		  out_insnp->opcode |= regno /* << 0 */;
		  out_insnp->opcode |= regno << 12;
		  continue;
		}

	    case 'f':
	      /* Flags from the condition code register.  */
	      {
		int flags = 0;

		if (! get_flags (&s, &flags))
		  break;

		out_insnp->opcode |= ((flags & 0xf0) << 8) | (flags & 0xf);
		continue;
	      }

	    case 'i':
	      /* A 6-bit signed immediate in bits <5:0>.  */
	      if (! cris_get_expression (&s, &out_insnp->expr))
		break;
	      else
		{
		  if (out_insnp->expr.X_op == O_constant
		      && (out_insnp->expr.X_add_number < -32
			  || out_insnp->expr.X_add_number > 31))
		    as_bad (_("Immediate value not in 6 bit range: %ld"),
			    out_insnp->expr.X_add_number);
		  out_insnp->reloc = BFD_RELOC_CRIS_SIGNED_6;
		  continue;
		}

	    case 'I':
	      /* A 6-bit unsigned immediate in bits <5:0>.  */
	      if (! cris_get_expression (&s, &out_insnp->expr))
		break;
	      else
		{
		  if (out_insnp->expr.X_op == O_constant
		      && (out_insnp->expr.X_add_number < 0
			  || out_insnp->expr.X_add_number > 63))
		    as_bad (_("Immediate value not in 6 bit unsigned range: %ld"),
			    out_insnp->expr.X_add_number);
		  out_insnp->reloc = BFD_RELOC_CRIS_UNSIGNED_6;
		  continue;
		}

	    case 'M':
	      /* A size modifier, B, W or D, to be put in a bit position
		 suitable for CLEAR instructions (i.e. reflecting a zero
		 register).  */
	      if (! get_bwd_size_modifier (&s, &size_bits))
		break;
	      else
		{
		  switch (size_bits)
		    {
		    case 0:
		      out_insnp->opcode |= 0 << 12;
		      break;

		    case 1:
		      out_insnp->opcode |= 4 << 12;
		      break;

		    case 2:
		      out_insnp->opcode |= 8 << 12;
		      break;
		    }
		  continue;
		}

	    case 'm':
	      /* A size modifier, B, W or D, to be put in bits <5:4>.  */
	      if (modified_char != '.'
		  || ! get_bwd_size_modifier (&s, &size_bits))
		break;
	      else
		{
		  out_insnp->opcode |= size_bits << 4;
		  continue;
		}

	    case 'o':
	      /* A branch expression.  */
	      if (! cris_get_expression (&s, &out_insnp->expr))
		break;
	      else
		{
		  out_insnp->insn_type = CRIS_INSN_BRANCH;
		  continue;
		}

	    case 'Q':
	      /* A 8-bit quick BDAP expression, "expr,R".  */
	      if (! cris_get_expression (&s, &out_insnp->expr))
		break;

	      if (*s != ',')
		break;

	      s++;

	      if (!get_gen_reg (&s, &regno))
		break;

	      out_insnp->opcode |= regno << 12;
	      out_insnp->reloc = BFD_RELOC_CRIS_SIGNED_8;
	      continue;
	      
	    case 'O':
	      /* A BDAP expression for any size, "expr,R".  */
	      if (! cris_get_expression (&s, &prefixp->expr))
		break;
	      else
		{
		  if (*s != ',')
		    break;

		  s++;

		  if (!get_gen_reg (&s, &prefixp->base_reg_number))
		    break;

		  /* Since 'O' is used with an explicit bdap, we have no
		     "real" instruction.  */
		  prefixp->kind = PREFIX_BDAP_IMM;
		  prefixp->opcode
		    = BDAP_QUICK_OPCODE | (prefixp->base_reg_number << 12);

		  out_insnp->insn_type = CRIS_INSN_NONE;
		  continue;
		}

	    case 'P':
	      /* Special register in bits <15:12>.  */
	      if (! get_spec_reg (&s, &out_insnp->spec_reg))
		break;
	      else
		{
		  /* Use of some special register names come with a
		     specific warning.	Note that we have no ".cpu type"
		     pseudo yet, so some of this is just unused
		     framework.  */
		  if (out_insnp->spec_reg->warning)
		    as_warn (out_insnp->spec_reg->warning);
		  else if (out_insnp->spec_reg->applicable_version
			   == cris_ver_warning)
		    /* Others have a generic warning.  */
		    as_warn (_("Unimplemented register `%s' specified"),
			     out_insnp->spec_reg->name);

		  out_insnp->opcode
		    |= out_insnp->spec_reg->number << 12;
		  continue;
		}

	    case 'p':
	      /* This character is used in the disassembler to
		 recognize a prefix instruction to fold into the
		 addressing mode for the next instruction.  It is
		 ignored here.  */
	      continue;

	    case 'R':
	      /* General register in bits <15:12>.  */
	      if (! get_gen_reg (&s, &regno))
		break;
	      else
		{
		  out_insnp->opcode |= regno << 12;
		  continue;
		}

	    case 'r':
	      /* General register in bits <3:0>.  */
	      if (! get_gen_reg (&s, &regno))
		break;
	      else
		{
		  out_insnp->opcode |= regno /* << 0 */;
		  continue;
		}

	    case 'S':
	      /* Source operand in bit <10> and a prefix; a 3-operand
		 prefix.  */
	      if (! get_3op_or_dip_prefix_op (&s, prefixp))
		break;
	      else
		continue;

	    case 's':
	      /* Source operand in bits <10>, <3:0> and optionally a
		 prefix; i.e. an indirect operand or an side-effect
		 prefix (where valid).  */
	      if (! get_autoinc_prefix_or_indir_op (&s, prefixp, &mode,
						    &regno,
						    &imm_expr_found,
						    &out_insnp->expr))
		break;
	      else
		{
		  if (prefixp->kind != PREFIX_NONE)
		    {
		      /* A prefix, so it has the autoincrement bit
			 set.  */
		      out_insnp->opcode |= (AUTOINCR_BIT << 8);
		    }
		  else
		    {
		      /* No prefix.  The "mode" variable contains bits like
			 whether or not this is autoincrement mode.  */
		      out_insnp->opcode |= (mode << 10);

		      /* If there was a PIC reloc specifier, then it was
			 attached to the prefix.  Note that we can't check
			 that the reloc size matches, since we don't have
			 all the operands yet in all cases.  */
		      if (prefixp->reloc != BFD_RELOC_NONE)
			out_insnp->reloc = prefixp->reloc;
		    }

		  out_insnp->opcode |= regno /* << 0 */ ;
		  continue;
		}

	    case 'N':
	    case 'Y':
	      /* Like 's', but immediate operand only.  Also does not
		 modify insn.  There are no insns where a PIC reloc
		 specifier makes sense.  */
	      if (cris_get_expression (&s, &out_insnp->expr))
		{
		  imm_expr_found = 1;
		  continue;
		}
	      break;

	    case 'n':
	      /* Like 'N', but PC-relative to the start of the insn.
		 There might be a :PLT to request a PLT entry.  */
	      if (cris_get_expression (&s, &out_insnp->expr))
		{
		  imm_expr_found = 1;
		  out_insnp->reloc = BFD_RELOC_32_PCREL;

		  /* We have to adjust the expression, because that
		     relocation is to the location *after* the
		     relocation.  So add 2 for the insn and 4 for the
		     relocation.  */
		  out_insnp->expr.X_add_number += 6;

		  if (pic && *s == PIC_SUFFIX_CHAR)
		    cris_get_pic_suffix (&s, &out_insnp->reloc,
					 &out_insnp->expr);

		  continue;
		}
	      break;

	    case 'U':
	      /* Maybe 'u', maybe 'n'.  Only for LAPC/LAPCQ.  */
	      if (cris_get_expression (&s, &out_insnp->expr))
		{
		  out_insnp->reloc = BFD_RELOC_CRIS_LAPCQ_OFFSET;

		  /* Define 1 as relaxing.  */
		  out_insnp->expr.X_md = 1;
		  continue;
		}
	      break;

	    case 'u':
	      /* Four PC-relative bits in <3:0> representing <4:1>:0 of
		 an offset relative to the beginning of the current
		 insn.  */
	      if (cris_get_expression (&s, &out_insnp->expr))
		{
		  out_insnp->reloc = BFD_RELOC_CRIS_LAPCQ_OFFSET;

		  /* Define 0 as non-relaxing.  */
		  out_insnp->expr.X_md = 0;

		  /* We have to adjust the expression, because that
		     relocation is to the location *after* the
		     insn.  So add 2 for the insn.  */
		  out_insnp->expr.X_add_number += 2;
		  continue;
		}
	      break;

	    case 'x':
	      /* Rs.m in bits <15:12> and <5:4>.  */
	      if (! get_gen_reg (&s, &regno)
		  || ! get_bwd_size_modifier (&s, &size_bits))
		break;
	      else
		{
		  out_insnp->opcode |= (regno << 12) | (size_bits << 4);
		  continue;
		}

	    case 'y':
	      /* Source operand in bits <10>, <3:0> and optionally a
		 prefix; i.e. an indirect operand or an side-effect
		 prefix.

		 The difference to 's' is that this does not allow an
		 "immediate" expression.  */
	      if (! get_autoinc_prefix_or_indir_op (&s, prefixp,
						    &mode, &regno,
						    &imm_expr_found,
						    &out_insnp->expr)
		  || imm_expr_found)
		break;
	      else
		{
		  if (prefixp->kind != PREFIX_NONE)
		    {
		      /* A prefix, and those matched here always have
			 side-effects (see 's' case).  */
		      out_insnp->opcode |= (AUTOINCR_BIT << 8);
		    }
		  else
		    {
		      /* No prefix.  The "mode" variable contains bits
			 like whether or not this is autoincrement
			 mode.  */
		      out_insnp->opcode |= (mode << 10);
		    }

		  out_insnp->opcode |= regno /* << 0 */;
		  continue;
		}

	    case 'z':
	      /* Size modifier (B or W) in bit <4>.  */
	      if (! get_bw_size_modifier (&s, &size_bits))
		break;
	      else
		{
		  out_insnp->opcode |= size_bits << 4;
		  continue;
		}

	    case 'T':
	      if (cris_arch == arch_crisv32
		  && get_sup_reg (&s, &regno))
		{
		  out_insnp->opcode |= regno << 12;
		  continue;
		}
	      break;

	    default:
	      BAD_CASE (*args);
	    }

	  /* We get here when we fail a match above or we found a
	     complete match.  Break out of this loop.  */
	  break;
	}

      /* Was it a match or a miss?  */
      if (match == 0)
	{
	  /* If it's just that the args don't match, maybe the next
	     item in the table is the same opcode but with
	     matching operands.  First skip any invalid ones.  */
	  while (instruction[1].name != NULL
		 && strcmp (instruction->name, instruction[1].name) == 0
		 && ! cris_insn_ver_valid_for_arch (instruction[1]
						    .applicable_version,
						    cris_arch))
	    ++instruction;

	  if (instruction[1].name != NULL
	      && strcmp (instruction->name, instruction[1].name) == 0
	      && cris_insn_ver_valid_for_arch (instruction[1]
					       .applicable_version,
					       cris_arch))
	    {
	      /* Yep.  Restart and try that one instead.  */
	      ++instruction;
	      s = operands;
	      continue;
	    }
	  else
	    {
	      /* We've come to the end of instructions with this
		 opcode, so it must be an error.  */
	      as_bad (_("Illegal operands"));

	      /* As discard_rest_of_line, but without continuing to the
		 next line.  */
	      while (!is_end_of_line[(unsigned char) *input_line_pointer])
		input_line_pointer++;
	      return;
	    }
	}
      else
	{
	  /* We have a match.  Check if there's anything more to do.  */
	  if (imm_expr_found)
	    {
	      /* There was an immediate mode operand, so we must check
		 that it has an appropriate size.  */
	      switch (instruction->imm_oprnd_size)
		{
		default:
		case SIZE_NONE:
		  /* Shouldn't happen; this one does not have immediate
		     operands with different sizes.  */
		  BAD_CASE (instruction->imm_oprnd_size);
		  break;

		case SIZE_FIX_32:
		  out_insnp->imm_oprnd_size = 4;
		  break;

		case SIZE_SPEC_REG:
		  if (cris_arch == arch_crisv32)
		    /* All immediate loads of special registers are
		       32-bit on CRISv32.  */
		    out_insnp->imm_oprnd_size = 4;
		  else
		    switch (out_insnp->spec_reg->reg_size)
		      {
		      case 1:
			if (out_insnp->expr.X_op == O_constant
			    && (out_insnp->expr.X_add_number < -128
				|| out_insnp->expr.X_add_number > 255))
			  as_bad (_("Immediate value not in 8 bit range: %ld"),
				  out_insnp->expr.X_add_number);
			/* Fall through.  */
		      case 2:
			/* FIXME:  We need an indicator in the instruction
			   table to pass on, to indicate if we need to check
			   overflow for a signed or unsigned number.  */
			if (out_insnp->expr.X_op == O_constant
			    && (out_insnp->expr.X_add_number < -32768
				|| out_insnp->expr.X_add_number > 65535))
			  as_bad (_("Immediate value not in 16 bit range: %ld"),
				  out_insnp->expr.X_add_number);
			out_insnp->imm_oprnd_size = 2;
			break;

		      case 4:
			out_insnp->imm_oprnd_size = 4;
			break;

		      default:
			BAD_CASE (out_insnp->spec_reg->reg_size);
		      }
		  break;

		case SIZE_FIELD:
		case SIZE_FIELD_SIGNED:
		case SIZE_FIELD_UNSIGNED:
		  switch (size_bits)
		    {
		      /* FIXME: Find way to pass un/signedness to
			 caller, and set reloc type instead, postponing
			 this check until cris_number_to_imm.  That
			 necessarily corrects the reloc type for the
			 byte case, maybe requiring further changes.  */
		    case 0:
		      if (out_insnp->expr.X_op == O_constant)
			{
			  if (instruction->imm_oprnd_size == SIZE_FIELD
			      && (out_insnp->expr.X_add_number < -128
				  || out_insnp->expr.X_add_number > 255))
			    as_bad (_("Immediate value not in 8 bit range: %ld"),
				    out_insnp->expr.X_add_number);
			  else if (instruction->imm_oprnd_size == SIZE_FIELD_SIGNED
			      && (out_insnp->expr.X_add_number < -128
				  || out_insnp->expr.X_add_number > 127))
			    as_bad (_("Immediate value not in 8 bit signed range: %ld"),
				    out_insnp->expr.X_add_number);
			  else if (instruction->imm_oprnd_size == SIZE_FIELD_UNSIGNED
				   && (out_insnp->expr.X_add_number < 0
				       || out_insnp->expr.X_add_number > 255))
			    as_bad (_("Immediate value not in 8 bit unsigned range: %ld"),
				    out_insnp->expr.X_add_number);
			}

		      /* Fall through.  */
		    case 1:
		      if (out_insnp->expr.X_op == O_constant)
			{
			  if (instruction->imm_oprnd_size == SIZE_FIELD
			      && (out_insnp->expr.X_add_number < -32768
				  || out_insnp->expr.X_add_number > 65535))
			    as_bad (_("Immediate value not in 16 bit range: %ld"),
				    out_insnp->expr.X_add_number);
			  else if (instruction->imm_oprnd_size == SIZE_FIELD_SIGNED
			      && (out_insnp->expr.X_add_number < -32768
				  || out_insnp->expr.X_add_number > 32767))
			    as_bad (_("Immediate value not in 16 bit signed range: %ld"),
				    out_insnp->expr.X_add_number);
			  else if (instruction->imm_oprnd_size == SIZE_FIELD_UNSIGNED
			      && (out_insnp->expr.X_add_number < 0
				  || out_insnp->expr.X_add_number > 65535))
			    as_bad (_("Immediate value not in 16 bit unsigned range: %ld"),
				    out_insnp->expr.X_add_number);
			}
		      out_insnp->imm_oprnd_size = 2;
		      break;

		    case 2:
		      out_insnp->imm_oprnd_size = 4;
		      break;

		    default:
		      BAD_CASE (out_insnp->spec_reg->reg_size);
		    }
		}

	      /* If there was a relocation specified for the immediate
		 expression (i.e. it had a PIC modifier) check that the
		 size of the PIC relocation matches the size specified by
		 the opcode.  */
	      if (out_insnp->reloc != BFD_RELOC_NONE
		  && (cris_get_pic_reloc_size (out_insnp->reloc)
		      != (unsigned int) out_insnp->imm_oprnd_size))
		as_bad (_("PIC relocation size does not match operand size"));
	    }
	  else if (instruction->op == cris_muls_op
		   || instruction->op == cris_mulu_op)
	    out_insnp->insn_type = CRIS_INSN_MUL;
	}
      break;
    }
}

/* Get a B, W, or D size modifier from the string pointed out by *cPP,
   which must point to a '.' in front of the modifier.	On successful
   return, *cPP is advanced to the character following the size
   modifier, and is undefined otherwise.

   cPP		Pointer to pointer to string starting
		with the size modifier.

   size_bitsp	Pointer to variable to contain the size bits on
		successful return.

   Return 1 iff a correct size modifier is found, else 0.  */

static int
get_bwd_size_modifier (char **cPP, int *size_bitsp)
{
  if (**cPP != '.')
    return 0;
  else
    {
      /* Consume the '.'.  */
      (*cPP)++;

      switch (**cPP)
	{
	case 'B':
	case 'b':
	  *size_bitsp = 0;
	  break;

	case 'W':
	case 'w':
	  *size_bitsp = 1;
	  break;

	case 'D':
	case 'd':
	  *size_bitsp = 2;
	  break;

	default:
	  return 0;
	}

      /* Consume the size letter.  */
      (*cPP)++;
      return 1;
    }
}

/* Get a B or W size modifier from the string pointed out by *cPP,
   which must point to a '.' in front of the modifier.	On successful
   return, *cPP is advanced to the character following the size
   modifier, and is undefined otherwise.

   cPP		Pointer to pointer to string starting
		with the size modifier.

   size_bitsp	Pointer to variable to contain the size bits on
		successful return.

   Return 1 iff a correct size modifier is found, else 0.  */

static int
get_bw_size_modifier (char **cPP, int *size_bitsp)
{
  if (**cPP != '.')
    return 0;
  else
    {
      /* Consume the '.'.  */
      (*cPP)++;

      switch (**cPP)
	{
	case 'B':
	case 'b':
	  *size_bitsp = 0;
	  break;

	case 'W':
	case 'w':
	  *size_bitsp = 1;
	  break;

	default:
	  return 0;
	}

      /* Consume the size letter.  */
      (*cPP)++;
      return 1;
    }
}

/* Get a general register from the string pointed out by *cPP.  The
   variable *cPP is advanced to the character following the general
   register name on a successful return, and has its initial position
   otherwise.

   cPP	    Pointer to pointer to string, beginning with a general
	    register name.

   regnop   Pointer to int containing the register number.

   Return 1 iff a correct general register designator is found,
	    else 0.  */

static int
get_gen_reg (char **cPP, int *regnop)
{
  char *oldp;
  oldp = *cPP;

  /* Handle a sometimes-mandatory dollar sign as register prefix.  */
  if (**cPP == REGISTER_PREFIX_CHAR)
    (*cPP)++;
  else if (demand_register_prefix)
    return 0;

  switch (**cPP)
    {
    case 'P':
    case 'p':
      /* "P" as in "PC"?  Consume the "P".  */
      (*cPP)++;

      if ((**cPP == 'C' || **cPP == 'c')
	  && ! ISALNUM ((*cPP)[1])
	  /* Here's a little twist: For v32 and the compatibility mode,
	     we only recognize PC as a register number if there's '+]'
	     after.  We don't consume that, but the presence can only be
	     valid after a register in a post-increment context, which
	     is also the only valid context for PC as a register for
	     v32.  Not that it's used very often, but saying "MOVE.D
	     [PC+],R5" should remain valid.  It's not supported for
	     jump-type insns or other insns with no [Rn+] mode, though.  */
	  && ((cris_arch != arch_crisv32
	       && cris_arch != arch_cris_common_v10_v32)
	      || ((*cPP)[1] == '+' && (*cPP)[2] == ']')))
	{
	  /* It's "PC": consume the "c" and we're done.  */
	  (*cPP)++;
	  *regnop = REG_PC;
	  return 1;
	}
      break;

      /* Like with PC, we recognize ACR, but only if it's *not* followed
	 by '+', and only for v32.  */
    case 'A':
    case 'a':
      if (cris_arch != arch_crisv32
	  || ((*cPP)[1] != 'c' && (*cPP)[1] != 'C')
	  || ((*cPP)[2] != 'r' && (*cPP)[2] != 'R')
	  || ISALNUM ((*cPP)[3])
	  || (*cPP)[3] == '+')
	break;
      (*cPP) += 3;
      *regnop = 15;
      return 1;

    case 'R':
    case 'r':
      /* Hopefully r[0-9] or r1[0-5].  Consume 'R' or 'r'.  */
      (*cPP)++;

      if (ISDIGIT (**cPP))
	{
	  /* It's r[0-9].  Consume and check the next digit.  */
	  *regnop = **cPP - '0';
	  (*cPP)++;

	  if (! ISALNUM (**cPP))
	    {
	      /* No more digits, we're done.  */
	      return 1;
	    }
	  else
	    {
	      /* One more digit.  Consume and add.  */
	      *regnop = *regnop * 10 + (**cPP - '0');

	      /* We need to check for a valid register number; Rn,
		 0 <= n <= MAX_REG.  */
	      if (*regnop <= MAX_REG)
		{
		  /* Consume second digit.  */
		  (*cPP)++;
		  return 1;
		}
	    }
	}
      break;

    case 'S':
    case 's':
      /* "S" as in "SP"?  Consume the "S".  */
      (*cPP)++;
      if (**cPP == 'P' || **cPP == 'p')
	{
	  /* It's "SP": consume the "p" and we're done.  */
	  (*cPP)++;
	  *regnop = REG_SP;
	  return 1;
	}
      break;

    default:
      /* Just here to silence compilation warnings.  */
      ;
    }

  /* We get here if we fail.  Restore the pointer.  */
  *cPP = oldp;
  return 0;
}

/* Get a special register from the string pointed out by *cPP. The
   variable *cPP is advanced to the character following the special
   register name if one is found, and retains its original position
   otherwise.

   cPP	    Pointer to pointer to string starting with a special register
	    name.

   sregpp   Pointer to Pointer to struct spec_reg, where a pointer to the
	    register description will be stored.

   Return 1 iff a correct special register name is found.  */

static int
get_spec_reg (char **cPP, const struct cris_spec_reg **sregpp)
{
  char *s1;
  const char *s2;
  char *name_begin = *cPP;

  const struct cris_spec_reg *sregp;

  /* Handle a sometimes-mandatory dollar sign as register prefix.  */
  if (*name_begin == REGISTER_PREFIX_CHAR)
    name_begin++;
  else if (demand_register_prefix)
    return 0;

  /* Loop over all special registers.  */
  for (sregp = cris_spec_regs; sregp->name != NULL; sregp++)
    {
      /* Start over from beginning of the supposed name.  */
      s1 = name_begin;
      s2 = sregp->name;

      while (*s2 != '\0' && TOLOWER (*s1) == *s2)
	{
	  s1++;
	  s2++;
	}

      /* For a match, we must have consumed the name in the table, and we
	 must be outside what could be part of a name.	Assume here that a
	 test for alphanumerics is sufficient for a name test.  */
      if (*s2 == 0 && ! ISALNUM (*s1)
	  && cris_insn_ver_valid_for_arch (sregp->applicable_version,
					   cris_arch))
	{
	  /* We have a match.  Update the pointer and be done.  */
	  *cPP = s1;
	  *sregpp = sregp;
	  return 1;
	}
    }

  /* If we got here, we did not find any name.  */
  return 0;
}

/* Get a support register from the string pointed out by *cPP. The
   variable *cPP is advanced to the character following the support-
   register name if one is found, and retains its original position
   otherwise.

   cPP	    Pointer to pointer to string starting with a support-register
	    name.

   sregpp   Pointer to int containing the register number.

   Return 1 iff a correct support-register name is found.  */

static int
get_sup_reg (char **cPP, int *regnop)
{
  char *s1;
  const char *s2;
  char *name_begin = *cPP;

  const struct cris_support_reg *sregp;

  /* Handle a sometimes-mandatory dollar sign as register prefix.  */
  if (*name_begin == REGISTER_PREFIX_CHAR)
    name_begin++;
  else if (demand_register_prefix)
    return 0;

  /* Loop over all support-registers.  */
  for (sregp = cris_support_regs; sregp->name != NULL; sregp++)
    {
      /* Start over from beginning of the supposed name.  */
      s1 = name_begin;
      s2 = sregp->name;

      while (*s2 != '\0' && TOLOWER (*s1) == *s2)
	{
	  s1++;
	  s2++;
	}

      /* For a match, we must have consumed the name in the table, and we
	 must be outside what could be part of a name.	Assume here that a
	 test for alphanumerics is sufficient for a name test.  */
      if (*s2 == 0 && ! ISALNUM (*s1))
	{
	  /* We have a match.  Update the pointer and be done.  */
	  *cPP = s1;
	  *regnop = sregp->number;
	  return 1;
	}
    }

  /* If we got here, we did not find any name.  */
  return 0;
}

/* Get an unprefixed or side-effect-prefix operand from the string pointed
   out by *cPP.  The pointer *cPP is advanced to the character following
   the indirect operand if we have success, else it contains an undefined
   value.

   cPP		 Pointer to pointer to string beginning with the first
		 character of the supposed operand.

   prefixp	 Pointer to structure containing an optional instruction
		 prefix.

   is_autoincp	 Pointer to int indicating the indirect or autoincrement
		 bits.

   src_regnop	 Pointer to int containing the source register number in
		 the instruction.

   imm_foundp	 Pointer to an int indicating if an immediate expression
		 is found.

   imm_exprP	 Pointer to a structure containing an immediate
		 expression, if success and if *imm_foundp is nonzero.

   Return 1 iff a correct indirect operand is found.  */

static int
get_autoinc_prefix_or_indir_op (char **cPP, struct cris_prefix *prefixp,
				int *is_autoincp, int *src_regnop,
				int *imm_foundp, expressionS *imm_exprP)
{
  /* Assume there was no immediate mode expression.  */
  *imm_foundp = 0;

  if (**cPP == '[')
    {
      /* So this operand is one of:
	 Indirect: [rN]
	 Autoincrement: [rN+]
	 Indexed with assign: [rN=rM+rO.S]
	 Offset with assign: [rN=rM+I], [rN=rM+[rO].s], [rN=rM+[rO+].s]

	 Either way, consume the '['.  */
      (*cPP)++;

      /* Get the rN register.  */
      if (! get_gen_reg (cPP, src_regnop))
	/* If there was no register, then this cannot match.  */
	return 0;
      else
	{
	  /* We got the register, now check the next character.  */
	  switch (**cPP)
	    {
	    case ']':
	      /* Indirect mode.  We're done here.  */
	      prefixp->kind = PREFIX_NONE;
	      *is_autoincp = 0;
	      break;

	    case '+':
	      /* This must be an auto-increment mode, if there's a
		 match.  */
	      prefixp->kind = PREFIX_NONE;
	      *is_autoincp = 1;

	      /* We consume this character and break out to check the
		 closing ']'.  */
	      (*cPP)++;
	      break;

	    case '=':
	      /* This must be indexed with assign, or offset with assign
		 to match.  Not supported for crisv32 or in
		 compatibility mode.  */
	      if (cris_arch == arch_crisv32
		  || cris_arch == arch_cris_common_v10_v32)
		return 0;

	      (*cPP)++;

	      /* Either way, the next thing must be a register.  */
	      if (! get_gen_reg (cPP, &prefixp->base_reg_number))
		/* No register, no match.  */
		return 0;
	      else
		{
		  /* We've consumed "[rN=rM", so we must be looking at
		     "+rO.s]" or "+I]", or "-I]", or "+[rO].s]" or
		     "+[rO+].s]".  */
		  if (**cPP == '+')
		    {
		      int index_reg_number;
		      (*cPP)++;

		      if (**cPP == '[')
			{
			  int size_bits;
			  /* This must be [rx=ry+[rz].s] or
			     [rx=ry+[rz+].s] or no match.  We must be
			     looking at rz after consuming the '['.  */
			  (*cPP)++;

			  if (!get_gen_reg (cPP, &index_reg_number))
			    return 0;

			  prefixp->kind = PREFIX_BDAP;
			  prefixp->opcode
			    = (BDAP_INDIR_OPCODE
			       + (prefixp->base_reg_number << 12)
			       + index_reg_number);

			  if (**cPP == '+')
			    {
			      /* We've seen "[rx=ry+[rz+" here, so now we
				 know that there must be "].s]" left to
				 check.  */
			      (*cPP)++;
			      prefixp->opcode |= AUTOINCR_BIT << 8;
			    }

			  /* If it wasn't autoincrement, we don't need to
			     add anything.  */

			  /* Check the next-to-last ']'.  */
			  if (**cPP != ']')
			    return 0;

			  (*cPP)++;

			  /* Check the ".s" modifier.  */
			  if (! get_bwd_size_modifier (cPP, &size_bits))
			    return 0;

			  prefixp->opcode |= size_bits << 4;

			  /* Now we got [rx=ry+[rz+].s or [rx=ry+[rz].s.
			     We break out to check the final ']'.  */
			  break;
			}
		      /* It wasn't an indirection.  Check if it's a
			 register.  */
		      else if (get_gen_reg (cPP, &index_reg_number))
			{
			  int size_bits;

			  /* Indexed with assign mode: "[rN+rM.S]".  */
			  prefixp->kind = PREFIX_BIAP;
			  prefixp->opcode
			    = (BIAP_OPCODE + (index_reg_number << 12)
			       + prefixp->base_reg_number /* << 0 */);

			  if (! get_bwd_size_modifier (cPP, &size_bits))
			    /* Size missing, this isn't a match.  */
			    return 0;
			  else
			    {
			      /* Size found, break out to check the
				 final ']'.  */
			      prefixp->opcode |= size_bits << 4;
			      break;
			    }
			}
		      /* Not a register.  Then this must be "[rN+I]".  */
		      else if (cris_get_expression (cPP, &prefixp->expr))
			{
			  /* We've got offset with assign mode.  Fill
			     in the blanks and break out to match the
			     final ']'.  */
			  prefixp->kind = PREFIX_BDAP_IMM;

			  /* We tentatively put an opcode corresponding to
			     a 32-bit operand here, although it may be
			     relaxed when there's no PIC specifier for the
			     operand.  */
			  prefixp->opcode
			    = (BDAP_INDIR_OPCODE
			       | (prefixp->base_reg_number << 12)
			       | (AUTOINCR_BIT << 8)
			       | (2 << 4)
			       | REG_PC /* << 0 */);

			  /* This can have a PIC suffix, specifying reloc
			     type to use.  */
			  if (pic && **cPP == PIC_SUFFIX_CHAR)
			    {
			      unsigned int relocsize;

			      cris_get_pic_suffix (cPP, &prefixp->reloc,
						   &prefixp->expr);

			      /* Tweak the size of the immediate operand
				 in the prefix opcode if it isn't what we
				 set.  */
			      relocsize
				= cris_get_pic_reloc_size (prefixp->reloc);
			      if (relocsize != 4)
				prefixp->opcode
				  = ((prefixp->opcode & ~(3 << 4))
				     | ((relocsize >> 1) << 4));
			    }
			  break;
			}
		      else
			/* Neither register nor expression found, so
			   this can't be a match.  */
			return 0;
		    }
		  /* Not "[rN+" but perhaps "[rN-"?  */
		  else if (**cPP == '-')
		    {
		      /* We must have an offset with assign mode.  */
		      if (! cris_get_expression (cPP, &prefixp->expr))
			/* No expression, no match.  */
			return 0;
		      else
			{
			  /* We've got offset with assign mode.  Fill
			     in the blanks and break out to match the
			     final ']'.

			     Note that we don't allow a PIC suffix for an
			     operand with a minus sign.  */
			  prefixp->kind = PREFIX_BDAP_IMM;
			  break;
			}
		    }
		  else
		    /* Neither '+' nor '-' after "[rN=rM".  Lose.  */
		    return 0;
		}
	    default:
	      /* Neither ']' nor '+' nor '=' after "[rN".  Lose.  */
	      return 0;
	    }
	}

      /* When we get here, we have a match and will just check the closing
	 ']'.  We can still fail though.  */
      if (**cPP != ']')
	return 0;
      else
	{
	  /* Don't forget to consume the final ']'.
	     Then return in glory.  */
	  (*cPP)++;
	  return 1;
	}
    }
  /* No indirection.  Perhaps a constant?  */
  else if (cris_get_expression (cPP, imm_exprP))
    {
      /* Expression found, this is immediate mode.  */
      prefixp->kind = PREFIX_NONE;
      *is_autoincp = 1;
      *src_regnop = REG_PC;
      *imm_foundp = 1;

      /* This can have a PIC suffix, specifying reloc type to use.  The
	 caller must check that the reloc size matches the operand size.  */
      if (pic && **cPP == PIC_SUFFIX_CHAR)
	cris_get_pic_suffix (cPP, &prefixp->reloc, imm_exprP);

      return 1;
    }

  /* No luck today.  */
  return 0;
}

/* This function gets an indirect operand in a three-address operand
   combination from the string pointed out by *cPP.  The pointer *cPP is
   advanced to the character following the indirect operand on success, or
   has an unspecified value on failure.

   cPP	     Pointer to pointer to string beginning
	     with the operand

   prefixp   Pointer to structure containing an
	     instruction prefix

   Returns 1 iff a correct indirect operand is found.  */

static int
get_3op_or_dip_prefix_op (char **cPP, struct cris_prefix *prefixp)
{
  int reg_number;

  if (**cPP != '[')
    /* We must have a '[' or it's a clean failure.  */
    return 0;

  /* Eat the first '['.  */
  (*cPP)++;

  if (**cPP == '[')
    {
      /* A second '[', so this must be double-indirect mode.  */
      (*cPP)++;
      prefixp->kind = PREFIX_DIP;
      prefixp->opcode = DIP_OPCODE;

      /* Get the register or fail entirely.  */
      if (! get_gen_reg (cPP, &reg_number))
	return 0;
      else
	{
	  prefixp->opcode |= reg_number /* << 0 */ ;
	  if (**cPP == '+')
	    {
	      /* Since we found a '+', this must be double-indirect
		 autoincrement mode.  */
	      (*cPP)++;
	      prefixp->opcode |= AUTOINCR_BIT << 8;
	    }

	  /* There's nothing particular to do, if this was a
	     double-indirect *without* autoincrement.  */
	}

      /* Check the first ']'.  The second one is checked at the end.  */
      if (**cPP != ']')
	return 0;

      /* Eat the first ']', so we'll be looking at a second ']'.  */
      (*cPP)++;
    }
  /* No second '['.  Then we should have a register here, making
     it "[rN".  */
  else if (get_gen_reg (cPP, &prefixp->base_reg_number))
    {
      /* This must be indexed or offset mode: "[rN+I]" or
	 "[rN+rM.S]" or "[rN+[rM].S]" or "[rN+[rM+].S]".  */
      if (**cPP == '+')
	{
	  int index_reg_number;

	  (*cPP)++;

	  if (**cPP == '[')
	    {
	      /* This is "[rx+["...  Expect a register next.  */
	      int size_bits;
	      (*cPP)++;

	      if (!get_gen_reg (cPP, &index_reg_number))
		return 0;

	      prefixp->kind = PREFIX_BDAP;
	      prefixp->opcode
		= (BDAP_INDIR_OPCODE
		   + (prefixp->base_reg_number << 12)
		   + index_reg_number);

	      /* We've seen "[rx+[ry", so check if this is
		 autoincrement.  */
	      if (**cPP == '+')
		{
		  /* Yep, now at "[rx+[ry+".  */
		  (*cPP)++;
		  prefixp->opcode |= AUTOINCR_BIT << 8;
		}
	      /* If it wasn't autoincrement, we don't need to
		 add anything.  */

	      /* Check a first closing ']': "[rx+[ry]" or
		 "[rx+[ry+]".  */
	      if (**cPP != ']')
		return 0;
	      (*cPP)++;

	      /* Now expect a size modifier ".S".  */
	      if (! get_bwd_size_modifier (cPP, &size_bits))
		return 0;

	      prefixp->opcode |= size_bits << 4;

	      /* Ok, all interesting stuff has been seen:
		 "[rx+[ry+].S" or "[rx+[ry].S".  We only need to
		 expect a final ']', which we'll do in a common
		 closing session.  */
	    }
	  /* Seen "[rN+", but not a '[', so check if we have a
	     register.  */
	  else if (get_gen_reg (cPP, &index_reg_number))
	    {
	      /* This is indexed mode: "[rN+rM.S]" or
		 "[rN+rM.S+]".  */
	      int size_bits;
	      prefixp->kind = PREFIX_BIAP;
	      prefixp->opcode
		= (BIAP_OPCODE
		   | prefixp->base_reg_number /* << 0 */
		   | (index_reg_number << 12));

	      /* Consume the ".S".  */
	      if (! get_bwd_size_modifier (cPP, &size_bits))
		/* Missing size, so fail.  */
		return 0;
	      else
		/* Size found.  Add that piece and drop down to
		   the common checking of the closing ']'.  */
		prefixp->opcode |= size_bits << 4;
	    }
	  /* Seen "[rN+", but not a '[' or a register, so then
	     it must be a constant "I".

	     As a quality of implementation improvement, we check for a
	     closing ']', like in an erroneous "[rN+]".  If we don't,
	     the expression parser will emit a confusing "bad
	     expression" when it sees the ']', probably because it
	     doesn't like seeing no expression.  */
	  else if (**cPP != ']' && cris_get_expression (cPP, &prefixp->expr))
	    {
	      /* Expression found, so fill in the bits of offset
		 mode and drop down to check the closing ']'.  */
	      prefixp->kind = PREFIX_BDAP_IMM;

	      /* We tentatively put an opcode corresponding to a 32-bit
		 operand here, although it may be relaxed when there's no
		 PIC specifier for the operand.  */
	      prefixp->opcode
		= (BDAP_INDIR_OPCODE
		   | (prefixp->base_reg_number << 12)
		   | (AUTOINCR_BIT << 8)
		   | (2 << 4)
		   | REG_PC /* << 0 */);

	      /* This can have a PIC suffix, specifying reloc type to use.  */
	      if (pic && **cPP == PIC_SUFFIX_CHAR)
		{
		  unsigned int relocsize;

		  cris_get_pic_suffix (cPP, &prefixp->reloc, &prefixp->expr);

		  /* Tweak the size of the immediate operand in the prefix
		     opcode if it isn't what we set.  */
		  relocsize = cris_get_pic_reloc_size (prefixp->reloc);
		  if (relocsize != 4)
		    prefixp->opcode
		      = ((prefixp->opcode & ~(3 << 4))
			 | ((relocsize >> 1) << 4));
		}
	    }
	  else
	    /* Nothing valid here: lose.  */
	    return 0;
	}
      /* Seen "[rN" but no '+', so check if it's a '-'.  */
      else if (**cPP == '-')
	{
	  /* Yep, we must have offset mode.  */
	  if (! cris_get_expression (cPP, &prefixp->expr))
	    /* No expression, so we lose.  */
	    return 0;
	  else
	    {
	      /* Expression found to make this offset mode, so
		 fill those bits and drop down to check the
		 closing ']'.

		 Note that we don't allow a PIC suffix for
		 an operand with a minus sign like this.  */
	      prefixp->kind = PREFIX_BDAP_IMM;
	    }
	}
      else
	{
	  /* We've seen "[rN", but not '+' or '-'; rather a ']'.
	     Hmm.  Normally this is a simple indirect mode that we
	     shouldn't match, but if we expect ']', then we have a
	     zero offset, so it can be a three-address-operand,
	     like "[rN],rO,rP", thus offset mode.

	     Don't eat the ']', that will be done in the closing
	     ceremony.  */
	  prefixp->expr.X_op = O_constant;
	  prefixp->expr.X_add_number = 0;
	  prefixp->expr.X_add_symbol = NULL;
	  prefixp->expr.X_op_symbol = NULL;
	  prefixp->kind = PREFIX_BDAP_IMM;
	}
    }
  /* A '[', but no second '[', and no register.  Check if we
     have an expression, making this "[I]" for a double-indirect
     prefix.  */
  else if (cris_get_expression (cPP, &prefixp->expr))
    {
      /* Expression found, the so called absolute mode for a
	 double-indirect prefix on PC.  */
      prefixp->kind = PREFIX_DIP;
      prefixp->opcode = DIP_OPCODE | (AUTOINCR_BIT << 8) | REG_PC;
      prefixp->reloc = BFD_RELOC_32;
    }
  else
    /* Neither '[' nor register nor expression.  We lose.  */
    return 0;

  /* We get here as a closing ceremony to a successful match.  We just
     need to check the closing ']'.  */
  if (**cPP != ']')
    /* Oops.  Close but no air-polluter.  */
    return 0;

  /* Don't forget to consume that ']', before returning in glory.  */
  (*cPP)++;
  return 1;
}

/* Get an expression from the string pointed out by *cPP.
   The pointer *cPP is advanced to the character following the expression
   on a success, or retains its original value otherwise.

   cPP	   Pointer to pointer to string beginning with the expression.

   exprP   Pointer to structure containing the expression.

   Return 1 iff a correct expression is found.  */

static int
cris_get_expression (char **cPP, expressionS *exprP)
{
  char *saved_input_line_pointer;
  segT exp;

  /* The "expression" function expects to find an expression at the
     global variable input_line_pointer, so we have to save it to give
     the impression that we don't fiddle with global variables.  */
  saved_input_line_pointer = input_line_pointer;
  input_line_pointer = *cPP;

  /* Avoid a common error, confusing addressing modes.  Beware that the
     call to expression below does not signal that error; it treats []
     as parentheses, unless #define NEED_INDEX_OPERATOR in which case it
     gives them other confusing semantics rather than plain outlawing
     them, which is what we want.  */
  if (*input_line_pointer == '[')
    {
      input_line_pointer = saved_input_line_pointer;
      return 0;
    }

  exp = expression (exprP);
  if (exprP->X_op == O_illegal || exprP->X_op == O_absent)
    {
      input_line_pointer = saved_input_line_pointer;
      return 0;
    }

  /* Everything seems to be fine, just restore the global
     input_line_pointer and say we're successful.  */
  *cPP = input_line_pointer;
  input_line_pointer = saved_input_line_pointer;
  return 1;
}

/* Get a sequence of flag characters from *spp.  The pointer *cPP is
   advanced to the character following the expression.	The flag
   characters are consecutive, no commas or spaces.

   cPP	     Pointer to pointer to string beginning with the expression.

   flagp     Pointer to int to return the flags expression.

   Return 1 iff a correct flags expression is found.  */

static int
get_flags (char **cPP, int *flagsp)
{
  for (;;)
    {
      switch (**cPP)
	{
	case 'd':
	case 'D':
	  if (! cris_insn_ver_valid_for_arch (cris_ver_v0_3,
					      cris_arch))
	    return 0;
	  *flagsp |= 0x80;
	  break;

	case 'm':
	case 'M':
	  if (! cris_insn_ver_valid_for_arch (cris_ver_v8_10,
					      cris_arch))
	    return 0;
	  *flagsp |= 0x80;
	  break;

	case 'e':
	case 'E':
	  if (! cris_insn_ver_valid_for_arch (cris_ver_v0_3,
					      cris_arch))
	    return 0;
	  *flagsp |= 0x40;
	  break;

	case 'b':
	case 'B':
	  if (! cris_insn_ver_valid_for_arch (cris_ver_v8_10,
					      cris_arch))
	    return 0;
	  *flagsp |= 0x40;
	  break;

	case 'p':
	case 'P':
	  if (! cris_insn_ver_valid_for_arch (cris_ver_v32p,
					      cris_arch))
	    return 0;
	  *flagsp |= 0x80;
	  break;

	case 'u':
	case 'U':
	  if (! cris_insn_ver_valid_for_arch (cris_ver_v32p,
					      cris_arch))
	    return 0;
	  *flagsp |= 0x40;
	  break;

	case 'i':
	case 'I':
	  *flagsp |= 0x20;
	  break;

	case 'x':
	case 'X':
	  *flagsp |= 0x10;
	  break;

	case 'n':
	case 'N':
	  *flagsp |= 0x8;
	  break;

	case 'z':
	case 'Z':
	  *flagsp |= 0x4;
	  break;

	case 'v':
	case 'V':
	  *flagsp |= 0x2;
	  break;

	case 'c':
	case 'C':
	  *flagsp |= 1;
	  break;

	default:
	  /* We consider this successful if we stop at a comma or
	     whitespace.  Anything else, and we consider it a failure.  */
	  if (**cPP != ','
	      && **cPP != 0
	      && ! ISSPACE (**cPP))
	    return 0;
	  else
	    return 1;
	}

      /* Don't forget to consume each flag character.  */
      (*cPP)++;
    }
}

/* Generate code and fixes for a BDAP prefix.
   For v32, this handles ADDOQ because thankfully the opcodes are the
   same.

   base_regno	Int containing the base register number.

   exprP	Pointer to structure containing the offset expression.  */

static void
gen_bdap (int base_regno, expressionS *exprP)
{
  unsigned int opcode;
  char *opcodep;

  /* Put out the prefix opcode; assume quick immediate mode at first.  */
  opcode = BDAP_QUICK_OPCODE | (base_regno << 12);
  opcodep = cris_insn_first_word_frag ();
  md_number_to_chars (opcodep, opcode, 2);

  if (exprP->X_op == O_constant)
    {
      /* We have an absolute expression that we know the size of right
	 now.  */
      long int value;
      int size;

      value = exprP->X_add_number;
      if (value < -32768 || value > 32767)
	/* Outside range for a "word", make it a dword.  */
	size = 2;
      else
	/* Assume "word" size.  */
	size = 1;

      /* If this is a signed-byte value, we can fit it into the prefix
	 insn itself.  */
      if (value >= -128 && value <= 127)
	opcodep[0] = value;
      else
	{
	  /* This is a word or dword displacement, which will be put in a
	     word or dword after the prefix.  */
	  char *p;

	  opcodep[0] = BDAP_PC_LOW + (size << 4);
	  opcodep[1] &= 0xF0;
	  opcodep[1] |= BDAP_INCR_HIGH;
	  p = frag_more (1 << size);
	  md_number_to_chars (p, value, 1 << size);
	}
    }
  else
    {
      /* Handle complex expressions.  */
      valueT addvalue
	= SIMPLE_EXPR (exprP) ? exprP->X_add_number : 0;
      symbolS *sym
	= (SIMPLE_EXPR (exprP)
	   ? exprP->X_add_symbol : make_expr_symbol (exprP));

      /* The expression is not defined yet but may become absolute.  We
	 make it a relocation to be relaxed.  */
      frag_var (rs_machine_dependent, 4, 0,
		ENCODE_RELAX (STATE_BASE_PLUS_DISP_PREFIX, STATE_UNDF),
		sym, addvalue, opcodep);
    }
}

/* Encode a branch displacement in the range -256..254 into the form used
   by CRIS conditional branch instructions.

   offset  The displacement value in bytes.  */

static int
branch_disp (int offset)
{
  int disp;

  /* Adjust all short branch offsets here.  */
  if (cris_arch == arch_crisv32 || cris_arch == arch_cris_common_v10_v32)
    offset += 2;

  disp = offset & 0xFE;

  if (offset < 0)
    disp |= 1;

  return disp;
}

/* Generate code and fixes for a 32-bit conditional branch instruction
   created by "extending" an existing 8-bit branch instruction.

   opcodep    Pointer to the word containing the original 8-bit branch
	      instruction.

   writep     Pointer to "extension area" following the first instruction
	      word.

   fragP      Pointer to the frag containing the instruction.

   add_symP,  Parts of the destination address expression.
   sub_symP,
   add_num.  */

static void
gen_cond_branch_32 (char *opcodep, char *writep, fragS *fragP,
		    symbolS *add_symP, symbolS *sub_symP, long int add_num)
{
  int nop_opcode;
  int opc_offset;
  int branch_offset;

  if (cris_arch == arch_crisv32)
    {
      nop_opcode = NOP_OPCODE_V32;
      opc_offset = 10;
      branch_offset = -2 - 8;
    }
  else if (pic)
    {
      nop_opcode = NOP_OPCODE;
      opc_offset = 10;
      branch_offset = -2 - 8;
    }
  else
    {
      nop_opcode = NOP_OPCODE;
      opc_offset = 8;
      branch_offset = -2 - 6;
    }

  /* We should never get here for compatibility mode.  */
  if (cris_arch == arch_cris_common_v10_v32)
    as_fatal (_("Calling gen_cond_branch_32 for .arch common_v10_v32\n"));

  if (warn_for_branch_expansion)
    as_warn_where (fragP->fr_file, fragP->fr_line,
		   _("32-bit conditional branch generated"));

  /* Here, writep points to what will be opcodep + 2.  First, we change
     the actual branch in opcodep[0] and opcodep[1], so that in the
     final insn, it will look like:
       opcodep+10: Bcc .-6

     This means we don't have to worry about changing the opcode or
     messing with the delay-slot instruction.  So, we move it to last in
     the "extended" branch, and just change the displacement.  Admittedly,
     it's not the optimal extended construct, but we should get this
     rarely enough that it shouldn't matter.  */

  writep[opc_offset] = branch_disp (branch_offset);
  writep[opc_offset + 1] = opcodep[1];

  /* Then, we change the branch to an unconditional branch over the
     extended part, to the new location of the Bcc:
       opcodep:	  BA .+10
       opcodep+2: NOP

     Note that these two writes are to currently different locations,
     merged later.  */

  md_number_to_chars (opcodep, BA_QUICK_OPCODE
		      + (cris_arch == arch_crisv32 ? 12 : (pic ? 10 : 8)),
		      2);
  md_number_to_chars (writep, nop_opcode, 2);

  /* Then the extended thing, the 32-bit jump insn.
       opcodep+4: JUMP [PC+]
     or, in the PIC case,
       opcodep+4: MOVE [PC=PC+N],P0.  */

  md_number_to_chars (writep + 2,
		      cris_arch == arch_crisv32
		      ? BA_DWORD_OPCODE
		      : (pic ? MOVE_PC_INCR_OPCODE_PREFIX
			 : JUMP_PC_INCR_OPCODE), 2);

  /* We have to fill in the actual value too.
       opcodep+6: .DWORD
     This is most probably an expression, but we can cope with an absolute
     value too.  FIXME: Testcase needed with and without pic.  */

  if (add_symP == NULL && sub_symP == NULL)
    {
      /* An absolute address.  */
      if (pic || cris_arch == arch_crisv32)
	fix_new (fragP, writep + 4 - fragP->fr_literal, 4,
		 section_symbol (absolute_section),
		 add_num
		 + (cris_arch == arch_crisv32 ? 6 : 0),
		 1, BFD_RELOC_32_PCREL);
      else
	md_number_to_chars (writep + 4, add_num, 4);
    }
  else
    {
      if (sub_symP != NULL)
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("Complex expression not supported"));

      /* Not absolute, we have to make it a frag for later evaluation.  */
      fix_new (fragP, writep + 4 - fragP->fr_literal, 4, add_symP,
	       add_num + (cris_arch == arch_crisv32 ? 6 : 0),
	       pic || cris_arch == arch_crisv32 ? 1 : 0,
	       pic || cris_arch == arch_crisv32
	       ? BFD_RELOC_32_PCREL : BFD_RELOC_32);
    }

  if (cris_arch == arch_crisv32)
    /* Follow it with a "NOP" for CRISv32.  */
    md_number_to_chars (writep + 8, NOP_OPCODE_V32, 2);
  else if (pic)
    /* ...and the rest of the move-opcode for pre-v32 PIC.  */
    md_number_to_chars (writep + 8, MOVE_PC_INCR_OPCODE_SUFFIX, 2);
}

/* Get the size of an immediate-reloc in bytes.  Only valid for PIC
   relocs.  */

static unsigned int
cris_get_pic_reloc_size (bfd_reloc_code_real_type reloc)
{
  return reloc == BFD_RELOC_CRIS_16_GOTPLT || reloc == BFD_RELOC_CRIS_16_GOT
    ? 2 : 4;
}

/* Store a reloc type at *RELOCP corresponding to the PIC suffix at *CPP.
   Adjust *EXPRP with any addend found after the PIC suffix.  */

static void
cris_get_pic_suffix (char **cPP, bfd_reloc_code_real_type *relocp,
		     expressionS *exprP)
{
  char *s = *cPP;
  unsigned int i;
  expressionS const_expr;

  const struct pic_suffixes_struct
  {
    const char *const suffix;
    unsigned int len;
    bfd_reloc_code_real_type reloc;
  } pic_suffixes[] =
    {
#undef PICMAP
#define PICMAP(s, r) {s, sizeof (s) - 1, r}
      /* Keep this in order with longest unambiguous prefix first.  */
      PICMAP ("GOTPLT16", BFD_RELOC_CRIS_16_GOTPLT),
      PICMAP ("GOTPLT", BFD_RELOC_CRIS_32_GOTPLT),
      PICMAP ("PLTG", BFD_RELOC_CRIS_32_PLT_GOTREL),
      PICMAP ("PLT", BFD_RELOC_CRIS_32_PLT_PCREL),
      PICMAP ("GOTOFF", BFD_RELOC_CRIS_32_GOTREL),
      PICMAP ("GOT16", BFD_RELOC_CRIS_16_GOT),
      PICMAP ("GOT", BFD_RELOC_CRIS_32_GOT)
    };

  /* We've already seen the ':', so consume it.  */
  s++;

  for (i = 0; i < sizeof (pic_suffixes)/sizeof (pic_suffixes[0]); i++)
    {
      if (strncmp (s, pic_suffixes[i].suffix, pic_suffixes[i].len) == 0
	  && ! is_part_of_name (s[pic_suffixes[i].len]))
	{
	  /* We have a match.  Consume the suffix and set the relocation
	     type.   */
	  s += pic_suffixes[i].len;

	  /* There can be a constant term appended.  If so, we will add it
	     to *EXPRP.  */
	  if (*s == '+' || *s == '-')
	    {
	      if (! cris_get_expression (&s, &const_expr))
		/* There was some kind of syntax error.  Bail out.  */
		break;

	      /* Allow complex expressions as the constant part.  It still
		 has to be an assembly-time constant or there will be an
		 error emitting the reloc.  This makes the PIC qualifiers
		 idempotent; foo:GOTOFF+32 == foo+32:GOTOFF.  The former we
		 recognize here; the latter is parsed in the incoming
		 expression.  */
	      exprP->X_add_symbol = make_expr_symbol (exprP);
	      exprP->X_op = O_add;
	      exprP->X_add_number = 0;
	      exprP->X_op_symbol = make_expr_symbol (&const_expr);
	    }

	  *relocp = pic_suffixes[i].reloc;
	  *cPP = s;
	  return;
	}
    }

  /* No match.  Don't consume anything; fall back and there will be a
     syntax error.  */
}

/* This *could* have been:

   Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.

   type	  A character from FLTCHARS that describes what kind of
	  floating-point number is wanted.

   litp	  A pointer to an array that the result should be stored in.

   sizep  A pointer to an integer where the size of the result is stored.

   But we don't support floating point constants in assembly code *at all*,
   since it's suboptimal and just opens up bug opportunities.  GCC emits
   the bit patterns as hex.  All we could do here is to emit what GCC
   would have done in the first place.	*Nobody* writes floating-point
   code as assembly code, but if they do, they should be able enough to
   find out the correct bit patterns and use them.  */

char *
md_atof (int type ATTRIBUTE_UNUSED, char *litp ATTRIBUTE_UNUSED,
	 int *sizep ATTRIBUTE_UNUSED)
{
  /* FIXME:  Is this function mentioned in the internals.texi manual?  If
     not, add it.  */
  return  _("Bad call to md_atof () - floating point formats are not supported");
}

/* Turn a number as a fixS * into a series of bytes that represents the
   number on the target machine.  The purpose of this procedure is the
   same as that of md_number_to_chars but this procedure is supposed to
   handle general bit field fixes and machine-dependent fixups.

   bufp	       Pointer to an array where the result should be stored.

   val	      The value to store.

   n	      The number of bytes in "val" that should be stored.

   fixP	      The fix to be applied to the bit field starting at bufp.

   seg	      The segment containing this number.  */

static void
cris_number_to_imm (char *bufp, long val, int n, fixS *fixP, segT seg)
{
  segT sym_seg;

  know (n <= 4);
  know (fixP);

  /* We put the relative "vma" for the other segment for inter-segment
     relocations in the object data to stay binary "compatible" (with an
     uninteresting old version) for the relocation.
     Maybe delete some day.  */
  if (fixP->fx_addsy
      && (sym_seg = S_GET_SEGMENT (fixP->fx_addsy)) != seg)
    val += sym_seg->vma;

  if (fixP->fx_addsy != NULL || fixP->fx_pcrel)
    switch (fixP->fx_r_type)
      {
	/* These must be fully resolved when getting here.  */
      case BFD_RELOC_16_PCREL:
      case BFD_RELOC_8_PCREL:
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("PC-relative relocation must be trivially resolved"));
      default:
	;
      }

  /* Only do this for old-arch binaries.  */
  if (cris_arch != arch_cris_any_v0_v10
      && (fixP->fx_addsy != NULL || fixP->fx_pcrel))
    return;

  switch (fixP->fx_r_type)
    {
      /* Ditto here, we put the addend into the object code as
	 well as the reloc addend.  Keep it that way for now, to simplify
	 regression tests on the object file contents.	FIXME:	Seems
	 uninteresting now that we have a test suite.  */

    case BFD_RELOC_CRIS_16_GOT:
    case BFD_RELOC_CRIS_32_GOT:
    case BFD_RELOC_CRIS_32_GOTREL:
    case BFD_RELOC_CRIS_16_GOTPLT:
    case BFD_RELOC_CRIS_32_GOTPLT:
    case BFD_RELOC_CRIS_32_PLT_GOTREL:
    case BFD_RELOC_CRIS_32_PLT_PCREL:
      /* We don't want to put in any kind of non-zero bits in the data
	 being relocated for these.  */
      break;

    case BFD_RELOC_32_PCREL:
      /* If this one isn't fully resolved, we don't want to put anything
	 in the object.  */
      if (fixP->fx_addsy != NULL || fixP->fx_pcrel)
	break;

      /* Fall through.  */
    case BFD_RELOC_32:
      /* No use having warnings here, since most hosts have a 32-bit type
	 for "long" (which will probably change soon, now that I wrote
	 this).  */
      bufp[3] = (val >> 24) & 0xFF;
      bufp[2] = (val >> 16) & 0xFF;
      bufp[1] = (val >> 8) & 0xFF;
      bufp[0] = val & 0xFF;
      break;

      /* FIXME: The 16 and 8-bit cases should have a way to check
	 whether a signed or unsigned (or any signedness) number is
	 accepted.  */

    case BFD_RELOC_16:
    case BFD_RELOC_16_PCREL:
      if (val > 0xffff || val < -32768)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 16 bit range: %ld"), val);
      if (! fixP->fx_addsy)
	{
	  bufp[1] = (val >> 8) & 0xFF;
	  bufp[0] = val & 0xFF;
	}
      break;

    case BFD_RELOC_CRIS_SIGNED_16:
      if (val > 32767 || val < -32768)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 16 bit signed range: %ld"), val);
      if (! fixP->fx_addsy)
	{
	  bufp[1] = (val >> 8) & 0xFF;
	  bufp[0] = val & 0xFF;
	}
      break;

    case BFD_RELOC_8:
    case BFD_RELOC_8_PCREL:
      if (val > 255 || val < -128)
	as_bad_where (fixP->fx_file, fixP->fx_line, _("Value not in 8 bit range: %ld"), val);
      if (! fixP->fx_addsy)
	bufp[0] = val & 0xFF;
      break;

    case BFD_RELOC_CRIS_SIGNED_8:
      if (val > 127 || val < -128)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 8 bit signed range: %ld"), val);
      if (! fixP->fx_addsy)
	bufp[0] = val & 0xFF;
      break;

    case BFD_RELOC_CRIS_LAPCQ_OFFSET:
      /* FIXME: Test-cases for out-of-range values.  Probably also need
	 to use as_bad_where.  */
    case BFD_RELOC_CRIS_UNSIGNED_4:
      if (val > 15 || val < 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 4 bit unsigned range: %ld"), val);
      if (! fixP->fx_addsy)
	bufp[0] |= val & 0x0F;
      break;

    case BFD_RELOC_CRIS_UNSIGNED_5:
      if (val > 31 || val < 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 5 bit unsigned range: %ld"), val);
      if (! fixP->fx_addsy)
	bufp[0] |= val & 0x1F;
      break;

    case BFD_RELOC_CRIS_SIGNED_6:
      if (val > 31 || val < -32)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 6 bit range: %ld"), val);
      if (! fixP->fx_addsy)
	bufp[0] |= val & 0x3F;
      break;

    case BFD_RELOC_CRIS_UNSIGNED_6:
      if (val > 63 || val < 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Value not in 6 bit unsigned range: %ld"), val);
      if (! fixP->fx_addsy)
	bufp[0] |= val & 0x3F;
      break;

    case BFD_RELOC_CRIS_BDISP8:
      if (! fixP->fx_addsy)
	bufp[0] = branch_disp (val);
      break;

    case BFD_RELOC_NONE:
      /* May actually happen automatically.  For example at broken
	 words, if the word turns out not to be broken.
	 FIXME: When?  Which testcase?  */
      if (! fixP->fx_addsy)
	md_number_to_chars (bufp, val, n);
      break;

    case BFD_RELOC_VTABLE_INHERIT:
      /* This borrowed from tc-ppc.c on a whim.  */
      if (fixP->fx_addsy
	  && !S_IS_DEFINED (fixP->fx_addsy)
	  && !S_IS_WEAK (fixP->fx_addsy))
	S_SET_WEAK (fixP->fx_addsy);
      /* Fall through.  */

    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      break;

    default:
      BAD_CASE (fixP->fx_r_type);
    }
}

/* Processes machine-dependent command line options.  Called once for
   each option on the command line that the machine-independent part of
   GAS does not understand.  */

int
md_parse_option (int arg, char *argp ATTRIBUTE_UNUSED)
{
  switch (arg)
    {
    case 'H':
    case 'h':
      printf (_("Please use --help to see usage and options for this assembler.\n"));
      md_show_usage (stdout);
      exit (EXIT_SUCCESS);

    case 'N':
      warn_for_branch_expansion = 1;
      break;

    case OPTION_NO_US:
      demand_register_prefix = TRUE;

      if (OUTPUT_FLAVOR == bfd_target_aout_flavour)
	as_bad (_("--no-underscore is invalid with a.out format"));
      else
	symbols_have_leading_underscore = FALSE;
      break;

    case OPTION_US:
      demand_register_prefix = FALSE;
      symbols_have_leading_underscore = TRUE;
      break;

    case OPTION_PIC:
      pic = TRUE;
      if (cris_arch != arch_crisv32)
	md_long_jump_size = cris_any_v0_v10_long_jump_size_pic;
      else
	md_long_jump_size = crisv32_long_jump_size;
      break;

    case OPTION_ARCH:
      {
	char *str = argp;
	enum cris_archs argarch = cris_arch_from_string (&str);

	if (argarch == arch_cris_unknown)
	    as_bad (_("invalid <arch> in --march=<arch>: %s"), argp);
	else
	  cris_arch = argarch;

	if (argarch == arch_crisv32)
	  {
	    err_for_dangerous_mul_placement = 0;
	    md_long_jump_size = crisv32_long_jump_size;
	  }
	else
	  {
	    if (pic)
	      md_long_jump_size = cris_any_v0_v10_long_jump_size_pic;
	    else
	      md_long_jump_size = cris_any_v0_v10_long_jump_size;
	  }
      }
      break;

    case OPTION_MULBUG_ABORT_OFF:
      err_for_dangerous_mul_placement = 0;
      break;

    case OPTION_MULBUG_ABORT_ON:
      err_for_dangerous_mul_placement = 1;
      break;

    default:
      return 0;
    }

  return 1;
}

/* Round up a section size to the appropriate boundary.  */
valueT
md_section_align (segT segment, valueT size)
{
  /* Round all sects to multiple of 4, except the bss section, which
     we'll round to word-size.

     FIXME: Check if this really matters.  All sections should be
     rounded up, and all sections should (optionally) be assumed to be
     dword-aligned, it's just that there is actual usage of linking to a
     multiple of two.  */
  if (OUTPUT_FLAVOR == bfd_target_aout_flavour)
    {
      if (segment == bss_section)
	return (size + 1) & ~1;
      return (size + 3) & ~3;
    }
  else
    {
      /* FIXME: Is this wanted?  It matches the testsuite, but that's not
	 really a valid reason.  */
      if (segment == text_section)
	return (size + 3) & ~3;
    }

  return size;
}

/* Generate a machine-dependent relocation.  */
arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixP)
{
  arelent *relP;
  bfd_reloc_code_real_type code;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_CRIS_SIGNED_8:
      code = BFD_RELOC_8;
      break;

    case BFD_RELOC_CRIS_SIGNED_16:
      code = BFD_RELOC_16;
      break;

    case BFD_RELOC_CRIS_16_GOT:
    case BFD_RELOC_CRIS_32_GOT:
    case BFD_RELOC_CRIS_16_GOTPLT:
    case BFD_RELOC_CRIS_32_GOTPLT:
    case BFD_RELOC_CRIS_32_GOTREL:
    case BFD_RELOC_CRIS_32_PLT_GOTREL:
    case BFD_RELOC_CRIS_32_PLT_PCREL:
    case BFD_RELOC_32:
    case BFD_RELOC_32_PCREL:
    case BFD_RELOC_16:
    case BFD_RELOC_8:
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
    case BFD_RELOC_CRIS_UNSIGNED_8:
    case BFD_RELOC_CRIS_UNSIGNED_16:
    case BFD_RELOC_CRIS_LAPCQ_OFFSET:
      code = fixP->fx_r_type;
      break;
    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("Semantics error.  This type of operand can not be relocated, it must be an assembly-time constant"));
      return 0;
    }

  relP = (arelent *) xmalloc (sizeof (arelent));
  assert (relP != 0);
  relP->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *relP->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  relP->address = fixP->fx_frag->fr_address + fixP->fx_where;

  relP->addend = fixP->fx_offset;

  /* This is the standard place for KLUDGEs to work around bugs in
     bfd_install_relocation (first such note in the documentation
     appears with binutils-2.8).

     That function bfd_install_relocation does the wrong thing with
     putting stuff into the addend of a reloc (it should stay out) for a
     weak symbol.  The really bad thing is that it adds the
     "segment-relative offset" of the symbol into the reloc.  In this
     case, the reloc should instead be relative to the symbol with no
     other offset than the assembly code shows; and since the symbol is
     weak, any local definition should be ignored until link time (or
     thereafter).
     To wit:  weaksym+42  should be weaksym+42 in the reloc,
     not weaksym+(offset_from_segment_of_local_weaksym_definition)

     To "work around" this, we subtract the segment-relative offset of
     "known" weak symbols.  This evens out the extra offset.

     That happens for a.out but not for ELF, since for ELF,
     bfd_install_relocation uses the "special function" field of the
     howto, and does not execute the code that needs to be undone.  */

  if (OUTPUT_FLAVOR == bfd_target_aout_flavour
      && fixP->fx_addsy && S_IS_WEAK (fixP->fx_addsy)
      && ! bfd_is_und_section (S_GET_SEGMENT (fixP->fx_addsy)))
    {
      relP->addend -= S_GET_VALUE (fixP->fx_addsy);
    }

  relP->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (! relP->howto)
    {
      const char *name;

      name = S_GET_NAME (fixP->fx_addsy);
      if (name == NULL)
	name = _("<unknown>");
      as_fatal (_("Cannot generate relocation type for symbol %s, code %s"),
		name, bfd_get_reloc_code_name (code));
    }

  return relP;
}

/* Machine-dependent usage-output.  */

void
md_show_usage (FILE *stream)
{
  /* The messages are formatted to line up with the generic options.  */
  fprintf (stream, _("CRIS-specific options:\n"));
  fprintf (stream, "%s",
	   _("  -h, -H                  Don't execute, print this help text.  Deprecated.\n"));
  fprintf (stream, "%s",
	   _("  -N                      Warn when branches are expanded to jumps.\n"));
  fprintf (stream, "%s",
	   _("  --underscore            User symbols are normally prepended with underscore.\n"));
  fprintf (stream, "%s",
	   _("                          Registers will not need any prefix.\n"));
  fprintf (stream, "%s",
	   _("  --no-underscore         User symbols do not have any prefix.\n"));
  fprintf (stream, "%s",
	   _("                          Registers will require a `$'-prefix.\n"));
  fprintf (stream, "%s",
	   _("  --pic			Enable generation of position-independent code.\n"));
  fprintf (stream, "%s",
	   _("  --march=<arch>		Generate code for <arch>.  Valid choices for <arch>\n\
				are v0_v10, v10, v32 and common_v10_v32.\n"));
}

/* Apply a fixS (fixup of an instruction or data that we didn't have
   enough info to complete immediately) to the data in a frag.  */

void
md_apply_fix (fixS *fixP, valueT *valP, segT seg)
{
  /* This assignment truncates upper bits if valueT is 64 bits (as with
     --enable-64-bit-bfd), which is fine here, though we cast to avoid
     any compiler warnings.  */
  long val = (long) *valP;
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;

  if (fixP->fx_addsy == 0 && !fixP->fx_pcrel)
    fixP->fx_done = 1;

  if (fixP->fx_bit_fixP || fixP->fx_im_disp != 0)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line, _("Invalid relocation"));
      fixP->fx_done = 1;
    }
  else
    {
      /* We can't actually support subtracting a symbol.  */
      if (fixP->fx_subsy != (symbolS *) NULL)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("expression too complex"));

      /* This operand-type is scaled.  */
      if (fixP->fx_r_type == BFD_RELOC_CRIS_LAPCQ_OFFSET)
	val /= 2;
      cris_number_to_imm (buf, val, fixP->fx_size, fixP, seg);
    }
}

/* All relocations are relative to the location just after the fixup;
   the address of the fixup plus its size.  */

long
md_pcrel_from (fixS *fixP)
{
  valueT addr = fixP->fx_where + fixP->fx_frag->fr_address;

  /* FIXME:  We get here only at the end of assembly, when X in ".-X" is
     still unknown.  Since we don't have pc-relative relocations in a.out,
     this is invalid.  What to do if anything for a.out, is to add
     pc-relative relocations everywhere including the elinux program
     loader.  For ELF, allow straight-forward PC-relative relocations,
     which are always relative to the location after the relocation.  */
  if (OUTPUT_FLAVOR != bfd_target_elf_flavour
      || (fixP->fx_r_type != BFD_RELOC_8_PCREL
	  && fixP->fx_r_type != BFD_RELOC_16_PCREL
	  && fixP->fx_r_type != BFD_RELOC_32_PCREL
	  && fixP->fx_r_type != BFD_RELOC_CRIS_LAPCQ_OFFSET))
    as_bad_where (fixP->fx_file, fixP->fx_line,
		  _("Invalid pc-relative relocation"));
  return fixP->fx_size + addr;
}

/* We have no need to give defaults for symbol-values.  */
symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return 0;
}

/* If this function returns non-zero, it prevents the relocation
   against symbol(s) in the FIXP from being replaced with relocations
   against section symbols, and guarantees that a relocation will be
   emitted even when the value can be resolved locally.  */
int
md_cris_force_relocation (struct fix *fixp)
{
  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_CRIS_16_GOT:
    case BFD_RELOC_CRIS_32_GOT:
    case BFD_RELOC_CRIS_16_GOTPLT:
    case BFD_RELOC_CRIS_32_GOTPLT:
    case BFD_RELOC_CRIS_32_GOTREL:
    case BFD_RELOC_CRIS_32_PLT_GOTREL:
    case BFD_RELOC_CRIS_32_PLT_PCREL:
      return 1;
    default:
      ;
    }

  return generic_force_reloc (fixp);
}

/* Check and emit error if broken-word handling has failed to fix up a
   case-table.	This is called from write.c, after doing everything it
   knows about how to handle broken words.  */

void
tc_cris_check_adjusted_broken_word (offsetT new_offset, struct broken_word *brokwP)
{
  if (new_offset > 32767 || new_offset < -32768)
    /* We really want a genuine error, not a warning, so make it one.  */
    as_bad_where (brokwP->frag->fr_file, brokwP->frag->fr_line,
		  _("Adjusted signed .word (%ld) overflows: `switch'-statement too large."),
		  (long) new_offset);
}

/* Make a leading REGISTER_PREFIX_CHAR mandatory for all registers.  */

static void
cris_force_reg_prefix (void)
{
  demand_register_prefix = TRUE;
}

/* Do not demand a leading REGISTER_PREFIX_CHAR for all registers.  */

static void
cris_relax_reg_prefix (void)
{
  demand_register_prefix = FALSE;
}

/* Adjust for having a leading '_' on all user symbols.  */

static void
cris_sym_leading_underscore (void)
{
  /* We can't really do anything more than assert that what the program
     thinks symbol starts with agrees with the command-line options, since
     the bfd is already created.  */

  if (!symbols_have_leading_underscore)
    as_bad (_(".syntax %s requires command-line option `--underscore'"),
	    SYNTAX_USER_SYM_LEADING_UNDERSCORE);
}

/* Adjust for not having any particular prefix on user symbols.  */

static void cris_sym_no_leading_underscore (void)
{
  if (symbols_have_leading_underscore)
    as_bad (_(".syntax %s requires command-line option `--no-underscore'"),
	    SYNTAX_USER_SYM_NO_LEADING_UNDERSCORE);
}

/* Handle the .syntax pseudo, which takes an argument that decides what
   syntax the assembly code has.  */

static void
s_syntax (int ignore ATTRIBUTE_UNUSED)
{
  static const struct syntaxes
  {
    const char *const operand;
    void (*fn) (void);
  } syntax_table[] =
    {{SYNTAX_ENFORCE_REG_PREFIX, cris_force_reg_prefix},
     {SYNTAX_RELAX_REG_PREFIX, cris_relax_reg_prefix},
     {SYNTAX_USER_SYM_LEADING_UNDERSCORE, cris_sym_leading_underscore},
     {SYNTAX_USER_SYM_NO_LEADING_UNDERSCORE, cris_sym_no_leading_underscore}};

  const struct syntaxes *sp;

  for (sp = syntax_table;
       sp < syntax_table + sizeof (syntax_table) / sizeof (syntax_table[0]);
       sp++)
    {
      if (strncmp (input_line_pointer, sp->operand,
		   strlen (sp->operand)) == 0)
	{
	  (sp->fn) ();

	  input_line_pointer += strlen (sp->operand);
	  demand_empty_rest_of_line ();
	  return;
	}
    }

  as_bad (_("Unknown .syntax operand"));
}

/* Wrapper for dwarf2_directive_file to emit error if this is seen when
   not emitting ELF.  */

static void
s_cris_file (int dummy)
{
  if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
    as_bad (_("Pseudodirective .file is only valid when generating ELF"));
  else
    dwarf2_directive_file (dummy);
}

/* Wrapper for dwarf2_directive_loc to emit error if this is seen when not
   emitting ELF.  */

static void
s_cris_loc (int dummy)
{
  if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
    as_bad (_("Pseudodirective .loc is only valid when generating ELF"));
  else
    dwarf2_directive_loc (dummy);
}

/* Translate a <arch> string (as common to --march=<arch> and .arch <arch>)
   into an enum.  If the string *STR is recognized, *STR is updated to point
   to the end of the string.  If the string is not recognized,
   arch_cris_unknown is returned.  */

static enum cris_archs
cris_arch_from_string (char **str)
{
  static const struct cris_arch_struct
  {
    const char *const name;
    enum cris_archs arch;
  } arch_table[] =
      /* Keep in order longest-first for choices where one is a prefix
	 of another.  */
    {{"v0_v10", arch_cris_any_v0_v10},
     {"v10", arch_crisv10},
     {"v32", arch_crisv32},
     {"common_v10_v32", arch_cris_common_v10_v32}};

  const struct cris_arch_struct *ap;

  for (ap = arch_table;
       ap < arch_table + sizeof (arch_table) / sizeof (arch_table[0]);
       ap++)
    {
      int len = strlen (ap->name);

      if (strncmp (*str, ap->name, len) == 0
	  && (str[0][len] == 0 || ISSPACE (str[0][len])))
	{
	  *str += strlen (ap->name);
	  return ap->arch;
	}
    }

  return arch_cris_unknown;
}

/* Return nonzero if architecture version ARCH matches version range in
   IVER.  */

static int
cris_insn_ver_valid_for_arch (enum cris_insn_version_usage iver,
			      enum cris_archs arch)
{
  switch (arch)
    {
    case arch_cris_any_v0_v10:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_warning
	 || iver == cris_ver_v0_3
	 || iver == cris_ver_v3p
	 || iver == cris_ver_v0_10
	 || iver == cris_ver_sim_v0_10
	 || iver == cris_ver_v3_10
	 || iver == cris_ver_v8
	 || iver == cris_ver_v8p
	 || iver == cris_ver_v8_10
	 || iver == cris_ver_v10
	 || iver == cris_ver_v10p);
      
    case arch_crisv32:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_v3p
	 || iver == cris_ver_v8p
	 || iver == cris_ver_v10p
	 || iver == cris_ver_v32p);

    case arch_cris_common_v10_v32:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_v3p
	 || iver == cris_ver_v8p
	 || iver == cris_ver_v10p);

    case arch_crisv0:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_v0_3
	 || iver == cris_ver_v0_10
	 || iver == cris_ver_sim_v0_10);

    case arch_crisv3:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_v0_3
	 || iver == cris_ver_v3p
	 || iver == cris_ver_v0_10
	 || iver == cris_ver_sim_v0_10
	 || iver == cris_ver_v3_10);

    case arch_crisv8:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_v3p
	 || iver == cris_ver_v0_10
	 || iver == cris_ver_sim_v0_10
	 || iver == cris_ver_v3_10
	 || iver == cris_ver_v8
	 || iver == cris_ver_v8p
	 || iver == cris_ver_v8_10);

    case arch_crisv10:
      return
	(iver == cris_ver_version_all
	 || iver == cris_ver_v3p
	 || iver == cris_ver_v0_10
	 || iver == cris_ver_sim_v0_10
	 || iver == cris_ver_v3_10
	 || iver == cris_ver_v8p
	 || iver == cris_ver_v8_10
	 || iver == cris_ver_v10
	 || iver == cris_ver_v10p);

    default:
      BAD_CASE (arch);
   }
}

/* Assert that the .arch ARCHCHOICE1 is compatible with the specified or
   default --march=<ARCHCHOICE2> option.  */

static void
s_cris_arch (int dummy ATTRIBUTE_UNUSED)
{
  /* Right now we take the easy route and check for sameness.  It's not
     obvious that allowing e.g. --march=v32 and .arch common_v0_v32
     would be more useful than confusing, implementation-wise and
     user-wise.  */

  char *str = input_line_pointer;
  enum cris_archs arch = cris_arch_from_string (&str);

  if (arch == arch_cris_unknown)
    {
      as_bad (_("unknown operand to .arch"));

      /* For this one, str does not reflect the end of the operand,
	 since there was no matching arch.  Skip it manually; skip
	 things that can be part of a word (a name).  */
      while (is_part_of_name (*str))
	str++;
    }
  else if (arch != cris_arch)
    as_bad (_(".arch <arch> requires a matching --march=... option"));

  input_line_pointer = str;
  demand_empty_rest_of_line ();
  return;
}

/*
 * Local variables:
 * eval: (c-set-style "gnu")
 * indent-tabs-mode: t
 * End:
 */
