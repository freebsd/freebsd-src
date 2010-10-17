/* tc-sh64.c -- Assemble code for the SuperH SH SHcompact and SHmedia.
   Copyright 2000, 2001, 2002, 2003 Free Software Foundation.

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

/* This file defines SHmedia ISA-specific functions and includes tc-sh.c.
   The SHcompact ISA is in all useful aspects the "old" sh4 as implemented
   in tc-sh.c.  Not making this file part of tc-sh.c makes it easier to
   keep a leaner sh[1-4]-only implementation.  */

#define HAVE_SH64

#include <stdio.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcodes/sh64-opc.h"

#ifndef OBJ_ELF
#error This file assumes object output is in the ELF format
#endif

/* Suffix used when we make "datalabel" symbol copies.  It must not
   collide with anything that can normally appear in a symbol, "faked
   symbol" or local symbol.  */
#define DATALABEL_SUFFIX " DL"

/* See shmedia_md_apply_fix3 and shmedia_md_pcrel_from_section for usage.  */
#define SHMEDIA_MD_PCREL_FROM_FIX(FIXP) \
 ((FIXP)->fx_size + (FIXP)->fx_where + (FIXP)->fx_frag->fr_address - 4)

/* We use this internally to see which one is PT and which is a PTA/PTB
   that should be error-checked.  We give it a better name here (but not
   one that looks official).  Adding it to reloc.c would make it look too
   much of a real reloc; it is just used temporarily as a fixup-type.  */
#define SHMEDIA_BFD_RELOC_PT BFD_RELOC_12_PCREL

typedef struct
 {
   shmedia_arg_type type;

   /* These could go into a union, but that would uglify the code.  */
   int reg;
   expressionS immediate;

   /* If IMMEDIATE was a shift-expression, like "(S >> N) & 65535", where
      N = 0, 16, 32, 48, used to extract a certain 16-bit-field to make up
      a MOVI or SHORI relocation for a symbol, then we put the
      corresponding reloc-type here and modify the "immediate" expression
      to S.  Otherwise, this is just BFD_RELOC_NONE.  */
   bfd_reloc_code_real_type reloctype;
 } shmedia_operand_info;

/* Frag containing last base instruction.  This is put in the TC field in
   a frag, so we can emit fixups for fr_opcode without needing to make
   sure that the opcode is in the same frag as any variant operand.  */
fragS *sh64_last_insn_frag = NULL;

typedef struct
 {
   shmedia_operand_info operands[3];
   unsigned long ops_val;
 } shmedia_operands_info;

enum sh64_abi_values
 { sh64_abi_unspecified, sh64_abi_32, sh64_abi_64 };

/* What ISA are we assembling code for?  */
enum sh64_isa_values sh64_isa_mode = sh64_isa_unspecified;

/* What ABI was specified, if any (implicitly or explicitly)?  */
static enum sh64_abi_values sh64_abi = sh64_abi_unspecified;

/* A note that says if we're in a sequence of insns without label
   settings, segment or ISA mode changes or emitted data.  */
static bfd_boolean seen_insn = FALSE;

/* This is set to TRUE in shmedia_md_end, so that we don't emit any
   .cranges entries when the assembler calls output functions while
   grinding along after all input is seen.  */
static bfd_boolean sh64_end_of_assembly = FALSE;

/* Controlled by the option -no-mix, this invalidates mixing SHcompact and
   SHmedia code in the same section, and also invalidates mixing data and
   SHmedia code in the same section.  No .cranges will therefore be
   emitted, unless -shcompact-const-crange is specified and there is a
   constant pool in SHcompact code.  */
static bfd_boolean sh64_mix = TRUE;

static bfd_boolean sh64_shcompact_const_crange = FALSE;

/* Controlled by the option -no-expand, this says whether or not we expand
   MOVI and PT/PTA/PTB.  When we do not expand these insns to fit an
   operand, we will emit errors for operands out of range and generate the
   basic instruction and reloc for an external symbol.  */
static bfd_boolean sh64_expand = TRUE;

/* Controlled by the option -expand-pt32, this says whether we expand
   PT/PTA/PTB of an external symbol to (only) 32 or (the full) 64 bits
   when -abi=64 is in effect.  */
static bfd_boolean sh64_pt32 = FALSE;

/* When emitting a .cranges descriptor, we want to avoid getting recursive
   calls through emit_expr.  */
static bfd_boolean emitting_crange = FALSE;

/* SHmedia mnemonics.  */
static struct hash_control *shmedia_opcode_hash_control = NULL;

static const unsigned char shmedia_big_nop_pattern[4] =
 {
   (SHMEDIA_NOP_OPC >> 24) & 255, (SHMEDIA_NOP_OPC >> 16) & 255,
   (SHMEDIA_NOP_OPC >> 8) & 255, SHMEDIA_NOP_OPC & 255
 };

static const unsigned char shmedia_little_nop_pattern[4] =
 {
   SHMEDIA_NOP_OPC & 255, (SHMEDIA_NOP_OPC >> 8) & 255,
   (SHMEDIA_NOP_OPC >> 16) & 255, (SHMEDIA_NOP_OPC >> 24) & 255
 };

static void shmedia_md_begin (void);
static int shmedia_parse_reg (char *, int *, int *, shmedia_arg_type);
static void shmedia_md_assemble (char *);
static void shmedia_md_apply_fix3 (fixS *, valueT *);
static int shmedia_md_estimate_size_before_relax (fragS *, segT);
static int shmedia_init_reloc (arelent *, fixS *);
static char *shmedia_get_operands (shmedia_opcode_info *, char *,
				   shmedia_operands_info *);
static void s_sh64_mode (int);
static void s_sh64_abi (int);
static void shmedia_md_convert_frag (bfd *, segT, fragS *, bfd_boolean);
static void shmedia_check_limits  (offsetT *, bfd_reloc_code_real_type,
				   fixS *);
static void sh64_set_contents_type (enum sh64_elf_cr_type);
static void shmedia_get_operand (char **, shmedia_operand_info *,
				 shmedia_arg_type);
static unsigned long shmedia_immediate_op (char *, shmedia_operand_info *,
					   int, bfd_reloc_code_real_type);
static char *shmedia_parse_exp (char *, shmedia_operand_info *);
static void shmedia_frob_file_before_adjust (void);
static void sh64_emit_crange (symbolS *, symbolS *, enum sh64_elf_cr_type);
static void sh64_flush_last_crange (bfd *, asection *, void *);
static void sh64_flag_output (void);
static void sh64_update_contents_mark (bfd_boolean);
static void sh64_vtable_entry (int);
static void sh64_vtable_inherit (int);
static char *strip_datalabels (void);
static int shmedia_build_Mytes (shmedia_opcode_info *,
				shmedia_operands_info *);
static shmedia_opcode_info *shmedia_find_cooked_opcode (char **);
static unsigned long shmedia_mask_number (unsigned long,
					  bfd_reloc_code_real_type);

#include "tc-sh.c"

void
shmedia_md_end (void)
{
  symbolS *symp;

  /* First, update the last range to include whatever data was last
     emitted.  */
  sh64_update_contents_mark (TRUE);

  /* Make sure frags generated after this point are not marked with the
     wrong ISA; make them easily spottable.  We still want to distinguish
     it from sh64_isa_unspecified when we compile for SHcompact or
     SHmedia.  */
  if (sh64_isa_mode != sh64_isa_unspecified)
    sh64_isa_mode = sh64_isa_sh5_guard;

  sh64_end_of_assembly = TRUE;

  bfd_map_over_sections (stdoutput, sh64_flush_last_crange, NULL);

  /* Iterate over segments and emit the last .cranges descriptor.  */
  for (symp = symbol_rootP; symp != NULL; symp = symp->sy_next)
    {
      symbolS *mainsym = *symbol_get_tc (symp);

      /* Is this a datalabel symbol; does it have a pointer to the main
	 symbol?  */
      if (mainsym != NULL)
	{
	  /* If the datalabel symbol is undefined, check if the main
	     symbol has changed in that respect.  */
	  if (S_GET_SEGMENT (symp) == undefined_section)
	    {
	      segT symseg;

	      symseg = S_GET_SEGMENT (mainsym);

	      /* If the symbol is now defined to something that is not
		 global and without STO_SH5_ISA32, we just equate the
		 datalabel symbol to the main symbol, and the lack of
		 STO_SH5_ISA32 will handle the datalabelness.  */
	      if (symseg != undefined_section)
		{
		  if (S_GET_OTHER (mainsym) != STO_SH5_ISA32)
		    {
		      symp->sy_value.X_op = O_symbol;
		      symp->sy_value.X_add_symbol = mainsym;
		      symp->sy_value.X_op_symbol = NULL;
		      symp->sy_value.X_add_number = 0;
		      S_SET_SEGMENT (symp, S_GET_SEGMENT (mainsym));
		      symbol_set_frag (symp, &zero_address_frag);
		      copy_symbol_attributes (symp, mainsym);
		    }
		  else
		    {
		      /* An undefined symbol has since we saw it at
			 "datalabel", been defined to a BranchTarget
			 symbol.  What we need to do here is very similar
			 to when we find the "datalabel" for a defined
			 symbol.  FIXME: Break out to common function.  */
		      symbol_set_value_expression (symp,
						   symbol_get_value_expression
						   (mainsym));
		      S_SET_SEGMENT (symp, symseg);
		      symbol_set_frag (symp, symbol_get_frag (mainsym));
		      copy_symbol_attributes (symp, mainsym);

		      /* Unset the BranchTarget mark that can be set at
			 attribute-copying.  */
		      S_SET_OTHER (symp,
				   S_GET_OTHER (symp) & ~STO_SH5_ISA32);

		      /* The GLOBAL and WEAK attributes are not copied
			 over by copy_symbol_attributes.  Do it here.  */
		      if (S_IS_WEAK (mainsym))
			S_SET_WEAK (symp);
		      else if (S_IS_EXTERNAL (mainsym))
			S_SET_EXTERNAL (symp);
		    }
		}
	      else
		{
		  /* A symbol that was defined at the time we saw
		     "datalabel" can since have been attributed with being
		     weak or global.  */
		  if (S_IS_WEAK (mainsym))
		    S_SET_WEAK (symp);
		  else if (S_IS_EXTERNAL (mainsym))
		    S_SET_EXTERNAL (symp);
		}
	    }
	}
    }

  for (symp = symbol_rootP; symp != NULL; symp = symp->sy_next)
    if (S_GET_OTHER (symp) & STO_SH5_ISA32)
      symp->sy_value.X_add_number++;
}

/* When resolving symbols, the main assembler has done us a misfavour.  It
   has removed the equation to the main symbol for a datalabel reference
   that should be equal to the main symbol, e.g. when it's a global or
   weak symbol and is a non-BranchTarget symbol anyway.  We change that
   back, so that relocs are against the main symbol, not the local "section
   + offset" value.  */

static void
shmedia_frob_file_before_adjust (void)
{
  symbolS *symp;
  for (symp = symbol_rootP; symp != NULL; symp = symp->sy_next)
    {
      symbolS *mainsym = *symbol_get_tc (symp);

      if (mainsym != NULL
	  && S_GET_OTHER (mainsym) != STO_SH5_ISA32
	  && (S_IS_EXTERN (mainsym) || S_IS_WEAK (mainsym)))
	{
	  symp->sy_value.X_op = O_symbol;
	  symp->sy_value.X_add_symbol = mainsym;
	  symp->sy_value.X_op_symbol = NULL;
	  symp->sy_value.X_add_number = 0;

	  /* For the "equation trick" to work, we have to set the section
	     to undefined.  */
	  S_SET_SEGMENT (symp, undefined_section);
	  symbol_set_frag (symp, &zero_address_frag);
	  copy_symbol_attributes (symp, mainsym);

	  /* Don't forget to remove the STO_SH5_ISA32 attribute after
	     copying the other attributes.  */
	  S_SET_OTHER (symp, S_GET_OTHER (symp) & ~STO_SH5_ISA32);
	}
    }
}

/* We need to mark the current location after the alignment.  This is
   copied code the caller, do_align.  We mark the frag location before and
   after as we need and arrange to skip the same code in do_align.

   An alternative to code duplication is to call the do_align recursively,
   arranging to fall through into do_align if we're already here.  That
   would require do_align as an incoming function parameter, since it's
   static in read.c.  That solution was discarded a too kludgy.  */

void
sh64_do_align (int n, const char *fill, int len, int max)
{
  /* Update region, or put a data region in front.  */
  sh64_update_contents_mark (TRUE);

  /* Only make a frag if we HAVE to...  */
  if (n != 0 && !need_pass_2)
    {
      if (fill == NULL)
	{
	  if (subseg_text_p (now_seg))
	    frag_align_code (n, max);
	  else
	    frag_align (n, 0, max);
	}
      else if (len <= 1)
	frag_align (n, *fill, max);
      else
	frag_align_pattern (n, fill, len, max);
    }

  /* Update mark for current region with current type.  */
  sh64_update_contents_mark (FALSE);
}

/* The MAX_MEM_FOR_RS_ALIGN_CODE worker.  We have to find out the ISA of
   the current segment at this position.  We can't look just at
   sh64_isa_shmedia, and we can't look at frag_now.  This is brittle:
   callers are currently frag_align_code from subsegs_finish in write.c
   (end of assembly) and frag_align_code from do_align in read.c (during
   assembly).  */

int
sh64_max_mem_for_rs_align_code (void)
{
  segment_info_type *seginfo;
  fragS *mode_start_frag;
  seginfo = seg_info (now_seg);

  /* We don't use the contents type we find at the tc_segment_info_data,
     since that does not give us absolute information about the ISA; the
     contents type can presumably be CRT_DATA and we'd be none the wiser.
     Instead we use the information stored at the frag of the symbol at
     the start of this range.  If any information is missing or NULL,
     assume SHcompact.  */
  return
    /* If the current ISA mode is SHmedia, that's the mode that we're
       going to assign to the new frag, so request enough memory for
       it, even if we switch modes afterwards, otherwise we may
       allocate too little memory and end up overflowing our buffer.  */
    (sh64_isa_mode == sh64_isa_shmedia
     || (sh64_isa_mode != sh64_isa_unspecified
	 && seginfo != NULL
	 && seginfo->tc_segment_info_data.mode_start_symbol != NULL
	 && ((mode_start_frag
	      = (symbol_get_frag
		 (seginfo->tc_segment_info_data.mode_start_symbol)))
	     != NULL)
	 && mode_start_frag->tc_frag_data.isa == sh64_isa_shmedia))
    ? (3 + 4) : (2 + 1);
}

/* Put in SHmedia NOP:s if the alignment was created when in SHmedia mode.  */

void
sh64_handle_align (fragS * frag)
{
  int bytes = frag->fr_next->fr_address - frag->fr_address - frag->fr_fix;
  char * p  = frag->fr_literal + frag->fr_fix;

  if (frag->tc_frag_data.isa == sh64_isa_shmedia
      && frag->fr_type == rs_align_code)
    {
      while (bytes & 3)
	{
	  *p++ = 0;
	  bytes--;
	  frag->fr_fix += 1;
	}

      if (target_big_endian)
	{
	  memcpy (p, shmedia_big_nop_pattern,
		  sizeof shmedia_big_nop_pattern);
	  frag->fr_var = sizeof shmedia_big_nop_pattern;
	}
      else
	{
	  memcpy (p, shmedia_little_nop_pattern,
		  sizeof shmedia_little_nop_pattern);
	  frag->fr_var = sizeof shmedia_little_nop_pattern;
	}
    }
  else
    /* Punt to SHcompact function.  */
    sh_handle_align (frag);
}

