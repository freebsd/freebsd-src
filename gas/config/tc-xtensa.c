/* tc-xtensa.c -- Assemble Xtensa instructions.
   Copyright 2003 Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, 
   MA 02111-1307, USA.  */

#include <string.h>
#include "as.h"
#include "sb.h"
#include "safe-ctype.h"
#include "tc-xtensa.h"
#include "frags.h"
#include "subsegs.h"
#include "xtensa-relax.h"
#include "xtensa-istack.h"
#include "dwarf2dbg.h"
#include "struc-symbol.h"
#include "xtensa-config.h"

#ifndef uint32
#define uint32 unsigned int
#endif
#ifndef int32
#define int32 signed int
#endif

/* Notes:

   There are 3 forms for instructions,
   1) the MEMORY format -- this is the encoding 2 or 3 byte instruction
   2) the TInsn -- handles instructions/labels and literals;
      all operands are assumed to be expressions
   3) the IStack -- a stack of TInsn.  this allows us to 
      reason about the generated expansion instructions
  
   Naming conventions (used somewhat inconsistently):
      The xtensa_ functions are exported
      The xg_ functions are internal

   We also have a couple of different extensibility mechanisms.
   1) The idiom replacement:
      This is used when a line is first parsed to
      replace an instruction pattern with another instruction
      It is currently limited to replacements of instructions
      with constant operands.
   2) The xtensa-relax.c mechanism that has stronger instruction
      replacement patterns.  When an instruction's immediate field
      does not fit the next instruction sequence is attempted.
      In addition, "narrow" opcodes are supported this way.  */


/* Define characters with special meanings to GAS.  */
const char comment_chars[] = "#";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = ";";
const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "rRsSfFdDxXpP";


/* Flag to indicate whether the hardware supports the density option.
   If not, enabling density instructions (via directives or --density flag)
   is illegal.  */

#if STATIC_LIBISA
bfd_boolean density_supported = XCHAL_HAVE_DENSITY;
#else
bfd_boolean density_supported = TRUE;
#endif

#define XTENSA_FETCH_WIDTH 4

/* Flags for properties of the last instruction in a segment.  */
#define FLAG_IS_A0_WRITER	0x1
#define FLAG_IS_BAD_LOOPEND	0x2


/* We define a special segment names ".literal" to place literals
   into.  The .fini and .init sections are special because they
   contain code that is moved together by the linker.  We give them
   their own special .fini.literal and .init.literal sections.  */

#define LITERAL_SECTION_NAME		xtensa_section_rename (".literal")
#define FINI_SECTION_NAME		xtensa_section_rename (".fini")
#define INIT_SECTION_NAME		xtensa_section_rename (".init")
#define FINI_LITERAL_SECTION_NAME	xtensa_section_rename (".fini.literal")
#define INIT_LITERAL_SECTION_NAME	xtensa_section_rename (".init.literal")


/* This type is used for the directive_stack to keep track of the 
   state of the literal collection pools.  */

typedef struct lit_state_struct
{
  const char *lit_seg_name;
  const char *init_lit_seg_name;
  const char *fini_lit_seg_name;
  segT lit_seg;
  segT init_lit_seg;
  segT fini_lit_seg;
} lit_state;

static lit_state default_lit_sections;


/* We keep lists of literal segments.  The seg_list type is the node
   for such a list.  The *_literal_head locals are the heads of the
   various lists.  All of these lists have a dummy node at the start.  */

typedef struct seg_list_struct
{
  struct seg_list_struct *next;
  segT seg;
} seg_list;

static seg_list literal_head_h;
static seg_list *literal_head = &literal_head_h;
static seg_list init_literal_head_h;
static seg_list *init_literal_head = &init_literal_head_h;
static seg_list fini_literal_head_h;
static seg_list *fini_literal_head = &fini_literal_head_h;


/* Lists of symbols.  We keep a list of symbols that label the current
   instruction, so that we can adjust the symbols when inserting alignment
   for various instructions.  We also keep a list of all the symbols on
   literals, so that we can fix up those symbols when the literals are
   later moved into the text sections.  */

typedef struct sym_list_struct
{
  struct sym_list_struct *next;
  symbolS *sym;
} sym_list;

static sym_list *insn_labels = NULL;
static sym_list *free_insn_labels = NULL;
static sym_list *saved_insn_labels = NULL;

static sym_list *literal_syms;


/* Global flag to indicate when we are emitting literals.  */
int generating_literals = 0;


/* Structure for saving the current state before emitting literals.  */
typedef struct emit_state_struct
{
  const char *name;
  segT now_seg;
  subsegT now_subseg;
  int generating_literals;
} emit_state;


/* Directives.  */

typedef enum
{
  directive_none = 0,
  directive_literal,
  directive_density,
  directive_generics,
  directive_relax,
  directive_freeregs,
  directive_longcalls,
  directive_literal_prefix
} directiveE;

typedef struct
{
  const char *name;
  bfd_boolean can_be_negated;
} directive_infoS;

const directive_infoS directive_info[] =
{
  {"none",	FALSE},
  {"literal",	FALSE},
  {"density",	TRUE},
  {"generics",	TRUE},
  {"relax",	TRUE},
  {"freeregs",	FALSE},
  {"longcalls",	TRUE},
  {"literal_prefix", FALSE}
};

bfd_boolean directive_state[] =
{
  FALSE,			/* none */
  FALSE,			/* literal */
#if STATIC_LIBISA && !XCHAL_HAVE_DENSITY
  FALSE,			/* density */
#else
  TRUE,				/* density */
#endif
  TRUE,				/* generics */
  TRUE,				/* relax */
  FALSE,			/* freeregs */
  FALSE,			/* longcalls */
  FALSE				/* literal_prefix */
};


enum xtensa_relax_statesE
{
  RELAX_ALIGN_NEXT_OPCODE,
  /* Use the first opcode of the next fragment to determine the
     alignment requirements.  This is ONLY used for LOOPS
     currently.  */

  RELAX_DESIRE_ALIGN_IF_TARGET,
  /* These are placed in front of labels.  They will all be converted
     to RELAX_DESIRE_ALIGN / RELAX_LOOP_END or rs_fill of 0 before
     relaxation begins.  */

  RELAX_ADD_NOP_IF_A0_B_RETW,
  /* These are placed in front of conditional branches.  It will be
     turned into a NOP (using a1) if the branch is immediately
     followed by a RETW or RETW.N.  Otherwise it will be turned into
     an rs_fill of 0 before relaxation begins.  */

  RELAX_ADD_NOP_IF_PRE_LOOP_END,
  /* These are placed after JX instructions.  It will be turned into a
     NOP if there is one instruction before a loop end label.
     Otherwise it will be turned into an rs_fill of 0 before
     relaxation begins.  This is used to avoid a hardware TIE
     interlock issue prior to T1040.  */

  RELAX_ADD_NOP_IF_SHORT_LOOP,
  /* These are placed after LOOP instructions.  It will be turned into
     a NOP when: (1) there are less than 3 instructions in the loop;
     we place 2 of these in a row to add up to 2 NOPS in short loops;
     or (2) The instructions in the loop do not include a branch or
     jump.  Otherwise it will be turned into an rs_fill of 0 before
     relaxation begins.  This is used to avoid hardware bug
     PR3830.  */

  RELAX_ADD_NOP_IF_CLOSE_LOOP_END,
  /* These are placed after LOOP instructions.  It will be turned into
     a NOP if there are less than 12 bytes to the end of some other
     loop's end.  Otherwise it will be turned into an rs_fill of 0
     before relaxation begins.  This is used to avoid hardware bug
     PR3830.  */

  RELAX_DESIRE_ALIGN,
  /* The next fragment like its first instruction to NOT cross a
     4-byte boundary.  */

  RELAX_LOOP_END,
  /* This will be turned into a NOP or NOP.N if the previous
     instruction is expanded to negate a loop.  */

  RELAX_LOOP_END_ADD_NOP,
  /* When the code density option is available, this will generate a
     NOP.N marked RELAX_NARROW.  Otherwise, it will create an rs_fill
     fragment with a NOP in it.  */

  RELAX_LITERAL,
  /* Another fragment could generate an expansion here but has not yet.  */

  RELAX_LITERAL_NR,
  /* Expansion has been generated by an instruction that generates a
     literal.  However, the stretch has NOT been reported yet in this
     fragment.  */

  RELAX_LITERAL_FINAL,
  /* Expansion has been generated by an instruction that generates a
     literal.  */

  RELAX_LITERAL_POOL_BEGIN,
  RELAX_LITERAL_POOL_END,
  /* Technically these are not relaxations at all, but mark a location
     to store literals later.  Note that fr_var stores the frchain for
     BEGIN frags and fr_var stores now_seg for END frags.  */

  RELAX_NARROW,
  /* The last instruction in this fragment (at->fr_opcode) can be
     freely replaced with a single wider instruction if a future
     alignment desires or needs it.  */

  RELAX_IMMED,
  /* The last instruction in this fragment (at->fr_opcode) contains
     the value defined by fr_symbol (fr_offset = 0).  If the value
     does not fit, use the specified expansion.  This is similar to
     "NARROW", except that these may not be expanded in order to align
     code.  */
  
  RELAX_IMMED_STEP1,
  /* The last instruction in this fragment (at->fr_opcode) contains a
     literal.  It has already been expanded at least 1 step.  */

  RELAX_IMMED_STEP2
  /* The last instruction in this fragment (at->fr_opcode) contains a
     literal.  It has already been expanded at least 2 steps.  */
};

/* This is used as a stopper to bound the number of steps that
   can be taken.  */
#define RELAX_IMMED_MAXSTEPS (RELAX_IMMED_STEP2 - RELAX_IMMED)


typedef bfd_boolean (*frag_predicate) (const fragS *);


/* Directive functions.  */

static bfd_boolean use_generics
  PARAMS ((void));
static bfd_boolean use_longcalls
  PARAMS ((void));
static bfd_boolean code_density_available
  PARAMS ((void));
static bfd_boolean can_relax
  PARAMS ((void));
static void directive_push
  PARAMS ((directiveE, bfd_boolean, const void *));
static void directive_pop
  PARAMS ((directiveE *, bfd_boolean *, const char **,
	   unsigned int *, const void **));
static void directive_balance
  PARAMS ((void));
static bfd_boolean inside_directive
  PARAMS ((directiveE));
static void get_directive
  PARAMS ((directiveE *, bfd_boolean *));
static void xtensa_begin_directive
  PARAMS ((int));
static void xtensa_end_directive
  PARAMS ((int));
static void xtensa_literal_prefix
  PARAMS ((char const *, int));
static void xtensa_literal_position
  PARAMS ((int));
static void xtensa_literal_pseudo
  PARAMS ((int));

/* Parsing and Idiom Translation Functions.  */

static const char *expression_end
  PARAMS ((const char *));
static unsigned tc_get_register
  PARAMS ((const char *));
static void expression_maybe_register
  PARAMS ((xtensa_operand, expressionS *));
static int tokenize_arguments
  PARAMS ((char **, char *));
static bfd_boolean parse_arguments
  PARAMS ((TInsn *, int, char **));
static int xg_translate_idioms
  PARAMS ((char **, int *, char **));
static int xg_translate_sysreg_op
  PARAMS ((char **, int *, char **));
static void xg_reverse_shift_count
  PARAMS ((char **));
static int xg_arg_is_constant
  PARAMS ((char *, offsetT *));
static void xg_replace_opname
  PARAMS ((char **, char *));
static int xg_check_num_args
  PARAMS ((int *, int, char *, char **));

/* Functions for dealing with the Xtensa ISA.  */

static bfd_boolean operand_is_immed
  PARAMS ((xtensa_operand));
static bfd_boolean operand_is_pcrel_label
  PARAMS ((xtensa_operand));
static int get_relaxable_immed
  PARAMS ((xtensa_opcode));
static xtensa_opcode get_opcode_from_buf
  PARAMS ((const char *));
static bfd_boolean is_direct_call_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_call_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_entry_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_loop_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_the_loop_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_jx_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_windowed_return_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_conditional_branch_opcode
  PARAMS ((xtensa_opcode));
static bfd_boolean is_branch_or_jump_opcode
  PARAMS ((xtensa_opcode));
static bfd_reloc_code_real_type opnum_to_reloc
  PARAMS ((int));
static int reloc_to_opnum
  PARAMS ((bfd_reloc_code_real_type));
static void xtensa_insnbuf_set_operand
  PARAMS ((xtensa_insnbuf, xtensa_opcode, xtensa_operand, int32,
	   const char *, unsigned int));
static uint32 xtensa_insnbuf_get_operand
  PARAMS ((xtensa_insnbuf, xtensa_opcode, int));
static void xtensa_insnbuf_set_immediate_field
  PARAMS ((xtensa_opcode, xtensa_insnbuf, int32, const char *,
	   unsigned int));
static bfd_boolean is_negatable_branch
  PARAMS ((TInsn *));

/* Various Other Internal Functions.  */

static bfd_boolean is_unique_insn_expansion
  PARAMS ((TransitionRule *));
static int xg_get_insn_size
  PARAMS ((TInsn *));
static int xg_get_build_instr_size
  PARAMS ((BuildInstr *));
static bfd_boolean xg_is_narrow_insn
  PARAMS ((TInsn *));
static bfd_boolean xg_is_single_relaxable_insn
  PARAMS ((TInsn *));
static int xg_get_max_narrow_insn_size
  PARAMS ((xtensa_opcode));
static int xg_get_max_insn_widen_size
  PARAMS ((xtensa_opcode));
static int xg_get_max_insn_widen_literal_size
  PARAMS ((xtensa_opcode));
static bfd_boolean xg_is_relaxable_insn
  PARAMS ((TInsn *, int));
static symbolS *get_special_literal_symbol
  PARAMS ((void));
static symbolS *get_special_label_symbol
  PARAMS ((void));
static bfd_boolean xg_build_to_insn
  PARAMS ((TInsn *, TInsn *, BuildInstr *));
static bfd_boolean xg_build_to_stack
  PARAMS ((IStack *, TInsn *, BuildInstr *));
static bfd_boolean xg_expand_to_stack
  PARAMS ((IStack *, TInsn *, int));
static bfd_boolean xg_expand_narrow
  PARAMS ((TInsn *, TInsn *));
static bfd_boolean xg_immeds_fit
  PARAMS ((const TInsn *));
static bfd_boolean xg_symbolic_immeds_fit
  PARAMS ((const TInsn *, segT, fragS *, offsetT, long));
static bfd_boolean xg_check_operand
  PARAMS ((int32, xtensa_operand));
static int is_dnrange
  PARAMS ((fragS *, symbolS *, long));
static int xg_assembly_relax
  PARAMS ((IStack *, TInsn *, segT, fragS *, offsetT, int, long));
static void xg_force_frag_space
  PARAMS ((int));
static void xg_finish_frag
  PARAMS ((char *, enum xtensa_relax_statesE, int, bfd_boolean));
static bfd_boolean is_branch_jmp_to_next
  PARAMS ((TInsn *, fragS *));
static void xg_add_branch_and_loop_targets
  PARAMS ((TInsn *));
static bfd_boolean xg_instruction_matches_rule
  PARAMS ((TInsn *, TransitionRule *));
static TransitionRule *xg_instruction_match
  PARAMS ((TInsn *));
static bfd_boolean xg_build_token_insn
  PARAMS ((BuildInstr *, TInsn *, TInsn *));
static bfd_boolean xg_simplify_insn
  PARAMS ((TInsn *, TInsn *));
static bfd_boolean xg_expand_assembly_insn
  PARAMS ((IStack *, TInsn *));
static symbolS *xg_assemble_literal
  PARAMS ((TInsn *));
static void xg_assemble_literal_space
  PARAMS ((int));
static symbolS *xtensa_create_literal_symbol
  PARAMS ((segT, fragS *));
static void xtensa_add_literal_sym
  PARAMS ((symbolS *));
static void xtensa_add_insn_label
  PARAMS ((symbolS *));
static void xtensa_clear_insn_labels
  PARAMS ((void));
static bfd_boolean get_is_linkonce_section
  PARAMS ((bfd *, segT));
static bfd_boolean xg_emit_insn
  PARAMS ((TInsn *, bfd_boolean));
static bfd_boolean xg_emit_insn_to_buf
  PARAMS ((TInsn *, char *, fragS *, offsetT, bfd_boolean));
static bfd_boolean xg_add_opcode_fix
  PARAMS ((xtensa_opcode, int, expressionS *, fragS *, offsetT));
static void xg_resolve_literals
  PARAMS ((TInsn *, symbolS *));
static void xg_resolve_labels
  PARAMS ((TInsn *, symbolS *));
static void xg_assemble_tokens
  PARAMS ((TInsn *));
static bfd_boolean is_register_writer
  PARAMS ((const TInsn *, const char *, int));
static bfd_boolean is_bad_loopend_opcode
  PARAMS ((const TInsn *));
static bfd_boolean is_unaligned_label
  PARAMS ((symbolS *));
static fragS *next_non_empty_frag
  PARAMS ((const fragS *));
static xtensa_opcode next_frag_opcode
  PARAMS ((const fragS *));
static void update_next_frag_nop_state
  PARAMS ((fragS *));
static bfd_boolean next_frag_is_branch_target
  PARAMS ((const fragS *));
static bfd_boolean next_frag_is_loop_target
  PARAMS ((const fragS *));
static addressT next_frag_pre_opcode_bytes
  PARAMS ((const fragS *));
static bfd_boolean is_next_frag_target
  PARAMS ((const fragS *, const fragS *));
static void xtensa_mark_literal_pool_location
  PARAMS ((void));
static void xtensa_move_labels
  PARAMS ((fragS *, valueT, bfd_boolean));
static void assemble_nop
  PARAMS ((size_t, char *));
static addressT get_expanded_loop_offset
  PARAMS ((xtensa_opcode));
static fragS *get_literal_pool_location
  PARAMS ((segT));
static void set_literal_pool_location
  PARAMS ((segT, fragS *));

/* Helpers for xtensa_end().  */

static void xtensa_cleanup_align_frags
  PARAMS ((void));
static void xtensa_fix_target_frags
  PARAMS ((void));
static bfd_boolean frag_can_negate_branch
  PARAMS ((fragS *));
static void xtensa_fix_a0_b_retw_frags
  PARAMS ((void));
static bfd_boolean next_instrs_are_b_retw
  PARAMS ((fragS *));
static void xtensa_fix_b_j_loop_end_frags
  PARAMS ((void));
static bfd_boolean next_instr_is_loop_end
  PARAMS ((fragS *));
static void xtensa_fix_close_loop_end_frags
  PARAMS ((void));
static size_t min_bytes_to_other_loop_end
  PARAMS ((fragS *, fragS *, offsetT, size_t));
static size_t unrelaxed_frag_min_size
  PARAMS ((fragS *));
static void xtensa_fix_short_loop_frags
  PARAMS ((void));
static size_t count_insns_to_loop_end
  PARAMS ((fragS *, bfd_boolean, size_t));
static size_t unrelaxed_frag_min_insn_count
  PARAMS ((fragS *));
static bfd_boolean branch_before_loop_end
  PARAMS ((fragS *));
static bfd_boolean unrelaxed_frag_has_b_j
  PARAMS ((fragS *));
static void xtensa_sanity_check
  PARAMS ((void));
static bfd_boolean is_empty_loop
  PARAMS ((const TInsn *, fragS *));
static bfd_boolean is_local_forward_loop
  PARAMS ((const TInsn *, fragS *));

/* Alignment Functions.  */

static size_t get_text_align_power
  PARAMS ((int));
static addressT get_text_align_max_fill_size
  PARAMS ((int, bfd_boolean, bfd_boolean));
static addressT get_text_align_fill_size
  PARAMS ((addressT, int, int, bfd_boolean, bfd_boolean));
static size_t get_text_align_nop_count
  PARAMS ((size_t, bfd_boolean));
static size_t get_text_align_nth_nop_size
  PARAMS ((size_t, size_t, bfd_boolean));
static addressT get_noop_aligned_address
  PARAMS ((fragS *, addressT));
static addressT get_widen_aligned_address
  PARAMS ((fragS *, addressT));

/* Helpers for xtensa_relax_frag().  */

static long relax_frag_text_align
  PARAMS ((fragS *, long));
static long relax_frag_add_nop
  PARAMS ((fragS *));
static long relax_frag_narrow
  PARAMS ((fragS *, long));
static bfd_boolean future_alignment_required
  PARAMS ((fragS *, long));
static long relax_frag_immed
  PARAMS ((segT, fragS *, long, int, int *));

/* Helpers for md_convert_frag().  */

static void convert_frag_align_next_opcode
  PARAMS ((fragS *));
static void convert_frag_narrow
  PARAMS ((fragS *));
static void convert_frag_immed
  PARAMS ((segT, fragS *, int));
static fixS *fix_new_exp_in_seg
  PARAMS ((segT, subsegT, fragS *, int, int, expressionS *, int,
	   bfd_reloc_code_real_type));
static void convert_frag_immed_finish_loop
  PARAMS ((segT, fragS *, TInsn *));
static offsetT get_expression_value
  PARAMS ((segT, expressionS *));

/* Flags for the Last Instruction in Each Subsegment.  */

static unsigned get_last_insn_flags
  PARAMS ((segT, subsegT));
static void set_last_insn_flags
  PARAMS ((segT, subsegT, unsigned, bfd_boolean));

/* Segment list functions.  */

static void xtensa_remove_section
  PARAMS ((segT));
static void xtensa_insert_section
  PARAMS ((segT, segT));
static void xtensa_move_seg_list_to_beginning
  PARAMS ((seg_list *));
static void xtensa_move_literals
  PARAMS ((void));
static void mark_literal_frags
  PARAMS ((seg_list *));
static void xtensa_reorder_seg_list
  PARAMS ((seg_list *, segT));
static void xtensa_reorder_segments
  PARAMS ((void));
static segT get_last_sec
  PARAMS ((void));
static void xtensa_switch_to_literal_fragment
  PARAMS ((emit_state *));
static void xtensa_switch_section_emit_state
  PARAMS ((emit_state *, segT, subsegT));
static void xtensa_restore_emit_state
  PARAMS ((emit_state *));
static void cache_literal_section
  PARAMS ((seg_list *, const char *, segT *));
static segT retrieve_literal_seg
  PARAMS ((seg_list *, const char *));
static segT seg_present
  PARAMS ((const char *));
static void add_seg_list
  PARAMS ((seg_list *, segT));

/* Property Table (e.g., ".xt.insn" and ".xt.lit") Functions.  */

static void xtensa_create_property_segments
  PARAMS ((frag_predicate, const char *, xt_section_type));
static segment_info_type *retrieve_segment_info
  PARAMS ((segT));
static segT retrieve_xtensa_section
  PARAMS ((char *));
static bfd_boolean section_has_property
  PARAMS ((segT sec, frag_predicate));
static void add_xt_block_frags
  PARAMS ((segT, segT, xtensa_block_info **, frag_predicate));
static bfd_boolean get_frag_is_literal
  PARAMS ((const fragS *));
static bfd_boolean get_frag_is_insn
  PARAMS ((const fragS *));

/* Import from elf32-xtensa.c in BFD library.  */
extern char *xtensa_get_property_section_name
  PARAMS ((asection *, const char *));

/* TInsn and IStack functions.  */
static bfd_boolean tinsn_has_symbolic_operands
  PARAMS ((const TInsn *));
static bfd_boolean tinsn_has_invalid_symbolic_operands
  PARAMS ((const TInsn *));
static bfd_boolean tinsn_has_complex_operands
  PARAMS ((const TInsn *));
static bfd_boolean tinsn_to_insnbuf
  PARAMS ((TInsn *, xtensa_insnbuf));
static bfd_boolean tinsn_check_arguments
  PARAMS ((const TInsn *));
static void tinsn_from_chars
  PARAMS ((TInsn *, char *));
static void tinsn_immed_from_frag
  PARAMS ((TInsn *, fragS *));
static int get_num_stack_text_bytes
  PARAMS ((IStack *));
static int get_num_stack_literal_bytes
  PARAMS ((IStack *));

/* Expression Utilities.  */
bfd_boolean expr_is_const
  PARAMS ((const expressionS *));
offsetT get_expr_const
  PARAMS ((const expressionS *));
void set_expr_const
  PARAMS ((expressionS *, offsetT));
void set_expr_symbol_offset
  PARAMS ((expressionS *, symbolS *, offsetT));
bfd_boolean expr_is_equal
  PARAMS ((expressionS *, expressionS *));
static void copy_expr
  PARAMS ((expressionS *, const expressionS *));

#ifdef XTENSA_SECTION_RENAME
static void build_section_rename
  PARAMS ((const char *));
static void add_section_rename
  PARAMS ((char *, char *));
#endif


/* ISA imported from bfd.  */
extern xtensa_isa xtensa_default_isa;

extern int target_big_endian;

static xtensa_opcode xtensa_addi_opcode;
static xtensa_opcode xtensa_addmi_opcode;
static xtensa_opcode xtensa_call0_opcode;
static xtensa_opcode xtensa_call4_opcode;
static xtensa_opcode xtensa_call8_opcode;
static xtensa_opcode xtensa_call12_opcode;
static xtensa_opcode xtensa_callx0_opcode;
static xtensa_opcode xtensa_callx4_opcode;
static xtensa_opcode xtensa_callx8_opcode;
static xtensa_opcode xtensa_callx12_opcode;
static xtensa_opcode xtensa_entry_opcode;
static xtensa_opcode xtensa_isync_opcode;
static xtensa_opcode xtensa_j_opcode;
static xtensa_opcode xtensa_jx_opcode;
static xtensa_opcode xtensa_loop_opcode;
static xtensa_opcode xtensa_loopnez_opcode;
static xtensa_opcode xtensa_loopgtz_opcode;
static xtensa_opcode xtensa_nop_n_opcode;
static xtensa_opcode xtensa_or_opcode;
static xtensa_opcode xtensa_ret_opcode;
static xtensa_opcode xtensa_ret_n_opcode;
static xtensa_opcode xtensa_retw_opcode;
static xtensa_opcode xtensa_retw_n_opcode;
static xtensa_opcode xtensa_rsr_opcode;
static xtensa_opcode xtensa_waiti_opcode;


/* Command-line Options.  */

bfd_boolean use_literal_section = TRUE;
static bfd_boolean align_targets = TRUE;
static bfd_boolean align_only_targets = FALSE;
static bfd_boolean software_a0_b_retw_interlock = TRUE;
static bfd_boolean has_a0_b_retw = FALSE;
static bfd_boolean workaround_a0_b_retw = TRUE;

static bfd_boolean software_avoid_b_j_loop_end = TRUE;
static bfd_boolean workaround_b_j_loop_end = TRUE;
static bfd_boolean maybe_has_b_j_loop_end = FALSE;

static bfd_boolean software_avoid_short_loop = TRUE;
static bfd_boolean workaround_short_loop = TRUE;
static bfd_boolean maybe_has_short_loop = FALSE;

static bfd_boolean software_avoid_close_loop_end = TRUE;
static bfd_boolean workaround_close_loop_end = TRUE;
static bfd_boolean maybe_has_close_loop_end = FALSE;

/* When avoid_short_loops is true, all loops with early exits must
   have at least 3 instructions.  avoid_all_short_loops is a modifier
   to the avoid_short_loop flag.  In addition to the avoid_short_loop
   actions, all straightline loopgtz and loopnez must have at least 3
   instructions.  */

static bfd_boolean software_avoid_all_short_loops = TRUE;
static bfd_boolean workaround_all_short_loops = TRUE;

/* This is on a per-instruction basis.  */
static bfd_boolean specific_opcode = FALSE;

enum
{
  option_density = OPTION_MD_BASE,
  option_no_density,

  option_relax,
  option_no_relax,

  option_generics,
  option_no_generics,

  option_text_section_literals,
  option_no_text_section_literals,

  option_align_targets,
  option_no_align_targets,

  option_align_only_targets,
  option_no_align_only_targets,

  option_longcalls,
  option_no_longcalls,

  option_workaround_a0_b_retw,
  option_no_workaround_a0_b_retw,

  option_workaround_b_j_loop_end,
  option_no_workaround_b_j_loop_end,

  option_workaround_short_loop,
  option_no_workaround_short_loop,

  option_workaround_all_short_loops,
  option_no_workaround_all_short_loops,

  option_workaround_close_loop_end,
  option_no_workaround_close_loop_end,

  option_no_workarounds,

#ifdef XTENSA_SECTION_RENAME
  option_literal_section_name,
  option_text_section_name,
  option_data_section_name,
  option_bss_section_name,
  option_rename_section_name,
#endif

  option_eb,
  option_el
};

const char *md_shortopts = "";

struct option md_longopts[] =
{
  {"density", no_argument, NULL, option_density},
  {"no-density", no_argument, NULL, option_no_density},
  /* At least as early as alameda, --[no-]relax didn't work as
     documented, so as of albany, --[no-]relax is equivalent to
     --[no-]generics.  Both of these will be deprecated in
     BearValley.  */
  {"relax", no_argument, NULL, option_generics},
  {"no-relax", no_argument, NULL, option_no_generics},
  {"generics", no_argument, NULL, option_generics},
  {"no-generics", no_argument, NULL, option_no_generics},
  {"text-section-literals", no_argument, NULL, option_text_section_literals},
  {"no-text-section-literals", no_argument, NULL,
   option_no_text_section_literals},
  /* This option was changed from -align-target to -target-align
     because it conflicted with the "-al" option.  */
  {"target-align", no_argument, NULL, option_align_targets},
  {"no-target-align", no_argument, NULL,
   option_no_align_targets},
#if 0
  /* This option  should do a better job aligning targets because
     it will only attempt to align targets that are the target of a 
     branch.  */
   { "target-align-only", no_argument, NULL, option_align_only_targets },
   { "no-target-align-only", no_argument, NULL, option_no_align_only_targets },
#endif /* 0 */
  {"longcalls", no_argument, NULL, option_longcalls},
  {"no-longcalls", no_argument, NULL, option_no_longcalls},

  {"no-workaround-a0-b-retw", no_argument, NULL,
   option_no_workaround_a0_b_retw},
  {"workaround-a0-b-retw", no_argument, NULL, option_workaround_a0_b_retw},
  
  {"no-workaround-b-j-loop-end", no_argument, NULL,
   option_no_workaround_b_j_loop_end},
  {"workaround-b-j-loop-end", no_argument, NULL,
   option_workaround_b_j_loop_end},
  
  {"no-workaround-short-loops", no_argument, NULL,
   option_no_workaround_short_loop},
  {"workaround-short-loops", no_argument, NULL, option_workaround_short_loop},

  {"no-workaround-all-short-loops", no_argument, NULL,
   option_no_workaround_all_short_loops},
  {"workaround-all-short-loop", no_argument, NULL,
   option_workaround_all_short_loops},

