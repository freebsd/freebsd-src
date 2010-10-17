/* Assemble Matsushita MN10300 instructions.
   Copyright 1996, 1997, 1998, 1999, 2000, 2004 Free Software Foundation, Inc.

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

/* This file is formatted at > 80 columns.  Attempting to read it on a
   screeen with less than 80 columns will be difficult.  */
#include "sysdep.h"
#include "opcode/mn10300.h"


const struct mn10300_operand mn10300_operands[] = {
#define UNUSED	0
  {0, 0, 0}, 

/* dn register in the first register operand position.  */
#define DN0      (UNUSED+1)
  {2, 0, MN10300_OPERAND_DREG},

/* dn register in the second register operand position.  */
#define DN1      (DN0+1)
  {2, 2, MN10300_OPERAND_DREG},

/* dn register in the third register operand position.  */
#define DN2      (DN1+1)
  {2, 4, MN10300_OPERAND_DREG},

/* dm register in the first register operand position.  */
#define DM0      (DN2+1)
  {2, 0, MN10300_OPERAND_DREG},

/* dm register in the second register operand position.  */
#define DM1      (DM0+1)
  {2, 2, MN10300_OPERAND_DREG},

/* dm register in the third register operand position.  */
#define DM2      (DM1+1)
  {2, 4, MN10300_OPERAND_DREG},

/* an register in the first register operand position.  */
#define AN0      (DM2+1)
  {2, 0, MN10300_OPERAND_AREG},

/* an register in the second register operand position.  */
#define AN1      (AN0+1)
  {2, 2, MN10300_OPERAND_AREG},

/* an register in the third register operand position.  */
#define AN2      (AN1+1)
  {2, 4, MN10300_OPERAND_AREG},

/* am register in the first register operand position.  */
#define AM0      (AN2+1)
  {2, 0, MN10300_OPERAND_AREG},

/* am register in the second register operand position.  */
#define AM1      (AM0+1)
  {2, 2, MN10300_OPERAND_AREG},

/* am register in the third register operand position.  */
#define AM2      (AM1+1)
  {2, 4, MN10300_OPERAND_AREG},

/* 8 bit unsigned immediate which may promote to a 16bit
   unsigned immediate.  */
#define IMM8    (AM2+1)
  {8, 0, MN10300_OPERAND_PROMOTE},

/* 16 bit unsigned immediate which may promote to a 32bit
   unsigned immediate.  */
#define IMM16    (IMM8+1)
  {16, 0, MN10300_OPERAND_PROMOTE},

/* 16 bit pc-relative immediate which may promote to a 16bit
   pc-relative immediate.  */
#define IMM16_PCREL    (IMM16+1)
  {16, 0, MN10300_OPERAND_PCREL | MN10300_OPERAND_RELAX | MN10300_OPERAND_SIGNED},

/* 16bit unsigned displacement in a memory operation which
   may promote to a 32bit displacement.  */
#define IMM16_MEM    (IMM16_PCREL+1)
  {16, 0, MN10300_OPERAND_PROMOTE | MN10300_OPERAND_MEMADDR},

/* 32bit immediate, high 16 bits in the main instruction
   word, 16bits in the extension word. 

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32    (IMM16_MEM+1)
  {16, 0, MN10300_OPERAND_SPLIT},

/* 32bit pc-relative offset.  */
#define IMM32_PCREL    (IMM32+1)
  {16, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_PCREL},

/* 32bit memory offset.  */
#define IMM32_MEM    (IMM32_PCREL+1)
  {16, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_MEMADDR},

/* 32bit immediate, high 16 bits in the main instruction
   word, 16bits in the extension word, low 16bits are left
   shifted 8 places. 

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_LOWSHIFT8    (IMM32_MEM+1)
  {16, 8, MN10300_OPERAND_SPLIT | MN10300_OPERAND_MEMADDR},

/* 32bit immediate, high 24 bits in the main instruction
   word, 8 in the extension word.

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_HIGH24    (IMM32_LOWSHIFT8+1)
  {24, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_PCREL},

/* 32bit immediate, high 24 bits in the main instruction
   word, 8 in the extension word, low 8 bits are left
   shifted 16 places. 

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_HIGH24_LOWSHIFT16    (IMM32_HIGH24+1)
  {24, 16, MN10300_OPERAND_SPLIT | MN10300_OPERAND_PCREL},

/* Stack pointer.  */
#define SP    (IMM32_HIGH24_LOWSHIFT16+1)
  {8, 0, MN10300_OPERAND_SP},

/* Processor status word.  */
#define PSW    (SP+1)
  {0, 0, MN10300_OPERAND_PSW},

/* MDR register.  */
#define MDR    (PSW+1)
  {0, 0, MN10300_OPERAND_MDR},

/* Index register.  */
#define DI (MDR+1)
  {2, 2, MN10300_OPERAND_DREG},

/* 8 bit signed displacement, may promote to 16bit signed displacement.  */
#define SD8    (DI+1)
  {8, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 16 bit signed displacement, may promote to 32bit displacement.  */
#define SD16    (SD8+1)
  {16, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 8 bit signed displacement that can not promote.  */
#define SD8N    (SD16+1)
  {8, 0, MN10300_OPERAND_SIGNED},

/* 8 bit pc-relative displacement.  */
#define SD8N_PCREL    (SD8N+1)
  {8, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PCREL | MN10300_OPERAND_RELAX},

/* 8 bit signed displacement shifted left 8 bits in the instruction.  */
#define SD8N_SHIFT8    (SD8N_PCREL+1)
  {8, 8, MN10300_OPERAND_SIGNED},

/* 8 bit signed immediate which may promote to 16bit signed immediate.  */
#define SIMM8    (SD8N_SHIFT8+1)
  {8, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 16 bit signed immediate which may promote to 32bit  immediate.  */
#define SIMM16    (SIMM8+1)
  {16, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* Either an open paren or close paren.  */
#define PAREN	(SIMM16+1)
  {0, 0, MN10300_OPERAND_PAREN}, 

/* dn register that appears in the first and second register positions.  */
#define DN01     (PAREN+1)
  {2, 0, MN10300_OPERAND_DREG | MN10300_OPERAND_REPEATED},

/* an register that appears in the first and second register positions.  */
#define AN01     (DN01+1)
  {2, 0, MN10300_OPERAND_AREG | MN10300_OPERAND_REPEATED},

/* 16bit pc-relative displacement which may promote to 32bit pc-relative
   displacement.  */
#define D16_SHIFT (AN01+1)
  {16, 8, MN10300_OPERAND_PCREL | MN10300_OPERAND_RELAX | MN10300_OPERAND_SIGNED},

/* 8 bit immediate found in the extension word.  */
#define IMM8E    (D16_SHIFT+1)
  {8, 0, MN10300_OPERAND_EXTENDED},

/* Register list found in the extension word shifted 8 bits left.  */
#define REGSE_SHIFT8    (IMM8E+1)
  {8, 8, MN10300_OPERAND_EXTENDED | MN10300_OPERAND_REG_LIST},

/* Register list shifted 8 bits left.  */
#define REGS_SHIFT8 (REGSE_SHIFT8 + 1)
  {8, 8, MN10300_OPERAND_REG_LIST},

/* Reigster list.  */
#define REGS    (REGS_SHIFT8+1)
  {8, 0, MN10300_OPERAND_REG_LIST},

/* UStack pointer.  */
#define USP    (REGS+1)
  {0, 0, MN10300_OPERAND_USP},

/* SStack pointer.  */
#define SSP    (USP+1)
  {0, 0, MN10300_OPERAND_SSP},

/* MStack pointer.  */
#define MSP    (SSP+1)
  {0, 0, MN10300_OPERAND_MSP},

/* PC .  */
#define PC    (MSP+1)
  {0, 0, MN10300_OPERAND_PC},

/* 4 bit immediate for syscall.  */
#define IMM4    (PC+1)
  {4, 0, 0},

/* Processor status word.  */
#define EPSW    (IMM4+1)
  {0, 0, MN10300_OPERAND_EPSW},

/* rn register in the first register operand position.  */
#define RN0      (EPSW+1)
  {4, 0, MN10300_OPERAND_RREG},

/* rn register in the fourth register operand position.  */
#define RN2      (RN0+1)
  {4, 4, MN10300_OPERAND_RREG},

/* rm register in the first register operand position.  */
#define RM0      (RN2+1)
  {4, 0, MN10300_OPERAND_RREG},

/* rm register in the second register operand position.  */
#define RM1      (RM0+1)
  {4, 2, MN10300_OPERAND_RREG},

/* rm register in the third register operand position.  */
#define RM2      (RM1+1)
  {4, 4, MN10300_OPERAND_RREG},

#define RN02      (RM2+1)
  {4, 0, MN10300_OPERAND_RREG | MN10300_OPERAND_REPEATED},

#define XRN0      (RN02+1)
  {4, 0, MN10300_OPERAND_XRREG},

#define XRM2      (XRN0+1)
  {4, 4, MN10300_OPERAND_XRREG},

/* + for autoincrement */
#define PLUS	(XRM2+1)
  {0, 0, MN10300_OPERAND_PLUS}, 

#define XRN02      (PLUS+1)
  {4, 0, MN10300_OPERAND_XRREG | MN10300_OPERAND_REPEATED},

/* Ick */
#define RD0      (XRN02+1)
  {4, -8, MN10300_OPERAND_RREG},

#define RD2      (RD0+1)
  {4, -4, MN10300_OPERAND_RREG},

/* 8 unsigned displacement in a memory operation which
   may promote to a 32bit displacement.  */
#define IMM8_MEM    (RD2+1)
  {8, 0, MN10300_OPERAND_PROMOTE | MN10300_OPERAND_MEMADDR},

/* Index register.  */
#define RI (IMM8_MEM+1)
  {4, 4, MN10300_OPERAND_RREG},

/* 24 bit signed displacement, may promote to 32bit displacement.  */
#define SD24    (RI+1)
  {8, 0, MN10300_OPERAND_24BIT | MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 24 bit unsigned immediate which may promote to a 32bit
   unsigned immediate.  */
#define IMM24    (SD24+1)
  {8, 0, MN10300_OPERAND_24BIT | MN10300_OPERAND_PROMOTE},

/* 24 bit signed immediate which may promote to a 32bit
   signed immediate.  */
#define SIMM24    (IMM24+1)
  {8, 0, MN10300_OPERAND_24BIT | MN10300_OPERAND_PROMOTE | MN10300_OPERAND_SIGNED},

/* 24bit unsigned displacement in a memory operation which
   may promote to a 32bit displacement.  */
#define IMM24_MEM    (SIMM24+1)
  {8, 0, MN10300_OPERAND_24BIT | MN10300_OPERAND_PROMOTE | MN10300_OPERAND_MEMADDR},
/* 32bit immediate, high 8 bits in the main instruction
   word, 24 in the extension word.

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_HIGH8    (IMM24_MEM+1)
  {8, 0, MN10300_OPERAND_SPLIT},

/* Similarly, but a memory address.  */
#define IMM32_HIGH8_MEM  (IMM32_HIGH8+1)
  {8, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_MEMADDR},

/* rm register in the seventh register operand position.  */
#define RM6      (IMM32_HIGH8_MEM+1)
  {4, 12, MN10300_OPERAND_RREG},

/* rm register in the fifth register operand position.  */
#define RN4      (RM6+1)
  {4, 8, MN10300_OPERAND_RREG},

/* 4 bit immediate for dsp instructions.  */
#define IMM4_2    (RN4+1)
  {4, 4, 0},

/* 4 bit immediate for dsp instructions.  */
#define SIMM4_2    (IMM4_2+1)
  {4, 4, MN10300_OPERAND_SIGNED},

/* 4 bit immediate for dsp instructions.  */
#define SIMM4_6    (SIMM4_2+1)
  {4, 12, MN10300_OPERAND_SIGNED},

#define FPCR      (SIMM4_6+1)
  {0, 0, MN10300_OPERAND_FPCR},

/* We call f[sd]m registers those whose most significant bit is stored
 * within the opcode half-word, i.e., in a bit on the left of the 4
 * least significant bits, and f[sd]n registers those whose most
 * significant bit is stored at the end of the full word, after the 4
 * least significant bits.  They're not numbered after their position
 * in the mnemonic asm instruction, but after their position in the
 * opcode word, i.e., depending on the amount of shift they need.
 *
 * The additional bit is shifted as follows: for `n' registers, it
 * will be shifted by (|shift|/4); for `m' registers, it will be
 * shifted by (8+(8&shift)+(shift&4)/4); for accumulator, whose
 * specifications are only 3-bits long, the two least-significant bits
 * are shifted by 16, and the most-significant bit is shifted by -2
 * (i.e., it's stored in the least significant bit of the full
 * word).  */

/* fsm register in the first register operand position.  */
#define FSM0      (FPCR+1)
  {5, 0, MN10300_OPERAND_FSREG },

/* fsm register in the second register operand position.  */
#define FSM1      (FSM0+1)
  {5, 4, MN10300_OPERAND_FSREG },

/* fsm register in the third register operand position.  */
#define FSM2      (FSM1+1)
  {5, 8, MN10300_OPERAND_FSREG },

/* fsm register in the fourth register operand position.  */
#define FSM3      (FSM2+1)
  {5, 12, MN10300_OPERAND_FSREG },

/* fsn register in the first register operand position.  */
#define FSN1      (FSM3+1)
  {5, -4, MN10300_OPERAND_FSREG },

/* fsn register in the second register operand position.  */
#define FSN2      (FSN1+1)
  {5, -8, MN10300_OPERAND_FSREG },

/* fsm register in the third register operand position.  */
#define FSN3      (FSN2+1)
  {5, -12, MN10300_OPERAND_FSREG },

/* fsm accumulator, in the fourth register operand position.  */
#define FSACC     (FSN3+1)
  {3, -16, MN10300_OPERAND_FSREG },

/* fdm register in the first register operand position.  */
#define FDM0      (FSACC+1)
  {5, 0, MN10300_OPERAND_FDREG },

/* fdm register in the second register operand position.  */
#define FDM1      (FDM0+1)
  {5, 4, MN10300_OPERAND_FDREG },

/* fdm register in the third register operand position.  */
#define FDM2      (FDM1+1)
  {5, 8, MN10300_OPERAND_FDREG },

/* fdm register in the fourth register operand position.  */
#define FDM3      (FDM2+1)
  {5, 12, MN10300_OPERAND_FDREG },

/* fdn register in the first register operand position.  */
#define FDN1      (FDM3+1)
  {5, -4, MN10300_OPERAND_FDREG },

/* fdn register in the second register operand position.  */
#define FDN2      (FDN1+1)
  {5, -8, MN10300_OPERAND_FDREG },

/* fdn register in the third register operand position.  */
#define FDN3      (FDN2+1)
  {5, -12, MN10300_OPERAND_FDREG },

} ; 

#define MEM(ADDR) PAREN, ADDR, PAREN 
#define MEMINC(ADDR) PAREN, ADDR, PLUS, PAREN 
#define MEMINC2(ADDR,INC) PAREN, ADDR, PLUS, INC, PAREN 
#define MEM2(ADDR1,ADDR2) PAREN, ADDR1, ADDR2, PAREN 

/* The opcode table.

   The format of the opcode table is:

   NAME		OPCODE		MASK	MATCH_MASK, FORMAT, PROCESSOR	{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct mn10300_opcode mn10300_opcodes[] = {
{ "mov",	0x8000,	     0xf000,	  0,    FMT_S1, 0,	{SIMM8, DN01}},
{ "mov",	0x80,	     0xf0,	  0x3,  FMT_S0, 0,	{DM1, DN0}},
{ "mov",	0xf1e0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, AN0}},
{ "mov",	0xf1d0,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, DN0}},
{ "mov",	0x9000,	     0xf000,	  0,    FMT_S1, 0,	{IMM8, AN01}},
{ "mov",	0x90,	     0xf0,	  0x3,  FMT_S0, 0,	{AM1, AN0}},
{ "mov",	0x3c,	     0xfc,	  0,    FMT_S0, 0,	{SP, AN0}},
{ "mov",	0xf2f0,	     0xfff3,	  0,    FMT_D0, 0,	{AM1, SP}},
{ "mov",	0xf2e4,	     0xfffc,	  0,    FMT_D0, 0,	{PSW, DN0}},
{ "mov",	0xf2f3,	     0xfff3,	  0,    FMT_D0, 0,	{DM1, PSW}},
{ "mov",	0xf2e0,	     0xfffc,	  0,    FMT_D0, 0,	{MDR, DN0}},
{ "mov",	0xf2f2,	     0xfff3,	  0,    FMT_D0, 0,	{DM1, MDR}},
{ "mov",	0x70,	     0xf0,	  0,    FMT_S0, 0,	{MEM(AM0), DN1}},
{ "mov",	0x5800,	     0xfcff,	  0,    FMT_S1, 0,	{MEM(SP), DN0}},
{ "mov",	0x300000,    0xfc0000,    0,    FMT_S2, 0,	{MEM(IMM16_MEM), DN0}},
{ "mov",	0xf000,	     0xfff0,	  0,    FMT_D0, 0,	{MEM(AM0), AN1}},
{ "mov",	0x5c00,	     0xfcff,	  0,    FMT_S1, 0,	{MEM(SP), AN0}},
{ "mov",	0xfaa00000,  0xfffc0000,  0,    FMT_D2, 0,	{MEM(IMM16_MEM), AN0}},
{ "mov",	0x60,	     0xf0,	  0,    FMT_S0, 0,	{DM1, MEM(AN0)}},
{ "mov",	0x4200,	     0xf3ff,	  0,    FMT_S1, 0,	{DM1, MEM(SP)}},
{ "mov",	0x010000,    0xf30000,    0,    FMT_S2, 0,	{DM1, MEM(IMM16_MEM)}},
{ "mov",	0xf010,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, MEM(AN0)}},
{ "mov",	0x4300,	     0xf3ff,	  0,    FMT_S1, 0,	{AM1, MEM(SP)}},
{ "mov",	0xfa800000,  0xfff30000,  0,    FMT_D2, 0,	{AM1, MEM(IMM16_MEM)}},
{ "mov",	0x5c00,	     0xfc00,	  0,    FMT_S1, 0,	{MEM2(IMM8, SP), AN0}},
{ "mov",	0xf80000,    0xfff000,    0,    FMT_D1, 0,	{MEM2(SD8, AM0), DN1}},
{ "mov",	0xfa000000,  0xfff00000,  0,    FMT_D2, 0,	{MEM2(SD16, AM0), DN1}},
{ "mov",	0x5800,	     0xfc00,	  0,    FMT_S1, 0,	{MEM2(IMM8, SP), DN0}},
{ "mov",	0xfab40000,  0xfffc0000,  0,    FMT_D2, 0,	{MEM2(IMM16, SP), DN0}},
{ "mov",	0xf300,	     0xffc0,	  0,    FMT_D0, 0,	{MEM2(DI, AM0), DN2}},
{ "mov",	0xf82000,    0xfff000,    0,    FMT_D1, 0,	{MEM2(SD8,AM0), AN1}},
{ "mov",	0xfa200000,  0xfff00000,  0,    FMT_D2, 0,	{MEM2(SD16, AM0), AN1}},
{ "mov",	0xfab00000,  0xfffc0000,  0,    FMT_D2, 0,	{MEM2(IMM16, SP), AN0}},
{ "mov",	0xf380,	     0xffc0,	  0,    FMT_D0, 0,	{MEM2(DI, AM0), AN2}},
{ "mov",	0x4300,	     0xf300,	  0,    FMT_S1, 0,	{AM1, MEM2(IMM8, SP)}},
{ "mov",	0xf81000,    0xfff000,    0,    FMT_D1, 0,	{DM1, MEM2(SD8, AN0)}},
{ "mov",	0xfa100000,  0xfff00000,  0,    FMT_D2, 0,	{DM1, MEM2(SD16, AN0)}},
{ "mov",	0x4200,	     0xf300,	  0,    FMT_S1, 0,	{DM1, MEM2(IMM8, SP)}},
{ "mov",	0xfa910000,  0xfff30000,  0,    FMT_D2, 0,	{DM1, MEM2(IMM16, SP)}},
{ "mov",	0xf340,	     0xffc0,	  0,    FMT_D0, 0,	{DM2, MEM2(DI, AN0)}},
{ "mov",	0xf83000,    0xfff000,    0,    FMT_D1, 0,	{AM1, MEM2(SD8, AN0)}},
{ "mov",	0xfa300000,  0xfff00000,  0,    FMT_D2, 0,	{AM1, MEM2(SD16, AN0)}},
{ "mov",	0xfa900000,  0xfff30000,  0,    FMT_D2, 0,	{AM1, MEM2(IMM16, SP)}},
{ "mov",	0xf3c0,	     0xffc0,	  0,    FMT_D0, 0,	{AM2, MEM2(DI, AN0)}},

{ "mov",	0xf020,	     0xfffc,	  0,    FMT_D0, AM33,	{USP, AN0}},
{ "mov",	0xf024,	     0xfffc,	  0,    FMT_D0, AM33,	{SSP, AN0}},
{ "mov",	0xf028,	     0xfffc,	  0,    FMT_D0, AM33,	{MSP, AN0}},
{ "mov",	0xf02c,	     0xfffc,	  0,    FMT_D0, AM33,	{PC, AN0}},
{ "mov",	0xf030,	     0xfff3,	  0,    FMT_D0, AM33,	{AN1, USP}},
{ "mov",	0xf031,	     0xfff3,	  0,    FMT_D0, AM33,	{AN1, SSP}},
{ "mov",	0xf032,	     0xfff3,	  0,    FMT_D0, AM33,	{AN1, MSP}},
{ "mov",	0xf2ec,	     0xfffc,	  0,    FMT_D0, AM33,	{EPSW, DN0}},
{ "mov",	0xf2f1,	     0xfff3,	  0,    FMT_D0, AM33,	{DM1, EPSW}},
{ "mov",	0xf500,	     0xffc0,	  0,    FMT_D0, AM33,	{AM2, RN0}},
{ "mov",	0xf540,	     0xffc0,	  0,    FMT_D0, AM33,	{DM2, RN0}},
{ "mov",	0xf580,	     0xffc0,	  0,    FMT_D0, AM33,	{RM1, AN0}},
{ "mov",	0xf5c0,	     0xffc0,	  0,    FMT_D0, AM33,	{RM1, DN0}},
{ "mov",	0xf90800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "mov",	0xf9e800,    0xffff00,    0,    FMT_D6, AM33,	{XRM2, RN0}},
{ "mov",	0xf9f800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, XRN0}},
{ "mov",	0xf90a00,    0xffff00,    0,    FMT_D6, AM33,	{MEM(RM0), RN2}},
{ "mov",	0xf98a00,    0xffff0f,    0,    FMT_D6, AM33,	{MEM(SP), RN2}},
{ "mov",	0xf96a00,    0xffff00,    0x12, FMT_D6, AM33,	{MEMINC(RM0), RN2}},
{ "mov",	0xfb0e0000,  0xffff0f00,  0,    FMT_D7, AM33,	{MEM(IMM8_MEM), RN2}},
{ "mov",	0xfd0e0000,  0xffff0f00,  0,    FMT_D8, AM33,	{MEM(IMM24_MEM), RN2}},
{ "mov",	0xf91a00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, MEM(RN0)}},
{ "mov",	0xf99a00,    0xffff0f,    0,    FMT_D6, AM33,	{RM2, MEM(SP)}},
{ "mov",	0xf97a00,    0xffff00,    0,	FMT_D6, AM33,	{RM2, MEMINC(RN0)}},
{ "mov",	0xfb1e0000,  0xffff0f00,  0,    FMT_D7, AM33,	{RM2, MEM(IMM8_MEM)}},
{ "mov",	0xfd1e0000,  0xffff0f00,  0,    FMT_D8, AM33,	{RM2, MEM(IMM24_MEM)}},
{ "mov",	0xfb0a0000,  0xffff0000,  0,    FMT_D7, AM33,	{MEM2(SD8, RM0), RN2}},
{ "mov",	0xfd0a0000,  0xffff0000,  0,    FMT_D8, AM33,	{MEM2(SD24, RM0), RN2}},
{ "mov",	0xfb8e0000,  0xffff000f,  0,    FMT_D7, AM33,	{MEM2(RI, RM0), RD2}},
{ "mov",	0xfb1a0000,  0xffff0000,  0,    FMT_D7, AM33,	{RM2, MEM2(SD8, RN0)}},
{ "mov",	0xfd1a0000,  0xffff0000,  0,    FMT_D8, AM33,	{RM2, MEM2(SD24, RN0)}},
{ "mov",	0xfb8a0000,  0xffff0f00,  0,    FMT_D7, AM33,	{MEM2(IMM8, SP), RN2}},
{ "mov",	0xfd8a0000,  0xffff0f00,  0,    FMT_D8, AM33,	{MEM2(IMM24, SP), RN2}},
{ "mov",	0xfb9a0000,  0xffff0f00,  0,    FMT_D7, AM33,	{RM2, MEM2(IMM8, SP)}},
{ "mov",	0xfd9a0000,  0xffff0f00,  0,    FMT_D8, AM33,	{RM2, MEM2(IMM24, SP)}},
{ "mov",	0xfb9e0000,  0xffff000f,  0,    FMT_D7, AM33,	{RD2, MEM2(RI, RN0)}},
{ "mov",	0xfb6a0000,  0xffff0000,  0x22, FMT_D7, AM33,	{MEMINC2 (RM0, SIMM8), RN2}},
{ "mov",	0xfb7a0000,  0xffff0000,  0,	FMT_D7, AM33,	{RM2, MEMINC2 (RN0, SIMM8)}},
{ "mov",	0xfd6a0000,  0xffff0000,  0x22, FMT_D8, AM33,	{MEMINC2 (RM0, IMM24), RN2}},
{ "mov",	0xfd7a0000,  0xffff0000,  0,	FMT_D8, AM33,	{RM2, MEMINC2 (RN0, IMM24)}},
{ "mov",	0xfe6a0000,  0xffff0000,  0x22, FMT_D9, AM33,	{MEMINC2 (RM0, IMM32_HIGH8), RN2}},
{ "mov",	0xfe7a0000,  0xffff0000,  0,	FMT_D9, AM33,	{RN2, MEMINC2 (RM0, IMM32_HIGH8)}},
/* These must come after most of the other move instructions to avoid matching
   a symbolic name with IMMxx operands.  Ugh.  */
{ "mov",	0x2c0000,    0xfc0000,    0,    FMT_S2, 0,	{SIMM16, DN0}},
{ "mov",	0xfccc0000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "mov",	0x240000,    0xfc0000,    0,    FMT_S2, 0,	{IMM16, AN0}},
{ "mov",	0xfcdc0000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, AN0}},
{ "mov",	0xfca40000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM(IMM32_MEM), DN0}},
{ "mov",	0xfca00000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM(IMM32_MEM), AN0}},
{ "mov",	0xfc810000,  0xfff30000,  0,    FMT_D4, 0,	{DM1, MEM(IMM32_MEM)}},
{ "mov",	0xfc800000,  0xfff30000,  0,    FMT_D4, 0,	{AM1, MEM(IMM32_MEM)}},
{ "mov",	0xfc000000,  0xfff00000,  0,    FMT_D4, 0,	{MEM2(IMM32,AM0), DN1}},
{ "mov",	0xfcb40000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM2(IMM32, SP), DN0}},
{ "mov",	0xfc200000,  0xfff00000,  0,    FMT_D4, 0,	{MEM2(IMM32,AM0), AN1}},
{ "mov",	0xfcb00000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM2(IMM32, SP), AN0}},
{ "mov",	0xfc100000,  0xfff00000,  0,    FMT_D4, 0,	{DM1, MEM2(IMM32,AN0)}},
{ "mov",	0xfc910000,  0xfff30000,  0,    FMT_D4, 0,	{DM1, MEM2(IMM32, SP)}},
{ "mov",	0xfc300000,  0xfff00000,  0,    FMT_D4, 0,	{AM1, MEM2(IMM32,AN0)}},
{ "mov",	0xfc900000,  0xfff30000,  0,    FMT_D4, 0,	{AM1, MEM2(IMM32, SP)}},
/* These non-promoting variants need to come after all the other memory
   moves.  */
{ "mov",	0xf8f000,    0xfffc00,    0,    FMT_D1, AM30,	{MEM2(SD8N, AM0), SP}},
{ "mov",	0xf8f400,    0xfffc00,    0,    FMT_D1, AM30,	{SP, MEM2(SD8N, AN0)}},
/* These are the same as the previous non-promoting versions.  The am33
   does not have restrictions on the offsets used to load/store the stack
   pointer.  */
{ "mov",	0xf8f000,    0xfffc00,    0,    FMT_D1, AM33,	{MEM2(SD8, AM0), SP}},
{ "mov",	0xf8f400,    0xfffc00,    0,    FMT_D1, AM33,	{SP, MEM2(SD8, AN0)}},
/* These must come last so that we favor shorter move instructions for
   loading immediates into d0-d3/a0-a3.  */
{ "mov",	0xfb080000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "mov",	0xfd080000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "mov",	0xfe080000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "mov",	0xfbf80000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, XRN02}},
{ "mov",	0xfdf80000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, XRN02}},
{ "mov",	0xfef80000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, XRN02}},
{ "mov",	0xfe0e0000,  0xffff0f00,  0,    FMT_D9, AM33,	{MEM(IMM32_HIGH8_MEM), RN2}},
{ "mov",	0xfe1e0000,  0xffff0f00,  0,    FMT_D9, AM33,	{RM2, MEM(IMM32_HIGH8_MEM)}},
{ "mov",	0xfe0a0000,  0xffff0000,  0,    FMT_D9, AM33,	{MEM2(IMM32_HIGH8,RM0), RN2}},
{ "mov",	0xfe1a0000,  0xffff0000,  0,    FMT_D9, AM33,	{RM2, MEM2(IMM32_HIGH8, RN0)}},
{ "mov",	0xfe8a0000,  0xffff0f00,  0,    FMT_D9, AM33,	{MEM2(IMM32_HIGH8, SP), RN2}},
{ "mov",	0xfe9a0000,  0xffff0f00,  0,    FMT_D9, AM33,	{RM2, MEM2(IMM32_HIGH8, SP)}},

