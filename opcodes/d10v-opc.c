/* d10v-opc.c -- D10V opcode list
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Written by Martin Hunt, Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
2, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "sysdep.h"
#include "opcode/d10v.h"


/*   The table is sorted. Suitable for searching by a binary search. */
const struct pd_reg d10v_predefined_registers[] =
{
  { "a0", NULL, OPERAND_ACC0+0 },
  { "a1", NULL, OPERAND_ACC1+1 },
  { "bpc", NULL, OPERAND_CONTROL+3 },
  { "bpsw", NULL, OPERAND_CONTROL+1 },
  { "c", NULL, OPERAND_CFLAG+3 },
  { "cr0", "psw", OPERAND_CONTROL },
  { "cr1", "bpsw", OPERAND_CONTROL+1 },
  { "cr10", "mod_s", OPERAND_CONTROL+10 },
  { "cr11", "mod_e", OPERAND_CONTROL+11 },
  { "cr12", NULL, OPERAND_CONTROL+12 },
  { "cr13", NULL, OPERAND_CONTROL+13 },
  { "cr14", "iba", OPERAND_CONTROL+14 },
  { "cr15", NULL, OPERAND_CONTROL+15 },
  { "cr2", "pc", OPERAND_CONTROL+2 },
  { "cr3", "bpc", OPERAND_CONTROL+3 },
  { "cr4", "dpsw", OPERAND_CONTROL+4 },
  { "cr5", "dpc", OPERAND_CONTROL+5 },
  { "cr6", NULL, OPERAND_CONTROL+6 },
  { "cr7", "rpt_c", OPERAND_CONTROL+7 },
  { "cr8", "rpt_s", OPERAND_CONTROL+8 },
  { "cr9", "rpt_e", OPERAND_CONTROL+9 },
  { "dpc", NULL, OPERAND_CONTROL+5 },
  { "dpsw", NULL, OPERAND_CONTROL+4 },
  { "f0", NULL, OPERAND_FFLAG+0 },
  { "f1", NULL, OPERAND_FFLAG+1 },
  { "iba", NULL, OPERAND_CONTROL+14 },
  { "link", "r13", OPERAND_GPR+13 },
  { "mod_e", NULL, OPERAND_CONTROL+11 },
  { "mod_s", NULL, OPERAND_CONTROL+10 },
  { "pc", NULL, OPERAND_CONTROL+2 },
  { "psw", NULL, OPERAND_CONTROL+0 },
  { "r0", NULL, OPERAND_GPR+0 },
  { "r0-r1", NULL, OPERAND_GPR+0},
  { "r1", NULL, OPERAND_GPR+1 },
  { "r1", NULL, OPERAND_GPR+1 },
  { "r10", NULL, OPERAND_GPR+10 },
  { "r10-r11", NULL, OPERAND_GPR+10 },
  { "r11", NULL, OPERAND_GPR+11 },
  { "r12", NULL, OPERAND_GPR+12 },
  { "r12-r13", NULL, OPERAND_GPR+12 },
  { "r13", NULL, OPERAND_GPR+13 },
  { "r14", NULL, OPERAND_GPR+14 },
  { "r14-r15", NULL, OPERAND_GPR+14 },
  { "r15", "sp", OPERAND_SP|(OPERAND_GPR+15) },
  { "r2", NULL, OPERAND_GPR+2 },
  { "r2-r3", NULL, OPERAND_GPR+2 },
  { "r3", NULL, OPERAND_GPR+3 },
  { "r4", NULL, OPERAND_GPR+4 },
  { "r4-r5", NULL, OPERAND_GPR+4 },
  { "r5", NULL, OPERAND_GPR+5 },
  { "r6", NULL, OPERAND_GPR+6 },
  { "r6-r7", NULL, OPERAND_GPR+6 },
  { "r7", NULL, OPERAND_GPR+7 },
  { "r8", NULL, OPERAND_GPR+8 },
  { "r8-r9", NULL, OPERAND_GPR+8 },
  { "r9", NULL, OPERAND_GPR+9 },
  { "rpt_c", NULL, OPERAND_CONTROL+7 },
  { "rpt_e", NULL, OPERAND_CONTROL+9 },
  { "rpt_s", NULL, OPERAND_CONTROL+8 },
  { "sp", NULL, OPERAND_SP|(OPERAND_GPR+15) },
};

