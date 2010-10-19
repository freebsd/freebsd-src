/* tc-mt.h -- Header file for tc-mt.c.
   Copyright (C) 2005 Free Software Foundation, Inc.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA. */

#define TC_MT

#define LISTING_HEADER "MT GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_mt

#define TARGET_FORMAT "elf32-mt"

#define TARGET_BYTES_BIG_ENDIAN 1

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* .-foo gets turned into PC relative relocs.  */
#define DIFF_EXPR_OK

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* All mt instructions are multiples of 32 bits.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 4

#define LITERAL_PREFIXDOLLAR_HEX
#define LITERAL_PREFIXPERCENT_BIN

#define md_apply_fix mt_apply_fix
extern void mt_apply_fix (struct fix *, valueT *, segT); 

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section (FIXP, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

#define obj_fix_adjustable(fixP) iq2000_fix_adjustable (fixP)
extern bfd_boolean mt_fix_adjustable (struct fix *);

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define tc_gen_reloc gas_cgen_tc_gen_reloc

#define md_operand(x) gas_cgen_md_operand (x)
extern void gas_cgen_md_operand (expressionS *);

#define TC_FORCE_RELOCATION(fixp) mt_force_relocation (fixp)
extern int mt_force_relocation (struct fix *);

#define tc_fix_adjustable(fixP) mt_fix_adjustable (fixP)
extern bfd_boolean mt_fix_adjustable (struct fix *);

