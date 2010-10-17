/* BFD back-end for Intel i860 COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Created mostly by substituting "860" for "386" in coff-i386.c
   Harry Dolan <dolan@ssd.intel.com>, October 1995

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#include "coff/i860.h"

#include "coff/internal.h"

#include "libcoff.h"


#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)
/* The page size is a guess based on ELF.  */

#define COFF_PAGE_SIZE 0x1000

/* For some reason when using i860 COFF the value stored in the .text
   section for a reference to a common symbol is the value itself plus
   any desired offset.  Ian Taylor, Cygnus Support.  */

/* If we are producing relocatable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type
coff_i860_reloc (bfd *abfd,
		 arelent *reloc_entry,
		 asymbol *symbol,
		 void *data,
		 asection *input_section ATTRIBUTE_UNUSED,
		 bfd *output_bfd,
		 char **error_message ATTRIBUTE_UNUSED)
{
  symvalue diff;

  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  if (bfd_is_com_section (symbol->section))
    {
      /* We are relocating a common symbol.  The current value in the
	 object file is ORIG + OFFSET, where ORIG is the value of the
	 common symbol as seen by the object file when it was compiled
	 (this may be zero if the symbol was undefined) and OFFSET is
	 the offset into the common symbol (normally zero, but may be
	 non-zero when referring to a field in a common structure).
	 ORIG is the negative of reloc_entry->addend, which is set by
	 the CALC_ADDEND macro below.  We want to replace the value in
	 the object file with NEW + OFFSET, where NEW is the value of
	 the common symbol which we are going to put in the final
	 object file.  NEW is symbol->value.  */
      diff = symbol->value + reloc_entry->addend;
    }
  else
    {
      /* For some reason bfd_perform_relocation always effectively
	 ignores the addend for a COFF target when producing
	 relocatable output.  This seems to be always wrong for 860
	 COFF, so we handle the addend here instead.  */
      diff = reloc_entry->addend;
    }

#define DOIT(x) \
  x = ((x & ~howto->dst_mask) | (((x & howto->src_mask) + diff) & howto->dst_mask))

    if (diff != 0)
      {
	reloc_howto_type *howto = reloc_entry->howto;
	unsigned char *addr = (unsigned char *) data + reloc_entry->address;

	switch (howto->size)
	  {
	  case 0:
	    {
	      char x = bfd_get_8 (abfd, addr);
	      DOIT (x);
	      bfd_put_8 (abfd, x, addr);
	    }
	    break;

	  case 1:
	    {
	      short x = bfd_get_16 (abfd, addr);
	      DOIT (x);
	      bfd_put_16 (abfd, (bfd_vma) x, addr);
	    }
	    break;

	  case 2:
	    {
	      long x = bfd_get_32 (abfd, addr);
	      DOIT (x);
	      bfd_put_32 (abfd, (bfd_vma) x, addr);
	    }
	    break;

	  default:
	    abort ();
	  }
      }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}

/* This is just a temporary measure until we teach bfd to generate 
   these relocations.  */

static bfd_reloc_status_type
coff_i860_reloc_nyi (bfd *abfd ATTRIBUTE_UNUSED,
		     arelent *reloc_entry,
		     asymbol *symbol ATTRIBUTE_UNUSED,
		     void *data ATTRIBUTE_UNUSED,
		     asection *input_section ATTRIBUTE_UNUSED,
		     bfd *output_bfd ATTRIBUTE_UNUSED,
		     char **error_message ATTRIBUTE_UNUSED)
{
  reloc_howto_type *howto = reloc_entry->howto;
  fprintf (stderr, _("Relocation `%s' not yet implemented\n"), howto->name);
  return bfd_reloc_notsupported;
}

#ifndef PCRELOFFSET
#define PCRELOFFSET FALSE
#endif

