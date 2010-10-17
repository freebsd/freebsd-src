/* BFD back-end for Texas Instruments TMS320C80 Multimedia Video Processor (MVP).
   Copyright 1996, 1997, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

   Written by Fred Fish (fnf@cygnus.com)

   There is nothing new under the sun. This file draws a lot on other
   coff files.

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
Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "bfdlink.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/tic80.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)
#define COFF_ALIGN_IN_SECTION_HEADER 1
#define COFF_ALIGN_IN_SFLAGS 1

#define GET_SCNHDR_FLAGS H_GET_16
#define PUT_SCNHDR_FLAGS H_PUT_16

static void rtype2howto
  PARAMS ((arelent *cache_ptr, struct internal_reloc *dst));
static bfd_reloc_status_type ppbase_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type glob15_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type glob16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type local16_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_boolean coff_tic80_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));
static reloc_howto_type * coff_tic80_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));

static reloc_howto_type tic80_howto_table[] =
{

  HOWTO (R_RELLONG,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "RELLONG",			/* name */
	 TRUE,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_MPPCR,			/* type */
	 2,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 TRUE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_signed,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "MPPCR",			/* name */
	 TRUE,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 TRUE),				/* pcrel_offset */

  HOWTO (R_ABS,				/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "ABS",				/* name */
	 TRUE,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 FALSE),				/* pcrel_offset */

  HOWTO (R_PPBASE,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 ppbase_reloc,			/* special_function */
	 "PPBASE",			/* name */
	 TRUE,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPLBASE,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 32,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 ppbase_reloc,			/* special_function */
	 "PPLBASE",			/* name */
	 TRUE,				/* partial_inplace */
	 0xffffffff,			/* src_mask */
	 0xffffffff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PP15,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob15_reloc,			/* special_function */
	 "PP15",			/* name */
	 TRUE,				/* partial_inplace */
	 0x1ffc0,			/* src_mask */
	 0x1ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PP15W,			/* type */
	 2,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob15_reloc,			/* special_function */
	 "PP15W",			/* name */
	 TRUE,				/* partial_inplace */
	 0x1ffc0,			/* src_mask */
	 0x1ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PP15H,			/* type */
	 1,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob15_reloc,			/* special_function */
	 "PP15H",			/* name */
	 TRUE,				/* partial_inplace */
	 0x1ffc0,			/* src_mask */
	 0x1ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PP16B,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob16_reloc,			/* special_function */
	 "PP16B",			/* name */
	 TRUE,				/* partial_inplace */
	 0x3ffc0,			/* src_mask */
	 0x3ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPL15,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "PPL15",			/* name */
	 TRUE,				/* partial_inplace */
	 0x7fff,			/* src_mask */
	 0x7fff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPL15W,			/* type */
	 2,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "PPL15W",			/* name */
	 TRUE,				/* partial_inplace */
	 0x7fff,			/* src_mask */
	 0x7fff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPL15H,			/* type */
	 1,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "PPL15H",			/* name */
	 TRUE,				/* partial_inplace */
	 0x7fff,			/* src_mask */
	 0x7fff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPL16B,			/* type */
	 0,				/* rightshift */
	 2,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 local16_reloc,			/* special_function */
	 "PPL16B",			/* name */
	 TRUE,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPN15,			/* type */
	 0,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob15_reloc,			/* special_function */
	 "PPN15",			/* name */
	 TRUE,				/* partial_inplace */
	 0x1ffc0,			/* src_mask */
	 0x1ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPN15W,			/* type */
	 2,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob15_reloc,			/* special_function */
	 "PPN15W",			/* name */
	 TRUE,				/* partial_inplace */
	 0x1ffc0,			/* src_mask */
	 0x1ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPN15H,			/* type */
	 1,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob15_reloc,			/* special_function */
	 "PPN15H",			/* name */
	 TRUE,				/* partial_inplace */
	 0x1ffc0,			/* src_mask */
	 0x1ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPN16B,			/* type */
	 0,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 16,				/* bitsize */
	 FALSE,				/* pc_relative */
	 6,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 glob16_reloc,			/* special_function */
	 "PPN16B",			/* name */
	 TRUE,				/* partial_inplace */
	 0x3ffc0,			/* src_mask */
	 0x3ffc0,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPLN15,			/* type */
	 0,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "PPLN15",			/* name */
	 TRUE,				/* partial_inplace */
	 0x7fff,			/* src_mask */
	 0x7fff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPLN15W,			/* type */
	 2,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "PPLN15W",			/* name */
	 TRUE,				/* partial_inplace */
	 0x7fff,			/* src_mask */
	 0x7fff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPLN15H,			/* type */
	 1,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 NULL,				/* special_function */
	 "PPLN15H",			/* name */
	 TRUE,				/* partial_inplace */
	 0x7fff,			/* src_mask */
	 0x7fff,			/* dst_mask */
	 FALSE),			/* pcrel_offset */

  HOWTO (R_PPLN16B,			/* type */
	 0,				/* rightshift */
	 -2,				/* size (0 = byte, 1 = short, 2 = long) */
	 15,				/* bitsize */
	 FALSE,				/* pc_relative */
	 0,				/* bitpos */
	 complain_overflow_dont,	/* complain_on_overflow */
	 local16_reloc,			/* special_function */
	 "PPLN16B",			/* name */
	 TRUE,				/* partial_inplace */
	 0xffff,			/* src_mask */
	 0xffff,			/* dst_mask */
	 FALSE)				/* pcrel_offset */
};

