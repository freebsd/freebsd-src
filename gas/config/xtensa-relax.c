/* Table of relaxations for Xtensa assembly.
   Copyright 2003, 2004, 2005 Free Software Foundation, Inc.

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
   the Free Software Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* This file contains the code for generating runtime data structures
   for relaxation pattern matching from statically specified strings.
   Each action contains an instruction pattern to match and
   preconditions for the match as well as an expansion if the pattern
   matches.  The preconditions can specify that two operands are the
   same or an operand is a specific constant or register.  The expansion
   uses the bound variables from the pattern to specify that specific
   operands from the pattern should be used in the result.

   The code determines whether the condition applies to a constant or
   a register depending on the type of the operand.  You may get
   unexpected results if you don't match the rule against the operand
   type correctly.

   The patterns match a language like:

   INSN_PATTERN ::= INSN_TEMPL ( '|' PRECOND )* ( '?' OPTIONPRED )*
   INSN_TEMPL   ::= OPCODE ' ' [ OPERAND (',' OPERAND)* ]
   OPCODE       ::=  id
   OPERAND      ::= CONSTANT | VARIABLE | SPECIALFN '(' VARIABLE ')'
   SPECIALFN    ::= 'HI24S' | 'F32MINUS' | 'LOW8'
                    | 'HI16' | 'LOW16'
   VARIABLE     ::= '%' id
   PRECOND      ::= OPERAND CMPOP OPERAND
   CMPOP        ::= '==' | '!='
   OPTIONPRED   ::= OPTIONNAME ('+' OPTIONNAME)
   OPTIONNAME   ::= '"' id '"'

   The replacement language
   INSN_REPL      ::= INSN_LABEL_LIT ( ';' INSN_LABEL_LIT )*
   INSN_LABEL_LIT ::= INSN_TEMPL
                      | 'LABEL' num
                      | 'LITERAL' num ' ' VARIABLE

   The operands in a PRECOND must be constants or variables bound by
   the INSN_PATTERN.

   The configuration options define a predicate on the availability of
   options which must be TRUE for this rule to be valid.  Examples are
   requiring "density" for replacements with density instructions,
   requiring "const16" for replacements that require const16
   instructions, etc.  The names are interpreted by the assembler to a
   truth value for a particular frag.

   The operands in the INSN_REPL must be constants, variables bound in
   the associated INSN_PATTERN, special variables that are bound in
   the INSN_REPL by LABEL or LITERAL definitions, or special value
   manipulation functions.

   A simple example of a replacement pattern:
   {"movi.n %as,%imm", "movi %as,%imm"} would convert the narrow
   movi.n instruction to the wide movi instruction.

   A more complex example of a branch around:
   {"beqz %as,%label", "bnez %as,%LABEL0;j %label;LABEL0"}
   would convert a branch to a negated branch to the following instruction
   with a jump to the original label.

   An Xtensa-specific example that generates a literal:
   {"movi %at,%imm", "LITERAL0 %imm; l32r %at,%LITERAL0"}
   will convert a movi instruction to an l32r of a literal
   literal defined in the literal pool.

   Even more complex is a conversion of a load with immediate offset
   to a load of a freshly generated literal, an explicit add and
   a load with 0 offset.  This transformation is only valid, though
   when the first and second operands are not the same as specified
   by the "| %at!=%as" precondition clause.
   {"l32i %at,%as,%imm | %at!=%as",
   "LITERAL0 %imm; l32r %at,%LITERAL0; add %at,%at,%as; l32i %at,%at,0"}

   There is special case for loop instructions here, but because we do
   not currently have the ability to represent the difference of two
   symbols, the conversion requires special code in the assembler to
   write the operands of the addi/addmi pair representing the
   difference of the old and new loop end label.  */

#include "as.h"
#include "xtensa-isa.h"
#include "xtensa-relax.h"
#include <stddef.h>
#include "xtensa-config.h"

#ifndef XCHAL_HAVE_WIDE_BRANCHES
#define XCHAL_HAVE_WIDE_BRANCHES 0
#endif

/* Imported from bfd.  */
extern xtensa_isa xtensa_default_isa;

/* The opname_list is a small list of names that we use for opcode and
   operand variable names to simplify ownership of these commonly used
   strings.  Strings entered in the table can be compared by pointer
   equality.  */

typedef struct opname_list_struct opname_list;
typedef opname_list opname_e;

struct opname_list_struct
{
  char *opname;
  opname_list *next;
};

static opname_list *local_opnames = NULL;


/* The "opname_map" and its element structure "opname_map_e" are used
   for binding an operand number to a name or a constant.  */

typedef struct opname_map_e_struct opname_map_e;
typedef struct opname_map_struct opname_map;

struct opname_map_e_struct
{
  const char *operand_name;	/* If null, then use constant_value.  */
  int operand_num;
  unsigned constant_value;
  opname_map_e *next;
};

struct opname_map_struct
{
  opname_map_e *head;
  opname_map_e **tail;
};

/* The "precond_list" and its element structure "precond_e" represents
   explicit preconditions comparing operand variables and constants.
   In the "precond_e" structure, a variable is identified by the name
   in the "opname" field.   If that field is NULL, then the operand
   is the constant in field "opval".  */

typedef struct precond_e_struct precond_e;
typedef struct precond_list_struct precond_list;

struct precond_e_struct
{
  const char *opname1;
  unsigned opval1;
  CmpOp cmpop;
  const char *opname2;
  unsigned opval2;
  precond_e *next;
};

struct precond_list_struct
{
  precond_e *head;
  precond_e **tail;
};


/* The insn_templ represents the INSN_TEMPL instruction template.  It
   is an opcode name with a list of operands.  These are used for
   instruction patterns and replacement patterns.  */

typedef struct insn_templ_struct insn_templ;
struct insn_templ_struct
{
  const char *opcode_name;
  opname_map operand_map;
};


/* The insn_pattern represents an INSN_PATTERN instruction pattern.
   It is an instruction template with preconditions that specify when
   it actually matches a given instruction.  */

typedef struct insn_pattern_struct insn_pattern;
struct insn_pattern_struct
{
  insn_templ t;
  precond_list preconds;
  ReqOptionList *options;
};


/* The "insn_repl" and associated element structure "insn_repl_e"
   instruction replacement list is a list of
   instructions/LITERALS/LABELS with constant operands or operands
   with names bound to the operand names in the associated pattern.  */

typedef struct insn_repl_e_struct insn_repl_e;
struct insn_repl_e_struct
{
  insn_templ t;
  insn_repl_e *next;
};

typedef struct insn_repl_struct insn_repl;
struct insn_repl_struct
{
  insn_repl_e *head;
  insn_repl_e **tail;
};


/* The split_rec is a vector of allocated char * pointers.  */

typedef struct split_rec_struct split_rec;
struct split_rec_struct
{
  char **vec;
  int count;
};

/* The "string_pattern_pair" is a set of pairs containing instruction
   patterns and replacement strings.  */

typedef struct string_pattern_pair_struct string_pattern_pair;
struct string_pattern_pair_struct
{
  const char *pattern;
  const char *replacement;
};


/* The widen_spec_list is a list of valid substitutions that generate
   wider representations.  These are generally used to specify
   replacements for instructions whose immediates do not fit their
   encodings.  A valid transition may require multiple steps of
   one-to-one instruction replacements with a final multiple
   instruction replacement.  As an example, here are the transitions
   required to replace an 'addi.n' with an 'addi', 'addmi'.

     addi.n a4, 0x1010
     => addi a4, 0x1010
     => addmi a4, 0x1010
     => addmi a4, 0x1000, addi a4, 0x10.  */

