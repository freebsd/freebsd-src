/* CPU data for xc16x.

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
#include "xc16x-desc.h"
#include "xc16x-opc.h"
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
  { "xc16x", MACH_XC16X },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] ATTRIBUTE_UNUSED =
{
  { "xc16x", ISA_XC16X },
  { "max", ISA_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY PIPE_attr[] ATTRIBUTE_UNUSED =
{
  { "NONE", PIPE_NONE },
  { "OS", PIPE_OS },
  { 0, 0 }
};

const CGEN_ATTR_TABLE xc16x_cgen_ifield_attr_table[] =
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

const CGEN_ATTR_TABLE xc16x_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE xc16x_cgen_operand_attr_table[] =
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
  { "DOT-PREFIX", &bool_attr[0], &bool_attr[0] },
  { "POF-PREFIX", &bool_attr[0], &bool_attr[0] },
  { "PAG-PREFIX", &bool_attr[0], &bool_attr[0] },
  { "SOF-PREFIX", &bool_attr[0], &bool_attr[0] },
  { "SEG-PREFIX", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE xc16x_cgen_insn_attr_table[] =
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
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA xc16x_cgen_isa_table[] = {
  { "xc16x", 16, 32, 16, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH xc16x_cgen_mach_table[] = {
  { "xc16x", "xc16x", MACH_XC16X, 32 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_gr_names_entries[] =
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
  { "r15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_gr_names =
{
  & xc16x_cgen_opval_gr_names_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_ext_names_entries[] =
{
  { "0x1", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "0x2", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "0x3", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "0x4", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "1", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "2", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "3", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "4", 3, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_ext_names =
{
  & xc16x_cgen_opval_ext_names_entries[0],
  8,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_psw_names_entries[] =
{
  { "IEN", 136, {0, {{{0, 0}}}}, 0, 0 },
  { "r0.11", 240, {0, {{{0, 0}}}}, 0, 0 },
  { "r1.11", 241, {0, {{{0, 0}}}}, 0, 0 },
  { "r2.11", 242, {0, {{{0, 0}}}}, 0, 0 },
  { "r3.11", 243, {0, {{{0, 0}}}}, 0, 0 },
  { "r4.11", 244, {0, {{{0, 0}}}}, 0, 0 },
  { "r5.11", 245, {0, {{{0, 0}}}}, 0, 0 },
  { "r6.11", 246, {0, {{{0, 0}}}}, 0, 0 },
  { "r7.11", 247, {0, {{{0, 0}}}}, 0, 0 },
  { "r8.11", 248, {0, {{{0, 0}}}}, 0, 0 },
  { "r9.11", 249, {0, {{{0, 0}}}}, 0, 0 },
  { "r10.11", 250, {0, {{{0, 0}}}}, 0, 0 },
  { "r11.11", 251, {0, {{{0, 0}}}}, 0, 0 },
  { "r12.11", 252, {0, {{{0, 0}}}}, 0, 0 },
  { "r13.11", 253, {0, {{{0, 0}}}}, 0, 0 },
  { "r14.11", 254, {0, {{{0, 0}}}}, 0, 0 },
  { "r15.11", 255, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_psw_names =
{
  & xc16x_cgen_opval_psw_names_entries[0],
  17,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_grb_names_entries[] =
{
  { "rl0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "rh0", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "rl1", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "rh1", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "rl2", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "rh2", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "rl3", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "rh3", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "rl4", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "rh4", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "rl5", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "rh5", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "rl6", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "rh6", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "rl7", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "rh7", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_grb_names =
{
  & xc16x_cgen_opval_grb_names_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_conditioncode_names_entries[] =
{
  { "cc_UC", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NET", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_Z", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_EQ", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NZ", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NE", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_V", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NV", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_N", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NN", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_ULT", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_UGE", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_C", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NC", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SGT", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SLE", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SLT", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SGE", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_UGT", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_ULE", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_conditioncode_names =
{
  & xc16x_cgen_opval_conditioncode_names_entries[0],
  20,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_extconditioncode_names_entries[] =
{
  { "cc_UC", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NET", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_Z", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_EQ", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NZ", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NE", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_V", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NV", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_N", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NN", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_ULT", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_UGE", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_C", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_NC", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SGT", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SLE", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SLT", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_SGE", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_UGT", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_ULE", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_nusr0", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_nusr1", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_usr0", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "cc_usr1", 7, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_extconditioncode_names =
{
  & xc16x_cgen_opval_extconditioncode_names_entries[0],
  24,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_grb8_names_entries[] =
{
  { "dpp0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 136, {0, {{{0, 0}}}}, 0, 0 },
  { "cp", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "mdl", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "mdh", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "mdc", 135, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "csp", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "vecseg", 137, {0, {{{0, 0}}}}, 0, 0 },
  { "stkov", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "stkun", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon1", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon2", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "zeros", 142, {0, {{{0, 0}}}}, 0, 0 },
  { "ones", 143, {0, {{{0, 0}}}}, 0, 0 },
  { "spseg", 134, {0, {{{0, 0}}}}, 0, 0 },
  { "tfr", 214, {0, {{{0, 0}}}}, 0, 0 },
  { "rl0", 240, {0, {{{0, 0}}}}, 0, 0 },
  { "rh0", 241, {0, {{{0, 0}}}}, 0, 0 },
  { "rl1", 242, {0, {{{0, 0}}}}, 0, 0 },
  { "rh1", 243, {0, {{{0, 0}}}}, 0, 0 },
  { "rl2", 244, {0, {{{0, 0}}}}, 0, 0 },
  { "rh2", 245, {0, {{{0, 0}}}}, 0, 0 },
  { "rl3", 246, {0, {{{0, 0}}}}, 0, 0 },
  { "rh3", 247, {0, {{{0, 0}}}}, 0, 0 },
  { "rl4", 248, {0, {{{0, 0}}}}, 0, 0 },
  { "rh4", 249, {0, {{{0, 0}}}}, 0, 0 },
  { "rl5", 250, {0, {{{0, 0}}}}, 0, 0 },
  { "rh5", 251, {0, {{{0, 0}}}}, 0, 0 },
  { "rl6", 252, {0, {{{0, 0}}}}, 0, 0 },
  { "rh6", 253, {0, {{{0, 0}}}}, 0, 0 },
  { "rl7", 254, {0, {{{0, 0}}}}, 0, 0 },
  { "rh7", 255, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_grb8_names =
{
  & xc16x_cgen_opval_grb8_names_entries[0],
  36,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_r8_names_entries[] =
{
  { "dpp0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 136, {0, {{{0, 0}}}}, 0, 0 },
  { "cp", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "mdl", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "mdh", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "mdc", 135, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "csp", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "vecseg", 137, {0, {{{0, 0}}}}, 0, 0 },
  { "stkov", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "stkun", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon1", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon2", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "zeros", 142, {0, {{{0, 0}}}}, 0, 0 },
  { "ones", 143, {0, {{{0, 0}}}}, 0, 0 },
  { "spseg", 134, {0, {{{0, 0}}}}, 0, 0 },
  { "tfr", 214, {0, {{{0, 0}}}}, 0, 0 },
  { "r0", 240, {0, {{{0, 0}}}}, 0, 0 },
  { "r1", 241, {0, {{{0, 0}}}}, 0, 0 },
  { "r2", 242, {0, {{{0, 0}}}}, 0, 0 },
  { "r3", 243, {0, {{{0, 0}}}}, 0, 0 },
  { "r4", 244, {0, {{{0, 0}}}}, 0, 0 },
  { "r5", 245, {0, {{{0, 0}}}}, 0, 0 },
  { "r6", 246, {0, {{{0, 0}}}}, 0, 0 },
  { "r7", 247, {0, {{{0, 0}}}}, 0, 0 },
  { "r8", 248, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 249, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 250, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 251, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 252, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 253, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 254, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 255, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_r8_names =
{
  & xc16x_cgen_opval_r8_names_entries[0],
  36,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_regmem8_names_entries[] =
{
  { "dpp0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 136, {0, {{{0, 0}}}}, 0, 0 },
  { "cp", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "mdl", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "mdh", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "mdc", 135, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "csp", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "vecseg", 137, {0, {{{0, 0}}}}, 0, 0 },
  { "stkov", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "stkun", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon1", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon2", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "zeros", 142, {0, {{{0, 0}}}}, 0, 0 },
  { "ones", 143, {0, {{{0, 0}}}}, 0, 0 },
  { "spseg", 134, {0, {{{0, 0}}}}, 0, 0 },
  { "tfr", 214, {0, {{{0, 0}}}}, 0, 0 },
  { "r0", 240, {0, {{{0, 0}}}}, 0, 0 },
  { "r1", 241, {0, {{{0, 0}}}}, 0, 0 },
  { "r2", 242, {0, {{{0, 0}}}}, 0, 0 },
  { "r3", 243, {0, {{{0, 0}}}}, 0, 0 },
  { "r4", 244, {0, {{{0, 0}}}}, 0, 0 },
  { "r5", 245, {0, {{{0, 0}}}}, 0, 0 },
  { "r6", 246, {0, {{{0, 0}}}}, 0, 0 },
  { "r7", 247, {0, {{{0, 0}}}}, 0, 0 },
  { "r8", 248, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 249, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 250, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 251, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 252, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 253, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 254, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 255, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_regmem8_names =
{
  & xc16x_cgen_opval_regmem8_names_entries[0],
  36,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_regdiv8_names_entries[] =
{
  { "r0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "r1", 17, {0, {{{0, 0}}}}, 0, 0 },
  { "r2", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "r3", 51, {0, {{{0, 0}}}}, 0, 0 },
  { "r4", 68, {0, {{{0, 0}}}}, 0, 0 },
  { "r5", 85, {0, {{{0, 0}}}}, 0, 0 },
  { "r6", 102, {0, {{{0, 0}}}}, 0, 0 },
  { "r7", 119, {0, {{{0, 0}}}}, 0, 0 },
  { "r8", 136, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 153, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 170, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 187, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 204, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 221, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 238, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 255, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_regdiv8_names =
{
  & xc16x_cgen_opval_regdiv8_names_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_reg0_name_entries[] =
{
  { "0x1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "0x2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "0x3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "0x4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "0x5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "0x6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "0x7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "0x8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "0x9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "0xa", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "0xb", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "0xc", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "0xd", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "0xe", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "0xf", 15, {0, {{{0, 0}}}}, 0, 0 },
  { "1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_reg0_name =
{
  & xc16x_cgen_opval_reg0_name_entries[0],
  30,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_reg0_name1_entries[] =
{
  { "0x1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "0x2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "0x3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "0x4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "0x5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "0x6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "0x7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "7", 7, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_reg0_name1 =
{
  & xc16x_cgen_opval_reg0_name1_entries[0],
  14,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_regbmem8_names_entries[] =
{
  { "dpp0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 136, {0, {{{0, 0}}}}, 0, 0 },
  { "cp", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "mdl", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "mdh", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "mdc", 135, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "csp", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "vecseg", 137, {0, {{{0, 0}}}}, 0, 0 },
  { "stkov", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "stkun", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon1", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon2", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "zeros", 142, {0, {{{0, 0}}}}, 0, 0 },
  { "ones", 143, {0, {{{0, 0}}}}, 0, 0 },
  { "spseg", 134, {0, {{{0, 0}}}}, 0, 0 },
  { "tfr", 214, {0, {{{0, 0}}}}, 0, 0 },
  { "rl0", 240, {0, {{{0, 0}}}}, 0, 0 },
  { "rh0", 241, {0, {{{0, 0}}}}, 0, 0 },
  { "rl1", 242, {0, {{{0, 0}}}}, 0, 0 },
  { "rh1", 243, {0, {{{0, 0}}}}, 0, 0 },
  { "rl2", 244, {0, {{{0, 0}}}}, 0, 0 },
  { "rh2", 245, {0, {{{0, 0}}}}, 0, 0 },
  { "rl3", 246, {0, {{{0, 0}}}}, 0, 0 },
  { "rh3", 247, {0, {{{0, 0}}}}, 0, 0 },
  { "rl4", 248, {0, {{{0, 0}}}}, 0, 0 },
  { "rh4", 249, {0, {{{0, 0}}}}, 0, 0 },
  { "rl5", 250, {0, {{{0, 0}}}}, 0, 0 },
  { "rh5", 251, {0, {{{0, 0}}}}, 0, 0 },
  { "rl6", 252, {0, {{{0, 0}}}}, 0, 0 },
  { "rh6", 253, {0, {{{0, 0}}}}, 0, 0 },
  { "rl7", 254, {0, {{{0, 0}}}}, 0, 0 },
  { "rh7", 255, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_regbmem8_names =
{
  & xc16x_cgen_opval_regbmem8_names_entries[0],
  36,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY xc16x_cgen_opval_memgr8_names_entries[] =
{
  { "dpp0", 65024, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp1", 65026, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp2", 65028, {0, {{{0, 0}}}}, 0, 0 },
  { "dpp3", 65030, {0, {{{0, 0}}}}, 0, 0 },
  { "psw", 65296, {0, {{{0, 0}}}}, 0, 0 },
  { "cp", 65040, {0, {{{0, 0}}}}, 0, 0 },
  { "mdl", 65038, {0, {{{0, 0}}}}, 0, 0 },
  { "mdh", 65036, {0, {{{0, 0}}}}, 0, 0 },
  { "mdc", 65294, {0, {{{0, 0}}}}, 0, 0 },
  { "sp", 65042, {0, {{{0, 0}}}}, 0, 0 },
  { "csp", 65032, {0, {{{0, 0}}}}, 0, 0 },
  { "vecseg", 65298, {0, {{{0, 0}}}}, 0, 0 },
  { "stkov", 65044, {0, {{{0, 0}}}}, 0, 0 },
  { "stkun", 65046, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon1", 65048, {0, {{{0, 0}}}}, 0, 0 },
  { "cpucon2", 65050, {0, {{{0, 0}}}}, 0, 0 },
  { "zeros", 65308, {0, {{{0, 0}}}}, 0, 0 },
  { "ones", 65310, {0, {{{0, 0}}}}, 0, 0 },
  { "spseg", 65292, {0, {{{0, 0}}}}, 0, 0 },
  { "tfr", 65452, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD xc16x_cgen_opval_memgr8_names =
{
  & xc16x_cgen_opval_memgr8_names_entries[0],
  20,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY xc16x_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PC), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_gr_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-ext", HW_H_EXT, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_ext_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-psw", HW_H_PSW, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_psw_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-grb", HW_H_GRB, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_grb_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-cc", HW_H_CC, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_conditioncode_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-ecc", HW_H_ECC, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_extconditioncode_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-grb8", HW_H_GRB8, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_grb8_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-r8", HW_H_R8, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_r8_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-regmem8", HW_H_REGMEM8, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_regmem8_names, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-regdiv8", HW_H_REGDIV8, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_regdiv8_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-r0", HW_H_R0, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_reg0_name, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-r01", HW_H_R01, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_reg0_name1, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-regbmem8", HW_H_REGBMEM8, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_regbmem8_names, { 0|A(CACHE_ADDR)|A(PROFILE), { { { (1<<MACH_BASE), 0 } } } } },
  { "h-memgr8", HW_H_MEMGR8, CGEN_ASM_KEYWORD, (PTR) & xc16x_cgen_opval_memgr8_names, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-cond", HW_H_COND, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-cbit", HW_H_CBIT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { "h-sgtdis", HW_H_SGTDIS, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } },
  { 0, 0, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } } } } }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD xc16x_cgen_ifld_table[] =
{
  { XC16X_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP1, "f-op1", 0, 32, 7, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP2, "f-op2", 0, 32, 3, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_CONDCODE, "f-condcode", 0, 32, 7, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_ICONDCODE, "f-icondcode", 0, 32, 15, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_RCOND, "f-rcond", 0, 32, 7, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_QCOND, "f-qcond", 0, 32, 7, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_EXTCCODE, "f-extccode", 0, 32, 15, 5, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_R0, "f-r0", 0, 32, 9, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_R1, "f-r1", 0, 32, 15, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_R2, "f-r2", 0, 32, 11, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_R3, "f-r3", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_R4, "f-r4", 0, 32, 11, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_UIMM2, "f-uimm2", 0, 32, 13, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_UIMM3, "f-uimm3", 0, 32, 10, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_UIMM4, "f-uimm4", 0, 32, 15, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_UIMM7, "f-uimm7", 0, 32, 15, 7, { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_UIMM8, "f-uimm8", 0, 32, 23, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_UIMM16, "f-uimm16", 0, 32, 31, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_MEMORY, "f-memory", 0, 32, 31, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_MEMGR8, "f-memgr8", 0, 32, 31, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_REL8, "f-rel8", 0, 32, 15, 8, { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_RELHI8, "f-relhi8", 0, 32, 23, 8, { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_REG8, "f-reg8", 0, 32, 15, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_REGMEM8, "f-regmem8", 0, 32, 15, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_REGOFF8, "f-regoff8", 0, 32, 15, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_REGHI8, "f-reghi8", 0, 32, 23, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_REGB8, "f-regb8", 0, 32, 15, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_SEG8, "f-seg8", 0, 32, 15, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_SEGNUM8, "f-segnum8", 0, 32, 23, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_MASK8, "f-mask8", 0, 32, 23, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_PAGENUM, "f-pagenum", 0, 32, 25, 10, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_DATAHI8, "f-datahi8", 0, 32, 31, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_DATA8, "f-data8", 0, 32, 23, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OFFSET16, "f-offset16", 0, 32, 31, 16, { 0|A(RELOC)|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BIT1, "f-op-bit1", 0, 32, 11, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BIT2, "f-op-bit2", 0, 32, 11, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BIT4, "f-op-bit4", 0, 32, 11, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BIT3, "f-op-bit3", 0, 32, 10, 3, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_2BIT, "f-op-2bit", 0, 32, 10, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BITONE, "f-op-bitone", 0, 32, 10, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_ONEBIT, "f-op-onebit", 0, 32, 9, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_1BIT, "f-op-1bit", 0, 32, 8, 1, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_LBIT4, "f-op-lbit4", 0, 32, 15, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_LBIT2, "f-op-lbit2", 0, 32, 15, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BIT8, "f-op-bit8", 0, 32, 31, 8, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_OP_BIT16, "f-op-bit16", 0, 32, 31, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_QBIT, "f-qbit", 0, 32, 7, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_QLOBIT, "f-qlobit", 0, 32, 31, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_QHIBIT, "f-qhibit", 0, 32, 27, 4, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_QLOBIT2, "f-qlobit2", 0, 32, 27, 2, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
  { XC16X_F_POF, "f-pof", 0, 32, 31, 16, { 0, { { { (1<<MACH_BASE), 0 } } } }  },
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
#define OPERAND(op) XC16X_OPERAND_##op
#else
#define OPERAND(op) XC16X_OPERAND_/**/op
#endif