int 
d10v_reg_name_cnt()
{
  return (sizeof(d10v_predefined_registers) / sizeof(struct pd_reg));
}

const struct d10v_operand d10v_operands[] =
{
#define UNUSED	(0)
  { 0, 0, 0 },
#define RSRC	(UNUSED + 1)
  { 4, 1, OPERAND_GPR|OPERAND_REG },
#define RSRC_SP (RSRC + 1)
  { 4, 1, OPERAND_SP|OPERAND_GPR|OPERAND_REG },
#define RSRC_NOSP (RSRC_SP + 1)
  { 4, 1, OPERAND_NOSP|OPERAND_GPR|OPERAND_REG },
#define RDST	(RSRC_NOSP + 1)
  { 4, 5, OPERAND_DEST|OPERAND_GPR|OPERAND_REG },
#define ASRC	(RDST + 1)
  { 1, 4, OPERAND_ACC0|OPERAND_ACC1|OPERAND_REG },
#define ASRC0ONLY (ASRC + 1)
  { 1, 4, OPERAND_ACC0|OPERAND_REG },
#define ADST	(ASRC0ONLY + 1)
  { 1, 8, OPERAND_DEST|OPERAND_ACC0|OPERAND_ACC1|OPERAND_REG },
#define RSRCE	(ADST + 1)
  { 4, 1, OPERAND_EVEN|OPERAND_GPR|OPERAND_REG },
#define RDSTE	(RSRCE + 1)
  { 4, 5, OPERAND_EVEN|OPERAND_DEST|OPERAND_GPR|OPERAND_REG },
#define NUM16	(RDSTE + 1)
  { 16, 0, OPERAND_NUM|OPERAND_SIGNED },
#define NUM3	(NUM16 + 1)			/* rac, rachi */
  { 3, 1, OPERAND_NUM|OPERAND_SIGNED|RESTRICTED_NUM3 },
#define NUM4	(NUM3 + 1)
  { 4, 1, OPERAND_NUM|OPERAND_SIGNED },
#define UNUM4	(NUM4 + 1)
  { 4, 1, OPERAND_NUM },
#define UNUM4S	(UNUM4 + 1)			/* addi, slli, srai, srli, subi */
  { 4, 1, OPERAND_NUM|OPERAND_SHIFT },
#define UNUM8	(UNUM4S + 1)			/* repi */
  { 8, 16, OPERAND_NUM },
#define UNUM16	(UNUM8 + 1)			/* cmpui */
  { 16, 0, OPERAND_NUM },
#define ANUM16	(UNUM16 + 1)
  { 16, 0, OPERAND_ADDR|OPERAND_SIGNED },
#define ANUM8	(ANUM16 + 1)
  { 8, 0, OPERAND_ADDR|OPERAND_SIGNED },
#define ASRC2	(ANUM8 + 1)
  { 1, 8, OPERAND_ACC0|OPERAND_ACC1|OPERAND_REG },
#define RSRC2	(ASRC2 + 1)
  { 4, 5, OPERAND_GPR|OPERAND_REG },
#define RSRC2E	(RSRC2 + 1)
  { 4, 5, OPERAND_GPR|OPERAND_REG|OPERAND_EVEN },
#define ASRC0	(RSRC2E + 1)
  { 1, 0, OPERAND_ACC0|OPERAND_ACC1|OPERAND_REG },
#define ADST0	(ASRC0 + 1)
  { 1, 0, OPERAND_ACC0|OPERAND_ACC1|OPERAND_REG|OPERAND_DEST },
#define FFSRC	(ADST0 + 1)
  { 2, 1, OPERAND_REG | OPERAND_FFLAG },
#define CFSRC	(FFSRC + 1)
  { 2, 1, OPERAND_REG | OPERAND_CFLAG },
#define FDST	(CFSRC + 1)
  { 1, 5, OPERAND_REG | OPERAND_FFLAG | OPERAND_DEST},
#define ATSIGN	(FDST + 1)
  { 0, 0, OPERAND_ATSIGN},
#define ATPAR	(ATSIGN + 1)	/* "@(" */
  { 0, 0, OPERAND_ATPAR},
#define PLUS	(ATPAR + 1)	/* postincrement */
  { 0, 0, OPERAND_PLUS},
#define MINUS	(PLUS + 1)	/* postdecrement */
  { 0, 0, OPERAND_MINUS},
#define ATMINUS	(MINUS + 1)	/* predecrement */
  { 0, 0, OPERAND_ATMINUS},
#define CSRC	(ATMINUS + 1)	/* control register */
  { 4, 1, OPERAND_REG|OPERAND_CONTROL},
#define CDST	(CSRC + 1)	/* control register */
  { 4, 5, OPERAND_REG|OPERAND_CONTROL|OPERAND_DEST},
};