static string_pattern_pair widen_spec_list[] =
{
  {"add.n %ar,%as,%at ? IsaUseDensityInstruction", "add %ar,%as,%at"},
  {"addi.n %ar,%as,%imm ? IsaUseDensityInstruction", "addi %ar,%as,%imm"},
  {"beqz.n %as,%label ? IsaUseDensityInstruction", "beqz %as,%label"},
  {"bnez.n %as,%label ? IsaUseDensityInstruction", "bnez %as,%label"},
  {"l32i.n %at,%as,%imm ? IsaUseDensityInstruction", "l32i %at,%as,%imm"},
  {"mov.n %at,%as ? IsaUseDensityInstruction", "or %at,%as,%as"},
  {"movi.n %as,%imm ? IsaUseDensityInstruction", "movi %as,%imm"},
  {"nop.n ? IsaUseDensityInstruction ? realnop", "nop"},
  {"nop.n ? IsaUseDensityInstruction ? no-realnop", "or 1,1,1"},
  {"ret.n %as ? IsaUseDensityInstruction", "ret %as"},
  {"retw.n %as ? IsaUseDensityInstruction", "retw %as"},
  {"s32i.n %at,%as,%imm ? IsaUseDensityInstruction", "s32i %at,%as,%imm"},
  {"srli %at,%as,%imm", "extui %at,%as,%imm,F32MINUS(%imm)"},
  {"slli %ar,%as,0", "or %ar,%as,%as"},

  /* Widening with literals or const16.  */
  {"movi %at,%imm ? IsaUseL32R ",
   "LITERAL0 %imm; l32r %at,%LITERAL0"},
  {"movi %at,%imm ? IsaUseConst16",
   "const16 %at,HI16U(%imm); const16 %at,LOW16U(%imm)"},

  {"addi %ar,%as,%imm", "addmi %ar,%as,%imm"},
  /* LOW8 is the low 8 bits of the Immed
     MID8S is the middle 8 bits of the Immed */
  {"addmi %ar,%as,%imm", "addmi %ar,%as,HI24S(%imm); addi %ar,%ar,LOW8(%imm)"},

  /* In the end convert to either an l32r or const16.  */
  {"addmi %ar,%as,%imm | %ar!=%as ? IsaUseL32R",
   "LITERAL0 %imm; l32r %ar,%LITERAL0; add %ar,%as,%ar"},
  {"addmi %ar,%as,%imm | %ar!=%as ? IsaUseConst16",
   "const16 %ar,HI16U(%imm); const16 %ar,LOW16U(%imm); add %ar,%as,%ar"},

  /* Widening the load instructions with too-large immediates */
  {"l8ui %at,%as,%imm | %at!=%as ? IsaUseL32R",
   "LITERAL0 %imm; l32r %at,%LITERAL0; add %at,%at,%as; l8ui %at,%at,0"},
  {"l16si %at,%as,%imm | %at!=%as ? IsaUseL32R",
   "LITERAL0 %imm; l32r %at,%LITERAL0; add %at,%at,%as; l16si %at,%at,0"},
  {"l16ui %at,%as,%imm | %at!=%as ? IsaUseL32R",
   "LITERAL0 %imm; l32r %at,%LITERAL0; add %at,%at,%as; l16ui %at,%at,0"},
  {"l32i %at,%as,%imm | %at!=%as ? IsaUseL32R",
   "LITERAL0 %imm; l32r %at,%LITERAL0; add %at,%at,%as; l32i %at,%at,0"},

  /* Widening load instructions with const16s.  */
  {"l8ui %at,%as,%imm | %at!=%as ? IsaUseConst16",
   "const16 %at,HI16U(%imm); const16 %at,LOW16U(%imm); add %at,%at,%as; l8ui %at,%at,0"},
  {"l16si %at,%as,%imm | %at!=%as ? IsaUseConst16",
   "const16 %at,HI16U(%imm); const16 %at,LOW16U(%imm); add %at,%at,%as; l16si %at,%at,0"},
  {"l16ui %at,%as,%imm | %at!=%as ? IsaUseConst16",
   "const16 %at,HI16U(%imm); const16 %at,LOW16U(%imm); add %at,%at,%as; l16ui %at,%at,0"},
  {"l32i %at,%as,%imm | %at!=%as ? IsaUseConst16",
   "const16 %at,HI16U(%imm); const16 %at,LOW16U(%imm); add %at,%at,%as; l32i %at,%at,0"},

  /* This is only PART of the loop instruction.  In addition,
     hardcoded into its use is a modification of the final operand in
     the instruction in bytes 9 and 12.  */
  {"loop %as,%label | %as!=1 ? IsaUseLoops",
   "loop %as,%LABEL0;"
   "rsr.lend    %as;"		/* LEND */
   "wsr.lbeg    %as;"		/* LBEG */
   "addi    %as, %as, 0;"	/* lo8(%label-%LABEL1) */
   "addmi   %as, %as, 0;"	/* mid8(%label-%LABEL1) */
   "wsr.lend    %as;"
   "isync;"
   "rsr.lcount    %as;"		/* LCOUNT */
   "addi    %as, %as, 1;"	/* density -> addi.n %as, %as, 1 */
   "LABEL0"},
  {"loopgtz %as,%label | %as!=1 ? IsaUseLoops",
   "beqz    %as,%label;"
   "bltz    %as,%label;"
   "loopgtz %as,%LABEL0;"
   "rsr.lend    %as;"		/* LEND */
   "wsr.lbeg    %as;"		/* LBEG */
   "addi    %as, %as, 0;"	/* lo8(%label-%LABEL1) */
   "addmi   %as, %as, 0;"	/* mid8(%label-%LABEL1) */
   "wsr.lend    %as;"
   "isync;"
   "rsr.lcount    %as;"		/* LCOUNT */
   "addi    %as, %as, 1;"	/* density -> addi.n %as, %as, 1 */
   "LABEL0"},
  {"loopnez %as,%label | %as!=1 ? IsaUseLoops",
   "beqz     %as,%label;"
   "loopnez %as,%LABEL0;"
   "rsr.lend    %as;"		/* LEND */
   "wsr.lbeg    %as;"		/* LBEG */
   "addi    %as, %as, 0;"	/* lo8(%label-%LABEL1) */
   "addmi   %as, %as, 0;"	/* mid8(%label-%LABEL1) */
   "wsr.lend    %as;"
   "isync;"
   "rsr.lcount    %as;"		/* LCOUNT */
   "addi    %as, %as, 1;"	/* density -> addi.n %as, %as, 1 */
   "LABEL0"},

  /* Relaxing to wide branches.  Order is important here.  With wide
     branches, there is more than one correct relaxation for an
     out-of-range branch.  Put the wide branch relaxations first in the
     table since they are more efficient than the branch-around
     relaxations.  */
  
  {"beqz %as,%label ? IsaUseWideBranches", "WIDE.beqz %as,%label"},
  {"bnez %as,%label ? IsaUseWideBranches", "WIDE.bnez %as,%label"},
  {"bgez %as,%label ? IsaUseWideBranches", "WIDE.bgez %as,%label"},
  {"bltz %as,%label ? IsaUseWideBranches", "WIDE.bltz %as,%label"},
  {"beqi %as,%imm,%label ? IsaUseWideBranches", "WIDE.beqi %as,%imm,%label"},
  {"bnei %as,%imm,%label ? IsaUseWideBranches", "WIDE.bnei %as,%imm,%label"},
  {"bgei %as,%imm,%label ? IsaUseWideBranches", "WIDE.bgei %as,%imm,%label"},
  {"blti %as,%imm,%label ? IsaUseWideBranches", "WIDE.blti %as,%imm,%label"},
  {"bgeui %as,%imm,%label ? IsaUseWideBranches", "WIDE.bgeui %as,%imm,%label"},
  {"bltui %as,%imm,%label ? IsaUseWideBranches", "WIDE.bltui %as,%imm,%label"},
  {"bbci %as,%imm,%label ? IsaUseWideBranches", "WIDE.bbci %as,%imm,%label"},
  {"bbsi %as,%imm,%label ? IsaUseWideBranches", "WIDE.bbsi %as,%imm,%label"},
  {"beq %as,%at,%label ? IsaUseWideBranches", "WIDE.beq %as,%at,%label"},
  {"bne %as,%at,%label ? IsaUseWideBranches", "WIDE.bne %as,%at,%label"},
  {"bge %as,%at,%label ? IsaUseWideBranches", "WIDE.bge %as,%at,%label"},
  {"blt %as,%at,%label ? IsaUseWideBranches", "WIDE.blt %as,%at,%label"},
  {"bgeu %as,%at,%label ? IsaUseWideBranches", "WIDE.bgeu %as,%at,%label"},
  {"bltu %as,%at,%label ? IsaUseWideBranches", "WIDE.bltu %as,%at,%label"},
  {"bany %as,%at,%label ? IsaUseWideBranches", "WIDE.bany %as,%at,%label"},
  {"bnone %as,%at,%label ? IsaUseWideBranches", "WIDE.bnone %as,%at,%label"},
  {"ball %as,%at,%label ? IsaUseWideBranches", "WIDE.ball %as,%at,%label"},
  {"bnall %as,%at,%label ? IsaUseWideBranches", "WIDE.bnall %as,%at,%label"},
  {"bbc %as,%at,%label ? IsaUseWideBranches", "WIDE.bbc %as,%at,%label"},
  {"bbs %as,%at,%label ? IsaUseWideBranches", "WIDE.bbs %as,%at,%label"},
  
  /* Widening branch comparisons eq/ne to zero.  Prefer relaxing to narrow
     branches if the density option is available.  */
  {"beqz %as,%label ? IsaUseDensityInstruction", "bnez.n %as,%LABEL0;j %label;LABEL0"},
  {"bnez %as,%label ? IsaUseDensityInstruction", "beqz.n %as,%LABEL0;j %label;LABEL0"},
  {"beqz %as,%label", "bnez %as,%LABEL0;j %label;LABEL0"},
  {"bnez %as,%label", "beqz %as,%LABEL0;j %label;LABEL0"},

  /* Widening expect-taken branches.  */
  {"beqzt %as,%label ? IsaUsePredictedBranches", "bnez %as,%LABEL0;j %label;LABEL0"},
  {"bnezt %as,%label ? IsaUsePredictedBranches", "beqz %as,%LABEL0;j %label;LABEL0"},
  {"beqt %as,%at,%label ? IsaUsePredictedBranches", "bne %as,%at,%LABEL0;j %label;LABEL0"},
  {"bnet %as,%at,%label ? IsaUsePredictedBranches", "beq %as,%at,%LABEL0;j %label;LABEL0"},

  /* Widening branches from the Xtensa boolean option.  */
  {"bt %bs,%label ? IsaUseBooleans", "bf %bs,%LABEL0;j %label;LABEL0"},
  {"bf %bs,%label ? IsaUseBooleans", "bt %bs,%LABEL0;j %label;LABEL0"},

  /* Other branch-around-jump widenings.  */
  {"bgez %as,%label", "bltz %as,%LABEL0;j %label;LABEL0"},
  {"bltz %as,%label", "bgez %as,%LABEL0;j %label;LABEL0"},
  {"beqi %as,%imm,%label", "bnei %as,%imm,%LABEL0;j %label;LABEL0"},
  {"bnei %as,%imm,%label", "beqi %as,%imm,%LABEL0;j %label;LABEL0"},
  {"bgei %as,%imm,%label", "blti %as,%imm,%LABEL0;j %label;LABEL0"},
  {"blti %as,%imm,%label", "bgei %as,%imm,%LABEL0;j %label;LABEL0"},
  {"bgeui %as,%imm,%label", "bltui %as,%imm,%LABEL0;j %label;LABEL0"},
  {"bltui %as,%imm,%label", "bgeui %as,%imm,%LABEL0;j %label;LABEL0"},
  {"bbci %as,%imm,%label", "bbsi %as,%imm,%LABEL0;j %label;LABEL0"},
  {"bbsi %as,%imm,%label", "bbci %as,%imm,%LABEL0;j %label;LABEL0"},
  {"beq %as,%at,%label", "bne %as,%at,%LABEL0;j %label;LABEL0"},
  {"bne %as,%at,%label", "beq %as,%at,%LABEL0;j %label;LABEL0"},
  {"bge %as,%at,%label", "blt %as,%at,%LABEL0;j %label;LABEL0"},
  {"blt %as,%at,%label", "bge %as,%at,%LABEL0;j %label;LABEL0"},
  {"bgeu %as,%at,%label", "bltu %as,%at,%LABEL0;j %label;LABEL0"},
  {"bltu %as,%at,%label", "bgeu %as,%at,%LABEL0;j %label;LABEL0"},
  {"bany %as,%at,%label", "bnone %as,%at,%LABEL0;j %label;LABEL0"},
  {"bnone %as,%at,%label", "bany %as,%at,%LABEL0;j %label;LABEL0"},
  {"ball %as,%at,%label", "bnall %as,%at,%LABEL0;j %label;LABEL0"},
  {"bnall %as,%at,%label", "ball %as,%at,%LABEL0;j %label;LABEL0"},
  {"bbc %as,%at,%label", "bbs %as,%at,%LABEL0;j %label;LABEL0"},
  {"bbs %as,%at,%label", "bbc %as,%at,%LABEL0;j %label;LABEL0"},

  /* Expanding calls with literals.  */
  {"call0 %label,%ar0 ? IsaUseL32R",
   "LITERAL0 %label; l32r a0,%LITERAL0; callx0 a0,%ar0"},
  {"call4 %label,%ar4 ? IsaUseL32R",
   "LITERAL0 %label; l32r a4,%LITERAL0; callx4 a4,%ar4"},
  {"call8 %label,%ar8 ? IsaUseL32R",
   "LITERAL0 %label; l32r a8,%LITERAL0; callx8 a8,%ar8"},
  {"call12 %label,%ar12 ? IsaUseL32R",
   "LITERAL0 %label; l32r a12,%LITERAL0; callx12 a12,%ar12"},

  /* Expanding calls with const16.  */
  {"call0 %label,%ar0 ? IsaUseConst16",
   "const16 a0,HI16U(%label); const16 a0,LOW16U(%label); callx0 a0,%ar0"},
  {"call4 %label,%ar4 ? IsaUseConst16",
   "const16 a4,HI16U(%label); const16 a4,LOW16U(%label); callx4 a4,%ar4"},
  {"call8 %label,%ar8 ? IsaUseConst16",
   "const16 a8,HI16U(%label); const16 a8,LOW16U(%label); callx8 a8,%ar8"},
  {"call12 %label,%ar12 ? IsaUseConst16",
   "const16 a12,HI16U(%label); const16 a12,LOW16U(%label); callx12 a12,%ar12"}
};

