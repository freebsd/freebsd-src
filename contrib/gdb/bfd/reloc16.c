/* 8 and 16 bit COFF relocation functions, for BFD.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
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

/* 
Most of this hacked by  Steve Chamberlain,
			sac@cygnus.com 
*/

/* These routines are used by coff-h8300 and coff-z8k to do
   relocation.

   FIXME: This code should be rewritten to support the new COFF
   linker.  Basically, they need to deal with COFF relocs rather than
   BFD generic relocs.  They should store the relocs in some location
   where coff_link_input_bfd can find them (and coff_link_input_bfd
   should be changed to use this location rather than rereading the
   file) (unless info->keep_memory is false, in which case they should
   free up the relocs after dealing with them).  */

#include "bfd.h"
#include "sysdep.h"
#include "obstack.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "coff/internal.h"
#include "libcoff.h"

bfd_vma
bfd_coff_reloc16_get_value (reloc, link_info, input_section)
     arelent *reloc;
     struct bfd_link_info *link_info;
     asection *input_section;
{
  bfd_vma value;
  asymbol *symbol = *(reloc->sym_ptr_ptr);
  /* A symbol holds a pointer to a section, and an offset from the
     base of the section.  To relocate, we find where the section will
     live in the output and add that in */

  if (bfd_is_und_section (symbol->section))
    {
      struct bfd_link_hash_entry *h;

      /* The symbol is undefined in this BFD.  Look it up in the
	 global linker hash table.  FIXME: This should be changed when
	 we convert this stuff to use a specific final_link function
	 and change the interface to bfd_relax_section to not require
	 the generic symbols.  */
      h = bfd_wrapped_link_hash_lookup (input_section->owner, link_info,
					bfd_asymbol_name (symbol),
					false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && (h->type == bfd_link_hash_defined
	      || h->type == bfd_link_hash_defweak))
	value = (h->u.def.value
		 + h->u.def.section->output_section->vma
		 + h->u.def.section->output_offset);
      else if (h != (struct bfd_link_hash_entry *) NULL
	       && h->type == bfd_link_hash_common)
	value = h->u.c.size;
      else
	{
	  if (! ((*link_info->callbacks->undefined_symbol)
		 (link_info, bfd_asymbol_name (symbol),
		  input_section->owner, input_section, reloc->address)))
	    abort ();
	  value = 0;
	}
    }
  else 
    {
      value = symbol->value +
	symbol->section->output_offset +
	  symbol->section->output_section->vma;
    }
  
  /* Add the value contained in the relocation */
  value += reloc->addend;
  
  return value;
}

void
bfd_perform_slip(abfd, slip, input_section, value)
     bfd *abfd;
     unsigned int slip;
     asection *input_section;
     bfd_vma value;
{
  asymbol **s;

  s = _bfd_generic_link_get_symbols (abfd);
  BFD_ASSERT (s != (asymbol **) NULL);

  /* Find all symbols past this point, and make them know
     what's happened */
  while (*s) 
    {
      asymbol *p = *s;
      if (p->section == input_section) 
	{
	  /* This was pointing into this section, so mangle it */
	  if (p->value > value)
	    {
	      p->value -= slip;
	      if (p->udata.p != NULL)
		{
		  struct generic_link_hash_entry *h;

		  h = (struct generic_link_hash_entry *) p->udata.p;
		  BFD_ASSERT (h->root.type == bfd_link_hash_defined
			      || h->root.type == bfd_link_hash_defweak);
		  h->root.u.def.value -= slip;
		  BFD_ASSERT (h->root.u.def.value == p->value);
		}
	    }
	}
      s++;
    }    
}

boolean 
bfd_coff_reloc16_relax_section (abfd, i, link_info, again)
     bfd *abfd;
     asection *i;
     struct bfd_link_info *link_info;
     boolean *again;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = i->owner;
  asection *input_section = i;
  int shrink = 0 ;
  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;

  /* We only run this relaxation once.  It might work to run it more
     often, but it hasn't been tested.  */
  *again = false;

  if (reloc_size < 0)
    return false;

  reloc_vector = (arelent **) bfd_malloc (reloc_size);
  if (!reloc_vector && reloc_size > 0)
    return false;

  /* Get the relocs and think about them */
  reloc_count =
    bfd_canonicalize_reloc (input_bfd, input_section, reloc_vector,
			    _bfd_generic_link_get_symbols (input_bfd));
  if (reloc_count < 0)
    {
      free (reloc_vector);
      return false;
    }

  if (reloc_count > 0)
    {
      arelent **parent;
      for (parent = reloc_vector; *parent; parent++) 
	{
	  shrink = bfd_coff_reloc16_estimate (abfd, input_section,
					      *parent, shrink, link_info);
	}
    }

  input_section->_cooked_size -= shrink;  
  free((char *)reloc_vector);
  return true;
}

bfd_byte *
bfd_coff_reloc16_get_relocated_section_contents(in_abfd,
						link_info,
						link_order,
						data,
						relocateable,
						symbols)
     bfd *in_abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;
  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector;
  long reloc_count;

  if (reloc_size < 0)
    return NULL;

  /* If producing relocateable output, don't bother to relax.  */
  if (relocateable)
    return bfd_generic_get_relocated_section_contents (in_abfd, link_info,
						       link_order,
						       data, relocateable,
						       symbols);

  /* read in the section */
  if (! bfd_get_section_contents(input_bfd,
				 input_section,
				 data,
				 0,
				 input_section->_raw_size))
    return NULL;
  
  
  reloc_vector = (arelent **) bfd_malloc((size_t) reloc_size);
  if (!reloc_vector && reloc_size != 0)
    return NULL;
  
  reloc_count = bfd_canonicalize_reloc (input_bfd, 
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    {
      free (reloc_vector);
      return NULL;
    }
    
  if (reloc_count > 0)
    {
      arelent **parent = reloc_vector;
      arelent *reloc ;
      unsigned int dst_address = 0;
      unsigned int src_address = 0;
      unsigned int run;
      unsigned int idx;
    
      /* Find how long a run we can do */
      while (dst_address < link_order->size) 
	{
	  reloc = *parent;
	  if (reloc) 
	    {
	      /* Note that the relaxing didn't tie up the addresses in the
		 relocation, so we use the original address to work out the
		 run of non-relocated data */
	      run = reloc->address - src_address;
	      parent++;
	    }
	  else 
	    {
	      run = link_order->size - dst_address;
	    }
	  /* Copy the bytes */
	  for (idx = 0; idx < run; idx++)
	    {
	      data[dst_address++] = data[src_address++];
	    }
    
	  /* Now do the relocation */
    
	  if (reloc) 
	    {
	      bfd_coff_reloc16_extra_cases (input_bfd, link_info, link_order,
					    reloc, data, &src_address,
					    &dst_address);
	    }    
	}
    }
  free((char *)reloc_vector);
  return data;
}

