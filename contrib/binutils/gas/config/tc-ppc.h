/* tc-ppc.h -- Header file for tc-ppc.c.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

#define TC_PPC

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

/* Set the endianness we are using.  Default to big endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 1
#endif

#ifndef BFD_ASSEMBLER
 #error PowerPC support requires BFD_ASSEMBLER
#endif

/* If OBJ_COFF is defined, and TE_PE is not defined, we are assembling
   XCOFF for AIX or PowerMac.  If TE_PE is defined, we are assembling
   COFF for Windows NT.  */

#ifdef OBJ_COFF
#ifndef TE_PE
#define OBJ_XCOFF
#endif
#endif

/* The target BFD architecture.  */
#define TARGET_ARCH (ppc_arch ())
#define TARGET_MACH (ppc_mach ())
extern enum bfd_architecture ppc_arch PARAMS ((void));
extern unsigned long ppc_mach PARAMS ((void));

/* Whether or not the target is big endian */
extern int target_big_endian;

/* The target BFD format.  */
#define TARGET_FORMAT (ppc_target_format ())
extern char* ppc_target_format ();

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* $ is used to refer to the current location.  */
#define DOLLAR_DOT

/* Strings do not use backslash escapes under COFF.  */
#ifdef OBJ_COFF
#define NO_STRING_ESCAPES
#endif

#ifdef OBJ_ELF
#define DIFF_EXPR_OK		/* foo-. gets turned into PC relative relocs */
#endif

#if TARGET_BYTES_BIG_ENDIAN
#define PPC_BIG_ENDIAN 1
#else
#define PPC_BIG_ENDIAN 0
#endif

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

/* We set the fx_done field appropriately in md_apply_fix.  */
#define TC_HANDLES_FX_DONE

#ifdef TE_PE

/* Question marks are permitted in symbol names.  */
#define LEX_QM 1

/* Don't adjust TOC relocs.  */
#define tc_fix_adjustable(fixp) ppc_pe_fix_adjustable (fixp)
extern int ppc_pe_fix_adjustable PARAMS ((struct fix *));

#endif

#ifdef OBJ_XCOFF

/* Declarations needed when generating XCOFF code.  XCOFF is an
   extension of COFF, used only on the RS/6000.  Rather than create an
   obj-xcoff, we just use obj-coff, and handle the extensions here in
   tc-ppc.  */

/* We need to keep some information for symbols.  */
struct ppc_tc_sy
{
  /* We keep a few linked lists of symbols.  */
  symbolS *next;
  /* Non-zero if the symbol should be output.  The RS/6000 assembler
     only outputs symbols that are external or are mentioned in a
     .globl or .lglobl statement.  */
  int output;
  /* The symbol class.  */
  int class;
  /* The real name, if the symbol was renamed.  */
  char *real_name;
  /* For a csect symbol, the subsegment we are using.  This is zero
     for symbols that are not csects.  */
  subsegT subseg;
  /* For a csect or common symbol, the alignment to use.  */
  int align;
  /* For a function symbol, a symbol whose value is the size.  The
     field is NULL if there is no size.  */
  symbolS *size;
  /* For a csect symbol, the last symbol which has been defined in
     this csect, or NULL if none have been defined so far.  For a .bs
     symbol, the referenced csect symbol.  */
  symbolS *within;
};

#define TC_SYMFIELD_TYPE struct ppc_tc_sy

/* We need an additional auxent for function symbols.  */
#define OBJ_COFF_MAX_AUXENTRIES 2

/* Square and curly brackets are permitted in symbol names.  */
#define LEX_BR 3

/* Canonicalize the symbol name.  */
#define tc_canonicalize_symbol_name(name) ppc_canonicalize_symbol_name (name)
extern char *ppc_canonicalize_symbol_name PARAMS ((char *));

/* Get the symbol class from the name.  */
#define tc_symbol_new_hook(sym) ppc_symbol_new_hook (sym)
extern void ppc_symbol_new_hook PARAMS ((symbolS *));

/* Set the symbol class of a label based on the csect.  */
#define tc_frob_label(sym) ppc_frob_label (sym)
extern void ppc_frob_label PARAMS ((symbolS *));

/* TOC relocs requires special handling.  */
#define tc_fix_adjustable(fixp) ppc_fix_adjustable (fixp)
extern int ppc_fix_adjustable PARAMS ((struct fix *));

/* A relocation from one csect to another must be kept.  */
#define TC_FORCE_RELOCATION(FIXP) ppc_force_relocation (FIXP)
extern int ppc_force_relocation PARAMS ((struct fix *));

/* We need to set the section VMA.  */
#define tc_frob_section(sec) ppc_frob_section (sec)
extern void ppc_frob_section PARAMS ((asection *));

/* Finish up the symbol.  */
#define tc_frob_symbol(sym, punt) punt = ppc_frob_symbol (sym)
extern int ppc_frob_symbol PARAMS ((symbolS *));

/* Finish up the entire symtab.  */
#define tc_adjust_symtab() ppc_adjust_symtab ()
extern void ppc_adjust_symtab PARAMS ((void));

/* Niclas Andersson <nican@ida.liu.se> says this is needed.  */
extern int ppc_subseg_align PARAMS ((void));
#define SUB_SEGMENT_ALIGN(SEG) ppc_subseg_align()

/* We also need to copy, in particular, the class of the symbol,
   over what obj-coff would otherwise have copied.  */
