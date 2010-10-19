/* tc-mmix.h -- Header file for tc-mmix.c.
   Copyright (C) 2001, 2002, 2003, 2005 Free Software Foundation, Inc.
   Written by Hans-Peter Nilsson (hp@bitrange.com).

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

#define TC_MMIX

/* See gas/doc/internals.texi for explanation of these macros.  */
#define TARGET_FORMAT "elf64-mmix"
#define TARGET_ARCH bfd_arch_mmix
#define TARGET_BYTES_BIG_ENDIAN 1

extern const char mmix_comment_chars[];
#define tc_comment_chars mmix_comment_chars

extern const char mmix_symbol_chars[];
#define tc_symbol_chars mmix_symbol_chars

extern const char mmix_exp_chars[];
#define EXP_CHARS mmix_exp_chars

extern const char mmix_flt_chars[];
#define FLT_CHARS mmix_flt_chars

/* "@" is a synonym for ".".  */
#define LEX_AT (LEX_BEGIN_NAME)

extern int mmix_label_without_colon_this_line (void);
#define LABELS_WITHOUT_COLONS mmix_label_without_colon_this_line ()

extern int mmix_next_semicolon_is_eoln;
#define TC_EOL_IN_INSN(p) (*(p) == ';' && ! mmix_next_semicolon_is_eoln)

/* This is one direction we can get mmixal compatibility.  */
extern void mmix_handle_mmixal (void);
#define md_start_line_hook mmix_handle_mmixal

extern void mmix_md_begin (void);
#define md_begin mmix_md_begin

extern void mmix_md_end (void);
#define md_end mmix_md_end

extern int mmix_current_location \
  (void (*fn) (expressionS *), expressionS *);
extern int mmix_parse_predefined_name (char *, expressionS *);

extern char *mmix_current_prefix;

/* A bit ugly, since we "know" that there's a static function
   current_location that does what we want.  We also strip off a leading
   ':' in another ugly way.

   The [DVWIOUZX]_Handler symbols are provided when-used.  */

extern int mmix_gnu_syntax;
#define md_parse_name(name, exp, mode, cpos)			\
 (! mmix_gnu_syntax						\
  && (name[0] == '@'						\
      ? (! is_part_of_name (name[1])				\
	 && mmix_current_location (current_location, exp))	\
      : ((name[0] == ':' || ISUPPER (name[0]))			\
	 && mmix_parse_predefined_name (name, exp))))

extern char *mmix_prefix_name (char *);

/* We implement when *creating* a symbol, we also need to strip a ':' or
   prepend a prefix.  */
#define tc_canonicalize_symbol_name(x) \
 (mmix_current_prefix == NULL && (x)[0] != ':' ? (x) : mmix_prefix_name (x))

#define md_undefined_symbol(x) NULL

extern void mmix_fb_label (expressionS *);

/* Since integer_constant is local to expr.c, we have to make this a
   macro.  FIXME: Do it cleaner.  */
#define md_operand(exp)							\
  do									\
    {									\
      if (input_line_pointer[0] == '#')					\
	{								\
	  input_line_pointer++;						\
	  integer_constant (16, (exp));					\
	}								\
      else if (input_line_pointer[0] == '&'				\
	       && input_line_pointer[1] != '&')				\
	as_bad (_("`&' serial number operator is not supported"));	\
      else								\
	mmix_fb_label (exp);						\
    }									\
  while (0)

/* Gas dislikes the 2ADD, 8ADD etc. insns, so we have to assemble them in
   the error-recovery loop.  Hopefully there are no significant
   differences.  Also, space on a line isn't gracefully handled.  */
extern int mmix_assemble_return_nonzero (char *);
#define tc_unrecognized_line(c)						\
 ((c) == ' '								\
  || (((c) == '1' || (c) == '2' || (c) == '4' || (c) == '8')		\
      && mmix_assemble_return_nonzero (input_line_pointer - 1)))

#define md_number_to_chars number_to_chars_bigendian

#define WORKING_DOT_WORD

