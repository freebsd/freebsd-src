/* ldctor.c -- constructor support routines
   Copyright (C) 1991, 92, 93, 94, 1995 Free Software Foundation, Inc.
   By Steve Chamberlain <sac@cygnus.com>
   
This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldmisc.h"
#include "ldgram.h"
#include "ldmain.h"
#include "ldctor.h"

/* The list of statements needed to handle constructors.  These are
   invoked by the command CONSTRUCTORS in the linker script.  */
lang_statement_list_type constructor_list;

/* The sets we have seen.  */
struct set_info *sets;

/* Add an entry to a set.  H is the entry in the linker hash table.
   RELOC is the relocation to use for an entry in the set.  SECTION
   and VALUE are the value to add.  This is called during the first
   phase of the link, when we are still gathering symbols together.
   We just record the information now.  The ldctor_find_constructors
   function will construct the sets.  */

void
ldctor_add_set_entry (h, reloc, name, section, value)
     struct bfd_link_hash_entry *h;
     bfd_reloc_code_real_type reloc;
     const char *name;
     asection *section;
     bfd_vma value;
{
  struct set_info *p;
  struct set_element *e;
  struct set_element **epp;

  for (p = sets; p != (struct set_info *) NULL; p = p->next)
    if (p->h == h)
      break;

  if (p == (struct set_info *) NULL)
    {
      p = (struct set_info *) xmalloc (sizeof (struct set_info));
      p->next = sets;
      sets = p;
      p->h = h;
      p->reloc = reloc;
      p->count = 0;
      p->elements = NULL;
    }
  else
    {
      if (p->reloc != reloc)
	{
	  einfo ("%P%X: Different relocs used in set %s\n", h->root.string);
	  return;
	}

      /* Don't permit a set to be constructed from different object
         file formats.  The same reloc may have different results.  We
         actually could sometimes handle this, but the case is
         unlikely to ever arise.  Sometimes constructor symbols are in
         unusual sections, such as the absolute section--this appears
         to be the case in Linux a.out--and in such cases we just
         assume everything is OK.  */
      if (p->elements != NULL
	  && section->owner != NULL
	  && p->elements->section->owner != NULL
	  && strcmp (bfd_get_target (section->owner),
		     bfd_get_target (p->elements->section->owner)) != 0)
	{
	  einfo ("%P%X: Different object file formats composing set %s\n",
		 h->root.string);
	  return;
	}
    }

  e = (struct set_element *) xmalloc (sizeof (struct set_element));
  e->next = NULL;
  e->name = name;
  e->section = section;
  e->value = value;

  for (epp = &p->elements; *epp != NULL; epp = &(*epp)->next)
    ;
  *epp = e;

  ++p->count;
}

/* This function is called after the first phase of the link and
   before the second phase.  At this point all set information has
   been gathered.  We now put the statements to build the sets
   themselves into constructor_list.  */

void
ldctor_build_sets ()
{
  static boolean called;
  lang_statement_list_type *old;
  boolean header_printed;
  struct set_info *p;

  /* The emulation code may call us directly, but we only want to do
     this once.  */
  if (called)
    return;
  called = true;

  old = stat_ptr;
  stat_ptr = &constructor_list;

  lang_list_init (stat_ptr);

  header_printed = false;
  for (p = sets; p != (struct set_info *) NULL; p = p->next)
    {
      struct set_element *e;
      reloc_howto_type *howto;
      int reloc_size, size;

      /* If the symbol is defined, we may have been invoked from
	 collect, and the sets may already have been built, so we do
	 not do anything.  */
      if (p->h->type == bfd_link_hash_defined
	  || p->h->type == bfd_link_hash_defweak)
	continue;

      /* For each set we build:
	   set:
	     .long number_of_elements
	     .long element0
	     ...
	     .long elementN
	     .long 0
	 except that we use the right size instead of .long.  When
	 generating relocateable output, we generate relocs instead of
	 addresses.  */
      howto = bfd_reloc_type_lookup (output_bfd, p->reloc);
      if (howto == (reloc_howto_type *) NULL)
	{
	  if (link_info.relocateable)
	    {
	      einfo ("%P%X: %s does not support reloc %s for set %s\n",
		     bfd_get_target (output_bfd),
		     bfd_get_reloc_code_name (p->reloc),
		     p->h->root.string);
	      continue;
	    }

	  /* If this is not a relocateable link, all we need is the
	     size, which we can get from the input BFD.  */
	  howto = bfd_reloc_type_lookup (p->elements->section->owner,
					 p->reloc);
	  if (howto == NULL)
	    {
	      einfo ("%P%X: %s does not support reloc %s for set %s\n",
		     bfd_get_target (p->elements->section->owner),
		     bfd_get_reloc_code_name (p->reloc),
		     p->h->root.string);
	      continue;
	    }
	}

      reloc_size = bfd_get_reloc_size (howto);
      switch (reloc_size)
	{
	case 1: size = BYTE; break;
	case 2: size = SHORT; break;
	case 4: size = LONG; break;
	case 8:
	  if (howto->complain_on_overflow == complain_overflow_signed)
	    size = SQUAD;
	  else
	    size = QUAD;
	  break;
	default:
	  einfo ("%P%X: Unsupported size %d for set %s\n",
		 bfd_get_reloc_size (howto), p->h->root.string);
	  size = LONG;
	  break;
	}

      lang_add_assignment (exp_assop ('=', ".",
				      exp_unop (ALIGN_K,
						exp_intop (reloc_size))));
      lang_add_assignment (exp_assop ('=', p->h->root.string,
				      exp_nameop (NAME, ".")));
      lang_add_data (size, exp_intop ((bfd_vma) p->count));

      for (e = p->elements; e != (struct set_element *) NULL; e = e->next)
	{
	  if (config.map_file != NULL)
	    {
	      int len;

	      if (! header_printed)
		{
		  minfo ("\nSet                 Symbol\n\n");
		  header_printed = true;
		}

	      minfo ("%s", p->h->root.string);
	      len = strlen (p->h->root.string);

	      if (len >= 19)
		{
		  print_nl ();
		  len = 0;
		}
	      while (len < 20)
		{
		  print_space ();
		  ++len;
		}

	      if (e->name != NULL)
		minfo ("%T\n", e->name);
	      else
		minfo ("%G\n", e->section->owner, e->section, e->value);
	    }

	  if (link_info.relocateable)
	    lang_add_reloc (p->reloc, howto, e->section, e->name,
			    exp_intop (e->value));
	  else
	    lang_add_data (size, exp_relop (e->section, e->value));
	}

      lang_add_data (size, exp_intop (0));
    }

  stat_ptr = old;
}
