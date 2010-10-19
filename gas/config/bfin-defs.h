/* bfin-defs.h ADI Blackfin gas header file
   Copyright 2005
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#ifndef BFIN_PARSE_H
#define BFIN_PARSE_H  

#include <bfd.h>
#include "as.h"

#define PCREL	1
#define CODE_FRAG_SIZE 4096  /* 1 page.  */  


/* Definition for all status bits.  */
typedef enum
{
  c_0,
  c_1,
  c_4,
  c_2,
  c_uimm2,
  c_uimm3,
  c_imm3,
  c_pcrel4, 
  c_imm4,
  c_uimm4s4,
  c_uimm4,
  c_uimm4s2,
  c_negimm5s4,
  c_imm5,
  c_uimm5,
  c_imm6,  
  c_imm7,
  c_imm8,
  c_uimm8,
  c_pcrel8,
  c_uimm8s4,
  c_pcrel8s4,
  c_lppcrel10,
  c_pcrel10, 
  c_pcrel12,
  c_imm16s4,
  c_luimm16,
  c_imm16,
  c_huimm16,
  c_rimm16,
  c_imm16s2,
  c_uimm16s4, 
  c_uimm16,
  c_pcrel24 
} const_forms_t;


/* High-Nibble: group code, low nibble: register code.  */


#define T_REG_R       0x00
#define T_REG_P       0x10
#define T_REG_I       0x20
#define T_REG_B       0x30
#define T_REG_L       0x34
#define T_REG_M       0x24
#define T_REG_A       0x40

/* All registers above this value don't
   belong to a usuable register group.  */
#define T_NOGROUP     0xa0

/* Flags.  */
#define F_REG_ALL    0x1000
#define F_REG_HIGH   0x2000  /* Half register: high half.  */

enum machine_registers
{
  REG_R0    = T_REG_R, REG_R1, REG_R2, REG_R3, REG_R4, REG_R5, REG_R6, REG_R7, 
  REG_P0    = T_REG_P, REG_P1, REG_P2, REG_P3, REG_P4, REG_P5, REG_SP, REG_FP,
  REG_I0    = T_REG_I, REG_I1, REG_I2, REG_I3,
  REG_M0    = T_REG_M, REG_M1, REG_M2, REG_M3, 
  REG_B0    = T_REG_B, REG_B1, REG_B2, REG_B3,
  REG_L0    = T_REG_L, REG_L1, REG_L2, REG_L3, 
  REG_A0x   = T_REG_A, REG_A0w, REG_A1x, REG_A1w,
  REG_ASTAT = 0x46,
  REG_RETS  = 0x47,
  REG_LC0   = 0x60, REG_LT0, REG_LB0,  REG_LC1, REG_LT1, REG_LB1,
              REG_CYCLES, REG_CYCLES2,
  REG_USP   = 0x70, REG_SEQSTAT, REG_SYSCFG,
	      REG_RETI, REG_RETX, REG_RETN, REG_RETE, REG_EMUDAT, 

/* These don't have groups.  */
  REG_sftreset = T_NOGROUP, REG_omode, REG_excause, REG_emucause,
	         REG_idle_req, REG_hwerrcause,
  REG_A0       = 0xc0, REG_A1, REG_CC,
/* Pseudo registers, used only for distinction from symbols.  */
		 REG_RL0, REG_RL1, REG_RL2, REG_RL3,
		 REG_RL4, REG_RL5, REG_RL6, REG_RL7, 
		 REG_RH0, REG_RH1, REG_RH2, REG_RH3,
		 REG_RH4, REG_RH5, REG_RH6, REG_RH7, 
		 REG_LASTREG
};

/* Status register flags.  */

enum statusflags
{
  S_AZ = 0,
  S_AN,
  S_AQ = 6,
  S_AC0 = 12,
  S_AC1,
  S_AV0 = 16,
  S_AV0S,
  S_AV1,
  S_AV1S,
  S_V = 24,
  S_VS = 25
}; 


enum reg_class
{
  rc_dregs_lo,
  rc_dregs_hi,
  rc_dregs,
  rc_dregs_pair,
  rc_pregs,
  rc_spfp,
  rc_dregs_hilo,
  rc_accum_ext,
  rc_accum_word,
  rc_accum,
  rc_iregs,
  rc_mregs,
  rc_bregs,
  rc_lregs,
  rc_dpregs,
  rc_gregs,
  rc_regs,
  rc_statbits,
  rc_ignore_bits,
  rc_ccstat,
  rc_counters,
  rc_dregs2_sysregs1,
  rc_open,
  rc_sysregs2,
  rc_sysregs3,
  rc_allregs,
  LIM_REG_CLASSES
};

/* mmod field.  */
#define M_S2RND 1
#define M_T     2
#define M_W32   3
#define M_FU    4
#define M_TFU   6
#define M_IS    8
#define M_ISS2  9
#define M_IH    11
#define M_IU    12

/* Register type checking macros.  */

#define CODE_MASK  0x07
#define CLASS_MASK 0xf0

#define REG_SAME(a, b)   ((a).regno == (b).regno)
#define REG_EQUAL(a, b)  (((a).regno & CODE_MASK) == ((b).regno & CODE_MASK))
#define REG_CLASS(a)     ((a.regno) & 0xf0)
#define IS_A1(a)         ((a).regno == REG_A1)
#define IS_H(a)          ((a).regno & F_REG_HIGH ? 1: 0)
#define IS_EVEN(r)       (r.regno % 2 == 0)
#define IS_HCOMPL(a, b)  (REG_EQUAL(a, b) && \
                         ((a).regno & F_REG_HIGH) != ((b).regno & F_REG_HIGH))

/* register type checking.  */
#define _TYPECHECK(r, x) (((r).regno & CLASS_MASK) == T_REG_##x)