/* Special relocation functions, used when the output file is not
   itself a COFF TIc80 file.  */

/* This special function is used for the base address type
   relocations.  */

static bfd_reloc_status_type
ppbase_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
	      error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol_in ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* FIXME.  */
  abort ();
}

/* This special function is used for the global 15 bit relocations.  */

static bfd_reloc_status_type
glob15_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
	      error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol_in ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* FIXME.  */
  abort ();
}

/* This special function is used for the global 16 bit relocations.  */

static bfd_reloc_status_type
glob16_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
	      error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol_in ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* FIXME.  */
  abort ();
}

/* This special function is used for the local 16 bit relocations.  */

static bfd_reloc_status_type
local16_reloc (abfd, reloc_entry, symbol_in, data, input_section, output_bfd,
	      error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry ATTRIBUTE_UNUSED;
     asymbol *symbol_in ATTRIBUTE_UNUSED;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     char **error_message ATTRIBUTE_UNUSED;
{
  /* FIXME.  */
  abort ();
}

/* Code to turn an external r_type into a pointer to an entry in the howto_table.
   If passed an r_type we don't recognize the abort rather than silently failing
   to generate an output file.  */

static void
rtype2howto (cache_ptr, dst)
     arelent *cache_ptr;
     struct internal_reloc *dst;
{
  unsigned int i;

  for (i = 0; i < sizeof tic80_howto_table / sizeof tic80_howto_table[0]; i++)
    {
      if (tic80_howto_table[i].type == dst->r_type)
	{
	  cache_ptr->howto = tic80_howto_table + i;
	  return;
	}
    }

  (*_bfd_error_handler) (_("Unrecognized reloc type 0x%x"),
			 (unsigned int) dst->r_type);
  cache_ptr->howto = tic80_howto_table + 0;
}

#define RTYPE2HOWTO(cache_ptr, dst) rtype2howto (cache_ptr, dst)
#define coff_rtype_to_howto coff_tic80_rtype_to_howto

static reloc_howto_type *
coff_tic80_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h ATTRIBUTE_UNUSED;
     struct internal_syment *sym ATTRIBUTE_UNUSED;
     bfd_vma *addendp;
{
  arelent genrel;

  if (rel -> r_symndx == -1 && addendp != NULL)
    {
      /* This is a TI "internal relocation", which means that the relocation
	 amount is the amount by which the current section is being relocated
	 in the output section.  */
      *addendp = (sec -> output_section -> vma + sec -> output_offset) - sec -> vma;
    }
  RTYPE2HOWTO (&genrel, rel);
  return genrel.howto;
}

#ifndef BADMAG
#define BADMAG(x) TIC80BADMAG(x)
#endif

#define coff_relocate_section coff_tic80_relocate_section

/* We need a special relocation routine to handle the PP relocs.  Most
   of this is a copy of _bfd_coff_generic_relocate_section.  */

static bfd_boolean
coff_tic80_relocate_section (output_bfd, info, input_bfd,
			     input_section, contents, relocs, syms,
			     sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      reloc_howto_type *howto;
      bfd_reloc_status_type rstat;
      bfd_vma addr;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else
	{
	  h = obj_coff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	}

      /* COFF treats common symbols in one of two ways.  Either the
         size of the symbol is included in the section contents, or it
         is not.  We assume that the size is not included, and force
         the rtype_to_howto function to adjust the addend as needed.  */

      if (sym != NULL && sym->n_scnum != 0)
	addend = - sym->n_value;
      else
	addend = 0;

      howto = bfd_coff_rtype_to_howto (input_bfd, input_section, rel, h,
				       sym, &addend);
      if (howto == NULL)
	return FALSE;

      val = 0;

      if (h == NULL)
	{
	  asection *sec;

	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
              val = (sec->output_section->vma
		     + sec->output_offset
		     + sym->n_value);
	      if (! obj_pe (output_bfd))
		val -= sec->vma;
	    }
	}
      else
	{
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection *sec;

	      sec = h->root.u.def.section;
	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	      }

	  else if (! info->relocatable)
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma, TRUE)))
		return FALSE;
	    }
	}

      addr = rel->r_vaddr - input_section->vma;

      /* FIXME: This code assumes little endian, but the PP can
         apparently be bi-endian.  I don't know if the bi-endianness
         applies to the instruction set or just to the data.  */
      switch (howto->type)
	{
	default:
	case R_ABS:
	case R_RELLONGX:
	case R_PPL15:
	case R_PPL15W:
	case R_PPL15H:
	case R_PPLN15:
	case R_PPLN15W:
	case R_PPLN15H:
	  rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents, addr, val, addend);
	  break;

	case R_PP15:
	case R_PP15W:
	case R_PP15H:
	case R_PPN15:
	case R_PPN15W:
	case R_PPN15H:
	  /* Offset the address so that we can use 4 byte relocations.  */
	  rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					    contents + 2, addr, val, addend);
	  break;

	case R_PP16B:
	case R_PPN16B:
	  {
	    /* The most significant bit is stored in bit 6.  */
	    bfd_byte hold;

	    hold = contents[addr + 4];
	    contents[addr + 4] &=~ 0x20;
	    contents[addr + 4] |= (contents[addr] >> 1) & 0x20;
	    rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					      contents + 2, addr,
					      val, addend);
	    contents[addr] &=~ 0x40;
	    contents[addr] |= (contents[addr + 4] << 1) & 0x40;
	    contents[addr + 4] &=~ 0x20;
	    contents[addr + 4] |= hold & 0x20;
	    break;
	  }

	case R_PPL16B:
	case R_PPLN16B:
	  {
	    /* The most significant bit is stored in bit 28.  */
	    bfd_byte hold;

	    hold = contents[addr + 1];
	    contents[addr + 1] &=~ 0x80;
	    contents[addr + 1] |= (contents[addr + 3] << 3) & 0x80;
	    rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					      contents, addr,
					      val, addend);
	    contents[addr + 3] &= ~0x10;
	    contents[addr + 3] |= (contents[addr + 1] >> 3) & 0x10;
	    contents[addr + 1] &=~ 0x80;
	    contents[addr + 1] |= hold & 0x80;
	    break;
	  }

	case R_PPBASE:
	  /* Parameter RAM is from 0x1000000 to 0x1000800.  */
	  contents[addr] &=~ 0x3;
	  if (val >= 0x1000000 && val < 0x1000800)
	    contents[addr] |= 0x3;
	  else
	    contents[addr] |= 0x2;
	  rstat = bfd_reloc_ok;
	  break;

	case R_PPLBASE:
	  /* Parameter RAM is from 0x1000000 to 0x1000800.  */
	  contents[addr + 2] &= ~0xc0;
	  if (val >= 0x1000000 && val < 0x1000800)
	    contents[addr + 2] |= 0xc0;
	  else
	    contents[addr + 2] |= 0x80;
	  rstat = bfd_reloc_ok;
	  break;
	}

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_outofrange:
	  (*_bfd_error_handler)
	    (_("%s: bad reloc address 0x%lx in section `%s'"),
	     bfd_archive_filename (input_bfd),
	     (unsigned long) rel->r_vaddr,
	     bfd_get_section_name (input_bfd, input_section));
	  return FALSE;
	case bfd_reloc_overflow:
	  {
	    const char *name;
	    char buf[SYMNMLEN + 1];

	    if (symndx == -1)
	      name = "*ABS*";
	    else if (h != NULL)
	      name = h->root.root.string;
	    else
	      {
		name = _bfd_coff_internal_syment_name (input_bfd, sym, buf);
		if (name == NULL)
		  return FALSE;
	      }

	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto->name, (bfd_vma) 0, input_bfd,
		    input_section, rel->r_vaddr - input_section->vma)))
	      return FALSE;
	  }
	}
    }
  return TRUE;
}

#define TIC80COFF 1		/* Customize coffcode.h */
#undef C_AUTOARG		/* Clashes with TIc80's C_UEXT */
#undef C_LASTENT		/* Clashes with TIc80's C_STATLAB */
#include "coffcode.h"

CREATE_LITTLE_COFF_TARGET_VEC (tic80coff_vec, "coff-tic80", D_PAGED, 0, '_', NULL, COFF_SWAP_TABLE)
