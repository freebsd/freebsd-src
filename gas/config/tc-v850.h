/* tc-v850.h -- Header file for tc-v850.c.
   Copyright 1996, 1997, 1998, 2000, 2001, 2002
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
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_V850

#include "elf/v850.h"

#define TARGET_BYTES_BIG_ENDIAN 0

#ifndef BFD_ASSEMBLER
 #error V850 support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH 		bfd_arch_v850

/* The target BFD format.  */
#define TARGET_FORMAT 		"elf32-v850"

#define md_operand(x)

#define tc_fix_adjustable(FIX) v850_fix_adjustable (FIX)
extern bfd_boolean v850_fix_adjustable PARAMS ((struct fix *));

#define TC_FORCE_RELOCATION(FIX) v850_force_relocation(FIX)
extern int v850_force_relocation PARAMS ((struct fix *));

#ifdef OBJ_ELF
/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0
#endif

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* foo-. gets turned into PC relative relocs.  */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars number_to_chars_littleendian

/* We need to handle lo(), hi(), etc etc in .hword, .word, etc
   directives, so we have to parse "cons" expressions ourselves.  */
#define TC_PARSE_CONS_EXPRESSION(EXP, NBYTES) parse_cons_expression_v850 (EXP)
extern void parse_cons_expression_v850 PARAMS ((expressionS *));

#define TC_CONS_FIX_NEW cons_fix_new_v850
extern void cons_fix_new_v850 PARAMS ((fragS *, int, int, expressionS *));

#define TC_GENERIC_RELAX_TABLE md_relax_table
extern const struct relax_type md_relax_table[];

/* When relaxing, we need to generate
   relocations for alignment directives.  */
#define HANDLE_ALIGN(frag) v850_handle_align (frag)
extern void v850_handle_align PARAMS ((fragS *));

#define MD_PCREL_FROM_SECTION(FIX, SEC) v850_pcrel_from_section (FIX, SEC)
extern long v850_pcrel_from_section PARAMS ((struct fix *, asection *));

#define DWARF2_LINE_MIN_INSN_LENGTH 2
