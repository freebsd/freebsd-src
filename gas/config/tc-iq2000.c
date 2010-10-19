/* tc-iq2000.c -- Assembler for the Sitera IQ2000.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation.

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

#include <stdio.h>
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "symcat.h"
#include "opcodes/iq2000-desc.h"
#include "opcodes/iq2000-opc.h"
#include "cgen.h"
#include "elf/common.h"
#include "elf/iq2000.h"
#include "libbfd.h"
#include "hash.h"
#include "macro.h"

/* Structure to hold all of the different components describing
   an individual instruction.  */
typedef struct
{
  const CGEN_INSN *	insn;
  const CGEN_INSN *	orig_insn;
  CGEN_FIELDS		fields;
#if CGEN_INT_INSN_P
  CGEN_INSN_INT         buffer [1];
#define INSN_VALUE(buf) (*(buf))
#else
  unsigned char         buffer [CGEN_MAX_INSN_SIZE];
#define INSN_VALUE(buf) (buf)
#endif
  char *		addr;
  fragS *		frag;
  int                   num_fixups;
  fixS *                fixups [GAS_CGEN_MAX_FIXUPS];
  int                   indices [MAX_OPERAND_INSTANCES];
}
iq2000_insn;

const char comment_chars[]        = "#";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = ";";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

/* Default machine.  */
#define DEFAULT_MACHINE bfd_mach_iq2000
#define DEFAULT_FLAGS	EF_IQ2000_CPU_IQ2000

static unsigned long iq2000_mach = bfd_mach_iq2000;
static int cpu_mach = (1 << MACH_IQ2000);

/* Flags to set in the elf header.  */
static flagword iq2000_flags = DEFAULT_FLAGS;

typedef struct proc
{
  symbolS *isym;
  unsigned long reg_mask;
  unsigned long reg_offset;
  unsigned long fpreg_mask;
  unsigned long fpreg_offset;
  unsigned long frame_offset;
  unsigned long frame_reg;
  unsigned long pc_reg;
} procS;

static procS cur_proc;
static procS *cur_proc_ptr;
static int numprocs;

/* Relocations against symbols are done in two
   parts, with a HI relocation and a LO relocation.  Each relocation
   has only 16 bits of space to store an addend.  This means that in
   order for the linker to handle carries correctly, it must be able
   to locate both the HI and the LO relocation.  This means that the
   relocations must appear in order in the relocation table.

   In order to implement this, we keep track of each unmatched HI
   relocation.  We then sort them so that they immediately precede the
   corresponding LO relocation.  */

struct iq2000_hi_fixup
{
  struct iq2000_hi_fixup * next;  /* Next HI fixup.  */
  fixS *                  fixp;   /* This fixup.  */
  segT                    seg;    /* The section this fixup is in.  */
};

/* The list of unmatched HI relocs.  */
static struct iq2000_hi_fixup * iq2000_hi_fixup_list;

/* Macro hash table, which we will add to.  */
extern struct hash_control *macro_hash;

const char *md_shortopts = "";
struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int c ATTRIBUTE_UNUSED,
		 char * arg ATTRIBUTE_UNUSED)
{
  return 0;
}

void
md_show_usage (FILE * stream ATTRIBUTE_UNUSED)
{
}

/* Automatically enter conditional branch macros.  */

typedef struct
{
  const char * mnemonic;
  const char ** expansion;
  const char ** args;
} iq2000_macro_defs_s;

