# This shell script emits a C file. -*- C -*-
#   Copyright 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
#
# This file is part of GLD, the Gnu Linker.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# This file is sourced from elf32.em and mmo.em, used to define
# linker MMIX-specifics common to ELF and MMO.

cat >>e${EMULATION_NAME}.c <<EOF
/* Need to have this define before mmix-elfnmmo, which includes
   needrelax.em which uses this name for the before_allocation function,
   normally defined in elf32.em.  */
#define gldmmo_before_allocation before_allocation_default
EOF

. ${srcdir}/emultempl/mmix-elfnmmo.em

cat >>e${EMULATION_NAME}.c <<EOF

/* Find the last output section before given output statement.
   Used by place_orphan.  */

static asection *
output_prev_sec_find (lang_output_section_statement_type *os)
{
  asection *s = NULL;
  lang_statement_union_type *u;
  lang_output_section_statement_type *lookup;

  for (u = lang_output_section_statement.head;
       u != (lang_statement_union_type *) NULL;
       u = lookup->next)
    {
      lookup = &u->output_section_statement;
      if (lookup == os)
	break;
      if (lookup->bfd_section != NULL
	  && lookup->bfd_section != bfd_abs_section_ptr
	  && lookup->bfd_section != bfd_com_section_ptr
	  && lookup->bfd_section != bfd_und_section_ptr)
	s = lookup->bfd_section;
    }

  if (u == NULL)
    return NULL;

  return s;
}

struct orphan_save {
  lang_output_section_statement_type *os;
  asection **section;
  lang_statement_union_type **stmt;
};

#define HAVE_SECTION(hold, name) \
(hold.os != NULL || (hold.os = lang_output_section_find (name)) != NULL)

/* Place an orphan section.  We use this to put random SEC_CODE or
   SEC_READONLY sections right after MMO_TEXT_SECTION_NAME.  Much borrowed
   from elf32.em.  */

static bfd_boolean
mmo_place_orphan (lang_input_statement_type *file, asection *s)
{
  static struct orphan_save hold_text;
  struct orphan_save *place;
  lang_output_section_statement_type *os;
  lang_statement_list_type *old;
  lang_statement_list_type add;
  asection *snew, **pps, *bfd_section;

  /* We have nothing to say for anything other than a final link.  */
  if (link_info.relocatable
      || (bfd_get_section_flags (s->owner, s)
	  & (SEC_EXCLUDE | SEC_LOAD)) != SEC_LOAD)
    return FALSE;

  /* Only care for sections we're going to load.  */
  os = lang_output_section_find (bfd_get_section_name (s->owner, s));

  /* We have an output section by this name.  Place the section inside it
     (regardless of whether the linker script lists it as input).  */
  if (os != NULL)
    {
      lang_add_section (&os->children, s, os, file);
      return TRUE;
    }

  /* If this section does not have .text-type section flags or there's no
     MMO_TEXT_SECTION_NAME, we don't have anything to say.  */
  if ((bfd_get_section_flags (s->owner, s) & (SEC_CODE | SEC_READONLY)) == 0)
    return FALSE;

  if (hold_text.os == NULL)
    hold_text.os = lang_output_section_find (MMO_TEXT_SECTION_NAME);

  place = &hold_text;

  /* If there's an output section by this name, we'll use it, regardless
     of section flags, in contrast to what's done in elf32.em.  */

  /* Start building a list of statements for this section.
     First save the current statement pointer.  */
  old = stat_ptr;

  /* Add the output section statements for this orphan to our own private
     list, inserting them later into the global statement list.  */
  stat_ptr = &add;
  lang_list_init (stat_ptr);

  os = lang_enter_output_section_statement (bfd_get_section_name (s->owner,
								  s),
					    NULL, 0,
					    (etree_type *) NULL,
					    (etree_type *) NULL,
					    (etree_type *) NULL);

  lang_add_section (&os->children, s, os, file);

  lang_leave_output_section_statement
    ((bfd_vma) 0, "*default*",
     (struct lang_output_section_phdr_list *) NULL, NULL);

  /* Restore the global list pointer.  */
  stat_ptr = old;

  snew = os->bfd_section;
  if (snew == NULL)
    /* /DISCARD/ section.  */
    return TRUE;

  /* We need an output section for .text as a root, so if there was none
     (might happen with a peculiar linker script such as in "map
     addresses", map-address.exp), we grab the output section created
     above.  */
  if (hold_text.os == NULL)
    {
      if (os == NULL)
	return FALSE;
      hold_text.os = os;
    }

  bfd_section = place->os->bfd_section;
  if (place->section == NULL && bfd_section == NULL)
    bfd_section = output_prev_sec_find (place->os);

  if (place->section != NULL
      || (bfd_section != NULL
	  && bfd_section != snew))
    {
      /* Shuffle the section to make the output file look neater.  This is
	 really only cosmetic.  */
      if (place->section == NULL)
	/* Put orphans after the first section on the list.  */
	place->section = &bfd_section->next;

      /* Unlink the section.  */
      for (pps = &output_bfd->sections; *pps != snew; pps = &(*pps)->next)
	;
      bfd_section_list_remove (output_bfd, pps);

      /* Now tack it on to the "place->os" section list.  */
      bfd_section_list_insert (output_bfd, place->section, snew);
    }
  place->section = &snew->next;	/* Save the end of this list.  */

  if (add.head != NULL)
    {
      /* We try to put the output statements in some sort of reasonable
	 order here, because they determine the final load addresses of
	 the orphan sections.  */
      if (place->stmt == NULL)
	{
	  /* Put the new statement list right at the head.  */
	  *add.tail = place->os->header.next;
	  place->os->header.next = add.head;
	}
      else
	{
	  /* Put it after the last orphan statement we added.  */
	  *add.tail = *place->stmt;
	  *place->stmt = add.head;
	}

      /* Fix the global list pointer if we happened to tack our new list
	 at the tail.  */
      if (*old->tail == add.head)
	old->tail = add.tail;

      /* Save the end of this list.  */
      place->stmt = add.tail;
    }

  return TRUE;
}

