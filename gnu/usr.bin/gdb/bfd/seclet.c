/* seclet.c
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
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

/* This module is part of BFD */


/* The intention is that one day, all the code which uses sections
   will change and use seclets instead - maybe seglet would have been
   a better name..

   Anyway, a seclet contains enough info to be able to describe an
   area of output memory in one go.

   The only description so far catered for is that of the
   <<bfd_indirect_seclet>>, which is a select which points to a
   <<section>> and the <<asymbols>> associated with the section, so
   that relocation can be done when needed.

   One day there will be more types - they will at least migrate from
   the linker's data structures - also there could be extra stuff,
   like a bss seclet, which descibes a lump of memory as containing
   zeros compactly, without the horrible SEC_* flag cruft.


*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "seclet.h"
#include "coff/internal.h"

/* Create a new seclet and attach it to a section.  */

bfd_seclet_type *
DEFUN(bfd_new_seclet,(abfd, section),
      bfd *abfd AND
      asection *section)
{
  bfd_seclet_type *n = (bfd_seclet_type *)bfd_alloc(abfd, sizeof(bfd_seclet_type));
  if (section->seclets_tail != (bfd_seclet_type *)NULL) {
      section->seclets_tail->next = n;
    }
  else
  {
    section->seclets_head = n;
  }
  section->seclets_tail = n;

  return n;
}

/* Given an indirect seclet which points to an input section, relocate
   the contents of the seclet and put the data in its final
   destination.  */

static boolean
DEFUN(rel,(abfd, seclet, output_section, data, relocateable),
      bfd *abfd AND
      bfd_seclet_type *seclet AND
      asection *output_section AND
      PTR data AND
      boolean relocateable)
{
  if ((output_section->flags & SEC_HAS_CONTENTS) != 0
      && seclet->size)
  {
    data = (PTR) bfd_get_relocated_section_contents(abfd, seclet, data,
						    relocateable);
    if(bfd_set_section_contents(abfd,
				output_section,
				data,
				seclet->offset,
				seclet->size) == false)
    {
      abort();
    }
  }
  return true;
}

/* Put the contents of a seclet in its final destination.  */

static boolean
DEFUN(seclet_dump_seclet,(abfd, seclet, section, data, relocateable),
      bfd *abfd AND
      bfd_seclet_type *seclet AND
      asection *section AND
      PTR data AND
      boolean relocateable)
{
  switch (seclet->type) 
    {
    case bfd_indirect_seclet:
      /* The contents of this section come from another one somewhere
	 else */
      return rel(abfd, seclet, section, data, relocateable);

    case bfd_fill_seclet:
      /* Fill in the section with us */
      {
	char *d = bfd_xmalloc(seclet->size);
	unsigned int i;
	for (i =0;  i < seclet->size; i+=2) {
	  d[i] = seclet->u.fill.value >> 8;
	}
	for (i = 1; i < seclet->size; i+=2) {
	  d[i] = seclet->u.fill.value ;
	}
	/* Don't bother to fill in empty sections */
	if (!(bfd_get_section_flags(abfd, section) & SEC_HAS_CONTENTS))
	  {
	    return true;
	  }
	return bfd_set_section_contents(abfd, section, d, seclet->offset,
					seclet->size);
      }

    default:
      abort();
    }

  return true;
}

/*
INTERNAL_FUNCTION
	bfd_generic_seclet_link

SYNOPSIS
	boolean bfd_generic_seclet_link
	 (bfd *abfd,
	  PTR data,
	  boolean relocateable);

DESCRIPTION

	The generic seclet linking routine.  The caller should have
	set up seclets for all the output sections.  The DATA argument
	should point to a memory area large enough to hold the largest
	section.  This function looks through the seclets and moves
	the contents into the output sections.  If RELOCATEABLE is
	true, the orelocation fields of the output sections must
	already be initialized.

*/

boolean
DEFUN(bfd_generic_seclet_link,(abfd, data, relocateable),
      bfd *abfd AND
      PTR data AND
      boolean relocateable)
{
  asection *o = abfd->sections;
  while (o != (asection *)NULL) 
  {
    bfd_seclet_type *p = o->seclets_head;
    while (p != (bfd_seclet_type *)NULL) 
    {
      if (seclet_dump_seclet(abfd, p, o, data, relocateable) == false)
	return false;
      p = p ->next;
    }
    o = o->next;
  }

  return true;
}
