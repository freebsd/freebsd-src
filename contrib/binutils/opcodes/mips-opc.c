/* mips.h.  Mips opcode list for GDB, the GNU debugger.
   Copyright 1993, 1994, 1995, 1996, 1997, 1998 Free Software Foundation, Inc.
   Contributed by Ralph Campbell and OSF
   Commented and modified by Ian Lance Taylor, Cygnus Support

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

#include <stdio.h>
#include "sysdep.h"
#include "opcode/mips.h"

/* Short hand so the lines aren't too long.  */

#define LDD     INSN_LOAD_MEMORY_DELAY
#define LCD	INSN_LOAD_COPROC_DELAY
#define UBD     INSN_UNCOND_BRANCH_DELAY
#define CBD	INSN_COND_BRANCH_DELAY
#define COD     INSN_COPROC_MOVE_DELAY
#define CLD	INSN_COPROC_MEMORY_DELAY
#define CBL	INSN_COND_BRANCH_LIKELY
#define TRAP	INSN_TRAP
#define SM	INSN_STORE_MEMORY

#define WR_d    INSN_WRITE_GPR_D
#define WR_t    INSN_WRITE_GPR_T
#define WR_31   INSN_WRITE_GPR_31       
#define WR_D    INSN_WRITE_FPR_D        
#define WR_T	INSN_WRITE_FPR_T
#define WR_S	INSN_WRITE_FPR_S
#define RD_s    INSN_READ_GPR_S         
#define RD_b    INSN_READ_GPR_S         
#define RD_t    INSN_READ_GPR_T         
#define RD_S    INSN_READ_FPR_S         
#define RD_T    INSN_READ_FPR_T         
#define RD_R	INSN_READ_FPR_R
#define WR_CC	INSN_WRITE_COND_CODE
#define RD_CC	INSN_READ_COND_CODE
#define RD_C0   INSN_COP
#define RD_C1	INSN_COP
#define RD_C2   INSN_COP
#define RD_C3   INSN_COP
#define WR_C0   INSN_COP
#define WR_C1	INSN_COP
#define WR_C2   INSN_COP
#define WR_C3   INSN_COP

#define WR_HI	INSN_WRITE_HI
#define RD_HI	INSN_READ_HI
#define MOD_HI  WR_HI|RD_HI

#define WR_LO	INSN_WRITE_LO
#define RD_LO	INSN_READ_LO
#define MOD_LO  WR_LO|RD_LO

#define WR_HILO WR_HI|WR_LO
#define RD_HILO RD_HI|RD_LO
#define MOD_HILO WR_HILO|RD_HILO

#define IS_M    INSN_MULT

#define I1	INSN_ISA1
#define I2	INSN_ISA2
#define I3	INSN_ISA3
#define I4	INSN_ISA4
#define I5	INSN_ISA5
#define P3	INSN_4650
#define L1	INSN_4010
#define V1      INSN_4100
#define T3      INSN_3900

#define G1      (T3                   \
                 )

#define G2      (T3                   \
                 )

#define G3 (I4             \
            )

#define G6      INSN_GP32

#define M1      0
#define M2      0

/* The order of overloaded instructions matters.  Label arguments and
   register arguments look the same. Instructions that can have either
   for arguments must apear in the correct order in this table for the
   assembler to pick the right one. In other words, entries with
   immediate operands must apear after the same instruction with
   registers.

   Many instructions are short hand for other instructions (i.e., The
   jal <register> instruction is short for jalr <register>).  */