/* Remove the spurious settings of SEC_RELOC that make it to the output at
   link time.  We are as confused as elflink.h:elf_bfd_final_link, and
   paper over the bug similarly.  */

static void
mmo_wipe_sec_reloc_flag (bfd *abfd, asection *sec, void *ptr ATTRIBUTE_UNUSED)
{
  bfd_set_section_flags (abfd, sec,
			 bfd_get_section_flags (abfd, sec) & ~SEC_RELOC);
}

/* Iterate with bfd_map_over_sections over mmo_wipe_sec_reloc_flag... */

static void
mmo_finish (void)
{
  bfd_map_over_sections (output_bfd, mmo_wipe_sec_reloc_flag, NULL);
}

/* To get on-demand global register allocation right, we need to parse the
   relocs, like what happens when linking to ELF.  It needs to be done
   before all input sections are supposed to be present.  When linking to
   ELF, it's done when reading symbols.  When linking to mmo, we do it
   when all input files are seen, which is equivalent.  */

static void
mmo_after_open (void)
{
  /* When there's a mismatch between the output format and the emulation
     (using weird combinations like "-m mmo --oformat elf64-mmix" for
     example), we'd count relocs twice because they'd also be counted
     along the usual route for ELF-only linking, which would lead to an
     internal accounting error.  */
  if (bfd_get_flavour (output_bfd) != bfd_target_elf_flavour)
    {
      LANG_FOR_EACH_INPUT_STATEMENT (is)
	{
	  if (bfd_get_flavour (is->the_bfd) == bfd_target_elf_flavour
	      && !_bfd_mmix_check_all_relocs (is->the_bfd, &link_info))
	    einfo ("%X%P: Internal problems scanning %B after opening it",
		   is->the_bfd);
	}
    }
}
EOF

LDEMUL_PLACE_ORPHAN=mmo_place_orphan
LDEMUL_FINISH=mmo_finish
LDEMUL_AFTER_OPEN=mmo_after_open
