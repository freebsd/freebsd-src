/* CPU data for m32r.

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
#include "m32r-desc.h"
#include "m32r-opc.h"
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
  { "m32r", MACH_M32R },
  { "m32rx", MACH_M32RX },
  { "m32r2", MACH_M32R2 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] ATTRIBUTE_UNUSED =
{
  { "m32r", ISA_M32R },
  { "max", ISA_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY PIPE_attr[] ATTRIBUTE_UNUSED =
{
  { "NONE", PIPE_NONE },
  { "O", PIPE_O },
  { "S", PIPE_S },
  { "OS", PIPE_OS },
  { "O_OS", PIPE_O_OS },
  { 0, 0 }
};

const CGEN_ATTR_TABLE m32r_cgen_ifield_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "RESERVED", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { "RELOC", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE m32r_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE m32r_cgen_operand_attr_table[] =
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
  { "RELOC", &bool_attr[0], &bool_attr[0] },
  { "HASH-PREFIX", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE m32r_cgen_insn_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "PIPE", & PIPE_attr[0], & PIPE_attr[0] },
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
  { "FILL-SLOT", &bool_attr[0], &bool_attr[0] },
  { "SPECIAL", &bool_attr[0], &bool_attr[0] },
  { "SPECIAL_M32R", &bool_attr[0], &bool_attr[0] },
  { "SPECIAL_FLOAT", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA m32r_cgen_isa_table[] = {
  { "m32r", 32, 32, 16, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH m32r_cgen_mach_table[] = {
  { "m32r", "m32r", MACH_M32R, 0 },
  { "m32rx", "m32rx", MACH_M32RX, 0 },
  { "m32r2", "m32r2", MACH_M32R2, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY m32r_cgen_opval_gr_names_entries[] =
{
  { "fp", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "lr", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 15, {0, {{{0, 0}}}}, 0, 0 },
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
  { "r15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD m32r_cgen_opval_gr_names =
{
  & m32r_cgen_opval_gr_names_entries[0],
  19,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY m32r_cgen_opval_cr_names_entries[] =
{
  { "psw", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "cbr", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "spi", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "spu", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "bpc", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "bbpsw", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "bbpc", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "evb", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "cr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "cr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "cr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "cr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "cr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "cr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "cr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "cr7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "cr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "cr9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "cr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "cr11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cr13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "cr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "cr15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD m32r_cgen_opval_cr_names =
{
  & m32r_cgen_opval_cr_names_entries[0],
  24,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY m32r_cgen_opval_h_accums_entries[] =
{
  { "a0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "a1", 1, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD m32r_cgen_opval_h_accums =
{
  & m32r_cgen_opval_h_accums_entries[0],
  2,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY m32r_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-hi16", HW_H_HI16, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-slo16", HW_H_SLO16, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-ulo16", HW_H_ULO16, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & m32r_cgen_opval_gr_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-cr", HW_H_CR, CGEN_ASM_KEYWORD, (PTR) & m32r_cgen_opval_cr_names, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-accum", HW_H_ACCUM, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-accums", HW_H_ACCUMS, CGEN_ASM_KEYWORD, (PTR) & m32r_cgen_opval_h_accums, { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } } } } },
  { "h-cond", HW_H_COND, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-psw", HW_H_PSW, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-bpsw", HW_H_BPSW, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-bbpsw", HW_H_BBPSW, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-lock", HW_H_LOCK, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { 0, 0, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD m32r_cgen_ifld_table[] =
{
  { M32R_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_OP1, "f-op1", 0, 32, 0, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_OP2, "f-op2", 0, 32, 8, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_COND, "f-cond", 0, 32, 4, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_R1, "f-r1", 0, 32, 4, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_R2, "f-r2", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_SIMM8, "f-simm8", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_SIMM16, "f-simm16", 0, 32, 16, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_SHIFT_OP2, "f-shift-op2", 0, 32, 8, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_UIMM3, "f-uimm3", 0, 32, 5, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_UIMM4, "f-uimm4", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_UIMM5, "f-uimm5", 0, 32, 11, 5, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_UIMM8, "f-uimm8", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_UIMM16, "f-uimm16", 0, 32, 16, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_UIMM24, "f-uimm24", 0, 32, 8, 24, { 0|A(RELOC)|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_HI16, "f-hi16", 0, 32, 16, 16, { 0|A(SIGN_OPT), { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_DISP8, "f-disp8", 0, 32, 8, 8, { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_DISP16, "f-disp16", 0, 32, 16, 16, { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_DISP24, "f-disp24", 0, 32, 8, 24, { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_OP23, "f-op23", 0, 32, 9, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_OP3, "f-op3", 0, 32, 14, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_ACC, "f-acc", 0, 32, 8, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_ACCS, "f-accs", 0, 32, 12, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_ACCD, "f-accd", 0, 32, 4, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_BITS67, "f-bits67", 0, 32, 6, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_BIT4, "f-bit4", 0, 32, 4, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_BIT14, "f-bit14", 0, 32, 14, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { M32R_F_IMM1, "f-imm1", 0, 32, 15, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { 0, 0, 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A



/* multi ifield declarations */



