/* BFD back-end for MAXQ COFF binaries.
   Copyright 2004    Free Software Foundation, Inc.

   Contributed by Vineet Sharma (vineets@noida.hcltech.com) Inderpreet S.
   (inderpreetb@noida.hcltech.com)

   HCL Technologies Ltd.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free 
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc., 
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/maxq.h"
#include "coff/internal.h"
#include "libcoff.h"
#include "libiberty.h"

#ifndef MAXQ20
#define MAXQ20 1
#endif

#define RTYPE2HOWTO(cache_ptr, dst)                                     \
  ((cache_ptr)->howto =                                                 \
   ((dst)->r_type < 48 							\
    ? howto_table + (((dst)->r_type==47) ? 6: ((dst)->r_type))		\
    : NULL))

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)

/* Code to swap in the reloc offset.  */
#define SWAP_IN_RELOC_OFFSET    H_GET_16
#define SWAP_OUT_RELOC_OFFSET   H_PUT_16

#define SHORT_JUMP 	        BFD_RELOC_16_PCREL_S2
#define LONG_JUMP 	        BFD_RELOC_14
#define ABSOLUTE_ADDR_FOR_DATA  BFD_RELOC_24

/* checks the range of short jump -127 to 128 */
#define IS_SJUMP_RANGE(x) ((x > -128) && (x < 129))
#define HIGH_WORD_MASK    0xff00
#define LOW_WORD_MASK     0x00ff

static long
get_symbol_value (asymbol *symbol)
{
  long relocation = 0;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value +
      symbol->section->output_section->vma + symbol->section->output_offset;

  return relocation;
}

/* This function performs all the maxq relocations.
   FIXME:  The handling of the addend in the 'BFD_*'
   relocations types.  */