  {"no-workaround-close-loop-end", no_argument, NULL,
   option_no_workaround_close_loop_end},
  {"workaround-close-loop-end", no_argument, NULL,
   option_workaround_close_loop_end},

  {"no-workarounds", no_argument, NULL, option_no_workarounds},

#ifdef XTENSA_SECTION_RENAME
  {"literal-section-name", required_argument, NULL,
   option_literal_section_name},
  {"text-section-name", required_argument, NULL,
   option_text_section_name},
  {"data-section-name", required_argument, NULL,
   option_data_section_name},
  {"rename-section", required_argument, NULL,
   option_rename_section_name},
  {"bss-section-name", required_argument, NULL,
   option_bss_section_name},
#endif /* XTENSA_SECTION_RENAME */

  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof md_longopts;


int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case option_density:
      if (!density_supported)
	{
	  as_bad (_("'--density' option not supported in this Xtensa "
		  "configuration"));
	  return 0;
	}
      directive_state[directive_density] = TRUE;
      return 1;
    case option_no_density:
      directive_state[directive_density] = FALSE;
      return 1;
    case option_generics:
      directive_state[directive_generics] = TRUE;
      return 1;
    case option_no_generics:
      directive_state[directive_generics] = FALSE;
      return 1;
    case option_longcalls:
      directive_state[directive_longcalls] = TRUE;
      return 1;
    case option_no_longcalls:
      directive_state[directive_longcalls] = FALSE;
      return 1;
    case option_text_section_literals:
      use_literal_section = FALSE;
      return 1;
    case option_no_text_section_literals:
      use_literal_section = TRUE;
      return 1;
    case option_workaround_a0_b_retw:
      workaround_a0_b_retw = TRUE;
      software_a0_b_retw_interlock = TRUE;
      return 1;
    case option_no_workaround_a0_b_retw:
      workaround_a0_b_retw = FALSE;
      software_a0_b_retw_interlock = FALSE;
      return 1;
    case option_workaround_b_j_loop_end:
      workaround_b_j_loop_end = TRUE;
      software_avoid_b_j_loop_end = TRUE;
      return 1;
    case option_no_workaround_b_j_loop_end:
      workaround_b_j_loop_end = FALSE;
      software_avoid_b_j_loop_end = FALSE;
      return 1;

    case option_workaround_short_loop:
      workaround_short_loop = TRUE;
      software_avoid_short_loop = TRUE;
      return 1;
    case option_no_workaround_short_loop:
      workaround_short_loop = FALSE;
      software_avoid_short_loop = FALSE;
      return 1;

    case option_workaround_all_short_loops:
      workaround_all_short_loops = TRUE;
      software_avoid_all_short_loops = TRUE;
      return 1;
    case option_no_workaround_all_short_loops:
      workaround_all_short_loops = FALSE;
      software_avoid_all_short_loops = FALSE;
      return 1;

    case option_workaround_close_loop_end:
      workaround_close_loop_end = TRUE;
      software_avoid_close_loop_end = TRUE;
      return 1;
    case option_no_workaround_close_loop_end:
      workaround_close_loop_end = FALSE;
      software_avoid_close_loop_end = FALSE;
      return 1;

    case option_no_workarounds:
      workaround_a0_b_retw = FALSE;
      software_a0_b_retw_interlock = FALSE;
      workaround_b_j_loop_end = FALSE;
      software_avoid_b_j_loop_end = FALSE;
      workaround_short_loop = FALSE;
      software_avoid_short_loop = FALSE;
      workaround_all_short_loops = FALSE;
      software_avoid_all_short_loops = FALSE;
      workaround_close_loop_end = FALSE;
      software_avoid_close_loop_end = FALSE;
      return 1;
      
    case option_align_targets:
      align_targets = TRUE;
      return 1;
    case option_no_align_targets:
      align_targets = FALSE;
      return 1;

    case option_align_only_targets:
      align_only_targets = TRUE;
      return 1;
    case option_no_align_only_targets:
      align_only_targets = FALSE;
      return 1;

#ifdef XTENSA_SECTION_RENAME
    case option_literal_section_name:
      add_section_rename (".literal", arg);
      as_warn (_("'--literal-section-name' is deprecated; "
		 "use '--rename-section .literal=NEWNAME'"));
      return 1;

    case option_text_section_name:
      add_section_rename (".text", arg);
      as_warn (_("'--text-section-name' is deprecated; "
		 "use '--rename-section .text=NEWNAME'"));
      return 1;

    case option_data_section_name:
      add_section_rename (".data", arg);
      as_warn (_("'--data-section-name' is deprecated; "
		 "use '--rename-section .data=NEWNAME'"));
      return 1;

    case option_bss_section_name:
      add_section_rename (".bss", arg);
      as_warn (_("'--bss-section-name' is deprecated; "
		 "use '--rename-section .bss=NEWNAME'"));
      return 1;

    case option_rename_section_name:
      build_section_rename (arg);
      return 1;
#endif /* XTENSA_SECTION_RENAME */

    case 'Q':
      /* -Qy, -Qn: SVR4 arguments controlling whether a .comment section
         should be emitted or not.  FIXME: Not implemented.  */
      return 1;
      
    default:
      return 0;
    }
}


void
md_show_usage (stream)
     FILE *stream;
{
  fputs ("\nXtensa options:\n"
	 "--[no-]density          [Do not] emit density instructions\n"
	 "--[no-]relax            [Do not] perform branch relaxation\n"
	 "--[no-]generics         [Do not] transform instructions\n"
	 "--[no-]longcalls        [Do not] emit 32-bit call sequences\n"
	 "--[no-]target-align     [Do not] try to align branch targets\n"
	 "--[no-]text-section-literals\n"
	 "                        [Do not] put literals in the text section\n"
	 "--no-workarounds        Do not use any Xtensa workarounds\n"
#ifdef XTENSA_SECTION_RENAME
	 "--rename-section old=new(:old1=new1)*\n"
	 "                        Rename section 'old' to 'new'\n"
	 "\nThe following Xtensa options are deprecated\n"
	 "--literal-section-name  Name of literal section (default .literal)\n"
	 "--text-section-name     Name of text section (default .text)\n"
	 "--data-section-name     Name of data section (default .data)\n"
	 "--bss-section-name      Name of bss section (default .bss)\n"
#endif
	 , stream);
}


/* Directive data and functions.  */

typedef struct state_stackS_struct
{
  directiveE directive;
  bfd_boolean negated;
  bfd_boolean old_state;
  const char *file;
  unsigned int line;
  const void *datum;
  struct state_stackS_struct *prev;
} state_stackS;

state_stackS *directive_state_stack;

const pseudo_typeS md_pseudo_table[] =
{
  {"align", s_align_bytes, 0},	/* Defaulting is invalid (0) */
  {"literal_position", xtensa_literal_position, 0},
  {"frame", s_ignore, 0},	/* formerly used for STABS debugging */
  {"word", cons, 4},
  {"begin", xtensa_begin_directive, 0},
  {"end", xtensa_end_directive, 0},
  {"literal", xtensa_literal_pseudo, 0},
  {NULL, 0, 0},
};


bfd_boolean
use_generics ()
{
  return directive_state[directive_generics];
}


bfd_boolean
use_longcalls ()
{
  return directive_state[directive_longcalls];
}


bfd_boolean
code_density_available ()
{
  return directive_state[directive_density];
}


bfd_boolean
can_relax ()
{
  return use_generics ();
}


static void
directive_push (directive, negated, datum)
     directiveE directive;
     bfd_boolean negated;
     const void *datum;
{
  char *file;
  unsigned int line;
  state_stackS *stack = (state_stackS *) xmalloc (sizeof (state_stackS));

  as_where (&file, &line);

  stack->directive = directive;
  stack->negated = negated;
  stack->old_state = directive_state[directive];
  stack->file = file;
  stack->line = line;
  stack->datum = datum;
  stack->prev = directive_state_stack;
  directive_state_stack = stack;

  directive_state[directive] = !negated;
}

static void
directive_pop (directive, negated, file, line, datum)
     directiveE *directive;
     bfd_boolean *negated;
     const char **file;
     unsigned int *line;
     const void **datum;
{
  state_stackS *top = directive_state_stack;

  if (!directive_state_stack)
    {
      as_bad (_("unmatched end directive"));
      *directive = directive_none;
      return;
    }

  directive_state[directive_state_stack->directive] = top->old_state;
  *directive = top->directive;
  *negated = top->negated;
  *file = top->file;
  *line = top->line;
  *datum = top->datum;
  directive_state_stack = top->prev;
  free (top);
}


static void
directive_balance ()
{
  while (directive_state_stack)
    {
      directiveE directive;
      bfd_boolean negated;
      const char *file;
      unsigned int line;
      const void *datum;

      directive_pop (&directive, &negated, &file, &line, &datum);
      as_warn_where ((char *) file, line,
		     _(".begin directive with no matching .end directive"));
    }
}


static bfd_boolean
inside_directive (dir)
     directiveE dir;
{
  state_stackS *top = directive_state_stack;

  while (top && top->directive != dir)
    top = top->prev;

  return (top != NULL);
}


static void
get_directive (directive, negated)
     directiveE *directive;
     bfd_boolean *negated;
{
  int len;
  unsigned i;

  if (strncmp (input_line_pointer, "no-", 3) != 0)
    *negated = FALSE;
  else
    {
      *negated = TRUE;
      input_line_pointer += 3;
    }

  len = strspn (input_line_pointer,
		"abcdefghijklmnopqrstuvwxyz_/0123456789.");

  for (i = 0; i < sizeof (directive_info) / sizeof (*directive_info); ++i)
    {
      if (strncmp (input_line_pointer, directive_info[i].name, len) == 0)
	{
	  input_line_pointer += len;
	  *directive = (directiveE) i;
	  if (*negated && !directive_info[i].can_be_negated)
	    as_bad (_("directive %s can't be negated"),
		    directive_info[i].name);
	  return;
	}
    }

  as_bad (_("unknown directive"));
  *directive = (directiveE) XTENSA_UNDEFINED;
}


static void
xtensa_begin_directive (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  directiveE directive;
  bfd_boolean negated;
  emit_state *state;
  int len;
  lit_state *ls;

  md_flush_pending_output ();

  get_directive (&directive, &negated);
  if (directive == (directiveE) XTENSA_UNDEFINED)
    {
      discard_rest_of_line ();
      return;
    }

  switch (directive)
    {
    case directive_literal:
      if (!inside_directive (directive_literal))
	{
	  /* Previous labels go with whatever follows this directive, not with
	     the literal, so save them now.  */
	  saved_insn_labels = insn_labels;
	  insn_labels = NULL;
	}
      state = (emit_state *) xmalloc (sizeof (emit_state));
      xtensa_switch_to_literal_fragment (state);
      directive_push (directive_literal, negated, state);
      break;

    case directive_literal_prefix:
      /* Check to see if the current fragment is a literal
	 fragment.  If it is, then this operation is not allowed.  */
      if (frag_now->tc_frag_data.is_literal)
	{
	  as_bad (_("cannot set literal_prefix inside literal fragment"));
	  return;
	}

      /* Allocate the literal state for this section and push
	 onto the directive stack.  */
      ls = xmalloc (sizeof (lit_state));
      assert (ls);

      *ls = default_lit_sections;

      directive_push (directive_literal_prefix, negated, ls);

      /* Parse the new prefix from the input_line_pointer.  */
      SKIP_WHITESPACE ();
      len = strspn (input_line_pointer,
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		    "abcdefghijklmnopqrstuvwxyz_/0123456789.$");

      /* Process the new prefix.  */
      xtensa_literal_prefix (input_line_pointer, len);

      /* Skip the name in the input line.  */
      input_line_pointer += len;
      break;

    case directive_freeregs:
      /* This information is currently unused, but we'll accept the statement
         and just discard the rest of the line.  This won't check the syntax,
         but it will accept every correct freeregs directive.  */
      input_line_pointer += strcspn (input_line_pointer, "\n");
      directive_push (directive_freeregs, negated, 0);
      break;

    case directive_density:
      if (!density_supported && !negated)
	{
	  as_warn (_("Xtensa density option not supported; ignored"));
	  break;
	}
      /* fall through */

    default:
      directive_push (directive, negated, 0);
      break;
    }

  demand_empty_rest_of_line ();
}


static void
xtensa_end_directive (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  directiveE begin_directive, end_directive;
  bfd_boolean begin_negated, end_negated;
  const char *file;
  unsigned int line;
  emit_state *state;
  lit_state *s;

  md_flush_pending_output ();

  get_directive (&end_directive, &end_negated);
  if (end_directive == (directiveE) XTENSA_UNDEFINED)
    {
      discard_rest_of_line ();
      return;
    }

  if (end_directive == directive_density && !density_supported && !end_negated)
    {
      as_warn (_("Xtensa density option not supported; ignored"));
      demand_empty_rest_of_line ();
      return;
    }

  directive_pop (&begin_directive, &begin_negated, &file, &line,
		 (const void **) &state);

  if (begin_directive != directive_none)
    {
      if (begin_directive != end_directive || begin_negated != end_negated)
	{
	  as_bad (_("does not match begin %s%s at %s:%d"),
		  begin_negated ? "no-" : "",
		  directive_info[begin_directive].name, file, line);
	}
      else
	{
	  switch (end_directive)
	    {
	    case directive_literal:
	      frag_var (rs_fill, 0, 0, 0, NULL, 0, NULL);
	      xtensa_restore_emit_state (state);
	      free (state);
	      if (!inside_directive (directive_literal))
		{
		  /* Restore the list of current labels.  */
		  xtensa_clear_insn_labels ();
		  insn_labels = saved_insn_labels;
		}
	      break;

	    case directive_freeregs:
	      break;

	    case directive_literal_prefix:
	      /* Restore the default collection sections from saved state.  */
	      s = (lit_state *) state;
	      assert (s);

	      if (use_literal_section)
		default_lit_sections = *s;

	      /* free the state storage */
	      free (s);
	      break;

	    default:
	      break;
	    }
	}
    }

  demand_empty_rest_of_line ();
}


/* Place an aligned literal fragment at the current location.  */

static void
xtensa_literal_position (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  if (inside_directive (directive_literal))
    as_warn (_(".literal_position inside literal directive; ignoring"));
  else if (!use_literal_section)
    xtensa_mark_literal_pool_location ();

  demand_empty_rest_of_line ();
  xtensa_clear_insn_labels ();
}


/* Support .literal label, value@plt + offset.  */

static void
xtensa_literal_pseudo (ignored)
     int ignored ATTRIBUTE_UNUSED;
{
  emit_state state;
  char *p, *base_name;
  char c;
  expressionS expP;
  segT dest_seg;

  if (inside_directive (directive_literal))
    {
      as_bad (_(".literal not allowed inside .begin literal region"));
      ignore_rest_of_line ();
      return;
    }

  /* Previous labels go with whatever follows this directive, not with
     the literal, so save them now.  */
  saved_insn_labels = insn_labels;
  insn_labels = NULL;

  /* If we are using text-section literals, then this is the right value... */
  dest_seg = now_seg;

  base_name = input_line_pointer;

  xtensa_switch_to_literal_fragment (&state);

  /* ...but if we aren't using text-section-literals, then we 
     need to put them in the section we just switched to.  */
  if (use_literal_section)
    dest_seg = now_seg;

  /* All literals are aligned to four-byte boundaries
     which is handled by switch to literal fragment.  */
  /* frag_align (2, 0, 0);  */

  c = get_symbol_end ();
  /* Just after name is now '\0'.  */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();

  if (*input_line_pointer != ',' && *input_line_pointer != ':')
    {
      as_bad (_("expected comma or colon after symbol name; "
		"rest of line ignored"));
      ignore_rest_of_line ();
      xtensa_restore_emit_state (&state);
      return;
    }
  *p = 0;

  colon (base_name);

  do 
    {
      input_line_pointer++;		/* skip ',' or ':' */
      
      expr (0, &expP);

      /* We only support 4-byte literals with .literal.  */
      emit_expr (&expP, 4);
    }
  while (*input_line_pointer == ',');

  *p = c;

  demand_empty_rest_of_line ();

  xtensa_restore_emit_state (&state);

  /* Restore the list of current labels.  */
  xtensa_clear_insn_labels ();
  insn_labels = saved_insn_labels;
}


static void
xtensa_literal_prefix (start, len)
     char const *start;
     int len;
{
  segT s_now;			/* Storage for the current seg and subseg.  */
  subsegT ss_now;
  char *name;			/* Pointer to the name itself.  */
  char *newname;

  if (!use_literal_section)
    return;

  /* Store away the current section and subsection.  */
  s_now = now_seg;
  ss_now = now_subseg;

  /* Get a null-terminated copy of the name.  */
  name = xmalloc (len + 1);
  assert (name);

  strncpy (name, start, len);
  name[len] = 0;

  /* Allocate the sections (interesting note: the memory pointing to
     the name is actually used for the name by the new section). */
  newname = xmalloc (len + strlen (".literal") + 1);
  strcpy (newname, name);
  strcpy (newname + len, ".literal");

  /* Note that retrieve_literal_seg does not create a segment if 
     it already exists.  */
  default_lit_sections.lit_seg = NULL;	/* retrieved on demand */

  /* Canonicalizing section names allows renaming literal
     sections to occur correctly.  */
  default_lit_sections.lit_seg_name =
    tc_canonicalize_symbol_name (newname);

  free (name);

  /* Restore the current section and subsection and set the 
     generation into the old segment.  */
  subseg_set (s_now, ss_now);
}


/* Parsing and Idiom Translation.  */

static const char *
expression_end (name)
     const char *name;
{
  while (1)
    {
      switch (*name)
	{
	case ';':
	case '\0':
	case ',':
	  return name;
	case ' ':
	case '\t':
	  ++name;
	  continue;
	default:
	  return 0;
	}
    }
}


#define ERROR_REG_NUM ((unsigned) -1)

static unsigned
tc_get_register (prefix)
     const char *prefix;
{
  unsigned reg;
  const char *next_expr;
  const char *old_line_pointer;

  SKIP_WHITESPACE ();
  old_line_pointer = input_line_pointer;

  if (*input_line_pointer == '$')
    ++input_line_pointer;

  /* Accept "sp" as a synonym for "a1".  */
  if (input_line_pointer[0] == 's' && input_line_pointer[1] == 'p'
      && expression_end (input_line_pointer + 2))
    {
      input_line_pointer += 2;
      return 1;  /* AR[1] */
    }

  while (*input_line_pointer++ == *prefix++)
    ;
  --input_line_pointer;
  --prefix;

  if (*prefix)
    {
      as_bad (_("bad register name: %s"), old_line_pointer);
      return ERROR_REG_NUM;
    }

  if (!ISDIGIT ((unsigned char) *input_line_pointer))
    {
      as_bad (_("bad register number: %s"), input_line_pointer);
      return ERROR_REG_NUM;
    }

  reg = 0;

  while (ISDIGIT ((int) *input_line_pointer))
    reg = reg * 10 + *input_line_pointer++ - '0';

  if (!(next_expr = expression_end (input_line_pointer)))
    {
      as_bad (_("bad register name: %s"), old_line_pointer);
      return ERROR_REG_NUM;
    }

  input_line_pointer = (char *) next_expr;

  return reg;
}


#define PLT_SUFFIX "@PLT"
#define plt_suffix "@plt"

static void
expression_maybe_register (opnd, tok)
     xtensa_operand opnd;
     expressionS *tok;
{
  char *kind = xtensa_operand_kind (opnd);

  if ((strlen (kind) == 1)
      && (*kind == 'l' || *kind == 'L' || *kind == 'i' || *kind == 'r'))
    {
      segT t = expression (tok);
      if (t == absolute_section && operand_is_pcrel_label (opnd))
	{
	  assert (tok->X_op == O_constant);
	  tok->X_op = O_symbol;
	  tok->X_add_symbol = &abs_symbol;
	}
      if (tok->X_op == O_symbol 
	  && (!strncmp (input_line_pointer, PLT_SUFFIX,
			strlen (PLT_SUFFIX) - 1)
	      || !strncmp (input_line_pointer, plt_suffix,
			   strlen (plt_suffix) - 1)))
	{
	  symbol_get_tc (tok->X_add_symbol)->plt = 1;
	  input_line_pointer += strlen (plt_suffix);
	}
    }
  else
    {
      unsigned reg = tc_get_register (kind);

      if (reg != ERROR_REG_NUM)	/* Already errored */
	{
	  uint32 buf = reg;
	  if ((xtensa_operand_encode (opnd, &buf) != xtensa_encode_result_ok)
	      || (reg != xtensa_operand_decode (opnd, buf)))
	    as_bad (_("register number out of range"));
	}

      tok->X_op = O_register;
      tok->X_add_symbol = 0;
      tok->X_add_number = reg;
    }
}


/* Split up the arguments for an opcode or pseudo-op.  */

static int
tokenize_arguments (args, str)
     char **args;
     char *str;
{
  char *old_input_line_pointer;
  bfd_boolean saw_comma = FALSE;
  bfd_boolean saw_arg = FALSE;
  int num_args = 0;
  char *arg_end, *arg;
  int arg_len;
  
  /* Save and restore input_line_pointer around this function.  */ 
  old_input_line_pointer = input_line_pointer;
  input_line_pointer = str;

  while (*input_line_pointer)
    {
      SKIP_WHITESPACE ();
      switch (*input_line_pointer)
	{
	case '\0':
	  goto fini;

	case ',':
	  input_line_pointer++;
	  if (saw_comma || !saw_arg)
	    goto err;
	  saw_comma = TRUE;
	  break;

	default:
	  if (!saw_comma && saw_arg)
	    goto err;

	  arg_end = input_line_pointer + 1;
	  while (!expression_end (arg_end))
	    arg_end += 1;
 
	  arg_len = arg_end - input_line_pointer;
	  arg = (char *) xmalloc (arg_len + 1);
	  args[num_args] = arg;

	  strncpy (arg, input_line_pointer, arg_len);
	  arg[arg_len] = '\0';
 
	  input_line_pointer = arg_end;
	  num_args += 1;
	  saw_comma = FALSE; 
	  saw_arg = TRUE; 
	  break;
	}
    }

fini:
  if (saw_comma)
    goto err;
  input_line_pointer = old_input_line_pointer;
  return num_args;

err:
  input_line_pointer = old_input_line_pointer;
  return -1;
}


/* Parse the arguments to an opcode.  Return true on error.  */

static bfd_boolean
parse_arguments (insn, num_args, arg_strings)
     TInsn *insn;
     int num_args;
     char **arg_strings;
{
  expressionS *tok = insn->tok;
  xtensa_opcode opcode = insn->opcode;
  bfd_boolean had_error = TRUE;
  xtensa_isa isa = xtensa_default_isa; 
  int n;
  int opcode_operand_count;
  int actual_operand_count = 0;
  xtensa_operand opnd = NULL; 
  char *old_input_line_pointer;

  if (insn->insn_type == ITYPE_LITERAL)
    opcode_operand_count = 1;
  else
    opcode_operand_count = xtensa_num_operands (isa, opcode);

  memset (tok, 0, sizeof (*tok) * MAX_INSN_ARGS);

  /* Save and restore input_line_pointer around this function.  */
  old_input_line_pointer = input_line_pointer; 

  for (n = 0; n < num_args; n++)
    { 
      input_line_pointer = arg_strings[n];

      if (actual_operand_count >= opcode_operand_count)
	{ 
	  as_warn (_("too many arguments")); 
	  goto err;
	} 
      assert (actual_operand_count < MAX_INSN_ARGS);

      opnd = xtensa_get_operand (isa, opcode, actual_operand_count); 
      expression_maybe_register (opnd, tok);

      if (tok->X_op == O_illegal || tok->X_op == O_absent) 
	goto err; 
      actual_operand_count++;
      tok++; 
    } 

  insn->ntok = tok - insn->tok;
  had_error = FALSE; 

 err:
  input_line_pointer = old_input_line_pointer; 
  return had_error;
}


static void
xg_reverse_shift_count (cnt_argp)
     char **cnt_argp;
{
  char *cnt_arg, *new_arg;
  cnt_arg = *cnt_argp;

  /* replace the argument with "31-(argument)" */
  new_arg = (char *) xmalloc (strlen (cnt_arg) + 6);
  sprintf (new_arg, "31-(%s)", cnt_arg);

  free (cnt_arg);
  *cnt_argp = new_arg;
}


/* If "arg" is a constant expression, return non-zero with the value
   in *valp.  */

static int
xg_arg_is_constant (arg, valp)
     char *arg;
     offsetT *valp;
{
  expressionS exp;
  char *save_ptr = input_line_pointer;

  input_line_pointer = arg;
  expression (&exp);
  input_line_pointer = save_ptr;

  if (exp.X_op == O_constant)
    {
      *valp = exp.X_add_number;
      return 1;
    }

  return 0;
}


static void
xg_replace_opname (popname, newop)
     char **popname;
     char *newop;
{
  free (*popname);
  *popname = (char *) xmalloc (strlen (newop) + 1);
  strcpy (*popname, newop);
}


static int
xg_check_num_args (pnum_args, expected_num, opname, arg_strings)
     int *pnum_args;
     int expected_num; 
     char *opname;
     char **arg_strings;
{
  int num_args = *pnum_args;

  if (num_args < expected_num) 
    {
      as_bad (_("not enough operands (%d) for '%s'; expected %d"),
	      num_args, opname, expected_num);
      return -1;
    }

  if (num_args > expected_num)
    {
      as_warn (_("too many operands (%d) for '%s'; expected %d"),
	       num_args, opname, expected_num);
      while (num_args-- > expected_num)
	{
	  free (arg_strings[num_args]);
	  arg_strings[num_args] = 0;
	}
      *pnum_args = expected_num;
      return -1;
    }

  return 0;
}


static int
xg_translate_sysreg_op (popname, pnum_args, arg_strings)
     char **popname;
     int *pnum_args;
     char **arg_strings;
{
  char *opname, *new_opname;
  offsetT val;
  bfd_boolean has_underbar = FALSE;

  opname = *popname;
  if (*opname == '_')
    {
      has_underbar = TRUE;
      opname += 1;
    }

  /* Opname == [rw]ur... */

  if (opname[3] == '\0')
    {
      /* If the register is not specified as part of the opcode,
	 then get it from the operand and move it to the opcode.  */

      if (xg_check_num_args (pnum_args, 2, opname, arg_strings))
	return -1;

      if (!xg_arg_is_constant (arg_strings[1], &val))
	{
	  as_bad (_("register number for `%s' is not a constant"), opname);
	  return -1;
	}
      if ((unsigned) val > 255)
	{
	  as_bad (_("register number (%ld) for `%s' is out of range"),
		  val, opname);
	  return -1;
	}

      /* Remove the last argument, which is now part of the opcode.  */
      free (arg_strings[1]);
      arg_strings[1] = 0;
      *pnum_args = 1;

      /* Translate the opcode.  */
      new_opname = (char *) xmalloc (8);
      sprintf (new_opname, "%s%cur%u", (has_underbar ? "_" : ""),
	       opname[0], (unsigned) val);
      free (*popname);
      *popname = new_opname;
    }

  return 0;
}


/* If the instruction is an idiom (i.e., a built-in macro), translate it.
   Returns non-zero if an error was found.  */

static int
xg_translate_idioms (popname, pnum_args, arg_strings)
     char **popname;
     int *pnum_args;
     char **arg_strings;
{
  char *opname = *popname;
  bfd_boolean has_underbar = FALSE;

  if (*opname == '_')
    {
      has_underbar = TRUE;
      opname += 1;
    }

  if (strcmp (opname, "mov") == 0)
    {
      if (!has_underbar && code_density_available ())
	xg_replace_opname (popname, "mov.n");
      else
	{
	  if (xg_check_num_args (pnum_args, 2, opname, arg_strings))
	    return -1;
	  xg_replace_opname (popname, (has_underbar ? "_or" : "or"));
	  arg_strings[2] = (char *) xmalloc (strlen (arg_strings[1]) + 1);
	  strcpy (arg_strings[2], arg_strings[1]);
	  *pnum_args = 3;
	}
      return 0;
    }

  if (strcmp (opname, "bbsi.l") == 0)
    {
      if (xg_check_num_args (pnum_args, 3, opname, arg_strings))
	return -1;
      xg_replace_opname (popname, (has_underbar ? "_bbsi" : "bbsi"));
      if (target_big_endian)
	xg_reverse_shift_count (&arg_strings[1]);
      return 0;
    }

  if (strcmp (opname, "bbci.l") == 0)
    {
      if (xg_check_num_args (pnum_args, 3, opname, arg_strings))
	return -1;
      xg_replace_opname (popname, (has_underbar ? "_bbci" : "bbci"));
      if (target_big_endian)
	xg_reverse_shift_count (&arg_strings[1]);
      return 0;
    }

  if (strcmp (opname, "nop") == 0)
    {
      if (!has_underbar && code_density_available ())
	xg_replace_opname (popname, "nop.n");
      else
	{
	  if (xg_check_num_args (pnum_args, 0, opname, arg_strings))
	    return -1;
	  xg_replace_opname (popname, (has_underbar ? "_or" : "or"));
	  arg_strings[0] = (char *) xmalloc (3);
	  arg_strings[1] = (char *) xmalloc (3);
	  arg_strings[2] = (char *) xmalloc (3);
	  strcpy (arg_strings[0], "a1");
	  strcpy (arg_strings[1], "a1");
	  strcpy (arg_strings[2], "a1");
	  *pnum_args = 3;
	}
      return 0;
    }

  if ((opname[0] == 'r' || opname[0] == 'w')
      && opname[1] == 'u'
      && opname[2] == 'r')
    return xg_translate_sysreg_op (popname, pnum_args, arg_strings);


  /* WIDENING DENSITY OPCODES

     questionable relaxations (widening) from old "tai" idioms:

       ADD.N --> ADD
       BEQZ.N --> BEQZ
       RET.N --> RET
       RETW.N --> RETW
       MOVI.N --> MOVI
       MOV.N --> MOV
       NOP.N --> NOP

     Note: this incomplete list was imported to match the "tai"
     behavior; other density opcodes are not handled.

     The xtensa-relax code may know how to do these but it doesn't do
     anything when these density opcodes appear inside a no-density
     region.  Somehow GAS should either print an error when that happens
     or do the widening.  The old "tai" behavior was to do the widening.
     For now, I'll make it widen but print a warning.

     FIXME: GAS needs to detect density opcodes inside no-density
     regions and treat them as errors.  This code should be removed
     when that is done.  */

  if (use_generics ()
      && !has_underbar
      && density_supported
      && !code_density_available ())
    {
      if (strcmp (opname, "add.n") == 0)
	xg_replace_opname (popname, "add");

      else if (strcmp (opname, "beqz.n") == 0)
	xg_replace_opname (popname, "beqz");

      else if (strcmp (opname, "ret.n") == 0)
	xg_replace_opname (popname, "ret");

      else if (strcmp (opname, "retw.n") == 0)
	xg_replace_opname (popname, "retw");

      else if (strcmp (opname, "movi.n") == 0)
	xg_replace_opname (popname, "movi");

      else if (strcmp (opname, "mov.n") == 0)
	{
	  if (xg_check_num_args (pnum_args, 2, opname, arg_strings))
	    return -1;
	  xg_replace_opname (popname, "or");
	  arg_strings[2] = (char *) xmalloc (strlen (arg_strings[1]) + 1);
	  strcpy (arg_strings[2], arg_strings[1]);
	  *pnum_args = 3;
	}

      else if (strcmp (opname, "nop.n") == 0)
	{
	  if (xg_check_num_args (pnum_args, 0, opname, arg_strings))
	    return -1;
	  xg_replace_opname (popname, "or");
	  arg_strings[0] = (char *) xmalloc (3);
	  arg_strings[1] = (char *) xmalloc (3);
	  arg_strings[2] = (char *) xmalloc (3);
	  strcpy (arg_strings[0], "a1");
	  strcpy (arg_strings[1], "a1");
	  strcpy (arg_strings[2], "a1");
	  *pnum_args = 3;
	}
    }

  return 0;
}


