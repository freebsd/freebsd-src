/* This file is tc-h8500.h
   Copyright 1993, 1995, 1997, 1998, 2000, 2003 Free Software Foundation, Inc.

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

#define TC_H8500

#define TARGET_BYTES_BIG_ENDIAN 1

#if ANSI_PROTOTYPES
struct internal_reloc;
#endif

#define WORKING_DOT_WORD

/* This macro translates between an internal fix and a coff reloc type.  */
#define TC_COFF_FIX2RTYPE(fixP) tc_coff_fix2rtype(fixP)

#define BFD_ARCH bfd_arch_h8500
#define COFF_MAGIC 0x8500
#define TC_COUNT_RELOC(x) ((x)->fx_addsy||(x)->fx_subsy)
#define IGNORE_NONSTANDARD_ESCAPES

#define TC_RELOC_MANGLE(s,a,b,c) tc_reloc_mangle(a,b,c)
extern void tc_reloc_mangle
  PARAMS ((struct fix *, struct internal_reloc *, bfd_vma));

#define DO_NOT_STRIP 0
#define LISTING_HEADER "Renesas H8/500 GAS "
#define NEED_FX_R_TYPE 1
#define RELOC_32 1234

#define TC_START_LABEL(ch, ptr)  (ch == ':' && start_label(ptr))
int start_label PARAMS ((char *));

#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep(frag)
int tc_coff_sizemachdep PARAMS ((struct frag *));

#define md_operand(x)

extern struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table
