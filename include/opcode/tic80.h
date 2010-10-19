/* tic80.h -- Header file for TI TMS320C80 (MV) opcode table
   Copyright 1996, 1997, 2003 Free Software Foundation, Inc.
   Written by Fred Fish (fnf@cygnus.com), Cygnus Support

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

#ifndef TIC80_H
#define TIC80_H

/* The opcode table is an array of struct tic80_opcode.  */

struct tic80_opcode
{
  /* The opcode name.  */

  const char *name;

  /* The opcode itself.  Those bits which will be filled in with operands
     are zeroes.  */

  unsigned long opcode;

  /* The opcode mask.  This is used by the disassembler.  This is a mask
     containing ones indicating those bits which must match the opcode
     field, and zeroes indicating those bits which need not match (and are
     presumably filled in by operands).  */

  unsigned long mask;

  /* Special purpose flags for this opcode. */

  unsigned char flags;

  /* An array of operand codes.  Each code is an index into the operand
     table.  They appear in the order which the operands must appear in
     assembly code, and are terminated by a zero.  FIXME: Adjust size to
     match actual requirements when TIc80 support is complete */

  unsigned char operands[8];
};

/* The table itself is sorted by major opcode number, and is otherwise in
   the order in which the disassembler should consider instructions.
   FIXME: This isn't currently true. */

extern const struct tic80_opcode tic80_opcodes[];
extern const int tic80_num_opcodes;


/* The operands table is an array of struct tic80_operand.  */

struct tic80_operand
{
  /* The number of bits in the operand.  */

  int bits;

  /* How far the operand is left shifted in the instruction.  */

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

  unsigned long (*insert)
    (unsigned long instruction, long op, const char **errmsg);

  /* Extraction function.  This is used by the disassembler.  To
     extract this operand type from an instruction, check this field.

     If it is NULL, compute
         op = ((i) >> o->shift) & ((1 << o->bits) - 1);
	 if ((o->flags & TIC80_OPERAND_SIGNED) != 0
	     && (op & (1 << (o->bits - 1))) != 0)
	   op -= 1 << o->bits;
     (i is the instruction, o is a pointer to this structure, and op
     is the result; this assumes twos complement arithmetic).

     If this field is not NULL, then simply call it with the
     instruction value.  It will return the value of the operand.  If
     the INVALID argument is not NULL, *INVALID will be set to
     non-zero if this operand type can not actually be extracted from
     this operand (i.e., the instruction does not match).  If the
     operand is valid, *INVALID will not be changed.  */

  long (*extract) (unsigned long instruction, int *invalid);

  /* One bit syntax flags.  */

