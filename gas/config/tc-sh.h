/* This file is tc-sh.h
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005 Free Software Foundation, Inc.

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
   the Free Software Foundation, 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#define TC_SH

#define TARGET_ARCH bfd_arch_sh

/* The type fixS is defined (to struct fix) in write.h, but write.h uses
   definitions from this file.  To avoid problems with including write.h
   after the "right" definitions, don't; just forward-declare struct fix
   here.  */
struct fix;
struct segment_info_struct;
struct internal_reloc;

/* Whether -relax was used.  */
extern int sh_relax;

/* Whether -small was used.  */
extern int sh_small;

/* Don't try to break words.  */
#define WORKING_DOT_WORD

/* We require .long, et. al., to be aligned correctly.  */
#define md_cons_align(nbytes) sh_cons_align (nbytes)
extern void sh_cons_align (int);

/* When relaxing, we need to generate relocations for alignment
   directives.  */
#define HANDLE_ALIGN(frag) sh_handle_align (frag)
extern void sh_handle_align (fragS *);

#define MAX_MEM_FOR_RS_ALIGN_CODE (1 + 2)

/* We need to force out some relocations when relaxing.  */
#define TC_FORCE_RELOCATION(fix) sh_force_relocation (fix)
extern int sh_force_relocation (struct fix *);

/* This macro decides whether a particular reloc is an entry in a
   switch table.  It is used when relaxing, because the linker needs
   to know about all such entries so that it can adjust them if
   necessary.  */

#define SWITCH_TABLE(FIX)				\
  ((FIX)->fx_addsy != NULL				\
   && (FIX)->fx_subsy != NULL				\
   && S_GET_SEGMENT ((FIX)->fx_addsy) == text_section	\
   && S_GET_SEGMENT ((FIX)->fx_subsy) == text_section	\
   && ((FIX)->fx_r_type == BFD_RELOC_32			\
       || (FIX)->fx_r_type == BFD_RELOC_16		\
       || (FIX)->fx_r_type == BFD_RELOC_8))

#define TC_FORCE_RELOCATION_SUB_SAME(FIX, SEC)		\
  (! SEG_NORMAL (SEC)					\
   || TC_FORCE_RELOCATION (FIX)				\
   || (sh_relax && SWITCH_TABLE (FIX)))

/* Don't complain when we leave fx_subsy around.  */
#define TC_VALIDATE_FIX_SUB(FIX)			\
  (sh_relax && SWITCH_TABLE (FIX))

#define MD_PCREL_FROM_SECTION(FIX, SEC) md_pcrel_from_section (FIX, SEC)
extern long md_pcrel_from_section (struct fix *, segT);

#define IGNORE_NONSTANDARD_ESCAPES

#define LISTING_HEADER \
  (!target_big_endian \
   ? "Renesas / SuperH SH GAS Little Endian" \
   : "Renesas / SuperH SH GAS Big Endian")

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/* We record, for each section, whether we have most recently output a
   CODE reloc or a DATA reloc.  */
struct sh_segment_info_type
{
  int in_code : 1;
};
#define TC_SEGMENT_INFO_TYPE struct sh_segment_info_type

/* We call a routine to emit a reloc for a label, so that the linker
   can align loads and stores without crossing a label.  */
extern void sh_frob_label (symbolS *);
#define tc_frob_label(sym) sh_frob_label (sym)

/* We call a routine to flush pending output in order to output a DATA
   reloc when required.  */
extern void sh_flush_pending_output (void);
#define md_flush_pending_output() sh_flush_pending_output ()

#define tc_frob_file_before_adjust sh_frob_file
extern void sh_frob_file (void);


#ifdef OBJ_COFF
/* COFF specific definitions.  */

#define COFF_MAGIC (!target_big_endian ? SH_ARCH_MAGIC_LITTLE : SH_ARCH_MAGIC_BIG)

#define tc_coff_symbol_emit_hook(a) ; /* not used */

#define TC_KEEP_FX_OFFSET 1

#define SEG_NAME(SEG) segment_name (SEG)

/* We align most sections to a 16 byte boundary.  */
#define SUB_SEGMENT_ALIGN(SEG, FRCHAIN)			\
  (strncmp (SEG_NAME (SEG), ".stabstr", 8) == 0		\
   ? 0							\
   : ((strncmp (SEG_NAME (SEG), ".stab", 5) == 0	\
       || strcmp (SEG_NAME (SEG), ".ctors") == 0	\
       || strcmp (SEG_NAME (SEG), ".dtors") == 0)	\
      ? 2						\
      : (sh_small ? 2 : 4)))

