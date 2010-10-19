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
# Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
#

# This file is sourced from elf32.em and mmo.em, used to define
# MMIX-specific things common to ELF and MMO.

cat >>e${EMULATION_NAME}.c <<EOF
#include "elf/mmix.h"

/* Set up handling of linker-allocated global registers.  */

static void
mmix_before_allocation (void)
{
  /* Call the default first.  */
  gld${EMULATION_NAME}_before_allocation ();

  /* There's a needrelax.em which uses this ..._before_allocation-hook and
     just has the statement below as payload.  It's more of a hassle to
     use that than to just include these two lines and take the
     maintenance burden to keep them in sync.  (Of course we lose the
     maintenance burden of checking that it still does what we need.)  */

  /* Force -relax on (regardless of whether we're doing a relocatable
     link).  */
  command_line.relax = TRUE;

  if (!_bfd_mmix_before_linker_allocation (output_bfd, &link_info))
    einfo ("%X%P: Internal problems setting up section %s",
	   MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);
}

/* We need to set the VMA of the .MMIX.reg_contents section when it has
   been allocated, and produce the final settings for the linker-generated
   GREGs.  */

static void
mmix_after_allocation (void)
{
  asection *sec
    = bfd_get_section_by_name (output_bfd, MMIX_REG_CONTENTS_SECTION_NAME);
  bfd_signed_vma regvma;

  /* If there's no register section, we don't need to do anything.  On the
     other hand, if there's a non-standard linker-script without a mapping
     from MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME when that section is
     present (as in the ld test "NOCROSSREFS 2"), that section (1) will be
     orphaned; not inserted in MMIX_REG_CONTENTS_SECTION_NAME and (2) we
     will not do the necessary preparations for those relocations that
     caused it to be created.  We'll SEGV from the latter error.  The
     former error in separation will result in a non-working binary, but
     that's expected when you play tricks with linker scripts.  The
     "NOCROSSREFS 2" test does not run the output so it does not matter
     there.  */
  if (sec == NULL)
    sec
      = bfd_get_section_by_name (output_bfd,
				 MMIX_LD_ALLOCATED_REG_CONTENTS_SECTION_NAME);
  if (sec == NULL)
    return;

  regvma = 256 * 8 - sec->size - 8;

  /* If we start on a local register, we have too many global registers.
     We treat this error as nonfatal (meaning processing will continue in
     search for other errors), because it's a link error in the same way
     as an undefined symbol.  */
  if (regvma < 32 * 8)
    {
      einfo ("%X%P: Too many global registers: %u, max 223\n",
	     (unsigned) sec->size / 8);
      regvma = 32 * 8;
    }

  /* Set vma to correspond to first such register number * 8.  */
  bfd_set_section_vma (output_bfd, sec, (bfd_vma) regvma);

  /* Simplify symbol output for the register section (without contents;
     created for register symbols) by setting the output offset to 0.
     This section is only present when there are register symbols.  */
  sec = bfd_get_section_by_name (output_bfd, MMIX_REG_SECTION_NAME);
  if (sec != NULL)
    bfd_set_section_vma (abfd, sec, 0);

  if (!_bfd_mmix_after_linker_allocation (output_bfd, &link_info))
    {
      /* This is a fatal error; make einfo call not return.  */
      einfo ("%F%P: Can't finalize linker-allocated global registers\n");
    }
}
EOF

LDEMUL_AFTER_ALLOCATION=mmix_after_allocation
LDEMUL_BEFORE_ALLOCATION=mmix_before_allocation