static const char * abs_args[] = { "rd", "rs", "scratch=%1", NULL };
static const char * abs_expn   = "\n sra \\rd,\\rs,31\n xor \\scratch,\\rd,\\rs\n sub \\rd,\\scratch,\\rd\n";
static const char * la_expn    = "\n lui \\reg,%hi(\\label)\n ori \\reg,\\reg,%lo(\\label)\n";
static const char * la_args[]  = { "reg", "label", NULL };
static const char * bxx_args[] = { "rs", "rt", "label", "scratch=%1", NULL };
static const char * bge_expn   = "\n slt \\scratch,\\rs,\\rt\n beq %0,\\scratch,\\label\n";
static const char * bgeu_expn  = "\n sltu \\scratch,\\rs,\\rt\n beq %0,\\scratch,\\label\n";
static const char * bgt_expn   = "\n slt \\scratch,\\rt,\\rs\n bne %0,\\scratch,\\label\n";
static const char * bgtu_expn  = "\n sltu \\scratch,\\rt,\\rs\n bne %0,\\scratch,\\label\n";
static const char * ble_expn   = "\n slt \\scratch,\\rt,\\rs\n beq %0,\\scratch,\\label\n";
static const char * bleu_expn  = "\n sltu \\scratch,\\rt,\\rs\n beq %0,\\scratch,\\label\n";
static const char * blt_expn   = "\n slt \\scratch,\\rs,\\rt\n bne %0,\\scratch,\\label\n";
static const char * bltu_expn  = "\n sltu \\scratch,\\rs,\\rt\n bne %0,\\scratch,\\label\n";
static const char * sxx_args[] = { "rd", "rs", "rt", NULL };
static const char * sge_expn   = "\n slt \\rd,\\rs,\\rt\n xori \\rd,\\rd,1\n";
static const char * sgeu_expn  = "\n sltu \\rd,\\rs,\\rt\n xori \\rd,\\rd,1\n";
static const char * sle_expn   = "\n slt \\rd,\\rt,\\rs\n xori \\rd,\\rd,1\n";
static const char * sleu_expn  = "\n sltu \\rd,\\rt,\\rs\n xori \\rd,\\rd,1\n";
static const char * sgt_expn   = "\n slt \\rd,\\rt,\\rs\n";
static const char * sgtu_expn  = "\n sltu \\rd,\\rt,\\rs\n";
static const char * sne_expn   = "\n xor \\rd,\\rt,\\rs\n sltu \\rd,%0,\\rd\n";
static const char * seq_expn   = "\n xor \\rd,\\rt,\\rs\n sltu \\rd,%0,\\rd\n xori \\rd,\\rd,1\n";
static const char * ai32_args[] = { "rt", "rs", "imm", NULL };
static const char * andi32_expn = "\n\
 .if (\\imm & 0xffff0000 == 0xffff0000)\n\
 andoi \\rt,\\rs,%lo(\\imm)\n\
 .elseif (\\imm & 0x0000ffff == 0x0000ffff)\n\
 andoui \\rt,\\rs,%uhi(\\imm)\n\
 .elseif (\\imm & 0xffff0000 == 0x00000000)\n\
 andi \\rt,\\rs,%lo(\\imm)\n\
 .else\n\
 andoui \\rt,\\rs,%uhi(\\imm)\n\
 andoi \\rt,\\rt,%lo(\\imm)\n\
 .endif\n";
static const char * ori32_expn  = "\n\
 .if (\\imm & 0xffff == 0)\n\
 orui \\rt,\\rs,%uhi(\\imm)\n\
 .elseif (\\imm & 0xffff0000 == 0)\n\
 ori \\rt,\\rs,%lo(\\imm)\n\
 .else\n\
 orui \\rt,\\rs,%uhi(\\imm)\n\
 ori \\rt,\\rt,%lo(\\imm)\n\
 .endif\n";

static const char * neg_args[] = { "rd", "rs", NULL };
static const char * neg_expn   = "\n sub \\rd,%0,\\rs\n";
static const char * negu_expn  = "\n subu \\rd,%0,\\rs\n";
static const char * li_args[]  = { "rt", "imm", NULL };
static const char * li_expn    = "\n\
 .if (\\imm & 0xffff0000 == 0x0)\n\
 ori \\rt,%0,\\imm\n\
 .elseif (\\imm & 0xffff0000 == 0xffff0000)\n\
 addi \\rt,%0,\\imm\n\
 .elseif (\\imm & 0x0000ffff == 0)\n\
 lui \\rt,%uhi(\\imm)\n\
 .else\n\
 lui \\rt,%uhi(\\imm)\n\
 ori \\rt,\\rt,%lo(\\imm)\n\
 .endif\n";