{ "movu",	0xfb180000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "movu",	0xfd180000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "movu",	0xfe180000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "mcst9",	0xf630,      0xfff0,	  0,    FMT_D0, AM33,	{DN01}},
{ "mcst48",	0xf660,	     0xfff0,	  0,    FMT_D0, AM33,	{DN01}},
{ "swap",	0xf680,	     0xfff0,	  0,    FMT_D0, AM33,	{DM1, DN0}},
{ "swap",	0xf9cb00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "swaph",	0xf690,	     0xfff0,	  0,    FMT_D0, AM33,	{DM1, DN0}},
{ "swaph",	0xf9db00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "getchx",	0xf6c0,	     0xfff0,	  0,    FMT_D0, AM33,	{DN01}},
{ "getclx",	0xf6d0,	     0xfff0,	  0,    FMT_D0, AM33,	{DN01}},
{ "mac",	0xfb0f0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "mac",	0xf90b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "mac",	0xfb0b0000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "mac",	0xfd0b0000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "mac",	0xfe0b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "macu",	0xfb1f0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "macu",	0xf91b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "macu",	0xfb1b0000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "macu",	0xfd1b0000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "macu",	0xfe1b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "macb",	0xfb2f0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "macb",	0xf92b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "macb",	0xfb2b0000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "macb",	0xfd2b0000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "macb",	0xfe2b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "macbu",	0xfb3f0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "macbu",	0xf93b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "macbu",	0xfb3b0000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "macbu",	0xfd3b0000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "macbu",	0xfe3b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "mach",	0xfb4f0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "mach",	0xf94b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "mach",	0xfb4b0000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "mach",	0xfd4b0000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "mach",	0xfe4b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "machu",	0xfb5f0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "machu",	0xf95b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "machu",	0xfb5b0000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "machu",	0xfd5b0000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "machu",	0xfe5b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "dmach",	0xfb6f0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "dmach",	0xf96b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "dmach",	0xfe6b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "dmachu",	0xfb7f0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "dmachu",	0xf97b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "dmachu",	0xfe7b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "dmulh",	0xfb8f0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "dmulh",	0xf98b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "dmulh",	0xfe8b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "dmulhu",	0xfb9f0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "dmulhu",	0xf99b00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "dmulhu",	0xfe9b0000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "mcste",	0xf9bb00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "mcste",	0xfbbb0000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "swhw",	0xf9eb00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},

