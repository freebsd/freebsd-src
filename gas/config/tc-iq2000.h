/* tc-iq2000.h -- Header file for tc-iq2000.c.
   Copyright (C) 2003 Free Software Foundation, Inc.

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

#define TC_IQ2000

#ifndef BFD_ASSEMBLER
/* leading space so will compile with cc */
 #error IQ2000 support requires BFD_ASSEMBLER
#endif

#define LISTING_HEADER "IQ2000 GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_iq2000

#define TARGET_FORMAT "elf32-iq2000"

#define TARGET_BYTES_BIG_ENDIAN 1

/* Permit temporary numeric labels. */
#define LOCAL_LABELS_FB 1

/* .-foo gets turned into PC relative relocs. */
#define DIFF_EXPR_OK

/* We don't need to handle .word strangely. */
#define WORKING_DOT_WORD

#define md_apply_fix3 gas_cgen_md_apply_fix3

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section (FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

#define tc_frob_file() iq2000_frob_file ()
extern void iq2000_frob_file PARAMS ((void));

#define obj_fix_adjustable(fixP) iq2000_fix_adjustable (fixP)
extern bfd_boolean iq2000_fix_adjustable PARAMS ((struct fix *));

/* After creating a fixup for an instruction operand, we need to check
   for HI16 relocs and queue them up for later sorting. */
#define md_cgen_record_fixup_exp  iq2000_cgen_record_fixup_exp

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) iq2000_force_relocation (fix)
extern int iq2000_force_relocation PARAMS ((struct fix *));

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define tc_gen_reloc gas_cgen_tc_gen_reloc