/* Set SEC_SH64_ISA32 for SHmedia sections.  */

void
shmedia_frob_section_type (asection *sec)
{
  segment_info_type *seginfo;
  seginfo = seg_info (sec);

  /* This and elf32-sh64.c:sh64_elf_fake_sections are the only places
     where we use anything else than ELF header flags to communicate the
     section as containing SHmedia or other contents.  BFD SEC_* section
     flags are running out and should not be overloaded with
     target-specific semantics.  This target is ELF only (semantics not
     defined for other formats), so we use the target-specific pointer
     field of the ELF section data.  */
  if (seginfo && sh64_abi == sh64_abi_32)
    {
      struct sh64_section_data *sec_elf_data;
      flagword sec_type = 0;

      if (seginfo->tc_segment_info_data.emitted_ranges != 0)
	sec_type = SHF_SH5_ISA32_MIXED;
      else if (seginfo->tc_segment_info_data.contents_type == CRT_SH5_ISA32)
	sec_type = SHF_SH5_ISA32;

      sec_elf_data = sh64_elf_section_data (sec)->sh64_info;
      if (sec_elf_data == NULL)
	{
	  sec_elf_data = xcalloc (1, sizeof (*sec_elf_data));
	  sh64_elf_section_data (sec)->sh64_info = sec_elf_data;
	}

      sec_elf_data->contents_flags = sec_type;
    }
}

/* This function is called by write_object_file right before the symbol
   table is written.  We subtract 1 from all symbols marked STO_SH5_ISA32,
   as their values are temporarily incremented in shmedia_md_end, before
   symbols values are used by relocs and fixups.

   To increment all symbols and then decrement here is admittedly a
   hackish solution.  The alternative is to add infrastructure and hooks
   to symbol evaluation that evaluates symbols differently internally to
   the value output into the object file, but at the moment that just
   seems too much for little benefit.  */

void
sh64_adjust_symtab (void)
{
  symbolS *symp;

  for (symp = symbol_rootP; symp; symp = symbol_next (symp))
    {
      symbolS *main_symbol = *symbol_get_tc (symp);

      if (main_symbol)
	{
	  char *sym_name = (char *) S_GET_NAME (symp);

	  /* All datalabels not used in relocs should be gone by now.

	     We change those remaining to have the name of the main
	     symbol, and we set the ELF type of the symbol of the reloc to
	     STT_DATALABEL.  */
	  sym_name[strlen (sym_name) - strlen (DATALABEL_SUFFIX)] = 0;
	  elf_symbol (symbol_get_bfdsym (symp))->internal_elf_sym.st_info
	    = STT_DATALABEL;

	  /* Also set this symbol to "undefined", so we'll have only one
	     definition.  */
	  S_SET_SEGMENT (symp, undefined_section);
	}
      else if (S_GET_OTHER (symp) & STO_SH5_ISA32)
	{
	  /* It's important to change the BFD symbol value, since it is now
	     set to the GAS symbolS value.  */
	  symp->bsym->value--;

	  /* Note that we do *not* adjust symp->sy_value.X_add_number.  If
	     you do this, the test case in sh/sh64/immexpr2.s will fail.
	     This is because *after* symbols have been output but before
	     relocs are output, fixups are inspected one more time, and
	     some leftover expressions are resolved.  To resolve to the
	     same values, those expressions must have the same GAS symbol
	     values before as after symbols have been output.  We could
	     "symp->sy_value.X_add_number++" on the STO_SH5_ISA32 symbols
	     through tc_frob_file after symbols have been output, but that
	     would be too gross.  */
	}
    }
}

/* Fill-in an allocated arelent.  */

static int
shmedia_init_reloc (arelent *rel, fixS *fixP)
{
  /* Adjust parts of *relp according to *fixp, and tell that it has been
     done, so default initializations will not happen.   */
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_64:
    case BFD_RELOC_64_PCREL:
    case BFD_RELOC_SH_IMM_LOW16:
    case BFD_RELOC_SH_IMM_MEDLOW16:
    case BFD_RELOC_SH_IMM_MEDHI16:
    case BFD_RELOC_SH_IMM_HI16:
    case BFD_RELOC_SH_IMM_LOW16_PCREL:
    case BFD_RELOC_SH_IMM_MEDLOW16_PCREL:
    case BFD_RELOC_SH_IMM_MEDHI16_PCREL:
    case BFD_RELOC_SH_IMM_HI16_PCREL:
    case BFD_RELOC_SH_IMMU5:
    case BFD_RELOC_SH_IMMU6:
    case BFD_RELOC_SH_IMMS6:
    case BFD_RELOC_SH_IMMS10:
    case BFD_RELOC_SH_IMMS10BY2:
    case BFD_RELOC_SH_IMMS10BY4:
    case BFD_RELOC_SH_IMMS10BY8:
    case BFD_RELOC_SH_IMMS16:
    case BFD_RELOC_SH_IMMU16:
    case BFD_RELOC_SH_PT_16:
    case BFD_RELOC_SH_GOT_LOW16:
    case BFD_RELOC_SH_GOT_MEDLOW16:
    case BFD_RELOC_SH_GOT_MEDHI16:
    case BFD_RELOC_SH_GOT_HI16:
    case BFD_RELOC_SH_GOT10BY4:
    case BFD_RELOC_SH_GOT10BY8:
    case BFD_RELOC_SH_GOTPLT_LOW16:
    case BFD_RELOC_SH_GOTPLT_MEDLOW16:
    case BFD_RELOC_SH_GOTPLT_MEDHI16:
    case BFD_RELOC_SH_GOTPLT_HI16:
    case BFD_RELOC_SH_GOTPLT10BY4:
    case BFD_RELOC_SH_GOTPLT10BY8:
    case BFD_RELOC_SH_GOTOFF_LOW16:
    case BFD_RELOC_SH_GOTOFF_MEDLOW16:
    case BFD_RELOC_SH_GOTOFF_MEDHI16:
    case BFD_RELOC_SH_GOTOFF_HI16:
    case BFD_RELOC_SH_GOTPC_LOW16:
    case BFD_RELOC_SH_GOTPC_MEDLOW16:
    case BFD_RELOC_SH_GOTPC_MEDHI16:
    case BFD_RELOC_SH_GOTPC_HI16:
    case BFD_RELOC_SH_PLT_LOW16:
    case BFD_RELOC_SH_PLT_MEDLOW16:
    case BFD_RELOC_SH_PLT_MEDHI16:
    case BFD_RELOC_SH_PLT_HI16:
      rel->addend = fixP->fx_addnumber + fixP->fx_offset;
      return 1;

    case BFD_RELOC_SH_IMMS6BY32:
      /* This must be resolved in assembly; we do not support it as a
	 reloc in an object file.  */
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("This operand must be constant at assembly time"));
      break;

      /* There are valid cases where we get here for other than SHmedia
	 relocs, so don't make a BAD_CASE out of this.  */
    default:
      ;
    }

  return 0;
}

/* Hook called from md_apply_fix3 in tc-sh.c.  */

static void
shmedia_md_apply_fix3 (fixS *fixP, valueT *valp)
{
  offsetT val = *valp;
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  unsigned long insn
    = target_big_endian ? bfd_getb32 (buf) : bfd_getl32 (buf);
  bfd_reloc_code_real_type orig_fx_r_type = fixP->fx_r_type;

  /* Change a 64-bit pc-relative reloc into the correct type, just like
     tc-sh.c:md_apply_fix.  */
  if (fixP->fx_pcrel)
    {
      switch (orig_fx_r_type)
	{
	case BFD_RELOC_64:
	case BFD_RELOC_SH_IMM_LOW16:
	case BFD_RELOC_SH_IMM_MEDLOW16:
	case BFD_RELOC_SH_IMM_MEDHI16:
	case BFD_RELOC_SH_IMM_HI16:
	  /* Because write.c calls MD_PCREL_FROM_SECTION twice, we need to
	     undo one of the adjustments, if the relocation is not
	     actually for a symbol within the same segment (which we
	     cannot check, because we're not called from md_apply_fix3, so
	     we have to keep the reloc).  FIXME: This is a bug in
	     write.c:fixup_segment affecting most targets that change
	     ordinary relocs to pcrel relocs in md_apply_fix.  */
	  fixP->fx_offset
	    = *valp + SHMEDIA_MD_PCREL_FROM_FIX (fixP);
	  break;

	case BFD_RELOC_SH_PLT_LOW16:
	case BFD_RELOC_SH_PLT_MEDLOW16:
	case BFD_RELOC_SH_PLT_MEDHI16:
	case BFD_RELOC_SH_PLT_HI16:
	case BFD_RELOC_SH_GOTPC_LOW16:
	case BFD_RELOC_SH_GOTPC_MEDLOW16:
	case BFD_RELOC_SH_GOTPC_MEDHI16:
	case BFD_RELOC_SH_GOTPC_HI16:
	  *valp = 0;
	  return;

	default:
	  ;
	}

      /* We might need to change some relocs into the corresponding
	 PC-relative one.  */
      switch (orig_fx_r_type)
	{
	case BFD_RELOC_64:
	  fixP->fx_r_type = BFD_RELOC_64_PCREL;
	  break;

	case BFD_RELOC_SH_IMM_LOW16:
	  fixP->fx_r_type = BFD_RELOC_SH_IMM_LOW16_PCREL;
	  break;

	case BFD_RELOC_SH_IMM_MEDLOW16:
	  fixP->fx_r_type = BFD_RELOC_SH_IMM_MEDLOW16_PCREL;
	  break;

	case BFD_RELOC_SH_IMM_MEDHI16:
	  fixP->fx_r_type = BFD_RELOC_SH_IMM_MEDHI16_PCREL;
	  break;

	case BFD_RELOC_SH_IMM_HI16:
	  fixP->fx_r_type = BFD_RELOC_SH_IMM_HI16_PCREL;
	  break;

	case SHMEDIA_BFD_RELOC_PT:
	  /* This is how we see a difference between PT and PTA when not
	     expanding (in which case we handle it in
	     shmedia_md_convert_frag).  Note that we don't see a
	     difference after the reloc is emitted.  */
	  fixP->fx_r_type = BFD_RELOC_SH_PT_16;
	  break;

	case BFD_RELOC_SH_PT_16:
	  /* This tells us there was a PTA or PTB insn explicitly
	     expressed as such (not as PT).  We "or" in a 1 into the
	     lowest bit in the (unused) destination field to tell the
	     linker that it should check the right ISA type of the
	     destination and not just change a PTA to PTB (if necessary).  */
	  md_number_to_chars (buf, insn | (1 << 10), 4);
	  break;

	case BFD_RELOC_64_PCREL:
	case BFD_RELOC_SH_IMM_LOW16_PCREL:
	case BFD_RELOC_SH_IMM_MEDLOW16_PCREL:
	case BFD_RELOC_SH_IMM_MEDHI16_PCREL:
	case BFD_RELOC_SH_IMM_HI16_PCREL:
	  /* Already handled.  */
	  break;

	default:
	  /* Everything else that changes into a pc-relative relocation is
	     an error.  */
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("Invalid operand expression"));
	  break;
	}

      return;
    }

  /* If an expression looked like it was PC-relative, but was completely
     resolvable, we end up here with the result only in *VALP, and no
     relocation will be emitted.  */
  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    {
      /* Emit error for an out-of-range value.  */
      shmedia_check_limits (valp, fixP->fx_r_type, fixP);

      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_SH_IMM_LOW16:
	  md_number_to_chars (buf, insn | ((val & 65535) << 10), 4);
	  break;

	case BFD_RELOC_SH_IMM_MEDLOW16:
	  md_number_to_chars (buf,
			      insn
			      | ((valueT) (val & ((valueT) 65535 << 16))
				 >> (16 - 10)), 4);
	  break;

	case BFD_RELOC_SH_IMM_MEDHI16:
	  md_number_to_chars (buf,
			      insn
			      | ((valueT) (val & ((valueT) 65535 << 32))
				 >> (32 - 10)), 4);
	  break;

	case BFD_RELOC_SH_IMM_HI16:
	  md_number_to_chars (buf,
			      insn
			      | ((valueT) (val & ((valueT) 65535 << 48))
				 >> (48 - 10)), 4);
	  break;

	case BFD_RELOC_SH_IMMS16:
	case BFD_RELOC_SH_IMMU16:
	  md_number_to_chars (buf, insn | ((val & 65535) << 10), 4);
	  break;

	case BFD_RELOC_SH_IMMS10:
	  md_number_to_chars (buf, insn | ((val & 0x3ff) << 10), 4);
	  break;

	case BFD_RELOC_SH_IMMS10BY2:
	  md_number_to_chars (buf,
			      insn | ((val & (0x3ff << 1)) << (10 - 1)), 4);
	  break;

	case BFD_RELOC_SH_IMMS10BY4:
	  md_number_to_chars (buf,
			      insn | ((val & (0x3ff << 2)) << (10 - 2)), 4);
	  break;

	case BFD_RELOC_SH_SHMEDIA_CODE:
	  /* We just ignore and remove this one for the moment.  FIXME:
	     Use it when implementing relaxing.  */
	  break;

	case BFD_RELOC_64:
	  md_number_to_chars (buf, val, 8);
	  break;

	case SHMEDIA_BFD_RELOC_PT:
	  /* Change a PT to PTB if the operand turned out to be SHcompact.
	     The basic opcode specified with PT is equivalent to PTA.  */
	  if ((val & 1) == 0)
	    insn |= SHMEDIA_PTB_BIT;
	  /* Fall through.  */

	case BFD_RELOC_SH_PT_16:
	  if (! sh64_expand || sh_relax)
	    {
	      /* Check if the operand of a PTA or PTB was for the "wrong"
		 ISA.  A PT had an incoming fixup of SHMEDIA_BFD_RELOC_PT,
		 which we have changed to the right type above.  */
	      if (orig_fx_r_type != SHMEDIA_BFD_RELOC_PT)
		{
		  if ((insn & SHMEDIA_PTB_BIT) != 0 && (val & 1) != 0)
		    as_bad_where (fixP->fx_file, fixP->fx_line,
				  _("PTB operand is a SHmedia symbol"));
		  else if ((insn & SHMEDIA_PTB_BIT) == 0 && (val & 1) == 0)
		    as_bad_where (fixP->fx_file, fixP->fx_line,
				  _("PTA operand is a SHcompact symbol"));
		}

	      md_number_to_chars (buf,
				  insn | ((val & (0xffff << 2))
					  << (10 - 2)),
				  4);
	      break;
	    }
	  /* Fall through.  */

	default:
	  /* This isn't a BAD_CASE, because presumably we can get here
	     from unexpected operands.  Since we don't handle them, make
	     them syntax errors.  */
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("invalid expression in operand"));
	}
      fixP->fx_done = 1;
    }
}

/* Hook called from md_convert_frag in tc-sh.c.  */