{ "movbu",	0xf040,	     0xfff0,	  0,    FMT_D0, 0,	{MEM(AM0), DN1}},
{ "movbu",	0xf84000,    0xfff000,    0,    FMT_D1, 0,	{MEM2(SD8, AM0), DN1}},
{ "movbu",	0xfa400000,  0xfff00000,  0,    FMT_D2, 0,	{MEM2(SD16, AM0), DN1}},
{ "movbu",	0xf8b800,    0xfffcff,    0,    FMT_D1, 0,	{MEM(SP), DN0}},
{ "movbu",	0xf8b800,    0xfffc00,    0,    FMT_D1, 0,	{MEM2(IMM8, SP), DN0}},
{ "movbu",	0xfab80000,  0xfffc0000,  0,    FMT_D2, 0,	{MEM2(IMM16, SP), DN0}},
{ "movbu",	0xf400,	     0xffc0,	  0,    FMT_D0, 0,	{MEM2(DI, AM0), DN2}},
{ "movbu",	0x340000,    0xfc0000,    0,    FMT_S2, 0,	{MEM(IMM16_MEM), DN0}},
{ "movbu",	0xf050,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, MEM(AN0)}},
{ "movbu",	0xf85000,    0xfff000,    0,    FMT_D1, 0,	{DM1, MEM2(SD8, AN0)}},
{ "movbu",	0xfa500000,  0xfff00000,  0,    FMT_D2, 0,	{DM1, MEM2(SD16, AN0)}},
{ "movbu",	0xf89200,    0xfff3ff,    0,    FMT_D1, 0,	{DM1, MEM(SP)}},
{ "movbu",	0xf89200,    0xfff300,    0,    FMT_D1, 0,	{DM1, MEM2(IMM8, SP)}},
{ "movbu",	0xfa920000,  0xfff30000,  0,    FMT_D2, 0,	{DM1, MEM2(IMM16, SP)}},
{ "movbu",	0xf440,	     0xffc0,	  0,    FMT_D0, 0,	{DM2, MEM2(DI, AN0)}},
{ "movbu",	0x020000,    0xf30000,    0,    FMT_S2, 0,	{DM1, MEM(IMM16_MEM)}},
{ "movbu",	0xf92a00,    0xffff00,    0,    FMT_D6, AM33,	{MEM(RM0), RN2}},
{ "movbu",	0xf93a00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, MEM(RN0)}},
{ "movbu",	0xf9aa00,    0xffff0f,    0,    FMT_D6, AM33,	{MEM(SP), RN2}},
{ "movbu",	0xf9ba00,    0xffff0f,    0,    FMT_D6, AM33,	{RM2, MEM(SP)}},
{ "movbu",	0xfb2a0000,  0xffff0000,  0,    FMT_D7, AM33,	{MEM2(SD8, RM0), RN2}},
{ "movbu",	0xfd2a0000,  0xffff0000,  0,    FMT_D8, AM33,	{MEM2(SD24, RM0), RN2}},
{ "movbu",	0xfb3a0000,  0xffff0000,  0,    FMT_D7, AM33,	{RM2, MEM2(SD8, RN0)}},
{ "movbu",	0xfd3a0000,  0xffff0000,  0,    FMT_D8, AM33,	{RM2, MEM2(SD24, RN0)}},
{ "movbu",	0xfbaa0000,  0xffff0f00,  0,    FMT_D7, AM33,	{MEM2(IMM8, SP), RN2}},
{ "movbu",	0xfdaa0000,  0xffff0f00,  0,    FMT_D8, AM33,	{MEM2(IMM24, SP), RN2}},
{ "movbu",	0xfbba0000,  0xffff0f00,  0,    FMT_D7, AM33,	{RM2, MEM2(IMM8, SP)}},
{ "movbu",	0xfdba0000,  0xffff0f00,  0,    FMT_D8, AM33,	{RM2, MEM2(IMM24, SP)}},
{ "movbu",	0xfb2e0000,  0xffff0f00,  0,    FMT_D7, AM33,	{MEM(IMM8_MEM), RN2}},
{ "movbu",	0xfd2e0000,  0xffff0f00,  0,    FMT_D8, AM33,	{MEM(IMM24_MEM), RN2}},
{ "movbu",	0xfb3e0000,  0xffff0f00,  0,    FMT_D7, AM33,	{RM2, MEM(IMM8_MEM)}},
{ "movbu",	0xfd3e0000,  0xffff0f00,  0,    FMT_D8, AM33,	{RM2, MEM(IMM24_MEM)}},
{ "movbu",	0xfbae0000,  0xffff000f,  0,    FMT_D7, AM33,	{MEM2(RI, RM0), RD2}},
{ "movbu",	0xfbbe0000,  0xffff000f,  0,    FMT_D7, AM33,	{RD2, MEM2(RI, RN0)}},
{ "movbu",	0xfc400000,  0xfff00000,  0,    FMT_D4, 0,	{MEM2(IMM32,AM0), DN1}},
{ "movbu",	0xfcb80000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM2(IMM32, SP), DN0}},
{ "movbu",	0xfca80000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM(IMM32_MEM), DN0}},
{ "movbu",	0xfc500000,  0xfff00000,  0,    FMT_D4, 0,	{DM1, MEM2(IMM32,AN0)}},
{ "movbu",	0xfc920000,  0xfff30000,  0,    FMT_D4, 0,	{DM1, MEM2(IMM32, SP)}},
{ "movbu",	0xfc820000,  0xfff30000,  0,    FMT_D4, 0,	{DM1, MEM(IMM32_MEM)}},
{ "movbu",	0xfe2a0000,  0xffff0000,  0,    FMT_D9, AM33,	{MEM2(IMM32_HIGH8,RM0), RN2}},
{ "movbu",	0xfe3a0000,  0xffff0000,  0,    FMT_D9, AM33,	{RM2, MEM2(IMM32_HIGH8, RN0)}},
{ "movbu",	0xfeaa0000,  0xffff0f00,  0,    FMT_D9, AM33,	{MEM2(IMM32_HIGH8,SP), RN2}},
{ "movbu",	0xfeba0000,  0xffff0f00,  0,    FMT_D9, AM33,	{RM2, MEM2(IMM32_HIGH8, SP)}},
{ "movbu",	0xfe2e0000,  0xffff0f00,  0,    FMT_D9, AM33,	{MEM(IMM32_HIGH8_MEM), RN2}},
{ "movbu",	0xfe3e0000,  0xffff0f00,  0,    FMT_D9, AM33,	{RM2, MEM(IMM32_HIGH8_MEM)}},

