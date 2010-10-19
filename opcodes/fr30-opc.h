/* Instruction opcode header for fr30.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

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
51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef FR30_OPC_H
#define FR30_OPC_H

/* -- opc.h */

/* ??? This can be improved upon.  */
#undef  CGEN_DIS_HASH_SIZE
#define CGEN_DIS_HASH_SIZE 16
#undef  CGEN_DIS_HASH
#define CGEN_DIS_HASH(buffer, value) (((unsigned char *) (buffer))[0] >> 4)

/* -- */
/* Enum declaration for fr30 instruction types.  */
typedef enum cgen_insn_type {
  FR30_INSN_INVALID, FR30_INSN_ADD, FR30_INSN_ADDI, FR30_INSN_ADD2
 , FR30_INSN_ADDC, FR30_INSN_ADDN, FR30_INSN_ADDNI, FR30_INSN_ADDN2
 , FR30_INSN_SUB, FR30_INSN_SUBC, FR30_INSN_SUBN, FR30_INSN_CMP
 , FR30_INSN_CMPI, FR30_INSN_CMP2, FR30_INSN_AND, FR30_INSN_OR
 , FR30_INSN_EOR, FR30_INSN_ANDM, FR30_INSN_ANDH, FR30_INSN_ANDB
 , FR30_INSN_ORM, FR30_INSN_ORH, FR30_INSN_ORB, FR30_INSN_EORM
 , FR30_INSN_EORH, FR30_INSN_EORB, FR30_INSN_BANDL, FR30_INSN_BORL
 , FR30_INSN_BEORL, FR30_INSN_BANDH, FR30_INSN_BORH, FR30_INSN_BEORH
 , FR30_INSN_BTSTL, FR30_INSN_BTSTH, FR30_INSN_MUL, FR30_INSN_MULU
 , FR30_INSN_MULH, FR30_INSN_MULUH, FR30_INSN_DIV0S, FR30_INSN_DIV0U
 , FR30_INSN_DIV1, FR30_INSN_DIV2, FR30_INSN_DIV3, FR30_INSN_DIV4S
 , FR30_INSN_LSL, FR30_INSN_LSLI, FR30_INSN_LSL2, FR30_INSN_LSR
 , FR30_INSN_LSRI, FR30_INSN_LSR2, FR30_INSN_ASR, FR30_INSN_ASRI
 , FR30_INSN_ASR2, FR30_INSN_LDI8, FR30_INSN_LDI20, FR30_INSN_LDI32
 , FR30_INSN_LD, FR30_INSN_LDUH, FR30_INSN_LDUB, FR30_INSN_LDR13
 , FR30_INSN_LDR13UH, FR30_INSN_LDR13UB, FR30_INSN_LDR14, FR30_INSN_LDR14UH
 , FR30_INSN_LDR14UB, FR30_INSN_LDR15, FR30_INSN_LDR15GR, FR30_INSN_LDR15DR
 , FR30_INSN_LDR15PS, FR30_INSN_ST, FR30_INSN_STH, FR30_INSN_STB
 , FR30_INSN_STR13, FR30_INSN_STR13H, FR30_INSN_STR13B, FR30_INSN_STR14
 , FR30_INSN_STR14H, FR30_INSN_STR14B, FR30_INSN_STR15, FR30_INSN_STR15GR
 , FR30_INSN_STR15DR, FR30_INSN_STR15PS, FR30_INSN_MOV, FR30_INSN_MOVDR
 , FR30_INSN_MOVPS, FR30_INSN_MOV2DR, FR30_INSN_MOV2PS, FR30_INSN_JMP
 , FR30_INSN_JMPD, FR30_INSN_CALLR, FR30_INSN_CALLRD, FR30_INSN_CALL
 , FR30_INSN_CALLD, FR30_INSN_RET, FR30_INSN_RET_D, FR30_INSN_INT
 , FR30_INSN_INTE, FR30_INSN_RETI, FR30_INSN_BRAD, FR30_INSN_BRA
 , FR30_INSN_BNOD, FR30_INSN_BNO, FR30_INSN_BEQD, FR30_INSN_BEQ
 , FR30_INSN_BNED, FR30_INSN_BNE, FR30_INSN_BCD, FR30_INSN_BC
 , FR30_INSN_BNCD, FR30_INSN_BNC, FR30_INSN_BND, FR30_INSN_BN
 , FR30_INSN_BPD, FR30_INSN_BP, FR30_INSN_BVD, FR30_INSN_BV
 , FR30_INSN_BNVD, FR30_INSN_BNV, FR30_INSN_BLTD, FR30_INSN_BLT
 , FR30_INSN_BGED, FR30_INSN_BGE, FR30_INSN_BLED, FR30_INSN_BLE
 , FR30_INSN_BGTD, FR30_INSN_BGT, FR30_INSN_BLSD, FR30_INSN_BLS
 , FR30_INSN_BHID, FR30_INSN_BHI, FR30_INSN_DMOVR13, FR30_INSN_DMOVR13H
 , FR30_INSN_DMOVR13B, FR30_INSN_DMOVR13PI, FR30_INSN_DMOVR13PIH, FR30_INSN_DMOVR13PIB
 , FR30_INSN_DMOVR15PI, FR30_INSN_DMOV2R13, FR30_INSN_DMOV2R13H, FR30_INSN_DMOV2R13B
 , FR30_INSN_DMOV2R13PI, FR30_INSN_DMOV2R13PIH, FR30_INSN_DMOV2R13PIB, FR30_INSN_DMOV2R15PD
 , FR30_INSN_LDRES, FR30_INSN_STRES, FR30_INSN_COPOP, FR30_INSN_COPLD
 , FR30_INSN_COPST, FR30_INSN_COPSV, FR30_INSN_NOP, FR30_INSN_ANDCCR
 , FR30_INSN_ORCCR, FR30_INSN_STILM, FR30_INSN_ADDSP, FR30_INSN_EXTSB
 , FR30_INSN_EXTUB, FR30_INSN_EXTSH, FR30_INSN_EXTUH, FR30_INSN_LDM0
 , FR30_INSN_LDM1, FR30_INSN_STM0, FR30_INSN_STM1, FR30_INSN_ENTER
 , FR30_INSN_LEAVE, FR30_INSN_XCHB
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID FR30_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) FR30_INSN_XCHB + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_op1;
  long f_op2;
  long f_op3;
  long f_op4;
  long f_op5;
  long f_cc;
  long f_ccc;
  long f_Rj;
  long f_Ri;
  long f_Rs1;
  long f_Rs2;
  long f_Rjc;
  long f_Ric;
  long f_CRj;
  long f_CRi;
  long f_u4;
  long f_u4c;
  long f_i4;
  long f_m4;
  long f_u8;
  long f_i8;
  long f_i20_4;
  long f_i20_16;
  long f_i20;
  long f_i32;
  long f_udisp6;
  long f_disp8;
  long f_disp9;
  long f_disp10;
  long f_s10;
  long f_u10;
  long f_rel9;
  long f_dir8;
  long f_dir9;
  long f_dir10;
  long f_rel12;
  long f_reglist_hi_st;
  long f_reglist_low_st;
  long f_reglist_hi_ld;
  long f_reglist_low_ld;
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


#endif /* FR30_OPC_H */
