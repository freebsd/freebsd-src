/* BFD back-end for Intel 386 COFF files.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
#include "coff/i386.h"
#include "coff/internal.h"
#include "libcoff.h"

static bfd_reloc_status_type coff_i386_reloc PARAMS ((bfd *abfd,
						      arelent *reloc_entry,
						      asymbol *symbol,
						      PTR data,
						      asection *input_section,
						      bfd *output_bfd,
						      char **error_message));

/* The page size is a guess based on ELF.  */
#define COFF_PAGE_SIZE 0x1000

/* For some reason when using i386 COFF the value stored in the .text
   section for a reference to a common symbol is the value itself plus
   any desired offset.  Ian Taylor, Cygnus Support.  */

/* If we are producing relocateable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type
coff_i386_reloc (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
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
	 relocateable output.  This seems to be always wrong for 386
	 COFF, so we handle the addend here instead.  */
      diff = reloc_entry->addend;
    }

#define DOIT(x) \
  x = ((x & ~howto->dst_mask) | (((x & howto->src_mask) + diff) & howto->dst_mask))

  if (diff != 0)
    {
      const reloc_howto_type *howto = reloc_entry->howto;
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
	    bfd_put_16 (abfd, x, addr);
	  }
	  break;

	case 2:
	  {
	    long x = bfd_get_32 (abfd, addr);
	    DOIT (x);
	    bfd_put_32 (abfd, x, addr);
	  }
	  break;

	default:
	  abort ();
	}
    }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}

static reloc_howto_type howto_table[] = 
{
  {0},
  {1},
  {2},
  {3},
  {4},
  {5},
  HOWTO (R_DIR32,               /* type */                                 
	 0,	                /* rightshift */                           
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 32,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,       /* special_function */                     
	 "dir32",               /* name */                                 
	 true,	                /* partial_inplace */                      
	 0xffffffff,            /* src_mask */                             
	 0xffffffff,            /* dst_mask */                             
	 false),                /* pcrel_offset */
  {7},
  {010},
  {011},
  {012},
  {013},
  {014},
  {015},
  {016},
  HOWTO (R_RELBYTE,		/* type */                                 
	 0,			/* rightshift */                           
	 0,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 8,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "8",			/* name */                                 
	 true,			/* partial_inplace */                      
	 0x000000ff,		/* src_mask */                             
	 0x000000ff,		/* dst_mask */                             
	 false),		/* pcrel_offset */
  HOWTO (R_RELWORD,		/* type */                                 
	 0,			/* rightshift */                           
	 1,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 16,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "16",			/* name */                                 
	 true,			/* partial_inplace */                      
	 0x0000ffff,		/* src_mask */                             
	 0x0000ffff,		/* dst_mask */                             
	 false),		/* pcrel_offset */
  HOWTO (R_RELLONG,		/* type */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 false,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "32",			/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 false),		/* pcrel_offset */
  HOWTO (R_PCRBYTE,		/* type */                                 
	 0,			/* rightshift */                           
	 0,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 8,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "DISP8",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0x000000ff,		/* src_mask */                             
	 0x000000ff,		/* dst_mask */                             
	 false),		/* pcrel_offset */
  HOWTO (R_PCRWORD,		/* type */                                 
	 0,			/* rightshift */                           
	 1,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 16,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "DISP16",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0x0000ffff,		/* src_mask */                             
	 0x0000ffff,		/* dst_mask */                             
	 false),		/* pcrel_offset */
  HOWTO (R_PCRLONG,		/* type */                                 
	 0,			/* rightshift */                           
	 2,			/* size (0 = byte, 1 = short, 2 = long) */ 
	 32,			/* bitsize */                   
	 true,			/* pc_relative */                          
	 0,			/* bitpos */                               
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_i386_reloc,	/* special_function */                     
	 "DISP32",		/* name */                                 
	 true,			/* partial_inplace */                      
	 0xffffffff,		/* src_mask */                             
	 0xffffffff,		/* dst_mask */                             
	 false)			/* pcrel_offset */
};

/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) I386BADMAG(x)
#define I386 1			/* Customize coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst) \
	    cache_ptr->howto = howto_table + (dst)->r_type;

/* On SCO Unix 3.2.2 the native assembler generates two .data
   sections.  We handle that by renaming the second one to .data2.  It
   does no harm to do this for any 386 COFF target.  */
#define TWO_DATA_SECS

/* For 386 COFF a STYP_NOLOAD | STYP_BSS section is part of a shared
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

#include "coffcode.h"

static const bfd_target *
i3coff_object_p(a)
     bfd *a;
{
  return coff_object_p(a);
}

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  i386coff_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "coff-i386",			/* name */
#endif
  bfd_target_coff_flavour,
  false,			/* data byte order is little */
  false,			/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
  0,				/* leading underscore */
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  2,				/* minimum alignment power */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

/* Note that we allow an object file to be treated as a core file as well. */
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

  COFF_SWAP_TABLE,
};