{ "movhu",	0xf060,	     0xfff0,	  0,    FMT_D0, 0,	{MEM(AM0), DN1}},
{ "movhu",	0xf86000,    0xfff000,    0,    FMT_D1, 0,	{MEM2(SD8, AM0), DN1}},
{ "movhu",	0xfa600000,  0xfff00000,  0,    FMT_D2, 0,	{MEM2(SD16, AM0), DN1}},
{ "movhu",	0xf8bc00,    0xfffcff,    0,    FMT_D1, 0,	{MEM(SP), DN0}},
{ "movhu",	0xf8bc00,    0xfffc00,    0,    FMT_D1, 0,	{MEM2(IMM8, SP), DN0}},
{ "movhu",	0xfabc0000,  0xfffc0000,  0,    FMT_D2, 0,	{MEM2(IMM16, SP), DN0}},
{ "movhu",	0xf480,	     0xffc0,	  0,    FMT_D0, 0,	{MEM2(DI, AM0), DN2}},
{ "movhu",	0x380000,    0xfc0000,    0,    FMT_S2, 0,	{MEM(IMM16_MEM), DN0}},
{ "movhu",	0xf070,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, MEM(AN0)}},
{ "movhu",	0xf87000,    0xfff000,    0,    FMT_D1, 0,	{DM1, MEM2(SD8, AN0)}},
{ "movhu",	0xfa700000,  0xfff00000,  0,    FMT_D2, 0,	{DM1, MEM2(SD16, AN0)}},
{ "movhu",	0xf89300,    0xfff3ff,    0,    FMT_D1, 0,	{DM1, MEM(SP)}},
{ "movhu",	0xf89300,    0xfff300,    0,    FMT_D1, 0,	{DM1, MEM2(IMM8, SP)}},
{ "movhu",	0xfa930000,  0xfff30000,  0,    FMT_D2, 0,	{DM1, MEM2(IMM16, SP)}},
{ "movhu",	0xf4c0,	     0xffc0,	  0,    FMT_D0, 0,	{DM2, MEM2(DI, AN0)}},
{ "movhu",	0x030000,    0xf30000,    0,    FMT_S2, 0,	{DM1, MEM(IMM16_MEM)}},
{ "movhu",	0xf94a00,    0xffff00,    0,    FMT_D6, AM33,	{MEM(RM0), RN2}},
{ "movhu",	0xf95a00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, MEM(RN0)}},
{ "movhu",	0xf9ca00,    0xffff0f,    0,    FMT_D6, AM33,	{MEM(SP), RN2}},
{ "movhu",	0xf9da00,    0xffff0f,    0,    FMT_D6, AM33,	{RM2, MEM(SP)}},
{ "movhu",	0xf9ea00,    0xffff00,    0x12, FMT_D6, AM33,	{MEMINC(RM0), RN2}},
{ "movhu",	0xf9fa00,    0xffff00,    0,	FMT_D6, AM33,	{RM2, MEMINC(RN0)}},
{ "movhu",	0xfb4a0000,  0xffff0000,  0,    FMT_D7, AM33,	{MEM2(SD8, RM0), RN2}},
{ "movhu",	0xfd4a0000,  0xffff0000,  0,    FMT_D8, AM33,	{MEM2(SD24, RM0), RN2}},
{ "movhu",	0xfb5a0000,  0xffff0000,  0,    FMT_D7, AM33,	{RM2, MEM2(SD8, RN0)}},
{ "movhu",	0xfd5a0000,  0xffff0000,  0,    FMT_D8, AM33,	{RM2, MEM2(SD24, RN0)}},
{ "movhu",	0xfbca0000,  0xffff0f00,  0,    FMT_D7, AM33,	{MEM2(IMM8, SP), RN2}},
{ "movhu",	0xfdca0000,  0xffff0f00,  0,    FMT_D8, AM33,	{MEM2(IMM24, SP), RN2}},
{ "movhu",	0xfbda0000,  0xffff0f00,  0,    FMT_D7, AM33,	{RM2, MEM2(IMM8, SP)}},
{ "movhu",	0xfdda0000,  0xffff0f00,  0,    FMT_D8, AM33,	{RM2, MEM2(IMM24, SP)}},
{ "movhu",	0xfb4e0000,  0xffff0f00,  0,    FMT_D7, AM33,	{MEM(IMM8_MEM), RN2}},
{ "movhu",	0xfd4e0000,  0xffff0f00,  0,    FMT_D8, AM33,	{MEM(IMM24_MEM), RN2}},
{ "movhu",	0xfbce0000,  0xffff000f,  0,    FMT_D7, AM33,	{MEM2(RI, RM0), RD2}},
{ "movhu",	0xfbde0000,  0xffff000f,  0,    FMT_D7, AM33,	{RD2, MEM2(RI, RN0)}},
{ "movhu",	0xfc600000,  0xfff00000,  0,    FMT_D4, 0,	{MEM2(IMM32,AM0), DN1}},
{ "movhu",	0xfcbc0000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM2(IMM32, SP), DN0}},
{ "movhu",	0xfcac0000,  0xfffc0000,  0,    FMT_D4, 0,	{MEM(IMM32_MEM), DN0}},
{ "movhu",	0xfc700000,  0xfff00000,  0,    FMT_D4, 0,	{DM1, MEM2(IMM32,AN0)}},
{ "movhu",	0xfc930000,  0xfff30000,  0,    FMT_D4, 0,	{DM1, MEM2(IMM32, SP)}},
{ "movhu",	0xfc830000,  0xfff30000,  0,    FMT_D4, 0,	{DM1, MEM(IMM32_MEM)}},
{ "movhu",	0xfe4a0000,  0xffff0000,  0,    FMT_D9, AM33,	{MEM2(IMM32_HIGH8,RM0), RN2}},
{ "movhu",	0xfe5a0000,  0xffff0000,  0,    FMT_D9, AM33,	{RM2, MEM2(IMM32_HIGH8, RN0)}},
{ "movhu",	0xfeca0000,  0xffff0f00,  0,    FMT_D9, AM33,	{MEM2(IMM32_HIGH8, SP), RN2}},
{ "movhu",	0xfeda0000,  0xffff0f00,  0,    FMT_D9, AM33,	{RM2, MEM2(IMM32_HIGH8, SP)}},
{ "movhu",	0xfe4e0000,  0xffff0f00,  0,    FMT_D9, AM33,	{MEM(IMM32_HIGH8_MEM), RN2}},
{ "movhu",	0xfb5e0000,  0xffff0f00,  0,    FMT_D7, AM33,	{RM2, MEM(IMM8_MEM)}},
{ "movhu",	0xfd5e0000,  0xffff0f00,  0,    FMT_D8, AM33,	{RM2, MEM(IMM24_MEM)}},
{ "movhu",	0xfe5e0000,  0xffff0f00,  0,    FMT_D9, AM33,	{RM2, MEM(IMM32_HIGH8_MEM)}},
{ "movhu",	0xfbea0000,  0xffff0000,  0x22, FMT_D7, AM33,	{MEMINC2 (RM0, SIMM8), RN2}},
{ "movhu",	0xfbfa0000,  0xffff0000,  0,	FMT_D7, AM33,	{RM2, MEMINC2 (RN0, SIMM8)}},
{ "movhu",	0xfdea0000,  0xffff0000,  0x22, FMT_D8, AM33,	{MEMINC2 (RM0, IMM24), RN2}},
{ "movhu",	0xfdfa0000,  0xffff0000,  0,	FMT_D8, AM33,	{RM2, MEMINC2 (RN0, IMM24)}},
{ "movhu",	0xfeea0000,  0xffff0000,  0x22, FMT_D9, AM33,	{MEMINC2 (RM0, IMM32_HIGH8), RN2}},
{ "movhu",	0xfefa0000,  0xffff0000,  0,	FMT_D9, AM33,	{RN2, MEMINC2 (RM0, IMM32_HIGH8)}},

{ "ext",	0xf2d0,	     0xfffc,	  0,    FMT_D0, 0,	{DN0}},
{ "ext",	0xf91800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "extb",	0xf92800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "extb",	0x10, 	     0xfc,	  0,    FMT_S0, 0,	{DN0}},
{ "extb",	0xf92800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "extbu",	0xf93800,    0xffff00,	  0,    FMT_D6, AM33,	{RM2, RN0}},
{ "extbu",	0x14,	     0xfc,	  0,    FMT_S0, 0,	{DN0}},
{ "extbu",	0xf93800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "exth",	0xf94800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "exth",	0x18,	     0xfc,	  0,    FMT_S0, 0,	{DN0}},
{ "exth",	0xf94800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "exthu",	0xf95800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "exthu",	0x1c,	     0xfc,	  0,    FMT_S0, 0,	{DN0}},
{ "exthu",	0xf95800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "movm",	0xce00,	     0xff00,	  0,    FMT_S1, 0,	{MEM(SP), REGS}},
{ "movm",	0xcf00,	     0xff00,	  0,    FMT_S1, 0,	{REGS, MEM(SP)}},
{ "movm",	0xf8ce00,    0xffff00,    0,    FMT_D1, AM33,	{MEM(USP), REGS}},
{ "movm",	0xf8cf00,    0xffff00,    0,    FMT_D1, AM33,	{REGS, MEM(USP)}},