#define IS_DREG(r)       _TYPECHECK(r, R)
#define IS_DREG_H(r)     (_TYPECHECK(r, R) && IS_H(r))
#define IS_DREG_L(r)     (_TYPECHECK(r, R) && !IS_H(r))
#define IS_PREG(r)       _TYPECHECK(r, P)
#define IS_IREG(r)       (((r).regno & 0xf4) == T_REG_I)
#define IS_MREG(r)       (((r).regno & 0xf4) == T_REG_M)
#define IS_BREG(r)       (((r).regno & 0xf4) == T_REG_B)
#define IS_LREG(r)       (((r).regno & 0xf4) == T_REG_L)
#define IS_CREG(r)       ((r).regno == REG_LC0 || (r).regno == REG_LC1)
#define IS_ALLREG(r)     ((r).regno < T_NOGROUP)

/* Expression value macros.  */

typedef enum
{ 
  ones_compl,
  twos_compl,
  mult,
  divide,
  mod,
  add,
  sub,
  lsh,
  rsh,
  logand,
  logior,
  logxor
} expr_opcodes_t;

struct expressionS;

#define SYMBOL_T       symbolS*
 
struct expression_cell
{
  int value;
  SYMBOL_T symbol;
};

/* User Type Definitions.  */
struct bfin_insn
{
  unsigned long value;
  struct bfin_insn *next;
  struct expression_cell *exp;
  int pcrel;
  int reloc;
};
    
#define INSTR_T struct bfin_insn*
#define EXPR_T  struct expression_cell* 

typedef struct expr_node_struct Expr_Node;
 
extern INSTR_T gencode (unsigned long x);
extern INSTR_T conscode (INSTR_T head, INSTR_T tail);   
extern INSTR_T conctcode (INSTR_T head, INSTR_T tail);
extern INSTR_T note_reloc
       (INSTR_T code, Expr_Node *, int reloc,int pcrel);
extern INSTR_T note_reloc1
       (INSTR_T code, const char * sym, int reloc, int pcrel);
extern INSTR_T note_reloc2
       (INSTR_T code, const char *symbol, int reloc, int value, int pcrel);
 
/* Types of expressions.  */
typedef enum 
{
  Expr_Node_Binop,		/* Binary operator.  */
  Expr_Node_Unop,		/* Unary operator.  */
  Expr_Node_Reloc,		/* Symbol to be relocated.  */
  Expr_Node_GOT_Reloc,		/* Symbol to be relocated using the GOT.  */
  Expr_Node_Constant 		/* Constant.  */
} Expr_Node_Type;

/* Types of operators.  */
typedef enum 
{
  Expr_Op_Type_Add,
  Expr_Op_Type_Sub,
  Expr_Op_Type_Mult,
  Expr_Op_Type_Div,
  Expr_Op_Type_Mod,
  Expr_Op_Type_Lshift,
  Expr_Op_Type_Rshift,
  Expr_Op_Type_BAND,		/* Bitwise AND.  */
  Expr_Op_Type_BOR,		/* Bitwise OR.  */
  Expr_Op_Type_BXOR,		/* Bitwise exclusive OR.  */
  Expr_Op_Type_LAND,		/* Logical AND.  */
  Expr_Op_Type_LOR,		/* Logical OR.  */
  Expr_Op_Type_NEG,
  Expr_Op_Type_COMP		/* Complement.  */
} Expr_Op_Type;

/* The value that can be stored ... depends on type.  */
typedef union
{
  const char *s_value;		/* if relocation symbol, the text.  */
  int i_value;			/* if constant, the value.  */
  Expr_Op_Type op_value;	/* if operator, the value.  */
} Expr_Node_Value;

/* The expression node.  */
struct expr_node_struct
{
  Expr_Node_Type 	type;
  Expr_Node_Value	value;
  Expr_Node		*Left_Child;
  Expr_Node		*Right_Child;
};


/* Operations on the expression node.  */
Expr_Node *Expr_Node_Create (Expr_Node_Type type, 
		         Expr_Node_Value value, 
			 Expr_Node *Left_Child, 
			 Expr_Node *Right_Child);

/* Generate the reloc structure as a series of instructions.  */
INSTR_T Expr_Node_Gen_Reloc (Expr_Node *head, int parent_reloc);
 
#define MKREF(x)	mkexpr (0,x)
#define ALLOCATE(x)	malloc (x)
 
#define NULL_CODE ((INSTR_T) 0)

#ifndef EXPR_VALUE
#define EXPR_VALUE(x)  (((x)->type == Expr_Node_Constant) ? ((x)->value.i_value) : 0)
#endif
#ifndef EXPR_SYMBOL
#define EXPR_SYMBOL(x) ((x)->symbol)
#endif


typedef long reg_t;


typedef struct _register
{
  reg_t regno;       /* Register ID as defined in machine_registers.  */
  int   flags;
} Register;


typedef struct _macfunc
{
  char n;
  char op;
  char w;
  char P;
  Register dst;
  Register s0;
  Register s1;
} Macfunc;

typedef struct _opt_mode
{
  int MM;
  int mod;
} Opt_mode;

typedef enum
{
  SEMANTIC_ERROR,
  NO_INSN_GENERATED,
  INSN_GENERATED
} parse_state;


#ifdef __cplusplus
extern "C" {
#endif

extern int debug_codeselection;

void error (char *format, ...);
void warn (char *format, ...);
int  semantic_error (char *syntax);
void semantic_error_2 (char *syntax);

EXPR_T mkexpr (int, SYMBOL_T);

extern void bfin_equals (Expr_Node *sym);
/* Defined in bfin-lex.l.  */
void set_start_state (void);

#ifdef __cplusplus
}
#endif

#endif  /* BFIN_PARSE_H */