/* Functions for dealing with the Xtensa ISA.  */

/* Return true if the given operand is an immed or target instruction,
   i.e., has a reloc associated with it.  Currently, this is only true
   if the operand kind is "i, "l" or "L".  */

static bfd_boolean
operand_is_immed (opnd)
     xtensa_operand opnd;
{
  const char *opkind = xtensa_operand_kind (opnd);
  if (opkind[0] == '\0' || opkind[1] != '\0')
    return FALSE;
  switch (opkind[0])
    {
    case 'i':
    case 'l':
    case 'L':
      return TRUE;
    }
  return FALSE;
}


/* Return true if the given operand is a pc-relative label.  This is
   true for "l", "L", and "r" operand kinds.  */

bfd_boolean
operand_is_pcrel_label (opnd)
     xtensa_operand opnd;
{
  const char *opkind = xtensa_operand_kind (opnd);
  if (opkind[0] == '\0' || opkind[1] != '\0')
    return FALSE;
  switch (opkind[0])
    {
    case 'r':
    case 'l':
    case 'L':
      return TRUE;
    }
  return FALSE;
}


/* Currently the assembler only allows us to use a single target per
   fragment.  Because of this, only one operand for a given
   instruction may be symbolic.  If there is an operand of kind "lrL",
   the last one is chosen.  Otherwise, the result is the number of the
   last operand of type "i", and if there are none of those, we fail
   and return -1.  */

int
get_relaxable_immed (opcode)
     xtensa_opcode opcode;
{
  int last_immed = -1;
  int noperands, opi;
  xtensa_operand operand;

  if (opcode == XTENSA_UNDEFINED)
    return -1;

  noperands = xtensa_num_operands (xtensa_default_isa, opcode);
  for (opi = noperands - 1; opi >= 0; opi--)
    {
      operand = xtensa_get_operand (xtensa_default_isa, opcode, opi);
      if (operand_is_pcrel_label (operand))
	return opi;
      if (last_immed == -1 && operand_is_immed (operand))
	last_immed = opi;
    }
  return last_immed;
}


xtensa_opcode
get_opcode_from_buf (buf)
     const char *buf;
{
  static xtensa_insnbuf insnbuf = NULL;
  xtensa_opcode opcode;
  xtensa_isa isa = xtensa_default_isa;
  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (isa);

  xtensa_insnbuf_from_chars (isa, insnbuf, buf);
  opcode = xtensa_decode_insn (isa, insnbuf);
  return opcode;
}


static bfd_boolean
is_direct_call_opcode (opcode)
     xtensa_opcode opcode;
{
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_call0_opcode
	  || opcode == xtensa_call4_opcode
	  || opcode == xtensa_call8_opcode
	  || opcode == xtensa_call12_opcode);
}


static bfd_boolean
is_call_opcode (opcode)
     xtensa_opcode opcode;
{
  if (is_direct_call_opcode (opcode))
    return TRUE;

  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_callx0_opcode
	  || opcode == xtensa_callx4_opcode
	  || opcode == xtensa_callx8_opcode
	  || opcode == xtensa_callx12_opcode);
}


/* Return true if the opcode is an entry opcode.  This is used because
   "entry" adds an implicit ".align 4" and also the entry instruction
   has an extra check for an operand value.  */

static bfd_boolean
is_entry_opcode (opcode)
     xtensa_opcode opcode;
{
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_entry_opcode);
}


/* Return true if it is one of the loop opcodes.  Loops are special
   because they need automatic alignment and they have a relaxation so
   complex that we hard-coded it.  */

static bfd_boolean
is_loop_opcode (opcode)
     xtensa_opcode opcode;
{
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_loop_opcode
	  || opcode == xtensa_loopnez_opcode
	  || opcode == xtensa_loopgtz_opcode);
}


static bfd_boolean
is_the_loop_opcode (opcode)
     xtensa_opcode opcode;
{
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_loop_opcode);
}


static bfd_boolean
is_jx_opcode (opcode)
     xtensa_opcode opcode;
{
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_jx_opcode);
}


/* Return true if the opcode is a retw or retw.n.
   Needed to add nops to avoid a hardware interlock issue.  */

static bfd_boolean
is_windowed_return_opcode (opcode)
     xtensa_opcode opcode;
{
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  return (opcode == xtensa_retw_opcode || opcode == xtensa_retw_n_opcode);
}


/* Return true if the opcode type is "l" and the opcode is NOT a jump.  */

static bfd_boolean
is_conditional_branch_opcode (opcode)
     xtensa_opcode opcode;
{
  xtensa_isa isa = xtensa_default_isa;
  int num_ops, i;

  if (opcode == xtensa_j_opcode && opcode != XTENSA_UNDEFINED)
    return FALSE;

  num_ops = xtensa_num_operands (isa, opcode);
  for (i = 0; i < num_ops; i++)
    {
      xtensa_operand operand = xtensa_get_operand (isa, opcode, i);
      if (strcmp (xtensa_operand_kind (operand), "l") == 0)
	return TRUE;
    }
  return FALSE;
}


/* Return true if the given opcode is a conditional branch
   instruction, i.e., currently this is true if the instruction 
   is a jx or has an operand with 'l' type and is not a loop.  */

bfd_boolean
is_branch_or_jump_opcode (opcode)
     xtensa_opcode opcode;
{
  int opn, op_count;

  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  if (is_loop_opcode (opcode))
    return FALSE;

  if (is_jx_opcode (opcode))
    return TRUE;

  op_count = xtensa_num_operands (xtensa_default_isa, opcode);
  for (opn = 0; opn < op_count; opn++)
    {
      xtensa_operand opnd =
	xtensa_get_operand (xtensa_default_isa, opcode, opn);
      const char *opkind = xtensa_operand_kind (opnd);
      if (opkind && opkind[0] == 'l' && opkind[1] == '\0')
	return TRUE;
    }
  return FALSE;
}


/* Convert from operand numbers to BFD relocation type code.
   Return BFD_RELOC_NONE on failure.  */

bfd_reloc_code_real_type
opnum_to_reloc (opnum)
     int opnum;
{
  switch (opnum)
    {
    case 0:
      return BFD_RELOC_XTENSA_OP0;
    case 1:
      return BFD_RELOC_XTENSA_OP1;
    case 2:
      return BFD_RELOC_XTENSA_OP2;
    default:
      break;
    }
  return BFD_RELOC_NONE;
}


/* Convert from BFD relocation type code to operand number.
   Return -1 on failure.  */

int
reloc_to_opnum (reloc)
     bfd_reloc_code_real_type reloc;
{
  switch (reloc)
    {
    case BFD_RELOC_XTENSA_OP0:
      return 0;
    case BFD_RELOC_XTENSA_OP1:
      return 1;
    case BFD_RELOC_XTENSA_OP2:
      return 2;
    default:
      break;
    }
  return -1;
}


static void
xtensa_insnbuf_set_operand (insnbuf, opcode, operand, value, file, line)
     xtensa_insnbuf insnbuf;
     xtensa_opcode opcode;
     xtensa_operand operand;
     int32 value;
     const char *file;
     unsigned int line;
{
  xtensa_encode_result encode_result;
  uint32 valbuf = value;

  encode_result = xtensa_operand_encode (operand, &valbuf);

  switch (encode_result)
    {
    case xtensa_encode_result_ok:
      break;
    case xtensa_encode_result_align:
      as_bad_where ((char *) file, line,
		    _("operand %d not properly aligned for '%s'"),
		    value, xtensa_opcode_name (xtensa_default_isa, opcode));
      break;
    case xtensa_encode_result_not_in_table:
      as_bad_where ((char *) file, line,
		    _("operand %d not in immediate table for '%s'"),
		    value, xtensa_opcode_name (xtensa_default_isa, opcode));
      break;
    case xtensa_encode_result_too_high:
      as_bad_where ((char *) file, line,
		    _("operand %d too large for '%s'"), value,
		    xtensa_opcode_name (xtensa_default_isa, opcode));
      break;
    case xtensa_encode_result_too_low:
      as_bad_where ((char *) file, line,
		    _("operand %d too small for '%s'"), value,
		    xtensa_opcode_name (xtensa_default_isa, opcode));
      break;
    case xtensa_encode_result_not_ok:
      as_bad_where ((char *) file, line,
		    _("operand %d is invalid for '%s'"), value,
		    xtensa_opcode_name (xtensa_default_isa, opcode));
      break;
    default:
      abort ();
    }

  xtensa_operand_set_field (operand, insnbuf, valbuf);
}


static uint32
xtensa_insnbuf_get_operand (insnbuf, opcode, opnum)
     xtensa_insnbuf insnbuf;
     xtensa_opcode opcode;
     int opnum;
{
  xtensa_operand op = xtensa_get_operand (xtensa_default_isa, opcode, opnum);
  return xtensa_operand_decode (op, xtensa_operand_get_field (op, insnbuf));
}


static void
xtensa_insnbuf_set_immediate_field (opcode, insnbuf, value, file, line)
     xtensa_opcode opcode;
     xtensa_insnbuf insnbuf;
     int32 value;
     const char *file;
     unsigned int line;
{
  xtensa_isa isa = xtensa_default_isa;
  int last_opnd = xtensa_num_operands (isa, opcode) - 1;
  xtensa_operand operand = xtensa_get_operand (isa, opcode, last_opnd);
  xtensa_insnbuf_set_operand (insnbuf, opcode, operand, value, file, line);
}


static bfd_boolean
is_negatable_branch (insn)
     TInsn *insn;
{
  xtensa_isa isa = xtensa_default_isa;
  int i;
  int num_ops = xtensa_num_operands (isa, insn->opcode);

  for (i = 0; i < num_ops; i++)
    {
      xtensa_operand opnd = xtensa_get_operand (isa, insn->opcode, i);
      char *kind = xtensa_operand_kind (opnd);
      if (strlen (kind) == 1 && *kind == 'l')
	return TRUE;
    }
  return FALSE;
}


/* Various Other Internal Functions.  */

static bfd_boolean
is_unique_insn_expansion (r)
     TransitionRule *r;
{
  if (!r->to_instr || r->to_instr->next != NULL)
    return FALSE;
  if (r->to_instr->typ != INSTR_INSTR)
    return FALSE;
  return TRUE;
}


static int
xg_get_insn_size (insn)
     TInsn *insn;
{
  assert (insn->insn_type == ITYPE_INSN);
  return xtensa_insn_length (xtensa_default_isa, insn->opcode);
}


static int
xg_get_build_instr_size (insn)
     BuildInstr *insn;
{
  assert (insn->typ == INSTR_INSTR);
  return xtensa_insn_length (xtensa_default_isa, insn->opcode);
}


