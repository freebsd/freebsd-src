/* CPU data for ip2k.

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
#include "ip2k-desc.h"
#include "ip2k-opc.h"
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
  { "ip2022", MACH_IP2022 },
  { "ip2022ext", MACH_IP2022EXT },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] ATTRIBUTE_UNUSED =
{
  { "ip2k", ISA_IP2K },
  { "max", ISA_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE ip2k_cgen_ifield_attr_table[] =
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

const CGEN_ATTR_TABLE ip2k_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE ip2k_cgen_operand_attr_table[] =
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

const CGEN_ATTR_TABLE ip2k_cgen_insn_attr_table[] =
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
  { "EXT-SKIP-INSN", &bool_attr[0], &bool_attr[0] },
  { "SKIPA", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA ip2k_cgen_isa_table[] = {
  { "ip2k", 16, 16, 16, 16 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH ip2k_cgen_mach_table[] = {
  { "ip2022", "ip2022", MACH_IP2022, 0 },
  { "ip2022ext", "ip2022ext", MACH_IP2022EXT, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY ip2k_cgen_opval_register_names_entries[] =
{
  { "ADDRSEL", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "ADDRX", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "IPH", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "IPL", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "SPH", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "SPL", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "PCH", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "PCL", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "WREG", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "STATUS", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "DPH", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "DPL", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "SPDREG", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "MULH", 15, {0, {{{0, 0}}}}, 0, 0 },
  { "ADDRH", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "ADDRL", 17, {0, {{{0, 0}}}}, 0, 0 },
  { "DATAH", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "DATAL", 19, {0, {{{0, 0}}}}, 0, 0 },
  { "INTVECH", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "INTVECL", 21, {0, {{{0, 0}}}}, 0, 0 },
  { "INTSPD", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "INTF", 23, {0, {{{0, 0}}}}, 0, 0 },
  { "INTE", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "INTED", 25, {0, {{{0, 0}}}}, 0, 0 },
  { "FCFG", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "TCTRL", 27, {0, {{{0, 0}}}}, 0, 0 },
  { "XCFG", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "EMCFG", 29, {0, {{{0, 0}}}}, 0, 0 },
  { "IPCH", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "IPCL", 31, {0, {{{0, 0}}}}, 0, 0 },
  { "RAIN", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "RAOUT", 33, {0, {{{0, 0}}}}, 0, 0 },
  { "RADIR", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "LFSRH", 35, {0, {{{0, 0}}}}, 0, 0 },
  { "RBIN", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "RBOUT", 37, {0, {{{0, 0}}}}, 0, 0 },
  { "RBDIR", 38, {0, {{{0, 0}}}}, 0, 0 },
  { "LFSRL", 39, {0, {{{0, 0}}}}, 0, 0 },
  { "RCIN", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "RCOUT", 41, {0, {{{0, 0}}}}, 0, 0 },
  { "RCDIR", 42, {0, {{{0, 0}}}}, 0, 0 },
  { "LFSRA", 43, {0, {{{0, 0}}}}, 0, 0 },
  { "RDIN", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "RDOUT", 45, {0, {{{0, 0}}}}, 0, 0 },
  { "RDDIR", 46, {0, {{{0, 0}}}}, 0, 0 },
  { "REIN", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "REOUT", 49, {0, {{{0, 0}}}}, 0, 0 },
  { "REDIR", 50, {0, {{{0, 0}}}}, 0, 0 },
  { "RFIN", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "RFOUT", 53, {0, {{{0, 0}}}}, 0, 0 },
  { "RFDIR", 54, {0, {{{0, 0}}}}, 0, 0 },
  { "RGOUT", 57, {0, {{{0, 0}}}}, 0, 0 },
  { "RGDIR", 58, {0, {{{0, 0}}}}, 0, 0 },
  { "RTTMR", 64, {0, {{{0, 0}}}}, 0, 0 },
  { "RTCFG", 65, {0, {{{0, 0}}}}, 0, 0 },
  { "T0TMR", 66, {0, {{{0, 0}}}}, 0, 0 },
  { "T0CFG", 67, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CNTH", 68, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CNTL", 69, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CAP1H", 70, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CAP1L", 71, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CAP2H", 72, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CMP2H", 72, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CAP2L", 73, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CMP2L", 73, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CMP1H", 74, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CMP1L", 75, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CFG1H", 76, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CFG1L", 77, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CFG2H", 78, {0, {{{0, 0}}}}, 0, 0 },
  { "T1CFG2L", 79, {0, {{{0, 0}}}}, 0, 0 },
  { "ADCH", 80, {0, {{{0, 0}}}}, 0, 0 },
  { "ADCL", 81, {0, {{{0, 0}}}}, 0, 0 },
  { "ADCCFG", 82, {0, {{{0, 0}}}}, 0, 0 },
  { "ADCTMR", 83, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CNTH", 84, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CNTL", 85, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CAP1H", 86, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CAP1L", 87, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CAP2H", 88, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CMP2H", 88, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CAP2L", 89, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CMP2L", 89, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CMP1H", 90, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CMP1L", 91, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CFG1H", 92, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CFG1L", 93, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CFG2H", 94, {0, {{{0, 0}}}}, 0, 0 },
  { "T2CFG2L", 95, {0, {{{0, 0}}}}, 0, 0 },
  { "S1TMRH", 96, {0, {{{0, 0}}}}, 0, 0 },
  { "S1TMRL", 97, {0, {{{0, 0}}}}, 0, 0 },
  { "S1TBUFH", 98, {0, {{{0, 0}}}}, 0, 0 },
  { "S1TBUFL", 99, {0, {{{0, 0}}}}, 0, 0 },
  { "S1TCFG", 100, {0, {{{0, 0}}}}, 0, 0 },
  { "S1RCNT", 101, {0, {{{0, 0}}}}, 0, 0 },
  { "S1RBUFH", 102, {0, {{{0, 0}}}}, 0, 0 },
  { "S1RBUFL", 103, {0, {{{0, 0}}}}, 0, 0 },
  { "S1RCFG", 104, {0, {{{0, 0}}}}, 0, 0 },
  { "S1RSYNC", 105, {0, {{{0, 0}}}}, 0, 0 },
  { "S1INTF", 106, {0, {{{0, 0}}}}, 0, 0 },
  { "S1INTE", 107, {0, {{{0, 0}}}}, 0, 0 },
  { "S1MODE", 108, {0, {{{0, 0}}}}, 0, 0 },
  { "S1SMASK", 109, {0, {{{0, 0}}}}, 0, 0 },
  { "PSPCFG", 110, {0, {{{0, 0}}}}, 0, 0 },
  { "CMPCFG", 111, {0, {{{0, 0}}}}, 0, 0 },
  { "S2TMRH", 112, {0, {{{0, 0}}}}, 0, 0 },
  { "S2TMRL", 113, {0, {{{0, 0}}}}, 0, 0 },
  { "S2TBUFH", 114, {0, {{{0, 0}}}}, 0, 0 },
  { "S2TBUFL", 115, {0, {{{0, 0}}}}, 0, 0 },
  { "S2TCFG", 116, {0, {{{0, 0}}}}, 0, 0 },
  { "S2RCNT", 117, {0, {{{0, 0}}}}, 0, 0 },
  { "S2RBUFH", 118, {0, {{{0, 0}}}}, 0, 0 },
  { "S2RBUFL", 119, {0, {{{0, 0}}}}, 0, 0 },
  { "S2RCFG", 120, {0, {{{0, 0}}}}, 0, 0 },
  { "S2RSYNC", 121, {0, {{{0, 0}}}}, 0, 0 },
  { "S2INTF", 122, {0, {{{0, 0}}}}, 0, 0 },
  { "S2INTE", 123, {0, {{{0, 0}}}}, 0, 0 },
  { "S2MODE", 124, {0, {{{0, 0}}}}, 0, 0 },
  { "S2SMASK", 125, {0, {{{0, 0}}}}, 0, 0 },
  { "CALLH", 126, {0, {{{0, 0}}}}, 0, 0 },
  { "CALLL", 127, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD ip2k_cgen_opval_register_names =
{
  & ip2k_cgen_opval_register_names_entries[0],
  121,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY ip2k_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-spr", HW_H_SPR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-registers", HW_H_REGISTERS, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-stack", HW_H_STACK, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-pabits", HW_H_PABITS, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-zbit", HW_H_ZBIT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-cbit", HW_H_CBIT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-dcbit", HW_H_DCBIT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { { { (1<<MACH_BASE), 0 } } } } },
  { 0, 0, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD ip2k_cgen_ifld_table[] =
{
  { IP2K_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_IMM8, "f-imm8", 0, 16, 7, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_REG, "f-reg", 0, 16, 8, 9, { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_ADDR16CJP, "f-addr16cjp", 0, 16, 12, 13, { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_DIR, "f-dir", 0, 16, 9, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_BITNO, "f-bitno", 0, 16, 11, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP3, "f-op3", 0, 16, 15, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP4, "f-op4", 0, 16, 15, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP4MID, "f-op4mid", 0, 16, 11, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP6, "f-op6", 0, 16, 15, 6, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP8, "f-op8", 0, 16, 15, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP6_10LOW, "f-op6-10low", 0, 16, 9, 10, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_OP6_7LOW, "f-op6-7low", 0, 16, 9, 7, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_RETI3, "f-reti3", 0, 16, 2, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_SKIPB, "f-skipb", 0, 16, 12, 1, { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { IP2K_F_PAGE3, "f-page3", 0, 16, 2, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
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
#define OPERAND(op) IP2K_OPERAND_##op
#else
#define OPERAND(op) IP2K_OPERAND_/**/op
#endif

