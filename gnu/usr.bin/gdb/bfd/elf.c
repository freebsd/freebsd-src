/* ELF executable support for BFD.
   Copyright 1993 Free Software Foundation, Inc.

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

/*

SECTION
	ELF backends

	BFD support for ELF formats is being worked on.
	Currently, the best supported back ends are for sparc and i386
	(running svr4 or Solaris 2).

	Documentation of the internals of the support code still needs
	to be written.  The code is changing quickly enough that we
	haven't bothered yet.
 */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#define ARCH_SIZE 0
#include "libelf.h"

/* Standard ELF hash function.  Do not change this function; you will
   cause invalid hash tables to be generated.  (Well, you would if this
   were being used yet.)  */
unsigned long
DEFUN (bfd_elf_hash, (name),
       CONST unsigned char *name)
{
  unsigned long h = 0;
  unsigned long g;
  int ch;

  while ((ch = *name++) != '\0')
    {
      h = (h << 4) + ch;
      if ((g = (h & 0xf0000000)) != 0)
	{
	  h ^= g >> 24;
	  h &= ~g;
	}
    }
  return h;
}

/* Read a specified number of bytes at a specified offset in an ELF
   file, into a newly allocated buffer, and return a pointer to the
   buffer. */

static char *
DEFUN (elf_read, (abfd, offset, size),
       bfd * abfd AND
       long offset AND
       int size)
{
  char *buf;

  if ((buf = bfd_alloc (abfd, size)) == NULL)
    {
      bfd_error = no_memory;
      return NULL;
    }
  if (bfd_seek (abfd, offset, SEEK_SET) == -1)
    {
      bfd_error = system_call_error;
      return NULL;
    }
  if (bfd_read ((PTR) buf, size, 1, abfd) != size)
    {
      bfd_error = system_call_error;
      return NULL;
    }
  return buf;
}

boolean
DEFUN (elf_mkobject, (abfd), bfd * abfd)
{
  /* this just does initialization */
  /* coff_mkobject zalloc's space for tdata.coff_obj_data ... */
  elf_tdata (abfd) = (struct elf_obj_tdata *)
    bfd_zalloc (abfd, sizeof (struct elf_obj_tdata));
  if (elf_tdata (abfd) == 0)
    {
      bfd_error = no_memory;
      return false;
    }
  /* since everything is done at close time, do we need any
     initialization? */

  return true;
}

char *
DEFUN (elf_get_str_section, (abfd, shindex),
       bfd * abfd AND
       unsigned int shindex)
{
  Elf_Internal_Shdr **i_shdrp;
  char *shstrtab = NULL;
  unsigned int offset;
  unsigned int shstrtabsize;

  i_shdrp = elf_elfsections (abfd);
  if (i_shdrp == 0 || i_shdrp[shindex] == 0)
    return 0;

  shstrtab = i_shdrp[shindex]->rawdata;
  if (shstrtab == NULL)
    {
      /* No cached one, attempt to read, and cache what we read. */
      offset = i_shdrp[shindex]->sh_offset;
      shstrtabsize = i_shdrp[shindex]->sh_size;
      shstrtab = elf_read (abfd, offset, shstrtabsize);
      i_shdrp[shindex]->rawdata = (void *) shstrtab;
    }
  return shstrtab;
}

char *
DEFUN (elf_string_from_elf_section, (abfd, shindex, strindex),
       bfd * abfd AND
       unsigned int shindex AND
       unsigned int strindex)
{
  Elf_Internal_Shdr *hdr;

  if (strindex == 0)
    return "";

  hdr = elf_elfsections (abfd)[shindex];

  if (!hdr->rawdata
      && elf_get_str_section (abfd, shindex) == NULL)
    return NULL;

  return ((char *) hdr->rawdata) + strindex;
}

/*
INTERNAL_FUNCTION
	bfd_elf_find_section

SYNOPSIS
	struct elf_internal_shdr *bfd_elf_find_section (bfd *abfd, char *name);

DESCRIPTION
	Helper functions for GDB to locate the string tables.
	Since BFD hides string tables from callers, GDB needs to use an
	internal hook to find them.  Sun's .stabstr, in particular,
	isn't even pointed to by the .stab section, so ordinary
	mechanisms wouldn't work to find it, even if we had some.
*/

struct elf_internal_shdr *
DEFUN (bfd_elf_find_section, (abfd, name),
       bfd * abfd AND
       char *name)
{
  Elf_Internal_Shdr **i_shdrp;
  char *shstrtab;
  unsigned int max;
  unsigned int i;

  i_shdrp = elf_elfsections (abfd);
  if (i_shdrp != NULL)
    {
      shstrtab = elf_get_str_section (abfd, elf_elfheader (abfd)->e_shstrndx);
      if (shstrtab != NULL)
	{
	  max = elf_elfheader (abfd)->e_shnum;
	  for (i = 1; i < max; i++)
	    if (!strcmp (&shstrtab[i_shdrp[i]->sh_name], name))
	      return i_shdrp[i];
	}
    }
  return 0;
}

const struct bfd_elf_arch_map bfd_elf_arch_map[] = {
  { bfd_arch_sparc, EM_SPARC },
  { bfd_arch_i386, EM_386 },
  { bfd_arch_m68k, EM_68K },
  { bfd_arch_m88k, EM_88K },
  { bfd_arch_i860, EM_860 },
  { bfd_arch_mips, EM_MIPS },
  { bfd_arch_hppa, EM_HPPA },
};

const int bfd_elf_arch_map_size = sizeof (bfd_elf_arch_map) / sizeof (bfd_elf_arch_map[0]);

const char *const bfd_elf_section_type_names[] = {
  "SHT_NULL", "SHT_PROGBITS", "SHT_SYMTAB", "SHT_STRTAB",
  "SHT_RELA", "SHT_HASH", "SHT_DYNAMIC", "SHT_NOTE",
  "SHT_NOBITS", "SHT_REL", "SHT_SHLIB", "SHT_DYNSYM",
};

/* ELF relocs are against symbols.  If we are producing relocateable
   output, and the reloc is against an external symbol, and nothing
   has given us any additional addend, the resulting reloc will also
   be against the same symbol.  In such a case, we don't want to
   change anything about the way the reloc is handled, since it will
   all be done at final link time.  Rather than put special case code
   into bfd_perform_relocation, all the reloc types use this howto
   function.  It just short circuits the reloc if producing
   relocateable output against an external symbol.  */

bfd_reloc_status_type
bfd_elf_generic_reloc (abfd,
		       reloc_entry,
		       symbol,
		       data,
		       input_section,
		       output_bfd)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}