#define WIDEN_COUNT (sizeof (widen_spec_list) / sizeof (string_pattern_pair))


/* The simplify_spec_list specifies simplifying transformations that
   will reduce the instruction width or otherwise simplify an
   instruction.  These are usually applied before relaxation in the
   assembler.  It is always legal to simplify.  Even for "addi as, 0",
   the "addi.n as, 0" will eventually be widened back to an "addi 0"
   after the widening table is applied.  Note: The usage of this table
   has changed somewhat so that it is entirely specific to "narrowing"
   instructions to use the density option.  This table is not used at
   all when the density option is not available.  */

string_pattern_pair simplify_spec_list[] =
{
  {"add %ar,%as,%at ? IsaUseDensityInstruction", "add.n %ar,%as,%at"},
  {"addi.n %ar,%as,0 ? IsaUseDensityInstruction", "mov.n %ar,%as"},
  {"addi %ar,%as,0 ? IsaUseDensityInstruction", "mov.n %ar,%as"},
  {"addi %ar,%as,%imm ? IsaUseDensityInstruction", "addi.n %ar,%as,%imm"},
  {"addmi %ar,%as,%imm ? IsaUseDensityInstruction", "addi.n %ar,%as,%imm"},
  {"beqz %as,%label ? IsaUseDensityInstruction", "beqz.n %as,%label"},
  {"bnez %as,%label ? IsaUseDensityInstruction", "bnez.n %as,%label"},
  {"l32i %at,%as,%imm ? IsaUseDensityInstruction", "l32i.n %at,%as,%imm"},
  {"movi %as,%imm ? IsaUseDensityInstruction", "movi.n %as,%imm"},
  {"nop ? realnop ? IsaUseDensityInstruction", "nop.n"},
  {"or %ar,%as,%at | %ar==%as | %as==%at ? IsaUseDensityInstruction", "nop.n"},
  {"or %ar,%as,%at | %ar!=%as | %as==%at ? IsaUseDensityInstruction", "mov.n %ar,%as"},
  {"ret %as ? IsaUseDensityInstruction", "ret.n %as"},
  {"retw %as ? IsaUseDensityInstruction", "retw.n %as"},
  {"s32i %at,%as,%imm ? IsaUseDensityInstruction", "s32i.n %at,%as,%imm"},
  {"slli %ar,%as,0 ? IsaUseDensityInstruction", "mov.n %ar,%as"}
};