#define OBJ_COPY_SYMBOL_ATTRIBUTES(dest,src)			\
do {								\
  if (SF_GET_GET_SEGMENT (dest))				\
    S_SET_SEGMENT (dest, S_GET_SEGMENT (src));			\
  symbol_get_tc (dest)->size = symbol_get_tc (src)->size;	\
  symbol_get_tc (dest)->align = symbol_get_tc (src)->align;	\
  symbol_get_tc (dest)->class = symbol_get_tc (src)->class;	\
  symbol_get_tc (dest)->within = symbol_get_tc (src)->within;	\
} while (0)

#endif /* OBJ_XCOFF */

#ifdef OBJ_ELF

/* Branch prediction relocations must force relocation, as must
   the vtable description relocs.  */
#define TC_FORCE_RELOCATION(FIXP)					\
((FIXP)->fx_r_type == BFD_RELOC_PPC_B16_BRTAKEN				\
 || (FIXP)->fx_r_type == BFD_RELOC_PPC_B16_BRNTAKEN			\
 || (FIXP)->fx_r_type == BFD_RELOC_PPC_BA16_BRTAKEN			\
 || (FIXP)->fx_r_type == BFD_RELOC_PPC_BA16_BRNTAKEN			\
 || (FIXP)->fx_r_type == BFD_RELOC_VTABLE_INHERIT			\
 || (FIXP)->fx_r_type == BFD_RELOC_VTABLE_ENTRY)

#define TC_FORCE_RELOCATION_SECTION(FIXP,SEC)				\
(TC_FORCE_RELOCATION (FIXP)						\
 || ((FIXP)->fx_addsy && !(FIXP)->fx_subsy				\
     && S_GET_SEGMENT ((FIXP)->fx_addsy) != SEC))

/* Support for SHF_EXCLUDE and SHT_ORDERED */
extern int ppc_section_letter PARAMS ((int, char **));
extern int ppc_section_type PARAMS ((char *, size_t));
extern int ppc_section_word PARAMS ((char *, size_t));
extern int ppc_section_flags PARAMS ((int, int, int));

#define md_elf_section_letter(LETTER, PTR_MSG)	ppc_section_letter (LETTER, PTR_MSG)
#define md_elf_section_type(STR, LEN)		ppc_section_type (STR, LEN)
#define md_elf_section_word(STR, LEN)		ppc_section_word (STR, LEN)
#define md_elf_section_flags(FLAGS, ATTR, TYPE)	ppc_section_flags (FLAGS, ATTR, TYPE)

/* Add extra PPC sections -- Note, for now, make .sbss2 and .PPC.EMB.sbss0 a
   normal section, and not a bss section so that the linker doesn't crater
   when trying to make more than 2 sections.  */
#define ELF_TC_SPECIAL_SECTIONS \
  { ".tags",		SHT_ORDERED,	SHF_ALLOC }, \
  { ".sdata",		SHT_PROGBITS,	SHF_ALLOC + SHF_WRITE }, \
  { ".sbss",		SHT_NOBITS,	SHF_ALLOC + SHF_WRITE }, \
  { ".sdata2",		SHT_PROGBITS,	SHF_ALLOC }, \
  { ".sbss2",		SHT_PROGBITS,	SHF_ALLOC }, \
  { ".PPC.EMB.sdata0",	SHT_PROGBITS,	SHF_ALLOC }, \
  { ".PPC.EMB.sbss0",	SHT_PROGBITS,	SHF_ALLOC },

#define tc_comment_chars ppc_comment_chars
extern const char *ppc_comment_chars;

/* Keep relocations relative to the GOT, or non-PC relative.  */
#define tc_fix_adjustable(FIX)                          		\
  ((FIX)->fx_r_type != BFD_RELOC_16_GOTOFF              		\
   && (FIX)->fx_r_type != BFD_RELOC_LO16_GOTOFF         		\
   && (FIX)->fx_r_type != BFD_RELOC_HI16_GOTOFF         		\
   && (FIX)->fx_r_type != BFD_RELOC_HI16_S_GOTOFF       		\
   && (FIX)->fx_r_type != BFD_RELOC_GPREL16             		\
   && (FIX)->fx_r_type != BFD_RELOC_VTABLE_INHERIT			\
   && (FIX)->fx_r_type != BFD_RELOC_VTABLE_ENTRY			\
   && ! S_IS_EXTERNAL ((FIX)->fx_addsy)					\
   && ! S_IS_WEAK ((FIX)->fx_addsy)					\
   && ((FIX)->fx_pcrel				        		\
       || ((FIX)->fx_subsy != NULL					\
	   && (S_GET_SEGMENT ((FIX)->fx_subsy)				\
	       == S_GET_SEGMENT ((FIX)->fx_addsy)))			\
       || S_IS_LOCAL ((FIX)->fx_addsy)))

/* We must never ever try to resolve references to externally visible
   symbols in the assembler, because the .o file might go into a shared
   library, and some other shared library might override that symbol.  */
#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)  \
  ((FIX)->fx_addsy == NULL \
   || (! S_IS_EXTERNAL ((FIX)->fx_addsy) \
       && ! S_IS_WEAK ((FIX)->fx_addsy) \
       && S_IS_DEFINED ((FIX)->fx_addsy) \
       && ! S_IS_COMMON ((FIX)->fx_addsy)))

#define DWARF2_LINE_MIN_INSN_LENGTH 4
#endif /* OBJ_ELF */

/* call md_apply_fix3 with segment instead of md_apply_fix */
#define MD_APPLY_FIX3

/* call md_pcrel_from_section, not md_pcrel_from */
#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section(FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

#define md_parse_name(name, exp) ppc_parse_name (name, exp)
extern int ppc_parse_name PARAMS ((const char *, struct expressionS *));

#define md_operand(x)
