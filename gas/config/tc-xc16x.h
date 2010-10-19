/* This file is tc-xc16x.h
   Copyright 2006 Free Software Foundation, Inc.
   Contributed by KPIT Cummins Infosystems 

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

#define TC_XC16X

#define TARGET_BYTES_BIG_ENDIAN 0

#define TARGET_ARCH bfd_arch_xc16x

#ifdef BFD_ASSEMBLER
/* Fixup debug sections since we will never relax them.  */
#define TC_LINKRELAX_FIXUP(seg) (seg->flags & SEC_ALLOC)
#endif

#ifdef OBJ_ELF
#define TARGET_FORMAT       "elf32-xc16x"
#define LOCAL_LABEL_PREFIX  '.'
#define LOCAL_LABEL(NAME)   (NAME[0] == '.' && NAME[1] == 'L')
#define FAKE_LABEL_NAME     ".L0\001"
#endif

#if ANSI_PROTOTYPES
struct fix;
struct internal_reloc;
#endif

#define WORKING_DOT_WORD

#define BFD_ARCH bfd_arch_xc16x
#define TC_COUNT_RELOC(x)  1
#define IGNORE_NONSTANDARD_ESCAPES

#define TC_RELOC_MANGLE(s,a,b,c) tc_reloc_mangle(a,b,c)
extern void tc_reloc_mangle (struct fix *, struct internal_reloc *, bfd_vma);

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* Minimum instruction is of 16 bits.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2

#define DO_NOT_STRIP 0
#define LISTING_HEADER "Infineon XC16X GAS "
#define NEED_FX_R_TYPE 1
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

#define md_operand(x)