bfd_boolean
xg_is_narrow_insn (insn)
     TInsn *insn;
{
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;
  int num_match = 0;
  assert (insn->insn_type == ITYPE_INSN);
  assert (insn->opcode < table->num_opcodes);

  for (l = table->table[insn->opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;

      if (xg_instruction_matches_rule (insn, rule)
	  && is_unique_insn_expansion (rule))
	{
	  /* It only generates one instruction... */
	  assert (insn->insn_type == ITYPE_INSN);
	  /* ...and it is a larger instruction.  */
	  if (xg_get_insn_size (insn)
	      < xg_get_build_instr_size (rule->to_instr))
	    {
	      num_match++;
	      if (num_match > 1)
		return FALSE;
	    }
	}
    }
  return (num_match == 1);
}


bfd_boolean
xg_is_single_relaxable_insn (insn)
     TInsn *insn;
{
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;
  int num_match = 0;
  assert (insn->insn_type == ITYPE_INSN);
  assert (insn->opcode < table->num_opcodes);

  for (l = table->table[insn->opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;

      if (xg_instruction_matches_rule (insn, rule)
	  && is_unique_insn_expansion (rule))
	{
	  assert (insn->insn_type == ITYPE_INSN);
	  /* ... and it is a larger instruction.  */
	  if (xg_get_insn_size (insn)
	      <= xg_get_build_instr_size (rule->to_instr))
	    {
	      num_match++;
	      if (num_match > 1)
		return FALSE;
	    }
	}
    }
  return (num_match == 1);
}


/* Return the largest size instruction that this instruction can
   expand to.  Currently, in all cases, this is 3 bytes.  Of course we
   could just calculate this once and generate a table.  */

int
xg_get_max_narrow_insn_size (opcode)
     xtensa_opcode opcode;
{
  /* Go ahead and compute it, but it better be 3.  */
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;
  int old_size = xtensa_insn_length (xtensa_default_isa, opcode);
  assert (opcode < table->num_opcodes);

  /* Actually we can do better. Check to see of Only one applies.  */
  for (l = table->table[opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;

      /* If it only generates one instruction.  */
      if (is_unique_insn_expansion (rule))
	{
	  int new_size = xtensa_insn_length (xtensa_default_isa,
					     rule->to_instr->opcode);
	  if (new_size > old_size)
	    {
	      assert (new_size == 3);
	      return 3;
	    }
	}
    }
  return old_size;
}


/* Return the maximum number of bytes this opcode can expand to.  */

int
xg_get_max_insn_widen_size (opcode)
     xtensa_opcode opcode;
{
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;
  int max_size = xtensa_insn_length (xtensa_default_isa, opcode);

  assert (opcode < table->num_opcodes);

  for (l = table->table[opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;
      BuildInstr *build_list;
      int this_size = 0;

      if (!rule)
	continue;
      build_list = rule->to_instr;
      if (is_unique_insn_expansion (rule))
	{
	  assert (build_list->typ == INSTR_INSTR);
	  this_size = xg_get_max_insn_widen_size (build_list->opcode);
	}
      else
	for (; build_list != NULL; build_list = build_list->next)
	  {
	    switch (build_list->typ)
	      {
	      case INSTR_INSTR:
		this_size += xtensa_insn_length (xtensa_default_isa,
						 build_list->opcode);

		break;
	      case INSTR_LITERAL_DEF:
	      case INSTR_LABEL_DEF:
	      default:
		break;
	      }
	  }
      if (this_size > max_size)
	max_size = this_size;
    }
  return max_size;
}


/* Return the maximum number of literal bytes this opcode can generate.  */

int
xg_get_max_insn_widen_literal_size (opcode)
     xtensa_opcode opcode;
{
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;
  int max_size = 0;

  assert (opcode < table->num_opcodes);

  for (l = table->table[opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;
      BuildInstr *build_list;
      int this_size = 0;

      if (!rule)
	continue;
      build_list = rule->to_instr;
      if (is_unique_insn_expansion (rule))
	{
	  assert (build_list->typ == INSTR_INSTR);
	  this_size = xg_get_max_insn_widen_literal_size (build_list->opcode);
	}
      else
	for (; build_list != NULL; build_list = build_list->next)
	  {
	    switch (build_list->typ)
	      {
	      case INSTR_LITERAL_DEF:
		/* hard coded 4-byte literal.  */
		this_size += 4;
		break;
	      case INSTR_INSTR:
	      case INSTR_LABEL_DEF:
	      default:
		break;
	      }
	  }
      if (this_size > max_size)
	max_size = this_size;
    }
  return max_size;
}


bfd_boolean
xg_is_relaxable_insn (insn, lateral_steps)
     TInsn *insn;
     int lateral_steps;
{
  int steps_taken = 0;
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;

  assert (insn->insn_type == ITYPE_INSN);
  assert (insn->opcode < table->num_opcodes);

  for (l = table->table[insn->opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;

      if (xg_instruction_matches_rule (insn, rule))
	{
	  if (steps_taken == lateral_steps)
	    return TRUE;
	  steps_taken++;
	}
    }
  return FALSE;
}


static symbolS *
get_special_literal_symbol ()
{
  static symbolS *sym = NULL;

  if (sym == NULL)
    sym = symbol_find_or_make ("SPECIAL_LITERAL0\001");
  return sym;
}


static symbolS *
get_special_label_symbol ()
{
  static symbolS *sym = NULL;

  if (sym == NULL)
    sym = symbol_find_or_make ("SPECIAL_LABEL0\001");
  return sym;
}


/* Return true on success.  */

bfd_boolean
xg_build_to_insn (targ, insn, bi)
     TInsn *targ;
     TInsn *insn;
     BuildInstr *bi;
{
  BuildOp *op;
  symbolS *sym;

  memset (targ, 0, sizeof (TInsn));
  switch (bi->typ)
    {
    case INSTR_INSTR:
      op = bi->ops;
      targ->opcode = bi->opcode;
      targ->insn_type = ITYPE_INSN;
      targ->is_specific_opcode = FALSE;

      for (; op != NULL; op = op->next)
	{
	  int op_num = op->op_num;
	  int op_data = op->op_data;

	  assert (op->op_num < MAX_INSN_ARGS);

	  if (targ->ntok <= op_num)
	    targ->ntok = op_num + 1;

	  switch (op->typ)
	    {
	    case OP_CONSTANT:
	      set_expr_const (&targ->tok[op_num], op_data);
	      break;
	    case OP_OPERAND:
	      assert (op_data < insn->ntok);
	      copy_expr (&targ->tok[op_num], &insn->tok[op_data]);
	      break;
	    case OP_LITERAL:
	      sym = get_special_literal_symbol ();
	      set_expr_symbol_offset (&targ->tok[op_num], sym, 0);
	      break;
	    case OP_LABEL:
	      sym = get_special_label_symbol ();
	      set_expr_symbol_offset (&targ->tok[op_num], sym, 0);
	      break;
	    default:
	      /* currently handles:
		 OP_OPERAND_LOW8
		 OP_OPERAND_HI24S
		 OP_OPERAND_F32MINUS */
	      if (xg_has_userdef_op_fn (op->typ))
		{
		  assert (op_data < insn->ntok);
		  if (expr_is_const (&insn->tok[op_data]))
		    {
		      long val;
		      copy_expr (&targ->tok[op_num], &insn->tok[op_data]);
		      val = xg_apply_userdef_op_fn (op->typ,
						    targ->tok[op_num].
						    X_add_number);
		      targ->tok[op_num].X_add_number = val;
		    }
		  else
		    return FALSE; /* We cannot use a relocation for this.  */
		  break;
		}
	      assert (0);
	      break;
	    }
	}
      break;

    case INSTR_LITERAL_DEF:
      op = bi->ops;
      targ->opcode = XTENSA_UNDEFINED;
      targ->insn_type = ITYPE_LITERAL;
      targ->is_specific_opcode = FALSE;
      for (; op != NULL; op = op->next)
	{
	  int op_num = op->op_num;
	  int op_data = op->op_data;
	  assert (op->op_num < MAX_INSN_ARGS);

	  if (targ->ntok <= op_num)
	    targ->ntok = op_num + 1;

	  switch (op->typ)
	    {
	    case OP_OPERAND:
	      assert (op_data < insn->ntok);
	      copy_expr (&targ->tok[op_num], &insn->tok[op_data]);
	      break;
	    case OP_LITERAL:
	    case OP_CONSTANT:
	    case OP_LABEL:
	    default:
	      assert (0);
	      break;
	    }
	}
      break;

    case INSTR_LABEL_DEF:
      op = bi->ops;
      targ->opcode = XTENSA_UNDEFINED;
      targ->insn_type = ITYPE_LABEL;
      targ->is_specific_opcode = FALSE;
      /* Literal with no ops. is a label?  */
      assert (op == NULL);
      break;

    default:
      assert (0);
    }

  return TRUE;
}


/* Return true on success.  */

bfd_boolean
xg_build_to_stack (istack, insn, bi)
     IStack *istack;
     TInsn *insn;
     BuildInstr *bi;
{
  for (; bi != NULL; bi = bi->next)
    {
      TInsn *next_insn = istack_push_space (istack);

      if (!xg_build_to_insn (next_insn, insn, bi))
	return FALSE;
    }
  return TRUE;
}


/* Return true on valid expansion.  */

bfd_boolean
xg_expand_to_stack (istack, insn, lateral_steps)
     IStack *istack;
     TInsn *insn;
     int lateral_steps;
{
  int stack_size = istack->ninsn;
  int steps_taken = 0;
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;

  assert (insn->insn_type == ITYPE_INSN);
  assert (insn->opcode < table->num_opcodes);

  for (l = table->table[insn->opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;

      if (xg_instruction_matches_rule (insn, rule))
	{
	  if (lateral_steps == steps_taken)
	    {
	      int i;

	      /* This is it.  Expand the rule to the stack.  */
	      if (!xg_build_to_stack (istack, insn, rule->to_instr))
		return FALSE;

	      /* Check to see if it fits.  */
	      for (i = stack_size; i < istack->ninsn; i++)
		{
		  TInsn *insn = &istack->insn[i];

		  if (insn->insn_type == ITYPE_INSN
		      && !tinsn_has_symbolic_operands (insn)
		      && !xg_immeds_fit (insn))
		    {
		      istack->ninsn = stack_size;
		      return FALSE;
		    }
		}
	      return TRUE;
	    }
	  steps_taken++;
	}
    }
  return FALSE;
}


bfd_boolean
xg_expand_narrow (targ, insn)
     TInsn *targ;
     TInsn *insn;
{
  TransitionTable *table = xg_build_widen_table ();
  TransitionList *l;

  assert (insn->insn_type == ITYPE_INSN);
  assert (insn->opcode < table->num_opcodes);

  for (l = table->table[insn->opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;
      if (xg_instruction_matches_rule (insn, rule)
	  && is_unique_insn_expansion (rule))
	{
	  /* Is it a larger instruction?  */
	  if (xg_get_insn_size (insn)
	      <= xg_get_build_instr_size (rule->to_instr))
	    {
	      xg_build_to_insn (targ, insn, rule->to_instr);
	      return FALSE;
	    }
	}
    }
  return TRUE;
}


/* Assumes: All immeds are constants.  Check that all constants fit
   into their immeds; return false if not.  */

static bfd_boolean
xg_immeds_fit (insn)
     const TInsn *insn;
{
  int i;

  int n = insn->ntok;
  assert (insn->insn_type == ITYPE_INSN);
  for (i = 0; i < n; ++i)
    {
      const expressionS *expr = &insn->tok[i];
      xtensa_operand opnd = xtensa_get_operand (xtensa_default_isa,
						insn->opcode, i);
      if (!operand_is_immed (opnd))
	continue;

      switch (expr->X_op)
	{
	case O_register:
	case O_constant:
	  {
	    if (xg_check_operand (expr->X_add_number, opnd))
	      return FALSE;
	  }
	  break;
	default:
	  /* The symbol should have a fixup associated with it.  */
	  assert (FALSE);
	  break;
	}
    }
  return TRUE;
}


/* This should only be called after we have an initial
   estimate of the addresses.  */

static bfd_boolean
xg_symbolic_immeds_fit (insn, pc_seg, pc_frag, pc_offset, stretch)
     const TInsn *insn;
     segT pc_seg;
     fragS *pc_frag;
     offsetT pc_offset;
     long stretch;
{
  symbolS *symbolP;
  offsetT target, pc, new_offset;
  int i;
  int n = insn->ntok;

  assert (insn->insn_type == ITYPE_INSN);

  for (i = 0; i < n; ++i)
    {
      const expressionS *expr = &insn->tok[i];
      xtensa_operand opnd = xtensa_get_operand (xtensa_default_isa,
						insn->opcode, i);
      if (!operand_is_immed (opnd))
	continue;

      switch (expr->X_op)
	{
	case O_register:
	case O_constant:
	  if (xg_check_operand (expr->X_add_number, opnd))
	    return FALSE;
	  break;

	case O_symbol:
	  /* We only allow symbols for pc-relative stuff.
	     If pc_frag == 0, then we don't have frag locations yet.  */
	  if (pc_frag == 0)
	    return FALSE;

	  /* If it is PC-relative and the symbol is in the same segment as
	     the PC.... */
	  if (!xtensa_operand_isPCRelative (opnd)
	      || S_GET_SEGMENT (expr->X_add_symbol) != pc_seg)
	    return FALSE;

	  symbolP = expr->X_add_symbol;
	  target = S_GET_VALUE (symbolP) + expr->X_add_number;
	  pc = pc_frag->fr_address + pc_offset;

	  /* If frag has yet to be reached on this pass, assume it
	     will move by STRETCH just as we did.  If this is not so,
	     it will be because some frag between grows, and that will
	     force another pass.  Beware zero-length frags.  There
	     should be a faster way to do this.  */

	  if (stretch && is_dnrange (pc_frag, symbolP, stretch))
	    target += stretch;

	  new_offset = xtensa_operand_do_reloc (opnd, target, pc);
	  if (xg_check_operand (new_offset, opnd))
	    return FALSE;
	  break;

	default:
	  /* The symbol should have a fixup associated with it.  */
	  return FALSE;
	}
    }

  return TRUE;
}


/* This will check to see if the value can be converted into the
   operand type.  It will return true if it does not fit.  */

static bfd_boolean
xg_check_operand (value, operand)
     int32 value;
     xtensa_operand operand;
{
  uint32 valbuf = value;
  return (xtensa_operand_encode (operand, &valbuf) != xtensa_encode_result_ok);
}


/* Check if a symbol is pointing to somewhere after
   the start frag, given that the segment has stretched 
   by stretch during relaxation.

   This is more complicated than it might appear at first blush
   because of the stretching that goes on. Here is how the check
   works:

   If the symbol and the frag are in the same segment, then
   the symbol could be down range. Note that this function 
   assumes that start_frag is in now_seg.

   If the symbol is pointing to a frag with an address greater than 
   than the start_frag's address, then it _could_ be down range. 

   The problem comes because target_frag may or may not have had
   stretch bytes added to its address already, depending on if it is 
   before or after start frag. (And if we knew that, then we wouldn't
   need this function.) start_frag has definitely already had stretch
   bytes added to its address.
   
   If target_frag's address hasn't been adjusted yet, then to 
   determine if it comes after start_frag, we need to subtract
   stretch from start_frag's address.

   If target_frag's address has been adjusted, then it might have
   been adjusted such that it comes after start_frag's address minus
   stretch bytes.

   So, in that case, we scan for it down stream to within 
   stretch bytes. We could search to the end of the fr_chain, but
   that ends up taking too much time (over a minute on some gnu 
   tests).  */

int
is_dnrange (start_frag, sym, stretch)
     fragS *start_frag;
     symbolS *sym;
     long stretch;
{
  if (S_GET_SEGMENT (sym) == now_seg)
    {
      fragS *cur_frag = symbol_get_frag (sym);

      if (cur_frag->fr_address >= start_frag->fr_address - stretch)
	{
	  int distance = stretch;

	  while (cur_frag && distance >= 0) 
	    {
	      distance -= cur_frag->fr_fix;
	      if (cur_frag == start_frag)
		return 0;
	      cur_frag = cur_frag->fr_next;
	    }
	  return 1;
	}
    }
  return 0;
}


/* Relax the assembly instruction at least "min_steps".
   Return the number of steps taken.  */

int
xg_assembly_relax (istack, insn, pc_seg, pc_frag, pc_offset, min_steps,
		   stretch)
     IStack *istack;
     TInsn *insn;
     segT pc_seg;
     fragS *pc_frag;		/* If pc_frag == 0, then no pc-relative.  */
     offsetT pc_offset;		/* Offset in fragment.  */
     int min_steps;		/* Minimum number of conversion steps.  */
     long stretch;		/* Number of bytes stretched so far.  */
{
  int steps_taken = 0;

  /* assert (has no symbolic operands)
     Some of its immeds don't fit.
     Try to build a relaxed version.
     This may go through a couple of stages
     of single instruction transformations before
     we get there.  */

  TInsn single_target;
  TInsn current_insn;
  int lateral_steps = 0;
  int istack_size = istack->ninsn;

  if (xg_symbolic_immeds_fit (insn, pc_seg, pc_frag, pc_offset, stretch)
      && steps_taken >= min_steps)
    {
      istack_push (istack, insn);
      return steps_taken;
    }
  tinsn_copy (&current_insn, insn);

  /* Walk through all of the single instruction expansions. */
  while (xg_is_single_relaxable_insn (&current_insn))
    {
      int error_val = xg_expand_narrow (&single_target, &current_insn);

      assert (!error_val);

      if (xg_symbolic_immeds_fit (&single_target, pc_seg, pc_frag, pc_offset,
				  stretch))
	{
	  steps_taken++;
	  if (steps_taken >= min_steps)
	    {
	      istack_push (istack, &single_target);
	      return steps_taken;
	    }
	}
      tinsn_copy (&current_insn, &single_target);
    }

  /* Now check for a multi-instruction expansion.  */
  while (xg_is_relaxable_insn (&current_insn, lateral_steps))
    {
      if (xg_symbolic_immeds_fit (&current_insn, pc_seg, pc_frag, pc_offset,
				  stretch))
	{
	  if (steps_taken >= min_steps)
	    {
	      istack_push (istack, &current_insn);
	      return steps_taken;
	    }
	}
      steps_taken++;
      if (xg_expand_to_stack (istack, &current_insn, lateral_steps))
	{
	  if (steps_taken >= min_steps)
	    return steps_taken;
	}
      lateral_steps++;
      istack->ninsn = istack_size;
    }

  /* It's not going to work -- use the original.  */
  istack_push (istack, insn);
  return steps_taken;
}


static void
xg_force_frag_space (size)
     int size;
{
  /* This may have the side effect of creating a new fragment for the
     space to go into.  I just do not like the name of the "frag"
     functions.  */
  frag_grow (size);
}


void
xg_finish_frag (last_insn, state, max_growth, is_insn)
     char *last_insn;
     enum xtensa_relax_statesE state;
     int max_growth;
     bfd_boolean is_insn;
{
  /* Finish off this fragment so that it has at LEAST the desired
     max_growth.  If it doesn't fit in this fragment, close this one
     and start a new one.  In either case, return a pointer to the
     beginning of the growth area.  */

  fragS *old_frag;
  xg_force_frag_space (max_growth);

  old_frag = frag_now;

  frag_now->fr_opcode = last_insn;
  if (is_insn)
    frag_now->tc_frag_data.is_insn = TRUE;

  frag_var (rs_machine_dependent, max_growth, max_growth,
	    state, frag_now->fr_symbol, frag_now->fr_offset, last_insn);

  /* Just to make sure that we did not split it up.  */
  assert (old_frag->fr_next == frag_now);
}


static bfd_boolean
is_branch_jmp_to_next (insn, fragP)
     TInsn *insn;
     fragS *fragP;
{
  xtensa_isa isa = xtensa_default_isa;
  int i;
  int num_ops = xtensa_num_operands (isa, insn->opcode);
  int target_op = -1;
  symbolS *sym;
  fragS *target_frag;

  if (is_loop_opcode (insn->opcode))
    return FALSE;

  for (i = 0; i < num_ops; i++)
    {
      xtensa_operand opnd = xtensa_get_operand (isa, insn->opcode, i);
      char *kind = xtensa_operand_kind (opnd);
      if (strlen (kind) == 1 && *kind == 'l')
	{
	  target_op = i;
	  break;
	}
    }
  if (target_op == -1)
    return FALSE;

  if (insn->ntok <= target_op)
    return FALSE;

  if (insn->tok[target_op].X_op != O_symbol)
    return FALSE;

  sym = insn->tok[target_op].X_add_symbol;
  if (sym == NULL)
    return FALSE;

  if (insn->tok[target_op].X_add_number != 0)
    return FALSE;

  target_frag = symbol_get_frag (sym);
  if (target_frag == NULL)
    return FALSE;

  if (is_next_frag_target (fragP->fr_next, target_frag) 
      && S_GET_VALUE (sym) == target_frag->fr_address)
    return TRUE;

  return FALSE;
}


static void
xg_add_branch_and_loop_targets (insn)
     TInsn *insn;
{
  xtensa_isa isa = xtensa_default_isa;
  int num_ops = xtensa_num_operands (isa, insn->opcode);

  if (is_loop_opcode (insn->opcode))
    {
      int i = 1;
      xtensa_operand opnd = xtensa_get_operand (isa, insn->opcode, i);
      char *kind = xtensa_operand_kind (opnd);
      if (strlen (kind) == 1 && *kind == 'l')
	if (insn->tok[i].X_op == O_symbol)
	  symbol_get_tc (insn->tok[i].X_add_symbol)->is_loop_target = TRUE;
      return;
    }

  /* Currently, we do not add branch targets.  This is an optimization
     for later that tries to align only branch targets, not just any
     label in a text section.  */

  if (align_only_targets)
    {
      int i;

      for (i = 0; i < insn->ntok && i < num_ops; i++)
	{
	  xtensa_operand opnd = xtensa_get_operand (isa, insn->opcode, i);
	  char *kind = xtensa_operand_kind (opnd);
	  if (strlen (kind) == 1 && *kind == 'l'
	      && insn->tok[i].X_op == O_symbol)
	    {
	      symbolS *sym = insn->tok[i].X_add_symbol;
	      symbol_get_tc (sym)->is_branch_target = TRUE;
	      if (S_IS_DEFINED (sym))
		symbol_get_frag (sym)->tc_frag_data.is_branch_target = TRUE;
	    }
	}
    }
}


/* Return the transition rule that matches or NULL if none matches.  */

bfd_boolean
xg_instruction_matches_rule (insn, rule)
     TInsn *insn;
     TransitionRule *rule;
{
  PreconditionList *condition_l;

  if (rule->opcode != insn->opcode)
    return FALSE;

  for (condition_l = rule->conditions;
       condition_l != NULL;
       condition_l = condition_l->next)
    {
      expressionS *exp1;
      expressionS *exp2;
      Precondition *cond = condition_l->precond;

      switch (cond->typ)
	{
	case OP_CONSTANT:
	  /* The expression must be the constant.  */
	  assert (cond->op_num < insn->ntok);
	  exp1 = &insn->tok[cond->op_num];
	  if (!expr_is_const (exp1))
	    return FALSE;
	  switch (cond->cmp)
	    {
	    case OP_EQUAL:
	      if (get_expr_const (exp1) != cond->op_data)
		return FALSE;
	      break;
	    case OP_NOTEQUAL:
	      if (get_expr_const (exp1) == cond->op_data)
		return FALSE;
	      break;
	    }
	  break;

	case OP_OPERAND:
	  assert (cond->op_num < insn->ntok);
	  assert (cond->op_data < insn->ntok);
	  exp1 = &insn->tok[cond->op_num];
	  exp2 = &insn->tok[cond->op_data];

	  switch (cond->cmp)
	    {
	    case OP_EQUAL:
	      if (!expr_is_equal (exp1, exp2))
		return FALSE;
	      break;
	    case OP_NOTEQUAL:
	      if (expr_is_equal (exp1, exp2))
		return FALSE;
	      break;
	    }
	  break;

	case OP_LITERAL:
	case OP_LABEL:
	default:
	  return FALSE;
	}
    }
  return TRUE;
}


TransitionRule *
xg_instruction_match (insn)
     TInsn *insn;
{
  TransitionTable *table = xg_build_simplify_table ();
  TransitionList *l;
  assert (insn->opcode < table->num_opcodes);

  /* Walk through all of the possible transitions.  */
  for (l = table->table[insn->opcode]; l != NULL; l = l->next)
    {
      TransitionRule *rule = l->rule;
      if (xg_instruction_matches_rule (insn, rule))
	return rule;
    }
  return NULL;
}


/* Return false if no error.  */

bfd_boolean
xg_build_token_insn (instr_spec, old_insn, new_insn)
     BuildInstr *instr_spec;
     TInsn *old_insn;
     TInsn *new_insn;
{
  int num_ops = 0;
  BuildOp *b_op;

  switch (instr_spec->typ)
    {
    case INSTR_INSTR:
      new_insn->insn_type = ITYPE_INSN;
      new_insn->opcode = instr_spec->opcode;
      new_insn->is_specific_opcode = FALSE;
      break;
    case INSTR_LITERAL_DEF:
      new_insn->insn_type = ITYPE_LITERAL;
      new_insn->opcode = XTENSA_UNDEFINED;
      new_insn->is_specific_opcode = FALSE;
      break;
    case INSTR_LABEL_DEF:
      as_bad (_("INSTR_LABEL_DEF not supported yet"));
      break;
    }

  for (b_op = instr_spec->ops; b_op != NULL; b_op = b_op->next)
    {
      expressionS *exp;
      const expressionS *src_exp;

      num_ops++;
      switch (b_op->typ)
	{
	case OP_CONSTANT:
	  /* The expression must be the constant.  */
	  assert (b_op->op_num < MAX_INSN_ARGS);
	  exp = &new_insn->tok[b_op->op_num];
	  set_expr_const (exp, b_op->op_data);
	  break;

	case OP_OPERAND:
	  assert (b_op->op_num < MAX_INSN_ARGS);
	  assert (b_op->op_data < (unsigned) old_insn->ntok);
	  src_exp = &old_insn->tok[b_op->op_data];
	  exp = &new_insn->tok[b_op->op_num];
	  copy_expr (exp, src_exp);
	  break;

	case OP_LITERAL:
	case OP_LABEL:
	  as_bad (_("can't handle generation of literal/labels yet"));
	  assert (0);

	default:
	  as_bad (_("can't handle undefined OP TYPE"));
	  assert (0);
	}
    }

  new_insn->ntok = num_ops;
  return FALSE;
}


/* Return true if it was simplified.  */

bfd_boolean
xg_simplify_insn (old_insn, new_insn)
     TInsn *old_insn;
     TInsn *new_insn;
{
  TransitionRule *rule = xg_instruction_match (old_insn);
  BuildInstr *insn_spec;
  if (rule == NULL)
    return FALSE;

  insn_spec = rule->to_instr;
  /* There should only be one.  */
  assert (insn_spec != NULL);
  assert (insn_spec->next == NULL);
  if (insn_spec->next != NULL)
    return FALSE;

  xg_build_token_insn (insn_spec, old_insn, new_insn);

  return TRUE;
}


/* xg_expand_assembly_insn: (1) Simplify the instruction, i.e., l32i ->
   l32i.n. (2) Check the number of operands.  (3) Place the instruction
   tokens into the stack or if we can relax it at assembly time, place
   multiple instructions/literals onto the stack.  Return false if no
   error.  */

static bfd_boolean
xg_expand_assembly_insn (istack, orig_insn)
     IStack *istack;
     TInsn *orig_insn;
{
  int noperands;
  TInsn new_insn;
  memset (&new_insn, 0, sizeof (TInsn));

  /* On return, we will be using the "use_tokens" with "use_ntok".
     This will reduce things like addi to addi.n.  */
  if (code_density_available () && !orig_insn->is_specific_opcode)
    {
      if (xg_simplify_insn (orig_insn, &new_insn))
	orig_insn = &new_insn;
    }

  noperands = xtensa_num_operands (xtensa_default_isa, orig_insn->opcode);
  if (orig_insn->ntok < noperands)
    {
      as_bad (_("found %d operands for '%s':  Expected %d"),
	      orig_insn->ntok,
	      xtensa_opcode_name (xtensa_default_isa, orig_insn->opcode),
	      noperands);
      return TRUE;
    }
  if (orig_insn->ntok > noperands)
    as_warn (_("found too many (%d) operands for '%s':  Expected %d"),
	     orig_insn->ntok,
	     xtensa_opcode_name (xtensa_default_isa, orig_insn->opcode),
	     noperands);

  /* If there are not enough operands, we will assert above. If there
     are too many, just cut out the extras here.  */

  orig_insn->ntok = noperands;

  /* Cases: 

     Instructions with all constant immeds:
     Assemble them and relax the instruction if possible.
     Give error if not possible; no fixup needed.

     Instructions with symbolic immeds:
     Assemble them with a Fix up (that may cause instruction expansion).
     Also close out the fragment if the fixup may cause instruction expansion. 
     
     There are some other special cases where we need alignment.
     1) before certain instructions with required alignment (OPCODE_ALIGN)
     2) before labels that have jumps (LABEL_ALIGN)
     3) after call instructions (RETURN_ALIGN)
        Multiple of these may be possible on the same fragment. 
	If so, make sure to satisfy the required alignment. 
	Then try to get the desired alignment.  */

  if (tinsn_has_invalid_symbolic_operands (orig_insn))
    return TRUE;

  if (orig_insn->is_specific_opcode || !can_relax ())
    {
      istack_push (istack, orig_insn);
      return FALSE;
    }

  if (tinsn_has_symbolic_operands (orig_insn))
    {
      if (tinsn_has_complex_operands (orig_insn))
	xg_assembly_relax (istack, orig_insn, 0, 0, 0, 0, 0);
      else
	istack_push (istack, orig_insn);
    }
  else
    {
      if (xg_immeds_fit (orig_insn))
	istack_push (istack, orig_insn);
      else
	xg_assembly_relax (istack, orig_insn, 0, 0, 0, 0, 0);
    }

#if 0
  for (i = 0; i < istack->ninsn; i++)
    {
      if (xg_simplify_insn (&new_insn, &istack->insn[i]))
	istack->insn[i] = new_insn;
    }
#endif

  return FALSE;
}


/* Currently all literals that are generated here are 32-bit L32R targets.  */

symbolS *
xg_assemble_literal (insn)
     /* const */ TInsn *insn;
{
  emit_state state;
  symbolS *lit_sym = NULL;

  /* size = 4 for L32R.  It could easily be larger when we move to
     larger constants.  Add a parameter later.  */
  offsetT litsize = 4;
  offsetT litalign = 2;		/* 2^2 = 4 */
  expressionS saved_loc;
  set_expr_symbol_offset (&saved_loc, frag_now->fr_symbol, frag_now_fix ());

  assert (insn->insn_type == ITYPE_LITERAL);
  assert (insn->ntok = 1);	/* must be only one token here */

  xtensa_switch_to_literal_fragment (&state);

  /* Force a 4-byte align here.  Note that this opens a new frag, so all
     literals done with this function have a frag to themselves.  That's
     important for the way text section literals work.  */
  frag_align (litalign, 0, 0);

  emit_expr (&insn->tok[0], litsize);

  assert (frag_now->tc_frag_data.literal_frag == NULL);
  frag_now->tc_frag_data.literal_frag = get_literal_pool_location (now_seg);
  frag_now->fr_symbol = xtensa_create_literal_symbol (now_seg, frag_now);
  lit_sym = frag_now->fr_symbol;
  frag_now->tc_frag_data.is_literal = TRUE;

  /* Go back.  */
  xtensa_restore_emit_state (&state);
  return lit_sym;
}


static void
xg_assemble_literal_space (size)
     /* const */ int size;
{
  emit_state state;
  /* We might have to do something about this alignment.  It only  
     takes effect if something is placed here.  */
  offsetT litalign = 2;		/* 2^2 = 4 */
  fragS *lit_saved_frag;

  expressionS saved_loc;

  assert (size % 4 == 0);
  set_expr_symbol_offset (&saved_loc, frag_now->fr_symbol, frag_now_fix ());

  xtensa_switch_to_literal_fragment (&state);

  /* Force a 4-byte align here.  */
  frag_align (litalign, 0, 0);

  xg_force_frag_space (size);

  lit_saved_frag = frag_now;
  frag_now->tc_frag_data.literal_frag = get_literal_pool_location (now_seg);
  frag_now->tc_frag_data.is_literal = TRUE;
  frag_now->fr_symbol = xtensa_create_literal_symbol (now_seg, frag_now);
  xg_finish_frag (0, RELAX_LITERAL, size, FALSE);

  /* Go back.  */
  xtensa_restore_emit_state (&state);
  frag_now->tc_frag_data.literal_frag = lit_saved_frag;
}


symbolS *
xtensa_create_literal_symbol (sec, frag)
     segT sec;
     fragS *frag;
{
  static int lit_num = 0;
  static char name[256];
  symbolS *symbolP;

  sprintf (name, ".L_lit_sym%d", lit_num);

  /* Create a local symbol.  If it is in a linkonce section, we have to
     be careful to make sure that if it is used in a relocation that the
     symbol will be in the output file.  */
  if (get_is_linkonce_section (stdoutput, sec))
    {
      symbolP = symbol_new (name, sec, 0, frag);
      S_CLEAR_EXTERNAL (symbolP);
      /* symbolP->local = 1; */
    }
  else
    symbolP = symbol_new (name, sec, 0, frag);

  xtensa_add_literal_sym (symbolP);

  frag->tc_frag_data.is_literal = TRUE;
  lit_num++;
  return symbolP;
}


static void
xtensa_add_literal_sym (sym)
     symbolS *sym;
{
  sym_list *l;

  l = (sym_list *) xmalloc (sizeof (sym_list));
  l->sym = sym;
  l->next = literal_syms;
  literal_syms = l;
}


static void
xtensa_add_insn_label (sym)
     symbolS *sym;
{
  sym_list *l;

  if (!free_insn_labels)
    l = (sym_list *) xmalloc (sizeof (sym_list));
  else
    {
      l = free_insn_labels;
      free_insn_labels = l->next;
    }

  l->sym = sym;
  l->next = insn_labels;
  insn_labels = l;
}


static void
xtensa_clear_insn_labels (void)
{
  sym_list **pl;

  for (pl = &free_insn_labels; *pl != NULL; pl = &(*pl)->next)
    ;
  *pl = insn_labels;
  insn_labels = NULL;
}


/* Return true if the section flags are marked linkonce
   or the name is .gnu.linkonce*.  */

bfd_boolean
get_is_linkonce_section (abfd, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec;
{
  flagword flags, link_once_flags;

  flags = bfd_get_section_flags (abfd, sec);
  link_once_flags = (flags & SEC_LINK_ONCE);

  /* Flags might not be set yet.  */
  if (!link_once_flags)
    {
      static size_t len = sizeof ".gnu.linkonce.t.";

      if (strncmp (segment_name (sec), ".gnu.linkonce.t.", len - 1) == 0)
	link_once_flags = SEC_LINK_ONCE;
    }
  return (link_once_flags != 0);
}


/* Emit an instruction to the current fragment.  If record_fix is true,
   then this instruction will not change and we can go ahead and record
   the fixup.  If record_fix is false, then the instruction may change
   and we are going to close out this fragment.  Go ahead and set the
   fr_symbol and fr_offset instead of adding a fixup.  */

static bfd_boolean
xg_emit_insn (t_insn, record_fix)
     TInsn *t_insn;
     bfd_boolean record_fix;
{
  bfd_boolean ok = TRUE;
  xtensa_isa isa = xtensa_default_isa;
  xtensa_opcode opcode = t_insn->opcode;
  bfd_boolean has_fixup = FALSE;
  int noperands;
  int i, byte_count;
  fragS *oldfrag;
  size_t old_size;
  char *f;
  static xtensa_insnbuf insnbuf = NULL;
  
  /* Use a static pointer to the insn buffer so we don't have to call 
     malloc each time through.  */
  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  has_fixup = tinsn_to_insnbuf (t_insn, insnbuf);

  noperands = xtensa_num_operands (isa, opcode);
  assert (noperands == t_insn->ntok);

  byte_count = xtensa_insn_length (isa, opcode);
  oldfrag = frag_now;
  /* This should NEVER cause us to jump into a new frag;
     we've already reserved space.  */
  old_size = frag_now_fix ();
  f = frag_more (byte_count);
  assert (oldfrag == frag_now);

  /* This needs to generate a record that lists the parts that are
     instructions.  */
  if (!frag_now->tc_frag_data.is_insn)
    {
      /* If we are at the beginning of a fragment, switch this
	 fragment to an instruction fragment.  */
      if (now_seg != absolute_section && old_size != 0)
	as_warn (_("instruction fragment may contain data"));
      frag_now->tc_frag_data.is_insn = TRUE;
    }

  xtensa_insnbuf_to_chars (isa, insnbuf, f);

  dwarf2_emit_insn (byte_count);

  /* Now spit out the opcode fixup.... */
  if (!has_fixup)
    return !ok;

  for (i = 0; i < noperands; ++i)
    {
      expressionS *expr = &t_insn->tok[i];
      switch (expr->X_op)
	{
	case O_symbol:
	  if (get_relaxable_immed (opcode) == i)
	    {
	      if (record_fix)
		{
		  if (!xg_add_opcode_fix (opcode, i, expr, frag_now,
					  f - frag_now->fr_literal))
		    ok = FALSE;
		}
	      else
		{
		  /* Write it to the fr_offset, fr_symbol.  */
		  frag_now->fr_symbol = expr->X_add_symbol;
		  frag_now->fr_offset = expr->X_add_number;
		}
	    }
	  else
	    {
	      as_bad (_("invalid operand %d on '%s'"),
		      i, xtensa_opcode_name (isa, opcode));
	      ok = FALSE;
	    }
	  break;

	case O_constant:
	case O_register:
	  break;

	default:
	  as_bad (_("invalid expression for operand %d on '%s'"),
		  i, xtensa_opcode_name (isa, opcode));
	  ok = FALSE;
	  break;
	}
    }

  return !ok;
}


static bfd_boolean
xg_emit_insn_to_buf (t_insn, buf, fragP, offset, build_fix)
     TInsn *t_insn;
     char *buf;
     fragS *fragP;
     offsetT offset;
     bfd_boolean build_fix;
{
  static xtensa_insnbuf insnbuf = NULL;
  bfd_boolean has_symbolic_immed = FALSE;
  bfd_boolean ok = TRUE;
  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  has_symbolic_immed = tinsn_to_insnbuf (t_insn, insnbuf);
  if (has_symbolic_immed && build_fix)
    {
      /* Add a fixup.  */
      int opnum = get_relaxable_immed (t_insn->opcode);
      expressionS *exp = &t_insn->tok[opnum];

      if (!xg_add_opcode_fix (t_insn->opcode, 
			      opnum, exp, fragP, offset))
	ok = FALSE;
    }
  fragP->tc_frag_data.is_insn = TRUE;
  xtensa_insnbuf_to_chars (xtensa_default_isa, insnbuf, buf);
  return ok;
}


/* Put in a fixup record based on the opcode.
   Return true on success.  */

bfd_boolean
xg_add_opcode_fix (opcode, opnum, expr, fragP, offset)
     xtensa_opcode opcode;
     int opnum;
     expressionS *expr;
     fragS *fragP;
     offsetT offset;
{ 
  bfd_reloc_code_real_type reloc; 
  reloc_howto_type *howto; 
  int insn_length;
  fixS *the_fix;

  reloc = opnum_to_reloc (opnum);
  if (reloc == BFD_RELOC_NONE)
    {
      as_bad (_("invalid relocation operand %i on '%s'"),
	      opnum, xtensa_opcode_name (xtensa_default_isa, opcode));
      return FALSE;
    }

  howto = bfd_reloc_type_lookup (stdoutput, reloc);

  if (!howto)
    {
      as_bad (_("undefined symbol for opcode \"%s\"."),
	      xtensa_opcode_name (xtensa_default_isa, opcode));
      return FALSE;
    }

  insn_length = xtensa_insn_length (xtensa_default_isa, opcode);
  the_fix = fix_new_exp (fragP, offset, insn_length, expr,
			 howto->pc_relative, reloc);

  if (expr->X_add_symbol && 
      (S_IS_EXTERNAL (expr->X_add_symbol) || S_IS_WEAK (expr->X_add_symbol)))
    the_fix->fx_plt = TRUE;
  
  return TRUE;
}


void
xg_resolve_literals (insn, lit_sym)
     TInsn *insn;
     symbolS *lit_sym;
{
  symbolS *sym = get_special_literal_symbol ();
  int i;
  if (lit_sym == 0)
    return;
  assert (insn->insn_type == ITYPE_INSN);
  for (i = 0; i < insn->ntok; i++)
    if (insn->tok[i].X_add_symbol == sym)
      insn->tok[i].X_add_symbol = lit_sym;

}


void
xg_resolve_labels (insn, label_sym)
     TInsn *insn;
     symbolS *label_sym;
{
  symbolS *sym = get_special_label_symbol ();
  int i;
  /* assert(!insn->is_literal); */
  for (i = 0; i < insn->ntok; i++)
    if (insn->tok[i].X_add_symbol == sym)
      insn->tok[i].X_add_symbol = label_sym;

}


static void
xg_assemble_tokens (insn)
     /*const */ TInsn *insn;
{
  /* By the time we get here, there's not too much left to do. 
     1) Check our assumptions. 
     2) Check if the current instruction is "narrow". 
        If so, then finish the frag, create another one.
        We could also go back to change some previous
        "narrow" frags into no-change ones if we have more than
        MAX_NARROW_ALIGNMENT of them without alignment restrictions
        between them.

     Cases:
        1) It has constant operands and doesn't fit.
           Go ahead and assemble it so it will fail.
        2) It has constant operands that fit.
           If narrow and !is_specific_opcode,
              assemble it and put in a relocation
           else
              assemble it.
        3) It has a symbolic immediate operand
           a) Find the worst-case relaxation required
           b) Find the worst-case literal pool space required.
              Insert appropriate alignment & space in the literal.
              Assemble it.
              Add the relocation.  */

  assert (insn->insn_type == ITYPE_INSN);

  if (!tinsn_has_symbolic_operands (insn))
    {
      if (xg_is_narrow_insn (insn) && !insn->is_specific_opcode)
	{
	  /* assemble it but add max required space */
	  int max_size = xg_get_max_narrow_insn_size (insn->opcode);
	  int min_size = xg_get_insn_size (insn);
	  char *last_insn;
	  assert (max_size == 3);
	  /* make sure we have enough space to widen it */
	  xg_force_frag_space (max_size);
	  /* Output the instruction.  It may cause an error if some 
	     operands do not fit.  */
	  last_insn = frag_more (0);
	  if (xg_emit_insn (insn, TRUE))
	    as_warn (_("instruction with constant operands does not fit"));
	  xg_finish_frag (last_insn, RELAX_NARROW, max_size - min_size, TRUE);
	}
      else
	{
	  /* Assemble it.  No relocation needed.  */
	  int max_size = xg_get_insn_size (insn);
	  xg_force_frag_space (max_size);
	  if (xg_emit_insn (insn, FALSE))
	    as_warn (_("instruction with constant operands does not "
		       "fit without widening"));
	  /* frag_more (max_size); */

	  /* Special case for jx.  If the jx is the next to last
	     instruction in a loop, we will add a NOP after it.  This
	     avoids a hardware issue that could occur if the jx jumped
	     to the next instruction.  */
	  if (software_avoid_b_j_loop_end
	      && is_jx_opcode (insn->opcode))
	    {
	      maybe_has_b_j_loop_end = TRUE;
	      /* add 2 of these */
	      frag_now->tc_frag_data.is_insn = TRUE;
	      frag_var (rs_machine_dependent, 4, 4,
			RELAX_ADD_NOP_IF_PRE_LOOP_END,
			frag_now->fr_symbol, frag_now->fr_offset, NULL);
	    }
	}
    }
  else
    {
      /* Need to assemble it with space for the relocation.  */
      if (!insn->is_specific_opcode)
	{
	  /* Assemble it but add max required space.  */
	  char *last_insn;
	  int min_size = xg_get_insn_size (insn);
	  int max_size = xg_get_max_insn_widen_size (insn->opcode);
	  int max_literal_size =
	    xg_get_max_insn_widen_literal_size (insn->opcode);

#if 0
	  symbolS *immed_sym = xg_get_insn_immed_symbol (insn);
	  set_frag_segment (frag_now, now_seg);
#endif /* 0 */

	  /* Make sure we have enough space to widen the instruction. 
	     This may open a new fragment.  */
	  xg_force_frag_space (max_size);
	  if (max_literal_size != 0)
	    xg_assemble_literal_space (max_literal_size);

	  /* Output the instruction.  It may cause an error if some 
	     operands do not fit.  Emit the incomplete instruction.  */
	  last_insn = frag_more (0);
	  xg_emit_insn (insn, FALSE);

	  xg_finish_frag (last_insn, RELAX_IMMED, max_size - min_size, TRUE);

	  /* Special cases for loops:
	     close_loop_end should be inserted AFTER short_loop.
	     Make sure that CLOSE loops are processed BEFORE short_loops
	     when converting them.  */

	  /* "short_loop": add a NOP if the loop is < 4 bytes.  */
	  if (software_avoid_short_loop
	      && is_loop_opcode (insn->opcode))
	    {
	      maybe_has_short_loop = TRUE;
	      frag_now->tc_frag_data.is_insn = TRUE;
	      frag_var (rs_machine_dependent, 4, 4,
			RELAX_ADD_NOP_IF_SHORT_LOOP,
			frag_now->fr_symbol, frag_now->fr_offset, NULL);
	      frag_now->tc_frag_data.is_insn = TRUE;
	      frag_var (rs_machine_dependent, 4, 4,
			RELAX_ADD_NOP_IF_SHORT_LOOP,
			frag_now->fr_symbol, frag_now->fr_offset, NULL);
	    }

	  /* "close_loop_end": Add up to 12 bytes of NOPs to keep a
	     loop at least 12 bytes away from another loop's loop
	     end.  */
	  if (software_avoid_close_loop_end
	      && is_loop_opcode (insn->opcode))
	    {
	      maybe_has_close_loop_end = TRUE;
	      frag_now->tc_frag_data.is_insn = TRUE;
	      frag_var (rs_machine_dependent, 12, 12,
			RELAX_ADD_NOP_IF_CLOSE_LOOP_END,
			frag_now->fr_symbol, frag_now->fr_offset, NULL);
	    }
	}
      else
	{
	  /* Assemble it in place.  No expansion will be required, 
	     but we'll still need a relocation record.  */
	  int max_size = xg_get_insn_size (insn);
	  xg_force_frag_space (max_size);
	  if (xg_emit_insn (insn, TRUE))
	    as_warn (_("instruction's constant operands do not fit"));
	}
    }
}


/* Return true if the instruction can write to the specified
   integer register.  */

static bfd_boolean
is_register_writer (insn, regset, regnum)
     const TInsn *insn;
     const char *regset;
     int regnum;
{
  int i;
  int num_ops;
  xtensa_isa isa = xtensa_default_isa;

  num_ops = xtensa_num_operands (isa, insn->opcode);

  for (i = 0; i < num_ops; i++)
    {
      xtensa_operand operand = xtensa_get_operand (isa, insn->opcode, i);
      char inout = xtensa_operand_inout (operand);

      if (inout == '>' || inout == '=')
	{
	  if (strcmp (xtensa_operand_kind (operand), regset) == 0)
	    {
	      if ((insn->tok[i].X_op == O_register)
		  && (insn->tok[i].X_add_number == regnum))
		return TRUE;
	    }
	}
    }
  return FALSE;
}


static bfd_boolean
is_bad_loopend_opcode (tinsn)
     const TInsn * tinsn;
{
  xtensa_opcode opcode = tinsn->opcode;

  if (opcode == XTENSA_UNDEFINED)
    return FALSE;

  if (opcode == xtensa_call0_opcode
      || opcode == xtensa_callx0_opcode
      || opcode == xtensa_call4_opcode
      || opcode == xtensa_callx4_opcode
      || opcode == xtensa_call8_opcode
      || opcode == xtensa_callx8_opcode
      || opcode == xtensa_call12_opcode
      || opcode == xtensa_callx12_opcode
      || opcode == xtensa_isync_opcode
      || opcode == xtensa_ret_opcode
      || opcode == xtensa_ret_n_opcode
      || opcode == xtensa_retw_opcode
      || opcode == xtensa_retw_n_opcode
      || opcode == xtensa_waiti_opcode)
    return TRUE;
  
  /* An RSR of LCOUNT is illegal as the last opcode in a loop.  */
  if (opcode == xtensa_rsr_opcode
      && tinsn->ntok >= 2
      && tinsn->tok[1].X_op == O_constant
      && tinsn->tok[1].X_add_number == 2)
    return TRUE;

  return FALSE;
}


/* Labels that begin with ".Ln" or ".LM"  are unaligned.
   This allows the debugger to add unaligned labels.
   Also, the assembler generates stabs labels that need
   not be aligned:  FAKE_LABEL_NAME . {"F", "L", "endfunc"}.  */

bfd_boolean
is_unaligned_label (sym) 
     symbolS *sym; 
{
  const char *name = S_GET_NAME (sym);
  static size_t fake_size = 0;

  if (name
      && name[0] == '.'
      && name[1] == 'L' && (name[2] == 'n' || name[2] == 'M'))
    return TRUE;

  /* FAKE_LABEL_NAME followed by "F", "L" or "endfunc" */
  if (fake_size == 0)
    fake_size = strlen (FAKE_LABEL_NAME);

  if (name 
      && strncmp (FAKE_LABEL_NAME, name, fake_size) == 0
      && (name[fake_size] == 'F'
	  || name[fake_size] == 'L'
	  || (name[fake_size] == 'e'
	      && strncmp ("endfunc", name+fake_size, 7) == 0)))
    return TRUE;

  return FALSE;
}


fragS *
next_non_empty_frag (fragP)
     const fragS *fragP;
{
  fragS *next_fragP = fragP->fr_next;

  /* Sometimes an empty will end up here due storage allocation issues. 
     So we have to skip until we find something legit.  */
  while (next_fragP && next_fragP->fr_fix == 0)
    next_fragP = next_fragP->fr_next;

  if (next_fragP == NULL || next_fragP->fr_fix == 0)
    return NULL;

  return next_fragP;
}


xtensa_opcode
next_frag_opcode (fragP)
     const fragS * fragP;
{
  const fragS *next_fragP = next_non_empty_frag (fragP);
  static xtensa_insnbuf insnbuf = NULL;
  xtensa_isa isa = xtensa_default_isa;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (isa);

  if (next_fragP == NULL)
    return XTENSA_UNDEFINED;

  xtensa_insnbuf_from_chars (isa, insnbuf, next_fragP->fr_literal);
  return xtensa_decode_insn (isa, insnbuf);
}


/* Return true if the target frag is one of the next non-empty frags.  */

bfd_boolean
is_next_frag_target (fragP, target)
     const fragS *fragP;
     const fragS *target;
{
  if (fragP == NULL)
    return FALSE;

  for (; fragP; fragP = fragP->fr_next)
    {
      if (fragP == target)
	return TRUE;
      if (fragP->fr_fix != 0)
	return FALSE;
      if (fragP->fr_type == rs_fill && fragP->fr_offset != 0)
	return FALSE;
      if ((fragP->fr_type == rs_align || fragP->fr_type == rs_align_code)
	  && ((fragP->fr_address % (1 << fragP->fr_offset)) != 0))
	return FALSE;
      if (fragP->fr_type == rs_space)
	return FALSE;
    }
  return FALSE;
}


/* If the next legit fragment is an end-of-loop marker,
   switch its state so it will instantiate a NOP.  */

static void
update_next_frag_nop_state (fragP)
     fragS *fragP;
{
  fragS *next_fragP = fragP->fr_next;

  while (next_fragP && next_fragP->fr_fix == 0)
    {
      if (next_fragP->fr_type == rs_machine_dependent
	  && next_fragP->fr_subtype == RELAX_LOOP_END)
	{
	  next_fragP->fr_subtype = RELAX_LOOP_END_ADD_NOP;
	  return;
	}
      next_fragP = next_fragP->fr_next;
    }
}


static bfd_boolean
next_frag_is_branch_target (fragP)
     const fragS *fragP;
{
  /* Sometimes an empty will end up here due storage allocation issues,
     so we have to skip until we find something legit.  */
  for (fragP = fragP->fr_next; fragP; fragP = fragP->fr_next)
    {
      if (fragP->tc_frag_data.is_branch_target)
	return TRUE;
      if (fragP->fr_fix != 0)
	break;
    }
  return FALSE;
}


static bfd_boolean
next_frag_is_loop_target (fragP)
     const fragS *fragP;
{
  /* Sometimes an empty will end up here due storage allocation issues. 
     So we have to skip until we find something legit. */
  for (fragP = fragP->fr_next; fragP; fragP = fragP->fr_next)
    {
      if (fragP->tc_frag_data.is_loop_target)
	return TRUE;
      if (fragP->fr_fix != 0)
	break;
    }
  return FALSE;
}


static addressT
next_frag_pre_opcode_bytes (fragp)
     const fragS *fragp;
{
  const fragS *next_fragp = fragp->fr_next;

  xtensa_opcode next_opcode = next_frag_opcode (fragp);
  if (!is_loop_opcode (next_opcode))
    return 0;

  /* Sometimes an empty will end up here due storage allocation issues.
     So we have to skip until we find something legit.  */
  while (next_fragp->fr_fix == 0)
    next_fragp = next_fragp->fr_next;

  if (next_fragp->fr_type != rs_machine_dependent)
    return 0;

  /* There is some implicit knowledge encoded in here.
     The LOOP instructions that are NOT RELAX_IMMED have
     been relaxed.  */
  if (next_fragp->fr_subtype > RELAX_IMMED)
      return get_expanded_loop_offset (next_opcode);

  return 0;
}


/* Mark a location where we can later insert literal frags.  Update
   the section's literal_pool_loc, so subsequent literals can be
   placed nearest to their use.  */

static void
xtensa_mark_literal_pool_location ()
{
  /* Any labels pointing to the current location need
     to be adjusted to after the literal pool.  */
  emit_state s;
  fragS *pool_location;

  frag_align (2, 0, 0);

  /* We stash info in the fr_var of these frags
     so we can later move the literal's fixes into this 
     frchain's fix list.  We can use fr_var because fr_var's
     interpretation depends solely on the fr_type and subtype.  */
  pool_location = frag_now;
  frag_variant (rs_machine_dependent, 0, (int) frchain_now, 
		RELAX_LITERAL_POOL_BEGIN, NULL, 0, NULL);
  frag_variant (rs_machine_dependent, 0, (int) now_seg, 
		RELAX_LITERAL_POOL_END, NULL, 0, NULL);

  /* Now put a frag into the literal pool that points to this location.  */
  set_literal_pool_location (now_seg, pool_location);
  xtensa_switch_to_literal_fragment (&s);

  /* Close whatever frag is there.  */
  frag_variant (rs_fill, 0, 0, 0, NULL, 0, NULL);
  frag_now->tc_frag_data.literal_frag = pool_location;
  frag_variant (rs_fill, 0, 0, 0, NULL, 0, NULL);
  xtensa_restore_emit_state (&s);
}


/* The "loops_ok" argument is provided to allow ignoring labels that 
   define loop ends.  This fixes a bug where the NOPs to align a 
   loop opcode were included in a previous zero-cost loop:

   loop a0, loopend
     <loop1 body>
   loopend:

   loop a2, loopend2
     <loop2 body>

   would become:

   loop a0, loopend
     <loop1 body>
     nop.n <===== bad!
   loopend:

   loop a2, loopend2
     <loop2 body>

   This argument is used to prevent moving the NOP to before the
   loop-end label, which is what you want in this special case.  */

static void
xtensa_move_labels (new_frag, new_offset, loops_ok)
     fragS *new_frag;
     valueT new_offset;
     bfd_boolean loops_ok;
{
  sym_list *lit;

  for (lit = insn_labels; lit; lit = lit->next)
    {
      symbolS *lit_sym = lit->sym;
      if (loops_ok || symbol_get_tc (lit_sym)->is_loop_target == 0)
	{
	  S_SET_VALUE (lit_sym, new_offset);
	  symbol_set_frag (lit_sym, new_frag);
	}
    }
}


/* Assemble a NOP of the requested size in the buffer.  User must have
   allocated "buf" with at least "size" bytes.  */

void
assemble_nop (size, buf)
     size_t size;
     char *buf;
{
  static xtensa_insnbuf insnbuf = NULL;
  TInsn t_insn;
  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  tinsn_init (&t_insn);
  switch (size)
    {
    case 2:
      t_insn.opcode = xtensa_nop_n_opcode;
      t_insn.ntok = 0;
      if (t_insn.opcode == XTENSA_UNDEFINED)
	as_fatal (_("opcode 'NOP.N' unavailable in this configuration"));
      tinsn_to_insnbuf (&t_insn, insnbuf);
      xtensa_insnbuf_to_chars (xtensa_default_isa, insnbuf, buf);
      break;

    case 3:
      t_insn.opcode = xtensa_or_opcode;
      assert (t_insn.opcode != XTENSA_UNDEFINED);
      if (t_insn.opcode == XTENSA_UNDEFINED)
	as_fatal (_("opcode 'OR' unavailable in this configuration"));
      set_expr_const (&t_insn.tok[0], 1);
      set_expr_const (&t_insn.tok[1], 1);
      set_expr_const (&t_insn.tok[2], 1);
      t_insn.ntok = 3;
      tinsn_to_insnbuf (&t_insn, insnbuf);
      xtensa_insnbuf_to_chars (xtensa_default_isa, insnbuf, buf);
      break;

    default:
      as_fatal (_("invalid %d-byte NOP requested"), size);
    }
}


/* Return the number of bytes for the offset of the expanded loop
   instruction.  This should be incorporated into the relaxation
   specification but is hard-coded here.  This is used to auto-align
   the loop instruction.  It is invalid to call this function if the
   configuration does not have loops or if the opcode is not a loop
   opcode.  */

static addressT
get_expanded_loop_offset (opcode)
     xtensa_opcode opcode;
{
  /* This is the OFFSET of the loop instruction in the expanded loop.
     This MUST correspond directly to the specification of the loop
     expansion.  It will be validated on fragment conversion.  */
  if (opcode == XTENSA_UNDEFINED)
    as_fatal (_("get_expanded_loop_offset: undefined opcode"));
  if (opcode == xtensa_loop_opcode)
    return 0;
  if (opcode == xtensa_loopnez_opcode)
    return 3;
  if (opcode == xtensa_loopgtz_opcode)
    return 6;
  as_fatal (_("get_expanded_loop_offset: invalid opcode"));
  return 0;
}


fragS *
get_literal_pool_location (seg)
     segT seg;
{
  return seg_info (seg)->tc_segment_info_data.literal_pool_loc;
}


static void
set_literal_pool_location (seg, literal_pool_loc)
     segT seg;
     fragS *literal_pool_loc;
{
  seg_info (seg)->tc_segment_info_data.literal_pool_loc = literal_pool_loc;
}


/* External Functions and Other GAS Hooks.  */

const char *
xtensa_target_format ()
{
  return (target_big_endian ? "elf32-xtensa-be" : "elf32-xtensa-le");
}


void
xtensa_file_arch_init (abfd)
     bfd *abfd;
{
  bfd_set_private_flags (abfd, 0x100 | 0x200);
}


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


/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will
   need.  */

void
md_begin ()
{
  segT current_section = now_seg;
  int current_subsec = now_subseg;
  xtensa_isa isa;

#if STATIC_LIBISA
  isa = xtensa_isa_init ();
#else
  /* ISA was already initialized by xtensa_init().  */
  isa = xtensa_default_isa;
#endif

  /* Set  up the .literal, .fini.literal and .init.literal sections.  */
  memset (&default_lit_sections, 0, sizeof (default_lit_sections));
  default_lit_sections.init_lit_seg_name = INIT_LITERAL_SECTION_NAME;
  default_lit_sections.fini_lit_seg_name = FINI_LITERAL_SECTION_NAME;
  default_lit_sections.lit_seg_name = LITERAL_SECTION_NAME;

  subseg_set (current_section, current_subsec);

  xtensa_addi_opcode = xtensa_opcode_lookup (isa, "addi");
  xtensa_addmi_opcode = xtensa_opcode_lookup (isa, "addmi");
  xtensa_call0_opcode = xtensa_opcode_lookup (isa, "call0");
  xtensa_call4_opcode = xtensa_opcode_lookup (isa, "call4");
  xtensa_call8_opcode = xtensa_opcode_lookup (isa, "call8");
  xtensa_call12_opcode = xtensa_opcode_lookup (isa, "call12");
  xtensa_callx0_opcode = xtensa_opcode_lookup (isa, "callx0");
  xtensa_callx4_opcode = xtensa_opcode_lookup (isa, "callx4");
  xtensa_callx8_opcode = xtensa_opcode_lookup (isa, "callx8");
  xtensa_callx12_opcode = xtensa_opcode_lookup (isa, "callx12");
  xtensa_entry_opcode = xtensa_opcode_lookup (isa, "entry");
  xtensa_isync_opcode = xtensa_opcode_lookup (isa, "isync");
  xtensa_j_opcode = xtensa_opcode_lookup (isa, "j");
  xtensa_jx_opcode = xtensa_opcode_lookup (isa, "jx");
  xtensa_loop_opcode = xtensa_opcode_lookup (isa, "loop");
  xtensa_loopnez_opcode = xtensa_opcode_lookup (isa, "loopnez");
  xtensa_loopgtz_opcode = xtensa_opcode_lookup (isa, "loopgtz");
  xtensa_nop_n_opcode = xtensa_opcode_lookup (isa, "nop.n");
  xtensa_or_opcode = xtensa_opcode_lookup (isa, "or");
  xtensa_ret_opcode = xtensa_opcode_lookup (isa, "ret");
  xtensa_ret_n_opcode = xtensa_opcode_lookup (isa, "ret.n");
  xtensa_retw_opcode = xtensa_opcode_lookup (isa, "retw");
  xtensa_retw_n_opcode = xtensa_opcode_lookup (isa, "retw.n");
  xtensa_rsr_opcode = xtensa_opcode_lookup (isa, "rsr");
  xtensa_waiti_opcode = xtensa_opcode_lookup (isa, "waiti");
}


/* tc_frob_label hook */

void
xtensa_frob_label (sym)
     symbolS *sym;
{
  if (generating_literals)
    xtensa_add_literal_sym (sym);
  else
    xtensa_add_insn_label (sym);

  if (symbol_get_tc (sym)->is_loop_target
      && (get_last_insn_flags (now_seg, now_subseg)
	  & FLAG_IS_BAD_LOOPEND) != 0)
    as_bad (_("invalid last instruction for a zero-overhead loop"));

  /* No target aligning in the absolute section.  */
  if (now_seg != absolute_section
      && align_targets
      && !is_unaligned_label (sym)
      && !frag_now->tc_frag_data.is_literal)
    {
      /* frag_now->tc_frag_data.is_insn = TRUE; */
      frag_var (rs_machine_dependent, 4, 4,
		RELAX_DESIRE_ALIGN_IF_TARGET,
		frag_now->fr_symbol, frag_now->fr_offset, NULL);
      xtensa_move_labels (frag_now, 0, TRUE);

      /* If the label is already known to be a branch target, i.e., a
	 forward branch, mark the frag accordingly.  Backward branches
	 are handled by xg_add_branch_and_loop_targets.  */
      if (symbol_get_tc (sym)->is_branch_target)
	symbol_get_frag (sym)->tc_frag_data.is_branch_target = TRUE;

      /* Loops only go forward, so they can be identified here.  */
      if (symbol_get_tc (sym)->is_loop_target)
	symbol_get_frag (sym)->tc_frag_data.is_loop_target = TRUE;
    }
}


/* md_flush_pending_output hook */

void
xtensa_flush_pending_output ()
{
  /* If there is a non-zero instruction fragment, close it.  */
  if (frag_now_fix () != 0 && frag_now->tc_frag_data.is_insn)
    {
      frag_wane (frag_now);
      frag_new (0);
    }
  frag_now->tc_frag_data.is_insn = FALSE;

  xtensa_clear_insn_labels ();
}


void
md_assemble (str)
     char *str;
{
  xtensa_isa isa = xtensa_default_isa;
  char *opname;
  unsigned opnamelen;
  bfd_boolean has_underbar = FALSE;
  char *arg_strings[MAX_INSN_ARGS]; 
  int num_args;
  IStack istack;		/* Put instructions into here.  */
  TInsn orig_insn;		/* Original instruction from the input.  */
  int i;
  symbolS *lit_sym = NULL;

  if (frag_now->tc_frag_data.is_literal)
    {
      static bfd_boolean reported = 0;
      if (reported < 4)
	as_bad (_("cannot assemble '%s' into a literal fragment"), str);
      if (reported == 3)
	as_bad (_("..."));
      reported++;
      return;
    }

  istack_init (&istack);
  tinsn_init (&orig_insn);

  /* Split off the opcode.  */
  opnamelen = strspn (str, "abcdefghijklmnopqrstuvwxyz_/0123456789.");
  opname = xmalloc (opnamelen + 1);
  memcpy (opname, str, opnamelen);
  opname[opnamelen] = '\0';

  num_args = tokenize_arguments (arg_strings, str + opnamelen);
  if (num_args == -1)
    {
      as_bad (_("syntax error"));
      return;
    }

  if (xg_translate_idioms (&opname, &num_args, arg_strings))
    return;

  /* Check for an underbar prefix.  */
  if (*opname == '_')
    {
      has_underbar = TRUE;
      opname += 1;
    }

  orig_insn.insn_type = ITYPE_INSN;
  orig_insn.ntok = 0;
  orig_insn.is_specific_opcode = (has_underbar || !use_generics ());
  specific_opcode = orig_insn.is_specific_opcode;

  orig_insn.opcode = xtensa_opcode_lookup (isa, opname);
  if (orig_insn.opcode == XTENSA_UNDEFINED)
    {
      as_bad (_("unknown opcode %s"), opname);
      return;
    }

  if (frag_now_fix () != 0 && !frag_now->tc_frag_data.is_insn)
    {
      frag_wane (frag_now);
      frag_new (0);
    }

  if (software_a0_b_retw_interlock)
    {
      if ((get_last_insn_flags (now_seg, now_subseg) & FLAG_IS_A0_WRITER) != 0
	  && is_conditional_branch_opcode (orig_insn.opcode))
	{
	  has_a0_b_retw = TRUE;

	  /* Mark this fragment with the special RELAX_ADD_NOP_IF_A0_B_RETW.
	     After the first assembly pass we will check all of them and
	     add a nop if needed.  */
	  frag_now->tc_frag_data.is_insn = TRUE;
	  frag_var (rs_machine_dependent, 4, 4,
		    RELAX_ADD_NOP_IF_A0_B_RETW,
		    frag_now->fr_symbol, frag_now->fr_offset, NULL);
	  frag_now->tc_frag_data.is_insn = TRUE;
	  frag_var (rs_machine_dependent, 4, 4,
		    RELAX_ADD_NOP_IF_A0_B_RETW,
		    frag_now->fr_symbol, frag_now->fr_offset, NULL);
	}
    }

  /* Special case: The call instructions should be marked "specific opcode"
     to keep them from expanding.  */
  if (!use_longcalls () && is_direct_call_opcode (orig_insn.opcode))
    orig_insn.is_specific_opcode = TRUE;

  /* Parse the arguments.  */
  if (parse_arguments (&orig_insn, num_args, arg_strings))
    {
      as_bad (_("syntax error"));
      return;
    }

  /* Free the opcode and argument strings, now that they've been parsed.  */
  free (has_underbar ? opname - 1 : opname);
  opname = 0;
  while (num_args-- > 0)
    free (arg_strings[num_args]);

  /* Check for the right number and type of arguments.  */
  if (tinsn_check_arguments (&orig_insn))
    return;

  /* See if the instruction implies an aligned section.  */
  if (is_entry_opcode (orig_insn.opcode) || is_loop_opcode (orig_insn.opcode))
    record_alignment (now_seg, 2);

  xg_add_branch_and_loop_targets (&orig_insn);

  /* Special cases for instructions that force an alignment... */
  if (!orig_insn.is_specific_opcode && is_loop_opcode (orig_insn.opcode))
    {
      size_t max_fill;

      frag_now->tc_frag_data.is_insn = TRUE;
      frag_now->tc_frag_data.is_no_density = !code_density_available ();
      max_fill = get_text_align_max_fill_size
	(get_text_align_power (XTENSA_FETCH_WIDTH),
	 TRUE, frag_now->tc_frag_data.is_no_density);
      frag_var (rs_machine_dependent, max_fill, max_fill,
		RELAX_ALIGN_NEXT_OPCODE, frag_now->fr_symbol,
		frag_now->fr_offset, NULL);

      xtensa_move_labels (frag_now, 0, FALSE);
    }

  /* Special-case for "entry" instruction.  */
  if (is_entry_opcode (orig_insn.opcode))
    {
      /* Check that the second opcode (#1) is >= 16.  */
      if (orig_insn.ntok >= 2)
	{
	  expressionS *exp = &orig_insn.tok[1];
	  switch (exp->X_op)
	    {
	    case O_constant:
	      if (exp->X_add_number < 16)
		as_warn (_("entry instruction with stack decrement < 16"));
	      break;

	    default:
	      as_warn (_("entry instruction with non-constant decrement"));
	    }
	}

      if (!orig_insn.is_specific_opcode)
	{
	  xtensa_mark_literal_pool_location ();

	  /* Automatically align ENTRY instructions.  */
	  xtensa_move_labels (frag_now, 0, TRUE);
	  frag_align (2, 0, 0);
	}
    }

  /* Any extra alignment frags have been inserted now, and we're about to
     emit a new instruction so clear the list of labels.  */
  xtensa_clear_insn_labels ();

  if (software_a0_b_retw_interlock)
    set_last_insn_flags (now_seg, now_subseg, FLAG_IS_A0_WRITER,
			 is_register_writer (&orig_insn, "a", 0));

  set_last_insn_flags (now_seg, now_subseg, FLAG_IS_BAD_LOOPEND,
		       is_bad_loopend_opcode (&orig_insn));

  /* Finish it off:
     assemble_tokens (opcode, tok, ntok); 
     expand the tokens from the orig_insn into the 
     stack of instructions that will not expand 
     unless required at relaxation time.  */
  if (xg_expand_assembly_insn (&istack, &orig_insn))
    return;

  for (i = 0; i < istack.ninsn; i++)
    {
      TInsn *insn = &istack.insn[i];
      if (insn->insn_type == ITYPE_LITERAL)
	{
	  assert (lit_sym == NULL);
	  lit_sym = xg_assemble_literal (insn);
	}
      else
	{
	  if (lit_sym)
	    xg_resolve_literals (insn, lit_sym);
	  xg_assemble_tokens (insn);
	}
    }

  /* Now, if the original opcode was a call... */
  if (align_targets && is_call_opcode (orig_insn.opcode))
    {
      frag_now->tc_frag_data.is_insn = TRUE;
      frag_var (rs_machine_dependent, 4, 4,
		RELAX_DESIRE_ALIGN,
		frag_now->fr_symbol,
		frag_now->fr_offset,
		NULL);
    }
}


/* TC_CONS_FIX_NEW hook: Check for "@PLT" suffix on symbol references.
   If found, use an XTENSA_PLT reloc for 4-byte values.  Otherwise, this
   is the same as the standard code in read.c.  */

void 
xtensa_cons_fix_new (frag, where, size, exp)
     fragS *frag;
     int where; 
     int size;
     expressionS *exp;
{
  bfd_reloc_code_real_type r;
  bfd_boolean plt = FALSE;

  if (*input_line_pointer == '@') 
    {
      if (!strncmp (input_line_pointer, PLT_SUFFIX, strlen (PLT_SUFFIX) - 1)
	  && !strncmp (input_line_pointer, plt_suffix,
		       strlen (plt_suffix) - 1))
	{
	  as_bad (_("undefined @ suffix '%s', expected '%s'"), 
		  input_line_pointer, plt_suffix);
	  ignore_rest_of_line ();
	  return;
	}

      input_line_pointer += strlen (plt_suffix);
      plt = TRUE;
    }

  switch (size)
    {
    case 1:
      r = BFD_RELOC_8;
      break;
    case 2:
      r = BFD_RELOC_16;
      break;
    case 4:
      r = plt ? BFD_RELOC_XTENSA_PLT : BFD_RELOC_32;
      break;
    case 8:
      r = BFD_RELOC_64;
      break;
    default:
      as_bad (_("unsupported BFD relocation size %u"), size);
      r = BFD_RELOC_32;
      break;
    }
  fix_new_exp (frag, where, size, exp, 0, r);
}
  

/* TC_FRAG_INIT hook */

void
xtensa_frag_init (frag)
     fragS *frag;
{
  frag->tc_frag_data.is_no_density = !code_density_available ();
}


symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return NULL;
}


/* Round up a section size to the appropriate boundary.  */

valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
  return size;			/* Byte alignment is fine.  */
}


long
md_pcrel_from (fixP)
     fixS *fixP;
{
  char *insn_p;
  static xtensa_insnbuf insnbuf = NULL;
  int opnum;
  xtensa_operand operand;
  xtensa_opcode opcode;
  xtensa_isa isa = xtensa_default_isa;
  valueT addr = fixP->fx_where + fixP->fx_frag->fr_address;

  if (fixP->fx_done)
    return addr;

  if (fixP->fx_r_type == BFD_RELOC_XTENSA_ASM_EXPAND)
    return addr;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (isa);

  insn_p = &fixP->fx_frag->fr_literal[fixP->fx_where];
  xtensa_insnbuf_from_chars (isa, insnbuf, insn_p);
  opcode = xtensa_decode_insn (isa, insnbuf);

  opnum = reloc_to_opnum (fixP->fx_r_type);

  if (opnum < 0)
    as_fatal (_("invalid operand relocation for '%s' instruction"),
	      xtensa_opcode_name (isa, opcode));
  if (opnum >= xtensa_num_operands (isa, opcode))
    as_fatal (_("invalid relocation for operand %d in '%s' instruction"),
	      opnum, xtensa_opcode_name (isa, opcode));
  operand = xtensa_get_operand (isa, opcode, opnum);
  if (!operand)
    {
      as_warn_where (fixP->fx_file,
		     fixP->fx_line,
		     _("invalid relocation type %d for %s instruction"),
		     fixP->fx_r_type, xtensa_opcode_name (isa, opcode));
      return addr;
    }

  if (!operand_is_pcrel_label (operand))
    {
      as_bad_where (fixP->fx_file,
		    fixP->fx_line,
		    _("invalid relocation for operand %d of '%s'"),
		    opnum, xtensa_opcode_name (isa, opcode));
      return addr;
    }
  if (!xtensa_operand_isPCRelative (operand))
    {
      as_warn_where (fixP->fx_file,
		     fixP->fx_line,
		     _("non-PCREL relocation operand %d for '%s': %s"),
		     opnum, xtensa_opcode_name (isa, opcode),
		     bfd_get_reloc_code_name (fixP->fx_r_type));
      return addr;
    }

  return 0 - xtensa_operand_do_reloc (operand, 0, addr);
}


/* tc_symbol_new_hook */

void
xtensa_symbol_new_hook (symbolP)
     symbolS *symbolP;
{
  symbol_get_tc (symbolP)->plt = 0;
}


/* tc_fix_adjustable hook */

bfd_boolean
xtensa_fix_adjustable (fixP)
     fixS *fixP;
{
  /* We need the symbol name for the VTABLE entries.  */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}


void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  if (fixP->fx_pcrel == 0 && fixP->fx_addsy == 0)
    {
      /* This happens when the relocation is within the current section. 
         It seems this implies a PCREL operation.  We'll catch it and error 
         if not.  */

      char *const fixpos = fixP->fx_frag->fr_literal + fixP->fx_where;
      static xtensa_insnbuf insnbuf = NULL;
      xtensa_opcode opcode;
      xtensa_isa isa;

      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_XTENSA_ASM_EXPAND:
	  fixP->fx_done = 1;
	  break;

	case BFD_RELOC_XTENSA_ASM_SIMPLIFY:
	  as_bad (_("unhandled local relocation fix %s"),
		  bfd_get_reloc_code_name (fixP->fx_r_type));
	  break;

	case BFD_RELOC_32:
	case BFD_RELOC_16:
	case BFD_RELOC_8:
	  /* The only one we support that isn't an instruction field.  */
	  md_number_to_chars (fixpos, *valP, fixP->fx_size);
	  fixP->fx_done = 1;
	  break;

	case BFD_RELOC_XTENSA_OP0:
	case BFD_RELOC_XTENSA_OP1:
	case BFD_RELOC_XTENSA_OP2:
	  isa = xtensa_default_isa;
	  if (!insnbuf)
	    insnbuf = xtensa_insnbuf_alloc (isa);

	  xtensa_insnbuf_from_chars (isa, insnbuf, fixpos);
	  opcode = xtensa_decode_insn (isa, insnbuf);
	  if (opcode == XTENSA_UNDEFINED)
	    as_fatal (_("undecodable FIX"));

	  xtensa_insnbuf_set_immediate_field (opcode, insnbuf, *valP,
					      fixP->fx_file, fixP->fx_line);

	  fixP->fx_frag->tc_frag_data.is_insn = TRUE;
	  xtensa_insnbuf_to_chars (isa, insnbuf, fixpos);
	  fixP->fx_done = 1;
	  break;

	case BFD_RELOC_VTABLE_INHERIT:
	case BFD_RELOC_VTABLE_ENTRY:
	  fixP->fx_done = 0;
	  break;

	default:
	  as_bad (_("unhandled local relocation fix %s"),
		  bfd_get_reloc_code_name (fixP->fx_r_type));
	}
    }
}


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
      return "bad call to md_atof";
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      int idx = i;
      if (target_big_endian)
	idx = (prec - 1 - i);

      md_number_to_chars (litP, (valueT) words[idx], 2);
      litP += 2;
    }

  return NULL;
}


int
md_estimate_size_before_relax (fragP, seg)
     fragS *fragP;
     segT seg ATTRIBUTE_UNUSED;
{
  return fragP->tc_frag_data.text_expansion;
}


/* Translate internal representation of relocation info to BFD target
   format.  */

arelent *
tc_gen_reloc (section, fixp)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));
  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  /* Make sure none of our internal relocations make it this far.
     They'd better have been fully resolved by this point.  */
  assert ((int) fixp->fx_r_type > 0);

  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("cannot represent `%s' relocation in object file"),
		    bfd_get_reloc_code_name (fixp->fx_r_type));
      return NULL;
    }

  if (!fixp->fx_pcrel != !reloc->howto->pc_relative)
    {
      as_fatal (_("internal error? cannot generate `%s' relocation"),
		bfd_get_reloc_code_name (fixp->fx_r_type));
    }
  assert (!fixp->fx_pcrel == !reloc->howto->pc_relative);

  reloc->addend = fixp->fx_offset;

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_XTENSA_OP0:
    case BFD_RELOC_XTENSA_OP1:
    case BFD_RELOC_XTENSA_OP2:
    case BFD_RELOC_XTENSA_ASM_EXPAND:
    case BFD_RELOC_32: 
    case BFD_RELOC_XTENSA_PLT: 
    case BFD_RELOC_VTABLE_INHERIT:
    case BFD_RELOC_VTABLE_ENTRY:
      break;

    case BFD_RELOC_XTENSA_ASM_SIMPLIFY:
      as_warn (_("emitting simplification relocation"));
      break;

    default:
      as_warn (_("emitting unknown relocation"));
    }

  return reloc;
}


