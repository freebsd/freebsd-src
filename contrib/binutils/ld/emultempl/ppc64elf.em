# This shell script emits a C file. -*- C -*-
#   Copyright 2002 Free Software Foundation, Inc.
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

# This file is sourced from elf32.em, and defines extra powerpc64-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

#include "elf64-ppc.h"

static int need_laying_out = 0;

static void gld${EMULATION_NAME}_after_allocation PARAMS ((void));
static void gld${EMULATION_NAME}_finish PARAMS ((void));

/* Call the back-end function to set TOC base after we have placed all
   the sections.  */
static void
gld${EMULATION_NAME}_after_allocation ()
{
  if (!ppc64_elf_set_toc (output_bfd, &link_info))
    einfo ("%X%P: can not set TOC base: %E\n");
}

/* Final emulation specific call.  PowerPC64 has 24 byte .plt entries,
   and needs different call stubs for any entries that cross a 64k
   boundary relative to the TOC.  That means we need to wait until all
   sections have been laid out to initialise the stubs.  */

static void
gld${EMULATION_NAME}_finish ()
{
  /* e_entry on PowerPC64 points to the function descriptor for
     _start.  If _start is missing, default to the first function
     descriptor in the .opd section.  */
  entry_section = ".opd";

  /* If generating a relocatable output file, then we don't have any
     stubs.  */
  if (link_info.relocateable)
    return;

  /* bfd_elf64_discard_info just plays with debugging sections,
     ie. doesn't affect any code, so we can delay resizing the
     sections.  It's likely we'll resize everything in the process of
     adjusting stub sizes.  */
  if (bfd_elf${ELFSIZE}_discard_info (output_bfd, &link_info))
    need_laying_out = 1;

  while (1)
    {
      /* Call into the BFD backend to do the real work.  */
      if (! ppc64_elf_size_stubs (output_bfd, &link_info, &need_laying_out))
	{
	  einfo ("%X%P: can not size stub section: %E\n");
	  return;
	}

      if (!need_laying_out)
	break;

      /* If we have changed the size of the stub section, then we need
	 to recalculate all the section offsets.  After this, we may
	 need to adjust the stub size again.  */
      need_laying_out = 0;

      lang_reset_memory_regions ();

      /* Resize the sections.  */
      lang_size_sections (stat_ptr->head, abs_output_section,
			  &stat_ptr->head, 0, (bfd_vma) 0, NULL);

      /* Recalculate TOC base.  */
      ldemul_after_allocation ();

      /* Do the assignments again.  */
      lang_do_assignments (stat_ptr->head, abs_output_section,
			   (fill_type) 0, (bfd_vma) 0);
    }

  if (! ppc64_elf_build_stubs (output_bfd, &link_info))
    einfo ("%X%P: can not build stubs: %E\n");
}
EOF

# Put these extra ppc64elf routines in ld_${EMULATION_NAME}_emulation
#
LDEMUL_AFTER_ALLOCATION=gld${EMULATION_NAME}_after_allocation
LDEMUL_FINISH=gld${EMULATION_NAME}_finish
