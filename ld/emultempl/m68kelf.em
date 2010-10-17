# This shell script emits a C file. -*- C -*-
#   Copyright 2000, 2001, 2003 Free Software Foundation, Inc.
#   Written by Michael Sokolov <msokolov@ivan.Harhan.ORG>, based on armelf.em
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

# This file is sourced from elf32.em, and defines some extra routines for m68k
# embedded systems using ELF and for some other systems using m68k ELF.  While
# it is sourced from elf32.em for all m68k ELF configurations, here we include
# only the features we want depending on the configuration.

case ${target} in
  m68*-*-elf)
    echo "#define SUPPORT_EMBEDDED_RELOCS" >>e${EMULATION_NAME}.c
    ;;
esac

cat >>e${EMULATION_NAME}.c <<EOF

#ifdef SUPPORT_EMBEDDED_RELOCS
static void check_sections (bfd *, asection *, void *);
#endif

/* This function is run after all the input files have been opened.  */

static void
m68k_elf_after_open (void)
{
  /* Call the standard elf routine.  */
  gld${EMULATION_NAME}_after_open ();

#ifdef SUPPORT_EMBEDDED_RELOCS
  if (command_line.embedded_relocs
      && (! link_info.relocatable))
    {
      bfd *abfd;

      /* In the embedded relocs mode we create a .emreloc section for each
	 input file with a nonzero .data section.  The BFD backend will fill in
	 these sections with magic numbers which can be used to relocate the
	 data section at run time.  */
      for (abfd = link_info.input_bfds; abfd != NULL; abfd = abfd->link_next)
	{
	  asection *datasec;

	  /* As first-order business, make sure that each input BFD is either
	     COFF or ELF.  We need to call a special BFD backend function to
	     generate the embedded relocs, and we have such functions only for
	     COFF and ELF.  */
	  if (bfd_get_flavour (abfd) != bfd_target_coff_flavour
	      && bfd_get_flavour (abfd) != bfd_target_elf_flavour)
	    einfo ("%F%B: all input objects must be COFF or ELF for --embedded-relocs\n");

	  datasec = bfd_get_section_by_name (abfd, ".data");

	  /* Note that we assume that the reloc_count field has already
	     been set up.  We could call bfd_get_reloc_upper_bound, but
	     that returns the size of a memory buffer rather than a reloc
	     count.  We do not want to call bfd_canonicalize_reloc,
	     because although it would always work it would force us to
	     read in the relocs into BFD canonical form, which would waste
	     a significant amount of time and memory.  */
	  if (datasec != NULL && datasec->reloc_count > 0)
	    {
	      asection *relsec;

	      relsec = bfd_make_section (abfd, ".emreloc");
	      if (relsec == NULL
		  || ! bfd_set_section_flags (abfd, relsec,
					      (SEC_ALLOC
					       | SEC_LOAD
					       | SEC_HAS_CONTENTS
					       | SEC_IN_MEMORY))
		  || ! bfd_set_section_alignment (abfd, relsec, 2)
		  || ! bfd_set_section_size (abfd, relsec,
					     datasec->reloc_count * 12))
		einfo ("%F%B: can not create .emreloc section: %E\n");
	    }

	  /* Double check that all other data sections are empty, as is
	     required for embedded PIC code.  */
	  bfd_map_over_sections (abfd, check_sections, datasec);
	}
    }
#endif /* SUPPORT_EMBEDDED_RELOCS */
}

#ifdef SUPPORT_EMBEDDED_RELOCS
/* Check that of the data sections, only the .data section has
   relocs.  This is called via bfd_map_over_sections.  */

static void
check_sections (bfd *abfd, asection *sec, void *datasec)
{
  if ((bfd_get_section_flags (abfd, sec) & SEC_DATA)
      && sec != datasec
      && sec->reloc_count != 0)
    einfo ("%B%X: section %s has relocs; can not use --embedded-relocs\n",
	   abfd, bfd_get_section_name (abfd, sec));
}

#endif /* SUPPORT_EMBEDDED_RELOCS */

/* This function is called after the section sizes and offsets have
   been set.  */

static void
m68k_elf_after_allocation (void)
{
  /* Call the standard elf routine.  */
  after_allocation_default ();

#ifdef SUPPORT_EMBEDDED_RELOCS
  if (command_line.embedded_relocs
      && (! link_info.relocatable))
    {
      bfd *abfd;

      /* If we are generating embedded relocs, call a special BFD backend
	 routine to do the work.  */
      for (abfd = link_info.input_bfds; abfd != NULL; abfd = abfd->link_next)
	{
	  asection *datasec, *relsec;
	  char *errmsg;

	  datasec = bfd_get_section_by_name (abfd, ".data");

	  if (datasec == NULL || datasec->reloc_count == 0)
	    continue;

	  relsec = bfd_get_section_by_name (abfd, ".emreloc");
	  ASSERT (relsec != NULL);

	  if (bfd_get_flavour (abfd) == bfd_target_coff_flavour)
	    {
	      if (! bfd_m68k_coff_create_embedded_relocs (abfd, &link_info,
							  datasec, relsec,
							  &errmsg))
		{
		  if (errmsg == NULL)
		    einfo ("%B%X: can not create runtime reloc information: %E\n",
			   abfd);
		  else
		    einfo ("%X%B: can not create runtime reloc information: %s\n",
			   abfd, errmsg);
		}
	    }
	  else if (bfd_get_flavour (abfd) == bfd_target_elf_flavour)
	    {
	      if (! bfd_m68k_elf32_create_embedded_relocs (abfd, &link_info,
							   datasec, relsec,
							   &errmsg))
		{
		  if (errmsg == NULL)
		    einfo ("%B%X: can not create runtime reloc information: %E\n",
			   abfd);
		  else
		    einfo ("%X%B: can not create runtime reloc information: %s\n",
			   abfd, errmsg);
		}
	    }
	  else
	    abort ();
	}
    }
#endif /* SUPPORT_EMBEDDED_RELOCS */
}

EOF

# We have our own after_open and after_allocation functions, but they call
# the standard routines, so give them a different name.
LDEMUL_AFTER_OPEN=m68k_elf_after_open
LDEMUL_AFTER_ALLOCATION=m68k_elf_after_allocation