void
xtensa_end ()
{
  directive_balance ();
  xtensa_move_literals ();

  xtensa_reorder_segments ();
  xtensa_cleanup_align_frags ();
  xtensa_fix_target_frags ();
  if (software_a0_b_retw_interlock && has_a0_b_retw)
    xtensa_fix_a0_b_retw_frags ();
  if (software_avoid_b_j_loop_end && maybe_has_b_j_loop_end)
    xtensa_fix_b_j_loop_end_frags ();

  /* "close_loop_end" should be processed BEFORE "short_loop".  */
  if (software_avoid_close_loop_end && maybe_has_close_loop_end)
    xtensa_fix_close_loop_end_frags ();

  if (software_avoid_short_loop && maybe_has_short_loop)
    xtensa_fix_short_loop_frags ();

  xtensa_sanity_check ();
}


static void
xtensa_cleanup_align_frags ()
{
  frchainS *frchP;

  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      fragS *fragP;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  if ((fragP->fr_type == rs_align
	       || fragP->fr_type == rs_align_code
	       || (fragP->fr_type == rs_machine_dependent
		   && (fragP->fr_subtype == RELAX_DESIRE_ALIGN
		       || fragP->fr_subtype == RELAX_DESIRE_ALIGN_IF_TARGET)))
	      && fragP->fr_fix == 0)
	    {
	      fragS * next = fragP->fr_next;

	      while (next
		     && next->fr_type == rs_machine_dependent 
		     && next->fr_subtype == RELAX_DESIRE_ALIGN_IF_TARGET) 
		{
		  frag_wane (next);
		  next = next->fr_next;
		}
	    }
	}
    }
}


