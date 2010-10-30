/* tc-xstormy16.c -- Assembler for the Sanyo XSTORMY16.
   Copyright 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation.

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

#include "as.h"
#include "subsegs.h"
#include "symcat.h"
#include "opcodes/xstormy16-desc.h"
#include "opcodes/xstormy16-opc.h"
#include "cgen.h"

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
xstormy16_insn;

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = "|";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

#define O_fptr_symbol	(O_max + 1)

#define XSTORMY16_SHORTOPTS ""
const char * md_shortopts = XSTORMY16_SHORTOPTS;

struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (int    c ATTRIBUTE_UNUSED,
		 char * arg ATTRIBUTE_UNUSED)
{
  return 0;
}

void
md_show_usage (FILE * stream)
{
  fprintf (stream, _(" XSTORMY16 specific command line options:\n"));
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word",	cons,		4 },
  { NULL, 	NULL, 		0 }
};


void
md_begin (void)
{
  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = xstormy16_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, 0,
					  CGEN_CPU_OPEN_ENDIAN,
					  CGEN_ENDIAN_LITTLE,
					  CGEN_CPU_OPEN_END);
  xstormy16_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);
}

static bfd_boolean skipping_fptr = FALSE;

void
md_assemble (char * str)
{
  xstormy16_insn insn;
  char *    errmsg;

  /* Make sure that if we had an erroneous input line which triggered
     the skipping_fptr boolean that it does not affect following lines.  */
  skipping_fptr = FALSE;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = xstormy16_cgen_assemble_insn
    (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad (errmsg);
      return;
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 0, NULL);
}

void
md_operand (expressionS * e)
{
  if (*input_line_pointer != '@')
    return;

  if (strncmp (input_line_pointer + 1, "fptr", 4) == 0)
    {
      input_line_pointer += 5;
      SKIP_WHITESPACE ();
      if (*input_line_pointer != '(')
	{
	  as_bad ("Expected '('");
	  goto err;
	}
      input_line_pointer++;

      expression (e);

      if (*input_line_pointer != ')')
	{
	  as_bad ("Missing ')'");
	  goto err;
	}
      input_line_pointer++;
      SKIP_WHITESPACE ();

      if (e->X_op != O_symbol)
	as_bad ("Not a symbolic expression");
      else if (* input_line_pointer == '-')
	/* We are computing the difference of two function pointers
	   like this:

	    .hword  @fptr (foo) - @fptr (bar)

	  In this situation we do not want to generate O_fptr_symbol
	  operands because the result is an absolute value, not a
	  function pointer.

	  We need to make the check here, rather than when the fixup
	  is generated as the function names (foo & bar in the above
	  example) might be local symbols and we want the expression
	  to be evaluated now.  This kind of thing can happen when
	  gcc is generating computed gotos.  */
	skipping_fptr = TRUE;
      else if (skipping_fptr)
	skipping_fptr = FALSE;
      else
        e->X_op = O_fptr_symbol;
    }

  return;
 err:
  ignore_rest_of_line ();
}

/* Called while parsing data to create a fixup.
   Create BFD_RELOC_XSTORMY16_FPTR16 relocations.  */

void
xstormy16_cons_fix_new (fragS *f,
			int where,
			int nbytes,
			expressionS *exp)
{
  bfd_reloc_code_real_type code;
  fixS *fix;

  if (exp->X_op == O_fptr_symbol)
    {
      switch (nbytes)
	{
 	case 4:
 	  /* This can happen when gcc is generating debug output.
 	     For example it can create a stab with the address of
 	     a function:
 	     
 	     	.stabs	"foo:F(0,21)",36,0,0,@fptr(foo)
 
 	     Since this does not involve switching code pages, we
 	     just allow the reloc to be generated without any
 	     @fptr behaviour.  */
 	  exp->X_op = O_symbol;
 	  code = BFD_RELOC_32;
 	  break;

 	case 2:
 	  exp->X_op = O_symbol;
 	  code = BFD_RELOC_XSTORMY16_FPTR16;
 	  break;

 	default:
	  as_bad ("unsupported fptr fixup size %d", nbytes);
	  return;
	}
    }
  else if (nbytes == 1)
    code = BFD_RELOC_8;
  else if (nbytes == 2)
    code = BFD_RELOC_16;
  else if (nbytes == 4)
    code = BFD_RELOC_32;
  else
    {
      as_bad ("unsupported fixup size %d", nbytes);
      return;
    }

  fix = fix_new_exp (f, where, nbytes, exp, 0, code);
}

/* Called while parsing an instruction to create a fixup.
   Create BFD_RELOC_XSTORMY16_FPTR16 relocations.  */