#define SIMPLIFY_COUNT \
  (sizeof (simplify_spec_list) / sizeof (string_pattern_pair))


/* Externally visible functions.  */

extern bfd_boolean xg_has_userdef_op_fn (OpType);
extern long xg_apply_userdef_op_fn (OpType, long);


static void
append_transition (TransitionTable *tt,
		   xtensa_opcode opcode,
		   TransitionRule *t,
		   transition_cmp_fn cmp)
{
  TransitionList *tl = (TransitionList *) xmalloc (sizeof (TransitionList));
  TransitionList *prev;
  TransitionList **t_p;
  assert (tt != NULL);
  assert (opcode < tt->num_opcodes);

  prev = tt->table[opcode];
  tl->rule = t;
  tl->next = NULL;
  if (prev == NULL)
    {
      tt->table[opcode] = tl;
      return;
    }

  for (t_p = &tt->table[opcode]; (*t_p) != NULL; t_p = &(*t_p)->next)
    {
      if (cmp && cmp (t, (*t_p)->rule) < 0)
	{
	  /* Insert it here.  */
	  tl->next = *t_p;
	  *t_p = tl;
	  return;
	}
    }
  (*t_p) = tl;
}


static void
append_condition (TransitionRule *tr, Precondition *cond)
{
  PreconditionList *pl =
    (PreconditionList *) xmalloc (sizeof (PreconditionList));
  PreconditionList *prev = tr->conditions;
  PreconditionList *nxt;

  pl->precond = cond;
  pl->next = NULL;
  if (prev == NULL)
    {
      tr->conditions = pl;
      return;
    }
  nxt = prev->next;
  while (nxt != NULL)
    {
      prev = nxt;
      nxt = nxt->next;
    }
  prev->next = pl;
}


static void
append_value_condition (TransitionRule *tr,
			CmpOp cmp,
			unsigned op1,
			unsigned op2)
{
  Precondition *cond = (Precondition *) xmalloc (sizeof (Precondition));

  cond->cmp = cmp;
  cond->op_num = op1;
  cond->typ = OP_OPERAND;
  cond->op_data = op2;
  append_condition (tr, cond);
}


static void
append_constant_value_condition (TransitionRule *tr,
				 CmpOp cmp,
				 unsigned op1,
				 unsigned cnst)
{
  Precondition *cond = (Precondition *) xmalloc (sizeof (Precondition));

  cond->cmp = cmp;
  cond->op_num = op1;
  cond->typ = OP_CONSTANT;
  cond->op_data = cnst;
  append_condition (tr, cond);
}


static void
append_build_insn (TransitionRule *tr, BuildInstr *bi)
{
  BuildInstr *prev = tr->to_instr;
  BuildInstr *nxt;

  bi->next = NULL;
  if (prev == NULL)
    {
      tr->to_instr = bi;
      return;
    }
  nxt = prev->next;
  while (nxt != 0)
    {
      prev = nxt;
      nxt = prev->next;
    }
  prev->next = bi;
}


static void
append_op (BuildInstr *bi, BuildOp *b_op)
{
  BuildOp *prev = bi->ops;
  BuildOp *nxt;

  if (prev == NULL)
    {
      bi->ops = b_op;
      return;
    }
  nxt = prev->next;
  while (nxt != NULL)
    {
      prev = nxt;
      nxt = nxt->next;
    }
  prev->next = b_op;
}


static void
append_literal_op (BuildInstr *bi, unsigned op1, unsigned litnum)
{
  BuildOp *b_op = (BuildOp *) xmalloc (sizeof (BuildOp));

  b_op->op_num = op1;
  b_op->typ = OP_LITERAL;
  b_op->op_data = litnum;
  b_op->next = NULL;
  append_op (bi, b_op);
}


static void
append_label_op (BuildInstr *bi, unsigned op1, unsigned labnum)
{
  BuildOp *b_op = (BuildOp *) xmalloc (sizeof (BuildOp));

  b_op->op_num = op1;
  b_op->typ = OP_LABEL;
  b_op->op_data = labnum;
  b_op->next = NULL;
  append_op (bi, b_op);
}


static void
append_constant_op (BuildInstr *bi, unsigned op1, unsigned cnst)
{
  BuildOp *b_op = (BuildOp *) xmalloc (sizeof (BuildOp));

  b_op->op_num = op1;
  b_op->typ = OP_CONSTANT;
  b_op->op_data = cnst;
  b_op->next = NULL;
  append_op (bi, b_op);
}


static void
append_field_op (BuildInstr *bi, unsigned op1, unsigned src_op)
{
  BuildOp *b_op = (BuildOp *) xmalloc (sizeof (BuildOp));

  b_op->op_num = op1;
  b_op->typ = OP_OPERAND;
  b_op->op_data = src_op;
  b_op->next = NULL;
  append_op (bi, b_op);
}


/* These could be generated but are not currently.  */

static void
append_user_fn_field_op (BuildInstr *bi,
			 unsigned op1,
			 OpType typ,
			 unsigned src_op)
{
  BuildOp *b_op = (BuildOp *) xmalloc (sizeof (BuildOp));

  b_op->op_num = op1;
  b_op->typ = typ;
  b_op->op_data = src_op;
  b_op->next = NULL;
  append_op (bi, b_op);
}


/* These operand functions are the semantics of user-defined
   operand functions.  */

static long
operand_function_HI24S (long a)
{
  if (a & 0x80)
    return (a & (~0xff)) + 0x100;
  else
    return (a & (~0xff));
}


static long
operand_function_F32MINUS (long a)
{
  return (32 - a);
}


static long
operand_function_LOW8 (long a)
{
  if (a & 0x80)
    return (a & 0xff) | ~0xff;
  else
    return (a & 0xff);
}


static long
operand_function_LOW16U (long a)
{
  return (a & 0xffff);
}


static long
operand_function_HI16U (long a)
{
  unsigned long b = a & 0xffff0000;
  return (long) (b >> 16);
}


bfd_boolean
xg_has_userdef_op_fn (OpType op)
{
  switch (op)
    {
    case OP_OPERAND_F32MINUS:
    case OP_OPERAND_LOW8:
    case OP_OPERAND_HI24S:
    case OP_OPERAND_LOW16U:
    case OP_OPERAND_HI16U:
      return TRUE;
    default:
      break;
    }
  return FALSE;
}


long
xg_apply_userdef_op_fn (OpType op, long a)
{
  switch (op)
    {
    case OP_OPERAND_F32MINUS:
      return operand_function_F32MINUS (a);
    case OP_OPERAND_LOW8:
      return operand_function_LOW8 (a);
    case OP_OPERAND_HI24S:
      return operand_function_HI24S (a);
    case OP_OPERAND_LOW16U:
      return operand_function_LOW16U (a);
    case OP_OPERAND_HI16U:
      return operand_function_HI16U (a);
    default:
      break;
    }
  return FALSE;
}


