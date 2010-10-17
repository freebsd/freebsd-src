/* Instruction opcode header for ip2k.

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

#ifndef IP2K_OPC_H
#define IP2K_OPC_H

/* -- opc.h */

/* Check applicability of instructions against machines.  */
#define CGEN_VALIDATE_INSN_SUPPORTED

/* Allows reason codes to be output when assembler errors occur.  */
#define CGEN_VERBOSE_ASSEMBLER_ERRORS

/* Override disassembly hashing - there are variable bits in the top
   byte of these instructions.  */
#define CGEN_DIS_HASH_SIZE 8
#define CGEN_DIS_HASH(buf,value) (((* (unsigned char*) (buf)) >> 5) % CGEN_DIS_HASH_SIZE)

#define CGEN_ASM_HASH_SIZE 127
#define CGEN_ASM_HASH(insn) ip2k_asm_hash(insn)

extern unsigned int ip2k_asm_hash PARAMS ((const char *insn));
extern int ip2k_cgen_insn_supported
  PARAMS ((CGEN_CPU_DESC, const CGEN_INSN *));

/* -- opc.c */
/* Enum declaration for ip2k instruction types.  */
typedef enum cgen_insn_type {
  IP2K_INSN_INVALID, IP2K_INSN_JMP, IP2K_INSN_CALL, IP2K_INSN_SB
 , IP2K_INSN_SNB, IP2K_INSN_SETB, IP2K_INSN_CLRB, IP2K_INSN_XORW_L
 , IP2K_INSN_ANDW_L, IP2K_INSN_ORW_L, IP2K_INSN_ADDW_L, IP2K_INSN_SUBW_L
 , IP2K_INSN_CMPW_L, IP2K_INSN_RETW_L, IP2K_INSN_CSEW_L, IP2K_INSN_CSNEW_L
 , IP2K_INSN_PUSH_L, IP2K_INSN_MULSW_L, IP2K_INSN_MULUW_L, IP2K_INSN_LOADL_L
 , IP2K_INSN_LOADH_L, IP2K_INSN_LOADL_A, IP2K_INSN_LOADH_A, IP2K_INSN_ADDCFR_W
 , IP2K_INSN_ADDCW_FR, IP2K_INSN_INCSNZ_FR, IP2K_INSN_INCSNZW_FR, IP2K_INSN_MULSW_FR
 , IP2K_INSN_MULUW_FR, IP2K_INSN_DECSNZ_FR, IP2K_INSN_DECSNZW_FR, IP2K_INSN_SUBCW_FR
 , IP2K_INSN_SUBCFR_W, IP2K_INSN_POP_FR, IP2K_INSN_PUSH_FR, IP2K_INSN_CSEW_FR
 , IP2K_INSN_CSNEW_FR, IP2K_INSN_INCSZ_FR, IP2K_INSN_INCSZW_FR, IP2K_INSN_SWAP_FR
 , IP2K_INSN_SWAPW_FR, IP2K_INSN_RL_FR, IP2K_INSN_RLW_FR, IP2K_INSN_RR_FR
 , IP2K_INSN_RRW_FR, IP2K_INSN_DECSZ_FR, IP2K_INSN_DECSZW_FR, IP2K_INSN_INC_FR
 , IP2K_INSN_INCW_FR, IP2K_INSN_NOT_FR, IP2K_INSN_NOTW_FR, IP2K_INSN_TEST_FR
 , IP2K_INSN_MOVW_L, IP2K_INSN_MOVFR_W, IP2K_INSN_MOVW_FR, IP2K_INSN_ADDFR_W
 , IP2K_INSN_ADDW_FR, IP2K_INSN_XORFR_W, IP2K_INSN_XORW_FR, IP2K_INSN_ANDFR_W
 , IP2K_INSN_ANDW_FR, IP2K_INSN_ORFR_W, IP2K_INSN_ORW_FR, IP2K_INSN_DEC_FR
 , IP2K_INSN_DECW_FR, IP2K_INSN_SUBFR_W, IP2K_INSN_SUBW_FR, IP2K_INSN_CLR_FR
 , IP2K_INSN_CMPW_FR, IP2K_INSN_SPEED, IP2K_INSN_IREADI, IP2K_INSN_IWRITEI
 , IP2K_INSN_FREAD, IP2K_INSN_FWRITE, IP2K_INSN_IREAD, IP2K_INSN_IWRITE
 , IP2K_INSN_PAGE, IP2K_INSN_SYSTEM, IP2K_INSN_RETI, IP2K_INSN_RET
 , IP2K_INSN_INT, IP2K_INSN_BREAKX, IP2K_INSN_CWDT, IP2K_INSN_FERASE
 , IP2K_INSN_RETNP, IP2K_INSN_BREAK, IP2K_INSN_NOP
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID IP2K_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) IP2K_INSN_NOP + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_imm8;
  long f_reg;
  long f_addr16cjp;
  long f_dir;
  long f_bitno;
  long f_op3;
  long f_op4;
  long f_op4mid;
  long f_op6;
  long f_op8;
  long f_op6_10low;
  long f_op6_7low;
  long f_reti3;
  long f_skipb;
  long f_page3;
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


#endif /* IP2K_OPC_H */
