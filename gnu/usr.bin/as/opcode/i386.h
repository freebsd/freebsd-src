/* i386-opcode.h -- Intel 80386 opcode table
   Copyright (C) 1989, 1991, Free Software Foundation.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* $Id: i386.h,v 1.4.2.2 1998/02/15 15:20:01 jkh Exp $ */

static const template i386_optab[] = {

#define _ None
/* move instructions */
{ "mov", 2, 0xa0, _, DW|NoModrm, Disp32, Acc, 0 },
{ "mov", 2, 0x88, _, DW|Modrm, Reg, Reg|Mem, 0 },
{ "mov", 2, 0xb0, _, ShortFormW, Imm, Reg, 0 },
{ "mov", 2, 0xc6, _,  W|Modrm,  Imm, Reg|Mem, 0 },
{ "mov", 2, 0x8c, _, D|Modrm,  SReg3|SReg2, WordReg|WordMem, 0 },
/* move to/from control debug registers */
{ "mov", 2, 0x0f20, _, D|Modrm, Control, Reg32, 0},
{ "mov", 2, 0x0f21, _, D|Modrm, Debug, Reg32, 0},
{ "mov", 2, 0x0f24, _, D|Modrm, Test, Reg32, 0},

/* move with sign extend */
/* "movsbl" & "movsbw" must not be unified into "movsb" to avoid
   conflict with the "movs" string move instruction.  Thus,
   {"movsb", 2, 0x0fbe, _, ReverseRegRegmem|Modrm, Reg8|Mem,  Reg16|Reg32, 0},
   is not kosher; we must seperate the two instructions. */
{"movsbl", 2, 0x0fbe, _, ReverseRegRegmem|Modrm, Reg8|Mem,  Reg32, 0},
{"movsbw", 2, 0x660fbe, _, ReverseRegRegmem|Modrm, Reg8|Mem,  Reg16, 0},
{"movswl", 2, 0x0fbf, _, ReverseRegRegmem|Modrm, Reg16|Mem, Reg32, 0},

/* move with zero extend */
{"movzb", 2, 0x0fb6, _, ReverseRegRegmem|Modrm, Reg8|Mem, Reg16|Reg32, 0},
{"movzwl", 2, 0x0fb7, _, ReverseRegRegmem|Modrm, Reg16|Mem, Reg32, 0},

/* push instructions */
{"push", 1, 0x50, _, ShortForm, WordReg,0,0 },
{"push", 1, 0xff, 0x6,  Modrm, WordReg|WordMem, 0, 0 },
{"push", 1, 0x6a, _, NoModrm, Imm8S, 0, 0},
{"push", 1, 0x68, _, NoModrm, Imm16|Imm32, 0, 0},
{"push", 1, 0x06, _,  Seg2ShortForm, SReg2,0,0 },
{"push", 1, 0x0fa0, _, Seg3ShortForm, SReg3,0,0 },
/* push all */
{"pusha", 0, 0x60, _, NoModrm, 0, 0, 0 },

/* pop instructions */
{"pop", 1, 0x58, _, ShortForm, WordReg,0,0 },
{"pop", 1, 0x8f, 0x0,  Modrm, WordReg|WordMem, 0, 0 },
#define POP_SEG_SHORT 0x7
{"pop", 1, 0x07, _,  Seg2ShortForm, SReg2,0,0 },
{"pop", 1, 0x0fa1, _, Seg3ShortForm, SReg3,0,0 },
/* pop all */
{"popa", 0, 0x61, _, NoModrm, 0, 0, 0 },

/* xchg exchange instructions
   xchg commutes:  we allow both operand orders */
{"xchg", 2, 0x90, _, ShortForm, WordReg, Acc, 0 },
{"xchg", 2, 0x90, _, ShortForm, Acc, WordReg, 0 },
{"xchg", 2, 0x86, _, W|Modrm, Reg, Reg|Mem, 0 },
{"xchg", 2, 0x86, _, W|Modrm, Reg|Mem, Reg, 0 },

/* in/out from ports */
{"in", 2, 0xe4, _, W|NoModrm, Imm8, Acc, 0 },
{"in", 2, 0xec, _, W|NoModrm, InOutPortReg, Acc, 0 },
{"out", 2, 0xe6, _, W|NoModrm, Acc, Imm8, 0 },
{"out", 2, 0xee, _, W|NoModrm, Acc, InOutPortReg, 0 },

#if 0
{"inb",  1, 0xe4, _, NoModrm, Imm8, 0, 0 },
{"inb",  1, 0xec, _, NoModrm, WordMem, 0, 0 },
{"inw",  1, 0x66e5, _, NoModrm, Imm8, 0, 0 },
{"inw",  1, 0x66ed, _, NoModrm, WordMem, 0, 0 },
{"outb", 1, 0xe6, _, NoModrm, Imm8, 0, 0 },
{"outb", 1, 0xee, _, NoModrm, WordMem, 0, 0 },
{"outw", 1, 0x66e7, _, NoModrm, Imm8, 0, 0 },
{"outw", 1, 0x66ef, _, NoModrm, WordMem, 0, 0 },
#endif

/* load effective address */
{"lea", 2, 0x8d, _, Modrm, WordMem, WordReg, 0 },

/* load segment registers from memory */
{"lds", 2, 0xc5, _, Modrm, Mem, Reg32, 0},
{"les", 2, 0xc4, _, Modrm, Mem, Reg32, 0},
{"lfs", 2, 0x0fb4, _, Modrm, Mem, Reg32, 0},
{"lgs", 2, 0x0fb5, _, Modrm, Mem, Reg32, 0},
{"lss", 2, 0x0fb2, _, Modrm, Mem, Reg32, 0},

/* flags register instructions */
{"clc", 0, 0xf8, _, NoModrm, 0, 0, 0},
{"cld", 0, 0xfc, _, NoModrm, 0, 0, 0},
{"cli", 0, 0xfa, _, NoModrm, 0, 0, 0},
{"clts", 0, 0x0f06, _, NoModrm, 0, 0, 0},
{"cmc", 0, 0xf5, _, NoModrm, 0, 0, 0},
{"lahf", 0, 0x9f, _, NoModrm, 0, 0, 0},
{"sahf", 0, 0x9e, _, NoModrm, 0, 0, 0},
{"pushf", 0, 0x9c, _, NoModrm, 0, 0, 0},
{"popf", 0, 0x9d, _, NoModrm, 0, 0, 0},
{"stc", 0, 0xf9, _, NoModrm, 0, 0, 0},
{"std", 0, 0xfd, _, NoModrm, 0, 0, 0},
{"sti", 0, 0xfb, _, NoModrm, 0, 0, 0},

{"add", 2, 0x0,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"add", 2, 0x83, 0,  Modrm, Imm8S, WordReg|WordMem, 0},
{"add", 2, 0x4,  _,  W|NoModrm, Imm,  Acc,    0},
{"add", 2, 0x80, 0, W|Modrm, Imm, Reg|Mem, 0},

{"inc", 1, 0x40, _, ShortForm, WordReg, 0, 0},
{"inc", 1, 0xfe, 0, W|Modrm, Reg|Mem, 0, 0},

{"sub", 2, 0x28,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"sub", 2, 0x83, 5,  Modrm, Imm8S, WordReg|WordMem, 0},
{"sub", 2, 0x2c,  _,  W|NoModrm, Imm,  Acc,    0},
{"sub", 2, 0x80, 5,  W|Modrm, Imm, Reg|Mem, 0},

{"dec", 1, 0x48, _, ShortForm, WordReg, 0, 0},
{"dec", 1, 0xfe, 1, W|Modrm, Reg|Mem, 0, 0},

{"sbb", 2, 0x18,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"sbb", 2, 0x83, 3,  Modrm, Imm8S, WordReg|WordMem, 0},
{"sbb", 2, 0x1c,  _,  W|NoModrm, Imm,  Acc,    0},
{"sbb", 2, 0x80, 3,  W|Modrm, Imm, Reg|Mem, 0},

{"cmp", 2, 0x38,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"cmp", 2, 0x83, 7,  Modrm, Imm8S, WordReg|WordMem, 0},
{"cmp", 2, 0x3c,  _,  W|NoModrm, Imm,  Acc,    0},
{"cmp", 2, 0x80, 7,  W|Modrm, Imm, Reg|Mem, 0},

{"test", 2, 0x84, _, W|Modrm, Reg|Mem, Reg, 0},
{"test", 2, 0x84, _, W|Modrm, Reg, Reg|Mem, 0},
{"test", 2, 0xa8, _, W|NoModrm, Imm, Acc, 0},
{"test", 2, 0xf6, 0, W|Modrm, Imm, Reg|Mem, 0},

{"and", 2, 0x20,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"and", 2, 0x83, 4,  Modrm, Imm8S, WordReg|WordMem, 0},
{"and", 2, 0x24,  _,  W|NoModrm, Imm,  Acc,    0},
{"and", 2, 0x80, 4,  W|Modrm, Imm, Reg|Mem, 0},

{"or", 2, 0x08,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"or", 2, 0x83, 1,  Modrm, Imm8S, WordReg|WordMem, 0},
{"or", 2, 0x0c,  _,  W|NoModrm, Imm,  Acc,    0},
{"or", 2, 0x80, 1,  W|Modrm, Imm, Reg|Mem, 0},

{"xor", 2, 0x30,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"xor", 2, 0x83, 6,  Modrm, Imm8S, WordReg|WordMem, 0},
{"xor", 2, 0x34,  _,  W|NoModrm, Imm,  Acc,    0},
{"xor", 2, 0x80, 6,  W|Modrm, Imm, Reg|Mem, 0},

{"adc", 2, 0x10,  _, DW|Modrm, Reg, Reg|Mem, 0},
{"adc", 2, 0x83, 2,  Modrm, Imm8S, WordReg|WordMem, 0},
{"adc", 2, 0x14,  _,  W|NoModrm, Imm,  Acc,    0},
{"adc", 2, 0x80, 2,  W|Modrm, Imm, Reg|Mem, 0},

{"neg", 1, 0xf6, 3, W|Modrm, Reg|Mem, 0, 0},
{"not", 1, 0xf6, 2, W|Modrm, Reg|Mem, 0, 0},

{"aaa", 0, 0x37, _, NoModrm, 0, 0, 0},
{"aas", 0, 0x3f, _, NoModrm, 0, 0, 0},
{"daa", 0, 0x27, _, NoModrm, 0, 0, 0},
{"das", 0, 0x2f, _, NoModrm, 0, 0, 0},
{"aad", 0, 0xd50a, _, NoModrm, 0, 0, 0},
{"aam", 0, 0xd40a, _, NoModrm, 0, 0, 0},

/* conversion insns */
/* conversion:  intel naming */
{"cbw", 0, 0x6698, _, NoModrm, 0, 0, 0},
{"cwd", 0, 0x6699, _, NoModrm, 0, 0, 0},
{"cwde", 0, 0x98, _, NoModrm, 0, 0, 0},
{"cdq", 0, 0x99, _, NoModrm, 0, 0, 0},
/*  att naming */
{"cbtw", 0, 0x6698, _, NoModrm, 0, 0, 0},
{"cwtl", 0, 0x98, _, NoModrm, 0, 0, 0},
{"cwtd", 0, 0x6699, _, NoModrm, 0, 0, 0},
{"cltd", 0, 0x99, _, NoModrm, 0, 0, 0},

/* Warning! the mul/imul (opcode 0xf6) must only have 1 operand!  They are
   expanding 64-bit multiplies, and *cannot* be selected to accomplish
   'imul %ebx, %eax' (opcode 0x0faf must be used in this case)
   These multiplies can only be selected with single opearnd forms. */
{"mul",  1, 0xf6, 4, W|Modrm, Reg|Mem, 0, 0},
{"imul", 1, 0xf6, 5, W|Modrm, Reg|Mem, 0, 0},




/* imulKludge here is needed to reverse the i.rm.reg & i.rm.regmem fields.
   These instructions are exceptions:  'imul $2, %eax, %ecx' would put
   '%eax' in the reg field and '%ecx' in the regmem field if we did not
   switch them. */
{"imul", 2, 0x0faf, _, Modrm|ReverseRegRegmem, WordReg|Mem, WordReg, 0},
{"imul", 3, 0x6b, _, Modrm|ReverseRegRegmem, Imm8S, WordReg|Mem, WordReg},
{"imul", 3, 0x69, _, Modrm|ReverseRegRegmem, Imm16|Imm32, WordReg|Mem, WordReg},
/*
  imul with 2 operands mimicks imul with 3 by puting register both
  in i.rm.reg & i.rm.regmem fields
*/
{"imul", 2, 0x6b, _, Modrm|imulKludge, Imm8S, WordReg, 0},
{"imul", 2, 0x69, _, Modrm|imulKludge, Imm16|Imm32, WordReg, 0},
{"div", 1, 0xf6, 6, W|Modrm, Reg|Mem, 0, 0},
{"div", 2, 0xf6, 6, W|Modrm, Reg|Mem, Acc, 0},
{"idiv", 1, 0xf6, 7, W|Modrm, Reg|Mem, 0, 0},
{"idiv", 2, 0xf6, 7, W|Modrm, Reg|Mem, Acc, 0},

{"rol", 2, 0xd0, 0, W|Modrm, Imm1, Reg|Mem, 0},
{"rol", 2, 0xc0, 0, W|Modrm, Imm8, Reg|Mem, 0},
{"rol", 2, 0xd2, 0, W|Modrm, ShiftCount, Reg|Mem, 0},
{"rol", 1, 0xd0, 0, W|Modrm, Reg|Mem, 0, 0},

{"ror", 2, 0xd0, 1, W|Modrm, Imm1, Reg|Mem, 0},
{"ror", 2, 0xc0, 1, W|Modrm, Imm8, Reg|Mem, 0},
{"ror", 2, 0xd2, 1, W|Modrm, ShiftCount, Reg|Mem, 0},
{"ror", 1, 0xd0, 1, W|Modrm, Reg|Mem, 0, 0},

{"rcl", 2, 0xd0, 2, W|Modrm, Imm1, Reg|Mem, 0},
{"rcl", 2, 0xc0, 2, W|Modrm, Imm8, Reg|Mem, 0},
{"rcl", 2, 0xd2, 2, W|Modrm, ShiftCount, Reg|Mem, 0},
{"rcl", 1, 0xd0, 2, W|Modrm, Reg|Mem, 0, 0},

{"rcr", 2, 0xd0, 3, W|Modrm, Imm1, Reg|Mem, 0},
{"rcr", 2, 0xc0, 3, W|Modrm, Imm8, Reg|Mem, 0},
{"rcr", 2, 0xd2, 3, W|Modrm, ShiftCount, Reg|Mem, 0},
{"rcr", 1, 0xd0, 3, W|Modrm, Reg|Mem, 0, 0},

{"sal", 2, 0xd0, 4, W|Modrm, Imm1, Reg|Mem, 0},
{"sal", 2, 0xc0, 4, W|Modrm, Imm8, Reg|Mem, 0},
{"sal", 2, 0xd2, 4, W|Modrm, ShiftCount, Reg|Mem, 0},
{"sal", 1, 0xd0, 4, W|Modrm, Reg|Mem, 0, 0},
{"shl", 2, 0xd0, 4, W|Modrm, Imm1, Reg|Mem, 0},
{"shl", 2, 0xc0, 4, W|Modrm, Imm8, Reg|Mem, 0},
{"shl", 2, 0xd2, 4, W|Modrm, ShiftCount, Reg|Mem, 0},
{"shl", 1, 0xd0, 4, W|Modrm, Reg|Mem, 0, 0},

{"shld", 3, 0x0fa4, _, Modrm, Imm8, WordReg, WordReg|Mem},
{"shld", 3, 0x0fa5, _, Modrm, ShiftCount, WordReg, WordReg|Mem},

{"shr", 2, 0xd0, 5, W|Modrm, Imm1, Reg|Mem, 0},
{"shr", 2, 0xc0, 5, W|Modrm, Imm8, Reg|Mem, 0},
{"shr", 2, 0xd2, 5, W|Modrm, ShiftCount, Reg|Mem, 0},
{"shr", 1, 0xd0, 5, W|Modrm, Reg|Mem, 0, 0},

{"shrd", 3, 0x0fac, _, Modrm, Imm8, WordReg, WordReg|Mem},
{"shrd", 3, 0x0fad, _, Modrm, ShiftCount, WordReg, WordReg|Mem},

{"sar", 2, 0xd0, 7, W|Modrm, Imm1, Reg|Mem, 0},
{"sar", 2, 0xc0, 7, W|Modrm, Imm8, Reg|Mem, 0},
{"sar", 2, 0xd2, 7, W|Modrm, ShiftCount, Reg|Mem, 0},
{"sar", 1, 0xd0, 7, W|Modrm, Reg|Mem, 0, 0},

/* control transfer instructions */
#define CALL_PC_RELATIVE 0xe8
{"call", 1, 0xe8, _, JumpDword, Disp32, 0, 0},
{"call", 1, 0xff, 2, Modrm, Reg|Mem|JumpAbsolute, 0, 0},
#define CALL_FAR_IMMEDIATE 0x9a
{"lcall", 2, 0x9a, _, JumpInterSegment, Imm16, Abs32, 0},
{"lcall", 1, 0xff, 3, Modrm, Mem, 0, 0},

#define JUMP_PC_RELATIVE 0xeb
{"jmp", 1, 0xeb, _, Jump, Disp, 0, 0},
{"jmp", 1, 0xff, 4, Modrm, Reg32|Mem|JumpAbsolute, 0, 0},
#define JUMP_FAR_IMMEDIATE 0xea
{"ljmp", 2, 0xea, _, JumpInterSegment, Imm16, Imm32, 0},
{"ljmp", 1, 0xff, 5, Modrm, Mem, 0, 0},

{"ret", 0, 0xc3, _, NoModrm, 0, 0, 0},
{"ret", 1, 0xc2, _, NoModrm, Imm16, 0, 0},
{"lret", 0, 0xcb, _, NoModrm, 0, 0, 0},
{"lret", 1, 0xca, _, NoModrm, Imm16, 0, 0},
{"enter", 2, 0xc8, _, NoModrm, Imm16, Imm8, 0},
{"leave", 0, 0xc9, _, NoModrm, 0, 0, 0},

/* conditional jumps */
{"jo", 1, 0x70, _, Jump, Disp, 0, 0},

{"jno", 1, 0x71, _, Jump, Disp, 0, 0},

{"jb", 1, 0x72, _, Jump, Disp, 0, 0},
{"jc", 1, 0x72, _, Jump, Disp, 0, 0},
{"jnae", 1, 0x72, _, Jump, Disp, 0, 0},

{"jnb", 1, 0x73, _, Jump, Disp, 0, 0},
{"jnc", 1, 0x73, _, Jump, Disp, 0, 0},
{"jae", 1, 0x73, _, Jump, Disp, 0, 0},

{"je", 1, 0x74, _, Jump, Disp, 0, 0},
{"jz", 1, 0x74, _, Jump, Disp, 0, 0},

{"jne", 1, 0x75, _, Jump, Disp, 0, 0},
{"jnz", 1, 0x75, _, Jump, Disp, 0, 0},

{"jbe", 1, 0x76, _, Jump, Disp, 0, 0},
{"jna", 1, 0x76, _, Jump, Disp, 0, 0},

{"jnbe", 1, 0x77, _, Jump, Disp, 0, 0},
{"ja", 1, 0x77, _, Jump, Disp, 0, 0},

{"js", 1, 0x78, _, Jump, Disp, 0, 0},

{"jns", 1, 0x79, _, Jump, Disp, 0, 0},

{"jp", 1, 0x7a, _, Jump, Disp, 0, 0},
{"jpe", 1, 0x7a, _, Jump, Disp, 0, 0},

{"jnp", 1, 0x7b, _, Jump, Disp, 0, 0},
{"jpo", 1, 0x7b, _, Jump, Disp, 0, 0},

{"jl", 1, 0x7c, _, Jump, Disp, 0, 0},
{"jnge", 1, 0x7c, _, Jump, Disp, 0, 0},

{"jnl", 1, 0x7d, _, Jump, Disp, 0, 0},
{"jge", 1, 0x7d, _, Jump, Disp, 0, 0},

{"jle", 1, 0x7e, _, Jump, Disp, 0, 0},
{"jng", 1, 0x7e, _, Jump, Disp, 0, 0},

{"jnle", 1, 0x7f, _, Jump, Disp, 0, 0},
{"jg", 1, 0x7f, _, Jump, Disp, 0, 0},

/* these turn into pseudo operations when disp is larger than 8 bits */
#define IS_JUMP_ON_CX_ZERO(o) \
  (o == 0x67e3)
#define IS_JUMP_ON_ECX_ZERO(o) \
  (o == 0xe3)

{"jcxz", 1, 0x67e3, _, JumpByte, Disp, 0, 0},
{"jecxz", 1, 0xe3, _, JumpByte, Disp, 0, 0},

#define IS_LOOP_ECX_TIMES(o) \
  (o == 0xe2 || o == 0xe1 || o == 0xe0)

{"loop", 1, 0xe2, _, JumpByte, Disp, 0, 0},

{"loopz", 1, 0xe1, _, JumpByte, Disp, 0, 0},
{"loope", 1, 0xe1, _, JumpByte, Disp, 0, 0},

{"loopnz", 1, 0xe0, _, JumpByte, Disp, 0, 0},
{"loopne", 1, 0xe0, _, JumpByte, Disp, 0, 0},

/* set byte on flag instructions */
{"seto", 1, 0x0f90, 0, Modrm, Reg8|Mem, 0, 0},

{"setno", 1, 0x0f91, 0, Modrm, Reg8|Mem, 0, 0},

{"setc", 1, 0x0f92, 0, Modrm, Reg8|Mem, 0, 0},
{"setb", 1, 0x0f92, 0, Modrm, Reg8|Mem, 0, 0},
{"setnae", 1, 0x0f92, 0, Modrm, Reg8|Mem, 0, 0},

{"setnc", 1, 0x0f93, 0, Modrm, Reg8|Mem, 0, 0},
{"setnb", 1, 0x0f93, 0, Modrm, Reg8|Mem, 0, 0},
{"setae", 1, 0x0f93, 0, Modrm, Reg8|Mem, 0, 0},

{"sete", 1, 0x0f94, 0, Modrm, Reg8|Mem, 0, 0},
{"setz", 1, 0x0f94, 0, Modrm, Reg8|Mem, 0, 0},

{"setne", 1, 0x0f95, 0, Modrm, Reg8|Mem, 0, 0},
{"setnz", 1, 0x0f95, 0, Modrm, Reg8|Mem, 0, 0},

{"setbe", 1, 0x0f96, 0, Modrm, Reg8|Mem, 0, 0},
{"setna", 1, 0x0f96, 0, Modrm, Reg8|Mem, 0, 0},

{"setnbe", 1, 0x0f97, 0, Modrm, Reg8|Mem, 0, 0},
{"seta", 1, 0x0f97, 0, Modrm, Reg8|Mem, 0, 0},

{"sets", 1, 0x0f98, 0, Modrm, Reg8|Mem, 0, 0},

{"setns", 1, 0x0f99, 0, Modrm, Reg8|Mem, 0, 0},

{"setp", 1, 0x0f9a, 0, Modrm, Reg8|Mem, 0, 0},
{"setpe", 1, 0x0f9a, 0, Modrm, Reg8|Mem, 0, 0},

{"setnp", 1, 0x0f9b, 0, Modrm, Reg8|Mem, 0, 0},
{"setpo", 1, 0x0f9b, 0, Modrm, Reg8|Mem, 0, 0},

{"setl", 1, 0x0f9c, 0, Modrm, Reg8|Mem, 0, 0},
{"setnge", 1, 0x0f9c, 0, Modrm, Reg8|Mem, 0, 0},

{"setnl", 1, 0x0f9d, 0, Modrm, Reg8|Mem, 0, 0},
{"setge", 1, 0x0f9d, 0, Modrm, Reg8|Mem, 0, 0},

{"setle", 1, 0x0f9e, 0, Modrm, Reg8|Mem, 0, 0},
{"setng", 1, 0x0f9e, 0, Modrm, Reg8|Mem, 0, 0},

{"setnle", 1, 0x0f9f, 0, Modrm, Reg8|Mem, 0, 0},
{"setg", 1, 0x0f9f, 0, Modrm, Reg8|Mem, 0, 0},

#define IS_STRING_INSTRUCTION(o) \
  ((o) == 0xa6 || (o) == 0x6c || (o) == 0x6e || (o) == 0x6e || \
   (o) == 0xac || (o) == 0xa4 || (o) == 0xae || (o) == 0xaa || \
   (o) == 0xd7)

/* string manipulation */
{"cmps", 0, 0xa6, _, W|NoModrm, 0, 0, 0},
{"scmp", 0, 0xa6, _, W|NoModrm, 0, 0, 0},
{"ins", 0, 0x6c, _, W|NoModrm, 0, 0, 0},
{"outs", 0, 0x6e, _, W|NoModrm, 0, 0, 0},
{"lods", 0, 0xac, _, W|NoModrm, 0, 0, 0},
{"slod", 0, 0xac, _, W|NoModrm, 0, 0, 0},
{"movs", 0, 0xa4, _, W|NoModrm, 0, 0, 0},
{"smov", 0, 0xa4, _, W|NoModrm, 0, 0, 0},
{"scas", 0, 0xae, _, W|NoModrm, 0, 0, 0},
{"ssca", 0, 0xae, _, W|NoModrm, 0, 0, 0},
{"stos", 0, 0xaa, _, W|NoModrm, 0, 0, 0},
{"ssto", 0, 0xaa, _, W|NoModrm, 0, 0, 0},
{"xlat", 0, 0xd7, _, NoModrm, 0, 0, 0},

/* bit manipulation */
{"bsf", 2, 0x0fbc, _, Modrm|ReverseRegRegmem, Reg|Mem, Reg, 0},
{"bsr", 2, 0x0fbd, _, Modrm|ReverseRegRegmem, Reg|Mem, Reg, 0},
{"bt", 2, 0x0fa3, _, Modrm, Reg, Reg|Mem, 0},
{"bt", 2, 0x0fba, 4, Modrm, Imm8, Reg|Mem, 0},
{"btc", 2, 0x0fbb, _, Modrm, Reg, Reg|Mem, 0},
{"btc", 2, 0x0fba, 7, Modrm, Imm8, Reg|Mem, 0},
{"btr", 2, 0x0fb3, _, Modrm, Reg, Reg|Mem, 0},
{"btr", 2, 0x0fba, 6, Modrm, Imm8, Reg|Mem, 0},
{"bts", 2, 0x0fab, _, Modrm, Reg, Reg|Mem, 0},
{"bts", 2, 0x0fba, 5, Modrm, Imm8, Reg|Mem, 0},

/* interrupts & op. sys insns */
/* See i386.c for conversion of 'int $3' into the special int 3 insn. */
#define INT_OPCODE 0xcd
#define INT3_OPCODE 0xcc
{"int", 1, 0xcd, _, NoModrm, Imm8, 0, 0},
{"int3", 0, 0xcc, _, NoModrm, 0, 0, 0},
{"into", 0, 0xce, _, NoModrm, 0, 0, 0},
{"iret", 0, 0xcf, _, NoModrm, 0, 0, 0},

{"boundl", 2, 0x62, _, Modrm, Reg32, Mem, 0},
{"boundw", 2, 0x6662, _, Modrm, Reg16, Mem, 0},

{"hlt", 0, 0xf4, _, NoModrm, 0, 0, 0},
{"wait", 0, 0x9b, _, NoModrm, 0, 0, 0},
/* nop is actually 'xchgl %eax, %eax' */
{"nop", 0, 0x90, _, NoModrm, 0, 0, 0},

/* protection control */
{"arpl", 2, 0x63, _, Modrm, Reg16, Reg16|Mem, 0},
{"lar", 2, 0x0f02, _, Modrm|ReverseRegRegmem, WordReg|Mem, WordReg, 0},
{"lgdt", 1, 0x0f01, 2, Modrm, Mem, 0, 0},
{"lidt", 1, 0x0f01, 3, Modrm, Mem, 0, 0},
{"lldt", 1, 0x0f00, 2, Modrm, WordReg|Mem, 0, 0},
{"lmsw", 1, 0x0f01, 6, Modrm, WordReg|Mem, 0, 0},
{"lsl", 2, 0x0f03, _, Modrm|ReverseRegRegmem, WordReg|Mem, WordReg, 0},
{"ltr", 1, 0x0f00, 3, Modrm, WordReg|Mem, 0, 0},

{"sgdt", 1, 0x0f01, 0, Modrm, Mem, 0, 0},
{"sidt", 1, 0x0f01, 1, Modrm, Mem, 0, 0},
{"sldt", 1, 0x0f00, 0, Modrm, WordReg|Mem, 0, 0},
{"smsw", 1, 0x0f01, 4, Modrm, WordReg|Mem, 0, 0},
{"str", 1, 0x0f00, 1, Modrm, Reg16|Mem, 0, 0},

{"verr", 1, 0x0f00, 4, Modrm, WordReg|Mem, 0, 0},
{"verw", 1, 0x0f00, 5, Modrm, WordReg|Mem, 0, 0},

/* floating point instructions */

/* load */
{"fld", 1, 0xd9c0, _, ShortForm, FloatReg, 0, 0}, /* register */
{"flds", 1, 0xd9, 0, Modrm, Mem, 0, 0},           /* %st0 <-- mem float */
{"fildl", 1, 0xdb, 0, Modrm, Mem, 0, 0},           /* %st0 <-- mem word */
{"fldl", 1, 0xdd, 0, Modrm, Mem, 0, 0},           /* %st0 <-- mem double */
{"fldl", 1, 0xd9c0, _, ShortForm, FloatReg, 0, 0}, /* register */
{"filds", 1, 0xdf, 0, Modrm, Mem, 0, 0},           /* %st0 <-- mem dword */
{"fildll", 1, 0xdf, 5, Modrm, Mem, 0, 0},           /* %st0 <-- mem qword */
{"fildq", 1, 0xdf, 5, Modrm, Mem, 0, 0},           /* %st0 <-- mem qword */
{"fldt", 1, 0xdb, 5, Modrm, Mem, 0, 0},           /* %st0 <-- mem efloat */
{"fbld", 1, 0xdf, 4, Modrm, Mem, 0, 0},           /* %st0 <-- mem bcd */

/* store (no pop) */
{"fst", 1, 0xddd0, _, ShortForm, FloatReg, 0, 0}, /* register */
{"fsts", 1, 0xd9, 2, Modrm, Mem, 0, 0},           /* %st0 --> mem float */
{"fistl", 1, 0xdb, 2, Modrm, Mem, 0, 0},           /* %st0 --> mem dword */
{"fstl", 1, 0xdd, 2, Modrm, Mem, 0, 0},           /* %st0 --> mem double */
{"fstl", 1, 0xddd0, _, ShortForm, FloatReg, 0, 0}, /* register */
{"fists", 1, 0xdf, 2, Modrm, Mem, 0, 0},           /* %st0 --> mem word */

/* store (with pop) */
{"fstp", 1, 0xddd8, _, ShortForm, FloatReg, 0, 0}, /* register */
{"fstps", 1, 0xd9, 3, Modrm, Mem, 0, 0},           /* %st0 --> mem float */
{"fistpl", 1, 0xdb, 3, Modrm, Mem, 0, 0},           /* %st0 --> mem word */
{"fstpl", 1, 0xdd, 3, Modrm, Mem, 0, 0},           /* %st0 --> mem double */
{"fstpl", 1, 0xddd8, _, ShortForm, FloatReg, 0, 0}, /* register */
{"fistps", 1, 0xdf, 3, Modrm, Mem, 0, 0},           /* %st0 --> mem dword */
{"fistpll", 1, 0xdf, 7, Modrm, Mem, 0, 0},           /* %st0 --> mem qword */
{"fistpq", 1, 0xdf, 7, Modrm, Mem, 0, 0},           /* %st0 --> mem qword */
{"fstpt", 1, 0xdb, 7, Modrm, Mem, 0, 0},           /* %st0 --> mem efloat */
{"fbstp", 1, 0xdf, 6, Modrm, Mem, 0, 0},           /* %st0 --> mem bcd */

/* exchange %st<n> with %st0 */
{"fxch", 1, 0xd9c8, _, ShortForm, FloatReg, 0, 0},

/* comparison (without pop) */
{"fcom", 1, 0xd8d0, _, ShortForm, FloatReg, 0, 0},
{"fcoms", 1, 0xd8, 2, Modrm, Mem, 0, 0}, /* compare %st0, mem float  */
{"ficoml", 1, 0xda, 2, Modrm, Mem, 0, 0}, /* compare %st0, mem word  */
{"fcoml", 1, 0xdc, 2, Modrm, Mem, 0, 0}, /* compare %st0, mem double  */
{"fcoml", 1, 0xd8d0, _, ShortForm, FloatReg, 0, 0},
{"ficoms", 1, 0xde, 2, Modrm, Mem, 0, 0}, /* compare %st0, mem dword */

/* comparison (with pop) */
{"fcomp", 1, 0xd8d8, _, ShortForm, FloatReg, 0, 0},
{"fcomps", 1, 0xd8, 3, Modrm, Mem, 0, 0}, /* compare %st0, mem float  */
{"ficompl", 1, 0xda, 3, Modrm, Mem, 0, 0}, /* compare %st0, mem word  */
{"fcompl", 1, 0xdc, 3, Modrm, Mem, 0, 0}, /* compare %st0, mem double  */
{"fcompl", 1, 0xd8d8, _, ShortForm, FloatReg, 0, 0},
{"ficomps", 1, 0xde, 3, Modrm, Mem, 0, 0}, /* compare %st0, mem dword */
{"fcompp", 0, 0xded9, _, NoModrm, 0, 0, 0}, /* compare %st0, %st1 & pop twice */

/* unordered comparison (with pop) */
{"fucom", 1, 0xdde0, _, ShortForm, FloatReg, 0, 0},
{"fucomp", 1, 0xdde8, _, ShortForm, FloatReg, 0, 0},
{"fucompp", 0, 0xdae9, _, NoModrm, 0, 0, 0}, /* ucompare %st0, %st1 & pop twice */

{"ftst", 0, 0xd9e4, _, NoModrm, 0, 0, 0},   /* test %st0 */
{"fxam", 0, 0xd9e5, _, NoModrm, 0, 0, 0},   /* examine %st0 */

/* load constants into %st0 */
{"fld1", 0, 0xd9e8, _, NoModrm, 0, 0, 0},   /* %st0 <-- 1.0 */
{"fldl2t", 0, 0xd9e9, _, NoModrm, 0, 0, 0},   /* %st0 <-- log2(10) */
{"fldl2e", 0, 0xd9ea, _, NoModrm, 0, 0, 0},   /* %st0 <-- log2(e) */
{"fldpi", 0, 0xd9eb, _, NoModrm, 0, 0, 0},   /* %st0 <-- pi */
{"fldlg2", 0, 0xd9ec, _, NoModrm, 0, 0, 0},   /* %st0 <-- log10(2) */
{"fldln2", 0, 0xd9ed, _, NoModrm, 0, 0, 0},   /* %st0 <-- ln(2) */
{"fldz", 0, 0xd9ee, _, NoModrm, 0, 0, 0},   /* %st0 <-- 0.0 */

/* arithmetic */

/* add */
{"fadd", 1, 0xd8c0, _, ShortForm, FloatReg, 0, 0},
{"fadd", 2, 0xd8c0, _, ShortForm|FloatD, FloatReg, FloatAcc, 0},
{"fadd", 0, 0xdcc1, _, NoModrm, 0, 0, 0}, /* alias for fadd %st, %st(1) */
{"faddp", 1, 0xdec0, _, ShortForm, FloatReg, 0, 0},
{"faddp", 2, 0xdac0, _, ShortForm|FloatD, FloatReg, FloatAcc, 0},
{"faddp", 0, 0xdec1, _, NoModrm, 0, 0, 0}, /* alias for faddp %st, %st(1) */
{"fadds", 1, 0xd8, 0, Modrm, Mem, 0, 0},
{"fiaddl", 1, 0xda, 0, Modrm, Mem, 0, 0},
{"faddl", 1, 0xdc, 0, Modrm, Mem, 0, 0},
{"fiadds", 1, 0xde, 0, Modrm, Mem, 0, 0},

/* sub */
/* Note:  intel has decided that certain of these operations are reversed
   in assembler syntax. */
{"fsub", 1, 0xd8e0, _, ShortForm, FloatReg, 0, 0},
{"fsub", 2, 0xd8e0, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fsub", 2, 0xdce8, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fsub", 2, 0xdce0, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fsub", 0, 0xdce1, _, NoModrm, 0, 0, 0},
{"fsubp", 1, 0xdee0, _, ShortForm, FloatReg, 0, 0},
{"fsubp", 2, 0xdee0, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fsubp", 2, 0xdee8, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fsubp", 2, 0xdee0, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fsubp", 0, 0xdee1, _, NoModrm, 0, 0, 0},
{"fsubs", 1, 0xd8, 4, Modrm, Mem, 0, 0},
{"fisubl", 1, 0xda, 4, Modrm, Mem, 0, 0},
{"fsubl", 1, 0xdc, 4, Modrm, Mem, 0, 0},
{"fisubs", 1, 0xde, 4, Modrm, Mem, 0, 0},

/* sub reverse */
{"fsubr", 1, 0xd8e8, _, ShortForm, FloatReg, 0, 0},
{"fsubr", 2, 0xd8e8, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fsubr", 2, 0xdce0, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fsubr", 2, 0xdce8, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fsubr", 0, 0xdce9, _, NoModrm, 0, 0, 0},
{"fsubrp", 1, 0xdee8, _, ShortForm, FloatReg, 0, 0},
{"fsubrp", 2, 0xdee8, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fsubrp", 2, 0xdee0, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fsubrp", 2, 0xdee8, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fsubrp", 0, 0xdee9, _, NoModrm, 0, 0, 0},
{"fsubrs", 1, 0xd8, 5, Modrm, Mem, 0, 0},
{"fisubrl", 1, 0xda, 5, Modrm, Mem, 0, 0},
{"fsubrl", 1, 0xdc, 5, Modrm, Mem, 0, 0},
{"fisubrs", 1, 0xde, 5, Modrm, Mem, 0, 0},

/* mul */
{"fmul", 1, 0xd8c8, _, ShortForm, FloatReg, 0, 0},
{"fmul", 2, 0xd8c8, _, ShortForm|FloatD, FloatReg, FloatAcc, 0},
{"fmul", 0, 0xdcc9, _, NoModrm, 0, 0, 0},
{"fmulp", 1, 0xdec8, _, ShortForm, FloatReg, 0, 0},
{"fmulp", 2, 0xdec8, _, ShortForm|FloatD, FloatReg, FloatAcc, 0},
{"fmulp", 0, 0xdec9, _, NoModrm, 0, 0, 0},
{"fmuls", 1, 0xd8, 1, Modrm, Mem, 0, 0},
{"fimull", 1, 0xda, 1, Modrm, Mem, 0, 0},
{"fmull", 1, 0xdc, 1, Modrm, Mem, 0, 0},
{"fimuls", 1, 0xde, 1, Modrm, Mem, 0, 0},

/* div */
/* Note:  intel has decided that certain of these operations are reversed
   in assembler syntax. */
{"fdiv", 1, 0xd8f0, _, ShortForm, FloatReg, 0, 0},
{"fdiv", 2, 0xd8f0, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fdiv", 2, 0xdcf8, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fdiv", 2, 0xdcf0, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fdiv", 0, 0xdcf1, _, NoModrm, 0, 0, 0},
{"fdivp", 1, 0xdef0, _, ShortForm, FloatReg, 0, 0},
{"fdivp", 2, 0xdef0, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fdivp", 2, 0xdef8, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fdivp", 2, 0xdef0, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fdivp", 0, 0xdef1, _, NoModrm, 0, 0, 0},
{"fdivs", 1, 0xd8, 6, Modrm, Mem, 0, 0},
{"fidivl", 1, 0xda, 6, Modrm, Mem, 0, 0},
{"fdivl", 1, 0xdc, 6, Modrm, Mem, 0, 0},
{"fidivs", 1, 0xde, 6, Modrm, Mem, 0, 0},

/* div reverse */
{"fdivr", 1, 0xd8f8, _, ShortForm, FloatReg, 0, 0},
{"fdivr", 2, 0xd8f8, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fdivr", 2, 0xdcf0, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fdivr", 2, 0xdcf8, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fdivr", 0, 0xdcf9, _, NoModrm, 0, 0, 0},
{"fdivrp", 1, 0xdef8, _, ShortForm, FloatReg, 0, 0},
{"fdivrp", 2, 0xdef8, _, ShortForm, FloatReg, FloatAcc, 0},
#ifdef NON_BROKEN_OPCODES
{"fdivrp", 2, 0xdef0, _, ShortForm, FloatAcc, FloatReg, 0},
#else
{"fdivrp", 2, 0xdef8, _, ShortForm, FloatAcc, FloatReg, 0},
#endif
{"fdivrp", 0, 0xdef9, _, NoModrm, 0, 0, 0},
{"fdivrs", 1, 0xd8, 7, Modrm, Mem, 0, 0},
{"fidivrl", 1, 0xda, 7, Modrm, Mem, 0, 0},
{"fdivrl", 1, 0xdc, 7, Modrm, Mem, 0, 0},
{"fidivrs", 1, 0xde, 7, Modrm, Mem, 0, 0},

{"f2xm1", 0,   0xd9f0, _, NoModrm, 0, 0, 0},
{"fyl2x", 0,   0xd9f1, _, NoModrm, 0, 0, 0},
{"fptan", 0,   0xd9f2, _, NoModrm, 0, 0, 0},
{"fpatan", 0,  0xd9f3, _, NoModrm, 0, 0, 0},
{"fxtract", 0, 0xd9f4, _, NoModrm, 0, 0, 0},
{"fprem1", 0,  0xd9f5, _, NoModrm, 0, 0, 0},
{"fdecstp", 0,  0xd9f6, _, NoModrm, 0, 0, 0},
{"fincstp", 0,  0xd9f7, _, NoModrm, 0, 0, 0},
{"fprem", 0,   0xd9f8, _, NoModrm, 0, 0, 0},
{"fyl2xp1", 0, 0xd9f9, _, NoModrm, 0, 0, 0},
{"fsqrt", 0,   0xd9fa, _, NoModrm, 0, 0, 0},
{"fsincos", 0, 0xd9fb, _, NoModrm, 0, 0, 0},
{"frndint", 0, 0xd9fc, _, NoModrm, 0, 0, 0},
{"fscale", 0,  0xd9fd, _, NoModrm, 0, 0, 0},
{"fsin", 0,    0xd9fe, _, NoModrm, 0, 0, 0},
{"fcos", 0,    0xd9ff, _, NoModrm, 0, 0, 0},

{"fchs", 0, 0xd9e0, _, NoModrm, 0, 0, 0},
{"fabs", 0, 0xd9e1, _, NoModrm, 0, 0, 0},

/* processor control */
{"fninit", 0, 0xdbe3, _, NoModrm, 0, 0, 0},
{"finit", 0, 0xdbe3, _, NoModrm, 0, 0, 0},
{"fldcw", 1, 0xd9, 5, Modrm, Mem, 0, 0},
{"fnstcw", 1, 0xd9, 7, Modrm, Mem, 0, 0},
{"fstcw", 1, 0xd9, 7, Modrm, Mem, 0, 0},
{"fnstsw", 1, 0xdfe0, _, NoModrm, Acc, 0, 0},
{"fnstsw", 1, 0xdd, 7, Modrm, Mem, 0, 0},
{"fnstsw", 0, 0xdfe0, _, NoModrm, 0, 0, 0},
{"fstsw", 1, 0xdfe0, _, NoModrm, Acc, 0, 0},
{"fstsw", 1, 0xdd, 7, Modrm, Mem, 0, 0},
{"fstsw", 0, 0xdfe0, _, NoModrm, 0, 0, 0},
{"fnclex", 0, 0xdbe2, _, NoModrm, 0, 0, 0},
{"fclex", 0, 0xdbe2, _, NoModrm, 0, 0, 0},
/*
 We ignore the short format (287) versions of fstenv/fldenv & fsave/frstor
 instructions;  i'm not sure how to add them or how they are different.
 My 386/387 book offers no details about this.
*/
{"fnstenv", 1, 0xd9, 6, Modrm, Mem, 0, 0},
{"fstenv", 1, 0xd9, 6, Modrm, Mem, 0, 0},
{"fldenv", 1, 0xd9, 4, Modrm, Mem, 0, 0},
{"fnsave", 1, 0xdd, 6, Modrm, Mem, 0, 0},
{"fsave", 1, 0xdd, 6, Modrm, Mem, 0, 0},
{"frstor", 1, 0xdd, 4, Modrm, Mem, 0, 0},

{"ffree", 1, 0xddc0, _, ShortForm, FloatReg, 0, 0},
{"fnop", 0, 0xd9d0, _, NoModrm, 0, 0, 0},
{"fwait", 0, 0x9b, _, NoModrm, 0, 0, 0},

/*
  opcode prefixes; we allow them as seperate insns too
  (see prefix table below)
*/
{"aword", 0, 0x67, _, NoModrm, 0, 0, 0},
{"addr16", 0, 0x67, _, NoModrm, 0, 0, 0},
{"word", 0, 0x66, _, NoModrm, 0, 0, 0},
{"data16", 0, 0x66, _, NoModrm, 0, 0, 0},
{"lock", 0, 0xf0, _, NoModrm, 0, 0, 0},
{"cs", 0, 0x2e, _, NoModrm, 0, 0, 0},
{"ds", 0, 0x3e, _, NoModrm, 0, 0, 0},
{"es", 0, 0x26, _, NoModrm, 0, 0, 0},
{"fs", 0, 0x64, _, NoModrm, 0, 0, 0},
{"gs", 0, 0x65, _, NoModrm, 0, 0, 0},
{"ss", 0, 0x36, _, NoModrm, 0, 0, 0},
{"rep", 0, 0xf3, _, NoModrm, 0, 0, 0},
{"repe", 0, 0xf3, _, NoModrm, 0, 0, 0},
{ "repne", 0, 0xf2, _, NoModrm, 0, 0, 0},
{"repz", 0, 0xf3, _, NoModrm, 0, 0, 0},
{ "repnz", 0, 0xf2, _, NoModrm, 0, 0, 0},

/* 486 extensions */
{"bswap", 1, 0x0fc8, _, ShortForm, Reg32,0,0 },
{"xadd", 2, 0x0fc0, _, DW|Modrm, Reg, Reg|Mem, 0 },
{"cmpxchg", 2, 0x0fb0, _, DW|Modrm, Reg, Reg|Mem, 0 },
{"invd", 0, 0x0f08, _, NoModrm, 0, 0, 0},
{"wbinvd", 0, 0x0f09, _, NoModrm, 0, 0, 0},
{"invlpg", 1, 0x0f01, 7, Modrm, Mem, 0, 0},

/* Pentium and late-model 486 extensions */
{"cpuid", 0, 0x0fa2, _, NoModrm, 0, 0, 0},

/* Pentium extensions */
{"wrmsr", 0, 0x0f30, _, NoModrm, 0, 0, 0},
{"rdtsc", 0, 0x0f31, _, NoModrm, 0, 0, 0},
{"rdmsr", 0, 0x0f32, _, NoModrm, 0, 0, 0},
{"cmpxchg8b", 1, 0x0fc7, 1, Modrm, Mem, 0, 0},

/* Pentium Pro extensions */
{"rdpmc", 0, 0x0f33, _, NoModrm, 0, 0, 0},

{"ud2", 0, 0x0fff, _, NoModrm, 0, 0, 0}, /* official undefined instr. */

{"cmovo",  2, 0x0f40, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovno", 2, 0x0f41, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovb",  2, 0x0f42, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovae", 2, 0x0f43, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmove",  2, 0x0f44, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovne", 2, 0x0f45, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovbe", 2, 0x0f46, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmova",  2, 0x0f47, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovs",  2, 0x0f48, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovns", 2, 0x0f49, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovp",  2, 0x0f4a, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovnp", 2, 0x0f4b, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovl",  2, 0x0f4c, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovge", 2, 0x0f4d, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovle", 2, 0x0f4e, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},
{"cmovg",  2, 0x0f4f, _, Modrm|ReverseRegRegmem, WordReg|WordMem, WordReg, 0},

{"fcmovb", 2, 0xdac0, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmove", 2, 0xdac8, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmovbe",2, 0xdad0, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmovu", 2, 0xdad8, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmovnb", 2, 0xdbc0, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmovne", 2, 0xdbc8, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmovnbe",2, 0xdbd0, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcmovnu", 2, 0xdbd8, _, ShortForm, FloatReg, FloatAcc, 0},

{"fcomi",  2, 0xdbf0, _, ShortForm, FloatReg, FloatAcc, 0},
{"fucomi", 2, 0xdbe8, _, ShortForm, FloatReg, FloatAcc, 0},
{"fcomip", 2, 0xdff0, _, ShortForm, FloatReg, FloatAcc, 0},
{"fucomip",2, 0xdfe8, _, ShortForm, FloatReg, FloatAcc, 0},


{"", 0, 0, 0, 0, 0, 0, 0}	/* sentinal */
};
#undef _

static const template *i386_optab_end
  = i386_optab + sizeof (i386_optab)/sizeof(i386_optab[0]);

/* 386 register table */

static const reg_entry i386_regtab[] = {
  /* 8 bit regs */
  {"al", Reg8|Acc, 0}, {"cl", Reg8|ShiftCount, 1}, {"dl", Reg8, 2},
  {"bl", Reg8, 3},
  {"ah", Reg8, 4}, {"ch", Reg8, 5}, {"dh", Reg8, 6}, {"bh", Reg8, 7},
  /* 16 bit regs */
  {"ax", Reg16|Acc, 0}, {"cx", Reg16, 1}, {"dx", Reg16|InOutPortReg, 2}, {"bx", Reg16, 3},
  {"sp", Reg16, 4}, {"bp", Reg16, 5}, {"si", Reg16, 6}, {"di", Reg16, 7},
  /* 32 bit regs */
  {"eax", Reg32|Acc, 0}, {"ecx", Reg32, 1}, {"edx", Reg32, 2}, {"ebx", Reg32, 3},
  {"esp", Reg32, 4}, {"ebp", Reg32, 5}, {"esi", Reg32, 6}, {"edi", Reg32, 7},
  /* segment registers */
  {"es", SReg2, 0}, {"cs", SReg2, 1}, {"ss", SReg2, 2},
  {"ds", SReg2, 3}, {"fs", SReg3, 4}, {"gs", SReg3, 5},
  /* control registers */
  {"cr0", Control, 0},   {"cr2", Control, 2},   {"cr3", Control, 3},
  {"cr4", Control, 4},
  /* debug registers */
  {"db0", Debug, 0},   {"db1", Debug, 1},   {"db2", Debug, 2},
  {"db3", Debug, 3},   {"db6", Debug, 6},   {"db7", Debug, 7},
  {"dr0", Debug, 0},   {"dr1", Debug, 1},   {"dr2", Debug, 2},
  {"dr3", Debug, 3},   {"dr6", Debug, 6},   {"dr7", Debug, 7},
  /* test registers */
  {"tr3", Test, 3}, {"tr4", Test, 4}, {"tr5", Test, 5},
  {"tr6", Test, 6}, {"tr7", Test, 7},
  /* float registers */
  {"st(0)", FloatReg|FloatAcc, 0},
  {"st", FloatReg|FloatAcc, 0},
  {"st(1)", FloatReg, 1}, {"st(2)", FloatReg, 2},
  {"st(3)", FloatReg, 3}, {"st(4)", FloatReg, 4}, {"st(5)", FloatReg, 5},
  {"st(6)", FloatReg, 6}, {"st(7)", FloatReg, 7}
};

#define MAX_REG_NAME_SIZE 8	/* for parsing register names from input */

static const reg_entry *i386_regtab_end
  = i386_regtab + sizeof(i386_regtab)/sizeof(i386_regtab[0]);

/* segment stuff */
static const seg_entry cs = { "cs", 0x2e };
static const seg_entry ds = { "ds", 0x3e };
static const seg_entry ss = { "ss", 0x36 };
static const seg_entry es = { "es", 0x26 };
static const seg_entry fs = { "fs", 0x64 };
static const seg_entry gs = { "gs", 0x65 };
static const seg_entry null = { "", 0x0 };

/*
  This table is used to store the default segment register implied by all
  possible memory addressing modes.
  It is indexed by the mode & modrm entries of the modrm byte as follows:
      index = (mode<<3) | modrm;
*/
static const seg_entry *one_byte_segment_defaults[] = {
  /* mode 0 */
  &ds, &ds, &ds, &ds, &null, &ds, &ds, &ds,
  /* mode 1 */
  &ds, &ds, &ds, &ds, &null, &ss, &ds, &ds,
  /* mode 2 */
  &ds, &ds, &ds, &ds, &null, &ss, &ds, &ds,
  /* mode 3 --- not a memory reference; never referenced */
};

static const seg_entry *two_byte_segment_defaults[] = {
  /* mode 0 */
  &ds, &ds, &ds, &ds, &ss, &ds, &ds, &ds,
  /* mode 1 */
  &ds, &ds, &ds, &ds, &ss, &ds, &ds, &ds,
  /* mode 2 */
  &ds, &ds, &ds, &ds, &ss, &ds, &ds, &ds,
  /* mode 3 --- not a memory reference; never referenced */
};

static const prefix_entry i386_prefixtab[] = {
  { "addr16", 0x67 },		/* address size prefix ==> 16bit addressing
				 * (How is this useful?) */
#define WORD_PREFIX_OPCODE 0x66
  { "data16", 0x66 },		/* operand size prefix */
  { "lock", 0xf0 },		/* bus lock prefix */
  { "wait", 0x9b },		/* wait for coprocessor */
  { "cs", 0x2e }, { "ds", 0x3e }, /* segment overrides ... */
  { "es", 0x26 }, { "fs", 0x64 },
  { "gs", 0x65 }, { "ss", 0x36 },
/* REPE & REPNE used to detect rep/repne with a non-string instruction */
#define REPNE 0xf2
#define REPE  0xf3
  { "rep", 0xf3 }, { "repe", 0xf3 }, { "repz", 0xf3 }, /* repeat string instructions */
  { "repne", 0xf2 }, { "repnz", 0xf2 }
};

static const prefix_entry *i386_prefixtab_end
  = i386_prefixtab + sizeof(i386_prefixtab)/sizeof(i386_prefixtab[0]);

/* end of i386-opcode.h */
