/* Instruction opcode header for mt.

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

#ifndef MT_OPC_H
#define MT_OPC_H

/* -- opc.h */

/* Check applicability of instructions against machines.  */
#define CGEN_VALIDATE_INSN_SUPPORTED

/* Allows reason codes to be output when assembler errors occur.  */
#define CGEN_VERBOSE_ASSEMBLER_ERRORS

/* Override disassembly hashing - there are variable bits in the top
   byte of these instructions.  */
#define CGEN_DIS_HASH_SIZE 8
#define CGEN_DIS_HASH(buf, value) (((* (unsigned char *) (buf)) >> 5) % CGEN_DIS_HASH_SIZE)

#define CGEN_ASM_HASH_SIZE 127
#define CGEN_ASM_HASH(insn) mt_asm_hash (insn)

extern unsigned int mt_asm_hash (const char *);

extern int mt_cgen_insn_supported (CGEN_CPU_DESC, const CGEN_INSN *);


/* -- opc.c */
/* Enum declaration for mt instruction types.  */
typedef enum cgen_insn_type {
  MT_INSN_INVALID, MT_INSN_ADD, MT_INSN_ADDU, MT_INSN_ADDI
 , MT_INSN_ADDUI, MT_INSN_SUB, MT_INSN_SUBU, MT_INSN_SUBI
 , MT_INSN_SUBUI, MT_INSN_MUL, MT_INSN_MULI, MT_INSN_AND
 , MT_INSN_ANDI, MT_INSN_OR, MT_INSN_NOP, MT_INSN_ORI
 , MT_INSN_XOR, MT_INSN_XORI, MT_INSN_NAND, MT_INSN_NANDI
 , MT_INSN_NOR, MT_INSN_NORI, MT_INSN_XNOR, MT_INSN_XNORI
 , MT_INSN_LDUI, MT_INSN_LSL, MT_INSN_LSLI, MT_INSN_LSR
 , MT_INSN_LSRI, MT_INSN_ASR, MT_INSN_ASRI, MT_INSN_BRLT
 , MT_INSN_BRLE, MT_INSN_BREQ, MT_INSN_BRNE, MT_INSN_JMP
 , MT_INSN_JAL, MT_INSN_DBNZ, MT_INSN_EI, MT_INSN_DI
 , MT_INSN_SI, MT_INSN_RETI, MT_INSN_LDW, MT_INSN_STW
 , MT_INSN_BREAK, MT_INSN_IFLUSH, MT_INSN_LDCTXT, MT_INSN_LDFB
 , MT_INSN_STFB, MT_INSN_FBCB, MT_INSN_MFBCB, MT_INSN_FBCCI
 , MT_INSN_FBRCI, MT_INSN_FBCRI, MT_INSN_FBRRI, MT_INSN_MFBCCI
 , MT_INSN_MFBRCI, MT_INSN_MFBCRI, MT_INSN_MFBRRI, MT_INSN_FBCBDR
 , MT_INSN_RCFBCB, MT_INSN_MRCFBCB, MT_INSN_CBCAST, MT_INSN_DUPCBCAST
 , MT_INSN_WFBI, MT_INSN_WFB, MT_INSN_RCRISC, MT_INSN_FBCBINC
 , MT_INSN_RCXMODE, MT_INSN_INTERLEAVER, MT_INSN_WFBINC, MT_INSN_MWFBINC
 , MT_INSN_WFBINCR, MT_INSN_MWFBINCR, MT_INSN_FBCBINCS, MT_INSN_MFBCBINCS
 , MT_INSN_FBCBINCRS, MT_INSN_MFBCBINCRS, MT_INSN_LOOP, MT_INSN_LOOPI
 , MT_INSN_DFBC, MT_INSN_DWFB, MT_INSN_FBWFB, MT_INSN_DFBR
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID MT_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) MT_INSN_DFBR + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_msys;
  long f_opc;
  long f_imm;
  long f_uu24;
  long f_sr1;
  long f_sr2;
  long f_dr;
  long f_drrr;
  long f_imm16u;
  long f_imm16s;
  long f_imm16a;
  long f_uu4a;
  long f_uu4b;
  long f_uu12;
  long f_uu8;
  long f_uu16;
  long f_uu1;
  long f_msopc;
  long f_uu_26_25;
  long f_mask;
  long f_bankaddr;
  long f_rda;
  long f_uu_2_25;
  long f_rbbc;
  long f_perm;
  long f_mode;
  long f_uu_1_24;
  long f_wr;
  long f_fbincr;
  long f_uu_2_23;
  long f_xmode;
  long f_a23;
  long f_mask1;
  long f_cr;
  long f_type;
  long f_incamt;
  long f_cbs;
  long f_uu_1_19;
  long f_ball;
  long f_colnum;
  long f_brc;
  long f_incr;
  long f_fbdisp;
  long f_uu_4_15;
  long f_length;
  long f_uu_1_15;
  long f_rc;
  long f_rcnum;
  long f_rownum;
  long f_cbx;
  long f_id;
  long f_size;
  long f_rownum1;
  long f_uu_3_11;
  long f_rc1;
  long f_ccb;
  long f_cbrb;
  long f_cdb;
  long f_rownum2;
  long f_cell;
  long f_uu_3_9;
  long f_contnum;
  long f_uu_1_6;
  long f_dup;
  long f_rc2;
  long f_ctxdisp;
  long f_imm16l;
  long f_loopo;
  long f_cb1sel;
  long f_cb2sel;
  long f_cb1incr;
  long f_cb2incr;
  long f_rc3;
  long f_msysfrsr2;
  long f_brc2;
  long f_ball2;
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


#endif /* MT_OPC_H */