static iq2000_macro_defs_s iq2000_macro_defs[] =
{
  {"abs",   (const char **) & abs_expn,   (const char **) & abs_args},
  {"la",    (const char **) & la_expn,    (const char **) & la_args},
  {"bge",   (const char **) & bge_expn,   (const char **) & bxx_args},
  {"bgeu",  (const char **) & bgeu_expn,  (const char **) & bxx_args},
  {"bgt",   (const char **) & bgt_expn,   (const char **) & bxx_args},
  {"bgtu",  (const char **) & bgtu_expn,  (const char **) & bxx_args},
  {"ble",   (const char **) & ble_expn,   (const char **) & bxx_args},
  {"bleu",  (const char **) & bleu_expn,  (const char **) & bxx_args},
  {"blt",   (const char **) & blt_expn,   (const char **) & bxx_args},
  {"bltu",  (const char **) & bltu_expn,  (const char **) & bxx_args},
  {"sge",   (const char **) & sge_expn,   (const char **) & sxx_args},
  {"sgeu",  (const char **) & sgeu_expn,  (const char **) & sxx_args},
  {"sle",   (const char **) & sle_expn,   (const char **) & sxx_args},
  {"sleu",  (const char **) & sleu_expn,  (const char **) & sxx_args},
  {"sgt",   (const char **) & sgt_expn,   (const char **) & sxx_args},
  {"sgtu",  (const char **) & sgtu_expn,  (const char **) & sxx_args},
  {"seq",   (const char **) & seq_expn,   (const char **) & sxx_args},
  {"sne",   (const char **) & sne_expn,   (const char **) & sxx_args},
  {"neg",   (const char **) & neg_expn,   (const char **) & neg_args},
  {"negu",  (const char **) & negu_expn,  (const char **) & neg_args},
  {"li",    (const char **) & li_expn,    (const char **) & li_args},
  {"ori32", (const char **) & ori32_expn, (const char **) & ai32_args},
  {"andi32",(const char **) & andi32_expn,(const char **) & ai32_args},
};

static void
iq2000_add_macro (const char *  name,
		  const char *  semantics,
		  const char ** arguments)
{
  macro_entry *macro;
  sb macro_name;
  const char *namestr;

  macro = xmalloc (sizeof (macro_entry));
  sb_new (& macro->sub);
  sb_new (& macro_name);

  macro->formal_count = 0;
  macro->formals = 0;

  sb_add_string (& macro->sub, semantics);

  if (arguments != NULL)
    {
      formal_entry ** p = &macro->formals;

      macro->formal_count = 0;
      macro->formal_hash = hash_new ();

      while (*arguments != NULL)
	{
	  formal_entry *formal;

	  formal = xmalloc (sizeof (formal_entry));

	  sb_new (& formal->name);
	  sb_new (& formal->def);
	  sb_new (& formal->actual);

	  /* chlm: Added the following to allow defaulted args.  */
	  if (strchr (*arguments,'='))
	    {
	      char * tt_args = strdup (*arguments);
	      char * tt_dflt = strchr (tt_args,'=');

	      *tt_dflt = 0;
	      sb_add_string (& formal->name, tt_args);
	      sb_add_string (& formal->def,  tt_dflt + 1);
	    }
	  else
	    sb_add_string (& formal->name, *arguments);

	  /* Add to macro's hash table.  */
	  hash_jam (macro->formal_hash, sb_terminate (& formal->name), formal);

	  formal->index = macro->formal_count;
	  macro->formal_count++;
	  *p = formal;
	  p = & formal->next;
	  *p = NULL;
	  ++arguments;
	}
    }

  sb_add_string (&macro_name, name);
  namestr = sb_terminate (&macro_name);
  hash_jam (macro_hash, namestr, macro);

  macro_defined = 1;
}

