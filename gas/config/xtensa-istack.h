/* Declarations for stacks of tokenized Xtensa instructions.
   Copyright (C) 2003 Free Software Foundation, Inc.

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

#ifndef XTENSA_ISTACK_H
#define XTENSA_ISTACK_H

#include "xtensa-isa.h"

#define MAX_ISTACK 12
#define MAX_INSN_ARGS 6

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
  
  bfd_boolean is_specific_opcode; 
  xtensa_opcode opcode;	/* Literals have an invalid opcode.  */
  int ntok;
  expressionS tok[MAX_INSN_ARGS];
} TInsn;


/* tinsn_stack:  This is a stack of instructions to  be placed.  */

typedef struct tinsn_stack
{
  int ninsn;
  TInsn insn[MAX_ISTACK];
} IStack;


void         istack_init        PARAMS ((IStack *));
bfd_boolean  istack_empty       PARAMS ((IStack *));
bfd_boolean  istack_full        PARAMS ((IStack *));
TInsn *      istack_top         PARAMS ((IStack *));
void         istack_push        PARAMS ((IStack *, TInsn *));
TInsn *      istack_push_space  PARAMS ((IStack *)); 
void         istack_pop         PARAMS ((IStack *));

/* TInsn utilities.  */
void         tinsn_init         PARAMS ((TInsn *));
void         tinsn_copy         PARAMS ((TInsn *, const TInsn *));
expressionS *tinsn_get_tok      PARAMS ((TInsn *, int));

#endif /* !XTENSA_ISTACK_H */
