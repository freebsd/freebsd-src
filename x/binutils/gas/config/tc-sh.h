/* This file is tc-sh.h
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#define TC_SH

#define TARGET_ARCH bfd_arch_sh

#if ANSI_PROTOTYPES
struct segment_info_struct;
struct internal_reloc;
#endif

/* Whether -relax was used.  */
extern int sh_relax;

/* Whether -small was used.  */
extern int sh_small;

/* Don't try to break words.  */
#define WORKING_DOT_WORD

/* All SH instructions are multiples of 16 bits.  */
#define DWARF2_LINE_MIN_INSN_LENGTH 2

/* We require .long, et. al., to be aligned correctly.  */
#define md_cons_align(nbytes) sh_cons_align (nbytes)
extern void sh_cons_align PARAMS ((int));

/* When relaxing, we need to generate relocations for alignment
   directives.  */
#define HANDLE_ALIGN(frag) sh_handle_align (frag)
extern void sh_handle_align PARAMS ((fragS *));

#define MAX_MEM_FOR_RS_ALIGN_CODE (1 + 2)

/* We need to force out some relocations when relaxing.  */
#define TC_FORCE_RELOCATION(fix) sh_force_relocation (fix)

/* The type fixS is defined (to struct fix) in write.h, but write.h uses
   definitions from this file.  To avoid problems with including write.h
   after the "right" definitions, don't; just forward-declare struct fix
   here.  */
struct fix;
extern int sh_force_relocation PARAMS ((struct fix *));

#ifdef OBJ_ELF
#define obj_fix_adjustable(fixP) sh_fix_adjustable(fixP)
struct fix;
extern boolean sh_fix_adjustable PARAMS ((struct fix *));

/* This arranges for gas/write.c to not apply a relocation if
   obj_fix_adjustable() says it is not adjustable.  */
/* ??? fixups with symbols in SEC_MERGE sections are marked with
   obj_fix_adjustable and have a non-section symbol, as in
   "vwxyz"+1 in execute/string-opt-6.c .  Maybe the test of
   (symbol_used_in_reloc_p should be done in the machine-independent code.  */
#define TC_FIX_ADJUSTABLE(fixP) \
  (! symbol_used_in_reloc_p (fixP->fx_addsy) && obj_fix_adjustable (fixP))
#endif

#define MD_PCREL_FROM_SECTION(FIXP, SEC) md_pcrel_from_section (FIXP, SEC)
extern long md_pcrel_from_section PARAMS ((struct fix *, segT));

#define IGNORE_NONSTANDARD_ESCAPES

#define LISTING_HEADER \
  (!target_big_endian \
   ? "Hitachi Super-H GAS Little Endian" : "Hitachi Super-H GAS Big Endian")

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
extern void sh_frob_label PARAMS ((void));
#define tc_frob_label(sym) sh_frob_label ()

/* We call a routine to flush pending output in order to output a DATA
   reloc when required.  */
extern void sh_flush_pending_output PARAMS ((void));
#define md_flush_pending_output() sh_flush_pending_output ()

#ifdef BFD_ASSEMBLER
#define tc_frob_file_before_adjust sh_frob_file
#else
#define tc_frob_file sh_frob_file
#endif
extern void sh_frob_file PARAMS ((void));

#ifdef OBJ_COFF
/* COFF specific definitions.  */

#define DO_NOT_STRIP 0

/* This macro translates between an internal fix and an coff reloc type */
#define TC_COFF_FIX2RTYPE(fix) ((fix)->fx_r_type)

#define BFD_ARCH TARGET_ARCH

#define COFF_MAGIC (!target_big_endian ? SH_ARCH_MAGIC_LITTLE : SH_ARCH_MAGIC_BIG)

/* We need to write out relocs which have not been completed.  */
#define TC_COUNT_RELOC(fix) ((fix)->fx_addsy != NULL)

#define TC_RELOC_MANGLE(seg, fix, int, paddr) \
  sh_coff_reloc_mangle ((seg), (fix), (int), (paddr))
extern void sh_coff_reloc_mangle
  PARAMS ((struct segment_info_struct *, struct fix *,
	   struct internal_reloc *, unsigned int));

#define tc_coff_symbol_emit_hook(a) ; /* not used */

#define NEED_FX_R_TYPE 1

#define TC_KEEP_FX_OFFSET 1

#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep(frag)
extern int tc_coff_sizemachdep PARAMS ((fragS *));

#ifdef BFD_ASSEMBLER
#define SEG_NAME(SEG) segment_name (SEG)
#else
#define SEG_NAME(SEG) obj_segment_name (SEG)
#endif

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
#else
#define TARGET_FORMAT (!target_big_endian ? "elf32-shl" : "elf32-sh")
#endif

#define elf_tc_final_processing sh_elf_final_processing
extern void sh_elf_final_processing PARAMS ((void));

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

/* This expression evaluates to false if the relocation is for a local object
   for which we still want to do the relocation at runtime.  True if we
   are willing to perform this relocation while building the .o file.
   This is only used for pcrel relocations, so GOTOFF does not need to be
   checked here.  I am not sure if some of the others are ever used with
   pcrel, but it is easier to be safe than sorry.

   We can't resolve references to the GOT or the PLT when creating the
   object file, since these tables are only created by the linker.
   Also, if the symbol is global, weak, common or not defined, the
   assembler can't compute the appropriate reloc, since its location
   can only be determined at link time.  */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)				\
  ((FIX)->fx_r_type != BFD_RELOC_32_PLT_PCREL			\
   && (FIX)->fx_r_type != BFD_RELOC_32_GOT_PCREL		\
   && (FIX)->fx_r_type != BFD_RELOC_SH_GOTPC			\
   && ((FIX)->fx_addsy == NULL					\
       || (! S_IS_EXTERNAL ((FIX)->fx_addsy)			\
	   && ! S_IS_WEAK ((FIX)->fx_addsy)			\
	   && S_IS_DEFINED ((FIX)->fx_addsy)			\
	   && ! S_IS_COMMON ((FIX)->fx_addsy))))

#define md_parse_name(name, exprP, nextcharP) \
  sh_parse_name ((name), (exprP), (nextcharP))
int sh_parse_name PARAMS ((char const *name,
			   expressionS *exprP,
			   char *nextchar));

#define TC_CONS_FIX_NEW(FRAG, OFF, LEN, EXP) \
  sh_cons_fix_new ((FRAG), (OFF), (LEN), (EXP))
void sh_cons_fix_new PARAMS ((fragS *, int, int, expressionS *));

/* This is used to construct expressions out of @GOTOFF, @PLT and @GOT
   symbols.  The relocation type is stored in X_md.  */
#define O_PIC_reloc O_md1

#endif /* OBJ_ELF */
