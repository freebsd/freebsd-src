/* Copyright (C) 2000, 2003 Free Software Foundation
   Contributed by Alexandre Oliva <aoliva@cygnus.com>

   This file is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Generator of tests for Maverick.

   See the following file for usage and documentation.  */
#include "../all/test-gen.c"

/* These are the ARM registers.  Some of them have canonical names
   other than r##, so we'll use both in the asm input, but only the
   canonical names in the expected disassembler output.  */
char *arm_regs[] =
  {
    /* Canonical names.  */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "sl", "fp", "ip", "sp", "lr", "pc",
    /* Alternate names, i.e., those that can be used in the assembler,
     * but that will never be emitted by the disassembler.  */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
  };

/* The various types of registers: ARM's registers, Maverick's
   f/d/fx/dx registers, Maverick's accumulators and Maverick's
   status register.  */
#define armreg(shift) \
  reg_r (arm_regs, shift, 0xf, mk_get_bits (5u))
#define mvreg(prefix, shift) \
  reg_p ("mv" prefix, shift, mk_get_bits (4u))
#define acreg(shift) \
  reg_p ("mvax", shift, mk_get_bits (2u))
#define dspsc \
  literal ("dspsc"), tick_random

/* This outputs the condition flag that may follow each ARM insn.
   Since the condition 15 is invalid, we use it to check that the
   assembler recognizes the absence of a condition as `al'.  However,
   the disassembler won't ever output `al', so, if we emit it in the
   assembler, expect the condition to be omitted in the disassembler
   output.  */

int
arm_cond (func_arg * arg, insn_data * data)
#define arm_cond { arm_cond }
{
  static const char conds[16][3] =
    {
      "eq", "ne", "cs", "cc",
      "mi", "pl", "vs", "vc",
      "hi", "ls", "ge", "lt",
      "gt", "le", "al", ""
    };
  unsigned val = get_bits (4u);

  data->as_in = data->dis_out = strdup (conds[val]);
  if (val == 14)
    data->dis_out = strdup ("");
  data->bits = (val == 15 ? 14 : val) << 28;
  return 0;
}

/* The sign of an offset is actually used to determined whether the
   absolute value of the offset should be added or subtracted, so we
   must adjust negative values so that they do not overflow: -1024 is
   not valid, but -0 is distinct from +0.  */
int
off8s (func_arg * arg, insn_data * data)
#define off8s { off8s }
{
  int val;
  char value[9];

  /* Zero values are problematical.
     The assembler performs translations on the addressing modes
     for these values, meaning that we cannot just recreate the
     disassembler string in the LDST macro without knowing what
     value had been generated in off8s.  */
  do
    {
      val  = get_bits (9s);
    }
  while (val == -1 || val == 0);
  
  val <<= 2;
  if (val < 0)
    {
      val = -4 - val;
      sprintf (value, ", #-%i", val);
      data->dis_out = strdup (value);
      sprintf (value, ", #-%i", val);
      data->as_in = strdup (value);
      data->bits = val >> 2;
    }
  else
    {
      sprintf (value, ", #%i", val);
      data->as_in = data->dis_out = strdup (value);
      data->bits = (val >> 2) | (1 << 23);
    }
  
  return 0;
}

/* This function generates a 7-bit signed constant, emitted as
   follows: the 4 least-significant bits are stored in the 4
   least-significant bits of the word; the 3 most-significant bits are
   stored in bits 7:5, i.e., bit 4 is skipped.  */
int
imm7 (func_arg *arg, insn_data *data)
#define imm7 { imm7 }
{
  int val = get_bits (7s);
  char value[6];

  data->bits = (val & 0x0f) | (2 * (val & 0x70));
  sprintf (value, "#%i", val);
  data->as_in = data->dis_out = strdup (value);
  return 0;
}

/* Convenience wrapper to define_insn, that prefixes every insn with
   `cf' (so, if you specify command-line arguments, remember that `cf'
   must *not* be part of the string), and post-fixes a condition code.
   insname and insnvar specify the main insn name and a variant;
   they're just concatenated, and insnvar is often empty.  word is the
   bit pattern that defines the insn, properly shifted, and funcs is a
   sequence of funcs that define the operands and the syntax of the
   insn.  */
