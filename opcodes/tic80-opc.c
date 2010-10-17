/* Opcode table for TI TMS320C80 (MVP).
   Copyright 1996, 1997, 2000 Free Software Foundation, Inc.

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
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <stdio.h>
#include "sysdep.h"
#include "opcode/tic80.h"

/* This file holds various tables for the TMS320C80 (MVP).

   The opcode table is strictly constant data, so the compiler should
   be able to put it in the .text section.

   This file also holds the operand table.  All knowledge about
   inserting operands into instructions and vice-versa is kept in this
   file.

   The predefined register table maps from register names to register
   values.  */


/* Table of predefined symbol names, such as general purpose registers,
   floating point registers, condition codes, control registers, and bit
   numbers.

   The table is sorted case independently by name so that it is suitable for
   searching via a binary search using a case independent comparison
   function.

   Note that the type of the symbol is stored in the upper bits of the value
   field, which allows the value and type to be passed around as a unit in a
   single int.  The types have to be masked off before using the numeric
   value as a number.
*/

const struct predefined_symbol tic80_predefined_symbols[] =
{
  { "a0",	TIC80_OPERAND_FPA | 0 },
  { "a1",	TIC80_OPERAND_FPA | 1 },
  { "alw.b",	TIC80_OPERAND_CC | 7 },
  { "alw.h",	TIC80_OPERAND_CC | 15 },
  { "alw.w",	TIC80_OPERAND_CC | 23 },
  { "ANASTAT",	TIC80_OPERAND_CR | 0x34 },
  { "BRK1",	TIC80_OPERAND_CR | 0x39 },
  { "BRK2",	TIC80_OPERAND_CR | 0x3A },
  { "CONFIG",	TIC80_OPERAND_CR | 2 },
  { "DLRU",	TIC80_OPERAND_CR | 0x500 },
  { "DTAG0",	TIC80_OPERAND_CR | 0x400 },
  { "DTAG1",	TIC80_OPERAND_CR | 0x401 },
  { "DTAG10",	TIC80_OPERAND_CR | 0x40A },
  { "DTAG11",	TIC80_OPERAND_CR | 0x40B },
  { "DTAG12",	TIC80_OPERAND_CR | 0x40C },
  { "DTAG13",	TIC80_OPERAND_CR | 0x40D },
  { "DTAG14",	TIC80_OPERAND_CR | 0x40E },
  { "DTAG15",	TIC80_OPERAND_CR | 0x40F },
  { "DTAG2",	TIC80_OPERAND_CR | 0x402 },
  { "DTAG3",	TIC80_OPERAND_CR | 0x403 },
  { "DTAG4",	TIC80_OPERAND_CR | 0x404 },
  { "DTAG5",	TIC80_OPERAND_CR | 0x405 },
  { "DTAG6",	TIC80_OPERAND_CR | 0x406 },
  { "DTAG7",	TIC80_OPERAND_CR | 0x407 },
  { "DTAG8",	TIC80_OPERAND_CR | 0x408 },
  { "DTAG9",	TIC80_OPERAND_CR | 0x409 },
  { "ECOMCNTL",	TIC80_OPERAND_CR | 0x33 },
  { "EIP",	TIC80_OPERAND_CR | 1 },
  { "EPC",	TIC80_OPERAND_CR | 0 },
  { "eq.b",	TIC80_OPERAND_BITNUM  | 0 },
  { "eq.f",	TIC80_OPERAND_BITNUM  | 20 },
  { "eq.h",	TIC80_OPERAND_BITNUM  | 10 },
  { "eq.w",	TIC80_OPERAND_BITNUM  | 20 },
  { "eq0.b",	TIC80_OPERAND_CC | 2 },
  { "eq0.h",	TIC80_OPERAND_CC | 10 },
  { "eq0.w",	TIC80_OPERAND_CC | 18 },
  { "FLTADR",	TIC80_OPERAND_CR | 0x11 },
  { "FLTDTH",	TIC80_OPERAND_CR | 0x14 },
  { "FLTDTL",	TIC80_OPERAND_CR | 0x13 },
  { "FLTOP",	TIC80_OPERAND_CR | 0x10 },
  { "FLTTAG",	TIC80_OPERAND_CR | 0x12 },
  { "FPST",	TIC80_OPERAND_CR | 8 },
  { "ge.b",	TIC80_OPERAND_BITNUM  | 5 },
  { "ge.f",	TIC80_OPERAND_BITNUM  | 25 },
  { "ge.h",	TIC80_OPERAND_BITNUM  | 15 },
  { "ge.w",	TIC80_OPERAND_BITNUM  | 25 },
  { "ge0.b",	TIC80_OPERAND_CC | 3 },
  { "ge0.h",	TIC80_OPERAND_CC | 11 },
  { "ge0.w",	TIC80_OPERAND_CC | 19 },
  { "gt.b",	TIC80_OPERAND_BITNUM  | 2 },
  { "gt.f",	TIC80_OPERAND_BITNUM  | 22 },
  { "gt.h",	TIC80_OPERAND_BITNUM  | 12 },
  { "gt.w",	TIC80_OPERAND_BITNUM  | 22 },
  { "gt0.b",	TIC80_OPERAND_CC | 1 },
  { "gt0.h",	TIC80_OPERAND_CC | 9 },
  { "gt0.w",	TIC80_OPERAND_CC | 17 },
  { "hi.b",	TIC80_OPERAND_BITNUM  | 6 },
  { "hi.h",	TIC80_OPERAND_BITNUM  | 16 },
  { "hi.w",	TIC80_OPERAND_BITNUM  | 26 },
  { "hs.b",	TIC80_OPERAND_BITNUM  | 9 },
  { "hs.h",	TIC80_OPERAND_BITNUM  | 19 },
  { "hs.w",	TIC80_OPERAND_BITNUM  | 29 },
  { "ib.f",	TIC80_OPERAND_BITNUM  | 28 },
  { "IE",	TIC80_OPERAND_CR | 6 },
  { "ILRU",	TIC80_OPERAND_CR | 0x300 },
  { "in.f",	TIC80_OPERAND_BITNUM  | 27 },
  { "IN0P",	TIC80_OPERAND_CR | 0x4000 },
  { "IN1P",	TIC80_OPERAND_CR | 0x4001 },
  { "INTPEN",	TIC80_OPERAND_CR | 4 },
  { "ITAG0",	TIC80_OPERAND_CR | 0x200 },
  { "ITAG1",	TIC80_OPERAND_CR | 0x201 },
  { "ITAG10",	TIC80_OPERAND_CR | 0x20A },
  { "ITAG11",	TIC80_OPERAND_CR | 0x20B },
  { "ITAG12",	TIC80_OPERAND_CR | 0x20C },
  { "ITAG13",	TIC80_OPERAND_CR | 0x20D },
  { "ITAG14",	TIC80_OPERAND_CR | 0x20E },
  { "ITAG15",	TIC80_OPERAND_CR | 0x20F },
  { "ITAG2",	TIC80_OPERAND_CR | 0x202 },
  { "ITAG3",	TIC80_OPERAND_CR | 0x203 },
  { "ITAG4",	TIC80_OPERAND_CR | 0x204 },
  { "ITAG5",	TIC80_OPERAND_CR | 0x205 },
  { "ITAG6",	TIC80_OPERAND_CR | 0x206 },
  { "ITAG7",	TIC80_OPERAND_CR | 0x207 },
  { "ITAG8",	TIC80_OPERAND_CR | 0x208 },
  { "ITAG9",	TIC80_OPERAND_CR | 0x209 },
  { "le.b",	TIC80_OPERAND_BITNUM  | 3 },
  { "le.f",	TIC80_OPERAND_BITNUM  | 23 },
  { "le.h",	TIC80_OPERAND_BITNUM  | 13 },
  { "le.w",	TIC80_OPERAND_BITNUM  | 23 },
  { "le0.b",	TIC80_OPERAND_CC | 6 },
  { "le0.h",	TIC80_OPERAND_CC | 14 },
  { "le0.w",	TIC80_OPERAND_CC | 22 },
  { "lo.b",	TIC80_OPERAND_BITNUM  | 8 },
  { "lo.h",	TIC80_OPERAND_BITNUM  | 18 },
  { "lo.w",	TIC80_OPERAND_BITNUM  | 28 },
  { "ls.b",	TIC80_OPERAND_BITNUM  | 7 },
  { "ls.h",	TIC80_OPERAND_BITNUM  | 17 },
  { "ls.w",	TIC80_OPERAND_BITNUM  | 27 },
  { "lt.b",	TIC80_OPERAND_BITNUM  | 4 },
  { "lt.f",	TIC80_OPERAND_BITNUM  | 24 },
  { "lt.h",	TIC80_OPERAND_BITNUM  | 14 },
  { "lt.w",	TIC80_OPERAND_BITNUM  | 24 },
  { "lt0.b",	TIC80_OPERAND_CC | 4 },
  { "lt0.h",	TIC80_OPERAND_CC | 12 },
  { "lt0.w",	TIC80_OPERAND_CC | 20 },
  { "MIP",	TIC80_OPERAND_CR | 0x31 },
  { "MPC",	TIC80_OPERAND_CR | 0x30 },
  { "ne.b",	TIC80_OPERAND_BITNUM  | 1 },
  { "ne.f",	TIC80_OPERAND_BITNUM  | 21 },
  { "ne.h",	TIC80_OPERAND_BITNUM  | 11 },
  { "ne.w",	TIC80_OPERAND_BITNUM  | 21 },
  { "ne0.b",	TIC80_OPERAND_CC | 5 },
  { "ne0.h",	TIC80_OPERAND_CC | 13 },
  { "ne0.w",	TIC80_OPERAND_CC | 21 },
  { "nev.b",	TIC80_OPERAND_CC | 0 },
  { "nev.h",	TIC80_OPERAND_CC | 8 },
  { "nev.w",	TIC80_OPERAND_CC | 16 },
  { "ob.f",	TIC80_OPERAND_BITNUM  | 29 },
  { "or.f",	TIC80_OPERAND_BITNUM  | 31 },
  { "ou.f",	TIC80_OPERAND_BITNUM  | 26 },
  { "OUTP",	TIC80_OPERAND_CR | 0x4002 },
  { "PKTREQ",	TIC80_OPERAND_CR | 0xD },
  { "PPERROR",	TIC80_OPERAND_CR | 0xA },
  { "r0",	TIC80_OPERAND_GPR | 0 },
  { "r1",	TIC80_OPERAND_GPR | 1 },
  { "r10",	TIC80_OPERAND_GPR | 10 },
  { "r11",	TIC80_OPERAND_GPR | 11 },
  { "r12",	TIC80_OPERAND_GPR | 12 },
  { "r13",	TIC80_OPERAND_GPR | 13 },
  { "r14",	TIC80_OPERAND_GPR | 14 },
  { "r15",	TIC80_OPERAND_GPR | 15 },
  { "r16",	TIC80_OPERAND_GPR | 16 },
  { "r17",	TIC80_OPERAND_GPR | 17 },
  { "r18",	TIC80_OPERAND_GPR | 18 },
  { "r19",	TIC80_OPERAND_GPR | 19 },
  { "r2",	TIC80_OPERAND_GPR | 2 },
  { "r20",	TIC80_OPERAND_GPR | 20 },
  { "r21",	TIC80_OPERAND_GPR | 21 },
  { "r22",	TIC80_OPERAND_GPR | 22 },
  { "r23",	TIC80_OPERAND_GPR | 23 },
  { "r24",	TIC80_OPERAND_GPR | 24 },
  { "r25",	TIC80_OPERAND_GPR | 25 },
  { "r26",	TIC80_OPERAND_GPR | 26 },
  { "r27",	TIC80_OPERAND_GPR | 27 },
  { "r28",	TIC80_OPERAND_GPR | 28 },
  { "r29",	TIC80_OPERAND_GPR | 29 },
  { "r3",	TIC80_OPERAND_GPR | 3 },
  { "r30",	TIC80_OPERAND_GPR | 30 },
  { "r31",	TIC80_OPERAND_GPR | 31 },
  { "r4",	TIC80_OPERAND_GPR | 4 },
  { "r5",	TIC80_OPERAND_GPR | 5 },
  { "r6",	TIC80_OPERAND_GPR | 6 },
  { "r7",	TIC80_OPERAND_GPR | 7 },
  { "r8",	TIC80_OPERAND_GPR | 8 },
  { "r9",	TIC80_OPERAND_GPR | 9 },
  { "SYSSTK",	TIC80_OPERAND_CR | 0x20 },
  { "SYSTMP",	TIC80_OPERAND_CR | 0x21 },
  { "TCOUNT",	TIC80_OPERAND_CR | 0xE },
  { "TSCALE",	TIC80_OPERAND_CR | 0xF },
  { "uo.f",	TIC80_OPERAND_BITNUM  | 30 },
};

