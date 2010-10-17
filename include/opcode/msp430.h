/* Opcode table for the TI MSP430 microcontrollers

   Copyright 2002 Free Software Foundation, Inc.
   Contributed by Dmitry Diky <diwil@mail.ru>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef __MSP430_H_
#define __MSP430_H_

struct msp430_operand_s
{
  int ol;	/* Operand length words.  */
  int am;	/* Addr mode.  */
  int reg;	/* Register.  */
  int mode;	/* Pperand mode.  */
#define OP_REG		0
#define OP_EXP		1
#ifndef DASM_SECTION
  expressionS	exp;
#endif
};

#define BYTE_OPERATION  (1 << 6)  /* Byte operation flag for all instructions.  */

struct  msp430_opcode_s
{
  char *name;
  int fmt;
  int insn_opnumb;
  int bin_opcode;
  int bin_mask;
};

#define MSP_INSN(name, size, numb, bin, mask) { #name, size, numb, bin, mask }

static struct msp430_opcode_s msp430_opcodes[] = 
{
  MSP_INSN (and,   1, 2, 0xf000, 0xf000),
  MSP_INSN (inv,   0, 1, 0xe330, 0xfff0),
  MSP_INSN (xor,   1, 2, 0xe000, 0xf000),
  MSP_INSN (setz,  0, 0, 0xd322, 0xffff),
  MSP_INSN (setc,  0, 0, 0xd312, 0xffff),
  MSP_INSN (eint,  0, 0, 0xd232, 0xffff),
  MSP_INSN (setn,  0, 0, 0xd222, 0xffff),
  MSP_INSN (bis,   1, 2, 0xd000, 0xf000),
  MSP_INSN (clrz,  0, 0, 0xc322, 0xffff),
  MSP_INSN (clrc,  0, 0, 0xc312, 0xffff),
  MSP_INSN (dint,  0, 0, 0xc232, 0xffff),
  MSP_INSN (clrn,  0, 0, 0xc222, 0xffff),
  MSP_INSN (bic,   1, 2, 0xc000, 0xf000),
  MSP_INSN (bit,   1, 2, 0xb000, 0xf000),
  MSP_INSN (dadc,  0, 1, 0xa300, 0xff30),
  MSP_INSN (dadd,  1, 2, 0xa000, 0xf000),
  MSP_INSN (tst,   0, 1, 0x9300, 0xff30),
  MSP_INSN (cmp,   1, 2, 0x9000, 0xf000),
  MSP_INSN (decd,  0, 1, 0x8320, 0xff30),
  MSP_INSN (dec,   0, 1, 0x8310, 0xff30),
  MSP_INSN (sub,   1, 2, 0x8000, 0xf000),
  MSP_INSN (sbc,   0, 1, 0x7300, 0xff30),
  MSP_INSN (subc,  1, 2, 0x7000, 0xf000),
  MSP_INSN (adc,   0, 1, 0x6300, 0xff30),
  MSP_INSN (rlc,   0, 2, 0x6000, 0xf000),
  MSP_INSN (addc,  1, 2, 0x6000, 0xf000),
  MSP_INSN (incd,  0, 1, 0x5320, 0xff30),
  MSP_INSN (inc,   0, 1, 0x5310, 0xff30),
  MSP_INSN (rla,   0, 2, 0x5000, 0xf000),
  MSP_INSN (add,   1, 2, 0x5000, 0xf000),
  MSP_INSN (nop,   0, 0, 0x4303, 0xffff),
  MSP_INSN (clr,   0, 1, 0x4300, 0xff30),
  MSP_INSN (ret,   0, 0, 0x4130, 0xff30),
  MSP_INSN (pop,   0, 1, 0x4130, 0xff30),
  MSP_INSN (br,    0, 3, 0x4000, 0xf000),
  MSP_INSN (mov,   1, 2, 0x4000, 0xf000),
  MSP_INSN (jmp,   3, 1, 0x3c00, 0xfc00),
  MSP_INSN (jl,    3, 1, 0x3800, 0xfc00),
  MSP_INSN (jge,   3, 1, 0x3400, 0xfc00),
  MSP_INSN (jn,    3, 1, 0x3000, 0xfc00),
  MSP_INSN (jc,    3, 1, 0x2c00, 0xfc00),
  MSP_INSN (jhs,   3, 1, 0x2c00, 0xfc00),
  MSP_INSN (jnc,   3, 1, 0x2800, 0xfc00),
  MSP_INSN (jlo,   3, 1, 0x2800, 0xfc00),
  MSP_INSN (jz,    3, 1, 0x2400, 0xfc00),
  MSP_INSN (jeq,   3, 1, 0x2400, 0xfc00),
  MSP_INSN (jnz,   3, 1, 0x2000, 0xfc00),
  MSP_INSN (jne,   3, 1, 0x2000, 0xfc00),
  MSP_INSN (reti,  2, 0, 0x1300, 0xffc0),
  MSP_INSN (call,  2, 1, 0x1280, 0xffc0),
  MSP_INSN (push,  2, 1, 0x1200, 0xff80),
  MSP_INSN (sxt,   2, 1, 0x1180, 0xffc0),
  MSP_INSN (rra,   2, 1, 0x1100, 0xff80),
  MSP_INSN (swpb,  2, 1, 0x1080, 0xffc0),
  MSP_INSN (rrc,   2, 1, 0x1000, 0xff80),

  /* End of instruction set.  */
  { NULL, 0, 0, 0, 0 }
};

#endif
