/* tc-xc16x.c -- Assembler for the Infineon XC16X.
   Copyright 2006 Free Software Foundation, Inc.
   Contributed by KPIT Cummins Infosystems 

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
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */


#include <stdio.h>
#include "as.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "symcat.h"
#include "opcodes/xc16x-desc.h"
#include "opcodes/xc16x-opc.h"
#include "cgen.h"
#include "bfd.h"
#include "dwarf2dbg.h"


#ifdef OBJ_ELF
#include "elf/xc16x.h"
#endif

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
  unsigned char buffer [CGEN_MAX_INSN_SIZE];
#define INSN_VALUE(buf) (buf)
#endif
  char *		addr;
  fragS *		frag;
  int  			num_fixups;
  fixS *		fixups [GAS_CGEN_MAX_FIXUPS];
  int  			indices [MAX_OPERAND_INSTANCES];
}
xc16x_insn;

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = "";
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

#define XC16X_SHORTOPTS ""
const char * md_shortopts = XC16X_SHORTOPTS;

struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

static void
xc16xlmode (int arg ATTRIBUTE_UNUSED)
{
 if (stdoutput != NULL)
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_xc16x, bfd_mach_xc16xl))
    as_warn (_("could not set architecture and machine"));
}

static void
xc16xsmode (int arg ATTRIBUTE_UNUSED)
{
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_xc16x, bfd_mach_xc16xs))
    as_warn (_("could not set architecture and machine"));
}

static void
xc16xmode (int arg ATTRIBUTE_UNUSED)
{
  if (!bfd_set_arch_mach (stdoutput, bfd_arch_xc16x, bfd_mach_xc16x))
    as_warn (_("could not set architecture and machine"));
}

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "word",	cons,		2 },
  {"xc16xl",  xc16xlmode,  0},
  {"xc16xs", xc16xsmode, 0},
  {"xc16x",  xc16xmode,  0},
  { NULL, 	NULL, 		0 }
};

void
md_begin (void)
{
  /* Initialize the `cgen' interface.  */

  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = xc16x_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, 0,
					   CGEN_CPU_OPEN_ENDIAN,
					   CGEN_ENDIAN_LITTLE,
					   CGEN_CPU_OPEN_END);
  xc16x_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);
}

void
md_assemble (char *str)
{
  xc16x_insn insn;
  char *errmsg;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = xc16x_cgen_assemble_insn
    (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad (errmsg);
      return;
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, NULL);
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *insn ATTRIBUTE_UNUSED,
		      const CGEN_OPERAND *operand,
		      fixS *fixP)
{
  switch (operand->type)
    {
    case XC16X_OPERAND_REL:
      fixP->fx_where += 1;
      fixP->fx_pcrel = 1;
      return BFD_RELOC_8_PCREL;

    case XC16X_OPERAND_CADDR:
      fixP->fx_where += 2;
      return BFD_RELOC_16;

    case XC16X_OPERAND_UIMM7:
      fixP->fx_where += 1;
      fixP->fx_pcrel = 1;
      return BFD_RELOC_8_PCREL;

    case XC16X_OPERAND_UIMM16:
    case XC16X_OPERAND_MEMORY:
      fixP->fx_where += 2;
      return BFD_RELOC_16;

    case XC16X_OPERAND_UPOF16:
      fixP->fx_where += 2;
      return BFD_RELOC_XC16X_POF;

    case XC16X_OPERAND_UPAG16:
      fixP->fx_where += 2;
      return BFD_RELOC_XC16X_PAG;

    case XC16X_OPERAND_USEG8:
      fixP->fx_where += 1;
      return BFD_RELOC_XC16X_SEG;

    case XC16X_OPERAND_USEG16:
    case  XC16X_OPERAND_USOF16:
      fixP->fx_where += 2;
      return BFD_RELOC_XC16X_SOF;

    default : /* avoid -Wall warning */
      break;
    }

  fixP->fx_where += 2;
  return BFD_RELOC_XC16X_SOF;
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  number_to_chars_littleendian (buf, val, n);
}

void
md_show_usage (FILE * stream)
{
  fprintf (stream, _(" XC16X specific command line options:\n"));
}

int
md_parse_option (int c ATTRIBUTE_UNUSED,
		 char *arg ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

char *
md_atof (int type, char *litP, int *sizeP)
{
  int i;
  int prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *t;

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

      /* FIXME: Some targets allow other format chars for bigger sizes
         here.  */

    default:
      *sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);

   for (i = prec - 1; i >= 0; i--)
     {
       md_number_to_chars (litP, (valueT) words[i],
			   sizeof (LITTLENUM_TYPE));
       litP += sizeof (LITTLENUM_TYPE);
     }

  return NULL;
}

valueT
md_section_align (segT segment, valueT size)
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return NULL;
}

int
md_estimate_size_before_relax (fragS *fragP ATTRIBUTE_UNUSED,
			       segT segment_type ATTRIBUTE_UNUSED)
{
  printf (_("call tomd_estimate_size_before_relax \n"));
  abort ();
}


long
md_pcrel_from (fixS *fixP)
{
  long temp_val;
  temp_val=fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;

  return temp_val;
}

long
md_pcrel_from_section (fixS *fixP, segT sec)
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (! S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec
          || S_IS_EXTERNAL (fixP->fx_addsy)
          || S_IS_WEAK (fixP->fx_addsy)))
    {
      return 0;
    }

  return md_pcrel_from (fixP);
}

arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  if (fixp->fx_addsy && fixp->fx_subsy)
    {
      if ((S_GET_SEGMENT (fixp->fx_addsy) != S_GET_SEGMENT (fixp->fx_subsy))
	  || S_GET_SEGMENT (fixp->fx_addsy) == undefined_section)
	{
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			"Difference of symbols in different sections is not supported");
	  return NULL;
	}
    }

  rel = xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;
  rel->addend = fixp->fx_offset;

  r_type = fixp->fx_r_type;

#define DEBUG 0
#if DEBUG
  fprintf (stderr, "%s\n", bfd_get_reloc_code_name (r_type));
  fflush(stderr);
#endif

  rel->howto = bfd_reloc_type_lookup (stdoutput, r_type);
   if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Cannot represent relocation type %s"),
		    bfd_get_reloc_code_name (r_type));
      return NULL;
    }

  return rel;
}

void
md_apply_fix (fixS *fixP, valueT *valP, segT seg ATTRIBUTE_UNUSED)
{
  if(!strstr (seg->name,".debug"))
    {
      if (*valP < 128)
	*valP /= 2;
      if (*valP>268435455)
	{
	  *valP = *valP * (-1);
	  *valP /= 2;
	  *valP = 256 - (*valP);
	}
    }
  
  gas_cgen_md_apply_fix (fixP, valP, seg);
  return;
}

void
md_convert_frag (bfd *headers ATTRIBUTE_UNUSED,
		 segT seg ATTRIBUTE_UNUSED,
		 fragS *fragP ATTRIBUTE_UNUSED)
{
  printf (_("call to md_convert_frag \n"));
  abort ();
}


