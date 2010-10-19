/* tc-xstormy16.h -- Header file for tc-xstormy16.c.
   Copyright 2000, 2001, 2002, 2005 Free Software Foundation, Inc.

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
   the Free Software Foundation, 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#define TC_XSTORMY16

#define LISTING_HEADER "XSTORMY16 GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_xstormy16

#define TARGET_FORMAT "elf32-xstormy16"

#define TARGET_BYTES_BIG_ENDIAN 0

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* foo-. gets turned into PC relative relocs.  */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define md_apply_fix xstormy16_md_apply_fix

#define tc_fix_adjustable(FIX) xstormy16_fix_adjustable (FIX)
extern bfd_boolean xstormy16_fix_adjustable (struct fix *);

#define TC_FORCE_RELOCATION(fix) xstormy16_force_relocation (fix)
extern int xstormy16_force_relocation (struct fix *);

#define TC_HANDLES_FX_DONE

#define tc_gen_reloc gas_cgen_tc_gen_reloc

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

#define TC_CONS_FIX_NEW xstormy16_cons_fix_new
extern void xstormy16_cons_fix_new (fragS *f, int, int, expressionS *);

#define md_cgen_record_fixup_exp  xstormy16_cgen_record_fixup_exp

/* Minimum instruction is two bytes.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2