static void
shmedia_md_convert_frag (bfd *output_bfd ATTRIBUTE_UNUSED,
			 segT seg ATTRIBUTE_UNUSED, fragS *fragP,
			 bfd_boolean final)
{
  /* Pointer to first byte in variable-sized part of the frag.	*/
  char *var_partp;

  /* Pointer to first opcode byte in frag.  */
  char *opcodep;

  /* Pointer to frag of opcode.  */
  fragS *opc_fragP = fragP->tc_frag_data.opc_frag;

  /* Size in bytes of variable-sized part of frag.  */
  int var_part_size = 0;

  /* This is part of *fragP.  It contains all information about addresses
     and offsets to varying parts.  */
  symbolS *symbolP = fragP->fr_symbol;

  bfd_boolean reloc_needed
    = (! final
       || sh_relax
       || symbolP == NULL
       || ! S_IS_DEFINED (symbolP)
       || S_IS_EXTERN (symbolP)
       || S_IS_WEAK (symbolP)
       || (S_GET_SEGMENT (fragP->fr_symbol) != absolute_section
	   && S_GET_SEGMENT (fragP->fr_symbol) != seg));

  bfd_reloc_code_real_type reloctype = BFD_RELOC_NONE;

  unsigned long var_part_offset;

  /* Where, in file space, does addr point?  */
  bfd_vma target_address;
  bfd_vma opcode_address;

  /* What was the insn?  */
  unsigned long insn;
  know (fragP->fr_type == rs_machine_dependent);

  var_part_offset = fragP->fr_fix;
  var_partp = fragP->fr_literal + var_part_offset;
  opcodep = fragP->fr_opcode;

  insn = target_big_endian ? bfd_getb32 (opcodep) : bfd_getl32 (opcodep);

  target_address
    = ((symbolP && final && ! sh_relax ? S_GET_VALUE (symbolP) : 0)
       + fragP->fr_offset);

  /* The opcode that would be extended is the last four "fixed" bytes.  */
  opcode_address = fragP->fr_address + fragP->fr_fix - 4;

  switch (fragP->fr_subtype)
    {
    case C (SH64PCREL16PT_64, SH64PCREL16):
    case C (SH64PCREL16PT_32, SH64PCREL16):
      /* We can get a PT to a relaxed SHcompact address if it is in the
	 same section; a mixed-ISA section.  Change the opcode to PTB if
	 so.  */
      if ((target_address & 1) == 0)
	insn |= SHMEDIA_PTB_BIT;
      /* Fall through.  */

    case C (SH64PCREL16_32, SH64PCREL16):
    case C (SH64PCREL16_64, SH64PCREL16):
      /* Check that a PTA or PTB points to the right type of target.  We
	 can get here for a SHcompact target if we are in a mixed-ISA
	 section.  */
      if (((target_address & 1) == 0) && ((insn & SHMEDIA_PTB_BIT) == 0))
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("PTA operand is a SHcompact symbol"));
      if (((target_address & 1) != 0) && ((insn & SHMEDIA_PTB_BIT) != 0))
	as_bad_where (fragP->fr_file, fragP->fr_line,
		      _("PTB operand is a SHmedia symbol"));

      /* When relaxing, we do not output the address in the insn, but
	 instead a 1 into the low bit.  This matches what the linker
	 expects to find for a BFD_RELOC_SH_PT_16 reloc, when it checks
	 correctness for PTA/PTB insn; used when the target address is
	 unknown (which is not the case here).  */
      md_number_to_chars (opcodep,
			  insn
			  | (((sh_relax
			       ? 1 : ((target_address - opcode_address) / 4))
			      & ((1 << 16) - 1)) << 10),
			  4);

      /* Note that we do not emit info that this was originally a PT since
	 we have resolved to which one of PTA or PTB it will be.  */
      if (sh_relax)
	fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		 fragP->fr_symbol, fragP->fr_offset, 1, BFD_RELOC_SH_PT_16);
      var_part_size = 0;
      break;

    case C (SH64PCREL16_32, SH64PCRELPLT):
    case C (SH64PCREL16PT_32, SH64PCRELPLT):
      reloctype = BFD_RELOC_32_PLT_PCREL;
      reloc_needed = 1;
      /* Fall through */

    case C (SH64PCREL16_32, SH64PCREL32):
    case C (SH64PCREL16_64, SH64PCREL32):
    case C (SH64PCREL16PT_32, SH64PCREL32):
    case C (SH64PCREL16PT_64, SH64PCREL32):
      /* In the fixed bit, put in a MOVI.  */
      md_number_to_chars (opcodep,
			  SHMEDIA_MOVI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | ((((reloc_needed
				? 0 : (target_address - (opcode_address + 8))
				) >> 16) & 65535) << 10),
			  4);

      /* Fill in a SHORI for the low part.  */
      md_number_to_chars (var_partp,
			  SHMEDIA_SHORI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | (((reloc_needed
			       ? 0 : (target_address - (opcode_address + 8)))
			      & 65535) << 10),
			  4);

      /* End with a "PTREL R25,TRd".  */
      md_number_to_chars (var_partp + 4,
			  SHMEDIA_PTREL_OPC | (insn & SHMEDIA_LIKELY_BIT)
			  | (SHMEDIA_TEMP_REG << 10)
			  | (insn & (7 << 4)),
			  4);

      /* We need relocs only if the target symbol was undefined or if
	 we're relaxing.  */
      if (reloc_needed)
	{
	  fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		   fragP->fr_symbol, fragP->fr_offset - 8, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_MEDLOW16
		   : BFD_RELOC_SH_IMM_MEDLOW16_PCREL);
	  fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		   fragP->fr_offset - 4, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_LOW16
		   : BFD_RELOC_SH_IMM_LOW16_PCREL);
	}

      var_part_size = 8;
      break;

    case C (SH64PCREL16_64, SH64PCREL48):
    case C (SH64PCREL16PT_64, SH64PCREL48):
      /* In the fixed bit, put in a MOVI.  */
      md_number_to_chars (opcodep,
			  SHMEDIA_MOVI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | ((((reloc_needed
				? 0 : (target_address - (opcode_address + 12))
				) >> 32) & 65535) << 10),
			  4);

      /* The first SHORI, for the medium part.  */
      md_number_to_chars (var_partp,
			  SHMEDIA_SHORI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | ((((reloc_needed
				? 0 : (target_address - (opcode_address + 12))
				) >> 16) & 65535) << 10),
			  4);

      /* Fill in a SHORI for the low part.  */
      md_number_to_chars (var_partp + 4,
			  SHMEDIA_SHORI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | (((reloc_needed
			       ? 0 : (target_address - (opcode_address + 12)))
			      & 65535) << 10),
			  4);

      /* End with a "PTREL R25,TRd".  */
      md_number_to_chars (var_partp + 8,
			  SHMEDIA_PTREL_OPC | (insn & SHMEDIA_LIKELY_BIT)
			  | (SHMEDIA_TEMP_REG << 10)
			  | (insn & (7 << 4)),
			  4);

      /* We need relocs only if the target symbol was undefined or if
	 we're relaxing.  */
      if (reloc_needed)
	{
	  fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		   fragP->fr_symbol, fragP->fr_offset - 12, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_MEDHI16
		   : BFD_RELOC_SH_IMM_MEDHI16_PCREL);
	  fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		   fragP->fr_offset - 8, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_MEDLOW16
		   : BFD_RELOC_SH_IMM_MEDLOW16_PCREL);
	  fix_new (fragP, var_partp - fragP->fr_literal + 4, 4, fragP->fr_symbol,
		   fragP->fr_offset - 4, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_LOW16
		   : BFD_RELOC_SH_IMM_LOW16_PCREL);
	}

      var_part_size = 12;
      break;

    case C (SH64PCREL16_64, SH64PCRELPLT):
    case C (SH64PCREL16PT_64, SH64PCRELPLT):
      reloctype = BFD_RELOC_32_PLT_PCREL;
      reloc_needed = 1;
      /* Fall through */

    case C (SH64PCREL16_64, SH64PCREL64):
    case C (SH64PCREL16PT_64, SH64PCREL64):
      /* In the fixed bit, put in a MOVI.  */
      md_number_to_chars (opcodep,
			  SHMEDIA_MOVI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | ((((reloc_needed
				? 0 : (target_address - (opcode_address + 16))
				) >> 48) & 65535) << 10),
			  4);

      /* The first SHORI, for the medium-high part.  */
      md_number_to_chars (var_partp,
			  SHMEDIA_SHORI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | ((((reloc_needed
				? 0 : (target_address - (opcode_address + 16))
				) >> 32) & 65535) << 10),
			  4);

      /* A SHORI, for the medium-low part.  */
      md_number_to_chars (var_partp + 4,
			  SHMEDIA_SHORI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | ((((reloc_needed
				? 0 : (target_address - (opcode_address + 16))
				) >> 16) & 65535) << 10),
			  4);

      /* Fill in a SHORI for the low part.  */
      md_number_to_chars (var_partp + 8,
			  SHMEDIA_SHORI_OPC
			  | (SHMEDIA_TEMP_REG << 4)
			  | (((reloc_needed
			       ? 0 : (target_address - (opcode_address + 16)))
			      & 65535) << 10),
			  4);

      /* End with a "PTREL R25,TRd".  */
      md_number_to_chars (var_partp + 12,
			  SHMEDIA_PTREL_OPC | (insn & SHMEDIA_LIKELY_BIT)
			  | (SHMEDIA_TEMP_REG << 10)
			  | (insn & (7 << 4)),
			  4);

      /* We need relocs only if the target symbol was undefined or if
	 we're relaxing.  */
      if (reloc_needed)
	{
	  fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		   fragP->fr_symbol, fragP->fr_offset - 16, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_HI16
		   : BFD_RELOC_SH_IMM_HI16_PCREL);
	  fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		   fragP->fr_offset - 12, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_MEDHI16
		   : BFD_RELOC_SH_IMM_MEDHI16_PCREL);
	  fix_new (fragP, var_partp - fragP->fr_literal + 4, 4, fragP->fr_symbol,
		   fragP->fr_offset - 8, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_MEDLOW16
		   : BFD_RELOC_SH_IMM_MEDLOW16_PCREL);
	  fix_new (fragP, var_partp - fragP->fr_literal + 8, 4, fragP->fr_symbol,
		   fragP->fr_offset - 4, 1,
		   reloctype == BFD_RELOC_32_PLT_PCREL
		   ? BFD_RELOC_SH_PLT_LOW16
		   : BFD_RELOC_SH_IMM_LOW16_PCREL);
	}

      var_part_size = 16;
      break;

    case C (MOVI_IMM_64, MOVI_GOTOFF):
      reloctype = BFD_RELOC_32_GOTOFF;
      reloc_needed = 1;
      /* Fall through.  */

    case C (MOVI_IMM_64, UNDEF_MOVI):
    case C (MOVI_IMM_64, MOVI_64):
      {
	/* We only get here for undefined symbols, so we can simplify
	   handling compared to those above; we have 0 in the parts that
	   will be filled with the symbol parts.  */

	int reg = (insn >> 4) & 0x3f;

	/* In the fixed bit, put in a MOVI.  */
	md_number_to_chars (opcodep, SHMEDIA_MOVI_OPC | (reg << 4), 4);
	fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		 fragP->fr_symbol, fragP->fr_offset, 0,
		 reloctype == BFD_RELOC_NONE
		 ? BFD_RELOC_SH_IMM_HI16
		 : reloctype == BFD_RELOC_32_GOTOFF
		 ? BFD_RELOC_SH_GOTOFF_HI16
		 : (abort (), BFD_RELOC_SH_IMM_HI16));

	/* The first SHORI, for the medium-high part.  */
	md_number_to_chars (var_partp, SHMEDIA_SHORI_OPC | (reg << 4), 4);
	fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		 fragP->fr_offset, 0,
		 reloctype == BFD_RELOC_NONE
		 ? BFD_RELOC_SH_IMM_MEDHI16
		 : reloctype == BFD_RELOC_32_GOTOFF
		 ? BFD_RELOC_SH_GOTOFF_MEDHI16
		 : (abort (), BFD_RELOC_SH_IMM_MEDHI16));

	/* A SHORI, for the medium-low part.  */
	md_number_to_chars (var_partp + 4,
			    SHMEDIA_SHORI_OPC | (reg << 4), 4);
	fix_new (fragP, var_partp - fragP->fr_literal + 4, 4, fragP->fr_symbol,
		 fragP->fr_offset, 0,
		 reloctype == BFD_RELOC_NONE
		 ? BFD_RELOC_SH_IMM_MEDLOW16
		 : reloctype == BFD_RELOC_32_GOTOFF
		 ? BFD_RELOC_SH_GOTOFF_MEDLOW16
		 : (abort (), BFD_RELOC_SH_IMM_MEDLOW16));

	/* Fill in a SHORI for the low part.  */
	md_number_to_chars (var_partp + 8,
			    SHMEDIA_SHORI_OPC | (reg << 4), 4);
	fix_new (fragP, var_partp - fragP->fr_literal + 8, 4, fragP->fr_symbol,
		 fragP->fr_offset, 0,
		 reloctype == BFD_RELOC_NONE
		 ? BFD_RELOC_SH_IMM_LOW16
		 : reloctype == BFD_RELOC_32_GOTOFF
		 ? BFD_RELOC_SH_GOTOFF_LOW16
		 : (abort (), BFD_RELOC_SH_IMM_LOW16));

	var_part_size = 12;
	break;
      }

    case C (MOVI_IMM_32, MOVI_GOTOFF):
      reloctype = BFD_RELOC_32_GOTOFF;
      reloc_needed = 1;
      /* Fall through.  */

    case C (MOVI_IMM_32, UNDEF_MOVI):
    case C (MOVI_IMM_32, MOVI_32):
      {
	/* Note that we only get here for undefined symbols.  */

	int reg = (insn >> 4) & 0x3f;

	/* A MOVI, for the high part.  */
	md_number_to_chars (opcodep, SHMEDIA_MOVI_OPC | (reg << 4), 4);
	fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		 fragP->fr_symbol, fragP->fr_offset, 0,
		 reloctype == BFD_RELOC_NONE
		 ? BFD_RELOC_SH_IMM_MEDLOW16
		 : reloctype == BFD_RELOC_32_GOTOFF
		 ? BFD_RELOC_SH_GOTOFF_MEDLOW16
		 : reloctype == BFD_RELOC_SH_GOTPC
		 ? BFD_RELOC_SH_GOTPC_MEDLOW16
		 : reloctype == BFD_RELOC_32_PLT_PCREL
		 ? BFD_RELOC_SH_PLT_MEDLOW16
		 : (abort (), BFD_RELOC_SH_IMM_MEDLOW16));

	/* Fill in a SHORI for the low part.  */
	md_number_to_chars (var_partp,
			    SHMEDIA_SHORI_OPC | (reg << 4), 4);
	fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		 fragP->fr_offset, 0,
		 reloctype == BFD_RELOC_NONE
		 ? BFD_RELOC_SH_IMM_LOW16
		 : reloctype == BFD_RELOC_32_GOTOFF
		 ? BFD_RELOC_SH_GOTOFF_LOW16
		 : reloctype == BFD_RELOC_SH_GOTPC
		 ? BFD_RELOC_SH_GOTPC_LOW16
		 : reloctype == BFD_RELOC_32_PLT_PCREL
		 ? BFD_RELOC_SH_PLT_LOW16
		 : (abort (), BFD_RELOC_SH_IMM_LOW16));

	var_part_size = 4;
	break;
      }

    case C (MOVI_IMM_32_PCREL, MOVI_16):
    case C (MOVI_IMM_64_PCREL, MOVI_16):
      md_number_to_chars (opcodep,
			  insn
			  | (((reloc_needed
			       ? 0 : (target_address - opcode_address))
			      & 65535) << 10),
			  4);
      if (reloc_needed)
	fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		 fragP->fr_symbol, fragP->fr_offset, 1,
		 BFD_RELOC_SH_IMM_LOW16_PCREL);
      var_part_size = 0;
      break;

    case C (MOVI_IMM_32, MOVI_16):
    case C (MOVI_IMM_64, MOVI_16):
      md_number_to_chars (opcodep,
			  insn
			  | (((reloc_needed ? 0 : target_address)
			      & 65535) << 10),
			  4);
      if (reloc_needed)
	abort ();
      var_part_size = 0;
      break;

    case C (MOVI_IMM_32_PCREL, MOVI_PLT):
      reloctype = BFD_RELOC_32_PLT_PCREL;
      goto movi_imm_32_pcrel_reloc_needed;

    case C (MOVI_IMM_32_PCREL, MOVI_GOTPC):
      reloctype = BFD_RELOC_SH_GOTPC;
      /* Fall through.  */

    movi_imm_32_pcrel_reloc_needed:
      reloc_needed = 1;
      /* Fall through.  */

    case C (MOVI_IMM_32_PCREL, MOVI_32):
    case C (MOVI_IMM_64_PCREL, MOVI_32):
      {
	int reg = (insn >> 4) & 0x3f;

	md_number_to_chars (opcodep,
			    insn
			    | (((((reloc_needed
				   ? 0 : (target_address - opcode_address)))
				>> 16) & 65535) << 10), 4);

	/* A SHORI, for the low part.  */
	md_number_to_chars (var_partp,
			    SHMEDIA_SHORI_OPC
			    | (reg << 4)
			    | (((reloc_needed
				 ? 0 : (target_address - opcode_address))
				& 65535) << 10), 4);
	if (reloc_needed)
	  {
	    fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		     fragP->fr_symbol, fragP->fr_offset, 1,
		     reloctype == BFD_RELOC_NONE
		     ? BFD_RELOC_SH_IMM_MEDLOW16_PCREL
		     : reloctype == BFD_RELOC_SH_GOTPC
		     ? BFD_RELOC_SH_GOTPC_MEDLOW16
		     : reloctype == BFD_RELOC_32_PLT_PCREL
		     ? BFD_RELOC_SH_PLT_MEDLOW16
		     : (abort (), BFD_RELOC_SH_IMM_MEDLOW16_PCREL));
	    fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		     fragP->fr_offset + 4, 1,
		     reloctype == BFD_RELOC_NONE
		     ? BFD_RELOC_SH_IMM_LOW16_PCREL
		     : reloctype == BFD_RELOC_SH_GOTPC
		     ? BFD_RELOC_SH_GOTPC_LOW16
		     : reloctype == BFD_RELOC_32_PLT_PCREL
		     ? BFD_RELOC_SH_PLT_LOW16
		     : (abort (), BFD_RELOC_SH_IMM_LOW16_PCREL));
	  }
	var_part_size = 4;
      }
      break;

    case C (MOVI_IMM_32_PCREL, MOVI_48):
    case C (MOVI_IMM_64_PCREL, MOVI_48):
      {
	int reg = (insn >> 4) & 0x3f;

	md_number_to_chars (opcodep,
			    insn
			    | (((((reloc_needed
				   ? 0 : (target_address - opcode_address)))
				>> 32) & 65535) << 10), 4);

	/* A SHORI, for the medium part.  */
	md_number_to_chars (var_partp,
			    SHMEDIA_SHORI_OPC
			    | (reg << 4)
			    | ((((reloc_needed
				  ? 0 : (target_address - opcode_address))
				 >> 16) & 65535) << 10), 4);

	/* A SHORI, for the low part.  */
	md_number_to_chars (var_partp + 4,
			    SHMEDIA_SHORI_OPC
			    | (reg << 4)
			    | (((reloc_needed
				 ? 0 : (target_address - opcode_address))
				& 65535) << 10), 4);
	if (reloc_needed)
	  {
	    fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		     fragP->fr_symbol, fragP->fr_offset, 1,
		     BFD_RELOC_SH_IMM_MEDHI16_PCREL);
	    fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		     fragP->fr_offset + 4, 1, BFD_RELOC_SH_IMM_MEDLOW16_PCREL);
	    fix_new (fragP, var_partp - fragP->fr_literal + 4, 4, fragP->fr_symbol,
		     fragP->fr_offset + 8, 1, BFD_RELOC_SH_IMM_LOW16_PCREL);
	  }
	var_part_size = 8;
      }
      break;

    case C (MOVI_IMM_64_PCREL, MOVI_PLT):
      reloctype = BFD_RELOC_32_PLT_PCREL;
      goto movi_imm_64_pcrel_reloc_needed;

    case C (MOVI_IMM_64_PCREL, MOVI_GOTPC):
      reloctype = BFD_RELOC_SH_GOTPC;
      /* Fall through.  */

    movi_imm_64_pcrel_reloc_needed:
      reloc_needed = 1;
      /* Fall through.  */

    case C (MOVI_IMM_32_PCREL, MOVI_64):
    case C (MOVI_IMM_64_PCREL, MOVI_64):
      {
	int reg = (insn >> 4) & 0x3f;

	md_number_to_chars (opcodep,
			    insn
			    | (((((reloc_needed
				   ? 0 : (target_address - opcode_address)))
				>> 48) & 65535) << 10), 4);

	/* A SHORI, for the medium-high part.  */
	md_number_to_chars (var_partp,
			    SHMEDIA_SHORI_OPC
			    | (reg << 4)
			    | ((((reloc_needed
				  ? 0 : (target_address - opcode_address))
				 >> 32) & 65535) << 10), 4);

	/* A SHORI, for the medium-low part.  */
	md_number_to_chars (var_partp + 4,
			    SHMEDIA_SHORI_OPC
			    | (reg << 4)
			    | ((((reloc_needed
				  ? 0 : (target_address - opcode_address))
				 >> 16) & 65535) << 10), 4);

	/* A SHORI, for the low part.  */
	md_number_to_chars (var_partp + 8,
			    SHMEDIA_SHORI_OPC
			    | (reg << 4)
			    | (((reloc_needed
				 ? 0 : (target_address - opcode_address))
				& 65535) << 10), 4);
	if (reloc_needed)
	  {
	    fix_new (opc_fragP, opcodep - opc_fragP->fr_literal, 4,
		     fragP->fr_symbol, fragP->fr_offset, 1,
		     reloctype == BFD_RELOC_NONE
		     ? BFD_RELOC_SH_IMM_HI16_PCREL
		     : reloctype == BFD_RELOC_SH_GOTPC
		     ? BFD_RELOC_SH_GOTPC_HI16
		     : reloctype == BFD_RELOC_32_PLT_PCREL
		     ? BFD_RELOC_SH_PLT_HI16
		     : (abort (), BFD_RELOC_SH_IMM_HI16_PCREL));
	    fix_new (fragP, var_partp - fragP->fr_literal, 4, fragP->fr_symbol,
		     fragP->fr_offset + 4, 1,
		     reloctype == BFD_RELOC_NONE
		     ? BFD_RELOC_SH_IMM_MEDHI16_PCREL
		     : reloctype == BFD_RELOC_SH_GOTPC
		     ? BFD_RELOC_SH_GOTPC_MEDHI16
		     : reloctype == BFD_RELOC_32_PLT_PCREL
		     ? BFD_RELOC_SH_PLT_MEDHI16
		     : (abort (), BFD_RELOC_SH_IMM_MEDHI16_PCREL));
	    fix_new (fragP, var_partp - fragP->fr_literal + 4, 4,
		     fragP->fr_symbol,
		     fragP->fr_offset + 8, 1,
		     reloctype == BFD_RELOC_NONE
		     ? BFD_RELOC_SH_IMM_MEDLOW16_PCREL
		     : reloctype == BFD_RELOC_SH_GOTPC
		     ? BFD_RELOC_SH_GOTPC_MEDLOW16
		     : reloctype == BFD_RELOC_32_PLT_PCREL
		     ? BFD_RELOC_SH_PLT_MEDLOW16
		     : (abort (), BFD_RELOC_SH_IMM_MEDLOW16_PCREL));
	    fix_new (fragP, var_partp - fragP->fr_literal + 8, 4,
		     fragP->fr_symbol,
		     fragP->fr_offset + 12, 1,
		     reloctype == BFD_RELOC_NONE
		     ? BFD_RELOC_SH_IMM_LOW16_PCREL
		     : reloctype == BFD_RELOC_SH_GOTPC
		     ? BFD_RELOC_SH_GOTPC_LOW16
		     : reloctype == BFD_RELOC_32_PLT_PCREL
		     ? BFD_RELOC_SH_PLT_LOW16
		     : (abort (), BFD_RELOC_SH_IMM_LOW16_PCREL));
	  }
	var_part_size = 12;
      }
      break;

    default:
      BAD_CASE (fragP->fr_subtype);
    }

  fragP->fr_fix += var_part_size;
  fragP->fr_var = 0;
}

