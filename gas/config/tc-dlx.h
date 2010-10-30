/* tc-dlx.h -- Assemble for the DLX
   Copyright 2002, 2003, 2005, 2006 Free Software Foundation, Inc.

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

/* Initially created by Kuang Hwa Lin, 3/20/2002.  */

#define TC_DLX

/* The target BFD architecture.  */
#define TARGET_ARCH              bfd_arch_dlx
#define TARGET_FORMAT            "elf32-dlx"
#define TARGET_BYTES_BIG_ENDIAN	 1

#define WORKING_DOT_WORD

#define LEX_DOLLAR 1

extern void dlx_pop_insert         (void);
extern int set_dlx_skip_hi16_flag  (int);
extern int dlx_unrecognized_line   (int);
extern bfd_boolean md_dlx_fix_adjustable  (struct fix *);

#define md_pop_insert()		        dlx_pop_insert ()

#define md_convert_frag(b,s,f)		as_fatal ("convert_frag called\n")
#define md_estimate_size_before_relax(f,s) \
			(as_fatal ("estimate_size_before_relax called"),1)

#define tc_unrecognized_line(c) dlx_unrecognized_line (c)

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

#define tc_fix_adjustable(FIX) md_dlx_fix_adjustable (FIX)

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* Zero Based Segment?? sound very dangerous to me!     */
#define ZERO_BASED_SEGMENTS

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#undef  LOCAL_LABELS_DOLLAR
#define LOCAL_LABELS_DOLLAR 0

/* .-foo gets turned into PC relative relocs.  */
#define DIFF_EXPR_OK
