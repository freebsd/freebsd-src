/* tc-openrisc.c -- Assembler for the OpenRISC family.
   Copyright 2001, 2002, 2003 Free Software Foundation.
   Contributed by Johan Rydberg, jrydberg@opencores.org

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

#include <stdio.h>
#include "as.h"
#include "subsegs.h"
#include "symcat.h"
#include "opcodes/openrisc-desc.h"
#include "opcodes/openrisc-opc.h"
#include "cgen.h"

/* Structure to hold all of the different components describing
   an individual instruction.  */
typedef struct openrisc_insn openrisc_insn;

struct openrisc_insn
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
};


const char comment_chars[]        = "#";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = ";";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";


#define OPENRISC_SHORTOPTS "m:"
const char * md_shortopts = OPENRISC_SHORTOPTS;

struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

unsigned long openrisc_machine = 0; /* default */

int
md_parse_option (c, arg)
     int    c ATTRIBUTE_UNUSED;
     char * arg ATTRIBUTE_UNUSED;
{
  return 0;
}

void
md_show_usage (stream)
  FILE * stream ATTRIBUTE_UNUSED;
{
}

static void ignore_pseudo PARAMS ((int));

static void
ignore_pseudo (val)
     int val ATTRIBUTE_UNUSED;
{
  discard_rest_of_line ();
}

const char openrisc_comment_chars [] = ";#";

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word",     cons,           4 },
  { "proc",     ignore_pseudo,  0 },
  { "endproc",  ignore_pseudo,  0 },
  { NULL, 	NULL, 		0 }
};



void
md_begin ()
{
  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = openrisc_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, 0,
                                              CGEN_CPU_OPEN_ENDIAN,
                                              CGEN_ENDIAN_BIG,
                                              CGEN_CPU_OPEN_END);
  openrisc_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);
}

void
md_assemble (str)
     char * str;
{
  static int last_insn_had_delay_slot = 0;
  openrisc_insn insn;
  char *    errmsg;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = openrisc_cgen_assemble_insn
    (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad (errmsg);
      return;
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, NULL);

#if 0 /* Currently disabled  */
  /* Warn about invalid insns in delay slots.  */
  if (last_insn_had_delay_slot
      && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_NOT_IN_DELAY_SLOT))
    as_warn (_("Instruction %s not allowed in a delay slot."),
	     CGEN_INSN_NAME (insn.insn));
#endif

  last_insn_had_delay_slot
    = CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_DELAY_SLOT);
}


/* The syntax in the manual says constants begin with '#'.
   We just ignore it.  */

void
md_operand (expressionP)
     expressionS * expressionP;
{
  if (* input_line_pointer == '#')
    {
      input_line_pointer ++;
      expression (expressionP);
    }
}

valueT
md_section_align (segment, size)
     segT   segment;
     valueT size;
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (name)
     char * name ATTRIBUTE_UNUSED;
{
  return 0;
}


/* Interface to relax_segment.  */

/* FIXME: Look through this.  */

const relax_typeS md_relax_table[] =
{
/* The fields are:
   1) most positive reach of this state,
   2) most negative reach of this state,
   3) how many bytes this mode will add to the size of the current frag
   4) which index into the table to try if we can't fit into this one.  */

  /* The first entry must be unused because an `rlx_more' value of zero ends
     each list.  */
  {1, 1, 0, 0},

  /* The displacement used by GAS is from the end of the 2 byte insn,
     so we subtract 2 from the following.  */
  /* 16 bit insn, 8 bit disp -> 10 bit range.
     This doesn't handle a branch in the right slot at the border:
     the "& -4" isn't taken into account.  It's not important enough to
     complicate things over it, so we subtract an extra 2 (or + 2 in -ve
     case).  */
  {511 - 2 - 2, -512 - 2 + 2, 0, 2 },
  /* 32 bit insn, 24 bit disp -> 26 bit range.  */
  {0x2000000 - 1 - 2, -0x2000000 - 2, 2, 0 },
  /* Same thing, but with leading nop for alignment.  */
  {0x2000000 - 1 - 2, -0x2000000 - 2, 4, 0 }
};