/* Mask NUMBER (originating from a signed number) corresponding to the HOW
   reloc.  */

static unsigned long
shmedia_mask_number (unsigned long number, bfd_reloc_code_real_type how)
{
  switch (how)
    {
    case BFD_RELOC_SH_IMMU5:
      number &= (1 << 5) - 1;
      break;

    case BFD_RELOC_SH_IMMS6:
    case BFD_RELOC_SH_IMMU6:
      number &= (1 << 6) - 1;
      break;

    case BFD_RELOC_SH_IMMS6BY32:
      number = (number & ((1 << (6 + 5)) - 1)) >> 5;
      break;

    case BFD_RELOC_SH_IMMS10:
      number &= (1 << 10) - 1;
      break;

    case BFD_RELOC_SH_IMMS10BY2:
      number = (number & ((1 << (10 + 1)) - 1)) >> 1;
      break;

    case BFD_RELOC_SH_IMMS10BY4:
      number = (number & ((1 << (10 + 2)) - 1)) >> 2;
      break;

    case BFD_RELOC_SH_IMMS10BY8:
      number = (number & ((1 << (10 + 3)) - 1)) >> 3;
      break;

    case BFD_RELOC_SH_IMMS16:
    case BFD_RELOC_SH_IMMU16:
      number &= (1 << 16) - 1;
      break;

    default:
      BAD_CASE (how);
    }

  return number;
}

/* Emit errors for values out-of-range, using as_bad_where if FRAGP is
   non-NULL, as_bad otherwise.  */

static void
shmedia_check_limits (offsetT *valp, bfd_reloc_code_real_type reloc,
		      fixS *fixp)
{
  offsetT val = *valp;

  char *msg = NULL;

  switch (reloc)
    {
    case BFD_RELOC_SH_IMMU5:
      if (val < 0 || val > (1 << 5) - 1)
	msg = _("invalid operand, not a 5-bit unsigned value: %d");
      break;

    case BFD_RELOC_SH_IMMS6:
      if (val < -(1 << 5) || val > (1 << 5) - 1)
	msg = _("invalid operand, not a 6-bit signed value: %d");
      break;

    case BFD_RELOC_SH_IMMU6:
      if (val < 0 || val > (1 << 6) - 1)
	msg = _("invalid operand, not a 6-bit unsigned value: %d");
      break;

    case BFD_RELOC_SH_IMMS6BY32:
      if (val < -(1 << 10) || val > (1 << 10) - 1)
	msg = _("invalid operand, not a 11-bit signed value: %d");
      else if (val & 31)
	msg = _("invalid operand, not a multiple of 32: %d");
      break;

    case BFD_RELOC_SH_IMMS10:
      if (val < -(1 << 9) || val > (1 << 9) - 1)
	msg = _("invalid operand, not a 10-bit signed value: %d");
      break;

    case BFD_RELOC_SH_IMMS10BY2:
      if (val < -(1 << 10) || val > (1 << 10) - 1)
	msg = _("invalid operand, not a 11-bit signed value: %d");
      else if (val & 1)
	msg = _("invalid operand, not an even value: %d");
      break;

    case BFD_RELOC_SH_IMMS10BY4:
      if (val < -(1 << 11) || val > (1 << 11) - 1)
	msg = _("invalid operand, not a 12-bit signed value: %d");
      else if (val & 3)
	msg = _("invalid operand, not a multiple of 4: %d");
      break;

    case BFD_RELOC_SH_IMMS10BY8:
      if (val < -(1 << 12) || val > (1 << 12) - 1)
	msg = _("invalid operand, not a 13-bit signed value: %d");
      else if (val & 7)
	msg = _("invalid operand, not a multiple of 8: %d");
      break;

    case BFD_RELOC_SH_IMMS16:
      if (val < -(1 << 15) || val > (1 << 15) - 1)
	msg = _("invalid operand, not a 16-bit signed value: %d");
      break;

    case BFD_RELOC_SH_IMMU16:
      if (val < 0 || val > (1 << 16) - 1)
	msg = _("invalid operand, not an 16-bit unsigned value: %d");
      break;

    case BFD_RELOC_SH_PT_16:
    case SHMEDIA_BFD_RELOC_PT:
      if (val < -(1 << 15) * 4 || val > ((1 << 15) - 1) * 4 + 1)
	msg = _("operand out of range for PT, PTA and PTB");
      else if ((val % 4) != 0 && ((val - 1) % 4) != 0)
	msg = _("operand not a multiple of 4 for PT, PTA or PTB: %d");
      break;

      /* These have no limits; they take a 16-bit slice of a 32- or 64-bit
	 number.  */
    case BFD_RELOC_SH_IMM_HI16:
    case BFD_RELOC_SH_IMM_MEDHI16:
    case BFD_RELOC_SH_IMM_MEDLOW16:
    case BFD_RELOC_SH_IMM_LOW16:
    case BFD_RELOC_SH_IMM_HI16_PCREL:
    case BFD_RELOC_SH_IMM_MEDHI16_PCREL:
    case BFD_RELOC_SH_IMM_MEDLOW16_PCREL:
    case BFD_RELOC_SH_IMM_LOW16_PCREL:

    case BFD_RELOC_SH_SHMEDIA_CODE:
      break;

      /* This one has limits out of our reach.  */
    case BFD_RELOC_64:
      break;

    default:
      BAD_CASE (reloc);
    }

  if (msg)
    {
      if (fixp)
	as_bad_where (fixp->fx_file, fixp->fx_line, msg, val);
      else
	as_bad (msg, val);
    }
}

/* Handle an immediate operand by checking limits and noting it for later
   evaluation if not computable yet, and return a bitfield suitable to
   "or" into the opcode (non-zero if the value was a constant number).  */

static unsigned long
shmedia_immediate_op (char *where, shmedia_operand_info *op, int pcrel,
		      bfd_reloc_code_real_type how)
{
  unsigned long retval = 0;

  /* If this is not an absolute number, make it a fixup.  A constant in
     place of a pc-relative operand also needs a fixup.  */
  if (op->immediate.X_op != O_constant || pcrel)
    fix_new_exp (frag_now,
		 where - frag_now->fr_literal,
		 4,
		 &op->immediate,
		 pcrel,
		 how);
  else
    {
      /* Check that the number is within limits as represented by the
	 reloc, and return the number.  */
      shmedia_check_limits (&op->immediate.X_add_number, how, NULL);

      retval
	= shmedia_mask_number ((unsigned long) op->immediate.X_add_number,
			       how);
    }

  return retval << 10;
}

/* Try and parse a register name case-insensitively, return the number of
   chars consumed.  */

