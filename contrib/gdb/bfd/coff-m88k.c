/* BFD back-end for Motorola 88000 COFF "Binary Compatability Standard" files.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define M88 1		/* Customize various include files */
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
#include "coff/m88k.h"
#include "coff/internal.h"
#include "libcoff.h"

static bfd_reloc_status_type m88k_special_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static void rtype2howto PARAMS ((arelent *, struct internal_reloc *));
static void reloc_processing
  PARAMS ((arelent *, struct internal_reloc *, asymbol **, bfd *, asection *));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (3)

static bfd_reloc_status_type 
m88k_special_reloc (abfd, reloc_entry, symbol, data,
		    input_section, output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
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

	  /* Work out which section the relocation is targetted at and the
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
	      bfd_put_16 (abfd, relocation, (unsigned char *) data + addr);
	}

      return bfd_reloc_ok;
      break;

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
	 true,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "PCR16L",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_PCR26L,			/* type */
	 02,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 26,				/* bitsize */
	 true,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "PCR26L",			/* name */
	 false,				/* partial_inplace */
	 0x03ffffff,			/* src_mask */
	 0x03ffffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_VRT16,			/* type */
	 00,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "VRT16",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_HVRT16,			/* type */
	 16,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "HVRT16",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_LVRT16,			/* type */
	 00,				/* rightshift */
	 1,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "LVRT16",			/* name */
	 false,				/* partial_inplace */
	 0x0000ffff,			/* src_mask */
	 0x0000ffff,			/* dst_mask */
	 true),				/* pcrel_offset */

  HOWTO (R_VRT32,			/* type */
	 00,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 false,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 m88k_special_reloc,		/* special_function */
	 "VRT32",			/* name */
	 false,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 true),				/* pcrel_offset */
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
#define SWAP_IN_RELOC_OFFSET  bfd_h_get_16
#define SWAP_OUT_RELOC_OFFSET bfd_h_put_16


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

const bfd_target m88kbcs_vec =
{
  "coff-m88kbcs",		/* name */
  bfd_target_coff_flavour,
  BFD_ENDIAN_BIG,		/* data byte order is big */
  BFD_ENDIAN_BIG,		/* header byte order is big */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
     bfd_getb32, bfd_getb_signed_32, bfd_putb32,
     bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

    {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
       bfd_generic_archive_p, _bfd_dummy_target},
    {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
       bfd_false},
    {bfd_false, coff_write_object_contents, /* bfd_write_contents */
       _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (coff),
     BFD_JUMP_TABLE_COPY (coff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
     BFD_JUMP_TABLE_SYMBOLS (coff),
     BFD_JUMP_TABLE_RELOCS (coff),
     BFD_JUMP_TABLE_WRITE (coff),
     BFD_JUMP_TABLE_LINK (coff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  COFF_SWAP_TABLE,
};
