/* tc-cris.h -- Header file for tc-cris.c, the CRIS GAS port.
   Copyright 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

   Contributed by Axis Communications AB, Lund, Sweden.
   Originally written for GAS 1.38.1 by Mikael Asker.
   Updates, BFDizing, GNUifying and ELF by Hans-Peter Nilsson.

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

/* See the GAS "internal" document for general documentation on this.
   It is called internals.texi (internals.info when makeinfo:d), but is
   not installed or makeinfo:d by "make info".  */

/* Functions and variables that aren't declared in tc.h are declared here,
   with the type/prototype that is used in the local extern-declaration of
   their usage.  */

#ifndef TC_CRIS
#define TC_CRIS

/* Multi-target support is always on.  */
extern const char *cris_target_format (void);
#define TARGET_FORMAT cris_target_format ()

#define TARGET_ARCH bfd_arch_cris

extern unsigned int cris_mach (void);
#define TARGET_MACH (cris_mach ())

#define TARGET_BYTES_BIG_ENDIAN 0

extern const char *md_shortopts;
extern struct option md_longopts[];
extern size_t md_longopts_size;

extern const pseudo_typeS md_pseudo_table[];

#define tc_comment_chars cris_comment_chars
extern const char cris_comment_chars[];
extern const char line_comment_chars[];
extern const char line_separator_chars[];
extern const char EXP_CHARS[];
extern const char FLT_CHARS[];

/* This should be optional, since it is ignored as an escape (assumed to
   be itself) if it is not recognized.  */
#define ONLY_STANDARD_ESCAPES

/* Note that we do not define TC_EQUAL_IN_INSN, since its current use is
   in the instruction rather than the operand, and thus does not come to
   use for side-effect assignments such as "and.d [r0 = r1 + 42], r3".  */
#define md_operand(x)

#define md_number_to_chars number_to_chars_littleendian

/* There's no use having different functions for this; the sizes are the
   same.  Note that we can't #define md_short_jump_size here.  */
#define md_create_short_jump md_create_long_jump

extern const struct relax_type md_cris_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_cris_relax_table

long cris_relax_frag (segT, fragS *, long);

/* GAS only handles relaxations for pc-relative data targeting addresses
   in the same segment, so we have to handle the rest on our own.  */
#define md_relax_frag(SEG, FRAGP, STRETCH)		\
 ((FRAGP)->fr_symbol != NULL				\
  && S_GET_SEGMENT ((FRAGP)->fr_symbol) == (SEG)	\
  ? relax_frag (SEG, FRAGP, STRETCH)			\
  : cris_relax_frag (SEG, FRAGP, STRETCH))

#define TC_FORCE_RELOCATION(FIX) md_cris_force_relocation (FIX)
extern int md_cris_force_relocation (struct fix *);

#define IS_CRIS_PIC_RELOC(RTYPE)			\
  ((RTYPE) == BFD_RELOC_CRIS_16_GOT			\
   || (RTYPE) == BFD_RELOC_CRIS_32_GOT			\
   || (RTYPE) == BFD_RELOC_CRIS_16_GOTPLT		\
   || (RTYPE) == BFD_RELOC_CRIS_32_GOTPLT		\
   || (RTYPE) == BFD_RELOC_CRIS_32_GOTREL		\
   || (RTYPE) == BFD_RELOC_CRIS_32_PLT_GOTREL		\
   || (RTYPE) == BFD_RELOC_CRIS_32_PLT_PCREL)

/* Make sure we don't resolve fixups for which we want to emit dynamic
   relocations.  FIXME: Set fx_plt instead of using IS_CRIS_PIC_RELOC.  */
#define TC_FORCE_RELOCATION_LOCAL(FIX)			\
  (!(FIX)->fx_pcrel					\
   || (FIX)->fx_plt					\
   || IS_CRIS_PIC_RELOC ((FIX)->fx_r_type)		\
   || TC_FORCE_RELOCATION (FIX))

/* For some reloc types, don't adjust fixups by reducing to a section
   symbol.  */
#define tc_fix_adjustable(FIX)				\
 ((FIX)->fx_r_type != BFD_RELOC_VTABLE_INHERIT		\
  && (FIX)->fx_r_type != BFD_RELOC_VTABLE_ENTRY		\
  && (! IS_CRIS_PIC_RELOC ((FIX)->fx_r_type)		\
      || (FIX)->fx_r_type == BFD_RELOC_CRIS_32_GOTREL))

/* FIXME: This *should* be a redundant definition, as the
   TC_FORCE_RELOCATION* definitions already told about the cases where
   we *don't* want the symbol value calculated.  Here we seem to answer
   the "are you sure" question.  It certainly has very little to do with
   whether the symbol value is passed to md_apply_fix.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* When we have fixups against constant expressions, we get a GAS-specific
   section symbol at no extra charge for obscure reasons in
   adjust_reloc_syms.  Since ELF outputs section symbols, it gladly
   outputs this "*ABS*" symbol in every object.  Avoid that.
   Also, don't emit undefined symbols (that aren't used in relocations).
   They pop up when tentatively parsing register names as symbols.  */
#define tc_frob_symbol(symp, punt)			\
 do {							\
  if ((OUTPUT_FLAVOR == bfd_target_elf_flavour		\
       && (symp) == section_symbol (absolute_section))	\
      || ! S_IS_DEFINED (symp))				\
    (punt) = 1;						\
 } while (0)

#define LISTING_HEADER "GAS for CRIS"

#if 0
/* The testsuite does not let me define these, although they IMHO should
   be preferred over the default.  */
#define LISTING_WORD_SIZE 2
#define LISTING_LHS_WIDTH 4
#define LISTING_LHS_WIDTH_SECOND 4
#endif

/* END of declaration and definitions described in the "internals"
   document.  */

/* Do this, or we will never know what hit us when the
   broken-word-fixes break.  Do _not_ use WARN_SIGNED_OVERFLOW_WORD,
   it is only for use with WORKING_DOT_WORD and warns about most stuff.
   (still in 2.9.1).  */
struct broken_word;
extern void tc_cris_check_adjusted_broken_word (offsetT,
						struct broken_word *);
#define TC_CHECK_ADJUSTED_BROKEN_DOT_WORD(new_offset, brokw) \
 tc_cris_check_adjusted_broken_word ((offsetT) (new_offset), brokw)

/* We don't want any implicit alignment, so we do nothing.  */
#define TC_IMPLICIT_LCOMM_ALIGNMENT(SIZE, P2VAR) do { } while (0)

/* CRIS instructions, with operands and prefixes included, are a multiple
   of two bytes long.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2

/* Make port immune to unwanted difference in te-generic.h vs. te-linux.h.  */
#define LOCAL_LABELS_DOLLAR 1

#endif /* TC_CRIS */
/*
 * Local variables:
 * eval: (c-set-style "gnu")
 * indent-tabs-mode: t
 * End:
 */