const int tic80_num_predefined_symbols = sizeof (tic80_predefined_symbols) / sizeof (struct predefined_symbol);

/* This function takes a predefined symbol name in NAME, symbol class
   in CLASS, and translates it to a numeric value, which it returns.

   If CLASS is zero, any symbol that matches NAME is translated.  If
   CLASS is non-zero, then only a symbol that has class CLASS is
   matched.

   If no translation is possible, it returns -1, a value not used by
   any predefined symbol. Note that the predefined symbol array is
   presorted case independently by name.

   This function is implemented with the assumption that there are no
   duplicate names in the predefined symbol array, which happens to be
   true at the moment.

 */

int
tic80_symbol_to_value (name, class)
     char *name;
     int class;
{
  const struct predefined_symbol *pdsp;
  int low = 0;
  int middle;
  int high = tic80_num_predefined_symbols - 1;
  int cmp;
  int rtnval = -1;

  while (low <= high)
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, tic80_predefined_symbols[middle].name);
      if (cmp < 0)
	{
	  high = middle - 1;
	}
      else if (cmp > 0)
	{
	  low = middle + 1;
	}
      else 
	{
	  pdsp = &tic80_predefined_symbols[middle];
	  if ((class == 0) || (class & PDS_VALUE (pdsp)))
	    {
	      rtnval = PDS_VALUE (pdsp);
	    }
	  /* For now we assume that there are no duplicate names */
	  break;
	}
    }
  return (rtnval);
}

/* This function takes a value VAL and finds a matching predefined
   symbol that is in the operand class specified by CLASS.  If CLASS
   is zero, the first matching symbol is returned. */

const char *
tic80_value_to_symbol (val, class)
     int val;
     int class;
{
  const struct predefined_symbol *pdsp;
  int ival;
  char *name;

  name = NULL;
  for (pdsp = tic80_predefined_symbols;
       pdsp < tic80_predefined_symbols + tic80_num_predefined_symbols;
       pdsp++)
    {
      ival = PDS_VALUE (pdsp) & ~TIC80_OPERAND_MASK;
      if (ival == val)
	{
	  if ((class == 0) || (class & PDS_VALUE (pdsp)))
	    {
	      /* Found the desired match */
	      name = PDS_NAME (pdsp);
	      break;
	    }
	}
    }
  return (name);
}

/* This function returns a pointer to the next symbol in the predefined
   symbol table after PDSP, or NULL if PDSP points to the last symbol.  If
   PDSP is NULL, it returns the first symbol in the table.  Thus it can be
   used to walk through the table by first calling it with NULL and then
   calling it with each value it returned on the previous call, until it
   returns NULL. */

const struct predefined_symbol *
tic80_next_predefined_symbol (pdsp)
     const struct predefined_symbol *pdsp;
{
  if (pdsp == NULL)
    {
      pdsp = tic80_predefined_symbols;
    }
  else if (pdsp >= tic80_predefined_symbols &&
	   pdsp < tic80_predefined_symbols + tic80_num_predefined_symbols - 1)
    {
      pdsp++;
    }
  else
    {
      pdsp = NULL;
    }
  return (pdsp);
}



/* The operands table.  The fields are:

	bits, shift, insertion function, extraction function, flags
 */

