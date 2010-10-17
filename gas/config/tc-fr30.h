/* tc-fr30.h -- Header file for tc-fr30.c.
   Copyright 1998, 1999, 2000, 2001, 2002, 2003
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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#define TC_FR30

#ifndef BFD_ASSEMBLER
/* leading space so will compile with cc */
 #error FR30 support requires BFD_ASSEMBLER
#endif

#define LISTING_HEADER "FR30 GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_fr30

#define TARGET_FORMAT "elf32-fr30"

#define TARGET_BYTES_BIG_ENDIAN 1

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define md_apply_fix3 gas_cgen_md_apply_fix3

#define tc_fix_adjustable(FIX) fr30_fix_adjustable (FIX)
struct fix;
extern bfd_boolean fr30_fix_adjustable PARAMS ((struct fix *));

#define tc_gen_reloc gas_cgen_tc_gen_reloc

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

/* For 8 vs 16 vs 32 bit branch selection.  */
#define TC_GENERIC_RELAX_TABLE md_relax_table
extern const struct relax_type md_relax_table[];

/* We need a special version of the TC_START_LABEL macro so that we
   allow the LDI:8, LDI:20, LDI:32 and delay slot instructions to be
   parsed as such.  Note - in a HORRIBLE HACK, we make use of the
   knowledge that this marco is only ever evaluated in one place
   (read_a_source_file in read.c) where we can access the local
   variable 's' - the start of the symbol that was terminated by
   'character'.  Also we need to be able to change the contents of
   the local variable 'c' which is passed to this macro as 'character'.  */
#define TC_START_LABEL(character, i_l_p)			\
  ((character) != ':' ? 0 : (character = fr30_is_colon_insn (s)) ? 0 : ((character = ':'), 1))
extern char fr30_is_colon_insn PARAMS ((char *));