#define mv_insn(insname, insnvar, word, funcs...) \
  define_insn (insname ## insnvar, \
	      literal ("cf"), \
	      insn_bits (insname, word), \
	      arm_cond, \
	      tab, \
	      ## funcs)

/* Define a single LDC/STC variant.  op is the main insn opcode; ld
   stands for load (it should be 0 on stores), dword selects 64-bit
   operations, pre should be enabled for pre-increment, and wb, for
   write-back.  sep1, sep2 and sep3 are syntactical elements ([]!)
   that the assembler will use to enable pre and wb.  It would
   probably have been cleaner to couple the syntactical elements with
   the pre/wb bits directly, but it would have required the definition
   of more functions.  */
#define LDST(insname, insnvar, op, ld, dword, regname, pre, wb, sep1, sep2, sep3) \
  mv_insn (insname, insnvar, \
	   (12 << 24) | (op << 8) | (ld << 20) | (pre << 24) | (dword << 22) | (wb << 21), \
	    mvreg (regname, 12), comma, \
	    lsqbkt, armreg (16), sep1, off8s, sep2, sep3, \
	    tick_random)

/* Define all variants of an LDR or STR instruction, namely,
   pre-indexed without write-back, pre-indexed with write-back and
   post-indexed.  */
#define LDSTall(insname, op, ld, dword, regname) \
  LDST (insname, _p, op, ld, dword, regname, 1, 0, nothing, rsqbkt, nothing); \
  LDST (insname, _pw, op, ld, dword, regname, 1, 1, nothing, rsqbkt, literal ("!")); \
  LDST (insname, ,op, ld, dword, regname, 0, 1, rsqbkt, nothing, nothing)

/* Produce the insn identifiers of all LDST variants of a given insn.
   To be used in the initialization of an insn group array.  */
#define insns_LDSTall(insname) \
  insn (insname ## _p), insn (insname ## _pw), insn (insname)

/* Define a CDP variant that uses two registers, at offsets 12 and 16.
   The two opcodes and the co-processor number identify the CDP
   insn.  */
#define CDP2(insname, var, cpnum, opcode1, opcode2, reg1name, reg2name) \
  mv_insn (insname##var, , \
	   (14 << 24) | ((opcode1) << 20) | ((cpnum) << 8) | ((opcode2) << 5), \
	   mvreg (reg1name, 12), comma, mvreg (reg2name, 16))

/* Define a 32-bit integer CDP instruction with two operands.  */
#define CDP2fx(insname, opcode1, opcode2) \
  CDP2 (insname, 32, 5, opcode1, opcode2, "fx", "fx")

/* Define a 64-bit integer CDP instruction with two operands.  */
#define CDP2dx(insname, opcode1, opcode2) \
  CDP2 (insname, 64, 5, opcode1, opcode2, "dx", "dx")

/* Define a float CDP instruction with two operands.  */
#define CDP2f(insname, opcode1, opcode2) \
  CDP2 (insname, s, 4, opcode1, opcode2, "f", "f")

/* Define a double CDP instruction with two operands.  */
#define CDP2d(insname, opcode1, opcode2) \
  CDP2 (insname, d, 4, opcode1, opcode2, "d", "d")

/* Define a CDP instruction with two register operands and one 7-bit
   signed immediate generated with imm7.  */
#define CDP2_imm7(insname, cpnum, opcode1, reg1name, reg2name) \
  mv_insn (insname, , (14 << 24) | ((opcode1) << 20) | ((cpnum) << 8), \
	   mvreg (reg1name, 12), comma, mvreg (reg2name, 16), comma, imm7, \
	   tick_random)

/* Produce the insn identifiers of CDP floating-point or integer insn
   pairs (i.e., it appends the suffixes for 32-bit and 64-bit
   insns.  */
#define CDPfp_insns(insname) \
  insn (insname ## s), insn (insname ## d)
#define CDPx_insns(insname) \
  insn (insname ## 32), insn (insname ## 64)

/* Define a CDP instruction with 3 operands, at offsets 12, 16, 0.  */
#define CDP3(insname, var, cpnum, opcode1, opcode2, reg1name, reg2name, reg3name) \
  mv_insn (insname##var, , \
	   (14 << 24) | ((opcode1) << 20) | ((cpnum) << 8) | ((opcode2) << 5), \
	   mvreg (reg1name, 12), comma, mvreg (reg2name, 16), comma, \
	   mvreg (reg3name, 0), tick_random)

/* Define a 32-bit integer CDP instruction with three operands.  */
#define CDP3fx(insname, opcode1, opcode2) \
  CDP3 (insname, 32, 5, opcode1, opcode2, "fx", "fx", "fx")

/* Define a 64-bit integer CDP instruction with three operands.  */
#define CDP3dx(insname, opcode1, opcode2) \
  CDP3 (insname, 64, 5, opcode1, opcode2, "dx", "dx", "dx")

/* Define a float CDP instruction with three operands.  */
#define CDP3f(insname, opcode1, opcode2) \
  CDP3 (insname, s, 4, opcode1, opcode2, "f", "f", "f")

/* Define a double CDP instruction with three operands.  */
#define CDP3d(insname, opcode1, opcode2) \
  CDP3 (insname, d, 4, opcode1, opcode2, "d", "d", "d")

/* Define a CDP instruction with four operands, at offsets 5, 12, 16
 * and 0.  Used only for ACC instructions.  */
#define CDP4(insname, opcode1, reg2spec, reg3name, reg4name) \
  mv_insn (insname, , (14 << 24) | ((opcode1) << 20) | (6 << 8), \
	   acreg (5), comma, reg2spec, comma, \
	   mvreg (reg3name, 16), comma, mvreg (reg4name, 0))

/* Define a CDP4 instruction with one accumulator operands.  */
#define CDP41A(insname, opcode1) \
  CDP4 (insname, opcode1, mvreg ("fx", 12), "fx", "fx")

/* Define a CDP4 instruction with two accumulator operands.  */
#define CDP42A(insname, opcode1) \
  CDP4 (insname, opcode1, acreg (12), "fx", "fx")

/* Define a MCR or MRC instruction with two register operands.  */
#define MCRC2(insname, cpnum, opcode1, dir, opcode2, reg1spec, reg2spec) \
  mv_insn (insname, , \
	   ((14 << 24) | ((opcode1) << 21) | ((dir) << 20)| \
	    ((cpnum) << 8) | ((opcode2) << 5) | (1 << 4)), \
	   reg1spec, comma, reg2spec)

/* Define a move from a DSP register to an ARM register.  */
#define MVDSPARM(insname, cpnum, opcode2, regDSPname) \
  MCRC2 (mv ## insname, cpnum, 0, 0, opcode2, \
	 mvreg (regDSPname, 16), armreg (12))

/* Define a move from an ARM register to a DSP register.  */
#define MVARMDSP(insname, cpnum, opcode2, regDSPname) \
  MCRC2 (mv ## insname, cpnum, 0, 1, opcode2, \
	 armreg (12), mvreg (regDSPname, 16))

/* Move between coprocessor registers. A two operand CDP insn.   */
#define MCC2(insname, opcode1, opcode2, reg1spec, reg2spec) \
  mv_insn (insname, , \
	   ((14 << 24) | ((opcode1) << 20) | \
	    (4 << 8) | ((opcode2) << 5)), \
	   reg1spec, comma, reg2spec)

/* Define a move from a DSP register to a DSP accumulator.  */
#define MVDSPACC(insname, opcode2, regDSPname) \
  MCC2 (mv ## insname, 2, opcode2, acreg (12), mvreg (regDSPname, 16))

/* Define a move from a DSP accumulator to a DSP register.  */
#define MVACCDSP(insname, opcode2, regDSPname) \
  MCC2 (mv ## insname, 1, opcode2, mvreg (regDSPname, 12), acreg (16))

/* Define move insns between a float DSP register and an ARM
   register.  */
#define MVf(nameAD, nameDA, opcode2) \
  MVDSPARM (nameAD, 4, opcode2, "f"); \
  MVARMDSP (nameDA, 4, opcode2, "f")

/* Define move insns between a double DSP register and an ARM
   register.  */
#define MVd(nameAD, nameDA, opcode2) \
  MVDSPARM (nameAD, 4, opcode2, "d"); \
  MVARMDSP (nameDA, 4, opcode2, "d")

/* Define move insns between a 32-bit integer DSP register and an ARM
   register.  */
#define MVfx(nameAD, nameDA, opcode2) \
  MVDSPARM (nameAD, 5, opcode2, "fx"); \
  MVARMDSP (nameDA, 5, opcode2, "fx")

/* Define move insns between a 64-bit integer DSP register and an ARM
   register.  */
#define MVdx(nameAD, nameDA, opcode2) \
  MVDSPARM (nameAD, 5, opcode2, "dx"); \
  MVARMDSP (nameDA, 5, opcode2, "dx")

/* Define move insns between a 32-bit DSP register and a DSP
   accumulator.  */
#define MVfxa(nameFA, nameAF, opcode2) \
  MVDSPACC (nameFA, opcode2, "fx"); \
  MVACCDSP (nameAF, opcode2, "fx")

/* Define move insns between a 64-bit DSP register and a DSP
   accumulator.  */
#define MVdxa(nameDA, nameAD, opcode2) \
  MVDSPACC (nameDA, opcode2, "dx"); \
  MVACCDSP (nameAD, opcode2, "dx")

/* Produce the insn identifiers for a pair of mv insns.  */
#define insns_MV(name1, name2) \
  insn (mv ## name1), insn (mv ## name2)

/* Define a MCR or MRC instruction with three register operands.  */
#define MCRC3(insname, cpnum, opcode1, dir, opcode2, reg1spec, reg2spec, reg3spec) \
  mv_insn (insname, , \
	   ((14 << 24) | ((opcode1) << 21) | ((dir) << 20)| \
	    ((cpnum) << 8) | ((opcode2) << 5) | (1 << 4)), \
	   reg1spec, comma, reg2spec, comma, reg3spec, \
	   tick_random)

/* Define all load_store insns.  */
LDSTall (ldrs, 4, 1, 0, "f");
LDSTall (ldrd, 4, 1, 1, "d");
LDSTall (ldr32, 5, 1, 0, "fx");
LDSTall (ldr64, 5, 1, 1, "dx");
LDSTall (strs, 4, 0, 0, "f");
LDSTall (strd, 4, 0, 1, "d");
LDSTall (str32, 5, 0, 0, "fx");
LDSTall (str64, 5, 0, 1, "dx");

/* Create the load_store insn group.  */
func *load_store_insns[] =
  {
    insns_LDSTall (ldrs),  insns_LDSTall (ldrd),
    insns_LDSTall (ldr32), insns_LDSTall (ldr64),
    insns_LDSTall (strs),  insns_LDSTall (strd),
    insns_LDSTall (str32), insns_LDSTall (str64),
    0
  };

/* Define all move insns.  */
MVf (sr, rs, 2);
MVd (dlr, rdl, 0);
MVd (dhr, rdh, 1);
MVdx (64lr, r64l, 0);
MVdx (64hr, r64h, 1);
MVfxa (al32, 32al, 2);
MVfxa (am32, 32am, 3);
MVfxa (ah32, 32ah, 4);
MVfxa (a32, 32a, 5);
MVdxa (a64, 64a, 6);
MCC2 (mvsc32, 2, 7, dspsc, mvreg ("dx", 12));
MCC2 (mv32sc, 1, 7, mvreg ("dx", 12), dspsc);
CDP2 (cpys, , 4, 0, 0, "f", "f");
CDP2 (cpyd, , 4, 0, 1, "d", "d");

/* Create the move insns group.  */
func * move_insns[] =
  {
    insns_MV (sr, rs), insns_MV (dlr, rdl), insns_MV (dhr, rdh),
    insns_MV (64lr, r64l), insns_MV (64hr, r64h),
    insns_MV (al32, 32al), insns_MV (am32, 32am), insns_MV (ah32, 32ah),
    insns_MV (a32, 32a), insns_MV (a64, 64a),
    insn (mvsc32), insn (mv32sc), insn (cpys), insn (cpyd),
    0
  };

/* Define all conversion insns.  */
CDP2 (cvtsd, , 4, 0, 3, "d", "f");
CDP2 (cvtds, , 4, 0, 2, "f", "d");
CDP2 (cvt32s, , 4, 0, 4, "f", "fx");
CDP2 (cvt32d, , 4, 0, 5, "d", "fx");
CDP2 (cvt64s, , 4, 0, 6, "f", "dx");
CDP2 (cvt64d, , 4, 0, 7, "d", "dx");
CDP2 (cvts32, , 5, 1, 4, "fx", "f");
CDP2 (cvtd32, , 5, 1, 5, "fx", "d");
CDP2 (truncs32, , 5, 1, 6, "fx", "f");
CDP2 (truncd32, , 5, 1, 7, "fx", "d");

/* Create the conv insns group.  */
func * conv_insns[] =
  {
    insn (cvtsd), insn (cvtds), insn (cvt32s), insn (cvt32d),
    insn (cvt64s), insn (cvt64d), insn (cvts32), insn (cvtd32),
    insn (truncs32), insn (truncd32),
    0
  };

/* Define all shift insns.  */
MCRC3 (rshl32, 5, 0, 0, 2, mvreg ("fx", 16), mvreg ("fx", 0), armreg (12));
MCRC3 (rshl64, 5, 0, 0, 3, mvreg ("dx", 16), mvreg ("dx", 0), armreg (12));
CDP2_imm7 (sh32, 5, 0, "fx", "fx");
CDP2_imm7 (sh64, 5, 2, "dx", "dx");

/* Create the shift insns group.  */
func *shift_insns[] =
  {
    insn (rshl32), insn (rshl64),
    insn (sh32), insn (sh64),
    0
  };

/* Define all comparison insns.  */
MCRC3 (cmps, 4, 0, 1, 4, armreg (12), mvreg ("f", 16), mvreg ("f", 0));
MCRC3 (cmpd, 4, 0, 1, 5, armreg (12), mvreg ("d", 16), mvreg ("d", 0));
MCRC3 (cmp32, 5, 0, 1, 4, armreg (12), mvreg ("fx", 16), mvreg ("fx", 0));
MCRC3 (cmp64, 5, 0, 1, 5, armreg (12), mvreg ("dx", 16), mvreg ("dx", 0));

/* Create the comp insns group.  */
func *comp_insns[] =
  {
    insn (cmps), insn (cmpd),
    insn (cmp32), insn (cmp64),
    0
  };

/* Define all floating-point arithmetic insns.  */
CDP2f (abs, 3, 0);
CDP2d (abs, 3, 1);
CDP2f (neg, 3, 2);
CDP2d (neg, 3, 3);
CDP3f (add, 3, 4);
CDP3d (add, 3, 5);
CDP3f (sub, 3, 6);
CDP3d (sub, 3, 7);
CDP3f (mul, 1, 0);
CDP3d (mul, 1, 1);

/* Create the fp-arith insns group.  */
func *fp_arith_insns[] =
  {
    CDPfp_insns (abs), CDPfp_insns (neg),
    CDPfp_insns (add), CDPfp_insns (sub), CDPfp_insns (mul),
    0
  };

/* Define all integer arithmetic insns.  */
CDP2fx (abs, 3, 0);
CDP2dx (abs, 3, 1);
CDP2fx (neg, 3, 2);
CDP2dx (neg, 3, 3);
CDP3fx (add, 3, 4);
CDP3dx (add, 3, 5);
CDP3fx (sub, 3, 6);
CDP3dx (sub, 3, 7);
CDP3fx (mul, 1, 0);
CDP3dx (mul, 1, 1);
CDP3fx (mac, 1, 2);
CDP3fx (msc, 1, 3);

/* Create the int-arith insns group.  */
func * int_arith_insns[] =
  {
    CDPx_insns (abs), CDPx_insns (neg),
    CDPx_insns (add), CDPx_insns (sub), CDPx_insns (mul),
    insn (mac32), insn (msc32),
    0
  };

/* Define all accumulator arithmetic insns.  */
CDP41A (madd32, 0);
CDP41A (msub32, 1);
CDP42A (madda32, 2);
CDP42A (msuba32, 3);

/* Create the acc-arith insns group.  */
func * acc_arith_insns[] =
  {
    insn (madd32), insn (msub32),
    insn (madda32), insn (msuba32),
    0
  };

/* Create the set of all groups.  */
group_t groups[] =
  {
    { "load_store", load_store_insns },
    { "move", move_insns },
    { "conv", conv_insns },
    { "shift", shift_insns },
    { "comp", comp_insns },
    { "fp_arith", fp_arith_insns },
    { "int_arith", int_arith_insns },
    { "acc_arith", acc_arith_insns },
    { 0 }
  };

int
main (int argc, char *argv[])
{
  FILE *as_in = stdout, *dis_out = stderr;

  /* Check whether we're filtering insns.  */
  if (argc > 1)
    skip_list = argv + 1;

  /* Output assembler header.  */
  fputs ("\t.text\n"
	 "\t.align\n",
	 as_in);
  /* Output comments for the testsuite-driver and the initial
     disassembler output.  */
  fputs ("#objdump: -dr --prefix-address --show-raw-insn\n"
	 "#name: Maverick\n"
	 "#as: -mcpu=ep9312\n"
	 "\n"
	 "# Test the instructions of the Cirrus Maverick floating point co-processor\n"
	 "\n"
	 ".*: +file format.*arm.*\n"
	 "\n"
	 "Disassembly of section .text:\n",
	 dis_out);

  /* Now emit all (selected) insns.  */
  output_groups (groups, as_in, dis_out);

  exit (0);
}