static void
iq2000_load_macros (void)
{
  int i;
  int mcnt = ARRAY_SIZE (iq2000_macro_defs);

  for (i = 0; i < mcnt; i++)
    iq2000_add_macro (iq2000_macro_defs[i].mnemonic,
    		      *iq2000_macro_defs[i].expansion,
		      iq2000_macro_defs[i].args);
}

void
md_begin (void)
{
  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = iq2000_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, cpu_mach,
					   CGEN_CPU_OPEN_ENDIAN,
					   CGEN_ENDIAN_BIG,
					   CGEN_CPU_OPEN_END);
  iq2000_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Set the ELF flags if desired.  */
  if (iq2000_flags)
    bfd_set_private_flags (stdoutput, iq2000_flags);

  /* Set the machine type */
  bfd_default_set_arch_mach (stdoutput, bfd_arch_iq2000, iq2000_mach);

  iq2000_load_macros ();
}

void
md_assemble (char * str)
{
  static long delayed_load_register = 0;
  static int last_insn_had_delay_slot = 0;
  static int last_insn_has_load_delay = 0;
  static int last_insn_unconditional_jump = 0;
  static int last_insn_was_ldw = 0;

  iq2000_insn insn;
  char * errmsg;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = iq2000_cgen_assemble_insn
      (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad ("%s", errmsg);
      return;
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, NULL);

  /* We need to generate an error if there's a yielding instruction in the delay
     slot of a control flow modifying instruction (jump (yes), load (no))  */
  if ((last_insn_had_delay_slot && !last_insn_has_load_delay) &&
      CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_YIELD_INSN))
      as_bad (_("the yielding instruction %s may not be in a delay slot."),
              CGEN_INSN_NAME (insn.insn));

  /* Warn about odd numbered base registers for paired-register
     instructions like LDW.  On iq2000, result is always rt.  */
  if (iq2000_mach == bfd_mach_iq2000
      && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_EVEN_REG_NUM)
      && (insn.fields.f_rt % 2))
    as_bad (_("Register number (R%ld) for double word access must be even."),
	    insn.fields.f_rt);

  /* Warn about insns that reference the target of a previous load.  */
  /* NOTE: R0 is a special case and is not subject to load delays (except for ldw).  */
  if (delayed_load_register && (last_insn_has_load_delay || last_insn_was_ldw))
    {
      if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_RD) &&
	  insn.fields.f_rd == delayed_load_register)
	as_warn (_("operand references R%ld of previous load."),
		 insn.fields.f_rd);

      if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_RS) &&
	  insn.fields.f_rs == delayed_load_register)
	as_warn (_("operand references R%ld of previous load."),
		 insn.fields.f_rs);

      if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_RT) &&
	  insn.fields.f_rt == delayed_load_register)
	as_warn (_("operand references R%ld of previous load."),
		 insn.fields.f_rt);

      if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_R31) &&
	  delayed_load_register == 31)
	as_warn (_("instruction implicitly accesses R31 of previous load."));
    }

  /* Warn about insns that reference the (target + 1) of a previous ldw.  */
  if (last_insn_was_ldw)
    {
      if ((CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_RD)
           && insn.fields.f_rd == delayed_load_register + 1)
       || (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_RS)
           && insn.fields.f_rs == delayed_load_register + 1)
       || (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_RT)
           && insn.fields.f_rt == delayed_load_register + 1))
        as_warn (_("operand references R%ld of previous load."),
                delayed_load_register + 1);
    }

  last_insn_had_delay_slot =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_DELAY_SLOT);

  last_insn_has_load_delay =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_LOAD_DELAY);

  if (last_insn_unconditional_jump)
    last_insn_has_load_delay = last_insn_unconditional_jump = 0;
  else if (! strcmp (CGEN_INSN_MNEMONIC (insn.insn), "j")
	   || ! strcmp (CGEN_INSN_MNEMONIC (insn.insn), "jal"))
	   last_insn_unconditional_jump = 1;

  /* The meaning of EVEN_REG_NUM was overloaded to also imply LDW.  Since
     that's not true for IQ10, let's make the above logic specific to LDW.  */
  last_insn_was_ldw = ! strcmp ("ldw", CGEN_INSN_NAME (insn.insn));

  /* The assumption here is that the target of a load is always rt.  */
  delayed_load_register = insn.fields.f_rt;
}