const struct tic80_operand tic80_operands[] =
{

  /* The zero index is used to indicate the end of the list of operands.  */

#define UNUSED (0)
  { 0, 0, 0, 0, 0 },

  /* Short signed immediate value in bits 14-0. */

#define SSI (UNUSED + 1)
  { 15, 0, NULL, NULL, TIC80_OPERAND_SIGNED },

  /* Short unsigned immediate value in bits 14-0 */

#define SUI (SSI + 1)
  { 15, 0, NULL, NULL, 0 },

  /* Short unsigned bitfield in bits 14-0.  We distinguish this
     from a regular unsigned immediate value only for the convenience
     of the disassembler and the user. */

#define SUBF (SUI + 1)
  { 15, 0, NULL, NULL, TIC80_OPERAND_BITFIELD },

  /* Long signed immediate in following 32 bit word */

#define LSI (SUBF + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_SIGNED },

  /* Long unsigned immediate in following 32 bit word */

#define LUI (LSI + 1)
  { 32, 0, NULL, NULL, 0 },

  /* Long unsigned bitfield in following 32 bit word.  We distinguish
     this from a regular unsigned immediate value only for the
     convenience of the disassembler and the user. */

#define LUBF (LUI + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_BITFIELD },

  /* Single precision floating point immediate in following 32 bit
     word. */

#define SPFI (LUBF + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_FLOAT },

  /* Register in bits 4-0 */

#define REG_0 (SPFI + 1)
  { 5, 0, NULL, NULL, TIC80_OPERAND_GPR },

  /* Even register in bits 4-0 */

#define REG_0_E (REG_0 + 1)
  { 5, 0, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_EVEN },

  /* Register in bits 26-22 */

#define REG_22 (REG_0_E + 1)
  { 5, 22, NULL, NULL, TIC80_OPERAND_GPR },

  /* Even register in bits 26-22 */

#define REG_22_E (REG_22 + 1)
  { 5, 22, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_EVEN },

  /* Register in bits 31-27 */

#define REG_DEST (REG_22_E + 1)
  { 5, 27, NULL, NULL, TIC80_OPERAND_GPR },

  /* Even register in bits 31-27 */

#define REG_DEST_E (REG_DEST + 1)
  { 5, 27, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_EVEN },

  /* Floating point accumulator register (a0-a3) specified by bit 16 (MSB)
     and bit 11 (LSB) */
  /* FIXME!  Needs to use functions to insert and extract the register
     number in bits 16 and 11. */

#define REG_FPA (REG_DEST_E + 1)
  { 0, 0, NULL, NULL, TIC80_OPERAND_FPA },

  /* Short signed PC word offset in bits 14-0 */

#define OFF_SS_PC (REG_FPA + 1)
  { 15, 0, NULL, NULL, TIC80_OPERAND_PCREL | TIC80_OPERAND_SIGNED },

  /* Long signed PC word offset in following 32 bit word */

#define OFF_SL_PC (OFF_SS_PC + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_PCREL | TIC80_OPERAND_SIGNED },

  /* Short signed base relative byte offset in bits 14-0 */

#define OFF_SS_BR (OFF_SL_PC + 1)
  { 15, 0, NULL, NULL, TIC80_OPERAND_BASEREL | TIC80_OPERAND_SIGNED },

  /* Long signed base relative byte offset in following 32 bit word */

#define OFF_SL_BR (OFF_SS_BR + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_BASEREL | TIC80_OPERAND_SIGNED },

  /* Long signed base relative byte offset in following 32 bit word
     with optional ":s" modifier flag in bit 11 */

#define OFF_SL_BR_SCALED (OFF_SL_BR + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_BASEREL | TIC80_OPERAND_SIGNED | TIC80_OPERAND_SCALED },

  /* BITNUM in bits 31-27 */

#define BITNUM (OFF_SL_BR_SCALED + 1)
  { 5, 27, NULL, NULL, TIC80_OPERAND_BITNUM },

  /* Condition code in bits 31-27 */

#define CC (BITNUM + 1)
  { 5, 27, NULL, NULL, TIC80_OPERAND_CC },

  /* Control register number in bits 14-0 */

#define CR_SI (CC + 1)
  { 15, 0, NULL, NULL, TIC80_OPERAND_CR },

  /* Control register number in next 32 bit word */

#define CR_LI (CR_SI + 1)
  { 32, 0, NULL, NULL, TIC80_OPERAND_CR },

  /* A base register in bits 26-22, enclosed in parens */

#define REG_BASE (CR_LI + 1)
  { 5, 22, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_PARENS },

  /* A base register in bits 26-22, enclosed in parens, with optional ":m"
     flag in bit 17 (short immediate instructions only) */

#define REG_BASE_M_SI (REG_BASE + 1)
  { 5, 22, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_PARENS | TIC80_OPERAND_M_SI },

  /* A base register in bits 26-22, enclosed in parens, with optional ":m"
   flag in bit 15 (long immediate and register instructions only) */

#define REG_BASE_M_LI (REG_BASE_M_SI + 1)
  { 5, 22, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_PARENS | TIC80_OPERAND_M_LI },

  /* Scaled register in bits 4-0, with optional ":s" modifier flag in bit 11 */

#define REG_SCALED (REG_BASE_M_LI + 1)
  { 5, 0, NULL, NULL, TIC80_OPERAND_GPR | TIC80_OPERAND_SCALED },

  /* Unsigned immediate in bits 4-0, used only for shift instructions */

#define ROTATE (REG_SCALED + 1)
  { 5, 0, NULL, NULL, 0 },

  /* Unsigned immediate in bits 9-5, used only for shift instructions */
#define ENDMASK (ROTATE + 1)
  { 5, 5, NULL, NULL, TIC80_OPERAND_ENDMASK },

};

const int tic80_num_operands = sizeof (tic80_operands)/sizeof(*tic80_operands);


/* Macros used to generate entries for the opcodes table. */

#define FIXME 0

/* Short-Immediate Format Instructions - basic opcode */
#define OP_SI(x)	(((x) & 0x7F) << 15)
#define MASK_SI		OP_SI(0x7F)

/* Long-Immediate Format Instructions - basic opcode */
#define OP_LI(x)	(((x) & 0x3FF) << 12)
#define MASK_LI		OP_LI(0x3FF)

/* Register Format Instructions - basic opcode */
#define OP_REG(x)	OP_LI(x)	/* For readability */
#define MASK_REG	MASK_LI		/* For readability */

/* The 'n' bit at bit 10 */
#define n(x)		((x) << 10)

/* The 'i' bit at bit 11 */
#define i(x)		((x) << 11)

/* The 'F' bit at bit 27 */
#define F(x)		((x) << 27)

/* The 'E' bit at bit 27 */
#define E(x)		((x) << 27)

/* The 'M' bit at bit 15 in register and long immediate opcodes */
#define M_REG(x)	((x) << 15)
#define M_LI(x)		((x) << 15)

/* The 'M' bit at bit 17 in short immediate opcodes */
#define M_SI(x)		((x) << 17)

/* The 'SZ' field at bits 14-13 in register and long immediate opcodes */
#define SZ_REG(x)	((x) << 13)
#define SZ_LI(x)	((x) << 13)

/* The 'SZ' field at bits 16-15 in short immediate opcodes */
#define SZ_SI(x)	((x) << 15)

/* The 'D' (direct external memory access) bit at bit 10 in long immediate
   and register opcodes. */
#define D(x)		((x) << 10)

/* The 'S' (scale offset by data size) bit at bit 11 in long immediate
   and register opcodes. */
#define S(x)		((x) << 11)

/* The 'PD' field at bits 10-9 in floating point instructions */
#define PD(x)		((x) << 9)

/* The 'P2' field at bits 8-7 in floating point instructions */
#define P2(x)		((x) << 7)

/* The 'P1' field at bits 6-5 in floating point instructions */
#define P1(x)		((x) << 5)

/* The 'a' field at bit 16 in vector instructions */
#define V_a1(x)		((x) << 16)

/* The 'a' field at bit 11 in vector instructions */
#define V_a0(x)		((x) << 11)

/* The 'm' field at bit 10 in vector instructions */
#define V_m(x)		((x) << 10)

/* The 'S' field at bit 9 in vector instructions */
#define V_S(x)		((x) << 9)

/* The 'Z' field at bit 8 in vector instructions */
#define V_Z(x)		((x) << 8)

/* The 'p' field at bit 6 in vector instructions */
#define V_p(x)		((x) << 6)

/* The opcode field at bits 21-17 for vector instructions */
#define OP_V(x)		((x) << 17)
#define MASK_V		OP_V(0x1F)


/* The opcode table.  Formatted for better readability on a wide screen.  Also, all
 entries with the same mnemonic are sorted so that they are adjacent in the table,
 allowing the use of a hash table to locate the first of a sequence of opcodes that have
 a particular name.  The short immediate forms also come before the long immediate forms
 so that the assembler will pick the "best fit" for the size of the operand, except for
 the case of the PC relative forms, where the long forms come first and are the default
 forms. */

