/* tc-or32.h -- Assemble for the OpenRISC 1000.
   Copyright (C) 2002, 2003. 2005 Free Software Foundation, Inc.
   Contributed by Damjan Lampret <lampret@opencores.org>.
   Based upon a29k port.

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

#define TC_OR32

#define TARGET_BYTES_BIG_ENDIAN 1

#define LEX_DOLLAR 1

#ifdef OBJ_ELF
#define TARGET_FORMAT  "elf32-or32"
#define TARGET_ARCH    bfd_arch_or32
#endif

#ifdef OBJ_COFF
#define TARGET_FORMAT  "coff-or32-big"
#define reloc_type     int
#endif

#define tc_unrecognized_line(c) or32_unrecognized_line (c)

extern int or32_unrecognized_line (int);

#define tc_coff_symbol_emit_hook(a) ; /* Not used.  */

#define COFF_MAGIC                  SIPFBOMAGIC

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

#ifdef OBJ_ELF
/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0
#endif

#define ZERO_BASED_SEGMENTS
