/* GAS interface for targets using CGEN: Cpu tools GENerator.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

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
along with GAS; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "ansidecl.h"
#include "bfd.h"
#include "cgen-opc.h"
#include "as.h"
#include "subsegs.h"

/* Callback to insert a register into the symbol table.
   A target may choose to let GAS parse the registers.
   ??? Not currently used.  */

void
cgen_asm_record_register (name, number)
     char *name;
     int number;
{
  /* Use symbol_create here instead of symbol_new so we don't try to
     output registers into the object file's symbol table.  */
  symbol_table_insert (symbol_create (name, reg_section,
				      number, &zero_address_frag));
}

/* We need to keep a list of fixups.  We can't simply generate them as
   we go, because that would require us to first create the frag, and
   that would screw up references to ``.''.

   This is used by cpu's with simple operands.  It keeps knowledge of what
   an `expressionS' is and what a `fixup' is out of CGEN which for the time
   being is preferable.

   OPINDEX is the index in the operand table.
   OPINFO is something the caller chooses to help in reloc determination.  */

struct fixup
{
  int opindex;
  int opinfo;
  expressionS exp;
};

#define MAX_FIXUPS 5

static struct fixup fixups[MAX_FIXUPS];
static int num_fixups;

/* Prepare to parse an instruction.
   ??? May wish to make this static and delete calls in md_assemble.  */

void
cgen_asm_init_parse ()
{
  num_fixups = 0;
}

/* Queue a fixup.  */

void
cgen_queue_fixup (opindex, opinfo, expP)
     int opindex;
     expressionS *expP;
{
  /* We need to generate a fixup for this expression.  */
  if (num_fixups >= MAX_FIXUPS)
    as_fatal ("too many fixups");
  fixups[num_fixups].exp = *expP;
  fixups[num_fixups].opindex = opindex;
  fixups[num_fixups].opinfo = opinfo;
  ++num_fixups;
}

/* Default routine to record a fixup.
   This is a cover function to fix_new.
   It exists because we record INSN with the fixup.

   FRAG and WHERE are their respective arguments to fix_new_exp.
   LENGTH is in bits.
   OPINFO is something the caller chooses to help in reloc determination.

   At this point we do not use a bfd_reloc_code_real_type for
   operands residing in the insn, but instead just use the
   operand index.  This lets us easily handle fixups for any
   operand type.  We pick a BFD reloc type in md_apply_fix.  */

fixS *
cgen_record_fixup (frag, where, insn, length, operand, opinfo, symbol, offset)
     fragS *frag;
     int where;
     const struct cgen_insn *insn;
     int length;
     const struct cgen_operand *operand;
     int opinfo;
     symbolS *symbol;
     offsetT offset;
{
  fixS *fixP;

  /* It may seem strange to use operand->attrs and not insn->attrs here,
     but it is the operand that has a pc relative relocation.  */

  fixP = fix_new (frag, where, length / 8, symbol, offset,
		  CGEN_OPERAND_ATTR (operand, CGEN_OPERAND_PCREL_ADDR) != 0,
		  (bfd_reloc_code_real_type) ((int) BFD_RELOC_UNUSED + CGEN_OPERAND_INDEX (operand)));
  fixP->tc_fix_data.insn = (PTR) insn;
  fixP->tc_fix_data.opinfo = opinfo;

  return fixP;
}

/* Default routine to record a fixup given an expression.
   This is a cover function to fix_new_exp.
   It exists because we record INSN with the fixup.

   FRAG and WHERE are their respective arguments to fix_new_exp.
   LENGTH is in bits.
   OPINFO is something the caller chooses to help in reloc determination.

   At this point we do not use a bfd_reloc_code_real_type for
   operands residing in the insn, but instead just use the
   operand index.  This lets us easily handle fixups for any
   operand type.  We pick a BFD reloc type in md_apply_fix.  */

