/* d30v.h -- Header file for D30V opcode table
   Copyright 1997, 1998, 1999, 2000, 2001, 2003 Free Software Foundation, Inc.
   Written by Martin Hunt (hunt@cygnus.com), Cygnus Solutions

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
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef D30V_H
#define D30V_H

#define NOP 0x00F00000

/* Structure to hold information about predefined registers.  */
struct pd_reg
{
  char *name;		/* name to recognize */
  char *pname;		/* name to print for this register */
  int value;
};

extern const struct pd_reg pre_defined_registers[];
int reg_name_cnt (void);

/* the number of control registers */
#define MAX_CONTROL_REG	64

/* define the format specifiers */
#define FM00	0
#define FM01	0x80000000
#define FM10	0x8000000000000000LL
#define FM11	0x8000000080000000LL

/* define the opcode classes */
#define BRA	0
#define LOGIC	1
#define IMEM	2
#define IALU1	4
#define IALU2	5

/* define the execution condition codes */
#define ECC_AL	0	/* ALways (default) */
#define ECC_TX	1	/* F0=True, F1=Don't care */
#define ECC_FX	2	/* F0=False, F1=Don't care */
#define ECC_XT	3	/* F0=Don't care, F1=True */
#define ECC_XF	4	/* F0=Don't care, F1=False */
#define ECC_TT	5	/* F0=True, F1=True */
#define ECC_TF	6	/* F0=True, F1=False */
#define ECC_RESERVED	7	/* reserved */
#define ECC_MAX	ECC_RESERVED

extern const char *d30v_ecc_names[];

/* condition code table for CMP and CMPU */
extern const char *d30v_cc_names[];

/* The opcode table is an array of struct d30v_opcode.  */
struct d30v_opcode
{
  /* The opcode name.  */
  const char *name;

  /* the opcode */
  int op1;	/* first part, "IALU1" for example */
  int op2;	/* the rest of the opcode */

  /* opcode format(s).  These numbers correspond to entries */
  /* in the d30v_format_table */
  unsigned char format[4];

#define SHORT_M		1
#define SHORT_M2	5	/* for ld2w and st2w */
#define SHORT_A		9
#define SHORT_B1	11
#define SHORT_B2	12
#define SHORT_B2r     13
#define SHORT_B3      14
#define SHORT_B3r     16
#define SHORT_B3b     18
#define SHORT_B3br    20
#define SHORT_D1r     22
#define SHORT_D2      24
#define SHORT_D2r     26
#define SHORT_D2Br    28
#define SHORT_U       30      /* unary SHORT_A.  ABS for example */
#define SHORT_F       31      /* SHORT_A with flag registers */
#define SHORT_AF      33      /* SHORT_A with only the first register a flag register */
#define SHORT_T       35      /* for trap instruction */
#define SHORT_A5      36      /* SHORT_A with a 5-bit immediate instead of 6 */
#define SHORT_CMP     38      /* special form for CMPcc */
#define SHORT_CMPU    40      /* special form for CMPUcc */
#define SHORT_A1      42      /* special form of SHORT_A for MACa opcodes where a=1 */
#define SHORT_AA      44      /* SHORT_A with the first register an accumulator */
#define SHORT_RA      46      /* SHORT_A with the second register an accumulator */
#define SHORT_MODINC  48      
#define SHORT_MODDEC  49
#define SHORT_C1      50
#define SHORT_C2      51
#define SHORT_UF      52
#define SHORT_A2      53
#define SHORT_NONE    55      /* no operands */
#define SHORT_AR      56      /* like SHORT_AA but only accept register as third parameter  */
#define LONG          57
#define LONG_U        58      /* unary LONG */
#define LONG_Ur       59      /* LONG pc-relative */
#define LONG_CMP      60      /* special form for CMPcc and CMPUcc */
#define LONG_M        61      /* Memory long for ldb, stb */
#define LONG_M2       62      /* Memory long for ld2w, st2w */
#define LONG_2        63      /* LONG with 2 operands; jmptnz */
#define LONG_2r       64      /* LONG with 2 operands; bratnz */
#define LONG_2b       65      /* LONG_2 with modifier of 3 */
#define LONG_2br      66      /* LONG_2r with modifier of 3 */
#define LONG_D        67      /* for DJMPI */
#define LONG_Dr       68      /* for DBRAI */
#define LONG_Dbr      69      /* for repeati */

  /* the execution unit(s) used */
  int unit;
#define EITHER	0
#define IU	1
#define MU	2
#define EITHER_BUT_PREFER_MU 3

