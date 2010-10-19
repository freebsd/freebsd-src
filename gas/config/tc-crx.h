/* tc-crx.h -- Header file for tc-crx.c, the CRX GAS port.
   Copyright 2004 Free Software Foundation, Inc.

   Contributed by Tomer Levi, NSC, Israel.
   Originally written for GAS 2.12 by Tomer Levi, NSC, Israel.
   Updates, BFDizing, GNUifying and ELF support by Tomer Levi.

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
   along with GAS; see the file COPYING.  If not, write to the
   Free Software Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#ifndef TC_CRX_H
#define TC_CRX_H

#define TC_CRX 1

#define TARGET_BYTES_BIG_ENDIAN 0

#define TARGET_FORMAT "elf32-crx"
#define TARGET_ARCH   bfd_arch_crx

#define WORKING_DOT_WORD
#define LOCAL_LABEL_PREFIX '.'

#define md_undefined_symbol(s)	0
#define md_number_to_chars	number_to_chars_littleendian

/* We do relaxing in the assembler as well as the linker.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/* We do not want to adjust any relocations to make implementation of
   linker relaxations easier.  */
#define tc_fix_adjustable(fixP)	0

/* We need to force out some relocations when relaxing.  */
#define TC_FORCE_RELOCATION(FIXP) crx_force_relocation (FIXP)
extern int crx_force_relocation (struct fix *);

/* Fixup debug sections since we will never relax them.  */
#define TC_LINKRELAX_FIXUP(seg) (seg->flags & SEC_ALLOC)

/* CRX instructions, with operands included, are a multiple
   of two bytes long.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2

/* This is called by emit_expr when creating a reloc for a cons.
   We could use the definition there, except that we want to handle 
   the CRX reloc type specially, rather than the BFD_RELOC type.  */
#define TC_CONS_FIX_NEW(FRAG, OFF, LEN, EXP) \
      fix_new_exp (FRAG, OFF, (int) LEN, EXP, 0, \
	LEN == 1 ? BFD_RELOC_CRX_NUM8 \
	: LEN == 2 ? BFD_RELOC_CRX_NUM16 \
	: LEN == 4 ? BFD_RELOC_CRX_NUM32 \
	: BFD_RELOC_NONE);

/* Give an error if a frag containing code is not aligned to a 2-byte 
   boundary.  */
#define md_frag_check(FRAGP) \
  if ((FRAGP)->has_code							\
      && (((FRAGP)->fr_address + (FRAGP)->insn_addr) & 1) != 0)		\
     as_bad_where ((FRAGP)->fr_file, (FRAGP)->fr_line,			\
		   _("instruction address is not a multiple of 2"));

#endif /* TC_CRX_H */