fixS *
cgen_record_fixup_exp (frag, where, insn, length, operand, opinfo, exp)
     fragS *frag;
     int where;
     const struct cgen_insn *insn;
     int length;
     const struct cgen_operand *operand;
     int opinfo;
     expressionS *exp;
{
  fixS *fixP;

  /* It may seem strange to use operand->attrs and not insn->attrs here,
     but it is the operand that has a pc relative relocation.  */

  fixP = fix_new_exp (frag, where, length / 8, exp,
		      CGEN_OPERAND_ATTR (operand, CGEN_OPERAND_PCREL_ADDR) != 0,
		      (bfd_reloc_code_real_type) ((int) BFD_RELOC_UNUSED + CGEN_OPERAND_INDEX (operand)));
  fixP->tc_fix_data.insn = (PTR) insn;
  fixP->tc_fix_data.opinfo = opinfo;

  return fixP;
}

/* Callback for cgen interface.  Parse the expression at *STRP.
   The result is an error message or NULL for success (in which case
   *STRP is advanced past the parsed text).
   WANT is an indication of what the caller is looking for.
   If WANT == CGEN_ASM_PARSE_INIT the caller is beginning to try to match
   a table entry with the insn, reset the queued fixups counter.
   An enum cgen_parse_operand_result is stored in RESULTP.
   OPINDEX is the operand's table entry index.
   OPINFO is something the caller chooses to help in reloc determination.
   The resulting value is stored in VALUEP.  */

const char *
cgen_parse_operand (want, strP, opindex, opinfo, resultP, valueP)
     enum cgen_parse_operand_type want;
     const char **strP;
     int opindex;
     int opinfo;
     enum cgen_parse_operand_result *resultP;
     bfd_vma *valueP;
{
  char *hold;
  const char *errmsg = NULL;
  expressionS exp;

  if (want == CGEN_PARSE_OPERAND_INIT)
    {
      cgen_asm_init_parse ();
      return NULL;
    }

  hold = input_line_pointer;
  input_line_pointer = (char *) *strP;
  expression (&exp);
  *strP = input_line_pointer;
  input_line_pointer = hold;

  /* FIXME: Need to check `want'.  */

  switch (exp.X_op)
    {
    case O_illegal :
      errmsg = "illegal operand";
      *resultP = CGEN_PARSE_OPERAND_RESULT_ERROR;
      break;
    case O_absent :
      errmsg = "missing operand";
      *resultP = CGEN_PARSE_OPERAND_RESULT_ERROR;
      break;
    case O_constant :
      *valueP = exp.X_add_number;
      *resultP = CGEN_PARSE_OPERAND_RESULT_NUMBER;
      break;
    case O_register :
      *valueP = exp.X_add_number;
      *resultP = CGEN_PARSE_OPERAND_RESULT_REGISTER;
      break;
    default :
      cgen_queue_fixup (opindex, opinfo, &exp);
      *valueP = 0;
      *resultP = CGEN_PARSE_OPERAND_RESULT_QUEUED;
      break;
    }

  return errmsg;
}

/* Finish assembling instruction INSN.
   BUF contains what we've built up so far.
   LENGTH is the size of the insn in bits.  */

