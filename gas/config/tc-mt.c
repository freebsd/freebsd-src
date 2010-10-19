/* tc-mt.c -- Assembler for the Morpho Technologies mt .
   Copyright (C) 2005 Free Software Foundation.

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
#include "dwarf2dbg.h"
#include "subsegs.h"     
#include "symcat.h"
#include "opcodes/mt-desc.h"
#include "opcodes/mt-opc.h"
#include "cgen.h"
#include "elf/common.h"
#include "elf/mt.h"
#include "libbfd.h"

/* Structure to hold all of the different components
   describing an individual instruction.  */
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
mt_insn;


const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = ""; 
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
    { "word",   cons,                   4 }, 
    { NULL, 	NULL,			0 }
};



static int no_scheduling_restrictions = 0;

struct option md_longopts[] = 
{
#define OPTION_NO_SCHED_REST	(OPTION_MD_BASE)
  { "nosched",	   no_argument, NULL, OPTION_NO_SCHED_REST },
#define OPTION_MARCH		(OPTION_MD_BASE + 1)
  { "march", required_argument, NULL, OPTION_MARCH},
  { NULL,	   no_argument, NULL, 0 },
};
size_t md_longopts_size = sizeof (md_longopts);

const char * md_shortopts = "";

/* Mach selected from command line.  */
static int mt_mach = bfd_mach_ms1;
static unsigned mt_mach_bitmask = 1 << MACH_MS1;

/* Flags to set in the elf header */
static flagword mt_flags = EF_MT_CPU_MRISC;

/* The architecture to use.  */
enum mt_architectures
  {
    ms1_64_001,
    ms1_16_002,
    ms1_16_003,
    ms2
  };

/* MT architecture we are using for this output file.  */
static enum mt_architectures mt_arch = ms1_16_002;

int
md_parse_option (int c ATTRIBUTE_UNUSED, char * arg)
{
  switch (c)
    {
    case OPTION_MARCH:
      if (strcmp (arg, "ms1-64-001") == 0)
 	{
 	  mt_flags = (mt_flags & ~EF_MT_CPU_MASK) | EF_MT_CPU_MRISC;
 	  mt_mach = bfd_mach_ms1;
 	  mt_mach_bitmask = 1 << MACH_MS1;
 	  mt_arch = ms1_64_001;
 	}
      else if (strcmp (arg, "ms1-16-002") == 0)
 	{
 	  mt_flags = (mt_flags & ~EF_MT_CPU_MASK) | EF_MT_CPU_MRISC;
 	  mt_mach = bfd_mach_ms1;
 	  mt_mach_bitmask = 1 << MACH_MS1;
 	  mt_arch = ms1_16_002;
 	}
      else if (strcmp (arg, "ms1-16-003") == 0)
 	{
 	  mt_flags = (mt_flags & ~EF_MT_CPU_MASK) | EF_MT_CPU_MRISC2;
 	  mt_mach = bfd_mach_mrisc2;
 	  mt_mach_bitmask = 1 << MACH_MS1_003;
 	  mt_arch = ms1_16_003;
 	}
      else if (strcmp (arg, "ms2") == 0)
 	{
 	  mt_flags = (mt_flags & ~EF_MT_CPU_MASK) | EF_MT_CPU_MS2;
 	  mt_mach = bfd_mach_mrisc2;
 	  mt_mach_bitmask = 1 << MACH_MS2;
 	  mt_arch = ms2;
 	}
    case OPTION_NO_SCHED_REST:
      no_scheduling_restrictions = 1;
      break;
    default:
      return 0;
    }

  return 1;
}


void
md_show_usage (FILE * stream)
{
  fprintf (stream, _("MT specific command line options:\n"));
  fprintf (stream, _("  -march=ms1-64-001         allow ms1-64-001 instructions\n"));
  fprintf (stream, _("  -march=ms1-16-002         allow ms1-16-002 instructions (default)\n"));
  fprintf (stream, _("  -march=ms1-16-003         allow ms1-16-003 instructions\n"));
  fprintf (stream, _("  -march=ms2                allow ms2 instructions \n"));
  fprintf (stream, _("  -nosched                  disable scheduling restrictions\n"));
}


