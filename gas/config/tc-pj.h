/* This file is tc-pj.h
   Copyright 1999, 2000, 2002, 2003 Free Software Foundation, Inc.

   Contributed by Steve Chamberlain of Transmeta, sac@pobox.com

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
   Boston, MA 02111-1307, USA.  */

/* Contributed by Steve Chamberlain, of Transmeta. sac@pobox.com.  */

#define WORKING_DOT_WORD
#define IGNORE_NONSTANDARD_ESCAPES
#define TARGET_ARCH bfd_arch_pj
#define TARGET_FORMAT (target_big_endian ? "elf32-pj" : "elf32-pjl")
#define LISTING_HEADER                    				\
  (target_big_endian                      				\
   ? "Pico Java GAS Big Endian"           				\
   : "Pico Java GAS Little Endian")

void pj_cons_fix_new_pj PARAMS ((struct frag *, int, int, expressionS *));
arelent *tc_gen_reloc PARAMS((asection *, struct fix *));

#define md_section_align(SEGMENT, SIZE)     (SIZE)
#define md_convert_frag(B, S, F)            (as_fatal (_("convert_frag\n")), 0)
#define md_estimate_size_before_relax(A, B) (as_fatal (_("estimate size\n")),0)
#define md_undefined_symbol(NAME)           0

/* PC relative operands are relative to the start of the opcode, and
   the operand is always one byte into the opcode.  */

#define md_pcrel_from(FIX) 						\
	((FIX)->fx_where + (FIX)->fx_frag->fr_address - 1)

#define TC_CONS_FIX_NEW(FRAG, WHERE, NBYTES, EXP) \
	pj_cons_fix_new_pj (FRAG, WHERE, NBYTES, EXP)

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

#define tc_fix_adjustable(FIX) 					\
          (! ((FIX)->fx_r_type == BFD_RELOC_VTABLE_INHERIT         	\
	   || (FIX)->fx_r_type == BFD_RELOC_VTABLE_ENTRY))