const CGEN_OPERAND ip2k_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", IP2K_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_NIL] } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* addr16cjp: 13-bit address */
  { "addr16cjp", IP2K_OPERAND_ADDR16CJP, HW_H_UINT, 12, 13,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_ADDR16CJP] } }, 
    { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* fr: register */
  { "fr", IP2K_OPERAND_FR, HW_H_REGISTERS, 8, 9,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_REG] } }, 
    { 0|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* lit8: 8-bit signed literal */
  { "lit8", IP2K_OPERAND_LIT8, HW_H_SINT, 7, 8,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bitno: bit number */
  { "bitno", IP2K_OPERAND_BITNO, HW_H_UINT, 11, 3,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_BITNO] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* addr16p: page number */
  { "addr16p", IP2K_OPERAND_ADDR16P, HW_H_UINT, 2, 3,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_PAGE3] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* addr16h: high 8 bits of address */
  { "addr16h", IP2K_OPERAND_ADDR16H, HW_H_UINT, 7, 8,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* addr16l: low 8 bits of address */
  { "addr16l", IP2K_OPERAND_ADDR16L, HW_H_UINT, 7, 8,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* reti3: reti flags */
  { "reti3", IP2K_OPERAND_RETI3, HW_H_UINT, 2, 3,
    { 0, { (const PTR) &ip2k_cgen_ifld_table[IP2K_F_RETI3] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* pabits: page bits */
  { "pabits", IP2K_OPERAND_PABITS, HW_H_PABITS, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* zbit: zero bit */
  { "zbit", IP2K_OPERAND_ZBIT, HW_H_ZBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* cbit: carry bit */
  { "cbit", IP2K_OPERAND_CBIT, HW_H_CBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* dcbit: digit carry bit */
  { "dcbit", IP2K_OPERAND_DCBIT, HW_H_DCBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
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

static const CGEN_IBASE ip2k_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
/* jmp $addr16cjp */
  {
    IP2K_INSN_JMP, "jmp", "jmp", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* call $addr16cjp */
  {
    IP2K_INSN_CALL, "call", "call", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* sb $fr,$bitno */
  {
    IP2K_INSN_SB, "sb", "sb", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* snb $fr,$bitno */
  {
    IP2K_INSN_SNB, "snb", "snb", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* setb $fr,$bitno */
  {
    IP2K_INSN_SETB, "setb", "setb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* clrb $fr,$bitno */
  {
    IP2K_INSN_CLRB, "clrb", "clrb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor W,#$lit8 */
  {
    IP2K_INSN_XORW_L, "xorw_l", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* and W,#$lit8 */
  {
    IP2K_INSN_ANDW_L, "andw_l", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* or W,#$lit8 */
  {
    IP2K_INSN_ORW_L, "orw_l", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add W,#$lit8 */
  {
    IP2K_INSN_ADDW_L, "addw_l", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub W,#$lit8 */
  {
    IP2K_INSN_SUBW_L, "subw_l", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* cmp W,#$lit8 */
  {
    IP2K_INSN_CMPW_L, "cmpw_l", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* retw #$lit8 */
  {
    IP2K_INSN_RETW_L, "retw_l", "retw", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* cse W,#$lit8 */
  {
    IP2K_INSN_CSEW_L, "csew_l", "cse", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* csne W,#$lit8 */
  {
    IP2K_INSN_CSNEW_L, "csnew_l", "csne", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* push #$lit8 */
  {
    IP2K_INSN_PUSH_L, "push_l", "push", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* muls W,#$lit8 */
  {
    IP2K_INSN_MULSW_L, "mulsw_l", "muls", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mulu W,#$lit8 */
  {
    IP2K_INSN_MULUW_L, "muluw_l", "mulu", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* loadl #$lit8 */
  {
    IP2K_INSN_LOADL_L, "loadl_l", "loadl", 16,
    { 0|A(EXT_SKIP_INSN), { { { (1<<MACH_BASE), 0 } } } }
  },
/* loadh #$lit8 */
  {
    IP2K_INSN_LOADH_L, "loadh_l", "loadh", 16,
    { 0|A(EXT_SKIP_INSN), { { { (1<<MACH_BASE), 0 } } } }
  },
/* loadl $addr16l */
  {
    IP2K_INSN_LOADL_A, "loadl_a", "loadl", 16,
    { 0|A(EXT_SKIP_INSN), { { { (1<<MACH_BASE), 0 } } } }
  },
/* loadh $addr16h */
  {
    IP2K_INSN_LOADH_A, "loadh_a", "loadh", 16,
    { 0|A(EXT_SKIP_INSN), { { { (1<<MACH_BASE), 0 } } } }
  },
/* addc $fr,W */
  {
    IP2K_INSN_ADDCFR_W, "addcfr_w", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* addc W,$fr */
  {
    IP2K_INSN_ADDCW_FR, "addcw_fr", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* incsnz $fr */
  {
    IP2K_INSN_INCSNZ_FR, "incsnz_fr", "incsnz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* incsnz W,$fr */
  {
    IP2K_INSN_INCSNZW_FR, "incsnzw_fr", "incsnz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* muls W,$fr */
  {
    IP2K_INSN_MULSW_FR, "mulsw_fr", "muls", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mulu W,$fr */
  {
    IP2K_INSN_MULUW_FR, "muluw_fr", "mulu", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* decsnz $fr */
  {
    IP2K_INSN_DECSNZ_FR, "decsnz_fr", "decsnz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* decsnz W,$fr */
  {
    IP2K_INSN_DECSNZW_FR, "decsnzw_fr", "decsnz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* subc W,$fr */
  {
    IP2K_INSN_SUBCW_FR, "subcw_fr", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* subc $fr,W */
  {
    IP2K_INSN_SUBCFR_W, "subcfr_w", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* pop $fr */
  {
    IP2K_INSN_POP_FR, "pop_fr", "pop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* push $fr */
  {
    IP2K_INSN_PUSH_FR, "push_fr", "push", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* cse W,$fr */
  {
    IP2K_INSN_CSEW_FR, "csew_fr", "cse", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* csne W,$fr */
  {
    IP2K_INSN_CSNEW_FR, "csnew_fr", "csne", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* incsz $fr */
  {
    IP2K_INSN_INCSZ_FR, "incsz_fr", "incsz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* incsz W,$fr */
  {
    IP2K_INSN_INCSZW_FR, "incszw_fr", "incsz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* swap $fr */
  {
    IP2K_INSN_SWAP_FR, "swap_fr", "swap", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* swap W,$fr */
  {
    IP2K_INSN_SWAPW_FR, "swapw_fr", "swap", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rl $fr */
  {
    IP2K_INSN_RL_FR, "rl_fr", "rl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rl W,$fr */
  {
    IP2K_INSN_RLW_FR, "rlw_fr", "rl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rr $fr */
  {
    IP2K_INSN_RR_FR, "rr_fr", "rr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* rr W,$fr */
  {
    IP2K_INSN_RRW_FR, "rrw_fr", "rr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* decsz $fr */
  {
    IP2K_INSN_DECSZ_FR, "decsz_fr", "decsz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* decsz W,$fr */
  {
    IP2K_INSN_DECSZW_FR, "decszw_fr", "decsz", 16,
    { 0|A(SKIP_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* inc $fr */
  {
    IP2K_INSN_INC_FR, "inc_fr", "inc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* inc W,$fr */
  {
    IP2K_INSN_INCW_FR, "incw_fr", "inc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* not $fr */
  {
    IP2K_INSN_NOT_FR, "not_fr", "not", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* not W,$fr */
  {
    IP2K_INSN_NOTW_FR, "notw_fr", "not", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* test $fr */
  {
    IP2K_INSN_TEST_FR, "test_fr", "test", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov W,#$lit8 */
  {
    IP2K_INSN_MOVW_L, "movw_l", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov $fr,W */
  {
    IP2K_INSN_MOVFR_W, "movfr_w", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* mov W,$fr */
  {
    IP2K_INSN_MOVW_FR, "movw_fr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add $fr,W */
  {
    IP2K_INSN_ADDFR_W, "addfr_w", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* add W,$fr */
  {
    IP2K_INSN_ADDW_FR, "addw_fr", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor $fr,W */
  {
    IP2K_INSN_XORFR_W, "xorfr_w", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* xor W,$fr */
  {
    IP2K_INSN_XORW_FR, "xorw_fr", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* and $fr,W */
  {
    IP2K_INSN_ANDFR_W, "andfr_w", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* and W,$fr */
  {
    IP2K_INSN_ANDW_FR, "andw_fr", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* or $fr,W */
  {
    IP2K_INSN_ORFR_W, "orfr_w", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* or W,$fr */
  {
    IP2K_INSN_ORW_FR, "orw_fr", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* dec $fr */
  {
    IP2K_INSN_DEC_FR, "dec_fr", "dec", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* dec W,$fr */
  {
    IP2K_INSN_DECW_FR, "decw_fr", "dec", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub $fr,W */
  {
    IP2K_INSN_SUBFR_W, "subfr_w", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* sub W,$fr */
  {
    IP2K_INSN_SUBW_FR, "subw_fr", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* clr $fr */
  {
    IP2K_INSN_CLR_FR, "clr_fr", "clr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* cmp W,$fr */
  {
    IP2K_INSN_CMPW_FR, "cmpw_fr", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* speed #$lit8 */
  {
    IP2K_INSN_SPEED, "speed", "speed", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* ireadi */
  {
    IP2K_INSN_IREADI, "ireadi", "ireadi", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* iwritei */
  {
    IP2K_INSN_IWRITEI, "iwritei", "iwritei", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* fread */
  {
    IP2K_INSN_FREAD, "fread", "fread", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* fwrite */
  {
    IP2K_INSN_FWRITE, "fwrite", "fwrite", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* iread */
  {
    IP2K_INSN_IREAD, "iread", "iread", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* iwrite */
  {
    IP2K_INSN_IWRITE, "iwrite", "iwrite", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* page $addr16p */
  {
    IP2K_INSN_PAGE, "page", "page", 16,
    { 0|A(EXT_SKIP_INSN), { { { (1<<MACH_BASE), 0 } } } }
  },
/* system */
  {
    IP2K_INSN_SYSTEM, "system", "system", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* reti #$reti3 */
  {
    IP2K_INSN_RETI, "reti", "reti", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* ret */
  {
    IP2K_INSN_RET, "ret", "ret", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* int */
  {
    IP2K_INSN_INT, "int", "int", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* breakx */
  {
    IP2K_INSN_BREAKX, "breakx", "breakx", 16,
    { 0|A(EXT_SKIP_INSN), { { { (1<<MACH_BASE), 0 } } } }
  },
/* cwdt */
  {
    IP2K_INSN_CWDT, "cwdt", "cwdt", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* ferase */
  {
    IP2K_INSN_FERASE, "ferase", "ferase", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* retnp */
  {
    IP2K_INSN_RETNP, "retnp", "retnp", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } } } }
  },
/* break */
  {
    IP2K_INSN_BREAK, "break", "break", 16,
    { 0, { { { (1<<MACH_BASE), 0 } } } }
  },
/* nop */
  {
    IP2K_INSN_NOP, "nop", "nop", 16,
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
static void ip2k_cgen_rebuild_tables (CGEN_CPU_TABLE *);

/* Subroutine of ip2k_cgen_cpu_open to look up a mach via its bfd name.  */

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

/* Subroutine of ip2k_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & ip2k_cgen_hw_table[0];
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

/* Subroutine of ip2k_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (CGEN_CPU_TABLE *cd)
{
  cd->ifld_table = & ip2k_cgen_ifld_table[0];
}

/* Subroutine of ip2k_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & ip2k_cgen_operand_table[0];
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

/* Subroutine of ip2k_cgen_cpu_open to build the hardware table.
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
  const CGEN_IBASE *ib = & ip2k_cgen_insn_table[0];
  CGEN_INSN *insns = xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of ip2k_cgen_cpu_open to rebuild the tables.  */

static void
ip2k_cgen_rebuild_tables (CGEN_CPU_TABLE *cd)
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
	const CGEN_ISA *isa = & ip2k_cgen_isa_table[i];

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
	const CGEN_MACH *mach = & ip2k_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "ip2k_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
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
ip2k_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
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
	      lookup_mach_via_bfd_name (ip2k_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "ip2k_cgen_cpu_open: unsupported argument `%d'\n",
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
      fprintf (stderr, "ip2k_cgen_cpu_open: no endianness specified\n");
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
  cd->rebuild_tables = ip2k_cgen_rebuild_tables;
  ip2k_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to ip2k_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
ip2k_cgen_cpu_open_1 (const char *mach_name, enum cgen_endian endian)
{
  return ip2k_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
ip2k_cgen_cpu_close (CGEN_CPU_DESC cd)
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