const struct tic80_opcode tic80_opcodes[] = {

  /* The "nop" instruction is really "rdcr 0,r0".  We put it first so that this
     specific bit pattern will get disassembled as a nop rather than an rdcr. The
     mask of all ones ensures that this will happen. */

  {"nop",	OP_SI(0x4),	~0,		0,		{0}			},

  /* The "br" instruction is really "bbz target,r0,31".  We put it first so that
     this specific bit pattern will get disassembled as a br rather than bbz. */

  {"br",	OP_SI(0x48),	0xFFFF8000,	0,	{OFF_SS_PC}	},
  {"br",	OP_LI(0x391),	0xFFFFF000,	0,	{OFF_SL_PC}	},
  {"br",	OP_REG(0x390),	0xFFFFF000,	0,	{REG_0}		},
  {"br.a",	OP_SI(0x49),	0xFFFF8000,	0,	{OFF_SS_PC}	},
  {"br.a",	OP_LI(0x393),	0xFFFFF000,	0,	{OFF_SL_PC}	},
  {"br.a",	OP_REG(0x392),	0xFFFFF000,	0,	{REG_0}		},

  /* Signed integer ADD */

  {"add",	OP_SI(0x58),	MASK_SI,	0,	{SSI, REG_22, REG_DEST}		},
  {"add",	OP_LI(0x3B1),	MASK_LI,	0,	{LSI, REG_22, REG_DEST}		},
  {"add",	OP_REG(0x3B0),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Unsigned integer ADD */

  {"addu",	OP_SI(0x59),	MASK_SI,	0,	{SSI, REG_22, REG_DEST}		},
  {"addu",	OP_LI(0x3B3),	MASK_LI,	0,	{LSI, REG_22, REG_DEST}		},
  {"addu",	OP_REG(0x3B2),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Bitwise AND */

  {"and",	OP_SI(0x11),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST}	},
  {"and",	OP_LI(0x323),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST}	},
  {"and",	OP_REG(0x322),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},
  {"and.tt",	OP_SI(0x11),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST}	},
  {"and.tt",	OP_LI(0x323),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST}	},
  {"and.tt",	OP_REG(0x322),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Bitwise AND with ones complement of both sources */

  {"and.ff",	OP_SI(0x18),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST}	},
  {"and.ff",	OP_LI(0x331),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST}	},
  {"and.ff",	OP_REG(0x330),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Bitwise AND with ones complement of source 1 */

  {"and.ft",	OP_SI(0x14),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST}	},
  {"and.ft",	OP_LI(0x329),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST}	},
  {"and.ft",	OP_REG(0x328),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Bitwise AND with ones complement of source 2 */

  {"and.tf",	OP_SI(0x12),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST}	},
  {"and.tf",	OP_LI(0x325),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST}	},
  {"and.tf",	OP_REG(0x324),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Branch Bit One - nonannulled */

  {"bbo",	OP_SI(0x4A),	MASK_SI,	0, 	{OFF_SS_PC, REG_22, BITNUM}	},
  {"bbo",	OP_LI(0x395),	MASK_LI,	0, 	{OFF_SL_PC, REG_22, BITNUM}	},
  {"bbo",	OP_REG(0x394),	MASK_REG,	0,	{REG_0, REG_22, BITNUM}		},

  /* Branch Bit One - annulled */

  {"bbo.a",	OP_SI(0x4B),	MASK_SI,	0, 	{OFF_SS_PC, REG_22, BITNUM}	},
  {"bbo.a",	OP_LI(0x397),	MASK_LI,	0, 	{OFF_SL_PC, REG_22, BITNUM}	},
  {"bbo.a",	OP_REG(0x396),	MASK_REG,	0,	{REG_0, REG_22, BITNUM}		},

  /* Branch Bit Zero - nonannulled */

  {"bbz",	OP_SI(0x48),	MASK_SI,	0, 	{OFF_SS_PC, REG_22, BITNUM}	},
  {"bbz",	OP_LI(0x391),	MASK_LI,	0, 	{OFF_SL_PC, REG_22, BITNUM}	},
  {"bbz",	OP_REG(0x390),	MASK_REG,	0,	{REG_0, REG_22, BITNUM}		},

  /* Branch Bit Zero - annulled */

  {"bbz.a",	OP_SI(0x49),	MASK_SI,	0, 	{OFF_SS_PC, REG_22, BITNUM}	},
  {"bbz.a",	OP_LI(0x393),	MASK_LI,	0, 	{OFF_SL_PC, REG_22, BITNUM}	},
  {"bbz.a",	OP_REG(0x392),	MASK_REG,	0,	{REG_0, REG_22, BITNUM}		},

  /* Branch Conditional - nonannulled */

  {"bcnd",	OP_SI(0x4C),	MASK_SI,	0, 	{OFF_SS_PC, REG_22, CC}	},
  {"bcnd",	OP_LI(0x399),	MASK_LI,	0, 	{OFF_SL_PC, REG_22, CC}	},
  {"bcnd",	OP_REG(0x398),	MASK_REG,	0,	{REG_0, REG_22, CC}	},

  /* Branch Conditional - annulled */

  {"bcnd.a",	OP_SI(0x4D),	MASK_SI,	0, 	{OFF_SS_PC, REG_22, CC}	},
  {"bcnd.a",	OP_LI(0x39B),	MASK_LI,	0, 	{OFF_SL_PC, REG_22, CC}	},
  {"bcnd.a",	OP_REG(0x39A),	MASK_REG,	0,	{REG_0, REG_22, CC}	},

  /* Branch Control Register */

  {"brcr",	OP_SI(0x6),	MASK_SI,	0,	{CR_SI}	},
  {"brcr",	OP_LI(0x30D),	MASK_LI,	0,	{CR_LI}	},
  {"brcr",	OP_REG(0x30C),	MASK_REG,	0,	{REG_0}	},

  /* Branch and save return - nonannulled */

  {"bsr",	OP_SI(0x40),	MASK_SI,	0,	{OFF_SS_PC, REG_DEST}	},
  {"bsr",	OP_LI(0x381),	MASK_LI,	0, 	{OFF_SL_PC, REG_DEST}	},
  {"bsr",	OP_REG(0x380),	MASK_REG,	0,	{REG_0, REG_DEST}	},

  /* Branch and save return - annulled */

  {"bsr.a",	OP_SI(0x41),	MASK_SI,	0, 	{OFF_SS_PC, REG_DEST}	},
  {"bsr.a",	OP_LI(0x383),	MASK_LI,	0, 	{OFF_SL_PC, REG_DEST}	},
  {"bsr.a",	OP_REG(0x382),	MASK_REG,	0,	{REG_0, REG_DEST}	},

  /* Send command */

  {"cmnd",	OP_SI(0x2),	MASK_SI,	0, 	{SUI}	},
  {"cmnd",	OP_LI(0x305),	MASK_LI,	0, 	{LUI}	},
  {"cmnd",	OP_REG(0x304),	MASK_REG,	0,	{REG_0}	},

  /* Integer compare */

  {"cmp",	OP_SI(0x50),	MASK_SI,	0, 	{SSI, REG_22, REG_DEST}		},
  {"cmp",	OP_LI(0x3A1),	MASK_LI,	0, 	{LSI, REG_22, REG_DEST}		},
  {"cmp",	OP_REG(0x3A0),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Flush data cache subblock - don't clear subblock preset flag */

  {"dcachec",	OP_SI(0x38),	F(1) | (MASK_SI  & ~M_SI(1)),			0, {SSI, REG_BASE_M_SI}		},
  {"dcachec",	OP_LI(0x371),	F(1) | (MASK_LI  & ~M_LI(1))  | S(1) | D(1),	0, {LSI, REG_BASE_M_LI}		},
  {"dcachec",	OP_REG(0x370),	F(1) | (MASK_REG & ~M_REG(1)) | S(1) | D(1),	0, {REG_0, REG_BASE_M_LI}	},

  /* Flush data cache subblock - clear subblock preset flag */

  {"dcachef",	OP_SI(0x38)   | F(1),	F(1) | (MASK_SI  & ~M_SI(1)),			0, {SSI, REG_BASE_M_SI}		},
  {"dcachef",	OP_LI(0x371)  | F(1),	F(1) | (MASK_LI  & ~M_LI(1))   | S(1) | D(1),	0, {LSI, REG_BASE_M_LI}		},
  {"dcachef",	OP_REG(0x370) | F(1),	F(1) | (MASK_REG & ~M_REG(1)) | S(1) | D(1),	0, {REG_0, REG_BASE_M_LI}	},

  /* Direct load signed data into register */

  {"dld",	OP_LI(0x345)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dld",	OP_REG(0x344) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"dld.b",	OP_LI(0x341)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dld.b",	OP_REG(0x340) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"dld.d",	OP_LI(0x347)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST_E}	},
  {"dld.d",	OP_REG(0x346) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST_E}		},
  {"dld.h",	OP_LI(0x343)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dld.h",	OP_REG(0x342) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},

  /* Direct load unsigned data into register */

  {"dld.ub",	OP_LI(0x351)  | D(1),	(MASK_LI  &  ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dld.ub",	OP_REG(0x350) | D(1),	(MASK_REG & ~M_REG(1))  | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"dld.uh",	OP_LI(0x353)  | D(1),	(MASK_LI  &  ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dld.uh",	OP_REG(0x352) | D(1),	(MASK_REG & ~M_REG(1))  | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},

  /* Direct store data into memory */

  {"dst",	OP_LI(0x365)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dst",	OP_REG(0x364) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"dst.b",	OP_LI(0x361)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dst.b",	OP_REG(0x360) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"dst.d",	OP_LI(0x367)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST_E}	},
  {"dst.d",	OP_REG(0x366) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST_E}		},
  {"dst.h",	OP_LI(0x363)  | D(1),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"dst.h",	OP_REG(0x362) | D(1),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},

  /* Emulation stop */

  {"estop",	OP_LI(0x3FC),	MASK_LI,	0,		{0}	},

  /* Emulation trap */

  {"etrap",	OP_SI(0x1)    | E(1),	MASK_SI  | E(1),	0,	{SUI}	},
  {"etrap",	OP_LI(0x303)  | E(1),	MASK_LI  | E(1),	0,	{LUI}	},
  {"etrap",	OP_REG(0x302) | E(1),	MASK_REG | E(1),	0,	{REG_0}	},

  /* Floating-point addition */

  {"fadd.ddd",	OP_REG(0x3E0) | PD(1) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22_E, REG_DEST_E}	},
  {"fadd.dsd",	OP_REG(0x3E0) | PD(1) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22, REG_DEST_E}	},
  {"fadd.sdd",	OP_LI(0x3E1)  | PD(1) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22_E, REG_DEST_E}	},
  {"fadd.sdd",	OP_REG(0x3E0) | PD(1) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22_E, REG_DEST_E}	},
  {"fadd.ssd",	OP_LI(0x3E1)  | PD(1) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST_E}	},
  {"fadd.ssd",	OP_REG(0x3E0) | PD(1) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST_E}	},
  {"fadd.sss",	OP_LI(0x3E1)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST}	},
  {"fadd.sss",	OP_REG(0x3E0) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST}	},

  /* Floating point compare */

  {"fcmp.dd",	OP_REG(0x3EA) | PD(0) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3),  0,	 {REG_0_E, REG_22_E, REG_DEST}	},
  {"fcmp.ds",	OP_REG(0x3EA) | PD(0) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3),  0,	 {REG_0_E, REG_22, REG_DEST}	},
  {"fcmp.sd",	OP_LI(0x3EB)  | PD(0) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3),  0,	 {SPFI, REG_22_E, REG_DEST}	},
  {"fcmp.sd",	OP_REG(0x3EA) | PD(0) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3),  0,	 {REG_0, REG_22_E, REG_DEST}	},
  {"fcmp.ss",	OP_LI(0x3EB)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3),  0,	 {SPFI, REG_22, REG_DEST}	},
  {"fcmp.ss",	OP_REG(0x3EA) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3),  0,	 {REG_0, REG_22, REG_DEST}	},

  /* Floating point divide */

  {"fdiv.ddd",	OP_REG(0x3E6) | PD(1) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22_E, REG_DEST_E}	},
  {"fdiv.dsd",	OP_REG(0x3E6) | PD(1) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22, REG_DEST_E}	},
  {"fdiv.sdd",	OP_LI(0x3E7)  | PD(1) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22_E, REG_DEST_E}	},
  {"fdiv.sdd",	OP_REG(0x3E6) | PD(1) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22_E, REG_DEST_E}	},
  {"fdiv.ssd",	OP_LI(0x3E7)  | PD(1) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST_E}	},
  {"fdiv.ssd",	OP_REG(0x3E6) | PD(1) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST_E}	},
  {"fdiv.sss",	OP_LI(0x3E7)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST}	},
  {"fdiv.sss",	OP_REG(0x3E6) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST}	},

  /* Floating point multiply */

  {"fmpy.ddd",	OP_REG(0x3E4) | PD(1) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22_E, REG_DEST_E}	},
  {"fmpy.dsd",	OP_REG(0x3E4) | PD(1) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22, REG_DEST_E}	},
  {"fmpy.iii",	OP_LI(0x3E5)  | PD(2) | P2(2) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_22, REG_DEST}	},
  {"fmpy.iii",	OP_REG(0x3E4) | PD(2) | P2(2) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST}	},
  {"fmpy.sdd",	OP_LI(0x3E5)  | PD(1) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22_E, REG_DEST_E}	},
  {"fmpy.sdd",	OP_REG(0x3E4) | PD(1) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22_E, REG_DEST_E}	},
  {"fmpy.ssd",	OP_LI(0x3E5)  | PD(1) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST_E}	},
  {"fmpy.ssd",	OP_REG(0x3E4) | PD(1) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST_E}	},
  {"fmpy.sss",	OP_LI(0x3E5)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST}	},
  {"fmpy.sss",	OP_REG(0x3E4) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST}	},
  {"fmpy.uuu",	OP_LI(0x3E5)  | PD(3) | P2(3) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LUI, REG_22, REG_DEST}	},
  {"fmpy.uuu",	OP_REG(0x3E4) | PD(3) | P2(3) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST}	},

  /* Convert/Round to Minus Infinity */

  {"frndm.dd",	OP_REG(0x3E8) | PD(1) | P2(3) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST_E}	},
  {"frndm.di",	OP_REG(0x3E8) | PD(2) | P2(3) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndm.ds",	OP_REG(0x3E8) | PD(0) | P2(3) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndm.du",	OP_REG(0x3E8) | PD(3) | P2(3) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndm.id",	OP_LI(0x3E9)  | PD(1) | P2(3) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndm.id",	OP_REG(0x3E8) | PD(1) | P2(3) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndm.is",	OP_LI(0x3E9)  | PD(0) | P2(3) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndm.is",	OP_REG(0x3E8) | PD(0) | P2(3) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndm.sd",	OP_LI(0x3E9)  | PD(1) | P2(3) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST_E}	},
  {"frndm.sd",	OP_REG(0x3E8) | PD(1) | P2(3) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndm.si",	OP_LI(0x3E9)  | PD(2) | P2(3) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndm.si",	OP_REG(0x3E8) | PD(2) | P2(3) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndm.ss",	OP_LI(0x3E9)  | PD(0) | P2(3) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndm.ss",	OP_REG(0x3E8) | PD(0) | P2(3) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndm.su",	OP_LI(0x3E9)  | PD(3) | P2(3) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndm.su",	OP_REG(0x3E8) | PD(3) | P2(3) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndm.ud",	OP_LI(0x3E9)  | PD(1) | P2(3) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndm.ud",	OP_REG(0x3E8) | PD(1) | P2(3) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndm.us",	OP_LI(0x3E9)  | PD(0) | P2(3) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndm.us",	OP_REG(0x3E8) | PD(0) | P2(3) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},

  /* Convert/Round to Nearest */

  {"frndn.dd",	OP_REG(0x3E8) | PD(1) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST_E}	},
  {"frndn.di",	OP_REG(0x3E8) | PD(2) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndn.ds",	OP_REG(0x3E8) | PD(0) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndn.du",	OP_REG(0x3E8) | PD(3) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndn.id",	OP_LI(0x3E9)  | PD(1) | P2(0) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndn.id",	OP_REG(0x3E8) | PD(1) | P2(0) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndn.is",	OP_LI(0x3E9)  | PD(0) | P2(0) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndn.is",	OP_REG(0x3E8) | PD(0) | P2(0) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndn.sd",	OP_LI(0x3E9)  | PD(1) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST_E}	},
  {"frndn.sd",	OP_REG(0x3E8) | PD(1) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndn.si",	OP_LI(0x3E9)  | PD(2) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndn.si",	OP_REG(0x3E8) | PD(2) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndn.ss",	OP_LI(0x3E9)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndn.ss",	OP_REG(0x3E8) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndn.su",	OP_LI(0x3E9)  | PD(3) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndn.su",	OP_REG(0x3E8) | PD(3) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndn.ud",	OP_LI(0x3E9)  | PD(1) | P2(0) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndn.ud",	OP_REG(0x3E8) | PD(1) | P2(0) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndn.us",	OP_LI(0x3E9)  | PD(0) | P2(0) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndn.us",	OP_REG(0x3E8) | PD(0) | P2(0) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},

  /* Convert/Round to Positive Infinity */

  {"frndp.dd",	OP_REG(0x3E8) | PD(1) | P2(2) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST_E}	},
  {"frndp.di",	OP_REG(0x3E8) | PD(2) | P2(2) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndp.ds",	OP_REG(0x3E8) | PD(0) | P2(2) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndp.du",	OP_REG(0x3E8) | PD(3) | P2(2) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndp.id",	OP_LI(0x3E9)  | PD(1) | P2(2) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndp.id",	OP_REG(0x3E8) | PD(1) | P2(2) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndp.is",	OP_LI(0x3E9)  | PD(0) | P2(2) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndp.is",	OP_REG(0x3E8) | PD(0) | P2(2) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndp.sd",	OP_LI(0x3E9)  | PD(1) | P2(2) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST_E}	},
  {"frndp.sd",	OP_REG(0x3E8) | PD(1) | P2(2) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndp.si",	OP_LI(0x3E9)  | PD(2) | P2(2) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndp.si",	OP_REG(0x3E8) | PD(2) | P2(2) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndp.ss",	OP_LI(0x3E9)  | PD(0) | P2(2) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndp.ss",	OP_REG(0x3E8) | PD(0) | P2(2) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndp.su",	OP_LI(0x3E9)  | PD(3) | P2(2) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndp.su",	OP_REG(0x3E8) | PD(3) | P2(2) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndp.ud",	OP_LI(0x3E9)  | PD(1) | P2(2) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndp.ud",	OP_REG(0x3E8) | PD(1) | P2(2) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndp.us",	OP_LI(0x3E9)  | PD(0) | P2(2) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndp.us",	OP_REG(0x3E8) | PD(0) | P2(2) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},

  /* Convert/Round to Zero */

  {"frndz.dd",	OP_REG(0x3E8) | PD(1) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST_E}	},
  {"frndz.di",	OP_REG(0x3E8) | PD(2) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndz.ds",	OP_REG(0x3E8) | PD(0) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndz.du",	OP_REG(0x3E8) | PD(3) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST}	},
  {"frndz.id",	OP_LI(0x3E9)  | PD(1) | P2(1) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndz.id",	OP_REG(0x3E8) | PD(1) | P2(1) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndz.is",	OP_LI(0x3E9)  | PD(0) | P2(1) | P1(2),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndz.is",	OP_REG(0x3E8) | PD(0) | P2(1) | P1(2),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndz.sd",	OP_LI(0x3E9)  | PD(1) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST_E}	},
  {"frndz.sd",	OP_REG(0x3E8) | PD(1) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndz.si",	OP_LI(0x3E9)  | PD(2) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndz.si",	OP_REG(0x3E8) | PD(2) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndz.ss",	OP_LI(0x3E9)  | PD(0) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndz.ss",	OP_REG(0x3E8) | PD(0) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndz.su",	OP_LI(0x3E9)  | PD(3) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"frndz.su",	OP_REG(0x3E8) | PD(3) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},
  {"frndz.ud",	OP_LI(0x3E9)  | PD(1) | P2(1) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST_E}	},
  {"frndz.ud",	OP_REG(0x3E8) | PD(1) | P2(1) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"frndz.us",	OP_LI(0x3E9)  | PD(0) | P2(1) | P1(3),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {LSI, REG_DEST}	},
  {"frndz.us",	OP_REG(0x3E8) | PD(0) | P2(1) | P1(3),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},

  /* Floating point square root */

  {"fsqrt.dd",	OP_REG(0x3EE) | PD(1) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_DEST_E}	},
  {"fsqrt.sd",	OP_LI(0x3EF)  | PD(1) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST_E}	},
  {"fsqrt.sd",	OP_REG(0x3EE) | PD(1) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST_E}	},
  {"fsqrt.ss",	OP_LI(0x3EF)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_DEST}	},
  {"fsqrt.ss",	OP_REG(0x3EE) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_DEST}	},

  /* Floating point subtraction */

  { "fsub.ddd",	OP_REG(0x3E2) | PD(1) | P2(1) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22_E, REG_DEST_E}	},
  { "fsub.dsd",	OP_REG(0x3E2) | PD(1) | P2(0) | P1(1),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0_E, REG_22, REG_DEST_E}	},
  { "fsub.sdd",	OP_LI(0x3E3)  | PD(1) | P2(1) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22_E, REG_DEST_E}	},
  { "fsub.sdd",	OP_REG(0x3E2) | PD(1) | P2(1) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22_E, REG_DEST_E}	},
  { "fsub.ssd",	OP_LI(0x3E3)  | PD(1) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST_E}	},
  { "fsub.ssd",	OP_REG(0x3E2) | PD(1) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST_E}	},
  { "fsub.sss",	OP_LI(0x3E3)  | PD(0) | P2(0) | P1(0),	MASK_LI  | PD(3) | P2(3) | P1(3), 0,	 {SPFI, REG_22, REG_DEST}	},
  { "fsub.sss",	OP_REG(0x3E2) | PD(0) | P2(0) | P1(0),	MASK_REG | PD(3) | P2(3) | P1(3), 0,	 {REG_0, REG_22, REG_DEST}	},

  /* Illegal instructions */

  {"illop0",	OP_SI(0x0),	MASK_SI,	0,	{0}	},
  {"illopF",	0x1FF << 13,	0x1FF << 13,	0,	{0}	},

  /* Jump and save return */

  {"jsr",	OP_SI(0x44),	MASK_SI,	0,	{OFF_SS_BR, REG_BASE, REG_DEST}	},
  {"jsr",	OP_LI(0x389),	MASK_LI,	0,	{OFF_SL_BR, REG_BASE, REG_DEST}	},
  {"jsr",	OP_REG(0x388),	MASK_REG,	0,	{REG_0, REG_BASE, REG_DEST}	},
  {"jsr.a",	OP_SI(0x45),	MASK_SI,	0,	{OFF_SS_BR, REG_BASE, REG_DEST}	},
  {"jsr.a",	OP_LI(0x38B),	MASK_LI,	0,	{OFF_SL_BR, REG_BASE, REG_DEST}	},
  {"jsr.a",	OP_REG(0x38A),	MASK_REG,	0,	{REG_0, REG_BASE, REG_DEST}	},

  /* Load Signed Data Into Register */

  {"ld",	OP_SI(0x22),		(MASK_SI  & ~M_SI(1)),		0,	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"ld",	OP_LI(0x345)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"ld",	OP_REG(0x344) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"ld.b",	OP_SI(0x20),		(MASK_SI  & ~M_SI(1)),		0,	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"ld.b",	OP_LI(0x341)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"ld.b",	OP_REG(0x340) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"ld.d",	OP_SI(0x23),		(MASK_SI  & ~M_SI(1)),		0,	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST_E}		},
  {"ld.d",	OP_LI(0x347)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST_E}	},
  {"ld.d",	OP_REG(0x346) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST_E}		},
  {"ld.h",	OP_SI(0x21),		(MASK_SI  & ~M_SI(1)),		0,	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"ld.h",	OP_LI(0x343)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"ld.h",	OP_REG(0x342) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},

  /* Load Unsigned Data Into Register */

  {"ld.ub",	OP_SI(0x28),		(MASK_SI  & ~M_SI(1)),		0,	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"ld.ub",	OP_LI(0x351)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"ld.ub",	OP_REG(0x350) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"ld.uh",	OP_SI(0x29),		(MASK_SI  & ~M_SI(1)),		0,	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"ld.uh",	OP_LI(0x353)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"ld.uh",	OP_REG(0x352) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},

  /* Leftmost one */

  {"lmo",	OP_LI(0x3F0),	MASK_LI,	0,	{REG_22, REG_DEST}	},

  /* Bitwise logical OR.  Note that "or.tt" and "or" are the same instructions. */

  {"or.ff",	OP_SI(0x1E),	MASK_SI,	0,	{SUI, REG_22, REG_DEST}		},
  {"or.ff",	OP_LI(0x33D),	MASK_LI,	0,	{LUI, REG_22, REG_DEST}		},
  {"or.ff",	OP_REG(0x33C),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},
  {"or.ft",	OP_SI(0x1D),	MASK_SI,	0,	{SUI, REG_22, REG_DEST}		},
  {"or.ft",	OP_LI(0x33B),	MASK_LI,	0,	{LUI, REG_22, REG_DEST}		},
  {"or.ft",	OP_REG(0x33A),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},
  {"or.tf",	OP_SI(0x1B),	MASK_SI,	0,	{SUI, REG_22, REG_DEST}		},
  {"or.tf",	OP_LI(0x337),	MASK_LI,	0,	{LUI, REG_22, REG_DEST}		},
  {"or.tf",	OP_REG(0x336),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},
  {"or.tt",	OP_SI(0x17),	MASK_SI,	0,	{SUI, REG_22, REG_DEST}		},
  {"or.tt",	OP_LI(0x32F),	MASK_LI,	0,	{LUI, REG_22, REG_DEST}		},
  {"or.tt",	OP_REG(0x32E),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},
  {"or",	OP_SI(0x17),	MASK_SI,	0,	{SUI, REG_22, REG_DEST}		},
  {"or",	OP_LI(0x32F),	MASK_LI,	0,	{LUI, REG_22, REG_DEST}		},
  {"or",	OP_REG(0x32E),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Read Control Register */

  {"rdcr",	OP_SI(0x4),	MASK_SI  | (0x1F << 22),	0,	{CR_SI, REG_DEST}	},
  {"rdcr",	OP_LI(0x309),	MASK_LI  | (0x1F << 22),	0,	{CR_LI, REG_DEST}	},
  {"rdcr",	OP_REG(0x308),	MASK_REG | (0x1F << 22),	0,	{REG_0, REG_DEST}	},

  /* Rightmost one */

  {"rmo",	OP_LI(0x3F2),	MASK_LI,	0,		{REG_22, REG_DEST}	},

  /* Shift Register Left - note that rotl, shl, and ins are all alternate names for one of the shift instructions.
     They appear prior to their sl equivalent so that they will be diassembled as the alternate name. */


  {"ins",	OP_REG(0x31E) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"ins",	OP_SI(0xF)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0,	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"rotl",	OP_REG(0x310) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"rotl",	OP_SI(0x8)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0,	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"shl",	OP_REG(0x31C) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"shl",	OP_SI(0xE)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.dm",	OP_REG(0x312) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.dm",	OP_SI(0x9)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.ds",	OP_REG(0x314) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.ds",	OP_SI(0xA)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.dz",	OP_REG(0x310) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.dz",	OP_SI(0x8)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.em",	OP_REG(0x318) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.em",	OP_SI(0xC)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.es",	OP_REG(0x31A) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.es",	OP_SI(0xD)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.ez",	OP_REG(0x316) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.ez",	OP_SI(0xB)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.im",	OP_REG(0x31E) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.im",	OP_SI(0xF)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sl.iz",	OP_REG(0x31C) | i(0) | n(0),	MASK_REG | i(1) | n(1),	0, 	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sl.iz",	OP_SI(0xE)    | i(0) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},

  /* Shift Register Left With Inverted Endmask */

  {"sli.dm",	OP_REG(0x312) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.dm",	OP_SI(0x9)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.ds",	OP_REG(0x314) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.ds",	OP_SI(0xA)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.dz",	OP_REG(0x310) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.dz",	OP_SI(0x8)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.em",	OP_REG(0x318) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.em",	OP_SI(0xC)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.es",	OP_REG(0x31A) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.es",	OP_SI(0xD)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.ez",	OP_REG(0x316) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.ez",	OP_SI(0xB)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.im",	OP_REG(0x31E) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.im",	OP_SI(0xF)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sli.iz",	OP_REG(0x31C) | i(1) | n(0),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sli.iz",	OP_SI(0xE)    | i(1) | n(0),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},

  /* Shift Register Right - note that exts, extu, rotr, sra, and srl are all alternate names for one of the shift instructions.
     They appear prior to their sr equivalent so that they will be diassembled as the alternate name. */

  {"exts",	OP_REG(0x314) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"exts",	OP_SI(0xA)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"extu",	OP_REG(0x310) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"extu",	OP_SI(0x8)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"rotr",	OP_REG(0x310) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"rotr",	OP_SI(0x8)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0,	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sra",	OP_REG(0x31A) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sra",	OP_SI(0xD)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"srl",	OP_REG(0x316) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"srl",	OP_SI(0xB)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.dm",	OP_REG(0x312) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.dm",	OP_SI(0x9)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.ds",	OP_REG(0x314) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.ds",	OP_SI(0xA)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.dz",	OP_REG(0x310) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.dz",	OP_SI(0x8)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.em",	OP_REG(0x318) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.em",	OP_SI(0xC)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.es",	OP_REG(0x31A) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.es",	OP_SI(0xD)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.ez",	OP_REG(0x316) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.ez",	OP_SI(0xB)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.im",	OP_REG(0x31E) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.im",	OP_SI(0xF)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sr.iz",	OP_REG(0x31C) | i(0) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sr.iz",	OP_SI(0xE)    | i(0) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},

  /* Shift Register Right With Inverted Endmask */

  {"sri.dm",	OP_REG(0x312) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.dm",	OP_SI(0x9)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.ds",	OP_REG(0x314) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.ds",	OP_SI(0xA)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.dz",	OP_REG(0x310) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.dz",	OP_SI(0x8)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.em",	OP_REG(0x318) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.em",	OP_SI(0xC)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.es",	OP_REG(0x31A) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.es",	OP_SI(0xD)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.ez",	OP_REG(0x316) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.ez",	OP_SI(0xB)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.im",	OP_REG(0x31E) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.im",	OP_SI(0xF)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},
  {"sri.iz",	OP_REG(0x31C) | i(1) | n(1),	MASK_REG | i(1) | n(1),	0,	{REG_0, ENDMASK, REG_22, REG_DEST}	},
  {"sri.iz",	OP_SI(0xE)    | i(1) | n(1),	MASK_SI  | i(1) | n(1),	0, 	{ROTATE, ENDMASK, REG_22, REG_DEST}	},

  /* Store Data into Memory */

  {"st",	OP_SI(0x32),		(MASK_SI  & ~M_SI(1)),		0, 	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"st",	OP_LI(0x365)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"st",	OP_REG(0x364) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"st.b",	OP_SI(0x30),		(MASK_SI  & ~M_SI(1)),		0, 	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"st.b",	OP_LI(0x361)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"st.b",	OP_REG(0x360) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},
  {"st.d",	OP_SI(0x33),		(MASK_SI  & ~M_SI(1)),		0, 	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST_E}		},
  {"st.d",	OP_LI(0x367)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST_E}	},
  {"st.d",	OP_REG(0x366) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST_E}		},
  {"st.h",	OP_SI(0x31),		(MASK_SI  & ~M_SI(1)),		0, 	{OFF_SS_BR, REG_BASE_M_SI, REG_DEST}		},
  {"st.h",	OP_LI(0x363)  | D(0),	(MASK_LI  & ~M_REG(1)) | D(1),	0,	{OFF_SL_BR_SCALED, REG_BASE_M_LI, REG_DEST}	},
  {"st.h",	OP_REG(0x362) | D(0),	(MASK_REG & ~M_REG(1)) | D(1),	0,	{REG_SCALED, REG_BASE_M_LI, REG_DEST}		},

  /* Signed Integer Subtract */

  {"sub",	OP_SI(0x5A),	MASK_SI,	0, 	{SSI, REG_22, REG_DEST}		},
  {"sub",	OP_LI(0x3B5),	MASK_LI,	0,	{LSI, REG_22, REG_DEST}		},
  {"sub",	OP_REG(0x3B4),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Unsigned Integer Subtract */

  {"subu",	OP_SI(0x5B),	MASK_SI,	0, 	{SSI, REG_22, REG_DEST}		},
  {"subu",	OP_LI(0x3B7),	MASK_LI,	0,	{LSI, REG_22, REG_DEST}		},
  {"subu",	OP_REG(0x3B6),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Write Control Register
     Is a special form of the "swcr" instruction so comes before it in the table. */

  {"wrcr",	OP_SI(0x5),	MASK_SI | (0x1F << 27),		0,	{CR_SI, REG_22}	},
  {"wrcr",	OP_LI(0x30B),	MASK_LI | (0x1F << 27),		0,	{CR_LI, REG_22}	},
  {"wrcr",	OP_REG(0x30A),	MASK_REG | (0x1F << 27),	0,	{REG_0, REG_22}	},

  /* Swap Control Register */

  {"swcr",	OP_SI(0x5),	MASK_SI,	0,	{CR_SI, REG_22, REG_DEST}	},
  {"swcr",	OP_LI(0x30B),	MASK_LI,	0,	{CR_LI, REG_22, REG_DEST}	},
  {"swcr",	OP_REG(0x30A),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST}	},

  /* Trap */

  {"trap",	OP_SI(0x1)    | E(0),	MASK_SI  | E(1),	0,	{SUI}	},
  {"trap",	OP_LI(0x303)  | E(0),	MASK_LI  | E(1),	0,	{LUI}	},
  {"trap",	OP_REG(0x302) | E(0),	MASK_REG | E(1),	0,	{REG_0}	},

  /* Vector Floating-Point Add */

  {"vadd.dd",	OP_REG(0x3C0) | P2(1) | P1(1),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{REG_0_E, REG_22_E, REG_22_E}	},
  {"vadd.sd",	OP_LI(0x3C1)  | P2(1) | P1(0),	MASK_LI  | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{SPFI, REG_22_E, REG_22_E}	},
  {"vadd.sd",	OP_REG(0x3C0) | P2(1) | P1(0),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{REG_0, REG_22_E, REG_22_E}	},
  {"vadd.ss",	OP_LI(0x3C1)  | P2(0) | P1(0),	MASK_LI  | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{SPFI, REG_22, REG_22}	},
  {"vadd.ss",	OP_REG(0x3C0) | P2(0) | P1(0),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{REG_0, REG_22, REG_22}	},

  /* Vector Floating-Point Multiply and Add to Accumulator FIXME! This is not yet fully implemented.
   From the documentation there appears to be no way to tell the difference between the opcodes for
   instructions that have register destinations and instructions that have accumulator destinations.
   Further investigation is necessary.  Since this isn't critical to getting a TIC80 toolchain up
   and running, it is defered until later. */

  /* Vector Floating-Point Multiply
   Note: If r0 is in the destination reg, then this is a "vector nop" instruction. */

  {"vmpy.dd",	OP_REG(0x3C4) | P2(1) | P1(1),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR | TIC80_NO_R0_DEST, {REG_0_E, REG_22_E, REG_22_E} },
  {"vmpy.sd",	OP_LI(0x3C5)  | P2(1) | P1(0),	MASK_LI  | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR | TIC80_NO_R0_DEST, {SPFI, REG_22_E, REG_22_E}	},
  {"vmpy.sd",	OP_REG(0x3C4) | P2(1) | P1(0),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR | TIC80_NO_R0_DEST, {REG_0, REG_22_E, REG_22_E} },
  {"vmpy.ss",	OP_LI(0x3C5)  | P2(0) | P1(0),	MASK_LI  | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR | TIC80_NO_R0_DEST, {SPFI, REG_22, REG_22}	},
  {"vmpy.ss",	OP_REG(0x3C4) | P2(0) | P1(0),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR | TIC80_NO_R0_DEST, {REG_0, REG_22, REG_22} },

  /* Vector Floating-Point Multiply and Subtract from Accumulator
     FIXME: See note above for vmac instruction */

  /* Vector Floating-Point Subtract Accumulator From Source
     FIXME: See note above for vmac instruction */

  /* Vector Round With Floating-Point Input
     FIXME: See note above for vmac instruction */

  /* Vector Round with Integer Input */

  {"vrnd.id",	OP_LI (0x3CB)  | P2(1) | P1(0),	MASK_LI  | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {LSI, REG_22_E}},
  {"vrnd.id",	OP_REG (0x3CA) | P2(1) | P1(0),	MASK_REG | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {REG_0, REG_22_E}},
  {"vrnd.is",	OP_LI (0x3CB)  | P2(0) | P1(0),	MASK_LI  | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {LSI, REG_22}},
  {"vrnd.is",	OP_REG (0x3CA) | P2(0) | P1(0),	MASK_REG | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {REG_0, REG_22}},
  {"vrnd.ud",	OP_LI (0x3CB)  | P2(1) | P1(1),	MASK_LI  | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {LUI, REG_22_E}},
  {"vrnd.ud",	OP_REG (0x3CA) | P2(1) | P1(1),	MASK_REG | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {REG_0, REG_22_E}},
  {"vrnd.us",	OP_LI (0x3CB)  | P2(0) | P1(1),	MASK_LI  | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {LUI, REG_22}},
  {"vrnd.us",	OP_REG (0x3CA) | P2(0) | P1(1),	MASK_REG | V_a0(1) | V_Z(1) | P2(1) | P1(1),	TIC80_VECTOR, {REG_0, REG_22}},

  /* Vector Floating-Point Subtract */

  {"vsub.dd",	OP_REG(0x3C2) | P2(1) | P1(1),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{REG_0_E, REG_22_E, REG_22_E}	},
  {"vsub.sd",	OP_LI(0x3C3)  | P2(1) | P1(0),	MASK_LI  | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{SPFI, REG_22_E, REG_22_E}	},
  {"vsub.sd",	OP_REG(0x3C2) | P2(1) | P1(0),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{REG_0, REG_22_E, REG_22_E}	},
  {"vsub.ss",	OP_LI(0x3C3)  | P2(0) | P1(0),	MASK_LI  | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{SPFI, REG_22, REG_22}	},
  {"vsub.ss",	OP_REG(0x3C2) | P2(0) | P1(0),	MASK_REG | V_a1(1) | P2(1) | P1(1),	TIC80_VECTOR,	{REG_0, REG_22, REG_22}	},

  /* Vector Load Data Into Register - Note that the vector load/store instructions come after the other
   vector instructions so that the disassembler will always print the load/store instruction second for
   vector instructions that have two instructions in the same opcode. */

  {"vld0.d",	OP_V(0x1E) | V_m(1) | V_S(1) | V_p(0),	MASK_V | V_m(1) | V_S(1) | V_p(1),	TIC80_VECTOR, {REG_DEST_E} },
  {"vld0.s",	OP_V(0x1E) | V_m(1) | V_S(0) | V_p(0),	MASK_V | V_m(1) | V_S(1) | V_p(1),	TIC80_VECTOR, {REG_DEST} },
  {"vld1.d",	OP_V(0x1E) | V_m(1) | V_S(1) | V_p(1),	MASK_V | V_m(1) | V_S(1) | V_p(1),	TIC80_VECTOR, {REG_DEST_E} },
  {"vld1.s",	OP_V(0x1E) | V_m(1) | V_S(0) | V_p(1),	MASK_V | V_m(1) | V_S(1) | V_p(1),	TIC80_VECTOR, {REG_DEST} },

  /* Vector Store Data Into Memory - Note that the vector load/store instructions come after the other
   vector instructions so that the disassembler will always print the load/store instruction second for
   vector instructions that have two instructions in the same opcode. */

  {"vst.d",	OP_V(0x1E) | V_m(0) | V_S(1) | V_p(1),	MASK_V | V_m(1) | V_S(1) | V_p(1),	TIC80_VECTOR, {REG_DEST_E} },
  {"vst.s",	OP_V(0x1E) | V_m(0) | V_S(0) | V_p(1),	MASK_V | V_m(1) | V_S(1) | V_p(1),	TIC80_VECTOR, {REG_DEST} },

  {"xnor",	OP_SI(0x19),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST} },
  {"xnor",	OP_LI(0x333),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST} },
  {"xnor",	OP_REG(0x332),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST} },

  {"xor",	OP_SI(0x16),	MASK_SI,	0,	{SUBF, REG_22, REG_DEST} },
  {"xor",	OP_LI(0x32D),	MASK_LI,	0,	{LUBF, REG_22, REG_DEST} },
  {"xor",	OP_REG(0x32C),	MASK_REG,	0,	{REG_0, REG_22, REG_DEST} },

};

const int tic80_num_opcodes = sizeof (tic80_opcodes) / sizeof (tic80_opcodes[0]);
