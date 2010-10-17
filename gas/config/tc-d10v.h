/* tc-d10v.h -- Header file for tc-d10v.c.
   Copyright 1996, 1997, 1998, 2000, 2001, 2002, 2003
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
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_D10V

#define TARGET_BYTES_BIG_ENDIAN 0

#ifndef BFD_ASSEMBLER
 #error D10V support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_d10v

#define TARGET_FORMAT "elf32-d10v"

/* Call md_pcrel_from_section, not md_pcrel_from.  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section(FIX, SEC)
struct fix;
long md_pcrel_from_section PARAMS ((struct fix *, segT));

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars	     number_to_chars_bigendian

int d10v_cleanup PARAMS ((void));
#define md_after_pass_hook()	     d10v_cleanup ()
#define md_cleanup()		     d10v_cleanup ()
#define md_do_align(a,b,c,d,e)	     d10v_cleanup ()
#define tc_frob_label(sym) do {\
  d10v_cleanup (); \
  symbol_set_frag (sym, frag_now);					\
  S_SET_VALUE (sym, (valueT) frag_now_fix ());				\
} while (0)

#define tc_fix_adjustable(FIX) d10v_fix_adjustable(FIX)
bfd_boolean d10v_fix_adjustable PARAMS ((struct fix *));

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

#define md_flush_pending_output  d10v_cleanup
