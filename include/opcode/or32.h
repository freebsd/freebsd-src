/* Table of opcodes for the OpenRISC 1000 ISA.
   Copyright 2002, 2003 Free Software Foundation, Inc.
   Contributed by Damjan Lampret (lampret@opencores.org).
   
   This file is part of or1k_gen_isa, or1ksim, GDB and GAS.

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

/* We treat all letters the same in encode/decode routines so
   we need to assign some characteristics to them like signess etc.  */

#ifndef OR32_H_ISA
#define OR32_H_ISA

#define NUM_UNSIGNED (0)
#define NUM_SIGNED (1)

#define MAX_GPRS 32
#define PAGE_SIZE 4096
#undef __HALF_WORD_INSN__

#define OPERAND_DELIM (',')

#define OR32_IF_DELAY (1)
#define OR32_W_FLAG   (2)
#define OR32_R_FLAG   (4)

struct or32_letter
{
  char letter;
  int  sign;
  /* int  reloc; relocation per letter ??  */
};

/* Main instruction specification array.  */
struct or32_opcode
{
  /* Name of the instruction.  */
  char *name;

  /* A string of characters which describe the operands.
     Valid characters are:
     ,() Itself.  Characters appears in the assembly code.
     rA	 Register operand.
     rB  Register operand.
     rD  Register operand.
     I	 An immediate operand, range -32768 to 32767.
     J	 An immediate operand, range . (unused)
     K	 An immediate operand, range 0 to 65535.
     L	 An immediate operand, range 0 to 63.
     M	 An immediate operand, range . (unused)
     N	 An immediate operand, range -33554432 to 33554431.
     O	 An immediate operand, range . (unused).  */
  char *args;
  
  /* Opcode and operand encoding.  */
  char *encoding;
  void (*exec) (void);
  unsigned int flags;
};

#define OPTYPE_LAST (0x80000000)
#define OPTYPE_OP   (0x40000000)
#define OPTYPE_REG  (0x20000000)
#define OPTYPE_SIG  (0x10000000)
#define OPTYPE_DIS  (0x08000000)
#define OPTYPE_DST  (0x04000000)
#define OPTYPE_SBIT (0x00001F00)
#define OPTYPE_SHR  (0x0000001F)
#define OPTYPE_SBIT_SHR (8)

/* MM: Data how to decode operands.  */
extern struct insn_op_struct
{
  unsigned long type;
  unsigned long data;
} **op_start;

#ifdef HAS_EXECUTION
extern void l_invalid (void);
extern void l_sfne    (void);
extern void l_bf      (void);
extern void l_add     (void);
extern void l_sw      (void);
extern void l_sb      (void);
extern void l_sh      (void);
extern void l_lwz     (void);
extern void l_lbs     (void);
extern void l_lbz     (void);
extern void l_lhs     (void);
extern void l_lhz     (void);
extern void l_movhi   (void);
extern void l_and     (void);
extern void l_or      (void);
extern void l_xor     (void);
extern void l_sub     (void);
extern void l_mul     (void);
extern void l_div     (void);
extern void l_divu    (void);
extern void l_sll     (void);
extern void l_sra     (void);
extern void l_srl     (void);
extern void l_j       (void);
extern void l_jal     (void);
extern void l_jalr    (void);
extern void l_jr      (void);
extern void l_rfe     (void);
extern void l_nop     (void);
extern void l_bnf     (void);
extern void l_sfeq    (void);
extern void l_sfgts   (void);
extern void l_sfges   (void);
extern void l_sflts   (void);
extern void l_sfles   (void);
extern void l_sfgtu   (void);
extern void l_sfgeu   (void);
extern void l_sfltu   (void);
extern void l_sfleu   (void);
extern void l_mtspr   (void);
extern void l_mfspr   (void);
extern void l_sys     (void);
extern void l_trap    (void); /* CZ 21/06/01.  */
extern void l_macrc   (void);
extern void l_mac     (void);
extern void l_msb     (void);
extern void l_invalid (void);
extern void l_cust1   (void);
extern void l_cust2   (void);
extern void l_cust3   (void);
extern void l_cust4   (void);
#endif
extern void l_none    (void);

extern const struct or32_letter or32_letters[];

extern const struct  or32_opcode or32_opcodes[];

extern const unsigned int or32_num_opcodes;

/* Calculates instruction length in bytes.  Always 4 for OR32.  */
extern int insn_len (int);

/* Is individual insn's operand signed or unsigned?  */
extern int letter_signed (char);

/* Number of letters in the individual lettered operand.  */
extern int letter_range (char);

/* MM: Returns index of given instruction name.  */
extern int insn_index (char *);

/* MM: Returns instruction name from index.  */
extern const char *insn_name (int);

/* MM: Constructs new FSM, based on or32_opcodes.  */ 
extern void build_automata (void);

/* MM: Destructs FSM.  */ 
extern void destruct_automata (void);

/* MM: Decodes instruction using FSM.  Call build_automata first.  */
extern int insn_decode (unsigned int);

/* Disassemble one instruction from insn to disassemble.
   Return the size of the instruction.  */
int disassemble_insn (unsigned long);

#endif
