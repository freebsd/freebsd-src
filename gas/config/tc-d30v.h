/* tc-310v.h -- Header file for tc-d30v.c.
   Copyright 1997, 1998, 2000, 2001, 2002, 2005
   Free Software Foundation, Inc.
   Written by Martin Hunt, Cygnus Support.

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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define TC_D30V

/* The target BFD architecture.  */
#define TARGET_ARCH              bfd_arch_d30v
#define TARGET_FORMAT            "elf32-d30v"
#define TARGET_BYTES_BIG_ENDIAN  1

#define md_operand(x)

/* Call md_pcrel_from_section, not md_pcrel_from.  */
struct fix;
extern long md_pcrel_from_section (struct fix *, segT);
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* .-foo gets turned into PC relative relocs.  */
#define DIFF_EXPR_OK

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars           number_to_chars_bigendian

int d30v_cleanup (int);
#define md_after_pass_hook()	     d30v_cleanup (FALSE)
#define md_cleanup()		     d30v_cleanup (FALSE)
#define TC_START_LABEL(ch, ptr)      (ch == ':' && d30v_cleanup (FALSE))
void d30v_start_line (void);
#define md_start_line_hook()	     d30v_start_line ()

void d30v_frob_label (symbolS *);
#define tc_frob_label(sym)	     d30v_frob_label (sym)

void d30v_cons_align (int);
#define md_cons_align(nbytes)	     d30v_cons_align (nbytes)

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0
