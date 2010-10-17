/* mn10200.h -- Header file for Matsushita 10200 opcode table
   Copyright 1996, 1997 Free Software Foundation, Inc.
   Written by Jeff Law, Cygnus Support

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

#ifndef MN10200_H
#define MN10200_H

/* The opcode table is an array of struct mn10200_opcode.  */

struct mn10200_opcode
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

  /* The format of this opcode.  */
  unsigned char format;

  /* An array of operand codes.  Each code is an index into the
     operand table.  They appear in the order which the operands must
     appear in assembly code, and are terminated by a zero.  */
  unsigned char operands[8];
};

/* The table itself is sorted by major opcode number, and is otherwise
   in the order in which the disassembler should consider
   instructions.  */
extern const struct mn10200_opcode mn10200_opcodes[];
extern const int mn10200_num_opcodes;


/* The operands table is an array of struct mn10200_operand.  */

struct mn10200_operand
{
  /* The number of bits in the operand.  */
  int bits;

  /* How far the operand is left shifted in the instruction.  */
  int shift;

  /* One bit syntax flags.  */
  int flags;
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the mn10200_opcodes table.  */

extern const struct mn10200_operand mn10200_operands[];

/* Values defined for the flags field of a struct mn10200_operand.  */
#define MN10200_OPERAND_DREG 0x1

#define MN10200_OPERAND_AREG 0x2

#define MN10200_OPERAND_PSW 0x4

#define MN10200_OPERAND_MDR 0x8

#define MN10200_OPERAND_SIGNED 0x10

#define MN10200_OPERAND_PROMOTE 0x20

#define MN10200_OPERAND_PAREN 0x40

#define MN10200_OPERAND_REPEATED 0x80

#define MN10200_OPERAND_EXTENDED 0x100

#define MN10200_OPERAND_NOCHECK 0x200

#define MN10200_OPERAND_PCREL 0x400

#define MN10200_OPERAND_MEMADDR 0x800

#define MN10200_OPERAND_RELAX 0x1000

#define FMT_1 1
#define FMT_2 2
#define FMT_3 3
#define FMT_4 4
#define FMT_5 5
#define FMT_6 6
#define FMT_7 7
#endif /* MN10200_H */
