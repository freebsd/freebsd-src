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
#include "bfdlink.h"
#include "libbfd.h"
#define ARCH_SIZE 0
#include "libelf.h"

/* Standard ELF hash function.  Do not change this function; you will
   cause invalid hash tables to be generated.  (Well, you would if this
   were being used yet.)  */
unsigned long
bfd_elf_hash (name)
     CONST unsigned char *name;
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
elf_read (abfd, offset, size)
     bfd * abfd;
     long offset;
     int size;
{
  char *buf;

  if ((buf = bfd_alloc (abfd, size)) == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }
  if (bfd_seek (abfd, offset, SEEK_SET) == -1)
    return NULL;
  if (bfd_read ((PTR) buf, size, 1, abfd) != size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_file_truncated);
      return NULL;
    }
  return buf;
}

boolean
elf_mkobject (abfd)
     bfd * abfd;
{
  /* this just does initialization */
  /* coff_mkobject zalloc's space for tdata.coff_obj_data ... */
  elf_tdata (abfd) = (struct elf_obj_tdata *)
    bfd_zalloc (abfd, sizeof (struct elf_obj_tdata));
  if (elf_tdata (abfd) == 0)
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  /* since everything is done at close time, do we need any
     initialization? */

  return true;
}

char *
elf_get_str_section (abfd, shindex)
     bfd * abfd;
     unsigned int shindex;
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
elf_string_from_elf_section (abfd, shindex, strindex)
     bfd * abfd;
     unsigned int shindex;
     unsigned int strindex;
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

/* Make a BFD section from an ELF section.  We store a pointer to the
   BFD section in the rawdata field of the header.  */

boolean
_bfd_elf_make_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     const char *name;
{
  asection *newsect;
  flagword flags;

  if (hdr->rawdata != NULL)
    {
      BFD_ASSERT (strcmp (name, ((asection *) hdr->rawdata)->name) == 0);
      return true;
    }

  newsect = bfd_make_section_anyway (abfd, name);
  if (newsect == NULL)
    return false;

  newsect->filepos = hdr->sh_offset;

  if (! bfd_set_section_vma (abfd, newsect, hdr->sh_addr)
      || ! bfd_set_section_size (abfd, newsect, hdr->sh_size)
      || ! bfd_set_section_alignment (abfd, newsect,
				      bfd_log2 (hdr->sh_addralign)))
    return false;

  flags = SEC_NO_FLAGS;
  if (hdr->sh_type != SHT_NOBITS)
    flags |= SEC_HAS_CONTENTS;
  if ((hdr->sh_flags & SHF_ALLOC) != 0)
    {
      flags |= SEC_ALLOC;
      if (hdr->sh_type != SHT_NOBITS)
	flags |= SEC_LOAD;
    }
  if ((hdr->sh_flags & SHF_WRITE) == 0)
    flags |= SEC_READONLY;
  if ((hdr->sh_flags & SHF_EXECINSTR) != 0)
    flags |= SEC_CODE;
  else if ((flags & SEC_LOAD) != 0)
    flags |= SEC_DATA;

  /* The debugging sections appear to be recognized only by name, not
     any sort of flag.  */
  if (strncmp (name, ".debug", sizeof ".debug" - 1) == 0
      || strncmp (name, ".line", sizeof ".line" - 1) == 0
      || strncmp (name, ".stab", sizeof ".stab" - 1) == 0)
    flags |= SEC_DEBUGGING;

  if (! bfd_set_section_flags (abfd, newsect, flags))
    return false;

  hdr->rawdata = (PTR) newsect;
  elf_section_data (newsect)->this_hdr = *hdr;

  return true;
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
bfd_elf_find_section (abfd, name)
     bfd * abfd;
     char *name;
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

/*ARGSUSED*/
bfd_reloc_status_type
bfd_elf_generic_reloc (abfd,
		       reloc_entry,
		       symbol,
		       data,
		       input_section,
		       output_bfd,
		       error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc_entry->howto->partial_inplace
	  || reloc_entry->addend == 0))
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  return bfd_reloc_continue;
}

/* Create an entry in an ELF linker hash table.  */

struct bfd_hash_entry *
_bfd_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf_link_hash_entry *ret = (struct elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_link_hash_entry *) NULL)
    ret = ((struct elf_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct elf_link_hash_entry)));
  if (ret == (struct elf_link_hash_entry *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return (struct bfd_hash_entry *) ret;
    }

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));
  if (ret != (struct elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->indx = -1;
      ret->size = 0;
      ret->align = 0;
      ret->dynindx = -1;
      ret->dynstr_index = 0;
      ret->weakdef = NULL;
      ret->type = STT_NOTYPE;
      ret->elf_link_hash_flags = 0;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize an ELF linker hash table.  */

boolean
_bfd_elf_link_hash_table_init (table, abfd, newfunc)
     struct elf_link_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->dynobj = NULL;
  table->dynsymcount = 0;
  table->dynstr = NULL;
  table->bucketcount = 0;
  return _bfd_link_hash_table_init (&table->root, abfd, newfunc);
}

/* Create an ELF linker hash table.  */

struct bfd_link_hash_table *
_bfd_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct elf_link_hash_table *ret;

  ret = ((struct elf_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct elf_link_hash_table)));
  if (ret == (struct elf_link_hash_table *) NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  if (! _bfd_elf_link_hash_table_init (ret, abfd, _bfd_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

  return &ret->root;
}

/* This is a hook for the ELF emulation code in the generic linker to
   tell the backend linker what file name to use for the DT_NEEDED
   entry for a dynamic object.  */

void
bfd_elf_set_dt_needed_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  elf_dt_needed_name (abfd) = name;
}
