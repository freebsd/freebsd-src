/* BFD back-end for Intel 386 COFF files.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 1998
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#include "coff/i386.h"

#include "coff/internal.h"

#ifdef COFF_WITH_PE
#include "coff/pe.h"
#endif

#ifdef COFF_GO32_EXE
#include "coff/go32exe.h"
#endif

#include "libcoff.h"

static bfd_reloc_status_type coff_i386_reloc 
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *coff_i386_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,

	   bfd_vma *));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)
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
#ifndef COFF_WITH_PE
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
#else
      /* In PE mode, we do not offset the common symbol.  */
      diff = reloc_entry->addend;
#endif
    }
  else
    {
      /* For some reason bfd_perform_relocation always effectively
	 ignores the addend for a COFF target when producing
	 relocateable output.  This seems to be always wrong for 386
	 COFF, so we handle the addend here instead.  */
      diff = reloc_entry->addend;
    }

#ifdef COFF_WITH_PE
  /* FIXME: How should this case be handled?  */
  if (reloc_entry->howto->type == R_IMAGEBASE && diff != 0)
    abort ();
#endif

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

#ifdef COFF_WITH_PE
/* Return true if this relocation should
   appear in the output .reloc section. */

static boolean in_reloc_p(abfd, howto)
     bfd * abfd;
     reloc_howto_type *howto;
{
  return ! howto->pc_relative && howto->type != R_IMAGEBASE;
}     
#endif

#ifndef PCRELOFFSET
#define PCRELOFFSET false
#endif

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
	 true),                /* pcrel_offset */
  /* {7}, */
  HOWTO (R_IMAGEBASE,            /* type */                                 
	 0,	                /* rightshift */                           
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */ 
	 32,	                /* bitsize */                   
	 false,	                /* pc_relative */                          
	 0,	                /* bitpos */                               
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_i386_reloc,       /* special_function */                     
	 "rva32",	           /* name */                                 
	 true,	                /* partial_inplace */                      
	 0xffffffff,            /* src_mask */                             
	 0xffffffff,            /* dst_mask */                             
	 false),                /* pcrel_offset */
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
	 PCRELOFFSET),		/* pcrel_offset */
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
	 PCRELOFFSET),		/* pcrel_offset */
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
	 PCRELOFFSET),		/* pcrel_offset */
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
	 PCRELOFFSET),		/* pcrel_offset */
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
	 PCRELOFFSET),		/* pcrel_offset */
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
	 PCRELOFFSET)		/* pcrel_offset */
};

/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) I386BADMAG(x)
#define I386 1			/* Customize coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst) \
	    (cache_ptr)->howto = howto_table + (dst)->r_type;

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

/* We use the special COFF backend linker.  For normal i386 COFF, we
   can use the generic relocate_section routine.  For PE, we need our
   own routine.  */

#ifndef COFF_WITH_PE

#define coff_relocate_section _bfd_coff_generic_relocate_section

#else /* COFF_WITH_PE */

/* The PE relocate section routine.  The only difference between this
   and the regular routine is that we don't want to do anything for a
   relocateable link.  */

static boolean coff_pe_i386_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));

static boolean
coff_pe_i386_relocate_section (output_bfd, info, input_bfd,
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
  if (info->relocateable)
    return true;

  return _bfd_coff_generic_relocate_section (output_bfd, info, input_bfd,
					     input_section, contents,
					     relocs, syms, sections);
}

#define coff_relocate_section coff_pe_i386_relocate_section

#endif /* COFF_WITH_PE */

/* Convert an rtype to howto for the COFF backend linker.  */

static reloc_howto_type *
coff_i386_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{

  reloc_howto_type *howto;

  howto = howto_table + rel->r_type;

#ifdef COFF_WITH_PE
  *addendp = 0;
#endif

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

#ifndef COFF_WITH_PE
      /* I think we *do* want to bypass this.  If we don't, I have
	 seen some data parameters get the wrong relocation address.
	 If I link two versions with and without this section bypassed
	 and then do a binary comparison, the addresses which are
	 different can be looked up in the map.  The case in which
	 this section has been bypassed has addresses which correspond
	 to values I can find in the map.  */
      *addendp -= sym->n_value;
#endif
    }