static reloc_howto_type howto_table[] =
{
  EMPTY_HOWTO (0),
  EMPTY_HOWTO (1),
  EMPTY_HOWTO (2),
  EMPTY_HOWTO (3),
  EMPTY_HOWTO (4),
  EMPTY_HOWTO (5),
  HOWTO (R_DIR32,               /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i860_reloc,       /* special_function */
	 "dir32",               /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 TRUE),                /* pcrel_offset */
  /* {7}, */
  HOWTO (R_IMAGEBASE,            /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 FALSE,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i860_reloc,       /* special_function */
	 "rva32",	           /* name */
	 TRUE,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 FALSE),                /* pcrel_offset */
  EMPTY_HOWTO (010),
  EMPTY_HOWTO (011),
  EMPTY_HOWTO (012),
  EMPTY_HOWTO (013),
  EMPTY_HOWTO (014),
  EMPTY_HOWTO (015),
  EMPTY_HOWTO (016),
  HOWTO (R_RELBYTE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "8",			/* name */
	 TRUE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_RELWORD,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "16",			/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_RELLONG,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "32",			/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_PCRBYTE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "DISP8",		/* name */
	 TRUE,			/* partial_inplace */
	 0x000000ff,		/* src_mask */
	 0x000000ff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_PCRWORD,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "DISP16",		/* name */
	 TRUE,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  HOWTO (R_PCRLONG,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "DISP32",		/* name */
	 TRUE,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 PCRELOFFSET),		/* pcrel_offset */
  EMPTY_HOWTO (0x15),
  EMPTY_HOWTO (0x16),
  EMPTY_HOWTO (0x17),
  EMPTY_HOWTO (0x18),
  EMPTY_HOWTO (0x19),
  EMPTY_HOWTO (0x1a),
  EMPTY_HOWTO (0x1b),
  HOWTO (COFF860_R_PAIR,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc_nyi,	/* special_function */
	 "PAIR",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  EMPTY_HOWTO (0x1d),
  HOWTO (COFF860_R_HIGH,	/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "HIGH",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_LOW0,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "LOW0",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_LOW1,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "LOW1",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfffe,		/* src_mask */
	 0xfffe,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_LOW2,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "LOW2",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfffc,		/* src_mask */
	 0xfffc,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_LOW3,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "LOW3",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfff8,		/* src_mask */
	 0xfff8,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_LOW4,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc,	/* special_function */
	 "LOW4",		/* name */
	 FALSE,			/* partial_inplace */
	 0xfff0,		/* src_mask */
	 0xfff0,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_SPLIT0,      /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc_nyi,	/* special_function */
	 "SPLIT0",		/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07ff,		/* src_mask */
	 0x1f07ff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_SPLIT1,      /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc_nyi,	/* special_function */
	 "SPLIT1",		/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07fe,		/* src_mask */
	 0x1f07fe,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_SPLIT2,      /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc_nyi,	/* special_function */
	 "SPLIT2",		/* name */
	 FALSE,			/* partial_inplace */
	 0x1f07fc,		/* src_mask */
	 0x1f07fc,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_HIGHADJ,     /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_i860_reloc_nyi,	/* special_function */
	 "HIGHADJ",		/* name */
	 FALSE,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 FALSE),	        /* pcrel_offset */
  HOWTO (COFF860_R_BRADDR,      /* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i860_reloc_nyi,	/* special_function */
	 "BRADDR",		/* name */
	 FALSE,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 TRUE)		        /* pcrel_offset */
};

/* Turn a howto into a reloc number.  */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) I860BADMAG(x)
#define I860 1			/* Customize coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst)					\
  ((cache_ptr)->howto =							\
   ((dst)->r_type < sizeof (howto_table) / sizeof (howto_table[0])	\
    ? howto_table + (dst)->r_type					\
    : NULL))

/* For 860 COFF a STYP_NOLOAD | STYP_BSS section is part of a shared
   library.  On some other COFF targets STYP_BSS is normally
   STYP_NOLOAD.  */
#define BSS_NOLOAD_IS_SHARED_LIBRARY

/* Compute the addend of a reloc.  If the reloc is to a common symbol,
   the object file contains the value of the common symbol.  By the
   time this is called, the linker may be using a different symbol
   from a different object file with a different value.  Therefore, we
   hack wildly to locate the original symbol from this file so that we
   can make the correct adjustment.  This macro sets coffsym to the
   symbol from the original file, and uses it to set the addend value
   correctly.  If this is not a common symbol, the usual addend
   calculation is done, except that an additional tweak is needed for
   PC relative relocs.
   FIXME: This macro refers to symbols and asect; these are from the
   calling function, not the macro arguments.  */

/* FIXME: This was copied from the i386 version originally but
   appears to be wrong for i860.  For now we'll do nothing.  */
#if 0
#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)		\
  {								\
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;	\
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)			\
      coffsym = (obj_symbols (abfd)				\
	         + (cache_ptr->sym_ptr_ptr - symbols));		\
    else if (ptr)						\
      coffsym = coff_symbol_from (abfd, ptr);			\
    if (coffsym != (coff_symbol_type *) NULL			\
	&& coffsym->native->u.syment.n_scnum == 0)		\
      cache_ptr->addend = - coffsym->native->u.syment.n_value;	\
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd		\
	     && ptr->section != (asection *) NULL)		\
      cache_ptr->addend = - (ptr->section->vma + ptr->value);	\
    else							\
      cache_ptr->addend = 0;					\
    if (ptr && howto_table[reloc.r_type].pc_relative)		\
      cache_ptr->addend += asect->vma;				\
  }