/* multi ifield definitions */


/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) M32R_OPERAND_##op
#else
#define OPERAND(op) M32R_OPERAND_/**/op
#endif

const CGEN_OPERAND m32r_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", M32R_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_NIL] } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* sr: source register */
  { "sr", M32R_OPERAND_SR, HW_H_GR, 12, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_R2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* dr: destination register */
  { "dr", M32R_OPERAND_DR, HW_H_GR, 4, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_R1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* src1: source register 1 */
  { "src1", M32R_OPERAND_SRC1, HW_H_GR, 4, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_R1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* src2: source register 2 */
  { "src2", M32R_OPERAND_SRC2, HW_H_GR, 12, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_R2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* scr: source control register */
  { "scr", M32R_OPERAND_SCR, HW_H_CR, 12, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_R2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* dcr: destination control register */
  { "dcr", M32R_OPERAND_DCR, HW_H_CR, 4, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_R1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* simm8: 8 bit signed immediate */
  { "simm8", M32R_OPERAND_SIMM8, HW_H_SINT, 8, 8,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_SIMM8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* simm16: 16 bit signed immediate */
  { "simm16", M32R_OPERAND_SIMM16, HW_H_SINT, 16, 16,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_SIMM16] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm3: 3 bit unsigned number */
  { "uimm3", M32R_OPERAND_UIMM3, HW_H_UINT, 5, 3,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM3] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm4: 4 bit trap number */
  { "uimm4", M32R_OPERAND_UIMM4, HW_H_UINT, 12, 4,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM4] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm5: 5 bit shift count */
  { "uimm5", M32R_OPERAND_UIMM5, HW_H_UINT, 11, 5,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM5] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm8: 8 bit unsigned immediate */
  { "uimm8", M32R_OPERAND_UIMM8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm16: 16 bit unsigned immediate */
  { "uimm16", M32R_OPERAND_UIMM16, HW_H_UINT, 16, 16,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM16] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* imm1: 1 bit immediate */
  { "imm1", M32R_OPERAND_IMM1, HW_H_UINT, 15, 1,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_IMM1] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } } } }  },
/* accd: accumulator destination register */
  { "accd", M32R_OPERAND_ACCD, HW_H_ACCUMS, 4, 2,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_ACCD] } }, 
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } } } }  },
/* accs: accumulator source register */
  { "accs", M32R_OPERAND_ACCS, HW_H_ACCUMS, 12, 2,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_ACCS] } }, 
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } } } }  },
/* acc: accumulator reg (d) */
  { "acc", M32R_OPERAND_ACC, HW_H_ACCUMS, 8, 1,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_ACC] } }, 
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } } } }  },
/* hash: # prefix */
  { "hash", M32R_OPERAND_HASH, HW_H_SINT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* hi16: high 16 bit immediate, sign optional */
  { "hi16", M32R_OPERAND_HI16, HW_H_HI16, 16, 16,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_HI16] } }, 
    { 0|A(SIGN_OPT), { { { (1<<MACH_BASE), 0 } } } }  },