{ "clr",	0x00,	     0xf3,	  0,    FMT_S0, 0,	{DN1}},
{ "clr",	0xf96800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "add",	0xfb7c0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "add",	0xe0,	     0xf0,	  0,    FMT_S0, 0,	{DM1, DN0}},
{ "add",	0xf160,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, AN0}},
{ "add",	0xf150,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, DN0}},
{ "add",	0xf170,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, AN0}},
{ "add",	0x2800,	     0xfc00,	  0,    FMT_S1, 0,	{SIMM8, DN0}},
{ "add",	0xfac00000,  0xfffc0000,  0,    FMT_D2, 0,	{SIMM16, DN0}},
{ "add",	0x2000,	     0xfc00,	  0,    FMT_S1, 0,	{SIMM8, AN0}},
{ "add",	0xfad00000,  0xfffc0000,  0,    FMT_D2, 0,	{SIMM16, AN0}},
{ "add",	0xf8fe00,    0xffff00,    0,    FMT_D1, 0,	{SIMM8, SP}},
{ "add",	0xfafe0000,  0xffff0000,  0,    FMT_D2, 0,	{SIMM16, SP}},
{ "add",	0xf97800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "add",	0xfcc00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "add",	0xfcd00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, AN0}},
{ "add",	0xfcfe0000,  0xffff0000,  0,    FMT_D4, 0,	{IMM32, SP}},
{ "add",	0xfb780000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "add",	0xfd780000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "add",	0xfe780000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "addc",	0xfb8c0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "addc",	0xf140,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "addc",	0xf98800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "addc",	0xfb880000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "addc",	0xfd880000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "addc",	0xfe880000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "sub",	0xfb9c0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "sub",	0xf100,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "sub",	0xf120,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, AN0}},
{ "sub",	0xf110,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, DN0}},
{ "sub",	0xf130,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, AN0}},
{ "sub",	0xf99800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "sub",	0xfcc40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "sub",	0xfcd40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, AN0}},
{ "sub",	0xfb980000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "sub",	0xfd980000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "sub",	0xfe980000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "subc",	0xfbac0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "subc",	0xf180,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "subc",	0xf9a800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "subc",	0xfba80000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "subc",	0xfda80000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "subc",	0xfea80000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "mul",	0xfbad0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "mul",	0xf240,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "mul",	0xf9a900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "mul",	0xfba90000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "mul",	0xfda90000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "mul",	0xfea90000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "mulu",	0xfbbd0000,  0xffff0000,  0xc,  FMT_D7, AM33,	{RM2, RN0, RD2, RD0}},
{ "mulu",	0xf250,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "mulu",	0xf9b900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "mulu",	0xfbb90000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "mulu",	0xfdb90000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "mulu",	0xfeb90000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "div",	0xf260,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "div",	0xf9c900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},

{ "divu",	0xf270,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "divu",	0xf9d900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},

{ "inc",	0x40,	     0xf3,	  0,    FMT_S0, 0,	{DN1}},
{ "inc",	0x41,	     0xf3,	  0,    FMT_S0, 0,	{AN1}},
{ "inc",	0xf9b800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "inc4",	0x50,	     0xfc,	  0,    FMT_S0, 0,	{AN0}},
{ "inc4",	0xf9c800,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "cmp",	0xa000,	     0xf000,	  0,    FMT_S1, 0,	{SIMM8, DN01}},
{ "cmp",	0xa0,	     0xf0,	  0x3,  FMT_S0, 0,	{DM1, DN0}},
{ "cmp",	0xf1a0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, AN0}},
{ "cmp",	0xf190,	     0xfff0,	  0,    FMT_D0, 0,	{AM1, DN0}},
{ "cmp",	0xb000,	     0xf000,	  0,    FMT_S1, 0,	{IMM8, AN01}},
{ "cmp",	0xb0,	     0xf0,	  0x3,  FMT_S0, 0,	{AM1, AN0}},
{ "cmp",	0xfac80000,  0xfffc0000,  0,    FMT_D2, 0,	{SIMM16, DN0}},
{ "cmp",	0xfad80000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM16, AN0}},
{ "cmp",	0xf9d800,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "cmp",	0xfcc80000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "cmp",	0xfcd80000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, AN0}},
{ "cmp",	0xfbd80000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "cmp",	0xfdd80000,  0xffff0000,  0,    FMT_D8, AM33,	{SIMM24, RN02}},
{ "cmp",	0xfed80000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "and",	0xfb0d0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "and",	0xf200,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "and",	0xf8e000,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "and",	0xfae00000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM16, DN0}},
{ "and",	0xfafc0000,  0xffff0000,  0,    FMT_D2, 0,	{IMM16, PSW}},
{ "and",	0xfcfc0000,  0xffff0000,  0,    FMT_D4, AM33,	{IMM32, EPSW}},
{ "and",	0xf90900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "and",	0xfce00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "and",	0xfb090000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "and",	0xfd090000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "and",	0xfe090000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "or",		0xfb1d0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "or",		0xf210,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "or",		0xf8e400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "or",		0xfae40000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM16, DN0}},
{ "or",		0xfafd0000,  0xffff0000,  0,    FMT_D2, 0,	{IMM16, PSW}},
{ "or",		0xfcfd0000,  0xffff0000,  0,    FMT_D4, AM33,	{IMM32, EPSW}},
{ "or",		0xf91900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "or",		0xfce40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "or",		0xfb190000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "or",		0xfd190000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "or",		0xfe190000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "xor",	0xfb2d0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "xor",	0xf220,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "xor",	0xfae80000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM16, DN0}},
{ "xor",	0xf92900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "xor",	0xfce80000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "xor",	0xfb290000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "xor",	0xfd290000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "xor",	0xfe290000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},

{ "not",	0xf230,	     0xfffc,	  0,    FMT_D0, 0,	{DN0}},
{ "not",	0xf93900,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "btst",	0xf8ec00,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "btst",	0xfaec0000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM16, DN0}},
{ "btst",	0xfcec0000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
/* Place these before the ones with IMM8E and SD8N_SHIFT8 since we want the
   them to match last since they do not promote.  */
{ "btst",	0xfbe90000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "btst",	0xfde90000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "btst",	0xfee90000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "btst",	0xfe820000,  0xffff0000,  0,    FMT_D3, AM33_2, {IMM8E, MEM(IMM16_MEM)}},
{ "btst",	0xfe020000,  0xffff0000,  0,    FMT_D5, 0,	{IMM8E, MEM(IMM32_LOWSHIFT8)}},
{ "btst",	0xfaf80000,  0xfffc0000,  0,    FMT_D2, 0, 	{IMM8, MEM2(SD8N_SHIFT8, AN0)}},

{ "bset",	0xf080,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, MEM(AN0)}},
{ "bset",	0xfe800000,  0xffff0000,  0,    FMT_D3, AM33_2, {IMM8E, MEM(IMM16_MEM)}},
{ "bset",	0xfe000000,  0xffff0000,  0,    FMT_D5, 0,	{IMM8E, MEM(IMM32_LOWSHIFT8)}},
{ "bset",	0xfaf00000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM8, MEM2(SD8N_SHIFT8, AN0)}},

{ "bclr",	0xf090,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, MEM(AN0)}},
{ "bclr",	0xfe810000,  0xffff0000,  0,    FMT_D3, AM33_2, {IMM8E, MEM(IMM16_MEM)}},
{ "bclr",	0xfe010000,  0xffff0000,  0,    FMT_D5, 0,	{IMM8E, MEM(IMM32_LOWSHIFT8)}},
{ "bclr",	0xfaf40000,  0xfffc0000,  0,    FMT_D2, 0,	{IMM8, MEM2(SD8N_SHIFT8,AN0)}},

{ "asr",	0xfb4d0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "asr",	0xf2b0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "asr",	0xf8c800,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "asr",	0xf94900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "asr",	0xfb490000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "asr",	0xfd490000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "asr",	0xfe490000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "asr",	0xf8c801,    0xfffcff,    0,    FMT_D1, 0,	{DN0}},
{ "asr",	0xfb490001,  0xffff00ff,  0,    FMT_D7, AM33,	{RN02}},

{ "lsr",	0xfb5d0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "lsr",	0xf2a0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "lsr",	0xf8c400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "lsr",	0xf95900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "lsr",	0xfb590000,  0xffff0000,  0,    FMT_D7, AM33,	{IMM8, RN02}},
{ "lsr",	0xfd590000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "lsr",	0xfe590000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "lsr",	0xf8c401,    0xfffcff,    0,    FMT_D1, 0,	{DN0}},
{ "lsr",	0xfb590001,  0xffff00ff,  0,    FMT_D7, AM33,	{RN02}},

{ "asl",	0xfb6d0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "asl",	0xf290,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "asl",	0xf8c000,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "asl",	0xf96900,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},
{ "asl",	0xfb690000,  0xffff0000,  0,    FMT_D7, AM33,	{SIMM8, RN02}},
{ "asl",	0xfd690000,  0xffff0000,  0,    FMT_D8, AM33,	{IMM24, RN02}},
{ "asl",	0xfe690000,  0xffff0000,  0,    FMT_D9, AM33,	{IMM32_HIGH8, RN02}},
{ "asl",	0xf8c001,    0xfffcff,    0,    FMT_D1, 0,	{DN0}},
{ "asl",	0xfb690001,  0xffff00ff,  0,    FMT_D7, AM33,	{RN02}},

{ "asl2",	0x54,	     0xfc,	  0,    FMT_S0, 0,	{DN0}},
{ "asl2",	0xf97900,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "ror",	0xf284,	     0xfffc,	  0,    FMT_D0, 0,	{DN0}},
{ "ror",	0xf98900,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "rol",	0xf280,	     0xfffc,	  0,    FMT_D0, 0,	{DN0}},
{ "rol",	0xf99900,    0xffff00,    0,    FMT_D6, AM33,	{RN02}},