/* Re-process all of the fragments looking to convert all of the
   RELAX_DESIRE_ALIGN_IF_TARGET fragments.  If there is a branch
   target in the next fragment, convert this to RELAX_DESIRE_ALIGN.
   If the next fragment starts with a loop target, AND the previous
   fragment can be expanded to negate the branch, convert this to a
   RELAX_LOOP_END.  Otherwise, convert to a .fill 0.  */

static void
xtensa_fix_target_frags ()
{
  frchainS *frchP;

  /* When this routine is called, all of the subsections are still intact
     so we walk over subsections instead of sections.  */
  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      bfd_boolean prev_frag_can_negate_branch = FALSE;
      fragS *fragP;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_DESIRE_ALIGN_IF_TARGET)
	    {
	      if (next_frag_is_loop_target (fragP))
		{
		  if (prev_frag_can_negate_branch)
		    fragP->fr_subtype = RELAX_LOOP_END;
		  else
		    {
		      if (!align_only_targets ||
			  next_frag_is_branch_target (fragP))
			fragP->fr_subtype = RELAX_DESIRE_ALIGN;
		      else
			frag_wane (fragP);
		    }
		}
	      else if (!align_only_targets
		       || next_frag_is_branch_target (fragP))
		fragP->fr_subtype = RELAX_DESIRE_ALIGN;
	      else
		frag_wane (fragP);
	    }
	  if (fragP->fr_fix != 0)
	    prev_frag_can_negate_branch = FALSE;
	  if (frag_can_negate_branch (fragP))
	    prev_frag_can_negate_branch = TRUE;
	}
    }
}


static bfd_boolean
frag_can_negate_branch (fragP)
     fragS *fragP;
{
  if (fragP->fr_type == rs_machine_dependent
      && fragP->fr_subtype == RELAX_IMMED)
    {
      TInsn t_insn;
      tinsn_from_chars (&t_insn, fragP->fr_opcode);
      if (is_negatable_branch (&t_insn))
	return TRUE;
    }
  return FALSE;
}


/* Re-process all of the fragments looking to convert all of the
   RELAX_ADD_NOP_IF_A0_B_RETW.  If the next instruction is a
   conditional branch or a retw/retw.n, convert this frag to one that
   will generate a NOP.  In any case close it off with a .fill 0.  */

static void
xtensa_fix_a0_b_retw_frags ()
{
  frchainS *frchP;

  /* When this routine is called, all of the subsections are still intact
     so we walk over subsections instead of sections.  */
  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      fragS *fragP;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_ADD_NOP_IF_A0_B_RETW)
	    {
	      if (next_instrs_are_b_retw (fragP))
		relax_frag_add_nop (fragP);
	      else
		frag_wane (fragP);
	    }
	}
    }
}


bfd_boolean
next_instrs_are_b_retw (fragP)
     fragS * fragP;
{
  xtensa_opcode opcode;
  const fragS *next_fragP = next_non_empty_frag (fragP);
  static xtensa_insnbuf insnbuf = NULL;
  xtensa_isa isa = xtensa_default_isa;
  int offset = 0;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (isa);

  if (next_fragP == NULL)
    return FALSE;

  /* Check for the conditional branch.  */
  xtensa_insnbuf_from_chars (isa, insnbuf, &next_fragP->fr_literal[offset]);
  opcode = xtensa_decode_insn (isa, insnbuf);

  if (!is_conditional_branch_opcode (opcode))
    return FALSE;

  offset += xtensa_insn_length (isa, opcode);
  if (offset == next_fragP->fr_fix)
    {
      next_fragP = next_non_empty_frag (next_fragP);
      offset = 0;
    }
  if (next_fragP == NULL)
    return FALSE;

  /* Check for the retw/retw.n.  */
  xtensa_insnbuf_from_chars (isa, insnbuf, &next_fragP->fr_literal[offset]);
  opcode = xtensa_decode_insn (isa, insnbuf);

  if (is_windowed_return_opcode (opcode))
    return TRUE;
  return FALSE;
}


/* Re-process all of the fragments looking to convert all of the
   RELAX_ADD_NOP_IF_PRE_LOOP_END.  If there is one instruction and a
   loop end label, convert this frag to one that will generate a NOP.
   In any case close it off with a .fill 0.  */

static void
xtensa_fix_b_j_loop_end_frags ()
{
  frchainS *frchP;

  /* When this routine is called, all of the subsections are still intact
     so we walk over subsections instead of sections.  */
  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      fragS *fragP;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_ADD_NOP_IF_PRE_LOOP_END)
	    {
	      if (next_instr_is_loop_end (fragP))
		relax_frag_add_nop (fragP);
	      else
		frag_wane (fragP);
	    }
	}
    }
}


bfd_boolean
next_instr_is_loop_end (fragP)
     fragS * fragP;
{
  const fragS *next_fragP;

  if (next_frag_is_loop_target (fragP))
    return FALSE;

  next_fragP = next_non_empty_frag (fragP);
  if (next_fragP == NULL)
    return FALSE;

  if (!next_frag_is_loop_target (next_fragP))
    return FALSE;

  /* If the size is >= 3 then there is more than one instruction here.
     The hardware bug will not fire.  */
  if (next_fragP->fr_fix > 3)
    return FALSE;

  return TRUE;
}


/* Re-process all of the fragments looking to convert all of the
   RELAX_ADD_NOP_IF_CLOSE_LOOP_END.  If there is an loop end that is
   not MY loop's loop end within 12 bytes, add enough nops here to
   make it at least 12 bytes away.  In any case close it off with a
   .fill 0.  */

static void
xtensa_fix_close_loop_end_frags ()
{
  frchainS *frchP;

  /* When this routine is called, all of the subsections are still intact
     so we walk over subsections instead of sections.  */
  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      fragS *fragP;

      fragS *current_target = NULL;
      offsetT current_offset = 0;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_IMMED)
	    {
	      /* Read it.  If the instruction is a loop, get the target.  */
	      xtensa_opcode opcode = get_opcode_from_buf (fragP->fr_opcode);
	      if (is_loop_opcode (opcode))
		{
		  TInsn t_insn;

		  tinsn_from_chars (&t_insn, fragP->fr_opcode);
		  tinsn_immed_from_frag (&t_insn, fragP);

		  /* Get the current fragment target.  */
		  if (fragP->fr_symbol)
		    {
		      current_target = symbol_get_frag (fragP->fr_symbol);
		      current_offset = fragP->fr_offset;
		    }
		}
	    }

	  if (current_target
	      && fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_ADD_NOP_IF_CLOSE_LOOP_END)
	    {
	      size_t min_bytes;
	      size_t bytes_added = 0;

#define REQUIRED_LOOP_DIVIDING_BYTES 12
	      /* Max out at 12.  */
	      min_bytes = min_bytes_to_other_loop_end
		(fragP->fr_next, current_target, current_offset,
		 REQUIRED_LOOP_DIVIDING_BYTES);

	      if (min_bytes < REQUIRED_LOOP_DIVIDING_BYTES)
		{
		  while (min_bytes + bytes_added
			 < REQUIRED_LOOP_DIVIDING_BYTES)
		    {
		      int length = 3;

		      if (fragP->fr_var < length)
			as_warn (_("fr_var %lu < length %d; ignoring"),
				 fragP->fr_var, length);
		      else
			{
			  assemble_nop (length,
					fragP->fr_literal + fragP->fr_fix);
			  fragP->fr_fix += length;
			  fragP->fr_var -= length;
			}
		      bytes_added += length;
		    }
		}
	      frag_wane (fragP);
	    }
	}
    }
}


size_t
min_bytes_to_other_loop_end (fragP, current_target, current_offset, max_size)
     fragS *fragP;
     fragS *current_target;
     offsetT current_offset;
     size_t max_size;
{
  size_t offset = 0;
  fragS *current_fragP;

  for (current_fragP = fragP;
       current_fragP;
       current_fragP = current_fragP->fr_next)
    {
      if (current_fragP->tc_frag_data.is_loop_target
	  && current_fragP != current_target)
	return offset + current_offset;

      offset += unrelaxed_frag_min_size (current_fragP);

      if (offset + current_offset >= max_size)
	return max_size;
    }
  return max_size;
}


size_t
unrelaxed_frag_min_size (fragP)
     fragS * fragP;
{
  size_t size = fragP->fr_fix;

  /* add fill size */
  if (fragP->fr_type == rs_fill)
    size += fragP->fr_offset;

  return size;
}


/* Re-process all of the fragments looking to convert all
   of the RELAX_ADD_NOP_IF_SHORT_LOOP.  If:

   A)
     1) the instruction size count to the loop end label
        is too short (<= 2 instructions),
     2) loop has a jump or branch in it

   or B)
     1) software_avoid_all_short_loops is true
     2) The generating loop was a  'loopgtz' or 'loopnez'
     3) the instruction size count to the loop end label is too short
        (<= 2 instructions)
   then convert this frag (and maybe the next one) to generate a NOP.
   In any case close it off with a .fill 0.  */

static void
xtensa_fix_short_loop_frags ()
{
  frchainS *frchP;

  /* When this routine is called, all of the subsections are still intact
     so we walk over subsections instead of sections.  */
  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      fragS *fragP;
      fragS *current_target = NULL;
      offsetT current_offset = 0;
      xtensa_opcode current_opcode = XTENSA_UNDEFINED;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  /* check on the current loop */
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_IMMED)
	    {
	      /* Read it.  If the instruction is a loop, get the target.  */
	      xtensa_opcode opcode = get_opcode_from_buf (fragP->fr_opcode);
	      if (is_loop_opcode (opcode))
		{
		  TInsn t_insn;

		  tinsn_from_chars (&t_insn, fragP->fr_opcode);
		  tinsn_immed_from_frag (&t_insn, fragP);

		  /* Get the current fragment target.  */
		  if (fragP->fr_symbol)
		    {
		      current_target = symbol_get_frag (fragP->fr_symbol);
		      current_offset = fragP->fr_offset;
		      current_opcode = opcode;
		    }
		}
	    }

	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_ADD_NOP_IF_SHORT_LOOP)
	    {
	      size_t insn_count =
		count_insns_to_loop_end (fragP->fr_next, TRUE, 3);
	      if (insn_count < 3
		  && (branch_before_loop_end (fragP->fr_next)
		      || (software_avoid_all_short_loops
			  && current_opcode != XTENSA_UNDEFINED
			  && !is_the_loop_opcode (current_opcode))))
		relax_frag_add_nop (fragP);
	      else
		frag_wane (fragP);
	    }
	}
    }
}


size_t
count_insns_to_loop_end (base_fragP, count_relax_add, max_count)
     fragS *base_fragP;
     bfd_boolean count_relax_add;
     size_t max_count;
{
  fragS *fragP = NULL;
  size_t insn_count = 0;

  fragP = base_fragP;

  for (; fragP && !fragP->tc_frag_data.is_loop_target; fragP = fragP->fr_next)
    {
      insn_count += unrelaxed_frag_min_insn_count (fragP);
      if (insn_count >= max_count)
	return max_count;

      if (count_relax_add)
	{
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_ADD_NOP_IF_SHORT_LOOP)
	    {
	      /* In order to add the appropriate number of
	         NOPs, we count an instruction for downstream
	         occurrences.  */
	      insn_count++;
	      if (insn_count >= max_count)
		return max_count;
	    }
	}
    }
  return insn_count;
}


size_t
unrelaxed_frag_min_insn_count (fragP)
     fragS *fragP;
{
  size_t insn_count = 0;
  int offset = 0;

  if (!fragP->tc_frag_data.is_insn)
    return insn_count;

  /* Decode the fixed instructions.  */
  while (offset < fragP->fr_fix)
    {
      xtensa_opcode opcode = get_opcode_from_buf (fragP->fr_literal + offset);
      if (opcode == XTENSA_UNDEFINED)
	{
	  as_fatal (_("undecodable instruction in instruction frag"));
	  return insn_count;
	}
      offset += xtensa_insn_length (xtensa_default_isa, opcode);
      insn_count++;
    }

  return insn_count;
}


bfd_boolean
branch_before_loop_end (base_fragP)
     fragS *base_fragP;
{
  fragS *fragP;

  for (fragP = base_fragP;
       fragP && !fragP->tc_frag_data.is_loop_target;
       fragP = fragP->fr_next)
    {
      if (unrelaxed_frag_has_b_j (fragP))
	return TRUE;
    }
  return FALSE;
}


bfd_boolean
unrelaxed_frag_has_b_j (fragP)
     fragS *fragP;
{
  size_t insn_count = 0;
  int offset = 0;

  if (!fragP->tc_frag_data.is_insn)
    return FALSE;

  /* Decode the fixed instructions.  */
  while (offset < fragP->fr_fix)
    {
      xtensa_opcode opcode = get_opcode_from_buf (fragP->fr_literal + offset);
      if (opcode == XTENSA_UNDEFINED)
	{
	  as_fatal (_("undecodable instruction in instruction frag"));
	  return insn_count;
	}
      if (is_branch_or_jump_opcode (opcode))
	return TRUE;
      offset += xtensa_insn_length (xtensa_default_isa, opcode);
    }
  return FALSE;
}


/* Checks to be made after initial assembly but before relaxation.  */

static void
xtensa_sanity_check ()
{
  char *file_name;
  int line;

  frchainS *frchP;

  as_where (&file_name, &line);
  for (frchP = frchain_root; frchP; frchP = frchP->frch_next)
    {
      fragS *fragP;

      /* Walk over all of the fragments in a subsection.  */
      for (fragP = frchP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  /* Currently we only check for empty loops here.  */
	  if (fragP->fr_type == rs_machine_dependent
	      && fragP->fr_subtype == RELAX_IMMED)
	    {
	      static xtensa_insnbuf insnbuf = NULL;
	      TInsn t_insn;

	      if (fragP->fr_opcode != NULL)
		{
		  if (!insnbuf)
		    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);
		  tinsn_from_chars (&t_insn, fragP->fr_opcode);
		  tinsn_immed_from_frag (&t_insn, fragP);

		  if (is_loop_opcode (t_insn.opcode))
		    {
		      if (is_empty_loop (&t_insn, fragP))
			{
			  new_logical_line (fragP->fr_file, fragP->fr_line);
			  as_bad (_("invalid empty loop"));
			}
		      if (!is_local_forward_loop (&t_insn, fragP))
			{
			  new_logical_line (fragP->fr_file, fragP->fr_line);
			  as_bad (_("loop target does not follow "
				    "loop instruction in section"));
			}
		    }
		}
	    }
	}
    }
  new_logical_line (file_name, line);
}


#define LOOP_IMMED_OPN 1

/* Return true if the loop target is the next non-zero fragment.  */

bfd_boolean
is_empty_loop (insn, fragP)
     const TInsn *insn;
     fragS *fragP;
{
  const expressionS *expr;
  symbolS *symbolP;
  fragS *next_fragP;

  if (insn->insn_type != ITYPE_INSN)
    return FALSE;

  if (!is_loop_opcode (insn->opcode))
    return FALSE;

  if (insn->ntok <= LOOP_IMMED_OPN)
    return FALSE;

  expr = &insn->tok[LOOP_IMMED_OPN];

  if (expr->X_op != O_symbol)
    return FALSE;

  symbolP = expr->X_add_symbol;
  if (!symbolP)
    return FALSE;

  if (symbol_get_frag (symbolP) == NULL)
    return FALSE;

  if (S_GET_VALUE (symbolP) != 0)
    return FALSE;

  /* Walk through the zero-size fragments from this one.  If we find
     the target fragment, then this is a zero-size loop.  */
  for (next_fragP = fragP->fr_next;
       next_fragP != NULL;
       next_fragP = next_fragP->fr_next)
    {
      if (next_fragP == symbol_get_frag (symbolP))
	return TRUE;
      if (next_fragP->fr_fix != 0)
	return FALSE;
    }
  return FALSE;
}


bfd_boolean
is_local_forward_loop (insn, fragP)
     const TInsn *insn;
     fragS *fragP;
{
  const expressionS *expr;
  symbolS *symbolP;
  fragS *next_fragP;

  if (insn->insn_type != ITYPE_INSN)
    return FALSE;

  if (!is_loop_opcode (insn->opcode))
    return FALSE;

  if (insn->ntok <= LOOP_IMMED_OPN)
    return FALSE;

  expr = &insn->tok[LOOP_IMMED_OPN];

  if (expr->X_op != O_symbol)
    return FALSE;

  symbolP = expr->X_add_symbol;
  if (!symbolP)
    return FALSE;

  if (symbol_get_frag (symbolP) == NULL)
    return FALSE;

  /* Walk through fragments until we find the target.
     If we do not find the target, then this is an invalid loop.  */
  for (next_fragP = fragP->fr_next;
       next_fragP != NULL;
       next_fragP = next_fragP->fr_next)
    if (next_fragP == symbol_get_frag (symbolP))
      return TRUE;

  return FALSE;
}


/* Alignment Functions.  */

size_t
get_text_align_power (target_size)
     int target_size;
{
  size_t i = 0;
  for (i = 0; i < sizeof (size_t); i++)
    {
      if (target_size <= (1 << i))
	return i;
    }
  as_fatal (_("get_text_align_power: argument too large"));
  return 0;
}


addressT
get_text_align_max_fill_size (align_pow, use_nops, use_no_density)
     int align_pow;
     bfd_boolean use_nops;
     bfd_boolean use_no_density;
{
  if (!use_nops)
    return (1 << align_pow);
  if (use_no_density)
    return 3 * (1 << align_pow);

  return 1 + (1 << align_pow);
}


/* get_text_align_fill_size ()
  
   Desired alignments:
      give the address
      target_size = size of next instruction
      align_pow = get_text_align_power (target_size).
      use_nops = 0
      use_no_density = 0;
   Loop alignments:
      address = current address + loop instruction size;
      target_size = 3 (for 2 or 3 byte target)
                  = 8 (for 8 byte target)
      align_pow = get_text_align_power (target_size);
      use_nops = 1
      use_no_density = set appropriately
   Text alignments:
      address = current address + loop instruction size;
      target_size = 0
      align_pow = get_text_align_power (target_size);
      use_nops = 0
      use_no_density = 0.  */

addressT
get_text_align_fill_size (address, align_pow, target_size,
			  use_nops, use_no_density)
     addressT address;
     int align_pow;
     int target_size;
     bfd_boolean use_nops;
     bfd_boolean use_no_density;
{
  /* Input arguments:

     align_pow: log2 (required alignment).

     target_size: alignment must allow the new_address and
     new_address+target_size-1.

     use_nops: if true, then we can only use 2 or 3 byte nops.

     use_no_density: if use_nops and use_no_density, we can only use
     3-byte nops.

     Usually, for non-zero target_size, the align_pow is the power of 2
     that is greater than or equal to the target_size.  This handles the
     2-byte, 3-byte and 8-byte instructions.  */

  size_t alignment = (1 << align_pow);
  if (!use_nops)
    {
      /* This is the easy case.  */
      size_t mod;
      mod = address % alignment;
      if (mod != 0)
	mod = alignment - mod;
      assert ((address + mod) % alignment == 0);
      return mod;
    }

  /* This is the slightly harder case.  */
  assert ((int) alignment >= target_size);
  assert (target_size > 0);
  if (!use_no_density)
    {
      size_t i;
      for (i = 0; i < alignment * 2; i++)
	{
	  if (i == 1)
	    continue;
	  if ((address + i) >> align_pow ==
	      (address + i + target_size - 1) >> align_pow)
	    return i;
	}
    }
  else
    {
      size_t i;

      /* Can only fill multiples of 3.  */
      for (i = 0; i <= alignment * 3; i += 3)
	{
	  if ((address + i) >> align_pow ==
	      (address + i + target_size - 1) >> align_pow)
	    return i;
	}
    }
  assert (0);
  return 0;
}


/* This will assert if it is not possible.  */

size_t
get_text_align_nop_count (fill_size, use_no_density)
     size_t fill_size;
     bfd_boolean use_no_density;
{
  size_t count = 0;
  if (use_no_density)
    {
      assert (fill_size % 3 == 0);
      return (fill_size / 3);
    }

  assert (fill_size != 1);	/* Bad argument.  */

  while (fill_size > 1)
    {
      size_t insn_size = 3;
      if (fill_size == 2 || fill_size == 4)
	insn_size = 2;
      fill_size -= insn_size;
      count++;
    }
  assert (fill_size != 1);	/* Bad algorithm.  */
  return count;
}


size_t
get_text_align_nth_nop_size (fill_size, n, use_no_density)
     size_t fill_size;
     size_t n;
     bfd_boolean use_no_density;
{
  size_t count = 0;

  assert (get_text_align_nop_count (fill_size, use_no_density) > n);

  if (use_no_density)
    return 3;

  while (fill_size > 1)
    {
      size_t insn_size = 3;
      if (fill_size == 2 || fill_size == 4)
	insn_size = 2;
      fill_size -= insn_size;
      count++;
      if (n + 1 == count)
	return insn_size;
    }
  assert (0);
  return 0;
}


/* For the given fragment, find the appropriate address
   for it to begin at if we are using NOPs to align it.  */

static addressT
get_noop_aligned_address (fragP, address)
     fragS *fragP;
     addressT address;
{
  static xtensa_insnbuf insnbuf = NULL;
  size_t fill_size = 0;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  switch (fragP->fr_type)
    {
    case rs_machine_dependent:
      if (fragP->fr_subtype == RELAX_ALIGN_NEXT_OPCODE)
	{
	  /* The rule is: get next fragment's FIRST instruction.  Find
	     the smallest number of bytes that need to be added to
	     ensure that the next fragment's FIRST instruction will fit
	     in a single word.

	     E.G.,   2 bytes : 0, 1, 2 mod 4 
		     3 bytes: 0, 1 mod 4 

	     If the FIRST instruction MIGHT be relaxed, 
	     assume that it will become a 3 byte instruction.  */

	  int target_insn_size;
	  xtensa_opcode opcode = next_frag_opcode (fragP);
	  addressT pre_opcode_bytes;

	  if (opcode == XTENSA_UNDEFINED)
	    {
	      as_bad_where (fragP->fr_file, fragP->fr_line,
			    _("invalid opcode for RELAX_ALIGN_NEXT_OPCODE"));
	      as_fatal (_("cannot continue"));
	    }

	  target_insn_size = xtensa_insn_length (xtensa_default_isa, opcode);

	  pre_opcode_bytes = next_frag_pre_opcode_bytes (fragP);

	  if (is_loop_opcode (opcode))
	    {
	      /* next_fragP should be the loop.  */
	      const fragS *next_fragP = next_non_empty_frag (fragP);
	      xtensa_opcode next_opcode = next_frag_opcode (next_fragP);
	      size_t alignment;

	      pre_opcode_bytes += target_insn_size;

	      /* For loops, the alignment depends on the size of the
		 instruction following the loop, not the loop instruction.  */
	      if (next_opcode == XTENSA_UNDEFINED)
		target_insn_size = 3;
	      else
		{
		  target_insn_size =
		    xtensa_insn_length (xtensa_default_isa, next_opcode);

		  if (target_insn_size == 2)
		    target_insn_size = 3;	/* ISA specifies this.  */
		}

	      /* If it was 8, then we'll need a larger alignment
	         for the section.  */
	      alignment = get_text_align_power (target_insn_size);

	      /* Is Now_seg valid */
	      record_alignment (now_seg, alignment);
	    }
	  else
	    as_fatal (_("expected loop opcode in relax align next target"));

	  fill_size = get_text_align_fill_size
	    (address + pre_opcode_bytes,
	     get_text_align_power (target_insn_size),
	     target_insn_size, TRUE, fragP->tc_frag_data.is_no_density);
	}
      break;
#if 0
    case rs_align:
    case rs_align_code:
      fill_size = get_text_align_fill_size
	(address, fragP->fr_offset, 1, TRUE,
	 fragP->tc_frag_data.is_no_density);
      break;
#endif
    default:
      as_fatal (_("expected align_code or RELAX_ALIGN_NEXT_OPCODE"));
    }

  return address + fill_size;
}


/* 3 mechanisms for relaxing an alignment: 
   
   Align to a power of 2. 
   Align so the next fragment's instruction does not cross a word boundary. 
   Align the current instruction so that if the next instruction 
       were 3 bytes, it would not cross a word boundary. 
   
   We can align with:

   zeros    - This is easy; always insert zeros. 
   nops     - 3 and 2 byte instructions 
              2 - 2 byte nop 
              3 - 3 byte nop 
              4 - 2, 2-byte nops 
              >=5 : 3 byte instruction + fn(n-3) 
   widening - widen previous instructions.  */

static addressT
get_widen_aligned_address (fragP, address)
     fragS *fragP;
     addressT address;
{
  addressT align_pow, new_address, loop_insn_offset;
  fragS *next_frag;
  int insn_size;
  xtensa_opcode opcode, next_opcode;
  static xtensa_insnbuf insnbuf = NULL;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  if (fragP->fr_type == rs_align || fragP->fr_type == rs_align_code)
    {
      align_pow = fragP->fr_offset;
      new_address = ((address + ((1 << align_pow) - 1))
		     << align_pow) >> align_pow;
      return new_address;
    }

  if (fragP->fr_type == rs_machine_dependent)
    {
      switch (fragP->fr_subtype)
	{
	case RELAX_DESIRE_ALIGN:

	  /* The rule is: get the next fragment's FIRST instruction. 
	     Find the smallest number of bytes needed to be added 
	     in order to ensure that the next fragment is FIRST 
	     instruction will fit in a single word. 
	     i.e.    2 bytes : 0, 1, 2.  mod 4 
	             3 bytes: 0, 1 mod 4 
	     If the FIRST instruction MIGHT be relaxed, 
	     assume that it will become a 3-byte instruction.  */

	  insn_size = 3;
	  /* Check to see if it might be 2 bytes.  */
	  next_opcode = next_frag_opcode (fragP);
	  if (next_opcode != XTENSA_UNDEFINED
	      && xtensa_insn_length (xtensa_default_isa, next_opcode) == 2)
	    insn_size = 2;

	  assert (insn_size <= 4);
	  for (new_address = address; new_address < address + 4; new_address++)
	    {
	      if (new_address >> 2 == (new_address + insn_size - 1) >> 2)
		return new_address;
	    }
	  as_bad (_("internal error aligning"));
	  return address;

	case RELAX_ALIGN_NEXT_OPCODE:
	  /* The rule is: get next fragment's FIRST instruction. 
	     Find the smallest number of bytes needed to be added 
	     in order to ensure that the next fragment's FIRST 
	     instruction will fit in a single word. 
	     i.e.    2 bytes : 0, 1, 2.  mod 4 
	             3 bytes: 0, 1 mod 4 
	     If the FIRST instruction MIGHT be relaxed, 
	     assume that it will become a 3 byte instruction.  */

	  opcode = next_frag_opcode (fragP);
	  if (opcode == XTENSA_UNDEFINED)
	    {
	      as_bad_where (fragP->fr_file, fragP->fr_line,
			    _("invalid opcode for RELAX_ALIGN_NEXT_OPCODE"));
	      as_fatal (_("cannot continue"));
	    }
	  insn_size = xtensa_insn_length (xtensa_default_isa, opcode);
	  assert (insn_size <= 4);
	  assert (is_loop_opcode (opcode));

	  loop_insn_offset = 0;
	  next_frag = next_non_empty_frag (fragP);

	  /* If the loop has been expanded then the loop
	     instruction could be at an offset from this fragment.  */
	  if (next_frag->fr_subtype != RELAX_IMMED)
	    loop_insn_offset = get_expanded_loop_offset (opcode);

	  for (new_address = address; new_address < address + 4; new_address++)
	    {
	      if ((new_address + loop_insn_offset + insn_size) >> 2 ==
		  (new_address + loop_insn_offset + insn_size + 2) >> 2)
		return new_address;
	    }
	  as_bad (_("internal error aligning"));
	  return address;

	default:
	  as_bad (_("internal error aligning"));
	  return address;
	}
    }
  as_bad (_("internal error aligning"));
  return address;
}


/* md_relax_frag Hook and Helper Functions.  */

/* Return the number of bytes added to this fragment, given that the
   input has been stretched already by "stretch".  */

long
xtensa_relax_frag (fragP, stretch, stretched_p)
     fragS *fragP;
     long stretch;
     int *stretched_p;
{
  int unreported = fragP->tc_frag_data.unreported_expansion;
  long new_stretch = 0;
  char *file_name;
  int line, lit_size;

  as_where (&file_name, &line);
  new_logical_line (fragP->fr_file, fragP->fr_line);

  fragP->tc_frag_data.unreported_expansion = 0;

  switch (fragP->fr_subtype)
    {
    case RELAX_ALIGN_NEXT_OPCODE:
      /* Always convert.  */
      new_stretch = relax_frag_text_align (fragP, stretch);
      break;

    case RELAX_LOOP_END:
      /* Do nothing.  */
      break;

    case RELAX_LOOP_END_ADD_NOP:
      /* Add a NOP and switch to .fill 0.  */
      new_stretch = relax_frag_add_nop (fragP);
      break;

    case RELAX_DESIRE_ALIGN:
      /* We REALLY want to change the relaxation order here.  This
         should do NOTHING.  The narrowing before it will either align
         it or not.  */
      break;

    case RELAX_LITERAL:
    case RELAX_LITERAL_FINAL:
      return 0;

    case RELAX_LITERAL_NR:
      lit_size = 4;
      fragP->fr_subtype = RELAX_LITERAL_FINAL;
      assert (unreported == lit_size);
      memset (&fragP->fr_literal[fragP->fr_fix], 0, 4);
      fragP->fr_var -= lit_size;
      fragP->fr_fix += lit_size;
      new_stretch = 4;
      break;

    case RELAX_NARROW:
      new_stretch = relax_frag_narrow (fragP, stretch);
      break;

    case RELAX_IMMED:
    case RELAX_IMMED_STEP1:
    case RELAX_IMMED_STEP2:
      /* Place the immediate.  */
      new_stretch = relax_frag_immed (now_seg, fragP, stretch,
				      fragP->fr_subtype - RELAX_IMMED,
				      stretched_p);
      break;

    case RELAX_LITERAL_POOL_BEGIN:
    case RELAX_LITERAL_POOL_END:
      /* No relaxation required.  */
      break;

    default:
      as_bad (_("bad relaxation state"));
    }

  new_logical_line (file_name, line);
  return new_stretch;
}


