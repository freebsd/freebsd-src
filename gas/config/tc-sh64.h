/* This file is tc-sh64.h
   Copyright 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#define TC_SH64
#include "config/tc-sh.h"
#include "elf/sh.h"
#include "elf32-sh64.h"

/* We need to override the tc-sh.h settings of HANDLE_ALIGN and
   MAX_MEM_FOR_RS_ALIGN_CODE; we might need to put in SHmedia NOP:s, not
   SHcompact NOP:s.  */
#undef  HANDLE_ALIGN
#define HANDLE_ALIGN(frag) sh64_handle_align (frag)
extern void sh64_handle_align (fragS *);

#undef  MAX_MEM_FOR_RS_ALIGN_CODE
#define MAX_MEM_FOR_RS_ALIGN_CODE sh64_max_mem_for_rs_align_code ()
extern int sh64_max_mem_for_rs_align_code (void);

#undef  LISTING_HEADER
#define LISTING_HEADER					\
  (target_big_endian ?					\
     "SuperH SHcompact/SHmedia Big Endian GAS"		\
   : "SuperH SHcompact/SHmedia Little Endian GAS")

/* We need to record the new frag position after an .align.  */
extern void sh64_do_align (int, const char *, int, int);
#define md_do_align(n, fill, len, max, l) \
 do { sh64_do_align (n, fill, len, max); goto l; } while (0)

struct sh64_segment_info_type
{
  /* The type of the section is initialized when the range_start_symbol
     member is non-NULL.  */
  symbolS *mode_start_symbol;
  subsegT mode_start_subseg;

  /* A stored symbol indicating location of last call of
     "md_flush_pending_output".  It is NULLed when we actually use it;
     otherwise the contents is just filled in with segment, frag and
     offset within frag.  */
  symbolS *last_contents_mark;

  unsigned int emitted_ranges;
  enum sh64_elf_cr_type contents_type;

  /* This is used by the SH1-4 parts; we set it to 0 for SHmedia code and
     data.  */
  unsigned int in_code : 1;
};

#undef  TC_SEGMENT_INFO_TYPE
#define TC_SEGMENT_INFO_TYPE struct sh64_segment_info_type

#undef  TARGET_FORMAT
#define TARGET_FORMAT sh64_target_format ()
extern const char *sh64_target_format (void);

#define TARGET_MACH sh64_target_mach ()
extern int sh64_target_mach (void);

#undef TC_FORCE_RELOCATION_LOCAL
#define TC_FORCE_RELOCATION_LOCAL(FIX)			\
  (!(FIX)->fx_pcrel					\
   || (FIX)->fx_plt					\
   || (FIX)->fx_r_type == BFD_RELOC_32_PLT_PCREL	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_PLT_LOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_PLT_MEDLOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_PLT_MEDHI16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_PLT_HI16		\
   || (FIX)->fx_r_type == BFD_RELOC_32_GOT_PCREL	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOT_LOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOT_MEDLOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOT_MEDHI16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOT_HI16		\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOT10BY4		\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOT10BY8		\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT32		\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT_LOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT_MEDLOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT_MEDHI16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT_HI16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT10BY4	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPLT10BY8	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPC		\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPC_LOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPC_MEDLOW16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPC_MEDHI16	\
   || (FIX)->fx_r_type == BFD_RELOC_SH_GOTPC_HI16	\
   || TC_FORCE_RELOCATION (FIX))

#undef TC_FORCE_RELOCATION_SUB_SAME
#define TC_FORCE_RELOCATION_SUB_SAME(FIX, SEC)		\
  (! SEG_NORMAL (SEC)					\
   || TC_FORCE_RELOCATION (FIX)				\
   || (sh_relax && SWITCH_TABLE (FIX))			\
   || *symbol_get_tc ((FIX)->fx_addsy) != NULL)

/* Don't complain when we leave fx_subsy around.  */
#undef TC_VALIDATE_FIX_SUB
#define TC_VALIDATE_FIX_SUB(FIX)			\
  ((FIX)->fx_r_type == BFD_RELOC_32_PLT_PCREL		\
   || (sh_relax && SWITCH_TABLE (FIX))			\
   || *symbol_get_tc ((FIX)->fx_addsy) != NULL)

