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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Generator of tests for insns introduced in AM33 2.0.
  
   See the following file for usage and documentation.  */
#include "../all/test-gen.c"

/* Define any char*[]s you may need here.  */
char *named_regs[] = { "a", "b", "c", "d" };

/* Define helper macros to generate register accesses here.  */
#define namedregs(shift) \
  reg_r (named_regs, shift, 0x3, mk_get_bits (2u))
#define numberedregs(shift) \
  reg_p ("f", shift, mk_get_bits (2u))

/* Define helper functions here.  */
int
jmp_cond (func_arg * arg, insn_data * data)
#define jmp_cond(shift) { jmp_cond, { i1: shift } }
{
  static const char conds[4][2] = { "z", "n", "g", "l" };
  unsigned val = get_bits (2u);

  data->as_in = data->dis_out = strdup (conds[val]);
  data->bits = val << arg->i1;

  /* Do not forget to return 0, otherwise the insn will be skipped.  */
  return 0;
}

/* Define convenience wrappers to define_insn.  */
#define cond_jmp_insn(insname, word, funcs...) \
  define_insn (insname, \
	       insn_size_bits (insname, 1, word), \
	       jmp_cond (4), \
	       tab, \
	       ## funcs)

/* Define insns.  */
cond_jmp_insn (j, 0x40, numberedregs(2), comma, namedregs (0));

/* Define an insn group.  */
func *jmp_insns[] =
  {
    insn (j),
    0
  };

/* Define the set of all groups.  */
group_t
groups[] =
  {
    { "jumps", jmp_insns },
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
	 "#name: Foo Digital Processor\n"
	 "#as: -mfood\n"
	 "\n"
	 "# Test the instructions of FooD\n"
	 "\n"
	 ".*: +file format.*food.*\n"
	 "\n"
	 "Disassembly of section .text:\n",
	 dis_out);

  /* Now emit all (selected) insns.  */
  output_groups (groups, as_in, dis_out);

  exit (0);
}