/* Generate a transition table.  */

static const char *
enter_opname_n (const char *name, int len)
{
  opname_e *op;

  for (op = local_opnames; op != NULL; op = op->next)
    {
      if (strlen (op->opname) == (unsigned) len
	  && strncmp (op->opname, name, len) == 0)
	return op->opname;
    }
  op = (opname_e *) xmalloc (sizeof (opname_e));
  op->opname = (char *) xmalloc (len + 1);
  strncpy (op->opname, name, len);
  op->opname[len] = '\0';
  return op->opname;
}


static const char *
enter_opname (const char *name)
{
  opname_e *op;

  for (op = local_opnames; op != NULL; op = op->next)
    {
      if (strcmp (op->opname, name) == 0)
	return op->opname;
    }
  op = (opname_e *) xmalloc (sizeof (opname_e));
  op->opname = xstrdup (name);
  return op->opname;
}


static void
init_opname_map (opname_map *m)
{
  m->head = NULL;
  m->tail = &m->head;
}


static void
clear_opname_map (opname_map *m)
{
  opname_map_e *e;

  while (m->head != NULL)
    {
      e = m->head;
      m->head = e->next;
      free (e);
    }
  m->tail = &m->head;
}


static bfd_boolean
same_operand_name (const opname_map_e *m1, const opname_map_e *m2)
{
  if (m1->operand_name == NULL || m1->operand_name == NULL)
    return FALSE;
  return (m1->operand_name == m2->operand_name);
}


static opname_map_e *
get_opmatch (opname_map *map, const char *operand_name)
{
  opname_map_e *m;

  for (m = map->head; m != NULL; m = m->next)
    {
      if (strcmp (m->operand_name, operand_name) == 0)
	return m;
    }
  return NULL;
}


static bfd_boolean
op_is_constant (const opname_map_e *m1)
{
  return (m1->operand_name == NULL);
}


static unsigned
op_get_constant (const opname_map_e *m1)
{
  assert (m1->operand_name == NULL);
  return m1->constant_value;
}


static void
init_precond_list (precond_list *l)
{
  l->head = NULL;
  l->tail = &l->head;
}


static void
clear_precond_list (precond_list *l)
{
  precond_e *e;

  while (l->head != NULL)
    {
      e = l->head;
      l->head = e->next;
      free (e);
    }
  l->tail = &l->head;
}


static void
init_insn_templ (insn_templ *t)
{
  t->opcode_name = NULL;
  init_opname_map (&t->operand_map);
}


static void
clear_insn_templ (insn_templ *t)
{
  clear_opname_map (&t->operand_map);
}


static void
init_insn_pattern (insn_pattern *p)
{
  init_insn_templ (&p->t);
  init_precond_list (&p->preconds);
  p->options = NULL;
}


static void
clear_insn_pattern (insn_pattern *p)
{
  clear_insn_templ (&p->t);
  clear_precond_list (&p->preconds);
}


static void
init_insn_repl (insn_repl *r)
{
  r->head = NULL;
  r->tail = &r->head;
}


static void
clear_insn_repl (insn_repl *r)
{
  insn_repl_e *e;

  while (r->head != NULL)
    {
      e = r->head;
      r->head = e->next;
      clear_insn_templ (&e->t);
    }
  r->tail = &r->head;
}


static int
insn_templ_operand_count (const insn_templ *t)
{
  int i = 0;
  const opname_map_e *op;

  for (op = t->operand_map.head; op != NULL; op = op->next, i++)
    ;
  return i;
}


/* Convert a string to a number.  E.G.: parse_constant("10", &num) */

static bfd_boolean
parse_constant (const char *in, unsigned *val_p)
{
  unsigned val = 0;
  const char *p;

  if (in == NULL)
    return FALSE;
  p = in;

  while (*p != '\0')
    {
      if (*p >= '0' && *p <= '9')
	val = val * 10 + (*p - '0');
      else
	return FALSE;
      ++p;
    }
  *val_p = val;
  return TRUE;
}


/* Match a pattern like "foo1" with
   parse_id_constant("foo1", "foo", &num).
   This may also be used to just match a number.  */

static bfd_boolean
parse_id_constant (const char *in, const char *name, unsigned *val_p)
{
  unsigned namelen = 0;
  const char *p;

  if (in == NULL)
    return FALSE;

  if (name != NULL)
    namelen = strlen (name);

  if (name != NULL && strncmp (in, name, namelen) != 0)
    return FALSE;

  p = &in[namelen];
  return parse_constant (p, val_p);
}


static bfd_boolean
parse_special_fn (const char *name,
		  const char **fn_name_p,
		  const char **arg_name_p)
{
  char *p_start;
  const char *p_end;

  p_start = strchr (name, '(');
  if (p_start == NULL)
    return FALSE;

  p_end = strchr (p_start, ')');

  if (p_end == NULL)
    return FALSE;

  if (p_end[1] != '\0')
    return FALSE;

  *fn_name_p = enter_opname_n (name, p_start - name);
  *arg_name_p = enter_opname_n (p_start + 1, p_end - p_start - 1);
  return TRUE;
}


static const char *
skip_white (const char *p)
{
  if (p == NULL)
    return p;
  while (*p == ' ')
    ++p;
  return p;
}


static void
trim_whitespace (char *in)
{
  char *last_white = NULL;
  char *p = in;

  while (p && *p != '\0')
    {
      while (*p == ' ')
	{
	  if (last_white == NULL)
	    last_white = p;
	  p++;
	}
      if (*p != '\0')
	{
	  last_white = NULL;
	  p++;
	}
    }
  if (last_white)
    *last_white = '\0';
}


/* Split a string into component strings where "c" is the
   delimiter.  Place the result in the split_rec.  */

static void
split_string (split_rec *rec,
	      const char *in,
	      char c,
	      bfd_boolean elide_whitespace)
{
  int cnt = 0;
  int i;
  const char *p = in;

  while (p != NULL && *p != '\0')
    {
      cnt++;
      p = strchr (p, c);
      if (p)
	p++;
    }
  rec->count = cnt;
  rec->vec = NULL;

  if (rec->count == 0)
    return;

  rec->vec = (char **) xmalloc (sizeof (char *) * cnt);
  for (i = 0; i < cnt; i++)
    rec->vec[i] = 0;

  p = in;
  for (i = 0; i < cnt; i++)
    {
      const char *q;
      int len;

      q = p;
      if (elide_whitespace)
	q = skip_white (q);

      p = strchr (q, c);
      if (p == NULL)
	rec->vec[i] = xstrdup (q);
      else
	{
	  len = p - q;
	  rec->vec[i] = (char *) xmalloc (sizeof (char) * (len + 1));
	  strncpy (rec->vec[i], q, len);
	  rec->vec[i][len] = '\0';
	  p++;
	}

      if (elide_whitespace)
	trim_whitespace (rec->vec[i]);
    }
}


static void
clear_split_rec (split_rec *rec)
{
  int i;

  for (i = 0; i < rec->count; i++)
    free (rec->vec[i]);

  if (rec->count > 0)
    free (rec->vec);
}


/* Initialize a split record.  The split record must be initialized
   before split_string is called.  */

static void
init_split_rec (split_rec *rec)
{
  rec->vec = NULL;
  rec->count = 0;
}


/* Parse an instruction template like "insn op1, op2, op3".  */