static int
shmedia_parse_reg (char *src, int *mode, int *reg, shmedia_arg_type argtype)
{
  int l0 = TOLOWER (src[0]);
  int l1 = l0 ? TOLOWER (src[1]) : 0;

  if (l0 == 'r')
    {
      if (src[1] >= '1' && src[1] <= '5')
	{
	  if (src[2] >= '0' && src[2] <= '9'
	      && ! IDENT_CHAR ((unsigned char) src[3]))
	    {
	      *mode = A_GREG_M;
	      *reg = 10 * (src[1] - '0') + src[2] - '0';
	      return 3;
	    }
	}

      if (src[1] == '6')
	{
	  if (src[2] >= '0' && src[2] <= '3'
	      && ! IDENT_CHAR ((unsigned char) src[3]))
	    {
	      *mode = A_GREG_M;
	      *reg = 60 + src[2] - '0';
	      return 3;
	    }
	}

      if (src[1] >= '0' && src[1] <= '9'
	  && ! IDENT_CHAR ((unsigned char) src[2]))
	{
	  *mode = A_GREG_M;
	  *reg = (src[1] - '0');
	  return 2;
	}
    }

  if (l0 == 't' && l1 == 'r')
    {
      if (src[2] >= '0' && src[2] <= '7'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_TREG_B;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }

  if (l0 == 'f' && l1 == 'r')
    {
      if (src[2] >= '1' && src[2] <= '5')
	{
	  if (src[3] >= '0' && src[3] <= '9'
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_FREG_G;
	      *reg = 10 * (src[2] - '0') + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] == '6')
	{
	  if (src[3] >= '0' && src[3] <= '3'
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_FREG_G;
	      *reg = 60 + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] >= '0' && src[2] <= '9'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_FREG_G;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }

  if (l0 == 'f' && l1 == 'v')
    {
      if (src[2] >= '1' && src[2] <= '5')
	{
	  if (src[3] >= '0' && src[3] <= '9'
	      && ((10 * (src[2] - '0') + src[3] - '0') % 4) == 0
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_FVREG_G;
	      *reg = 10 * (src[2] - '0') + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] == '6')
	{
	  if (src[3] == '0'
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_FVREG_G;
	      *reg = 60 + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] >= '0' && src[2] <= '9'
	  && ((src[2] - '0') % 4) == 0
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_FVREG_G;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }

  if (l0 == 'd' && l1 == 'r')
    {
      if (src[2] >= '1' && src[2] <= '5')
	{
	  if (src[3] >= '0' && src[3] <= '9'
	      && ((src[3] - '0') % 2) == 0
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_DREG_G;
	      *reg = 10 * (src[2] - '0') + src[3] - '0';
	      return 4;
	    }
	}

      if (src[2] == '6')
	{
	  if ((src[3] == '0' || src[3] == '2')
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_DREG_G;
	      *reg = 60 + src[3] - '0';
	      return 4;
	    }
	}

      if (src[2] >= '0' && src[2] <= '9'
	  && ((src[2] - '0') % 2) == 0
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_DREG_G;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }

  if (l0 == 'f' && l1 == 'p')
    {
      if (src[2] >= '1' && src[2] <= '5')
	{
	  if (src[3] >= '0' && src[3] <= '9'
	      && ((src[3] - '0') % 2) == 0
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_FPREG_G;
	      *reg = 10 * (src[2] - '0') + src[3] - '0';
	      return 4;
	    }
	}

      if (src[2] == '6')
	{
	  if ((src[3] == '0' || src[3] == '2')
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_FPREG_G;
	      *reg = 60 + src[3] - '0';
	      return 4;
	    }
	}

      if (src[2] >= '0' && src[2] <= '9'
	  && ((src[2] - '0') % 2) == 0
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_FPREG_G;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }

  if (l0 == 'm' && strncasecmp (src, "mtrx", 4) == 0)
    {
      if (src[4] == '0' && ! IDENT_CHAR ((unsigned char) src[5]))
	{
	  *mode = A_FMREG_G;
	  *reg = 0;
	  return 5;
	}

      if (src[4] == '1' && src[5] == '6'
	  && ! IDENT_CHAR ((unsigned char) src[6]))
	{
	  *mode = A_FMREG_G;
	  *reg = 16;
	  return 6;
	}

      if (src[4] == '3' && src[5] == '2'
	  && ! IDENT_CHAR ((unsigned char) src[6]))
	{
	  *mode = A_FMREG_G;
	  *reg = 32;
	  return 6;
	}

      if (src[4] == '4' && src[5] == '8'
	  && ! IDENT_CHAR ((unsigned char) src[6]))
	{
	  *mode = A_FMREG_G;
	  *reg = 48;
	  return 6;
	}
    }

  if (l0 == 'c' && l1 == 'r')
    {
      if (src[2] >= '1' && src[2] <= '5')
	{
	  if (src[3] >= '0' && src[3] <= '9'
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_CREG_K;
	      *reg = 10 * (src[2] - '0') + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] == '6')
	{
	  if (src[3] >= '0' && src[3] <= '3'
	      && ! IDENT_CHAR ((unsigned char) src[4]))
	    {
	      *mode = A_CREG_K;
	      *reg = 60 + src[3] - '0';
	      return 4;
	    }
	}
      if (src[2] >= '0' && src[2] <= '9'
	  && ! IDENT_CHAR ((unsigned char) src[3]))
	{
	  *mode = A_CREG_K;
	  *reg = (src[2] - '0');
	  return 3;
	}
    }

  /* We either have an error, a symbol or a control register by predefined
     name.  To keep things simple but still fast for normal cases, we do
     linear search in the (not to big) table of predefined control
     registers.  We only do this when we *expect* a control register.
     Those instructions should be rare enough that linear searching is ok.
     Or just read them into a hash-table in shmedia_md_begin.  Since they
     cannot be specified in the same place of symbol operands, don't add
     them there to the *main* symbol table as being in "reg_section".  */
  if (argtype == A_CREG_J || argtype == A_CREG_K)
    {
      const shmedia_creg_info *cregp;
      int len = 0;

      for (cregp = shmedia_creg_table; cregp->name != NULL; cregp++)
	{
	  len = strlen (cregp->name);
	  if (strncasecmp (cregp->name, src, len) == 0
	      && ! IDENT_CHAR (src[len]))
	    break;
	}

      if (cregp->name != NULL)
	{
	  *mode = A_CREG_K;
	  *reg = cregp->cregno;
	  return len;
	}
    }

  return 0;
}

/* Called from md_estimate_size_before_relax in tc-sh.c  */

static int
shmedia_md_estimate_size_before_relax (fragS *fragP,
				       segT segment_type ATTRIBUTE_UNUSED)
{
  int old_fr_fix;
  expressionS *exp;

  /* For ELF, we can't relax externally visible symbols; see tc-i386.c.  */
  bfd_boolean sym_relaxable
    = (fragP->fr_symbol
       && S_GET_SEGMENT (fragP->fr_symbol) == segment_type
       && ! S_IS_EXTERNAL (fragP->fr_symbol)
       && ! S_IS_WEAK (fragP->fr_symbol));

  old_fr_fix = fragP->fr_fix;

  switch (fragP->fr_subtype)
    {
    case C (SH64PCREL16_32, UNDEF_SH64PCREL):
    case C (SH64PCREL16PT_32, UNDEF_SH64PCREL):
      /* Used to be to somewhere which was unknown.  */
      if (sym_relaxable)
	{
	  int what = GET_WHAT (fragP->fr_subtype);

	  /* In this segment, so head for shortest.  */
	  fragP->fr_subtype = C (what, SH64PCREL16);
	}
      else
	{
	  int what = GET_WHAT (fragP->fr_subtype);
	  /* We know the abs value, but we don't know where we will be
	     linked, so we must make it the longest.  Presumably we could
	     switch to a non-pcrel representation, but having absolute
	     values in PT operands should be rare enough not to be worth
	     adding that code.  */
	  fragP->fr_subtype = C (what, SH64PCREL32);
	}
      fragP->fr_var = md_relax_table[fragP->fr_subtype].rlx_length;
      break;

    case C (SH64PCREL16_64, UNDEF_SH64PCREL):
    case C (SH64PCREL16PT_64, UNDEF_SH64PCREL):
      /* Used to be to somewhere which was unknown.  */
      if (sym_relaxable)
	{
	  int what = GET_WHAT (fragP->fr_subtype);

	  /* In this segment, so head for shortest.  */
	  fragP->fr_subtype = C (what, SH64PCREL16);
	}
      else
	{
	  int what = GET_WHAT (fragP->fr_subtype);
	  /* We know the abs value, but we don't know where we will be
	     linked, so we must make it the longest.  Presumably we could
	     switch to a non-pcrel representation, but having absolute
	     values in PT operands should be rare enough not to be worth
	     adding that code.  */
	  fragP->fr_subtype = C (what, SH64PCREL64);
	}
      fragP->fr_var = md_relax_table[fragP->fr_subtype].rlx_length;
      break;

    case C (MOVI_IMM_64, UNDEF_MOVI):
    case C (MOVI_IMM_32, UNDEF_MOVI):
      exp = NULL;

      /* Look inside the "symbol".  If we find a PC-relative expression,
	 change this to a PC-relative, relaxable expression.  */
      if (fragP->fr_symbol != NULL
	  && (exp = symbol_get_value_expression (fragP->fr_symbol)) != NULL
	  && exp->X_op == O_subtract
	  && exp->X_op_symbol != NULL
	  && S_GET_SEGMENT (exp->X_op_symbol) == segment_type)
	{
	  int what = GET_WHAT (fragP->fr_subtype);
	  int what_high = what == MOVI_IMM_32 ? MOVI_32 : MOVI_64;
	  expressionS *opexp
	    = symbol_get_value_expression (exp->X_op_symbol);
	  expressionS *addexp
	    = symbol_get_value_expression (exp->X_add_symbol);

	  /* Change the MOVI expression to the "X" in "X - Y" and subtract
	     Y:s offset to this location from X.  Note that we can only
	     allow an Y which is offset from this frag.  */
	  if (opexp != NULL
	      && addexp != NULL
	      && opexp->X_op == O_constant
	      && fragP == symbol_get_frag (exp->X_op_symbol))
	    {
	      /* At this point, before relaxing, the add-number of opexp
		 is the offset from the fr_fix part.  */
	      fragP->fr_offset
		= (exp->X_add_number
		   - (opexp->X_add_number - (fragP->fr_fix - 4)));
	      fragP->fr_symbol = exp->X_add_symbol;

	      what = what == MOVI_IMM_32
		? MOVI_IMM_32_PCREL : MOVI_IMM_64_PCREL;

	      /* Check the "X" symbol to estimate the size of this
		 PC-relative expression.  */
	      if (S_GET_SEGMENT (exp->X_add_symbol) == segment_type
		  && ! S_IS_EXTERNAL (exp->X_add_symbol)
		  && ! S_IS_WEAK (exp->X_add_symbol))
		fragP->fr_subtype = C (what, MOVI_16);
	      else
		fragP->fr_subtype = C (what, what_high);

	      /* This is now a PC-relative expression, fit to be relaxed.  */
	    }
	  else
	    fragP->fr_subtype = C (what, what_high);
	}
      else if (fragP->fr_symbol == NULL
	       || (S_GET_SEGMENT (fragP->fr_symbol) == absolute_section
		   && exp->X_op == O_constant))
	{
	  unsigned long insn
	    = (target_big_endian
	       ? bfd_getb32 (fragP->fr_opcode)
	       : bfd_getl32 (fragP->fr_opcode));
	  offsetT one = (offsetT) 1;
	  offsetT value = fragP->fr_offset
	    + (fragP->fr_symbol == NULL ? 0 : S_GET_VALUE (fragP->fr_symbol));

	  if (value >= ((offsetT) -1 << 15) && value < ((offsetT) 1 << 15))
	    {
	      /* Fits in 16-bit signed number.  */
	      int what = GET_WHAT (fragP->fr_subtype);
	      fragP->fr_subtype = C (what, MOVI_16);

	      /* Just "or" in the value.  */
	      md_number_to_chars (fragP->fr_opcode,
				  insn | ((value & ((1 << 16) - 1)) << 10),
				  4);
	    }
	  else if (value >= -(one << 31)
		   && (value < (one << 31)
		       || (sh64_abi == sh64_abi_32 && value < (one << 32))))
	    {
	      /* The value fits in a 32-bit signed number.  */
	      int reg = (insn >> 4) & 0x3f;

	      /* Just "or" in the high bits of the value, making the first
		 MOVI.  */
	      md_number_to_chars (fragP->fr_opcode,
				  insn
				  | (((value >> 16) & ((1 << 16) - 1)) << 10),
				  4);

	      /* Add a SHORI with the low bits.  Note that this insn lives
		 in the variable fragment part.  */
	      md_number_to_chars (fragP->fr_literal + old_fr_fix,
				  SHMEDIA_SHORI_OPC
				  | (reg << 4)
				  | ((value & ((1 << 16) - 1)) << 10),
				  4);

	      /* We took a piece of the variable part.  */
	      fragP->fr_fix += 4;
	    }
	  else if (GET_WHAT (fragP->fr_subtype) == MOVI_IMM_32)
	    {
	      /* Value out of range.  */
	      as_bad_where (fragP->fr_file, fragP->fr_line,
			    _("MOVI operand is not a 32-bit signed value: 0x%8x%08x"),
			    ((unsigned int) (value >> 32)
			     & (unsigned int) 0xffffffff),
			    (unsigned int) value & (unsigned int) 0xffffffff);

	      /* Must advance size, or we will get internal inconsistency
		 and fall into an assert.  */
	      fragP->fr_fix += 4;
	    }
	  /* Now we know we are allowed to expand to 48- and 64-bit values.  */
	  else if (value >= -(one << 47) && value < (one << 47))
	    {
	      /* The value fits in a 48-bit signed number.  */
	      int reg = (insn >> 4) & 0x3f;

	      /* Just "or" in the high bits of the value, making the first
		 MOVI.  */
	      md_number_to_chars (fragP->fr_opcode,
				  insn
				  | (((value >> 32) & ((1 << 16) - 1)) << 10),
				  4);

	      /* Add a SHORI with the middle bits.  Note that this insn lives
		 in the variable fragment part.  */
	      md_number_to_chars (fragP->fr_literal + old_fr_fix,
				  SHMEDIA_SHORI_OPC
				  | (reg << 4)
				  | (((value >> 16) & ((1 << 16) - 1)) << 10),
				  4);

	      /* Add a SHORI with the low bits.  */
	      md_number_to_chars (fragP->fr_literal + old_fr_fix + 4,
				  SHMEDIA_SHORI_OPC
				  | (reg << 4)
				  | ((value & ((1 << 16) - 1)) << 10),
				  4);

	      /* We took a piece of the variable part.  */
	      fragP->fr_fix += 8;
	    }
	  else
	    {
	      /* A 64-bit number.  */
	      int reg = (insn >> 4) & 0x3f;

	      /* Just "or" in the high bits of the value, making the first
		 MOVI.  */
	      md_number_to_chars (fragP->fr_opcode,
				  insn
				  | (((value >> 48) & ((1 << 16) - 1)) << 10),
				  4);

	      /* Add a SHORI with the midhigh bits.  Note that this insn lives
		 in the variable fragment part.  */
	      md_number_to_chars (fragP->fr_literal + old_fr_fix,
				  SHMEDIA_SHORI_OPC
				  | (reg << 4)
				  | (((value >> 32) & ((1 << 16) - 1)) << 10),
				  4);

	      /* Add a SHORI with the midlow bits.  */
	      md_number_to_chars (fragP->fr_literal + old_fr_fix + 4,
				  SHMEDIA_SHORI_OPC
				  | (reg << 4)
				  | (((value >> 16) & ((1 << 16) - 1)) << 10),
				  4);

	      /* Add a SHORI with the low bits.  */
	      md_number_to_chars (fragP->fr_literal + old_fr_fix + 8,
				  SHMEDIA_SHORI_OPC
				  | (reg << 4)
				  | ((value & ((1 << 16) - 1)) << 10), 4);
	      /* We took all of the variable part.  */
	      fragP->fr_fix += 12;
	    }

	  /* MOVI expansions that get here have not been converted to
	     PC-relative frags, but instead expanded by
	     md_number_to_chars or by calling shmedia_md_convert_frag
	     with final == FALSE.  We must not have them around as
	     frags anymore; symbols would be prematurely evaluated
	     when relaxing.  We will not need to have md_convert_frag
	     called again with them; any further handling is through
	     the already emitted fixups.  */
	  frag_wane (fragP);
	  break;
	}
      fragP->fr_var = md_relax_table[fragP->fr_subtype].rlx_length;
      break;

      /* For relaxation states that remain unchanged, report the
         estimated length.  */
    case C (SH64PCREL16_32, SH64PCREL16):
    case C (SH64PCREL16PT_32, SH64PCREL16):
    case C (SH64PCREL16_32, SH64PCREL32):
    case C (SH64PCREL16PT_32, SH64PCREL32):
    case C (SH64PCREL16_32, SH64PCRELPLT):
    case C (SH64PCREL16PT_32, SH64PCRELPLT):
    case C (SH64PCREL16_64, SH64PCREL16):
    case C (SH64PCREL16PT_64, SH64PCREL16):
    case C (SH64PCREL16_64, SH64PCREL32):
    case C (SH64PCREL16PT_64, SH64PCREL32):
    case C (SH64PCREL16_64, SH64PCREL48):
    case C (SH64PCREL16PT_64, SH64PCREL48):
    case C (SH64PCREL16_64, SH64PCREL64):
    case C (SH64PCREL16PT_64, SH64PCREL64):
    case C (SH64PCREL16_64, SH64PCRELPLT):
    case C (SH64PCREL16PT_64, SH64PCRELPLT):
    case C (MOVI_IMM_32, MOVI_16):
    case C (MOVI_IMM_32, MOVI_32):
    case C (MOVI_IMM_32, MOVI_GOTOFF):
    case C (MOVI_IMM_32_PCREL, MOVI_16):
    case C (MOVI_IMM_32_PCREL, MOVI_32):
    case C (MOVI_IMM_32_PCREL, MOVI_PLT):
    case C (MOVI_IMM_32_PCREL, MOVI_GOTPC):
    case C (MOVI_IMM_64, MOVI_16):
    case C (MOVI_IMM_64, MOVI_32):
    case C (MOVI_IMM_64, MOVI_48):
    case C (MOVI_IMM_64, MOVI_64):
    case C (MOVI_IMM_64, MOVI_GOTOFF):
    case C (MOVI_IMM_64_PCREL, MOVI_16):
    case C (MOVI_IMM_64_PCREL, MOVI_32):
    case C (MOVI_IMM_64_PCREL, MOVI_48):
    case C (MOVI_IMM_64_PCREL, MOVI_64):
    case C (MOVI_IMM_64_PCREL, MOVI_PLT):
    case C (MOVI_IMM_64_PCREL, MOVI_GOTPC):
      fragP->fr_var = md_relax_table[fragP->fr_subtype].rlx_length;
      break;

    default:
      abort ();
    }

  return fragP->fr_var + (fragP->fr_fix - old_fr_fix);
}

/* Parse an expression, SH64-style.  Copied from tc-sh.c, but with
   datatypes adjusted.  */

static char *
shmedia_parse_exp (char *s, shmedia_operand_info *op)
{
  char *save;
  char *new;

  save = input_line_pointer;
  input_line_pointer = s;
  expression (&op->immediate);
  if (op->immediate.X_op == O_absent)
    as_bad (_("missing operand"));
  new = input_line_pointer;
  input_line_pointer = save;
  return new;
}

/* Parse an operand.  Store pointer to next character in *PTR.  */

static void
shmedia_get_operand (char **ptr, shmedia_operand_info *op,
		     shmedia_arg_type argtype)
{
  char *src = *ptr;
  int mode = -1;
  unsigned int len;

  len = shmedia_parse_reg (src, &mode, &(op->reg), argtype);
  if (len)
    {
      *ptr = src + len;
      op->type = mode;
    }
  else
    {
      /* Not a reg, so it must be a displacement.  */
      *ptr = shmedia_parse_exp (src, op);
      op->type = A_IMMM;

      /* This is just an initialization; shmedia_get_operands will change
	 as needed.  */
      op->reloctype = BFD_RELOC_NONE;
    }
}

/* Parse the operands for this insn; return NULL if invalid, else return
   how much text was consumed.  */

static char *
shmedia_get_operands (shmedia_opcode_info *info, char *args,
		      shmedia_operands_info *operands)
{
  char *ptr = args;
  int i;

  if (*ptr == ' ')
    ptr++;

  for (i = 0; info->arg[i] != 0; i++)
    {
      memset (operands->operands + i, 0, sizeof (operands->operands[0]));

      /* No operand to get for these fields.  */
      if (info->arg[i] == A_REUSE_PREV)
	continue;

      shmedia_get_operand (&ptr, &operands->operands[i], info->arg[i]);

      /* Check operands type match.  */
      switch (info->arg[i])
	{
	case A_GREG_M:
	case A_GREG_N:
	case A_GREG_D:
	  if (operands->operands[i].type != A_GREG_M)
	    return NULL;
	  break;

	case A_FREG_G:
	case A_FREG_H:
	case A_FREG_F:
	  if (operands->operands[i].type != A_FREG_G)
	    return NULL;
	  break;

	case A_FVREG_G:
	case A_FVREG_H:
	case A_FVREG_F:
	  if (operands->operands[i].type != A_FVREG_G)
	    return NULL;
	  break;

	case A_FMREG_G:
	case A_FMREG_H:
	case A_FMREG_F:
	  if (operands->operands[i].type != A_FMREG_G)
	    return NULL;
	  break;

	case A_FPREG_G:
	case A_FPREG_H:
	case A_FPREG_F:
	  if (operands->operands[i].type != A_FPREG_G)
	    return NULL;
	  break;

	case A_DREG_G:
	case A_DREG_H:
	case A_DREG_F:
	  if (operands->operands[i].type != A_DREG_G)
	    return NULL;
	  break;

	case A_TREG_A:
	case A_TREG_B:
	  if (operands->operands[i].type != A_TREG_B)
	    return NULL;
	  break;

	case A_CREG_J:
	case A_CREG_K:
	  if (operands->operands[i].type != A_CREG_K)
	    return NULL;
	  break;

	case A_IMMS16:
	case A_IMMU16:
	  /* Check for an expression that looks like S & 65535 or
	     (S >> N) & 65535, where N = 0, 16, 32, 48.

	     Get the S and put at operands->operands[i].immediate, and
	     adjust operands->operands[i].reloctype.  */
	  {
	    expressionS *imm_expr = &operands->operands[i].immediate;
	    expressionS *right_expr;

	    if (operands->operands[i].type == A_IMMM
		&& imm_expr->X_op == O_bit_and
		&& imm_expr->X_op_symbol != NULL
		&& ((right_expr
		     = symbol_get_value_expression (imm_expr->X_op_symbol))
		    ->X_op == O_constant)
		&& right_expr->X_add_number == 0xffff)
	      {
		symbolS *inner = imm_expr->X_add_symbol;
		bfd_reloc_code_real_type reloctype = BFD_RELOC_SH_IMM_LOW16;
		expressionS *inner_expr
		  = symbol_get_value_expression (inner);

		if (inner_expr->X_op == O_right_shift)
		  {
		    expressionS *inner_right;

		    if (inner_expr->X_op_symbol != NULL
		      && ((inner_right
			   = symbol_get_value_expression (inner_expr
							  ->X_op_symbol))
			  ->X_op == O_constant))
		      {
			offsetT addnum
			  = inner_right->X_add_number;

			if (addnum == 0 || addnum == 16 || addnum == 32
			    || addnum == 48)
			  {
			    reloctype
			      = (addnum == 0
				 ? BFD_RELOC_SH_IMM_LOW16
				 : (addnum == 16
				    ? BFD_RELOC_SH_IMM_MEDLOW16
				    : (addnum == 32
				       ? BFD_RELOC_SH_IMM_MEDHI16
				       : BFD_RELOC_SH_IMM_HI16)));

			    inner = inner_expr->X_add_symbol;
			    inner_expr = symbol_get_value_expression (inner);
			  }
		      }
		  }

		/* I'm not sure I understand the logic, but evidently the
		   inner expression of a lone symbol is O_constant, with
		   the actual symbol in expr_section.  For a constant, the
		   section would be absolute_section.  For sym+offset,
		   it's O_symbol as always.  See expr.c:make_expr_symbol,
		   first statements.  */

		if (inner_expr->X_op == O_constant
		    && S_GET_SEGMENT (inner) != absolute_section)
		  {
		    operands->operands[i].immediate.X_op = O_symbol;
		    operands->operands[i].immediate.X_add_symbol = inner;
		    operands->operands[i].immediate.X_add_number = 0;
		  }
		else
		  operands->operands[i].immediate
		    = *symbol_get_value_expression (inner);

		operands->operands[i].reloctype = reloctype;
	      }
	  }
	  /* Fall through.  */
	case A_IMMS6:
	case A_IMMS6BY32:
	case A_IMMS10:
	case A_IMMS10BY1:
	case A_IMMS10BY2:
	case A_IMMS10BY4:
	case A_IMMS10BY8:
	case A_PCIMMS16BY4:
	case A_PCIMMS16BY4_PT:
	case A_IMMU5:
	case A_IMMU6:
	  if (operands->operands[i].type != A_IMMM)
	    return NULL;

	  if (sh_check_fixup (&operands->operands[i].immediate,
			      &operands->operands[i].reloctype))
	    {
	      as_bad (_("invalid PIC reference"));
	      return NULL;
	    }

	  break;

	default:
	  BAD_CASE (info->arg[i]);
	}

      if (*ptr == ',' && info->arg[i + 1])
	ptr++;
    }
  return ptr;
}


/* Find an opcode at the start of *STR_P in the hash table, and set
   *STR_P to the first character after the last one read.  */

static shmedia_opcode_info *
shmedia_find_cooked_opcode (char **str_p)
{
  char *str = *str_p;
  char *op_start;
  char *op_end;
  char name[20];
  unsigned int nlen = 0;

  /* Drop leading whitespace.  */
  while (*str == ' ')
    str++;

  /* Find the op code end.  */
  for (op_start = op_end = str;
       *op_end
       && nlen < sizeof (name) - 1
       && ! is_end_of_line[(unsigned char) *op_end]
       && ! ISSPACE ((unsigned char) *op_end);
       op_end++)
    {
      unsigned char c = op_start[nlen];

      /* The machine independent code will convert CMP/EQ into cmp/EQ
	 because it thinks the '/' is the end of the symbol.  Moreover,
	 all but the first sub-insn is a parallel processing insn won't
	 be capitalized.  Instead of hacking up the machine independent
	 code, we just deal with it here.  */
      c = TOLOWER (c);
      name[nlen] = c;
      nlen++;
    }

  name[nlen] = 0;
  *str_p = op_end;

  if (nlen == 0)
    as_bad (_("can't find opcode"));

  return
    (shmedia_opcode_info *) hash_find (shmedia_opcode_hash_control, name);
}

/* Build up an instruction, including allocating the frag.  */

static int
shmedia_build_Mytes (shmedia_opcode_info *opcode,
		     shmedia_operands_info *operands)
{
  unsigned long insn = opcode->opcode_base;
  int i, j;
  char *insn_loc = frag_more (4);

  /* The parameter to dwarf2_emit_insn is actually the offset to the start
     of the insn from the fix piece of instruction that was emitted.
     Since we want .debug_line addresses to record (address | 1) for
     SHmedia insns, we get the wanted effect by taking one off the size,
     knowing it's a multiple of 4.  We count from the first fix piece of
     the insn.  There must be no frags changes (frag_more or frag_var)
     calls in-between the frag_more call we account for, and this
     dwarf2_emit_insn call.  */
  dwarf2_emit_insn (3);

  /* This is stored into any frag_var operand.  */
  sh64_last_insn_frag = frag_now;

  /* Loop over opcode info, emit an instruction.  */
  for (i = 0, j = 0; opcode->arg[i]; i++)
    {
      shmedia_arg_type argtype = opcode->arg[i];
      shmedia_operand_info *opjp = &operands->operands[j];
      switch (argtype)
	{
	case A_TREG_A:
	case A_TREG_B:
	case A_GREG_M:
	case A_GREG_N:
	case A_GREG_D:
	case A_FREG_G:
	case A_FREG_H:
	case A_FREG_F:
	case A_FVREG_G:
	case A_FVREG_H:
	case A_FVREG_F:
	case A_FMREG_G:
	case A_FMREG_H:
	case A_FMREG_F:
	case A_FPREG_G:
	case A_FPREG_H:
	case A_FPREG_F:
	case A_DREG_G:
	case A_DREG_H:
	case A_DREG_F:
	case A_CREG_J:
	case A_CREG_K:
	  /* Six-bit register fields.  They just get filled with the
	     parsed register number.  */
	  insn |= (opjp->reg << opcode->nibbles[i]);
	  j++;
	  break;

	case A_REUSE_PREV:
	  /* Copy the register for the previous operand to this position.  */
	  insn |= (operands->operands[j - 1].reg << opcode->nibbles[i]);
	  j++;
	  break;

	case A_IMMS6:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					BFD_RELOC_SH_IMMS6);
	  j++;
	  break;

	case A_IMMS6BY32:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					BFD_RELOC_SH_IMMS6BY32);
	  j++;
	  break;

	case A_IMMS10BY1:
	case A_IMMS10:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					BFD_RELOC_SH_IMMS10);
	  j++;
	  break;

	case A_IMMS10BY2:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					BFD_RELOC_SH_IMMS10BY2);
	  j++;
	  break;

	case A_IMMS10BY4:
	  if (opjp->reloctype == BFD_RELOC_NONE)
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  BFD_RELOC_SH_IMMS10BY4);
	  else if (opjp->reloctype == BFD_RELOC_SH_GOTPLT32)
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  BFD_RELOC_SH_GOTPLT10BY4);
	  else if (opjp->reloctype == BFD_RELOC_32_GOT_PCREL)
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  BFD_RELOC_SH_GOT10BY4);
	  else
	    as_bad (_("invalid PIC reference"));
	  j++;
	  break;

	case A_IMMS10BY8:
	  if (opjp->reloctype == BFD_RELOC_NONE)
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  BFD_RELOC_SH_IMMS10BY8);
	  else if (opjp->reloctype == BFD_RELOC_SH_GOTPLT32)
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  BFD_RELOC_SH_GOTPLT10BY8);
	  else if (opjp->reloctype == BFD_RELOC_32_GOT_PCREL)
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  BFD_RELOC_SH_GOT10BY8);
	  else
	    as_bad (_("invalid PIC reference"));
	  j++;
	  break;

	case A_IMMS16:
	  /* Sneak a peek if this is the MOVI insn.  If so, check if we
	     should expand it.  */
	  if (opjp->reloctype == BFD_RELOC_32_GOT_PCREL)
	    opjp->reloctype = BFD_RELOC_SH_GOT_LOW16;
	  else if (opjp->reloctype == BFD_RELOC_SH_GOTPLT32)
	    opjp->reloctype = BFD_RELOC_SH_GOTPLT_LOW16;

	  if ((opjp->reloctype == BFD_RELOC_NONE
	       || opjp->reloctype == BFD_RELOC_32_GOTOFF
	       || opjp->reloctype == BFD_RELOC_32_PLT_PCREL
	       || opjp->reloctype == BFD_RELOC_SH_GOTPC)
	      && opcode->opcode_base == SHMEDIA_MOVI_OPC
	      && (opjp->immediate.X_op != O_constant
		  || opjp->immediate.X_add_number < -32768
		  || opjp->immediate.X_add_number > 32767)
	      && (sh64_expand
		  || opjp->reloctype == BFD_RELOC_32_GOTOFF
		  || opjp->reloctype == BFD_RELOC_32_PLT_PCREL
		  || opjp->reloctype == BFD_RELOC_SH_GOTPC))
	    {
	      int what = sh64_abi == sh64_abi_64 ? MOVI_IMM_64 : MOVI_IMM_32;
	      offsetT max = sh64_abi == sh64_abi_64 ? MOVI_64 : MOVI_32;
	      offsetT min = MOVI_16;
	      offsetT init = UNDEF_MOVI;
	      valueT addvalue
		= opjp->immediate.X_op_symbol != NULL
		? 0 : opjp->immediate.X_add_number;
	      symbolS *sym
		= opjp->immediate.X_op_symbol != NULL
		? make_expr_symbol (&opjp->immediate)
		: opjp->immediate.X_add_symbol;

	      if (opjp->reloctype == BFD_RELOC_32_GOTOFF)
		init = max = min = MOVI_GOTOFF;
	      else if (opjp->reloctype == BFD_RELOC_32_PLT_PCREL)
		{
		  init = max = min = MOVI_PLT;
		  what = (sh64_abi == sh64_abi_64
			  ? MOVI_IMM_64_PCREL
			  : MOVI_IMM_32_PCREL);
		}
	      else if (opjp->reloctype == BFD_RELOC_SH_GOTPC)
		{
		  init = max = min = MOVI_GOTPC;
		  what = (sh64_abi == sh64_abi_64
			  ? MOVI_IMM_64_PCREL
			  : MOVI_IMM_32_PCREL);
		}

	      frag_var (rs_machine_dependent,
			md_relax_table[C (what, max)].rlx_length,
			md_relax_table[C (what, min)].rlx_length,
			C (what, init), sym, addvalue, insn_loc);
	    }
	  else
	    insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					  (opjp->reloctype
					   == BFD_RELOC_NONE)
					  ? BFD_RELOC_SH_IMMS16
					  : opjp->reloctype);
	  j++;
	  break;

	case A_PCIMMS16BY4:
	  {
	    int what
	      = ((sh64_abi == sh64_abi_64 && ! sh64_pt32)
		 ? SH64PCREL16_64 : SH64PCREL16_32);
	    offsetT max
	      = ((sh64_abi == sh64_abi_64 && ! sh64_pt32)
		 ? SH64PCREL64 : SH64PCREL32);
	    offsetT min = SH64PCREL16;
	    offsetT init = UNDEF_SH64PCREL;

	    /* Don't allow complex expressions here.  */
	    if (opjp->immediate.X_op_symbol != NULL)
	      return 0;

	    if (opjp->reloctype == BFD_RELOC_32_PLT_PCREL)
	      init = max = min = SH64PCRELPLT;

	    /* If we're not expanding, then just emit a fixup.  */
	    if (sh64_expand || opjp->reloctype != BFD_RELOC_NONE)
	      frag_var (rs_machine_dependent,
			md_relax_table[C (what, max)].rlx_length,
			md_relax_table[C (what, min)].rlx_length,
			C (what, init),
			opjp->immediate.X_add_symbol,
			opjp->immediate.X_add_number,
			insn_loc);
	    else
	      insn |= shmedia_immediate_op (insn_loc, opjp, 1,
					    opjp->reloctype == BFD_RELOC_NONE
					    ? BFD_RELOC_SH_PT_16
					    : opjp->reloctype);

	    j++;
	    break;
	  }

	case A_PCIMMS16BY4_PT:
	  {
	    int what
	      = ((sh64_abi == sh64_abi_64 && ! sh64_pt32)
		 ? SH64PCREL16PT_64 : SH64PCREL16PT_32);
	    offsetT max
	      = ((sh64_abi == sh64_abi_64 && ! sh64_pt32)
		 ? SH64PCREL64 : SH64PCREL32);
	    offsetT min = SH64PCREL16;
	    offsetT init = UNDEF_SH64PCREL;

	    /* Don't allow complex expressions here.  */
	    if (opjp->immediate.X_op_symbol != NULL)
	      return 0;

	    if (opjp->reloctype == BFD_RELOC_32_PLT_PCREL)
	      init = max = min = SH64PCRELPLT;

	    /* If we're not expanding, then just emit a fixup.  */
	    if (sh64_expand || opjp->reloctype != BFD_RELOC_NONE)
	      frag_var (rs_machine_dependent,
			md_relax_table[C (what, max)].rlx_length,
			md_relax_table[C (what, min)].rlx_length,
			C (what, init),
			opjp->immediate.X_add_symbol,
			opjp->immediate.X_add_number,
			insn_loc);
	    else
	      /* This reloc-type is just temporary, so we can distinguish
		 PTA from PT.  It is changed in shmedia_md_apply_fix3 to
		 BFD_RELOC_SH_PT_16.  */
	      insn |= shmedia_immediate_op (insn_loc, opjp, 1,
					    opjp->reloctype == BFD_RELOC_NONE
					    ? SHMEDIA_BFD_RELOC_PT
					    : opjp->reloctype);

	    j++;
	    break;
	  }

	case A_IMMU5:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					BFD_RELOC_SH_IMMU5);
	  j++;
	  break;

	case A_IMMU6:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					BFD_RELOC_SH_IMMU6);
	  j++;
	  break;

	case A_IMMU16:
	  insn |= shmedia_immediate_op (insn_loc, opjp, 0,
					(opjp->reloctype
					 == BFD_RELOC_NONE)
					? BFD_RELOC_SH_IMMU16
					: opjp->reloctype);
	  j++;
	  break;

	default:
	  BAD_CASE (argtype);
	}
    }

  md_number_to_chars (insn_loc, insn, 4);
  return 4;
}