fixS *
xstormy16_cgen_record_fixup_exp (fragS *              frag,
				 int                  where,
				 const CGEN_INSN *    insn,
				 int                  length,
				 const CGEN_OPERAND * operand,
				 int                  opinfo,
				 expressionS *        exp)
{
  fixS *fixP;
  operatorT op = exp->X_op;

  if (op == O_fptr_symbol)
    exp->X_op = O_symbol;

  fixP = gas_cgen_record_fixup_exp (frag, where, insn, length,
				    operand, opinfo, exp);

  if (op == O_fptr_symbol)
    {
      if (operand->type != XSTORMY16_OPERAND_IMM16)
	as_bad ("unsupported fptr fixup");
      else
	{
	  fixP->fx_r_type = BFD_RELOC_XSTORMY16_FPTR16;
	  fixP->fx_where += 2;
	}
    }

  return fixP;
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
md_estimate_size_before_relax (fragS * fragP ATTRIBUTE_UNUSED,
			       segT    segment ATTRIBUTE_UNUSED)
{
  /* No assembler relaxation is defined (or necessary) for this port.  */
  abort ();
}

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (bfd *   abfd ATTRIBUTE_UNUSED,
		 segT    sec ATTRIBUTE_UNUSED,
		 fragS * fragP ATTRIBUTE_UNUSED)
{
  /* No assembler relaxation is defined (or necessary) for this port.  */
  abort ();
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixS * fixP, segT sec)
{
  if ((fixP->fx_addsy != (symbolS *) NULL
       && (! S_IS_DEFINED (fixP->fx_addsy)
	   || S_GET_SEGMENT (fixP->fx_addsy) != sec))
      || xstormy16_force_relocation (fixP))
    /* The symbol is undefined,
       or it is defined but not in this section,
       or the relocation will be relative to this symbol not the section symbol.	 
       Let the linker figure it out.  */
    return 0;

  return fixP->fx_frag->fr_address + fixP->fx_where;
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *    insn ATTRIBUTE_UNUSED,
		      const CGEN_OPERAND * operand,
		      fixS *               fixP)
{
  switch (operand->type)
    {
    case XSTORMY16_OPERAND_IMM2:
    case XSTORMY16_OPERAND_IMM3:
    case XSTORMY16_OPERAND_IMM3B:
    case XSTORMY16_OPERAND_IMM4:
    case XSTORMY16_OPERAND_HMEM8:
      return BFD_RELOC_NONE;

    case XSTORMY16_OPERAND_IMM12:
      fixP->fx_where += 2;
      return BFD_RELOC_XSTORMY16_12;

    case XSTORMY16_OPERAND_IMM8:
    case XSTORMY16_OPERAND_LMEM8:
      return fixP->fx_pcrel ? BFD_RELOC_8_PCREL : BFD_RELOC_8;

    case XSTORMY16_OPERAND_IMM16:
      /* This might have been processed at parse time.  */
      fixP->fx_where += 2;
      if (fixP->fx_cgen.opinfo && fixP->fx_cgen.opinfo != BFD_RELOC_NONE)
	return fixP->fx_cgen.opinfo;
      return fixP->fx_pcrel ? BFD_RELOC_16_PCREL : BFD_RELOC_16;

    case XSTORMY16_OPERAND_ABS24:
      return BFD_RELOC_XSTORMY16_24;

    case XSTORMY16_OPERAND_REL8_4:
      fixP->fx_addnumber -= 2;
    case XSTORMY16_OPERAND_REL8_2:
      fixP->fx_addnumber -= 2;
      fixP->fx_pcrel = 1;
      return BFD_RELOC_8_PCREL;

    case XSTORMY16_OPERAND_REL12:
      fixP->fx_where += 2;
      /* Fall through...  */
    case XSTORMY16_OPERAND_REL12A:
      fixP->fx_addnumber -= 2;
      fixP->fx_pcrel = 1;
      return BFD_RELOC_XSTORMY16_REL_12;

    default : /* avoid -Wall warning */
      abort ();
    }
}

/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
xstormy16_force_relocation (fixS * fix)
{
  if (fix->fx_r_type == BFD_RELOC_XSTORMY16_FPTR16)
    return 1;

  return generic_force_reloc (fix);
}

/* Return true if a relocation against a symbol may be replaced with
   a relocation against section+offset.  */

bfd_boolean
xstormy16_fix_adjustable (fixS * fixP)
{
  /* We need the symbol name for the VTABLE entries.  */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return FALSE;

  if (fixP->fx_r_type == BFD_RELOC_XSTORMY16_FPTR16)
    return FALSE;

  return TRUE;
}

/* This is a copy of gas_cgen_md_apply_fix, with some enhancements to
   do various things that would not be valid for all ports.  */

void
xstormy16_md_apply_fix (fixS *   fixP,
			 valueT * valueP,
			 segT     seg ATTRIBUTE_UNUSED)
{
  char *where = fixP->fx_frag->fr_literal + fixP->fx_where;
  valueT value = *valueP;
  /* Canonical name, since used a lot.  */
  CGEN_CPU_DESC cd = gas_cgen_cpu_desc;

  /* md_cgen_lookup_reloc() will adjust this to compensate for where
     in the opcode the relocation happens, for pcrel relocations.  We
     have no other way of keeping track of what this offset needs to
     be.  */
  fixP->fx_addnumber = 0;

  /* This port has pc-relative relocs and DIFF_EXPR_OK defined, so
     it must deal with turning a BFD_RELOC_{8,16,32,64} into a
     BFD_RELOC_*_PCREL for the case of

	.word something-.  */
  if (fixP->fx_pcrel)
    switch (fixP->fx_r_type)
      {
      case BFD_RELOC_8:
	fixP->fx_r_type = BFD_RELOC_8_PCREL;
	break;
      case BFD_RELOC_16:
	fixP->fx_r_type = BFD_RELOC_16_PCREL;
	break;
      case BFD_RELOC_32:
	fixP->fx_r_type = BFD_RELOC_32_PCREL;
	break;
      case BFD_RELOC_64:
	fixP->fx_r_type = BFD_RELOC_64_PCREL;
	break;
      default:
	break;
      }

  if (fixP->fx_addsy == (symbolS *) NULL)
    fixP->fx_done = 1;

  /* We don't actually support subtracting a symbol.  */
  if (fixP->fx_subsy != (symbolS *) NULL)
    as_bad_where (fixP->fx_file, fixP->fx_line, _("expression too complex"));

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      int opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      const CGEN_OPERAND *operand = cgen_operand_lookup_by_num (cd, opindex);
      const char *errmsg;
      bfd_reloc_code_real_type reloc_type;
      CGEN_FIELDS *fields = alloca (CGEN_CPU_SIZEOF_FIELDS (cd));
      const CGEN_INSN *insn = fixP->fx_cgen.insn;

      /* If the reloc has been fully resolved finish the operand here.  */
      /* FIXME: This duplicates the capabilities of code in BFD.  */
      if (fixP->fx_done)
	{
	  CGEN_CPU_SET_FIELDS_BITSIZE (cd) (fields, CGEN_INSN_BITSIZE (insn));
	  CGEN_CPU_SET_VMA_OPERAND (cd) (cd, opindex, fields, (bfd_vma) value);

#if CGEN_INT_INSN_P
	  {
	    CGEN_INSN_INT insn_value =
	      cgen_get_insn_value (cd, (unsigned char *) where,
				   CGEN_INSN_BITSIZE (insn));

	    /* ??? 0 is passed for `pc'.  */
	    errmsg = CGEN_CPU_INSERT_OPERAND (cd) (cd, opindex, fields,
						   &insn_value, (bfd_vma) 0);
	    cgen_put_insn_value (cd, (unsigned char *) where,
				 CGEN_INSN_BITSIZE (insn), insn_value);
	  }
#else
	  /* ??? 0 is passed for `pc'.  */
	  errmsg = CGEN_CPU_INSERT_OPERAND (cd) (cd, opindex, fields,
						 (unsigned char *) where,
						 (bfd_vma) 0);
#endif
	  if (errmsg)
	    as_bad_where (fixP->fx_file, fixP->fx_line, "%s", errmsg);
	}

      if (fixP->fx_done)
	return;

      /* The operand isn't fully resolved.  Determine a BFD reloc value
	 based on the operand information and leave it to
	 bfd_install_relocation.  Note that this doesn't work when
	 !partial_inplace.  */

      reloc_type = md_cgen_lookup_reloc (insn, operand, fixP);
      if (reloc_type != BFD_RELOC_NONE)
	fixP->fx_r_type = reloc_type;
      else
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("unresolved expression that must be resolved"));
	  fixP->fx_done = 1;
	  return;
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
	case BFD_RELOC_64:
	  md_number_to_chars (where, value, 8);
	  break;
	default:
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("internal error: can't install fix for reloc type %d (`%s')"),
			fixP->fx_r_type, bfd_get_reloc_code_name (fixP->fx_r_type));
	  break;
	}
    }
  else
    {
      /* bfd_install_relocation will be called to finish things up.  */
    }

  /* This is a RELA port.  Thus, it does not need to store a
     value if it is going to make a reloc.  What's more, when
     assembling a line like

     .byte global-0x7f00

     we'll get a spurious error message if we try to stuff 0x7f00 into
     the byte.  */
  if (! fixP->fx_done)
    *valueP = 0;

  /* Tuck `value' away for use by tc_gen_reloc.
     See the comment describing fx_addnumber in write.h.
     This field is misnamed (or misused :-).  */
  fixP->fx_addnumber += value;
}


/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  number_to_chars_littleendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.
*/

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

char *
md_atof (int type, char * litP, int * sizeP)
{
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  LITTLENUM_TYPE   *wordP;
  char *           t;

  switch (type)
    {
    case 'f':
    case 'F':
      prec = 2;
      break;

    case 'd':
    case 'D':
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

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  /* This loops outputs the LITTLENUMs in REVERSE order; in accord with
     the littleendianness of the processor.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}
