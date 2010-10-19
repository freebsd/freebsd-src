/* tc-ip2k.h -- Header file for tc-ip2k.c.
   Copyright (C) 2000, 2002, 2005 Free Software Foundation, Inc.

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

#define TC_IP2K

#define LISTING_HEADER "IP2xxx GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_ip2k

#define TARGET_FORMAT "elf32-ip2k"

#define TARGET_BYTES_BIG_ENDIAN 1

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* .-foo gets turned into PC relative relocs.  */
#define DIFF_EXPR_OK

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define LITERAL_PREFIXDOLLAR_HEX
#define LITERAL_PREFIXPERCENT_BIN
#define DOUBLESLASH_LINE_COMMENTS

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define md_apply_fix ip2k_apply_fix

#define TC_HANDLES_FX_DONE

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

#define TC_FORCE_RELOCATION(FIX) ip2k_force_relocation (FIX)
extern int ip2k_force_relocation (struct fix *);

#define tc_gen_reloc gas_cgen_tc_gen_reloc

#define md_elf_section_flags ip2k_elf_section_flags
extern int ip2k_elf_section_flags (int, int, int);

#define md_operand(x) gas_cgen_md_operand (x)
extern void gas_cgen_md_operand (expressionS *);