/* Assemble a SHmedia instruction.  */

static void
shmedia_md_assemble (char *str)
{
  char *op_end;
  shmedia_opcode_info *opcode;
  shmedia_operands_info operands;
  int size;

  opcode = shmedia_find_cooked_opcode (&str);
  op_end = str;

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode"));
      return;
    }

  /* Start a SHmedia code region, if there has been pseudoinsns or similar
     seen since the last one.  */
  if (!seen_insn)
    {
      sh64_update_contents_mark (TRUE);
      sh64_set_contents_type (CRT_SH5_ISA32);
      seen_insn = TRUE;
    }

  op_end = shmedia_get_operands (opcode, op_end, &operands);

  if (op_end == NULL)
    {
      as_bad (_("invalid operands to %s"), opcode->name);
      return;
    }

  if (*op_end)
    {
      as_bad (_("excess operands to %s"), opcode->name);
      return;
    }

  size = shmedia_build_Mytes (opcode, &operands);
  if (size == 0)
    return;
}

/* Hook called from md_begin in tc-sh.c.  */

void
shmedia_md_begin (void)
{
  const shmedia_opcode_info *shmedia_opcode;
  shmedia_opcode_hash_control = hash_new ();

  /* Create opcode table for SHmedia mnemonics.  */
  for (shmedia_opcode = shmedia_table;
       shmedia_opcode->name;
       shmedia_opcode++)
    hash_insert (shmedia_opcode_hash_control, shmedia_opcode->name,
		 (char *) shmedia_opcode);
}

