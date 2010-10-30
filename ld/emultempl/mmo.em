# This shell script emits a C file. -*- C -*-
#   Copyright 2001, 2002, 2003, 2004, 2006 Free Software Foundation, Inc.
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
# Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
#

# This file is sourced from generic.em.

cat >>e${EMULATION_NAME}.c <<EOF
/* Need to have this macro defined before mmix-elfnmmo, which uses the
   name for the before_allocation function, defined in ldemul.c (for
   the mmo "emulation") or in elf32.em (for the elf64mmix
   "emulation").  */
#define gldmmo_before_allocation before_allocation_default

/* We include this header *not* because we expect to handle ELF here
   but because we re-use the map_segments function in elf-generic.em,
   a file which is rightly somewhat ELF-centric.  But this is only to
   get a weird testcase right; ld-mmix/bpo-22, forcing ELF to be
   output from the mmo emulation: -m mmo --oformat elf64-mmix!  */
#include "elf-bfd.h"
EOF

. ${srcdir}/emultempl/elf-generic.em
. ${srcdir}/emultempl/mmix-elfnmmo.em

cat >>e${EMULATION_NAME}.c <<EOF

/* Place an orphan section.  We use this to put random SEC_CODE or
   SEC_READONLY sections right after MMO_TEXT_SECTION_NAME.  Much borrowed
   from elf32.em.  */

static bfd_boolean
mmo_place_orphan (asection *s)
{
  static struct orphan_save hold_text =
    {
      MMO_TEXT_SECTION_NAME,
      SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE,
      0, 0, 0, 0
    };
  struct orphan_save *place;
  const char *secname;
  lang_output_section_statement_type *after;
  lang_output_section_statement_type *os;

  /* We have nothing to say for anything other than a final link.  */
  if (link_info.relocatable
      || (s->flags & (SEC_EXCLUDE | SEC_LOAD)) != SEC_LOAD)
    return FALSE;

  /* Only care for sections we're going to load.  */
  secname = s->name;
  os = lang_output_section_find (secname);

  /* We have an output section by this name.  Place the section inside it
     (regardless of whether the linker script lists it as input).  */
  if (os != NULL)
    {
      lang_add_section (&os->children, s, os);
      return TRUE;
    }

  /* If this section does not have .text-type section flags or there's no
     MMO_TEXT_SECTION_NAME, we don't have anything to say.  */
  if ((s->flags & (SEC_CODE | SEC_READONLY)) == 0)
    return FALSE;

  if (hold_text.os == NULL)
    hold_text.os = lang_output_section_find (hold_text.name);

  place = &hold_text;
  if (hold_text.os != NULL)
    after = hold_text.os;
  else
    after = &lang_output_section_statement.head->output_section_statement;

  /* If there's an output section by this name, we'll use it, regardless
     of section flags, in contrast to what's done in elf32.em.  */
  os = lang_insert_orphan (s, secname, after, place, NULL, NULL);

  /* We need an output section for .text as a root, so if there was none
     (might happen with a peculiar linker script such as in "map
     addresses", map-address.exp), we grab the output section created
     above.  */
  if (hold_text.os == NULL)
    hold_text.os = os;

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
  gld${EMULATION_NAME}_map_segments (FALSE);
  finish_default ();
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
