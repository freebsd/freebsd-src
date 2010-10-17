/* tc-ip2k.c -- Assembler for the Scenix IP2xxx.
   Copyright (C) 2000, 2002, 2003 Free Software Foundation.

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
#include "opcodes/ip2k-desc.h"
#include "opcodes/ip2k-opc.h"
#include "cgen.h"
#include "elf/common.h"
#include "elf/ip2k.h"
#include "libbfd.h"

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
ip2k_insn;

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = ""; 
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

static void ip2k_elf_section_text (int);
static void ip2k_elf_section_rtn (int);

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
    { "text",   ip2k_elf_section_text,  0 },
    { "sect",   ip2k_elf_section_rtn,   0 },
    { NULL, 	NULL,			0 }
};



#define OPTION_CPU_IP2022    (OPTION_MD_BASE)
#define OPTION_CPU_IP2022EXT (OPTION_MD_BASE+1)

struct option md_longopts[] = 
{
  { "mip2022",     no_argument, NULL, OPTION_CPU_IP2022 },
  { "mip2022ext",  no_argument, NULL, OPTION_CPU_IP2022EXT },
  { NULL,           no_argument, NULL, 0 },
};
size_t md_longopts_size = sizeof (md_longopts);

const char * md_shortopts = "";

/* Flag to detect when switching to code section where insn alignment is
   implied.  */
static int force_code_align = 0;

/* Mach selected from command line.  */
int ip2k_mach = 0;
unsigned ip2k_mach_bitmask = 0;

int
md_parse_option (c, arg)
    int c ATTRIBUTE_UNUSED;
    char * arg ATTRIBUTE_UNUSED;
{
  switch (c)
    {
    case OPTION_CPU_IP2022:
      ip2k_mach = bfd_mach_ip2022;
      ip2k_mach_bitmask = 1 << MACH_IP2022;
      break;

    case OPTION_CPU_IP2022EXT:
      ip2k_mach = bfd_mach_ip2022ext;
      ip2k_mach_bitmask = 1 << MACH_IP2022EXT;
      break;

    default:
      return 0;
    }

  return 1;
}


void
md_show_usage (stream)
    FILE * stream;
{
  fprintf (stream, _("IP2K specific command line options:\n"));
  fprintf (stream, _("  -mip2022               restrict to IP2022 insns \n"));
  fprintf (stream, _("  -mip2022ext            permit extended IP2022 insn\n"));
}


void
md_begin ()
{
  /* Initialize the `cgen' interface.  */
  
  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = ip2k_cgen_cpu_open (CGEN_CPU_OPEN_MACHS,
					  ip2k_mach_bitmask,
					  CGEN_CPU_OPEN_ENDIAN,
					  CGEN_ENDIAN_BIG,
					  CGEN_CPU_OPEN_END);
  ip2k_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Set the machine type.  */
  bfd_default_set_arch_mach (stdoutput, bfd_arch_ip2k, ip2k_mach);
}


void
md_assemble (str)
     char * str;
{
  ip2k_insn insn;
  char * errmsg;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = ip2k_cgen_assemble_insn
      (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad ("%s", errmsg);
      return;
    }

  /* Check for special relocation required by SKIP instructions.  */
  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_SKIPA))
    /* Unconditional skip has a 1-bit relocation of the current pc, so
       that we emit either sb pcl.0 or snb pcl.0 depending on whether
       the PCL (pc + 2) >> 1 is odd or even.  */
    {
      enum cgen_parse_operand_result result_type;
      long value;
      const char *curpc_plus_2 = ".+2";
      const char *err;

      err = cgen_parse_address (gas_cgen_cpu_desc, & curpc_plus_2,
				IP2K_OPERAND_ADDR16CJP,
				BFD_RELOC_IP2K_PC_SKIP,
				& result_type, & value);
      if (err)
	{
	  as_bad ("%s", err);
	  return;
	}
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, NULL);
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

int
md_estimate_size_before_relax (fragP, segment)
     fragS * fragP ATTRIBUTE_UNUSED;
     segT    segment ATTRIBUTE_UNUSED;
{
  as_fatal (_("md_estimate_size_before_relax\n"));
  return 1;
} 


/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (abfd, sec, fragP)
    bfd   * abfd  ATTRIBUTE_UNUSED;
    segT    sec   ATTRIBUTE_UNUSED;
    fragS * fragP ATTRIBUTE_UNUSED;
{
}


/* Functions concerning relocs.  */

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  as_fatal (_("md_pcrel_from\n"));

  /* Return the address of the delay slot. */
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}


