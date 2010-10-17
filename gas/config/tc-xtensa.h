/* tc-xtensa.h -- Header file for tc-xtensa.c.
   Copyright (C) 2003 Free Software Foundation, Inc.

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

#ifndef TC_XTENSA
#define TC_XTENSA 1

#ifdef ANSI_PROTOTYPES
struct fix;
#endif

#ifndef BFD_ASSEMBLER
#error Xtensa support requires BFD_ASSEMBLER
#endif

#ifndef OBJ_ELF
#error Xtensa support requires ELF object format
#endif

#include "xtensa-config.h"

#define TARGET_BYTES_BIG_ENDIAN XCHAL_HAVE_BE


struct xtensa_frag_type
{
  unsigned is_literal:1;
  unsigned is_text:1;
  unsigned is_loop_target:1;
  unsigned is_branch_target:1;
  unsigned is_insn:1;

  /* Info about the current state of assembly, i.e., density, relax,
     generics, freeregs, longcalls.  These need to be passed to the
     backend and then to the linking file.  */

  unsigned is_no_density:1;
  unsigned is_relax:1;
  unsigned is_generics:1;
  unsigned is_longcalls:1;

  /* For text fragments that can generate literals at relax time, this
     variable points to the frag where the literal will be stored.  For
     literal frags, this variable points to the nearest literal pool
     location frag.  This literal frag will be moved to after this
     location.  */

  fragS *literal_frag;

  /* The destination segment for literal frags.  (Note that this is only
     valid after xtensa_move_literals.  */

  segT lit_seg;

  /* For the relaxation scheme, some literal fragments can have their
     expansions modified by an instruction that relaxes.  */

  unsigned text_expansion;
  unsigned literal_expansion;
  unsigned unreported_expansion;
};

typedef struct xtensa_block_info_struct
{
  segT sec;
  bfd_vma offset;
  size_t size;
  struct xtensa_block_info_struct *next;
} xtensa_block_info;

typedef enum
{
  xt_insn_sec,
  xt_literal_sec,
  max_xt_sec
} xt_section_type;

typedef struct xtensa_segment_info_struct
{
  fragS *literal_pool_loc;
  xtensa_block_info *blocks[max_xt_sec];
} xtensa_segment_info;

typedef struct xtensa_symfield_type_struct
{
  unsigned int plt : 1;
  unsigned int is_loop_target : 1;
  unsigned int is_branch_target : 1;
} xtensa_symfield_type;


/* Section renaming is only supported in Tensilica's version of GAS.  */
#define XTENSA_SECTION_RENAME 1
#ifdef  XTENSA_SECTION_RENAME
extern const char *xtensa_section_rename
  PARAMS ((const char *));
#else
/* Tensilica's section renaming feature is not included here.  */
#define xtensa_section_rename(name)	(name)
#endif /* XTENSA_SECTION_RENAME */


extern const char *xtensa_target_format
  PARAMS ((void));
extern void xtensa_frag_init
  PARAMS ((fragS *));
extern void xtensa_cons_fix_new
  PARAMS ((fragS *, int, int, expressionS *));
extern void xtensa_frob_label
  PARAMS ((struct symbol *));
extern void xtensa_end
  PARAMS ((void));
extern void xtensa_post_relax_hook
  PARAMS ((void));
extern void xtensa_file_arch_init
  PARAMS ((bfd *));
extern void xtensa_flush_pending_output
  PARAMS ((void));
extern bfd_boolean xtensa_fix_adjustable
  PARAMS ((struct fix *));
extern void xtensa_symbol_new_hook
  PARAMS ((symbolS *));
extern long xtensa_relax_frag
  PARAMS ((fragS *, long, int *));

#define TARGET_FORMAT			xtensa_target_format ()
#define TARGET_ARCH			bfd_arch_xtensa
#define TC_SEGMENT_INFO_TYPE		xtensa_segment_info
#define TC_SYMFIELD_TYPE		xtensa_symfield_type
#define TC_FRAG_TYPE			struct xtensa_frag_type
#define TC_FRAG_INIT(frag)		xtensa_frag_init (frag)
#define TC_CONS_FIX_NEW			xtensa_cons_fix_new
#define tc_canonicalize_symbol_name(s)	xtensa_section_rename (s)
#define tc_init_after_args()		xtensa_file_arch_init (stdoutput)
#define tc_fix_adjustable(fix)		xtensa_fix_adjustable (fix)
#define tc_frob_label(sym)		xtensa_frob_label (sym)
#define tc_symbol_new_hook(s)		xtensa_symbol_new_hook (s)
#define md_elf_section_rename(name)	xtensa_section_rename (name)
#define md_end				xtensa_end
#define md_flush_pending_output()	xtensa_flush_pending_output ()
#define md_operand(x)
#define TEXT_SECTION_NAME		xtensa_section_rename (".text")
#define DATA_SECTION_NAME		xtensa_section_rename (".data")
#define BSS_SECTION_NAME		xtensa_section_rename (".bss")


/* The renumber_section function must be mapped over all the sections
   after calling xtensa_post_relax_hook.  That function is static in
   write.c so it cannot be called from xtensa_post_relax_hook itself.  */

#define md_post_relax_hook \
  do \
    { \
      int i = 0; \
      xtensa_post_relax_hook (); \
      bfd_map_over_sections (stdoutput, renumber_sections, &i); \
    } \
  while (0)


/* Because xtensa relaxation can insert a new literal into the middle of
   fragment and thus require re-running the relaxation pass on the
   section, we need an explicit flag here.  We explicitly use the name
   "stretched" here to avoid changing the source code in write.c.  */

#define md_relax_frag(segment, fragP, stretch) \
  xtensa_relax_frag (fragP, stretch, &stretched)


#define LOCAL_LABELS_FB 1
#define WORKING_DOT_WORD 1
#define DOUBLESLASH_LINE_COMMENTS
#define TC_HANDLES_FX_DONE
#define TC_FINALIZE_SYMS_BEFORE_SIZE_SEG 0

#define MD_APPLY_SYM_VALUE(FIX) 0

#endif /* TC_XTENSA */