{ "beq",	0xc800,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bne",	0xc900,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bgt",	0xc100,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bge",	0xc200,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "ble",	0xc300,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "blt",	0xc000,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bhi",	0xc500,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bcc",	0xc600,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bls",	0xc700,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bcs",	0xc400,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},
{ "bvc",	0xf8e800,    0xffff00,    0,    FMT_D1, 0,	{SD8N_PCREL}},
{ "bvs",	0xf8e900,    0xffff00,    0,    FMT_D1, 0,	{SD8N_PCREL}},
{ "bnc",	0xf8ea00,    0xffff00,    0,    FMT_D1, 0,	{SD8N_PCREL}},
{ "bns",	0xf8eb00,    0xffff00,    0,    FMT_D1, 0,	{SD8N_PCREL}},
{ "bra",	0xca00,	     0xff00,	  0,    FMT_S1, 0,	{SD8N_PCREL}},

{ "leq",	0xd8,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lne",	0xd9,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lgt",	0xd1,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lge",	0xd2,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lle",	0xd3,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "llt",	0xd0,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lhi",	0xd5,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lcc",	0xd6,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lls",	0xd7,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lcs",	0xd4,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "lra",	0xda,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},
{ "setlb",	0xdb,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},

{ "fbeq",	0xf8d000,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbne",	0xf8d100,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbgt",	0xf8d200,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbge",	0xf8d300,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fblt",	0xf8d400,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fble",	0xf8d500,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbuo",	0xf8d600,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fblg",	0xf8d700,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbleg",	0xf8d800,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbug",	0xf8d900,    0xffff00,	  0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbuge",	0xf8da00,    0xffff00,    0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbul",	0xf8db00,    0xffff00,    0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbule",	0xf8dc00,    0xffff00,    0,    FMT_D1, AM33_2,	{SD8N_PCREL}},
{ "fbue",	0xf8dd00,    0xffff00,    0,    FMT_D1, AM33_2,	{SD8N_PCREL}},

{ "fleq",	0xf0d0,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flne",	0xf0d1,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flgt",	0xf0d2,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flge",	0xf0d3,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "fllt",	0xf0d4,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flle",	0xf0d5,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "fluo",	0xf0d6,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "fllg",	0xf0d7,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flleg",	0xf0d8,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flug",	0xf0d9,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "fluge",	0xf0da,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flul",	0xf0db,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flule",	0xf0dc,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},
{ "flue",	0xf0dd,	     0xffff,	  0,    FMT_D0, AM33_2,	{UNUSED}},

{ "jmp",	0xf0f4,	     0xfffc,	  0,    FMT_D0, 0,	{PAREN,AN0,PAREN}},
{ "jmp",	0xcc0000,    0xff0000,    0,    FMT_S2, 0,	{IMM16_PCREL}},
{ "jmp",	0xdc000000,  0xff000000,  0,    FMT_S4, 0,	{IMM32_HIGH24}},
{ "call",	0xcd000000,  0xff000000,  0,    FMT_S4, 0,	{D16_SHIFT,REGS,IMM8E}},
{ "call",	0xdd000000,  0xff000000,  0,    FMT_S6, 0,	{IMM32_HIGH24_LOWSHIFT16, REGSE_SHIFT8,IMM8E}},
{ "calls",	0xf0f0,	     0xfffc,	  0,    FMT_D0, 0,	{PAREN,AN0,PAREN}},
{ "calls",	0xfaff0000,  0xffff0000,  0,    FMT_D2, 0,	{IMM16_PCREL}},
{ "calls",	0xfcff0000,  0xffff0000,  0,    FMT_D4, 0,	{IMM32_PCREL}},

{ "ret",	0xdf0000,    0xff0000,    0,    FMT_S2, 0,	{REGS_SHIFT8, IMM8}},
{ "retf",	0xde0000,    0xff0000,    0,    FMT_S2, 0,	{REGS_SHIFT8, IMM8}},
{ "rets",	0xf0fc,	     0xffff,	  0,    FMT_D0, 0,	{UNUSED}},
{ "rti",	0xf0fd,	     0xffff,	  0,    FMT_D0, 0,	{UNUSED}},
{ "trap",	0xf0fe,	     0xffff,	  0,    FMT_D0, 0,	{UNUSED}},
{ "rtm",	0xf0ff,	     0xffff,	  0,    FMT_D0, 0,	{UNUSED}},
{ "nop",	0xcb,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},

{ "dcpf",	0xf9a600,    0xffff0f,    0,	FMT_D6, AM33_2,  {MEM (RM2)}},
{ "dcpf",	0xf9a700,    0xffffff,    0,	FMT_D6, AM33_2,  {MEM (SP)}},
{ "dcpf",	0xfba60000,  0xffff00ff,  0,	FMT_D7, AM33_2,  {MEM2 (RI,RM0)}},
{ "dcpf",	0xfba70000,  0xffff0f00,  0,	FMT_D7, AM33_2,  {MEM2 (SD8,RM2)}},
{ "dcpf",	0xfda70000,  0xffff0f00,  0,    FMT_D8, AM33_2,  {MEM2 (SD24,RM2)}},
{ "dcpf",	0xfe460000,  0xffff0f00,  0,    FMT_D9, AM33_2,  {MEM2 (IMM32_HIGH8,RM2)}},

{ "fmov",	0xf92000,    0xfffe00,    0,    FMT_D6, AM33_2,  {MEM (RM2), FSM0}},
{ "fmov",	0xf92200,    0xfffe00,    0,    FMT_D6, AM33_2,  {MEMINC (RM2), FSM0}},
{ "fmov",	0xf92400,    0xfffef0,    0,    FMT_D6, AM33_2,  {MEM (SP), FSM0}},
{ "fmov",	0xf92600,    0xfffe00,    0,    FMT_D6, AM33_2,  {RM2, FSM0}},
{ "fmov",	0xf93000,    0xfffd00,    0,    FMT_D6, AM33_2,  {FSM1, MEM (RM0)}},
{ "fmov",	0xf93100,    0xfffd00,    0,    FMT_D6, AM33_2,  {FSM1, MEMINC (RM0)}},
{ "fmov",	0xf93400,    0xfffd0f,    0,    FMT_D6, AM33_2,  {FSM1, MEM (SP)}},
{ "fmov",	0xf93500,    0xfffd00,    0,    FMT_D6, AM33_2,  {FSM1, RM0}},
{ "fmov",	0xf94000,    0xfffc00,    0,    FMT_D6, AM33_2,  {FSM1, FSM0}},
{ "fmov",	0xf9a000,    0xfffe01,    0,    FMT_D6, AM33_2,  {MEM (RM2), FDM0}},
{ "fmov",	0xf9a200,    0xfffe01,    0,    FMT_D6, AM33_2,  {MEMINC (RM2), FDM0}},
{ "fmov",	0xf9a400,    0xfffef1,    0,    FMT_D6, AM33_2,  {MEM (SP), FDM0}},
{ "fmov",	0xf9b000,    0xfffd10,    0,    FMT_D6, AM33_2,  {FDM1, MEM (RM0)}},
{ "fmov",	0xf9b100,    0xfffd10,    0,    FMT_D6, AM33_2,  {FDM1, MEMINC (RM0)}},
{ "fmov",	0xf9b400,    0xfffd1f,    0,    FMT_D6, AM33_2,  {FDM1, MEM (SP)}},
{ "fmov",	0xf9b500,    0xffff0f,    0,    FMT_D6, AM33_2,  {RM2, FPCR}},
{ "fmov",	0xf9b700,    0xfffff0,    0,    FMT_D6, AM33_2,  {FPCR, RM0}},
{ "fmov",	0xf9c000,    0xfffc11,    0,    FMT_D6, AM33_2,  {FDM1, FDM0}},
{ "fmov",	0xfb200000,  0xfffe0000,  0,    FMT_D7, AM33_2,	{MEM2 (SD8, RM2), FSM2}},
{ "fmov",	0xfb220000,  0xfffe0000,  0,    FMT_D7, AM33_2,	{MEMINC2 (RM2, SIMM8), FSM2}},
{ "fmov",	0xfb240000,  0xfffef000,  0,    FMT_D7, AM33_2,	{MEM2 (IMM8, SP), FSM2}},
{ "fmov",	0xfb270000,  0xffff000d,  0,    FMT_D7, AM33_2,	{MEM2 (RI, RM0), FSN1}},
{ "fmov",	0xfb300000,  0xfffd0000,  0,    FMT_D7, AM33_2,	{FSM3, MEM2 (SD8, RM0)}},
{ "fmov",	0xfb310000,  0xfffd0000,  0,    FMT_D7, AM33_2,	{FSM3, MEMINC2 (RM0, SIMM8)}},
{ "fmov",	0xfb340000,  0xfffd0f00,  0,    FMT_D7, AM33_2,	{FSM3, MEM2 (IMM8, SP)}},
{ "fmov",	0xfb370000,  0xffff000d,  0,    FMT_D7, AM33_2,	{FSN1, MEM2(RI, RM0)}},
  /* FIXME: the spec doesn't say the fd register must be even for the
   * next two insns.  Assuming it was a mistake in the spec.  */
{ "fmov",	0xfb470000,  0xffff001d,  0,    FMT_D7, AM33_2,	{MEM2 (RI, RM0), FDN1}},
{ "fmov",	0xfb570000,  0xffff001d,  0,    FMT_D7, AM33_2,	{FDN1, MEM2(RI, RM0)}},
  /* END of FIXME */
{ "fmov",	0xfba00000,  0xfffe0100,  0,    FMT_D7, AM33_2,	{MEM2 (SD8, RM2), FDM2}},
{ "fmov",	0xfba20000,  0xfffe0100,  0,    FMT_D7, AM33_2,	{MEMINC2 (RM2, SIMM8), FDM2}},
{ "fmov",	0xfba40000,  0xfffef100,  0,    FMT_D7, AM33_2,	{MEM2 (IMM8, SP), FDM2}},
{ "fmov",	0xfbb00000,  0xfffd1000,  0,    FMT_D7, AM33_2,	{FDM3, MEM2 (SD8, RM0)}},
{ "fmov",	0xfbb10000,  0xfffd1000,  0,    FMT_D7, AM33_2,	{FDM3, MEMINC2 (RM0, SIMM8)}},
{ "fmov",	0xfbb40000,  0xfffd1f00,  0,    FMT_D7, AM33_2,	{FDM3, MEM2 (IMM8, SP)}},
{ "fmov",	0xfd200000,  0xfffe0000,  0,    FMT_D8, AM33_2,	{MEM2 (SIMM24, RM2), FSM2}},
{ "fmov",	0xfd220000,  0xfffe0000,  0,    FMT_D8, AM33_2,	{MEMINC2 (RM2, SIMM24), FSM2}},
{ "fmov",	0xfd240000,  0xfffef000,  0,    FMT_D8, AM33_2,	{MEM2 (IMM24, SP), FSM2}},
{ "fmov",	0xfd300000,  0xfffd0000,  0,    FMT_D8, AM33_2,	{FSM3, MEM2 (SIMM24, RM0)}},
{ "fmov",	0xfd310000,  0xfffd0000,  0,    FMT_D8, AM33_2,	{FSM3, MEMINC2 (RM0, SIMM24)}},
{ "fmov",	0xfd340000,  0xfffd0f00,  0,    FMT_D8, AM33_2,	{FSM3, MEM2 (IMM24, SP)}},
{ "fmov",	0xfda00000,  0xfffe0100,  0,    FMT_D8, AM33_2,	{MEM2 (SIMM24, RM2), FDM2}},
{ "fmov",	0xfda20000,  0xfffe0100,  0,    FMT_D8, AM33_2,	{MEMINC2 (RM2, SIMM24), FDM2}},
{ "fmov",	0xfda40000,  0xfffef100,  0,    FMT_D8, AM33_2,	{MEM2 (IMM24, SP), FDM2}},
{ "fmov",	0xfdb00000,  0xfffd1000,  0,    FMT_D8, AM33_2,	{FDM3, MEM2 (SIMM24, RM0)}},
{ "fmov",	0xfdb10000,  0xfffd1000,  0,    FMT_D8, AM33_2,	{FDM3, MEMINC2 (RM0, SIMM24)}},
{ "fmov",	0xfdb40000,  0xfffd1f00,  0,    FMT_D8, AM33_2,	{FDM3, MEM2 (IMM24, SP)}},
{ "fmov",	0xfdb50000,  0xffff0000,  0,    FMT_D4, AM33_2,	{IMM32, FPCR}},
{ "fmov",	0xfe200000,  0xfffe0000,  0,    FMT_D9, AM33_2,	{MEM2 (IMM32_HIGH8, RM2), FSM2}},
{ "fmov",	0xfe220000,  0xfffe0000,  0,    FMT_D9, AM33_2,	{MEMINC2 (RM2, IMM32_HIGH8), FSM2}},
{ "fmov",	0xfe240000,  0xfffef000,  0,    FMT_D9, AM33_2,	{MEM2 (IMM32_HIGH8, SP), FSM2}},
{ "fmov",	0xfe260000,  0xfffef000,  0,    FMT_D9, AM33_2,	{IMM32_HIGH8, FSM2}},
{ "fmov",	0xfe300000,  0xfffd0000,  0,    FMT_D9, AM33_2,	{FSM3, MEM2 (IMM32_HIGH8, RM0)}},
{ "fmov",	0xfe310000,  0xfffd0000,  0,    FMT_D9, AM33_2,	{FSM3, MEMINC2 (RM0, IMM32_HIGH8)}},
{ "fmov",	0xfe340000,  0xfffd0f00,  0,    FMT_D9, AM33_2,	{FSM3, MEM2 (IMM32_HIGH8, SP)}},
{ "fmov",	0xfe400000,  0xfffe0100,  0,    FMT_D9, AM33_2,	{MEM2 (IMM32_HIGH8, RM2), FDM2}},
{ "fmov",	0xfe420000,  0xfffe0100,  0,    FMT_D9, AM33_2,	{MEMINC2 (RM2, IMM32_HIGH8), FDM2}},
{ "fmov",	0xfe440000,  0xfffef100,  0,    FMT_D9, AM33_2,	{MEM2 (IMM32_HIGH8, SP), FDM2}},
{ "fmov",	0xfe500000,  0xfffd1000,  0,    FMT_D9, AM33_2,	{FDM3, MEM2 (IMM32_HIGH8, RM0)}},
{ "fmov",	0xfe510000,  0xfffd1000,  0,    FMT_D9, AM33_2,	{FDM3, MEMINC2 (RM0, IMM32_HIGH8)}},
{ "fmov",	0xfe540000,  0xfffd1f00,  0,    FMT_D9, AM33_2,	{FDM3, MEM2 (IMM32_HIGH8, SP)}},

  /* FIXME: these are documented in the instruction bitmap, but not in
   * the instruction manual.  */
{ "ftoi",	0xfb400000,  0xffff0f05,  0,    FMT_D10,AM33_2,  {FSN3, FSN1}},
{ "itof",	0xfb420000,  0xffff0f05,  0,    FMT_D10,AM33_2,  {FSN3, FSN1}},
{ "ftod",	0xfb520000,  0xffff0f15,  0,    FMT_D10,AM33_2,  {FSN3, FDN1}},
{ "dtof",	0xfb560000,  0xffff1f05,  0,    FMT_D10,AM33_2,  {FDN3, FSN1}},
  /* END of FIXME */

{ "fabs",	0xfb440000,  0xffff0f05,  0,    FMT_D10,AM33_2,  {FSN3, FSN1}},
{ "fabs",	0xfbc40000,  0xffff1f15,  0,    FMT_D10,AM33_2,  {FDN3, FDN1}},
{ "fabs",	0xf94400,    0xfffef0,    0,    FMT_D6, AM33_2,  {FSM0}},
{ "fabs",	0xf9c400,    0xfffef1,    0,    FMT_D6, AM33_2,  {FDM0}},

{ "fneg",	0xfb460000,  0xffff0f05,  0,    FMT_D10,AM33_2,  {FSN3, FSN1}},
{ "fneg",	0xfbc60000,  0xffff1f15,  0,    FMT_D10,AM33_2,  {FDN3, FDN1}},
{ "fneg",	0xf94600,    0xfffef0,    0,    FMT_D6, AM33_2,  {FSM0}},
{ "fneg",	0xf9c600,    0xfffef1,    0,    FMT_D6, AM33_2,  {FDM0}},

{ "frsqrt",	0xfb500000,  0xffff0f05,  0,    FMT_D10,AM33_2,  {FSN3, FSN1}},
{ "frsqrt",	0xfbd00000,  0xffff1f15,  0,    FMT_D10,AM33_2,  {FDN3, FDN1}},
{ "frsqrt",	0xf95000,    0xfffef0,    0,    FMT_D6, AM33_2,  {FSM0}},
{ "frsqrt",	0xf9d000,    0xfffef1,    0,    FMT_D6, AM33_2,  {FDM0}},

  /* FIXME: this is documented in the instruction bitmap, but not in
   * the instruction manual.  */
{ "fsqrt",	0xfb540000,  0xffff0f05,  0,    FMT_D10,AM33_2,  {FSN3, FSN1}},
{ "fsqrt",	0xfbd40000,  0xffff1f15,  0,    FMT_D10,AM33_2,  {FDN3, FDN1}},
{ "fsqrt",	0xf95200,    0xfffef0,    0,    FMT_D6, AM33_2,  {FSM0}},
{ "fsqrt",	0xf9d200,    0xfffef1,    0,    FMT_D6, AM33_2,  {FDM0}},
  /* END of FIXME */

{ "fcmp",	0xf95400,    0xfffc00,    0,    FMT_D6, AM33_2,  {FSM1, FSM0}},
{ "fcmp",	0xf9d400,    0xfffc11,    0,    FMT_D6, AM33_2,  {FDM1, FDM0}},
{ "fcmp",	0xfe350000,  0xfffd0f00,  0,    FMT_D9, AM33_2,  {IMM32_HIGH8, FSM3}},

{ "fadd",	0xfb600000,  0xffff0001,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1}},
{ "fadd",	0xfbe00000,  0xffff1111,  0,    FMT_D10,AM33_2,  {FDN3, FDN2, FDN1}},
{ "fadd",	0xf96000,    0xfffc00,    0,    FMT_D6, AM33_2,  {FSM1, FSM0}},
{ "fadd",	0xf9e000,    0xfffc11,    0,    FMT_D6, AM33_2,  {FDM1, FDM0}},
{ "fadd",	0xfe600000,  0xfffc0000,  0,    FMT_D9, AM33_2,  {IMM32_HIGH8, FSM3, FSM2}},