static long
relax_frag_text_align (fragP, stretch)
     fragS *fragP;
     long stretch;
{
  addressT old_address, old_next_address, old_size;
  addressT new_address, new_next_address, new_size;
  addressT growth;

  /* Overview of the relaxation procedure for alignment
     inside an executable section:
    
     The old size is stored in the tc_frag_data.text_expansion field.
    
     Calculate the new address, fix up the text_expansion and
     return the growth.  */

  /* Calculate the old address of this fragment and the next fragment.  */
  old_address = fragP->fr_address - stretch;
  old_next_address = (fragP->fr_address - stretch + fragP->fr_fix +
		      fragP->tc_frag_data.text_expansion);
  old_size = old_next_address - old_address;

  /* Calculate the new address of this fragment and the next fragment.  */
  new_address = fragP->fr_address;
  new_next_address =
    get_noop_aligned_address (fragP, fragP->fr_address + fragP->fr_fix);
  new_size = new_next_address - new_address;

  growth = new_size - old_size;

  /* Fix up the text_expansion field and return the new growth.  */
  fragP->tc_frag_data.text_expansion += growth;
  return growth;
}


/* Add a NOP (i.e., "or a1, a1, a1").  Use the 3-byte one because we
   don't know about the availability of density yet.  TODO: When the
   flags are stored per fragment, use NOP.N when possible.  */

static long
relax_frag_add_nop (fragP)
     fragS *fragP;
{
  static xtensa_insnbuf insnbuf = NULL;
  TInsn t_insn;
  char *nop_buf = fragP->fr_literal + fragP->fr_fix;
  int length;
  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  tinsn_init (&t_insn);
  t_insn.opcode = xtensa_or_opcode;
  assert (t_insn.opcode != XTENSA_UNDEFINED);

  t_insn.ntok = 3;
  set_expr_const (&t_insn.tok[0], 1);
  set_expr_const (&t_insn.tok[1], 1);
  set_expr_const (&t_insn.tok[2], 1);

  tinsn_to_insnbuf (&t_insn, insnbuf);
  fragP->tc_frag_data.is_insn = TRUE;
  xtensa_insnbuf_to_chars (xtensa_default_isa, insnbuf, nop_buf);

  length = xtensa_insn_length (xtensa_default_isa, t_insn.opcode);
  if (fragP->fr_var < length)
    {
      as_warn (_("fr_var (%ld) < length (%d); ignoring"),
	       fragP->fr_var, length);
      frag_wane (fragP);
      return 0;
    }

  fragP->fr_fix += length;
  fragP->fr_var -= length;
  frag_wane (fragP);
  return length;
}


static long
relax_frag_narrow (fragP, stretch)
     fragS *fragP;
     long stretch;
{
  /* Overview of the relaxation procedure for alignment inside an
     executable section: Find the number of widenings required and the
     number of nop bytes required. Store the number of bytes ALREADY
     widened. If there are enough instructions to widen (must go back
     ONLY through NARROW fragments), mark each of the fragments as TO BE
     widened, recalculate the fragment addresses.  */

  assert (fragP->fr_type == rs_machine_dependent
	  && fragP->fr_subtype == RELAX_NARROW);

  if (!future_alignment_required (fragP, 0))
    {
      /* If already expanded but no longer needed because of a prior
         stretch, it is SAFE to unexpand because the next fragment will
         NEVER start at an address > the previous time through the
         relaxation.  */
      if (fragP->tc_frag_data.text_expansion)
	{
	  if (stretch > 0)
	    {
	      fragP->tc_frag_data.text_expansion = 0;
	      return -1;
	    }
	  /* Otherwise we have to live with this bad choice.  */
	  return 0;
	}
      return 0;
    }

  if (fragP->tc_frag_data.text_expansion == 0)
    {
      fragP->tc_frag_data.text_expansion = 1;
      return 1;
    }

  return 0;
}


static bfd_boolean
future_alignment_required (fragP, stretch)
     fragS *fragP;
     long stretch;
{
  long address = fragP->fr_address + stretch;
  int num_widens = 0;
  addressT aligned_address;
  offsetT desired_diff;

  while (fragP)
    {
      /* Limit this to a small search.  */
      if (num_widens > 8)
	return FALSE;
      address += fragP->fr_fix;

      switch (fragP->fr_type)
	{
	case rs_fill:
	  address += fragP->fr_offset * fragP->fr_var;
	  break;

	case rs_machine_dependent:
	  switch (fragP->fr_subtype)
	    {
	    case RELAX_NARROW:
	      /* address += fragP->fr_fix; */
	      num_widens++;
	      break;

	    case RELAX_IMMED:
	      address += (/* fragP->fr_fix + */
			  fragP->tc_frag_data.text_expansion);
	      break;

	    case RELAX_ALIGN_NEXT_OPCODE:
	    case RELAX_DESIRE_ALIGN:
	      /* address += fragP->fr_fix; */
	      aligned_address = get_widen_aligned_address (fragP, address);
	      desired_diff = aligned_address - address;
	      assert (desired_diff >= 0);
	      /* If there are enough wideners in between do it.  */
	      /* return (num_widens == desired_diff); */
	      if (num_widens == desired_diff)
		return TRUE;
	      if (fragP->fr_subtype == RELAX_ALIGN_NEXT_OPCODE)
		return FALSE;
	      break;

	    default:
	      return FALSE;
	    }
	  break;

	default:
	  return FALSE;
	}
      fragP = fragP->fr_next;
    }

  return FALSE;
}


static long
relax_frag_immed (segP, fragP, stretch, min_steps, stretched_p)
     segT segP;
     fragS *fragP;
     long stretch;
     int min_steps;
     int *stretched_p;
{
  static xtensa_insnbuf insnbuf = NULL;
  TInsn t_insn;
  int old_size;
  bfd_boolean negatable_branch = FALSE;
  bfd_boolean branch_jmp_to_next = FALSE;
  IStack istack;
  offsetT frag_offset;
  int num_steps;
  fragS *lit_fragP;
  int num_text_bytes, num_literal_bytes;
  int literal_diff, text_diff;

  assert (fragP->fr_opcode != NULL);

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  tinsn_from_chars (&t_insn, fragP->fr_opcode);
  tinsn_immed_from_frag (&t_insn, fragP);

  negatable_branch = is_negatable_branch (&t_insn);

  old_size = xtensa_insn_length (xtensa_default_isa, t_insn.opcode);

  if (software_avoid_b_j_loop_end)
    branch_jmp_to_next = is_branch_jmp_to_next (&t_insn, fragP);

  /* Special case: replace a branch to the next instruction with a NOP.
     This is required to work around a hardware bug in T1040.0 and also
     serves as an optimization.  */

  if (branch_jmp_to_next
      && ((old_size == 2) || (old_size == 3))
      && !next_frag_is_loop_target (fragP))
    return 0;

  /* Here is the fun stuff: Get the immediate field from this
     instruction.  If it fits, we are done.  If not, find the next
     instruction sequence that fits.  */

  frag_offset = fragP->fr_opcode - fragP->fr_literal;
  istack_init (&istack);
  num_steps = xg_assembly_relax (&istack, &t_insn, segP, fragP, frag_offset,
				 min_steps, stretch);
  if (num_steps < min_steps)
    {
      as_fatal (_("internal error: relaxation failed"));
      return 0;
    }

  if (num_steps > RELAX_IMMED_MAXSTEPS)
    {
      as_fatal (_("internal error: relaxation requires too many steps"));
      return 0;
    }

  fragP->fr_subtype = (int) RELAX_IMMED + num_steps;

  /* Figure out the number of bytes needed.  */
  lit_fragP = 0;
  num_text_bytes = get_num_stack_text_bytes (&istack) - old_size;
  num_literal_bytes = get_num_stack_literal_bytes (&istack);
  literal_diff = num_literal_bytes - fragP->tc_frag_data.literal_expansion;
  text_diff = num_text_bytes - fragP->tc_frag_data.text_expansion;

  /* It MUST get larger.  If not, we could get an infinite loop.  */
  know (num_text_bytes >= 0);
  know (literal_diff >= 0 && text_diff >= 0);

  fragP->tc_frag_data.text_expansion = num_text_bytes;
  fragP->tc_frag_data.literal_expansion = num_literal_bytes;

  /* Find the associated expandable literal for this.  */
  if (literal_diff != 0)
    {
      lit_fragP = fragP->tc_frag_data.literal_frag;
      if (lit_fragP)
	{
	  assert (literal_diff == 4);
	  lit_fragP->tc_frag_data.unreported_expansion += literal_diff;

	  /* We expect that the literal section state has NOT been
	     modified yet.  */
	  assert (lit_fragP->fr_type == rs_machine_dependent
		  && lit_fragP->fr_subtype == RELAX_LITERAL);
	  lit_fragP->fr_subtype = RELAX_LITERAL_NR;

	  /* We need to mark this section for another iteration
	     of relaxation.  */
	  (*stretched_p)++;
	}
    }

  /* This implicitly uses the assumption that a branch is negated
     when the size of the output increases by at least 2 bytes.  */

  if (negatable_branch && num_text_bytes >= 2)
    {
      /* If next frag is a loop end, then switch it to add a NOP.  */
      update_next_frag_nop_state (fragP);
    }

  return text_diff;
}


/* md_convert_frag Hook and Helper Functions.  */

void
md_convert_frag (abfd, sec, fragp)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec;
     fragS *fragp;
{
  char *file_name;
  int line;

  as_where (&file_name, &line);
  new_logical_line (fragp->fr_file, fragp->fr_line);

  switch (fragp->fr_subtype)
    {
    case RELAX_ALIGN_NEXT_OPCODE:
      /* Always convert.  */
      convert_frag_align_next_opcode (fragp);
      break;

    case RELAX_DESIRE_ALIGN:
      /* Do nothing.  If not aligned already, too bad.  */
      break;

    case RELAX_LITERAL:
    case RELAX_LITERAL_FINAL:
      break;

    case RELAX_NARROW:
      /* No conversion.  */
      convert_frag_narrow (fragp);
      break;

    case RELAX_IMMED:
    case RELAX_IMMED_STEP1:
    case RELAX_IMMED_STEP2:
      /* Place the immediate.  */
      convert_frag_immed (sec, fragp, fragp->fr_subtype - RELAX_IMMED);
      break;

    case RELAX_LITERAL_NR:
      if (use_literal_section)
	{
	  /* This should have been handled during relaxation.  When
	     relaxing a code segment, literals sometimes need to be
	     added to the corresponding literal segment.  If that
	     literal segment has already been relaxed, then we end up
	     in this situation.  Marking the literal segments as data
	     would make this happen less often (since GAS always relaxes
	     code before data), but we could still get into trouble if
	     there are instructions in a segment that is not marked as
	     containing code.  Until we can implement a better solution,
	     cheat and adjust the addresses of all the following frags.
	     This could break subsequent alignments, but the linker's
	     literal coalescing will do that anyway.  */

	  fragS *f;
	  fragp->fr_subtype = RELAX_LITERAL_FINAL;
	  assert (fragp->tc_frag_data.unreported_expansion == 4);
	  memset (&fragp->fr_literal[fragp->fr_fix], 0, 4);
	  fragp->fr_var -= 4;
	  fragp->fr_fix += 4;
	  for (f = fragp->fr_next; f; f = f->fr_next)
	    f->fr_address += 4;
	}
      else
	as_bad (_("invalid relaxation fragment result"));
      break;
    }

  fragp->fr_var = 0;
  new_logical_line (file_name, line);
}


void
convert_frag_align_next_opcode (fragp)
     fragS *fragp;
{
  char *nop_buf;		/* Location for Writing.  */
  size_t i;

  bfd_boolean use_no_density = fragp->tc_frag_data.is_no_density;
  addressT aligned_address;
  size_t fill_size, nop_count;

  aligned_address = get_noop_aligned_address (fragp, fragp->fr_address +
					      fragp->fr_fix);
  fill_size = aligned_address - (fragp->fr_address + fragp->fr_fix);
  nop_count = get_text_align_nop_count (fill_size, use_no_density);
  nop_buf = fragp->fr_literal + fragp->fr_fix;

  for (i = 0; i < nop_count; i++)
    {
      size_t nop_size;
      nop_size = get_text_align_nth_nop_size (fill_size, i, use_no_density);

      assemble_nop (nop_size, nop_buf);
      nop_buf += nop_size;
    }

  fragp->fr_fix += fill_size;
  fragp->fr_var -= fill_size;
}


static void
convert_frag_narrow (fragP)
     fragS *fragP;
{
  static xtensa_insnbuf insnbuf = NULL;
  TInsn t_insn, single_target;
  int size, old_size, diff, error_val;
  offsetT frag_offset;

  if (fragP->tc_frag_data.text_expansion == 0)
    {
      /* No conversion.  */
      fragP->fr_var = 0;
      return;
    }

  assert (fragP->fr_opcode != NULL);

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  tinsn_from_chars (&t_insn, fragP->fr_opcode);
  tinsn_immed_from_frag (&t_insn, fragP);

  /* Just convert it to a wide form....  */
  size = 0;
  old_size = xtensa_insn_length (xtensa_default_isa, t_insn.opcode);

  tinsn_init (&single_target);
  frag_offset = fragP->fr_opcode - fragP->fr_literal;

  error_val = xg_expand_narrow (&single_target, &t_insn);
  if (error_val)
    as_bad (_("unable to widen instruction"));

  size = xtensa_insn_length (xtensa_default_isa, single_target.opcode);
  xg_emit_insn_to_buf (&single_target, fragP->fr_opcode,
		       fragP, frag_offset, TRUE);

  diff = size - old_size;
  assert (diff >= 0);
  assert (diff <= fragP->fr_var);
  fragP->fr_var -= diff;
  fragP->fr_fix += diff;

  /* clean it up */
  fragP->fr_var = 0;
}


static void
convert_frag_immed (segP, fragP, min_steps)
     segT segP;
     fragS *fragP;
     int min_steps;
{
  char *immed_instr = fragP->fr_opcode;
  static xtensa_insnbuf insnbuf = NULL;
  TInsn orig_t_insn;
  bfd_boolean expanded = FALSE;
  char *fr_opcode = fragP->fr_opcode;
  bfd_boolean branch_jmp_to_next = FALSE;
  int size;

  assert (fragP->fr_opcode != NULL);

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (xtensa_default_isa);

  tinsn_from_chars (&orig_t_insn, fragP->fr_opcode);
  tinsn_immed_from_frag (&orig_t_insn, fragP);

  /* Here is the fun stuff:  Get the immediate field from this
     instruction.  If it fits, we're done.  If not, find the next
     instruction sequence that fits.  */

  if (software_avoid_b_j_loop_end)
    branch_jmp_to_next = is_branch_jmp_to_next (&orig_t_insn, fragP);

  if (branch_jmp_to_next && !next_frag_is_loop_target (fragP))
    {
      /* Conversion just inserts a NOP and marks the fix as completed.  */
      size = xtensa_insn_length (xtensa_default_isa, orig_t_insn.opcode);
      assemble_nop (size, fragP->fr_opcode);
      fragP->fr_var = 0;
    }
  else
    {
      IStack istack;
      int i;
      symbolS *lit_sym = NULL;
      int total_size = 0;
      int old_size;
      int diff;
      symbolS *gen_label = NULL;
      offsetT frag_offset;

      /* It does not fit.  Find something that does and 
         convert immediately.  */
      frag_offset = fragP->fr_opcode - fragP->fr_literal;
      istack_init (&istack);
      xg_assembly_relax (&istack, &orig_t_insn,
			 segP, fragP, frag_offset, min_steps, 0);

      old_size = xtensa_insn_length (xtensa_default_isa, orig_t_insn.opcode);

      /* Assemble this right inline.  */

      /* First, create the mapping from a label name to the REAL label.  */
      total_size = 0;
      for (i = 0; i < istack.ninsn; i++)
	{
	  TInsn *t_insn = &istack.insn[i];
	  int size = 0;
	  fragS *lit_frag;

	  switch (t_insn->insn_type)
	    {
	    case ITYPE_LITERAL:
	      if (lit_sym != NULL)
		as_bad (_("multiple literals in expansion"));
	      /* First find the appropriate space in the literal pool.  */
	      lit_frag = fragP->tc_frag_data.literal_frag;
	      if (lit_frag == NULL)
		as_bad (_("no registered fragment for literal"));
	      if (t_insn->ntok != 1)
		as_bad (_("number of literal tokens != 1"));

	      /* Set the literal symbol and add a fixup.  */
	      lit_sym = lit_frag->fr_symbol;
	      break;

	    case ITYPE_LABEL:
	      assert (gen_label == NULL);
	      gen_label = symbol_new (FAKE_LABEL_NAME, now_seg,
				      fragP->fr_opcode - fragP->fr_literal +
				      total_size, fragP);
	      break;

	    case ITYPE_INSN:
	      size = xtensa_insn_length (xtensa_default_isa, t_insn->opcode);
	      total_size += size;
	      break;
	    }
	}

      total_size = 0;
      for (i = 0; i < istack.ninsn; i++)
	{
	  TInsn *t_insn = &istack.insn[i];
	  fragS *lit_frag;
	  int size;
	  segT target_seg;

	  switch (t_insn->insn_type)
	    {
	    case ITYPE_LITERAL:
	      lit_frag = fragP->tc_frag_data.literal_frag;
	      /* already checked */
	      assert (lit_frag != NULL);
	      assert (lit_sym != NULL);
	      assert (t_insn->ntok == 1);
	      /* add a fixup */
	      target_seg = S_GET_SEGMENT (lit_sym);
	      assert (target_seg);
	      fix_new_exp_in_seg (target_seg, 0, lit_frag, 0, 4,
				  &t_insn->tok[0], FALSE, BFD_RELOC_32);
	      break;

	    case ITYPE_LABEL:
	      break;

	    case ITYPE_INSN:
	      xg_resolve_labels (t_insn, gen_label);
	      xg_resolve_literals (t_insn, lit_sym);
	      size = xtensa_insn_length (xtensa_default_isa, t_insn->opcode);
	      total_size += size;
	      xg_emit_insn_to_buf (t_insn, immed_instr, fragP,
				   immed_instr - fragP->fr_literal, TRUE);
	      immed_instr += size;
	      break;
	    }
	}

      diff = total_size - old_size;
      assert (diff >= 0);
      if (diff != 0)
	expanded = TRUE;
      assert (diff <= fragP->fr_var);
      fragP->fr_var -= diff;
      fragP->fr_fix += diff;
    }

  /* Clean it up.  */
  fragP->fr_var = 0;

  /* Check for undefined immediates in LOOP instructions.  */
  if (is_loop_opcode (orig_t_insn.opcode))
    {
      symbolS *sym;
      sym = orig_t_insn.tok[1].X_add_symbol;
      if (sym != NULL && !S_IS_DEFINED (sym))
	{
	  as_bad (_("unresolved loop target symbol: %s"), S_GET_NAME (sym));
	  return;
	}
      sym = orig_t_insn.tok[1].X_op_symbol;
      if (sym != NULL && !S_IS_DEFINED (sym))
	{
	  as_bad (_("unresolved loop target symbol: %s"), S_GET_NAME (sym));
	  return;
	}
    }

  if (expanded && is_loop_opcode (orig_t_insn.opcode))
    convert_frag_immed_finish_loop (segP, fragP, &orig_t_insn);

  if (expanded && is_direct_call_opcode (orig_t_insn.opcode))
    {
      /* Add an expansion note on the expanded instruction.  */
      fix_new_exp_in_seg (now_seg, 0, fragP, fr_opcode - fragP->fr_literal, 4,
			  &orig_t_insn.tok[0], TRUE,
			  BFD_RELOC_XTENSA_ASM_EXPAND);

    }
}


/* Add a new fix expression into the desired segment.  We have to
   switch to that segment to do this.  */

static fixS *
fix_new_exp_in_seg (new_seg, new_subseg,
		    frag, where, size, exp, pcrel, r_type)
     segT new_seg;
     subsegT new_subseg;
     fragS *frag;
     int where;
     int size;
     expressionS *exp;
     int pcrel;
     bfd_reloc_code_real_type r_type;
{
  fixS *new_fix;
  segT seg = now_seg;
  subsegT subseg = now_subseg;
  assert (new_seg != 0);
  subseg_set (new_seg, new_subseg);

  if (r_type == BFD_RELOC_32
      && exp->X_add_symbol
      && symbol_get_tc (exp->X_add_symbol)->plt == 1)
    {
      r_type = BFD_RELOC_XTENSA_PLT;
    }

  new_fix = fix_new_exp (frag, where, size, exp, pcrel, r_type);
  subseg_set (seg, subseg);
  return new_fix;
}


/* Relax a loop instruction so that it can span loop >256 bytes.  */
/* 
                  loop    as, .L1 
          .L0: 
                  rsr     as, LEND 
                  wsr     as, LBEG 
                  addi    as, as, lo8(label-.L1) 
                  addmi   as, as, mid8(label-.L1) 
                  wsr     as, LEND 
                  isync 
                  rsr     as, LCOUNT 
                  addi    as, as, 1 
          .L1: 
                  <<body>> 
          label:                                     */

static void
convert_frag_immed_finish_loop (segP, fragP, t_insn)
     segT segP;
     fragS *fragP;
     TInsn *t_insn;
{
  TInsn loop_insn;
  TInsn addi_insn;
  TInsn addmi_insn;
  unsigned long target;
  static xtensa_insnbuf insnbuf = NULL;
  unsigned int loop_length, loop_length_hi, loop_length_lo;
  xtensa_isa isa = xtensa_default_isa;
  addressT loop_offset;
  addressT addi_offset = 9;
  addressT addmi_offset = 12;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (isa);

  /* Get the loop offset.  */
  loop_offset = get_expanded_loop_offset (t_insn->opcode);
  /* Validate that there really is a LOOP at the loop_offset.  */
  tinsn_from_chars (&loop_insn, fragP->fr_opcode + loop_offset);

  if (!is_loop_opcode (loop_insn.opcode))
    {
      as_bad_where (fragP->fr_file, fragP->fr_line,
		    _("loop relaxation specification does not correspond"));
      assert (0);
    }
  addi_offset += loop_offset;
  addmi_offset += loop_offset;

  assert (t_insn->ntok == 2);
  target = get_expression_value (segP, &t_insn->tok[1]);

  know (symbolP);
  know (symbolP->sy_frag);
  know (!(S_GET_SEGMENT (symbolP) == absolute_section)
	|| symbol_get_frag (symbolP) == &zero_address_frag);

  loop_length = target - (fragP->fr_address + fragP->fr_fix);
  loop_length_hi = loop_length & ~0x0ff;
  loop_length_lo = loop_length & 0x0ff;
  if (loop_length_lo >= 128)
    {
      loop_length_lo -= 256;
      loop_length_hi += 256;
    }

  /* Because addmi sign-extends the immediate, 'loop_length_hi' can be at most 
     32512.  If the loop is larger than that, then we just fail.  */
  if (loop_length_hi > 32512)
    as_bad_where (fragP->fr_file, fragP->fr_line,
		  _("loop too long for LOOP instruction"));

  tinsn_from_chars (&addi_insn, fragP->fr_opcode + addi_offset);
  assert (addi_insn.opcode == xtensa_addi_opcode);

  tinsn_from_chars (&addmi_insn, fragP->fr_opcode + addmi_offset);
  assert (addmi_insn.opcode == xtensa_addmi_opcode);

  set_expr_const (&addi_insn.tok[2], loop_length_lo);
  tinsn_to_insnbuf (&addi_insn, insnbuf);
  
  fragP->tc_frag_data.is_insn = TRUE;
  xtensa_insnbuf_to_chars (isa, insnbuf, fragP->fr_opcode + addi_offset);

  set_expr_const (&addmi_insn.tok[2], loop_length_hi);
  tinsn_to_insnbuf (&addmi_insn, insnbuf);
  xtensa_insnbuf_to_chars (isa, insnbuf, fragP->fr_opcode + addmi_offset);
}


static offsetT
get_expression_value (segP, exp)
     segT segP;
     expressionS *exp;
{
  if (exp->X_op == O_constant)
    return exp->X_add_number;
  if (exp->X_op == O_symbol)
    {
      /* Find the fragment.  */
      symbolS *sym = exp->X_add_symbol;

      assert (S_GET_SEGMENT (sym) == segP
	      || S_GET_SEGMENT (sym) == absolute_section);

      return (S_GET_VALUE (sym) + exp->X_add_number);
    }
  as_bad (_("invalid expression evaluation type %d"), exp->X_op);
  return 0;
}


/* A map that keeps information on a per-subsegment basis.  This is
   maintained during initial assembly, but is invalid once the
   subsegments are smashed together.  I.E., it cannot be used during
   the relaxation.  */

typedef struct subseg_map_struct
{
  /* the key */
  segT seg;
  subsegT subseg;

  /* the data */
  unsigned flags;

  struct subseg_map_struct *next;
} subseg_map;

static subseg_map *sseg_map = NULL;


static unsigned
get_last_insn_flags (seg, subseg)
     segT seg;
     subsegT subseg;
{
  subseg_map *subseg_e;

  for (subseg_e = sseg_map; subseg_e != NULL; subseg_e = subseg_e->next)
    if (seg == subseg_e->seg && subseg == subseg_e->subseg)
      return subseg_e->flags;

  return 0;
}


static void
set_last_insn_flags (seg, subseg, fl, val)
     segT seg;
     subsegT subseg;
     unsigned fl;
     bfd_boolean val;
{
  subseg_map *subseg_e;

  for (subseg_e = sseg_map; subseg_e; subseg_e = subseg_e->next)
    if (seg == subseg_e->seg && subseg == subseg_e->subseg)
      break;

  if (!subseg_e)
    {
      subseg_e = (subseg_map *) xmalloc (sizeof (subseg_map));
      memset (subseg_e, 0, sizeof (subseg_map));
      subseg_e->seg = seg;
      subseg_e->subseg = subseg;
      subseg_e->flags = 0;
      subseg_e->next = sseg_map;
      sseg_map = subseg_e;
    }

  if (val)
    subseg_e->flags |= fl;
  else
    subseg_e->flags &= ~fl;
}


/* Segment Lists and emit_state Stuff.  */

/* Remove the segment from the global sections list.  */

static void
xtensa_remove_section (sec)
     segT sec;
{
  /* Handle brain-dead bfd_section_list_remove macro, which
     expect the address of the prior section's "next" field, not
     just the address of the section to remove.  */

  segT *ps_next_ptr = &stdoutput->sections;
  while (*ps_next_ptr != sec && *ps_next_ptr != NULL) 
    ps_next_ptr = &(*ps_next_ptr)->next;
  
  assert (*ps_next_ptr != NULL);

  bfd_section_list_remove (stdoutput, ps_next_ptr);
}


static void
xtensa_insert_section (after_sec, sec)
     segT after_sec;
     segT sec;
{
  segT *after_sec_next;
  if (after_sec == NULL)
    after_sec_next = &stdoutput->sections;
  else
    after_sec_next = &after_sec->next;

  bfd_section_list_insert (stdoutput, after_sec_next, sec);
}


static void
xtensa_move_seg_list_to_beginning (head)
     seg_list *head;
{
  head = head->next;
  while (head)
    {
      segT literal_section = head->seg;

      /* Move the literal section to the front of the section list.  */
      assert (literal_section);
      xtensa_remove_section (literal_section);
      xtensa_insert_section (NULL, literal_section);

      head = head->next;
    }
}


void
xtensa_move_literals ()
{
  seg_list *segment;
  frchainS *frchain_from, *frchain_to;
  fragS *search_frag, *next_frag, *last_frag, *literal_pool, *insert_after;
  fragS **frag_splice;
  emit_state state;
  segT dest_seg;
  fixS *fix, *next_fix, **fix_splice;
  sym_list *lit;

  mark_literal_frags (literal_head->next);
  mark_literal_frags (init_literal_head->next);
  mark_literal_frags (fini_literal_head->next);

  if (use_literal_section)
    return;

  segment = literal_head->next;
  while (segment)
    {
      frchain_from = seg_info (segment->seg)->frchainP;
      search_frag = frchain_from->frch_root;
      literal_pool = NULL;
      frchain_to = NULL;
      frag_splice = &(frchain_from->frch_root);

      while (!search_frag->tc_frag_data.literal_frag)
	{
	  assert (search_frag->fr_fix == 0
		  || search_frag->fr_type == rs_align);
	  search_frag = search_frag->fr_next;
	}

      assert (search_frag->tc_frag_data.literal_frag->fr_subtype
	      == RELAX_LITERAL_POOL_BEGIN);
      xtensa_switch_section_emit_state (&state, segment->seg, 0);

      /* Make sure that all the frags in this series are closed, and
	 that there is at least one left over of zero-size.  This
	 prevents us from making a segment with an frchain without any
	 frags in it.  */
      frag_variant (rs_fill, 0, 0, 0, NULL, 0, NULL);
      last_frag = frag_now;
      frag_variant (rs_fill, 0, 0, 0, NULL, 0, NULL);

      while (search_frag != frag_now) 
	{
	  next_frag = search_frag->fr_next;

	  /* First, move the frag out of the literal section and 
	     to the appropriate place.  */
	  if (search_frag->tc_frag_data.literal_frag)
	    {
	      literal_pool = search_frag->tc_frag_data.literal_frag;
	      assert (literal_pool->fr_subtype == RELAX_LITERAL_POOL_BEGIN);
	      /* Note that we set this fr_var to be a fix 
		 chain when we created the literal pool location
		 as RELAX_LITERAL_POOL_BEGIN.  */
	      frchain_to = (frchainS *) literal_pool->fr_var;
	    }
	  insert_after = literal_pool;
	  
	  while (insert_after->fr_next->fr_subtype != RELAX_LITERAL_POOL_END)
	    insert_after = insert_after->fr_next;

	  dest_seg = (segT) insert_after->fr_next->fr_var;
	  
	  *frag_splice = next_frag;
	  search_frag->fr_next = insert_after->fr_next;
	  insert_after->fr_next = search_frag;
	  search_frag->tc_frag_data.lit_seg = dest_seg;

	  /* Now move any fixups associated with this frag to the
	     right section.  */
	  fix = frchain_from->fix_root;
	  fix_splice = &(frchain_from->fix_root);
	  while (fix)
	    {
	      next_fix = fix->fx_next;
	      if (fix->fx_frag == search_frag)
		{
		  *fix_splice = next_fix;
		  fix->fx_next = frchain_to->fix_root;
		  frchain_to->fix_root = fix;
		  if (frchain_to->fix_tail == NULL)
		    frchain_to->fix_tail = fix;
		}
	      else
		fix_splice = &(fix->fx_next);
	      fix = next_fix;
	    }
	  search_frag = next_frag;
	}

      if (frchain_from->fix_root != NULL)
	{
	  frchain_from = seg_info (segment->seg)->frchainP;
	  as_warn (_("fixes not all moved from %s"), segment->seg->name);

	  assert (frchain_from->fix_root == NULL);
	}
      frchain_from->fix_tail = NULL;
      xtensa_restore_emit_state (&state);
      segment = segment->next;
    }

  /* Now fix up the SEGMENT value for all the literal symbols.  */
  for (lit = literal_syms; lit; lit = lit->next)
    {
      symbolS *lit_sym = lit->sym;
      segT dest_seg = symbol_get_frag (lit_sym)->tc_frag_data.lit_seg;
      S_SET_SEGMENT (lit_sym, dest_seg);
    }
}


