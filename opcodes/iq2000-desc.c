/* CPU data for iq2000.

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
#include "iq2000-desc.h"
#include "iq2000-opc.h"
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
  { "iq2000", MACH_IQ2000 },
  { "iq10", MACH_IQ10 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] =
{
  { "iq2000", ISA_IQ2000 },
  { "max", ISA_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE iq2000_cgen_ifield_attr_table[] =
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

const CGEN_ATTR_TABLE iq2000_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE iq2000_cgen_operand_attr_table[] =
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

const CGEN_ATTR_TABLE iq2000_cgen_insn_attr_table[] =
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
  { "YIELD-INSN", &bool_attr[0], &bool_attr[0] },
  { "LOAD-DELAY", &bool_attr[0], &bool_attr[0] },
  { "EVEN-REG-NUM", &bool_attr[0], &bool_attr[0] },
  { "UNSUPPORTED", &bool_attr[0], &bool_attr[0] },
  { "USES-RD", &bool_attr[0], &bool_attr[0] },
  { "USES-RS", &bool_attr[0], &bool_attr[0] },
  { "USES-RT", &bool_attr[0], &bool_attr[0] },
  { "USES-R31", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA iq2000_cgen_isa_table[] = {
  { "iq2000", 32, 32, 23, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH iq2000_cgen_mach_table[] = {
  { "iq2000", "iq2000", MACH_IQ2000, 0 },
  { "iq10", "iq10", MACH_IQ10, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY iq2000_cgen_opval_gr_names_entries[] =
{
  { "r0", 0, {0, {0}}, 0, 0 },
  { "%0", 0, {0, {0}}, 0, 0 },
  { "r1", 1, {0, {0}}, 0, 0 },
  { "%1", 1, {0, {0}}, 0, 0 },
  { "r2", 2, {0, {0}}, 0, 0 },
  { "%2", 2, {0, {0}}, 0, 0 },
  { "r3", 3, {0, {0}}, 0, 0 },
  { "%3", 3, {0, {0}}, 0, 0 },
  { "r4", 4, {0, {0}}, 0, 0 },
  { "%4", 4, {0, {0}}, 0, 0 },
  { "r5", 5, {0, {0}}, 0, 0 },
  { "%5", 5, {0, {0}}, 0, 0 },
  { "r6", 6, {0, {0}}, 0, 0 },
  { "%6", 6, {0, {0}}, 0, 0 },
  { "r7", 7, {0, {0}}, 0, 0 },
  { "%7", 7, {0, {0}}, 0, 0 },
  { "r8", 8, {0, {0}}, 0, 0 },
  { "%8", 8, {0, {0}}, 0, 0 },
  { "r9", 9, {0, {0}}, 0, 0 },
  { "%9", 9, {0, {0}}, 0, 0 },
  { "r10", 10, {0, {0}}, 0, 0 },
  { "%10", 10, {0, {0}}, 0, 0 },
  { "r11", 11, {0, {0}}, 0, 0 },
  { "%11", 11, {0, {0}}, 0, 0 },
  { "r12", 12, {0, {0}}, 0, 0 },
  { "%12", 12, {0, {0}}, 0, 0 },
  { "r13", 13, {0, {0}}, 0, 0 },
  { "%13", 13, {0, {0}}, 0, 0 },
  { "r14", 14, {0, {0}}, 0, 0 },
  { "%14", 14, {0, {0}}, 0, 0 },
  { "r15", 15, {0, {0}}, 0, 0 },
  { "%15", 15, {0, {0}}, 0, 0 },
  { "r16", 16, {0, {0}}, 0, 0 },
  { "%16", 16, {0, {0}}, 0, 0 },
  { "r17", 17, {0, {0}}, 0, 0 },
  { "%17", 17, {0, {0}}, 0, 0 },
  { "r18", 18, {0, {0}}, 0, 0 },
  { "%18", 18, {0, {0}}, 0, 0 },
  { "r19", 19, {0, {0}}, 0, 0 },
  { "%19", 19, {0, {0}}, 0, 0 },
  { "r20", 20, {0, {0}}, 0, 0 },
  { "%20", 20, {0, {0}}, 0, 0 },
  { "r21", 21, {0, {0}}, 0, 0 },
  { "%21", 21, {0, {0}}, 0, 0 },
  { "r22", 22, {0, {0}}, 0, 0 },
  { "%22", 22, {0, {0}}, 0, 0 },
  { "r23", 23, {0, {0}}, 0, 0 },
  { "%23", 23, {0, {0}}, 0, 0 },
  { "r24", 24, {0, {0}}, 0, 0 },
  { "%24", 24, {0, {0}}, 0, 0 },
  { "r25", 25, {0, {0}}, 0, 0 },
  { "%25", 25, {0, {0}}, 0, 0 },
  { "r26", 26, {0, {0}}, 0, 0 },
  { "%26", 26, {0, {0}}, 0, 0 },
  { "r27", 27, {0, {0}}, 0, 0 },
  { "%27", 27, {0, {0}}, 0, 0 },
  { "r28", 28, {0, {0}}, 0, 0 },
  { "%28", 28, {0, {0}}, 0, 0 },
  { "r29", 29, {0, {0}}, 0, 0 },
  { "%29", 29, {0, {0}}, 0, 0 },
  { "r30", 30, {0, {0}}, 0, 0 },
  { "%30", 30, {0, {0}}, 0, 0 },
  { "r31", 31, {0, {0}}, 0, 0 },
  { "%31", 31, {0, {0}}, 0, 0 }
};

CGEN_KEYWORD iq2000_cgen_opval_gr_names =
{
  & iq2000_cgen_opval_gr_names_entries[0],
  64,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY iq2000_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { (1<<MACH_BASE) } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { (1<<MACH_BASE) } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & iq2000_cgen_opval_gr_names, { 0, { (1<<MACH_BASE) } } },
  { 0, 0, CGEN_ASM_NONE, 0, {0, {0}} }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD iq2000_cgen_ifld_table[] =
{
  { IQ2000_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_OPCODE, "f-opcode", 0, 32, 31, 6, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_RS, "f-rs", 0, 32, 25, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_RT, "f-rt", 0, 32, 20, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_RD, "f-rd", 0, 32, 15, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_SHAMT, "f-shamt", 0, 32, 10, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CP_OP, "f-cp-op", 0, 32, 10, 3, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CP_OP_10, "f-cp-op-10", 0, 32, 10, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CP_GRP, "f-cp-grp", 0, 32, 7, 2, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_FUNC, "f-func", 0, 32, 5, 6, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_IMM, "f-imm", 0, 32, 15, 16, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_RD_RS, "f-rd-rs", 0, 0, 0, 0,{ 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
  { IQ2000_F_RD_RT, "f-rd-rt", 0, 0, 0, 0,{ 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
  { IQ2000_F_RT_RS, "f-rt-rs", 0, 0, 0, 0,{ 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
  { IQ2000_F_JTARG, "f-jtarg", 0, 32, 15, 16, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { IQ2000_F_JTARGQ10, "f-jtargq10", 0, 32, 20, 21, { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
  { IQ2000_F_OFFSET, "f-offset", 0, 32, 15, 16, { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
  { IQ2000_F_COUNT, "f-count", 0, 32, 15, 7, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_BYTECOUNT, "f-bytecount", 0, 32, 7, 8, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_INDEX, "f-index", 0, 32, 8, 9, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_MASK, "f-mask", 0, 32, 9, 4, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_MASKQ10, "f-maskq10", 0, 32, 10, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_MASKL, "f-maskl", 0, 32, 4, 5, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_EXCODE, "f-excode", 0, 32, 25, 20, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_RSRVD, "f-rsrvd", 0, 32, 25, 10, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_10_11, "f-10-11", 0, 32, 10, 11, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_24_19, "f-24-19", 0, 32, 24, 19, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_5, "f-5", 0, 32, 5, 1, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_10, "f-10", 0, 32, 10, 1, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_25, "f-25", 0, 32, 25, 1, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CAM_Z, "f-cam-z", 0, 32, 5, 3, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CAM_Y, "f-cam-y", 0, 32, 2, 3, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CM_3FUNC, "f-cm-3func", 0, 32, 5, 3, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CM_4FUNC, "f-cm-4func", 0, 32, 5, 4, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CM_3Z, "f-cm-3z", 0, 32, 1, 2, { 0, { (1<<MACH_BASE) } }  },
  { IQ2000_F_CM_4Z, "f-cm-4z", 0, 32, 2, 3, { 0, { (1<<MACH_BASE) } }  },
  { 0, 0, 0, 0, 0, 0, {0, {0}} }
};

#undef A



/* multi ifield declarations */

const CGEN_MAYBE_MULTI_IFLD IQ2000_F_RD_RS_MULTI_IFIELD [];
const CGEN_MAYBE_MULTI_IFLD IQ2000_F_RD_RT_MULTI_IFIELD [];
const CGEN_MAYBE_MULTI_IFLD IQ2000_F_RT_RS_MULTI_IFIELD [];


/* multi ifield definitions */

const CGEN_MAYBE_MULTI_IFLD IQ2000_F_RD_RS_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RD] } },
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RS] } },
    { 0, { (const PTR) 0 } }
};
const CGEN_MAYBE_MULTI_IFLD IQ2000_F_RD_RT_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RD] } },
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RT] } },
    { 0, { (const PTR) 0 } }
};
const CGEN_MAYBE_MULTI_IFLD IQ2000_F_RT_RS_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RT] } },
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RS] } },
    { 0, { (const PTR) 0 } }
};

/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) IQ2000_OPERAND_##op
#else
#define OPERAND(op) IQ2000_OPERAND_/**/op
#endif

const CGEN_OPERAND iq2000_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", IQ2000_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_NIL] } }, 
    { 0|A(SEM_ONLY), { (1<<MACH_BASE) } }  },
/* rs: register Rs */
  { "rs", IQ2000_OPERAND_RS, HW_H_GR, 25, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RS] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rt: register Rt */
  { "rt", IQ2000_OPERAND_RT, HW_H_GR, 20, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RT] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rd: register Rd */
  { "rd", IQ2000_OPERAND_RD, HW_H_GR, 15, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RD] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* rd-rs: register Rd from Rs */
  { "rd-rs", IQ2000_OPERAND_RD_RS, HW_H_GR, 15, 10,
    { 2, { (const PTR) &IQ2000_F_RD_RS_MULTI_IFIELD[0] } }, 
    { 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
/* rd-rt: register Rd from Rt */
  { "rd-rt", IQ2000_OPERAND_RD_RT, HW_H_GR, 15, 10,
    { 2, { (const PTR) &IQ2000_F_RD_RT_MULTI_IFIELD[0] } }, 
    { 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
/* rt-rs: register Rt from Rs */
  { "rt-rs", IQ2000_OPERAND_RT_RS, HW_H_GR, 20, 10,
    { 2, { (const PTR) &IQ2000_F_RT_RS_MULTI_IFIELD[0] } }, 
    { 0|A(VIRTUAL), { (1<<MACH_BASE) } }  },
/* shamt: shift amount */
  { "shamt", IQ2000_OPERAND_SHAMT, HW_H_UINT, 10, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_SHAMT] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* imm: immediate */
  { "imm", IQ2000_OPERAND_IMM, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_IMM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* offset: pc-relative offset */
  { "offset", IQ2000_OPERAND_OFFSET, HW_H_IADDR, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_OFFSET] } }, 
    { 0|A(PCREL_ADDR), { (1<<MACH_BASE) } }  },
/* baseoff: base register offset */
  { "baseoff", IQ2000_OPERAND_BASEOFF, HW_H_IADDR, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_IMM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* jmptarg: jump target */
  { "jmptarg", IQ2000_OPERAND_JMPTARG, HW_H_IADDR, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_JTARG] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
/* mask: mask */
  { "mask", IQ2000_OPERAND_MASK, HW_H_UINT, 9, 4,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_MASK] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* maskq10: iq10 mask */
  { "maskq10", IQ2000_OPERAND_MASKQ10, HW_H_UINT, 10, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_MASKQ10] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* maskl: mask left */
  { "maskl", IQ2000_OPERAND_MASKL, HW_H_UINT, 4, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_MASKL] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* count: count */
  { "count", IQ2000_OPERAND_COUNT, HW_H_UINT, 15, 7,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_COUNT] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* f-index: index */
  { "f-index", IQ2000_OPERAND_F_INDEX, HW_H_UINT, 8, 9,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_INDEX] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* execode: execcode */
  { "execode", IQ2000_OPERAND_EXECODE, HW_H_UINT, 25, 20,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_EXCODE] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* bytecount: byte count */
  { "bytecount", IQ2000_OPERAND_BYTECOUNT, HW_H_UINT, 7, 8,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_BYTECOUNT] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cam-y: cam global opn y */
  { "cam-y", IQ2000_OPERAND_CAM_Y, HW_H_UINT, 2, 3,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_CAM_Y] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cam-z: cam global mask z */
  { "cam-z", IQ2000_OPERAND_CAM_Z, HW_H_UINT, 5, 3,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_CAM_Z] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cm-3func: CM 3 bit fn field */
  { "cm-3func", IQ2000_OPERAND_CM_3FUNC, HW_H_UINT, 5, 3,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_CM_3FUNC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cm-4func: CM 4 bit fn field */
  { "cm-4func", IQ2000_OPERAND_CM_4FUNC, HW_H_UINT, 5, 4,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_CM_4FUNC] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cm-3z: CM 3 bit Z field */
  { "cm-3z", IQ2000_OPERAND_CM_3Z, HW_H_UINT, 1, 2,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_CM_3Z] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* cm-4z: CM 4 bit Z field */
  { "cm-4z", IQ2000_OPERAND_CM_4Z, HW_H_UINT, 2, 3,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_CM_4Z] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* base: base register */
  { "base", IQ2000_OPERAND_BASE, HW_H_GR, 25, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RS] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* maskr: mask right */
  { "maskr", IQ2000_OPERAND_MASKR, HW_H_UINT, 25, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RS] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* bitnum: bit number */
  { "bitnum", IQ2000_OPERAND_BITNUM, HW_H_UINT, 20, 5,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_RT] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* hi16: high 16 bit immediate */
  { "hi16", IQ2000_OPERAND_HI16, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_IMM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* lo16: 16 bit signed immediate, for low */
  { "lo16", IQ2000_OPERAND_LO16, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_IMM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* mlo16: negated 16 bit signed immediate */
  { "mlo16", IQ2000_OPERAND_MLO16, HW_H_UINT, 15, 16,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_IMM] } }, 
    { 0, { (1<<MACH_BASE) } }  },
/* jmptargq10: iq10 21-bit jump offset */
  { "jmptargq10", IQ2000_OPERAND_JMPTARGQ10, HW_H_IADDR, 20, 21,
    { 0, { (const PTR) &iq2000_cgen_ifld_table[IQ2000_F_JTARGQ10] } }, 
    { 0|A(ABS_ADDR), { (1<<MACH_BASE) } }  },
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

static const CGEN_IBASE iq2000_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, {0, {0}} },
/* add ${rd-rs},$rt */
  {
    -1, "add2", "add", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* add $rd,$rs,$rt */
  {
    IQ2000_INSN_ADD, "add", "add", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* addi ${rt-rs},$lo16 */
  {
    -1, "addi2", "addi", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* addi $rt,$rs,$lo16 */
  {
    IQ2000_INSN_ADDI, "addi", "addi", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* addiu ${rt-rs},$lo16 */
  {
    -1, "addiu2", "addiu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* addiu $rt,$rs,$lo16 */
  {
    IQ2000_INSN_ADDIU, "addiu", "addiu", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* addu ${rd-rs},$rt */
  {
    -1, "addu2", "addu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* addu $rd,$rs,$rt */
  {
    IQ2000_INSN_ADDU, "addu", "addu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* ado16 ${rd-rs},$rt */
  {
    -1, "ado162", "ado16", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* ado16 $rd,$rs,$rt */
  {
    IQ2000_INSN_ADO16, "ado16", "ado16", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* and ${rd-rs},$rt */
  {
    -1, "and2", "and", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* and $rd,$rs,$rt */
  {
    IQ2000_INSN_AND, "and", "and", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* andi ${rt-rs},$lo16 */
  {
    -1, "andi2", "andi", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* andi $rt,$rs,$lo16 */
  {
    IQ2000_INSN_ANDI, "andi", "andi", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* andoi ${rt-rs},$lo16 */
  {
    -1, "andoi2", "andoi", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* andoi $rt,$rs,$lo16 */
  {
    IQ2000_INSN_ANDOI, "andoi", "andoi", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* nor ${rd-rs},$rt */
  {
    -1, "nor2", "nor", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* nor $rd,$rs,$rt */
  {
    IQ2000_INSN_NOR, "nor", "nor", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* or ${rd-rs},$rt */
  {
    -1, "or2", "or", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* or $rd,$rs,$rt */
  {
    IQ2000_INSN_OR, "or", "or", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* ori ${rt-rs},$lo16 */
  {
    -1, "ori2", "ori", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* ori $rt,$rs,$lo16 */
  {
    IQ2000_INSN_ORI, "ori", "ori", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* ram $rd,$rt,$shamt,$maskl,$maskr */
  {
    IQ2000_INSN_RAM, "ram", "ram", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* sll $rd,$rt,$shamt */
  {
    IQ2000_INSN_SLL, "sll", "sll", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* sllv ${rd-rt},$rs */
  {
    -1, "sllv2", "sllv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* sllv $rd,$rt,$rs */
  {
    IQ2000_INSN_SLLV, "sllv", "sllv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* slmv ${rd-rt},$rs,$shamt */
  {
    -1, "slmv2", "slmv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* slmv $rd,$rt,$rs,$shamt */
  {
    IQ2000_INSN_SLMV, "slmv", "slmv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* slt ${rd-rs},$rt */
  {
    -1, "slt2", "slt", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* slt $rd,$rs,$rt */
  {
    IQ2000_INSN_SLT, "slt", "slt", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* slti ${rt-rs},$imm */
  {
    -1, "slti2", "slti", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* slti $rt,$rs,$imm */
  {
    IQ2000_INSN_SLTI, "slti", "slti", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* sltiu ${rt-rs},$imm */
  {
    -1, "sltiu2", "sltiu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* sltiu $rt,$rs,$imm */
  {
    IQ2000_INSN_SLTIU, "sltiu", "sltiu", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* sltu ${rd-rs},$rt */
  {
    -1, "sltu2", "sltu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* sltu $rd,$rs,$rt */
  {
    IQ2000_INSN_SLTU, "sltu", "sltu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* sra ${rd-rt},$shamt */
  {
    -1, "sra2", "sra", 32,
    { 0|A(USES_RT)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* sra $rd,$rt,$shamt */
  {
    IQ2000_INSN_SRA, "sra", "sra", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* srav ${rd-rt},$rs */
  {
    -1, "srav2", "srav", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* srav $rd,$rt,$rs */
  {
    IQ2000_INSN_SRAV, "srav", "srav", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* srl $rd,$rt,$shamt */
  {
    IQ2000_INSN_SRL, "srl", "srl", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* srlv ${rd-rt},$rs */
  {
    -1, "srlv2", "srlv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* srlv $rd,$rt,$rs */
  {
    IQ2000_INSN_SRLV, "srlv", "srlv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* srmv ${rd-rt},$rs,$shamt */
  {
    -1, "srmv2", "srmv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* srmv $rd,$rt,$rs,$shamt */
  {
    IQ2000_INSN_SRMV, "srmv", "srmv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* sub ${rd-rs},$rt */
  {
    -1, "sub2", "sub", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* sub $rd,$rs,$rt */
  {
    IQ2000_INSN_SUB, "sub", "sub", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* subu ${rd-rs},$rt */
  {
    -1, "subu2", "subu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* subu $rd,$rs,$rt */
  {
    IQ2000_INSN_SUBU, "subu", "subu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* xor ${rd-rs},$rt */
  {
    -1, "xor2", "xor", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* xor $rd,$rs,$rt */
  {
    IQ2000_INSN_XOR, "xor", "xor", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_BASE) } }
  },
/* xori ${rt-rs},$lo16 */
  {
    -1, "xori2", "xori", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_BASE) } }
  },
/* xori $rt,$rs,$lo16 */
  {
    IQ2000_INSN_XORI, "xori", "xori", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* bbi $rs($bitnum),$offset */
  {
    IQ2000_INSN_BBI, "bbi", "bbi", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bbin $rs($bitnum),$offset */
  {
    IQ2000_INSN_BBIN, "bbin", "bbin", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bbv $rs,$rt,$offset */
  {
    IQ2000_INSN_BBV, "bbv", "bbv", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bbvn $rs,$rt,$offset */
  {
    IQ2000_INSN_BBVN, "bbvn", "bbvn", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beq $rs,$rt,$offset */
  {
    IQ2000_INSN_BEQ, "beq", "beq", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* beql $rs,$rt,$offset */
  {
    IQ2000_INSN_BEQL, "beql", "beql", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bgez $rs,$offset */
  {
    IQ2000_INSN_BGEZ, "bgez", "bgez", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bgezal $rs,$offset */
  {
    IQ2000_INSN_BGEZAL, "bgezal", "bgezal", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bgezall $rs,$offset */
  {
    IQ2000_INSN_BGEZALL, "bgezall", "bgezall", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bgezl $rs,$offset */
  {
    IQ2000_INSN_BGEZL, "bgezl", "bgezl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bltz $rs,$offset */
  {
    IQ2000_INSN_BLTZ, "bltz", "bltz", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bltzl $rs,$offset */
  {
    IQ2000_INSN_BLTZL, "bltzl", "bltzl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bltzal $rs,$offset */
  {
    IQ2000_INSN_BLTZAL, "bltzal", "bltzal", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bltzall $rs,$offset */
  {
    IQ2000_INSN_BLTZALL, "bltzall", "bltzall", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bmb0 $rs,$rt,$offset */
  {
    IQ2000_INSN_BMB0, "bmb0", "bmb0", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bmb1 $rs,$rt,$offset */
  {
    IQ2000_INSN_BMB1, "bmb1", "bmb1", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bmb2 $rs,$rt,$offset */
  {
    IQ2000_INSN_BMB2, "bmb2", "bmb2", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bmb3 $rs,$rt,$offset */
  {
    IQ2000_INSN_BMB3, "bmb3", "bmb3", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bne $rs,$rt,$offset */
  {
    IQ2000_INSN_BNE, "bne", "bne", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* bnel $rs,$rt,$offset */
  {
    IQ2000_INSN_BNEL, "bnel", "bnel", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* jalr $rd,$rs */
  {
    IQ2000_INSN_JALR, "jalr", "jalr", 32,
    { 0|A(USES_RS)|A(USES_RD)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* jr $rs */
  {
    IQ2000_INSN_JR, "jr", "jr", 32,
    { 0|A(USES_RS)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_BASE) } }
  },
/* lb $rt,$lo16($base) */
  {
    IQ2000_INSN_LB, "lb", "lb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(LOAD_DELAY), { (1<<MACH_BASE) } }
  },
/* lbu $rt,$lo16($base) */
  {
    IQ2000_INSN_LBU, "lbu", "lbu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(LOAD_DELAY), { (1<<MACH_BASE) } }
  },
/* lh $rt,$lo16($base) */
  {
    IQ2000_INSN_LH, "lh", "lh", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(LOAD_DELAY), { (1<<MACH_BASE) } }
  },
/* lhu $rt,$lo16($base) */
  {
    IQ2000_INSN_LHU, "lhu", "lhu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(LOAD_DELAY), { (1<<MACH_BASE) } }
  },
/* lui $rt,$hi16 */
  {
    IQ2000_INSN_LUI, "lui", "lui", 32,
    { 0|A(USES_RT), { (1<<MACH_BASE) } }
  },
/* lw $rt,$lo16($base) */
  {
    IQ2000_INSN_LW, "lw", "lw", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(LOAD_DELAY), { (1<<MACH_BASE) } }
  },
/* sb $rt,$lo16($base) */
  {
    IQ2000_INSN_SB, "sb", "sb", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* sh $rt,$lo16($base) */
  {
    IQ2000_INSN_SH, "sh", "sh", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* sw $rt,$lo16($base) */
  {
    IQ2000_INSN_SW, "sw", "sw", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_BASE) } }
  },
/* break */
  {
    IQ2000_INSN_BREAK, "break", "break", 32,
    { 0, { (1<<MACH_BASE) } }
  },
/* syscall */
  {
    IQ2000_INSN_SYSCALL, "syscall", "syscall", 32,
    { 0|A(YIELD_INSN), { (1<<MACH_BASE) } }
  },
/* andoui $rt,$rs,$hi16 */
  {
    IQ2000_INSN_ANDOUI, "andoui", "andoui", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ2000) } }
  },
/* andoui ${rt-rs},$hi16 */
  {
    -1, "andoui2", "andoui", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_IQ2000) } }
  },
/* orui ${rt-rs},$hi16 */
  {
    -1, "orui2", "orui", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_IQ2000) } }
  },
/* orui $rt,$rs,$hi16 */
  {
    IQ2000_INSN_ORUI, "orui", "orui", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ2000) } }
  },
/* bgtz $rs,$offset */
  {
    IQ2000_INSN_BGTZ, "bgtz", "bgtz", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bgtzl $rs,$offset */
  {
    IQ2000_INSN_BGTZL, "bgtzl", "bgtzl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* blez $rs,$offset */
  {
    IQ2000_INSN_BLEZ, "blez", "blez", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* blezl $rs,$offset */
  {
    IQ2000_INSN_BLEZL, "blezl", "blezl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* mrgb $rd,$rs,$rt,$mask */
  {
    IQ2000_INSN_MRGB, "mrgb", "mrgb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* mrgb ${rd-rs},$rt,$mask */
  {
    -1, "mrgb2", "mrgb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_IQ2000) } }
  },
/* bctxt $rs,$offset */
  {
    IQ2000_INSN_BCTXT, "bctxt", "bctxt", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc0f $offset */
  {
    IQ2000_INSN_BC0F, "bc0f", "bc0f", 32,
    { 0|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc0fl $offset */
  {
    IQ2000_INSN_BC0FL, "bc0fl", "bc0fl", 32,
    { 0|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc3f $offset */
  {
    IQ2000_INSN_BC3F, "bc3f", "bc3f", 32,
    { 0|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc3fl $offset */
  {
    IQ2000_INSN_BC3FL, "bc3fl", "bc3fl", 32,
    { 0|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc0t $offset */
  {
    IQ2000_INSN_BC0T, "bc0t", "bc0t", 32,
    { 0|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc0tl $offset */
  {
    IQ2000_INSN_BC0TL, "bc0tl", "bc0tl", 32,
    { 0|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc3t $offset */
  {
    IQ2000_INSN_BC3T, "bc3t", "bc3t", 32,
    { 0|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bc3tl $offset */
  {
    IQ2000_INSN_BC3TL, "bc3tl", "bc3tl", 32,
    { 0|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* cfc0 $rt,$rd */
  {
    IQ2000_INSN_CFC0, "cfc0", "cfc0", 32,
    { 0|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* cfc1 $rt,$rd */
  {
    IQ2000_INSN_CFC1, "cfc1", "cfc1", 32,
    { 0|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* cfc2 $rt,$rd */
  {
    IQ2000_INSN_CFC2, "cfc2", "cfc2", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* cfc3 $rt,$rd */
  {
    IQ2000_INSN_CFC3, "cfc3", "cfc3", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* chkhdr $rd,$rt */
  {
    IQ2000_INSN_CHKHDR, "chkhdr", "chkhdr", 32,
    { 0|A(YIELD_INSN)|A(USES_RD)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* ctc0 $rt,$rd */
  {
    IQ2000_INSN_CTC0, "ctc0", "ctc0", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* ctc1 $rt,$rd */
  {
    IQ2000_INSN_CTC1, "ctc1", "ctc1", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* ctc2 $rt,$rd */
  {
    IQ2000_INSN_CTC2, "ctc2", "ctc2", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* ctc3 $rt,$rd */
  {
    IQ2000_INSN_CTC3, "ctc3", "ctc3", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* jcr $rs */
  {
    IQ2000_INSN_JCR, "jcr", "jcr", 32,
    { 0|A(USES_RS)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* luc32 $rt,$rd */
  {
    IQ2000_INSN_LUC32, "luc32", "luc32", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* luc32l $rt,$rd */
  {
    IQ2000_INSN_LUC32L, "luc32l", "luc32l", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* luc64 $rt,$rd */
  {
    IQ2000_INSN_LUC64, "luc64", "luc64", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* luc64l $rt,$rd */
  {
    IQ2000_INSN_LUC64L, "luc64l", "luc64l", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* luk $rt,$rd */
  {
    IQ2000_INSN_LUK, "luk", "luk", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* lulck $rt */
  {
    IQ2000_INSN_LULCK, "lulck", "lulck", 32,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* lum32 $rt,$rd */
  {
    IQ2000_INSN_LUM32, "lum32", "lum32", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* lum32l $rt,$rd */
  {
    IQ2000_INSN_LUM32L, "lum32l", "lum32l", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* lum64 $rt,$rd */
  {
    IQ2000_INSN_LUM64, "lum64", "lum64", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* lum64l $rt,$rd */
  {
    IQ2000_INSN_LUM64L, "lum64l", "lum64l", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* lur $rt,$rd */
  {
    IQ2000_INSN_LUR, "lur", "lur", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* lurl $rt,$rd */
  {
    IQ2000_INSN_LURL, "lurl", "lurl", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* luulck $rt */
  {
    IQ2000_INSN_LUULCK, "luulck", "luulck", 32,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* mfc0 $rt,$rd */
  {
    IQ2000_INSN_MFC0, "mfc0", "mfc0", 32,
    { 0|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* mfc1 $rt,$rd */
  {
    IQ2000_INSN_MFC1, "mfc1", "mfc1", 32,
    { 0|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* mfc2 $rt,$rd */
  {
    IQ2000_INSN_MFC2, "mfc2", "mfc2", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* mfc3 $rt,$rd */
  {
    IQ2000_INSN_MFC3, "mfc3", "mfc3", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(LOAD_DELAY), { (1<<MACH_IQ2000) } }
  },
/* mtc0 $rt,$rd */
  {
    IQ2000_INSN_MTC0, "mtc0", "mtc0", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* mtc1 $rt,$rd */
  {
    IQ2000_INSN_MTC1, "mtc1", "mtc1", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* mtc2 $rt,$rd */
  {
    IQ2000_INSN_MTC2, "mtc2", "mtc2", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* mtc3 $rt,$rd */
  {
    IQ2000_INSN_MTC3, "mtc3", "mtc3", 32,
    { 0|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* pkrl $rd,$rt */
  {
    IQ2000_INSN_PKRL, "pkrl", "pkrl", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* pkrlr1 $rt,$count */
  {
    IQ2000_INSN_PKRLR1, "pkrlr1", "pkrlr1", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* pkrlr30 $rt,$count */
  {
    IQ2000_INSN_PKRLR30, "pkrlr30", "pkrlr30", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* rb $rd,$rt */
  {
    IQ2000_INSN_RB, "rb", "rb", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* rbr1 $rt,$count */
  {
    IQ2000_INSN_RBR1, "rbr1", "rbr1", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* rbr30 $rt,$count */
  {
    IQ2000_INSN_RBR30, "rbr30", "rbr30", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* rfe */
  {
    IQ2000_INSN_RFE, "rfe", "rfe", 32,
    { 0, { (1<<MACH_IQ2000) } }
  },
/* rx $rd,$rt */
  {
    IQ2000_INSN_RX, "rx", "rx", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* rxr1 $rt,$count */
  {
    IQ2000_INSN_RXR1, "rxr1", "rxr1", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* rxr30 $rt,$count */
  {
    IQ2000_INSN_RXR30, "rxr30", "rxr30", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* sleep */
  {
    IQ2000_INSN_SLEEP, "sleep", "sleep", 32,
    { 0|A(YIELD_INSN), { (1<<MACH_IQ2000) } }
  },
/* srrd $rt */
  {
    IQ2000_INSN_SRRD, "srrd", "srrd", 32,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* srrdl $rt */
  {
    IQ2000_INSN_SRRDL, "srrdl", "srrdl", 32,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* srulck $rt */
  {
    IQ2000_INSN_SRULCK, "srulck", "srulck", 32,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* srwr $rt,$rd */
  {
    IQ2000_INSN_SRWR, "srwr", "srwr", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* srwru $rt,$rd */
  {
    IQ2000_INSN_SRWRU, "srwru", "srwru", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* trapqfl */
  {
    IQ2000_INSN_TRAPQFL, "trapqfl", "trapqfl", 32,
    { 0|A(YIELD_INSN), { (1<<MACH_IQ2000) } }
  },
/* trapqne */
  {
    IQ2000_INSN_TRAPQNE, "trapqne", "trapqne", 32,
    { 0|A(YIELD_INSN), { (1<<MACH_IQ2000) } }
  },
/* traprel $rt */
  {
    IQ2000_INSN_TRAPREL, "traprel", "traprel", 32,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wb $rd,$rt */
  {
    IQ2000_INSN_WB, "wb", "wb", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* wbu $rd,$rt */
  {
    IQ2000_INSN_WBU, "wbu", "wbu", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* wbr1 $rt,$count */
  {
    IQ2000_INSN_WBR1, "wbr1", "wbr1", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wbr1u $rt,$count */
  {
    IQ2000_INSN_WBR1U, "wbr1u", "wbr1u", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wbr30 $rt,$count */
  {
    IQ2000_INSN_WBR30, "wbr30", "wbr30", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wbr30u $rt,$count */
  {
    IQ2000_INSN_WBR30U, "wbr30u", "wbr30u", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wx $rd,$rt */
  {
    IQ2000_INSN_WX, "wx", "wx", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* wxu $rd,$rt */
  {
    IQ2000_INSN_WXU, "wxu", "wxu", 32,
    { 0|A(YIELD_INSN)|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ2000) } }
  },
/* wxr1 $rt,$count */
  {
    IQ2000_INSN_WXR1, "wxr1", "wxr1", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wxr1u $rt,$count */
  {
    IQ2000_INSN_WXR1U, "wxr1u", "wxr1u", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wxr30 $rt,$count */
  {
    IQ2000_INSN_WXR30, "wxr30", "wxr30", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* wxr30u $rt,$count */
  {
    IQ2000_INSN_WXR30U, "wxr30u", "wxr30u", 23,
    { 0|A(YIELD_INSN)|A(USES_RT), { (1<<MACH_IQ2000) } }
  },
/* ldw $rt,$lo16($base) */
  {
    IQ2000_INSN_LDW, "ldw", "ldw", 32,
    { 0|A(USES_RT)|A(LOAD_DELAY)|A(EVEN_REG_NUM), { (1<<MACH_IQ2000) } }
  },
/* sdw $rt,$lo16($base) */
  {
    IQ2000_INSN_SDW, "sdw", "sdw", 32,
    { 0|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ2000) } }
  },
/* j $jmptarg */
  {
    IQ2000_INSN_J, "j", "j", 32,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* jal $jmptarg */
  {
    IQ2000_INSN_JAL, "jal", "jal", 32,
    { 0|A(USES_R31)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* bmb $rs,$rt,$offset */
  {
    IQ2000_INSN_BMB, "bmb", "bmb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ2000) } }
  },
/* andoui $rt,$rs,$hi16 */
  {
    IQ2000_INSN_ANDOUI_Q10, "andoui-q10", "andoui", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* andoui ${rt-rs},$hi16 */
  {
    -1, "andoui2-q10", "andoui", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_IQ10) } }
  },
/* orui $rt,$rs,$hi16 */
  {
    IQ2000_INSN_ORUI_Q10, "orui-q10", "orui", 32,
    { 0|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* orui ${rt-rs},$hi16 */
  {
    -1, "orui2-q10", "orui", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(NO_DIS)|A(ALIAS), { (1<<MACH_IQ10) } }
  },
/* mrgb $rd,$rs,$rt,$maskq10 */
  {
    IQ2000_INSN_MRGBQ10, "mrgbq10", "mrgb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* mrgb ${rd-rs},$rt,$maskq10 */
  {
    -1, "mrgbq102", "mrgb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD)|A(NO_DIS)|A(ALIAS), { (1<<MACH_IQ10) } }
  },
/* j $jmptarg */
  {
    IQ2000_INSN_JQ10, "jq10", "j", 32,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* jal $rt,$jmptarg */
  {
    IQ2000_INSN_JALQ10, "jalq10", "jal", 32,
    { 0|A(USES_RT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* jal $jmptarg */
  {
    IQ2000_INSN_JALQ10_2, "jalq10-2", "jal", 32,
    { 0|A(USES_RT)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bbil $rs($bitnum),$offset */
  {
    IQ2000_INSN_BBIL, "bbil", "bbil", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bbinl $rs($bitnum),$offset */
  {
    IQ2000_INSN_BBINL, "bbinl", "bbinl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bbvl $rs,$rt,$offset */
  {
    IQ2000_INSN_BBVL, "bbvl", "bbvl", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bbvnl $rs,$rt,$offset */
  {
    IQ2000_INSN_BBVNL, "bbvnl", "bbvnl", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bgtzal $rs,$offset */
  {
    IQ2000_INSN_BGTZAL, "bgtzal", "bgtzal", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bgtzall $rs,$offset */
  {
    IQ2000_INSN_BGTZALL, "bgtzall", "bgtzall", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* blezal $rs,$offset */
  {
    IQ2000_INSN_BLEZAL, "blezal", "blezal", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* blezall $rs,$offset */
  {
    IQ2000_INSN_BLEZALL, "blezall", "blezall", 32,
    { 0|A(USES_R31)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bgtz $rs,$offset */
  {
    IQ2000_INSN_BGTZ_Q10, "bgtz-q10", "bgtz", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bgtzl $rs,$offset */
  {
    IQ2000_INSN_BGTZL_Q10, "bgtzl-q10", "bgtzl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* blez $rs,$offset */
  {
    IQ2000_INSN_BLEZ_Q10, "blez-q10", "blez", 32,
    { 0|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* blezl $rs,$offset */
  {
    IQ2000_INSN_BLEZL_Q10, "blezl-q10", "blezl", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bmb $rs,$rt,$offset */
  {
    IQ2000_INSN_BMB_Q10, "bmb-q10", "bmb", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bmbl $rs,$rt,$offset */
  {
    IQ2000_INSN_BMBL, "bmbl", "bmbl", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bri $rs,$offset */
  {
    IQ2000_INSN_BRI, "bri", "bri", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* brv $rs,$offset */
  {
    IQ2000_INSN_BRV, "brv", "brv", 32,
    { 0|A(USES_RS)|A(SKIP_CTI)|A(COND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* bctx $rs,$offset */
  {
    IQ2000_INSN_BCTX, "bctx", "bctx", 32,
    { 0|A(USES_RS)|A(UNCOND_CTI)|A(DELAY_SLOT), { (1<<MACH_IQ10) } }
  },
/* yield */
  {
    IQ2000_INSN_YIELD, "yield", "yield", 32,
    { 0, { (1<<MACH_IQ10) } }
  },
/* crc32 $rd,$rs,$rt */
  {
    IQ2000_INSN_CRC32, "crc32", "crc32", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* crc32b $rd,$rs,$rt */
  {
    IQ2000_INSN_CRC32B, "crc32b", "crc32b", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* cnt1s $rd,$rs */
  {
    IQ2000_INSN_CNT1S, "cnt1s", "cnt1s", 32,
    { 0|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* avail $rd */
  {
    IQ2000_INSN_AVAIL, "avail", "avail", 32,
    { 0|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* free $rd,$rs */
  {
    IQ2000_INSN_FREE, "free", "free", 32,
    { 0|A(USES_RD)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* tstod $rd,$rs */
  {
    IQ2000_INSN_TSTOD, "tstod", "tstod", 32,
    { 0|A(USES_RD)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cmphdr $rd */
  {
    IQ2000_INSN_CMPHDR, "cmphdr", "cmphdr", 32,
    { 0|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* mcid $rd,$rt */
  {
    IQ2000_INSN_MCID, "mcid", "mcid", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* dba $rd */
  {
    IQ2000_INSN_DBA, "dba", "dba", 32,
    { 0|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* dbd $rd,$rs,$rt */
  {
    IQ2000_INSN_DBD, "dbd", "dbd", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* dpwt $rd,$rs */
  {
    IQ2000_INSN_DPWT, "dpwt", "dpwt", 32,
    { 0|A(USES_RD)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* chkhdr $rd,$rs */
  {
    IQ2000_INSN_CHKHDRQ10, "chkhdrq10", "chkhdr", 32,
    { 0|A(USES_RD)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* rba $rd,$rs,$rt */
  {
    IQ2000_INSN_RBA, "rba", "rba", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* rbal $rd,$rs,$rt */
  {
    IQ2000_INSN_RBAL, "rbal", "rbal", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* rbar $rd,$rs,$rt */
  {
    IQ2000_INSN_RBAR, "rbar", "rbar", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* wba $rd,$rs,$rt */
  {
    IQ2000_INSN_WBA, "wba", "wba", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* wbau $rd,$rs,$rt */
  {
    IQ2000_INSN_WBAU, "wbau", "wbau", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* wbac $rd,$rs,$rt */
  {
    IQ2000_INSN_WBAC, "wbac", "wbac", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* rbi $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_RBI, "rbi", "rbi", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* rbil $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_RBIL, "rbil", "rbil", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* rbir $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_RBIR, "rbir", "rbir", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* wbi $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_WBI, "wbi", "wbi", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* wbic $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_WBIC, "wbic", "wbic", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* wbiu $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_WBIU, "wbiu", "wbiu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* pkrli $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_PKRLI, "pkrli", "pkrli", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* pkrlih $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_PKRLIH, "pkrlih", "pkrlih", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* pkrliu $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_PKRLIU, "pkrliu", "pkrliu", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* pkrlic $rd,$rs,$rt,$bytecount */
  {
    IQ2000_INSN_PKRLIC, "pkrlic", "pkrlic", 32,
    { 0|A(USES_RT)|A(USES_RS)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* pkrla $rd,$rs,$rt */
  {
    IQ2000_INSN_PKRLA, "pkrla", "pkrla", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* pkrlau $rd,$rs,$rt */
  {
    IQ2000_INSN_PKRLAU, "pkrlau", "pkrlau", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* pkrlah $rd,$rs,$rt */
  {
    IQ2000_INSN_PKRLAH, "pkrlah", "pkrlah", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* pkrlac $rd,$rs,$rt */
  {
    IQ2000_INSN_PKRLAC, "pkrlac", "pkrlac", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* lock $rd,$rt */
  {
    IQ2000_INSN_LOCK, "lock", "lock", 32,
    { 0|A(USES_RT)|A(USES_RD), { (1<<MACH_IQ10) } }
  },
/* unlk $rd,$rt */
  {
    IQ2000_INSN_UNLK, "unlk", "unlk", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* swrd $rd,$rt */
  {
    IQ2000_INSN_SWRD, "swrd", "swrd", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* swrdl $rd,$rt */
  {
    IQ2000_INSN_SWRDL, "swrdl", "swrdl", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* swwr $rd,$rs,$rt */
  {
    IQ2000_INSN_SWWR, "swwr", "swwr", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* swwru $rd,$rs,$rt */
  {
    IQ2000_INSN_SWWRU, "swwru", "swwru", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* dwrd $rd,$rt */
  {
    IQ2000_INSN_DWRD, "dwrd", "dwrd", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* dwrdl $rd,$rt */
  {
    IQ2000_INSN_DWRDL, "dwrdl", "dwrdl", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cam36 $rd,$rt,${cam-z},${cam-y} */
  {
    IQ2000_INSN_CAM36, "cam36", "cam36", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cam72 $rd,$rt,${cam-y},${cam-z} */
  {
    IQ2000_INSN_CAM72, "cam72", "cam72", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cam144 $rd,$rt,${cam-y},${cam-z} */
  {
    IQ2000_INSN_CAM144, "cam144", "cam144", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cam288 $rd,$rt,${cam-y},${cam-z} */
  {
    IQ2000_INSN_CAM288, "cam288", "cam288", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cm32and $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32AND, "cm32and", "cm32and", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32andn $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32ANDN, "cm32andn", "cm32andn", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32or $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32OR, "cm32or", "cm32or", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32ra $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32RA, "cm32ra", "cm32ra", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32rd $rd,$rt */
  {
    IQ2000_INSN_CM32RD, "cm32rd", "cm32rd", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cm32ri $rd,$rt */
  {
    IQ2000_INSN_CM32RI, "cm32ri", "cm32ri", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cm32rs $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32RS, "cm32rs", "cm32rs", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32sa $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32SA, "cm32sa", "cm32sa", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32sd $rd,$rt */
  {
    IQ2000_INSN_CM32SD, "cm32sd", "cm32sd", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cm32si $rd,$rt */
  {
    IQ2000_INSN_CM32SI, "cm32si", "cm32si", 32,
    { 0|A(USES_RD)|A(USES_RT), { (1<<MACH_IQ10) } }
  },
/* cm32ss $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32SS, "cm32ss", "cm32ss", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm32xor $rd,$rs,$rt */
  {
    IQ2000_INSN_CM32XOR, "cm32xor", "cm32xor", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm64clr $rd,$rt */
  {
    IQ2000_INSN_CM64CLR, "cm64clr", "cm64clr", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64ra $rd,$rs,$rt */
  {
    IQ2000_INSN_CM64RA, "cm64ra", "cm64ra", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64rd $rd,$rt */
  {
    IQ2000_INSN_CM64RD, "cm64rd", "cm64rd", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64ri $rd,$rt */
  {
    IQ2000_INSN_CM64RI, "cm64ri", "cm64ri", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64ria2 $rd,$rs,$rt */
  {
    IQ2000_INSN_CM64RIA2, "cm64ria2", "cm64ria2", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64rs $rd,$rs,$rt */
  {
    IQ2000_INSN_CM64RS, "cm64rs", "cm64rs", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64sa $rd,$rs,$rt */
  {
    IQ2000_INSN_CM64SA, "cm64sa", "cm64sa", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64sd $rd,$rt */
  {
    IQ2000_INSN_CM64SD, "cm64sd", "cm64sd", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64si $rd,$rt */
  {
    IQ2000_INSN_CM64SI, "cm64si", "cm64si", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64sia2 $rd,$rs,$rt */
  {
    IQ2000_INSN_CM64SIA2, "cm64sia2", "cm64sia2", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm64ss $rd,$rs,$rt */
  {
    IQ2000_INSN_CM64SS, "cm64ss", "cm64ss", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm128ria2 $rd,$rs,$rt */
  {
    IQ2000_INSN_CM128RIA2, "cm128ria2", "cm128ria2", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm128ria3 $rd,$rs,$rt,${cm-3z} */
  {
    IQ2000_INSN_CM128RIA3, "cm128ria3", "cm128ria3", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm128ria4 $rd,$rs,$rt,${cm-4z} */
  {
    IQ2000_INSN_CM128RIA4, "cm128ria4", "cm128ria4", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm128sia2 $rd,$rs,$rt */
  {
    IQ2000_INSN_CM128SIA2, "cm128sia2", "cm128sia2", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm128sia3 $rd,$rs,$rt,${cm-3z} */
  {
    IQ2000_INSN_CM128SIA3, "cm128sia3", "cm128sia3", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS)|A(EVEN_REG_NUM), { (1<<MACH_IQ10) } }
  },
/* cm128sia4 $rd,$rs,$rt,${cm-4z} */
  {
    IQ2000_INSN_CM128SIA4, "cm128sia4", "cm128sia4", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cm128vsa $rd,$rs,$rt */
  {
    IQ2000_INSN_CM128VSA, "cm128vsa", "cm128vsa", 32,
    { 0|A(USES_RD)|A(USES_RT)|A(USES_RS), { (1<<MACH_IQ10) } }
  },
/* cfc $rd,$rt */
  {
    IQ2000_INSN_CFC, "cfc", "cfc", 32,
    { 0|A(YIELD_INSN)|A(USES_RD)|A(LOAD_DELAY), { (1<<MACH_IQ10) } }
  },
/* ctc $rs,$rt */
  {
    IQ2000_INSN_CTC, "ctc", "ctc", 32,
    { 0|A(USES_RS), { (1<<MACH_IQ10) } }
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
static void iq2000_cgen_rebuild_tables PARAMS ((CGEN_CPU_TABLE *));

/* Subroutine of iq2000_cgen_cpu_open to look up a mach via its bfd name.  */

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

/* Subroutine of iq2000_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & iq2000_cgen_hw_table[0];
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

/* Subroutine of iq2000_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (cd)
     CGEN_CPU_TABLE *cd;
{
  cd->ifld_table = & iq2000_cgen_ifld_table[0];
}

/* Subroutine of iq2000_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (cd)
     CGEN_CPU_TABLE *cd;
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & iq2000_cgen_operand_table[0];
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

/* Subroutine of iq2000_cgen_cpu_open to build the hardware table.
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
  const CGEN_IBASE *ib = & iq2000_cgen_insn_table[0];
  CGEN_INSN *insns = (CGEN_INSN *) xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of iq2000_cgen_cpu_open to rebuild the tables.  */

static void
iq2000_cgen_rebuild_tables (cd)
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
	const CGEN_ISA *isa = & iq2000_cgen_isa_table[i];

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
	const CGEN_MACH *mach = & iq2000_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "iq2000_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
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
iq2000_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
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
	      lookup_mach_via_bfd_name (iq2000_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "iq2000_cgen_cpu_open: unsupported argument `%d'\n",
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
      fprintf (stderr, "iq2000_cgen_cpu_open: no endianness specified\n");
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
  cd->rebuild_tables = iq2000_cgen_rebuild_tables;
  iq2000_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to iq2000_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
iq2000_cgen_cpu_open_1 (mach_name, endian)
     const char *mach_name;
     enum cgen_endian endian;
{
  return iq2000_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
iq2000_cgen_cpu_close (cd)
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

