/* This file is tc-sh.h
   Copyright (C) 1993, 94, 95, 96, 1997 Free Software Foundation, Inc.

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

#define TARGET_BYTES_BIG_ENDIAN 0

#define TARGET_ARCH bfd_arch_sh

/* Whether in little endian mode.  */
extern int shl;

/* Whether -relax was used.  */
extern int sh_relax;

/* Whether -small was used.  */
extern int sh_small;

/* Don't try to break words.  */
#define WORKING_DOT_WORD

/* We require .long, et. al., to be aligned correctly.  */
#define md_cons_align(nbytes) sh_cons_align (nbytes)
extern void sh_cons_align PARAMS ((int));

/* When relaxing, we need to generate relocations for alignment
   directives.  */
#define HANDLE_ALIGN(frag) sh_handle_align (frag)
extern void sh_handle_align PARAMS ((fragS *));

/* We need to force out some relocations when relaxing.  */
#define TC_FORCE_RELOCATION(fix) sh_force_relocation (fix)
extern int sh_force_relocation ();

#define IGNORE_NONSTANDARD_ESCAPES

#define LISTING_HEADER (shl ? "Hitachi Super-H GAS Little Endian" : "Hitachi Super-H GAS Big Endian")

#define md_operand(x)

extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

/* We use a special alignment function to insert the correct nop
   pattern.  */
extern int sh_do_align PARAMS ((int, const char *, int, int));
#define md_do_align(n,fill,len,max,l) if (sh_do_align (n,fill,len,max)) goto l

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

#define COFF_MAGIC (shl ? SH_ARCH_MAGIC_LITTLE : SH_ARCH_MAGIC_BIG)

/* We need to write out relocs which have not been completed.  */
#define TC_COUNT_RELOC(fix) ((fix)->fx_addsy != NULL)

#define TC_RELOC_MANGLE(seg, fix, int, paddr) \
  sh_coff_reloc_mangle ((seg), (fix), (int), (paddr))
extern void sh_coff_reloc_mangle ();

#define tc_coff_symbol_emit_hook(a) ; /* not used */

#define NEED_FX_R_TYPE 1

#define TC_KEEP_FX_OFFSET 1

#define TC_COFF_SIZEMACHDEP(frag) tc_coff_sizemachdep(frag)
extern int tc_coff_sizemachdep PARAMS ((fragS *));

/* We align most sections to a 16 byte boundary.  */
#define SUB_SEGMENT_ALIGN(SEG)					\
  (strncmp (obj_segment_name (SEG), ".stabstr", 8) == 0		\
   ? 0								\
   : ((strncmp (obj_segment_name (SEG), ".stab", 5) == 0	\
       || strcmp (obj_segment_name (SEG), ".ctors") == 0	\
       || strcmp (obj_segment_name (SEG), ".dtors") == 0)	\
      ? 2							\
      : (sh_small ? 2 : 4)))

#endif /* OBJ_COFF */

#ifdef OBJ_ELF
/* ELF specific definitions.  */

/* Whether or not the target is big endian */
extern int target_big_endian;

#define TARGET_FORMAT (shl ? "elf32-shl" : "elf32-sh")

#endif /* OBJ_ELF */

/* end of tc-sh.h */
