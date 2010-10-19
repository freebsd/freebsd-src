/* CPU data for xstormy16.

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

#include "sysdep.h"
#include <stdio.h>
#include <stdarg.h>
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "xstormy16-desc.h"
#include "xstormy16-opc.h"
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

static const CGEN_ATTR_ENTRY MACH_attr[] ATTRIBUTE_UNUSED =
{
  { "base", MACH_BASE },
  { "xstormy16", MACH_XSTORMY16 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] ATTRIBUTE_UNUSED =
{
  { "xstormy16", ISA_XSTORMY16 },
  { "max", ISA_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE xstormy16_cgen_ifield_attr_table[] =
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

const CGEN_ATTR_TABLE xstormy16_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE xstormy16_cgen_operand_attr_table[] =
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

const CGEN_ATTR_TABLE xstormy16_cgen_insn_attr_table[] =
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
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA xstormy16_cgen_isa_table[] = {
  { "xstormy16", 32, 32, 16, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH xstormy16_cgen_mach_table[] = {
  { "xstormy16", "xstormy16", MACH_XSTORMY16, 16 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY xstormy16_cgen_opval_gr_names_entries[] =
{
  { "r0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "r1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "r2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "r3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "r4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "r5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "r6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "r7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "r8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 15, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xstormy16_cgen_opval_gr_names =
{
  & xstormy16_cgen_opval_gr_names_entries[0],
  18,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xstormy16_cgen_opval_gr_Rb_names_entries[] =
{
  { "r8", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 7, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xstormy16_cgen_opval_gr_Rb_names =
{
  & xstormy16_cgen_opval_gr_Rb_names_entries[0],
  10,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xstormy16_cgen_opval_h_branchcond_entries[] =
{
  { "ge", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "nc", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "lt", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "c", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "gt", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "hi", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "le", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "ls", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "pl", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "nv", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "mi", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "v", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "nz.b", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "nz", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "z.b", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "z", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xstormy16_cgen_opval_h_branchcond =
{
  & xstormy16_cgen_opval_h_branchcond_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xstormy16_cgen_opval_h_wordsize_entries[] =
{
  { ".b", 0, {0, {{{0, 0}}}}, 0, 0 },
  { ".w", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "", 1, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xstormy16_cgen_opval_h_wordsize =
{
  & xstormy16_cgen_opval_h_wordsize_entries[0],
  3,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY xstormy16_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PC), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & xstormy16_cgen_opval_gr_names, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-Rb", HW_H_RB, CGEN_ASM_KEYWORD, (PTR) & xstormy16_cgen_opval_gr_Rb_names, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-Rbj", HW_H_RBJ, CGEN_ASM_KEYWORD, (PTR) & xstormy16_cgen_opval_gr_Rb_names, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-Rpsw", HW_H_RPSW, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-z8", HW_H_Z8, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-z16", HW_H_Z16, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-cy", HW_H_CY, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-hc", HW_H_HC, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-ov", HW_H_OV, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-pt", HW_H_PT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-s", HW_H_S, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-branchcond", HW_H_BRANCHCOND, CGEN_ASM_KEYWORD, (PTR) & xstormy16_cgen_opval_h_branchcond, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-wordsize", HW_H_WORDSIZE, CGEN_ASM_KEYWORD, (PTR) & xstormy16_cgen_opval_h_wordsize, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { 0, 0, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD xstormy16_cgen_ifld_table[] =
{
  { XSTORMY16_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_RD, "f-Rd", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_RDM, "f-Rdm", 0, 32, 13, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_RM, "f-Rm", 0, 32, 4, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_RS, "f-Rs", 0, 32, 8, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_RB, "f-Rb", 0, 32, 17, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_RBJ, "f-Rbj", 0, 32, 11, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP1, "f-op1", 0, 32, 0, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP2, "f-op2", 0, 32, 4, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP2A, "f-op2a", 0, 32, 4, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP2M, "f-op2m", 0, 32, 7, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP3, "f-op3", 0, 32, 8, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP3A, "f-op3a", 0, 32, 8, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP3B, "f-op3b", 0, 32, 8, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP4, "f-op4", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP4M, "f-op4m", 0, 32, 12, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP4B, "f-op4b", 0, 32, 15, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP5, "f-op5", 0, 32, 16, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP5A, "f-op5a", 0, 32, 16, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_OP, "f-op", 0, 32, 0, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM2, "f-imm2", 0, 32, 10, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM3, "f-imm3", 0, 32, 4, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM3B, "f-imm3b", 0, 32, 17, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM4, "f-imm4", 0, 32, 8, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM8, "f-imm8", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM12, "f-imm12", 0, 32, 20, 12, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_IMM16, "f-imm16", 0, 32, 16, 16, { 0|A(SIGN_OPT), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_LMEM8, "f-lmem8", 0, 32, 8, 8, { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_HMEM8, "f-hmem8", 0, 32, 8, 8, { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_REL8_2, "f-rel8-2", 0, 32, 8, 8, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_REL8_4, "f-rel8-4", 0, 32, 8, 8, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_REL12, "f-rel12", 0, 32, 20, 12, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_REL12A, "f-rel12a", 0, 32, 4, 11, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_ABS24_1, "f-abs24-1", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_ABS24_2, "f-abs24-2", 0, 32, 16, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XSTORMY16_F_ABS24, "f-abs24", 0, 0, 0, 0,{ 0|A(ABS_ADDR)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } }  },
  { 0, 0, 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A



/* multi ifield declarations */