#ifndef COFF_WITH_PE
  /* If the output symbol is common (in which case this must be a
     relocateable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common) 
    *addendp += h->root.u.c.size;
#endif

#ifdef COFF_WITH_PE
  if (howto->pc_relative)
    {
      *addendp -= 4;

      /* If the symbol is defined, then the generic code is going to
         add back the symbol value in order to cancel out an
         adjustment it made to the addend.  However, we set the addend
         to 0 at the start of this function.  We need to adjust here,
         to avoid the adjustment the generic code will make.  FIXME:
         This is getting a bit hackish.  */
      if (sym != NULL && sym->n_scnum != 0)
	*addendp -= sym->n_value;
    }

  if (rel->r_type == R_IMAGEBASE)
    {
      *addendp -= pe_data(sec->output_section->owner)->pe_opthdr.ImageBase;
    }
#endif

  return howto;
}


#define coff_bfd_reloc_type_lookup coff_i386_reloc_type_lookup


static reloc_howto_type *
coff_i386_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  switch (code)
    {
    case BFD_RELOC_RVA:
      return howto_table +R_IMAGEBASE;
    case BFD_RELOC_32:
      return howto_table + R_DIR32;
    case BFD_RELOC_32_PCREL:
      return howto_table + R_PCRLONG;
    default:
      BFD_FAIL ();
      return 0;
    }
}

#define coff_rtype_to_howto coff_i386_rtype_to_howto

#ifdef TARGET_UNDERSCORE

/* If i386 gcc uses underscores for symbol names, then it does not use
   a leading dot for local labels, so if TARGET_UNDERSCORE is defined
   we treat all symbols starting with L as local.  */

static boolean coff_i386_is_local_label_name PARAMS ((bfd *, const char *));

static boolean
coff_i386_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == 'L')
    return true;

  return _bfd_coff_is_local_label_name (abfd, name);
}

#define coff_bfd_is_local_label_name coff_i386_is_local_label_name

#endif /* TARGET_UNDERSCORE */

#include "coffcode.h"

static const bfd_target *
i3coff_object_p (abfd)
     bfd *abfd;
{
#ifdef COFF_IMAGE_WITH_PE
  /* We need to hack badly to handle a PE image correctly.  In PE
     images created by the GNU linker, the offset to the COFF header
     is always the size.  However, this is not the case in images
     generated by other PE linkers.  The PE format stores a four byte
     offset to the PE signature just before the COFF header at
     location 0x3c of the file.  We pick up that offset, verify that
     the PE signature is there, and then set ourselves up to read in
     the COFF header.  */
  {
    bfd_byte ext_offset[4];
    file_ptr offset;
    bfd_byte ext_signature[4];
    unsigned long signature;

    if (bfd_seek (abfd, 0x3c, SEEK_SET) != 0
	|| bfd_read (ext_offset, 1, 4, abfd) != 4)
      {
	if (bfd_get_error () != bfd_error_system_call)
	  bfd_set_error (bfd_error_wrong_format);
	return NULL;
      }
    offset = bfd_h_get_32 (abfd, ext_offset);
    if (bfd_seek (abfd, offset, SEEK_SET) != 0
	|| bfd_read (ext_signature, 1, 4, abfd) != 4)
      {
	if (bfd_get_error () != bfd_error_system_call)
	  bfd_set_error (bfd_error_wrong_format);
	return NULL;
      }
    signature = bfd_h_get_32 (abfd, ext_signature);

    if (signature != 0x4550)
      {
	bfd_set_error (bfd_error_wrong_format);
	return NULL;
      }

    /* Here is the hack.  coff_object_p wants to read filhsz bytes to
       pick up the COFF header.  We adjust so that that will work.  20
       is the size of the i386 COFF filehdr.  */

    if (bfd_seek (abfd,
		  (bfd_tell (abfd)
		   - bfd_coff_filhsz (abfd)
		   + 20),
		  SEEK_SET)
	!= 0)
      {
	if (bfd_get_error () != bfd_error_system_call)
	  bfd_set_error (bfd_error_wrong_format);
	return NULL;
      }
  }
#endif

  return coff_object_p (abfd);
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
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

#ifndef COFF_WITH_PE
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC /* section flags */
   | SEC_CODE | SEC_DATA),
#else
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC /* section flags */
   | SEC_CODE | SEC_DATA
   | SEC_LINK_ONCE | SEC_LINK_DUPLICATES),
#endif

#ifdef TARGET_UNDERSCORE
  TARGET_UNDERSCORE,		/* leading underscore */
#else
  0,				/* leading underscore */
#endif
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

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
