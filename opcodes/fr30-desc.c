/* CPU data for fr30.

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
#include "fr30-desc.h"
#include "fr30-opc.h"
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
  { "fr30", MACH_FR30 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] =
{
  { "fr30", ISA_FR30 },
  { "max", ISA_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_ifield_attr_table[] =
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

const CGEN_ATTR_TABLE fr30_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_operand_attr_table[] =
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
  { "HASH-PREFIX", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE fr30_cgen_insn_attr_table[] =
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

static const CGEN_ISA fr30_cgen_isa_table[] = {
  { "fr30", 16, 16, 16, 48 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH fr30_cgen_mach_table[] = {
  { "fr30", "fr30", MACH_FR30, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_gr_names_entries[] =
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
  { "ac", 13, {0, {0}}, 0, 0 },
  { "fp", 14, {0, {0}}, 0, 0 },
  { "sp", 15, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_gr_names =
{
  & fr30_cgen_opval_gr_names_entries[0],
  19,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_cr_names_entries[] =
{
  { "cr0", 0, {0, {0}}, 0, 0 },
  { "cr1", 1, {0, {0}}, 0, 0 },
  { "cr2", 2, {0, {0}}, 0, 0 },
  { "cr3", 3, {0, {0}}, 0, 0 },
  { "cr4", 4, {0, {0}}, 0, 0 },
  { "cr5", 5, {0, {0}}, 0, 0 },
  { "cr6", 6, {0, {0}}, 0, 0 },
  { "cr7", 7, {0, {0}}, 0, 0 },
  { "cr8", 8, {0, {0}}, 0, 0 },
  { "cr9", 9, {0, {0}}, 0, 0 },
  { "cr10", 10, {0, {0}}, 0, 0 },
  { "cr11", 11, {0, {0}}, 0, 0 },
  { "cr12", 12, {0, {0}}, 0, 0 },
  { "cr13", 13, {0, {0}}, 0, 0 },
  { "cr14", 14, {0, {0}}, 0, 0 },
  { "cr15", 15, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_cr_names =
{
  & fr30_cgen_opval_cr_names_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_dr_names_entries[] =
{
  { "tbr", 0, {0, {0}}, 0, 0 },
  { "rp", 1, {0, {0}}, 0, 0 },
  { "ssp", 2, {0, {0}}, 0, 0 },
  { "usp", 3, {0, {0}}, 0, 0 },
  { "mdh", 4, {0, {0}}, 0, 0 },
  { "mdl", 5, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_dr_names =
{
  & fr30_cgen_opval_dr_names_entries[0],
  6,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_ps_entries[] =
{
  { "ps", 0, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_ps =
{
  & fr30_cgen_opval_h_ps_entries[0],
  1,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_r13_entries[] =
{
  { "r13", 0, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_r13 =
{
  & fr30_cgen_opval_h_r13_entries[0],
  1,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_r14_entries[] =
{
  { "r14", 0, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_r14 =
{
  & fr30_cgen_opval_h_r14_entries[0],
  1,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY fr30_cgen_opval_h_r15_entries[] =
{
  { "r15", 0, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD fr30_cgen_opval_h_r15 =
{
  & fr30_cgen_opval_h_r15_entries[0],
  1,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY fr30_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { (1<<MACH_BASE) } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_gr_names, { 0|A(CACHE_ADDR)|A(PROFILE), { (1<<MACH_BASE) } } },
  { "h-cr", HW_H_CR, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_cr_names, { 0, { (1<<MACH_BASE) } } },
  { "h-dr", HW_H_DR, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_dr_names, { 0, { (1<<MACH_BASE) } } },
  { "h-ps", HW_H_PS, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_ps, { 0, { (1<<MACH_BASE) } } },
  { "h-r13", HW_H_R13, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_r13, { 0, { (1<<MACH_BASE) } } },
  { "h-r14", HW_H_R14, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_r14, { 0, { (1<<MACH_BASE) } } },
  { "h-r15", HW_H_R15, CGEN_ASM_KEYWORD, (PTR) & fr30_cgen_opval_h_r15, { 0, { (1<<MACH_BASE) } } },
  { "h-nbit", HW_H_NBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-zbit", HW_H_ZBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-vbit", HW_H_VBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-cbit", HW_H_CBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-ibit", HW_H_IBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-sbit", HW_H_SBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-tbit", HW_H_TBIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-d0bit", HW_H_D0BIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-d1bit", HW_H_D1BIT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-ccr", HW_H_CCR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-scr", HW_H_SCR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-ilm", HW_H_ILM, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { 0, 0, CGEN_ASM_NONE, 0, {0, {0}} }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD fr30_cgen_ifld_table[] =
{
  { FR30_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_OP1, "f-op1", 0, 16, 0, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_OP2, "f-op2", 0, 16, 4, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_OP3, "f-op3", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_OP4, "f-op4", 0, 16, 12, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_OP5, "f-op5", 0, 16, 4, 1, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_CC, "f-cc", 0, 16, 4, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_CCC, "f-ccc", 16, 16, 0, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_RJ, "f-Rj", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_RI, "f-Ri", 0, 16, 12, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_RS1, "f-Rs1", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_RS2, "f-Rs2", 0, 16, 12, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_RJC, "f-Rjc", 16, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_RIC, "f-Ric", 16, 16, 12, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_CRJ, "f-CRj", 16, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_CRI, "f-CRi", 16, 16, 12, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_U4, "f-u4", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_U4C, "f-u4c", 0, 16, 12, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_I4, "f-i4", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_M4, "f-m4", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_U8, "f-u8", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_I8, "f-i8", 0, 16, 4, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_I20_4, "f-i20-4", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_I20_16, "f-i20-16", 16, 16, 0, 16, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_I20, "f-i20", 0, 0, 0, 0,{ 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
  { FR30_F_I32, "f-i32", 16, 32, 0, 32, { 0|A(SIGN_OPT), { (1<<MACH_BASE) } }  },
  { FR30_F_UDISP6, "f-udisp6", 0, 16, 8, 4, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DISP8, "f-disp8", 0, 16, 4, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DISP9, "f-disp9", 0, 16, 4, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DISP10, "f-disp10", 0, 16, 4, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_S10, "f-s10", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_U10, "f-u10", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_REL9, "f-rel9", 0, 16, 8, 8, { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { FR30_F_DIR8, "f-dir8", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DIR9, "f-dir9", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_DIR10, "f-dir10", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_REL12, "f-rel12", 0, 16, 5, 11, { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_HI_ST, "f-reglist_hi_st", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_LOW_ST, "f-reglist_low_st", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_HI_LD, "f-reglist_hi_ld", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { FR30_F_REGLIST_LOW_LD, "f-reglist_low_ld", 0, 16, 8, 8, { 0, { (1<<MACH_BASE) } }  },
  { 0, 0, 0, 0, 0, 0, {0, {0}} }
};

#undef A



/* multi ifield declarations */

