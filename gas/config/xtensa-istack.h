/* Declarations for stacks of tokenized Xtensa instructions.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.

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

#ifndef XTENSA_ISTACK_H
#define XTENSA_ISTACK_H

#include "xtensa-isa.h"

#define MAX_ISTACK 12
#define MAX_INSN_ARGS 10

enum itype_enum
{
  ITYPE_INSN,
  ITYPE_LITERAL,
  ITYPE_LABEL
};


/* Literals have 1 token and no opcode.
   Labels have 1 token and no opcode.  */

typedef struct tinsn_struct
{
  enum itype_enum insn_type;

  xtensa_opcode opcode;	/* Literals have an invalid opcode.  */
  bfd_boolean is_specific_opcode;
  bfd_boolean keep_wide;
  int ntok;
  expressionS tok[MAX_INSN_ARGS];
  unsigned linenum;

  struct fixP *fixup;

  /* Filled out by relaxation_requirements:  */
  enum xtensa_relax_statesE subtype;
  int literal_space;
  /* Filled out by vinsn_to_insnbuf:  */
  symbolS *symbol;
  offsetT offset;
  fragS *literal_frag;
} TInsn;


/* tinsn_stack:  This is a stack of instructions to  be placed.  */

typedef struct tinsn_stack
{
  int ninsn;
  TInsn insn[MAX_ISTACK];
} IStack;


void istack_init (IStack *);
bfd_boolean istack_empty (IStack *);
bfd_boolean istack_full (IStack *);
TInsn *istack_top (IStack *);
void istack_push (IStack *, TInsn *);
TInsn *istack_push_space (IStack *);
void istack_pop (IStack *);

/* TInsn utilities.  */
void tinsn_init (TInsn *);
expressionS *tinsn_get_tok (TInsn *, int);


/* vliw_insn: bundles of TInsns.  */

typedef struct vliw_insn
{
  xtensa_format format;
  int num_slots;
  unsigned int inside_bundle;
  TInsn slots[MAX_SLOTS];
  xtensa_insnbuf insnbuf;
  xtensa_insnbuf slotbuf[MAX_SLOTS];
} vliw_insn;

#endif /* !XTENSA_ISTACK_H */
