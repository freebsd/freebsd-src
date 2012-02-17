/* simple.c -- BFD simple client routines
   Copyright 2002, 2003, 2004
   Free Software Foundation, Inc.
   Contributed by MontaVista Software, Inc.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"

static bfd_boolean
simple_dummy_warning (struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
		      const char *warning ATTRIBUTE_UNUSED,
		      const char *symbol ATTRIBUTE_UNUSED,
		      bfd *abfd ATTRIBUTE_UNUSED,
		      asection *section ATTRIBUTE_UNUSED,
		      bfd_vma address ATTRIBUTE_UNUSED)
{
  return TRUE;
}

static bfd_boolean
simple_dummy_undefined_symbol (struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
			       const char *name ATTRIBUTE_UNUSED,
			       bfd *abfd ATTRIBUTE_UNUSED,
			       asection *section ATTRIBUTE_UNUSED,
			       bfd_vma address ATTRIBUTE_UNUSED,
			       bfd_boolean fatal ATTRIBUTE_UNUSED)
{
  return TRUE;
}

static bfd_boolean
simple_dummy_reloc_overflow (struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
			     const char *name ATTRIBUTE_UNUSED,
			     const char *reloc_name ATTRIBUTE_UNUSED,
			     bfd_vma addend ATTRIBUTE_UNUSED,
			     bfd *abfd ATTRIBUTE_UNUSED,
			     asection *section ATTRIBUTE_UNUSED,
			     bfd_vma address ATTRIBUTE_UNUSED)
{
  return TRUE;
}

static bfd_boolean
simple_dummy_reloc_dangerous (struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
			      const char *message ATTRIBUTE_UNUSED,
			      bfd *abfd ATTRIBUTE_UNUSED,
			      asection *section ATTRIBUTE_UNUSED,
			      bfd_vma address ATTRIBUTE_UNUSED)
{
  return TRUE;
}

static bfd_boolean
simple_dummy_unattached_reloc (struct bfd_link_info *link_info ATTRIBUTE_UNUSED,
			       const char *name ATTRIBUTE_UNUSED,
			       bfd *abfd ATTRIBUTE_UNUSED,
			       asection *section ATTRIBUTE_UNUSED,
			       bfd_vma address ATTRIBUTE_UNUSED)
{
  return TRUE;
}

struct saved_output_info
{
  bfd_vma offset;
  asection *section;
};

static void
simple_save_output_info (bfd *abfd ATTRIBUTE_UNUSED,
			 asection *section,
			 void *ptr)
{
  struct saved_output_info *output_info = ptr;
  output_info[section->index].offset = section->output_offset;
  output_info[section->index].section = section->output_section;
  section->output_offset = 0;
  section->output_section = section;
}

static void
simple_restore_output_info (bfd *abfd ATTRIBUTE_UNUSED,
			    asection *section,
			    void *ptr)
{
  struct saved_output_info *output_info = ptr;
  section->output_offset = output_info[section->index].offset;
  section->output_section = output_info[section->index].section;
}

/*
FUNCTION
	bfd_simple_relocate_secton

SYNOPSIS
	bfd_byte *bfd_simple_get_relocated_section_contents
	  (bfd *abfd, asection *sec, bfd_byte *outbuf, asymbol **symbol_table);

DESCRIPTION
	Returns the relocated contents of section @var{sec}.  The symbols in
	@var{symbol_table} will be used, or the symbols from @var{abfd} if
	@var{symbol_table} is NULL.  The output offsets for all sections will
	be temporarily reset to 0.  The result will be stored at @var{outbuf}
	or allocated with @code{bfd_malloc} if @var{outbuf} is @code{NULL}.

	Generally all sections in @var{abfd} should have their
	@code{output_section} pointing back to the original section.

	Returns @code{NULL} on a fatal error; ignores errors applying
	particular relocations.
*/

