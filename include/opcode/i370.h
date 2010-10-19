/* i370.h -- Header file for S/390 opcode table
   Copyright 1994, 1995, 1998, 1999, 2000, 2003 Free Software Foundation, Inc.
   PowerPC version written by Ian Lance Taylor, Cygnus Support
   Rewritten for i370 ESA/390 support, Linas Vepstas <linas@linas.org>

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

#ifndef I370_H
#define I370_H

/* The opcode table is an array of struct i370_opcode.  */
typedef union
{
   unsigned int   i[2];
   unsigned short s[4];
   unsigned char  b[8];
}  i370_insn_t;

struct i370_opcode
{
  /* The opcode name.  */
  const char *name;

  /* the length of the instruction */
  char len;

  /* The opcode itself.  Those bits which will be filled in with
     operands are zeroes.  */
  i370_insn_t opcode;

  /* The opcode mask.  This is used by the disassembler.  This is a
     mask containing ones indicating those bits which must match the
     opcode field, and zeroes indicating those bits which need not
     match (and are presumably filled in by operands).  */
  i370_insn_t mask;

  /* One bit flags for the opcode.  These are used to indicate which
     specific processors support the instructions.  The defined values
     are listed below.  */
  unsigned long flags;

  /* An array of operand codes.  Each code is an index into the
     operand table.  They appear in the order which the operands must
     appear in assembly code, and are terminated by a zero.  */
  unsigned char operands[8];
};

/* The table itself is sorted by major opcode number, and is otherwise
   in the order in which the disassembler should consider
   instructions.  */
extern const struct i370_opcode i370_opcodes[];
extern const int i370_num_opcodes;

/* Values defined for the flags field of a struct i370_opcode.  */

/* Opcode is defined for the original 360 architecture.  */
#define I370_OPCODE_360 (0x01)

/* Opcode is defined for the 370 architecture.  */
#define I370_OPCODE_370 (0x02)

/* Opcode is defined for the 370-XA architecture.  */
#define I370_OPCODE_370_XA (0x04)

/* Opcode is defined for the ESA/370 architecture.  */
#define I370_OPCODE_ESA370 (0x08)

/* Opcode is defined for the ESA/390 architecture.  */
#define I370_OPCODE_ESA390 (0x10)

/* Opcode is defined for the ESA/390 w/ BFP facility.  */
#define I370_OPCODE_ESA390_BF (0x20)

/* Opcode is defined for the ESA/390 w/ branch & set authority facility.  */
#define I370_OPCODE_ESA390_BS (0x40)

/* Opcode is defined for the ESA/390 w/ checksum facility.  */
#define I370_OPCODE_ESA390_CK (0x80)

/* Opcode is defined for the ESA/390 w/ compare & move extended facility.  */
#define I370_OPCODE_ESA390_CM (0x100)

/* Opcode is defined for the ESA/390 w/ flt.pt. support extensions facility. */
#define I370_OPCODE_ESA390_FX (0x200)

/* Opcode is defined for the ESA/390 w/ HFP facility. */
#define I370_OPCODE_ESA390_HX (0x400)

/* Opcode is defined for the ESA/390 w/ immediate & relative facility.  */
#define I370_OPCODE_ESA390_IR (0x800)

/* Opcode is defined for the ESA/390 w/ move-inverse facility.  */
#define I370_OPCODE_ESA390_MI (0x1000)

/* Opcode is defined for the ESA/390 w/ program-call-fast facility.  */
#define I370_OPCODE_ESA390_PC (0x2000)

/* Opcode is defined for the ESA/390 w/ perform-locked-op facility.  */
#define I370_OPCODE_ESA390_PL (0x4000)

/* Opcode is defined for the ESA/390 w/ square-root facility.  */
#define I370_OPCODE_ESA390_QR (0x8000)

/* Opcode is defined for the ESA/390 w/ resume-program facility.  */
#define I370_OPCODE_ESA390_RP (0x10000)

/* Opcode is defined for the ESA/390 w/ set-address-space-fast facility.  */
#define I370_OPCODE_ESA390_SA (0x20000)

/* Opcode is defined for the ESA/390 w/ subspace group facility.  */
#define I370_OPCODE_ESA390_SG (0x40000)

/* Opcode is defined for the ESA/390 w/ string facility.  */
#define I370_OPCODE_ESA390_SR (0x80000)

/* Opcode is defined for the ESA/390 w/ trap facility.  */
#define I370_OPCODE_ESA390_TR (0x100000)

#define I370_OPCODE_ESA390_SUPERSET (0x1fffff)


/* The operands table is an array of struct i370_operand.  */

struct i370_operand
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
  i370_insn_t (*insert)
    (i370_insn_t instruction, long op, const char **errmsg);

  /* Extraction function.  This is used by the disassembler.  To
     extract this operand type from an instruction, check this field.

     If it is NULL, compute
         op = ((i) >> o->shift) & ((1 << o->bits) - 1);
	 if ((o->flags & I370_OPERAND_SIGNED) != 0
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
  long (*extract) (i370_insn_t instruction, int *invalid);

  /* One bit syntax flags.  */
  unsigned long flags;

  /* name -- handy for debugging, otherwise pointless */
  char * name;
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the i370_opcodes table.  */

extern const struct i370_operand i370_operands[];

/* Values defined for the flags field of a struct i370_operand.  */

/* This operand should be wrapped in parentheses rather than
   separated from the previous by a comma.  This is used for S, RS and
   SS form instructions which want their operands to look like
   reg,displacement(basereg) */
#define I370_OPERAND_SBASE (0x01)

/* This operand is a base register.  It may or may not appear next
   to an index register, i.e. either of the two forms
   reg,displacement(basereg)
   reg,displacement(index,basereg) */
#define I370_OPERAND_BASE (0x02)

/* This pair of operands should be wrapped in parentheses rather than
   separated from the last by a comma.  This is used for the RX form
   instructions which want their operands to look like
   reg,displacement(index,basereg) */
#define I370_OPERAND_INDEX (0x04)

/* This operand names a register.  The disassembler uses this to print
   register names with a leading 'r'.  */
#define I370_OPERAND_GPR (0x08)

/* This operand names a floating point register.  The disassembler
   prints these with a leading 'f'.  */
#define I370_OPERAND_FPR (0x10)

/* This operand is a displacement.  */
#define I370_OPERAND_RELATIVE (0x20)

/* This operand is a length, such as that in SS form instructions.  */
#define I370_OPERAND_LENGTH (0x40)

/* This operand is optional, and is zero if omitted.  This is used for
   the optional B2 field in the shift-left, shift-right instructions.  The
   assembler must count the number of operands remaining on the line,
   and the number of operands remaining for the opcode, and decide
   whether this operand is present or not.  The disassembler should
   print this operand out only if it is not zero.  */
#define I370_OPERAND_OPTIONAL (0x80)


/* Define some misc macros.  We keep them with the operands table
   for simplicity.  The macro table is an array of struct i370_macro.  */

struct i370_macro
{
  /* The macro name.  */
  const char *name;

  /* The number of operands the macro takes.  */
  unsigned int operands;

  /* One bit flags for the opcode.  These are used to indicate which
     specific processors support the instructions.  The values are the
     same as those for the struct i370_opcode flags field.  */
  unsigned long flags;

  /* A format string to turn the macro into a normal instruction.
     Each %N in the string is replaced with operand number N (zero
     based).  */
  const char *format;
};

extern const struct i370_macro i370_macros[];
extern const int i370_num_macros;


#endif /* I370_H */