  /* this field is used to decide if two instructions */
  /* can be executed in parallel */
  long flags_used;
  long flags_set;
#define FLAG_0		(1L<<0)
#define FLAG_1		(1L<<1)
#define FLAG_2		(1L<<2)
#define FLAG_3		(1L<<3)
#define FLAG_4		(1L<<4)		/* S (saturation) */
#define FLAG_5		(1L<<5)		/* V (overflow) */
#define FLAG_6		(1L<<6)		/* VA (accumulated overflow) */
#define FLAG_7		(1L<<7)		/* C (carry/borrow) */
#define FLAG_SM		(1L<<8)		/* SM (stack mode) */
#define FLAG_RP		(1L<<9)		/* RP (repeat enable) */
#define FLAG_CONTROL	(1L<<10)	/* control registers */
#define FLAG_A0		(1L<<11)	/* A0 */
#define FLAG_A1		(1L<<12)	/* A1 */
#define FLAG_JMP	(1L<<13)	/* instruction is a branch */
#define FLAG_JSR	(1L<<14)	/* subroutine call.  must be aligned */
#define FLAG_MEM	(1L<<15)	/* reads/writes memory */
#define FLAG_NOT_WITH_ADDSUBppp	 (1L<<16) /* Old meaning: a 2 word 4 byter operation
					   New meaning: operation cannot be 
					   combined in parallel with ADD/SUBppp. */
#define FLAG_MUL16	(1L<<17)	/* 16 bit multiply */
#define FLAG_MUL32	(1L<<18)	/* 32 bit multiply */
#define FLAG_ADDSUBppp	(1L<<19)	/* ADDppp or SUBppp */
#define FLAG_DELAY	(1L<<20)	/* This is a delayed branch or jump */
#define FLAG_LKR	(1L<<21)	/* insn in left slot kills right slot */
#define FLAG_CVVA	(FLAG_5|FLAG_6|FLAG_7)
#define FLAG_C		FLAG_7
#define FLAG_ALL	(FLAG_0 | \
			 FLAG_1 | \
			 FLAG_2 | \
			 FLAG_3 | \
			 FLAG_4 | \
			 FLAG_5 | \
			 FLAG_6 | \
			 FLAG_7 | \
			 FLAG_SM | \
			 FLAG_RP | \
			 FLAG_CONTROL)

  int reloc_flag;
#define RELOC_PCREL	1
#define RELOC_ABS	2
};

extern const struct d30v_opcode d30v_opcode_table[];
extern const int d30v_num_opcodes;

/* The operands table is an array of struct d30v_operand.  */
struct d30v_operand
{
  /* the length of the field */
  int length;

  /* The number of significant bits in the operand.  */
  int bits;

  /* position relative to Ra */
  int position;

  /* syntax flags.  */
  long flags;
};
extern const struct d30v_operand d30v_operand_table[];

/* Values defined for the flags field of a struct d30v_operand.  */

/* this is the destination register; it will be modified */
/* this is used by the optimizer */
#define OPERAND_DEST	(1)

/* number or symbol */
#define OPERAND_NUM	(2)

/* address or label */
#define OPERAND_ADDR	(4)

/* register */
#define OPERAND_REG	(8)

/* postincrement +  */
#define OPERAND_PLUS	(0x10)

/* postdecrement -  */
#define OPERAND_MINUS	(0x20)

/* signed number */
#define OPERAND_SIGNED	(0x40)

/* this operand must be shifted left by 3 */
#define OPERAND_SHIFT	(0x80)

/* flag register */
#define OPERAND_FLAG	(0x100)

/* control register  */
#define OPERAND_CONTROL	(0x200)

/* accumulator */
#define OPERAND_ACC	(0x400)

/* @  */
#define OPERAND_ATSIGN	(0x800)

/* @(  */
#define OPERAND_ATPAR	(0x1000)

/* predecrement mode '@-sp'  */
#define OPERAND_ATMINUS	(0x2000)

/* this operand changes the instruction name */
/* for example, CPMcc, CMPUcc */
#define OPERAND_NAME	(0x4000)

/* fake operand for mvtsys and mvfsys */
#define OPERAND_SPECIAL	(0x8000)

/* let the optimizer know that two registers are affected */
#define OPERAND_2REG	(0x10000)

/* This operand is pc-relative.  Note that repeati can have two immediate
   operands, one of which is pcrel, the other (the IMM6U one) is not.  */
#define OPERAND_PCREL	(0x20000)

/* The format table is an array of struct d30v_format.  */
struct d30v_format
{
  int	form;		/* SHORT_A, LONG, etc */
  int	modifier;	/* two bit modifier following opcode */
  unsigned char operands[5];
};
extern const struct d30v_format d30v_format_table[];


/* an instruction is defined by an opcode and a format */
/* for example, "add" has one opcode, but three different */
/* formats, 2 SHORT_A forms and a LONG form. */
struct d30v_insn
{
  struct d30v_opcode *op;	/* pointer to an entry in the opcode table */
  struct d30v_format *form;	/* pointer to an entry in the format table */
  int ecc;			/* execution condition code */
};

/* an expressionS only has one register type, so we fake it */
/* by setting high bits to indicate type */
#define REGISTER_MASK	0xFF

#endif /* D30V_H */
