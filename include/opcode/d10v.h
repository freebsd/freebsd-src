/* d10v.h -- Header file for D10V opcode table
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2003
   Free Software Foundation, Inc.
   Written by Martin Hunt (hunt@cygnus.com), Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef D10V_H
#define D10V_H

/* Format Specifier */
#define FM00	0
#define FM01	0x40000000
#define FM10	0x80000000
#define FM11	0xC0000000

#define NOP 0x5e00
#define OPCODE_DIVS	0x14002800

/* The opcode table is an array of struct d10v_opcode.  */

struct d10v_opcode
{
  /* The opcode name.  */
  const char *name;

  /* the opcode format */
  int format;

  /* These numbers were picked so we can do if( i & SHORT_OPCODE) */
#define SHORT_OPCODE 1
#define LONG_OPCODE  8
#define SHORT_2	     1		/* short with 2 operands */
#define SHORT_B	     3		/* short with 8-bit branch */
#define LONG_B	     8		/* long with 16-bit branch */
#define LONG_L       10		/* long with 3 operands */
#define LONG_R       12		/* reserved */

  /* just a placeholder for variable-length instructions */
  /* for example, "bra" will be a fake for "bra.s" and bra.l" */
  /* which will immediately follow in the opcode table.  */
#define OPCODE_FAKE  32

  /* the number of cycles */
  int cycles;

  /* the execution unit(s) used */
  int unit;
#define EITHER	0
#define IU	1
#define MU	2
#define BOTH	3

  /* execution type; parallel or sequential */
  /* this field is used to decide if two instructions */
  /* can be executed in parallel */
  int exec_type;
#define PARONLY 1	/* parallel only */
#define SEQ	2	/* must be sequential */
#define PAR	4	/* may be parallel */
#define BRANCH_LINK 8	/* subroutine call.  must be aligned */
#define RMEM     16	/* reads memory */
#define WMEM     32	/* writes memory */
#define RF0      64	/* reads f0 */
#define WF0     128	/* modifies f0 */
#define WCAR    256	/* write Carry */
#define BRANCH  512	/* branch, no link */
#define ALONE  1024	/* short but pack with a NOP if on asm line alone */

  /* the opcode */
  long opcode;

  /* mask.  if( (i & mask) == opcode ) then match */
  long mask;

  /* An array of operand codes.  Each code is an index into the
     operand table.  They appear in the order which the operands must
     appear in assembly code, and are terminated by a zero.  */
  unsigned char operands[6];
};

/* The table itself is sorted by major opcode number, and is otherwise
   in the order in which the disassembler should consider
   instructions.  */
extern const struct d10v_opcode d10v_opcodes[];
extern const int d10v_num_opcodes;

/* The operands table is an array of struct d10v_operand.  */
struct d10v_operand
{
  /* The number of bits in the operand.  */
  int bits;

  /* How far the operand is left shifted in the instruction.  */
  int shift;

  /* One bit syntax flags.  */
  int flags;
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the d10v_opcodes table.  */

extern const struct d10v_operand d10v_operands[];

/* Values defined for the flags field of a struct d10v_operand.  */

/* the operand must be an even number */
#define OPERAND_EVEN	(1)

/* the operand must be an odd number */
#define OPERAND_ODD	(2)	

/* this is the destination register; it will be modified */
/* this is used by the optimizer */
#define OPERAND_DEST	(4)

/* number or symbol */
#define OPERAND_NUM	(8)

/* address or label */
#define OPERAND_ADDR	(0x10)

/* register */
#define OPERAND_REG	(0x20)

/* postincrement +  */
#define OPERAND_PLUS	(0x40)

/* postdecrement -  */
#define OPERAND_MINUS	(0x80)

/* @  */
#define OPERAND_ATSIGN	(0x100)

/* @(  */
#define OPERAND_ATPAR	(0x200)

/* accumulator 0 */
#define OPERAND_ACC0	(0x400)

/* accumulator 1 */
#define OPERAND_ACC1	(0x800)

/* f0 / f1 flag register */
#define OPERAND_FFLAG	(0x1000)

/* c flag register */
#define OPERAND_CFLAG	(0x2000)

/* control register  */
#define OPERAND_CONTROL	(0x4000)

/* predecrement mode '@-sp'  */
#define OPERAND_ATMINUS	(0x8000)

/* signed number */
#define OPERAND_SIGNED	(0x10000)

/* special accumulator shifts need a 4-bit number */
/* 1 <= x <= 16 */
#define OPERAND_SHIFT	(0x20000)

/* general purpose register */
#define OPERAND_GPR	(0x40000)

/* special imm3 values with range restricted to -2 <= imm3 <= 3 */
/* needed for rac/rachi */
#define RESTRICTED_NUM3	(0x80000)

/* Pre-decrement is only supported for SP.  */
#define OPERAND_SP      (0x100000)

/* Post-decrement is not supported for SP.  Like OPERAND_EVEN, and
   unlike OPERAND_SP, this flag doesn't prevent the instruction from
   matching, it only fails validation later on.  */
#define OPERAND_NOSP    (0x200000)

/* Structure to hold information about predefined registers.  */
struct pd_reg
{
  char *name;		/* name to recognize */
  char *pname;		/* name to print for this register */
  int value;
};

extern const struct pd_reg d10v_predefined_registers[];
int d10v_reg_name_cnt (void);

/* an expressionS only has one register type, so we fake it */
/* by setting high bits to indicate type */
#define REGISTER_MASK	0xFF

#endif /* D10V_H */