/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (insn, operand, fixP)
     const CGEN_INSN *    insn     ATTRIBUTE_UNUSED;
     const CGEN_OPERAND * operand;
     fixS *               fixP     ATTRIBUTE_UNUSED;
{
  bfd_reloc_code_real_type result;

  result = BFD_RELOC_NONE;

  switch (operand->type)
    {
    case IP2K_OPERAND_FR:
    case IP2K_OPERAND_ADDR16L:
    case IP2K_OPERAND_ADDR16H:
    case IP2K_OPERAND_LIT8:
      /* These may have been processed at parse time.  */
      if (fixP->fx_cgen.opinfo != 0)
	result = fixP->fx_cgen.opinfo;
      fixP->fx_no_overflow = 1;
      break;

    case IP2K_OPERAND_ADDR16CJP:
      result = fixP->fx_cgen.opinfo;
      if (result == 0 || result == BFD_RELOC_NONE)
	result = BFD_RELOC_IP2K_ADDR16CJP;
      fixP->fx_no_overflow = 1;
      break;

    case IP2K_OPERAND_ADDR16P:
      result = BFD_RELOC_IP2K_PAGE3;
      fixP->fx_no_overflow = 1;
      break;

    default:
      result = BFD_RELOC_NONE;
      break;
    }

  return result;
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
   emitted is stored in *sizeP .  An error message is returned, or NULL on
   OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c  */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char   type;
     char * litP;
     int *  sizeP;
{
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  LITTLENUM_TYPE  *wordP;
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

  /* This loops outputs the LITTLENUMs in REVERSE order; in accord with
     the ip2k endianness.  */
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
     
  return 0;
}


/* See whether we need to force a relocation into the output file.
   Force most of them, since the linker's bfd relocation engine
   understands range limits better than gas' cgen fixup engine.
   Consider the case of a fixup intermediate value being larger than
   the instruction it will be eventually encoded within.  */

int
ip2k_force_relocation (fix)
     fixS * fix;
{
  switch (fix->fx_r_type)
    {
    case BFD_RELOC_IP2K_FR9:
    case BFD_RELOC_IP2K_FR_OFFSET:
    case BFD_RELOC_IP2K_BANK:
    case BFD_RELOC_IP2K_ADDR16CJP:
    case BFD_RELOC_IP2K_PAGE3:
    case BFD_RELOC_IP2K_LO8DATA:
    case BFD_RELOC_IP2K_HI8DATA:
    case BFD_RELOC_IP2K_EX8DATA:
    case BFD_RELOC_IP2K_LO8INSN:
    case BFD_RELOC_IP2K_HI8INSN:
    case BFD_RELOC_IP2K_PC_SKIP:
    case BFD_RELOC_IP2K_TEXT:
      return 1;

    case BFD_RELOC_16:
      if (fix->fx_subsy && S_IS_DEFINED (fix->fx_subsy)
	  && fix->fx_addsy && S_IS_DEFINED (fix->fx_addsy)
	  && (S_GET_SEGMENT (fix->fx_addsy)->flags & SEC_CODE))
	{
	  fix->fx_r_type = BFD_RELOC_IP2K_TEXT;
	  return 0;
	}
      break;

    default:
      break;
    }

  return generic_force_reloc (fix);
}

void
ip2k_apply_fix3 (fixP, valueP, seg)
     fixS *fixP;
     valueT *valueP;
     segT seg;
{
  if (fixP->fx_r_type == BFD_RELOC_IP2K_TEXT
      && ! fixP->fx_addsy
      && ! fixP->fx_subsy)
    {
      *valueP = ((int)(*valueP)) / 2;
      fixP->fx_r_type = BFD_RELOC_16;
    }
  else if (fixP->fx_r_type == BFD_RELOC_UNUSED + IP2K_OPERAND_FR)
    {
      /* Must be careful when we are fixing up an FR.  We could be
	 fixing up an offset to (SP) or (DP) in which case we don't
	 want to step on the top 2 bits of the FR operand.  The
	 gas_cgen_md_apply_fix3 doesn't know any better and overwrites
	 the entire operand.  We counter this by adding the bits
	 to the new value.  */
      char *where = fixP->fx_frag->fr_literal + fixP->fx_where;

      /* Canonical name, since used a lot.  */
      CGEN_CPU_DESC cd = gas_cgen_cpu_desc;
      CGEN_INSN_INT insn_value
	= cgen_get_insn_value (cd, where,
			       CGEN_INSN_BITSIZE (fixP->fx_cgen.insn));
      /* Preserve (DP) or (SP) specification.  */
      *valueP += (insn_value & 0x180);
    }

  gas_cgen_md_apply_fix3 (fixP, valueP, seg);
}

int
ip2k_elf_section_flags (flags, attr, type)
     int flags;
     int attr ATTRIBUTE_UNUSED;
     int type ATTRIBUTE_UNUSED;
{
  /* This is used to detect when the section changes to an executable section.
     This function is called by the elf section processing.  When we note an
     executable section specifier we set an internal flag to denote when
     word alignment should be forced.  */
  if (flags & SEC_CODE)
    force_code_align = 1;
 
  return flags;
}

static void
ip2k_elf_section_rtn (int i)
{
  obj_elf_section(i);

  if (force_code_align)
    {
      /* The s_align_ptwo function expects that we are just after a .align
	 directive and it will either try and read the align value or stop
	 if end of line so we must fake it out so it thinks we are at the
	 end of the line.  */
      char *old_input_line_pointer = input_line_pointer;
      input_line_pointer = "\n";
      s_align_ptwo (1);
      force_code_align = 0;
      /* Restore.  */
      input_line_pointer = old_input_line_pointer;
    }
}

static void
ip2k_elf_section_text (int i)
{
  char *old_input_line_pointer;
  obj_elf_text(i);

  /* the s_align_ptwo function expects that we are just after a .align
     directive and it will either try and read the align value or stop if
     end of line so we must fake it out so it thinks we are at the end of
     the line.  */
  old_input_line_pointer = input_line_pointer;
  input_line_pointer = "\n";
  s_align_ptwo (1);
  force_code_align = 0;
  /* Restore.  */
  input_line_pointer = old_input_line_pointer;
}
