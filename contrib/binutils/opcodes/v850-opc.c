/* Assemble V850 instructions.
   Copyright (C) 1996 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "ansidecl.h"
#include "opcode/v850.h"
#include <stdio.h>

/* regular opcode */
#define OP(x)		((x & 0x3f) << 5)
#define OP_MASK		OP (0x3f)

/* conditional branch opcode */
#define BOP(x)		((0x0b << 7) | (x & 0x0f))
#define BOP_MASK	((0x0f << 7) | 0x0f)

/* one-word opcodes */
#define one(x)		((unsigned int) (x))

/* two-word opcodes */
#define two(x,y)	((unsigned int) (x) | ((unsigned int) (y) << 16))



/* The functions used to insert and extract complicated operands.  */

/* Note: There is a conspiracy between these functions and
   v850_insert_operand() in gas/config/tc-v850.c.  Error messages
   containing the string 'out of range' will be ignored unless a
   specific command line option is given to GAS.  */

static const char * not_valid    = "displacement value is not in range and is not aligned";
static const char * out_of_range = "displacement value is out of range";
static const char * not_aligned  = "displacement value is not aligned";

static const char * immediate_out_of_range = "immediate value is out of range";

static unsigned long
insert_d9 (insn, value, errmsg)
     unsigned long insn;
     long          value;
     const char ** errmsg;
{
  if (value > 0xff || value < -0x100)
    {
      if ((value % 2) != 0)
	* errmsg = "branch value not in range and to odd offset";
      else
	* errmsg = "branch value out of range";
    }
  else if ((value % 2) != 0)
    * errmsg = "branch to odd offset";

  return (insn | ((value & 0x1f0) << 7) | ((value & 0x0e) << 3));
}

static unsigned long
extract_d9 (insn, invalid)
     unsigned long insn;
     int *         invalid;
{
  unsigned long ret = ((insn & 0xf800) >> 7) | ((insn & 0x0070) >> 3);

  if ((insn & 0x8000) != 0)
    ret -= 0x0200;

  return ret;
}

static unsigned long
insert_d22 (insn, value, errmsg)
     unsigned long insn;
     long          value;
     const char ** errmsg;
{
  if (value > 0x1fffff || value < -0x200000)
    {
      if ((value % 2) != 0)
	* errmsg = "branch value not in range and to an odd offset";
      else
	* errmsg = "branch value out of range";
    }
  else if ((value % 2) != 0)
    * errmsg = "branch to odd offset";

  return (insn | ((value & 0xfffe) << 16) | ((value & 0x3f0000) >> 16));
}

static unsigned long
extract_d22 (insn, invalid)
     unsigned long insn;
     int *         invalid;
{
  signed long ret = ((insn & 0xfffe0000) >> 16) | ((insn & 0x3f) << 16);

  return (unsigned long) ((ret << 10) >> 10);
}

static unsigned long
insert_d16_15 (insn, value, errmsg)
     unsigned long insn;
     long          value;
     const char ** errmsg;
{
  if (value > 0x7fff || value < -0x8000)
    {
      if ((value % 2) != 0)
	* errmsg = not_valid;
      else
	* errmsg = out_of_range;
    }
  else if ((value % 2) != 0)
    * errmsg = not_aligned;

  return insn | ((value & 0xfffe) << 16);
}

static unsigned long
extract_d16_15 (insn, invalid)
     unsigned long insn;
     int *         invalid;
{
  signed long ret = (insn & 0xfffe0000);

  return ret >> 16;
}

static unsigned long
insert_d8_7 (insn, value, errmsg)
     unsigned long insn;
     long          value;
     const char ** errmsg;
{
  if (value > 0xff || value < 0)
    {
      if ((value % 2) != 0)
	* errmsg = not_valid;
      else
	* errmsg = out_of_range;
    }
  else if ((value % 2) != 0)
    * errmsg = not_aligned;

  value >>= 1;

  return (insn | (value & 0x7f));
}

static unsigned long
extract_d8_7 (insn, invalid)
     unsigned long insn;
     int *         invalid;
{
  unsigned long ret = (insn & 0x7f);

  return ret << 1;
}