valueT
md_section_align (segT segment, valueT size)
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (char * name ATTRIBUTE_UNUSED)
{
    return 0;
}

/* Interface to relax_segment.  */

/* Return an initial guess of the length by which a fragment must grow to
   hold a branch to reach its destination.
   Also updates fr_type/fr_subtype as necessary.

   Called just before doing relaxation.
   Any symbol that is now undefined will not become defined.
   The guess for fr_var is ACTUALLY the growth beyond fr_fix.
   Whatever we do to grow fr_fix or fr_var contributes to our returned value.
   Although it may not be explicit in the frag, pretend fr_var starts with a
   0 value.  */

int
md_estimate_size_before_relax (fragS * fragP,
			       segT    segment ATTRIBUTE_UNUSED)
{
  int    old_fr_fix = fragP->fr_fix;

  /* The only thing we have to handle here are symbols outside of the
     current segment.  They may be undefined or in a different segment in
     which case linker scripts may place them anywhere.
     However, we can't finish the fragment here and emit the reloc as insn
     alignment requirements may move the insn about.  */

  return (fragP->fr_var + fragP->fr_fix - old_fr_fix);
}

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (bfd   * abfd  ATTRIBUTE_UNUSED,
		 segT    sec   ATTRIBUTE_UNUSED,
		 fragS * fragP ATTRIBUTE_UNUSED)
{
}


/* Functions concerning relocs.  */

long
md_pcrel_from_section (fixS * fixP, segT sec)
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* The symbol is undefined (or is defined but not in this section).
	 Let the linker figure it out.  */
      return 0;
    }

  /* Return the address of the delay slot.  */
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *    insn     ATTRIBUTE_UNUSED,
		      const CGEN_OPERAND * operand,
		      fixS *               fixP     ATTRIBUTE_UNUSED)
{
  switch (operand->type)
    {
    case IQ2000_OPERAND_OFFSET:      return BFD_RELOC_16_PCREL_S2;
    case IQ2000_OPERAND_JMPTARG:     return BFD_RELOC_IQ2000_OFFSET_16;
    case IQ2000_OPERAND_JMPTARGQ10:  return BFD_RELOC_NONE;
    case IQ2000_OPERAND_HI16:        return BFD_RELOC_HI16;
    case IQ2000_OPERAND_LO16:        return BFD_RELOC_LO16;
    default: break;
    }

  return BFD_RELOC_NONE;
}

/* Record a HI16 reloc for later matching with its LO16 cousin.  */

static void
iq2000_record_hi16 (int    reloc_type,
		    fixS * fixP,
		    segT   seg ATTRIBUTE_UNUSED)
{
  struct iq2000_hi_fixup * hi_fixup;

  assert (reloc_type == BFD_RELOC_HI16);

  hi_fixup = xmalloc (sizeof * hi_fixup);
  hi_fixup->fixp = fixP;
  hi_fixup->seg  = now_seg;
  hi_fixup->next = iq2000_hi_fixup_list;

  iq2000_hi_fixup_list = hi_fixup;
}

/* Called while parsing an instruction to create a fixup.
   We need to check for HI16 relocs and queue them up for later sorting.  */

fixS *
iq2000_cgen_record_fixup_exp (fragS *              frag,
			      int                  where,
			      const CGEN_INSN *    insn,
			      int                  length,
			      const CGEN_OPERAND * operand,
			      int                  opinfo,
			      expressionS *        exp)
{
  fixS * fixP = gas_cgen_record_fixup_exp (frag, where, insn, length,
					   operand, opinfo, exp);

  if (operand->type == IQ2000_OPERAND_HI16
      /* If low/high was used, it is recorded in `opinfo'.  */
      && (fixP->fx_cgen.opinfo == BFD_RELOC_HI16
	  || fixP->fx_cgen.opinfo == BFD_RELOC_LO16))
    iq2000_record_hi16 (fixP->fx_cgen.opinfo, fixP, now_seg);

  return fixP;
}