const struct mips_opcode mips_builtin_opcodes[] = {
/* These instructions appear first so that the disassembler will find
   them first.  The assemblers uses a hash table based on the
   instruction name anyhow.  */
/* name,	args,	mask,		match,	pinfo */
{"nop",     "",		0x00000000, 0xffffffff,	0,		I1	},
{"li",      "t,j",      0x24000000, 0xffe00000, WR_t,		I1	}, /* addiu */
{"li",	    "t,i",	0x34000000, 0xffe00000, WR_t,		I1	}, /* ori */
{"li",      "t,I",	0,    (int) M_LI,	INSN_MACRO,	I1	},
{"move",    "d,s",	0x00000025, 0xfc1f07ff,	WR_d|RD_s,	I1|G6	},/* or */
{"move",    "d,s",	0x0000002d, 0xfc1f07ff, WR_d|RD_s,	I3	},/* daddu */
{"move",    "d,s",	0x00000021, 0xfc1f07ff, WR_d|RD_s,	I1	},/* addu */
{"move",    "d,s",	0x00000025, 0xfc1f07ff,	WR_d|RD_s,	I1	},/* or */
{"b",       "p",	0x10000000, 0xffff0000,	UBD,		I1	},/* beq 0,0 */
{"b",       "p",	0x04010000, 0xffff0000,	UBD,		I1	},/* bgez 0 */
{"bal",     "p",	0x04110000, 0xffff0000,	UBD|WR_31,	I1	},/* bgezal 0*/

{"abs",     "d,v",	0,    (int) M_ABS,	INSN_MACRO,	I1	},
{"abs.s",   "D,V",	0x46000005, 0xffff003f,	WR_D|RD_S|FP_S,	I1	},
{"abs.d",   "D,V",	0x46200005, 0xffff003f,	WR_D|RD_S|FP_D,	I1	},
{"abs.ps",  "D,V",	0x46c00005, 0xffff003f,	WR_D|RD_S|FP_D,	I5	},
{"add",     "d,v,t",	0x00000020, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"add",     "t,r,I",	0,    (int) M_ADD_I,	INSN_MACRO,	I1	},
{"add.s",   "D,V,T",	0x46000000, 0xffe0003f,	WR_D|RD_S|RD_T|FP_S,	I1},
{"add.d",   "D,V,T",	0x46200000, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I1},
{"add.ps",  "D,V,T",	0x46c00000, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5},
{"addi",    "t,r,j",	0x20000000, 0xfc000000,	WR_t|RD_s,	I1	},
{"addiu",   "t,r,j",	0x24000000, 0xfc000000,	WR_t|RD_s,	I1	},
{"addu",    "d,v,t",	0x00000021, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"addu",    "t,r,I",	0,    (int) M_ADDU_I,	INSN_MACRO,	I1	},
{"alnv.ps", "D,V,T,s",	0x4c00001e, 0xfc00003f,	WR_D|RD_S|RD_T|FP_D,	I5},
{"and",     "d,v,t",	0x00000024, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"and",     "t,r,I",	0,    (int) M_AND_I,	INSN_MACRO,	I1	},
{"andi",    "t,r,i",	0x30000000, 0xfc000000,	WR_t|RD_s,	I1	},
/* b is at the top of the table.  */
/* bal is at the top of the table.  */
{"bc0f",    "p",	0x41000000, 0xffff0000,	CBD|RD_CC,	I1	},
{"bc0fl",   "p",	0x41020000, 0xffff0000,	CBL|RD_CC,	I2|T3	},
{"bc1f",    "p",	0x45000000, 0xffff0000,	CBD|RD_CC|FP_S,	I1|M1	},
{"bc1f",    "N,p",	0x45000000, 0xffe30000,	CBD|RD_CC|FP_S,	I4|M1	},
{"bc1fl",   "p",	0x45020000, 0xffff0000,	CBL|RD_CC|FP_S,	I2|T3|M1},
{"bc1fl",   "N,p",	0x45020000, 0xffe30000,	CBL|RD_CC|FP_S,	I4|M1	},
{"bc2f",    "p",	0x49000000, 0xffff0000,	CBD|RD_CC,	I1	},
{"bc2fl",   "p",	0x49020000, 0xffff0000,	CBL|RD_CC,	I2|T3	},
{"bc3f",    "p",	0x4d000000, 0xffff0000,	CBD|RD_CC,	I1	},
{"bc3fl",   "p",	0x4d020000, 0xffff0000,	CBL|RD_CC,	I2|T3	},
{"bc0t",    "p",	0x41010000, 0xffff0000,	CBD|RD_CC,	I1	},
{"bc0tl",   "p",	0x41030000, 0xffff0000,	CBL|RD_CC,	I2|T3	},
{"bc1t",    "p",	0x45010000, 0xffff0000,	CBD|RD_CC|FP_S,	I1	},
{"bc1t",    "N,p",	0x45010000, 0xffe30000,	CBD|RD_CC|FP_S,	I4	},
{"bc1tl",   "p",	0x45030000, 0xffff0000,	CBL|RD_CC|FP_S,	I2|T3	},
{"bc1tl",   "N,p",	0x45030000, 0xffe30000,	CBL|RD_CC|FP_S,	I4	},
{"bc2t",    "p",	0x49010000, 0xffff0000,	CBD|RD_CC,	I1	},
{"bc2tl",   "p",	0x49030000, 0xffff0000,	CBL|RD_CC,	I2|T3	},
{"bc3t",    "p",	0x4d010000, 0xffff0000,	CBD|RD_CC,	I1	},
{"bc3tl",   "p",	0x4d030000, 0xffff0000,	CBL|RD_CC,	I2|T3	},
{"beqz",    "s,p",	0x10000000, 0xfc1f0000,	CBD|RD_s,	I1	},
{"beqzl",   "s,p",	0x50000000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"beq",     "s,t,p",	0x10000000, 0xfc000000,	CBD|RD_s|RD_t,	I1	},
{"beq",     "s,I,p",	0,    (int) M_BEQ_I,	INSN_MACRO,	I1	},
{"beql",    "s,t,p",	0x50000000, 0xfc000000,	CBL|RD_s|RD_t,	I2|T3	},
{"beql",    "s,I,p",	0,    (int) M_BEQL_I,	INSN_MACRO,	I2	},
{"bge",     "s,t,p",	0,    (int) M_BGE,	INSN_MACRO,	I1	},
{"bge",     "s,I,p",	0,    (int) M_BGE_I,	INSN_MACRO,	I1	},
{"bgel",    "s,t,p",	0,    (int) M_BGEL,	INSN_MACRO,	I2	},
{"bgel",    "s,I,p",	0,    (int) M_BGEL_I,	INSN_MACRO,	I2	},
{"bgeu",    "s,t,p",	0,    (int) M_BGEU,	INSN_MACRO,	I1	},
{"bgeu",    "s,I,p",	0,    (int) M_BGEU_I,	INSN_MACRO,	I1	},
{"bgeul",   "s,t,p",	0,    (int) M_BGEUL,	INSN_MACRO,	I2	},
{"bgeul",   "s,I,p",	0,    (int) M_BGEUL_I,	INSN_MACRO,	I2	},
{"bgez",    "s,p",	0x04010000, 0xfc1f0000,	CBD|RD_s,	I1	},
{"bgezl",   "s,p",	0x04030000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"bgezal",  "s,p",	0x04110000, 0xfc1f0000,	CBD|RD_s|WR_31,	I1	},
{"bgezall", "s,p",	0x04130000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"bgt",     "s,t,p",	0,    (int) M_BGT,	INSN_MACRO,	I1	},
{"bgt",     "s,I,p",	0,    (int) M_BGT_I,	INSN_MACRO,	I1	},
{"bgtl",    "s,t,p",	0,    (int) M_BGTL,	INSN_MACRO,	I2	},
{"bgtl",    "s,I,p",	0,    (int) M_BGTL_I,	INSN_MACRO,	I2	},
{"bgtu",    "s,t,p",	0,    (int) M_BGTU,	INSN_MACRO,	I1	},
{"bgtu",    "s,I,p",	0,    (int) M_BGTU_I,	INSN_MACRO,	I1	},
{"bgtul",   "s,t,p",	0,    (int) M_BGTUL,	INSN_MACRO,	I2	},
{"bgtul",   "s,I,p",	0,    (int) M_BGTUL_I,	INSN_MACRO,	I2	},
{"bgtz",    "s,p",	0x1c000000, 0xfc1f0000,	CBD|RD_s,	I1	},
{"bgtzl",   "s,p",	0x5c000000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"ble",     "s,t,p",	0,    (int) M_BLE,	INSN_MACRO,	I1	},
{"ble",     "s,I,p",	0,    (int) M_BLE_I,	INSN_MACRO,	I1	},
{"blel",    "s,t,p",	0,    (int) M_BLEL,	INSN_MACRO,	I2	},
{"blel",    "s,I,p",	0,    (int) M_BLEL_I,	INSN_MACRO,	I2	},
{"bleu",    "s,t,p",	0,    (int) M_BLEU,	INSN_MACRO,	I1	},
{"bleu",    "s,I,p",	0,    (int) M_BLEU_I,	INSN_MACRO,	I1	},
{"bleul",   "s,t,p",	0,    (int) M_BLEUL,	INSN_MACRO,	I2	},
{"bleul",   "s,I,p",	0,    (int) M_BLEUL_I,	INSN_MACRO,	I2	},
{"blez",    "s,p",	0x18000000, 0xfc1f0000,	CBD|RD_s,	I1	},
{"blezl",   "s,p",	0x58000000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"blt",     "s,t,p",	0,    (int) M_BLT,	INSN_MACRO,	I1	},
{"blt",     "s,I,p",	0,    (int) M_BLT_I,	INSN_MACRO,	I1	},
{"bltl",    "s,t,p",	0,    (int) M_BLTL,	INSN_MACRO,	I2	},
{"bltl",    "s,I,p",	0,    (int) M_BLTL_I,	INSN_MACRO,	I2	},
{"bltu",    "s,t,p",	0,    (int) M_BLTU,	INSN_MACRO,	I1	},
{"bltu",    "s,I,p",	0,    (int) M_BLTU_I,	INSN_MACRO,	I1	},
{"bltul",   "s,t,p",	0,    (int) M_BLTUL,	INSN_MACRO,	I2	},
{"bltul",   "s,I,p",	0,    (int) M_BLTUL_I,	INSN_MACRO,	I2	},
{"bltz",    "s,p",	0x04000000, 0xfc1f0000,	CBD|RD_s,	I1	},
{"bltzl",   "s,p",	0x04020000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"bltzal",  "s,p",	0x04100000, 0xfc1f0000,	CBD|RD_s|WR_31,	I1	},
{"bltzall", "s,p",	0x04120000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"bnez",    "s,p",	0x14000000, 0xfc1f0000,	CBD|RD_s,	I1	},
{"bnezl",   "s,p",	0x54000000, 0xfc1f0000,	CBL|RD_s,	I2|T3	},
{"bne",     "s,t,p",	0x14000000, 0xfc000000,	CBD|RD_s|RD_t,	I1	},
{"bne",     "s,I,p",	0,    (int) M_BNE_I,	INSN_MACRO,	I1	},
{"bnel",    "s,t,p",	0x54000000, 0xfc000000,	CBL|RD_s|RD_t, 	I2|T3	},
{"bnel",    "s,I,p",	0,    (int) M_BNEL_I,	INSN_MACRO,	I2	},
{"break",   "",		0x0000000d, 0xffffffff,	TRAP,		I1	},
{"break",   "c",	0x0000000d, 0xfc00ffff,	TRAP,		I1	},
{"break",   "c,q",	0x0000000d, 0xfc00003f,	TRAP,		I1	},
{"c.f.d",   "S,T",	0x46200030, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.f.d",   "M,S,T",	0x46200030, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.f.s",   "S,T",	0x46000030, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.f.s",   "M,S,T",	0x46000030, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.f.ps",  "S,T",	0x46c00030, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.f.ps",  "M,S,T",	0x46c00030, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.un.d",  "S,T",	0x46200031, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.un.d",  "M,S,T",	0x46200031, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.un.s",  "S,T",	0x46000031, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.un.s",  "M,S,T",	0x46000031, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.un.ps", "S,T",	0x46c00031, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.un.ps", "M,S,T",	0x46c00031, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.eq.d",  "S,T",	0x46200032, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.eq.d",  "M,S,T",	0x46200032, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.eq.s",  "S,T",	0x46000032, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.eq.s",  "M,S,T",	0x46000032, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.eq.ps", "S,T",	0x46c00032, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.eq.ps", "M,S,T",	0x46c00032, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ueq.d", "S,T",	0x46200033, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ueq.d", "M,S,T",	0x46200033, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ueq.s", "S,T",	0x46000033, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ueq.s", "M,S,T",	0x46000033, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ueq.ps","S,T",	0x46c00033, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ueq.ps","M,S,T",	0x46c00033, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.lt.s",  "S,T",	0x4600003c, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.lt.s",  "M,S,T",	0x4600003c, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.olt.d", "S,T",	0x46200034, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.olt.d", "M,S,T",	0x46200034, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.olt.s", "S,T",	0x46000034, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.olt.s", "M,S,T",	0x46000034, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.olt.ps","S,T",	0x46c00034, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.olt.ps","M,S,T",	0x46c00034, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ult.d", "S,T",	0x46200035, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ult.d", "M,S,T",	0x46200035, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ult.s", "S,T",	0x46000035, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ult.s", "M,S,T",	0x46000035, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ult.ps","S,T",	0x46c00035, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ult.ps","M,S,T",	0x46c00035, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.le.s",  "S,T",	0x4600003e, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.le.s",  "M,S,T",	0x4600003e, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ole.d", "S,T",	0x46200036, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ole.d", "M,S,T",	0x46200036, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ole.s", "S,T",	0x46000036, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ole.s", "M,S,T",	0x46000036, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ole.ps","S,T",	0x46c00036, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ole.ps","M,S,T",	0x46c00036, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ule.d", "S,T",	0x46200037, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ule.d", "M,S,T",	0x46200037, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ule.s", "S,T",	0x46000037, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ule.s", "M,S,T",	0x46000037, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ule.ps","S,T",	0x46c00037, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ule.ps","M,S,T",	0x46c00037, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.sf.d",  "S,T",	0x46200038, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.sf.d",  "M,S,T",	0x46200038, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.sf.s",  "S,T",	0x46000038, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.sf.s",  "M,S,T",	0x46000038, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.sf.ps", "S,T",	0x46c00038, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.sf.ps", "M,S,T",	0x46c00038, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ngle.d","S,T",	0x46200039, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ngle.d","M,S,T",	0x46200039, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ngle.s","S,T",	0x46000039, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ngle.s","M,S,T",	0x46000039, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ngle.ps","S,T",	0x46c00039, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ngle.ps","M,S,T",	0x46c00039, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.seq.d", "S,T",	0x4620003a, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.seq.d", "M,S,T",	0x4620003a, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.seq.s", "S,T",	0x4600003a, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.seq.s", "M,S,T",	0x4600003a, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.seq.ps","S,T",	0x46c0003a, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.seq.ps","M,S,T",	0x46c0003a, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ngl.d", "S,T",	0x4620003b, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ngl.d", "M,S,T",	0x4620003b, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ngl.s", "S,T",	0x4600003b, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ngl.s", "M,S,T",	0x4600003b, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ngl.ps","S,T",	0x46c0003b, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ngl.ps","M,S,T",	0x46c0003b, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.lt.d",  "S,T",	0x4620003c, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.lt.d",  "M,S,T",	0x4620003c, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.lt.ps", "S,T",	0x46c0003c, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.lt.ps", "M,S,T",	0x46c0003c, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.nge.d", "S,T",	0x4620003d, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.nge.d", "M,S,T",	0x4620003d, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.nge.s", "S,T",	0x4600003d, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.nge.s", "M,S,T",	0x4600003d, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.nge.ps","S,T",	0x46c0003d, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.nge.ps","M,S,T",	0x46c0003d, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.le.d",  "S,T",	0x4620003e, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.le.d",  "M,S,T",	0x4620003e, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.le.ps", "S,T",	0x46c0003e, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.le.ps", "M,S,T",	0x46c0003e, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ngt.d", "S,T",	0x4620003f, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I1	},
{"c.ngt.d", "M,S,T",	0x4620003f, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I4	},
{"c.ngt.s", "S,T",	0x4600003f, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_S,	I1	},
{"c.ngt.s", "M,S,T",	0x4600003f, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_S,	I4	},
{"c.ngt.ps","S,T",	0x46c0003f, 0xffe007ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"c.ngt.ps","M,S,T",	0x46c0003f, 0xffe000ff,	RD_S|RD_T|WR_CC|FP_D,	I5	},
{"cache",   "k,o(b)",	0xbc000000, 0xfc000000, RD_b,		I3|T3|M1	},
{"ceil.l.d", "D,S",	0x4620000a, 0xffff003f, WR_D|RD_S|FP_D,	I3	},
{"ceil.l.s", "D,S",	0x4600000a, 0xffff003f, WR_D|RD_S|FP_S,	I3	},
{"ceil.w.d", "D,S",	0x4620000e, 0xffff003f, WR_D|RD_S|FP_D,	I2	},
{"ceil.w.s", "D,S",	0x4600000e, 0xffff003f, WR_D|RD_S|FP_S,	I2	},
{"cfc0",    "t,G",	0x40400000, 0xffe007ff,	LCD|WR_t|RD_C0,	I1	},
{"cfc1",    "t,G",	0x44400000, 0xffe007ff,	LCD|WR_t|RD_C1|FP_S,	I1	},
{"cfc1",    "t,S",	0x44400000, 0xffe007ff,	LCD|WR_t|RD_C1|FP_S,	I1	},
{"cfc2",    "t,G",	0x48400000, 0xffe007ff,	LCD|WR_t|RD_C2,	I1	},
{"cfc3",    "t,G",	0x4c400000, 0xffe007ff,	LCD|WR_t|RD_C3,	I1	},
{"ctc0",    "t,G",	0x40c00000, 0xffe007ff,	COD|RD_t|WR_CC,	I1	},
{"ctc1",    "t,G",	0x44c00000, 0xffe007ff,	COD|RD_t|WR_CC|FP_S,	I1	},
{"ctc1",    "t,S",	0x44c00000, 0xffe007ff,	COD|RD_t|WR_CC|FP_S,	I1	},
{"ctc2",    "t,G",	0x48c00000, 0xffe007ff,	COD|RD_t|WR_CC,	I1	},
{"ctc3",    "t,G",	0x4cc00000, 0xffe007ff,	COD|RD_t|WR_CC,	I1	},
{"cvt.d.l", "D,S",	0x46a00021, 0xffff003f,	WR_D|RD_S|FP_D,	I3	},
{"cvt.d.s", "D,S",	0x46000021, 0xffff003f,	WR_D|RD_S|FP_D|FP_S,	I1	},
{"cvt.d.w", "D,S",	0x46800021, 0xffff003f,	WR_D|RD_S|FP_D,	I1	},
{"cvt.l.d", "D,S",	0x46200025, 0xffff003f,	WR_D|RD_S|FP_D,	I3	},
{"cvt.l.s", "D,S",	0x46000025, 0xffff003f,	WR_D|RD_S|FP_S,	I3	},
{"cvt.s.l", "D,S",	0x46a00020, 0xffff003f,	WR_D|RD_S|FP_S,	I3	},
{"cvt.s.d", "D,S",	0x46200020, 0xffff003f,	WR_D|RD_S|FP_S|FP_D,	I1	},
{"cvt.s.w", "D,S",	0x46800020, 0xffff003f,	WR_D|RD_S|FP_S,	I1	},
{"cvt.s.pl","D,S",	0x46c00028, 0xffff003f,	WR_D|RD_S|FP_S|FP_D,	I5	},
{"cvt.s.pu","D,S",	0x46c00020, 0xffff003f,	WR_D|RD_S|FP_S|FP_D,	I5	},
{"cvt.w.d", "D,S",	0x46200024, 0xffff003f,	WR_D|RD_S|FP_D,	I1	},
{"cvt.w.s", "D,S",	0x46000024, 0xffff003f,	WR_D|RD_S|FP_S,	I1	},
{"cvt.ps.s","D,V,T",	0x46000026, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5},
{"dabs",    "d,v",	0,    (int) M_DABS,	INSN_MACRO,	I3	},
{"dadd",    "d,v,t",	0x0000002c, 0xfc0007ff, WR_d|RD_s|RD_t,	I3	},
{"dadd",    "t,r,I",	0,    (int) M_DADD_I,	INSN_MACRO,	I3	},
{"daddi",   "t,r,j",	0x60000000, 0xfc000000, WR_t|RD_s,	I3	},
{"daddiu",  "t,r,j",	0x64000000, 0xfc000000, WR_t|RD_s,	I3	},
{"daddu",   "d,v,t",	0x0000002d, 0xfc0007ff, WR_d|RD_s|RD_t,	I3	},
{"daddu",   "t,r,I",	0,    (int) M_DADDU_I,	INSN_MACRO,	I3	},
/* dctr and dctw are used on the r5000.  */
{"dctr",    "o(b)",	0xbc050000, 0xfc1f0000, RD_b,	I3	},
{"dctw",    "o(b)",	0xbc090000, 0xfc1f0000, RD_b,	I3	},
{"deret",   "",         0x4200001f, 0xffffffff,    0,	G2|M1	},
/* For ddiv, see the comments about div.  */
{"ddiv",    "z,s,t",	0x0000001e, 0xfc00ffff, RD_s|RD_t|WR_HI|WR_LO,	I3	},
{"ddiv",    "d,v,t",	0,    (int) M_DDIV_3,	INSN_MACRO,	I3	},
{"ddiv",    "d,v,I",	0,    (int) M_DDIV_3I,	INSN_MACRO,	I3	},
/* For ddivu, see the comments about div.  */
{"ddivu",   "z,s,t",	0x0000001f, 0xfc00ffff, RD_s|RD_t|WR_HI|WR_LO,	I3	},
{"ddivu",   "d,v,t",	0,    (int) M_DDIVU_3,	INSN_MACRO,	I3	},
{"ddivu",   "d,v,I",	0,    (int) M_DDIVU_3I,	INSN_MACRO,	I3	},
/* The MIPS assembler treats the div opcode with two operands as
   though the first operand appeared twice (the first operand is both
   a source and a destination).  To get the div machine instruction,
   you must use an explicit destination of $0.  */
{"div",     "z,s,t",	0x0000001a, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,	I1	},
{"div",     "z,t",	0x0000001a, 0xffe0ffff,	RD_s|RD_t|WR_HI|WR_LO,	I1	},
{"div",     "d,v,t",	0,    (int) M_DIV_3,	INSN_MACRO,	I1	},
{"div",     "d,v,I",	0,    (int) M_DIV_3I,	INSN_MACRO,	I1	},
{"div.d",   "D,V,T",	0x46200003, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I1	},
{"div.s",   "D,V,T",	0x46000003, 0xffe0003f,	WR_D|RD_S|RD_T|FP_S,	I1	},
/* For divu, see the comments about div.  */
{"divu",    "z,s,t",	0x0000001b, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,	I1	},
{"divu",    "z,t",	0x0000001b, 0xffe0ffff,	RD_s|RD_t|WR_HI|WR_LO,	I1	},
{"divu",    "d,v,t",	0,    (int) M_DIVU_3,	INSN_MACRO,	I1	},
{"divu",    "d,v,I",	0,    (int) M_DIVU_3I,	INSN_MACRO,	I1	},
{"dla",     "t,o(b)",	0x64000000, 0xfc000000, WR_t|RD_s,	I3	}, /* daddiu */
{"dla",     "t,A(b)",	0,    (int) M_DLA_AB,	INSN_MACRO,	I3	},
{"dli",     "t,j",      0x24000000, 0xffe00000, WR_t,	I3	}, /* addiu */
{"dli",	    "t,i",	0x34000000, 0xffe00000, WR_t,	I3	}, /* ori */
{"dli",     "t,I",	0,    (int) M_DLI,	INSN_MACRO,	I3	},

{"dmadd16", "s,t",      0x00000029, 0xfc00ffff, RD_s|RD_t|WR_LO|RD_LO,	V1	},
{"dmfc0",   "t,G",	0x40200000, 0xffe007ff, LCD|WR_t|RD_C0,	I3	},
{"dmtc0",   "t,G",	0x40a00000, 0xffe007ff, COD|RD_t|WR_C0|WR_CC,	I3	},
{"dmfc1",   "t,S",	0x44200000, 0xffe007ff, LCD|WR_t|RD_S|FP_S,	I3	},
{"dmtc1",   "t,S",	0x44a00000, 0xffe007ff, COD|RD_t|WR_S|FP_S,	I3	},
{"dmfc2",   "t,S",	0x48200000, 0xffe007ff, LCD|WR_t|RD_S|FP_S,	I3	},
{"dmtc2",   "t,S",	0x48a00000, 0xffe007ff, COD|RD_t|WR_S|FP_S,	I3	},
{"dmul",    "d,v,t",	0,    (int) M_DMUL,	INSN_MACRO,	I3	},
{"dmul",    "d,v,I",	0,    (int) M_DMUL_I,	INSN_MACRO,	I3	},
{"dmulo",   "d,v,t",	0,    (int) M_DMULO,	INSN_MACRO,	I3	},
{"dmulo",   "d,v,I",	0,    (int) M_DMULO_I,	INSN_MACRO,	I3	},
{"dmulou",  "d,v,t",	0,    (int) M_DMULOU,	INSN_MACRO,	I3	},
{"dmulou",  "d,v,I",	0,    (int) M_DMULOU_I,	INSN_MACRO,	I3	},
{"dmult",   "s,t",	0x0000001c, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,	    I3},
{"dmultu",  "s,t",	0x0000001d, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,      I3},
{"dneg",    "d,w",	0x0000002e, 0xffe007ff,	WR_d|RD_t,	I3	}, /* dsub 0 */
{"dnegu",   "d,w",	0x0000002f, 0xffe007ff,	WR_d|RD_t,	I3	}, /* dsubu 0*/
{"drem",    "z,s,t",	0x0000001e, 0xfc00ffff, RD_s|RD_t|WR_HI|WR_LO,	I3	},
{"drem",    "d,v,t",	3,    (int) M_DREM_3,	INSN_MACRO,	I3	},
{"drem",    "d,v,I",	3,    (int) M_DREM_3I,	INSN_MACRO,	I3	},
{"dremu",   "z,s,t",	0x0000001f, 0xfc00ffff, RD_s|RD_t|WR_HI|WR_LO,	I3	},
{"dremu",   "d,v,t",	3,    (int) M_DREMU_3,	INSN_MACRO,	I3	},
{"dremu",   "d,v,I",	3,    (int) M_DREMU_3I,	INSN_MACRO,	I3	},
{"dsllv",   "d,t,s",	0x00000014, 0xfc0007ff,	WR_d|RD_t|RD_s,	I3	},
{"dsll32",  "d,w,<",	0x0000003c, 0xffe0003f, WR_d|RD_t,	I3	},
{"dsll",    "d,w,s",	0x00000014, 0xfc0007ff,	WR_d|RD_t|RD_s,	I3	}, /* dsllv */
{"dsll",    "d,w,>",	0x0000003c, 0xffe0003f, WR_d|RD_t,	I3	}, /* dsll32 */
{"dsll",    "d,w,<",	0x00000038, 0xffe0003f,	WR_d|RD_t,	I3	},
{"dsrav",   "d,t,s",	0x00000017, 0xfc0007ff,	WR_d|RD_t|RD_s,	I3	},
{"dsra32",  "d,w,<",	0x0000003f, 0xffe0003f, WR_d|RD_t,	I3	},
{"dsra",    "d,w,s",	0x00000017, 0xfc0007ff,	WR_d|RD_t|RD_s,	I3	}, /* dsrav */
{"dsra",    "d,w,>",	0x0000003f, 0xffe0003f, WR_d|RD_t,	I3	}, /* dsra32 */
{"dsra",    "d,w,<",	0x0000003b, 0xffe0003f,	WR_d|RD_t,	I3	},
{"dsrlv",   "d,t,s",	0x00000016, 0xfc0007ff,	WR_d|RD_t|RD_s,	I3	},
{"dsrl32",  "d,w,<",	0x0000003e, 0xffe0003f, WR_d|RD_t,	I3	},
{"dsrl",    "d,w,s",	0x00000016, 0xfc0007ff,	WR_d|RD_t|RD_s,	I3	}, /* dsrlv */
{"dsrl",    "d,w,>",	0x0000003e, 0xffe0003f, WR_d|RD_t,	I3	}, /* dsrl32 */
{"dsrl",    "d,w,<",	0x0000003a, 0xffe0003f,	WR_d|RD_t,	I3	},
{"dsub",    "d,v,t",	0x0000002e, 0xfc0007ff,	WR_d|RD_s|RD_t,	I3	},
{"dsub",    "d,v,I",	0,    (int) M_DSUB_I,	INSN_MACRO,	I3	},
{"dsubu",   "d,v,t",	0x0000002f, 0xfc0007ff,	WR_d|RD_s|RD_t,	I3	},
{"dsubu",   "d,v,I",	0,    (int) M_DSUBU_I,	INSN_MACRO,	I3	},
{"eret",    "",		0x42000018, 0xffffffff,	0,	I3|M1	},
{"floor.l.d", "D,S",	0x4620000b, 0xffff003f, WR_D|RD_S|FP_D,	I3	},
{"floor.l.s", "D,S",	0x4600000b, 0xffff003f, WR_D|RD_S|FP_S,	I3	},
{"floor.w.d", "D,S",	0x4620000f, 0xffff003f, WR_D|RD_S|FP_D,	I2	},
{"floor.w.s", "D,S",	0x4600000f, 0xffff003f, WR_D|RD_S|FP_S,	I2	},
{"flushi",  "",		0xbc010000, 0xffffffff, 0,		L1	},
{"flushd",  "",		0xbc020000, 0xffffffff, 0, L1		},
{"flushid", "",		0xbc030000, 0xffffffff, 0, L1		},
{"hibernate","",        0x42000023, 0xffffffff,	0, V1	},
{"jr",      "s",	0x00000008, 0xfc1fffff,	UBD|RD_s,	I1	},
{"j",       "s",	0x00000008, 0xfc1fffff,	UBD|RD_s,	I1	}, /* jr */
/* SVR4 PIC code requires special handling for j, so it must be a
   macro.  */
{"j",	    "a",	0,     (int) M_J_A,	INSN_MACRO,	I1	},
/* This form of j is used by the disassembler and internally by the
   assembler, but will never match user input (because the line above
   will match first).  */
{"j",       "a",	0x08000000, 0xfc000000,	UBD,		I1	},
{"jalr",    "s",	0x0000f809, 0xfc1fffff,	UBD|RD_s|WR_d,	I1	},
{"jalr",    "d,s",	0x00000009, 0xfc1f07ff,	UBD|RD_s|WR_d,	I1	},
/* SVR4 PIC code requires special handling for jal, so it must be a
   macro.  */
{"jal",     "d,s",	0,     (int) M_JAL_2,	INSN_MACRO,	I1	},
{"jal",     "s",	0,     (int) M_JAL_1,	INSN_MACRO,	I1	},
{"jal",     "a",	0,     (int) M_JAL_A,	INSN_MACRO,	I1	},
/* This form of jal is used by the disassembler and internally by the
   assembler, but will never match user input (because the line above
   will match first).  */
{"jal",     "a",	0x0c000000, 0xfc000000,	UBD|WR_31,	I1	},
  /* jalx really should only be avaliable if mips16 is available,
     but for now make it I1. */
{"jalx",    "a",	0x74000000, 0xfc000000, UBD|WR_31,	I1      },
{"la",      "t,o(b)",	0x24000000, 0xfc000000,	WR_t|RD_s,	I1	}, /* addiu */
{"la",      "t,A(b)",	0,    (int) M_LA_AB,	INSN_MACRO,	I1	},
{"lb",      "t,o(b)",	0x80000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lb",      "t,A(b)",	0,    (int) M_LB_AB,	INSN_MACRO,	I1	},
{"lbu",     "t,o(b)",	0x90000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lbu",     "t,A(b)",	0,    (int) M_LBU_AB,	INSN_MACRO,	I1	},
{"ld",	    "t,o(b)",   0xdc000000, 0xfc000000, WR_t|RD_b,	I3	},
{"ld",      "t,o(b)",	0,    (int) M_LD_OB,	INSN_MACRO,	I1	},
{"ld",      "t,A(b)",	0,    (int) M_LD_AB,	INSN_MACRO,	I1	},
{"ldc1",    "T,o(b)",	0xd4000000, 0xfc000000, CLD|RD_b|WR_T|FP_D,	I2	},
{"ldc1",    "E,o(b)",	0xd4000000, 0xfc000000, CLD|RD_b|WR_T|FP_D,	I2	},
{"ldc1",    "T,A(b)",	0,    (int) M_LDC1_AB,	INSN_MACRO,	I2	},
{"ldc1",    "E,A(b)",	0,    (int) M_LDC1_AB,	INSN_MACRO,	I2	},
{"l.d",     "T,o(b)",	0xd4000000, 0xfc000000, CLD|RD_b|WR_T|FP_D,	I2	}, /* ldc1 */
{"l.d",     "T,o(b)",	0,    (int) M_L_DOB,	INSN_MACRO,	I1	},
{"l.d",     "T,A(b)",	0,    (int) M_L_DAB,	INSN_MACRO,	I1	},
{"ldc2",    "E,o(b)",	0xd8000000, 0xfc000000, CLD|RD_b|WR_CC,	I2	},
{"ldc2",    "E,A(b)",	0,    (int) M_LDC2_AB,	INSN_MACRO,	I2	},
{"ldc3",    "E,o(b)",	0xdc000000, 0xfc000000, CLD|RD_b|WR_CC,	I2	},
{"ldc3",    "E,A(b)",	0,    (int) M_LDC3_AB,	INSN_MACRO,	I2	},
{"ldl",	    "t,o(b)",	0x68000000, 0xfc000000, LDD|WR_t|RD_b,	I3	},
{"ldl",	    "t,A(b)",	0,    (int) M_LDL_AB,	INSN_MACRO,	I3	},
{"ldr",	    "t,o(b)",	0x6c000000, 0xfc000000, LDD|WR_t|RD_b,	I3	},
{"ldr",     "t,A(b)",	0,    (int) M_LDR_AB,	INSN_MACRO,	I3	},
{"ldxc1",   "D,t(b)",	0x4c000001, 0xfc00f83f, LDD|WR_D|RD_t|RD_b,	I4	},
{"lh",      "t,o(b)",	0x84000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lh",      "t,A(b)",	0,    (int) M_LH_AB,	INSN_MACRO,	I1	},
{"lhu",     "t,o(b)",	0x94000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lhu",     "t,A(b)",	0,    (int) M_LHU_AB,	INSN_MACRO,	I1	},
/* li is at the start of the table.  */
{"li.d",    "t,F",	0,    (int) M_LI_D,	INSN_MACRO,	I1	},
{"li.d",    "T,L",	0,    (int) M_LI_DD,	INSN_MACRO,	I1	},
{"li.s",    "t,f",	0,    (int) M_LI_S,	INSN_MACRO,	I1	},
{"li.s",    "T,l",	0,    (int) M_LI_SS,	INSN_MACRO,	I1	},
{"ll",	    "t,o(b)",	0xc0000000, 0xfc000000, LDD|RD_b|WR_t,	I2	},
{"ll",	    "t,A(b)",	0,    (int) M_LL_AB,	INSN_MACRO,	I2	},
{"lld",	    "t,o(b)",	0xd0000000, 0xfc000000, LDD|RD_b|WR_t,	I3	},
{"lld",     "t,A(b)",	0,    (int) M_LLD_AB,	INSN_MACRO,	I3	},
{"lui",     "t,u",	0x3c000000, 0xffe00000,	WR_t,		I1	},
{"luxc1",   "D,t(b)",	0x4c000005, 0xfc00f83f, LDD|WR_D|RD_t|RD_b,	I5	},
{"lw",      "t,o(b)",	0x8c000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lw",      "t,A(b)",	0,    (int) M_LW_AB,	INSN_MACRO,	I1	},
{"lwc0",    "E,o(b)",	0xc0000000, 0xfc000000,	CLD|RD_b|WR_CC,	I1	},
{"lwc0",    "E,A(b)",	0,    (int) M_LWC0_AB,	INSN_MACRO,	I1	},
{"lwc1",    "T,o(b)",	0xc4000000, 0xfc000000,	CLD|RD_b|WR_T|FP_S,	I1	},
{"lwc1",    "E,o(b)",	0xc4000000, 0xfc000000,	CLD|RD_b|WR_T|FP_S,	I1	},
{"lwc1",    "T,A(b)",	0,    (int) M_LWC1_AB,	INSN_MACRO,	I1	},
{"lwc1",    "E,A(b)",	0,    (int) M_LWC1_AB,	INSN_MACRO,	I1	},
{"l.s",     "T,o(b)",	0xc4000000, 0xfc000000,	CLD|RD_b|WR_T|FP_S,	I1	}, /* lwc1 */
{"l.s",     "T,A(b)",	0,    (int) M_LWC1_AB,	INSN_MACRO,	I1	},
{"lwc2",    "E,o(b)",	0xc8000000, 0xfc000000,	CLD|RD_b|WR_CC,	I1	},
{"lwc2",    "E,A(b)",	0,    (int) M_LWC2_AB,	INSN_MACRO,	I1	},
{"lwc3",    "E,o(b)",	0xcc000000, 0xfc000000,	CLD|RD_b|WR_CC,	I1	},
{"lwc3",    "E,A(b)",	0,    (int) M_LWC3_AB,	INSN_MACRO,	I1	},
{"lwl",     "t,o(b)",	0x88000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lwl",     "t,A(b)",	0,    (int) M_LWL_AB,	INSN_MACRO,	I1	},
{"lcache",  "t,o(b)",	0x88000000, 0xfc000000,	LDD|RD_b|WR_t,	I2	}, /* same */
{"lcache",  "t,A(b)",	0,    (int) M_LWL_AB,	INSN_MACRO,	I2	}, /* as lwl */
{"lwr",     "t,o(b)",	0x98000000, 0xfc000000,	LDD|RD_b|WR_t,	I1	},
{"lwr",     "t,A(b)",	0,    (int) M_LWR_AB,	INSN_MACRO,	I1	},
{"flush",   "t,o(b)",	0x98000000, 0xfc000000,	LDD|RD_b|WR_t,	I2	}, /* same */
{"flush",   "t,A(b)",	0,    (int) M_LWR_AB,	INSN_MACRO,	I2	}, /* as lwr */
{"lwu",     "t,o(b)",	0x9c000000, 0xfc000000,	LDD|RD_b|WR_t,	I3	},
{"lwu",     "t,A(b)",	0,    (int) M_LWU_AB,	INSN_MACRO,	I3	},
{"lwxc1",   "D,t(b)",	0x4c000000, 0xfc00f83f, LDD|WR_D|RD_t|RD_b,	I4	},


{"mad",	    "s,t",	0x70000000, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO|RD_HI|RD_LO,	P3	},
{"madu",    "s,t",	0x70000001, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO|RD_HI|RD_LO,	P3	},
{"madd.d",  "D,R,S,T",	0x4c000021, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I4	},
{"madd.s",  "D,R,S,T",	0x4c000020, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_S,	I4	},
{"madd.ps", "D,R,S,T",	0x4c000026, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I5	},
{"madd",    "s,t",	0x0000001c, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,		L1	},
{"madd",    "s,t",	0x70000000, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO|IS_M,	        G1|M1	},
{"madd",    "d,s,t",	0x70000000, 0xfc0007ff,	RD_s|RD_t|WR_HI|WR_LO|WR_d|IS_M,	G1	},
{"maddu",   "s,t",	0x0000001d, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,		L1	},
{"maddu",   "s,t",	0x70000001, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO|IS_M,	G1|M1},
{"maddu",   "d,s,t",	0x70000001, 0xfc0007ff,	RD_s|RD_t|WR_HI|WR_LO|WR_d|IS_M,	G1},
{"madd16",  "s,t",      0x00000028, 0xfc00ffff, RD_s|RD_t|WR_HI|WR_LO|RD_HI|RD_LO,	V1	},
{"mfc0",    "t,G",	0x40000000, 0xffe007ff,	LCD|WR_t|RD_C0,	I1	},
{"mfc1",    "t,S",	0x44000000, 0xffe007ff,	LCD|WR_t|RD_S|FP_S,	I1	},
{"mfc1",    "t,G",	0x44000000, 0xffe007ff,	LCD|WR_t|RD_S|FP_S,	I1	},
{"mfc2",    "t,G",	0x48000000, 0xffe007ff,	LCD|WR_t|RD_C2,	I1	},
{"mfc3",    "t,G",	0x4c000000, 0xffe007ff,	LCD|WR_t|RD_C3,	I1	},
{"mfhi",    "d",	0x00000010, 0xffff07ff,	WR_d|RD_HI,	I1	},
{"mflo",    "d",	0x00000012, 0xffff07ff,	WR_d|RD_LO,	I1	},
{"mov.d",   "D,S",	0x46200006, 0xffff003f,	WR_D|RD_S|FP_D,	I1	},
{"mov.s",   "D,S",	0x46000006, 0xffff003f,	WR_D|RD_S|FP_S,	I1	},
{"mov.ps",  "D,S",	0x46c00006, 0xffff003f,	WR_D|RD_S|FP_D,	I5	},
{"movf",    "d,s,N",	0x00000001, 0xfc0307ff, WR_d|RD_s|RD_CC|FP_D|FP_S, I4|M1},
{"movf.d",  "D,S,N",	0x46200011, 0xffe3003f, WR_D|RD_S|RD_CC|FP_D,	I4|M1	},
{"movf.s",  "D,S,N",	0x46000011, 0xffe3003f, WR_D|RD_S|RD_CC|FP_S,	I4|M1	},
{"movf.ps", "D,S,N",	0x46c00011, 0xffe3003f, WR_D|RD_S|RD_CC|FP_D,	I5	},
{"movn",    "d,v,t",	0x0000000b, 0xfc0007ff,	WR_d|RD_s|RD_t,	I4|M1	},
{"ffc",     "d,v",	0x0000000b, 0xfc1f07ff,	WR_d|RD_s,L1	},
{"movn.d",  "D,S,t",	0x46200013, 0xffe0003f, WR_D|RD_S|RD_t|FP_D,	I4|M1	},
{"movn.s",  "D,S,t",	0x46000013, 0xffe0003f, WR_D|RD_S|RD_t|FP_S,	I4|M1	},
{"movt",    "d,s,N",	0x00010001, 0xfc0307ff, WR_d|RD_s|RD_CC,	I4|M1	},
{"movt.d",  "D,S,N",	0x46210011, 0xffe3003f, WR_D|RD_S|RD_CC|FP_D,	I4|M1	},
{"movt.s",  "D,S,N",	0x46010011, 0xffe3003f, WR_D|RD_S|RD_CC|FP_S,	I4|M1	},
{"movt.ps", "D,S,N",	0x46c10011, 0xffe3003f, WR_D|RD_S|RD_CC|FP_D,	I5},
{"movz",    "d,v,t",	0x0000000a, 0xfc0007ff,	WR_d|RD_s|RD_t,	I4|M1	},
{"ffs",     "d,v",	0x0000000a, 0xfc1f07ff,	WR_d|RD_s,L1	},
{"movz.d",  "D,S,t",	0x46200012, 0xffe0003f, WR_D|RD_S|RD_t|FP_D,	I4|M1	},
{"movz.s",  "D,S,t",	0x46000012, 0xffe0003f, WR_D|RD_S|RD_t|FP_S,	I4|M1	},
/* move is at the top of the table.  */
{"msub.d",  "D,R,S,T",	0x4c000029, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I4	},
{"msub.s",  "D,R,S,T",	0x4c000028, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_S,	I4	},
{"msub.ps", "D,R,S,T",	0x4c00002e, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I5	},
{"msub",    "s,t",	0x0000001e, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,L1	},
{"msubu",   "s,t",	0x0000001f, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,L1	},
{"mtc0",    "t,G",	0x40800000, 0xffe007ff,	COD|RD_t|WR_C0|WR_CC,	I1	},
{"mtc1",    "t,S",	0x44800000, 0xffe007ff,	COD|RD_t|WR_S|FP_S,	I1	},
{"mtc1",    "t,G",	0x44800000, 0xffe007ff,	COD|RD_t|WR_S|FP_S,	I1	},
{"mtc2",    "t,G",	0x48800000, 0xffe007ff,	COD|RD_t|WR_C2|WR_CC,	I1	},
{"mtc3",    "t,G",	0x4c800000, 0xffe007ff,	COD|RD_t|WR_C3|WR_CC,	I1	},
{"mthi",    "s",	0x00000011, 0xfc1fffff,	RD_s|WR_HI,	I1	},
{"mtlo",    "s",	0x00000013, 0xfc1fffff,	RD_s|WR_LO,	I1	},
{"mul.d",   "D,V,T",	0x46200002, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I1	},
{"mul.s",   "D,V,T",	0x46000002, 0xffe0003f,	WR_D|RD_S|RD_T|FP_S,	I1	},
{"mul.ps",  "D,V,T",	0x46c00002, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5	},
{"mul",     "d,v,t",	0x70000002, 0xfc0007ff, WR_d|RD_s|RD_t|WR_HI|WR_LO,P3},
{"mul",     "d,v,t",	0,    (int) M_MUL,	INSN_MACRO,	I1	},
{"mul",     "d,v,I",	0,    (int) M_MUL_I,	INSN_MACRO,	I1	},
{"mulo",    "d,v,t",	0,    (int) M_MULO,	INSN_MACRO,	I1	},
{"mulo",    "d,v,I",	0,    (int) M_MULO_I,	INSN_MACRO,	I1	},
{"mulou",   "d,v,t",	0,    (int) M_MULOU,	INSN_MACRO,	I1	},
{"mulou",   "d,v,I",	0,    (int) M_MULOU_I,	INSN_MACRO,	I1	},
{"mult",    "s,t",	0x00000018, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO|IS_M,	I1},
{"mult",    "d,s,t",	0x00000018, 0xfc0007ff,	RD_s|RD_t|WR_HI|WR_LO|WR_d|IS_M, G1},
{"multu",   "s,t",	0x00000019, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO|IS_M,	I1},
{"multu",   "d,s,t",	0x00000019, 0xfc0007ff,	RD_s|RD_t|WR_HI|WR_LO|WR_d|IS_M, G1},
{"neg",     "d,w",	0x00000022, 0xffe007ff,	WR_d|RD_t,	I1	}, /* sub 0 */
{"negu",    "d,w",	0x00000023, 0xffe007ff,	WR_d|RD_t,	I1	}, /* subu 0 */
{"neg.d",   "D,V",	0x46200007, 0xffff003f,	WR_D|RD_S|FP_D,	I1	},
{"neg.s",   "D,V",	0x46000007, 0xffff003f,	WR_D|RD_S|FP_S,	I1	},
{"neg.ps",  "D,V",	0x46c00007, 0xffff003f,	WR_D|RD_S|FP_D,I5	},
{"nmadd.d", "D,R,S,T",	0x4c000031, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I4	},
{"nmadd.s", "D,R,S,T",	0x4c000030, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_S,	I4	},
{"nmadd.ps","D,R,S,T",	0x4c000036, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I5	},
{"nmsub.d", "D,R,S,T",	0x4c000039, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I4	},
{"nmsub.s", "D,R,S,T",	0x4c000038, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_S,	I4	},
{"nmsub.ps","D,R,S,T",	0x4c00003e, 0xfc00003f, RD_R|RD_S|RD_T|WR_D|FP_D,	I5	},
/* nop is at the start of the table.  */
{"nor",     "d,v,t",	0x00000027, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"nor",     "t,r,I",	0,    (int) M_NOR_I,	INSN_MACRO,	I1	},
{"not",     "d,v",	0x00000027, 0xfc1f07ff,	WR_d|RD_s|RD_t,	I1	},/*nor d,s,0*/
{"or",      "d,v,t",	0x00000025, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"or",      "t,r,I",	0,    (int) M_OR_I,	INSN_MACRO,	I1	},
{"ori",     "t,r,i",	0x34000000, 0xfc000000,	WR_t|RD_s,	I1	},

{"pll.ps",  "D,V,T",	0x46c0002c, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5},
{"plu.ps",  "D,V,T",	0x46c0002d, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5},

{"pref",    "k,o(b)",	0xcc000000, 0xfc000000, RD_b,		G3|M1	},
{"prefx",   "h,t(b)",	0x4c00000f, 0xfc0007ff, RD_b|RD_t,	I4	},

{"pul.ps",  "D,V,T",	0x46c0002e, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5},
{"puu.ps",  "D,V,T",	0x46c0002f, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5},

{"recip.d", "D,S",	0x46200015, 0xffff003f, WR_D|RD_S|FP_D,	I4	},
{"recip.s", "D,S",	0x46000015, 0xffff003f, WR_D|RD_S|FP_S,	I4	},
{"rem",     "z,s,t",	0x0000001a, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,	I1 },
{"rem",     "d,v,t",	0,    (int) M_REM_3,	INSN_MACRO,	I1	},
{"rem",     "d,v,I",	0,    (int) M_REM_3I,	INSN_MACRO,	I1	},
{"remu",    "z,s,t",	0x0000001b, 0xfc00ffff,	RD_s|RD_t|WR_HI|WR_LO,	I1 },
{"remu",    "d,v,t",	0,    (int) M_REMU_3,	INSN_MACRO,	I1	},
{"remu",    "d,v,I",	0,    (int) M_REMU_3I,	INSN_MACRO,	I1	},
{"rfe",     "",		0x42000010, 0xffffffff,	0,	I1|T3		},
{"rol",     "d,v,t",	0,    (int) M_ROL,	INSN_MACRO,	I1	},
{"rol",     "d,v,I",	0,    (int) M_ROL_I,	INSN_MACRO,	I1	},
{"ror",     "d,v,t",	0,    (int) M_ROR,	INSN_MACRO,	I1	},
{"ror",     "d,v,I",	0,    (int) M_ROR_I,	INSN_MACRO,	I1	},
{"round.l.d", "D,S",	0x46200008, 0xffff003f, WR_D|RD_S|FP_D,	I3	},
{"round.l.s", "D,S",	0x46000008, 0xffff003f, WR_D|RD_S|FP_S,	I3	},
{"round.w.d", "D,S",	0x4620000c, 0xffff003f, WR_D|RD_S|FP_D,	I2	},
{"round.w.s", "D,S",	0x4600000c, 0xffff003f, WR_D|RD_S|FP_S,	I2	},
{"rsqrt.d", "D,S",	0x46200016, 0xffff003f, WR_D|RD_S|FP_D,	I4	},
{"rsqrt.s", "D,S",	0x46000016, 0xffff003f, WR_D|RD_S|FP_S,	I4	},
{"sb",      "t,o(b)",	0xa0000000, 0xfc000000,	SM|RD_t|RD_b,	I1	},
{"sb",      "t,A(b)",	0,    (int) M_SB_AB,	INSN_MACRO,	I1	},
{"sc",	    "t,o(b)",	0xe0000000, 0xfc000000, SM|RD_t|WR_t|RD_b,	I2	},
{"sc",	    "t,A(b)",	0,    (int) M_SC_AB,	INSN_MACRO,	I2	},
{"scd",	    "t,o(b)",	0xf0000000, 0xfc000000, SM|RD_t|WR_t|RD_b,	I3	},
{"scd",	    "t,A(b)",	0,    (int) M_SCD_AB,	INSN_MACRO,	I3	},
{"sd",	    "t,o(b)",	0xfc000000, 0xfc000000,	SM|RD_t|RD_b,	I3	},
{"sd",      "t,o(b)",	0,    (int) M_SD_OB,	INSN_MACRO,	I1	},
{"sd",      "t,A(b)",	0,    (int) M_SD_AB,	INSN_MACRO,	I1	},
{"sdbbp",   "",		0x0000000e, 0xffffffff,	TRAP,           G2|M1	},
{"sdbbp",   "c",	0x0000000e, 0xfc00ffff,	TRAP,		G2|M1	},
{"sdbbp",   "c,q",	0x0000000e, 0xfc00003f,	TRAP,		G2|M1	},
{"sdc1",    "T,o(b)",	0xf4000000, 0xfc000000, SM|RD_T|RD_b|FP_D,	I2	},
{"sdc1",    "E,o(b)",	0xf4000000, 0xfc000000, SM|RD_T|RD_b|FP_D,	I2	},
{"sdc1",    "T,A(b)",	0,    (int) M_SDC1_AB,	INSN_MACRO,	I2	},
{"sdc1",    "E,A(b)",	0,    (int) M_SDC1_AB,	INSN_MACRO,	I2	},
{"sdc2",    "E,o(b)",	0xf8000000, 0xfc000000, SM|RD_C2|RD_b,	I2	},
{"sdc2",    "E,A(b)",	0,    (int) M_SDC2_AB,	INSN_MACRO,	I2	},
{"sdc3",    "E,o(b)",	0xfc000000, 0xfc000000, SM|RD_C3|RD_b,	I2	},
{"sdc3",    "E,A(b)",	0,    (int) M_SDC3_AB,	INSN_MACRO,	I2	},
{"s.d",     "T,o(b)",	0xf4000000, 0xfc000000, SM|RD_T|RD_b|FP_D,	I2	},
{"s.d",     "T,o(b)",	0,    (int) M_S_DOB,	INSN_MACRO,	I1	},
{"s.d",     "T,A(b)",	0,    (int) M_S_DAB,	INSN_MACRO,	I1	},
{"sdl",     "t,o(b)",	0xb0000000, 0xfc000000,	SM|RD_t|RD_b,	I3	},
{"sdl",     "t,A(b)",	0,    (int) M_SDL_AB,	INSN_MACRO,	I3	},
{"sdr",     "t,o(b)",	0xb4000000, 0xfc000000,	SM|RD_t|RD_b,	I3	},
{"sdr",     "t,A(b)",	0,    (int) M_SDR_AB,	INSN_MACRO,	I3	},
{"sdxc1",   "S,t(b)",   0x4c000009, 0xfc0007ff, SM|RD_S|RD_t|RD_b,	I4	},
{"selsl",   "d,v,t",	0x00000005, 0xfc0007ff,	WR_d|RD_s|RD_t,L1	},
{"selsr",   "d,v,t",	0x00000001, 0xfc0007ff,	WR_d|RD_s|RD_t,L1	},
{"seq",     "d,v,t",	0,    (int) M_SEQ,	INSN_MACRO,	I1	},
{"seq",     "d,v,I",	0,    (int) M_SEQ_I,	INSN_MACRO,	I1	},
{"sge",     "d,v,t",	0,    (int) M_SGE,	INSN_MACRO,	I1	},
{"sge",     "d,v,I",	0,    (int) M_SGE_I,	INSN_MACRO,	I1	},
{"sgeu",    "d,v,t",	0,    (int) M_SGEU,	INSN_MACRO,	I1	},
{"sgeu",    "d,v,I",	0,    (int) M_SGEU_I,	INSN_MACRO,	I1	},
{"sgt",     "d,v,t",	0,    (int) M_SGT,	INSN_MACRO,	I1	},
{"sgt",     "d,v,I",	0,    (int) M_SGT_I,	INSN_MACRO,	I1	},
{"sgtu",    "d,v,t",	0,    (int) M_SGTU,	INSN_MACRO,	I1	},
{"sgtu",    "d,v,I",	0,    (int) M_SGTU_I,	INSN_MACRO,	I1	},
{"sh",      "t,o(b)",	0xa4000000, 0xfc000000,	SM|RD_t|RD_b,	I1	},
{"sh",      "t,A(b)",	0,    (int) M_SH_AB,	INSN_MACRO,	I1	},
{"sle",     "d,v,t",	0,    (int) M_SLE,	INSN_MACRO,	I1	},
{"sle",     "d,v,I",	0,    (int) M_SLE_I,	INSN_MACRO,	I1	},
{"sleu",    "d,v,t",	0,    (int) M_SLEU,	INSN_MACRO,	I1	},
{"sleu",    "d,v,I",	0,    (int) M_SLEU_I,	INSN_MACRO,	I1	},
{"sllv",    "d,t,s",	0x00000004, 0xfc0007ff,	WR_d|RD_t|RD_s,	I1	},
{"sll",     "d,w,s",	0x00000004, 0xfc0007ff,	WR_d|RD_t|RD_s,	I1	}, /* sllv */
{"sll",     "d,w,<",	0x00000000, 0xffe0003f,	WR_d|RD_t,	I1	},
{"slt",     "d,v,t",	0x0000002a, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"slt",     "d,v,I",	0,    (int) M_SLT_I,	INSN_MACRO,	I1	},
{"slti",    "t,r,j",	0x28000000, 0xfc000000,	WR_t|RD_s,	I1	},
{"sltiu",   "t,r,j",	0x2c000000, 0xfc000000,	WR_t|RD_s,	I1	},
{"sltu",    "d,v,t",	0x0000002b, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"sltu",    "d,v,I",	0,    (int) M_SLTU_I,	INSN_MACRO,	I1	},
{"sne",     "d,v,t",	0,    (int) M_SNE,	INSN_MACRO,	I1	},
{"sne",     "d,v,I",	0,    (int) M_SNE_I,	INSN_MACRO,	I1	},
{"sqrt.d",  "D,S",	0x46200004, 0xffff003f, WR_D|RD_S|FP_D,	I2	},
{"sqrt.s",  "D,S",	0x46000004, 0xffff003f, WR_D|RD_S|FP_S,	I2	},
{"srav",    "d,t,s",	0x00000007, 0xfc0007ff,	WR_d|RD_t|RD_s,	I1	},
{"sra",     "d,w,s",	0x00000007, 0xfc0007ff,	WR_d|RD_t|RD_s,	I1	}, /* srav */
{"sra",     "d,w,<",	0x00000003, 0xffe0003f,	WR_d|RD_t,	I1	},
{"srlv",    "d,t,s",	0x00000006, 0xfc0007ff,	WR_d|RD_t|RD_s,	I1	},
{"srl",     "d,w,s",	0x00000006, 0xfc0007ff,	WR_d|RD_t|RD_s,	I1	}, /* srlv */
{"srl",     "d,w,<",	0x00000002, 0xffe0003f,	WR_d|RD_t,	I1	},
{"ssnop",     "",	0x00000040, 0xffffffff,	0,		M1	},
{"standby", "",         0x42000021, 0xffffffff,	0,		V1	},
{"sub",     "d,v,t",	0x00000022, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"sub",     "d,v,I",	0,    (int) M_SUB_I,	INSN_MACRO,	I1	},
{"sub.d",   "D,V,T",	0x46200001, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I1	},     
{"sub.s",   "D,V,T",	0x46000001, 0xffe0003f,	WR_D|RD_S|RD_T|FP_S,	I1	},
{"sub.ps",  "D,V,T",	0x46c00001, 0xffe0003f,	WR_D|RD_S|RD_T|FP_D,	I5	},
{"subu",    "d,v,t",	0x00000023, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"subu",    "d,v,I",	0,    (int) M_SUBU_I,	INSN_MACRO,	I1	},
{"suspend", "",         0x42000022, 0xffffffff,	0,		V1	},
{"suxc1",   "S,t(b)",   0x4c00000d, 0xfc0007ff, SM|RD_S|RD_t|RD_b,	I5	},
{"sw",      "t,o(b)",	0xac000000, 0xfc000000,	SM|RD_t|RD_b,	I1	},
{"sw",      "t,A(b)",	0,    (int) M_SW_AB,	INSN_MACRO,	I1	},
{"swc0",    "E,o(b)",	0xe0000000, 0xfc000000,	SM|RD_C0|RD_b,	I1	},
{"swc0",    "E,A(b)",	0,    (int) M_SWC0_AB,	INSN_MACRO,	I1	},
{"swc1",    "T,o(b)",	0xe4000000, 0xfc000000,	SM|RD_T|RD_b|FP_S,	I1	},
{"swc1",    "E,o(b)",	0xe4000000, 0xfc000000,	SM|RD_T|RD_b|FP_S,	I1	},
{"swc1",    "T,A(b)",	0,    (int) M_SWC1_AB,	INSN_MACRO,	I1	},
{"swc1",    "E,A(b)",	0,    (int) M_SWC1_AB,	INSN_MACRO,	I1	},
{"s.s",     "T,o(b)",	0xe4000000, 0xfc000000,	SM|RD_T|RD_b|FP_S,	I1	}, /* swc1 */
{"s.s",     "T,A(b)",	0,    (int) M_SWC1_AB,	INSN_MACRO,	I1	},
{"swc2",    "E,o(b)",	0xe8000000, 0xfc000000,	SM|RD_C2|RD_b,	I1	},
{"swc2",    "E,A(b)",	0,    (int) M_SWC2_AB,	INSN_MACRO,	I1	},
{"swc3",    "E,o(b)",	0xec000000, 0xfc000000,	SM|RD_C3|RD_b,	I1	},
{"swc3",    "E,A(b)",	0,    (int) M_SWC3_AB,	INSN_MACRO,	I1	},
{"swl",     "t,o(b)",	0xa8000000, 0xfc000000,	SM|RD_t|RD_b,	I1	},
{"swl",     "t,A(b)",	0,    (int) M_SWL_AB,	INSN_MACRO,	I1	},
{"scache",  "t,o(b)",	0xa8000000, 0xfc000000,	RD_t|RD_b,	I2	}, /* same */
{"scache",  "t,A(b)",	0,    (int) M_SWL_AB,	INSN_MACRO,	I2	}, /* as swl */
{"swr",     "t,o(b)",	0xb8000000, 0xfc000000,	SM|RD_t|RD_b,	I1	},
{"swr",     "t,A(b)",	0,    (int) M_SWR_AB,	INSN_MACRO,	I1	},
{"invalidate", "t,o(b)",0xb8000000, 0xfc000000,	RD_t|RD_b,	I2	}, /* same */
{"invalidate", "t,A(b)",0,    (int) M_SWR_AB,	INSN_MACRO,	I2	}, /* as swr */
{"swxc1",   "S,t(b)",   0x4c000008, 0xfc0007ff, SM|RD_S|RD_t|RD_b,	I4	},
{"sync",    "",		0x0000000f, 0xffffffff,	INSN_SYNC,	I2|G1	},
{"sync.p",  "",		0x0000040f, 0xffffffff,	INSN_SYNC,	I2	},
{"sync.l",  "",		0x0000000f, 0xffffffff,	INSN_SYNC,	I2	},
{"syscall", "",		0x0000000c, 0xffffffff,	TRAP,	I1		},
{"syscall", "B",	0x0000000c, 0xfc00003f,	TRAP,	I1		},
{"teqi",    "s,j",	0x040c0000, 0xfc1f0000, RD_s|TRAP,	I2	},
{"teq",	    "s,t",	0x00000034, 0xfc00ffff, RD_s|RD_t|TRAP,	I2	},
{"teq",	    "s,t,q",	0x00000034, 0xfc00003f, RD_s|RD_t|TRAP,	I2	},
{"teq",     "s,j",	0x040c0000, 0xfc1f0000, RD_s|TRAP,	I2	}, /* teqi */
{"teq",     "s,I",	0,    (int) M_TEQ_I,	INSN_MACRO,	I2	},
{"tgei",    "s,j",	0x04080000, 0xfc1f0000, RD_s|TRAP,	I2	},
{"tge",	    "s,t",	0x00000030, 0xfc00ffff,	RD_s|RD_t|TRAP,	I2	},
{"tge",	    "s,t,q",	0x00000030, 0xfc00003f,	RD_s|RD_t|TRAP,	I2	},
{"tge",     "s,j",	0x04080000, 0xfc1f0000, RD_s|TRAP,	I2	}, /* tgei */
{"tge",	    "s,I",	0,    (int) M_TGE_I,    INSN_MACRO,	I2	},
{"tgeiu",   "s,j",	0x04090000, 0xfc1f0000, RD_s|TRAP,	I2		},
{"tgeu",    "s,t",	0x00000031, 0xfc00ffff, RD_s|RD_t|TRAP,	I2	 },
{"tgeu",    "s,t,q",	0x00000031, 0xfc00003f, RD_s|RD_t|TRAP,	I2	 },
{"tgeu",    "s,j",	0x04090000, 0xfc1f0000, RD_s|TRAP,	I2		}, /* tgeiu */
{"tgeu",    "s,I",	0,    (int) M_TGEU_I,	INSN_MACRO,	I2	},
{"tlbp",    "",		0x42000008, 0xffffffff,	INSN_TLB,	I1|M1	},
{"tlbr",    "",		0x42000001, 0xffffffff,	INSN_TLB,	I1|M1	},
{"tlbwi",   "",		0x42000002, 0xffffffff,	INSN_TLB,	I1|M1	},
{"tlbwr",   "",		0x42000006, 0xffffffff,	INSN_TLB,	I1|M1	},
{"tlti",    "s,j",	0x040a0000, 0xfc1f0000,	RD_s|TRAP,	I2		},
{"tlt",     "s,t",	0x00000032, 0xfc00ffff, RD_s|RD_t|TRAP,	I2	 },
{"tlt",     "s,t,q",	0x00000032, 0xfc00003f, RD_s|RD_t|TRAP,	I2	 },
{"tlt",     "s,j",	0x040a0000, 0xfc1f0000,	RD_s|TRAP,	I2		}, /* tlti */
{"tlt",     "s,I",	0,    (int) M_TLT_I,	INSN_MACRO,	I2	},
{"tltiu",   "s,j",	0x040b0000, 0xfc1f0000, RD_s|TRAP,	I2		},
{"tltu",    "s,t",	0x00000033, 0xfc00ffff, RD_s|RD_t|TRAP,	I2	 },
{"tltu",    "s,t,q",	0x00000033, 0xfc00003f, RD_s|RD_t|TRAP,	I2	 },
{"tltu",    "s,j",	0x040b0000, 0xfc1f0000, RD_s|TRAP,	I2		}, /* tltiu */
{"tltu",    "s,I",	0,    (int) M_TLTU_I,	INSN_MACRO,	I2	},
{"tnei",    "s,j",	0x040e0000, 0xfc1f0000, RD_s|TRAP,	I2		},
{"tne",     "s,t",	0x00000036, 0xfc00ffff, RD_s|RD_t|TRAP,	I2	 },
{"tne",     "s,t,q",	0x00000036, 0xfc00003f, RD_s|RD_t|TRAP,	I2	 },
{"tne",     "s,j",	0x040e0000, 0xfc1f0000, RD_s|TRAP,	I2		}, /* tnei */
{"tne",     "s,I",	0,    (int) M_TNE_I,	INSN_MACRO,	I2	},
{"trunc.l.d", "D,S",	0x46200009, 0xffff003f, WR_D|RD_S|FP_D,	I3	},
{"trunc.l.s", "D,S",	0x46000009, 0xffff003f,	WR_D|RD_S|FP_S,	I3	},
{"trunc.w.d", "D,S",	0x4620000d, 0xffff003f, WR_D|RD_S|FP_D,	I2	},
{"trunc.w.d", "D,S,x",	0x4620000d, 0xffff003f, WR_D|RD_S|FP_D,	I2	},
{"trunc.w.d", "D,S,t",	0,    (int) M_TRUNCWD,	INSN_MACRO,	I1	},
{"trunc.w.s", "D,S",	0x4600000d, 0xffff003f,	WR_D|RD_S|FP_S,	I2	},
{"trunc.w.s", "D,S,x",	0x4600000d, 0xffff003f,	WR_D|RD_S|FP_S,	I2	},
{"trunc.w.s", "D,S,t",	0,    (int) M_TRUNCWS,	INSN_MACRO,	I1	},
{"uld",     "t,o(b)",	0,    (int) M_ULD,	INSN_MACRO,	I3	},
{"uld",     "t,A(b)",	0,    (int) M_ULD_A,	INSN_MACRO,	I3	},
{"ulh",     "t,o(b)",	0,    (int) M_ULH,	INSN_MACRO,	I1	},
{"ulh",     "t,A(b)",	0,    (int) M_ULH_A,	INSN_MACRO,	I1	},
{"ulhu",    "t,o(b)",	0,    (int) M_ULHU,	INSN_MACRO,	I1	},
{"ulhu",    "t,A(b)",	0,    (int) M_ULHU_A,	INSN_MACRO,	I1	},
{"ulw",     "t,o(b)",	0,    (int) M_ULW,	INSN_MACRO,	I1	},
{"ulw",     "t,A(b)",	0,    (int) M_ULW_A,	INSN_MACRO,	I1	},
{"usd",     "t,o(b)",	0,    (int) M_USD,	INSN_MACRO,	I3	},
{"usd",     "t,A(b)",	0,    (int) M_USD_A,	INSN_MACRO,	I3	},
{"ush",     "t,o(b)",	0,    (int) M_USH,	INSN_MACRO,	I1	},
{"ush",     "t,A(b)",	0,    (int) M_USH_A,	INSN_MACRO,	I1	},
{"usw",     "t,o(b)",	0,    (int) M_USW,	INSN_MACRO,	I1	},
{"usw",     "t,A(b)",	0,    (int) M_USW_A,	INSN_MACRO,	I1	},
{"xor",     "d,v,t",	0x00000026, 0xfc0007ff,	WR_d|RD_s|RD_t,	I1	},
{"xor",     "t,r,I",	0,    (int) M_XOR_I,	INSN_MACRO,	I1	},
{"xori",    "t,r,i",	0x38000000, 0xfc000000,	WR_t|RD_s,	I1	},
{"wait",    "",		0x42000020, 0xffffffff,	TRAP,	I3|M1	},
{"waiti",   "",		0x42000020, 0xffffffff,	TRAP,	L1	},
{"wb", 	    "o(b)",	0xbc040000, 0xfc1f0000, SM|RD_b,	L1	},
/* No hazard protection on coprocessor instructions--they shouldn't
   change the state of the processor and if they do it's up to the
   user to put in nops as necessary.  These are at the end so that the
   disasembler recognizes more specific versions first.  */
{"c0",      "C",	0x42000000, 0xfe000000,	0,	I1		},
{"c1",      "C",	0x46000000, 0xfe000000,	0,	I1		},
{"c2",      "C",	0x4a000000, 0xfe000000,	0,	I1		},
{"c3",      "C",	0x4e000000, 0xfe000000,	0,	I1		},
{"cop0",     "C",	0,    (int) M_COP0,	INSN_MACRO,	I1	},
{"cop1",     "C",	0,    (int) M_COP1,	INSN_MACRO,	I1	},
{"cop2",     "C",	0,    (int) M_COP2,	INSN_MACRO,	I1	},
{"cop3",     "C",	0,    (int) M_COP3,	INSN_MACRO,	I1	},

  /* Conflicts with the 4650's "mul" instruction.  Nobody's using the
     4010 any more, so move this insn out of the way.  If the object
     format gave us more info, we could do this right.  */
{"addciu",  "t,r,j",	0x70000000, 0xfc000000,	WR_t|RD_s,L1	},
};

#define MIPS_NUM_OPCODES \
	((sizeof mips_builtin_opcodes) / (sizeof (mips_builtin_opcodes[0])))
const int bfd_mips_num_builtin_opcodes = MIPS_NUM_OPCODES;

/* const removed from the following to allow for dynamic extensions to the 
 * built-in instruction set. */
struct mips_opcode *mips_opcodes =
  (struct mips_opcode *) mips_builtin_opcodes;
int bfd_mips_num_opcodes = MIPS_NUM_OPCODES;
#undef MIPS_NUM_OPCODES