void
cgen_asm_finish_insn (insn, buf, length)
     const struct cgen_insn *insn;
     cgen_insn_t *buf;
     unsigned int length;
{
  int i, relax_operand;
  char *f;
  unsigned int byte_len = length / 8;

  /* ??? Target foo issues various warnings here, so one might want to provide
     a hook here.  However, our caller is defined in tc-foo.c so there
     shouldn't be a need for a hook.  */

  /* Write out the instruction.
     It is important to fetch enough space in one call to `frag_more'.
     We use (f - frag_now->fr_literal) to compute where we are and we
     don't want frag_now to change between calls.

     Relaxable instructions: We need to ensure we allocate enough
     space for the largest insn.  */

  if (CGEN_INSN_ATTR (insn, CGEN_INSN_RELAX) != 0)
    abort (); /* These currently shouldn't get here.  */

  /* Is there a relaxable insn with the relaxable operand needing a fixup?  */

  relax_operand = -1;
  if (CGEN_INSN_ATTR (insn, CGEN_INSN_RELAXABLE) != 0)
    {
      /* Scan the fixups for the operand affected by relaxing
	 (i.e. the branch address).  */

      for (i = 0; i < num_fixups; ++i)
	{
	  if (CGEN_OPERAND_ATTR (& CGEN_SYM (operand_table) [fixups[i].opindex],
				 CGEN_OPERAND_RELAX) != 0)
	    {
	      relax_operand = i;
	      break;
	    }
	}
    }

  if (relax_operand != -1)
    {
      int max_len;
      fragS *old_frag;

#ifdef TC_CGEN_MAX_RELAX
      max_len = TC_CGEN_MAX_RELAX (insn, byte_len);
#else
      max_len = CGEN_MAX_INSN_SIZE;
#endif
      /* Ensure variable part and fixed part are in same fragment.  */
      /* FIXME: Having to do this seems like a hack.  */
      frag_grow (max_len);
      /* Allocate space for the fixed part.  */
      f = frag_more (byte_len);
      /* Create a relaxable fragment for this instruction.  */
      old_frag = frag_now;
      frag_var (rs_machine_dependent,
		max_len - byte_len /* max chars */,
		0 /* variable part already allocated */,
		/* FIXME: When we machine generate the relax table,
		   machine generate a macro to compute subtype.  */
		1 /* subtype */,
		fixups[relax_operand].exp.X_add_symbol,
		fixups[relax_operand].exp.X_add_number,
		f);
      /* Record the operand number with the fragment so md_convert_frag
	 can use cgen_md_record_fixup to record the appropriate reloc.  */
      /* FIXME: fr_targ.cgen is used pending deciding whether to
	 allow a target to add members to fragS.  For more info
	 see the comment above fr_targ in as.h.  */
      old_frag->fr_targ.cgen.insn = insn;
      old_frag->fr_targ.cgen.opindex = fixups[relax_operand].opindex;
      old_frag->fr_targ.cgen.opinfo = fixups[relax_operand].opinfo;
    }
  else
    f = frag_more (byte_len);

  /* If we're recording insns as numbers (rather than a string of bytes),
     target byte order handling is deferred until now.  */
#if 0 /*def CGEN_INT_INSN*/
  switch (length)
    {
    case 16:
      if (cgen_big_endian_p)
	bfd_putb16 ((bfd_vma) *buf, f);
      else
	bfd_putl16 ((bfd_vma) *buf, f);
      break;
    case 32:
      if (cgen_big_endian_p)
	bfd_putb32 ((bfd_vma) *buf, f);
      else
	bfd_putl32 ((bfd_vma) *buf, f);
      break;
    default:
      abort ();
    }
#else
  memcpy (f, buf, byte_len);
#endif

  /* Create any fixups.  */
  for (i = 0; i < num_fixups; ++i)
    {
      /* Don't create fixups for these.  That's done during relaxation.
	 We don't need to test for CGEN_INSN_RELAX as they can't get here
	 (see above).  */
      if (CGEN_INSN_ATTR (insn, CGEN_INSN_RELAXABLE) != 0
	  && CGEN_OPERAND_ATTR (& CGEN_SYM (operand_table) [fixups[i].opindex],
				CGEN_OPERAND_RELAX) != 0)
	continue;

#ifndef md_cgen_record_fixup_exp
#define md_cgen_record_fixup_exp cgen_record_fixup_exp
#endif

      md_cgen_record_fixup_exp (frag_now, f - frag_now->fr_literal,
				insn, length,
				& CGEN_SYM (operand_table) [fixups[i].opindex],
				fixups[i].opinfo,
				&fixups[i].exp);
    }
}

/* Apply a fixup to the object code.  This is called for all the
   fixups we generated by the call to fix_new_exp, above.  In the call
   above we used a reloc code which was the largest legal reloc code
   plus the operand index.  Here we undo that to recover the operand
   index.  At this point all symbol values should be fully resolved,
   and we attempt to completely resolve the reloc.  If we can not do
   that, we determine the correct reloc code and put it back in the fixup.  */

/* FIXME: This function handles some of the fixups and bfd_install_relocation
   handles the rest.  bfd_install_relocation (or some other bfd function)
   should handle them all.  */