#else
#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)
#endif

/* We use the special COFF backend linker.  */
#define coff_relocate_section _bfd_coff_generic_relocate_section

static reloc_howto_type *
coff_i860_rtype_to_howto (bfd *abfd ATTRIBUTE_UNUSED,
			  asection *sec,
			  struct internal_reloc *rel,
			  struct coff_link_hash_entry *h,
			  struct internal_syment *sym,
			  bfd_vma *addendp)
{

  reloc_howto_type *howto;

  if (rel->r_type > sizeof (howto_table) / sizeof (howto_table[0]))
    {
      bfd_set_error (bfd_error_bad_value);
      return NULL;
    }

  howto = howto_table + rel->r_type;

  if (howto->pc_relative)
    *addendp += sec->vma;

  if (sym != NULL && sym->n_scnum == 0 && sym->n_value != 0)
    {
      /* This is a common symbol.  The section contents include the
	 size (sym->n_value) as an addend.  The relocate_section
	 function will be adding in the final value of the symbol.  We
	 need to subtract out the current size in order to get the
	 correct result.  */

      BFD_ASSERT (h != NULL);

      /* I think we *do* want to bypass this.  If we don't, I have seen some data
	 parameters get the wrong relocation address.  If I link two versions
	 with and without this section bypassed and then do a binary comparison,
	 the addresses which are different can be looked up in the map.  The
	 case in which this section has been bypassed has addresses which correspond
	 to values I can find in the map.  */
      *addendp -= sym->n_value;
    }

  /* If the output symbol is common (in which case this must be a
     relocatable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common)
    *addendp += h->root.u.c.size;

  return howto;
}

static reloc_howto_type *
coff_i860_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			     bfd_reloc_code_real_type code)
{
  switch (code)
    {
    case BFD_RELOC_32:
      return howto_table + R_DIR32;
    case BFD_RELOC_860_PC26:
      return howto_table + COFF860_R_BRADDR;
    case BFD_RELOC_860_PC16:
      /* ??? How to handle PC16 for COFF?  SPLIT0 is close for now.  */
      return howto_table + COFF860_R_SPLIT0;
    case BFD_RELOC_860_LOW0:
      return howto_table + COFF860_R_LOW0;
    case BFD_RELOC_860_SPLIT0:
      return howto_table + COFF860_R_SPLIT0;
    case BFD_RELOC_860_LOW1:
      return howto_table + COFF860_R_LOW1;
    case BFD_RELOC_860_SPLIT1:
      return howto_table + COFF860_R_SPLIT1;
    case BFD_RELOC_860_LOW2:
      return howto_table + COFF860_R_LOW2;
    case BFD_RELOC_860_SPLIT2:
      return howto_table + COFF860_R_SPLIT2;
    case BFD_RELOC_860_LOW3:
      return howto_table + COFF860_R_LOW3;
    case BFD_RELOC_860_HIGHADJ:
      return howto_table + COFF860_R_HIGHADJ;
    case BFD_RELOC_860_HIGH:
      return howto_table + COFF860_R_HIGH;
    default:
      BFD_FAIL ();
      return 0;
    }
}