/* Walk over all the frags for segments in a list and mark them as
   containing literals.  As clunky as this is, we can't rely on frag_var
   and frag_variant to get called in all situations.  */

static void
mark_literal_frags (segment)
     seg_list *segment;
{
  frchainS *frchain_from;
  fragS *search_frag;

  while (segment)
    {
      frchain_from = seg_info (segment->seg)->frchainP;
      search_frag = frchain_from->frch_root;
      while (search_frag) 
	{
	  search_frag->tc_frag_data.is_literal = TRUE;
	  search_frag = search_frag->fr_next;
	}
      segment = segment->next;
    }
}


static void
xtensa_reorder_seg_list (head, after)
     seg_list *head;
     segT after;
{
  /* Move all of the sections in the section list to come
     after "after" in the gnu segment list.  */

  head = head->next;
  while (head)
    {
      segT literal_section = head->seg;

      /* Move the literal section after "after".  */
      assert (literal_section);
      if (literal_section != after)
	{
	  xtensa_remove_section (literal_section);
	  xtensa_insert_section (after, literal_section);
	}

      head = head->next;
    }
}


/* Push all the literal segments to the end of the gnu list.  */

void
xtensa_reorder_segments ()
{
  segT sec;
  segT last_sec;
  int old_count = 0;
  int new_count = 0;

  for (sec = stdoutput->sections; sec != NULL; sec = sec->next)
    old_count++;

  /* Now that we have the last section, push all the literal
     sections to the end.  */
  last_sec = get_last_sec ();
  xtensa_reorder_seg_list (literal_head, last_sec);
  xtensa_reorder_seg_list (init_literal_head, last_sec);
  xtensa_reorder_seg_list (fini_literal_head, last_sec);

  /* Now perform the final error check.  */
  for (sec = stdoutput->sections; sec != NULL; sec = sec->next)
    new_count++;
  assert (new_count == old_count);
}


segT
get_last_sec ()
{
  segT last_sec = stdoutput->sections;
  while (last_sec->next != NULL)
    last_sec = last_sec->next;

  return last_sec;
}


/* Change the emit state (seg, subseg, and frag related stuff) to the
   correct location.  Return a emit_state which can be passed to
   xtensa_restore_emit_state to return to current fragment.  */

void
xtensa_switch_to_literal_fragment (result)
     emit_state *result;
{
  /* When we mark a literal pool location, we want to put a frag in
     the literal pool that points to it.  But to do that, we want to
     switch_to_literal_fragment.  But literal sections don't have
     literal pools, so their location is always null, so we would
     recurse forever.  This is kind of hacky, but it works.  */

  static bfd_boolean recursive = FALSE;
  fragS *pool_location = get_literal_pool_location (now_seg);
  bfd_boolean is_init = 
    (now_seg && !strcmp (segment_name (now_seg), INIT_SECTION_NAME));

  bfd_boolean is_fini = 
    (now_seg && !strcmp (segment_name (now_seg), FINI_SECTION_NAME));
  

  if (pool_location == NULL 
      && !use_literal_section 
      && !recursive
      && !is_init && ! is_fini)
    {
      as_warn (_("inlining literal pool; "
		 "specify location with .literal_position."));
      recursive = TRUE;
      xtensa_mark_literal_pool_location ();
      recursive = FALSE;
    }

  /* Special case: If we are in the ".fini" or ".init" section, then
     we will ALWAYS be generating to the ".fini.literal" and
     ".init.literal" sections.  */

  if (is_init)
    {
      cache_literal_section (init_literal_head,
			     default_lit_sections.init_lit_seg_name,
			     &default_lit_sections.init_lit_seg);
      xtensa_switch_section_emit_state (result,
					default_lit_sections.init_lit_seg, 0);
    }
  else if (is_fini)
    {
      cache_literal_section (fini_literal_head,
			     default_lit_sections.fini_lit_seg_name,
			     &default_lit_sections.fini_lit_seg);
      xtensa_switch_section_emit_state (result,
					default_lit_sections.fini_lit_seg, 0);
    }
  else 
    {
      cache_literal_section (literal_head,
			     default_lit_sections.lit_seg_name,
			     &default_lit_sections.lit_seg);
      xtensa_switch_section_emit_state (result,
					default_lit_sections.lit_seg, 0);
    }

  if (!use_literal_section &&
      !is_init && !is_fini &&
      get_literal_pool_location (now_seg) != pool_location)
    {
      /* Close whatever frag is there.  */
      frag_variant (rs_fill, 0, 0, 0, NULL, 0, NULL);
      frag_now->tc_frag_data.literal_frag = pool_location;
      frag_variant (rs_fill, 0, 0, 0, NULL, 0, NULL);
    }

  /* Do a 4 byte align here.  */
  frag_align (2, 0, 0);
}


/* Call this function before emitting data into the literal section.
   This is a helper function for xtensa_switch_to_literal_fragment.
   This is similar to a .section new_now_seg subseg. */

void
xtensa_switch_section_emit_state (state, new_now_seg, new_now_subseg)
     emit_state *state;
     segT new_now_seg;
     subsegT new_now_subseg;
{
  state->name = now_seg->name;
  state->now_seg = now_seg;
  state->now_subseg = now_subseg;
  state->generating_literals = generating_literals;
  generating_literals++;
  subseg_new (segment_name (new_now_seg), new_now_subseg);
}


/* Use to restore the emitting into the normal place.  */

void
xtensa_restore_emit_state (state)
     emit_state *state;
{
  generating_literals = state->generating_literals;
  subseg_new (state->name, state->now_subseg);
}


/* Get a segment of a given name.  If the segment is already
   present, return it; otherwise, create a new one.  */

static void
cache_literal_section (head, name, seg)
     seg_list *head;
     const char *name;
     segT *seg;
{
  segT current_section = now_seg;
  int current_subsec = now_subseg;

  if (*seg != 0)
    return;
  *seg = retrieve_literal_seg (head, name);
  subseg_set (current_section, current_subsec);
}


/* Get a segment of a given name.  If the segment is already
   present, return it; otherwise, create a new one.  */

static segT
retrieve_literal_seg (head, name)
     seg_list *head;
     const char *name;
{
  segT ret = 0;

  assert (head);

  ret = seg_present (name);
  if (!ret)
    {
      ret = subseg_new (name, (subsegT) 0);
      add_seg_list (head, ret);
      bfd_set_section_flags (stdoutput, ret, SEC_HAS_CONTENTS |
			     SEC_READONLY | SEC_ALLOC | SEC_LOAD | SEC_CODE);
      bfd_set_section_alignment (stdoutput, ret, 2);
    }

  return ret;
}


/* Return a segment of a given name if it is present.  */

static segT
seg_present (name)
     const char *name;
{
  segT seg;
  seg = stdoutput->sections;

  while (seg)
    {
      if (!strcmp (segment_name (seg), name))
	return seg;
      seg = seg->next;
    }

  return 0;
}


/* Add a segment to a segment list.  */

static void
add_seg_list (head, seg)
     seg_list *head;
     segT seg;
{
  seg_list *n;
  n = (seg_list *) xmalloc (sizeof (seg_list));
  assert (n);

  n->seg = seg;
  n->next = head->next;
  head->next = n;
}


/* Set up Property Tables after Relaxation.  */

#define XTENSA_INSN_SEC_NAME ".xt.insn"
#define XTENSA_LIT_SEC_NAME ".xt.lit"

void
xtensa_post_relax_hook ()
{
  xtensa_move_seg_list_to_beginning (literal_head);
  xtensa_move_seg_list_to_beginning (init_literal_head);
  xtensa_move_seg_list_to_beginning (fini_literal_head);

  xtensa_create_property_segments (get_frag_is_insn,
				   XTENSA_INSN_SEC_NAME,
				   xt_insn_sec);
  xtensa_create_property_segments (get_frag_is_literal,
				   XTENSA_LIT_SEC_NAME,
				   xt_literal_sec);
}


static bfd_boolean
get_frag_is_literal (fragP)
     const fragS *fragP;
{
  assert (fragP != NULL);
  return (fragP->tc_frag_data.is_literal);
}
 

static bfd_boolean
get_frag_is_insn (fragP)
     const fragS *fragP;
{
  assert (fragP != NULL);
  return (fragP->tc_frag_data.is_insn);
}


static void
xtensa_create_property_segments (property_function, section_name_base, 
				 sec_type)
     frag_predicate property_function;
     const char * section_name_base;
     xt_section_type sec_type;
{
  segT *seclist;

  /* Walk over all of the current segments.
     Walk over each fragment
      For each fragment that has instructions
      Build an instruction record (append where possible).  */

  for (seclist = &stdoutput->sections;
       seclist && *seclist;
       seclist = &(*seclist)->next)
    {
      segT sec = *seclist;
      if (section_has_property (sec, property_function))
	{
	  char *property_section_name =
	    xtensa_get_property_section_name (sec, section_name_base);
	  segT insn_sec = retrieve_xtensa_section (property_section_name);
	  segment_info_type *xt_seg_info = retrieve_segment_info (insn_sec);
	  xtensa_block_info **xt_blocks = 
	    &xt_seg_info->tc_segment_info_data.blocks[sec_type];
	  /* Walk over all of the frchains here and add new sections.  */
	  add_xt_block_frags (sec, insn_sec, xt_blocks, property_function);
	}
    }

  /* Now we fill them out....  */

  for (seclist = &stdoutput->sections;
       seclist && *seclist;
       seclist = &(*seclist)->next)
    {
      segment_info_type *seginfo;
      xtensa_block_info *block;
      segT sec = *seclist;
      seginfo = seg_info (sec);
      block = seginfo->tc_segment_info_data.blocks[sec_type];

      if (block)
	{
	  xtensa_block_info *cur_block;
	  /* This is a section with some data.  */
	  size_t num_recs = 0;
	  size_t rec_size;

	  for (cur_block = block; cur_block; cur_block = cur_block->next)
	    num_recs++;

	  rec_size = num_recs * 8;
	  bfd_set_section_size (stdoutput, sec, rec_size);

	  /* In order to make this work with the assembler, we have to
	     build some frags and then build the "fixups" for it.  It
	     would be easier to just set the contents then set the
	     arlents.  */

	  if (num_recs)
	    {
	      /* Allocate a fragment and leak it.  */
	      fragS *fragP;
	      size_t frag_size;
	      fixS *fixes;
	      frchainS *frchainP;
	      size_t i;
	      char *frag_data;

	      frag_size = sizeof (fragS) + rec_size;
	      fragP = (fragS *) xmalloc (frag_size);

	      memset (fragP, 0, frag_size);
	      fragP->fr_address = 0;
	      fragP->fr_next = NULL;
	      fragP->fr_fix = rec_size;
	      fragP->fr_var = 0;
	      fragP->fr_type = rs_fill;
	      /* the rest are zeros */

	      frchainP = seginfo->frchainP;
	      frchainP->frch_root = fragP;
	      frchainP->frch_last = fragP;

	      fixes = (fixS *) xmalloc (sizeof (fixS) * num_recs);
	      memset (fixes, 0, sizeof (fixS) * num_recs);

	      seginfo->fix_root = fixes;
	      seginfo->fix_tail = &fixes[num_recs - 1];
	      cur_block = block;
	      frag_data = &fragP->fr_literal[0];
	      for (i = 0; i < num_recs; i++)
		{
		  fixS *fix = &fixes[i];
		  assert (cur_block);

		  /* Write the fixup.  */
		  if (i != num_recs - 1)
		    fix->fx_next = &fixes[i + 1];
		  else
		    fix->fx_next = NULL;
		  fix->fx_size = 4;
		  fix->fx_done = 0;
		  fix->fx_frag = fragP;
		  fix->fx_where = i * 8;
		  fix->fx_addsy = section_symbol (cur_block->sec);
		  fix->fx_offset = cur_block->offset;
		  fix->fx_r_type = BFD_RELOC_32;
		  fix->fx_file = "Internal Assembly";
		  fix->fx_line = 0;

		  /* Write the length.  */
		  md_number_to_chars (&frag_data[4 + 8 * i],
				      cur_block->size, 4);
		  cur_block = cur_block->next;
		}
	    }
	}
    }
}


segment_info_type *
retrieve_segment_info (seg)
     segT seg;
{
  segment_info_type *seginfo;
  seginfo = (segment_info_type *) bfd_get_section_userdata (stdoutput, seg);
  if (!seginfo)
    {
      frchainS *frchainP;

      seginfo = (segment_info_type *) xmalloc (sizeof (*seginfo));
      memset ((PTR) seginfo, 0, sizeof (*seginfo));
      seginfo->fix_root = NULL;
      seginfo->fix_tail = NULL;
      seginfo->bfd_section = seg;
      seginfo->sym = 0;
      /* We will not be dealing with these, only our special ones.  */
#if 0
      if (seg == bfd_abs_section_ptr)
	abs_seg_info = seginfo;
      else if (seg == bfd_und_section_ptr)
	und_seg_info = seginfo;
      else
#endif
	bfd_set_section_userdata (stdoutput, seg, (PTR) seginfo);
#if 0
      seg_fix_rootP = &segment_info[seg].fix_root;
      seg_fix_tailP = &segment_info[seg].fix_tail;
#endif

      frchainP = (frchainS *) xmalloc (sizeof (frchainS));
      frchainP->frch_root = NULL;
      frchainP->frch_last = NULL;
      frchainP->frch_next = NULL;
      frchainP->frch_seg = seg;
      frchainP->frch_subseg = 0;
      frchainP->fix_root = NULL;
      frchainP->fix_tail = NULL;
      /* Do not init the objstack.  */
      /* obstack_begin (&frchainP->frch_obstack, chunksize); */
      /* frchainP->frch_frag_now = fragP; */
      frchainP->frch_frag_now = NULL;

      seginfo->frchainP = frchainP;
    }

  return seginfo;
}


segT
retrieve_xtensa_section (sec_name)
     char *sec_name;
{
  bfd *abfd = stdoutput;
  flagword flags, out_flags, link_once_flags;
  segT s;

  flags = bfd_get_section_flags (abfd, now_seg);
  link_once_flags = (flags & SEC_LINK_ONCE);
  if (link_once_flags)
    link_once_flags |= (flags & SEC_LINK_DUPLICATES);
  out_flags = (SEC_RELOC | SEC_HAS_CONTENTS | SEC_READONLY | link_once_flags);

  s = bfd_make_section_old_way (abfd, sec_name);
  if (s == NULL)
    as_bad (_("could not create section %s"), sec_name);
  if (!bfd_set_section_flags (abfd, s, out_flags))
    as_bad (_("invalid flag combination on section %s"), sec_name);

  return s;
}


bfd_boolean
section_has_property (sec, property_function)
     segT sec;
     frag_predicate property_function;
{
  segment_info_type *seginfo = seg_info (sec);
  fragS *fragP;

  if (seginfo && seginfo->frchainP)
    {
      for (fragP = seginfo->frchainP->frch_root; fragP; fragP = fragP->fr_next)
	{
	  if (property_function (fragP)
	      && (fragP->fr_type != rs_fill || fragP->fr_fix != 0))
	    return TRUE;
	}
    }
  return FALSE;
}


/* Two types of block sections exist right now: literal and insns.  */

void
add_xt_block_frags (sec, xt_block_sec, xt_block, property_function)
     segT sec;
     segT xt_block_sec;
     xtensa_block_info **xt_block;
     frag_predicate property_function;
{
  segment_info_type *seg_info;
  segment_info_type *xt_seg_info;
  bfd_vma seg_offset;
  fragS *fragP;

  xt_seg_info = retrieve_segment_info (xt_block_sec);
  seg_info = retrieve_segment_info (sec);

  /* Build it if needed.  */
  while (*xt_block != NULL)
    xt_block = &(*xt_block)->next;
  /* We are either at NULL at the beginning or at the end.  */

  /* Walk through the frags.  */
  seg_offset = 0;

  if (seg_info->frchainP)
    {
      for (fragP = seg_info->frchainP->frch_root;
	   fragP;
	   fragP = fragP->fr_next)
	{
	  if (property_function (fragP)
	      && (fragP->fr_type != rs_fill || fragP->fr_fix != 0))
	    {
	      if (*xt_block != NULL)
		{
		  if ((*xt_block)->offset + (*xt_block)->size
		      == fragP->fr_address)
		    (*xt_block)->size += fragP->fr_fix;
		  else
		    xt_block = &((*xt_block)->next);
		}
	      if (*xt_block == NULL)
		{
		  xtensa_block_info *new_block = (xtensa_block_info *)
		    xmalloc (sizeof (xtensa_block_info));
		  new_block->sec = sec;
		  new_block->offset = fragP->fr_address;
		  new_block->size = fragP->fr_fix;
		  new_block->next = NULL;
		  *xt_block = new_block;
		}
	    }
	}
    }
}


/* Instruction Stack Functions (from "xtensa-istack.h").  */

void
istack_init (stack)
     IStack *stack;
{
  memset (stack, 0, sizeof (IStack));
  stack->ninsn = 0;
}


bfd_boolean
istack_empty (stack)
     IStack *stack;
{
  return (stack->ninsn == 0);
}


bfd_boolean
istack_full (stack)
     IStack *stack;
{
  return (stack->ninsn == MAX_ISTACK);
}


/* Return a pointer to the top IStack entry.
   It is an error to call this if istack_empty () is true. */

TInsn *
istack_top (stack)
     IStack *stack;
{
  int rec = stack->ninsn - 1;
  assert (!istack_empty (stack));
  return &stack->insn[rec];
}


/* Add a new TInsn to an IStack.
   It is an error to call this if istack_full () is true.  */

void
istack_push (stack, insn)
     IStack *stack;
     TInsn *insn;
{
  int rec = stack->ninsn;
  assert (!istack_full (stack));
  tinsn_copy (&stack->insn[rec], insn);
  stack->ninsn++;
}


/* Clear space for the next TInsn on the IStack and return a pointer
   to it.  It is an error to call this if istack_full () is true.  */

TInsn *
istack_push_space (stack)
     IStack *stack;
{
  int rec = stack->ninsn;
  TInsn *insn;
  assert (!istack_full (stack));
  insn = &stack->insn[rec];
  memset (insn, 0, sizeof (TInsn));
  stack->ninsn++;
  return insn;
}


/* Remove the last pushed instruction.  It is an error to call this if
   istack_empty () returns true.  */

void
istack_pop (stack)
     IStack *stack;
{
  int rec = stack->ninsn - 1;
  assert (!istack_empty (stack));
  stack->ninsn--;
  memset (&stack->insn[rec], 0, sizeof (TInsn));
}


/* TInsn functions.  */

void
tinsn_init (dst)
     TInsn *dst;
{
  memset (dst, 0, sizeof (TInsn));
}


void
tinsn_copy (dst, src)
     TInsn *dst;
     const TInsn *src;
{
  tinsn_init (dst);
  memcpy (dst, src, sizeof (TInsn));
}


/* Get the ``num''th token of the TInsn.
   It is illegal to call this if num > insn->ntoks.  */

expressionS *
tinsn_get_tok (insn, num)
     TInsn *insn;
     int num;
{
  assert (num < insn->ntok);
  return &insn->tok[num];
}


/* Return true if ANY of the operands in the insn are symbolic.  */

static bfd_boolean
tinsn_has_symbolic_operands (insn)
     const TInsn *insn;
{
  int i;
  int n = insn->ntok;

  assert (insn->insn_type == ITYPE_INSN);

  for (i = 0; i < n; ++i)
    {
      switch (insn->tok[i].X_op)
	{
	case O_register:
	case O_constant:
	  break;
	default:
	  return TRUE;
	}
    }
  return FALSE;
}


bfd_boolean
tinsn_has_invalid_symbolic_operands (insn)
     const TInsn *insn;
{
  int i;
  int n = insn->ntok;

  assert (insn->insn_type == ITYPE_INSN);

  for (i = 0; i < n; ++i)
    {
      switch (insn->tok[i].X_op)
	{
	case O_register:
	case O_constant:
	  break;
	default:
	  if (i == get_relaxable_immed (insn->opcode))
	    break;
	  as_bad (_("invalid symbolic operand %d on '%s'"),
		  i, xtensa_opcode_name (xtensa_default_isa, insn->opcode));
	  return TRUE;
	}
    }
  return FALSE;
}


/* For assembly code with complex expressions (e.g. subtraction),
   we have to build them in the literal pool so that
   their results are calculated correctly after relaxation.
   The relaxation only handles expressions that
   boil down to SYMBOL + OFFSET.  */

static bfd_boolean
tinsn_has_complex_operands (insn)
     const TInsn *insn;
{
  int i;
  int n = insn->ntok;
  assert (insn->insn_type == ITYPE_INSN);
  for (i = 0; i < n; ++i)
    {
      switch (insn->tok[i].X_op)
	{
	case O_register:
	case O_constant:
	case O_symbol:
	  break;
	default:
	  return TRUE;
	}
    }
  return FALSE;
}


/* Convert the constant operands in the t_insn to insnbuf.
   Return true if there is a symbol in the immediate field.

   Before this is called, 
   1) the number of operands are correct
   2) the t_insn is a ITYPE_INSN
   3) ONLY the relaxable_ is built
   4) All operands are O_constant, O_symbol.  All constants fit
   The return value tells whether there are any remaining O_symbols.  */

static bfd_boolean
tinsn_to_insnbuf (t_insn, insnbuf)
     TInsn *t_insn;
     xtensa_insnbuf insnbuf;
{
  xtensa_isa isa = xtensa_default_isa;
  xtensa_opcode opcode = t_insn->opcode;
  bfd_boolean has_fixup = FALSE;
  int noperands = xtensa_num_operands (isa, opcode);
  int i;
  uint32 opnd_value;
  char *file_name;
  int line;

  assert (t_insn->insn_type == ITYPE_INSN);
  if (noperands != t_insn->ntok)
    as_fatal (_("operand number mismatch"));

  xtensa_encode_insn (isa, opcode, insnbuf);

  for (i = 0; i < noperands; ++i)
    {
      expressionS *expr = &t_insn->tok[i];
      xtensa_operand operand = xtensa_get_operand (isa, opcode, i);
      switch (expr->X_op)
	{
	case O_register:
	  /* The register number has already been checked in  
	     expression_maybe_register, so we don't need to check here.  */
	  opnd_value = expr->X_add_number;
	  (void) xtensa_operand_encode (operand, &opnd_value);
	  xtensa_operand_set_field (operand, insnbuf, opnd_value);
	  break;

	case O_constant:
	  as_where (&file_name, &line);
	  /* It is a constant and we called this function,
	     then we have to try to fit it.  */
	  xtensa_insnbuf_set_operand (insnbuf, opcode, operand,
				      expr->X_add_number, file_name, line);
	  break;

	case O_symbol:
	default:
	  has_fixup = TRUE;
	  break;
	}
    }
  return has_fixup;
}


/* Check the instruction arguments.  Return true on failure.  */

bfd_boolean
tinsn_check_arguments (insn)
     const TInsn *insn;
{
  xtensa_isa isa = xtensa_default_isa;
  xtensa_opcode opcode = insn->opcode;

  if (opcode == XTENSA_UNDEFINED)
    {
      as_bad (_("invalid opcode"));
      return TRUE;
    }

  if (xtensa_num_operands (isa, opcode) > insn->ntok)
    {
      as_bad (_("too few operands"));
      return TRUE;
    }

  if (xtensa_num_operands (isa, opcode) < insn->ntok)
    {
      as_bad (_("too many operands"));
      return TRUE;
    }
  return FALSE;
}


/* Load an instruction from its encoded form.  */

static void
tinsn_from_chars (t_insn, f)
     TInsn *t_insn;
     char *f;
{
  static xtensa_insnbuf insnbuf = NULL;
  int i;
  xtensa_opcode opcode;
  xtensa_isa isa = xtensa_default_isa;

  if (!insnbuf)
    insnbuf = xtensa_insnbuf_alloc (isa);

  xtensa_insnbuf_from_chars (isa, insnbuf, f);
  opcode = xtensa_decode_insn (isa, insnbuf);

  /* Find the immed.  */
  tinsn_init (t_insn);
  t_insn->insn_type = ITYPE_INSN;
  t_insn->is_specific_opcode = FALSE;	/* Must not be specific.  */
  t_insn->opcode = opcode;
  t_insn->ntok = xtensa_num_operands (isa, opcode);
  for (i = 0; i < t_insn->ntok; i++)
    {
      set_expr_const (&t_insn->tok[i],
		      xtensa_insnbuf_get_operand (insnbuf, opcode, i));
    }
}


/* Read the value of the relaxable immed from the fr_symbol and fr_offset.  */

static void
tinsn_immed_from_frag (t_insn, fragP)
     TInsn *t_insn;
     fragS *fragP;
{
  xtensa_opcode opcode = t_insn->opcode;
  int opnum;

  if (fragP->fr_symbol)
    {
      opnum = get_relaxable_immed (opcode);
      set_expr_symbol_offset (&t_insn->tok[opnum],
			      fragP->fr_symbol, fragP->fr_offset);
    }
}


static int
get_num_stack_text_bytes (istack)
     IStack *istack;
{
  int i;
  int text_bytes = 0;

  for (i = 0; i < istack->ninsn; i++)
    {
      TInsn *t_insn = &istack->insn[i];
      if (t_insn->insn_type == ITYPE_INSN)
	text_bytes += xg_get_insn_size (t_insn);
    }
  return text_bytes;
}


static int
get_num_stack_literal_bytes (istack)
     IStack *istack;
{
  int i;
  int lit_bytes = 0;

  for (i = 0; i < istack->ninsn; i++)
    {
      TInsn *t_insn = &istack->insn[i];

      if (t_insn->insn_type == ITYPE_LITERAL && t_insn->ntok == 1)
	lit_bytes += 4;
    }
  return lit_bytes;
}


/* Expression utilities.  */

/* Return true if the expression is an integer constant.  */

bfd_boolean
expr_is_const (s)
     const expressionS *s;
{
  return (s->X_op == O_constant);
}


/* Get the expression constant.
   Calling this is illegal if expr_is_const () returns true.  */

offsetT
get_expr_const (s)
     const expressionS *s;
{
  assert (expr_is_const (s));
  return s->X_add_number;
}


/* Set the expression to a constant value.  */

void
set_expr_const (s, val)
     expressionS *s;
     offsetT val;
{
  s->X_op = O_constant;
  s->X_add_number = val;
  s->X_add_symbol = NULL;
  s->X_op_symbol = NULL;
}


/* Set the expression to a symbol + constant offset.  */

void
set_expr_symbol_offset (s, sym, offset)
     expressionS *s;
     symbolS *sym;
     offsetT offset;
{
  s->X_op = O_symbol;
  s->X_add_symbol = sym;
  s->X_op_symbol = NULL;	/* unused */
  s->X_add_number = offset;
}


bfd_boolean
expr_is_equal (s1, s2)
     expressionS *s1;
     expressionS *s2;
{
  if (s1->X_op != s2->X_op)
    return FALSE;
  if (s1->X_add_symbol != s2->X_add_symbol)
    return FALSE;
  if (s1->X_op_symbol != s2->X_op_symbol)
    return FALSE;
  if (s1->X_add_number != s2->X_add_number)
    return FALSE;
  return TRUE;
}


static void
copy_expr (dst, src)
     expressionS *dst;
     const expressionS *src;
{
  memcpy (dst, src, sizeof (expressionS));
}


/* Support for Tensilica's "--rename-section" option.  */

#ifdef XTENSA_SECTION_RENAME

struct rename_section_struct
{
  char *old_name;
  char *new_name;
  struct rename_section_struct *next;
};

static struct rename_section_struct *section_rename;


/* Parse the string oldname=new_name:oldname2=new_name2 
   and call add_section_rename.  */

void
build_section_rename (arg)
     const char *arg;
{
  char *this_arg = NULL;
  char *next_arg = NULL;

  for (this_arg = strdup (arg); this_arg != NULL; this_arg = next_arg)
    {
      if (this_arg)
	{
	  next_arg = strchr (this_arg, ':');
	  if (next_arg)
	    {
	      *next_arg = '\0';
	      next_arg++;
	    }
	}
      {
	char *old_name = this_arg;
	char *new_name = strchr (this_arg, '=');

	if (*old_name == '\0')
	  {
	    as_warn (_("ignoring extra '-rename-section' delimiter ':'"));
	    continue;
	  }
	if (!new_name || new_name[1] == '\0')
	  {
	    as_warn (_("ignoring invalid '-rename-section' "
		       "specification: '%s'"), old_name);
	    continue;
	  }
	*new_name = '\0';
	new_name++;
	add_section_rename (old_name, new_name);
      }
    }
}


static void
add_section_rename (old_name, new_name)
     char *old_name;
     char *new_name;
{
  struct rename_section_struct *r = section_rename;

  /* Check for invalid section renaming.  */
  for (r = section_rename; r != NULL; r = r->next)
    {
      if (strcmp (r->old_name, old_name) == 0)
	as_bad (_("section %s renamed multiple times"), old_name);
      if (strcmp (r->new_name, new_name) == 0)
	as_bad (_("multiple sections remapped to output section %s"),
		new_name);
    }

  /* Now add it.  */
  r = (struct rename_section_struct *)
    xmalloc (sizeof (struct rename_section_struct));
  r->old_name = strdup (old_name);
  r->new_name = strdup (new_name);
  r->next = section_rename;
  section_rename = r;
}


const char *
xtensa_section_rename (name)
     const char *name;
{
  struct rename_section_struct *r = section_rename;

  for (r = section_rename; r != NULL; r = r->next)
    if (strcmp (r->old_name, name) == 0)
      return r->new_name;

  return name;
}

#endif /* XTENSA_SECTION_RENAME */