{ "fsub",	0xfb640000,  0xffff0001,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1}},
{ "fsub",	0xfbe40000,  0xffff1111,  0,    FMT_D10,AM33_2,  {FDN3, FDN2, FDN1}},
{ "fsub",	0xf96400,    0xfffc00,    0,    FMT_D6, AM33_2,  {FSM1, FSM0}},
{ "fsub",	0xf9e400,    0xfffc11,    0,    FMT_D6, AM33_2,  {FDM1, FDM0}},
{ "fsub",	0xfe640000,  0xfffc0000,  0,    FMT_D9, AM33_2,  {IMM32_HIGH8, FSM3, FSM2}},

{ "fmul",	0xfb700000,  0xffff0001,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1}},
{ "fmul",	0xfbf00000,  0xffff1111,  0,    FMT_D10,AM33_2,  {FDN3, FDN2, FDN1}},
{ "fmul",	0xf97000,    0xfffc00,    0,    FMT_D6, AM33_2,  {FSM1, FSM0}},
{ "fmul",	0xf9f000,    0xfffc11,    0,    FMT_D6, AM33_2,  {FDM1, FDM0}},
{ "fmul",	0xfe700000,  0xfffc0000,  0,    FMT_D9, AM33_2,  {IMM32_HIGH8, FSM3, FSM2}},

{ "fdiv",	0xfb740000,  0xffff0001,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1}},
{ "fdiv",	0xfbf40000,  0xffff1111,  0,    FMT_D10,AM33_2,  {FDN3, FDN2, FDN1}},
{ "fdiv",	0xf97400,    0xfffc00,    0,    FMT_D6, AM33_2,  {FSM1, FSM0}},
{ "fdiv",	0xf9f400,    0xfffc11,    0,    FMT_D6, AM33_2,  {FDM1, FDM0}},
{ "fdiv",	0xfe740000,  0xfffc0000,  0,    FMT_D9, AM33_2,  {IMM32_HIGH8, FSM3, FSM2}},

{ "fmadd",	0xfb800000,  0xfffc0000,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1, FSACC}},
{ "fmsub",	0xfb840000,  0xfffc0000,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1, FSACC}},
{ "fnmadd",	0xfb900000,  0xfffc0000,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1, FSACC}},
{ "fnmsub",	0xfb940000,  0xfffc0000,  0,    FMT_D10,AM33_2,  {FSN3, FSN2, FSN1, FSACC}},

