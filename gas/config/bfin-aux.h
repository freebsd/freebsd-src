/* bfin-aux.h ADI Blackfin Header file for gas
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

#include "bfin-defs.h"

#define REG_T Register *

INSTR_T
bfin_gen_dsp32mac (int op1, int mm, int mmod, int w1, int p,
              int h01, int h11, int h00, int h10,
	      int op0, REG_T dst, REG_T src0, REG_T src1, int w0);

INSTR_T
bfin_gen_dsp32mult (int op1, int mm, int mmod, int w1, int p,
               int h01, int h11, int h00, int h10,
	       int op0, REG_T dst, REG_T src0, REG_T src1, int w0);

INSTR_T
bfin_gen_dsp32alu (int HL, int aopcde, int aop, int s, int x,
              REG_T dst0, REG_T dst1, REG_T src0, REG_T src1);

INSTR_T
bfin_gen_dsp32shift (int sopcde, REG_T dst0, REG_T src0, REG_T src1,
                int sop, int hls);

INSTR_T
bfin_gen_dsp32shiftimm (int sopcde, REG_T dst0, int immag, REG_T src1,
                   int sop, int hls);

INSTR_T
bfin_gen_ldimmhalf (REG_T reg, int h, int s, int z, Expr_Node *hword,
               int reloc);

INSTR_T
bfin_gen_ldstidxi (REG_T ptr, REG_T reg, int w, int sz, int z,
              Expr_Node *offset);

INSTR_T
bfin_gen_ldst (REG_T ptr, REG_T reg, int aop, int sz, int z, int w);

INSTR_T
bfin_gen_ldstii (REG_T ptr, REG_T reg, Expr_Node *offset, int w, int op);

INSTR_T
bfin_gen_ldstiifp (REG_T reg, Expr_Node *offset, int w);

INSTR_T
bfin_gen_ldstpmod (REG_T ptr, REG_T reg, int aop, int w, REG_T idx);

INSTR_T
bfin_gen_dspldst (REG_T i, REG_T reg, int aop, int w, int m);

INSTR_T
bfin_gen_alu2op (REG_T dst, REG_T src, int opc);

INSTR_T
bfin_gen_compi2opd (REG_T dst, int src, int op);

INSTR_T
bfin_gen_compi2opp (REG_T dst, int src, int op);

INSTR_T
bfin_gen_dagmodik (REG_T i, int op);

INSTR_T
bfin_gen_dagmodim (REG_T i, REG_T m, int op, int br);

INSTR_T
bfin_gen_ptr2op (REG_T dst, REG_T src, int opc);

INSTR_T
bfin_gen_logi2op (int dst, int src, int opc);

INSTR_T
bfin_gen_comp3op (REG_T src0, REG_T src1, REG_T dst, int opc);

INSTR_T
bfin_gen_ccmv (REG_T src, REG_T dst, int t);

INSTR_T
bfin_gen_ccflag (REG_T x, int y, int opc, int i, int g);

INSTR_T
bfin_gen_cc2stat (int cbit, int op, int d);

INSTR_T
bfin_gen_regmv (REG_T src, REG_T dst);

INSTR_T
bfin_gen_cc2dreg (int op, REG_T reg);

INSTR_T
bfin_gen_brcc (int t, int b, Expr_Node *offset);

INSTR_T
bfin_gen_ujump (Expr_Node *offset);

INSTR_T
bfin_gen_cactrl (REG_T reg, int a, int op);

INSTR_T
bfin_gen_progctrl (int prgfunc, int poprnd);

INSTR_T
bfin_gen_loopsetup (Expr_Node *soffset, REG_T c, int rop,
               Expr_Node *eoffset, REG_T reg);

INSTR_T
bfin_gen_loop (Expr_Node *expr, REG_T reg, int rop, REG_T preg);

INSTR_T
bfin_gen_pushpopmultiple (int dr, int pr, int d, int p, int w);

INSTR_T
bfin_gen_pushpopreg (REG_T reg, int w);

INSTR_T
bfin_gen_calla (Expr_Node *addr, int s);

INSTR_T
bfin_gen_linkage (int r, int framesize);

INSTR_T
bfin_gen_pseudodbg (int fn, int reg, int grp);

INSTR_T
bfin_gen_pseudodbg_assert (int dbgop, REG_T regtest, int expected);

bfd_boolean
bfin_resource_conflict (INSTR_T dsp32, INSTR_T dsp16_grp1, INSTR_T dsp16_grp2);

INSTR_T
bfin_gen_multi_instr (INSTR_T dsp32, INSTR_T dsp16_grp1, INSTR_T dsp16_grp2);