void
md_begin (void)
{
  /* Initialize the `cgen' interface.  */
  
  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = mt_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, mt_mach_bitmask,
					CGEN_CPU_OPEN_ENDIAN,
					CGEN_ENDIAN_BIG,
					CGEN_CPU_OPEN_END);
  mt_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Set the ELF flags if desired. */
  if (mt_flags)
    bfd_set_private_flags (stdoutput, mt_flags);

  /* Set the machine type.  */
  bfd_default_set_arch_mach (stdoutput, bfd_arch_mt, mt_mach);
}

void
md_assemble (char * str)
{
  static long delayed_load_register = 0;
  static long prev_delayed_load_register = 0;
  static int last_insn_had_delay_slot = 0;
  static int last_insn_in_noncond_delay_slot = 0;
  static int last_insn_has_load_delay = 0;
  static int last_insn_was_memory_access = 0;
  static int last_insn_was_io_insn = 0;
  static int last_insn_was_arithmetic_or_logic = 0;
  static int last_insn_was_branch_insn = 0;
  static int last_insn_was_conditional_branch_insn = 0;

  mt_insn insn;
  char * errmsg;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  insn.insn = mt_cgen_assemble_insn
      (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, & errmsg);

  if (!insn.insn)
    {
      as_bad ("%s", errmsg);
      return;
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, NULL);


  /* Handle Scheduling Restrictions.  */
  if (!no_scheduling_restrictions)
    {
      /* Detect consecutive Memory Accesses.  */
      if (last_insn_was_memory_access
	  && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_MEMORY_ACCESS)
	  && mt_mach == ms1_64_001)
	as_warn (_("instruction %s may not follow another memory access instruction."),
		 CGEN_INSN_NAME (insn.insn));

      /* Detect consecutive I/O Instructions.  */
      else if (last_insn_was_io_insn
	       && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_IO_INSN))
	as_warn (_("instruction %s may not follow another I/O instruction."),
		 CGEN_INSN_NAME (insn.insn));

      /* Detect consecutive branch instructions.  */
      else if (last_insn_was_branch_insn
	       && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_BR_INSN))
	as_warn (_("%s may not occupy the delay slot of another branch insn."),
		 CGEN_INSN_NAME (insn.insn));

      /* Detect data dependencies on delayed loads: memory and input insns.  */
      if (last_insn_has_load_delay && delayed_load_register)
	{
	  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR1)
	      && insn.fields.f_sr1 == delayed_load_register)
	    as_warn (_("operand references R%ld of previous load."),
		     insn.fields.f_sr1);

	  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR2)
	      && insn.fields.f_sr2 == delayed_load_register)
	    as_warn (_("operand references R%ld of previous load."),
		     insn.fields.f_sr2);
	}

      /* Detect JAL/RETI hazard */
      if (mt_mach == ms2
	  && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_JAL_HAZARD))
	{
	  if ((CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR1)
	       && insn.fields.f_sr1 == delayed_load_register)
	      || (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR2)
		  && insn.fields.f_sr2 == delayed_load_register))
	    as_warn (_("operand references R%ld of previous instrutcion."),
		     delayed_load_register);
	  else if ((CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR1)
		    && insn.fields.f_sr1 == prev_delayed_load_register)
		   || (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR2)
		       && insn.fields.f_sr2 == prev_delayed_load_register))
	    as_warn (_("operand references R%ld of instructcion before previous."),
		     prev_delayed_load_register);
	}
      
      /* Detect data dependency between conditional branch instruction
         and an immediately preceding arithmetic or logical instruction.  */
      if (last_insn_was_arithmetic_or_logic
	  && !last_insn_in_noncond_delay_slot
	  && (delayed_load_register != 0)
	  && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_BR_INSN)
	  && mt_arch == ms1_64_001)
	{
	  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR1)
	      && insn.fields.f_sr1 == delayed_load_register)
	    as_warn (_("conditional branch or jal insn's operand references R%ld of previous arithmetic or logic insn."),
		     insn.fields.f_sr1);

	  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR2)
	      && insn.fields.f_sr2 == delayed_load_register)
	    as_warn (_("conditional branch or jal insn's operand references R%ld of previous arithmetic or logic insn."),
		     insn.fields.f_sr2);
	}
    }

  /* Keep track of details of this insn for processing next insn.  */
  last_insn_in_noncond_delay_slot = last_insn_was_branch_insn
    && !last_insn_was_conditional_branch_insn;

  last_insn_had_delay_slot =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_DELAY_SLOT);

  last_insn_has_load_delay =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_LOAD_DELAY);

  last_insn_was_memory_access =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_MEMORY_ACCESS);

  last_insn_was_io_insn =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_IO_INSN);

  last_insn_was_arithmetic_or_logic =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_AL_INSN);

  last_insn_was_branch_insn =
    CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_BR_INSN);
  
  last_insn_was_conditional_branch_insn =
  CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_BR_INSN)
    && CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRSR2);
  
  prev_delayed_load_register = delayed_load_register;
  
  if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRDR))
     delayed_load_register = insn.fields.f_dr; 
  else if (CGEN_INSN_ATTR_VALUE (insn.insn, CGEN_INSN_USES_FRDRRR))
     delayed_load_register = insn.fields.f_drrr; 
  else  /* Insns has no destination register.  */
     delayed_load_register = 0; 

  /* Generate dwarf2 line numbers.  */
  dwarf2_emit_insn (4); 
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
    return NULL;
}