const CGEN_MAYBE_MULTI_IFLD XSTORMY16_F_ABS24_MULTI_IFIELD [];


/* multi ifield definitions */

const CGEN_MAYBE_MULTI_IFLD XSTORMY16_F_ABS24_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_ABS24_1] } },
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_ABS24_2] } },
    { 0, { (const PTR) 0 } }
};

/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) XSTORMY16_OPERAND_##op
#else
#define OPERAND(op) XSTORMY16_OPERAND_/**/op
#endif

const CGEN_OPERAND xstormy16_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", XSTORMY16_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_NIL] } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-z8:  */
  { "psw-z8", XSTORMY16_OPERAND_PSW_Z8, HW_H_Z8, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-z16:  */
  { "psw-z16", XSTORMY16_OPERAND_PSW_Z16, HW_H_Z16, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-cy:  */
  { "psw-cy", XSTORMY16_OPERAND_PSW_CY, HW_H_CY, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-hc:  */
  { "psw-hc", XSTORMY16_OPERAND_PSW_HC, HW_H_HC, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-ov:  */
  { "psw-ov", XSTORMY16_OPERAND_PSW_OV, HW_H_OV, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-pt:  */
  { "psw-pt", XSTORMY16_OPERAND_PSW_PT, HW_H_PT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw-s:  */
  { "psw-s", XSTORMY16_OPERAND_PSW_S, HW_H_S, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* Rd: general register destination */
  { "Rd", XSTORMY16_OPERAND_RD, HW_H_GR, 12, 4,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_RD] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* Rdm: general register destination */
  { "Rdm", XSTORMY16_OPERAND_RDM, HW_H_GR, 13, 3,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_RDM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* Rm: general register for memory */
  { "Rm", XSTORMY16_OPERAND_RM, HW_H_GR, 4, 3,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_RM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* Rs: general register source */
  { "Rs", XSTORMY16_OPERAND_RS, HW_H_GR, 8, 4,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_RS] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* Rb: base register */
  { "Rb", XSTORMY16_OPERAND_RB, HW_H_RB, 17, 3,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_RB] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* Rbj: base register for jump */
  { "Rbj", XSTORMY16_OPERAND_RBJ, HW_H_RBJ, 11, 1,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_RBJ] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bcond2: branch condition opcode */
  { "bcond2", XSTORMY16_OPERAND_BCOND2, HW_H_BRANCHCOND, 4, 4,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_OP2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* ws2: word size opcode */
  { "ws2", XSTORMY16_OPERAND_WS2, HW_H_WORDSIZE, 7, 1,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_OP2M] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bcond5: branch condition opcode */
  { "bcond5", XSTORMY16_OPERAND_BCOND5, HW_H_BRANCHCOND, 16, 4,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_OP5] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm2: 2 bit unsigned immediate */
  { "imm2", XSTORMY16_OPERAND_IMM2, HW_H_UINT, 10, 2,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm3: 3 bit unsigned immediate */
  { "imm3", XSTORMY16_OPERAND_IMM3, HW_H_UINT, 4, 3,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM3] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm3b: 3 bit unsigned immediate for bit tests */
  { "imm3b", XSTORMY16_OPERAND_IMM3B, HW_H_UINT, 17, 3,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM3B] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm4: 4 bit unsigned immediate */
  { "imm4", XSTORMY16_OPERAND_IMM4, HW_H_UINT, 8, 4,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm8: 8 bit unsigned immediate */
  { "imm8", XSTORMY16_OPERAND_IMM8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm8small: 8 bit unsigned immediate */
  { "imm8small", XSTORMY16_OPERAND_IMM8SMALL, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm12: 12 bit signed immediate */
  { "imm12", XSTORMY16_OPERAND_IMM12, HW_H_SINT, 20, 12,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM12] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* imm16: 16 bit immediate */
  { "imm16", XSTORMY16_OPERAND_IMM16, HW_H_UINT, 16, 16,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_IMM16] } }, 
    { 0|A(SIGN_OPT), { { { (1<<MACH_BASE), 0 } } } }  },
/* lmem8: 8 bit unsigned immediate low memory */
  { "lmem8", XSTORMY16_OPERAND_LMEM8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_LMEM8] } }, 
    { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* hmem8: 8 bit unsigned immediate high memory */
  { "hmem8", XSTORMY16_OPERAND_HMEM8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_HMEM8] } }, 
    { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* rel8-2: 8 bit relative address */
  { "rel8-2", XSTORMY16_OPERAND_REL8_2, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_REL8_2] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* rel8-4: 8 bit relative address */
  { "rel8-4", XSTORMY16_OPERAND_REL8_4, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_REL8_4] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* rel12: 12 bit relative address */
  { "rel12", XSTORMY16_OPERAND_REL12, HW_H_UINT, 20, 12,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_REL12] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* rel12a: 12 bit relative address */
  { "rel12a", XSTORMY16_OPERAND_REL12A, HW_H_UINT, 4, 11,
    { 0, { (const PTR) &xstormy16_cgen_ifld_table[XSTORMY16_F_REL12A] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* abs24: 24 bit absolute address */
  { "abs24", XSTORMY16_OPERAND_ABS24, HW_H_UINT, 8, 24,
    { 2, { (const PTR) &XSTORMY16_F_ABS24_MULTI_IFIELD[0] } }, 
    { 0|A(ABS_ADDR)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } }  },
/* psw: program status word */
  { "psw", XSTORMY16_OPERAND_PSW, HW_H_GR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* Rpsw: N0-N3 of the program status word */
  { "Rpsw", XSTORMY16_OPERAND_RPSW, HW_H_RPSW, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* sp: stack pointer */
  { "sp", XSTORMY16_OPERAND_SP, HW_H_GR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* R0: R0 */
  { "R0", XSTORMY16_OPERAND_R0, HW_H_GR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* R1: R1 */
  { "R1", XSTORMY16_OPERAND_R1, HW_H_GR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* R2: R2 */
  { "R2", XSTORMY16_OPERAND_R2, HW_H_GR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* R8: R8 */
  { "R8", XSTORMY16_OPERAND_R8, HW_H_GR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* sentinel */
  { 0, 0, 0, 0, 0,
    { 0, { (const PTR) 0 } },
    { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A


/* The instruction table.  */

#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif

static const CGEN_IBASE xstormy16_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
/* mov$ws2 $lmem8,#$imm16 */
  {
    XSTORMY16_INSN_MOVLMEMIMM, "movlmemimm", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $hmem8,#$imm16 */
  {
    XSTORMY16_INSN_MOVHMEMIMM, "movhmemimm", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rm,$lmem8 */
  {
    XSTORMY16_INSN_MOVLGRMEM, "movlgrmem", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rm,$hmem8 */
  {
    XSTORMY16_INSN_MOVHGRMEM, "movhgrmem", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $lmem8,$Rm */
  {
    XSTORMY16_INSN_MOVLMEMGR, "movlmemgr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $hmem8,$Rm */
  {
    XSTORMY16_INSN_MOVHMEMGR, "movhmemgr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rdm,($Rs) */
  {
    XSTORMY16_INSN_MOVGRGRI, "movgrgri", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rdm,($Rs++) */
  {
    XSTORMY16_INSN_MOVGRGRIPOSTINC, "movgrgripostinc", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rdm,(--$Rs) */
  {
    XSTORMY16_INSN_MOVGRGRIPREDEC, "movgrgripredec", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 ($Rs),$Rdm */
  {
    XSTORMY16_INSN_MOVGRIGR, "movgrigr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 ($Rs++),$Rdm */
  {
    XSTORMY16_INSN_MOVGRIPOSTINCGR, "movgripostincgr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 (--$Rs),$Rdm */
  {
    XSTORMY16_INSN_MOVGRIPREDECGR, "movgripredecgr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rdm,($Rs,$imm12) */
  {
    XSTORMY16_INSN_MOVGRGRII, "movgrgrii", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rdm,($Rs++,$imm12) */
  {
    XSTORMY16_INSN_MOVGRGRIIPOSTINC, "movgrgriipostinc", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 $Rdm,(--$Rs,$imm12) */
  {
    XSTORMY16_INSN_MOVGRGRIIPREDEC, "movgrgriipredec", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 ($Rs,$imm12),$Rdm */
  {
    XSTORMY16_INSN_MOVGRIIGR, "movgriigr", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 ($Rs++,$imm12),$Rdm */
  {
    XSTORMY16_INSN_MOVGRIIPOSTINCGR, "movgriipostincgr", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov$ws2 (--$Rs,$imm12),$Rdm */
  {
    XSTORMY16_INSN_MOVGRIIPREDECGR, "movgriipredecgr", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov $Rd,$Rs */
  {
    XSTORMY16_INSN_MOVGRGR, "movgrgr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov.w Rx,#$imm8 */
  {
    XSTORMY16_INSN_MOVWIMM8, "movwimm8", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov.w $Rm,#$imm8small */
  {
    XSTORMY16_INSN_MOVWGRIMM8, "movwgrimm8", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov.w $Rd,#$imm16 */
  {
    XSTORMY16_INSN_MOVWGRIMM16, "movwgrimm16", "mov.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov.b $Rd,RxL */
  {
    XSTORMY16_INSN_MOVLOWGR, "movlowgr", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov.b $Rd,RxH */
  {
    XSTORMY16_INSN_MOVHIGHGR, "movhighgr", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 $Rdm,($Rs) */
  {
    XSTORMY16_INSN_MOVFGRGRI, "movfgrgri", "movf", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 $Rdm,($Rs++) */
  {
    XSTORMY16_INSN_MOVFGRGRIPOSTINC, "movfgrgripostinc", "movf", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 $Rdm,(--$Rs) */
  {
    XSTORMY16_INSN_MOVFGRGRIPREDEC, "movfgrgripredec", "movf", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 ($Rs),$Rdm */
  {
    XSTORMY16_INSN_MOVFGRIGR, "movfgrigr", "movf", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 ($Rs++),$Rdm */
  {
    XSTORMY16_INSN_MOVFGRIPOSTINCGR, "movfgripostincgr", "movf", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 (--$Rs),$Rdm */
  {
    XSTORMY16_INSN_MOVFGRIPREDECGR, "movfgripredecgr", "movf", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 $Rdm,($Rb,$Rs,$imm12) */
  {
    XSTORMY16_INSN_MOVFGRGRII, "movfgrgrii", "movf", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 $Rdm,($Rb,$Rs++,$imm12) */
  {
    XSTORMY16_INSN_MOVFGRGRIIPOSTINC, "movfgrgriipostinc", "movf", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 $Rdm,($Rb,--$Rs,$imm12) */
  {
    XSTORMY16_INSN_MOVFGRGRIIPREDEC, "movfgrgriipredec", "movf", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 ($Rb,$Rs,$imm12),$Rdm */
  {
    XSTORMY16_INSN_MOVFGRIIGR, "movfgriigr", "movf", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 ($Rb,$Rs++,$imm12),$Rdm */
  {
    XSTORMY16_INSN_MOVFGRIIPOSTINCGR, "movfgriipostincgr", "movf", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* movf$ws2 ($Rb,--$Rs,$imm12),$Rdm */
  {
    XSTORMY16_INSN_MOVFGRIIPREDECGR, "movfgriipredecgr", "movf", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mask $Rd,$Rs */
  {
    XSTORMY16_INSN_MASKGRGR, "maskgrgr", "mask", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mask $Rd,#$imm16 */
  {
    XSTORMY16_INSN_MASKGRIMM16, "maskgrimm16", "mask", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* push $Rd */
  {
    XSTORMY16_INSN_PUSHGR, "pushgr", "push", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* pop $Rd */
  {
    XSTORMY16_INSN_POPGR, "popgr", "pop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* swpn $Rd */
  {
    XSTORMY16_INSN_SWPN, "swpn", "swpn", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* swpb $Rd */
  {
    XSTORMY16_INSN_SWPB, "swpb", "swpb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* swpw $Rd,$Rs */
  {
    XSTORMY16_INSN_SWPW, "swpw", "swpw", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* and $Rd,$Rs */
  {
    XSTORMY16_INSN_ANDGRGR, "andgrgr", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* and Rx,#$imm8 */
  {
    XSTORMY16_INSN_ANDIMM8, "andimm8", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* and $Rd,#$imm16 */
  {
    XSTORMY16_INSN_ANDGRIMM16, "andgrimm16", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* or $Rd,$Rs */
  {
    XSTORMY16_INSN_ORGRGR, "orgrgr", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* or Rx,#$imm8 */
  {
    XSTORMY16_INSN_ORIMM8, "orimm8", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* or $Rd,#$imm16 */
  {
    XSTORMY16_INSN_ORGRIMM16, "orgrimm16", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor $Rd,$Rs */
  {
    XSTORMY16_INSN_XORGRGR, "xorgrgr", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor Rx,#$imm8 */
  {
    XSTORMY16_INSN_XORIMM8, "xorimm8", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor $Rd,#$imm16 */
  {
    XSTORMY16_INSN_XORGRIMM16, "xorgrimm16", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* not $Rd */
  {
    XSTORMY16_INSN_NOTGR, "notgr", "not", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add $Rd,$Rs */
  {
    XSTORMY16_INSN_ADDGRGR, "addgrgr", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add $Rd,#$imm4 */
  {
    XSTORMY16_INSN_ADDGRIMM4, "addgrimm4", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add Rx,#$imm8 */
  {
    XSTORMY16_INSN_ADDIMM8, "addimm8", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add $Rd,#$imm16 */
  {
    XSTORMY16_INSN_ADDGRIMM16, "addgrimm16", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* adc $Rd,$Rs */
  {
    XSTORMY16_INSN_ADCGRGR, "adcgrgr", "adc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* adc $Rd,#$imm4 */
  {
    XSTORMY16_INSN_ADCGRIMM4, "adcgrimm4", "adc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* adc Rx,#$imm8 */
  {
    XSTORMY16_INSN_ADCIMM8, "adcimm8", "adc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* adc $Rd,#$imm16 */
  {
    XSTORMY16_INSN_ADCGRIMM16, "adcgrimm16", "adc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub $Rd,$Rs */
  {
    XSTORMY16_INSN_SUBGRGR, "subgrgr", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub $Rd,#$imm4 */
  {
    XSTORMY16_INSN_SUBGRIMM4, "subgrimm4", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub Rx,#$imm8 */
  {
    XSTORMY16_INSN_SUBIMM8, "subimm8", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub $Rd,#$imm16 */
  {
    XSTORMY16_INSN_SUBGRIMM16, "subgrimm16", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sbc $Rd,$Rs */
  {
    XSTORMY16_INSN_SBCGRGR, "sbcgrgr", "sbc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sbc $Rd,#$imm4 */
  {
    XSTORMY16_INSN_SBCGRIMM4, "sbcgrimm4", "sbc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sbc Rx,#$imm8 */
  {
    XSTORMY16_INSN_SBCGRIMM8, "sbcgrimm8", "sbc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sbc $Rd,#$imm16 */
  {
    XSTORMY16_INSN_SBCGRIMM16, "sbcgrimm16", "sbc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* inc $Rd,#$imm2 */
  {
    XSTORMY16_INSN_INCGRIMM2, "incgrimm2", "inc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* dec $Rd,#$imm2 */
  {
    XSTORMY16_INSN_DECGRIMM2, "decgrimm2", "dec", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rrc $Rd,$Rs */
  {
    XSTORMY16_INSN_RRCGRGR, "rrcgrgr", "rrc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rrc $Rd,#$imm4 */
  {
    XSTORMY16_INSN_RRCGRIMM4, "rrcgrimm4", "rrc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rlc $Rd,$Rs */
  {
    XSTORMY16_INSN_RLCGRGR, "rlcgrgr", "rlc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rlc $Rd,#$imm4 */
  {
    XSTORMY16_INSN_RLCGRIMM4, "rlcgrimm4", "rlc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* shr $Rd,$Rs */
  {
    XSTORMY16_INSN_SHRGRGR, "shrgrgr", "shr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* shr $Rd,#$imm4 */
  {
    XSTORMY16_INSN_SHRGRIMM, "shrgrimm", "shr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* shl $Rd,$Rs */
  {
    XSTORMY16_INSN_SHLGRGR, "shlgrgr", "shl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* shl $Rd,#$imm4 */
  {
    XSTORMY16_INSN_SHLGRIMM, "shlgrimm", "shl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* asr $Rd,$Rs */
  {
    XSTORMY16_INSN_ASRGRGR, "asrgrgr", "asr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* asr $Rd,#$imm4 */
  {
    XSTORMY16_INSN_ASRGRIMM, "asrgrimm", "asr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* set1 $Rd,#$imm4 */
  {
    XSTORMY16_INSN_SET1GRIMM, "set1grimm", "set1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* set1 $Rd,$Rs */
  {
    XSTORMY16_INSN_SET1GRGR, "set1grgr", "set1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* set1 $lmem8,#$imm3 */
  {
    XSTORMY16_INSN_SET1LMEMIMM, "set1lmemimm", "set1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* set1 $hmem8,#$imm3 */
  {
    XSTORMY16_INSN_SET1HMEMIMM, "set1hmemimm", "set1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* clr1 $Rd,#$imm4 */
  {
    XSTORMY16_INSN_CLR1GRIMM, "clr1grimm", "clr1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* clr1 $Rd,$Rs */
  {
    XSTORMY16_INSN_CLR1GRGR, "clr1grgr", "clr1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* clr1 $lmem8,#$imm3 */
  {
    XSTORMY16_INSN_CLR1LMEMIMM, "clr1lmemimm", "clr1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* clr1 $hmem8,#$imm3 */
  {
    XSTORMY16_INSN_CLR1HMEMIMM, "clr1hmemimm", "clr1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* cbw $Rd */
  {
    XSTORMY16_INSN_CBWGR, "cbwgr", "cbw", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rev $Rd */
  {
    XSTORMY16_INSN_REVGR, "revgr", "rev", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* b$bcond5 $Rd,$Rs,$rel12 */
  {
    XSTORMY16_INSN_BCCGRGR, "bccgrgr", "b", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* b$bcond5 $Rm,#$imm8,$rel12 */
  {
    XSTORMY16_INSN_BCCGRIMM8, "bccgrimm8", "b", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* b$bcond2 Rx,#$imm16,${rel8-4} */
  {
    XSTORMY16_INSN_BCCIMM16, "bccimm16", "b", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bn $Rd,#$imm4,$rel12 */
  {
    XSTORMY16_INSN_BNGRIMM4, "bngrimm4", "bn", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bn $Rd,$Rs,$rel12 */
  {
    XSTORMY16_INSN_BNGRGR, "bngrgr", "bn", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bn $lmem8,#$imm3b,$rel12 */
  {
    XSTORMY16_INSN_BNLMEMIMM, "bnlmemimm", "bn", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bn $hmem8,#$imm3b,$rel12 */
  {
    XSTORMY16_INSN_BNHMEMIMM, "bnhmemimm", "bn", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bp $Rd,#$imm4,$rel12 */
  {
    XSTORMY16_INSN_BPGRIMM4, "bpgrimm4", "bp", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bp $Rd,$Rs,$rel12 */
  {
    XSTORMY16_INSN_BPGRGR, "bpgrgr", "bp", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bp $lmem8,#$imm3b,$rel12 */
  {
    XSTORMY16_INSN_BPLMEMIMM, "bplmemimm", "bp", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* bp $hmem8,#$imm3b,$rel12 */
  {
    XSTORMY16_INSN_BPHMEMIMM, "bphmemimm", "bp", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* b$bcond2 ${rel8-2} */
  {
    XSTORMY16_INSN_BCC, "bcc", "b", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* br $Rd */
  {
    XSTORMY16_INSN_BGR, "bgr", "br", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* br $rel12a */
  {
    XSTORMY16_INSN_BR, "br", "br", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* jmp $Rbj,$Rd */
  {
    XSTORMY16_INSN_JMP, "jmp", "jmp", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* jmpf $abs24 */
  {
    XSTORMY16_INSN_JMPF, "jmpf", "jmpf", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* callr $Rd */
  {
    XSTORMY16_INSN_CALLRGR, "callrgr", "callr", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* callr $rel12a */
  {
    XSTORMY16_INSN_CALLRIMM, "callrimm", "callr", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* call $Rbj,$Rd */
  {
    XSTORMY16_INSN_CALLGR, "callgr", "call", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* callf $abs24 */
  {
    XSTORMY16_INSN_CALLFIMM, "callfimm", "callf", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* icallr $Rd */
  {
    XSTORMY16_INSN_ICALLRGR, "icallrgr", "icallr", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* icall $Rbj,$Rd */
  {
    XSTORMY16_INSN_ICALLGR, "icallgr", "icall", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* icallf $abs24 */
  {
    XSTORMY16_INSN_ICALLFIMM, "icallfimm", "icallf", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* iret */
  {
    XSTORMY16_INSN_IRET, "iret", "iret", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* ret */
  {
    XSTORMY16_INSN_RET, "ret", "ret", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* mul */
  {
    XSTORMY16_INSN_MUL, "mul", "mul", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* div */
  {
    XSTORMY16_INSN_DIV, "div", "div", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sdiv */
  {
    XSTORMY16_INSN_SDIV, "sdiv", "sdiv", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sdivlh */
  {
    XSTORMY16_INSN_SDIVLH, "sdivlh", "sdivlh", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* divlh */
  {
    XSTORMY16_INSN_DIVLH, "divlh", "divlh", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* reset */
  {
    XSTORMY16_INSN_RESET, "reset", "reset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* nop */
  {
    XSTORMY16_INSN_NOP, "nop", "nop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* halt */
  {
    XSTORMY16_INSN_HALT, "halt", "halt", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* hold */
  {
    XSTORMY16_INSN_HOLD, "hold", "hold", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* holdx */
  {
    XSTORMY16_INSN_HOLDX, "holdx", "holdx", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* brk */
  {
    XSTORMY16_INSN_BRK, "brk", "brk", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* --unused-- */
  {
    XSTORMY16_INSN_SYSCALL, "syscall", "--unused--", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
};

#undef OP
#undef A

/* Initialize anything needed to be done once, before any cpu_open call.  */

static void
init_tables (void)
{
}

static const CGEN_MACH * lookup_mach_via_bfd_name (const CGEN_MACH *, const char *);
static void build_hw_table      (CGEN_CPU_TABLE *);
static void build_ifield_table  (CGEN_CPU_TABLE *);
static void build_operand_table (CGEN_CPU_TABLE *);
static void build_insn_table    (CGEN_CPU_TABLE *);
static void xstormy16_cgen_rebuild_tables (CGEN_CPU_TABLE *);

/* Subroutine of xstormy16_cgen_cpu_open to look up a mach via its bfd name.  */

static const CGEN_MACH *
lookup_mach_via_bfd_name (const CGEN_MACH *table, const char *name)
{
  while (table->name)
    {
      if (strcmp (name, table->bfd_name) == 0)
	return table;
      ++table;
    }
  abort ();
}

/* Subroutine of xstormy16_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & xstormy16_cgen_hw_table[0];
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

/* Subroutine of xstormy16_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (CGEN_CPU_TABLE *cd)
{
  cd->ifld_table = & xstormy16_cgen_ifld_table[0];
}

/* Subroutine of xstormy16_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & xstormy16_cgen_operand_table[0];
  /* MAX_OPERANDS is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_OPERAND **selected = xmalloc (MAX_OPERANDS * sizeof (* selected));

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

/* Subroutine of xstormy16_cgen_cpu_open to build the hardware table.
   ??? This could leave out insns not supported by the specified mach/isa,
   but that would cause errors like "foo only supported by bar" to become
   "unknown insn", so for now we include all insns and require the app to
   do the checking later.
   ??? On the other hand, parsing of such insns may require their hardware or
   operand elements to be in the table [which they mightn't be].  */

static void
build_insn_table (CGEN_CPU_TABLE *cd)
{
  int i;
  const CGEN_IBASE *ib = & xstormy16_cgen_insn_table[0];
  CGEN_INSN *insns = xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of xstormy16_cgen_cpu_open to rebuild the tables.  */

static void
xstormy16_cgen_rebuild_tables (CGEN_CPU_TABLE *cd)
{
  int i;
  CGEN_BITSET *isas = cd->isas;
  unsigned int machs = cd->machs;

  cd->int_insn_p = CGEN_INT_INSN_P;

  /* Data derived from the isa spec.  */
#define UNSET (CGEN_SIZE_UNKNOWN + 1)
  cd->default_insn_bitsize = UNSET;
  cd->base_insn_bitsize = UNSET;
  cd->min_insn_bitsize = 65535; /* Some ridiculously big number.  */
  cd->max_insn_bitsize = 0;
  for (i = 0; i < MAX_ISAS; ++i)
    if (cgen_bitset_contains (isas, i))
      {
	const CGEN_ISA *isa = & xstormy16_cgen_isa_table[i];

	/* Default insn sizes of all selected isas must be
	   equal or we set the result to 0, meaning "unknown".  */
	if (cd->default_insn_bitsize == UNSET)
	  cd->default_insn_bitsize = isa->default_insn_bitsize;
	else if (isa->default_insn_bitsize == cd->default_insn_bitsize)
	  ; /* This is ok.  */
	else
	  cd->default_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Base insn sizes of all selected isas must be equal
	   or we set the result to 0, meaning "unknown".  */
	if (cd->base_insn_bitsize == UNSET)
	  cd->base_insn_bitsize = isa->base_insn_bitsize;
	else if (isa->base_insn_bitsize == cd->base_insn_bitsize)
	  ; /* This is ok.  */
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
	const CGEN_MACH *mach = & xstormy16_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "xstormy16_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
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
xstormy16_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
{
  CGEN_CPU_TABLE *cd = (CGEN_CPU_TABLE *) xmalloc (sizeof (CGEN_CPU_TABLE));
  static int init_p;
  CGEN_BITSET *isas = 0;  /* 0 = "unspecified" */
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
	  isas = va_arg (ap, CGEN_BITSET *);
	  break;
	case CGEN_CPU_OPEN_MACHS :
	  machs = va_arg (ap, unsigned int);
	  break;
	case CGEN_CPU_OPEN_BFDMACH :
	  {
	    const char *name = va_arg (ap, const char *);
	    const CGEN_MACH *mach =
	      lookup_mach_via_bfd_name (xstormy16_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "xstormy16_cgen_cpu_open: unsupported argument `%d'\n",
		   arg_type);
	  abort (); /* ??? return NULL? */
	}
      arg_type = va_arg (ap, enum cgen_cpu_open_arg);
    }
  va_end (ap);

  /* Mach unspecified means "all".  */
  if (machs == 0)
    machs = (1 << MAX_MACHS) - 1;
  /* Base mach is always selected.  */
  machs |= 1;
  if (endian == CGEN_ENDIAN_UNKNOWN)
    {
      /* ??? If target has only one, could have a default.  */
      fprintf (stderr, "xstormy16_cgen_cpu_open: no endianness specified\n");
      abort ();
    }

  cd->isas = cgen_bitset_copy (isas);
  cd->machs = machs;
  cd->endian = endian;
  /* FIXME: for the sparc case we can determine insn-endianness statically.
     The worry here is where both data and insn endian can be independently
     chosen, in which case this function will need another argument.
     Actually, will want to allow for more arguments in the future anyway.  */
  cd->insn_endian = endian;

  /* Table (re)builder.  */
  cd->rebuild_tables = xstormy16_cgen_rebuild_tables;
  xstormy16_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to xstormy16_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
xstormy16_cgen_cpu_open_1 (const char *mach_name, enum cgen_endian endian)
{
  return xstormy16_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
xstormy16_cgen_cpu_close (CGEN_CPU_DESC cd)
{
  unsigned int i;
  const CGEN_INSN *insns;

  if (cd->macro_insn_table.init_entries)
    {
      insns = cd->macro_insn_table.init_entries;
      for (i = 0; i < cd->macro_insn_table.num_init_entries; ++i, ++insns)
	if (CGEN_INSN_RX ((insns)))
	  regfree (CGEN_INSN_RX (insns));
    }

  if (cd->insn_table.init_entries)
    {
      insns = cd->insn_table.init_entries;
      for (i = 0; i < cd->insn_table.num_init_entries; ++i, ++insns)
	if (CGEN_INSN_RX (insns))
	  regfree (CGEN_INSN_RX (insns));
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

