/* Instruction opcode header for openrisc.

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

#ifndef OPENRISC_OPC_H
#define OPENRISC_OPC_H

/* -- opc.h */
#undef  CGEN_DIS_HASH_SIZE
#define CGEN_DIS_HASH_SIZE 64
#undef  CGEN_DIS_HASH
#define CGEN_DIS_HASH(buffer, value) (((unsigned char *) (buffer))[0] >> 2)

extern long openrisc_sign_extend_16bit (long);
/* -- */
/* Enum declaration for openrisc instruction types.  */
typedef enum cgen_insn_type {
  OPENRISC_INSN_INVALID, OPENRISC_INSN_L_J, OPENRISC_INSN_L_JAL, OPENRISC_INSN_L_JR
 , OPENRISC_INSN_L_JALR, OPENRISC_INSN_L_BAL, OPENRISC_INSN_L_BNF, OPENRISC_INSN_L_BF
 , OPENRISC_INSN_L_BRK, OPENRISC_INSN_L_RFE, OPENRISC_INSN_L_SYS, OPENRISC_INSN_L_NOP
 , OPENRISC_INSN_L_MOVHI, OPENRISC_INSN_L_MFSR, OPENRISC_INSN_L_MTSR, OPENRISC_INSN_L_LW
 , OPENRISC_INSN_L_LBZ, OPENRISC_INSN_L_LBS, OPENRISC_INSN_L_LHZ, OPENRISC_INSN_L_LHS
 , OPENRISC_INSN_L_SW, OPENRISC_INSN_L_SB, OPENRISC_INSN_L_SH, OPENRISC_INSN_L_SLL
 , OPENRISC_INSN_L_SLLI, OPENRISC_INSN_L_SRL, OPENRISC_INSN_L_SRLI, OPENRISC_INSN_L_SRA
 , OPENRISC_INSN_L_SRAI, OPENRISC_INSN_L_ROR, OPENRISC_INSN_L_RORI, OPENRISC_INSN_L_ADD
 , OPENRISC_INSN_L_ADDI, OPENRISC_INSN_L_SUB, OPENRISC_INSN_L_SUBI, OPENRISC_INSN_L_AND
 , OPENRISC_INSN_L_ANDI, OPENRISC_INSN_L_OR, OPENRISC_INSN_L_ORI, OPENRISC_INSN_L_XOR
 , OPENRISC_INSN_L_XORI, OPENRISC_INSN_L_MUL, OPENRISC_INSN_L_MULI, OPENRISC_INSN_L_DIV
 , OPENRISC_INSN_L_DIVU, OPENRISC_INSN_L_SFGTS, OPENRISC_INSN_L_SFGTU, OPENRISC_INSN_L_SFGES
 , OPENRISC_INSN_L_SFGEU, OPENRISC_INSN_L_SFLTS, OPENRISC_INSN_L_SFLTU, OPENRISC_INSN_L_SFLES
 , OPENRISC_INSN_L_SFLEU, OPENRISC_INSN_L_SFGTSI, OPENRISC_INSN_L_SFGTUI, OPENRISC_INSN_L_SFGESI
 , OPENRISC_INSN_L_SFGEUI, OPENRISC_INSN_L_SFLTSI, OPENRISC_INSN_L_SFLTUI, OPENRISC_INSN_L_SFLESI
 , OPENRISC_INSN_L_SFLEUI, OPENRISC_INSN_L_SFEQ, OPENRISC_INSN_L_SFEQI, OPENRISC_INSN_L_SFNE
 , OPENRISC_INSN_L_SFNEI
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID OPENRISC_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) OPENRISC_INSN_L_SFNEI + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_class;
  long f_sub;
  long f_r1;
  long f_r2;
  long f_r3;
  long f_simm16;
  long f_uimm16;
  long f_uimm5;
  long f_hi16;
  long f_lo16;
  long f_op1;
  long f_op2;
  long f_op3;
  long f_op4;
  long f_op5;
  long f_op6;
  long f_op7;
  long f_i16_1;
  long f_i16_2;
  long f_disp26;
  long f_abs26;
  long f_i16nc;
  long f_f_15_8;
  long f_f_10_3;
  long f_f_4_1;
  long f_f_7_3;
  long f_f_10_7;
  long f_f_10_11;
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


#endif /* OPENRISC_OPC_H */