const struct d10v_opcode d10v_opcodes[] = {
  { "abs", SHORT_2, 1, EITHER, PAR|WF0, 0x4607, 0x7e1f, { RDST } },
  { "abs", SHORT_2, 1, IU, PAR|WF0, 0x5607, 0x7eff, { ADST } },
  { "add", SHORT_2, 1, EITHER, PAR|WCAR, 0x0200, 0x7e01, { RDST, RSRC } },
  { "add", SHORT_2, 1, IU, PAR, 0x1201, 0x7ee3, { ADST, RSRCE } },
  { "add", SHORT_2, 1, IU, PAR, 0x1203, 0x7eef, { ADST, ASRC } },
  { "add2w", SHORT_2, 2, IU, PAR|WCAR, 0x1200, 0x7e23, { RDSTE, RSRCE } },
  { "add3", LONG_L, 1, MU, SEQ|WCAR, 0x1000000, 0x3f000000, { RDST, RSRC, NUM16 } },
  { "addac3", LONG_R, 1, IU, SEQ, 0x17000200, 0x3ffffe22, { RDSTE, RSRCE, ASRC0 } },
  { "addac3", LONG_R, 1, IU, SEQ, 0x17000202, 0x3ffffe2e, { RDSTE, ASRC, ASRC0 } },
  { "addac3s", LONG_R, 1, IU, SEQ, 0x17001200, 0x3ffffe22, { RDSTE, RSRCE, ASRC0 } },
  { "addac3s", LONG_R, 1, IU, SEQ, 0x17001202, 0x3ffffe2e, { RDSTE, ASRC, ASRC0 } },
  { "addi", SHORT_2, 1, EITHER, PAR|WCAR,  0x201, 0x7e01, { RDST, UNUM4S } },
  { "and", SHORT_2, 1, EITHER, PAR, 0xc00, 0x7e01, { RDST, RSRC } },
  { "and3", LONG_L, 1, MU, SEQ, 0x6000000, 0x3f000000, { RDST, RSRC, NUM16 } },
  { "bclri", SHORT_2, 1, IU, PAR, 0xc01, 0x7e01, { RDST, UNUM4 } },
  { "bl", OPCODE_FAKE, 0, 0, 0, 0, 0, { 0, 8, 16, 0 } },
  { "bl.s", SHORT_B, 3, MU, ALONE|BRANCH_LINK|PAR, 0x4900, 0x7f00, { ANUM8 } },
  { "bl.l", LONG_B, 3, MU, BRANCH_LINK|SEQ, 0x24800000, 0x3fff0000, { ANUM16 } },
  { "bnoti", SHORT_2, 1, IU, PAR, 0xa01, 0x7e01, { RDST, UNUM4 } },
  { "bra", OPCODE_FAKE, 0, 0, 0, 0, 0, { 0, 8, 16, 0 } },
  { "bra.s", SHORT_B, 3, MU, ALONE|BRANCH|PAR, 0x4800, 0x7f00, { ANUM8 } },
  { "bra.l", LONG_B, 3, MU, BRANCH|SEQ, 0x24000000, 0x3fff0000, { ANUM16 } },
  { "brf0f", OPCODE_FAKE, 0, 0, 0, 0, 0, { 0, 8, 16, 0 } },
  { "brf0f.s", SHORT_B, 3, MU, BRANCH|PAR|RF0, 0x4a00, 0x7f00, { ANUM8 } },
  { "brf0f.l", LONG_B, 3, MU, SEQ, 0x25000000, 0x3fff0000, { ANUM16 } },
  { "brf0t", OPCODE_FAKE, 0, 0, 0, 0, 0, { 0, 8, 16, 0 } },
  { "brf0t.s", SHORT_B, 3, MU, BRANCH|PAR|RF0, 0x4b00, 0x7f00, { ANUM8 } },
  { "brf0t.l", LONG_B, 3, MU, SEQ, 0x25800000, 0x3fff0000, { ANUM16 } },
  { "bseti", SHORT_2, 1, IU, PAR, 0x801, 0x7e01, { RDST, UNUM4 } },
  { "btsti", SHORT_2, 1, IU, PAR|WF0, 0xe01, 0x7e01, { RSRC2, UNUM4 } },
  { "clrac", SHORT_2, 1, IU, PAR, 0x5601, 0x7eff, { ADST } },
  { "cmp", SHORT_2, 1, EITHER, PAR|WF0, 0x600, 0x7e01, { RSRC2, RSRC } },
  { "cmp", SHORT_2, 1, IU, PAR|WF0, 0x1603, 0x7eef, { ASRC2, ASRC } },
  { "cmpeq", SHORT_2, 1, EITHER, PAR|WF0, 0x400, 0x7e01, { RSRC2, RSRC } },
  { "cmpeq", SHORT_2, 1, IU, PAR|WF0, 0x1403, 0x7eef, { ASRC2, ASRC } },
  { "cmpeqi", OPCODE_FAKE, 0, 0, 0, 0, 0, { 1, 4, 16, 0 } },
  { "cmpeqi.s", SHORT_2, 1, EITHER, PAR|WF0, 0x401, 0x7e01, { RSRC2, NUM4 } },
  { "cmpeqi.l", LONG_L, 1, MU, SEQ, 0x2000000, 0x3f0f0000, { RSRC2, NUM16 } },
  { "cmpi", OPCODE_FAKE, 0, 0, 0, 0, 0, { 1, 4, 16, 0 } },
  { "cmpi.s", SHORT_2, 1, EITHER, PAR|WF0, 0x601, 0x7e01, { RSRC2, NUM4 } },
  { "cmpi.l", LONG_L, 1, MU, SEQ, 0x3000000, 0x3f0f0000, { RSRC2, NUM16 } },
  { "cmpu", SHORT_2, 1, EITHER, PAR|WF0, 0x4600, 0x7e01, { RSRC2, RSRC } },
  { "cmpui", LONG_L, 1, MU, SEQ, 0x23000000, 0x3f0f0000, { RSRC2, UNUM16 } },
  { "cpfg", SHORT_2, 1, MU, PAR, 0x4e0f, 0x7fdf, { FDST, CFSRC } },
  { "cpfg", SHORT_2, 1, MU, PAR, 0x4e09, 0x7fd9, { FDST, FFSRC } },
  { "dbt", SHORT_2, 5, MU, ALONE|PAR, 0x5f20, 0x7fff, { 0 } },
  { "divs", LONG_L, 1, BOTH, SEQ, 0x14002800, 0x3f10fe21, { RDSTE, RSRC } },
  { "exef0f", SHORT_2, 1, EITHER, PARONLY, 0x4e04, 0x7fff, { 0 } },
  { "exef0t", SHORT_2, 1, EITHER, PARONLY, 0x4e24, 0x7fff, { 0 } },
  { "exef1f", SHORT_2, 1, EITHER, PARONLY, 0x4e40, 0x7fff, { 0 } },
  { "exef1t", SHORT_2, 1, EITHER, PARONLY, 0x4e42, 0x7fff, { 0 } },
  { "exefaf", SHORT_2, 1, EITHER, PARONLY, 0x4e00, 0x7fff, { 0 } },
  { "exefat", SHORT_2, 1, EITHER, PARONLY, 0x4e02, 0x7fff, { 0 } },
  { "exetaf", SHORT_2, 1, EITHER, PARONLY, 0x4e20, 0x7fff, { 0 } },
  { "exetat", SHORT_2, 1, EITHER, PARONLY, 0x4e22, 0x7fff, { 0 } },
  { "exp", LONG_R, 1, IU, SEQ, 0x15002a00, 0x3ffffe03, { RDST, RSRCE } },
  { "exp", LONG_R, 1, IU, SEQ, 0x15002a02, 0x3ffffe0f, { RDST, ASRC } },
  { "jl", SHORT_2, 3, MU, ALONE|BRANCH_LINK|PAR, 0x4d00, 0x7fe1, { RSRC } },
  { "jmp", SHORT_2, 3, MU, ALONE|BRANCH|PAR, 0x4c00, 0x7fe1, { RSRC } },
  { "ld", LONG_L, 1, MU, SEQ, 0x30000000, 0x3f000000, { RDST, ATPAR, NUM16, RSRC } },
  { "ld", SHORT_2, 1, MU, PAR|RMEM, 0x6401, 0x7e01, { RDST, ATSIGN, RSRC, MINUS } },
  { "ld", SHORT_2, 1, MU, PAR|RMEM, 0x6001, 0x7e01, { RDST, ATSIGN, RSRC, PLUS } },
  { "ld", SHORT_2, 1, MU, PAR|RMEM, 0x6000, 0x7e01, { RDST, ATSIGN, RSRC } },
  { "ld", LONG_L, 1, MU, SEQ, 0x32010000, 0x3f0f0000, { RDST, ATSIGN, NUM16 } },
  { "ld2w", LONG_L, 1, MU, SEQ, 0x31000000, 0x3f100000, { RDSTE, ATPAR, NUM16, RSRC } },
  { "ld2w", SHORT_2, 1, MU, PAR|RMEM, 0x6601, 0x7e21, { RDSTE, ATSIGN, RSRC, MINUS } },
  { "ld2w", SHORT_2, 1, MU, PAR|RMEM, 0x6201, 0x7e21, { RDSTE, ATSIGN, RSRC, PLUS } },
  { "ld2w", SHORT_2, 1, MU, PAR|RMEM, 0x6200, 0x7e21, { RDSTE, ATSIGN, RSRC } },
  { "ld2w", LONG_L, 1, MU, SEQ, 0x33010000, 0x3f1f0000, { RDSTE, ATSIGN, NUM16 } },
  { "ldb", LONG_L, 1, MU, SEQ, 0x38000000, 0x3f000000, { RDST, ATPAR, NUM16, RSRC } },
  { "ldb", SHORT_2, 1, MU, PAR|RMEM, 0x7000, 0x7e01, { RDST, ATSIGN, RSRC } },
  { "ldi", OPCODE_FAKE, 0, 0, 0, 0, 0, { 1, 4, 16, 0 } },
  { "ldi.s", SHORT_2, 1, EITHER, PAR|RMEM, 0x4001, 0x7e01 , { RDST, NUM4 } },
  { "ldi.l", LONG_L, 1, MU, SEQ, 0x20000000, 0x3f0f0000, { RDST, NUM16 } },
  { "ldub", LONG_L, 1, MU, SEQ, 0x39000000, 0x3f000000, { RDST, ATPAR, NUM16, RSRC } },
  { "ldub", SHORT_2, 1, MU, PAR|RMEM, 0x7200, 0x7e01, { RDST, ATSIGN, RSRC } },
  { "mac", SHORT_2, 1, IU, PAR, 0x2a00, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "macsu", SHORT_2, 1, IU, PAR, 0x1a00, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "macu", SHORT_2, 1, IU, PAR, 0x3a00, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "max", SHORT_2, 1, IU, PAR|WF0, 0x2600, 0x7e01, { RDST, RSRC } },
  { "max", SHORT_2, 1, IU, PAR|WF0, 0x3600, 0x7ee3, { ADST, RSRCE } },
  { "max", SHORT_2, 1, IU, PAR|WF0, 0x3602, 0x7eef, { ADST, ASRC } },
  { "min", SHORT_2, 1, IU, PAR|WF0, 0x2601, 0x7e01 , { RDST, RSRC } },
  { "min", SHORT_2, 1, IU, PAR|WF0, 0x3601, 0x7ee3 , { ADST, RSRCE } },
  { "min", SHORT_2, 1, IU, PAR|WF0, 0x3603, 0x7eef, { ADST, ASRC } },
  { "msb", SHORT_2, 1, IU, PAR, 0x2800, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "msbsu", SHORT_2, 1, IU, PAR, 0x1800, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "msbu", SHORT_2, 1, IU, PAR, 0x3800, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "mul", SHORT_2, 1, IU, PAR, 0x2e00, 0x7e01 , { RDST, RSRC } },
  { "mulx", SHORT_2, 1, IU, PAR, 0x2c00, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "mulxsu", SHORT_2, 1, IU, PAR, 0x1c00, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "mulxu", SHORT_2, 1, IU, PAR, 0x3c00, 0x7e00, { ADST0, RSRC2, RSRC } },
  { "mv", SHORT_2, 1, EITHER, PAR, 0x4000, 0x7e01, { RDST, RSRC } },
  { "mv2w", SHORT_2, 1, IU, PAR, 0x5000, 0x7e23, { RDSTE, RSRCE } },
  { "mv2wfac", SHORT_2, 1, IU, PAR, 0x3e00, 0x7e2f, { RDSTE, ASRC } },
  { "mv2wtac", SHORT_2, 1, IU, PAR, 0x3e01, 0x7ee3, { RSRCE, ADST } },
  { "mvac", SHORT_2, 1, IU, PAR, 0x3e03, 0x7eef, { ADST, ASRC } },
  { "mvb", SHORT_2, 1, IU, PAR, 0x5400, 0x7e01, { RDST, RSRC } },
  { "mvf0f", SHORT_2, 1, EITHER, PAR|RF0, 0x4400, 0x7e01, { RDST, RSRC } },
  { "mvf0t", SHORT_2, 1, EITHER, PAR|RF0, 0x4401, 0x7e01, { RDST, RSRC } },
  { "mvfacg", SHORT_2, 1, IU, PAR, 0x1e04, 0x7e0f, { RDST, ASRC } },
  { "mvfachi", SHORT_2, 1, IU, PAR, 0x1e00, 0x7e0f, { RDST, ASRC } },
  { "mvfaclo", SHORT_2, 1, IU, PAR, 0x1e02, 0x7e0f, { RDST, ASRC } },
  { "mvfc", SHORT_2, 1, MU, PAR, 0x5200, 0x7e01, { RDST, CSRC } },
  { "mvtacg", SHORT_2, 1, IU, PAR, 0x1e41, 0x7ee1, { RSRC, ADST } },
  { "mvtachi", SHORT_2, 1, IU, PAR, 0x1e01, 0x7ee1, { RSRC, ADST } },
  { "mvtaclo", SHORT_2, 1, IU, PAR, 0x1e21, 0x7ee1, { RSRC, ADST } },
  { "mvtc", SHORT_2, 1, MU, PAR, 0x5600, 0x7e01, { RSRC, CDST } },
  { "mvub", SHORT_2, 1, IU, PAR, 0x5401, 0x7e01, { RDST, RSRC } },
  { "neg", SHORT_2, 1, EITHER, PAR, 0x4605, 0x7e1f, { RDST } },
  { "neg", SHORT_2, 1, IU, PAR, 0x5605, 0x7eff, { ADST } },
  { "nop", SHORT_2, 1, EITHER, PAR, 0x5e00, 0x7fff, { 0 } },
  { "not", SHORT_2, 1, EITHER, PAR, 0x4603, 0x7e1f, { RDST } },
  { "or", SHORT_2, 1, EITHER, PAR, 0x800, 0x7e01, { RDST, RSRC } },
  { "or3", LONG_L, 1, MU, SEQ, 0x4000000, 0x3f000000, { RDST, RSRC, NUM16 } },
  /* Special case. sac&sachi must occur before rac&rachi because they have
     intersecting masks! The masks for rac&rachi will match sac&sachi but
     not the other way around.
   */
  { "sac", SHORT_2, 1, IU, PAR|RF0|WF0, 0x5209, 0x7e2f, { RDSTE, ASRC } },
  { "sachi", SHORT_2, 1, IU, PAR|RF0|WF0, 0x4209, 0x7e0f, { RDST, ASRC } },
  { "rac", SHORT_2, 1, IU, PAR|WF0, 0x5201, 0x7e21, { RDSTE, ASRC0ONLY, NUM3 } },
  { "rachi", SHORT_2, 1, IU, PAR|WF0, 0x4201, 0x7e01, { RDST, ASRC, NUM3 } },
  { "rep", LONG_L, 2, MU, SEQ, 0x27000000, 0x3ff00000, { RSRC, ANUM16 } },
  { "repi", LONG_L, 2, MU, SEQ, 0x2f000000, 0x3f000000, { UNUM8, ANUM16 } },
  { "rtd", SHORT_2, 3, MU, ALONE|PAR, 0x5f60, 0x7fff, { 0 } },
  { "rte", SHORT_2, 3, MU, ALONE|PAR, 0x5f40, 0x7fff, { 0 } },
  { "sadd", SHORT_2, 1, IU, PAR, 0x1223, 0x7eef, { ADST, ASRC } },
  { "setf0f", SHORT_2, 1, MU, PAR|RF0, 0x4611, 0x7e1f, { RDST } },
  { "setf0t", SHORT_2, 1, MU, PAR|RF0, 0x4613, 0x7e1f, { RDST } },
  { "slae", SHORT_2, 1, IU, PAR, 0x3220, 0x7ee1, { ADST, RSRC } },
  { "sleep", SHORT_2, 1, MU, ALONE|PAR, 0x5fc0, 0x7fff, { 0 } },
  { "sll", SHORT_2, 1, IU, PAR, 0x2200, 0x7e01, { RDST, RSRC } },
  { "sll", SHORT_2, 1, IU, PAR, 0x3200, 0x7ee1, { ADST, RSRC } },
  { "slli", SHORT_2, 1, IU, PAR, 0x2201, 0x7e01, { RDST, UNUM4 } },
  { "slli", SHORT_2, 1, IU, PAR, 0x3201, 0x7ee1, { ADST, UNUM4S } },
  { "slx", SHORT_2, 1, IU, PAR|RF0, 0x460b, 0x7e1f, { RDST } },
  { "sra", SHORT_2, 1, IU, PAR, 0x2400, 0x7e01, { RDST, RSRC } },
  { "sra", SHORT_2, 1, IU, PAR, 0x3400, 0x7ee1, { ADST, RSRC } },
  { "srai", SHORT_2, 1, IU, PAR, 0x2401, 0x7e01, { RDST, UNUM4 } },
  { "srai", SHORT_2, 1, IU, PAR, 0x3401, 0x7ee1, { ADST, UNUM4S } },
  { "srl", SHORT_2, 1, IU, PAR, 0x2000, 0x7e01, { RDST, RSRC } },
  { "srl", SHORT_2, 1, IU, PAR, 0x3000, 0x7ee1, { ADST, RSRC } },
  { "srli", SHORT_2, 1, IU, PAR, 0x2001, 0x7e01, { RDST, UNUM4 } },
  { "srli", SHORT_2, 1, IU, PAR, 0x3001, 0x7ee1, { ADST, UNUM4S } },
  { "srx", SHORT_2, 1, IU, PAR|RF0, 0x4609, 0x7e1f, { RDST } },
  { "st", LONG_L, 1, MU, SEQ, 0x34000000, 0x3f000000, { RSRC2, ATPAR, NUM16, RSRC } },
  { "st", SHORT_2, 1, MU, PAR|WMEM, 0x6800, 0x7e01, { RSRC2, ATSIGN, RSRC } },
  { "st", SHORT_2, 1, MU, PAR|WMEM, 0x6c1f, 0x7e1f, { RSRC2, ATMINUS, RSRC_SP } },
  { "st", SHORT_2, 1, MU, PAR|WMEM, 0x6801, 0x7e01, { RSRC2, ATSIGN, RSRC, PLUS } },
  { "st", SHORT_2, 1, MU, PAR|WMEM, 0x6c01, 0x7e01, { RSRC2, ATSIGN, RSRC_NOSP, MINUS } },
  { "st", LONG_L, 1, MU, SEQ, 0x36010000, 0x3f0f0000, { RSRC2, ATSIGN, NUM16 } },
  { "st2w", LONG_L, 1, MU, SEQ, 0x35000000, 0x3f100000, { RSRC2E, ATPAR, NUM16, RSRC } },
  { "st2w", SHORT_2, 1, MU, PAR|WMEM, 0x6a00, 0x7e21, { RSRC2E, ATSIGN, RSRC } },
  { "st2w", SHORT_2, 1, MU, PAR|WMEM, 0x6e1f, 0x7e3f, { RSRC2E, ATMINUS, RSRC_SP } },
  { "st2w", SHORT_2, 1, MU, PAR|WMEM, 0x6a01, 0x7e21, { RSRC2E, ATSIGN, RSRC, PLUS } },
  { "st2w", SHORT_2, 1, MU, PAR|WMEM, 0x6e01, 0x7e21, { RSRC2E, ATSIGN, RSRC_NOSP, MINUS } },
  { "st2w", LONG_L, 1, MU, SEQ, 0x37010000, 0x3f1f0000, { RSRC2E, ATSIGN, NUM16 } },
  { "stb", LONG_L, 1, MU, SEQ, 0x3c000000, 0x3f000000, { RSRC2, ATPAR, NUM16, RSRC } },
  { "stb", SHORT_2, 1, MU, PAR|WMEM, 0x7800, 0x7e01, { RSRC2, ATSIGN, RSRC } },
  { "stop", SHORT_2, 1, MU, ALONE|PAR, 0x5fe0, 0x7fff, { 0 } },
  { "sub", SHORT_2, 1, EITHER, PAR|WCAR, 0x0, 0x7e01, { RDST, RSRC } },
  { "sub", SHORT_2, 1, IU, PAR, 0x1001, 0x7ee3, { ADST, RSRC } },
  { "sub", SHORT_2, 1, IU, PAR, 0x1003, 0x7eef, { ADST, ASRC } },
  { "sub2w", SHORT_2, 1, IU, PAR|WCAR, 0x1000, 0x7e23, { RDSTE, RSRCE } },
  { "subac3", LONG_R, 1, IU, SEQ, 0x17000000, 0x3ffffe22, { RDSTE, RSRCE, ASRC0 } },
  { "subac3", LONG_R, 1, IU, SEQ, 0x17000002, 0x3ffffe2e, { RDSTE, ASRC, ASRC0 } },
  { "subac3s", LONG_R, 1, IU, SEQ, 0x17001000, 0x3ffffe22, { RDSTE, RSRCE, ASRC0 } },
  { "subac3s", LONG_R, 1, IU, SEQ, 0x17001002, 0x3ffffe2e, { RDSTE, ASRC, ASRC0 } },
  { "subi", SHORT_2, 1, EITHER, PAR, 0x1, 0x7e01, { RDST, UNUM4S } },
  { "trap", SHORT_2, 5, MU, ALONE|BRANCH_LINK|PAR, 0x5f00, 0x7fe1, { UNUM4 } },
  { "tst0i", LONG_L, 1, MU, SEQ, 0x7000000, 0x3f0f0000, { RSRC2, NUM16 } },
  { "tst1i", LONG_L, 1, MU, SEQ, 0xf000000, 0x3f0f0000, { RSRC2, NUM16 } },
  { "wait", SHORT_2, 1, MU, ALONE|PAR, 0x5f80, 0x7fff, { 0 } },
  { "xor", SHORT_2, 1, EITHER, PAR, 0xa00, 0x7e01, { RDST, RSRC } },
  { "xor3", LONG_L, 1, MU, SEQ, 0x5000000, 0x3f000000, { RDST, RSRC, NUM16 } },
  { 0, 0, 0, 0, 0, 0, 0, { 0 } },
};