/* Note the kludge: we want to put back C, and we also want to consume the
   expression, since we have handled it ourselves.  FIXME: What we really
   need is a new GAS infrastructure feature: md_qualifier.  */
#undef md_parse_name
#define md_parse_name(NAME, EXP, MODE, CP) \
 sh64_consume_datalabel (NAME, EXP, MODE, CP, operand)
extern int sh64_consume_datalabel (const char *, expressionS *,
				   enum expr_mode, char *,
				   segT (*) (expressionS *, enum expr_mode));

/* Saying "$" is the same as saying ".".  */
#define DOLLAR_DOT

#undef MD_PCREL_FROM_SECTION
#define MD_PCREL_FROM_SECTION(FIX, SEC)		\
  shmedia_md_pcrel_from_section (FIX, SEC)

extern valueT shmedia_md_pcrel_from_section (struct fix *, segT);

/* We need to mark this symbol as a BranchTarget; setting st_other for it
   and adding 1 to its value (temporarily).  */
extern void sh64_frob_label (symbolS *);

#undef  tc_frob_label
#define tc_frob_label(sym) \
  do { sh_frob_label (sym); sh64_frob_label (sym); } while (0)

#define tc_symbol_new_hook(s) sh64_frob_label (s)

/* We use this to mark our "datalabel" symbol copies.  The "mark" is NULL
   for an ordinary symbol, and the pointer to the "ordinary" symbol for a
   datalabel symbol.  */
#define TC_SYMFIELD_TYPE symbolS *

#define tc_frob_symbol(symp, punt)		\
 do						\
   {						\
     punt = sh64_exclude_symbol (symp);		\
   }						\
 while (0)

extern int sh64_exclude_symbol (symbolS *);

extern void sh64_adjust_symtab (void);
#define tc_adjust_symtab sh64_adjust_symtab

#undef  md_flush_pending_output
#define md_flush_pending_output() sh64_flush_pending_output ()
extern void sh64_flush_pending_output (void);

/* Note that tc-sh.c has a sh_frob_section, but it's called from
   tc_frob_file_before_adjust.  */
#define tc_frob_section(sec) shmedia_frob_section_type (sec)
extern void shmedia_frob_section_type (asection *);

/* We need to emit fixups relative to the frag in which the instruction
   resides.  Safest way without calculating max fragment growth or making
   it a fixed number is to provide a pointer to the opcode frag.

   We also need to emit the right NOP pattern in .align frags.  This is
   done after the text-to-bits assembly pass, so we need to mark it with
   the ISA setting at the time the .align was assembled.  */
#define TC_FRAG_TYPE struct sh64_tc_frag_data

enum sh64_isa_values
 {
   sh64_isa_unspecified,
   sh64_isa_shcompact,
   sh64_isa_shmedia,

   /* Special guard value used in contexts when we don't know which ISA it
      is, just that it's specified (not sh64_isa_unspecified).  */
   sh64_isa_sh5_guard
 };

struct sh64_tc_frag_data
{
  fragS *opc_frag;
  enum sh64_isa_values isa;
};

extern enum sh64_isa_values sh64_isa_mode;

#define TC_FRAG_INIT(FRAGP)					\
 do								\
   {								\
     (FRAGP)->tc_frag_data.opc_frag = sh64_last_insn_frag;	\
     (FRAGP)->tc_frag_data.isa = sh64_isa_mode;			\
   }								\
 while (0)

/* This variable is set whenever we generate (or grow) a new opcode frag
   in shmedia_build_Mytes.  */
extern fragS *sh64_last_insn_frag;

#define md_end() shmedia_md_end ()
void shmedia_md_end (void);

/* Because we make .debug_line hold the SHmedia instruction address | 1,
   we have to say we only have minimum byte-size insns.  */
#undef  DWARF2_LINE_MIN_INSN_LENGTH
#define DWARF2_LINE_MIN_INSN_LENGTH 1

#define TC_FAKE_LABEL(NAME) sh64_fake_label(NAME)
extern int sh64_fake_label (const char *);