  unsigned long flags;
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the tic80_opcodes table.  */

extern const struct tic80_operand tic80_operands[];


/* Values defined for the flags field of a struct tic80_operand.

   Note that flags for all predefined symbols, such as the general purpose
   registers (ex: r10), control registers (ex: FPST), condition codes (ex:
   eq0.b), bit numbers (ex: gt.b), etc are large enough that they can be
   or'd into an int where the lower bits contain the actual numeric value
   that correponds to this predefined symbol.  This way a single int can
   contain both the value of the symbol and it's type.
 */

/* This operand must be an even register number.  Floating point numbers
   for example are stored in even/odd register pairs. */

#define TIC80_OPERAND_EVEN	(1 << 0)

/* This operand must be an odd register number and must be one greater than
   the register number of the previous operand.  I.E. the second register in
   an even/odd register pair. */

#define TIC80_OPERAND_ODD	(1 << 1)

/* This operand takes signed values.  */

#define TIC80_OPERAND_SIGNED	(1 << 2)

/* This operand may be either a predefined constant name or a numeric value.
   An example would be a condition code like "eq0.b" which has the numeric
   value 0x2. */

#define TIC80_OPERAND_NUM	(1 << 3)

/* This operand should be wrapped in parentheses rather than separated
   from the previous one by a comma.  This is used for various
   instructions, like the load and store instructions, which want
   their operands to look like "displacement(reg)" */

#define TIC80_OPERAND_PARENS	(1 << 4)

/* This operand is a PC relative branch offset.  The disassembler prints
   these symbolically if possible.  Note that the offsets are taken as word
   offsets. */

#define TIC80_OPERAND_PCREL	(1 << 5)

/* This flag is a hint to the disassembler for using hex as the prefered
   printing format, even for small positive or negative immediate values.
   Normally values in the range -999 to 999 are printed as signed decimal
   values and other values are printed in hex. */

#define TIC80_OPERAND_BITFIELD	(1 << 6)

/* This operand may have a ":m" modifier specified by bit 17 in a short
   immediate form instruction. */

#define TIC80_OPERAND_M_SI	(1 << 7)

/* This operand may have a ":m" modifier specified by bit 15 in a long
   immediate or register form instruction. */

#define TIC80_OPERAND_M_LI	(1 << 8)

/* This operand may have a ":s" modifier specified in bit 11 in a long
   immediate or register form instruction. */

#define TIC80_OPERAND_SCALED	(1 << 9)

/* This operand is a floating point value */

#define TIC80_OPERAND_FLOAT	(1 << 10)

/* This operand is an byte offset from a base relocation. The lower
 two bits of the final relocated address are ignored when the value is
 written to the program counter. */

#define TIC80_OPERAND_BASEREL	(1 << 11)

/* This operand is an "endmask" field for a shift instruction.
   It is treated special in that it can have values of 0-32,
   where 0 and 32 result in the same instruction.  The assembler
   must be able to accept both endmask values.  This disassembler
   has no way of knowing from the instruction which value was 
   given at assembly time, so it just uses '0'. */

#define TIC80_OPERAND_ENDMASK	(1 << 12)

/* This operand is one of the 32 general purpose registers.
   The disassembler prints these with a leading 'r'. */

#define TIC80_OPERAND_GPR	(1 << 27)

/* This operand is a floating point accumulator register.
   The disassembler prints these with a leading 'a'. */

#define TIC80_OPERAND_FPA	( 1 << 28)

/* This operand is a control register number, either numeric or
   symbolic (like "EIF", "EPC", etc).
   The disassembler prints these symbolically. */

#define TIC80_OPERAND_CR	(1 << 29)

/* This operand is a condition code, either numeric or
   symbolic (like "eq0.b", "ne0.w", etc).
   The disassembler prints these symbolically. */

#define TIC80_OPERAND_CC	(1 << 30)

/* This operand is a bit number, either numeric or
   symbolic (like "eq.b", "or.f", etc).
   The disassembler prints these symbolically.
   Note that they appear in the instruction in 1's complement relative
   to the values given in the manual. */

#define TIC80_OPERAND_BITNUM	(1 << 31)

/* This mask is used to strip operand bits from an int that contains
   both operand bits and a numeric value in the lsbs. */

#define TIC80_OPERAND_MASK	(TIC80_OPERAND_GPR | TIC80_OPERAND_FPA | TIC80_OPERAND_CR | TIC80_OPERAND_CC | TIC80_OPERAND_BITNUM)


/* Flag bits for the struct tic80_opcode flags field. */

#define TIC80_VECTOR		01	/* Is a vector instruction */
#define TIC80_NO_R0_DEST	02	/* Register r0 cannot be a destination register */


/* The opcodes library contains a table that allows translation from predefined
   symbol names to numeric values, and vice versa. */

/* Structure to hold information about predefined symbols.  */

struct predefined_symbol
{
  char *name;		/* name to recognize */
  int value;
};

#define PDS_NAME(pdsp) ((pdsp) -> name)
#define PDS_VALUE(pdsp) ((pdsp) -> value)

/* Translation array.  */
extern const struct predefined_symbol tic80_predefined_symbols[];
/* How many members in the array.  */
extern const int tic80_num_predefined_symbols;

/* Translate value to symbolic name.  */
const char *tic80_value_to_symbol (int val, int class);

/* Translate symbolic name to value.  */
int tic80_symbol_to_value (char *name, int class);

const struct predefined_symbol *tic80_next_predefined_symbol
  (const struct predefined_symbol *);

#endif /* TIC80_H */
