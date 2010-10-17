/* tc-openrisc.h -- Header file for tc-openrisc.c.
   Copyright 2001, 2002, 2003 Free Software Foundation, Inc.

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
   Boston, MA 02111-1307, USA.  */

#define TC_OPENRISC

#ifndef BFD_ASSEMBLER
/* leading space so will compile with cc */
#  error OPENRISC support requires BFD_ASSEMBLER
#endif

#define LISTING_HEADER "OpenRISC GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_openrisc

extern unsigned long openrisc_machine;
#define TARGET_MACH (openrisc_machine)

#define TARGET_FORMAT		"elf32-openrisc"
#define TARGET_BYTES_BIG_ENDIAN	1

extern const char openrisc_comment_chars [];
#define tc_comment_chars openrisc_comment_chars

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB	1

#define DIFF_EXPR_OK	1	/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define md_apply_fix3 gas_cgen_md_apply_fix3

extern bfd_boolean openrisc_fix_adjustable PARAMS ((struct fix *));
#define tc_fix_adjustable(FIX) openrisc_fix_adjustable (FIX)

#define tc_gen_reloc gas_cgen_tc_gen_reloc

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)

/* For 8 vs 16 vs 32 bit branch selection.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table
