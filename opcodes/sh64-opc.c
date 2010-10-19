/* Definitions for SH64 opcodes.
   Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "sh64-opc.h"
#include <stdio.h>

/* Users currently assume that no mnemonic appears twice.  For
   disassembly, the first complete match is displayed.  */
const shmedia_opcode_info shmedia_table[] = {

/* 000000mmmmmm1001nnnnnndddddd0000  add <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "add",	    {A_GREG_M,A_GREG_N,A_GREG_D},
      {OFFSET_20,OFFSET_10,OFFSET_4}, SHMEDIA_ADD_OPC
    },
/* 000000mmmmmm1000nnnnnndddddd0000  add.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "add.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x00080000
    },
/* 110100mmmmmmssssssssssdddddd0000  addi <A_GREG_M>,<A_IMMS10>,<A_GREG_D>  */
    { "addi",	    {A_GREG_M,A_IMMS10BY1,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 
      SHMEDIA_ADDI_OPC
    },
/* 110101mmmmmmssssssssssdddddd0000  addi.l <A_GREG_M>,<A_IMMS10>,<A_GREG_D>  */
    { "addi.l",	    {A_GREG_M,A_IMMS10BY1,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xd4000000
    },
/* 000000mmmmmm1100nnnnnndddddd0000  addz.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "addz.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x000c0000
    },
/* 111000mmmmmm0100ssssss1111110000  alloco <A_GREG_M>,<A_IMMS6BY32>  */
    { "alloco",	    {A_GREG_M,A_IMMS6BY32},	      {OFFSET_20,OFFSET_10},	      0xe00403f0
    },
/* 000001mmmmmm1011nnnnnndddddd0000  and <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "and",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x040b0000
    },
/* 000001mmmmmm1111nnnnnndddddd0000  andc <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "andc",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x040f0000
    },
/* 110110mmmmmmssssssssssdddddd0000  andi <A_GREG_M>,<A_IMMS10>,<A_GREG_D>  */
    { "andi",	    {A_GREG_M,A_IMMS10,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0xd8000000
    },
/* 011001mmmmmm0001nnnnnnl00ccc0000  beq <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "beq/l",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64010200
    },
/* 011001mmmmmm0001nnnnnnl00ccc0000  beq <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "beq",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64010200
    },
/* 011001mmmmmm0001nnnnnn000ccc0000  beq/u <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "beq/u",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64010000
    },
/* 111001mmmmmm0001ssssssl00ccc0000  beqi <A_GREG_M>,<A_IMMS6>,<A_TREG_A>  */
    { "beqi/l",	    {A_GREG_M,A_IMMS6,A_TREG_A},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe4010200
    },
/* 111001mmmmmm0001ssssssl00ccc0000  beqi <A_GREG_M>,<A_IMMS6>,<A_TREG_A>  */
    { "beqi",	    {A_GREG_M,A_IMMS6,A_TREG_A},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe4010200
    },
/* 111001mmmmmm0001ssssss000ccc0000  beqi/u <A_GREG_M>,<A_IMMS6>,<A_TREG_A>  */
    { "beqi/u",     {A_GREG_M,A_IMMS6,A_TREG_A},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe4010000
    },
/* 011001mmmmmm0011nnnnnnl00ccc0000  bge <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bge/l",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64030200
    },
/* 011001mmmmmm0011nnnnnnl00ccc0000  bge <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bge",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64030200
    },
/* 011001mmmmmm0011nnnnnn000ccc0000  bge/u <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bge/u",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64030000
    },
/* 011001mmmmmm1011nnnnnnl00ccc0000  bgeu <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgeu/l",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x640b0200
    },
/* 011001mmmmmm1011nnnnnnl00ccc0000  bgeu <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgeu",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x640b0200
    },
/* 011001mmmmmm1011nnnnnn000ccc0000  bgeu/u <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgeu/u",     {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x640b0000
    },
/* 011001mmmmmm0111nnnnnnl00ccc0000  bgt <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgt/l",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64070200
    },
/* 011001mmmmmm0111nnnnnnl00ccc0000  bgt <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgt",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64070200
    },
/* 011001mmmmmm0111nnnnnn000ccc0000  bgt/u <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgt/u",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64070000
    },
/* 011001mmmmmm1111nnnnnnl00ccc0000  bgtu <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgtu/l",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x640f0200
    },
/* 011001mmmmmm1111nnnnnnl00ccc0000  bgtu <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgtu",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x640f0200
    },
/* 011001mmmmmm1111nnnnnn000ccc0000  bgtu/u <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bgtu/u",     {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x640f0000
    },
/* 010001000bbb0001111111dddddd0000  blink <A_TREG_B>,<A_GREG_D>  */
    { "blink",	    {A_TREG_B,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x4401fc00
    },
/* 011001mmmmmm0101nnnnnnl00ccc0000  bne <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bne/l",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64050200
    },
/* 011001mmmmmm0101nnnnnnl00ccc0000  bne <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bne",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64050200
    },
/* 011001mmmmmm0101nnnnnn000ccc0000  bne/u <A_GREG_M>,<A_GREG_N>,<A_TREG_A>  */
    { "bne/u",	    {A_GREG_M,A_GREG_N,A_TREG_A},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x64050000
    },
/* 111001mmmmmm0101ssssssl00ccc0000  bnei <A_GREG_M>,<A_IMMS6>,<A_TREG_A>  */
    { "bnei/l",	    {A_GREG_M,A_IMMS6,A_TREG_A},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe4050200
    },
/* 111001mmmmmm0101ssssssl00ccc0000  bnei <A_GREG_M>,<A_IMMS6>,<A_TREG_A>  */
    { "bnei",	    {A_GREG_M,A_IMMS6,A_TREG_A},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe4050200
    },
/* 111001mmmmmm0101ssssss000ccc0000  bnei/u <A_GREG_M>,<A_IMMS6>,<A_TREG_A>  */
    { "bnei/u",	    {A_GREG_M,A_IMMS6,A_TREG_A},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe4050000
    },
/* 01101111111101011111111111110000  brk  */
    { "brk",	    {A_NONE},			      {OFFSET_NONE},		      0x6ff5fff0
    },
/* 000000mmmmmm1111111111dddddd0000  byterev <A_GREG_M>,<A_GREG_D>  */
    { "byterev",    {A_GREG_M,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x000ffc00
    },
/* 000000mmmmmm0001nnnnnndddddd0000  cmpeq <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "cmpeq",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x00010000
    },
/* 000000mmmmmm0011nnnnnndddddd0000  cmpgt <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "cmpgt",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x00030000
    },
/* 000000mmmmmm0111nnnnnndddddd0000  cmpgtu <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "cmpgtu",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x00070000
    },
/* 001000mmmmmm0001nnnnnnwwwwww0000  cmveq <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "cmveq",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x20010000
    },
/* 001000mmmmmm0101nnnnnnwwwwww0000  cmvne <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "cmvne",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x20050000
    },
/* 000110gggggg0001ggggggffffff0000  fabs.d <A_DREG_G>,<A_DREG_F>  */
    { "fabs.d",	    {A_DREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x18010000
    },
/* 000110gggggg0000ggggggffffff0000  fabs.s <A_FREG_G>,<A_FREG_F>  */
    { "fabs.s",	    {A_FREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x18000000
    },
/* 001101gggggg0001hhhhhhffffff0000  fadd.s <A_DREG_G>,<A_DREG_H>,<A_DREG_F>  */
    { "fadd.d",	    {A_DREG_G,A_DREG_H,A_DREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34010000
    },
/* 001101gggggg0000hhhhhhffffff0000  fadd.s <A_FREG_G>,<A_FREG_H>,<A_FREG_F>  */
    { "fadd.s",	    {A_FREG_G,A_FREG_H,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34000000
    },
/* 001100gggggg1001hhhhhhdddddd0000  fcmpeq.s <A_DREG_G>,<A_DREG_H>,<A_GREG_D>  */
    { "fcmpeq.d",   {A_DREG_G,A_DREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x30090000
    },
/* 001100gggggg1000hhhhhhdddddd0000  fcmpeq.s <A_FREG_G>,<A_FREG_H>,<A_GREG_D>  */
    { "fcmpeq.s",   {A_FREG_G,A_FREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x30080000
    },
/* 001100gggggg1111hhhhhhdddddd0000  fcmpge.d <A_DREG_G>,<A_DREG_H>,<A_GREG_D>  */
    { "fcmpge.d",   {A_DREG_G,A_DREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x300f0000
    },
/* 001100gggggg1110hhhhhhdddddd0000  fcmpge.s <A_FREG_G>,<A_FREG_H>,<A_GREG_D>  */
    { "fcmpge.s",   {A_FREG_G,A_FREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x300e0000
    },
/* 001100gggggg1101hhhhhhdddddd0000  fcmpgt.d <A_DREG_G>,<A_DREG_H>,<A_GREG_D>  */
    { "fcmpgt.d",   {A_DREG_G,A_DREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x300d0000
    },
/* 001100gggggg1100hhhhhhdddddd0000  fcmpgt.s <A_FREG_G>,<A_FREG_H>,<A_GREG_D>  */
    { "fcmpgt.s",   {A_FREG_G,A_FREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x300c0000
    },
/* 001100gggggg1011hhhhhhdddddd0000  fcmpun.d <A_DREG_G>,<A_DREG_H>,<A_GREG_D>  */
    { "fcmpun.d",   {A_DREG_G,A_DREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x300b0000
    },
/* 001100gggggg1010hhhhhhdddddd0000  fcmpun.s <A_FREG_G>,<A_FREG_H>,<A_GREG_D>  */
    { "fcmpun.s",   {A_FREG_G,A_FREG_H,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x300a0000
    },
/* 001110gggggg0111ggggggffffff0000  fcnv.ds <A_DREG_G>,<A_FREG_F>  */
    { "fcnv.ds",    {A_DREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38070000
    },
/* 001110gggggg0110ggggggffffff0000  fcnv.sd <A_FREG_G>,<A_DREG_F>  */
    { "fcnv.sd",    {A_FREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38060000
    },
/* 001101gggggg0101hhhhhhffffff0000  fdiv.d <A_DREG_G>,<A_DREG_H>,<A_DREG_F>  */
    { "fdiv.d",	    {A_DREG_G,A_DREG_H,A_DREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34050000
    },
/* 001101gggggg0100hhhhhhffffff0000  fdiv.s <A_FREG_G>,<A_FREG_H>,<A_FREG_F>  */
    { "fdiv.s",	    {A_FREG_G,A_FREG_H,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34040000
    },
/* 0001111111110010111111ffffff0000  fgetscr <A_FREG_F>  */
    { "fgetscr",    {A_FREG_F},			      {OFFSET_4}, 0x1ff2fc00
    },
/* 000101gggggg0110hhhhhhffffff0000  fipr.s <A_FVREG_G>,<A_FVREG_H>,<A_FREG_F>  */
    { "fipr.s",	    {A_FVREG_G,A_FVREG_H,A_FREG_F},   {OFFSET_20,OFFSET_10,OFFSET_4}, 0x14060000
    },
/* 100111mmmmmmssssssssssffffff0000  fld.d <A_GREG_M>,<A_IMMS10BY8>,<A_DREG_F>  */
    { "fld.d",	    {A_GREG_M,A_IMMS10BY8,A_DREG_F},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x9c000000
    },
/* 100110mmmmmmssssssssssffffff0000  fld.p <A_GREG_M>,<A_IMMS10BY8>,<A_FPREG_F>  */
    { "fld.p",	    {A_GREG_M,A_IMMS10BY8,A_FPREG_F},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x98000000
    },
/* 100101mmmmmmssssssssssffffff0000  fld.s <A_GREG_M>,<A_IMMS10BY4>,<A_FREG_F>  */
    { "fld.s",	    {A_GREG_M,A_IMMS10BY4,A_FREG_F},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x94000000
    },
/* 000111mmmmmm1001nnnnnnffffff0000  fldx.d <A_GREG_M>,<A_GREG_N>,<A_DREG_F>  */
    { "fldx.d",	    {A_GREG_M,A_GREG_N,A_DREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x1c090000
    },
/* 000111mmmmmm1101nnnnnnffffff0000  fldx.p <A_GREG_M>,<A_GREG_N>,<A_FPREG_F>  */
    { "fldx.p",	    {A_GREG_M,A_GREG_N,A_FPREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x1c0d0000
    },
/* 000111mmmmmm1000nnnnnnffffff0000  fldx.s <A_GREG_M>,<A_GREG_N>,<A_FREG_F>  */
    { "fldx.s",	    {A_GREG_M,A_GREG_N,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x1c080000
    },
/* 001110gggggg1110ggggggffffff0000  float.ld <A_FREG_G>,<A_DREG_F>  */
    { "float.ld",   {A_FREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x380e0000
    },
/* 001110gggggg1100ggggggffffff0000  float.ls <A_FREG_G>,<A_FREG_F>  */
    { "float.ls",   {A_FREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x380c0000
    },
/* 001110gggggg1101ggggggffffff0000  float.qd <A_DREG_G>,<A_DREG_F>  */
    { "float.qd",   {A_DREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x380d0000
    },
/* 001110gggggg1111ggggggffffff0000  float.qs <A_DREG_G>,<A_FREG_F>  */
    { "float.qs",   {A_DREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x380f0000
    },
/* 001101gggggg1110hhhhhhqqqqqq0000  fmac.s <A_FREG_G>,<A_FREG_H>,<A_FREG_F>  */
    { "fmac.s",	    {A_FREG_G,A_FREG_H,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x340e0000
    },
/* 001110gggggg0001ggggggffffff0000  fmov.d <A_DREG_G>,<A_DREG_F>  */
    { "fmov.d",	    {A_DREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38010000
    },
/* 001100gggggg0001ggggggdddddd0000  fmov.dq <A_DREG_G>,<A_GREG_D>  */
    { "fmov.dq",    {A_DREG_G,A_REUSE_PREV,A_GREG_D},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x30010000
    },
/* 000111mmmmmm0000111111ffffff0000  fmov.ls <A_GREG_M>,<A_FREG_F>  */
    { "fmov.ls",    {A_GREG_M,A_FREG_F},	      {OFFSET_20,OFFSET_4},	      0x1c00fc00
    },
/* 000111mmmmmm0001111111ffffff0000  fmov.qd <A_GREG_M>,<A_DREG_F>  */
    { "fmov.qd",    {A_GREG_M,A_DREG_F},	      {OFFSET_20,OFFSET_4},	      0x1c01fc00
    },
/* 001110gggggg0000ggggggffffff0000  fmov.s <A_FREG_G>,<A_FREG_F>  */
    { "fmov.s",	    {A_FREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38000000
    },
/* 001100gggggg0000ggggggdddddd0000  fmov.sl <A_FREG_G>,<A_GREG_D>  */
    { "fmov.sl",    {A_FREG_G,A_REUSE_PREV,A_GREG_D},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x30000000
    },
/* 001101gggggg0111hhhhhhffffff0000  fmul.d <A_DREG_G>,<A_DREG_H>,<A_DREG_F>  */
    { "fmul.d",	    {A_DREG_G,A_DREG_H,A_DREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34070000
    },
/* 001101gggggg0110hhhhhhffffff0000  fmul.s <A_FREG_G>,<A_FREG_H>,<A_FREG_F>  */
    { "fmul.s",	    {A_FREG_G,A_FREG_H,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34060000
    },
/* 000110gggggg0011ggggggffffff0000  fneg.d <A_DREG_G>,<A_DREG_F>  */
    { "fneg.d",	    {A_DREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x18030000
    },
/* 000110gggggg0010ggggggffffff0000  fneg.s <A_FREG_G>,<A_FREG_F>  */
    { "fneg.s",	    {A_FREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x18020000
    },
/* 001100gggggg0010gggggg1111110000  fputscr <A_FREG_G>  */
    { "fputscr",    {A_FREG_G,A_REUSE_PREV},	      {OFFSET_20,OFFSET_10},	      0x300203f0
    },
/* 001110gggggg0101ggggggffffff0000  fsqrt.d <A_DREG_G>,<A_DREG_F>  */
    { "fsqrt.d",    {A_DREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38050000
    },
/* 001110gggggg0100ggggggffffff0000  fsqrt.s <A_FREG_G>,<A_FREG_F>  */
    { "fsqrt.s",    {A_FREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38040000
    },
/* 101111mmmmmmsssssssssszzzzzz0000  fst.d <A_GREG_M>,<A_IMMS10BY8>,<A_DREG_F>  */
    { "fst.d",	    {A_GREG_M,A_IMMS10BY8,A_DREG_F},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xbc000000
    },
/* 101110mmmmmmsssssssssszzzzzz0000  fst.p <A_GREG_M>,<A_IMMS10BY8>,<A_FPREG_F>  */
    { "fst.p",	    {A_GREG_M,A_IMMS10BY8,A_FPREG_F},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xb8000000
    },
/* 101101mmmmmmsssssssssszzzzzz0000  fst.s <A_GREG_M>,<A_IMMS10BY4>,<A_FREG_F>  */
    { "fst.s",	    {A_GREG_M,A_IMMS10BY4,A_FREG_F},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xb4000000
    },
/* 001111mmmmmm1001nnnnnnzzzzzz0000  fstx.d <A_GREG_M>,<A_GREG_N>,<A_DREG_F>  */
    { "fstx.d",	    {A_GREG_M,A_GREG_N,A_DREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x3c090000
    },
/* 001111mmmmmm1101nnnnnnzzzzzz0000  fstx.p <A_GREG_M>,<A_GREG_N>,<A_FPREG_F>  */
    { "fstx.p",	    {A_GREG_M,A_GREG_N,A_FPREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x3c0d0000
    },
/* 001111mmmmmm1000nnnnnnzzzzzz0000  fstx.s <A_GREG_M>,<A_GREG_N>,<A_FREG_F>  */
    { "fstx.s",	    {A_GREG_M,A_GREG_N,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x3c080000
    },
/* 001101gggggg0011hhhhhhffffff0000  fsub.d <A_DREG_G>,<A_DREG_H>,<A_DREG_F>  */
    { "fsub.d",	    {A_DREG_G,A_DREG_H,A_DREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34030000
    },
/* 001101gggggg0010hhhhhhffffff0000  fsub.s <A_FREG_G>,<A_FREG_H>,<A_FREG_F>  */
    { "fsub.s",	    {A_FREG_G,A_FREG_H,A_FREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x34020000
    },
/* 001110gggggg1011ggggggffffff0000  ftrc.dl <A_DREG_G>,<A_FREG_F>  */
    { "ftrc.dl",    {A_DREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x380b0000
    },
/* 001110gggggg1001ggggggffffff0000  ftrc.dq <A_DREG_G>,<A_DREG_F>  */
    { "ftrc.dq",    {A_DREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38090000
    },
/* 001110gggggg1000ggggggffffff0000  ftrc.sl <A_FREG_G>,<A_FREG_F>  */
    { "ftrc.sl",    {A_FREG_G,A_REUSE_PREV,A_FREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x38080000
    },
/* 001110gggggg1010ggggggffffff0000  ftrc.sq <A_FREG_G>,<A_DREG_F>  */
    { "ftrc.sq",    {A_FREG_G,A_REUSE_PREV,A_DREG_F},    {OFFSET_20,OFFSET_10,OFFSET_4}, 0x380a0000
    },
/* 000101gggggg1110hhhhhhffffff0000  ftrv.s <A_FMREG_G>,<A_FVREG_H>,<A_FVREG_F>  */
    { "ftrv.s",	    {A_FMREG_G,A_FVREG_H,A_FVREG_F},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x140e0000
    },
/* 110000mmmmmm1111ssssssdddddd0000  getcfg <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "getcfg",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc00f0000
    },
/* 001001kkkkkk1111111111dddddd0000  getcon <A_CREG_K>,<A_GREG_M>  */
    { "getcon",	    {A_CREG_K,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x240ffc00
    },
/* 010001rrrbbb0101111111dddddd0000  gettr <A_TREG_A>,<A_GREG_D>  */
    { "gettr",	    {A_TREG_B,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x4405fc00
    },
/* 111000mmmmmm0101ssssss1111110000  icbi <A_GREG_M>,<A_IMMS6BY32>  */
    { "icbi",	    {A_GREG_M,A_IMMS6BY32},	      {OFFSET_20,OFFSET_10},	      0xe00503f0
    },
/* 100000mmmmmmssssssssssdddddd0000  ld.b <A_GREG_M>,<A_IMMS10BY1>,<A_GREG_D>  */
    { "ld.b",	    {A_GREG_M,A_IMMS10BY1,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x80000000
    },
/* 100010mmmmmmssssssssssdddddd0000  ld.l <A_GREG_M>,<A_IMMS10BY4>,<A_GREG_D>  */
    { "ld.l",	    {A_GREG_M,A_IMMS10BY4,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x88000000
    },
/* 100011mmmmmmssssssssssdddddd0000  ld.q <A_GREG_M>,<A_IMMS10BY8>,<A_GREG_D>  */
    { "ld.q",	    {A_GREG_M,A_IMMS10BY8,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x8c000000
    },
/* 100100mmmmmmssssssssssdddddd0000  ld.ub <A_GREG_M>,<A_IMMS10BY1>,<A_GREG_D>  */
    { "ld.ub",	    {A_GREG_M,A_IMMS10BY1,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x90000000
    },
/* 101100mmmmmmssssssssssdddddd0000  ld.uw <A_GREG_M>,<A_IMMS10BY2>,<A_GREG_D>  */
    { "ld.uw",	    {A_GREG_M,A_IMMS10BY2,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xb0000000
    },
/* 100001mmmmmmssssssssssdddddd0000  ld.w <A_GREG_M>,<A_IMMS10BY2>,<A_GREG_D>  */
    { "ld.w",	    {A_GREG_M,A_IMMS10BY2,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0x84000000
    },
/* 110000mmmmmm0110ssssssdddddd0000  ldhi.l <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "ldhi.l",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc0060000
    },
/* 110000mmmmmm0111ssssssdddddd0000  ldhi.q <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "ldhi.q",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc0070000
    },
/* 110000mmmmmm0010ssssssdddddd0000  ldlo.l <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "ldlo.l",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc0020000
    },
/* 110000mmmmmm0011ssssssdddddd0000  ldlo.q <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "ldlo.q",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc0030000
    },
/* 010000mmmmmm0000nnnnnndddddd0000  ldx.b <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "ldx.b",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x40000000
    },
/* 010000mmmmmm0010nnnnnndddddd0000  ldx.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "ldx.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x40020000
    },
/* 010000mmmmmm0011nnnnnndddddd0000  ldx.q <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "ldx.q",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x40030000
    },
/* 010000mmmmmm0100nnnnnndddddd0000  ldx.ub <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "ldx.ub",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x40040000
    },
/* 010000mmmmmm0101nnnnnndddddd0000  ldx.uw <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "ldx.uw",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x40050000
    },
/* 010000mmmmmm0001nnnnnndddddd0000  ldx.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "ldx.w",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x40010000
    },
/* 001010mmmmmm1010111111dddddd0000  mabs.l <A_GREG_M>,<A_GREG_D>  */
    { "mabs.l",	    {A_GREG_M,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x280afc00
    },
/* 001010mmmmmm1001111111dddddd0000  mabs.w <A_GREG_M>,<A_GREG_D>  */
    { "mabs.w",	    {A_GREG_M,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x2809fc00
    },
/* 000010mmmmmm0010nnnnnndddddd0000  madd.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D> */
    { "madd.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x08020000
    },
/* 000010mmmmmm0001nnnnnndddddd0000  madd.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "madd.w",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x08010000
    },
/* 000010mmmmmm0110nnnnnndddddd0000  madds.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "madds.l",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x08060000
    },
/* 000010mmmmmm0100nnnnnndddddd0000  madds.ub <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "madds.ub",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x08040000
    },
/* 000010mmmmmm0101nnnnnndddddd0000  madds.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "madds.w",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x08050000
    },
/* 001010mmmmmm0000nnnnnndddddd0000  mcmpeq.b <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmpeq.b",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28000000
    },
/* 001010mmmmmm0010nnnnnndddddd0000  mcmpeq.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmpeq.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28020000
    },
/* 001010mmmmmm0001nnnnnndddddd0000  mcmpeq.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmpeq.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28010000
    },
/* 001010mmmmmm0110nnnnnndddddd0000  mcmpgt.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmpgt.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28060000
    },
/* 001010mmmmmm0100nnnnnndddddd0000  mcmpgt.ub <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmpgt.ub",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28040000
    },
/* 001010mmmmmm0101nnnnnndddddd0000  mcmpgt.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmpgt.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28050000
    },
/* 010010mmmmmm0011nnnnnnwwwwww0000  mcmv <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcmv",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x48030000
    },
/* 010011mmmmmm1101nnnnnndddddd0000  mcnvs.lw <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcnvs.lw",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c0d0000
    },
/* 010011mmmmmm1000nnnnnndddddd0000  mcnvs.wb <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcnvs.wb",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c080000
    },
/* 010011mmmmmm1100nnnnnndddddd0000  mcnvs.wub <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mcnvs.wub",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c0c0000
    },
/* 001010mmmmmm0111nnnnnndddddd0000  mextr1 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr1",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x28070000
    },
/* 001010mmmmmm1011nnnnnndddddd0000  mextr2 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr2",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x280b0000
    },
/* 001010mmmmmm1111nnnnnndddddd0000  mextr3 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr3",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x280f0000
    },
/* 001011mmmmmm0011nnnnnndddddd0000  mextr4 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr4",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c030000
    },
/* 001011mmmmmm0111nnnnnndddddd0000  mextr5 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr5",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c070000
    },
/* 001011mmmmmm1011nnnnnndddddd0000  mextr6 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr6",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c0b0000
    },
/* 001011mmmmmm1111nnnnnndddddd0000  mextr7 <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mextr7",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c0f0000
    },
/* 010010mmmmmm0001nnnnnnwwwwww0000  mmacfx.wl <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmacfx.wl",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x48010000
    },
/* 010010mmmmmm0101nnnnnnwwwwww0000  mmacnfx.wl <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmacnfx.wl", {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x48050000
    },
/* 010011mmmmmm0010nnnnnndddddd0000  mmul.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmul.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c020000
    },
/* 010011mmmmmm0001nnnnnndddddd0000  mmul.m <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmul.w",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c010000
    },
/* 010011mmmmmm0110nnnnnndddddd0000  mmulfx.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmulfx.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c060000
    },
/* 010011mmmmmm0101nnnnnndddddd0000  mmulfx.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmulfx.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c050000
    },
/* 010011mmmmmm1001nnnnnndddddd0000  mmulfxrp.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmulfxrp.w", {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c090000
    },
/* 010011mmmmmm1110nnnnnndddddd0000  mmulhi.wl <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmulhi.wl",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c0e0000
    },
/* 010011mmmmmm1010nnnnnndddddd0000  mmullo.wl <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmullo.wl",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x4c0a0000
    },
/* 010010mmmmmm1001nnnnnnwwwwww0000  mmulsum.wq <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mmulsum.wq", {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x48090000
    },
/* 110011ssssssssssssssssdddddd0000  movi <A_IMMS16>,<A_GREG_D>  */
    { "movi",	    {A_IMMS16,A_GREG_D}, {OFFSET_10,OFFSET_4}, SHMEDIA_MOVI_OPC
    },
/* 001010mmmmmm1101nnnnnndddddd0000  mperm.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mperm.w",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x280d0000
    },
/* 010010mmmmmm0000nnnnnnwwwwww0000  msad.ubq <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "msad.ubq",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x48000000
    },
/* 000011mmmmmm1010nnnnnndddddd0000  mshard.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshard.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c0a0000
    },
/* 000011mmmmmm1001nnnnnndddddd0000  mshard.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshard.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c090000
    },
/* 000011mmmmmm1011nnnnnndddddd0000  mshards.q <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshards.q",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c0b0000
    },
/* 001011mmmmmm0100nnnnnndddddd0000  mshfhi.b <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshfhi.b",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c040000
    },
/* 001011mmmmmm0110nnnnnndddddd0000  mshfhi.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshfhi.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c060000
    },
/* 001011mmmmmm0101nnnnnndddddd0000  mshfhi.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshfhi.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c050000
    },
/* 001011mmmmmm0000nnnnnndddddd0000  mshflo.b <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshflo.b",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c000000
    },
/* 001011mmmmmm0010nnnnnndddddd0000  mshflo.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshflo.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c020000
    },
/* 001011mmmmmm0001nnnnnndddddd0000  mshflo.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshflo.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x2c010000
    },
/* 000011mmmmmm0010nnnnnndddddd0000  mshlld.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshlld.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c020000
    },
/* 000011mmmmmm0001nnnnnndddddd0000  mshlld.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshlld.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c010000
    },
/* 000011mmmmmm0110nnnnnndddddd0000  mshalds.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshalds.l",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c060000
    },
/* 000011mmmmmm0101nnnnnndddddd0000  mshalds.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshalds.w",  {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c050000
    },
/* 000011mmmmmm1110nnnnnndddddd0000  mshlrd.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshlrd.l",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c0e0000
    },
/* 000011mmmmmm1101nnnnnndddddd0000  mshlrd.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mshlrd.w",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x0c0d0000
    },
/* 000010mmmmmm1010nnnnnndddddd0000  msub.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "msub.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x080a0000
    },
/* 000010mmmmmm1001nnnnnndddddd0000  msub.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "msub.w",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x08090000
    },
/* 000010mmmmmm1110nnnnnndddddd0000  msubs.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "msubs.l",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x080e0000
    },
/* 000010mmmmmm1100nnnnnndddddd0000  msubs.ub <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "msubs.ub",   {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x080c0000
    },
/* 000010mmmmmm1101nnnnnndddddd0000  msubs.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "msubs.w",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x080d0000
    },
/* 000001mmmmmm1110nnnnnndddddd0000  muls.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "muls.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x040e0000
    },
/* 000000mmmmmm1110nnnnnndddddd0000  mulu.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "mulu.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x000e0000
    },
/* 01101111111100001111111111110000  nop   */
    { "nop",	    {A_NONE},			      {OFFSET_NONE},
      SHMEDIA_NOP_OPC
    },
/* 000000mmmmmm1101111111dddddd0000  nsb <A_GREG_M>,<A_GREG_D>   */
    { "nsb",	    {A_GREG_M,A_GREG_D},	      {OFFSET_20,OFFSET_4},	      0x000dfc00
    },
/* 111000mmmmmm1001ssssss1111110000  ocbi <A_GREG_M>,<A_IMMS6BY32>  */
    { "ocbi",	    {A_GREG_M,A_IMMS6BY32},	      {OFFSET_20,OFFSET_10},	      0xe00903f0
    },
/* 111000mmmmmm1000ssssss1111110000  ocbp <A_GREG_M>,<A_IMMS6BY32>  */
    { "ocbp",	    {A_GREG_M,A_IMMS6BY32},	      {OFFSET_20,OFFSET_10},	      0xe00803f0
    },
/* 111000mmmmmm1100ssssss1111110000  ocbwb <A_GREG_M>,<A_IMMS6BY32>  */
    { "ocbwb",	    {A_GREG_M,A_IMMS6BY32},	      {OFFSET_20,OFFSET_10},	      0xe00c03f0
    },
/* 000001mmmmmm1001nnnnnndddddd0000  or <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "or",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04090000
    },
/* 110111mmmmmmssssssssssdddddd0000  ori <A_GREG_M>,<A_IMMS10>,<A_GREG_D>  */
    { "ori",	    {A_GREG_M,A_IMMS10,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0xdc000000
    },
/* 111000mmmmmm0001ssssss1111110000  prefi <A_GREG_M>,<A_IMMS6BY32>  */
    { "prefi",	    {A_GREG_M,A_IMMS6BY32},	      {OFFSET_20,OFFSET_10},	      0xe00103f0
    },
/* 111010sssssssssssssssslrraaa0000  pta <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "pta/l",	    {A_PCIMMS16BY4,A_TREG_A}, {OFFSET_10,OFFSET_4},
      SHMEDIA_PTA_OPC | SHMEDIA_LIKELY_BIT
    },
/* 111010sssssssssssssssslrraaa0000  pta <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "pta",	    {A_PCIMMS16BY4,A_TREG_A}, {OFFSET_10,OFFSET_4},
      SHMEDIA_PTA_OPC | SHMEDIA_LIKELY_BIT
    },
/* 111010ssssssssssssssss0rraaa0000  pta/u <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "pta/u",	    {A_PCIMMS16BY4,A_TREG_A}, {OFFSET_10,OFFSET_4},
      SHMEDIA_PTA_OPC
    },
/* 0110101111110001nnnnnnl00aaa0000  ptabs <A_GREG_M>,<A_TREG_A>  */
    { "ptabs/l",    {A_GREG_N,A_TREG_A},      {OFFSET_10,OFFSET_4}, 0x6bf10200
    },
/* 0110101111110001nnnnnnl00aaa0000  ptabs <A_GREG_M>,<A_TREG_A>  */
    { "ptabs",	    {A_GREG_N,A_TREG_A},      {OFFSET_10,OFFSET_4}, 0x6bf10200
    },
/* 0110101111110001nnnnnn000aaa0000  ptabs/u <A_GREG_M>,<A_TREG_A>  */
    { "ptabs/u",    {A_GREG_N,A_TREG_A},      {OFFSET_10,OFFSET_4}, 0x6bf10000
    },
/* 111011sssssssssssssssslrraaa0000  ptb <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "ptb/l",	    {A_PCIMMS16BY4,A_TREG_A}, {OFFSET_10,OFFSET_4},
      SHMEDIA_PTB_OPC | SHMEDIA_LIKELY_BIT
    },
/* 111011sssssssssssssssslrraaa0000  ptb <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "ptb",	    {A_PCIMMS16BY4,A_TREG_A}, {OFFSET_10,OFFSET_4},
      SHMEDIA_PTB_OPC | SHMEDIA_LIKELY_BIT
    },
/* 111011ssssssssssssssss0rraaa0000  ptb/u <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "ptb/u",	    {A_PCIMMS16BY4,A_TREG_A}, {OFFSET_10,OFFSET_4},
      SHMEDIA_PTB_OPC
    },
/* 111010sssssssssssssssslrraaa0000  pt/l <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "pt/l",	    {A_PCIMMS16BY4_PT,A_TREG_A},
      {OFFSET_10,OFFSET_4}, SHMEDIA_PT_OPC | SHMEDIA_LIKELY_BIT
    },
/* 111010sssssssssssssssslrraaa0000  pt <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "pt",	    {A_PCIMMS16BY4_PT,A_TREG_A},
      {OFFSET_10,OFFSET_4}, SHMEDIA_PT_OPC | SHMEDIA_LIKELY_BIT
    },
/* 111010ssssssssssssssss0rraaa0000  pt/u <A_PCIMMS16BY4>,<A_TREG_A>  */
    { "pt/u",	    {A_PCIMMS16BY4_PT,A_TREG_A},
      {OFFSET_10,OFFSET_4}, SHMEDIA_PT_OPC
    },
/* 0110101111110101nnnnnnl00aaa0000  ptrel <A_GREG_M>,<A_TREG_A>  */
    { "ptrel/l",    {A_GREG_N,A_TREG_A},      {OFFSET_10,OFFSET_4},
      SHMEDIA_PTREL_OPC | SHMEDIA_LIKELY_BIT
    },
/* 0110101111110101nnnnnnl00aaa0000  ptrel <A_GREG_M>,<A_TREG_A>  */
    { "ptrel",	    {A_GREG_N,A_TREG_A},      {OFFSET_10,OFFSET_4},
      SHMEDIA_PTREL_OPC | SHMEDIA_LIKELY_BIT
    },
/* 0110101111110101nnnnnn000aaa0000  ptrel/u <A_GREG_M>,<A_TREG_A>  */
    { "ptrel/u",    {A_GREG_N,A_TREG_A},      {OFFSET_10,OFFSET_4},
      SHMEDIA_PTREL_OPC
    },
/* 111000mmmmmm1111ssssssyyyyyy0000  putcfg <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "putcfg",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe00f0000
    },
/* 011011mmmmmm1111111111jjjjjj0000  putcon <A_GREG_M>,<A_CREG_J>  */
    { "putcon",	    {A_GREG_M,A_CREG_J},      {OFFSET_20,OFFSET_4}, 0x6c0ffc00
    },
/* 01101111111100111111111111110000  rte   */
    { "rte",	    {A_NONE},		      {OFFSET_NONE},	    0x6ff3fff0
    },
/* 000001mmmmmm0111nnnnnndddddd0000  shard <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "shard",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04070000
    },
/* 000001mmmmmm0110nnnnnndddddd0000  shard.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "shard.l",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04060000
    },
/* 110001mmmmmm0111ssssssdddddd0000  shari <A_GREG_M>,<A_IMMU6>,<A_GREG_D>  */
    { "shari",	    {A_GREG_M,A_IMMU6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc4070000
    },
/* 110001mmmmmm0110ssssssdddddd0000  shari <A_GREG_M>,<A_IMMU6>,<A_GREG_D>  */
    { "shari.l",    {A_GREG_M,A_IMMU6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc4060000
    },
/* 000001mmmmmm0001nnnnnndddddd0000  shlld <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "shlld",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04010000
    },
/* 000001mmmmmm0000nnnnnndddddd0000  shlld.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "shlld.l",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04000000
    },
/* 110001mmmmmm0001ssssssdddddd0000  shlli <A_GREG_M>,<A_IMMU6>,<A_GREG_D>  */
    { "shlli",	    {A_GREG_M,A_IMMU6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc4010000
    },
/* 110001mmmmmm0000ssssssdddddd0000  shlli.l <A_GREG_M>,<A_IMMU5>,<A_GREG_D>  */
    { "shlli.l",    {A_GREG_M,A_IMMU5,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc4000000
    },
/* 000001mmmmmm0011nnnnnndddddd0000  shlrd <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "shlrd",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04030000
    },
/* 000001mmmmmm0010nnnnnndddddd0000  shlrd.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "shlrd.l",    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x04020000
    },
/* 110001mmmmmm0011ssssssdddddd0000  shlri <A_GREG_M>,<A_IMMU6>,<A_GREG_D>  */
    { "shlri",	    {A_GREG_M,A_IMMU6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc4030000
    },
/* 110001mmmmmm0010ssssssdddddd0000  shlri.l <A_GREG_M>,<A_IMMU5>,<A_GREG_D>  */
    { "shlri.l",    {A_GREG_M,A_IMMU5,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc4020000
    },
/* 110010sssssssssssssssswwwwww0000  shori <A_IMMU16>,<A_GREG_D>  */
    { "shori",	    {A_IMMU16,A_GREG_D}, {OFFSET_10,OFFSET_4}, SHMEDIA_SHORI_OPC
    },
/* 01101111111101111111111111110000  sleep   */
    { "sleep",      {A_NONE},		 {OFFSET_NONE}, 0x6ff7fff0
    },
/* 101000mmmmmmssssssssssdddddd0000  st.b <A_GREG_M>,<A_IMMS10BY1>,<A_GREG_D>  */
    { "st.b",	    {A_GREG_M,A_IMMS10BY1,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xa0000000
    },
/* 101010mmmmmmssssssssssdddddd0000  st.l <A_GREG_M>,<A_IMMS10BY4>,<A_GREG_D>  */
    { "st.l",	    {A_GREG_M,A_IMMS10BY4,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xa8000000
    },
/* 101011mmmmmmssssssssssdddddd0000  st.q <A_GREG_M>,<A_IMMS10BY8>,<A_GREG_D>  */
    { "st.q",	    {A_GREG_M,A_IMMS10BY8,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xac000000
    },
/* 101001mmmmmmssssssssssdddddd0000  st.w <A_GREG_M>,<A_IMMS10BY2>,<A_GREG_D>  */
    { "st.w",	    {A_GREG_M,A_IMMS10BY2,A_GREG_D},  {OFFSET_20,OFFSET_10,OFFSET_4}, 0xa4000000
    },
/* 111000mmmmmm0110ssssssdddddd0000  sthi.l <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "sthi.l",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe0060000
    },
/* 111000mmmmmm0111ssssssdddddd0000  sthi.q <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "sthi.q",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe0070000
    },
/* 111000mmmmmm0010ssssssdddddd0000  stlo.l <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "stlo.l",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe0020000
    },
/* 111000mmmmmm0011ssssssdddddd0000  stlo.q <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "stlo.q",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xe0030000
    },
/* 011000mmmmmm0000nnnnnndddddd0000  stx.b <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "stx.b",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x60000000
    },
/* 011000mmmmmm0010nnnnnndddddd0000  stx.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "stx.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x60020000
    },
/* 011000mmmmmm0011nnnnnndddddd0000  stx.q <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "stx.q",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x60030000
    },
/* 011000mmmmmm0001nnnnnndddddd0000  stx.w <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "stx.w",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x60010000
    },
/* 000000mmmmmm1011nnnnnndddddd0000  sub <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "sub",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x000b0000
    },
/* 000000mmmmmm1010nnnnnndddddd0000  sub.l <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "sub.l",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x000a0000
    },
/* 001000mmmmmm0011nnnnnnwwwwww0000  swap.q <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "swap.q",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x20030000
    },
/* 01101111111100101111111111110000  synci   */
    { "synci",	    {A_NONE},			      {OFFSET_NONE},		      0x6ff2fff0
    },
/* 01101111111101101111111111110000  synco   */
    { "synco",	    {A_NONE},			      {OFFSET_NONE},		      0x6ff6fff0
    },
/* 011011mmmmmm00011111111111110000  trapa <A_GREG_M>   */
    { "trapa",	    {A_GREG_M},			      {OFFSET_20}, 0x6c01fff0
    },
/* 000001mmmmmm1101nnnnnndddddd0000  xor <A_GREG_M>,<A_GREG_N>,<A_GREG_D>  */
    { "xor",	    {A_GREG_M,A_GREG_N,A_GREG_D},     {OFFSET_20,OFFSET_10,OFFSET_4}, 0x040d0000
    },
/* 110001mmmmmm1101ssssssdddddd0000  xori <A_GREG_M>,<A_IMMS6>,<A_GREG_D>  */
    { "xori",	    {A_GREG_M,A_IMMS6,A_GREG_D},      {OFFSET_20,OFFSET_10,OFFSET_4}, 0xc40d0000
    },

    { NULL, {}, {}, 0 }
};

/* Predefined control register names as per SH-5/ST50-005-08.  */
const shmedia_creg_info shmedia_creg_table[] = {
  { 0, "sr" },
  { 1, "ssr" },
  { 2, "pssr" },

  { 4, "intevt" },
  { 5, "expevt" },
  { 6, "pexpevt" },
  { 7, "tra" },
  { 8, "spc" },
  { 9, "pspc" },
  { 10, "resvec" },
  { 11, "vbr" },

  { 13, "tea" },

  { 16, "dcr" },
  { 17, "kcr0" },
  { 18, "kcr1" },

  { 62, "ctc" },
  { 63, "usr" },
  { -1, (char *) 0 }
};

