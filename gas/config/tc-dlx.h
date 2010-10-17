/* tc-dlx.h -- Assemble for the DLX
   Copyright 2002, 2003 Free Software Foundation, Inc.

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

/* Initially created by Kuang Hwa Lin, 3/20/2002.  */

#define TC_DLX

#ifndef BFD_ASSEMBLER
 #error DLX support requires BFD_ASSEMBLER
#endif

#ifndef  __BFD_H_SEEN__
#include "bfd.h"
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_dlx
#define TARGET_FORMAT "elf32-dlx"
#define TARGET_BYTES_BIG_ENDIAN	1

#define WORKING_DOT_WORD

#define LEX_DOLLAR 1

/* #define md_operand(x) */
extern void dlx_pop_insert              PARAMS ((void));
extern int set_dlx_skip_hi16_flag       PARAMS ((int));

#define md_pop_insert()		        dlx_pop_insert ()

#define md_convert_frag(b,s,f)		as_fatal ("convert_frag called\n")
#define md_estimate_size_before_relax(f,s) \
			(as_fatal ("estimate_size_before_relax called"),1)

#define tc_unrecognized_line(c) dlx_unrecognized_line (c)

extern int dlx_unrecognized_line PARAMS ((int));

#define tc_headers_hook(a)		;	/* not used */
#define tc_headers_hook(a)		;	/* not used */
#define tc_crawl_symbol_chain(a)	;	/* not used */
#define tc_coff_symbol_emit_hook(a)	;	/* not used */

#define AOUT_MACHTYPE 101
#define TC_COFF_FIX2RTYPE(fix_ptr) tc_coff_fix2rtype (fix_ptr)
#define BFD_ARCH bfd_arch_dlx
#define COFF_MAGIC DLXMAGIC
/* Should the reloc be output ?
	on the 29k, this is true only if there is a symbol attached.
	on the h8, this is always true, since no fixup is done
        on dlx, I have no idea!! but lets keep it here just for fun.
*/
#define TC_COUNT_RELOC(x) (x->fx_addsy)
#define TC_CONS_RELOC BFD_RELOC_32_PCREL

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

#define tc_fix_adjustable(FIX) md_dlx_fix_adjustable (FIX)
extern bfd_boolean md_dlx_fix_adjustable PARAMS ((struct fix *));

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define NEED_FX_R_TYPE

/* Zero Based Segment?? sound very dangerous to me!     */
#define ZERO_BASED_SEGMENTS

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#undef  LOCAL_LABELS_DOLLAR
#define LOCAL_LABELS_DOLLAR 0

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */
