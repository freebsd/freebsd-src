/* ldwrite.c -- write out the linked file
   Copyright (C) 1991, 92, 93, 94, 95, 96, 97, 98, 2000
   Free Software Foundation, Inc.
   Written by Steve Chamberlain sac@cygnus.com

This file is part of GLD, the Gnu Linker.

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
#include "bfdlink.h"
#include "libiberty.h"

#include "ld.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldwrite.h"
#include "ldmisc.h"
#include "ldgram.h"
#include "ldmain.h"

static void build_link_order PARAMS ((lang_statement_union_type *));
static asection *clone_section PARAMS ((bfd *, asection *, const char *, int *));
static void split_sections PARAMS ((bfd *, struct bfd_link_info *));

/* Build link_order structures for the BFD linker.  */

static void
build_link_order (statement)
     lang_statement_union_type *statement;
{
  switch (statement->header.type)
    {
    case lang_data_statement_enum:
      {
	asection *output_section;
	struct bfd_link_order *link_order;
	bfd_vma value;
	boolean big_endian = false;

	output_section = statement->data_statement.output_section;
	ASSERT (output_section->owner == output_bfd);

	link_order = bfd_new_link_order (output_bfd, output_section);
	if (link_order == NULL)
	  einfo (_("%P%F: bfd_new_link_order failed\n"));

	link_order->type = bfd_data_link_order;
	link_order->offset = statement->data_statement.output_vma;
	link_order->u.data.contents = (bfd_byte *) xmalloc (QUAD_SIZE);

	value = statement->data_statement.value;

	/* If the endianness of the output BFD is not known, then we
	   base the endianness of the data on the first input file.
	   By convention, the bfd_put routines for an unknown
	   endianness are big endian, so we must swap here if the
	   input file is little endian.  */
	if (bfd_big_endian (output_bfd))
	  big_endian = true;
	else if (bfd_little_endian (output_bfd))
	  big_endian = false;
	else
	  {
	    boolean swap;

	    swap = false;
	    if (command_line.endian == ENDIAN_BIG)
	      big_endian = true;
	    else if (command_line.endian == ENDIAN_LITTLE)
	      {
		big_endian = false;
		swap = true;
	      }
	    else if (command_line.endian == ENDIAN_UNSET)
	      {
		big_endian = true;
		{
		  LANG_FOR_EACH_INPUT_STATEMENT (s)
		    {
		      if (s->the_bfd != NULL)
			{
			  if (bfd_little_endian (s->the_bfd))
			    {
			      big_endian = false;
			      swap = true;
			    }
			  break;
			}
		    }
		}
	      }

	    if (swap)
	      {
		bfd_byte buffer[8];

		switch (statement->data_statement.type)
		  {
		  case QUAD:
		  case SQUAD:
		    if (sizeof (bfd_vma) >= QUAD_SIZE)
		      {
			bfd_putl64 (value, buffer);
			value = bfd_getb64 (buffer);
			break;
		      }
		    /* Fall through.  */
		  case LONG:
		    bfd_putl32 (value, buffer);
		    value = bfd_getb32 (buffer);
		    break;
		  case SHORT:
		    bfd_putl16 (value, buffer);
		    value = bfd_getb16 (buffer);
		    break;
		  case BYTE:
		    break;
		  default:
		    abort ();
		  }
	      }
	  }

	ASSERT (output_section->owner == output_bfd);
	switch (statement->data_statement.type)
	  {
	  case QUAD:
	  case SQUAD:
	    if (sizeof (bfd_vma) >= QUAD_SIZE)
	      bfd_put_64 (output_bfd, value, link_order->u.data.contents);
	    else
	      {
		bfd_vma high;

		if (statement->data_statement.type == QUAD)
		  high = 0;
		else if ((value & 0x80000000) == 0)
		  high = 0;
		else
		  high = (bfd_vma) -1;
		bfd_put_32 (output_bfd, high,
			    (link_order->u.data.contents
			     + (big_endian ? 0 : 4)));
		bfd_put_32 (output_bfd, value,
			    (link_order->u.data.contents
			     + (big_endian ? 4 : 0)));
	      }
	    link_order->size = QUAD_SIZE;
	    break;
	  case LONG:
	    bfd_put_32 (output_bfd, value, link_order->u.data.contents);
	    link_order->size = LONG_SIZE;
	    break;
	  case SHORT:
	    bfd_put_16 (output_bfd, value, link_order->u.data.contents);
	    link_order->size = SHORT_SIZE;
	    break;
	  case BYTE:
	    bfd_put_8 (output_bfd, value, link_order->u.data.contents);
	    link_order->size = BYTE_SIZE;
	    break;
	  default:
	    abort ();
	  }
      }
      break;

    case lang_reloc_statement_enum:
      {
	lang_reloc_statement_type *rs;
	asection *output_section;
	struct bfd_link_order *link_order;

	rs = &statement->reloc_statement;

	output_section = rs->output_section;
	ASSERT (output_section->owner == output_bfd);

	link_order = bfd_new_link_order (output_bfd, output_section);
	if (link_order == NULL)
	  einfo (_("%P%F: bfd_new_link_order failed\n"));

	link_order->offset = rs->output_vma;
	link_order->size = bfd_get_reloc_size (rs->howto);

	link_order->u.reloc.p =
	  ((struct bfd_link_order_reloc *)
	   xmalloc (sizeof (struct bfd_link_order_reloc)));

	link_order->u.reloc.p->reloc = rs->reloc;
	link_order->u.reloc.p->addend = rs->addend_value;

	if (rs->name == NULL)
	  {
	    link_order->type = bfd_section_reloc_link_order;
	    if (rs->section->owner == output_bfd)
	      link_order->u.reloc.p->u.section = rs->section;
	    else
	      {
		link_order->u.reloc.p->u.section = rs->section->output_section;
		link_order->u.reloc.p->addend += rs->section->output_offset;
	      }
	  }
	else
	  {
	    link_order->type = bfd_symbol_reloc_link_order;
	    link_order->u.reloc.p->u.name = rs->name;
	  }
      }
      break;

    case lang_input_section_enum:
      /* Create a new link_order in the output section with this
	 attached */
      if (statement->input_section.ifile->just_syms_flag == false)
	{
	  asection *i = statement->input_section.section;
	  asection *output_section = i->output_section;

	  ASSERT (output_section->owner == output_bfd);

	  if ((output_section->flags & SEC_HAS_CONTENTS) != 0)
	    {
	      struct bfd_link_order *link_order;

	      link_order = bfd_new_link_order (output_bfd, output_section);

	      if (i->flags & SEC_NEVER_LOAD)
		{
		  /* We've got a never load section inside one which
		     is going to be output, we'll change it into a
		     fill link_order */
		  link_order->type = bfd_fill_link_order;
		  link_order->u.fill.value = 0;
		}
	      else
		{
		  link_order->type = bfd_indirect_link_order;
		  link_order->u.indirect.section = i;
		  ASSERT (i->output_section == output_section);
		}
	      if (i->_cooked_size)
		link_order->size = i->_cooked_size;
	      else
		link_order->size = bfd_get_section_size_before_reloc (i);
	      link_order->offset = i->output_offset;
	    }
	}
      break;

    case lang_padding_statement_enum:
      /* Make a new link_order with the right filler */
      {
	asection *output_section;
	struct bfd_link_order *link_order;

	output_section = statement->padding_statement.output_section;
	ASSERT (statement->padding_statement.output_section->owner
		== output_bfd);
	if ((output_section->flags & SEC_HAS_CONTENTS) != 0)
	  {
	    link_order = bfd_new_link_order (output_bfd, output_section);
	    link_order->type = bfd_fill_link_order;
	    link_order->size = statement->padding_statement.size;
	    link_order->offset = statement->padding_statement.output_offset;
	    link_order->u.fill.value = statement->padding_statement.fill;
	  }
      }
      break;

    default:
      /* All the other ones fall through */
      break;
    }
}

/* Call BFD to write out the linked file.  */

/**********************************************************************/

/* Wander around the input sections, make sure that
   we'll never try and create an output section with more relocs
   than will fit.. Do this by always assuming the worst case, and
   creating new output sections with all the right bits.  */
#define TESTIT 1
static asection *
clone_section (abfd, s, name, count)
     bfd *abfd;
     asection *s;
     const char *name;
     int *count;
{
  char templ[6];
  char *sname;
  asection *n;
  struct bfd_link_hash_entry *h;

  /* Invent a section name from the first five chars of the base
     section name and a digit suffix.  */
  strncpy (templ, name, sizeof (templ) - 1);
  templ[sizeof (templ) - 1] = '\0';
  if ((sname = bfd_get_unique_section_name (abfd, templ, count)) == NULL
      || (n = bfd_make_section_anyway (abfd, sname)) == NULL
      || (h = bfd_link_hash_lookup (link_info.hash,
				    sname, true, true, false)) == NULL)
    {
      einfo (_("%F%P: clone section failed: %E\n"));
      /* Silence gcc warnings.  einfo exits, so we never reach here.  */
      return NULL;
    }

  /* Set up section symbol.  */
  h->type = bfd_link_hash_defined;
  h->u.def.value = 0;
  h->u.def.section = n;

  n->flags = s->flags;
  n->vma = s->vma;
  n->user_set_vma = s->user_set_vma;
  n->lma = s->lma;
  n->_cooked_size = 0;
  n->_raw_size = 0;
  n->output_offset = s->output_offset;
  n->output_section = n;
  n->orelocation = 0;
  n->reloc_count = 0;
  n->alignment_power = s->alignment_power;
  return n;
}

#if TESTING
static void
ds (s)
     asection *s;
{
  struct bfd_link_order *l = s->link_order_head;
  printf ("vma %x size %x\n", s->vma, s->_raw_size);
  while (l)
    {
      if (l->type == bfd_indirect_link_order)
	{
	  printf ("%8x %s\n", l->offset, l->u.indirect.section->owner->filename);
	}
      else
	{
	  printf (_("%8x something else\n"), l->offset);
	}
      l = l->next;
    }
  printf ("\n");
}