extern const struct relax_type mmix_relax_table[];
#define TC_GENERIC_RELAX_TABLE mmix_relax_table

/* We use the relax table for everything except the GREG frags and PUSHJ.  */
extern long mmix_md_relax_frag (segT, fragS *, long);
#define md_relax_frag mmix_md_relax_frag

#define tc_fix_adjustable(FIX)					\
 (((FIX)->fx_addsy == NULL					\
   || S_GET_SEGMENT ((FIX)->fx_addsy) != reg_section)		\
  && (FIX)->fx_r_type != BFD_RELOC_VTABLE_INHERIT		\
  && (FIX)->fx_r_type != BFD_RELOC_VTABLE_ENTRY			\
  && (FIX)->fx_r_type != BFD_RELOC_MMIX_LOCAL)

/* Adjust symbols which are registers.  */
#define tc_adjust_symtab() mmix_adjust_symtab ()
extern void mmix_adjust_symtab (void);

/* Here's where we make all symbols global, when so requested.
   We must avoid doing that for expression symbols or section symbols,
   though.  */
extern int mmix_globalize_symbols;
#define tc_frob_symbol(sym, punt)				\
  do								\
    {								\
      if (S_GET_SEGMENT (sym) == reg_section)			\
	{							\
	  if (S_GET_NAME (sym)[0] != '$'			\
	      && S_GET_VALUE (sym) < 256)			\
	    {							\
	      if (mmix_globalize_symbols)			\
		S_SET_EXTERNAL (sym);				\
	      else						\
		symbol_mark_used_in_reloc (sym);		\
	    }							\
	}							\
      else if (mmix_globalize_symbols				\
	       && ! symbol_section_p (sym)			\
	       && sym != section_symbol (absolute_section)	\
	       && ! S_IS_LOCAL (sym))				\
	S_SET_EXTERNAL (sym);					\
    }								\
  while (0)

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* When relaxing, we need to emit various relocs we otherwise wouldn't.  */
#define TC_FORCE_RELOCATION(fix) mmix_force_relocation (fix)
extern int mmix_force_relocation (struct fix *);

/* Call md_pcrel_from_section(), not md_pcrel_from().  */
#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

#define md_section_align(seg, size) (size)

#define LISTING_HEADER "GAS for MMIX"

/* The default of 4 means Bcc expansion looks like it's missing a line.  */
#define LISTING_LHS_CONT_LINES 5

extern fragS *mmix_opcode_frag;
#define TC_FRAG_TYPE fragS *
#define TC_FRAG_INIT(frag) (frag)->tc_frag_data = mmix_opcode_frag

/* We need to associate each section symbol with a list of GREGs defined
   for that section/segment and sorted on offset, between the point where
   all symbols have been evaluated and all frags mapped, and when the
   fixups are done and relocs are output.  Similarly for each unknown
   symbol.  */
extern void mmix_frob_file (void);
#define tc_frob_file_before_fix mmix_frob_file

/* Used by mmix_frob_file.  Hangs on section symbols and unknown symbols.  */
struct mmix_symbol_gregs;
#define TC_SYMFIELD_TYPE struct mmix_symbol_gregs *

/* Used by relaxation, counting maximum needed PUSHJ stubs for a section.  */
struct mmix_segment_info_type
 {
   /* We only need to keep track of the last stubbable frag because
      there's no less hackish way to keep track of different relaxation
      rounds.  */
   fragS *last_stubfrag;
   bfd_size_type nstubs;
 };
#define TC_SEGMENT_INFO_TYPE struct mmix_segment_info_type

extern void mmix_md_elf_section_change_hook (void);
#define md_elf_section_change_hook mmix_md_elf_section_change_hook

extern void mmix_md_do_align (int, char *, int, int);
#define md_do_align(n, fill, len, max, label) \
 mmix_md_do_align (n, fill, len, max)

/* Each insn is a tetrabyte (4 bytes) long, but if there are BYTE
   sequences sprinkled in, we can get unaligned DWARF2 offsets, so let's
   explicitly say one byte.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 1