static unsigned long
insert_d8_6 (insn, value, errmsg)
     unsigned long insn;
     long          value;
     const char ** errmsg;
{
  if (value > 0xff || value < 0)
    {
      if ((value % 4) != 0)
	*errmsg = not_valid;
      else
	* errmsg = out_of_range;
    }
  else if ((value % 4) != 0)
    * errmsg = not_aligned;

  value >>= 1;

  return (insn | (value & 0x7e));
}

static unsigned long
extract_d8_6 (insn, invalid)
     unsigned long insn;
     int *         invalid;
{
  unsigned long ret = (insn & 0x7e);

  return ret << 1;
}



/* Warning: code in gas/config/tc-v850.c examines the contents of this array.
   If you change any of the values here, be sure to look for side effects in
   that code. */
const struct v850_operand v850_operands[] =
{
#define UNUSED	0
  { 0, 0, NULL, NULL, 0 }, 

/* The R1 field in a format 1, 6, 7, or 9 insn. */
#define R1	(UNUSED + 1)
  { 5, 0, NULL, NULL, V850_OPERAND_REG }, 

/* As above, but register 0 is not allowed.  */
#define R1_NOTR0 (R1 + 1)
  { 5, 0, NULL, NULL, V850_OPERAND_REG | V850_NOT_R0 }, 

/* The R2 field in a format 1, 2, 4, 5, 6, 7, 9 insn. */
#define R2	(R1_NOTR0 + 1)
  { 5, 11, NULL, NULL, V850_OPERAND_REG },

/* As above, but register 0 is not allowed.  */
#define R2_NOTR0 (R2 + 1)
  { 5, 11, NULL, NULL, V850_OPERAND_REG | V850_NOT_R0 },

/* The imm5 field in a format 2 insn. */
#define I5	(R2_NOTR0 + 1)
  { 5, 0, NULL, NULL, V850_OPERAND_SIGNED }, 

/* The unsigned imm5 field in a format 2 insn. */
#define I5U	(I5 + 1)
  { 5, 0, NULL, NULL, 0 },

/* The imm16 field in a format 6 insn. */
#define I16	(I5U + 1)
  { 16, 16, NULL, NULL, V850_OPERAND_SIGNED }, 

/* The signed disp7 field in a format 4 insn. */
#define D7	(I16 + 1)
  { 7, 0, NULL, NULL, 0},

/* The disp16 field in a format 6 insn. */
#define D16_15	(D7 + 1)
  { 15, 17, insert_d16_15, extract_d16_15, V850_OPERAND_SIGNED }, 

/* The 3 bit immediate field in format 8 insn.  */
#define B3	(D16_15 + 1)
  { 3, 11, NULL, NULL, 0 },

/* The 4 bit condition code in a setf instruction */
#define CCCC	(B3 + 1)
  { 4, 0, NULL, NULL, V850_OPERAND_CC },

/* The unsigned DISP8 field in a format 4 insn. */
#define D8_7	(CCCC + 1)
  { 7, 0, insert_d8_7, extract_d8_7, 0 },

/* The unsigned DISP8 field in a format 4 insn. */
#define D8_6	(D8_7 + 1)
  { 6, 1, insert_d8_6, extract_d8_6, 0 },

/* System register operands.  */
#define SR1	(D8_6 + 1)
  { 5, 0, NULL, NULL, V850_OPERAND_SRG },

/* EP Register.  */
#define EP	(SR1 + 1)
  { 0, 0, NULL, NULL, V850_OPERAND_EP },

/* The imm16 field (unsigned) in a format 6 insn. */
#define I16U	(EP + 1)
  { 16, 16, NULL, NULL, 0}, 

/* The R2 field as a system register.  */
#define SR2	(I16U + 1)
  { 5, 11, NULL, NULL, V850_OPERAND_SRG },

/* The disp16 field in a format 8 insn. */
#define D16	(SR2 + 1)
  { 16, 16, NULL, NULL, V850_OPERAND_SIGNED }, 

/* The DISP9 field in a format 3 insn, relaxable. */
#define D9_RELAX	(D16 + 1)
  { 9, 0, insert_d9, extract_d9, V850_OPERAND_RELAX | V850_OPERAND_SIGNED | V850_OPERAND_DISP },

/* The DISP22 field in a format 4 insn, relaxable.
   This _must_ follow D9_RELAX; the assembler assumes that the longer
   version immediately follows the shorter version for relaxing.  */
#define D22	(D9_RELAX + 1)
  { 22, 0, insert_d22, extract_d22, V850_OPERAND_SIGNED | V850_OPERAND_DISP },

} ; 


