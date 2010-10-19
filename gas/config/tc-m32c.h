/* tc-m32c.h -- Header file for tc-m32c.c.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA. */

#define TC_M32C

#define LISTING_HEADER "M16C/M32C GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_m32c

#define TARGET_FORMAT "elf32-m32c"

#define TARGET_BYTES_BIG_ENDIAN 1

#define md_end  m32c_md_end
extern void m32c_md_end (void);

#define md_start_line_hook m32c_start_line_hook
extern void m32c_start_line_hook (void);

/* call md_pcrel_from_section, not md_pcrel_from */
long md_pcrel_from_section PARAMS ((struct fix *, segT));
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section (FIXP, SEC)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_apply_fix m32c_apply_fix
extern void m32c_apply_fix PARAMS ((struct fix *, valueT *, segT));

#define tc_fix_adjustable(fixP) m32c_fix_adjustable (fixP)
extern bfd_boolean m32c_fix_adjustable PARAMS ((struct fix *));

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) m32c_force_relocation (fix)
extern int m32c_force_relocation PARAMS ((struct fix *));

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

extern void m32c_prepare_relax_scan PARAMS ((fragS *, offsetT *, relax_substateT state));
#define md_prepare_relax_scan(FRAGP, ADDR, AIM, STATE, TYPE) \
	m32c_prepare_relax_scan(FRAGP, &AIM, STATE)

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section (FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

/* We need a special version of the TC_START_LABEL macro so that we
   allow the :Z, :S, :Q and :G suffixes to be
   parsed as such.  Note - in a HORRIBLE HACK, we make use of the
   knowledge that this marco is only ever evaluated in one place
   (read_a_source_file in read.c) where we can access the local
   variable 's' - the start of the symbol that was terminated by
   'character'.  Also we need to be able to change the contents of
   the local variable 'c' which is passed to this macro as 'character'.  */
#define TC_START_LABEL(character, i_l_p)			\
  ((character) != ':' ? 0 : (character = m32c_is_colon_insn (s)) ? 0 : ((character = ':'), 1))
extern char m32c_is_colon_insn PARAMS ((char *));
