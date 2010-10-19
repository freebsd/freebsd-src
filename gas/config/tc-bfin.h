/* tc-bfin.h - header file for tc-bfin.c
   Copyright 2005
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

#define TC_BFIN 1
#define TC_ADI_BFIN 1

#define TARGET_BYTES_BIG_ENDIAN 0

#define TARGET_ARCH		bfd_arch_bfin

/*
 * Define the target format macro here.   The value for this should be
 * "elf32-bfin", not "elf32-little-bfin".  Since the BFD source file 
 * elf32-bfin.c defines TARGET_LITTLE_NAME to be "elf32-little-bfin",
 * we must use this value, until this is corrected and BFD is rebuilt.  */
#ifdef OBJ_ELF
#define TARGET_FORMAT		"elf32-bfin" 
#endif

#define LISTING_HEADER "BFIN GAS "

#define WORKING_DOT_WORD

extern void bfin_start_line_hook PARAMS ((void));
extern bfd_boolean bfin_start_label PARAMS ((char *));

#define md_start_line_hook()    bfin_start_line_hook()
#define md_number_to_chars	number_to_chars_littleendian
#define md_convert_frag(b,s,f)	as_fatal ("bfin convert_frag\n");

/* Allow for [, ], etc.  */
#define LEX_BR 6

#define TC_EOL_IN_INSN(PTR) (bfin_eol_in_insn(PTR) ? 1 : 0)
extern bfd_boolean bfin_eol_in_insn PARAMS ((char *));

/* The instruction is permitted to contain an = character.  */
#define TC_EQUAL_IN_INSN(C, NAME) (bfin_name_is_register (NAME) ? 1 : 0)
extern bfd_boolean bfin_name_is_register PARAMS ((char *));
#define NOP_OPCODE 0x0000 

#define LOCAL_LABELS_FB 1

#define DOUBLESLASH_LINE_COMMENTS

#define TC_START_LABEL(ch ,ptr) (ch == ':' && bfin_start_label (ptr))
#define tc_fix_adjustable(FIX) bfin_fix_adjustable (FIX)
extern bfd_boolean bfin_fix_adjustable PARAMS ((struct fix *));

#define TC_FORCE_RELOCATION(FIX) bfin_force_relocation (FIX)
extern int bfin_force_relocation PARAMS ((struct fix *));

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

/* Values passed to md_apply_fix3 don't include symbol values.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* end of tc-bfin.h */
