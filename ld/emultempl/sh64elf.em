# This shell script emits a C file. -*- C -*-
#   Copyright 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.
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

# This file is sourced from elf32.em, and defines extra sh64
# specific routines.
#

LDEMUL_AFTER_ALLOCATION=sh64_elf_${EMULATION_NAME}_after_allocation
LDEMUL_BEFORE_ALLOCATION=sh64_elf_${EMULATION_NAME}_before_allocation

cat >>e${EMULATION_NAME}.c <<EOF

#include "libiberty.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/sh.h"
#include "elf32-sh64.h"

/* Check if we need a .cranges section and create it if it's not in any
   input file.  It might seem better to always create it and if unneeded,
   discard it, but I don't find a simple way to discard it totally from
   the output.

   Putting it here instead of as a elf_backend_always_size_sections hook
   in elf32-sh64.c, means that we have access to linker command line
   options here, and we can access input sections in the order in which
   they will be linked.  */

static void
sh64_elf_${EMULATION_NAME}_before_allocation (void)
{
  asection *cranges;
  asection *osec;

  /* Call main function; we're just extending it.  */
  gld${EMULATION_NAME}_before_allocation ();

  cranges = bfd_get_section_by_name (output_bfd, SH64_CRANGES_SECTION_NAME);

  if (cranges != NULL)
    {
      if (command_line.relax)
	{
	  /* FIXME: Look through incoming sections with .cranges
	     descriptors, build up some kind of descriptors that the
	     relaxing function will pick up and adjust, or perhaps make it
	     find and adjust an associated .cranges descriptor.  We could
	     also look through incoming relocs and kill the ones marking
	     relaxation areas, but that wouldn't be TRT.  */
	  einfo
	    (_("%P: Sorry, turning off relaxing: .cranges section in input.\n"));
	  einfo (_(" A .cranges section is present in:\n"));

	  {
	    LANG_FOR_EACH_INPUT_STATEMENT (f)
	      {
		asection *input_cranges
		  = bfd_get_section_by_name (f->the_bfd,
					     SH64_CRANGES_SECTION_NAME);
		if (input_cranges != NULL)
		  einfo (" %I\n", f);
	      }
	  }

	  command_line.relax = FALSE;
	}

      /* We wouldn't need to do anything when there's already a .cranges
	 section (and have a return here), except that we need to set the
	 section flags right for output sections that *don't* need a
	 .cranges section.  */
    }

  if (command_line.relax)
    {
      LANG_FOR_EACH_INPUT_STATEMENT (f)
	{
	  if (bfd_get_flavour (f->the_bfd) == bfd_target_elf_flavour)
	    {
	      asection *isec;
	      for (isec = f->the_bfd->sections;
		   isec != NULL;
		   isec = isec->next)
		{
		  if (elf_section_data (isec)->this_hdr.sh_flags
		      & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED))
		    {
		      einfo (_("%P: Sorry, turning off relaxing: SHmedia sections present.\n"));
		      einfo ("  %I\n", f);
		      command_line.relax = FALSE;
		      goto done_scanning_shmedia_sections;
		    }
		}
	    }
	}
    }
 done_scanning_shmedia_sections:

  /* For each non-empty input section in each output section, check if it
     has the same SH64-specific flags.  If some input section differs, we
     need a .cranges section.  */
  for (osec = output_bfd->sections;
       osec != NULL;
       osec = osec->next)
    {
      struct sh64_section_data *sh64_sec_data;
      bfd_vma oflags_isa = 0;
      bfd_vma iflags_isa = 0;

      if (bfd_get_flavour (output_bfd) != bfd_target_elf_flavour)
	einfo (_("%FError: non-ELF output formats are not supported by this target's linker.\n"));

      sh64_sec_data = sh64_elf_section_data (osec)->sh64_info;

      /* Omit excluded or garbage-collected sections.  */
      if (bfd_get_section_flags (output_bfd, osec) & SEC_EXCLUDE)
	continue;

      /* Make sure we have the target section data initialized.  */
      if (sh64_sec_data == NULL)
	{
	  sh64_sec_data = xcalloc (1, sizeof (struct sh64_section_data));
	  sh64_elf_section_data (osec)->sh64_info = sh64_sec_data;
	}

      /* First find an input section so we have flags to compare with; the
	 flags in the output section are not valid.  */
      {
	LANG_FOR_EACH_INPUT_STATEMENT (f)
	  {
	    asection *isec;

	    for (isec = f->the_bfd->sections;
		 isec != NULL;
		 isec = isec->next)
	      {
		if (isec->output_section == osec
		    && isec->size != 0
		    && (bfd_get_section_flags (isec->owner, isec)
			& SEC_EXCLUDE) == 0)
		  {
		    oflags_isa
		      = (elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED));
		    goto break_1;
		  }
	      }
	  }
      }

    break_1:

      /* Check that all input sections have the same contents-type flags
         as the first input section.  */
      {
	LANG_FOR_EACH_INPUT_STATEMENT (f)
	  {
	    asection *isec;

	    for (isec = f->the_bfd->sections;
		 isec != NULL;
		 isec = isec->next)
	      {
		if (isec->output_section == osec
		    && isec->size != 0
		    && (bfd_get_section_flags (isec->owner, isec)
			& SEC_EXCLUDE) == 0)
		  {
		    iflags_isa
		      = (elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED));

		    /* If flags don't agree, we need a .cranges section.
		       Create it here if it did not exist through input
		       sections.  */
		    if (iflags_isa != oflags_isa)
		      {
			if (cranges == NULL)
			  {
			    /* This section will be *appended* to
			       sections, so the outer iteration will reach
			       it in due time and set
			       sh64_elf_section_data; no need to set it
			       specifically here.  */
			    cranges
			      = bfd_make_section (output_bfd,
						  SH64_CRANGES_SECTION_NAME);
			    if (cranges == NULL
				|| !bfd_set_section_flags (output_bfd,
							   cranges,
							   SEC_LINKER_CREATED
							   | SEC_KEEP
							   | SEC_HAS_CONTENTS
							   | SEC_DEBUGGING))
			      einfo
				(_("%P%E%F: Can't make .cranges section\n"));
			  }

			/* We don't need to look at more input sections,
			   and we know this section will have mixed
			   contents.  */
			goto break_2;
		      }
		  }
	      }
	  }
      }

      /* If we got here, then all input sections in this output section
	 have the same contents flag.  Put that where we expect to see
	 contents flags.  We don't need to do this for sections that will
	 need additional, linker-generated .cranges entries.  */
      sh64_sec_data->contents_flags = iflags_isa;

    break_2:
      ;
    }
}