static bfd_boolean
parse_insn_templ (const char *s, insn_templ *t)
{
  const char *p = s;
  int insn_name_len;
  split_rec oprec;
  int i;

  /* First find the first whitespace.  */

  init_split_rec (&oprec);

  p = skip_white (p);
  insn_name_len = strcspn (s, " ");
  if (insn_name_len == 0)
    return FALSE;

  init_insn_templ (t);
  t->opcode_name = enter_opname_n (p, insn_name_len);

  p = p + insn_name_len;

  /* Split by ',' and skip beginning and trailing whitespace.  */
  split_string (&oprec, p, ',', TRUE);

  for (i = 0; i < oprec.count; i++)
    {
      const char *opname = oprec.vec[i];
      opname_map_e *e = (opname_map_e *) xmalloc (sizeof (opname_map_e));
      e->next = NULL;
      e->operand_name = NULL;
      e->constant_value = 0;
      e->operand_num = i;

      /* If it begins with a number, assume that it is a number.  */
      if (opname && opname[0] >= '0' && opname[0] <= '9')
	{
	  unsigned val;

	  if (parse_constant (opname, &val))
	    e->constant_value = val;
	  else
	    {
	      free (e);
	      clear_split_rec (&oprec);
	      clear_insn_templ (t);
	      return FALSE;
	    }
	}
      else
	e->operand_name = enter_opname (oprec.vec[i]);

      *t->operand_map.tail = e;
      t->operand_map.tail = &e->next;
    }
  clear_split_rec (&oprec);
  return TRUE;
}


static bfd_boolean
parse_precond (const char *s, precond_e *precond)
{
  /* All preconditions are currently of the form:
     a == b or a != b or a == k (where k is a constant).
     Later we may use some special functions like DENSITY == 1
     to identify when density is available.  */

  const char *p = s;
  int len;
  precond->opname1 = NULL;
  precond->opval1 = 0;
  precond->cmpop = OP_EQUAL;
  precond->opname2 = NULL;
  precond->opval2 = 0;
  precond->next = NULL;

  p = skip_white (p);

  len = strcspn (p, " !=");

  if (len == 0)
    return FALSE;

  precond->opname1 = enter_opname_n (p, len);
  p = p + len;
  p = skip_white (p);

  /* Check for "==" and "!=".  */
  if (strncmp (p, "==", 2) == 0)
    precond->cmpop = OP_EQUAL;
  else if (strncmp (p, "!=", 2) == 0)
    precond->cmpop = OP_NOTEQUAL;
  else
    return FALSE;

  p = p + 2;
  p = skip_white (p);

  /* No trailing whitespace from earlier parsing.  */
  if (p[0] >= '0' && p[0] <= '9')
    {
      unsigned val;
      if (parse_constant (p, &val))
	precond->opval2 = val;
      else
	return FALSE;
    }
  else
    precond->opname2 = enter_opname (p);
  return TRUE;
}


static void
clear_req_or_option_list (ReqOrOption **r_p)
{
  if (*r_p == NULL)
    return;

  free ((*r_p)->option_name);
  clear_req_or_option_list (&(*r_p)->next);
  *r_p = NULL;
}


static void
clear_req_option_list (ReqOption **r_p)
{
  if (*r_p == NULL)
    return;

  clear_req_or_option_list (&(*r_p)->or_option_terms);
  clear_req_option_list (&(*r_p)->next);
  *r_p = NULL;
}


static ReqOrOption *
clone_req_or_option_list (ReqOrOption *req_or_option)
{
  ReqOrOption *new_req_or_option;

  if (req_or_option == NULL)
    return NULL;

  new_req_or_option = (ReqOrOption *) xmalloc (sizeof (ReqOrOption));
  new_req_or_option->option_name = xstrdup (req_or_option->option_name);
  new_req_or_option->is_true = req_or_option->is_true;
  new_req_or_option->next = NULL;
  new_req_or_option->next = clone_req_or_option_list (req_or_option->next);
  return new_req_or_option;
}


static ReqOption *
clone_req_option_list (ReqOption *req_option)
{
  ReqOption *new_req_option;

  if (req_option == NULL)
    return NULL;

  new_req_option = (ReqOption *) xmalloc (sizeof (ReqOption));
  new_req_option->or_option_terms = NULL;
  new_req_option->next = NULL;
  new_req_option->or_option_terms =
    clone_req_or_option_list (req_option->or_option_terms);
  new_req_option->next = clone_req_option_list (req_option->next);
  return new_req_option;
}


static bfd_boolean
parse_option_cond (const char *s, ReqOption *option)
{
  int i;
  split_rec option_term_rec;

  /* All option or conditions are of the form:
     optionA + no-optionB + ...
     "Ands" are divided by "?".  */

  init_split_rec (&option_term_rec);
  split_string (&option_term_rec, s, '+', TRUE);

  if (option_term_rec.count == 0)
    {
      clear_split_rec (&option_term_rec);
      return FALSE;
    }

  for (i = 0; i < option_term_rec.count; i++)
    {
      char *option_name = option_term_rec.vec[i];
      bfd_boolean is_true = TRUE;
      ReqOrOption *req;
      ReqOrOption **r_p;

      if (strncmp (option_name, "no-", 3) == 0)
	{
	  option_name = xstrdup (&option_name[3]);
	  is_true = FALSE;
	}
      else
	option_name = xstrdup (option_name);

      req = (ReqOrOption *) xmalloc (sizeof (ReqOrOption));
      req->option_name = option_name;
      req->is_true = is_true;
      req->next = NULL;

      /* Append to list.  */
      for (r_p = &option->or_option_terms; (*r_p) != NULL;
	   r_p = &(*r_p)->next)
	;
      (*r_p) = req;
    }
  return TRUE;
}


/* Parse a string like:
   "insn op1, op2, op3, op4 | op1 != op2 | op2 == op3 | op4 == 1".
   I.E., instruction "insn" with 4 operands where operand 1 and 2 are not
   the same and operand 2 and 3 are the same and operand 4 is 1.

   or:

   "insn op1 | op1 == 1 / density + boolean / no-useroption".
   i.e. instruction "insn" with 1 operands where operand 1 is 1
   when "density" or "boolean" options are available and
   "useroption" is not available.

   Because the current implementation of this parsing scheme uses
   split_string, it requires that '|' and '?' are only used as
   delimiters for predicates and required options.  */

static bfd_boolean
parse_insn_pattern (const char *in, insn_pattern *insn)
{
  split_rec rec;
  split_rec optionrec;
  int i;

  init_insn_pattern (insn);

  init_split_rec (&optionrec);
  split_string (&optionrec, in, '?', TRUE);
  if (optionrec.count == 0)
    {
      clear_split_rec (&optionrec);
      return FALSE;
    }

  init_split_rec (&rec);

  split_string (&rec, optionrec.vec[0], '|', TRUE);

  if (rec.count == 0)
    {
      clear_split_rec (&rec);
      clear_split_rec (&optionrec);
      return FALSE;
    }

  if (!parse_insn_templ (rec.vec[0], &insn->t))
    {
      clear_split_rec (&rec);
      clear_split_rec (&optionrec);
      return FALSE;
    }

  for (i = 1; i < rec.count; i++)
    {
      precond_e *cond = (precond_e *) xmalloc (sizeof (precond_e));

      if (!parse_precond (rec.vec[i], cond))
	{
	  clear_split_rec (&rec);
	  clear_split_rec (&optionrec);
	  clear_insn_pattern (insn);
	  return FALSE;
	}

      /* Append the condition.  */
      *insn->preconds.tail = cond;
      insn->preconds.tail = &cond->next;
    }

  for (i = 1; i < optionrec.count; i++)
    {
      /* Handle the option conditions.  */
      ReqOption **r_p;
      ReqOption *req_option = (ReqOption *) xmalloc (sizeof (ReqOption));
      req_option->or_option_terms = NULL;
      req_option->next = NULL;

      if (!parse_option_cond (optionrec.vec[i], req_option))
	{
	  clear_split_rec (&rec);
	  clear_split_rec (&optionrec);
	  clear_insn_pattern (insn);
	  clear_req_option_list (&req_option);
	  return FALSE;
	}

      /* Append the condition.  */
      for (r_p = &insn->options; (*r_p) != NULL; r_p = &(*r_p)->next)
	;

      (*r_p) = req_option;
    }

  clear_split_rec (&rec);
  clear_split_rec (&optionrec);
  return TRUE;
}