int
md_estimate_size_before_relax (fragS * fragP ATTRIBUTE_UNUSED,
			       segT    segment ATTRIBUTE_UNUSED)
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
md_convert_frag (bfd   * abfd  ATTRIBUTE_UNUSED,
		 segT    sec   ATTRIBUTE_UNUSED,
		 fragS * fragP ATTRIBUTE_UNUSED)
{
}


/* Functions concerning relocs.  */

long
md_pcrel_from_section (fixS *fixP, segT sec)
{
  if (fixP->fx_addsy != (symbolS *) NULL
      && (!S_IS_DEFINED (fixP->fx_addsy)
	  || S_GET_SEGMENT (fixP->fx_addsy) != sec))
    /* The symbol is undefined (or is defined but not in this section).
       Let the linker figure it out.  */
    return 0;

  /* Return the address of the opcode - cgen adjusts for opcode size
     itself, to be consistent with the disassembler, which must do
     so.  */
  return fixP->fx_where + fixP->fx_frag->fr_address;
}


/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (const CGEN_INSN *    insn     ATTRIBUTE_UNUSED,
		      const CGEN_OPERAND * operand,
		      fixS *               fixP     ATTRIBUTE_UNUSED)
{
  bfd_reloc_code_real_type result;

  result = BFD_RELOC_NONE;

  switch (operand->type)
    {
    case MT_OPERAND_IMM16O:
      result = BFD_RELOC_16_PCREL;
      fixP->fx_pcrel = 1;
      /* fixP->fx_no_overflow = 1; */
      break;
    case MT_OPERAND_IMM16:
    case MT_OPERAND_IMM16Z:
      /* These may have been processed at parse time.  */
      if (fixP->fx_cgen.opinfo != 0)
        result = fixP->fx_cgen.opinfo;
      fixP->fx_no_overflow = 1;
      break;
    case MT_OPERAND_LOOPSIZE:
      result = BFD_RELOC_MT_PCINSN8;
      fixP->fx_pcrel = 1;
      /* Adjust for the delay slot, which is not part of the loop  */
      fixP->fx_offset -= 8;
      break;
    default:
      result = BFD_RELOC_NONE;
      break;
    }

  return result;
}

/* Write a value out to the object file, using the appropriate endianness.  */

void
md_number_to_chars (char * buf, valueT val, int n)
{
  number_to_chars_bigendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.  */

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char   type;
     char * litP;
     int *  sizeP;
{
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  LITTLENUM_TYPE * wordP;
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

  /* This loops outputs the LITTLENUMs in REVERSE order;
     in accord with the mt endianness.  */
  for (wordP = words; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP++), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
     
  return 0;
}

/* See whether we need to force a relocation into the output file.  */

int
mt_force_relocation (fixS * fixp ATTRIBUTE_UNUSED)
{
  return 0;
}

void
mt_apply_fix (fixS *fixP, valueT *valueP, segT seg)
{
  if ((fixP->fx_pcrel != 0) && (fixP->fx_r_type == BFD_RELOC_32))
    fixP->fx_r_type = BFD_RELOC_32_PCREL;

  gas_cgen_md_apply_fix (fixP, valueP, seg);
}

bfd_boolean
mt_fix_adjustable (fixS * fixP)
{
  bfd_reloc_code_real_type reloc_type;

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      const CGEN_INSN *insn = NULL;
      int opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      const CGEN_OPERAND *operand;

      operand = cgen_operand_lookup_by_num(gas_cgen_cpu_desc, opindex);
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
  
  return 1;
}