/* Switch instruction set.  Only valid if one of the --isa or --abi
   options was specified.  */

static void
s_sh64_mode (int ignore ATTRIBUTE_UNUSED)
{
  char *name = input_line_pointer, ch;

  /* Make sure data up to this location is handled according to the
     previous ISA.  */
  sh64_update_contents_mark (TRUE);

  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    input_line_pointer++;
  ch = *input_line_pointer;
  *input_line_pointer = '\0';

  /* If the mode was not set before, explicitly or implicitly, then we're
     not emitting SH64 code, so this pseudo is invalid.  */
  if (sh64_isa_mode == sh64_isa_unspecified)
    as_bad (_("The `.mode %s' directive is not valid with this architecture"),
	    name);

  if (strcasecmp (name, "shcompact") == 0)
    sh64_isa_mode = sh64_isa_shcompact;
  else if (strcasecmp (name, "shmedia") == 0)
    sh64_isa_mode = sh64_isa_shmedia;
  else
    as_bad (_("Invalid argument to .mode: %s"), name);

  /* Make a new frag, marking it with the supposedly-changed ISA.  */
  frag_wane (frag_now);
  frag_new (0);

  /* Contents type up to this new point is the same as before; don't add a
     data region just because the new frag we created.  */
  sh64_update_contents_mark (FALSE);

  *input_line_pointer = ch;
  demand_empty_rest_of_line ();
}

/* Check that the right ABI is used.  Only valid if one of the --isa or
   --abi options was specified.  */

static void
s_sh64_abi (int ignore ATTRIBUTE_UNUSED)
{
  char *name = input_line_pointer, ch;

  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    input_line_pointer++;
  ch = *input_line_pointer;
  *input_line_pointer = '\0';

  /* If the mode was not set before, explicitly or implicitly, then we're
     not emitting SH64 code, so this pseudo is invalid.  */
  if (sh64_abi == sh64_abi_unspecified)
    as_bad (_("The `.abi %s' directive is not valid with this architecture"),
	    name);

  if (strcmp (name, "64") == 0)
    {
      if (sh64_abi != sh64_abi_64)
	as_bad (_("`.abi 64' but command-line options do not specify 64-bit ABI"));
    }
  else if (strcmp (name, "32") == 0)
    {
      if (sh64_abi != sh64_abi_32)
	as_bad (_("`.abi 32' but command-line options do not specify 32-bit ABI"));
    }
  else
    as_bad (_("Invalid argument to .abi: %s"), name);

  *input_line_pointer = ch;
  demand_empty_rest_of_line ();
}

/* This function is the first target-specific function called after
   parsing command-line options.  Therefore we set default values from
   command-line options here and do some sanity checking we couldn't do
   when options were being parsed.  */

const char *
sh64_target_format (void)
{
#ifdef TE_NetBSD
  /* For NetBSD, if the ISA is unspecified, always use SHmedia.  */
  if (sh64_isa_mode == sh64_isa_unspecified)
    sh64_isa_mode = sh64_isa_shmedia;

  /* If the ABI is unspecified, select a default: based on how
     we were configured: sh64 == sh64_abi_64, else sh64_abi_32.  */
  if (sh64_abi == sh64_abi_unspecified)
    {
      if (sh64_isa_mode == sh64_isa_shcompact)
	sh64_abi = sh64_abi_32;
      else if (strncmp (TARGET_CPU, "sh64", 4) == 0)
        sh64_abi = sh64_abi_64;
      else
        sh64_abi = sh64_abi_32;
    }
#endif

#ifdef TE_LINUX
  if (sh64_isa_mode == sh64_isa_unspecified)
    sh64_isa_mode = sh64_isa_shmedia;

  if (sh64_abi == sh64_abi_unspecified)
    sh64_abi = sh64_abi_32;
#endif

  if (sh64_abi == sh64_abi_64 && sh64_isa_mode == sh64_isa_unspecified)
    sh64_isa_mode = sh64_isa_shmedia;

  if (sh64_abi == sh64_abi_32 && sh64_isa_mode == sh64_isa_unspecified)
    sh64_isa_mode = sh64_isa_shcompact;

  if (sh64_isa_mode == sh64_isa_shcompact
      && sh64_abi == sh64_abi_unspecified)
    sh64_abi = sh64_abi_32;

  if (sh64_isa_mode == sh64_isa_shmedia
      && sh64_abi == sh64_abi_unspecified)
    sh64_abi = sh64_abi_64;

  if (sh64_isa_mode == sh64_isa_unspecified && ! sh64_mix)
    as_bad (_("-no-mix is invalid without specifying SHcompact or SHmedia"));

  if ((sh64_isa_mode == sh64_isa_unspecified
       || sh64_isa_mode == sh64_isa_shmedia)
      && sh64_shcompact_const_crange)
    as_bad (_("-shcompact-const-crange is invalid without SHcompact"));

  if (sh64_pt32 && sh64_abi != sh64_abi_64)
    as_bad (_("-expand-pt32 only valid with -abi=64"));

  if (! sh64_expand && sh64_isa_mode == sh64_isa_unspecified)
    as_bad (_("-no-expand only valid with SHcompact or SHmedia"));

  if (sh64_pt32 && ! sh64_expand)
    as_bad (_("-expand-pt32 invalid together with -no-expand"));

#ifdef TE_NetBSD
  if (sh64_abi == sh64_abi_64)
    return (target_big_endian ? "elf64-sh64-nbsd" : "elf64-sh64l-nbsd");
  else
    return (target_big_endian ? "elf32-sh64-nbsd" : "elf32-sh64l-nbsd");
#elif defined (TE_LINUX)
  if (sh64_abi == sh64_abi_64)
    return (target_big_endian ? "elf64-sh64big-linux" : "elf64-sh64-linux");
  else
    return (target_big_endian ? "elf32-sh64big-linux" : "elf32-sh64-linux");
#else
  /* When the ISA is not one of SHmedia or SHcompact, use the old SH
     object format.  */
  if (sh64_isa_mode == sh64_isa_unspecified)
    return (target_big_endian ? "elf32-sh" : "elf32-shl");
  else if (sh64_abi == sh64_abi_64)
    return (target_big_endian ? "elf64-sh64" : "elf64-sh64l");
  else
    return (target_big_endian ? "elf32-sh64" : "elf32-sh64l");
#endif
}

/* The worker function of TARGET_MACH.  */

int
sh64_target_mach (void)
{
  /* We need to explicitly set bfd_mach_sh5 instead of the default 0.  But
     we only do this for the 64-bit ABI: if we do it for the 32-bit ABI,
     the SH5 info in the bfd_arch_info structure will be selected.
     However correct, as the machine has 64-bit addresses, functions
     expected to emit 32-bit data for addresses will start failing.  For
     example, the dwarf2dbg.c functions will emit 64-bit debugging format,
     and we don't want that in the 32-bit ABI.

     We could have two bfd_arch_info structures for SH64; one for the
     32-bit ABI and one for the rest (64-bit ABI).  But that would be a
     bigger kludge: it's a flaw in the BFD design, and we need to just
     work around it by having the default machine set here in the
     assembler.  For everything else but the assembler, the various bfd
     functions will set the machine type right to bfd_mach_sh5 from object
     file header flags regardless of the 0 here.  */

  return (sh64_abi == sh64_abi_64) ? bfd_mach_sh5 : 0;
}

/* This is MD_PCREL_FROM_SECTION, we we define so it is called instead of
   md_pcrel_from (in tc-sh.c).  */

valueT
shmedia_md_pcrel_from_section (struct fix *fixP, segT sec ATTRIBUTE_UNUSED)
{
  know (fixP->fx_frag->fr_type == rs_machine_dependent);

  /* Use the ISA for the instruction to decide which offset to use.  We
     can glean it from the fisup type.  */
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_SH_IMM_LOW16:
    case BFD_RELOC_SH_IMM_MEDLOW16:
    case BFD_RELOC_SH_IMM_MEDHI16:
    case BFD_RELOC_SH_IMM_HI16:
    case BFD_RELOC_SH_IMM_LOW16_PCREL:
    case BFD_RELOC_SH_IMM_MEDLOW16_PCREL:
    case BFD_RELOC_SH_IMM_MEDHI16_PCREL:
    case BFD_RELOC_SH_IMM_HI16_PCREL:
    case BFD_RELOC_SH_IMMU5:
    case BFD_RELOC_SH_IMMU6:
    case BFD_RELOC_SH_IMMS6:
    case BFD_RELOC_SH_IMMS10:
    case BFD_RELOC_SH_IMMS10BY2:
    case BFD_RELOC_SH_IMMS10BY4:
    case BFD_RELOC_SH_IMMS10BY8:
    case BFD_RELOC_SH_IMMS16:
    case BFD_RELOC_SH_IMMU16:
    case BFD_RELOC_SH_PT_16:
    case SHMEDIA_BFD_RELOC_PT:
      /* PC-relative relocs are relative to the address of the last generated
	 instruction, i.e. fx_size - 4.  */
      return SHMEDIA_MD_PCREL_FROM_FIX (fixP);

    case BFD_RELOC_64:
    case BFD_RELOC_64_PCREL:
      know (0 /* Shouldn't get here.  */);
      break;

    default:
      /* If section was SHcompact, use its function.  */
      return (valueT) md_pcrel_from_section (fixP, sec);
    }

  know (0 /* Shouldn't get here.  */);
  return 0;
}