/* reg-reg instruction format (Format I) */
#define IF1	{R1, R2}

/* imm-reg instruction format (Format II) */
#define IF2	{I5, R2}

/* conditional branch instruction format (Format III) */
#define IF3	{D9_RELAX}

/* 3 operand instruction (Format VI) */
#define IF6	{I16, R1, R2}

/* 3 operand instruction (Format VI) */
#define IF6U	{I16U, R1, R2}



/* The opcode table.

   The format of the opcode table is:

   NAME		OPCODE			MASK		       { OPERANDS }	   MEMOP    PROCESSOR

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   OPERANDS is the list of operands.
   MEMOP specifies which operand (if any) is a memory operand.
   PROCESSORS specifies which CPU(s) support the opcode.
   
   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.

   The table is also sorted by name.  This is used by the assembler.
   When parsing an instruction the assembler finds the first occurance
   of the name of the instruciton in this table and then attempts to
   match the instruction's arguments with description of the operands
   associated with the entry it has just found in this table.  If the
   match fails the assembler looks at the next entry in this table.
   If that entry has the same name as the previous entry, then it
   tries to match the instruction against that entry and so on.  This
   is how the assembler copes with multiple, different formats of the
   same instruction.  */

const struct v850_opcode v850_opcodes[] =
{
{ "breakpoint",	0xffff,			0xffff,		      	{UNUSED},   		0, PROCESSOR_ALL },

{ "jmp",	one (0x0060),		one (0xffe0),	      	{R1}, 			1, PROCESSOR_ALL },
  
/* load/store instructions */
{ "sld.b",	one (0x0300),		one (0x0780),	      	{D7,   EP,   R2},	1, PROCESSOR_V850 },

  
{ "sld.h",	one (0x0400),		one (0x0780),	      	{D8_7, EP,   R2}, 	1, PROCESSOR_V850 },
{ "sld.w",	one (0x0500),		one (0x0781),	      	{D8_6, EP,   R2}, 	1, PROCESSOR_ALL },
{ "sst.b",	one (0x0380),		one (0x0780),	      	{R2,   D7,   EP}, 	2, PROCESSOR_ALL },
{ "sst.h",	one (0x0480),		one (0x0780),	      	{R2,   D8_7, EP}, 	2, PROCESSOR_ALL },
{ "sst.w",	one (0x0501),		one (0x0781),	      	{R2,   D8_6, EP}, 	2, PROCESSOR_ALL },

  

{ "ld.b",	two (0x0700, 0x0000),	two (0x07e0, 0x0000), 	{D16, R1, R2}, 		1, PROCESSOR_ALL },
{ "ld.h",	two (0x0720, 0x0000),	two (0x07e0, 0x0001), 	{D16_15, R1, R2}, 	1, PROCESSOR_ALL },
{ "ld.w",	two (0x0720, 0x0001),	two (0x07e0, 0x0001), 	{D16_15, R1, R2}, 	1, PROCESSOR_ALL },
{ "st.b",	two (0x0740, 0x0000),	two (0x07e0, 0x0000), 	{R2, D16, R1}, 		2, PROCESSOR_ALL },
{ "st.h",	two (0x0760, 0x0000),	two (0x07e0, 0x0001), 	{R2, D16_15, R1}, 	2, PROCESSOR_ALL },
{ "st.w",	two (0x0760, 0x0001),	two (0x07e0, 0x0001), 	{R2, D16_15, R1}, 	2, PROCESSOR_ALL },


/* arithmetic operation instructions */
{ "setf",	two (0x07e0, 0x0000),	two (0x07f0, 0xffff), 	{CCCC, R2}, 		0, PROCESSOR_ALL },
{ "divh",	OP  (0x02),		OP_MASK,		{R1, R2_NOTR0},		0, PROCESSOR_ALL },
  
  
{ "nop",	one (0x00),		one (0xffff),		{0}, 			0, PROCESSOR_ALL },
{ "mov",	OP  (0x10),		OP_MASK,		{I5, R2_NOTR0},		0, PROCESSOR_ALL },
{ "mov",        OP  (0x00),		OP_MASK,		{R1, R2_NOTR0},		0, PROCESSOR_ALL },
{ "movea",	OP  (0x31),		OP_MASK,		{I16, R1, R2_NOTR0},	0, PROCESSOR_ALL },
{ "movhi",	OP  (0x32),		OP_MASK,		{I16U, R1, R2_NOTR0},	0, PROCESSOR_ALL },
{ "add",	OP  (0x0e),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "add",	OP  (0x12),		OP_MASK,		IF2, 			0, PROCESSOR_ALL },
{ "addi",	OP  (0x30),		OP_MASK,		IF6, 			0, PROCESSOR_ALL },
{ "sub",	OP  (0x0d),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "subr", 	OP  (0x0c),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "mulh",	OP  (0x17),		OP_MASK,		{I5, R2_NOTR0},		0, PROCESSOR_ALL },
{ "mulh",	OP  (0x07),		OP_MASK,		{R1, R2_NOTR0},		0, PROCESSOR_ALL },
{ "mulhi",	OP  (0x37),		OP_MASK,		{I16, R1, R2_NOTR0},	0, PROCESSOR_ALL },
{ "cmp",	OP  (0x0f),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "cmp",	OP  (0x13),		OP_MASK,		IF2, 			0, PROCESSOR_ALL },
  
/* saturated operation instructions */
{ "satadd",	OP (0x11),		OP_MASK,		{I5, R2_NOTR0},		0, PROCESSOR_ALL },
{ "satadd",	OP (0x06),		OP_MASK,		{R1, R2_NOTR0},		0, PROCESSOR_ALL },
{ "satsub",	OP (0x05),		OP_MASK,		{R1, R2_NOTR0},		0, PROCESSOR_ALL },
{ "satsubi",	OP (0x33),		OP_MASK,		{I16, R1, R2_NOTR0},	0, PROCESSOR_ALL },
{ "satsubr",	OP (0x04),		OP_MASK,		{R1, R2_NOTR0},		0, PROCESSOR_ALL },

/* logical operation instructions */
{ "tst",	OP (0x0b),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "or",		OP (0x08),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "ori",	OP (0x34),		OP_MASK,		IF6U, 			0, PROCESSOR_ALL },
{ "and",	OP (0x0a),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "andi",	OP (0x36),		OP_MASK,		IF6U, 			0, PROCESSOR_ALL },
{ "xor",	OP (0x09),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "xori",	OP (0x35),		OP_MASK,		IF6U, 			0, PROCESSOR_ALL },
{ "not",	OP (0x01),		OP_MASK,		IF1, 			0, PROCESSOR_ALL },
{ "sar",	OP (0x15),		OP_MASK,		{I5U, R2}, 		0, PROCESSOR_ALL },
{ "sar",	two (0x07e0, 0x00a0),	two (0x07e0, 0xffff), 	{R1,  R2}, 		0, PROCESSOR_ALL },
{ "shl",	OP  (0x16),		OP_MASK,	      	{I5U, R2}, 		0, PROCESSOR_ALL },
{ "shl",	two (0x07e0, 0x00c0),	two (0x07e0, 0xffff), 	{R1,  R2}, 		0, PROCESSOR_ALL },
{ "shr",	OP  (0x14),		OP_MASK,	      	{I5U, R2}, 		0, PROCESSOR_ALL },
{ "shr",	two (0x07e0, 0x0080),	two (0x07e0, 0xffff), 	{R1,  R2}, 		0, PROCESSOR_ALL },

/* branch instructions */
	/* signed integer */
{ "bgt",	BOP (0xf),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bge",	BOP (0xe),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "blt",	BOP (0x6),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "ble",	BOP (0x7),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
	/* unsigned integer */
{ "bh",		BOP (0xb),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bnh",	BOP (0x3),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bl",		BOP (0x1),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bnl",	BOP (0x9),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
	/* common */
{ "be",		BOP (0x2),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bne",	BOP (0xa),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
	/* others */
{ "bv",		BOP (0x0),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bnv",	BOP (0x8),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bn",		BOP (0x4),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bp",		BOP (0xc),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bc",		BOP (0x1),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bnc",	BOP (0x9),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bz",		BOP (0x2),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bnz",	BOP (0xa),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "br",		BOP (0x5),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "bsa",	BOP (0xd),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },

/* Branch macros.

   We use the short form in the opcode/mask fields.  The assembler
   will twiddle bits as necessary if the long form is needed.  */

	/* signed integer */
{ "jgt",	BOP (0xf),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jge",	BOP (0xe),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jlt",	BOP (0x6),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jle",	BOP (0x7),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
	/* unsigned integer */
{ "jh",		BOP (0xb),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jnh",	BOP (0x3),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jl",		BOP (0x1),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jnl",	BOP (0x9),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
	/* common */
{ "je",		BOP (0x2),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jne",	BOP (0xa),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
	/* others */
{ "jv",		BOP (0x0),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jnv",	BOP (0x8),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jn",		BOP (0x4),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jp",		BOP (0xc),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jc",		BOP (0x1),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jnc",	BOP (0x9),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jz",		BOP (0x2),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jnz",	BOP (0xa),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jsa",	BOP (0xd),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
{ "jbr",	BOP (0x5),		BOP_MASK,		IF3, 			0, PROCESSOR_ALL },
  
{ "jr",		one (0x0780),		two (0xffc0, 0x0001),	{D22}, 			0, PROCESSOR_ALL },
{ "jarl",	one (0x0780),		two (0x07c0, 0x0001),	{D22, R2}, 		0, PROCESSOR_ALL}, 

/* bit manipulation instructions */
{ "set1",	two (0x07c0, 0x0000),	two (0xc7e0, 0x0000),	{B3, D16, R1}, 		2, PROCESSOR_ALL },
{ "not1",	two (0x47c0, 0x0000),	two (0xc7e0, 0x0000),	{B3, D16, R1}, 		2, PROCESSOR_ALL },
{ "clr1",	two (0x87c0, 0x0000),	two (0xc7e0, 0x0000),	{B3, D16, R1}, 		2, PROCESSOR_ALL },
{ "tst1",	two (0xc7c0, 0x0000),	two (0xc7e0, 0x0000),	{B3, D16, R1}, 		2, PROCESSOR_ALL },

/* special instructions */
{ "di",		two (0x07e0, 0x0160),	two (0xffff, 0xffff),	{0}, 			0, PROCESSOR_ALL },
{ "ei",		two (0x87e0, 0x0160),	two (0xffff, 0xffff),	{0}, 			0, PROCESSOR_ALL },
{ "halt",	two (0x07e0, 0x0120),	two (0xffff, 0xffff),	{0}, 			0, PROCESSOR_ALL },
{ "reti",	two (0x07e0, 0x0140),	two (0xffff, 0xffff),	{0}, 			0, PROCESSOR_ALL },
{ "trap",	two (0x07e0, 0x0100),	two (0xffe0, 0xffff),	{I5U}, 			0, PROCESSOR_ALL },
{ "ldsr",	two (0x07e0, 0x0020),	two (0x07e0, 0xffff),	{R1, SR2}, 		0, PROCESSOR_ALL },
{ "stsr",	two (0x07e0, 0x0040),	two (0x07e0, 0xffff),	{SR1, R2}, 		0, PROCESSOR_ALL },
{ 0, 0, 0, {0}, 0, 0 },

} ;

const int v850_num_opcodes =
  sizeof (v850_opcodes) / sizeof (v850_opcodes[0]);