static bfd_boolean
parse_insn_repl (const char *in, insn_repl *r_p)
{
  /* This is a list of instruction templates separated by ';'.  */
  split_rec rec;
  int i;

  split_string (&rec, in, ';', TRUE);

  for (i = 0; i < rec.count; i++)
    {
      insn_repl_e *e = (insn_repl_e *) xmalloc (sizeof (insn_repl_e));

      e->next = NULL;

      if (!parse_insn_templ (rec.vec[i], &e->t))
	{
	  free (e);
	  clear_insn_repl (r_p);
	  return FALSE;
	}
      *r_p->tail = e;
      r_p->tail = &e->next;
    }
  return TRUE;
}


static bfd_boolean
transition_applies (insn_pattern *initial_insn,
		    const char *from_string ATTRIBUTE_UNUSED,
		    const char *to_string ATTRIBUTE_UNUSED)
{
  ReqOption *req_option;

  for (req_option = initial_insn->options;
       req_option != NULL;
       req_option = req_option->next)
    {
      ReqOrOption *req_or_option = req_option->or_option_terms;

      if (req_or_option == NULL
	  || req_or_option->next != NULL)
	continue;

      if (strncmp (req_or_option->option_name, "IsaUse", 6) == 0)
	{
	  bfd_boolean option_available = FALSE;
	  char *option_name = req_or_option->option_name + 6;
	  if (!strcmp (option_name, "DensityInstruction"))
	    option_available = (XCHAL_HAVE_DENSITY == 1);
	  else if (!strcmp (option_name, "L32R"))
	    option_available = (XCHAL_HAVE_L32R == 1);
	  else if (!strcmp (option_name, "Const16"))
	    option_available = (XCHAL_HAVE_CONST16 == 1);
	  else if (!strcmp (option_name, "Loops"))
	    option_available = (XCHAL_HAVE_LOOPS == 1);
	  else if (!strcmp (option_name, "WideBranches"))
	    option_available = (XCHAL_HAVE_WIDE_BRANCHES == 1);
	  else if (!strcmp (option_name, "PredictedBranches"))
	    option_available = (XCHAL_HAVE_PREDICTED_BRANCHES == 1);
	  else if (!strcmp (option_name, "Booleans"))
	    option_available = (XCHAL_HAVE_BOOLEANS == 1);
	  else
	    as_warn (_("invalid configuration option '%s' in transition rule '%s'"),
		     req_or_option->option_name, from_string);
	  if ((option_available ^ req_or_option->is_true) != 0)
	    return FALSE;
	}
      else if (strcmp (req_or_option->option_name, "realnop") == 0)
	{
	  bfd_boolean nop_available =
	    (xtensa_opcode_lookup (xtensa_default_isa, "nop")
	     != XTENSA_UNDEFINED);
	  if ((nop_available ^ req_or_option->is_true) != 0)
	    return FALSE;
	}
    }
  return TRUE;
}


static bfd_boolean
wide_branch_opcode (const char *opcode_name,
		    char *suffix,
		    xtensa_opcode *popcode)
{
  xtensa_isa isa = xtensa_default_isa;
  xtensa_opcode opcode;
  static char wbr_name_buf[20];

  if (strncmp (opcode_name, "WIDE.", 5) != 0)
    return FALSE;

  strcpy (wbr_name_buf, opcode_name + 5);
  strcat (wbr_name_buf, suffix);
  opcode = xtensa_opcode_lookup (isa, wbr_name_buf);
  if (opcode != XTENSA_UNDEFINED)
    {
      *popcode = opcode;
      return TRUE;
    }

  return FALSE;
}


