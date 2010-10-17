/* tc-a29k.h -- Assemble for the AMD 29000.
   Copyright 1989, 1990, 1991, 1992, 1993, 1995, 1998
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

#define TC_A29K

#define TARGET_BYTES_BIG_ENDIAN	1

#define WORKING_DOT_WORD

#define LEX_DOLLAR 1

#define tc_unrecognized_line(c) a29k_unrecognized_line (c)
extern int a29k_unrecognized_line PARAMS ((int));

#define tc_headers_hook(a)		;	/* not used */
#define tc_headers_hook(a)		;	/* not used */
#define tc_crawl_symbol_chain(a)	;	/* not used */
#define tc_coff_symbol_emit_hook(a)	;	/* not used */

#define AOUT_MACHTYPE 101
#define TC_COFF_FIX2RTYPE(fix_ptr) tc_coff_fix2rtype(fix_ptr)
#define BFD_ARCH bfd_arch_a29k
#define COFF_MAGIC SIPFBOMAGIC
/* Should the reloc be output ?
	on the 29k, this is true only if there is a symbol attached.
	on the h8, this is always true, since no fixup is done
*/
#define TC_COUNT_RELOC(x) (x->fx_addsy)
#define TC_CONS_RELOC RELOC_32

#define COFF_FLAGS F_AR32W
#define reloc_type int
#define NEED_FX_R_TYPE

#define ZERO_BASED_SEGMENTS

/* end of tc-a29k.h */