const CGEN_OPERAND xc16x_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", XC16X_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_NIL] } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* sr: source register */
  { "sr", XC16X_OPERAND_SR, HW_H_GR, 11, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* dr: destination register */
  { "dr", XC16X_OPERAND_DR, HW_H_GR, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* dri: destination register */
  { "dri", XC16X_OPERAND_DRI, HW_H_GR, 11, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* srb: source register */
  { "srb", XC16X_OPERAND_SRB, HW_H_GRB, 11, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* drb: destination register */
  { "drb", XC16X_OPERAND_DRB, HW_H_GRB, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* sr2: 2 bit source register */
  { "sr2", XC16X_OPERAND_SR2, HW_H_GR, 9, 2,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R0] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* src1: source register 1 */
  { "src1", XC16X_OPERAND_SRC1, HW_H_GR, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* src2: source register 2 */
  { "src2", XC16X_OPERAND_SRC2, HW_H_GR, 11, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_R2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* srdiv: source register 2 */
  { "srdiv", XC16X_OPERAND_SRDIV, HW_H_REGDIV8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REG8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* RegNam: PSW bits */
  { "RegNam", XC16X_OPERAND_REGNAM, HW_H_PSW, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REG8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm2: 2 bit unsigned number */
  { "uimm2", XC16X_OPERAND_UIMM2, HW_H_EXT, 13, 2,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM2] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm3: 3 bit unsigned number */
  { "uimm3", XC16X_OPERAND_UIMM3, HW_H_R01, 10, 3,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM3] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm4: 4 bit unsigned number */
  { "uimm4", XC16X_OPERAND_UIMM4, HW_H_UINT, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM4] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm7: 7 bit trap number */
  { "uimm7", XC16X_OPERAND_UIMM7, HW_H_UINT, 15, 7,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM7] } }, 
    { 0|A(HASH_PREFIX)|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm8: 8 bit unsigned immediate */
  { "uimm8", XC16X_OPERAND_UIMM8, HW_H_UINT, 23, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* uimm16: 16 bit unsigned immediate */
  { "uimm16", XC16X_OPERAND_UIMM16, HW_H_UINT, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM16] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* upof16: 16 bit unsigned immediate */
  { "upof16", XC16X_OPERAND_UPOF16, HW_H_ADDR, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_MEMORY] } }, 
    { 0|A(POF_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* reg8: 8 bit word register number */
  { "reg8", XC16X_OPERAND_REG8, HW_H_R8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REG8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* regmem8: 8 bit word register number */
  { "regmem8", XC16X_OPERAND_REGMEM8, HW_H_REGMEM8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REGMEM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* regbmem8: 8 bit byte register number */
  { "regbmem8", XC16X_OPERAND_REGBMEM8, HW_H_REGBMEM8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REGMEM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* regoff8: 8 bit word register number */
  { "regoff8", XC16X_OPERAND_REGOFF8, HW_H_R8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REGOFF8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* reghi8: 8 bit word register number */
  { "reghi8", XC16X_OPERAND_REGHI8, HW_H_R8, 23, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REGHI8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* regb8: 8 bit byte register number */
  { "regb8", XC16X_OPERAND_REGB8, HW_H_GRB8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REGB8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* genreg: 8 bit word register number */
  { "genreg", XC16X_OPERAND_GENREG, HW_H_R8, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REGB8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* seg: 8 bit segment number */
  { "seg", XC16X_OPERAND_SEG, HW_H_UINT, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_SEG8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* seghi8: 8 bit hi segment number */
  { "seghi8", XC16X_OPERAND_SEGHI8, HW_H_UINT, 23, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_SEGNUM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* caddr: 16 bit address offset */
  { "caddr", XC16X_OPERAND_CADDR, HW_H_ADDR, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OFFSET16] } }, 
    { 0|A(RELOC)|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* rel: 8 bit signed relative offset */
  { "rel", XC16X_OPERAND_REL, HW_H_SINT, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_REL8] } }, 
    { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* relhi: hi 8 bit signed relative offset */
  { "relhi", XC16X_OPERAND_RELHI, HW_H_SINT, 23, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_RELHI8] } }, 
    { 0|A(RELOC)|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* condbit: condition bit */
  { "condbit", XC16X_OPERAND_CONDBIT, HW_H_COND, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* bit1: gap of 1 bit */
  { "bit1", XC16X_OPERAND_BIT1, HW_H_UINT, 11, 1,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_BIT1] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bit2: gap of 2 bits */
  { "bit2", XC16X_OPERAND_BIT2, HW_H_UINT, 11, 2,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_BIT2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bit4: gap of 4 bits */
  { "bit4", XC16X_OPERAND_BIT4, HW_H_UINT, 11, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_BIT4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* lbit4: gap of 4 bits */
  { "lbit4", XC16X_OPERAND_LBIT4, HW_H_UINT, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_LBIT4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* lbit2: gap of 2 bits */
  { "lbit2", XC16X_OPERAND_LBIT2, HW_H_UINT, 15, 2,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_LBIT2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bit8: gap of 8 bits */
  { "bit8", XC16X_OPERAND_BIT8, HW_H_UINT, 31, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_BIT8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* u4: gap of 4 bits */
  { "u4", XC16X_OPERAND_U4, HW_H_R0, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bitone: field of 1 bit */
  { "bitone", XC16X_OPERAND_BITONE, HW_H_UINT, 9, 1,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_ONEBIT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* bit01: field of 1 bit */
  { "bit01", XC16X_OPERAND_BIT01, HW_H_UINT, 8, 1,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OP_1BIT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* cond: condition code */
  { "cond", XC16X_OPERAND_COND, HW_H_CC, 7, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_CONDCODE] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* icond: indirect condition code */
  { "icond", XC16X_OPERAND_ICOND, HW_H_CC, 15, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_ICONDCODE] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* extcond: extended condition code */
  { "extcond", XC16X_OPERAND_EXTCOND, HW_H_ECC, 15, 5,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_EXTCCODE] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* memory: 16 bit memory */
  { "memory", XC16X_OPERAND_MEMORY, HW_H_ADDR, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_MEMORY] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* memgr8: 16 bit memory */
  { "memgr8", XC16X_OPERAND_MEMGR8, HW_H_MEMGR8, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_MEMGR8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* cbit: carry bit */
  { "cbit", XC16X_OPERAND_CBIT, HW_H_CBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* qbit: bit addr */
  { "qbit", XC16X_OPERAND_QBIT, HW_H_UINT, 7, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_QBIT] } }, 
    { 0|A(DOT_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* qlobit: bit addr */
  { "qlobit", XC16X_OPERAND_QLOBIT, HW_H_UINT, 31, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_QLOBIT] } }, 
    { 0|A(DOT_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* qhibit: bit addr */
  { "qhibit", XC16X_OPERAND_QHIBIT, HW_H_UINT, 27, 4,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_QHIBIT] } }, 
    { 0|A(DOT_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* mask8: 8 bit mask */
  { "mask8", XC16X_OPERAND_MASK8, HW_H_UINT, 23, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_MASK8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* masklo8: 8 bit mask */
  { "masklo8", XC16X_OPERAND_MASKLO8, HW_H_UINT, 31, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_DATAHI8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* pagenum: 10 bit page number */
  { "pagenum", XC16X_OPERAND_PAGENUM, HW_H_UINT, 25, 10,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_PAGENUM] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* data8: 8 bit data */
  { "data8", XC16X_OPERAND_DATA8, HW_H_UINT, 23, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_DATA8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* datahi8: 8 bit data */
  { "datahi8", XC16X_OPERAND_DATAHI8, HW_H_UINT, 31, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_DATAHI8] } }, 
    { 0|A(HASH_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* sgtdisbit: segmentation enable bit */
  { "sgtdisbit", XC16X_OPERAND_SGTDISBIT, HW_H_SGTDIS, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } } } }  },
/* upag16: 16 bit unsigned immediate */
  { "upag16", XC16X_OPERAND_UPAG16, HW_H_UINT, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_UIMM16] } }, 
    { 0|A(PAG_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* useg8: 8 bit segment  */
  { "useg8", XC16X_OPERAND_USEG8, HW_H_UINT, 15, 8,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_SEG8] } }, 
    { 0|A(SEG_PREFIX), { { { (1<<MACH_BASE), 0 } } } }  },
/* useg16: 16 bit address offset */
  { "useg16", XC16X_OPERAND_USEG16, HW_H_UINT, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OFFSET16] } }, 
    { 0|A(SEG_PREFIX)|A(RELOC)|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* usof16: 16 bit address offset */
  { "usof16", XC16X_OPERAND_USOF16, HW_H_UINT, 31, 16,
    { 0, { (const PTR) &xc16x_cgen_ifld_table[XC16X_F_OFFSET16] } }, 
    { 0|A(SOF_PREFIX)|A(RELOC)|A(ABS_ADDR), { { { (1<<MACH_BASE), 0 } } } }  },
/* hash: # prefix */
  { "hash", XC16X_OPERAND_HASH, HW_H_SINT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* dot: . prefix */
  { "dot", XC16X_OPERAND_DOT, HW_H_SINT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* pof: pof: prefix */
  { "pof", XC16X_OPERAND_POF, HW_H_SINT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* pag: pag: prefix */
  { "pag", XC16X_OPERAND_PAG, HW_H_SINT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* sof: sof: prefix */
  { "sof", XC16X_OPERAND_SOF, HW_H_SINT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } } } }  },
/* segm: seg: prefix */
  { "segm", XC16X_OPERAND_SEGM, HW_H_SINT, 0, 0,
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

static const CGEN_IBASE xc16x_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_NONE, 0 } } } } },
/* add $reg8,$pof$upof16 */
  {
    XC16X_INSN_ADDRPOF, "addrpof", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $reg8,$pof$upof16 */
  {
    XC16X_INSN_SUBRPOF, "subrpof", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regb8,$pof$upof16 */
  {
    XC16X_INSN_ADDBRPOF, "addbrpof", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regb8,$pof$upof16 */
  {
    XC16X_INSN_SUBBRPOF, "subbrpof", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $reg8,$pag$upag16 */
  {
    XC16X_INSN_ADDRPAG, "addrpag", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $reg8,$pag$upag16 */
  {
    XC16X_INSN_SUBRPAG, "subrpag", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regb8,$pag$upag16 */
  {
    XC16X_INSN_ADDBRPAG, "addbrpag", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regb8,$pag$upag16 */
  {
    XC16X_INSN_SUBBRPAG, "subbrpag", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $reg8,$pof$upof16 */
  {
    XC16X_INSN_ADDCRPOF, "addcrpof", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $reg8,$pof$upof16 */
  {
    XC16X_INSN_SUBCRPOF, "subcrpof", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regb8,$pof$upof16 */
  {
    XC16X_INSN_ADDCBRPOF, "addcbrpof", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regb8,$pof$upof16 */
  {
    XC16X_INSN_SUBCBRPOF, "subcbrpof", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $reg8,$pag$upag16 */
  {
    XC16X_INSN_ADDCRPAG, "addcrpag", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $reg8,$pag$upag16 */
  {
    XC16X_INSN_SUBCRPAG, "subcrpag", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regb8,$pag$upag16 */
  {
    XC16X_INSN_ADDCBRPAG, "addcbrpag", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regb8,$pag$upag16 */
  {
    XC16X_INSN_SUBCBRPAG, "subcbrpag", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $pof$upof16,$reg8 */
  {
    XC16X_INSN_ADDRPOFR, "addrpofr", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $pof$upof16,$reg8 */
  {
    XC16X_INSN_SUBRPOFR, "subrpofr", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $pof$upof16,$regb8 */
  {
    XC16X_INSN_ADDBRPOFR, "addbrpofr", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $pof$upof16,$regb8 */
  {
    XC16X_INSN_SUBBRPOFR, "subbrpofr", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $pof$upof16,$reg8 */
  {
    XC16X_INSN_ADDCRPOFR, "addcrpofr", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $pof$upof16,$reg8 */
  {
    XC16X_INSN_SUBCRPOFR, "subcrpofr", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $pof$upof16,$regb8 */
  {
    XC16X_INSN_ADDCBRPOFR, "addcbrpofr", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $pof$upof16,$regb8 */
  {
    XC16X_INSN_SUBCBRPOFR, "subcbrpofr", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $reg8,$hash$pof$uimm16 */
  {
    XC16X_INSN_ADDRHPOF, "addrhpof", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $reg8,$hash$pof$uimm16 */
  {
    XC16X_INSN_SUBRHPOF, "subrhpof", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $reg8,$hash$pag$uimm16 */
  {
    XC16X_INSN_ADDBRHPOF, "addbrhpof", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $reg8,$hash$pag$uimm16 */
  {
    XC16X_INSN_SUBBRHPOF, "subbrhpof", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $dr,$hash$pof$uimm3 */
  {
    XC16X_INSN_ADDRHPOF3, "addrhpof3", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $dr,$hash$pof$uimm3 */
  {
    XC16X_INSN_SUBRHPOF3, "subrhpof3", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $drb,$hash$pag$uimm3 */
  {
    XC16X_INSN_ADDBRHPAG3, "addbrhpag3", "addb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $drb,$hash$pag$uimm3 */
  {
    XC16X_INSN_SUBBRHPAG3, "subbrhpag3", "subb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $dr,$hash$pag$uimm3 */
  {
    XC16X_INSN_ADDRHPAG3, "addrhpag3", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $dr,$hash$pag$uimm3 */
  {
    XC16X_INSN_SUBRHPAG3, "subrhpag3", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $drb,$hash$pof$uimm3 */
  {
    XC16X_INSN_ADDBRHPOF3, "addbrhpof3", "addb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $drb,$hash$pof$uimm3 */
  {
    XC16X_INSN_SUBBRHPOF3, "subbrhpof3", "subb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regb8,$hash$pof$uimm8 */
  {
    XC16X_INSN_ADDRBHPOF, "addrbhpof", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regb8,$hash$pof$uimm8 */
  {
    XC16X_INSN_SUBRBHPOF, "subrbhpof", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regb8,$hash$pag$uimm8 */
  {
    XC16X_INSN_ADDBRHPAG, "addbrhpag", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regb8,$hash$pag$uimm8 */
  {
    XC16X_INSN_SUBBRHPAG, "subbrhpag", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $reg8,$hash$pof$uimm16 */
  {
    XC16X_INSN_ADDCRHPOF, "addcrhpof", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $reg8,$hash$pof$uimm16 */
  {
    XC16X_INSN_SUBCRHPOF, "subcrhpof", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $reg8,$hash$pag$uimm16 */
  {
    XC16X_INSN_ADDCBRHPOF, "addcbrhpof", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $reg8,$hash$pag$uimm16 */
  {
    XC16X_INSN_SUBCBRHPOF, "subcbrhpof", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $dr,$hash$pof$uimm3 */
  {
    XC16X_INSN_ADDCRHPOF3, "addcrhpof3", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $dr,$hash$pof$uimm3 */
  {
    XC16X_INSN_SUBCRHPOF3, "subcrhpof3", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $drb,$hash$pag$uimm3 */
  {
    XC16X_INSN_ADDCBRHPAG3, "addcbrhpag3", "addcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $drb,$hash$pag$uimm3 */
  {
    XC16X_INSN_SUBCBRHPAG3, "subcbrhpag3", "subcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $dr,$hash$pag$uimm3 */
  {
    XC16X_INSN_ADDCRHPAG3, "addcrhpag3", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $dr,$hash$pag$uimm3 */
  {
    XC16X_INSN_SUBCRHPAG3, "subcrhpag3", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $drb,$hash$pof$uimm3 */
  {
    XC16X_INSN_ADDCBRHPOF3, "addcbrhpof3", "addcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $drb,$hash$pof$uimm3 */
  {
    XC16X_INSN_SUBCBRHPOF3, "subcbrhpof3", "subcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regb8,$hash$pof$uimm8 */
  {
    XC16X_INSN_ADDCRBHPOF, "addcrbhpof", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regb8,$hash$pof$uimm8 */
  {
    XC16X_INSN_SUBCRBHPOF, "subcrbhpof", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regb8,$hash$pag$uimm8 */
  {
    XC16X_INSN_ADDCBRHPAG, "addcbrhpag", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regb8,$hash$pag$uimm8 */
  {
    XC16X_INSN_SUBCBRHPAG, "subcbrhpag", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $dr,$hash$uimm3 */
  {
    XC16X_INSN_ADDRI, "addri", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $dr,$hash$uimm3 */
  {
    XC16X_INSN_SUBRI, "subri", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $drb,$hash$uimm3 */
  {
    XC16X_INSN_ADDBRI, "addbri", "addb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $drb,$hash$uimm3 */
  {
    XC16X_INSN_SUBBRI, "subbri", "subb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $reg8,$hash$uimm16 */
  {
    XC16X_INSN_ADDRIM, "addrim", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $reg8,$hash$uimm16 */
  {
    XC16X_INSN_SUBRIM, "subrim", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_ADDBRIM, "addbrim", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_SUBBRIM, "subbrim", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $dr,$hash$uimm3 */
  {
    XC16X_INSN_ADDCRI, "addcri", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $dr,$hash$uimm3 */
  {
    XC16X_INSN_SUBCRI, "subcri", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $drb,$hash$uimm3 */
  {
    XC16X_INSN_ADDCBRI, "addcbri", "addcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $drb,$hash$uimm3 */
  {
    XC16X_INSN_SUBCBRI, "subcbri", "subcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $reg8,$hash$uimm16 */
  {
    XC16X_INSN_ADDCRIM, "addcrim", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $reg8,$hash$uimm16 */
  {
    XC16X_INSN_SUBCRIM, "subcrim", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_ADDCBRIM, "addcbrim", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_SUBCBRIM, "subcbrim", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $dr,$sr */
  {
    XC16X_INSN_ADDR, "addr", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $dr,$sr */
  {
    XC16X_INSN_SUBR, "subr", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $drb,$srb */
  {
    XC16X_INSN_ADDBR, "addbr", "addb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $drb,$srb */
  {
    XC16X_INSN_SUBBR, "subbr", "subb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $dr,[$sr2] */
  {
    XC16X_INSN_ADD2, "add2", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $dr,[$sr2] */
  {
    XC16X_INSN_SUB2, "sub2", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $drb,[$sr2] */
  {
    XC16X_INSN_ADDB2, "addb2", "addb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $drb,[$sr2] */
  {
    XC16X_INSN_SUBB2, "subb2", "subb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $dr,[$sr2+] */
  {
    XC16X_INSN_ADD2I, "add2i", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $dr,[$sr2+] */
  {
    XC16X_INSN_SUB2I, "sub2i", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $drb,[$sr2+] */
  {
    XC16X_INSN_ADDB2I, "addb2i", "addb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $drb,[$sr2+] */
  {
    XC16X_INSN_SUBB2I, "subb2i", "subb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $dr,$sr */
  {
    XC16X_INSN_ADDCR, "addcr", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $dr,$sr */
  {
    XC16X_INSN_SUBCR, "subcr", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $drb,$srb */
  {
    XC16X_INSN_ADDBCR, "addbcr", "addcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $drb,$srb */
  {
    XC16X_INSN_SUBBCR, "subbcr", "subcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $dr,[$sr2] */
  {
    XC16X_INSN_ADDCR2, "addcr2", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $dr,[$sr2] */
  {
    XC16X_INSN_SUBCR2, "subcr2", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $drb,[$sr2] */
  {
    XC16X_INSN_ADDBCR2, "addbcr2", "addcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $drb,[$sr2] */
  {
    XC16X_INSN_SUBBCR2, "subbcr2", "subcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $dr,[$sr2+] */
  {
    XC16X_INSN_ADDCR2I, "addcr2i", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $dr,[$sr2+] */
  {
    XC16X_INSN_SUBCR2I, "subcr2i", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $drb,[$sr2+] */
  {
    XC16X_INSN_ADDBCR2I, "addbcr2i", "addcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $drb,[$sr2+] */
  {
    XC16X_INSN_SUBBCR2I, "subbcr2i", "subcb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $regmem8,$memgr8 */
  {
    XC16X_INSN_ADDRM2, "addrm2", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $memgr8,$regmem8 */
  {
    XC16X_INSN_ADDRM3, "addrm3", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $reg8,$memory */
  {
    XC16X_INSN_ADDRM, "addrm", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* add $memory,$reg8 */
  {
    XC16X_INSN_ADDRM1, "addrm1", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $regmem8,$memgr8 */
  {
    XC16X_INSN_SUBRM3, "subrm3", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $memgr8,$regmem8 */
  {
    XC16X_INSN_SUBRM2, "subrm2", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $reg8,$memory */
  {
    XC16X_INSN_SUBRM1, "subrm1", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sub $memory,$reg8 */
  {
    XC16X_INSN_SUBRM, "subrm", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regbmem8,$memgr8 */
  {
    XC16X_INSN_ADDBRM2, "addbrm2", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $memgr8,$regbmem8 */
  {
    XC16X_INSN_ADDBRM3, "addbrm3", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $regb8,$memory */
  {
    XC16X_INSN_ADDBRM, "addbrm", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addb $memory,$regb8 */
  {
    XC16X_INSN_ADDBRM1, "addbrm1", "addb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regbmem8,$memgr8 */
  {
    XC16X_INSN_SUBBRM3, "subbrm3", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $memgr8,$regbmem8 */
  {
    XC16X_INSN_SUBBRM2, "subbrm2", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $regb8,$memory */
  {
    XC16X_INSN_SUBBRM1, "subbrm1", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subb $memory,$regb8 */
  {
    XC16X_INSN_SUBBRM, "subbrm", "subb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $regmem8,$memgr8 */
  {
    XC16X_INSN_ADDCRM2, "addcrm2", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $memgr8,$regmem8 */
  {
    XC16X_INSN_ADDCRM3, "addcrm3", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $reg8,$memory */
  {
    XC16X_INSN_ADDCRM, "addcrm", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addc $memory,$reg8 */
  {
    XC16X_INSN_ADDCRM1, "addcrm1", "addc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $regmem8,$memgr8 */
  {
    XC16X_INSN_SUBCRM3, "subcrm3", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $memgr8,$regmem8 */
  {
    XC16X_INSN_SUBCRM2, "subcrm2", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $reg8,$memory */
  {
    XC16X_INSN_SUBCRM1, "subcrm1", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subc $memory,$reg8 */
  {
    XC16X_INSN_SUBCRM, "subcrm", "subc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regbmem8,$memgr8 */
  {
    XC16X_INSN_ADDCBRM2, "addcbrm2", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $memgr8,$regbmem8 */
  {
    XC16X_INSN_ADDCBRM3, "addcbrm3", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $regb8,$memory */
  {
    XC16X_INSN_ADDCBRM, "addcbrm", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* addcb $memory,$regb8 */
  {
    XC16X_INSN_ADDCBRM1, "addcbrm1", "addcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regbmem8,$memgr8 */
  {
    XC16X_INSN_SUBCBRM3, "subcbrm3", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $memgr8,$regbmem8 */
  {
    XC16X_INSN_SUBCBRM2, "subcbrm2", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $regb8,$memory */
  {
    XC16X_INSN_SUBCBRM1, "subcbrm1", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* subcb $memory,$regb8 */
  {
    XC16X_INSN_SUBCBRM, "subcbrm", "subcb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mul $src1,$src2 */
  {
    XC16X_INSN_MULS, "muls", "mul", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mulu $src1,$src2 */
  {
    XC16X_INSN_MULU, "mulu", "mulu", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* div $srdiv */
  {
    XC16X_INSN_DIV, "div", "div", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* divl $srdiv */
  {
    XC16X_INSN_DIVL, "divl", "divl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* divlu $srdiv */
  {
    XC16X_INSN_DIVLU, "divlu", "divlu", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* divu $srdiv */
  {
    XC16X_INSN_DIVU, "divu", "divu", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cpl $dr */
  {
    XC16X_INSN_CPL, "cpl", "cpl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cplb $drb */
  {
    XC16X_INSN_CPLB, "cplb", "cplb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* neg $dr */
  {
    XC16X_INSN_NEG, "neg", "neg", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* negb $drb */
  {
    XC16X_INSN_NEGB, "negb", "negb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $dr,$sr */
  {
    XC16X_INSN_ANDR, "andr", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $dr,$sr */
  {
    XC16X_INSN_ORR, "orr", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $dr,$sr */
  {
    XC16X_INSN_XORR, "xorr", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $drb,$srb */
  {
    XC16X_INSN_ANDBR, "andbr", "andb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $drb,$srb */
  {
    XC16X_INSN_ORBR, "orbr", "orb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $drb,$srb */
  {
    XC16X_INSN_XORBR, "xorbr", "xorb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $dr,$hash$uimm3 */
  {
    XC16X_INSN_ANDRI, "andri", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $dr,$hash$uimm3 */
  {
    XC16X_INSN_ORRI, "orri", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $dr,$hash$uimm3 */
  {
    XC16X_INSN_XORRI, "xorri", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $drb,$hash$uimm3 */
  {
    XC16X_INSN_ANDBRI, "andbri", "andb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $drb,$hash$uimm3 */
  {
    XC16X_INSN_ORBRI, "orbri", "orb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $drb,$hash$uimm3 */
  {
    XC16X_INSN_XORBRI, "xorbri", "xorb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $reg8,$hash$uimm16 */
  {
    XC16X_INSN_ANDRIM, "andrim", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $reg8,$hash$uimm16 */
  {
    XC16X_INSN_ORRIM, "orrim", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $reg8,$hash$uimm16 */
  {
    XC16X_INSN_XORRIM, "xorrim", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_ANDBRIM, "andbrim", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_ORBRIM, "orbrim", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_XORBRIM, "xorbrim", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $dr,[$sr2] */
  {
    XC16X_INSN_AND2, "and2", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $dr,[$sr2] */
  {
    XC16X_INSN_OR2, "or2", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $dr,[$sr2] */
  {
    XC16X_INSN_XOR2, "xor2", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $drb,[$sr2] */
  {
    XC16X_INSN_ANDB2, "andb2", "andb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $drb,[$sr2] */
  {
    XC16X_INSN_ORB2, "orb2", "orb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $drb,[$sr2] */
  {
    XC16X_INSN_XORB2, "xorb2", "xorb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $dr,[$sr2+] */
  {
    XC16X_INSN_AND2I, "and2i", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $dr,[$sr2+] */
  {
    XC16X_INSN_OR2I, "or2i", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $dr,[$sr2+] */
  {
    XC16X_INSN_XOR2I, "xor2i", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $drb,[$sr2+] */
  {
    XC16X_INSN_ANDB2I, "andb2i", "andb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $drb,[$sr2+] */
  {
    XC16X_INSN_ORB2I, "orb2i", "orb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $drb,[$sr2+] */
  {
    XC16X_INSN_XORB2I, "xorb2i", "xorb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $pof$reg8,$upof16 */
  {
    XC16X_INSN_ANDPOFR, "andpofr", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $pof$reg8,$upof16 */
  {
    XC16X_INSN_ORPOFR, "orpofr", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $pof$reg8,$upof16 */
  {
    XC16X_INSN_XORPOFR, "xorpofr", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $pof$regb8,$upof16 */
  {
    XC16X_INSN_ANDBPOFR, "andbpofr", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $pof$regb8,$upof16 */
  {
    XC16X_INSN_ORBPOFR, "orbpofr", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $pof$regb8,$upof16 */
  {
    XC16X_INSN_XORBPOFR, "xorbpofr", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $pof$upof16,$reg8 */
  {
    XC16X_INSN_ANDRPOFR, "andrpofr", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $pof$upof16,$reg8 */
  {
    XC16X_INSN_ORRPOFR, "orrpofr", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $pof$upof16,$reg8 */
  {
    XC16X_INSN_XORRPOFR, "xorrpofr", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $pof$upof16,$regb8 */
  {
    XC16X_INSN_ANDBRPOFR, "andbrpofr", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $pof$upof16,$regb8 */
  {
    XC16X_INSN_ORBRPOFR, "orbrpofr", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $pof$upof16,$regb8 */
  {
    XC16X_INSN_XORBRPOFR, "xorbrpofr", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $regmem8,$memgr8 */
  {
    XC16X_INSN_ANDRM2, "andrm2", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $memgr8,$regmem8 */
  {
    XC16X_INSN_ANDRM3, "andrm3", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $reg8,$memory */
  {
    XC16X_INSN_ANDRM, "andrm", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* and $memory,$reg8 */
  {
    XC16X_INSN_ANDRM1, "andrm1", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $regmem8,$memgr8 */
  {
    XC16X_INSN_ORRM3, "orrm3", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $memgr8,$regmem8 */
  {
    XC16X_INSN_ORRM2, "orrm2", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $reg8,$memory */
  {
    XC16X_INSN_ORRM1, "orrm1", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* or $memory,$reg8 */
  {
    XC16X_INSN_ORRM, "orrm", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $regmem8,$memgr8 */
  {
    XC16X_INSN_XORRM3, "xorrm3", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $memgr8,$regmem8 */
  {
    XC16X_INSN_XORRM2, "xorrm2", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $reg8,$memory */
  {
    XC16X_INSN_XORRM1, "xorrm1", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xor $memory,$reg8 */
  {
    XC16X_INSN_XORRM, "xorrm", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $regbmem8,$memgr8 */
  {
    XC16X_INSN_ANDBRM2, "andbrm2", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $memgr8,$regbmem8 */
  {
    XC16X_INSN_ANDBRM3, "andbrm3", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $regb8,$memory */
  {
    XC16X_INSN_ANDBRM, "andbrm", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* andb $memory,$regb8 */
  {
    XC16X_INSN_ANDBRM1, "andbrm1", "andb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $regbmem8,$memgr8 */
  {
    XC16X_INSN_ORBRM3, "orbrm3", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $memgr8,$regbmem8 */
  {
    XC16X_INSN_ORBRM2, "orbrm2", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $regb8,$memory */
  {
    XC16X_INSN_ORBRM1, "orbrm1", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* orb $memory,$regb8 */
  {
    XC16X_INSN_ORBRM, "orbrm", "orb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $regbmem8,$memgr8 */
  {
    XC16X_INSN_XORBRM3, "xorbrm3", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $memgr8,$regbmem8 */
  {
    XC16X_INSN_XORBRM2, "xorbrm2", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $regb8,$memory */
  {
    XC16X_INSN_XORBRM1, "xorbrm1", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* xorb $memory,$regb8 */
  {
    XC16X_INSN_XORBRM, "xorbrm", "xorb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dr,$sr */
  {
    XC16X_INSN_MOVR, "movr", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $drb,$srb */
  {
    XC16X_INSN_MOVRB, "movrb", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dri,$hash$u4 */
  {
    XC16X_INSN_MOVRI, "movri", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $srb,$hash$u4 */
  {
    XC16X_INSN_MOVBRI, "movbri", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $reg8,$hash$uimm16 */
  {
    XC16X_INSN_MOVI, "movi", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_MOVBI, "movbi", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dr,[$sr] */
  {
    XC16X_INSN_MOVR2, "movr2", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $drb,[$sr] */
  {
    XC16X_INSN_MOVBR2, "movbr2", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [$sr],$dr */
  {
    XC16X_INSN_MOVRI2, "movri2", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [$sr],$drb */
  {
    XC16X_INSN_MOVBRI2, "movbri2", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [-$sr],$dr */
  {
    XC16X_INSN_MOVRI3, "movri3", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [-$sr],$drb */
  {
    XC16X_INSN_MOVBRI3, "movbri3", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dr,[$sr+] */
  {
    XC16X_INSN_MOV2I, "mov2i", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $drb,[$sr+] */
  {
    XC16X_INSN_MOVB2I, "movb2i", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [$dr],[$sr] */
  {
    XC16X_INSN_MOV6I, "mov6i", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [$dr],[$sr] */
  {
    XC16X_INSN_MOVB6I, "movb6i", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [$dr+],[$sr] */
  {
    XC16X_INSN_MOV7I, "mov7i", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [$dr+],[$sr] */
  {
    XC16X_INSN_MOVB7I, "movb7i", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [$dr],[$sr+] */
  {
    XC16X_INSN_MOV8I, "mov8i", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [$dr],[$sr+] */
  {
    XC16X_INSN_MOVB8I, "movb8i", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dr,[$sr+$hash$uimm16] */
  {
    XC16X_INSN_MOV9I, "mov9i", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $drb,[$sr+$hash$uimm16] */
  {
    XC16X_INSN_MOVB9I, "movb9i", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [$sr+$hash$uimm16],$dr */
  {
    XC16X_INSN_MOV10I, "mov10i", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [$sr+$hash$uimm16],$drb */
  {
    XC16X_INSN_MOVB10I, "movb10i", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov [$src2],$memory */
  {
    XC16X_INSN_MOVRI11, "movri11", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb [$src2],$memory */
  {
    XC16X_INSN_MOVBRI11, "movbri11", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $memory,[$src2] */
  {
    XC16X_INSN_MOVRI12, "movri12", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $memory,[$src2] */
  {
    XC16X_INSN_MOVBRI12, "movbri12", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regoff8,$hash$pof$upof16 */
  {
    XC16X_INSN_MOVEHM5, "movehm5", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regoff8,$hash$pag$upag16 */
  {
    XC16X_INSN_MOVEHM6, "movehm6", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regoff8,$hash$segm$useg16 */
  {
    XC16X_INSN_MOVEHM7, "movehm7", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regoff8,$hash$sof$usof16 */
  {
    XC16X_INSN_MOVEHM8, "movehm8", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regb8,$hash$pof$uimm8 */
  {
    XC16X_INSN_MOVEHM9, "movehm9", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regoff8,$hash$pag$uimm8 */
  {
    XC16X_INSN_MOVEHM10, "movehm10", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regoff8,$pof$upof16 */
  {
    XC16X_INSN_MOVRMP, "movrmp", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regb8,$pof$upof16 */
  {
    XC16X_INSN_MOVRMP1, "movrmp1", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regoff8,$pag$upag16 */
  {
    XC16X_INSN_MOVRMP2, "movrmp2", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regb8,$pag$upag16 */
  {
    XC16X_INSN_MOVRMP3, "movrmp3", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $pof$upof16,$regoff8 */
  {
    XC16X_INSN_MOVRMP4, "movrmp4", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $pof$upof16,$regb8 */
  {
    XC16X_INSN_MOVRMP5, "movrmp5", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dri,$hash$pof$u4 */
  {
    XC16X_INSN_MOVEHM1, "movehm1", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $srb,$hash$pof$u4 */
  {
    XC16X_INSN_MOVEHM2, "movehm2", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $dri,$hash$pag$u4 */
  {
    XC16X_INSN_MOVEHM3, "movehm3", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $srb,$hash$pag$u4 */
  {
    XC16X_INSN_MOVEHM4, "movehm4", "movb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $regmem8,$memgr8 */
  {
    XC16X_INSN_MVE12, "mve12", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $memgr8,$regmem8 */
  {
    XC16X_INSN_MVE13, "mve13", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $reg8,$memory */
  {
    XC16X_INSN_MOVER12, "mover12", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* mov $memory,$reg8 */
  {
    XC16X_INSN_MVR13, "mvr13", "mov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regbmem8,$memgr8 */
  {
    XC16X_INSN_MVER12, "mver12", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $memgr8,$regbmem8 */
  {
    XC16X_INSN_MVER13, "mver13", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $regb8,$memory */
  {
    XC16X_INSN_MOVR12, "movr12", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movb $memory,$regb8 */
  {
    XC16X_INSN_MOVR13, "movr13", "movb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $sr,$drb */
  {
    XC16X_INSN_MOVBSRR, "movbsrr", "movbs", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $sr,$drb */
  {
    XC16X_INSN_MOVBZRR, "movbzrr", "movbz", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $regmem8,$pof$upof16 */
  {
    XC16X_INSN_MOVBSRPOFM, "movbsrpofm", "movbs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $pof$upof16,$regbmem8 */
  {
    XC16X_INSN_MOVBSPOFMR, "movbspofmr", "movbs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $reg8,$pof$upof16 */
  {
    XC16X_INSN_MOVBZRPOFM, "movbzrpofm", "movbz", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $pof$upof16,$regb8 */
  {
    XC16X_INSN_MOVBZPOFMR, "movbzpofmr", "movbz", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $regmem8,$memgr8 */
  {
    XC16X_INSN_MOVEBS14, "movebs14", "movbs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $memgr8,$regbmem8 */
  {
    XC16X_INSN_MOVEBS15, "movebs15", "movbs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $reg8,$memory */
  {
    XC16X_INSN_MOVERBS14, "moverbs14", "movbs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $memory,$regb8 */
  {
    XC16X_INSN_MOVRBS15, "movrbs15", "movbs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $regmem8,$memgr8 */
  {
    XC16X_INSN_MOVEBZ14, "movebz14", "movbz", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $memgr8,$regbmem8 */
  {
    XC16X_INSN_MOVEBZ15, "movebz15", "movbz", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $reg8,$memory */
  {
    XC16X_INSN_MOVERBZ14, "moverbz14", "movbz", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $memory,$regb8 */
  {
    XC16X_INSN_MOVRBZ15, "movrbz15", "movbz", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbs $sr,$drb */
  {
    XC16X_INSN_MOVRBS, "movrbs", "movbs", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* movbz $sr,$drb */
  {
    XC16X_INSN_MOVRBZ, "movrbz", "movbz", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpa+ $extcond,$caddr */
  {
    XC16X_INSN_JMPA0, "jmpa0", "jmpa+", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpa $extcond,$caddr */
  {
    XC16X_INSN_JMPA1, "jmpa1", "jmpa", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpa- $extcond,$caddr */
  {
    XC16X_INSN_JMPA_, "jmpa-", "jmpa-", 32,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpi $icond,[$sr] */
  {
    XC16X_INSN_JMPI, "jmpi", "jmpi", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_NENZ, "jmpr_nenz", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_SGT, "jmpr_sgt", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_Z, "jmpr_z", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_V, "jmpr_v", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_NV, "jmpr_nv", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_N, "jmpr_n", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_NN, "jmpr_nn", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_C, "jmpr_c", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_NC, "jmpr_nc", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_EQ, "jmpr_eq", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_NE, "jmpr_ne", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_ULT, "jmpr_ult", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_ULE, "jmpr_ule", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_UGE, "jmpr_uge", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_UGT, "jmpr_ugt", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_SLE, "jmpr_sle", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_SGE, "jmpr_sge", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_NET, "jmpr_net", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_UC, "jmpr_uc", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmpr $cond,$rel */
  {
    XC16X_INSN_JMPR_SLT, "jmpr_slt", "jmpr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmps $hash$segm$useg8,$hash$sof$usof16 */
  {
    XC16X_INSN_JMPSEG, "jmpseg", "jmps", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jmps $seg,$caddr */
  {
    XC16X_INSN_JMPS, "jmps", "jmps", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jb $genreg$dot$qlobit,$relhi */
  {
    XC16X_INSN_JB, "jb", "jb", 32,
    { 0|A(UNCOND_CTI)|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jbc $genreg$dot$qlobit,$relhi */
  {
    XC16X_INSN_JBC, "jbc", "jbc", 32,
    { 0|A(UNCOND_CTI)|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jnb $genreg$dot$qlobit,$relhi */
  {
    XC16X_INSN_JNB, "jnb", "jnb", 32,
    { 0|A(UNCOND_CTI)|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* jnbs $genreg$dot$qlobit,$relhi */
  {
    XC16X_INSN_JNBS, "jnbs", "jnbs", 32,
    { 0|A(UNCOND_CTI)|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* calla+ $extcond,$caddr */
  {
    XC16X_INSN_CALLA0, "calla0", "calla+", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* calla $extcond,$caddr */
  {
    XC16X_INSN_CALLA1, "calla1", "calla", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* calla- $extcond,$caddr */
  {
    XC16X_INSN_CALLA_, "calla-", "calla-", 32,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* calli $icond,[$sr] */
  {
    XC16X_INSN_CALLI, "calli", "calli", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* callr $rel */
  {
    XC16X_INSN_CALLR, "callr", "callr", 16,
    { 0|A(COND_CTI)|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* calls $hash$segm$useg8,$hash$sof$usof16 */
  {
    XC16X_INSN_CALLSEG, "callseg", "calls", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* calls $seg,$caddr */
  {
    XC16X_INSN_CALLS, "calls", "calls", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* pcall $reg8,$caddr */
  {
    XC16X_INSN_PCALL, "pcall", "pcall", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* trap $hash$uimm7 */
  {
    XC16X_INSN_TRAP, "trap", "trap", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* ret */
  {
    XC16X_INSN_RET, "ret", "ret", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* rets */
  {
    XC16X_INSN_RETS, "rets", "rets", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* retp $reg8 */
  {
    XC16X_INSN_RETP, "retp", "retp", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* reti */
  {
    XC16X_INSN_RETI, "reti", "reti", 16,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* pop $reg8 */
  {
    XC16X_INSN_POP, "pop", "pop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* push $reg8 */
  {
    XC16X_INSN_PUSH, "push", "push", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* scxt $reg8,$hash$uimm16 */
  {
    XC16X_INSN_SCXTI, "scxti", "scxt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* scxt $reg8,$pof$upof16 */
  {
    XC16X_INSN_SCXTRPOFM, "scxtrpofm", "scxt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* scxt $regmem8,$memgr8 */
  {
    XC16X_INSN_SCXTMG, "scxtmg", "scxt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* scxt $reg8,$memory */
  {
    XC16X_INSN_SCXTM, "scxtm", "scxt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* nop */
  {
    XC16X_INSN_NOP, "nop", "nop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* srst */
  {
    XC16X_INSN_SRSTM, "srstm", "srst", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* idle */
  {
    XC16X_INSN_IDLEM, "idlem", "idle", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* pwrdn */
  {
    XC16X_INSN_PWRDNM, "pwrdnm", "pwrdn", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* diswdt */
  {
    XC16X_INSN_DISWDTM, "diswdtm", "diswdt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* enwdt */
  {
    XC16X_INSN_ENWDTM, "enwdtm", "enwdt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* einit */
  {
    XC16X_INSN_EINITM, "einitm", "einit", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* srvwdt */
  {
    XC16X_INSN_SRVWDTM, "srvwdtm", "srvwdt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* sbrk */
  {
    XC16X_INSN_SBRK, "sbrk", "sbrk", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* atomic $hash$uimm2 */
  {
    XC16X_INSN_ATOMIC, "atomic", "atomic", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extr $hash$uimm2 */
  {
    XC16X_INSN_EXTR, "extr", "extr", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extp $sr,$hash$uimm2 */
  {
    XC16X_INSN_EXTP, "extp", "extp", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extp $hash$pagenum,$hash$uimm2 */
  {
    XC16X_INSN_EXTP1, "extp1", "extp", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extp $hash$pag$upag16,$hash$uimm2 */
  {
    XC16X_INSN_EXTPG1, "extpg1", "extp", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extpr $sr,$hash$uimm2 */
  {
    XC16X_INSN_EXTPR, "extpr", "extpr", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extpr $hash$pagenum,$hash$uimm2 */
  {
    XC16X_INSN_EXTPR1, "extpr1", "extpr", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* exts $sr,$hash$uimm2 */
  {
    XC16X_INSN_EXTS, "exts", "exts", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* exts $hash$seghi8,$hash$uimm2 */
  {
    XC16X_INSN_EXTS1, "exts1", "exts", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extsr $sr,$hash$uimm2 */
  {
    XC16X_INSN_EXTSR, "extsr", "extsr", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* extsr $hash$seghi8,$hash$uimm2 */
  {
    XC16X_INSN_EXTSR1, "extsr1", "extsr", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* prior $dr,$sr */
  {
    XC16X_INSN_PRIOR, "prior", "prior", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $RegNam */
  {
    XC16X_INSN_BCLR18, "bclr18", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR0, "bclr0", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR1, "bclr1", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR2, "bclr2", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR3, "bclr3", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR4, "bclr4", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR5, "bclr5", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR6, "bclr6", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR7, "bclr7", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR8, "bclr8", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR9, "bclr9", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR10, "bclr10", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR11, "bclr11", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR12, "bclr12", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR13, "bclr13", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR14, "bclr14", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bclr $reg8$dot$qbit */
  {
    XC16X_INSN_BCLR15, "bclr15", "bclr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $RegNam */
  {
    XC16X_INSN_BSET19, "bset19", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET0, "bset0", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET1, "bset1", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET2, "bset2", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET3, "bset3", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET4, "bset4", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET5, "bset5", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET6, "bset6", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET7, "bset7", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET8, "bset8", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET9, "bset9", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET10, "bset10", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET11, "bset11", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET12, "bset12", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET13, "bset13", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET14, "bset14", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bset $reg8$dot$qbit */
  {
    XC16X_INSN_BSET15, "bset15", "bset", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bmov $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    XC16X_INSN_BMOV, "bmov", "bmov", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bmovn $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    XC16X_INSN_BMOVN, "bmovn", "bmovn", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* band $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    XC16X_INSN_BAND, "band", "band", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bor $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    XC16X_INSN_BOR, "bor", "bor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bxor $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    XC16X_INSN_BXOR, "bxor", "bxor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bcmp $reghi8$dot$qhibit,$reg8$dot$qlobit */
  {
    XC16X_INSN_BCMP, "bcmp", "bcmp", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bfldl $reg8,$hash$mask8,$hash$datahi8 */
  {
    XC16X_INSN_BFLDL, "bfldl", "bfldl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* bfldh $reg8,$hash$masklo8,$hash$data8 */
  {
    XC16X_INSN_BFLDH, "bfldh", "bfldh", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $src1,$src2 */
  {
    XC16X_INSN_CMPR, "cmpr", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $drb,$srb */
  {
    XC16X_INSN_CMPBR, "cmpbr", "cmpb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $src1,$hash$uimm3 */
  {
    XC16X_INSN_CMPRI, "cmpri", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $drb,$hash$uimm3 */
  {
    XC16X_INSN_CMPBRI, "cmpbri", "cmpb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $reg8,$hash$uimm16 */
  {
    XC16X_INSN_CMPI, "cmpi", "cmp", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $regb8,$hash$uimm8 */
  {
    XC16X_INSN_CMPBI, "cmpbi", "cmpb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $dr,[$sr2] */
  {
    XC16X_INSN_CMPR2, "cmpr2", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $drb,[$sr2] */
  {
    XC16X_INSN_CMPBR2, "cmpbr2", "cmpb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $dr,[$sr2+] */
  {
    XC16X_INSN_CMP2I, "cmp2i", "cmp", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $drb,[$sr2+] */
  {
    XC16X_INSN_CMPB2I, "cmpb2i", "cmpb", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $reg8,$pof$upof16 */
  {
    XC16X_INSN_CMP04, "cmp04", "cmp", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $regb8,$pof$upof16 */
  {
    XC16X_INSN_CMPB4, "cmpb4", "cmpb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $regmem8,$memgr8 */
  {
    XC16X_INSN_CMP004, "cmp004", "cmp", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmp $reg8,$memory */
  {
    XC16X_INSN_CMP0004, "cmp0004", "cmp", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $regbmem8,$memgr8 */
  {
    XC16X_INSN_CMPB04, "cmpb04", "cmpb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpb $regb8,$memory */
  {
    XC16X_INSN_CMPB004, "cmpb004", "cmpb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd1 $sr,$hash$uimm4 */
  {
    XC16X_INSN_CMPD1RI, "cmpd1ri", "cmpd1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd2 $sr,$hash$uimm4 */
  {
    XC16X_INSN_CMPD2RI, "cmpd2ri", "cmpd2", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi1 $sr,$hash$uimm4 */
  {
    XC16X_INSN_CMPI1RI, "cmpi1ri", "cmpi1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi2 $sr,$hash$uimm4 */
  {
    XC16X_INSN_CMPI2RI, "cmpi2ri", "cmpi2", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd1 $reg8,$hash$uimm16 */
  {
    XC16X_INSN_CMPD1RIM, "cmpd1rim", "cmpd1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd2 $reg8,$hash$uimm16 */
  {
    XC16X_INSN_CMPD2RIM, "cmpd2rim", "cmpd2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi1 $reg8,$hash$uimm16 */
  {
    XC16X_INSN_CMPI1RIM, "cmpi1rim", "cmpi1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi2 $reg8,$hash$uimm16 */
  {
    XC16X_INSN_CMPI2RIM, "cmpi2rim", "cmpi2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd1 $reg8,$pof$upof16 */
  {
    XC16X_INSN_CMPD1RP, "cmpd1rp", "cmpd1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd2 $reg8,$pof$upof16 */
  {
    XC16X_INSN_CMPD2RP, "cmpd2rp", "cmpd2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi1 $reg8,$pof$upof16 */
  {
    XC16X_INSN_CMPI1RP, "cmpi1rp", "cmpi1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi2 $reg8,$pof$upof16 */
  {
    XC16X_INSN_CMPI2RP, "cmpi2rp", "cmpi2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd1 $regmem8,$memgr8 */
  {
    XC16X_INSN_CMPD1RM, "cmpd1rm", "cmpd1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd2 $regmem8,$memgr8 */
  {
    XC16X_INSN_CMPD2RM, "cmpd2rm", "cmpd2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi1 $regmem8,$memgr8 */
  {
    XC16X_INSN_CMPI1RM, "cmpi1rm", "cmpi1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi2 $regmem8,$memgr8 */
  {
    XC16X_INSN_CMPI2RM, "cmpi2rm", "cmpi2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd1 $reg8,$memory */
  {
    XC16X_INSN_CMPD1RMI, "cmpd1rmi", "cmpd1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpd2 $reg8,$memory */
  {
    XC16X_INSN_CMPD2RMI, "cmpd2rmi", "cmpd2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi1 $reg8,$memory */
  {
    XC16X_INSN_CMPI1RMI, "cmpi1rmi", "cmpi1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* cmpi2 $reg8,$memory */
  {
    XC16X_INSN_CMPI2RMI, "cmpi2rmi", "cmpi2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* shl $dr,$sr */
  {
    XC16X_INSN_SHLR, "shlr", "shl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* shr $dr,$sr */
  {
    XC16X_INSN_SHRR, "shrr", "shr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* rol $dr,$sr */
  {
    XC16X_INSN_ROLR, "rolr", "rol", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* ror $dr,$sr */
  {
    XC16X_INSN_RORR, "rorr", "ror", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* ashr $dr,$sr */
  {
    XC16X_INSN_ASHRR, "ashrr", "ashr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* shl $sr,$hash$uimm4 */
  {
    XC16X_INSN_SHLRI, "shlri", "shl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* shr $sr,$hash$uimm4 */
  {
    XC16X_INSN_SHRRI, "shrri", "shr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* rol $sr,$hash$uimm4 */
  {
    XC16X_INSN_ROLRI, "rolri", "rol", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* ror $sr,$hash$uimm4 */
  {
    XC16X_INSN_RORRI, "rorri", "ror", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
  },
/* ashr $sr,$hash$uimm4 */
  {
    XC16X_INSN_ASHRRI, "ashrri", "ashr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { PIPE_OS, 0 } } } }
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
static void xc16x_cgen_rebuild_tables (CGEN_CPU_TABLE *);

/* Subroutine of xc16x_cgen_cpu_open to look up a mach via its bfd name.  */

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

/* Subroutine of xc16x_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & xc16x_cgen_hw_table[0];
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

/* Subroutine of xc16x_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (CGEN_CPU_TABLE *cd)
{
  cd->ifld_table = & xc16x_cgen_ifld_table[0];
}

/* Subroutine of xc16x_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & xc16x_cgen_operand_table[0];
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

/* Subroutine of xc16x_cgen_cpu_open to build the hardware table.
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
  const CGEN_IBASE *ib = & xc16x_cgen_insn_table[0];
  CGEN_INSN *insns = xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of xc16x_cgen_cpu_open to rebuild the tables.  */

static void
xc16x_cgen_rebuild_tables (CGEN_CPU_TABLE *cd)
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
	const CGEN_ISA *isa = & xc16x_cgen_isa_table[i];

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
	const CGEN_MACH *mach = & xc16x_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "xc16x_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
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
xc16x_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
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
	      lookup_mach_via_bfd_name (xc16x_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "xc16x_cgen_cpu_open: unsupported argument `%d'\n",
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
      fprintf (stderr, "xc16x_cgen_cpu_open: no endianness specified\n");
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
  cd->rebuild_tables = xc16x_cgen_rebuild_tables;
  xc16x_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to xc16x_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
xc16x_cgen_cpu_open_1 (const char *mach_name, enum cgen_endian endian)
{
  return xc16x_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
xc16x_cgen_cpu_close (CGEN_CPU_DESC cd)
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

