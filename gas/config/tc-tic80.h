/* This file is tc-tic80.h
   Copyright 1996, 1997, 2000 Free Software Foundation, Inc.

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

#define TC_TIC80

#define TARGET_BYTES_BIG_ENDIAN 0

#define TARGET_ARCH	bfd_arch_tic80
#define TARGET_FORMAT	"coff-tic80"
#define BFD_ARCH	TARGET_ARCH

/* We need the extra field in the fixup struct to put the relocation in.  */

#define NEED_FX_R_TYPE

/* Define md_number_to_chars as the appropriate standard big endian or
   little endian function.  Should we someday support endianness as a
   runtime decision, this will need to change.  */

#define md_number_to_chars number_to_chars_littleendian

/* Define away the call to md_operand in the expression parsing code.
   This is called whenever the expression parser can't parse the input
   and gives the assembler backend a chance to deal with it instead.  */

#define md_operand(x)

#ifdef OBJ_COFF

/* COFF specific definitions.  */

#define COFF_MAGIC	TIC80_ARCH_MAGIC

/* Whether a reloc should be output.  */

#define TC_COUNT_RELOC(fixp) ((fixp) -> fx_addsy != NULL)

/* This macro translates between an internal fix and a coff reloc type. */

#define TC_COFF_FIX2RTYPE(fixP) tc_coff_fix2rtype(fixP)

extern short tc_coff_fix2rtype PARAMS ((struct fix *));

#endif	/* OBJ_COFF */
