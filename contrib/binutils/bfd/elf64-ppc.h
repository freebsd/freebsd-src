/* PowerPC64-specific support for 64-bit ELF.
   Copyright 2002 Free Software Foundation, Inc.

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

boolean ppc64_elf_mark_entry_syms
  PARAMS ((struct bfd_link_info *));
bfd_vma ppc64_elf_toc
  PARAMS ((bfd *));
int ppc64_elf_setup_section_lists
  PARAMS ((bfd *, struct bfd_link_info *));
void ppc64_elf_next_input_section
  PARAMS ((struct bfd_link_info *, asection *));
boolean ppc64_elf_size_stubs
  PARAMS ((bfd *, bfd *, struct bfd_link_info *, bfd_signed_vma,
	   asection *(*) (const char *, asection *), void (*) (void)));
boolean ppc64_elf_build_stubs
  PARAMS ((struct bfd_link_info *));