/* This is called from coff_slurp_reloc_table for each relocation
   entry.  This special handling is due to the `PAIR' relocation
   which has a different meaning for the `r_symndx' field.  */

static void
i860_reloc_processing (arelent *cache_ptr, struct internal_reloc *dst,
		       asymbol **symbols, bfd *abfd, asection *asect)
{
  if (dst->r_type == COFF860_R_PAIR)
    {
      /* Handle the PAIR relocation specially.  */
      cache_ptr->howto = howto_table + dst->r_type;
      cache_ptr->address = dst->r_vaddr;
      cache_ptr->addend = dst->r_symndx;
      cache_ptr->sym_ptr_ptr= bfd_abs_section_ptr->symbol_ptr_ptr;
    }
  else
    {
      /* For every other relocation, do exactly what coff_slurp_reloc_table
         would do (which this code is taken directly from).  */
      asymbol *ptr = NULL;
      cache_ptr->address = dst->r_vaddr;

      if (dst->r_symndx != -1)
	{
	  if (dst->r_symndx < 0 || dst->r_symndx >= obj_conv_table_size (abfd))
	    {
	      (*_bfd_error_handler)
		(_("%s: warning: illegal symbol index %ld in relocs"),
		 bfd_archive_filename (abfd), dst->r_symndx);
	      cache_ptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	      ptr = NULL;
	    }
	  else
	    {
	      cache_ptr->sym_ptr_ptr = (symbols
					+ obj_convert (abfd)[dst->r_symndx]);
	      ptr = *(cache_ptr->sym_ptr_ptr);
	    }
	}
      else
	{
	  cache_ptr->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
	  ptr = NULL;
	}

      /* The symbols definitions that we have read in have been
	 relocated as if their sections started at 0. But the offsets
	 refering to the symbols in the raw data have not been
	 modified, so we have to have a negative addend to compensate.

	 Note that symbols which used to be common must be left alone.  */

      /* Calculate any reloc addend by looking at the symbol.  */
      CALC_ADDEND (abfd, ptr, (*dst), cache_ptr);

      cache_ptr->address -= asect->vma;

      /* Fill in the cache_ptr->howto field from dst->r_type.  */
      RTYPE2HOWTO (cache_ptr, dst);
    }
}

#define coff_rtype_to_howto		coff_i860_rtype_to_howto
#define coff_bfd_reloc_type_lookup	coff_i860_reloc_type_lookup

#define RELOC_PROCESSING(relent, reloc, symbols, abfd, section) \
  i860_reloc_processing (relent, reloc, symbols, abfd, section)

#include "coffcode.h"

static const bfd_target *
i3coff_object_p(bfd *a)
{
  return coff_object_p (a);
}

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  i860coff_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "coff-i860",			/* name */
#endif
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  '_',				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

/* Note that we allow an object file to be treated as a core file as well.  */
    {_bfd_dummy_target, i3coff_object_p, /* bfd_check_format */
       bfd_generic_archive_p, i3coff_object_p},
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

  NULL,

  COFF_SWAP_TABLE
};