/* Size up and extend the .cranges section, merging generated entries.  */

static void
sh64_elf_${EMULATION_NAME}_after_allocation (void)
{
  bfd_vma new_cranges = 0;
  bfd_vma cranges_growth = 0;
  asection *osec;
  bfd_byte *crangesp;

  asection *cranges
    = bfd_get_section_by_name (output_bfd, SH64_CRANGES_SECTION_NAME);

  /* If this ever starts doing something, we will pick it up.  */
  after_allocation_default ();

  /* If there is no .cranges section, it is because it was seen earlier on
     that none was needed.  Otherwise it must have been created then, or
     be present in input.  */
  if (cranges == NULL)
    return;

  /* First, we set the ISA flags for each output section according to the
     first non-discarded section.  For each input section in osec, we
     check if it has the same flags.  If it does not, we set flags to mark
     a mixed section (and exit the loop early).  */
  for (osec = output_bfd->sections;
       osec != NULL;
       osec = osec->next)
    {
      bfd_vma oflags_isa = 0;
      bfd_boolean need_check_cranges = FALSE;

      /* Omit excluded or garbage-collected sections.  */
      if (bfd_get_section_flags (output_bfd, osec) & SEC_EXCLUDE)
	continue;

      /* First find an input section so we have flags to compare with; the
	 flags in the output section are not valid.  */
      {
	LANG_FOR_EACH_INPUT_STATEMENT (f)
	  {
	    asection *isec;

	    for (isec = f->the_bfd->sections;
		 isec != NULL;
		 isec = isec->next)
	      {
		if (isec->output_section == osec
		    && isec->size != 0
		    && (bfd_get_section_flags (isec->owner, isec)
			& SEC_EXCLUDE) == 0)
		  {
		    oflags_isa
		      = (elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED));
		    goto break_1;
		  }
	      }
	  }
      }

    break_1:

      /* Check that all input sections have the same contents-type flags
         as the first input section.  */
      {
	LANG_FOR_EACH_INPUT_STATEMENT (f)
	  {
	    asection *isec;

	    for (isec = f->the_bfd->sections;
		 isec != NULL;
		 isec = isec->next)
	      {
		if (isec->output_section == osec
		    && isec->size != 0
		    && (bfd_get_section_flags (isec->owner, isec)
			& SEC_EXCLUDE) == 0)
		  {
		    bfd_vma iflags_isa
		      = (elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED));

		    /* If flags don't agree, set the target-specific data
		       of the section to mark that this section needs to
		       be have .cranges section entries added.  Don't
		       bother setting ELF section flags in output section;
		       they will be cleared later and will have to be
		       re-initialized before the linked file is written.  */
		    if (iflags_isa != oflags_isa)
		      {
			oflags_isa = SHF_SH5_ISA32_MIXED;

			BFD_ASSERT (sh64_elf_section_data (osec)->sh64_info);

			sh64_elf_section_data (osec)->sh64_info->contents_flags
			  = SHF_SH5_ISA32_MIXED;
			need_check_cranges = TRUE;
			goto break_2;
		      }
		  }
	      }
	  }
      }

    break_2:

      /* If there were no new ranges for this output section, we don't
	 need to iterate over the input sections to check how many are
	 needed.  */
      if (! need_check_cranges)
	continue;

      /* If we found a section with differing contents type, we need more
	 ranges to mark the sections that are not mixed (and already have
	 .cranges descriptors).  Calculate the maximum number of new
	 entries here.  We may merge some of them, so that number is not
	 final; it can shrink.  */
      {
	LANG_FOR_EACH_INPUT_STATEMENT (f)
	  {
	    asection *isec;

	    for (isec = f->the_bfd->sections;
		 isec != NULL;
		 isec = isec->next)
	      {
		if (isec->output_section == osec
		    && isec->size != 0
		    && (bfd_get_section_flags (isec->owner, isec)
			& SEC_EXCLUDE) == 0
		    && ((elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED))
			!= SHF_SH5_ISA32_MIXED))
		  new_cranges++;
	      }
	  }
      }
    }

  /* ldemul_after_allocation may be called twice.  First directly from
     lang_process, and the second time when lang_process calls ldemul_finish,
     which calls gld${EMULATION_NAME}_finish, e.g. gldshelf32_finish, which
     is defined in emultempl/elf32.em and calls ldemul_after_allocation,
     if bfd_elf_discard_info returned true.  */
  if (cranges->contents != NULL)
    free (cranges->contents);

  BFD_ASSERT (sh64_elf_section_data (cranges)->sh64_info != NULL);

  /* Make sure we have .cranges in memory even if there were only
     assembler-generated .cranges.  */
  cranges_growth = new_cranges * SH64_CRANGE_SIZE;
  cranges->contents = xcalloc (cranges->size + cranges_growth, 1);
  bfd_set_section_flags (cranges->owner, cranges,
			 bfd_get_section_flags (cranges->owner, cranges)
			 | SEC_IN_MEMORY);

  /* If we don't need to grow the .cranges section beyond what was in the
     input sections, we have nothing more to do here.  We then only got
     here because there was a .cranges section coming from input.  Zero
     out the number of generated .cranges.  */
  if (new_cranges == 0)
    {
      sh64_elf_section_data (cranges)->sh64_info->cranges_growth = 0;
      return;
    }

  crangesp = cranges->contents + cranges->size;

  /* Now pass over the sections again, and make reloc orders for the new
     .cranges entries.  Constants are set as we go.  */
  for (osec = output_bfd->sections;
       osec != NULL;
       osec = osec->next)
    {
      struct bfd_link_order *cr_addr_order = NULL;
      enum sh64_elf_cr_type last_cr_type = CRT_NONE;
      bfd_vma last_cr_size = 0;
      bfd_vma continuation_vma = 0;

      /* Omit excluded or garbage-collected sections, and output sections
	 which were not marked as needing further processing.  */
      if ((bfd_get_section_flags (output_bfd, osec) & SEC_EXCLUDE) != 0
	  || (sh64_elf_section_data (osec)->sh64_info->contents_flags
	      != SHF_SH5_ISA32_MIXED))
	continue;

      {
	LANG_FOR_EACH_INPUT_STATEMENT (f)
	  {
	    asection *isec;

	    for (isec = f->the_bfd->sections;
		 isec != NULL;
		 isec = isec->next)
	      {
		/* Allow only sections that have (at least initially) a
		   non-zero size, and are not excluded, and are not marked
		   as containing mixed data, thus already having .cranges
		   entries.  */
		if (isec->output_section == osec
		    && isec->size != 0
		    && (bfd_get_section_flags (isec->owner, isec)
			& SEC_EXCLUDE) == 0
		    && ((elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED))
			!= SHF_SH5_ISA32_MIXED))
		  {
		    enum sh64_elf_cr_type cr_type;
		    bfd_vma cr_size;
		    bfd_vma isa_flags
		      = (elf_section_data (isec)->this_hdr.sh_flags
			 & (SHF_SH5_ISA32 | SHF_SH5_ISA32_MIXED));

		    if (isa_flags == SHF_SH5_ISA32)
		      cr_type = CRT_SH5_ISA32;
		    else if ((bfd_get_section_flags (isec->owner, isec)
			      & SEC_CODE) == 0)
		      cr_type = CRT_DATA;
		    else
		      cr_type = CRT_SH5_ISA16;

		    cr_size = isec->size;

		    /* Sections can be empty, like .text in a file that
		       only contains other sections.  Ranges shouldn't be
		       emitted for them.  This can presumably happen after
		       relaxing and is not be caught at the "raw size"
		       test above.  */
		    if (cr_size == 0)
		      continue;

		    /* See if this is a continuation of the previous range
		       for the same output section.  If so, just change
		       the size of the last range and continue.  */
		    if (cr_type == last_cr_type
			&& (continuation_vma
			    == osec->vma + isec->output_offset))
		      {
			last_cr_size += cr_size;
			bfd_put_32 (output_bfd, last_cr_size,
				    crangesp - SH64_CRANGE_SIZE
				    + SH64_CRANGE_CR_SIZE_OFFSET);

			continuation_vma += cr_size;
			continue;
		      }

		    /* If we emit relocatable contents, we need a
		       relocation for the start address.  */
		    if (link_info.relocatable || link_info.emitrelocations)
		      {
			/* FIXME: We could perhaps use lang_add_reloc and
			   friends here, but I'm not really sure that
			   would leave us free to do some optimizations
			   later.  */
			cr_addr_order
			  = bfd_new_link_order (output_bfd, cranges);

			if (cr_addr_order == NULL)
			  {
			    einfo (_("%P%F: bfd_new_link_order failed\n"));
			    return;
			  }

			cr_addr_order->type = bfd_section_reloc_link_order;
			cr_addr_order->offset
			  = (cranges->output_offset
			     + crangesp + SH64_CRANGE_CR_ADDR_OFFSET
			     - cranges->contents);
			cr_addr_order->size = 4;
			cr_addr_order->u.reloc.p
			  = xmalloc (sizeof (struct bfd_link_order_reloc));

			cr_addr_order->u.reloc.p->reloc = BFD_RELOC_32;
			cr_addr_order->u.reloc.p->u.section = osec;

			/* Since SH, unlike normal RELA-targets, uses a
			   "partial inplace" REL-like relocation for this,
			   we put the addend in the contents and specify 0
			   for the reloc.  */
			bfd_put_32 (output_bfd, isec->output_offset,
				    crangesp + SH64_CRANGE_CR_ADDR_OFFSET);
			cr_addr_order->u.reloc.p->addend = 0;
		      }
		    else
		      bfd_put_32 (output_bfd,
				  osec->vma + isec->output_offset,
				  crangesp + SH64_CRANGE_CR_ADDR_OFFSET);

		    /* If we could make a reloc for cr_size we would do
		       it, but we would have to have a symbol for the size
		       of the _input_ section and there's no way to
		       generate that.  */
		    bfd_put_32 (output_bfd, cr_size,
				crangesp + SH64_CRANGE_CR_SIZE_OFFSET);

		    bfd_put_16 (output_bfd, cr_type,
				crangesp + SH64_CRANGE_CR_TYPE_OFFSET);

		    last_cr_type = cr_type;
		    last_cr_size = cr_size;
		    continuation_vma
		      = osec->vma + isec->output_offset + cr_size;
		    crangesp += SH64_CRANGE_SIZE;
		  }
	      }
	  }
      }
    }

  /* The .cranges section will have this size, no larger or smaller.
     Since relocs (if relocatable linking) will be emitted into the
     "extended" size, we must set the raw size to the total.  We have to
     keep track of the number of new .cranges entries.

     Sorting before writing is done by sh64_elf_final_write_processing.  */

  sh64_elf_section_data (cranges)->sh64_info->cranges_growth
    = crangesp - cranges->contents - cranges->size;
  cranges->size = crangesp - cranges->contents;
  cranges->rawsize = cranges->size;
}
