/* vax-inst.h - GNU - Part of vax.c
   Copyright (C) 1987, 1992 Free Software Foundation, Inc.
   
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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * This is part of vax-ins-parse.c & friends.
 * We want to parse a vax instruction text into a tree defined here.
 */

#define VIT_MAX_OPERANDS (6)	/* maximum number of operands in one       */
/* single vax instruction */

struct vop			/* vax instruction operand                 */
{
	short int vop_ndx;	/* -1, or index register. eg 7=[R7]	   */
	short int vop_reg;	/* -1, or register number. eg @I^#=0xF     */
	/* Helps distinguish "abs" from "abs(PC)". */
	short int vop_mode;	/* addressing mode 4 bits. eg I^#=0x9	   */
	char vop_short;	/* operand displacement length as written  */
	/* ' '=none, "bilsw"=B^I^L^S^W^.           */
	char vop_access;	/* 'b'branch ' 'no-instruction 'amrvw'norm */
	char vop_width;	/* Operand width, one of "bdfghloqw"	   */
	char *vop_warn;	/* warning message of this operand, if any */
	char *vop_error;	/* say if operand is inappropriate         */
	char *vop_expr_begin;	/* Unparsed expression, 1st char ...	   */
	char *vop_expr_end;	/* ... last char.			   */
	unsigned char vop_nbytes; /* number of bytes in datum		   */
};


typedef long vax_opcodeT;	/* For initialising array of opcodes	   */
/* Some synthetic opcodes > 16 bits!       */

#define VIT_OPCODE_SYNTHETIC 0x80000000	/* Not real hardware instruction.  */
#define VIT_OPCODE_SPECIAL   0x40000000	/* Not normal branch optimising.   */
/* Never set without ..._SYNTHETIC */

#define VAX_WIDTH_UNCONDITIONAL_JUMP '-' /* These are encoded into         */
#define VAX_WIDTH_CONDITIONAL_JUMP   '?' /* vop_width when vop_access == 'b' */
#define VAX_WIDTH_WORD_JUMP          '!' /* and VIT_OPCODE_SYNTHETIC set.  */
#define VAX_WIDTH_BYTE_JUMP	     ':' /*                                */

#define VAX_JMP (0x17)		/* Useful for branch optimising. Jump instr*/
#define VAX_PC_RELATIVE_MODE (0xef) /* Use it after VAX_JMP		   */
#define VAX_ABSOLUTE_MODE (0x9F) /* Use as @#...			   */
#define VAX_BRB (0x11)		/* Canonical branch.			   */
#define VAX_BRW (0x31)		/* Another canonical branch		   */
#define VAX_WIDEN_WORD (0x20)	/* Add this to byte branch to get word br. */
#define VAX_WIDEN_LONG (0x6)	/* Add this to byte branch to get long jmp.*/
/* Needs VAX_PC_RELATIVE_MODE byte after it*/

struct vit			/* vax instruction tree                    */
{
	/* vit_opcode is char[] for portability.   */
	char		   vit_opcode[ sizeof (vax_opcodeT) ];
	unsigned char    vit_opcode_nbytes; /* How long is _opcode? (chars)	   */
	unsigned char    vit_operands;/*					   */
	struct vop       vit_operand[VIT_MAX_OPERANDS];  /* operands             */
	char *	   vit_error;	/* "" or error text */
};

/* end of vax-inst.h */