const CGEN_MAYBE_MULTI_IFLD FR30_F_I20_MULTI_IFIELD [];


/* multi ifield definitions */

const CGEN_MAYBE_MULTI_IFLD FR30_F_I20_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_I20_4] } },
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_I20_16] } },
    { 0, { (const PTR) 0 } }
};

/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) FR30_OPERAND_##op
#else
#define OPERAND(op) FR30_OPERAND_/**/op
#endif

const CGEN_OPERAND fr30_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", FR30_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_NIL] } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* Ri: destination register */
  { "Ri", FR30_OPERAND_RI, HW_H_GR, 12, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_RI] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* Rj: source register */
  { "Rj", FR30_OPERAND_RJ, HW_H_GR, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_RJ] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* Ric: target register coproc insn */
  { "Ric", FR30_OPERAND_RIC, HW_H_GR, 12, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_RIC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* Rjc: source register coproc insn */
  { "Rjc", FR30_OPERAND_RJC, HW_H_GR, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_RJC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* CRi: coprocessor register */
  { "CRi", FR30_OPERAND_CRI, HW_H_CR, 12, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_CRI] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* CRj: coprocessor register */
  { "CRj", FR30_OPERAND_CRJ, HW_H_CR, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_CRJ] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* Rs1: dedicated register */
  { "Rs1", FR30_OPERAND_RS1, HW_H_DR, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_RS1] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* Rs2: dedicated register */
  { "Rs2", FR30_OPERAND_RS2, HW_H_DR, 12, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_RS2] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* R13: General Register 13 */
  { "R13", FR30_OPERAND_R13, HW_H_R13, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* R14: General Register 14 */
  { "R14", FR30_OPERAND_R14, HW_H_R14, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* R15: General Register 15 */
  { "R15", FR30_OPERAND_R15, HW_H_R15, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* ps: Program Status register */
  { "ps", FR30_OPERAND_PS, HW_H_PS, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* u4: 4  bit unsigned immediate */
  { "u4", FR30_OPERAND_U4, HW_H_UINT, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_U4] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* u4c: 4  bit unsigned immediate */
  { "u4c", FR30_OPERAND_U4C, HW_H_UINT, 12, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_U4C] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* u8: 8  bit unsigned immediate */
  { "u8", FR30_OPERAND_U8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_U8] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* i8: 8  bit unsigned immediate */
  { "i8", FR30_OPERAND_I8, HW_H_UINT, 4, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_I8] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* udisp6: 6  bit unsigned immediate */
  { "udisp6", FR30_OPERAND_UDISP6, HW_H_UINT, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_UDISP6] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* disp8: 8  bit signed   immediate */
  { "disp8", FR30_OPERAND_DISP8, HW_H_SINT, 4, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_DISP8] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* disp9: 9  bit signed   immediate */
  { "disp9", FR30_OPERAND_DISP9, HW_H_SINT, 4, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_DISP9] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* disp10: 10 bit signed   immediate */
  { "disp10", FR30_OPERAND_DISP10, HW_H_SINT, 4, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_DISP10] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* s10: 10 bit signed   immediate */
  { "s10", FR30_OPERAND_S10, HW_H_SINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_S10] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* u10: 10 bit unsigned immediate */
  { "u10", FR30_OPERAND_U10, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_U10] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* i32: 32 bit immediate */
  { "i32", FR30_OPERAND_I32, HW_H_UINT, 0, 32,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_I32] } }, 
    { 0|A(HASH_PREFIX)|A(SIGN_OPT), { (1<<MACH_BASE) } }  },
/* m4: 4  bit negative immediate */
  { "m4", FR30_OPERAND_M4, HW_H_SINT, 8, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_M4] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* i20: 20 bit immediate */
  { "i20", FR30_OPERAND_I20, HW_H_UINT, 0, 20,
    { 2, { (const PTR) &FR30_F_I20_MULTI_IFIELD[0] } }, 
    { 0|A(HASH_PREFIX)|A(VIRTUAL), { (1<<MACH_BASE) } }  },
/* dir8: 8  bit direct address */
  { "dir8", FR30_OPERAND_DIR8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_DIR8] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* dir9: 9  bit direct address */
  { "dir9", FR30_OPERAND_DIR9, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_DIR9] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* dir10: 10 bit direct address */
  { "dir10", FR30_OPERAND_DIR10, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_DIR10] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* label9: 9  bit pc relative address */
  { "label9", FR30_OPERAND_LABEL9, HW_H_IADDR, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_REL9] } }, 
    { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
/* label12: 12 bit pc relative address */
  { "label12", FR30_OPERAND_LABEL12, HW_H_IADDR, 5, 11,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_REL12] } }, 
    { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
/* reglist_low_ld: 8 bit low register mask for ldm */
  { "reglist_low_ld", FR30_OPERAND_REGLIST_LOW_LD, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_REGLIST_LOW_LD] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* reglist_hi_ld: 8 bit high register mask for ldm */
  { "reglist_hi_ld", FR30_OPERAND_REGLIST_HI_LD, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_REGLIST_HI_LD] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* reglist_low_st: 8 bit low register mask for stm */
  { "reglist_low_st", FR30_OPERAND_REGLIST_LOW_ST, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_REGLIST_LOW_ST] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* reglist_hi_st: 8 bit high register mask for stm */
  { "reglist_hi_st", FR30_OPERAND_REGLIST_HI_ST, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_REGLIST_HI_ST] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cc: condition codes */
  { "cc", FR30_OPERAND_CC, HW_H_UINT, 4, 4,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_CC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* ccc: coprocessor calc */
  { "ccc", FR30_OPERAND_CCC, HW_H_UINT, 0, 8,
    { 0, { (const PTR) &fr30_cgen_ifld_table[FR30_F_CCC] } }, 
    { 0|A(HASH_PREFIX), { (1<<MACH_BASE) } }  },
/* nbit: negative   bit */
  { "nbit", FR30_OPERAND_NBIT, HW_H_NBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* vbit: overflow   bit */
  { "vbit", FR30_OPERAND_VBIT, HW_H_VBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* zbit: zero       bit */
  { "zbit", FR30_OPERAND_ZBIT, HW_H_ZBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* cbit: carry      bit */
  { "cbit", FR30_OPERAND_CBIT, HW_H_CBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* ibit: interrupt  bit */
  { "ibit", FR30_OPERAND_IBIT, HW_H_IBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* sbit: stack      bit */
  { "sbit", FR30_OPERAND_SBIT, HW_H_SBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* tbit: trace trap bit */
  { "tbit", FR30_OPERAND_TBIT, HW_H_TBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* d0bit: division 0 bit */
  { "d0bit", FR30_OPERAND_D0BIT, HW_H_D0BIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* d1bit: division 1 bit */
  { "d1bit", FR30_OPERAND_D1BIT, HW_H_D1BIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* ccr: condition code bits */
  { "ccr", FR30_OPERAND_CCR, HW_H_CCR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* scr: system condition bits */
  { "scr", FR30_OPERAND_SCR, HW_H_SCR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* ilm: interrupt level mask */
  { "ilm", FR30_OPERAND_ILM, HW_H_ILM, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
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

static const CGEN_IBASE fr30_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, {0, {0}} },
/* add $Rj,$Ri */
  {
    FR30_INSN_ADD, "add", "add", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* add $u4,$Ri */
  {
    FR30_INSN_ADDI, "addi", "add", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* add2 $m4,$Ri */
  {
    FR30_INSN_ADD2, "add2", "add2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* addc $Rj,$Ri */
  {
    FR30_INSN_ADDC, "addc", "addc", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* addn $Rj,$Ri */
  {
    FR30_INSN_ADDN, "addn", "addn", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* addn $u4,$Ri */
  {
    FR30_INSN_ADDNI, "addni", "addn", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* addn2 $m4,$Ri */
  {
    FR30_INSN_ADDN2, "addn2", "addn2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* sub $Rj,$Ri */
  {
    FR30_INSN_SUB, "sub", "sub", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* subc $Rj,$Ri */
  {
    FR30_INSN_SUBC, "subc", "subc", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* subn $Rj,$Ri */
  {
    FR30_INSN_SUBN, "subn", "subn", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* cmp $Rj,$Ri */
  {
    FR30_INSN_CMP, "cmp", "cmp", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* cmp $u4,$Ri */
  {
    FR30_INSN_CMPI, "cmpi", "cmp", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* cmp2 $m4,$Ri */
  {
    FR30_INSN_CMP2, "cmp2", "cmp2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* and $Rj,$Ri */
  {
    FR30_INSN_AND, "and", "and", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* or $Rj,$Ri */
  {
    FR30_INSN_OR, "or", "or", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* eor $Rj,$Ri */
  {
    FR30_INSN_EOR, "eor", "eor", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* and $Rj,@$Ri */
  {
    FR30_INSN_ANDM, "andm", "and", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* andh $Rj,@$Ri */
  {
    FR30_INSN_ANDH, "andh", "andh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* andb $Rj,@$Ri */
  {
    FR30_INSN_ANDB, "andb", "andb", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* or $Rj,@$Ri */
  {
    FR30_INSN_ORM, "orm", "or", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* orh $Rj,@$Ri */
  {
    FR30_INSN_ORH, "orh", "orh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* orb $Rj,@$Ri */
  {
    FR30_INSN_ORB, "orb", "orb", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* eor $Rj,@$Ri */
  {
    FR30_INSN_EORM, "eorm", "eor", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* eorh $Rj,@$Ri */
  {
    FR30_INSN_EORH, "eorh", "eorh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* eorb $Rj,@$Ri */
  {
    FR30_INSN_EORB, "eorb", "eorb", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bandl $u4,@$Ri */
  {
    FR30_INSN_BANDL, "bandl", "bandl", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* borl $u4,@$Ri */
  {
    FR30_INSN_BORL, "borl", "borl", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beorl $u4,@$Ri */
  {
    FR30_INSN_BEORL, "beorl", "beorl", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bandh $u4,@$Ri */
  {
    FR30_INSN_BANDH, "bandh", "bandh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* borh $u4,@$Ri */
  {
    FR30_INSN_BORH, "borh", "borh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beorh $u4,@$Ri */
  {
    FR30_INSN_BEORH, "beorh", "beorh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* btstl $u4,@$Ri */
  {
    FR30_INSN_BTSTL, "btstl", "btstl", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* btsth $u4,@$Ri */
  {
    FR30_INSN_BTSTH, "btsth", "btsth", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* mul $Rj,$Ri */
  {
    FR30_INSN_MUL, "mul", "mul", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* mulu $Rj,$Ri */
  {
    FR30_INSN_MULU, "mulu", "mulu", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* mulh $Rj,$Ri */
  {
    FR30_INSN_MULH, "mulh", "mulh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* muluh $Rj,$Ri */
  {
    FR30_INSN_MULUH, "muluh", "muluh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* div0s $Ri */
  {
    FR30_INSN_DIV0S, "div0s", "div0s", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* div0u $Ri */
  {
    FR30_INSN_DIV0U, "div0u", "div0u", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* div1 $Ri */
  {
    FR30_INSN_DIV1, "div1", "div1", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* div2 $Ri */
  {
    FR30_INSN_DIV2, "div2", "div2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* div3 */
  {
    FR30_INSN_DIV3, "div3", "div3", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* div4s */
  {
    FR30_INSN_DIV4S, "div4s", "div4s", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lsl $Rj,$Ri */
  {
    FR30_INSN_LSL, "lsl", "lsl", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lsl $u4,$Ri */
  {
    FR30_INSN_LSLI, "lsli", "lsl", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lsl2 $u4,$Ri */
  {
    FR30_INSN_LSL2, "lsl2", "lsl2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lsr $Rj,$Ri */
  {
    FR30_INSN_LSR, "lsr", "lsr", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lsr $u4,$Ri */
  {
    FR30_INSN_LSRI, "lsri", "lsr", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lsr2 $u4,$Ri */
  {
    FR30_INSN_LSR2, "lsr2", "lsr2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* asr $Rj,$Ri */
  {
    FR30_INSN_ASR, "asr", "asr", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* asr $u4,$Ri */
  {
    FR30_INSN_ASRI, "asri", "asr", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* asr2 $u4,$Ri */
  {
    FR30_INSN_ASR2, "asr2", "asr2", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldi:8 $i8,$Ri */
  {
    FR30_INSN_LDI8, "ldi8", "ldi:8", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldi:20 $i20,$Ri */
  {
    FR30_INSN_LDI20, "ldi20", "ldi:20", 32,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldi:32 $i32,$Ri */
  {
    FR30_INSN_LDI32, "ldi32", "ldi:32", 48,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ld @$Rj,$Ri */
  {
    FR30_INSN_LD, "ld", "ld", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lduh @$Rj,$Ri */
  {
    FR30_INSN_LDUH, "lduh", "lduh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldub @$Rj,$Ri */
  {
    FR30_INSN_LDUB, "ldub", "ldub", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ld @($R13,$Rj),$Ri */
  {
    FR30_INSN_LDR13, "ldr13", "ld", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lduh @($R13,$Rj),$Ri */
  {
    FR30_INSN_LDR13UH, "ldr13uh", "lduh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldub @($R13,$Rj),$Ri */
  {
    FR30_INSN_LDR13UB, "ldr13ub", "ldub", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ld @($R14,$disp10),$Ri */
  {
    FR30_INSN_LDR14, "ldr14", "ld", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* lduh @($R14,$disp9),$Ri */
  {
    FR30_INSN_LDR14UH, "ldr14uh", "lduh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldub @($R14,$disp8),$Ri */
  {
    FR30_INSN_LDR14UB, "ldr14ub", "ldub", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ld @($R15,$udisp6),$Ri */
  {
    FR30_INSN_LDR15, "ldr15", "ld", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ld @$R15+,$Ri */
  {
    FR30_INSN_LDR15GR, "ldr15gr", "ld", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ld @$R15+,$Rs2 */
  {
    FR30_INSN_LDR15DR, "ldr15dr", "ld", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ld @$R15+,$ps */
  {
    FR30_INSN_LDR15PS, "ldr15ps", "ld", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* st $Ri,@$Rj */
  {
    FR30_INSN_ST, "st", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* sth $Ri,@$Rj */
  {
    FR30_INSN_STH, "sth", "sth", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* stb $Ri,@$Rj */
  {
    FR30_INSN_STB, "stb", "stb", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@($R13,$Rj) */
  {
    FR30_INSN_STR13, "str13", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* sth $Ri,@($R13,$Rj) */
  {
    FR30_INSN_STR13H, "str13h", "sth", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* stb $Ri,@($R13,$Rj) */
  {
    FR30_INSN_STR13B, "str13b", "stb", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@($R14,$disp10) */
  {
    FR30_INSN_STR14, "str14", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* sth $Ri,@($R14,$disp9) */
  {
    FR30_INSN_STR14H, "str14h", "sth", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* stb $Ri,@($R14,$disp8) */
  {
    FR30_INSN_STR14B, "str14b", "stb", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@($R15,$udisp6) */
  {
    FR30_INSN_STR15, "str15", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* st $Ri,@-$R15 */
  {
    FR30_INSN_STR15GR, "str15gr", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* st $Rs2,@-$R15 */
  {
    FR30_INSN_STR15DR, "str15dr", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* st $ps,@-$R15 */
  {
    FR30_INSN_STR15PS, "str15ps", "st", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* mov $Rj,$Ri */
  {
    FR30_INSN_MOV, "mov", "mov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* mov $Rs1,$Ri */
  {
    FR30_INSN_MOVDR, "movdr", "mov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* mov $ps,$Ri */
  {
    FR30_INSN_MOVPS, "movps", "mov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* mov $Ri,$Rs1 */
  {
    FR30_INSN_MOV2DR, "mov2dr", "mov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* mov $Ri,$ps */
  {
    FR30_INSN_MOV2PS, "mov2ps", "mov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* jmp @$Ri */
  {
    FR30_INSN_JMP, "jmp", "jmp", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* jmp:d @$Ri */
  {
    FR30_INSN_JMPD, "jmpd", "jmp:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* call @$Ri */
  {
    FR30_INSN_CALLR, "callr", "call", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* call:d @$Ri */
  {
    FR30_INSN_CALLRD, "callrd", "call:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* call $label12 */
  {
    FR30_INSN_CALL, "call", "call", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* call:d $label12 */
  {
    FR30_INSN_CALLD, "calld", "call:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ret */
  {
    FR30_INSN_RET, "ret", "ret", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* ret:d */
  {
    FR30_INSN_RET_D, "ret:d", "ret:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* int $u8 */
  {
    FR30_INSN_INT, "int", "int", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* inte */
  {
    FR30_INSN_INTE, "inte", "inte", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* reti */
  {
    FR30_INSN_RETI, "reti", "reti", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bra:d $label9 */
  {
    FR30_INSN_BRAD, "brad", "bra:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bra $label9 */
  {
    FR30_INSN_BRA, "bra", "bra", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(UNCOND_CTI), { (1<<MACH_BASE) } }
  },
/* bno:d $label9 */
  {
    FR30_INSN_BNOD, "bnod", "bno:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bno $label9 */
  {
    FR30_INSN_BNO, "bno", "bno", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beq:d $label9 */
  {
    FR30_INSN_BEQD, "beqd", "beq:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beq $label9 */
  {
    FR30_INSN_BEQ, "beq", "beq", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bne:d $label9 */
  {
    FR30_INSN_BNED, "bned", "bne:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bne $label9 */
  {
    FR30_INSN_BNE, "bne", "bne", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bc:d $label9 */
  {
    FR30_INSN_BCD, "bcd", "bc:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bc $label9 */
  {
    FR30_INSN_BC, "bc", "bc", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bnc:d $label9 */
  {
    FR30_INSN_BNCD, "bncd", "bnc:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bnc $label9 */
  {
    FR30_INSN_BNC, "bnc", "bnc", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bn:d $label9 */
  {
    FR30_INSN_BND, "bnd", "bn:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bn $label9 */
  {
    FR30_INSN_BN, "bn", "bn", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bp:d $label9 */
  {
    FR30_INSN_BPD, "bpd", "bp:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bp $label9 */
  {
    FR30_INSN_BP, "bp", "bp", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bv:d $label9 */
  {
    FR30_INSN_BVD, "bvd", "bv:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bv $label9 */
  {
    FR30_INSN_BV, "bv", "bv", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bnv:d $label9 */
  {
    FR30_INSN_BNVD, "bnvd", "bnv:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bnv $label9 */
  {
    FR30_INSN_BNV, "bnv", "bnv", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* blt:d $label9 */
  {
    FR30_INSN_BLTD, "bltd", "blt:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* blt $label9 */
  {
    FR30_INSN_BLT, "blt", "blt", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bge:d $label9 */
  {
    FR30_INSN_BGED, "bged", "bge:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bge $label9 */
  {
    FR30_INSN_BGE, "bge", "bge", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* ble:d $label9 */
  {
    FR30_INSN_BLED, "bled", "ble:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ble $label9 */
  {
    FR30_INSN_BLE, "ble", "ble", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bgt:d $label9 */
  {
    FR30_INSN_BGTD, "bgtd", "bgt:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bgt $label9 */
  {
    FR30_INSN_BGT, "bgt", "bgt", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bls:d $label9 */
  {
    FR30_INSN_BLSD, "blsd", "bls:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bls $label9 */
  {
    FR30_INSN_BLS, "bls", "bls", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* bhi:d $label9 */
  {
    FR30_INSN_BHID, "bhid", "bhi:d", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bhi $label9 */
  {
    FR30_INSN_BHI, "bhi", "bhi", 16,
    { 0|A(NOT_IN_DELAY_SLOT)|A(COND_CTI), { (1<<MACH_BASE) } }
  },
/* dmov $R13,@$dir10 */
  {
    FR30_INSN_DMOVR13, "dmovr13", "dmov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* dmovh $R13,@$dir9 */
  {
    FR30_INSN_DMOVR13H, "dmovr13h", "dmovh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* dmovb $R13,@$dir8 */
  {
    FR30_INSN_DMOVR13B, "dmovr13b", "dmovb", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* dmov @$R13+,@$dir10 */
  {
    FR30_INSN_DMOVR13PI, "dmovr13pi", "dmov", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovh @$R13+,@$dir9 */
  {
    FR30_INSN_DMOVR13PIH, "dmovr13pih", "dmovh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovb @$R13+,@$dir8 */
  {
    FR30_INSN_DMOVR13PIB, "dmovr13pib", "dmovb", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmov @$R15+,@$dir10 */
  {
    FR30_INSN_DMOVR15PI, "dmovr15pi", "dmov", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmov @$dir10,$R13 */
  {
    FR30_INSN_DMOV2R13, "dmov2r13", "dmov", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* dmovh @$dir9,$R13 */
  {
    FR30_INSN_DMOV2R13H, "dmov2r13h", "dmovh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* dmovb @$dir8,$R13 */
  {
    FR30_INSN_DMOV2R13B, "dmov2r13b", "dmovb", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* dmov @$dir10,@$R13+ */
  {
    FR30_INSN_DMOV2R13PI, "dmov2r13pi", "dmov", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovh @$dir9,@$R13+ */
  {
    FR30_INSN_DMOV2R13PIH, "dmov2r13pih", "dmovh", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmovb @$dir8,@$R13+ */
  {
    FR30_INSN_DMOV2R13PIB, "dmov2r13pib", "dmovb", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* dmov @$dir10,@-$R15 */
  {
    FR30_INSN_DMOV2R15PD, "dmov2r15pd", "dmov", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldres @$Ri+,$u4 */
  {
    FR30_INSN_LDRES, "ldres", "ldres", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* stres $u4,@$Ri+ */
  {
    FR30_INSN_STRES, "stres", "stres", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* copop $u4c,$ccc,$CRj,$CRi */
  {
    FR30_INSN_COPOP, "copop", "copop", 32,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* copld $u4c,$ccc,$Rjc,$CRi */
  {
    FR30_INSN_COPLD, "copld", "copld", 32,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* copst $u4c,$ccc,$CRj,$Ric */
  {
    FR30_INSN_COPST, "copst", "copst", 32,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* copsv $u4c,$ccc,$CRj,$Ric */
  {
    FR30_INSN_COPSV, "copsv", "copsv", 32,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* nop */
  {
    FR30_INSN_NOP, "nop", "nop", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* andccr $u8 */
  {
    FR30_INSN_ANDCCR, "andccr", "andccr", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* orccr $u8 */
  {
    FR30_INSN_ORCCR, "orccr", "orccr", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* stilm $u8 */
  {
    FR30_INSN_STILM, "stilm", "stilm", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* addsp $s10 */
  {
    FR30_INSN_ADDSP, "addsp", "addsp", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* extsb $Ri */
  {
    FR30_INSN_EXTSB, "extsb", "extsb", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* extub $Ri */
  {
    FR30_INSN_EXTUB, "extub", "extub", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* extsh $Ri */
  {
    FR30_INSN_EXTSH, "extsh", "extsh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* extuh $Ri */
  {
    FR30_INSN_EXTUH, "extuh", "extuh", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* ldm0 ($reglist_low_ld) */
  {
    FR30_INSN_LDM0, "ldm0", "ldm0", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* ldm1 ($reglist_hi_ld) */
  {
    FR30_INSN_LDM1, "ldm1", "ldm1", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* stm0 ($reglist_low_st) */
  {
    FR30_INSN_STM0, "stm0", "stm0", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* stm1 ($reglist_hi_st) */
  {
    FR30_INSN_STM1, "stm1", "stm1", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* enter $u10 */
  {
    FR30_INSN_ENTER, "enter", "enter", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* leave */
  {
    FR30_INSN_LEAVE, "leave", "leave", 16,
    { 0, { (1<<MACH_BASE) } }
  },
/* xchb @$Rj,$Ri */
  {
    FR30_INSN_XCHB, "xchb", "xchb", 16,
    { 0|A(NOT_IN_DELAY_SLOT), { (1<<MACH_BASE) } }
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
static void fr30_cgen_rebuild_tables PARAMS ((CGEN_CPU_TABLE *));

/* Subroutine of fr30_cgen_cpu_open to look up a mach via its bfd name.  */

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

/* Subroutine of fr30_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & fr30_cgen_hw_table[0];
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

/* Subroutine of fr30_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (cd)
     CGEN_CPU_TABLE *cd;
{
  cd->ifld_table = & fr30_cgen_ifld_table[0];
}

/* Subroutine of fr30_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & fr30_cgen_operand_table[0];
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

/* Subroutine of fr30_cgen_cpu_open to build the hardware table.
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
  const CGEN_IBASE *ib = & fr30_cgen_insn_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of fr30_cgen_cpu_open to rebuild the tables.  */

static void
fr30_cgen_rebuild_tables (cd)
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
	const CGEN_ISA *isa = & fr30_cgen_isa_table[i];

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
	const CGEN_MACH *mach = & fr30_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "fr30_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
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
fr30_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
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
	      lookup_mach_via_bfd_name (fr30_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "fr30_cgen_cpu_open: unsupported argument `%d'\n",
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
      fprintf (stderr, "fr30_cgen_cpu_open: no endianness specified\n");
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
  cd->rebuild_tables = fr30_cgen_rebuild_tables;
  fr30_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to fr30_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
fr30_cgen_cpu_open_1 (mach_name, endian)
     const char *mach_name;
     enum cgen_endian endian;
{
  return fr30_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
fr30_cgen_cpu_close (cd)
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

