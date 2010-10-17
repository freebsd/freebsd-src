/* tc-frv.h -- Header file for tc-frv.c.
   Copyright 2002 Free Software Foundation, Inc.

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

#define TC_FRV

#ifndef BFD_ASSEMBLER
/* leading space so will compile with cc */
 #error FRV support requires BFD_ASSEMBLER
#endif

#define LISTING_HEADER "FRV GAS "

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_frv

#define TARGET_FORMAT (frv_md_fdpic_enabled () \
		       ? "elf32-frvfdpic" : "elf32-frv")
extern bfd_boolean frv_md_fdpic_enabled (void);

#define TARGET_BYTES_BIG_ENDIAN 1

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

#define DIFF_EXPR_OK		/* .-foo gets turned into PC relative relocs */

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* Values passed to md_apply_fix3 don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

extern void frv_tomcat_workaround PARAMS ((void));
#define md_cleanup frv_tomcat_workaround

#define md_number_to_chars frv_md_number_to_chars

extern long frv_relax_frag PARAMS ((fragS *, long));
#define md_relax_frag(segment, fragP, stretch) frv_relax_frag(fragP, stretch)

#define tc_fix_adjustable(FIX) frv_fix_adjustable (FIX)
struct fix;
extern bfd_boolean frv_fix_adjustable PARAMS ((struct fix *));

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) frv_force_relocation (fix)
extern int frv_force_relocation PARAMS ((struct fix *));

/* If we simplify subtractions that aren't SUB_SAME or SUB_ABS, we end
   up with PCrel fixups, but since we don't have any PCrel relocs, we
   crash.  Preventing simplification gets us a good, early error.  */
#define TC_FORCE_RELOCATION_SUB_LOCAL(fixP) 1

#undef GAS_CGEN_MAX_FIXUPS
#define GAS_CGEN_MAX_FIXUPS 1

void frv_frob_label PARAMS ((symbolS *));
#define tc_frob_label(sym) frv_frob_label(sym)

#define tc_gen_reloc gas_cgen_tc_gen_reloc

#define md_cgen_record_fixup_exp frv_cgen_record_fixup_exp

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

/* After all of the symbols have been adjusted, go over the file looking
   for any relocations that pic won't support.  */
#define tc_frob_file() frv_frob_file ()
extern void frv_frob_file	PARAMS ((void));

/* We don't want 0x00 for code alignment because this generates `add.p
   gr0, gr0, gr0' patterns.  Although it's fine as a nop instruction,
   it has the VLIW packing bit set, which means if you have a bunch of
   them in a row and attempt to execute them, you'll exceed the VLIW
   capacity and fail.  This also gets GDB confused sometimes, because
   it won't set breakpoints in instructions other than the first of a
   VLIW pack, so you used to be unable to set a breakpoint in the
   initial instruction of a function that followed such
   alignment-introduced instructions.

   We could have arranged to emit `nop' instructions (0x80880000),
   maybe even VLIW-pack sequences of nop instructions as much as
   possible for the selected machine type, just in case the alignment
   code actually happens to run, but this is probably too much effort
   for little gain.  This code is not meant to be run anyway, so just
   emit nops.  */
#define MAX_MEM_FOR_RS_ALIGN_CODE (3 + 4)
#define HANDLE_ALIGN(FRAGP) do						\
  if ((FRAGP)->fr_type == rs_align_code) 				\
    {									\
      valueT count = ((FRAGP)->fr_next->fr_address			\
		      - ((FRAGP)->fr_address + (FRAGP)->fr_fix));	\
      unsigned char *dest = (FRAGP)->fr_literal + (FRAGP)->fr_fix;	\
      if ((count & 3) != 0)						\
	{								\
	  memset (dest, 0, (count & 3));				\
	  (FRAGP)->fr_fix += (count & 3);				\
	  dest += (count & 3);						\
	  count -= (count & 3);						\
	}								\
      if (count)							\
	{								\
	  (FRAGP)->fr_var = 4;						\
	  *dest++ = 0x80;						\
	  *dest++ = 0x88;						\
	  *dest++ = 0x00;						\
	  *dest++ = 0x00;						\
	}								\
    }									\
 while (0)
