/* v850.h -- Header file for NEC V850 opcode table
   Copyright 1996, 1997 Free Software Foundation, Inc.
   Written by J.T. Conklin, Cygnus Support

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

#ifndef V850_H
#define V850_H

/* The opcode table is an array of struct v850_opcode.  */

struct v850_opcode
{
  /* The opcode name.  */
  const char *name;

  /* The opcode itself.  Those bits which will be filled in with
     operands are zeroes.  */
  unsigned long opcode;

  /* The opcode mask.  This is used by the disassembler.  This is a
     mask containing ones indicating those bits which must match the
     opcode field, and zeroes indicating those bits which need not
     match (and are presumably filled in by operands).  */
  unsigned long mask;

  /* An array of operand codes.  Each code is an index into the
     operand table.  They appear in the order which the operands must
     appear in assembly code, and are terminated by a zero.  */
  unsigned char operands[8];

  /* Which (if any) operand is a memory operand.  */
  unsigned int memop;

  /* Target processor(s).  A bit field of processors which support
     this instruction.  Note a bit field is used as some instructions
     are available on multiple, different processor types, whereas
     other instructions are only available on one specific type.  */
  unsigned int processors;
};

/* Values for the processors field in the v850_opcode structure.  */
#define PROCESSOR_V850		(1 << 0)		/* Just the V850.  */
#define PROCESSOR_ALL		-1			/* Any processor.  */
#define PROCESSOR_V850E		(1 << 1)		/* Just the V850E. */
#define PROCESSOR_NOT_V850	(~ PROCESSOR_V850)	/* Any processor except the V850.  */
#define PROCESSOR_V850EA	(1 << 2)		/* Just the V850EA. */

/* The table itself is sorted by major opcode number, and is otherwise
   in the order in which the disassembler should consider
   instructions.  */
extern const struct v850_opcode v850_opcodes[];
extern const int v850_num_opcodes;


/* The operands table is an array of struct v850_operand.  */

struct v850_operand
{
  /* The number of bits in the operand.  */
  /* If this value is -1 then the operand's bits are in a discontinous distribution in the instruction. */
  int bits;

  /* (bits >= 0):  How far the operand is left shifted in the instruction.  */
  /* (bits == -1): Bit mask of the bits in the operand.  */
  int shift;

  /* Insertion function.  This is used by the assembler.  To insert an
     operand value into an instruction, check this field.

     If it is NULL, execute
         i |= (op & ((1 << o->bits) - 1)) << o->shift;
     (i is the instruction which we are filling in, o is a pointer to
     this structure, and op is the opcode value; this assumes twos
     complement arithmetic).

     If this field is not NULL, then simply call it with the
     instruction and the operand value.  It will return the new value
     of the instruction.  If the ERRMSG argument is not NULL, then if
     the operand value is illegal, *ERRMSG will be set to a warning
     string (the operand will be inserted in any case).  If the
     operand value is legal, *ERRMSG will be unchanged (most operands
     can accept any value).  */
  unsigned long (* insert) PARAMS ((unsigned long instruction, long op,
				   const char ** errmsg));

  /* Extraction function.  This is used by the disassembler.  To
     extract this operand type from an instruction, check this field.

     If it is NULL, compute
         op = o->bits == -1 ? ((i) & o->shift) : ((i) >> o->shift) & ((1 << o->bits) - 1);
	 if (o->flags & V850_OPERAND_SIGNED)
	     op = (op << (32 - o->bits)) >> (32 - o->bits);
     (i is the instruction, o is a pointer to this structure, and op
     is the result; this assumes twos complement arithmetic).

     If this field is not NULL, then simply call it with the
     instruction value.  It will return the value of the operand.  If
     the INVALID argument is not NULL, *INVALID will be set to
     non-zero if this operand type can not actually be extracted from
     this operand (i.e., the instruction does not match).  If the
     operand is valid, *INVALID will not be changed.  */
  unsigned long (* extract) PARAMS ((unsigned long instruction, int * invalid));

  /* One bit syntax flags.  */
  int flags;
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the v850_opcodes table.  */

extern const struct v850_operand v850_operands[];

/* Values defined for the flags field of a struct v850_operand.  */

/* This operand names a general purpose register */
#define V850_OPERAND_REG	0x01

/* This operand names a system register */
#define V850_OPERAND_SRG	0x02

/* This operand names a condition code used in the setf instruction */
#define V850_OPERAND_CC		0x04

/* This operand takes signed values */
#define V850_OPERAND_SIGNED	0x08

/* This operand is the ep register.  */
#define V850_OPERAND_EP		0x10

/* This operand is a PC displacement */
#define V850_OPERAND_DISP	0x20

/* This is a relaxable operand.   Only used for D9->D22 branch relaxing
   right now.  We may need others in the future (or maybe handle them like
   promoted operands on the mn10300?)  */
#define V850_OPERAND_RELAX	0x40

/* The register specified must not be r0 */
#define V850_NOT_R0	        0x80

/* CYGNUS LOCAL v850e */
/* push/pop type instruction, V850E specific.  */
#define V850E_PUSH_POP		0x100

/* 16 bit immediate follows instruction, V850E specific.  */
#define V850E_IMMEDIATE16	0x200

/* 32 bit immediate follows instruction, V850E specific.  */
#define V850E_IMMEDIATE32	0x400

#endif /* V850_H */
