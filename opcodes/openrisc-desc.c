/* CPU data for openrisc.

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

#include "sysdep.h"
#include <stdio.h>
#include <stdarg.h>
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "openrisc-desc.h"
#include "openrisc-opc.h"
#include "opintl.h"
#include "libiberty.h"
#include "xregex.h"

/* Attributes.  */

static const CGEN_ATTR_ENTRY bool_attr[] =
{
  { "#f", 0 },
  { "#t", 1 },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY MACH_attr[] =
{
  { "base", MACH_BASE },
  { "openrisc", MACH_OPENRISC },
  { "or1300", MACH_OR1300 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] =
{
  { "or32", ISA_OR32 },
  { "max", ISA_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY HAS_CACHE_attr[] =
{
  { "DATA_CACHE", HAS_CACHE_DATA_CACHE },
  { "INSN_CACHE", HAS_CACHE_INSN_CACHE },
  { 0, 0 }
};

const CGEN_ATTR_TABLE openrisc_cgen_ifield_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "RESERVED", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE openrisc_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE openrisc_cgen_operand_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { "NEGATIVE", &bool_attr[0], &bool_attr[0] },
  { "RELAX", &bool_attr[0], &bool_attr[0] },
  { "SEM-ONLY", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE openrisc_cgen_insn_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "ALIAS", &bool_attr[0], &bool_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNCOND-CTI", &bool_attr[0], &bool_attr[0] },
  { "COND-CTI", &bool_attr[0], &bool_attr[0] },
  { "SKIP-CTI", &bool_attr[0], &bool_attr[0] },
  { "DELAY-SLOT", &bool_attr[0], &bool_attr[0] },
  { "RELAXABLE", &bool_attr[0], &bool_attr[0] },
  { "RELAXED", &bool_attr[0], &bool_attr[0] },
  { "NO-DIS", &bool_attr[0], &bool_attr[0] },
  { "PBB", &bool_attr[0], &bool_attr[0] },
  { "NOT-IN-DELAY-SLOT", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA openrisc_cgen_isa_table[] = {
  { "or32", 32, 32, 32, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH openrisc_cgen_mach_table[] = {
  { "openrisc", "openrisc", MACH_OPENRISC, 0 },
  { "or1300", "openrisc:1300", MACH_OR1300, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY openrisc_cgen_opval_h_gr_entries[] =
{
  { "r0", 0, {0, {0}}, 0, 0 },
  { "r1", 1, {0, {0}}, 0, 0 },
  { "r2", 2, {0, {0}}, 0, 0 },
  { "r3", 3, {0, {0}}, 0, 0 },
  { "r4", 4, {0, {0}}, 0, 0 },
  { "r5", 5, {0, {0}}, 0, 0 },
  { "r6", 6, {0, {0}}, 0, 0 },
  { "r7", 7, {0, {0}}, 0, 0 },
  { "r8", 8, {0, {0}}, 0, 0 },
  { "r9", 9, {0, {0}}, 0, 0 },
  { "r10", 10, {0, {0}}, 0, 0 },
  { "r11", 11, {0, {0}}, 0, 0 },
  { "r12", 12, {0, {0}}, 0, 0 },
  { "r13", 13, {0, {0}}, 0, 0 },
  { "r14", 14, {0, {0}}, 0, 0 },
  { "r15", 15, {0, {0}}, 0, 0 },
  { "r16", 16, {0, {0}}, 0, 0 },
  { "r17", 17, {0, {0}}, 0, 0 },
  { "r18", 18, {0, {0}}, 0, 0 },
  { "r19", 19, {0, {0}}, 0, 0 },
  { "r20", 20, {0, {0}}, 0, 0 },
  { "r21", 21, {0, {0}}, 0, 0 },
  { "r22", 22, {0, {0}}, 0, 0 },
  { "r23", 23, {0, {0}}, 0, 0 },
  { "r24", 24, {0, {0}}, 0, 0 },
  { "r25", 25, {0, {0}}, 0, 0 },
  { "r26", 26, {0, {0}}, 0, 0 },
  { "r27", 27, {0, {0}}, 0, 0 },
  { "r28", 28, {0, {0}}, 0, 0 },
  { "r29", 29, {0, {0}}, 0, 0 },
  { "r30", 30, {0, {0}}, 0, 0 },
  { "r31", 31, {0, {0}}, 0, 0 },
  { "lr", 11, {0, {0}}, 0, 0 },
  { "sp", 1, {0, {0}}, 0, 0 },
  { "fp", 2, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD openrisc_cgen_opval_h_gr =
{
  & openrisc_cgen_opval_h_gr_entries[0],
  35,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY openrisc_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { (1<<MACH_BASE) } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & openrisc_cgen_opval_h_gr, { 0|A(PROFILE), { (1<<MACH_BASE) } } },
  { "h-sr", HW_H_SR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-hi16", HW_H_HI16, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-lo16", HW_H_LO16, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-cbit", HW_H_CBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-delay-insn", HW_H_DELAY_INSN, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { 0, 0, CGEN_ASM_NONE, 0, {0, {0}} }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD openrisc_cgen_ifld_table[] =
{
  { OPENRISC_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_CLASS, "f-class", 0, 32, 31, 2, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_SUB, "f-sub", 0, 32, 29, 4, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_R1, "f-r1", 0, 32, 25, 5, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_R2, "f-r2", 0, 32, 20, 5, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_R3, "f-r3", 0, 32, 15, 5, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_SIMM16, "f-simm16", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_UIMM16, "f-uimm16", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_UIMM5, "f-uimm5", 0, 32, 4, 5, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_HI16, "f-hi16", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_LO16, "f-lo16", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP1, "f-op1", 0, 32, 31, 2, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP2, "f-op2", 0, 32, 29, 4, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP3, "f-op3", 0, 32, 25, 2, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP4, "f-op4", 0, 32, 23, 3, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP5, "f-op5", 0, 32, 25, 5, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP6, "f-op6", 0, 32, 7, 3, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_OP7, "f-op7", 0, 32, 3, 4, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_I16_1, "f-i16-1", 0, 32, 10, 11, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_I16_2, "f-i16-2", 0, 32, 25, 5, { 0, { (1<<MACH_BASE) } }  },
  { OPENRISC_F_DISP26, "f-disp26", 0, 32, 25, 26, { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_ABS26, "f-abs26", 0, 32, 25, 26, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_I16NC, "f-i16nc", 0, 0, 0, 0,{ 0|A(SIGN_OPT)|A(VIRTUAL), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_F_15_8, "f-f-15-8", 0, 32, 15, 8, { 0|A(RESERVED), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_F_10_3, "f-f-10-3", 0, 32, 10, 3, { 0|A(RESERVED), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_F_4_1, "f-f-4-1", 0, 32, 4, 1, { 0|A(RESERVED), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_F_7_3, "f-f-7-3", 0, 32, 7, 3, { 0|A(RESERVED), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_F_10_7, "f-f-10-7", 0, 32, 10, 7, { 0|A(RESERVED), { (1<<MACH_BASE) } }  },
  { OPENRISC_F_F_10_11, "f-f-10-11", 0, 32, 10, 11, { 0|A(RESERVED), { (1<<MACH_BASE) } }  },
  { 0, 0, 0, 0, 0, 0, {0, {0}} }
};

#undef A



/* multi ifield declarations */

const CGEN_MAYBE_MULTI_IFLD OPENRISC_F_I16NC_MULTI_IFIELD [];


/* multi ifield definitions */

const CGEN_MAYBE_MULTI_IFLD OPENRISC_F_I16NC_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_I16_1] } },
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_I16_2] } },
    { 0, { (const PTR) 0 } }
};

/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) OPENRISC_OPERAND_##op
#else
#define OPERAND(op) OPENRISC_OPERAND_/**/op
#endif

const CGEN_OPERAND openrisc_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", OPENRISC_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_NIL] } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* sr: special register */
  { "sr", OPENRISC_OPERAND_SR, HW_H_SR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* cbit: condition bit */
  { "cbit", OPENRISC_OPERAND_CBIT, HW_H_CBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* simm-16: 16 bit signed immediate */
  { "simm-16", OPENRISC_OPERAND_SIMM_16, HW_H_SINT, 15, 16,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_SIMM16] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* uimm-16: 16 bit unsigned immediate */
  { "uimm-16", OPENRISC_OPERAND_UIMM_16, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_UIMM16] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* disp-26: pc-rel 26 bit */
  { "disp-26", OPENRISC_OPERAND_DISP_26, HW_H_IADDR, 25, 26,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_DISP26] } }, 
    { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
/* abs-26: abs 26 bit */
  { "abs-26", OPENRISC_OPERAND_ABS_26, HW_H_IADDR, 25, 26,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_ABS26] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
/* uimm-5: imm5 */
  { "uimm-5", OPENRISC_OPERAND_UIMM_5, HW_H_UINT, 4, 5,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_UIMM5] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rD: destination register */
  { "rD", OPENRISC_OPERAND_RD, HW_H_GR, 25, 5,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_R1] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rA: source register A */
  { "rA", OPENRISC_OPERAND_RA, HW_H_GR, 20, 5,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_R2] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rB: source register B */
  { "rB", OPENRISC_OPERAND_RB, HW_H_GR, 15, 5,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_R3] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* op-f-23: f-op23 */
  { "op-f-23", OPENRISC_OPERAND_OP_F_23, HW_H_UINT, 23, 3,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_OP4] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* op-f-3: f-op3 */
  { "op-f-3", OPENRISC_OPERAND_OP_F_3, HW_H_UINT, 25, 5,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_OP5] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* hi16: high 16 bit immediate, sign optional */
  { "hi16", OPENRISC_OPERAND_HI16, HW_H_HI16, 15, 16,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_SIMM16] } }, 
    { 0|A(SIGN_OPT), { (1<<MACH_BASE) } }  },
/* lo16: low 16 bit immediate, sign optional */
  { "lo16", OPENRISC_OPERAND_LO16, HW_H_LO16, 15, 16,
    { 0, { (const PTR) &openrisc_cgen_ifld_table[OPENRISC_F_LO16] } }, 
    { 0|A(SIGN_OPT), { (1<<MACH_BASE) } }  },
/* ui16nc: 16 bit immediate, sign optional */
  { "ui16nc", OPENRISC_OPERAND_UI16NC, HW_H_LO16, 10, 16,
    { 2, { (const PTR) &OPENRISC_F_I16NC_MULTI_IFIELD[0] } }, 
    { 0|A(SIGN_OPT)|A(VIRTUAL), { (1<<MACH_BASE) } }  },
/* sentinel */
  { 0, 0, 0, 0, 0,
    { 0, { (const PTR) 0 } },
    { 0, { 0 } } }
};

#undef A


/* The instruction table.  */

#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif

static const CGEN_IBASE openrisc_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, {0, {0}} },
/* l.j ${abs-26} */
  {
    OPENRISC_INSN_L_J, "l-j", "l.j", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.jal ${abs-26} */
  {
    OPENRISC_INSN_L_JAL, "l-jal", "l.jal", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.jr $rA */
  {
    OPENRISC_INSN_L_JR, "l-jr", "l.jr", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.jalr $rA */
  {
    OPENRISC_INSN_L_JALR, "l-jalr", "l.jalr", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.bal ${disp-26} */
  {
    OPENRISC_INSN_L_BAL, "l-bal", "l.bal", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.bnf ${disp-26} */
  {
    OPENRISC_INSN_L_BNF, "l-bnf", "l.bnf", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.bf ${disp-26} */
  {
    OPENRISC_INSN_L_BF, "l-bf", "l.bf", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.brk ${uimm-16} */
  {
    OPENRISC_INSN_L_BRK, "l-brk", "l.brk", 32,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.rfe $rA */
  {
    OPENRISC_INSN_L_RFE, "l-rfe", "l.rfe", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sys ${uimm-16} */
  {
    OPENRISC_INSN_L_SYS, "l-sys", "l.sys", 32,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.nop */
  {
    OPENRISC_INSN_L_NOP, "l-nop", "l.nop", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.movhi $rD,$hi16 */
  {
    OPENRISC_INSN_L_MOVHI, "l-movhi", "l.movhi", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.mfsr $rD,$rA */
  {
    OPENRISC_INSN_L_MFSR, "l-mfsr", "l.mfsr", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.mtsr $rA,$rB */
  {
    OPENRISC_INSN_L_MTSR, "l-mtsr", "l.mtsr", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.lw $rD,${simm-16}($rA) */
  {
    OPENRISC_INSN_L_LW, "l-lw", "l.lw", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.lbz $rD,${simm-16}($rA) */
  {
    OPENRISC_INSN_L_LBZ, "l-lbz", "l.lbz", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.lbs $rD,${simm-16}($rA) */
  {
    OPENRISC_INSN_L_LBS, "l-lbs", "l.lbs", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.lhz $rD,${simm-16}($rA) */
  {
    OPENRISC_INSN_L_LHZ, "l-lhz", "l.lhz", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.lhs $rD,${simm-16}($rA) */
  {
    OPENRISC_INSN_L_LHS, "l-lhs", "l.lhs", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sw ${ui16nc}($rA),$rB */
  {
    OPENRISC_INSN_L_SW, "l-sw", "l.sw", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sb ${ui16nc}($rA),$rB */
  {
    OPENRISC_INSN_L_SB, "l-sb", "l.sb", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sh ${ui16nc}($rA),$rB */
  {
    OPENRISC_INSN_L_SH, "l-sh", "l.sh", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sll $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_SLL, "l-sll", "l.sll", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.slli $rD,$rA,${uimm-5} */
  {
    OPENRISC_INSN_L_SLLI, "l-slli", "l.slli", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.srl $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_SRL, "l-srl", "l.srl", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.srli $rD,$rA,${uimm-5} */
  {
    OPENRISC_INSN_L_SRLI, "l-srli", "l.srli", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.sra $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_SRA, "l-sra", "l.sra", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.srai $rD,$rA,${uimm-5} */
  {
    OPENRISC_INSN_L_SRAI, "l-srai", "l.srai", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.ror $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_ROR, "l-ror", "l.ror", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.rori $rD,$rA,${uimm-5} */
  {
    OPENRISC_INSN_L_RORI, "l-rori", "l.rori", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.add $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_ADD, "l-add", "l.add", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.addi $rD,$rA,$lo16 */
  {
    OPENRISC_INSN_L_ADDI, "l-addi", "l.addi", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.sub $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_SUB, "l-sub", "l.sub", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.subi $rD,$rA,$lo16 */
  {
    OPENRISC_INSN_L_SUBI, "l-subi", "l.subi", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.and $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_AND, "l-and", "l.and", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.andi $rD,$rA,$lo16 */
  {
    OPENRISC_INSN_L_ANDI, "l-andi", "l.andi", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.or $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_OR, "l-or", "l.or", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.ori $rD,$rA,$lo16 */
  {
    OPENRISC_INSN_L_ORI, "l-ori", "l.ori", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.xor $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_XOR, "l-xor", "l.xor", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.xori $rD,$rA,$lo16 */
  {
    OPENRISC_INSN_L_XORI, "l-xori", "l.xori", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.mul $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_MUL, "l-mul", "l.mul", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.muli $rD,$rA,$lo16 */
  {
    OPENRISC_INSN_L_MULI, "l-muli", "l.muli", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* l.div $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_DIV, "l-div", "l.div", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.divu $rD,$rA,$rB */
  {
    OPENRISC_INSN_L_DIVU, "l-divu", "l.divu", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgts $rA,$rB */
  {
    OPENRISC_INSN_L_SFGTS, "l-sfgts", "l.sfgts", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgtu $rA,$rB */
  {
    OPENRISC_INSN_L_SFGTU, "l-sfgtu", "l.sfgtu", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfges $rA,$rB */
  {
    OPENRISC_INSN_L_SFGES, "l-sfges", "l.sfges", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgeu $rA,$rB */
  {
    OPENRISC_INSN_L_SFGEU, "l-sfgeu", "l.sfgeu", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sflts $rA,$rB */
  {
    OPENRISC_INSN_L_SFLTS, "l-sflts", "l.sflts", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfltu $rA,$rB */
  {
    OPENRISC_INSN_L_SFLTU, "l-sfltu", "l.sfltu", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfles $rA,$rB */
  {
    OPENRISC_INSN_L_SFLES, "l-sfles", "l.sfles", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfleu $rA,$rB */
  {
    OPENRISC_INSN_L_SFLEU, "l-sfleu", "l.sfleu", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgtsi $rA,${simm-16} */
  {
    OPENRISC_INSN_L_SFGTSI, "l-sfgtsi", "l.sfgtsi", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgtui $rA,${uimm-16} */
  {
    OPENRISC_INSN_L_SFGTUI, "l-sfgtui", "l.sfgtui", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgesi $rA,${simm-16} */
  {
    OPENRISC_INSN_L_SFGESI, "l-sfgesi", "l.sfgesi", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfgeui $rA,${uimm-16} */
  {
    OPENRISC_INSN_L_SFGEUI, "l-sfgeui", "l.sfgeui", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfltsi $rA,${simm-16} */
  {
    OPENRISC_INSN_L_SFLTSI, "l-sfltsi", "l.sfltsi", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfltui $rA,${uimm-16} */
  {
    OPENRISC_INSN_L_SFLTUI, "l-sfltui", "l.sfltui", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sflesi $rA,${simm-16} */
  {
    OPENRISC_INSN_L_SFLESI, "l-sflesi", "l.sflesi", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfleui $rA,${uimm-16} */
  {
    OPENRISC_INSN_L_SFLEUI, "l-sfleui", "l.sfleui", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfeq $rA,$rB */
  {
    OPENRISC_INSN_L_SFEQ, "l-sfeq", "l.sfeq", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfeqi $rA,${simm-16} */
  {
    OPENRISC_INSN_L_SFEQI, "l-sfeqi", "l.sfeqi", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfne $rA,$rB */
  {
    OPENRISC_INSN_L_SFNE, "l-sfne", "l.sfne", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* l.sfnei $rA,${simm-16} */
  {
    OPENRISC_INSN_L_SFNEI, "l-sfnei", "l.sfnei", 32,
    { 0|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
};

#undef OP
#undef A

/* Initialize anything needed to be done once, before any cpu_open call.  */
static void init_tables PARAMS ((void));

static void
init_tables ()
{
}

static const CGEN_MACH * lookup_mach_via_bfd_name
  PARAMS ((const CGEN_MACH *, const char *));
static void build_hw_table  PARAMS ((CGEN_CPU_TABLE *));
static void build_ifield_table  PARAMS ((CGEN_CPU_TABLE *));
static void build_operand_table PARAMS ((CGEN_CPU_TABLE *));
static void build_insn_table    PARAMS ((CGEN_CPU_TABLE *));
static void openrisc_cgen_rebuild_tables PARAMS ((CGEN_CPU_TABLE *));

/* Subroutine of openrisc_cgen_cpu_open to look up a mach via its bfd name.  */

static const CGEN_MACH *
lookup_mach_via_bfd_name (table, name)
     const CGEN_MACH *table;
     const char *name;
{
  while (table->name)
    {
      if (strcmp (name, table->bfd_name) == 0)
	return table;
      ++table;
    }
  abort ();
}

/* Subroutine of openrisc_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & openrisc_cgen_hw_table[0];
  /* MAX_HW is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_HW_ENTRY **selected =
    (const CGEN_HW_ENTRY **) xmalloc (MAX_HW * sizeof (CGEN_HW_ENTRY *));

  cd->hw_table.init_entries = init;
  cd->hw_table.entry_size = sizeof (CGEN_HW_ENTRY);
  memset (selected, 0, MAX_HW * sizeof (CGEN_HW_ENTRY *));
  /* ??? For now we just use machs to determine which ones we want.  */
  for (i = 0; init[i].name != NULL; ++i)
    if (CGEN_HW_ATTR_VALUE (&init[i], CGEN_HW_MACH)
	& machs)
      selected[init[i].type] = &init[i];
  cd->hw_table.entries = selected;
  cd->hw_table.num_entries = MAX_HW;
}

/* Subroutine of openrisc_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (cd)
     CGEN_CPU_TABLE *cd;
{
  cd->ifld_table = & openrisc_cgen_ifld_table[0];
}

/* Subroutine of openrisc_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & openrisc_cgen_operand_table[0];
  /* MAX_OPERANDS is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_OPERAND **selected =
    (const CGEN_OPERAND **) xmalloc (MAX_OPERANDS * sizeof (CGEN_OPERAND *));

  cd->operand_table.init_entries = init;
  cd->operand_table.entry_size = sizeof (CGEN_OPERAND);
  memset (selected, 0, MAX_OPERANDS * sizeof (CGEN_OPERAND *));
  /* ??? For now we just use mach to determine which ones we want.  */
  for (i = 0; init[i].name != NULL; ++i)
    if (CGEN_OPERAND_ATTR_VALUE (&init[i], CGEN_OPERAND_MACH)
	& machs)
      selected[init[i].type] = &init[i];
  cd->operand_table.entries = selected;
  cd->operand_table.num_entries = MAX_OPERANDS;
}

/* Subroutine of openrisc_cgen_cpu_open to build the hardware table.
   ??? This could leave out insns not supported by the specified mach/isa,
   but that would cause errors like "foo only supported by bar" to become
   "unknown insn", so for now we include all insns and require the app to
   do the checking later.
   ??? On the other hand, parsing of such insns may require their hardware or
   operand elements to be in the table [which they mightn't be].  */

static void
build_insn_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  const CGEN_IBASE *ib = & openrisc_cgen_insn_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of openrisc_cgen_cpu_open to rebuild the tables.  */

static void
openrisc_cgen_rebuild_tables (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  unsigned int isas = cd->isas;
  unsigned int machs = cd->machs;

  cd->int_insn_p = CGEN_INT_INSN_P;

  /* Data derived from the isa spec.  */
#define UNSET (CGEN_SIZE_UNKNOWN + 1)
  cd->default_insn_bitsize = UNSET;
  cd->base_insn_bitsize = UNSET;
  cd->min_insn_bitsize = 65535; /* some ridiculously big number */
  cd->max_insn_bitsize = 0;
  for (i = 0; i < MAX_ISAS; ++i)
    if (((1 << i) & isas) != 0)
      {
	const CGEN_ISA *isa = & openrisc_cgen_isa_table[i];

	/* Default insn sizes of all selected isas must be
	   equal or we set the result to 0, meaning "unknown".  */
	if (cd->default_insn_bitsize == UNSET)
	  cd->default_insn_bitsize = isa->default_insn_bitsize;
	else if (isa->default_insn_bitsize == cd->default_insn_bitsize)
	  ; /* this is ok */
	else
	  cd->default_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Base insn sizes of all selected isas must be equal
	   or we set the result to 0, meaning "unknown".  */
	if (cd->base_insn_bitsize == UNSET)
	  cd->base_insn_bitsize = isa->base_insn_bitsize;
	else if (isa->base_insn_bitsize == cd->base_insn_bitsize)
	  ; /* this is ok */
	else
	  cd->base_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Set min,max insn sizes.  */
	if (isa->min_insn_bitsize < cd->min_insn_bitsize)
	  cd->min_insn_bitsize = isa->min_insn_bitsize;
	if (isa->max_insn_bitsize > cd->max_insn_bitsize)
	  cd->max_insn_bitsize = isa->max_insn_bitsize;
      }

  /* Data derived from the mach spec.  */
  for (i = 0; i < MAX_MACHS; ++i)
    if (((1 << i) & machs) != 0)
      {
	const CGEN_MACH *mach = & openrisc_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "openrisc_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
		       cd->insn_chunk_bitsize, mach->insn_chunk_bitsize);
	      abort ();
	    }

 	  cd->insn_chunk_bitsize = mach->insn_chunk_bitsize;
	}
      }

  /* Determine which hw elements are used by MACH.  */
  build_hw_table (cd);

  /* Build the ifield table.  */
  build_ifield_table (cd);

  /* Determine which operands are used by MACH/ISA.  */
  build_operand_table (cd);

  /* Build the instruction table.  */
  build_insn_table (cd);
}

/* Initialize a cpu table and return a descriptor.
   It's much like opening a file, and must be the first function called.
   The arguments are a set of (type/value) pairs, terminated with
   CGEN_CPU_OPEN_END.

   Currently supported values:
   CGEN_CPU_OPEN_ISAS:    bitmap of values in enum isa_attr
   CGEN_CPU_OPEN_MACHS:   bitmap of values in enum mach_attr
   CGEN_CPU_OPEN_BFDMACH: specify 1 mach using bfd name
   CGEN_CPU_OPEN_ENDIAN:  specify endian choice
   CGEN_CPU_OPEN_END:     terminates arguments

   ??? Simultaneous multiple isas might not make sense, but it's not (yet)
   precluded.

   ??? We only support ISO C stdargs here, not K&R.
   Laziness, plus experiment to see if anything requires K&R - eventually
   K&R will no longer be supported - e.g. GDB is currently trying this.  */

CGEN_CPU_DESC
openrisc_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
{
  CGEN_CPU_TABLE *cd = (CGEN_CPU_TABLE *) xmalloc (sizeof (CGEN_CPU_TABLE));
  static int init_p;
  unsigned int isas = 0;  /* 0 = "unspecified" */
  unsigned int machs = 0; /* 0 = "unspecified" */
  enum cgen_endian endian = CGEN_ENDIAN_UNKNOWN;
  va_list ap;

  if (! init_p)
    {
      init_tables ();
      init_p = 1;
    }

  memset (cd, 0, sizeof (*cd));

  va_start (ap, arg_type);
  while (arg_type != CGEN_CPU_OPEN_END)
    {
      switch (arg_type)
	{
	case CGEN_CPU_OPEN_ISAS :
	  isas = va_arg (ap, unsigned int);
	  break;
	case CGEN_CPU_OPEN_MACHS :
	  machs = va_arg (ap, unsigned int);
	  break;
	case CGEN_CPU_OPEN_BFDMACH :
	  {
	    const char *name = va_arg (ap, const char *);
	    const CGEN_MACH *mach =
	      lookup_mach_via_bfd_name (openrisc_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "openrisc_cgen_cpu_open: unsupported argument `%d'\n",
		   arg_type);
	  abort (); /* ??? return NULL? */
	}
      arg_type = va_arg (ap, enum cgen_cpu_open_arg);
    }
  va_end (ap);

  /* mach unspecified means "all" */
  if (machs == 0)
    machs = (1 << MAX_MACHS) - 1;
  /* base mach is always selected */
  machs |= 1;
  /* isa unspecified means "all" */
  if (isas == 0)
    isas = (1 << MAX_ISAS) - 1;
  if (endian == CGEN_ENDIAN_UNKNOWN)
    {
      /* ??? If target has only one, could have a default.  */
      fprintf (stderr, "openrisc_cgen_cpu_open: no endianness specified\n");
      abort ();
    }

  cd->isas = isas;
  cd->machs = machs;
  cd->endian = endian;
  /* FIXME: for the sparc case we can determine insn-endianness statically.
     The worry here is where both data and insn endian can be independently
     chosen, in which case this function will need another argument.
     Actually, will want to allow for more arguments in the future anyway.  */
  cd->insn_endian = endian;

  /* Table (re)builder.  */
  cd->rebuild_tables = openrisc_cgen_rebuild_tables;
  openrisc_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to openrisc_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
openrisc_cgen_cpu_open_1 (mach_name, endian)
     const char *mach_name;
     enum cgen_endian endian;
{
  return openrisc_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
openrisc_cgen_cpu_close (cd)
     CGEN_CPU_DESC cd;
{
  unsigned int i;
  const CGEN_INSN *insns;

  if (cd->macro_insn_table.init_entries)
    {
      insns = cd->macro_insn_table.init_entries;
      for (i = 0; i < cd->macro_insn_table.num_init_entries; ++i, ++insns)
	{
	  if (CGEN_INSN_RX ((insns)))
	    regfree (CGEN_INSN_RX (insns));
	}
    }

  if (cd->insn_table.init_entries)
    {
      insns = cd->insn_table.init_entries;
      for (i = 0; i < cd->insn_table.num_init_entries; ++i, ++insns)
	{
	  if (CGEN_INSN_RX (insns))
	    regfree (CGEN_INSN_RX (insns));
	}
    }

  

  if (cd->macro_insn_table.init_entries)
    free ((CGEN_INSN *) cd->macro_insn_table.init_entries);

  if (cd->insn_table.init_entries)
    free ((CGEN_INSN *) cd->insn_table.init_entries);

  if (cd->hw_table.entries)
    free ((CGEN_HW_ENTRY *) cd->hw_table.entries);

  if (cd->operand_table.entries)
    free ((CGEN_HW_ENTRY *) cd->operand_table.entries);

  free (cd);
}