/* Return BFD reloc type from opinfo field in a fixS.
   It's tricky using fx_r_type in iq2000_frob_file because the values
   are BFD_RELOC_UNUSED + operand number.  */
#define FX_OPINFO_R_TYPE(f) ((f)->fx_cgen.opinfo)

/* Sort any unmatched HI16 relocs so that they immediately precede
   the corresponding LO16 reloc.  This is called before md_apply_fix and
   tc_gen_reloc.  */

void
iq2000_frob_file (void)
{
  struct iq2000_hi_fixup * l;

  for (l = iq2000_hi_fixup_list; l != NULL; l = l->next)
    {
      segment_info_type * seginfo;
      int                 pass;

      assert (FX_OPINFO_R_TYPE (l->fixp) == BFD_RELOC_HI16
	      || FX_OPINFO_R_TYPE (l->fixp) == BFD_RELOC_LO16);

      /* Check quickly whether the next fixup happens to be a matching low.  */
      if (l->fixp->fx_next != NULL
	  && FX_OPINFO_R_TYPE (l->fixp->fx_next) == BFD_RELOC_LO16
	  && l->fixp->fx_addsy == l->fixp->fx_next->fx_addsy
	  && l->fixp->fx_offset == l->fixp->fx_next->fx_offset)
	continue;

      /* Look through the fixups for this segment for a matching
         `low'.  When we find one, move the high just in front of it.
         We do this in two passes.  In the first pass, we try to find
         a unique `low'.  In the second pass, we permit multiple
         high's relocs for a single `low'.  */
      seginfo = seg_info (l->seg);
      for (pass = 0; pass < 2; pass++)
	{
	  fixS * f;
	  fixS * prev;

	  prev = NULL;
	  for (f = seginfo->fix_root; f != NULL; f = f->fx_next)
	    {
	      /* Check whether this is a `low' fixup which matches l->fixp.  */
	      if (FX_OPINFO_R_TYPE (f) == BFD_RELOC_LO16
		  && f->fx_addsy == l->fixp->fx_addsy
		  && f->fx_offset == l->fixp->fx_offset
		  && (pass == 1
		      || prev == NULL
		      || (FX_OPINFO_R_TYPE (prev) != BFD_RELOC_HI16)
		      || prev->fx_addsy != f->fx_addsy
		      || prev->fx_offset !=  f->fx_offset))
		{
		  fixS ** pf;

		  /* Move l->fixp before f.  */
		  for (pf = &seginfo->fix_root;
		       * pf != l->fixp;
		       pf = & (* pf)->fx_next)
		    assert (* pf != NULL);

		  * pf = l->fixp->fx_next;

		  l->fixp->fx_next = f;
		  if (prev == NULL)
		    seginfo->fix_root = l->fixp;
		  else
		    prev->fx_next = l->fixp;

		  break;
		}

	      prev = f;
	    }

	  if (f != NULL)
	    break;

	  if (pass == 1)
	    as_warn_where (l->fixp->fx_file, l->fixp->fx_line,
			   _("Unmatched high relocation"));
	}
    }
}

/* See whether we need to force a relocation into the output file.  */