long
openrisc_relax_frag (segment, fragP, stretch)
     segT    segment;
     fragS * fragP;
     long    stretch;
{
  /* Address of branch insn.  */
  long address = fragP->fr_address + fragP->fr_fix - 2;
  long growth = 0;

  /* Keep 32 bit insns aligned on 32 bit boundaries.  */
  if (fragP->fr_subtype == 2)
    {
      if ((address & 3) != 0)
	{
	  fragP->fr_subtype = 3;
	  growth = 2;
	}
    }
  else if (fragP->fr_subtype == 3)
    {
      if ((address & 3) == 0)
	{
	  fragP->fr_subtype = 2;
	  growth = -2;
	}
    }
  else
    {
      growth = relax_frag (segment, fragP, stretch);

      /* Long jump on odd halfword boundary?  */
      if (fragP->fr_subtype == 2 && (address & 3) != 0)
	{
	  fragP->fr_subtype = 3;
	  growth += 2;
	}
    }

  return growth;
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
md_estimate_size_before_relax (fragP, segment)
     fragS * fragP;
     segT    segment;
{
  /* The only thing we have to handle here are symbols outside of the
     current segment.  They may be undefined or in a different segment in
     which case linker scripts may place them anywhere.
     However, we can't finish the fragment here and emit the reloc as insn
     alignment requirements may move the insn about.  */

  if (S_GET_SEGMENT (fragP->fr_symbol) != segment)
    {
      /* The symbol is undefined in this segment.
	 Change the relaxation subtype to the max allowable and leave
	 all further handling to md_convert_frag.  */
      fragP->fr_subtype = 2;

      {
	const CGEN_INSN * insn;
	int               i;

	/* Update the recorded insn.
	   Fortunately we don't have to look very far.
	   FIXME: Change this to record in the instruction the next higher
	   relaxable insn to use.  */
	for (i = 0, insn = fragP->fr_cgen.insn; i < 4; i++, insn++)
	  {
	    if ((strcmp (CGEN_INSN_MNEMONIC (insn),
			 CGEN_INSN_MNEMONIC (fragP->fr_cgen.insn))
		 == 0)
		&& CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_RELAXED))
	      break;
	  }
	if (i == 4)
	  abort ();

	fragP->fr_cgen.insn = insn;
	return 2;
      }
    }

  return md_relax_table[fragP->fr_subtype].rlx_length;
}

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (abfd, sec, fragP)
  bfd *   abfd ATTRIBUTE_UNUSED;
  segT    sec  ATTRIBUTE_UNUSED;
  fragS * fragP ATTRIBUTE_UNUSED;
{
  /* FIXME */
}


/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixP, sec)
     fixS * fixP;
     segT   sec;
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* The symbol is undefined (or is defined but not in this section).
	 Let the linker figure it out.  */
      return 0;
    }

  return (fixP->fx_frag->fr_address + fixP->fx_where) & ~1;
}


/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (insn, operand, fixP)
     const CGEN_INSN *    insn ATTRIBUTE_UNUSED;
     const CGEN_OPERAND * operand;
     fixS *               fixP;
{
  bfd_reloc_code_real_type type;

  switch (operand->type)
    {
    case OPENRISC_OPERAND_ABS_26:
      fixP->fx_pcrel = 0;
      type = BFD_RELOC_OPENRISC_ABS_26;
      goto emit;
    case OPENRISC_OPERAND_DISP_26:
      fixP->fx_pcrel = 1;
      type = BFD_RELOC_OPENRISC_REL_26;
      goto emit;

    case OPENRISC_OPERAND_HI16:
      type = BFD_RELOC_HI16;
      goto emit;

    case OPENRISC_OPERAND_LO16:
      type = BFD_RELOC_LO16;
      goto emit;

    emit:
      return type;

    default : /* avoid -Wall warning */
      break;
    }

  return BFD_RELOC_NONE;
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (buf, val, n)
     char * buf;
     valueT val;
     int    n;
{
  number_to_chars_bigendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.
*/

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char   type;
     char * litP;
     int *  sizeP;
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
openrisc_fix_adjustable (fixP)
   fixS * fixP;
{
  /* We need the symbol name for the VTABLE entries */
  if (fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  return 1;
}