static TransitionRule *
build_transition (insn_pattern *initial_insn,
		  insn_repl *replace_insns,
		  const char *from_string,
		  const char *to_string)
{
  TransitionRule *tr = NULL;
  xtensa_opcode opcode;
  xtensa_isa isa = xtensa_default_isa;

  opname_map_e *op1;
  opname_map_e *op2;

  precond_e *precond;
  insn_repl_e *r;
  unsigned label_count = 0;
  unsigned max_label_count = 0;
  bfd_boolean has_label = FALSE;
  unsigned literal_count = 0;

  opcode = xtensa_opcode_lookup (isa, initial_insn->t.opcode_name);
  if (opcode == XTENSA_UNDEFINED)
    {
      /* It is OK to not be able to translate some of these opcodes.  */
      return NULL;
    }


  if (xtensa_opcode_num_operands (isa, opcode)
      != insn_templ_operand_count (&initial_insn->t))
    {
      /* This is also OK because there are opcodes that
	 have different numbers of operands on different
	 architecture variations.  */
      return NULL;
    }

  tr = (TransitionRule *) xmalloc (sizeof (TransitionRule));
  tr->opcode = opcode;
  tr->conditions = NULL;
  tr->to_instr = NULL;

  /* Build the conditions. First, equivalent operand condition....  */
  for (op1 = initial_insn->t.operand_map.head; op1 != NULL; op1 = op1->next)
    {
      for (op2 = op1->next; op2 != NULL; op2 = op2->next)
	{
	  if (same_operand_name (op1, op2))
	    {
	      append_value_condition (tr, OP_EQUAL,
				      op1->operand_num, op2->operand_num);
	    }
	}
    }

  /* Now the condition that an operand value must be a constant....  */
  for (op1 = initial_insn->t.operand_map.head; op1 != NULL; op1 = op1->next)
    {
      if (op_is_constant (op1))
	{
	  append_constant_value_condition (tr,
					   OP_EQUAL,
					   op1->operand_num,
					   op_get_constant (op1));
	}
    }


  /* Now add the explicit preconditions listed after the "|" in the spec.
     These are currently very limited, so we do a special case
     parse for them.  We expect spaces, opname != opname.  */
  for (precond = initial_insn->preconds.head;
       precond != NULL;
       precond = precond->next)
    {
      op1 = NULL;
      op2 = NULL;

      if (precond->opname1)
	{
	  op1 = get_opmatch (&initial_insn->t.operand_map, precond->opname1);
	  if (op1 == NULL)
	    {
	      as_fatal (_("opcode '%s': no bound opname '%s' "
			  "for precondition in '%s'"),
			xtensa_opcode_name (isa, opcode),
			precond->opname1, from_string);
	      return NULL;
	    }
	}

      if (precond->opname2)
	{
	  op2 = get_opmatch (&initial_insn->t.operand_map, precond->opname2);
	  if (op2 == NULL)
	    {
	      as_fatal (_("opcode '%s': no bound opname '%s' "
			  "for precondition in %s"),
		       xtensa_opcode_name (isa, opcode),
		       precond->opname2, from_string);
	      return NULL;
	    }
	}

      if (op1 == NULL && op2 == NULL)
	{
	  as_fatal (_("opcode '%s': precondition only contains "
		      "constants in '%s'"),
		    xtensa_opcode_name (isa, opcode), from_string);
	  return NULL;
	}
      else if (op1 != NULL && op2 != NULL)
	append_value_condition (tr, precond->cmpop,
				op1->operand_num, op2->operand_num);
      else if (op2 == NULL)
	append_constant_value_condition (tr, precond->cmpop,
					 op1->operand_num, precond->opval2);
      else
	append_constant_value_condition (tr, precond->cmpop,
					 op2->operand_num, precond->opval1);
    }

  tr->options = clone_req_option_list (initial_insn->options);

  /* Generate the replacement instructions.  Some of these
     "instructions" are actually labels and literals.  The literals
     must be defined in order 0..n and a literal must be defined
     (e.g., "LITERAL0 %imm") before use (e.g., "%LITERAL0").  The
     labels must be defined in order, but they can be used before they
     are defined.  Also there are a number of special operands (e.g.,
     HI24S).  */

  for (r = replace_insns->head; r != NULL; r = r->next)
    {
      BuildInstr *bi;
      const char *opcode_name;
      int operand_count;
      opname_map_e *op;
      unsigned idnum = 0;
      const char *fn_name;
      const char *operand_arg_name;

      bi = (BuildInstr *) xmalloc (sizeof (BuildInstr));
      append_build_insn (tr, bi);

      bi->id = 0;
      bi->opcode = XTENSA_UNDEFINED;
      bi->ops = NULL;
      bi->next = NULL;

      opcode_name = r->t.opcode_name;
      operand_count = insn_templ_operand_count (&r->t);

      if (parse_id_constant (opcode_name, "LITERAL", &idnum))
	{
	  bi->typ = INSTR_LITERAL_DEF;
	  bi->id = idnum;
	  if (idnum != literal_count)
	    as_fatal (_("generated literals must be numbered consecutively"));
	  ++literal_count;
	  if (operand_count != 1)
	    as_fatal (_("expected one operand for generated literal"));

	}
      else if (parse_id_constant (opcode_name, "LABEL", &idnum))
	{
	  bi->typ = INSTR_LABEL_DEF;
	  bi->id = idnum;
	  if (idnum != label_count)
	    as_fatal (_("generated labels must be numbered consecutively"));
	  ++label_count;
	  if (operand_count != 0)
	    as_fatal (_("expected 0 operands for generated label"));
	}
      else
	{
	  bi->typ = INSTR_INSTR;
	  if (wide_branch_opcode (opcode_name, ".w18", &bi->opcode)
	      || wide_branch_opcode (opcode_name, ".w15", &bi->opcode))
	    opcode_name = xtensa_opcode_name (isa, bi->opcode);
	  else
	    bi->opcode = xtensa_opcode_lookup (isa, opcode_name);

	  if (bi->opcode == XTENSA_UNDEFINED)
	    {
	      as_warn (_("invalid opcode '%s' in transition rule '%s'"),
		       opcode_name, to_string);
	      return NULL;
	    }

	  /* Check for the right number of ops.  */
	  if (xtensa_opcode_num_operands (isa, bi->opcode)
	      != (int) operand_count)
	    as_fatal (_("opcode '%s': replacement does not have %d ops"),
		      opcode_name,
		      xtensa_opcode_num_operands (isa, bi->opcode));
	}

      for (op = r->t.operand_map.head; op != NULL; op = op->next)
	{
	  unsigned idnum;

	  if (op_is_constant (op))
	    append_constant_op (bi, op->operand_num, op_get_constant (op));
	  else if (parse_id_constant (op->operand_name, "%LITERAL", &idnum))
	    {
	      if (idnum >= literal_count)
		as_fatal (_("opcode %s: replacement "
			    "literal %d >= literal_count(%d)"),
			  opcode_name, idnum, literal_count);
	      append_literal_op (bi, op->operand_num, idnum);
	    }
	  else if (parse_id_constant (op->operand_name, "%LABEL", &idnum))
	    {
	      has_label = TRUE;
	      if (idnum > max_label_count)
		max_label_count = idnum;
	      append_label_op (bi, op->operand_num, idnum);
	    }
	  else if (parse_id_constant (op->operand_name, "a", &idnum))
	    append_constant_op (bi, op->operand_num, idnum);
	  else if (op->operand_name[0] == '%')
	    {
	      opname_map_e *orig_op;
	      orig_op = get_opmatch (&initial_insn->t.operand_map,
				     op->operand_name);
	      if (orig_op == NULL)
		{
		  as_fatal (_("opcode %s: unidentified operand '%s' in '%s'"),
			    opcode_name, op->operand_name, to_string);

		  append_constant_op (bi, op->operand_num, 0);
		}
	      else
		append_field_op (bi, op->operand_num, orig_op->operand_num);
	    }
	  else if (parse_special_fn (op->operand_name,
				     &fn_name, &operand_arg_name))
	    {
	      opname_map_e *orig_op;
	      OpType typ = OP_CONSTANT;

	      if (strcmp (fn_name, "LOW8") == 0)
		typ = OP_OPERAND_LOW8;
	      else if (strcmp (fn_name, "HI24S") == 0)
		typ = OP_OPERAND_HI24S;
	      else if (strcmp (fn_name, "F32MINUS") == 0)
		typ = OP_OPERAND_F32MINUS;
	      else if (strcmp (fn_name, "LOW16U") == 0)
		typ = OP_OPERAND_LOW16U;
	      else if (strcmp (fn_name, "HI16U") == 0)
		typ = OP_OPERAND_HI16U;
	      else
		as_fatal (_("unknown user-defined function %s"), fn_name);

	      orig_op = get_opmatch (&initial_insn->t.operand_map,
				     operand_arg_name);
	      if (orig_op == NULL)
		{
		  as_fatal (_("opcode %s: unidentified operand '%s' in '%s'"),
			    opcode_name, op->operand_name, to_string);
		  append_constant_op (bi, op->operand_num, 0);
		}
	      else
		append_user_fn_field_op (bi, op->operand_num,
					 typ, orig_op->operand_num);
	    }
	  else
	    {
	      as_fatal (_("opcode %s: could not parse operand '%s' in '%s'"),
			opcode_name, op->operand_name, to_string);
	      append_constant_op (bi, op->operand_num, 0);
	    }
	}
    }
  if (has_label && max_label_count >= label_count)
    {
      as_fatal (_("opcode %s: replacement label %d >= label_count(%d)"),
		xtensa_opcode_name (isa, opcode),
		max_label_count, label_count);
      return NULL;
    }

  return tr;
}


static TransitionTable *
build_transition_table (const string_pattern_pair *transitions,
			int transition_count,
			transition_cmp_fn cmp)
{
  TransitionTable *table = NULL;
  int num_opcodes = xtensa_isa_num_opcodes (xtensa_default_isa);
  int i, tnum;

  if (table != NULL)
    return table;

  /* Otherwise, build it now.  */
  table = (TransitionTable *) xmalloc (sizeof (TransitionTable));
  table->num_opcodes = num_opcodes;
  table->table =
    (TransitionList **) xmalloc (sizeof (TransitionTable *) * num_opcodes);

  for (i = 0; i < num_opcodes; i++)
    table->table[i] = NULL;

  for (tnum = 0; tnum < transition_count; tnum++)
    {
      const char *from_string = transitions[tnum].pattern;
      const char *to_string = transitions[tnum].replacement;

      insn_pattern initial_insn;
      insn_repl replace_insns;
      TransitionRule *tr;

      init_insn_pattern (&initial_insn);
      if (!parse_insn_pattern (from_string, &initial_insn))
	{
	  as_fatal (_("could not parse INSN_PATTERN '%s'"), from_string);
	  clear_insn_pattern (&initial_insn);
	  continue;
	}

      init_insn_repl (&replace_insns);
      if (!parse_insn_repl (to_string, &replace_insns))
	{
	  as_fatal (_("could not parse INSN_REPL '%s'"), to_string);
	  clear_insn_pattern (&initial_insn);
	  clear_insn_repl (&replace_insns);
	  continue;
	}

      if (transition_applies (&initial_insn, from_string, to_string))
	{
	  tr = build_transition (&initial_insn, &replace_insns,
				 from_string, to_string);
	  if (tr)
	    append_transition (table, tr->opcode, tr, cmp);
	  else
	    {
#if TENSILICA_DEBUG
	      as_warn (_("could not build transition for %s => %s"),
		       from_string, to_string);
#endif
	    }
	}

      clear_insn_repl (&replace_insns);
      clear_insn_pattern (&initial_insn);
    }
  return table;
}


extern TransitionTable *
xg_build_widen_table (transition_cmp_fn cmp)
{
  static TransitionTable *table = NULL;
  if (table == NULL)
    table = build_transition_table (widen_spec_list, WIDEN_COUNT, cmp);
  return table;
}


extern TransitionTable *
xg_build_simplify_table (transition_cmp_fn cmp)
{
  static TransitionTable *table = NULL;
  if (table == NULL)
    table = build_transition_table (simplify_spec_list, SIMPLIFY_COUNT, cmp);
  return table;
}
