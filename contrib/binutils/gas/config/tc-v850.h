/* tc-v850.h -- Header file for tc-v850.c.
   Copyright 1996, 1997, 1998, 2000 Free Software Foundation, Inc.

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

#define TC_V850

#include <elf/v850.h>

#define TARGET_BYTES_BIG_ENDIAN 0

#ifndef BFD_ASSEMBLER
 #error V850 support requires BFD_ASSEMBLER
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH 		bfd_arch_v850

/* The target BFD format.  */
#define TARGET_FORMAT 		"elf32-v850"

#define MD_APPLY_FIX3
#define md_operand(x)

#define obj_fix_adjustable(fixP) v850_fix_adjustable(fixP)
#define TC_FORCE_RELOCATION(fixp) v850_force_relocation(fixp)

#ifdef OBJ_ELF
/* This arranges for gas/write.c to not apply a relocation if
   obj_fix_adjustable() says it is not adjustable.  */
#define TC_FIX_ADJUSTABLE(fixP) obj_fix_adjustable (fixP)
#endif

extern int v850_force_relocation PARAMS ((struct fix *));

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* foo-. gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars number_to_chars_littleendian

/* We need to handle lo(), hi(), etc etc in .hword, .word, etc
   directives, so we have to parse "cons" expressions ourselves.  */
#define TC_PARSE_CONS_EXPRESSION(EXP, NBYTES) parse_cons_expression_v850 (EXP)
#define TC_CONS_FIX_NEW cons_fix_new_v850
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/* This section must be in the small data area (pointed to by GP).  */
#define SHF_V850_GPREL		0x10000000
/* This section must be in the tiny data area (pointed to by EP).  */
#define SHF_V850_EPREL		0x20000000
/* This section must be in the zero data area (pointed to by R0).  */
#define SHF_V850_R0REL		0x40000000

#define ELF_TC_SPECIAL_SECTIONS \
  { ".sdata",	SHT_PROGBITS,		SHF_ALLOC + SHF_WRITE + SHF_V850_GPREL	}, \
  { ".rosdata",	SHT_PROGBITS,		SHF_ALLOC +             SHF_V850_GPREL	}, \
  { ".sbss",	SHT_NOBITS,		SHF_ALLOC + SHF_WRITE + SHF_V850_GPREL	}, \
  { ".scommon",	SHT_V850_SCOMMON, 	SHF_ALLOC + SHF_WRITE + SHF_V850_GPREL	}, \
  { ".tdata",	SHT_PROGBITS,		SHF_ALLOC + SHF_WRITE + SHF_V850_EPREL	}, \
  { ".tbss",	SHT_NOBITS,		SHF_ALLOC + SHF_WRITE + SHF_V850_EPREL	}, \
  { ".tcommon",	SHT_V850_TCOMMON,	SHF_ALLOC + SHF_WRITE + SHF_V850_R0REL	}, \
  { ".zdata",	SHT_PROGBITS,		SHF_ALLOC + SHF_WRITE + SHF_V850_R0REL	}, \
  { ".rozdata",	SHT_PROGBITS,		SHF_ALLOC +             SHF_V850_R0REL	}, \
  { ".zbss",	SHT_NOBITS,	  	SHF_ALLOC + SHF_WRITE + SHF_V850_R0REL	}, \
  { ".zcommon",	SHT_V850_ZCOMMON, 	SHF_ALLOC + SHF_WRITE + SHF_V850_R0REL	}, \
  { ".call_table_data",	SHT_PROGBITS,	SHF_ALLOC + SHF_WRITE },	   \
  { ".call_table_text",	SHT_PROGBITS,	SHF_ALLOC + SHF_WRITE + SHF_EXECINSTR },

#define MD_PCREL_FROM_SECTION(fixP,section) v850_pcrel_from_section (fixP, section)
extern long v850_pcrel_from_section ();

#define DWARF2_LINE_MIN_INSN_LENGTH 2