/* slo16: 16 bit signed immediate, for low() */
  { "slo16", M32R_OPERAND_SLO16, HW_H_SLO16, 16, 16,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_SIMM16] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* ulo16: 16 bit unsigned immediate, for low() */
  { "ulo16", M32R_OPERAND_ULO16, HW_H_ULO16, 16, 16,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM16] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm24: 24 bit address */
  { "uimm24", M32R_OPERAND_UIMM24, HW_H_ADDR, 8, 24,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_UIMM24] } }, 
    { 0|A(HASH_PREFIX)|A(RELOC)|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* disp8: 8 bit displacement */
  { "disp8", M32R_OPERAND_DISP8, HW_H_IADDR, 8, 8,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_DISP8] } }, 
    { 0|A(RELAX)|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* disp16: 16 bit displacement */
  { "disp16", M32R_OPERAND_DISP16, HW_H_IADDR, 16, 16,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_DISP16] } }, 
    { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* disp24: 24 bit displacement */
  { "disp24", M32R_OPERAND_DISP24, HW_H_IADDR, 8, 24,
    { 0, { (const PTR) &m32r_cgen_ifld_table[M32R_F_DISP24] } }, 
    { 0|A(RELAX)|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* condbit: condition bit */
  { "condbit", M32R_OPERAND_CONDBIT, HW_H_COND, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* accum: accumulator */
  { "accum", M32R_OPERAND_ACCUM, HW_H_ACCUM, 0, 0,
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

static const CGEN_IBASE m32r_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } } },
/* add $dr,$sr */
  {
    M32R_INSN_ADD, "add", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add3 $dr,$sr,$hash$slo16 */
  {
    M32R_INSN_ADD3, "add3", "add3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* and $dr,$sr */
  {
    M32R_INSN_AND, "and", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and3 $dr,$sr,$uimm16 */
  {
    M32R_INSN_AND3, "and3", "and3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* or $dr,$sr */
  {
    M32R_INSN_OR, "or", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or3 $dr,$sr,$hash$ulo16 */
  {
    M32R_INSN_OR3, "or3", "or3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* xor $dr,$sr */
  {
    M32R_INSN_XOR, "xor", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor3 $dr,$sr,$uimm16 */
  {
    M32R_INSN_XOR3, "xor3", "xor3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* addi $dr,$simm8 */
  {
    M32R_INSN_ADDI, "addi", "addi", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addv $dr,$sr */
  {
    M32R_INSN_ADDV, "addv", "addv", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addv3 $dr,$sr,$simm16 */
  {
    M32R_INSN_ADDV3, "addv3", "addv3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* addx $dr,$sr */
  {
    M32R_INSN_ADDX, "addx", "addx", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bc.s $disp8 */
  {
    M32R_INSN_BC8, "bc8", "bc.s", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bc.l $disp24 */
  {
    M32R_INSN_BC24, "bc24", "bc.l", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* beq $src1,$src2,$disp16 */
  {
    M32R_INSN_BEQ, "beq", "beq", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* beqz $src2,$disp16 */
  {
    M32R_INSN_BEQZ, "beqz", "beqz", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bgez $src2,$disp16 */
  {
    M32R_INSN_BGEZ, "bgez", "bgez", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bgtz $src2,$disp16 */
  {
    M32R_INSN_BGTZ, "bgtz", "bgtz", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* blez $src2,$disp16 */
  {
    M32R_INSN_BLEZ, "blez", "blez", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bltz $src2,$disp16 */
  {
    M32R_INSN_BLTZ, "bltz", "bltz", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bnez $src2,$disp16 */
  {
    M32R_INSN_BNEZ, "bnez", "bnez", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bl.s $disp8 */
  {
    M32R_INSN_BL8, "bl8", "bl.s", 16,
    { 0|A(FILL_SLOT)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bl.l $disp24 */
  {
    M32R_INSN_BL24, "bl24", "bl.l", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bcl.s $disp8 */
  {
    M32R_INSN_BCL8, "bcl8", "bcl.s", 16,
    { 0|A(FILL_SLOT)|A(COND_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bcl.l $disp24 */
  {
    M32R_INSN_BCL24, "bcl24", "bcl.l", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bnc.s $disp8 */
  {
    M32R_INSN_BNC8, "bnc8", "bnc.s", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bnc.l $disp24 */
  {
    M32R_INSN_BNC24, "bnc24", "bnc.l", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bne $src1,$src2,$disp16 */
  {
    M32R_INSN_BNE, "bne", "bne", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bra.s $disp8 */
  {
    M32R_INSN_BRA8, "bra8", "bra.s", 16,
    { 0|A(FILL_SLOT)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bra.l $disp24 */
  {
    M32R_INSN_BRA24, "bra24", "bra.l", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bncl.s $disp8 */
  {
    M32R_INSN_BNCL8, "bncl8", "bncl.s", 16,
    { 0|A(FILL_SLOT)|A(COND_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bncl.l $disp24 */
  {
    M32R_INSN_BNCL24, "bncl24", "bncl.l", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* cmp $src1,$src2 */
  {
    M32R_INSN_CMP, "cmp", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi $src2,$simm16 */
  {
    M32R_INSN_CMPI, "cmpi", "cmpi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* cmpu $src1,$src2 */
  {
    M32R_INSN_CMPU, "cmpu", "cmpu", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpui $src2,$simm16 */
  {
    M32R_INSN_CMPUI, "cmpui", "cmpui", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* cmpeq $src1,$src2 */
  {
    M32R_INSN_CMPEQ, "cmpeq", "cmpeq", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpz $src2 */
  {
    M32R_INSN_CMPZ, "cmpz", "cmpz", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* div $dr,$sr */
  {
    M32R_INSN_DIV, "div", "div", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* divu $dr,$sr */
  {
    M32R_INSN_DIVU, "divu", "divu", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* rem $dr,$sr */
  {
    M32R_INSN_REM, "rem", "rem", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* remu $dr,$sr */
  {
    M32R_INSN_REMU, "remu", "remu", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* remh $dr,$sr */
  {
    M32R_INSN_REMH, "remh", "remh", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* remuh $dr,$sr */
  {
    M32R_INSN_REMUH, "remuh", "remuh", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* remb $dr,$sr */
  {
    M32R_INSN_REMB, "remb", "remb", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* remub $dr,$sr */
  {
    M32R_INSN_REMUB, "remub", "remub", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* divuh $dr,$sr */
  {
    M32R_INSN_DIVUH, "divuh", "divuh", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* divb $dr,$sr */
  {
    M32R_INSN_DIVB, "divb", "divb", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* divub $dr,$sr */
  {
    M32R_INSN_DIVUB, "divub", "divub", 32,
    { 0, { { { (1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* divh $dr,$sr */
  {
    M32R_INSN_DIVH, "divh", "divh", 32,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* jc $sr */
  {
    M32R_INSN_JC, "jc", "jc", 16,
    { 0|A(SPECIAL)|A(COND_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* jnc $sr */
  {
    M32R_INSN_JNC, "jnc", "jnc", 16,
    { 0|A(SPECIAL)|A(COND_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* jl $sr */
  {
    M32R_INSN_JL, "jl", "jl", 16,
    { 0|A(FILL_SLOT)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* jmp $sr */
  {
    M32R_INSN_JMP, "jmp", "jmp", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* ld $dr,@$sr */
  {
    M32R_INSN_LD, "ld", "ld", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* ld $dr,@($slo16,$sr) */
  {
    M32R_INSN_LD_D, "ld-d", "ld", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* ldb $dr,@$sr */
  {
    M32R_INSN_LDB, "ldb", "ldb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* ldb $dr,@($slo16,$sr) */
  {
    M32R_INSN_LDB_D, "ldb-d", "ldb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* ldh $dr,@$sr */
  {
    M32R_INSN_LDH, "ldh", "ldh", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* ldh $dr,@($slo16,$sr) */
  {
    M32R_INSN_LDH_D, "ldh-d", "ldh", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* ldub $dr,@$sr */
  {
    M32R_INSN_LDUB, "ldub", "ldub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* ldub $dr,@($slo16,$sr) */
  {
    M32R_INSN_LDUB_D, "ldub-d", "ldub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* lduh $dr,@$sr */
  {
    M32R_INSN_LDUH, "lduh", "lduh", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* lduh $dr,@($slo16,$sr) */
  {
    M32R_INSN_LDUH_D, "lduh-d", "lduh", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* ld $dr,@$sr+ */
  {
    M32R_INSN_LD_PLUS, "ld-plus", "ld", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* ld24 $dr,$uimm24 */
  {
    M32R_INSN_LD24, "ld24", "ld24", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* ldi8 $dr,$simm8 */
  {
    M32R_INSN_LDI8, "ldi8", "ldi8", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* ldi16 $dr,$hash$slo16 */
  {
    M32R_INSN_LDI16, "ldi16", "ldi16", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* lock $dr,@$sr */
  {
    M32R_INSN_LOCK, "lock", "lock", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* machi $src1,$src2 */
  {
    M32R_INSN_MACHI, "machi", "machi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* machi $src1,$src2,$acc */
  {
    M32R_INSN_MACHI_A, "machi-a", "machi", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* maclo $src1,$src2 */
  {
    M32R_INSN_MACLO, "maclo", "maclo", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* maclo $src1,$src2,$acc */
  {
    M32R_INSN_MACLO_A, "maclo-a", "maclo", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* macwhi $src1,$src2 */
  {
    M32R_INSN_MACWHI, "macwhi", "macwhi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* macwhi $src1,$src2,$acc */
  {
    M32R_INSN_MACWHI_A, "macwhi-a", "macwhi", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* macwlo $src1,$src2 */
  {
    M32R_INSN_MACWLO, "macwlo", "macwlo", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* macwlo $src1,$src2,$acc */
  {
    M32R_INSN_MACWLO_A, "macwlo-a", "macwlo", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mul $dr,$sr */
  {
    M32R_INSN_MUL, "mul", "mul", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulhi $src1,$src2 */
  {
    M32R_INSN_MULHI, "mulhi", "mulhi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulhi $src1,$src2,$acc */
  {
    M32R_INSN_MULHI_A, "mulhi-a", "mulhi", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mullo $src1,$src2 */
  {
    M32R_INSN_MULLO, "mullo", "mullo", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mullo $src1,$src2,$acc */
  {
    M32R_INSN_MULLO_A, "mullo-a", "mullo", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulwhi $src1,$src2 */
  {
    M32R_INSN_MULWHI, "mulwhi", "mulwhi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulwhi $src1,$src2,$acc */
  {
    M32R_INSN_MULWHI_A, "mulwhi-a", "mulwhi", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulwlo $src1,$src2 */
  {
    M32R_INSN_MULWLO, "mulwlo", "mulwlo", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulwlo $src1,$src2,$acc */
  {
    M32R_INSN_MULWLO_A, "mulwlo-a", "mulwlo", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mv $dr,$sr */
  {
    M32R_INSN_MV, "mv", "mv", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mvfachi $dr */
  {
    M32R_INSN_MVFACHI, "mvfachi", "mvfachi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvfachi $dr,$accs */
  {
    M32R_INSN_MVFACHI_A, "mvfachi-a", "mvfachi", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvfaclo $dr */
  {
    M32R_INSN_MVFACLO, "mvfaclo", "mvfaclo", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvfaclo $dr,$accs */
  {
    M32R_INSN_MVFACLO_A, "mvfaclo-a", "mvfaclo", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvfacmi $dr */
  {
    M32R_INSN_MVFACMI, "mvfacmi", "mvfacmi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvfacmi $dr,$accs */
  {
    M32R_INSN_MVFACMI_A, "mvfacmi-a", "mvfacmi", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvfc $dr,$scr */
  {
    M32R_INSN_MVFC, "mvfc", "mvfc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* mvtachi $src1 */
  {
    M32R_INSN_MVTACHI, "mvtachi", "mvtachi", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvtachi $src1,$accs */
  {
    M32R_INSN_MVTACHI_A, "mvtachi-a", "mvtachi", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvtaclo $src1 */
  {
    M32R_INSN_MVTACLO, "mvtaclo", "mvtaclo", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvtaclo $src1,$accs */
  {
    M32R_INSN_MVTACLO_A, "mvtaclo-a", "mvtaclo", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mvtc $sr,$dcr */
  {
    M32R_INSN_MVTC, "mvtc", "mvtc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* neg $dr,$sr */
  {
    M32R_INSN_NEG, "neg", "neg", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* nop */
  {
    M32R_INSN_NOP, "nop", "nop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* not $dr,$sr */
  {
    M32R_INSN_NOT, "not", "not", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* rac */
  {
    M32R_INSN_RAC, "rac", "rac", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* rac $accd,$accs,$imm1 */
  {
    M32R_INSN_RAC_DSI, "rac-dsi", "rac", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* rach */
  {
    M32R_INSN_RACH, "rach", "rach", 16,
    { 0, { { { (1<<MACH_M32R), 0 } }, { { PIPE_S, 0 } } } }
  },
/* rach $accd,$accs,$imm1 */
  {
    M32R_INSN_RACH_DSI, "rach-dsi", "rach", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* rte */
  {
    M32R_INSN_RTE, "rte", "rte", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* seth $dr,$hash$hi16 */
  {
    M32R_INSN_SETH, "seth", "seth", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* sll $dr,$sr */
  {
    M32R_INSN_SLL, "sll", "sll", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O_OS, 0 } } } }
  },
/* sll3 $dr,$sr,$simm16 */
  {
    M32R_INSN_SLL3, "sll3", "sll3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* slli $dr,$uimm5 */
  {
    M32R_INSN_SLLI, "slli", "slli", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O_OS, 0 } } } }
  },
/* sra $dr,$sr */
  {
    M32R_INSN_SRA, "sra", "sra", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O_OS, 0 } } } }
  },
/* sra3 $dr,$sr,$simm16 */
  {
    M32R_INSN_SRA3, "sra3", "sra3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* srai $dr,$uimm5 */
  {
    M32R_INSN_SRAI, "srai", "srai", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O_OS, 0 } } } }
  },
/* srl $dr,$sr */
  {
    M32R_INSN_SRL, "srl", "srl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O_OS, 0 } } } }
  },
/* srl3 $dr,$sr,$simm16 */
  {
    M32R_INSN_SRL3, "srl3", "srl3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* srli $dr,$uimm5 */
  {
    M32R_INSN_SRLI, "srli", "srli", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O_OS, 0 } } } }
  },
/* st $src1,@$src2 */
  {
    M32R_INSN_ST, "st", "st", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* st $src1,@($slo16,$src2) */
  {
    M32R_INSN_ST_D, "st-d", "st", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* stb $src1,@$src2 */
  {
    M32R_INSN_STB, "stb", "stb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* stb $src1,@($slo16,$src2) */
  {
    M32R_INSN_STB_D, "stb-d", "stb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* sth $src1,@$src2 */
  {
    M32R_INSN_STH, "sth", "sth", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* sth $src1,@($slo16,$src2) */
  {
    M32R_INSN_STH_D, "sth-d", "sth", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* st $src1,@+$src2 */
  {
    M32R_INSN_ST_PLUS, "st-plus", "st", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* sth $src1,@$src2+ */
  {
    M32R_INSN_STH_PLUS, "sth-plus", "sth", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* stb $src1,@$src2+ */
  {
    M32R_INSN_STB_PLUS, "stb-plus", "stb", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* st $src1,@-$src2 */
  {
    M32R_INSN_ST_MINUS, "st-minus", "st", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* sub $dr,$sr */
  {
    M32R_INSN_SUB, "sub", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subv $dr,$sr */
  {
    M32R_INSN_SUBV, "subv", "subv", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subx $dr,$sr */
  {
    M32R_INSN_SUBX, "subx", "subx", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* trap $uimm4 */
  {
    M32R_INSN_TRAP, "trap", "trap", 16,
    { 0|A(FILL_SLOT)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* unlock $src1,@$src2 */
  {
    M32R_INSN_UNLOCK, "unlock", "unlock", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* satb $dr,$sr */
  {
    M32R_INSN_SATB, "satb", "satb", 32,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* sath $dr,$sr */
  {
    M32R_INSN_SATH, "sath", "sath", 32,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* sat $dr,$sr */
  {
    M32R_INSN_SAT, "sat", "sat", 32,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* pcmpbz $src2 */
  {
    M32R_INSN_PCMPBZ, "pcmpbz", "pcmpbz", 16,
    { 0|A(SPECIAL), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sadd */
  {
    M32R_INSN_SADD, "sadd", "sadd", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* macwu1 $src1,$src2 */
  {
    M32R_INSN_MACWU1, "macwu1", "macwu1", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* msblo $src1,$src2 */
  {
    M32R_INSN_MSBLO, "msblo", "msblo", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* mulwu1 $src1,$src2 */
  {
    M32R_INSN_MULWU1, "mulwu1", "mulwu1", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* maclh1 $src1,$src2 */
  {
    M32R_INSN_MACLH1, "maclh1", "maclh1", 16,
    { 0, { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_S, 0 } } } }
  },
/* sc */
  {
    M32R_INSN_SC, "sc", "sc", 16,
    { 0|A(SPECIAL)|A(SKIP_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* snc */
  {
    M32R_INSN_SNC, "snc", "snc", 16,
    { 0|A(SPECIAL)|A(SKIP_CTI), { { { (1<<MACH_M32RX)|(1<<MACH_M32R2), 0 } }, { { PIPE_O, 0 } } } }
  },
/* clrpsw $uimm8 */
  {
    M32R_INSN_CLRPSW, "clrpsw", "clrpsw", 16,
    { 0|A(SPECIAL_M32R), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* setpsw $uimm8 */
  {
    M32R_INSN_SETPSW, "setpsw", "setpsw", 16,
    { 0|A(SPECIAL_M32R), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
  },
/* bset $uimm3,@($slo16,$sr) */
  {
    M32R_INSN_BSET, "bset", "bset", 32,
    { 0|A(SPECIAL_M32R), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* bclr $uimm3,@($slo16,$sr) */
  {
    M32R_INSN_BCLR, "bclr", "bclr", 32,
    { 0|A(SPECIAL_M32R), { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } }
  },
/* btst $uimm3,$sr */
  {
    M32R_INSN_BTST, "btst", "btst", 16,
    { 0|A(SPECIAL_M32R), { { { (1<<MACH_BASE), 0 } }, { { PIPE_O, 0 } } } }
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
static void m32r_cgen_rebuild_tables (CGEN_CPU_TABLE *);

/* Subroutine of m32r_cgen_cpu_open to look up a mach via its bfd name.  */

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

/* Subroutine of m32r_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & m32r_cgen_hw_table[0];
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

/* Subroutine of m32r_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (CGEN_CPU_TABLE *cd)
{
  cd->ifld_table = & m32r_cgen_ifld_table[0];
}

/* Subroutine of m32r_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & m32r_cgen_operand_table[0];
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

/* Subroutine of m32r_cgen_cpu_open to build the hardware table.
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
  const CGEN_IBASE *ib = & m32r_cgen_insn_table[0];
  CGEN_INSN *insns = xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of m32r_cgen_cpu_open to rebuild the tables.  */

static void
m32r_cgen_rebuild_tables (CGEN_CPU_TABLE *cd)
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
	const CGEN_ISA *isa = & m32r_cgen_isa_table[i];

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
	const CGEN_MACH *mach = & m32r_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "m32r_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
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
m32r_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
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
	      lookup_mach_via_bfd_name (m32r_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "m32r_cgen_cpu_open: unsupported argument `%d'\n",
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
      fprintf (stderr, "m32r_cgen_cpu_open: no endianness specified\n");
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
  cd->rebuild_tables = m32r_cgen_rebuild_tables;
  m32r_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to m32r_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
m32r_cgen_cpu_open_1 (const char *mach_name, enum cgen_endian endian)
{
  return m32r_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
m32r_cgen_cpu_close (CGEN_CPU_DESC cd)
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

