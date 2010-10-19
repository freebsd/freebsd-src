/* BFD back-end for Motorola 88000 COFF "Binary Compatibility Standard" files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1997, 1998, 1999, 2000,
   2001, 2002, 2003
   Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#define M88 1		/* Customize various include files */
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/m88k.h"
#include "coff/internal.h"
#include "libcoff.h"

static bfd_boolean m88k_is_local_label_name PARAMS ((bfd *, const char *));
static bfd_reloc_status_type m88k_special_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static void rtype2howto PARAMS ((arelent *, struct internal_reloc *));
static void reloc_processing
  PARAMS ((arelent *, struct internal_reloc *, asymbol **, bfd *, asection *));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

#define GET_SCNHDR_NRELOC H_GET_32
#define GET_SCNHDR_NLNNO  H_GET_32

/* On coff-m88k, local labels start with '@'.  */

#define coff_bfd_is_local_label_name m88k_is_local_label_name

static bfd_boolean
m88k_is_local_label_name (abfd, name)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
{
  return name[0] == '@';
}

static bfd_reloc_status_type
m88k_special_reloc (abfd, reloc_entry, symbol, data,
		    input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  reloc_howto_type *howto = reloc_entry->howto;

  switch (howto->type)
    {
    case R_HVRT16:
    case R_LVRT16:
      if (output_bfd != (bfd *) NULL)
	{
	  /* This is a partial relocation, and we want to apply the
	     relocation to the reloc entry rather than the raw data.
	     Modify the reloc inplace to reflect what we now know.  */

	  reloc_entry->address += input_section->output_offset;
	}
      else
	{
	  bfd_vma output_base = 0;
	  bfd_vma addr = reloc_entry->address;
	  bfd_vma x = bfd_get_16 (abfd, (bfd_byte *) data + addr);
	  asection *reloc_target_output_section;
	  long relocation = 0;

	  /* Work out which section the relocation is targeted at and the
	     initial relocation command value.  */

	  /* Get symbol value.  (Common symbols are special.)  */
	  if (bfd_is_com_section (symbol->section))
	    relocation = 0;
	  else
	    relocation = symbol->value;

	  reloc_target_output_section = symbol->section->output_section;

	  /* Convert input-section-relative symbol value to absolute.  */
	  if (output_bfd)
	    output_base = 0;
	  else
	    output_base = reloc_target_output_section->vma;

	  relocation += output_base + symbol->section->output_offset;

	  /* Add in supplied addend.  */
	  relocation += ((reloc_entry->addend << howto->bitsize) + x);

	  reloc_entry->addend = 0;

	  relocation >>= (bfd_vma) howto->rightshift;

	  /* Shift everything up to where it's going to be used */

	  relocation <<= (bfd_vma) howto->bitpos;

	  if (relocation)
	    bfd_put_16 (abfd, (bfd_vma) relocation,
			(unsigned char *) data + addr);
	}

      /* If we are not producing relocatable output, return an error if
	 the symbol is not defined.  */
      if (bfd_is_und_section (symbol->section) && output_bfd == (bfd *) NULL)
	return bfd_reloc_undefined;

      return bfd_reloc_ok;

    default:
      if (output_bfd != (bfd *) NULL)
	{
	  /* This is a partial relocation, and we want to apply the
	     relocation to the reloc entry rather than the raw data.
	     Modify the reloc inplace to reflect what we now know.  */

	  reloc_entry->address += input_section->output_offset;
	  return bfd_reloc_ok;
	}
      break;
    }

  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  return bfd_reloc_ok;
}

static reloc_howto_type howto_table[] =
{
  HOWTO (R_PCR16L,			/* type */
	 02,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 TRUE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "PCR16L",			/* name */
	 FALSE,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */

  HOWTO (R_PCR26L,			/* type */
	 02,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 26,				/* bitsize */
	 TRUE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "PCR26L",			/* name */
	 FALSE,				/* partial_inplace */
	 0x03ffffff,			/* src_mask */
	 0x03ffffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */

  HOWTO (R_VRT16,			/* type */
	 00,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "VRT16",			/* name */
	 FALSE,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */

  HOWTO (R_HVRT16,			/* type */
	 16,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "HVRT16",			/* name */
	 FALSE,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */

  HOWTO (R_LVRT16,			/* type */
	 00,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "LVRT16",			/* name */
	 FALSE,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */

  HOWTO (R_VRT32,			/* type */
	 00,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "VRT32",			/* name */
	 FALSE,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */
};

/* Code to turn an external r_type into a pointer to an entry in the
   above howto table.  */
static void
rtype2howto (cache_ptr, dst)
     arelent *cache_ptr;
     struct internal_reloc *dst;
{
  if (dst->r_type >= R_PCR16L && dst->r_type <= R_VRT32)
    {
      cache_ptr->howto = howto_table + dst->r_type - R_PCR16L;
    }
  else
    {
      BFD_ASSERT (0);
    }
}

#define RTYPE2HOWTO(cache_ptr, dst) rtype2howto (cache_ptr, dst)

/* Code to swap in the reloc offset */
#define SWAP_IN_RELOC_OFFSET  H_GET_16
#define SWAP_OUT_RELOC_OFFSET H_PUT_16

#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section)	\
  reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (relent, reloc, symbols, abfd, section)
     arelent *relent;
     struct internal_reloc *reloc;
     asymbol **symbols;
     bfd *abfd;
     asection *section;
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (((int) reloc->r_symndx) > 0)
    {
      relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
    }
  else
    {
      relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
    }

  relent->addend = reloc->r_offset;
  relent->address -= section->vma;
}

#define BADMAG(x) MC88BADMAG(x)
#include "coffcode.h"

#undef coff_write_armap

CREATE_BIG_COFF_TARGET_VEC (m88kbcs_vec, "coff-m88kbcs", 0, 0, '_', NULL, COFF_SWAP_TABLE)
