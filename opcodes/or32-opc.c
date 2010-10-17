/* Table of opcodes for the OpenRISC 1000 ISA.
   Copyright 2002 Free Software Foundation, Inc.
   Contributed by Damjan Lampret (lampret@opencores.org).
   
   This file is part of gen_or1k_isa, or1k, GDB and GAS.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* We treat all letters the same in encode/decode routines so
   we need to assign some characteristics to them like signess etc.  */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "safe-ctype.h"
#include "ansidecl.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "opcode/or32.h"

static unsigned long           insn_extract         PARAMS ((char, char *));
static unsigned long *         cover_insn           PARAMS ((unsigned long *, int, unsigned int));
static int                     num_ones             PARAMS ((unsigned long));
static struct insn_op_struct * parse_params         PARAMS ((const struct or32_opcode *, struct insn_op_struct *));
static unsigned long           or32_extract         PARAMS ((char, char *, unsigned long));
static void                    or32_print_register  PARAMS ((char, char *, unsigned long));
static void                    or32_print_immediate PARAMS ((char, char *, unsigned long));
static unsigned long           extend_imm           PARAMS ((unsigned long, char));

const struct or32_letter or32_letters[] =
  {
    { 'A', NUM_UNSIGNED },
    { 'B', NUM_UNSIGNED },
    { 'D', NUM_UNSIGNED },
    { 'I', NUM_SIGNED },
    { 'K', NUM_UNSIGNED },
    { 'L', NUM_UNSIGNED },
    { 'N', NUM_SIGNED },
    { '0', NUM_UNSIGNED },
    { '\0', 0 }     /* Dummy entry.  */
  };

/* Opcode encoding:
   machine[31:30]: first two bits of opcode
   		   00 - neither of source operands is GPR
   		   01 - second source operand is GPR (rB)
   		   10 - first source operand is GPR (rA)
   		   11 - both source operands are GPRs (rA and rB)
   machine[29:26]: next four bits of opcode
   machine[25:00]: instruction operands (specific to individual instruction)

  Recommendation: irrelevant instruction bits should be set with a value of
  bits in same positions of instruction preceding current instruction in the
  code (when assembling).  */

#define EFN &l_none

#ifdef HAS_EXECUTION
#define EF(func) &(func)
#define EFI &l_invalid
#else  /* HAS_EXECUTION */
#define EF(func) EFN
#define EFI EFN
#endif /* HAS_EXECUTION */