/* Create one .cranges descriptor from two symbols, STARTSYM marking begin
   and ENDSYM marking end, and CR_TYPE specifying the type.  */

static void
sh64_emit_crange (symbolS *startsym, symbolS *endsym,
		  enum sh64_elf_cr_type cr_type)
{
  expressionS exp;
  segT current_seg = now_seg;
  subsegT current_subseg = now_subseg;

  asection *cranges
    = bfd_make_section_old_way (stdoutput,
				SH64_CRANGES_SECTION_NAME);

  /* Temporarily change to the .cranges section.  */
  subseg_set (cranges, 0);

  /* Emit the cr_addr part.  */
  exp.X_op = O_symbol;
  exp.X_add_number = 0;
  exp.X_op_symbol = NULL;
  exp.X_add_symbol = startsym;
  emit_expr (&exp, 4);

  /* Emit the cr_size part.  */
  exp.X_op = O_subtract;
  exp.X_add_number = 0;
  exp.X_add_symbol = endsym;
  exp.X_op_symbol = startsym;
  emit_expr (&exp, 4);

  /* Emit the cr_size part.  */
  exp.X_op = O_constant;
  exp.X_add_number = cr_type;
  exp.X_add_symbol = NULL;
  exp.X_op_symbol = NULL;
  emit_expr (&exp, 2);

  /* Now back to our regular program.  */
  subseg_set (current_seg, current_subseg);
}

/* Called when the assembler is about to emit contents of some type into
   SEG, so it is *known* that the type of that new contents is in
   NEW_CONTENTS_TYPE.  If just switching back and forth between different
   contents types (for example, with consecutive .mode pseudos), then this
   function isn't called.  */

static void
sh64_set_contents_type (enum sh64_elf_cr_type new_contents_type)
{
  segment_info_type *seginfo;

  /* We will not be called when emitting .cranges output, since callers
     stop that.  Validize that assumption.  */
  know (!emitting_crange);

  seginfo = seg_info (now_seg);

  if (seginfo)
    {
      symbolS *symp = seginfo->tc_segment_info_data.last_contents_mark;

      enum sh64_elf_cr_type contents_type
	= seginfo->tc_segment_info_data.contents_type;

      /* If it was just SHcompact switching between code and constant
	 pool, don't change contents type.  Just make sure we don't set
	 the contents type to data, as that would join with a data-region
	 in SHmedia mode.  */
      if (sh64_isa_mode == sh64_isa_shcompact
	  && ! sh64_shcompact_const_crange)
	new_contents_type = CRT_SH5_ISA16;

      /* If nothing changed, stop here.  */
      if (contents_type == new_contents_type)
	return;

      /* If we're in 64-bit ABI mode, we do not emit .cranges, as it is
	 only specified for 32-bit addresses.  It could presumably be
	 extended, but in 64-bit ABI mode we don't have SHcompact code, so
	 we would only use it to mark code and data.  */
      if (sh64_abi == sh64_abi_64)
	{
	  /* Make the code type "sticky".  We don't want to set the
	     sections contents type to data if there's any code in it as
	     we don't have .cranges in 64-bit mode to notice the
	     difference.  */
	  seginfo->tc_segment_info_data.contents_type
	    = (new_contents_type == CRT_SH5_ISA32
	       || contents_type == CRT_SH5_ISA32)
	    ? CRT_SH5_ISA32 : new_contents_type;
	  return;
	}

      /* If none was marked, create a start symbol for this range and
	 perhaps as a closing symbol for the old one.  */
      if (symp == NULL)
	symp = symbol_new (FAKE_LABEL_NAME, now_seg, (valueT) frag_now_fix (),
			   frag_now);

      /* We will use this symbol, so don't leave a pointer behind.  */
      seginfo->tc_segment_info_data.last_contents_mark = NULL;

      /* We'll be making only datalabel references to it, if we emit a
	 .cranges descriptor, so remove any code flag.  */
      S_SET_OTHER (symp, S_GET_OTHER (symp) & ~STO_SH5_ISA32);

      /* If we have already marked the start of a range, we need to close
	 and emit it before marking a new one, so emit a new .cranges
	 descriptor into the .cranges section.  */
      if (seginfo->tc_segment_info_data.mode_start_symbol)
	{
	  /* If we're not supposed to emit mixed-mode sections, make it an
	     error, but continue processing.  */
	  if (! sh64_mix
	      && (new_contents_type == CRT_SH5_ISA32
		  || contents_type == CRT_SH5_ISA32))
	    as_bad (
_("SHmedia code not allowed in same section as constants and SHcompact code"));

	  emitting_crange = TRUE;
	  sh64_emit_crange (seginfo->tc_segment_info_data.mode_start_symbol,
			    symp, contents_type);
	  emitting_crange = FALSE;
	  seginfo->tc_segment_info_data.emitted_ranges++;
	}

      seginfo->tc_segment_info_data.mode_start_symbol = symp;
      seginfo->tc_segment_info_data.mode_start_subseg = now_subseg;
      seginfo->tc_segment_info_data.contents_type = new_contents_type;

      /* Always reset this, so the SHcompact code will emit a reloc when
	 it prepares to relax.  */
      seginfo->tc_segment_info_data.in_code = 0;
    }
  else
    as_bad (_("No segment info for current section"));
}

/* Hook when defining symbols and labels.  We set the ST_OTHER field if
   the symbol is "shmedia" (with "bitor 1" automatically applied).  Simple
   semantics for a label being "shmedia" : It was defined when .mode
   SHmedia was in effect, and it was defined in a code section.  It
   doesn't matter whether or not an assembled opcode is nearby.  */

void
sh64_frob_label (symbolS *symp)
{
  segT seg = S_GET_SEGMENT (symp);
  static const symbolS *null = NULL;

  /* Reset the tc marker for all newly created symbols.  */
  symbol_set_tc (symp, (symbolS **) &null);

  if (seg != NULL && sh64_isa_mode == sh64_isa_shmedia && subseg_text_p (seg))
    S_SET_OTHER (symp, S_GET_OTHER (symp) | STO_SH5_ISA32);
}

/* Handle the "datalabel" qualifier.  We need to call "operand", but it's
   static, so a function pointer is passed here instead.  FIXME: A target
   hook for qualifiers is needed; we currently use the md_parse_name
   symbol hook.  */

int
sh64_consume_datalabel (const char *name, expressionS *exp, char *cp,
			segT (*operandf) (expressionS *))
{
  static int parsing_datalabel = 0;

  if (strcasecmp (name, "datalabel") == 0)
    {
      int save_parsing_datalabel = parsing_datalabel;

      if (parsing_datalabel)
	as_bad (_("duplicate datalabel operator ignored"));

      *input_line_pointer = *cp;
      parsing_datalabel = 1;
      (*operandf) (exp);
      parsing_datalabel = save_parsing_datalabel;

      if (exp->X_op == O_symbol || exp->X_op == O_PIC_reloc)
	{
	  symbolS *symp = exp->X_add_symbol;
	  segT symseg = S_GET_SEGMENT (symp);

	  /* If the symbol is defined to something that is already a
	     datalabel, we don't need to bother with any special handling.  */
	  if (symseg != undefined_section
	      && S_GET_OTHER (symp) != STO_SH5_ISA32)
	    /* Do nothing.  */
	    ;
	  else
	    {
	      symbolS *dl_symp;
	      const char *name = S_GET_NAME (symp);
	      char *dl_name
		= xmalloc (strlen (name) + sizeof (DATALABEL_SUFFIX));

	      /* Now we copy the datalabel-qualified symbol into a symbol
		 with the same name, but with " DL" appended.  We mark the
		 symbol using the TC_SYMFIELD_TYPE field with a pointer to
		 the main symbol, so we don't have to inspect all symbol
		 names.  Note that use of "datalabel" is not expected to
		 be a common case.  */
	      strcpy (dl_name, name);
	      strcat (dl_name, DATALABEL_SUFFIX);

	      /* A FAKE_LABEL_NAME marks "$" or ".".  There can be any
		 number of them and all have the same (faked) name; we
		 must make a new one each time.  */
	      if (strcmp (name, FAKE_LABEL_NAME) == 0)
		dl_symp = symbol_make (dl_name);
	      else
		dl_symp = symbol_find_or_make (dl_name);

	      free (dl_name);
	      symbol_set_value_expression (dl_symp,
					   symbol_get_value_expression (symp));
	      S_SET_SEGMENT (dl_symp, symseg);
	      symbol_set_frag (dl_symp, symbol_get_frag (symp));
	      symbol_set_tc (dl_symp, &symp);
	      copy_symbol_attributes (dl_symp, symp);
	      exp->X_add_symbol = dl_symp;

	      /* Unset the BranchTarget mark that can be set at symbol
		 creation or attributes copying.  */
	      S_SET_OTHER (dl_symp, S_GET_OTHER (dl_symp) & ~STO_SH5_ISA32);

	      /* The GLOBAL and WEAK attributes are not copied over by
		 copy_symbol_attributes.  Do it here.  */
	      if (S_IS_WEAK (symp))
		S_SET_WEAK (dl_symp);
	      else if (S_IS_EXTERNAL (symp))
		S_SET_EXTERNAL (dl_symp);
	    }
	}
      /* Complain about other types of operands than symbol, unless they
	 have already been complained about.  A constant is always a
	 datalabel.  Removing the low bit would therefore be wrong.
	 Complaining about it would also be wrong.  */
      else if (exp->X_op != O_illegal
	       && exp->X_op != O_absent
	       && exp->X_op != O_constant)
	as_bad (_("Invalid DataLabel expression"));

      *cp = *input_line_pointer;

      return 1;
    }

  return sh_parse_name (name, exp, cp);
}

/* This function is called just before symbols are being output.  It
   returns zero when a symbol must be output, non-zero otherwise.
   Datalabel references that were fully resolved to local symbols are not
   necessary to output.  We also do not want to output undefined symbols
   that are not used in relocs.  For symbols that are used in a reloc, it
   does not matter what we set here.  If it is *not* used in a reloc, then
   it was probably the datalabel counterpart that was used in a reloc;
   then we need not output the main symbol.  */

int
sh64_exclude_symbol (symbolS *symp)
{
  symbolS *main_symbol = *symbol_get_tc (symp);

  return main_symbol != NULL || ! S_IS_DEFINED (symp);
}

/* If we haven't seen an insn since the last update, and location
   indicators have moved (a new frag, new location within frag) we have
   emitted data, so change contents type to data.  Forget that we have
   seen a sequence of insns and store the current location so we can mark
   a new region if needed.  */

static void
sh64_update_contents_mark (bfd_boolean update_type)
{
  segment_info_type *seginfo;
  seginfo = seg_info (now_seg);

  if (seginfo != NULL)
    {
      symbolS *symp = seginfo->tc_segment_info_data.last_contents_mark;

      if (symp == NULL)
	{
	  symp = symbol_new (FAKE_LABEL_NAME, now_seg,
			     (valueT) frag_now_fix (), frag_now);
	  seginfo->tc_segment_info_data.last_contents_mark = symp;
	}
      else
	{
	  /* If we have moved location since last flush, we need to emit a
	     data range.  The previous contents type ended at the location
	     of the last update.  */
	  if ((S_GET_VALUE (symp) != frag_now_fix ()
	       || symbol_get_frag (symp) != frag_now))
	    {
	      enum sh64_elf_cr_type contents_type
		= seginfo->tc_segment_info_data.contents_type;

	      if (update_type
		  && contents_type != CRT_DATA
		  && contents_type != CRT_NONE
		  && ! seen_insn)
		{
		  sh64_set_contents_type (CRT_DATA);
		  symp = seginfo->tc_segment_info_data.last_contents_mark;
		}

	      /* If the symbol wasn't used up to make up a new range
		 descriptor, update it to this new location.  */
	      if (symp)
		{
		  S_SET_VALUE (symp, (valueT) frag_now_fix ());
		  symbol_set_frag (symp, frag_now);
		}
	    }
	}
    }

  seen_insn = FALSE;
}

/* Called when the assembler is about to output some data, or maybe it's
   just switching segments.  */

void
sh64_flush_pending_output (void)
{
  sh64_update_contents_mark (TRUE);
  sh_flush_pending_output ();
}

/* Flush out the last crange descriptor after all insns have been emitted.  */

static void
sh64_flush_last_crange (bfd *abfd ATTRIBUTE_UNUSED, asection *seg,
			void *countparg ATTRIBUTE_UNUSED)
{
  segment_info_type *seginfo;

  seginfo = seg_info (seg);

  if (seginfo
      /* Only emit .cranges descriptors if we would make it more than one.  */
      && seginfo->tc_segment_info_data.emitted_ranges != 0)
    {
      symbolS *symp;

      /* We need a closing symbol, so switch to the indicated section and
	 emit it.  */

      /* Change to the section we're about to handle.  */
      subseg_set (seg, seginfo->tc_segment_info_data.mode_start_subseg);

      symp = symbol_new (FAKE_LABEL_NAME, now_seg, (valueT) frag_now_fix (),
			 frag_now);

      /* We'll be making a datalabel reference to it, so remove any code
         flag.  */
      S_SET_OTHER (symp, S_GET_OTHER (symp) & ~STO_SH5_ISA32);

      sh64_emit_crange (seginfo->tc_segment_info_data.mode_start_symbol,
			symp,
			seginfo->tc_segment_info_data.contents_type);
    }
}

/* If and only if we see a call to md_number_to_chars without flagging the
   start of an insn, we set the contents type to CRT_DATA, and only when
   in SHmedia mode.  Note that by default we don't bother changing when
   going from SHcompact to data, as the constant pools in GCC-generated
   SHcompact code would create an inordinate amount of .cranges
   descriptors.  */

static void
sh64_flag_output (void)
{
  if (sh64_isa_mode != sh64_isa_unspecified
      && !seen_insn
      && !sh64_end_of_assembly
      && !emitting_crange)
    {
      md_flush_pending_output ();
      sh64_set_contents_type (CRT_DATA);
    }
}

/* Vtables don't need "datalabel" but we allow it by simply deleting
   any we find.  */

static char *
strip_datalabels (void)
{
  char *src, *dest, *start=input_line_pointer;

  for (src=input_line_pointer, dest=input_line_pointer; *src != '\n'; )
    {
      if (strncasecmp (src, "datalabel", 9) == 0
	  && ISSPACE (src[9])
	  && (src == start || !(ISALNUM (src[-1])) || src[-1] == '_'))
	src += 10;
      else
	*dest++ = *src++;
    }

  if (dest < src)
    *dest = '\n';
  return src + 1;
}

static void
sh64_vtable_entry (int ignore ATTRIBUTE_UNUSED)
{
  char *eol = strip_datalabels ();

  obj_elf_vtable_entry (0);
  input_line_pointer = eol;
}

static void
sh64_vtable_inherit (int ignore ATTRIBUTE_UNUSED)
{
  char *eol = strip_datalabels ();

  obj_elf_vtable_inherit (0);
  input_line_pointer = eol;
}