int
iq2000_force_relocation (fixS * fix)
{
  if (fix->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fix->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 1;

  return 0;
}

/* Handle the .set pseudo-op.  */

static void
s_iq2000_set (int x ATTRIBUTE_UNUSED)
{
  static const char * ignored_arguments [] =
    {
      "reorder",
      "noreorder",
      "at",
      "noat",
      "macro",
      "nomacro",
      "move",
      "novolatile",
      "nomove",
      "volatile",
      "bopt",
      "nobopt",
      NULL
    };
  const char ** ignored;
  char *name = input_line_pointer, ch;
  char *save_ILP = input_line_pointer;

  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    input_line_pointer++;
  ch = *input_line_pointer;
  *input_line_pointer = '\0';

  for (ignored = ignored_arguments; * ignored; ignored ++)
    if (strcmp (* ignored, name) == 0)
      break;
  if (* ignored == NULL)
    {
      /* We'd like to be able to use .set symbol, expn */
      input_line_pointer = save_ILP;
      s_set (0);
      return;
    }
  *input_line_pointer = ch;
  demand_empty_rest_of_line ();
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  number_to_chars_bigendian (buf, val, n);
}

void
md_operand (expressionS * exp)
{
  /* In case of a syntax error, escape back to try next syntax combo.  */
  if (exp->X_op == O_absent)
    gas_cgen_md_operand (exp);
}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP .  An error message is
   returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

char *
md_atof (int type, char * litP, int * sizeP)
{
  int              i;
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  char *           t;

  switch (type)
    {
    case 'f':
    case 'F':
    case 's':
    case 'S':
      prec = 2;
      break;

    case 'd':
    case 'D':
    case 'r':
    case 'R':
      prec = 4;
      break;

   /* FIXME: Some targets allow other format chars for bigger sizes here.  */

    default:
      * sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  * sizeP = prec * sizeof (LITTLENUM_TYPE);

  for (i = 0; i < prec; i++)
    {
      md_number_to_chars (litP, (valueT) words[i],
			  sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}


bfd_boolean
iq2000_fix_adjustable (fixS * fixP)
{
  bfd_reloc_code_real_type reloc_type;

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      const CGEN_INSN *insn = NULL;
      int opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      const CGEN_OPERAND *operand = cgen_operand_lookup_by_num(gas_cgen_cpu_desc, opindex);

      reloc_type = md_cgen_lookup_reloc (insn, operand, fixP);
    }
  else
    reloc_type = fixP->fx_r_type;

  if (fixP->fx_addsy == NULL)
    return TRUE;

  /* Prevent all adjustments to global symbols.  */
  if (S_IS_EXTERNAL (fixP->fx_addsy))
    return FALSE;

  if (S_IS_WEAK (fixP->fx_addsy))
    return FALSE;

  /* We need the symbol name for the VTABLE entries.  */
  if (   reloc_type == BFD_RELOC_VTABLE_INHERIT
      || reloc_type == BFD_RELOC_VTABLE_ENTRY)
    return FALSE;

  return TRUE;
}

static void
s_change_sec (int sec)
{
#ifdef OBJ_ELF
  /* The ELF backend needs to know that we are changing sections, so
     that .previous works correctly.  We could do something like check
     for a obj_section_change_hook macro, but that might be confusing
     as it would not be appropriate to use it in the section changing
     functions in read.c, since obj-elf.c intercepts those.  FIXME:
     This should be cleaner, somehow.  */
  obj_elf_section_change_hook ();
#endif

  switch (sec)
    {
    case 't':
      s_text (0);
      break;
    case 'd':
    case 'r':
      s_data (0);
      break;
    }
}

static symbolS *
get_symbol (void)
{
  int c;
  char *name;
  symbolS *p;

  name = input_line_pointer;
  c = get_symbol_end ();
  p = (symbolS *) symbol_find_or_make (name);
  *input_line_pointer = c;
  return p;
}

/* The .end directive.  */

static void
s_iq2000_end (int x ATTRIBUTE_UNUSED)
{
  symbolS *p;
  int maybe_text;

  if (!is_end_of_line[(unsigned char) *input_line_pointer])
    {
      p = get_symbol ();
      demand_empty_rest_of_line ();
    }
  else
    p = NULL;

  if ((bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
    maybe_text = 1;
  else
    maybe_text = 0;

  if (!maybe_text)
    as_warn (_(".end not in text section"));

  if (!cur_proc_ptr)
    {
      as_warn (_(".end directive without a preceding .ent directive."));
      demand_empty_rest_of_line ();
      return;
    }

  if (p != NULL)
    {
      assert (S_GET_NAME (p));
      if (strcmp (S_GET_NAME (p), S_GET_NAME (cur_proc_ptr->isym)))
	as_warn (_(".end symbol does not match .ent symbol."));
    }
  else
    as_warn (_(".end directive missing or unknown symbol"));

  cur_proc_ptr = NULL;
}

static int
get_number (void)
{
  int negative = 0;
  long val = 0;

  if (*input_line_pointer == '-')
    {
      ++input_line_pointer;
      negative = 1;
    }

  if (! ISDIGIT (*input_line_pointer))
    as_bad (_("Expected simple number."));

  if (input_line_pointer[0] == '0')
    {
      if (input_line_pointer[1] == 'x')
	{
	  input_line_pointer += 2;
	  while (ISXDIGIT (*input_line_pointer))
	    {
	      val <<= 4;
	      val |= hex_value (*input_line_pointer++);
	    }
	  return negative ? -val : val;
	}
      else
	{
	  ++input_line_pointer;

	  while (ISDIGIT (*input_line_pointer))
	    {
	      val <<= 3;
	      val |= *input_line_pointer++ - '0';
	    }
	  return negative ? -val : val;
	}
    }

  if (! ISDIGIT (*input_line_pointer))
    {
      printf (_(" *input_line_pointer == '%c' 0x%02x\n"),
	      *input_line_pointer, *input_line_pointer);
      as_warn (_("Invalid number"));
      return -1;
    }

  while (ISDIGIT (*input_line_pointer))
    {
      val *= 10;
      val += *input_line_pointer++ - '0';
    }

  return negative ? -val : val;
}

/* The .aent and .ent directives.  */

static void
s_iq2000_ent (int aent)
{
  int number = 0;
  symbolS *symbolP;
  int maybe_text;

  symbolP = get_symbol ();
  if (*input_line_pointer == ',')
    input_line_pointer++;
  SKIP_WHITESPACE ();
  if (ISDIGIT (*input_line_pointer) || *input_line_pointer == '-')
    number = get_number ();

  if ((bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
    maybe_text = 1;
  else
    maybe_text = 0;

  if (!maybe_text)
    as_warn (_(".ent or .aent not in text section."));

  if (!aent && cur_proc_ptr)
    as_warn (_("missing `.end'"));

  if (!aent)
    {
      cur_proc_ptr = &cur_proc;
      memset (cur_proc_ptr, '\0', sizeof (procS));

      cur_proc_ptr->isym = symbolP;

      symbol_get_bfdsym (symbolP)->flags |= BSF_FUNCTION;

      numprocs++;
    }

  demand_empty_rest_of_line ();
}

/* The .frame directive. If the mdebug section is present (IRIX 5 native)
   then ecoff.c (ecoff_directive_frame) is used. For embedded targets,
   s_iq2000_frame is used so that we can set the PDR information correctly.
   We can't use the ecoff routines because they make reference to the ecoff
   symbol table (in the mdebug section).  */

static void
s_iq2000_frame (int ignore)
{
  s_ignore (ignore);
}

/* The .fmask and .mask directives. If the mdebug section is present
   (IRIX 5 native) then ecoff.c (ecoff_directive_mask) is used. For
   embedded targets, s_iq2000_mask is used so that we can set the PDR
   information correctly. We can't use the ecoff routines because they
   make reference to the ecoff symbol table (in the mdebug section).  */

static void
s_iq2000_mask (int reg_type)
{
  s_ignore (reg_type);
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
    { "align",  s_align_bytes,           0 },
    { "word",   cons,                    4 },
    { "rdata",  s_change_sec, 		'r'},
    { "sdata",  s_change_sec, 		's'},
    { "set",	s_iq2000_set,		 0 },
    { "ent",    s_iq2000_ent, 		 0 },
    { "end",    s_iq2000_end,            0 },
    { "frame",  s_iq2000_frame, 	 0 },
    { "fmask",  s_iq2000_mask, 		'F'},
    { "mask",   s_iq2000_mask, 		'R'},
    { "dword",	cons, 			 8 },
    { "half",	cons, 			 2 },
    { NULL, 	NULL,			 0 }
};
