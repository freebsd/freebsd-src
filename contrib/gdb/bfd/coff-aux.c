/* BFD back-end for Apple M68K COFF A/UX 3.x files.
   Copyright 1996 Free Software Foundation, Inc.
   Portions written by Richard Henderson <rth@tamu.edu>,
   COMMON symbol munging cribbed from cf-m68klynx.c which was
   written by Cygnus Support.

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

#define TARGET_SYM	m68kaux_coff_vec
#define TARGET_NAME	"coff-m68k-aux"

#ifndef TARG_AUX
#define TARG_AUX
#endif

#define COFF_LONG_FILENAMES

/* 4k pages */
#define COFF_PAGE_SIZE 0x1000

/* On AUX, a STYP_NOLOAD|STYP_BSS section is part of a shared library. */
#define BSS_NOLOAD_IS_SHARED_LIBRARY

#define _bfd_m68kcoff_howto_table _bfd_m68kaux_howto_table	
#define _bfd_m68kcoff_rtype2howto _bfd_m68kaux_rtype2howto	
#define _bfd_m68kcoff_howto2rtype _bfd_m68kaux_howto2rtype	
#define _bfd_m68kcoff_reloc_type_lookup _bfd_m68kaux_reloc_type_lookup

/* Rather than change names lots of places, reuse the same hack */
#define LYNX_SPECIAL_FN _bfd_m68kaux_special_fn

#include "bfd.h"
#include "sysdep.h"

#ifdef ANSI_PROTOTYPES
struct internal_reloc;
struct coff_link_hash_entry;
struct internal_syment;
#endif


static bfd_reloc_status_type _bfd_m68kaux_special_fn
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *coff_m68k_aux_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,
	   bfd_vma *));
static boolean coff_m68k_aux_link_add_one_symbol
  PARAMS ((struct bfd_link_info *, bfd *, const char *, flagword, 
           asection *, bfd_vma, const char *, boolean, boolean, 
           struct bfd_link_hash_entry **));


#define coff_rtype_to_howto coff_m68k_aux_rtype_to_howto
#define coff_link_add_one_symbol coff_m68k_aux_link_add_one_symbol


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
    if (ptr && (reloc.r_type == R_PCRBYTE			\
		|| reloc.r_type == R_PCRWORD			\
		|| reloc.r_type == R_PCRLONG))			\
      cache_ptr->addend += asect->vma;				\
  }



#include "coff/aux-coff.h"  /* override coff/internal.h and coff/m68k.h */
#include "coff-m68k.c"



/* For some reason when using m68k COFF the value stored in the .text
   section for a reference to a common symbol is the value itself plus
   any desired offset.  (taken from work done by Ian Taylor, Cygnus Support,
   for I386 COFF).  */

/* If we are producing relocateable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type
_bfd_m68kaux_special_fn (abfd, reloc_entry, symbol, data, input_section,
			  output_bfd, error_message)
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
  x = ((x & ~howto->dst_mask) | \
       (((x & howto->src_mask) + diff) & howto->dst_mask))

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


/* coff-m68k.c uses the special COFF backend linker.  We need to
   adjust common symbols.  */

/*ARGSUSED*/
static reloc_howto_type *
coff_m68k_aux_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{
  arelent relent;
  reloc_howto_type *howto;

  RTYPE2HOWTO (&relent, rel);

  howto = relent.howto;

  if (sym != NULL && sym->n_scnum == 0 && sym->n_value != 0)
    {
      /* This is a common symbol.  The section contents include the
	 size (sym->n_value) as an addend.  The relocate_section
	 function will be adding in the final value of the symbol.  We
	 need to subtract out the current size in order to get the
	 correct result.  */
      BFD_ASSERT (h != NULL);
      *addendp -= sym->n_value;
    }

  /* If the output symbol is common (in which case this must be a
     relocateable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common)
    *addendp += h->root.u.c.size;

  return howto;
}


/* We need non-absolute symbols to override absolute symbols.  This
   mirrors Apple's "solution" to let a static library symbol override
   a shared library symbol.  On the whole not a good thing, given how
   shared libraries work here, but can work if you are careful with
   what you include in the shared object. */

boolean
coff_m68k_aux_link_add_one_symbol (info, abfd, name, flags, section, value,
				   string, copy, collect, hashp)
     struct bfd_link_info *info;
     bfd *abfd;
     const char *name;
     flagword flags;
     asection *section;
     bfd_vma value;
     const char *string;
     boolean copy;
     boolean collect;
     struct bfd_link_hash_entry **hashp;
{
  struct bfd_link_hash_entry *h;

  if ((flags & (BSF_WARNING | BSF_CONSTRUCTOR | BSF_WEAK)) == 0 &&
      !bfd_is_und_section (section) &&
      !bfd_is_com_section (section))
    {
      /* The new symbol is a definition or an indirect definition */

      /* This bit copied from linker.c */
      if (hashp != NULL && *hashp != NULL)
	{
	  h = *hashp;
	  BFD_ASSERT (strcmp (h->root.string, name) == 0);
	}
      else
	{
	  h = bfd_link_hash_lookup (info->hash, name, true, copy, false);
	  if (h == NULL)
	    {
	      if (hashp != NULL)
		*hashp = NULL;
	      return false;
	    }
	}

      if (info->notice_hash != (struct bfd_hash_table *) NULL
	  && (bfd_hash_lookup (info->notice_hash, name, false, false)
	      != (struct bfd_hash_entry *) NULL))
	{
	  if (! (*info->callbacks->notice) (info, name, abfd, section, value))
	    return false;
	}

      if (hashp != (struct bfd_link_hash_entry **) NULL)
	*hashp = h;
      /* end duplication from linker.c */

      if (h->type == bfd_link_hash_defined
	  || h->type == bfd_link_hash_indirect)
	{
	  asection *msec;

	  if (h->type == bfd_link_hash_defined)
	    msec = h->u.def.section;
	  else
	    msec = bfd_ind_section_ptr;

	  if (bfd_is_abs_section (msec) && !bfd_is_abs_section (section))
	    {
	      h->u.def.section = section;
	      h->u.def.value = value;
	      return true;
	    }
	  else if (bfd_is_abs_section (section) && !bfd_is_abs_section (msec))
	    return true;
	}
    }

  /* If we didn't exit early, finish processing in the generic routine */
  return _bfd_generic_link_add_one_symbol (info, abfd, name, flags, section,
					   value, string, copy, collect,
					   hashp);
}