/* UDF instructions.  */
{ "udf00",	0xf600,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf00",	0xf90000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf00",	0xfb000000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf00",	0xfd000000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf01",	0xf610,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf01",	0xf91000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf01",	0xfb100000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf01",	0xfd100000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf02",	0xf620,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf02",	0xf92000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf02",	0xfb200000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf02",	0xfd200000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf03",	0xf630,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf03",	0xf93000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf03",	0xfb300000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf03",	0xfd300000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf04",	0xf640,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf04",	0xf94000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf04",	0xfb400000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf04",	0xfd400000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf05",	0xf650,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf05",	0xf95000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf05",	0xfb500000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf05",	0xfd500000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf06",	0xf660,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf06",	0xf96000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf06",	0xfb600000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf06",	0xfd600000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf07",	0xf670,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf07",	0xf97000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf07",	0xfb700000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf07",	0xfd700000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf08",	0xf680,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf08",	0xf98000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf08",	0xfb800000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf08",	0xfd800000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf09",	0xf690,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf09",	0xf99000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf09",	0xfb900000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf09",	0xfd900000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf10",	0xf6a0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf10",	0xf9a000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf10",	0xfba00000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf10",	0xfda00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf11",	0xf6b0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf11",	0xf9b000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf11",	0xfbb00000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf11",	0xfdb00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf12",	0xf6c0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf12",	0xf9c000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf12",	0xfbc00000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf12",	0xfdc00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf13",	0xf6d0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf13",	0xf9d000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf13",	0xfbd00000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf13",	0xfdd00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf14",	0xf6e0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf14",	0xf9e000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf14",	0xfbe00000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf14",	0xfde00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf15",	0xf6f0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf15",	0xf9f000,    0xfffc00,    0,    FMT_D1, 0,	{SIMM8, DN0}},
{ "udf15",	0xfbf00000,  0xfffc0000,  0,    FMT_D2, 0,      {SIMM16, DN0}},
{ "udf15",	0xfdf00000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udf20",	0xf500,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf21",	0xf510,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf22",	0xf520,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf23",	0xf530,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf24",	0xf540,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf25",	0xf550,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf26",	0xf560,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf27",	0xf570,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf28",	0xf580,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf29",	0xf590,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf30",	0xf5a0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf31",	0xf5b0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf32",	0xf5c0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf33",	0xf5d0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf34",	0xf5e0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udf35",	0xf5f0,	     0xfff0,	  0,    FMT_D0, 0,	{DM1, DN0}},
{ "udfu00",	0xf90400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu00",	0xfb040000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu00",	0xfd040000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu01",	0xf91400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu01",	0xfb140000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu01",	0xfd140000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu02",	0xf92400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu02",	0xfb240000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu02",	0xfd240000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu03",	0xf93400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu03",	0xfb340000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu03",	0xfd340000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu04",	0xf94400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu04",	0xfb440000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu04",	0xfd440000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu05",	0xf95400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu05",	0xfb540000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu05",	0xfd540000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu06",	0xf96400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu06",	0xfb640000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu06",	0xfd640000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu07",	0xf97400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu07",	0xfb740000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu07",	0xfd740000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu08",	0xf98400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu08",	0xfb840000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu08",	0xfd840000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu09",	0xf99400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu09",	0xfb940000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu09",	0xfd940000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu10",	0xf9a400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu10",	0xfba40000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu10",	0xfda40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu11",	0xf9b400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu11",	0xfbb40000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu11",	0xfdb40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu12",	0xf9c400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu12",	0xfbc40000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu12",	0xfdc40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu13",	0xf9d400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu13",	0xfbd40000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu13",	0xfdd40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu14",	0xf9e400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu14",	0xfbe40000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu14",	0xfde40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},
{ "udfu15",	0xf9f400,    0xfffc00,    0,    FMT_D1, 0,	{IMM8, DN0}},
{ "udfu15",	0xfbf40000,  0xfffc0000,  0,    FMT_D2, 0,      {IMM16, DN0}},
{ "udfu15",	0xfdf40000,  0xfffc0000,  0,    FMT_D4, 0,	{IMM32, DN0}},

{ "putx",	0xf500,	     0xfff0,	  0,    FMT_D0, AM30,	{DN01}},
{ "getx",	0xf6f0,	     0xfff0,	  0,    FMT_D0, AM30,	{DN01}},
{ "mulq",	0xf600,	     0xfff0,	  0,    FMT_D0, AM30,	{DM1, DN0}},
{ "mulq",	0xf90000,    0xfffc00,    0,    FMT_D1, AM30,	{SIMM8, DN0}},
{ "mulq",	0xfb000000,  0xfffc0000,  0,    FMT_D2, AM30,	{SIMM16, DN0}},
{ "mulq",	0xfd000000,  0xfffc0000,  0,    FMT_D4, AM30,	{IMM32, DN0}},
{ "mulqu",	0xf610,	     0xfff0,	  0,    FMT_D0, AM30,	{DM1, DN0}},
{ "mulqu",	0xf91400,    0xfffc00,    0,    FMT_D1, AM30,	{SIMM8, DN0}},
{ "mulqu",	0xfb140000,  0xfffc0000,  0,    FMT_D2, AM30,	{SIMM16, DN0}},
{ "mulqu",	0xfd140000,  0xfffc0000,  0,    FMT_D4, AM30,	{IMM32, DN0}},
{ "sat16",	0xf640,	     0xfff0,	  0,    FMT_D0, AM30,	{DM1, DN0}},
{ "sat16",	0xf9ab00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},

{ "sat24",	0xf650,	     0xfff0,	  0,    FMT_D0, AM30,	{DM1, DN0}},
{ "sat24",	0xfbaf0000,  0xffff00ff,  0,    FMT_D7, AM33,	{RM2, RN0}},

{ "bsch",	0xfbff0000,  0xffff000f,  0,    FMT_D7, AM33,	{RM2, RN0, RD2}},
{ "bsch",	0xf670,	     0xfff0,	  0,    FMT_D0, AM30,	{DM1, DN0}},
{ "bsch",	0xf9fb00,    0xffff00,    0,    FMT_D6, AM33,	{RM2, RN0}},

/* Extension.  We need some instruction to trigger "emulated syscalls"
   for our simulator.  */
{ "syscall",	0xf0e0,	     0xfff0,	  0,    FMT_D0, AM33,	{IMM4}},
{ "syscall",    0xf0c0,      0xffff,      0,    FMT_D0, 0,	{UNUSED}},

/* Extension.  When talking to the simulator, gdb requires some instruction
   that will trigger a "breakpoint" (really just an instruction that isn't
   otherwise used by the tools.  This instruction must be the same size
   as the smallest instruction on the target machine.  In the case of the
   mn10x00 the "break" instruction must be one byte.  0xff is available on
   both mn10x00 architectures.  */
{ "break",	0xff,	     0xff,	  0,    FMT_S0, 0,	{UNUSED}},

{ "add_add",	0xf7000000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_add",	0xf7100000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "add_add",	0xf7040000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_add",	0xf7140000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "add_sub",	0xf7200000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_sub",	0xf7300000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "add_sub",	0xf7240000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_sub",	0xf7340000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "add_cmp",	0xf7400000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_cmp",	0xf7500000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "add_cmp",	0xf7440000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_cmp",	0xf7540000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "add_mov",	0xf7600000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_mov",	0xf7700000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "add_mov",	0xf7640000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_mov",	0xf7740000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "add_asr",	0xf7800000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_asr",	0xf7900000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "add_asr",	0xf7840000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_asr",	0xf7940000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "add_lsr",	0xf7a00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_lsr",	0xf7b00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "add_lsr",	0xf7a40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_lsr",	0xf7b40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "add_asl",	0xf7c00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "add_asl",	0xf7d00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "add_asl",	0xf7c40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "add_asl",	0xf7d40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "cmp_add",	0xf7010000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "cmp_add",	0xf7110000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "cmp_add",	0xf7050000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "cmp_add",	0xf7150000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "cmp_sub",	0xf7210000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "cmp_sub",	0xf7310000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "cmp_sub",	0xf7250000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "cmp_sub",	0xf7350000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "cmp_mov",	0xf7610000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "cmp_mov",	0xf7710000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "cmp_mov",	0xf7650000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "cmp_mov",	0xf7750000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "cmp_asr",	0xf7810000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "cmp_asr",	0xf7910000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "cmp_asr",	0xf7850000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "cmp_asr",	0xf7950000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "cmp_lsr",	0xf7a10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "cmp_lsr",	0xf7b10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "cmp_lsr",	0xf7a50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "cmp_lsr",	0xf7b50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "cmp_asl",	0xf7c10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "cmp_asl",	0xf7d10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "cmp_asl",	0xf7c50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "cmp_asl",	0xf7d50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "sub_add",	0xf7020000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_add",	0xf7120000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sub_add",	0xf7060000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_add",	0xf7160000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "sub_sub",	0xf7220000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_sub",	0xf7320000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sub_sub",	0xf7260000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_sub",	0xf7360000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "sub_cmp",	0xf7420000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_cmp",	0xf7520000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sub_cmp",	0xf7460000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_cmp",	0xf7560000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "sub_mov",	0xf7620000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_mov",	0xf7720000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sub_mov",	0xf7660000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_mov",	0xf7760000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "sub_asr",	0xf7820000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_asr",	0xf7920000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "sub_asr",	0xf7860000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_asr",	0xf7960000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "sub_lsr",	0xf7a20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_lsr",	0xf7b20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "sub_lsr",	0xf7a60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_lsr",	0xf7b60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "sub_asl",	0xf7c20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sub_asl",	0xf7d20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "sub_asl",	0xf7c60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "sub_asl",	0xf7d60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "mov_add",	0xf7030000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_add",	0xf7130000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "mov_add",	0xf7070000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_add",	0xf7170000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "mov_sub",	0xf7230000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_sub",	0xf7330000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "mov_sub",	0xf7270000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_sub",	0xf7370000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "mov_cmp",	0xf7430000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_cmp",	0xf7530000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "mov_cmp",	0xf7470000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_cmp",	0xf7570000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "mov_mov",	0xf7630000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_mov",	0xf7730000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "mov_mov",	0xf7670000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_mov",	0xf7770000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, SIMM4_2, RN0}},
{ "mov_asr",	0xf7830000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_asr",	0xf7930000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "mov_asr",	0xf7870000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_asr",	0xf7970000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "mov_lsr",	0xf7a30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_lsr",	0xf7b30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "mov_lsr",	0xf7a70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_lsr",	0xf7b70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "mov_asl",	0xf7c30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "mov_asl",	0xf7d30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "mov_asl",	0xf7c70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, RM2, RN0}},
{ "mov_asl",	0xf7d70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_6, RN4, IMM4_2, RN0}},
{ "and_add",	0xf7080000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_add",	0xf7180000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "and_sub",	0xf7280000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_sub",	0xf7380000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "and_cmp",	0xf7480000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_cmp",	0xf7580000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "and_mov",	0xf7680000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_mov",	0xf7780000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "and_asr",	0xf7880000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_asr",	0xf7980000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "and_lsr",	0xf7a80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_lsr",	0xf7b80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "and_asl",	0xf7c80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "and_asl",	0xf7d80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "dmach_add",	0xf7090000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_add",	0xf7190000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "dmach_sub",	0xf7290000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_sub",	0xf7390000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "dmach_cmp",	0xf7490000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_cmp",	0xf7590000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "dmach_mov",	0xf7690000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_mov",	0xf7790000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "dmach_asr",	0xf7890000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_asr",	0xf7990000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "dmach_lsr",	0xf7a90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_lsr",	0xf7b90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "dmach_asl",	0xf7c90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "dmach_asl",	0xf7d90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "xor_add",	0xf70a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_add",	0xf71a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "xor_sub",	0xf72a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_sub",	0xf73a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "xor_cmp",	0xf74a0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_cmp",	0xf75a0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "xor_mov",	0xf76a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_mov",	0xf77a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "xor_asr",	0xf78a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_asr",	0xf79a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "xor_lsr",	0xf7aa0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_lsr",	0xf7ba0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "xor_asl",	0xf7ca0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "xor_asl",	0xf7da0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "swhw_add",	0xf70b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_add",	0xf71b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "swhw_sub",	0xf72b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_sub",	0xf73b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "swhw_cmp",	0xf74b0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_cmp",	0xf75b0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "swhw_mov",	0xf76b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_mov",	0xf77b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "swhw_asr",	0xf78b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_asr",	0xf79b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "swhw_lsr",	0xf7ab0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_lsr",	0xf7bb0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "swhw_asl",	0xf7cb0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "swhw_asl",	0xf7db0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "or_add",	0xf70c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_add",	0xf71c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "or_sub",	0xf72c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_sub",	0xf73c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "or_cmp",	0xf74c0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_cmp",	0xf75c0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "or_mov",	0xf76c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_mov",	0xf77c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "or_asr",	0xf78c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_asr",	0xf79c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "or_lsr",	0xf7ac0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_lsr",	0xf7bc0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "or_asl",	0xf7cc0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "or_asl",	0xf7dc0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "sat16_add",	0xf70d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_add",	0xf71d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sat16_sub",	0xf72d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_sub",	0xf73d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sat16_cmp",	0xf74d0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_cmp",	0xf75d0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sat16_mov",	0xf76d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_mov",	0xf77d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, SIMM4_2, RN0}},
{ "sat16_asr",	0xf78d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_asr",	0xf79d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "sat16_lsr",	0xf7ad0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_lsr",	0xf7bd0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
{ "sat16_asl",	0xf7cd0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, RM2, RN0}},
{ "sat16_asl",	0xf7dd0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM6, RN4, IMM4_2, RN0}},
/* Ugh.  Synthetic instructions.  */
{ "add_and",	0xf7080000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "add_and",	0xf7180000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "add_dmach",	0xf7090000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "add_dmach",	0xf7190000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "add_or",	0xf70c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "add_or",	0xf71c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "add_sat16",	0xf70d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "add_sat16",	0xf71d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "add_swhw",	0xf70b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "add_swhw",	0xf71b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "add_xor",	0xf70a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "add_xor",	0xf71a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "asl_add",	0xf7c00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_add",	0xf7d00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_add",	0xf7c40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asl_add",	0xf7d40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asl_and",	0xf7c80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_and",	0xf7d80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_cmp",	0xf7c10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_cmp",	0xf7d10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4, }},
{ "asl_cmp",	0xf7c50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asl_cmp",	0xf7d50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asl_dmach",	0xf7c90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_dmach",	0xf7d90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_mov",	0xf7c30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_mov",	0xf7d30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_mov",	0xf7c70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asl_mov",	0xf7d70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asl_or",	0xf7cc0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_or",	0xf7dc0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_sat16",	0xf7cd0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_sat16",	0xf7dd0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_sub",	0xf7c20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_sub",	0xf7d20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_sub",	0xf7c60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asl_sub",	0xf7d60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asl_swhw",	0xf7cb0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_swhw",	0xf7db0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asl_xor",	0xf7ca0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asl_xor",	0xf7da0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_add",	0xf7800000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_add",	0xf7900000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_add",	0xf7840000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asr_add",	0xf7940000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asr_and",	0xf7880000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_and",	0xf7980000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_cmp",	0xf7810000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_cmp",	0xf7910000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4, }},
{ "asr_cmp",	0xf7850000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asr_cmp",	0xf7950000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asr_dmach",	0xf7890000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_dmach",	0xf7990000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_mov",	0xf7830000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_mov",	0xf7930000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_mov",	0xf7870000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asr_mov",	0xf7970000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asr_or",	0xf78c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_or",	0xf79c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_sat16",	0xf78d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_sat16",	0xf79d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_sub",	0xf7820000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_sub",	0xf7920000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_sub",	0xf7860000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "asr_sub",	0xf7960000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "asr_swhw",	0xf78b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_swhw",	0xf79b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "asr_xor",	0xf78a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "asr_xor",	0xf79a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "cmp_and",	0xf7480000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "cmp_and",	0xf7580000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "cmp_dmach",	0xf7490000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "cmp_dmach",	0xf7590000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "cmp_or",	0xf74c0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "cmp_or",	0xf75c0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "cmp_sat16",	0xf74d0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "cmp_sat16",	0xf75d0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "cmp_swhw",	0xf74b0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "cmp_swhw",	0xf75b0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "cmp_xor",	0xf74a0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "cmp_xor",	0xf75a0000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "lsr_add",	0xf7a00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_add",	0xf7b00000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_add",	0xf7a40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "lsr_add",	0xf7b40000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "lsr_and",	0xf7a80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_and",	0xf7b80000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_cmp",	0xf7a10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_cmp",	0xf7b10000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4, }},
{ "lsr_cmp",	0xf7a50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "lsr_cmp",	0xf7b50000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "lsr_dmach",	0xf7a90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_dmach",	0xf7b90000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_mov",	0xf7a30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_mov",	0xf7b30000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_mov",	0xf7a70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "lsr_mov",	0xf7b70000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "lsr_or",	0xf7ac0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_or",	0xf7bc0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_sat16",	0xf7ad0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_sat16",	0xf7bd0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_sub",	0xf7a20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_sub",	0xf7b20000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_sub",	0xf7a60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, SIMM4_6, RN4}},
{ "lsr_sub",	0xf7b60000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, SIMM4_6, RN4}},
{ "lsr_swhw",	0xf7ab0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_swhw",	0xf7bb0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "lsr_xor",	0xf7aa0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "lsr_xor",	0xf7ba0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {IMM4_2, RN0, RM6, RN4}},
{ "mov_and",	0xf7680000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "mov_and",	0xf7780000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "mov_dmach",	0xf7690000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "mov_dmach",	0xf7790000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "mov_or",	0xf76c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "mov_or",	0xf77c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "mov_sat16",	0xf76d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "mov_sat16",	0xf77d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "mov_swhw",	0xf76b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "mov_swhw",	0xf77b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "mov_xor",	0xf76a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "mov_xor",	0xf77a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "sub_and",	0xf7280000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "sub_and",	0xf7380000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "sub_dmach",	0xf7290000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "sub_dmach",	0xf7390000,  0xffff0000,  0x0,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "sub_or",	0xf72c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "sub_or",	0xf73c0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "sub_sat16",	0xf72d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "sub_sat16",	0xf73d0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "sub_swhw",	0xf72b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "sub_swhw",	0xf73b0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "sub_xor",	0xf72a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {RM2, RN0, RM6, RN4}},
{ "sub_xor",	0xf73a0000,  0xffff0000,  0xa,  FMT_D10, AM33,	 {SIMM4_2, RN0, RM6, RN4}},
{ "mov_llt",	0xf7e00000,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lgt",	0xf7e00001,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lge",	0xf7e00002,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lle",	0xf7e00003,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lcs",	0xf7e00004,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lhi",	0xf7e00005,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lcc",	0xf7e00006,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lls",	0xf7e00007,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_leq",	0xf7e00008,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lne",	0xf7e00009,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "mov_lra",	0xf7e0000a,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "llt_mov",	0xf7e00000,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lgt_mov",	0xf7e00001,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lge_mov",	0xf7e00002,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lle_mov",	0xf7e00003,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lcs_mov",	0xf7e00004,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lhi_mov",	0xf7e00005,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lcc_mov",	0xf7e00006,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lls_mov",	0xf7e00007,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "leq_mov",	0xf7e00008,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lne_mov",	0xf7e00009,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
{ "lra_mov",	0xf7e0000a,  0xffff000f,  0x22, FMT_D10, AM33,	 {MEMINC2 (RN4,SIMM4_2), RM6}},
 
{ 0, 0, 0, 0, 0, 0, {0}},

} ;

const int mn10300_num_opcodes =
  sizeof (mn10300_opcodes) / sizeof (mn10300_opcodes[0]);