dump (s, a1, a2)
     char *s;
     asection *a1;
     asection *a2;
{
  printf ("%s\n", s);
  ds (a1);
  ds (a2);
}

static void
sanity_check (abfd)
     bfd *abfd;
{
  asection *s;
  for (s = abfd->sections; s; s = s->next)
    {
      struct bfd_link_order *p;
      bfd_vma prev = 0;
      for (p = s->link_order_head; p; p = p->next)
	{
	  if (p->offset > 100000)
	    abort ();
	  if (p->offset < prev)
	    abort ();
	  prev = p->offset;
	}
    }
}
#else
#define sanity_check(a)
#define dump(a, b, c)
#endif

static void
split_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection *original_sec;
  int nsecs = abfd->section_count;
  sanity_check (abfd);
  /* Look through all the original sections.  */
  for (original_sec = abfd->sections;
       original_sec && nsecs;
       original_sec = original_sec->next, nsecs--)
    {
      int count = 0;
      unsigned int lines = 0;
      unsigned int relocs = 0;
      bfd_size_type sec_size = 0;
      struct bfd_link_order *l;
      struct bfd_link_order *p;
      bfd_vma vma = original_sec->vma;
      asection *cursor = original_sec;

      /* Count up the relocations and line entries to see if anything
	 would be too big to fit.  Accumulate section size too.  */
      for (l = NULL, p = cursor->link_order_head; p != NULL; p = l->next)
	{
	  unsigned int thislines = 0;
	  unsigned int thisrelocs = 0;
	  bfd_size_type thissize = 0;
	  if (p->type == bfd_indirect_link_order)
	    {
	      asection *sec;

	      sec = p->u.indirect.section;

	      if (info->strip == strip_none
		  || info->strip == strip_some)
		thislines = sec->lineno_count;

	      if (info->relocateable)
		thisrelocs = sec->reloc_count;

	      if (sec->_cooked_size != 0)
		thissize = sec->_cooked_size;
	      else
		thissize = sec->_raw_size;

	    }
	  else if (info->relocateable
		   && (p->type == bfd_section_reloc_link_order
		       || p->type == bfd_symbol_reloc_link_order))
	    thisrelocs++;

	  if (l != NULL
	      && (thisrelocs + relocs >= config.split_by_reloc
		  || thislines + lines >= config.split_by_reloc
		  || thissize + sec_size >= config.split_by_file))
	    {
	      /* Create a new section and put this link order and the
		 following link orders into it.  */
	      bfd_vma shift_offset;
	      asection *n;

	      n = clone_section (abfd, cursor, original_sec->name, &count);

	      /* Attach the link orders to the new section and snip
		 them off from the old section.  */
	      n->link_order_head = p;
	      n->link_order_tail = cursor->link_order_tail;
	      cursor->link_order_tail = l;
	      l->next = NULL;
	      l = p;

	      /* Change the size of the original section and
		 update the vma of the new one.  */

	      dump ("before snip", cursor, n);

	      shift_offset = p->offset;
	      if (cursor->_cooked_size != 0)
		{
		  n->_cooked_size = cursor->_cooked_size - shift_offset;
		  cursor->_cooked_size = shift_offset;
		}
	      n->_raw_size = cursor->_raw_size - shift_offset;
	      cursor->_raw_size = shift_offset;

	      vma += shift_offset;
	      n->lma = n->vma = vma;

	      /* Run down the chain and change the output section to
		 the right one, update the offsets too.  */
	      do
		{
		  p->offset -= shift_offset;
		  if (p->type == bfd_indirect_link_order)
		    {
		      p->u.indirect.section->output_section = n;
		      p->u.indirect.section->output_offset = p->offset;
		    }
		  p = p->next;
		}
	      while (p);

	      dump ("after snip", cursor, n);
	      cursor = n;
	      relocs = thisrelocs;
	      lines = thislines;
	      sec_size = thissize;
	    }
	  else
	    {
	      l = p;
	      relocs += thisrelocs;
	      lines += thislines;
	      sec_size += thissize;
	    }
	}
    }
  sanity_check (abfd);
}

/**********************************************************************/

void
ldwrite ()
{
  /* Reset error indicator, which can typically something like invalid
     format from opening up the .o files.  */
  bfd_set_error (bfd_error_no_error);
  lang_for_each_statement (build_link_order);

  if (config.split_by_reloc != (unsigned) -1
      || config.split_by_file != (bfd_size_type) -1)
    split_sections (output_bfd, &link_info);
  if (!bfd_final_link (output_bfd, &link_info))
    {
      /* If there was an error recorded, print it out.  Otherwise assume
	 an appropriate error message like unknown symbol was printed
	 out.  */

      if (bfd_get_error () != bfd_error_no_error)
	einfo (_("%F%P: final link failed: %E\n"));
      else
	xexit (1);
    }
}