bfd_byte *
bfd_simple_get_relocated_section_contents (bfd *abfd,
					   asection *sec,
					   bfd_byte *outbuf,
					   asymbol **symbol_table)
{
  struct bfd_link_info link_info;
  struct bfd_link_order link_order;
  struct bfd_link_callbacks callbacks;
  bfd_byte *contents, *data;
  int storage_needed;
  void *saved_offsets;
  bfd_size_type old_cooked_size;

  if (! (sec->flags & SEC_RELOC))
    {
      bfd_size_type size = bfd_section_size (abfd, sec);

      if (outbuf == NULL)
	contents = bfd_malloc (size);
      else
	contents = outbuf;

      if (contents)
	bfd_get_section_contents (abfd, sec, contents, 0, size);

      return contents;
    }

  /* In order to use bfd_get_relocated_section_contents, we need
     to forge some data structures that it expects.  */

  /* Fill in the bare minimum number of fields for our purposes.  */
  memset (&link_info, 0, sizeof (link_info));
  link_info.input_bfds = abfd;

  link_info.hash = _bfd_generic_link_hash_table_create (abfd);
  link_info.callbacks = &callbacks;
  callbacks.warning = simple_dummy_warning;
  callbacks.undefined_symbol = simple_dummy_undefined_symbol;
  callbacks.reloc_overflow = simple_dummy_reloc_overflow;
  callbacks.reloc_dangerous = simple_dummy_reloc_dangerous;
  callbacks.unattached_reloc = simple_dummy_unattached_reloc;

  memset (&link_order, 0, sizeof (link_order));
  link_order.next = NULL;
  link_order.type = bfd_indirect_link_order;
  link_order.offset = 0;
  link_order.size = bfd_section_size (abfd, sec);
  link_order.u.indirect.section = sec;

  data = NULL;
  if (outbuf == NULL)
    {
      data = bfd_malloc (bfd_section_size (abfd, sec));
      if (data == NULL)
	return NULL;
      outbuf = data;
    }

  /* The sections in ABFD may already have output sections and offsets set.
     Because this function is primarily for debug sections, and GCC uses the
     knowledge that debug sections will generally have VMA 0 when emitting
     relocations between DWARF-2 sections (which are supposed to be
     section-relative offsets anyway), we need to reset the output offsets
     to zero.  We also need to arrange for section->output_section->vma plus
     section->output_offset to equal section->vma, which we do by setting
     section->output_section to point back to section.  Save the original
     output offset and output section to restore later.  */
  saved_offsets = malloc (sizeof (struct saved_output_info)
			  * abfd->section_count);
  if (saved_offsets == NULL)
    {
      if (data)
	free (data);
      return NULL;
    }
  bfd_map_over_sections (abfd, simple_save_output_info, saved_offsets);

  if (symbol_table == NULL)
    {
      _bfd_generic_link_add_symbols (abfd, &link_info);

      storage_needed = bfd_get_symtab_upper_bound (abfd);
      symbol_table = bfd_malloc (storage_needed);
      bfd_canonicalize_symtab (abfd, symbol_table);
    }
  else
    storage_needed = 0;

  /* This function might be called before _cooked_size has been set, and
     bfd_perform_relocation needs _cooked_size to be valid.  */
  old_cooked_size = sec->_cooked_size;
  if (old_cooked_size == 0)
    sec->_cooked_size = sec->_raw_size;

  contents = bfd_get_relocated_section_contents (abfd,
						 &link_info,
						 &link_order,
						 outbuf,
						 0,
						 symbol_table);
  if (contents == NULL && data != NULL)
    free (data);

#if 0
  /* NOTE: cagney/2003-04-05: This free, which was introduced on
     2003-03-31 to stop a memory leak, caused a memory corruption
     between GDB and BFD.  The problem, which is stabs specific, can
     be identified by a bunch of failures in relocate.exp vis:

       gdb.base/relocate.exp: get address of static_bar

     Details of the problem can be found on the binutils@ mailing
     list, see the discussion thread: "gdb.mi/mi-cli.exp failures".  */
  if (storage_needed != 0)
    free (symbol_table);
#endif

  sec->_cooked_size = old_cooked_size;
  bfd_map_over_sections (abfd, simple_restore_output_info, saved_offsets);
  free (saved_offsets);

  _bfd_generic_link_hash_table_free (link_info.hash);

  return contents;
}