#endif /* OBJ_COFF */

#ifdef OBJ_ELF
/* ELF specific definitions.  */

/* Whether or not the target is big endian */
extern int target_big_endian;
#ifdef TE_LINUX
#define TARGET_FORMAT (!target_big_endian ? "elf32-sh-linux" : "elf32-shbig-linux")
#elif defined(TE_NetBSD)
#define TARGET_FORMAT (!target_big_endian ? "elf32-shl-nbsd" : "elf32-sh-nbsd")
#elif defined TARGET_SYMBIAN
#define TARGET_FORMAT (!target_big_endian ? "elf32-shl-symbian" : "elf32-sh-symbian")
#else
#define TARGET_FORMAT (!target_big_endian ? "elf32-shl" : "elf32-sh")
#endif

#define elf_tc_final_processing sh_elf_final_processing
extern void sh_elf_final_processing (void);

#define DIFF_EXPR_OK		/* foo-. gets turned into PC relative relocs */

#define GLOBAL_OFFSET_TABLE_NAME "_GLOBAL_OFFSET_TABLE_"

/* This is the relocation type for direct references to
   GLOBAL_OFFSET_TABLE.  It comes up in complicated expressions such
   as _GLOBAL_OFFSET_TABLE_+[.-.L284], which cannot be expressed
   normally with the regular expressions.  The fixup specified here
   when used at runtime implies that we should add the address of the
   GOT to the specified location, and as a result we have simplified
   the expression into something we can use.  */
#define TC_RELOC_GLOBAL_OFFSET_TABLE BFD_RELOC_SH_GOTPC

#define tc_fix_adjustable(FIX) sh_fix_adjustable(FIX)
extern bfd_boolean sh_fix_adjustable (struct fix *);

/* Values passed to md_apply_fix don't include symbol values.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* This expression evaluates to true if the relocation is for a local object
   for which we still want to do the relocation at runtime.  False if we
   are willing to perform this relocation while building the .o file.

   We can't resolve references to the GOT or the PLT when creating the
   object file, since these tables are only created by the linker.
   Also, if the symbol is global, weak, common or not defined, the
   assembler can't compute the appropriate reloc, since its location
   can only be determined at link time.  */

#define TC_FORCE_RELOCATION_LOCAL(FIX)			\
  (!(FIX)->fx_pcrel					\
   || (FIX)->fx_plt					\
   || (FIX)->fx_r_type == BFD_RELOC_32_PLT_PCREL	\
   || (FIX)->fx_r_type == BFD_RELOC_32_GOT_PCREL	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPC		\
   || TC_FORCE_RELOCATION (FIX))

#define TC_FORCE_RELOCATION_SUB_LOCAL(FIX) (sh_relax && SWITCH_TABLE (FIX))

/* This keeps the subtracted symbol around, for use by PLT_PCREL
   relocs.  */
#define TC_FORCE_RELOCATION_SUB_ABS(FIX)		\
  ((FIX)->fx_r_type == BFD_RELOC_32_PLT_PCREL)

/* Don't complain when we leave fx_subsy around.  */
#undef TC_VALIDATE_FIX_SUB
#define TC_VALIDATE_FIX_SUB(FIX)			\
  ((FIX)->fx_r_type == BFD_RELOC_32_PLT_PCREL		\
   || (sh_relax && SWITCH_TABLE (FIX)))

#define md_parse_name(name, exprP, mode, nextcharP) \
  sh_parse_name ((name), (exprP), (mode), (nextcharP))
int sh_parse_name (char const *name, expressionS *exprP,
		   enum expr_mode mode, char *nextchar);

#define TC_CONS_FIX_NEW(FRAG, OFF, LEN, EXP) \
  sh_cons_fix_new ((FRAG), (OFF), (LEN), (EXP))
void sh_cons_fix_new (fragS *, int, int, expressionS *);

/* This is used to construct expressions out of @GOTOFF, @PLT and @GOT
   symbols.  The relocation type is stored in X_md.  */
#define O_PIC_reloc O_md1

#define TARGET_USE_CFIPOP 1

#define tc_cfi_frame_initial_instructions sh_cfi_frame_initial_instructions
extern void sh_cfi_frame_initial_instructions (void);

#define tc_regname_to_dw2regnum sh_regname_to_dw2regnum
extern int sh_regname_to_dw2regnum (const char *regname);

/* All SH instructions are multiples of 16 bits.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2
#define DWARF2_DEFAULT_RETURN_COLUMN 17
#define DWARF2_CIE_DATA_ALIGNMENT -4

#endif /* OBJ_ELF */