const struct or32_opcode or32_opcodes[] =
  {
    { "l.j",       "N",            "00 0x0  NNNNN NNNNN NNNN NNNN NNNN NNNN", EF(l_j), OR32_IF_DELAY },
    { "l.jal",     "N",            "00 0x1  NNNNN NNNNN NNNN NNNN NNNN NNNN", EF(l_jal), OR32_IF_DELAY },
    { "l.bnf",     "N",            "00 0x3  NNNNN NNNNN NNNN NNNN NNNN NNNN", EF(l_bnf), OR32_IF_DELAY | OR32_R_FLAG},
    { "l.bf",      "N",            "00 0x4  NNNNN NNNNN NNNN NNNN NNNN NNNN", EF(l_bf), OR32_IF_DELAY | OR32_R_FLAG },
    { "l.nop",     "K",            "00 0x5  01--- ----- KKKK KKKK KKKK KKKK", EF(l_nop), 0 },
    { "l.movhi",   "rD,K",         "00 0x6  DDDDD ----0 KKKK KKKK KKKK KKKK", EF(l_movhi), 0 }, /*MM*/
    { "l.macrc",   "rD",           "00 0x6  DDDDD ----1 0000 0000 0000 0000", EF(l_macrc), 0 }, /*MM*/

    { "l.sys",     "K",            "00 0x8  00000 00000 KKKK KKKK KKKK KKKK", EF(l_sys), 0 },
    { "l.trap",    "K",            "00 0x8  01000 00000 KKKK KKKK KKKK KKKK", EF(l_trap), 0 }, /* CZ 21/06/01 */
    { "l.msync",   "",             "00 0x8  10000 00000 0000 0000 0000 0000", EFN, 0 },
    { "l.psync",   "",             "00 0x8  10100 00000 0000 0000 0000 0000", EFN, 0 },
    { "l.csync",   "",             "00 0x8  11000 00000 0000 0000 0000 0000", EFN, 0 },
    { "l.rfe",     "",             "00 0x9  ----- ----- ---- ---- ---- ----", EF(l_rfe), OR32_IF_DELAY },

    { "lv.all_eq.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x0", EFI, 0 },
    { "lv.all_eq.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x1", EFI, 0 },
    { "lv.all_ge.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x2", EFI, 0 },
    { "lv.all_ge.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x3", EFI, 0 },
    { "lv.all_gt.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x4", EFI, 0 },
    { "lv.all_gt.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x5", EFI, 0 },
    { "lv.all_le.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x6", EFI, 0 },
    { "lv.all_le.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x7", EFI, 0 },
    { "lv.all_lt.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x8", EFI, 0 },
    { "lv.all_lt.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0x9", EFI, 0 },
    { "lv.all_ne.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0xA", EFI, 0 },
    { "lv.all_ne.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x1 0xB", EFI, 0 },
    { "lv.any_eq.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x0", EFI, 0 },
    { "lv.any_eq.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x1", EFI, 0 },
    { "lv.any_ge.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x2", EFI, 0 },
    { "lv.any_ge.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x3", EFI, 0 },
    { "lv.any_gt.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x4", EFI, 0 },
    { "lv.any_gt.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x5", EFI, 0 },
    { "lv.any_le.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x6", EFI, 0 },
    { "lv.any_le.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x7", EFI, 0 },
    { "lv.any_lt.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x8", EFI, 0 },
    { "lv.any_lt.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0x9", EFI, 0 },
    { "lv.any_ne.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0xA", EFI, 0 },
    { "lv.any_ne.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x2 0xB", EFI, 0 },
    { "lv.add.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x0", EFI, 0 },
    { "lv.add.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x1", EFI, 0 },
    { "lv.adds.b", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x2", EFI, 0 },
    { "lv.adds.h", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x3", EFI, 0 },
    { "lv.addu.b", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x4", EFI, 0 },
    { "lv.addu.h", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x5", EFI, 0 },
    { "lv.addus.b","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x6", EFI, 0 },
    { "lv.addus.h","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x7", EFI, 0 },
    { "lv.and",    "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x8", EFI, 0 },
    { "lv.avg.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0x9", EFI, 0 },
    { "lv.avg.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x3 0xA", EFI, 0 },
    { "lv.cmp_eq.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x0", EFI, 0 },
    { "lv.cmp_eq.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x1", EFI, 0 },
    { "lv.cmp_ge.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x2", EFI, 0 },
    { "lv.cmp_ge.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x3", EFI, 0 },
    { "lv.cmp_gt.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x4", EFI, 0 },
    { "lv.cmp_gt.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x5", EFI, 0 },
    { "lv.cmp_le.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x6", EFI, 0 },
    { "lv.cmp_le.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x7", EFI, 0 },
    { "lv.cmp_lt.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x8", EFI, 0 },
    { "lv.cmp_lt.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0x9", EFI, 0 },
    { "lv.cmp_ne.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0xA", EFI, 0 },
    { "lv.cmp_ne.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x4 0xB", EFI, 0 },
    { "lv.madds.h","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0x4", EFI, 0 },
    { "lv.max.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0x5", EFI, 0 },
    { "lv.max.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0x6", EFI, 0 },
    { "lv.merge.b","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0x7", EFI, 0 },
    { "lv.merge.h","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0x8", EFI, 0 },
    { "lv.min.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0x9", EFI, 0 },
    { "lv.min.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0xA", EFI, 0 },
    { "lv.msubs.h","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0xB", EFI, 0 },
    { "lv.muls.h", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0xC", EFI, 0 },
    { "lv.nand",   "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0xD", EFI, 0 },
    { "lv.nor",    "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0xE", EFI, 0 },
    { "lv.or",     "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x5 0xF", EFI, 0 },
    { "lv.pack.b", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x0", EFI, 0 },
    { "lv.pack.h", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x1", EFI, 0 },
    { "lv.packs.b","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x2", EFI, 0 },
    { "lv.packs.h","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x3", EFI, 0 },
    { "lv.packus.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x4", EFI, 0 },
    { "lv.packus.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x5", EFI, 0 },
    { "lv.perm.n", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x6", EFI, 0 },
    { "lv.rl.b",   "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x7", EFI, 0 },
    { "lv.rl.h",   "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x8", EFI, 0 },
    { "lv.sll.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0x9", EFI, 0 },
    { "lv.sll.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0xA", EFI, 0 },
    { "lv.sll",    "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0xB", EFI, 0 },
    { "lv.srl.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0xC", EFI, 0 },
    { "lv.srl.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0xD", EFI, 0 },
    { "lv.sra.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0xE", EFI, 0 },
    { "lv.sra.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x6 0xF", EFI, 0 },
    { "lv.srl",    "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x0", EFI, 0 },
    { "lv.sub.b",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x1", EFI, 0 },
    { "lv.sub.h",  "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x2", EFI, 0 },
    { "lv.subs.b", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x3", EFI, 0 },
    { "lv.subs.h", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x4", EFI, 0 },
    { "lv.subu.b", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x5", EFI, 0 },
    { "lv.subu.h", "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x6", EFI, 0 },
    { "lv.subus.b","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x7", EFI, 0 },
    { "lv.subus.h","rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x8", EFI, 0 },
    { "lv.unpack.b","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0x9", EFI, 0 },
    { "lv.unpack.h","rD,rA,rB",    "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0xA", EFI, 0 },
    { "lv.xor",    "rD,rA,rB",     "00 0xA  DDDDD AAAAA BBBB B--- 0x7 0xB", EFI, 0 },
    { "lv.cust1",  "",	       "00 0xA  ----- ----- ---- ---- 0xC ----", EFI, 0 },
    { "lv.cust2",  "",	       "00 0xA  ----- ----- ---- ---- 0xD ----", EFI, 0 },
    { "lv.cust3",  "",	       "00 0xA  ----- ----- ---- ---- 0xE ----", EFI, 0 },
    { "lv.cust4",  "",	       "00 0xA  ----- ----- ---- ---- 0xF ----", EFI, 0 },

    { "lf.add.s",   "rD,rA,rB",    "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x0", EFI, 0 },
    { "lf.sub.s",   "rD,rA,rB",    "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x1", EFI, 0 },
    { "lf.mul.s",   "rD,rA,rB",    "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x2", EFI, 0 },
    { "lf.div.s",   "rD,rA,rB",    "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x3", EFI, 0 },
    { "lf.itof.s",  "rD,rA",       "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x4", EFI, 0 },
    { "lf.ftoi.s",  "rD,rA",       "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x5", EFI, 0 },
    { "lf.rem.s",   "rD,rA,rB",    "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x6", EFI, 0 },
    { "lf.madd.s",  "rD,rA,rB",    "00 0xB  DDDDD AAAAA BBBB B--- 0x1 0x7", EFI, 0 },
    { "lf.sfeq.s",  "rA,rB",       "00 0xB  ----- AAAAA BBBB B--- 0x1 0x8", EFI, 0 },
    { "lf.sfne.s",  "rA,rB",       "00 0xB  ----- AAAAA BBBB B--- 0x1 0x9", EFI, 0 },
    { "lf.sfgt.s",  "rA,rB",       "00 0xB  ----- AAAAA BBBB B--- 0x1 0xA", EFI, 0 },
    { "lf.sfge.s",  "rA,rB",       "00 0xB  ----- AAAAA BBBB B--- 0x1 0xB", EFI, 0 },
    { "lf.sflt.s",  "rA,rB",       "00 0xB  ----- AAAAA BBBB B--- 0x1 0xC", EFI, 0 },
    { "lf.sfle.s",  "rA,rB",       "00 0xB  ----- AAAAA BBBB B--- 0x1 0xD", EFI, 0 },
    { "lf.cust1.s", "",	       "00 0xB  ----- ----- ---- ---- 0xE ----", EFI, 0 },

    { "lf.add.d",   "rD,rA,rB",    "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x0", EFI, 0 },
    { "lf.sub.d",   "rD,rA,rB",    "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x1", EFI, 0 },
    { "lf.mul.d",   "rD,rA,rB",    "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x2", EFI, 0 },
    { "lf.div.d",   "rD,rA,rB",    "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x3", EFI, 0 },
    { "lf.itof.d",  "rD,rA",       "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x4", EFI, 0 },
    { "lf.ftoi.d",  "rD,rA",       "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x5", EFI, 0 },
    { "lf.rem.d",   "rD,rA,rB",    "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x6", EFI, 0 },
    { "lf.madd.d",  "rD,rA,rB",    "00 0xC  DDDDD AAAAA BBBB B--- 0x1 0x7", EFI, 0 },
    { "lf.sfeq.d",  "rA,rB",       "00 0xC  ----- AAAAA BBBB B--- 0x1 0x8", EFI, 0 },
    { "lf.sfne.d",  "rA,rB",       "00 0xC  ----- AAAAA BBBB B--- 0x1 0x9", EFI, 0 },
    { "lf.sfgt.d",  "rA,rB",       "00 0xC  ----- AAAAA BBBB B--- 0x1 0xA", EFI, 0 },
    { "lf.sfge.d",  "rA,rB",       "00 0xC  ----- AAAAA BBBB B--- 0x1 0xB", EFI, 0 },
    { "lf.sflt.d",  "rA,rB",       "00 0xC  ----- AAAAA BBBB B--- 0x1 0xC", EFI, 0 },
    { "lf.sfle.d",  "rA,rB",       "00 0xC  ----- AAAAA BBBB B--- 0x1 0xD", EFI, 0 },
    { "lf.cust1.d", "",	       "00 0xC  ----- ----- ---- ---- 0xE ----", EFI, 0 },

    { "lvf.ld",     "rD,0(rA)",    "00 0xD  DDDDD AAAAA ---- ---- 0x0 0x0", EFI, 0 },
    { "lvf.lw",     "rD,0(rA)",    "00 0xD  DDDDD AAAAA ---- ---- 0x0 0x1", EFI, 0 },
    { "lvf.sd",     "0(rA),rB",    "00 0xD  ----- AAAAA BBBB B--- 0x1 0x0", EFI, 0 },
    { "lvf.sw",     "0(rA),rB",    "00 0xD  ----- AAAAA BBBB B--- 0x1 0x1", EFI, 0 },

    { "l.jr",      "rB",           "01 0x1  ----- ----- BBBB B--- ---- ----", EF(l_jr), OR32_IF_DELAY },
    { "l.jalr",    "rB",           "01 0x2  ----- ----- BBBB B--- ---- ----", EF(l_jalr), OR32_IF_DELAY },
    { "l.maci",    "rB,I",         "01 0x3  IIIII ----- BBBB BIII IIII IIII", EF(l_mac), 0 },
    { "l.cust1",   "",	       "01 0xC  ----- ----- ---- ---- ---- ----", EF(l_cust1), 0 },
    { "l.cust2",   "",	       "01 0xD  ----- ----- ---- ---- ---- ----", EF(l_cust2), 0 },
    { "l.cust3",   "",	       "01 0xE  ----- ----- ---- ---- ---- ----", EF(l_cust3), 0 },
    { "l.cust4",   "",	       "01 0xF  ----- ----- ---- ---- ---- ----", EF(l_cust4), 0 },

    { "l.ld",      "rD,I(rA)",     "10 0x0  DDDDD AAAAA IIII IIII IIII IIII", EFI, 0 },
    { "l.lwz",     "rD,I(rA)",     "10 0x1  DDDDD AAAAA IIII IIII IIII IIII", EF(l_lwz), 0 },
    { "l.lws",     "rD,I(rA)",     "10 0x2  DDDDD AAAAA IIII IIII IIII IIII", EFI, 0 },
    { "l.lbz",     "rD,I(rA)",     "10 0x3  DDDDD AAAAA IIII IIII IIII IIII", EF(l_lbz), 0 },
    { "l.lbs",     "rD,I(rA)",     "10 0x4  DDDDD AAAAA IIII IIII IIII IIII", EF(l_lbs), 0 },
    { "l.lhz",     "rD,I(rA)",     "10 0x5  DDDDD AAAAA IIII IIII IIII IIII", EF(l_lhz), 0 },
    { "l.lhs",     "rD,I(rA)",     "10 0x6  DDDDD AAAAA IIII IIII IIII IIII", EF(l_lhs), 0 },

    { "l.addi",    "rD,rA,I",      "10 0x7  DDDDD AAAAA IIII IIII IIII IIII", EF(l_add), 0 },
    { "l.addic",   "rD,rA,I",      "10 0x8  DDDDD AAAAA IIII IIII IIII IIII", EFI, 0 },
    { "l.andi",    "rD,rA,K",      "10 0x9  DDDDD AAAAA KKKK KKKK KKKK KKKK", EF(l_and), 0 },
    { "l.ori",     "rD,rA,K",      "10 0xA  DDDDD AAAAA KKKK KKKK KKKK KKKK", EF(l_or), 0  },
    { "l.xori",    "rD,rA,I",      "10 0xB  DDDDD AAAAA IIII IIII IIII IIII", EF(l_xor), 0 },
    { "l.muli",    "rD,rA,I",      "10 0xC  DDDDD AAAAA IIII IIII IIII IIII", EFI, 0 },
    { "l.mfspr",   "rD,rA,K",      "10 0xD  DDDDD AAAAA KKKK KKKK KKKK KKKK", EF(l_mfspr), 0 },
    { "l.slli",    "rD,rA,L",      "10 0xE  DDDDD AAAAA ---- ---- 00LL LLLL", EF(l_sll), 0 },
    { "l.srli",    "rD,rA,L",      "10 0xE  DDDDD AAAAA ---- ---- 01LL LLLL", EF(l_srl), 0 },
    { "l.srai",    "rD,rA,L",      "10 0xE  DDDDD AAAAA ---- ---- 10LL LLLL", EF(l_sra), 0 },
    { "l.rori",    "rD,rA,L",      "10 0xE  DDDDD AAAAA ---- ---- 11LL LLLL", EFI, 0 },

    { "l.sfeqi",   "rA,I",         "10 0xF  00000 AAAAA IIII IIII IIII IIII", EF(l_sfeq), OR32_W_FLAG },
    { "l.sfnei",   "rA,I",         "10 0xF  00001 AAAAA IIII IIII IIII IIII", EF(l_sfne), OR32_W_FLAG },
    { "l.sfgtui",  "rA,I",         "10 0xF  00010 AAAAA IIII IIII IIII IIII", EF(l_sfgtu), OR32_W_FLAG },
    { "l.sfgeui",  "rA,I",         "10 0xF  00011 AAAAA IIII IIII IIII IIII", EF(l_sfgeu), OR32_W_FLAG },
    { "l.sfltui",  "rA,I",         "10 0xF  00100 AAAAA IIII IIII IIII IIII", EF(l_sfltu), OR32_W_FLAG },
    { "l.sfleui",  "rA,I",         "10 0xF  00101 AAAAA IIII IIII IIII IIII", EF(l_sfleu), OR32_W_FLAG },
    { "l.sfgtsi",  "rA,I",         "10 0xF  01010 AAAAA IIII IIII IIII IIII", EF(l_sfgts), OR32_W_FLAG },
    { "l.sfgesi",  "rA,I",         "10 0xF  01011 AAAAA IIII IIII IIII IIII", EF(l_sfges), OR32_W_FLAG },
    { "l.sfltsi",  "rA,I",         "10 0xF  01100 AAAAA IIII IIII IIII IIII", EF(l_sflts), OR32_W_FLAG },
    { "l.sflesi",  "rA,I",         "10 0xF  01101 AAAAA IIII IIII IIII IIII", EF(l_sfles), OR32_W_FLAG },

    { "l.mtspr",   "rA,rB,K",      "11 0x0  KKKKK AAAAA BBBB BKKK KKKK KKKK", EF(l_mtspr), 0 },
    { "l.mac",     "rA,rB",        "11 0x1  ----- AAAAA BBBB B--- ---- 0x1", EF(l_mac), 0 }, /*MM*/
    { "l.msb",     "rA,rB",        "11 0x1  ----- AAAAA BBBB B--- ---- 0x2", EF(l_msb), 0 }, /*MM*/

    { "l.sd",      "I(rA),rB",     "11 0x4  IIIII AAAAA BBBB BIII IIII IIII", EFI, 0 },
    { "l.sw",      "I(rA),rB",     "11 0x5  IIIII AAAAA BBBB BIII IIII IIII", EF(l_sw), 0 },
    { "l.sb",      "I(rA),rB",     "11 0x6  IIIII AAAAA BBBB BIII IIII IIII", EF(l_sb), 0 },
    { "l.sh",      "I(rA),rB",     "11 0x7  IIIII AAAAA BBBB BIII IIII IIII", EF(l_sh), 0 },
    
    { "l.add",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x0", EF(l_add), 0 },
    { "l.addc",    "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x1", EFI, 0 },
    { "l.sub",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x2", EF(l_sub), 0 },
    { "l.and",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x3", EF(l_and), 0 },
    { "l.or",      "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x4", EF(l_or), 0 },
    { "l.xor",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x5", EF(l_xor), 0 },
    { "l.mul",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-11 ---- 0x6", EF(l_mul), 0 },

    { "l.sll",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 00-- 0x8", EF(l_sll), 0 },
    { "l.srl",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 01-- 0x8", EF(l_srl), 0 },
    { "l.sra",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 10-- 0x8", EF(l_sra), 0 },
    { "l.ror",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 11-- 0x8", EFI, 0 },
    { "l.div",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0x9", EF(l_div), 0 },
    { "l.divu",    "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0xA", EF(l_divu), 0 },
    { "l.mulu",    "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-11 ---- 0xB", EFI, 0 },
    { "l.exths",   "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 00-- 0xC", EFI, 0 },
    { "l.extbs",   "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 01-- 0xC", EFI, 0 },
    { "l.exthz",   "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 10-- 0xC", EFI, 0 },
    { "l.extbz",   "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 11-- 0xC", EFI, 0 },
    { "l.extws",   "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 00-- 0xD", EFI, 0 },
    { "l.extwz",   "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 01-- 0xD", EFI, 0 },
    { "l.cmov",    "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0xE", EFI, 0 },
    { "l.ff1",     "rD,rA,rB",     "11 0x8  DDDDD AAAAA BBBB B-00 ---- 0xF", EFI, 0 },

    { "l.sfeq",    "rA,rB",        "11 0x9  00000 AAAAA BBBB B--- ---- ----", EF(l_sfeq), OR32_W_FLAG },
    { "l.sfne",    "rA,rB",        "11 0x9  00001 AAAAA BBBB B--- ---- ----", EF(l_sfne), OR32_W_FLAG },
    { "l.sfgtu",   "rA,rB",        "11 0x9  00010 AAAAA BBBB B--- ---- ----", EF(l_sfgtu), OR32_W_FLAG },
    { "l.sfgeu",   "rA,rB",        "11 0x9  00011 AAAAA BBBB B--- ---- ----", EF(l_sfgeu), OR32_W_FLAG },
    { "l.sfltu",   "rA,rB",        "11 0x9  00100 AAAAA BBBB B--- ---- ----", EF(l_sfltu), OR32_W_FLAG },
    { "l.sfleu",   "rA,rB",        "11 0x9  00101 AAAAA BBBB B--- ---- ----", EF(l_sfleu), OR32_W_FLAG },
    { "l.sfgts",   "rA,rB",        "11 0x9  01010 AAAAA BBBB B--- ---- ----", EF(l_sfgts), OR32_W_FLAG },
    { "l.sfges",   "rA,rB",        "11 0x9  01011 AAAAA BBBB B--- ---- ----", EF(l_sfges), OR32_W_FLAG },
    { "l.sflts",   "rA,rB",        "11 0x9  01100 AAAAA BBBB B--- ---- ----", EF(l_sflts), OR32_W_FLAG },
    { "l.sfles",   "rA,rB",        "11 0x9  01101 AAAAA BBBB B--- ---- ----", EF(l_sfles), OR32_W_FLAG },

    { "l.cust5",   "",	       "11 0xC  ----- ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust6",   "",	       "11 0xD  ----- ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust7",   "",	       "11 0xE  ----- ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust8",   "",	       "11 0xF  ----- ----- ---- ---- ---- ----", EFI, 0 },

    /* This section should not be defined in or1ksim, since it contains duplicates,
       which would cause machine builder to complain.  */
#ifdef HAS_CUST
    { "l.cust5_1",   "rD",	       "11 0xC  DDDDD ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust5_2",   "rD,rA"   ,   "11 0xC  DDDDD AAAAA ---- ---- ---- ----", EFI, 0 },
    { "l.cust5_3",   "rD,rA,rB",   "11 0xC  DDDDD AAAAA BBBB B--- ---- ----", EFI, 0 },

    { "l.cust6_1",   "rD",	       "11 0xD  DDDDD ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust6_2",   "rD,rA"   ,   "11 0xD  DDDDD AAAAA ---- ---- ---- ----", EFI, 0 },
    { "l.cust6_3",   "rD,rA,rB",   "11 0xD  DDDDD AAAAA BBBB B--- ---- ----", EFI, 0 },

    { "l.cust7_1",   "rD",	       "11 0xE  DDDDD ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust7_2",   "rD,rA"   ,   "11 0xE  DDDDD AAAAA ---- ---- ---- ----", EFI, 0 },
    { "l.cust7_3",   "rD,rA,rB",   "11 0xE  DDDDD AAAAA BBBB B--- ---- ----", EFI, 0 },

    { "l.cust8_1",   "rD",	       "11 0xF  DDDDD ----- ---- ---- ---- ----", EFI, 0 },
    { "l.cust8_2",   "rD,rA"   ,   "11 0xF  DDDDD AAAAA ---- ---- ---- ----", EFI, 0 },
    { "l.cust8_3",   "rD,rA,rB",   "11 0xF  DDDDD AAAAA BBBB B--- ---- ----", EFI, 0 },
#endif

    /* Dummy entry, not included in num_opcodes.  This
       lets code examine entry i+1 without checking
       if we've run off the end of the table.  */
    { "", "", "", EFI, 0 }
};

#undef EFI
#undef EFN
#undef EF 

/* Define dummy, if debug is not defined.  */

#if !defined HAS_DEBUG
static void debug PARAMS ((int, const char *, ...));

static void
debug (int level, const char *format, ...)
{
  /* Just to get rid of warnings.  */
  format = (char *) level = 0;
}
#endif

const unsigned int or32_num_opcodes = ((sizeof(or32_opcodes)) / (sizeof(struct or32_opcode))) - 1;

/* Calculates instruction length in bytes. Always 4 for OR32.  */

int
insn_len (insn_index)
     int insn_index ATTRIBUTE_UNUSED;
{
  return 4;
}

/* Is individual insn's operand signed or unsigned?  */

int
letter_signed (l)
     char l;
{
  const struct or32_letter *pletter;
  
  for (pletter = or32_letters; pletter->letter != '\0'; pletter++)
    if (pletter->letter == l)
      return pletter->sign;
  
  printf ("letter_signed(%c): Unknown letter.\n", l);
  return 0;
}

/* Number of letters in the individual lettered operand.  */

int
letter_range (l)
     char l;
{
  const struct or32_opcode *pinsn;
  char *enc;
  int range = 0;
  
  for (pinsn = or32_opcodes; strlen(pinsn->name); pinsn++)
    {
      if (strchr (pinsn->encoding,l))
	{
	  for (enc = pinsn->encoding; *enc != '\0'; enc++)
	    if ((*enc == '0') && (*(enc+1) == 'x'))
	      enc += 2;
	    else if (*enc == l)
	      range++;
	  return range;
	}
    }

  printf ("\nABORT: letter_range(%c): Never used letter.\n", l);
  exit (1);
}

/* MM: Returns index of given instruction name.  */

int
insn_index (char *insn)
{
  unsigned int i;
  int found = -1;

  for (i = 0; i < or32_num_opcodes; i++)
    if (!strcmp (or32_opcodes[i].name, insn))
      {
	found = i;
	break;
      }
  return found;
}

const char *
insn_name (index)
     int index;
{
  if (index >= 0 && index < (int) or32_num_opcodes)
    return or32_opcodes[index].name;
  else
    return "???";
}

void
l_none ()
{
}

/* Finite automata for instruction decoding building code.  */

/* Find simbols in encoding.  */
static unsigned long
insn_extract (param_ch, enc_initial)
     char param_ch;
     char *enc_initial;
{
  char *enc;
  unsigned long ret = 0;
  unsigned opc_pos = 32;

  for (enc = enc_initial; *enc != '\0'; )
    if ((*enc == '0') && (*(enc + 1) == 'x')) 
      {
	unsigned long tmp = strtol (enc+2, NULL, 16);

        opc_pos -= 4;
	if (param_ch == '0' || param_ch == '1')
	  {
	    if (param_ch == '0')
	      tmp = 15 - tmp;
	    ret |= tmp << opc_pos;
	  }
        enc += 3;
      }
    else
      {
	if (*enc == '0' || *enc == '1' || *enc == '-' || ISALPHA (*enc))
	  {
	    opc_pos--;
	    if (param_ch == *enc)
	      ret |= 1 << opc_pos;
	  }
	enc++;
      }
  return ret;
}

#define MAX_AUTOMATA_SIZE (1200)
#define MAX_OP_TABLE_SIZE (1200)
#define LEAF_FLAG         (0x80000000)
#define MAX_LEN           (8)

#ifndef MIN
# define MIN(x,y)          ((x) < (y) ? (x) : (y))
#endif

unsigned long *automata;
int nuncovered;
int curpass = 0;

/* MM: Struct that hold runtime build information about instructions.  */
struct temp_insn_struct
{
  unsigned long insn;
  unsigned long insn_mask;
  int in_pass;
} *ti;

struct insn_op_struct *op_data, **op_start;

/* Recursive utility function used to find best match and to build automata.  */

static unsigned long *
cover_insn (cur, pass, mask)
     unsigned long * cur;
     int pass;
     unsigned int mask;
{
  int best_first = 0, last_match = -1, ninstr = 0;
  unsigned int best_len = 0;
  unsigned int i;
  unsigned long cur_mask = mask;
  unsigned long *next;

  for (i = 0; i < or32_num_opcodes; i++)
    if (ti[i].in_pass == pass)
      {
	cur_mask &= ti[i].insn_mask;
	ninstr++;
	last_match = i;
      }
  
  debug (8, "%08X %08X\n", mask, cur_mask);

  if (ninstr == 0)
    return 0;

  if (ninstr == 1)
    {
      /* Leaf holds instruction index.  */
      debug (8, "%i>I%i %s\n",
	     cur - automata, last_match, or32_opcodes[last_match].name);

      *cur = LEAF_FLAG | last_match;
      cur++;
      nuncovered--;
    }
  else
    {
      /* Find longest match.  */
      for (i = 0; i < 32; i++)
	{
	  unsigned int len;

	  for (len = best_len + 1; len < MIN (MAX_LEN, 33 - i); len++)
	    {
	      unsigned long m = (1UL << ((unsigned long)len)) - 1;

	      debug (9, " (%i(%08X & %08X>>%i = %08X, %08X)",
		     len,m, cur_mask, i, (cur_mask >> (unsigned)i),
		     (cur_mask >> (unsigned)i) & m);

	      if ((m & (cur_mask >> (unsigned)i)) == m)
		{
		  best_len = len;
		  best_first = i;
		  debug (9, "!");
		}
	      else
		break;
	    }
	}

      debug (9, "\n");

      if (!best_len)
	{
	  fprintf (stderr, "%i instructions match mask 0x%08X:\n", ninstr, mask);

	  for (i = 0; i < or32_num_opcodes; i++)
	    if (ti[i].in_pass == pass)
	      fprintf (stderr, "%s ", or32_opcodes[i].name);
	  
	  fprintf (stderr, "\n");
	  exit (1);
	}

      debug (8, "%i> #### %i << %i (%i) ####\n",
	     cur - automata, best_len, best_first, ninstr);

      *cur = best_first;
      cur++;
      *cur = (1 << best_len) - 1;
      cur++;
      next = cur;    

      /* Allocate space for pointers.  */
      cur += 1 << best_len;
      cur_mask = (1 << (unsigned long)best_len) - 1;
      
      for (i = 0; i < ((unsigned) 1 << best_len); i++)
	{
	  unsigned int j;
	  unsigned long *c;

	  curpass++;
	  for (j = 0; j < or32_num_opcodes; j++)
	    if (ti[j].in_pass == pass
		&& ((ti[j].insn >> best_first) & cur_mask) == (unsigned long) i
		&& ((ti[j].insn_mask >> best_first) & cur_mask) == cur_mask)
	      ti[j].in_pass = curpass;

	  debug (9, "%08X %08X %i\n", mask, cur_mask, best_first);
	  c = cover_insn (cur, curpass, mask & (~(cur_mask << best_first)));
	  if (c)
	    {
	      debug (8, "%i> #%X -> %u\n", next - automata, i, cur - automata);
	      *next = cur - automata;
	      cur = c;	 
	    }
	  else 
	    {
	      debug (8, "%i> N/A\n", next - automata);
	      *next = 0;
	    }
	  next++;
	}
    }
  return cur;
}

/* Returns number of nonzero bits.  */

static int
num_ones (value)
     unsigned long value;
{
  int c = 0;

  while (value)
    {
      if (value & 1)
	c++;
      value >>= 1;
    }
  return c;
}

/* Utility function, which converts parameters from or32_opcode format to more binary form.  
   Parameters are stored in ti struct.  */

static struct insn_op_struct *
parse_params (opcode, cur)
     const struct or32_opcode * opcode;
     struct insn_op_struct * cur;
{
  char *args = opcode->args;
  int i, type;
  
  i = 0;
  type = 0;
  /* In case we don't have any parameters, we add dummy read from r0.  */

  if (!(*args))
    {
      cur->type = OPTYPE_REG | OPTYPE_OP | OPTYPE_LAST;
      cur->data = 0;
      debug (9, "#%08X %08X\n", cur->type, cur->data);
      cur++;
      return cur;
  }
  
  while (*args != '\0')
    {     
      if (*args == 'r')
	{
	  args++;
	  type |= OPTYPE_REG;
	}
      else if (ISALPHA (*args))
	{
	  unsigned long arg;

	  arg = insn_extract (*args, opcode->encoding);
	  debug (9, "%s : %08X ------\n", opcode->name, arg);
	  if (letter_signed (*args))
	    {
	      type |= OPTYPE_SIG;
	      type |= ((num_ones (arg) - 1) << OPTYPE_SBIT_SHR) & OPTYPE_SBIT;
	    }

	  /* Split argument to sequences of consecutive ones.  */
	  while (arg)
	    {
	      int shr = 0;
	      unsigned long tmp = arg, mask = 0;

	      while ((tmp & 1) == 0)
		{
		  shr++;
		  tmp >>= 1;
		}
	      while (tmp & 1)
		{
		  mask++;
		  tmp >>= 1;
		}
	      cur->type = type | shr;
	      cur->data = mask;
	      arg &= ~(((1 << mask) - 1) << shr);
	      debug (6, "|%08X %08X\n", cur->type, cur->data);
	      cur++;
	    }
	  args++;
	}
      else if (*args == '(')
	{
	  /* Next param is displacement.  Later we will treat them as one operand.  */
	  cur--;
	  cur->type = type | cur->type | OPTYPE_DIS | OPTYPE_OP;
	  debug (9, ">%08X %08X\n", cur->type, cur->data);
	  cur++;
	  type = 0;
	  i++;
	  args++;
	}
      else if (*args == OPERAND_DELIM)
	{
	  cur--;
	  cur->type = type | cur->type | OPTYPE_OP;
	  debug (9, ">%08X %08X\n", cur->type, cur->data);
	  cur++;
	  type = 0;
	  i++;
	  args++;
	}
      else if (*args == '0')
	{
	  cur->type = type;
	  cur->data = 0;
	  debug (9, ">%08X %08X\n", cur->type, cur->data);
	  cur++;
	  type = 0;
	  i++;
	  args++;
	}
      else if (*args == ')')
	args++;
      else
	{
	  fprintf (stderr, "%s : parse error in args.\n", opcode->name);
	  exit (1);
	}
    }

  cur--;
  cur->type = type | cur->type | OPTYPE_OP | OPTYPE_LAST;
  debug (9, "#%08X %08X\n", cur->type, cur->data);
  cur++;

  return cur;
}

/* Constructs new automata based on or32_opcodes array.  */

void
build_automata ()
{
  unsigned int i;
  unsigned long *end;
  struct insn_op_struct *cur;
  
  automata = (unsigned long *) malloc (MAX_AUTOMATA_SIZE * sizeof (unsigned long));
  ti = (struct temp_insn_struct *) malloc (sizeof (struct temp_insn_struct) * or32_num_opcodes);

  nuncovered = or32_num_opcodes;
  printf ("Building automata... ");
  /* Build temporary information about instructions.  */
  for (i = 0; i < or32_num_opcodes; i++)
    {
      unsigned long ones, zeros;
      char *encoding = or32_opcodes[i].encoding;

      ones  = insn_extract('1', encoding);
      zeros = insn_extract('0', encoding);

      ti[i].insn_mask = ones | zeros;
      ti[i].insn = ones;
      ti[i].in_pass = curpass = 0;

      /*debug(9, "%s: %s %08X %08X\n", or32_opcodes[i].name,
	or32_opcodes[i].encoding, ti[i].insn_mask, ti[i].insn);*/
    }
  
  /* Until all are covered search for best criteria to separate them.  */
  end = cover_insn (automata, curpass, 0xFFFFFFFF);

  if (end - automata > MAX_AUTOMATA_SIZE)
    {
      fprintf (stderr, "Automata too large. Increase MAX_AUTOMATA_SIZE.");
      exit (1);
    }

  printf ("done, num uncovered: %i/%i.\n", nuncovered, or32_num_opcodes);
  printf ("Parsing operands data... ");

  op_data = (struct insn_op_struct *) malloc (MAX_OP_TABLE_SIZE * sizeof (struct insn_op_struct));
  op_start = (struct insn_op_struct **) malloc (or32_num_opcodes * sizeof (struct insn_op_struct *));
  cur = op_data;

  for (i = 0; i < or32_num_opcodes; i++)
    {
      op_start[i] = cur;
      cur = parse_params (&or32_opcodes[i], cur);

      if (cur - op_data > MAX_OP_TABLE_SIZE)
	{
	  fprintf (stderr, "Operands table too small, increase MAX_OP_TABLE_SIZE.\n");
	  exit (1);
	}
    }
  printf ("done.\n");
}

void
destruct_automata ()
{
  free (ti);
  free (automata);
  free (op_data);
  free (op_start);
}

/* Decodes instruction and returns instruction index.  */

int
insn_decode (insn)
     unsigned int insn;
{
  unsigned long *a = automata;
  int i;

  while (!(*a & LEAF_FLAG))
    {
      unsigned int first = *a;

      debug (9, "%i ", a - automata);

      a++;
      i = (insn >> first) & *a;
      a++;
      if (!*(a + i))
	{
	  /* Invalid instruction found?  */
	  debug (9, "XXX\n", i);
	  return -1;
	}
      a = automata + *(a + i);
    }

  i = *a & ~LEAF_FLAG;

  debug (9, "%i\n", i);

  /* Final check - do we have direct match?
     (based on or32_opcodes this should be the only possibility,
     but in case of invalid/missing instruction we must perform a check)  */
  if ((ti[i].insn_mask & insn) == ti[i].insn) 
    return i;
  else
    return -1;
}

static char disassembled_str[50];
char *disassembled = &disassembled_str[0];

/* Automagically does zero- or sign- extension and also finds correct
   sign bit position if sign extension is correct extension. Which extension
   is proper is figured out from letter description.  */
   
static unsigned long
extend_imm (imm, l)
     unsigned long imm;
     char l;
{
  unsigned long mask;
  int letter_bits;
  
  /* First truncate all bits above valid range for this letter
     in case it is zero extend.  */
  letter_bits = letter_range (l);
  mask = (1 << letter_bits) - 1;
  imm &= mask;
  
  /* Do sign extend if this is the right one.  */
  if (letter_signed(l) && (imm >> (letter_bits - 1)))
    imm |= (~mask);

  return imm;
}

static unsigned long
or32_extract (param_ch, enc_initial, insn)
     char param_ch;
     char *enc_initial;
     unsigned long insn;
{
  char *enc;
  unsigned long ret = 0;
  int opc_pos = 0;
  int param_pos = 0;

  for (enc = enc_initial; *enc != '\0'; enc++)
    if (*enc == param_ch)
      {
        if (enc - 2 >= enc_initial && (*(enc - 2) == '0') && (*(enc - 1) == 'x'))
      	  continue;
        else
          param_pos++;
      }

#if DEBUG
  printf ("or32_extract: %x ", param_pos);
#endif
  opc_pos = 32;

  for (enc = enc_initial; *enc != '\0'; )
    if ((*enc == '0') && (*(enc + 1) == 'x')) 
      {
        opc_pos -= 4;
        if ((param_ch == '0') || (param_ch == '1')) 
          {
            unsigned long tmp = strtol (enc, NULL, 16);
#if DEBUG
            printf (" enc=%s, tmp=%x ", enc, tmp);
#endif
            if (param_ch == '0')
              tmp = 15 - tmp;
            ret |= tmp << opc_pos;
          }
        enc += 3;
      }
    else if ((*enc == '0') || (*enc == '1')) 
      {
        opc_pos--;
        if (param_ch == *enc)
          ret |= 1 << opc_pos;
        enc++;
      }
    else if (*enc == param_ch) 
      {
        opc_pos--;
        param_pos--;
#if DEBUG
        printf ("\n  ret=%x opc_pos=%x, param_pos=%x\n", ret, opc_pos, param_pos);
#endif  
        if (ISLOWER (param_ch))
          ret -= ((insn >> opc_pos) & 0x1) << param_pos;
        else
          ret += ((insn >> opc_pos) & 0x1) << param_pos;
        enc++;
      }
    else if (ISALPHA (*enc)) 
      {
        opc_pos--;
        enc++;
      }
    else if (*enc == '-') 
      {
        opc_pos--;
        enc++;
      }
    else
      enc++;

#if DEBUG
  printf ("ret=%x\n", ret);
#endif
  return ret;
}

/* Print register. Used only by print_insn.  */

static void
or32_print_register (param_ch, encoding, insn)
     char param_ch;
     char *encoding;
     unsigned long insn;
{
  int regnum = or32_extract(param_ch, encoding, insn);
  
  sprintf (disassembled, "%sr%d", disassembled, regnum);
}

/* Print immediate. Used only by print_insn.  */

static void
or32_print_immediate (param_ch, encoding, insn)
     char param_ch;
     char *encoding;
     unsigned long insn;
{
  int imm = or32_extract (param_ch, encoding, insn);

  imm = extend_imm (imm, param_ch);
  
  if (letter_signed (param_ch))
    {
      if (imm < 0)
        sprintf (disassembled, "%s%d", disassembled, imm);
      else
        sprintf (disassembled, "%s0x%x", disassembled, imm);
    }
  else
    sprintf (disassembled, "%s%#x", disassembled, imm);
}

/* Disassemble one instruction from insn to disassemble.
   Return the size of the instruction.  */

int
disassemble_insn (insn)
     unsigned long insn;
{
  int index;
  index = insn_decode (insn);

  if (index >= 0)
    {
      struct or32_opcode const *opcode = &or32_opcodes[index];
      char *s;

      sprintf (disassembled, "%s ", opcode->name);
      for (s = opcode->args; *s != '\0'; ++s)
        {
          switch (*s)
            {
            case '\0':
              return 4;
  
            case 'r':
              or32_print_register (*++s, opcode->encoding, insn);
              break;
  
            default:
              if (strchr (opcode->encoding, *s))
                or32_print_immediate (*s, opcode->encoding, insn);
              else
                sprintf (disassembled, "%s%c", disassembled, *s);
            }
        }
    }
  else
    {
      /* This used to be %8x for binutils.  */
      sprintf (disassembled, "%s.word 0x%08lx", disassembled, insn);
    }

  return insn_len (insn);
}