static bfd_reloc_status_type
coff_maxq20_reloc (bfd *      abfd,
		   arelent *  reloc_entry,
		   asymbol *  symbol_in,
		   void *     data,
		   asection * input_section ATTRIBUTE_UNUSED,
		   bfd *      output_bfd    ATTRIBUTE_UNUSED,
		   char **    error_message ATTRIBUTE_UNUSED)
{
  reloc_howto_type *howto = NULL;
  unsigned char *addr = NULL;
  unsigned long x = 0;
  long call_addr = 0;
  short addend = 0;
  long diff = 0;

  /* If this is an undefined symbol, return error.  */
  if (symbol_in->section == &bfd_und_section
      && (symbol_in->flags & BSF_WEAK) == 0)
    return bfd_reloc_continue;

  if (data && reloc_entry)
    {
      howto = reloc_entry->howto;
      addr = (unsigned char *) data + reloc_entry->address;
      call_addr = call_addr - call_addr;
      call_addr = get_symbol_value (symbol_in);

      /* Over here the value val stores the 8 bit/16 bit value. We will put a 
         check if we are moving a 16 bit immediate value into an 8 bit
         register. In that case we will generate a Upper bytes into PFX[0]
         and move the lower 8 bits as SRC.  */

      switch (reloc_entry->howto->type)
	{
	  /* BFD_RELOC_16_PCREL_S2 47 Handles all the relative jumps and
	     calls Note: Every relative jump or call is in words.  */
	case SHORT_JUMP:
	  /* Handle any addend.  */
	  addend = reloc_entry->addend;

	  if (addend > call_addr || addend > 0)
	    call_addr = symbol_in->section->output_section->vma + addend;
	  else if (addend < call_addr && addend > 0)
	    call_addr = call_addr + addend;
	  else if (addend < 0)
	    call_addr = call_addr + addend;

	  diff = ((call_addr << 1) - (reloc_entry->address << 1));

	  if (!IS_SJUMP_RANGE (diff))
	    {
	      bfd_perror (_("Can't Make it a Short Jump"));
	      return bfd_reloc_outofrange;
	    }

	  x = bfd_get_16 (abfd, addr);

	  x = x & LOW_WORD_MASK;
	  x = x | (diff << 8);
	  bfd_put_16 (abfd, (bfd_vma) x, addr);

	  return bfd_reloc_ok;

	case ABSOLUTE_ADDR_FOR_DATA:
	case LONG_JUMP:
	  /* BFD_RELOC_14 Handles intersegment or long jumps which might be
	     from code to code or code to data segment jumps. Note: When this 
	     fucntion is called by gas the section flags somehow do not
	     contain the info about the section type(CODE or DATA). Thus the
	     user needs to evoke the linker after assembling the files
	     because the Code-Code relocs are word aligned but code-data are
	     byte aligned.  */
	  addend = (reloc_entry->addend - reloc_entry->addend);

	  /* Handle any addend.  */
	  addend = reloc_entry->addend;

	  /* For relocation involving multiple file added becomes zero thus
	     this fails - check for zero added. In another case when we try
	     to add a stub to a file the addend shows the offset from the
	     start od this file.  */
	  addend = 0;

	  if (!bfd_is_com_section (symbol_in->section) &&
	      ((symbol_in->flags & BSF_OLD_COMMON) == 0))
	    {
	      if (reloc_entry->addend > symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;

	      if ((reloc_entry->addend < symbol_in->value)
		  && (reloc_entry->addend != 0))
		addend = reloc_entry->addend - symbol_in->value;

	      if (reloc_entry->addend == symbol_in->value)
		addend = 0;
	    }

	  if (bfd_is_com_section (symbol_in->section) ||
	      ((symbol_in->flags & BSF_OLD_COMMON) != 0))
	    addend = reloc_entry->addend;

	  if (addend < 0
	      &&  (call_addr < (long) (addend * (-1))))
	    addend = 0;

	  call_addr += addend;

	  /* FIXME: This check does not work well with the assembler,
	     linker needs to be run always.  */
	  if ((symbol_in->section->flags & SEC_CODE) == SEC_CODE)
	    {
	      /* Convert it into words.  */
	      call_addr = call_addr >> 1;

	      if (call_addr > 0xFFFF)	/* Intersegment Jump.  */
		{
		  bfd_perror (_("Exceeds Long Jump Range"));
		  return bfd_reloc_outofrange;
		}
	    }
	  else
	    {
	      /* case ABSOLUTE_ADDR_FOR_DATA : Resolves any code-data
		 segemnt relocs. These are NOT word aligned.  */

	      if (call_addr > 0xFFFF)	/* Intersegment Jump.  */
		{
		  bfd_perror (_("Absolute address Exceeds 16 bit Range"));
		  return bfd_reloc_outofrange;
		}
	    }

	  x = bfd_get_32 (abfd, addr);

	  x = (x & 0xFF00FF00);
	  x = (x | ((call_addr & HIGH_WORD_MASK) >> 8));
	  x = (x | (call_addr & LOW_WORD_MASK) << 16);

	  bfd_put_32 (abfd, (bfd_vma) x, addr);
	  return bfd_reloc_ok;

	case BFD_RELOC_8:
	  addend = (reloc_entry->addend - reloc_entry->addend);

	  if (!bfd_is_com_section (symbol_in->section) &&
	      ((symbol_in->flags & BSF_OLD_COMMON) == 0))
	    {
	      if (reloc_entry->addend > symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;
	      if (reloc_entry->addend < symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;
	      if (reloc_entry->addend == symbol_in->value)
		addend = 0;
	    }

	  if (bfd_is_com_section (symbol_in->section) ||
	      ((symbol_in->flags & BSF_OLD_COMMON) != 0))
	    addend = reloc_entry->addend;

	  if (addend < 0
	      && (call_addr < (long) (addend * (-1))))
	    addend = 0;

	  if (call_addr + addend > 0xFF)
	    {
	      bfd_perror (_("Absolute address Exceeds 8 bit Range"));
	      return bfd_reloc_outofrange;
	    }

	  x = bfd_get_8 (abfd, addr);
	  x = x & 0x00;
	  x = x | (call_addr + addend);

	  bfd_put_8 (abfd, (bfd_vma) x, addr);
	  return bfd_reloc_ok;

	case BFD_RELOC_16:
	  addend = (reloc_entry->addend - reloc_entry->addend);
	  if (!bfd_is_com_section (symbol_in->section) &&
	      ((symbol_in->flags & BSF_OLD_COMMON) == 0))
	    {
	      if (reloc_entry->addend > symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;

	      if (reloc_entry->addend < symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;

	      if (reloc_entry->addend == symbol_in->value)
		addend = 0;
	    }

	  if (bfd_is_com_section (symbol_in->section) ||
	      ((symbol_in->flags & BSF_OLD_COMMON) != 0))
	    addend = reloc_entry->addend;

	  if (addend < 0
	      && (call_addr < (long) (addend * (-1))))
	    addend = 0;

	  if ((call_addr + addend) > 0xFFFF)
	    {
	      bfd_perror (_("Absolute address Exceeds 16 bit Range"));
	      return bfd_reloc_outofrange;
	    }
	  else
	    {
	      unsigned short val = (call_addr + addend);

	      x = bfd_get_16 (abfd, addr);

	      /* LE */
	      x = (x & 0x0000);	/* Flush garbage value.  */
	      x = val;
	      if ((symbol_in->section->flags & SEC_CODE) == SEC_CODE)
		x = x >> 1;	/* Convert it into words.  */
	    }

	  bfd_put_16 (abfd, (bfd_vma) x, addr);
	  return bfd_reloc_ok;

	case BFD_RELOC_32:
	  addend = (reloc_entry->addend - reloc_entry->addend);

	  if (!bfd_is_com_section (symbol_in->section) &&
	      ((symbol_in->flags & BSF_OLD_COMMON) == 0))
	    {
	      if (reloc_entry->addend > symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;
	      if (reloc_entry->addend < symbol_in->value)
		addend = reloc_entry->addend - symbol_in->value;
	      if (reloc_entry->addend == symbol_in->value)
		addend = 0;
	    }

	  if (bfd_is_com_section (symbol_in->section) ||
	      ((symbol_in->flags & BSF_OLD_COMMON) != 0))
	    addend = reloc_entry->addend;

	  if (addend < 0
	      && (call_addr < (long) (addend * (-1))))
	    addend = 0;

	  if ((call_addr + addend) < 0)
	    {
	      bfd_perror ("Absolute address Exceeds 32 bit Range");
	      return bfd_reloc_outofrange;
	    }

	  x = bfd_get_32 (abfd, addr);
	  x = (x & 0x0000);	/* Flush garbage value.  */
	  x = call_addr + addend;
	  if ((symbol_in->section->flags & SEC_CODE) == SEC_CODE)
	    x = x >> 1;	/* Convert it into words.  */

	  bfd_put_32 (abfd, (bfd_vma) x, addr);
	  return bfd_reloc_ok;

	default:
	  bfd_perror (_("Unrecognized Reloc Type"));
	  return bfd_reloc_notsupported;
	}
    }

  return bfd_reloc_notsupported;
}

static reloc_howto_type howto_table[] =
{
  EMPTY_HOWTO (0),
  EMPTY_HOWTO (1),
  {
   BFD_RELOC_32, 0, 1, 8, FALSE, 0, complain_overflow_bitfield,
   coff_maxq20_reloc, "32Bit", TRUE, 0x000000ff, 0x000000ff, TRUE
  },
  {
   SHORT_JUMP, 0, 1, 8, FALSE, 0, complain_overflow_bitfield,
   coff_maxq20_reloc, "SHORT_JMP", TRUE, 0x000000ff, 0x000000ff, TRUE
  },
  {
   ABSOLUTE_ADDR_FOR_DATA, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
   coff_maxq20_reloc, "INTERSEGMENT_RELOC", TRUE, 0x00000000, 0x00000000,
   FALSE
  },
  {
   BFD_RELOC_16, 0, 1, 8, FALSE, 0, complain_overflow_bitfield,
   coff_maxq20_reloc, "16Bit", TRUE, 0x000000ff, 0x000000ff, TRUE
  },
  {
   LONG_JUMP, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
   coff_maxq20_reloc, "LONG_JUMP", TRUE, 0x00000000, 0x00000000, FALSE
  },
  {
   BFD_RELOC_8, 0, 1, 8, FALSE, 0, complain_overflow_bitfield,
   coff_maxq20_reloc, "8bit", TRUE, 0x000000ff, 0x000000ff, TRUE
  },
  EMPTY_HOWTO (8),
  EMPTY_HOWTO (9),
  EMPTY_HOWTO (10),
};

/* Map BFD reloc types to MAXQ COFF reloc types.  */

typedef struct maxq_reloc_map
{
  bfd_reloc_code_real_type  bfd_reloc_val;
  unsigned int              maxq_reloc_val;
  reloc_howto_type *        table;
}
reloc_map;

static const reloc_map maxq_reloc_map[] =
{
  {BFD_RELOC_16_PCREL_S2, SHORT_JUMP, howto_table},
  {BFD_RELOC_16,          LONG_JUMP,  howto_table},
};

static reloc_howto_type *
maxq_reloc_type_lookup (bfd * abfd ATTRIBUTE_UNUSED,
			bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0; i < ARRAY_SIZE (maxq_reloc_map); i++)
    {
      const reloc_map *entry;

      entry = maxq_reloc_map + i;

      switch (code)
	{
	  /* SHORT JUMP */
	case BFD_RELOC_16_PCREL_S2:
	  return howto_table + 3;

	  /* INTERSEGMENT JUMP */
	case BFD_RELOC_24:
	  return howto_table + 4;

	  /* BYTE RELOC */
	case BFD_RELOC_8:
	  return howto_table + 7;

	  /* WORD RELOC */
	case BFD_RELOC_16:
	  return howto_table + 5;

	  /* LONG RELOC */
	case BFD_RELOC_32:
	  return howto_table + 2;

	  /* LONG JUMP */
	case BFD_RELOC_14:
	  return howto_table + 6;

	default:
	  return NULL;
	}
    }

  return NULL;
}

#define coff_bfd_reloc_type_lookup maxq_reloc_type_lookup

/* Perform any necessary magic to the addend in a reloc entry.  */
#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;

#include "coffcode.h"

#ifndef TARGET_UNDERSCORE
#define TARGET_UNDERSCORE 1
#endif

#ifndef EXTRA_S_FLAGS
#define EXTRA_S_FLAGS 0
#endif

/* Forward declaration for use initialising alternative_target field.  */
CREATE_LITTLE_COFF_TARGET_VEC (maxqcoff_vec, "coff-maxq", 0, EXTRA_S_FLAGS,
			       TARGET_UNDERSCORE, NULL, COFF_SWAP_TABLE);

