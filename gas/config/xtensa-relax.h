/* Table of relaxations for Xtensa assembly.
   Copyright 2003, 2004 Free Software Foundation, Inc.

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

#ifndef XTENSA_RELAX_H
#define XTENSA_RELAX_H

#include "xtensa-isa.h"


/* Data structures for the table-driven relaxations for Xtensa processors.
   See xtensa-relax.c for details.  */

typedef struct transition_list TransitionList;
typedef struct transition_table TransitionTable;
typedef struct transition_rule TransitionRule;
typedef struct precondition_list PreconditionList;
typedef struct precondition Precondition;

typedef struct req_or_option_list ReqOrOptionList;
typedef struct req_or_option_list ReqOrOption;
typedef struct req_option_list ReqOptionList;
typedef struct req_option_list ReqOption;

struct transition_table
{
  int num_opcodes;
  TransitionList **table;	/* Possible transitions for each opcode.  */
};

struct transition_list
{
  TransitionRule *rule;
  TransitionList *next;
};

struct precondition_list
{
  Precondition *precond;
  PreconditionList *next;
};


/* The required options for a rule are represented with a two-level
   structure, with leaf expressions combined by logical ORs at the
   lower level, and the results then combined by logical ANDs at the
   top level.  The AND terms are linked in a list, and each one can
   contain a reference to a list of OR terms.  The leaf expressions,
   i.e., the OR options, can be negated by setting the is_true field
   to FALSE.  There are two classes of leaf expressions: (1) those
   that are properties of the Xtensa configuration and can be
   evaluated once when building the tables, and (2) those that depend
   of the state of directives or other settings that may vary during
   the assembly.  The following expressions may be used in group (1):

   IsaUse*:	Xtensa configuration settings.
   realnop:	TRUE if the instruction set includes a NOP instruction.

   There are currently no expressions in group (2), but they are still
   supported since there is a good chance they'll be needed again for
   something.  */

struct req_option_list
{
  ReqOrOptionList *or_option_terms;
  ReqOptionList *next;
};

struct req_or_option_list
{
  char *option_name;
  bfd_boolean is_true;
  ReqOrOptionList *next;
};

/* Operand types and constraints on operands:  */

typedef enum op_type OpType;
typedef enum cmp_op CmpOp;

enum op_type
{
  OP_CONSTANT,
  OP_OPERAND,
  OP_OPERAND_LOW8,		/* Sign-extended low 8 bits of immed.  */
  OP_OPERAND_HI24S,		/* High 24 bits of immed,
				   plus 0x100 if low 8 bits are signed.  */
  OP_OPERAND_F32MINUS,		/* 32 - immed.  */
  OP_OPERAND_LOW16U,		/* Low 16 bits of immed.  */
  OP_OPERAND_HI16U,		/* High 16 bits of immed.  */
  OP_LITERAL,
  OP_LABEL
};

enum cmp_op
{
  OP_EQUAL,
  OP_NOTEQUAL,
};

struct precondition
{
  CmpOp cmp;
  int op_num;
  OpType typ;			/* CONSTANT: op_data is a constant.
				   OPERAND: operand op_num must equal op_data.
				   Cannot be LITERAL or LABEL.  */
  int op_data;
};


typedef struct build_op BuildOp;

struct build_op
{
  int op_num;
  OpType typ;
  unsigned op_data;		/* CONSTANT: op_data is the value to encode.
				   OPERAND: op_data is the field in the
				   source instruction to take the value from
				   and encode in the op_num field here.
				   LITERAL or LABEL: op_data is the ordinal
				   that identifies the appropriate one, i.e.,
				   there can be more than one literal or
				   label in an expansion.  */
  BuildOp *next;
};

typedef struct build_instr BuildInstr;
typedef enum instr_type InstrType;

enum instr_type
{
  INSTR_INSTR,
  INSTR_LITERAL_DEF,
  INSTR_LABEL_DEF
};

struct build_instr
{
  InstrType typ;
  unsigned id;			/* LITERAL_DEF or LABEL_DEF: an ordinal to
				   identify which one.  */
  xtensa_opcode opcode;		/* Unused for LITERAL_DEF or LABEL_DEF.  */
  BuildOp *ops;
  BuildInstr *next;
};

struct transition_rule
{
  xtensa_opcode opcode;
  PreconditionList *conditions;
  ReqOptionList *options;
  BuildInstr *to_instr;
};

typedef int (*transition_cmp_fn) (const TransitionRule *,
				  const TransitionRule *);

extern TransitionTable *xg_build_simplify_table (transition_cmp_fn);
extern TransitionTable *xg_build_widen_table (transition_cmp_fn);

extern bfd_boolean xg_has_userdef_op_fn (OpType);
extern long xg_apply_userdef_op_fn (OpType, long);

#endif /* !XTENSA_RELAX_H */