int
cgen_md_apply_fix3 (fixP, valueP, seg)
     fixS *fixP;
     valueT *valueP;
     segT seg;
{
  char *where = fixP->fx_frag->fr_literal + fixP->fx_where;
  valueT value;

  /* FIXME FIXME FIXME: The value we are passed in *valuep includes
     the symbol values.  Since we are using BFD_ASSEMBLER, if we are
     doing this relocation the code in write.c is going to call
     bfd_install_relocation, which is also going to use the symbol
     value.  That means that if the reloc is fully resolved we want to
     use *valuep since bfd_install_relocation is not being used.
     However, if the reloc is not fully resolved we do not want to use
     *valuep, and must use fx_offset instead.  However, if the reloc
     is PC relative, we do want to use *valuep since it includes the
     result of md_pcrel_from.  This is confusing.  */

  if (fixP->fx_addsy == (symbolS *) NULL)
    {
      value = *valueP;
      fixP->fx_done = 1;
    }
  else if (fixP->fx_pcrel)
    value = *valueP;
  else
    {
      value = fixP->fx_offset;
      if (fixP->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixP->fx_subsy) == absolute_section)
	    value -= S_GET_VALUE (fixP->fx_subsy);
	  else
	    {
	      /* We don't actually support subtracting a symbol.  */
 	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    "expression too complex");
	    }
	}
    }

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      int opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      const struct cgen_operand *operand = & CGEN_SYM (operand_table) [opindex];
      const char *errmsg;
      bfd_reloc_code_real_type reloc_type;
      struct cgen_fields fields;
      const struct cgen_insn *insn = (struct cgen_insn *) fixP->tc_fix_data.insn;

      /* If the reloc has been fully resolved finish the operand here.  */
      /* FIXME: This duplicates the capabilities of code in BFD.  */
      if (fixP->fx_done
	  /* FIXME: If partial_inplace isn't set bfd_install_relocation won't
	     finish the job.  Testing for pcrel is a temporary hack.  */
	  || fixP->fx_pcrel)
	{
	  /* This may seem like overkill, and using bfd_install_relocation or
	     some such may be preferable, but this is simple.  */
	  CGEN_FIELDS_BITSIZE (&fields) = CGEN_INSN_BITSIZE (insn);
	  CGEN_SYM (set_operand) (opindex, &value, &fields);
	  errmsg = CGEN_SYM (validate_operand) (opindex, &fields);
	  if (errmsg)
	    as_warn_where (fixP->fx_file, fixP->fx_line, "%s\n", errmsg);
	  CGEN_SYM (insert_operand) (opindex, &fields, where);
	}

      if (fixP->fx_done)
	return 1;

      /* The operand isn't fully resolved.  Determine a BFD reloc value
	 based on the operand information and leave it to
	 bfd_install_relocation.  Note that this doesn't work when
	 partial_inplace == false.  */

      reloc_type = CGEN_SYM (lookup_reloc) (insn, operand, fixP);
      if (reloc_type != BFD_RELOC_NONE)
	{
	  fixP->fx_r_type = reloc_type;
	}
      else
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			"unresolved expression that must be resolved");
	  fixP->fx_done = 1;
	  return 1;
	}
    }
  else if (fixP->fx_done)
    {
      /* We're finished with this fixup.  Install it because
	 bfd_install_relocation won't be called to do it.  */
      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_8:
	  md_number_to_chars (where, value, 1);
	  break;
	case BFD_RELOC_16:
	  md_number_to_chars (where, value, 2);
	  break;
	case BFD_RELOC_32:
	  md_number_to_chars (where, value, 4);
	  break;
	/* FIXME: later add support for 64 bits.  */
	default:
	  abort ();
	}
    }
  else
    {
      /* bfd_install_relocation will be called to finish things up.  */
    }

  /* Tuck `value' away for use by tc_gen_reloc.
     See the comment describing fx_addnumber in write.h.
     This field is misnamed (or misused :-).  */
  fixP->fx_addnumber = value;

  return 1;
}

/* Translate internal representation of relocation info to BFD target format.

   FIXME: To what extent can we get all relevant targets to use this?  */

arelent *
cgen_tc_gen_reloc (section, fixP)
     asection *section;
     fixS *fixP;
{
  arelent *reloc;

  reloc = (arelent *) bfd_alloc (stdoutput, sizeof (arelent));

  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    "internal error: can't export reloc type %d (`%s')",
		    fixP->fx_r_type, bfd_get_reloc_code_name (fixP->fx_r_type));
      return NULL;
    }

  assert (!fixP->fx_pcrel == !reloc->howto->pc_relative);

  reloc->sym_ptr_ptr = &fixP->fx_addsy->bsym;
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;
  reloc->addend = fixP->fx_addnumber;

  return reloc;
}
