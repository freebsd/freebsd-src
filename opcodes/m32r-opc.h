/* Instruction opcode header for m32r.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef M32R_OPC_H
#define M32R_OPC_H

/* -- opc.h */

#undef  CGEN_DIS_HASH_SIZE
#define CGEN_DIS_HASH_SIZE 256
#undef  CGEN_DIS_HASH
#if 0
#define X(b) (((unsigned char *) (b))[0] & 0xf0)
#define CGEN_DIS_HASH(buffer, value) \
(X (buffer) | \
 (X (buffer) == 0x40 || X (buffer) == 0xe0 || X (buffer) == 0x60 || X (buffer) == 0x50 ? 0 \
  : X (buffer) == 0x70 || X (buffer) == 0xf0 ? (((unsigned char *) (buffer))[0] & 0xf) \
  : X (buffer) == 0x30 ? ((((unsigned char *) (buffer))[1] & 0x70) >> 4) \
  : ((((unsigned char *) (buffer))[1] & 0xf0) >> 4)))
#else
#define CGEN_DIS_HASH(buffer, value) m32r_cgen_dis_hash(buffer, value)
extern unsigned int m32r_cgen_dis_hash(const char *, CGEN_INSN_INT);
#endif

/* -- */
/* Enum declaration for m32r instruction types.  */
typedef enum cgen_insn_type {
  M32R_INSN_INVALID, M32R_INSN_ADD, M32R_INSN_ADD3, M32R_INSN_AND
 , M32R_INSN_AND3, M32R_INSN_OR, M32R_INSN_OR3, M32R_INSN_XOR
 , M32R_INSN_XOR3, M32R_INSN_ADDI, M32R_INSN_ADDV, M32R_INSN_ADDV3
 , M32R_INSN_ADDX, M32R_INSN_BC8, M32R_INSN_BC24, M32R_INSN_BEQ
 , M32R_INSN_BEQZ, M32R_INSN_BGEZ, M32R_INSN_BGTZ, M32R_INSN_BLEZ
 , M32R_INSN_BLTZ, M32R_INSN_BNEZ, M32R_INSN_BL8, M32R_INSN_BL24
 , M32R_INSN_BCL8, M32R_INSN_BCL24, M32R_INSN_BNC8, M32R_INSN_BNC24
 , M32R_INSN_BNE, M32R_INSN_BRA8, M32R_INSN_BRA24, M32R_INSN_BNCL8
 , M32R_INSN_BNCL24, M32R_INSN_CMP, M32R_INSN_CMPI, M32R_INSN_CMPU
 , M32R_INSN_CMPUI, M32R_INSN_CMPEQ, M32R_INSN_CMPZ, M32R_INSN_DIV
 , M32R_INSN_DIVU, M32R_INSN_REM, M32R_INSN_REMU, M32R_INSN_REMH
 , M32R_INSN_REMUH, M32R_INSN_REMB, M32R_INSN_REMUB, M32R_INSN_DIVUH
 , M32R_INSN_DIVB, M32R_INSN_DIVUB, M32R_INSN_DIVH, M32R_INSN_JC
 , M32R_INSN_JNC, M32R_INSN_JL, M32R_INSN_JMP, M32R_INSN_LD
 , M32R_INSN_LD_D, M32R_INSN_LDB, M32R_INSN_LDB_D, M32R_INSN_LDH
 , M32R_INSN_LDH_D, M32R_INSN_LDUB, M32R_INSN_LDUB_D, M32R_INSN_LDUH
 , M32R_INSN_LDUH_D, M32R_INSN_LD_PLUS, M32R_INSN_LD24, M32R_INSN_LDI8
 , M32R_INSN_LDI16, M32R_INSN_LOCK, M32R_INSN_MACHI, M32R_INSN_MACHI_A
 , M32R_INSN_MACLO, M32R_INSN_MACLO_A, M32R_INSN_MACWHI, M32R_INSN_MACWHI_A
 , M32R_INSN_MACWLO, M32R_INSN_MACWLO_A, M32R_INSN_MUL, M32R_INSN_MULHI
 , M32R_INSN_MULHI_A, M32R_INSN_MULLO, M32R_INSN_MULLO_A, M32R_INSN_MULWHI
 , M32R_INSN_MULWHI_A, M32R_INSN_MULWLO, M32R_INSN_MULWLO_A, M32R_INSN_MV
 , M32R_INSN_MVFACHI, M32R_INSN_MVFACHI_A, M32R_INSN_MVFACLO, M32R_INSN_MVFACLO_A
 , M32R_INSN_MVFACMI, M32R_INSN_MVFACMI_A, M32R_INSN_MVFC, M32R_INSN_MVTACHI
 , M32R_INSN_MVTACHI_A, M32R_INSN_MVTACLO, M32R_INSN_MVTACLO_A, M32R_INSN_MVTC
 , M32R_INSN_NEG, M32R_INSN_NOP, M32R_INSN_NOT, M32R_INSN_RAC
 , M32R_INSN_RAC_DSI, M32R_INSN_RACH, M32R_INSN_RACH_DSI, M32R_INSN_RTE
 , M32R_INSN_SETH, M32R_INSN_SLL, M32R_INSN_SLL3, M32R_INSN_SLLI
 , M32R_INSN_SRA, M32R_INSN_SRA3, M32R_INSN_SRAI, M32R_INSN_SRL
 , M32R_INSN_SRL3, M32R_INSN_SRLI, M32R_INSN_ST, M32R_INSN_ST_D
 , M32R_INSN_STB, M32R_INSN_STB_D, M32R_INSN_STH, M32R_INSN_STH_D
 , M32R_INSN_ST_PLUS, M32R_INSN_STH_PLUS, M32R_INSN_STB_PLUS, M32R_INSN_ST_MINUS
 , M32R_INSN_SUB, M32R_INSN_SUBV, M32R_INSN_SUBX, M32R_INSN_TRAP
 , M32R_INSN_UNLOCK, M32R_INSN_SATB, M32R_INSN_SATH, M32R_INSN_SAT
 , M32R_INSN_PCMPBZ, M32R_INSN_SADD, M32R_INSN_MACWU1, M32R_INSN_MSBLO
 , M32R_INSN_MULWU1, M32R_INSN_MACLH1, M32R_INSN_SC, M32R_INSN_SNC
 , M32R_INSN_CLRPSW, M32R_INSN_SETPSW, M32R_INSN_BSET, M32R_INSN_BCLR
 , M32R_INSN_BTST
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID M32R_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) M32R_INSN_BTST + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_op1;
  long f_op2;
  long f_cond;
  long f_r1;
  long f_r2;
  long f_simm8;
  long f_simm16;
  long f_shift_op2;
  long f_uimm3;
  long f_uimm4;
  long f_uimm5;
  long f_uimm8;
  long f_uimm16;
  long f_uimm24;
  long f_hi16;
  long f_disp8;
  long f_disp16;
  long f_disp24;
  long f_op23;
  long f_op3;
  long f_acc;
  long f_accs;
  long f_accd;
  long f_bits67;
  long f_bit4;
  long f_bit14;
  long f_imm1;
};

#define CGEN_INIT_PARSE(od) \
{\
}
#define CGEN_INIT_INSERT(od) \
{\
}
#define CGEN_INIT_EXTRACT(od) \
{\
}
#define CGEN_INIT_PRINT(od) \
{\
}


#endif /* M32R_OPC_H */
