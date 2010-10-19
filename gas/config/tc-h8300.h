/* This file is tc-h8300.h
   Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 2000, 2001, 2002, 2003, 2005
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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define TC_H8300

#define TARGET_BYTES_BIG_ENDIAN 1

#define TARGET_ARCH bfd_arch_h8300

/* Fixup debug sections since we will never relax them.  */
#define TC_LINKRELAX_FIXUP(seg) (seg->flags & SEC_ALLOC)
#ifdef OBJ_ELF
#define TARGET_FORMAT "elf32-h8300"
#define LOCAL_LABEL_PREFIX '.'
#define LOCAL_LABEL(NAME) (NAME[0] == '.' && NAME[1] == 'L')
#define FAKE_LABEL_NAME ".L0\001"
#endif

struct fix;
struct internal_reloc;

#define WORKING_DOT_WORD

#define COFF_MAGIC ( Smode && Nmode ? 0x8304 : Hmode && Nmode ? 0x8303 : Smode ? 0x8302 : Hmode ? 0x8301 : 0x8300)
#define IGNORE_NONSTANDARD_ESCAPES

#define tc_coff_symbol_emit_hook(a) ; /* not used */

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* Minimum instruction is of 16 bits.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2

#ifdef OBJ_ELF
/* Provide mappings from the original H8 COFF relocation names to
   their corresponding BFD relocation names.  This allows us to use
   most of tc-h8300.c without modifications for both ELF and COFF
   ports.  */
#define R_MOV24B1 BFD_RELOC_H8_DIR24A8
#define R_MOVL1 BFD_RELOC_H8_DIR32A16
#define R_MOV24B1 BFD_RELOC_H8_DIR24A8
#define R_MOVL1 BFD_RELOC_H8_DIR32A16
#define R_RELLONG BFD_RELOC_32
#define R_MOV16B1 BFD_RELOC_H8_DIR16A8
#define R_RELWORD BFD_RELOC_16
#define R_RELBYTE BFD_RELOC_8
#define R_PCRWORD BFD_RELOC_16_PCREL
#define R_PCRBYTE BFD_RELOC_8_PCREL
#define R_JMPL1 BFD_RELOC_H8_DIR24R8
#define R_MEM_INDIRECT BFD_RELOC_8

/* We do not want to adjust any relocations to make implementation of
   linker relaxations easier.  */
#define tc_fix_adjustable(FIX) 0
#endif

#define LISTING_HEADER "Renesas H8/300 GAS "
#ifndef OBJ_ELF
#define RELOC_32 1234
#endif

extern int Hmode;
extern int Smode;
extern int Nmode;
extern int SXmode;

#define md_operand(x)
