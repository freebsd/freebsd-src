/* This file is tc-h8300.h

   Copyright (C) 1987-1992 Free Software Foundation, Inc.

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


#define TC_H8300

/* This macro translates between an internal fix and an coff reloc type */
#define TC_COFF_FIX2RTYPE(fixP) abort();

#define BFD_ARCH bfd_arch_h8300
#define COFF_MAGIC 0x8300
#define TC_COUNT_RELOC(x) (1)


#define TC_RELOC_MANGLE(a,b,c) tc_reloc_mangle(a,b,c)

#define DO_NOT_STRIP 1
#define DO_STRIP 0
#define LISTING_HEADER "Hitachi H8/300 GAS "

/* end of tc-h8300.h */
