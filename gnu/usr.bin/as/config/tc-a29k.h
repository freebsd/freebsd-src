/* tc-a29k.h -- Assemble for the AMD 29000.
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
   
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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define TC_A29K

#define NO_LISTING

#define tc_aout_pre_write_hook(x)	{;} /* not used */
#define tc_coff_symbol_emit_hook(a)	{;} /* not used */
#define tc_crawl_symbol_chain(a)	{;} /* not used */
#define tc_headers_hook(a)		{;} /* not used */

#define AOUT_MACHTYPE 101
#define TC_COFF_FIX2RTYPE(fix_ptr) tc_coff_fix2rtype(fix_ptr)
#define BFD_ARCH bfd_arch_a29k
#define COFF_MAGIC SIPFBOMAGIC

/* Should the reloc be output ? 
   on the 29k, this is true only if there is a symbol attatched.
   on the h8, this is allways true, since no fixup is done
   */
#define TC_COUNT_RELOC(x) (x->fx_addsy)

/* end of tc-a29k.h */
