/* ELF linking support for BFD.
   Copyright 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

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
#include "bfdlink.h"
#include "libbfd.h"
#define ARCH_SIZE 0
#include "elf-bfd.h"

boolean
_bfd_elf_create_got_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct bfd_link_hash_entry *bh;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int ptralign;

  /* This function may be called more than once.  */
  if (bfd_get_section_by_name (abfd, ".got") != NULL)
    return true;

  switch (bed->s->arch_size)
    {
    case 32:
      ptralign = 2;
      break;

    case 64:
      ptralign = 3;
      break;

    default:
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || !bfd_set_section_flags (abfd, s, flags)
      || !bfd_set_section_alignment (abfd, s, ptralign))
    return false;

  if (bed->want_got_plt)
    {
      s = bfd_make_section (abfd, ".got.plt");
      if (s == NULL
	  || !bfd_set_section_flags (abfd, s, flags)
	  || !bfd_set_section_alignment (abfd, s, ptralign))
	return false;
    }

  if (bed->want_got_sym)
    {
      /* Define the symbol _GLOBAL_OFFSET_TABLE_ at the start of the .got
	 (or .got.plt) section.  We don't do this in the linker script
	 because we don't want to define the symbol if we are not creating
	 a global offset table.  */
      bh = NULL;
      if (!(_bfd_generic_link_add_one_symbol
	    (info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s,
	     bed->got_symbol_offset, (const char *) NULL, false,
	     bed->collect, &bh)))
	return false;
      h = (struct elf_link_hash_entry *) bh;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (info->shared
	  && ! _bfd_elf_link_record_dynamic_symbol (info, h))
	return false;

      elf_hash_table (info)->hgot = h;
    }

  /* The first bit of the global offset table is the header.  */
  s->_raw_size += bed->got_header_size + bed->got_symbol_offset;

  return true;
}

/* Create dynamic sections when linking against a dynamic object.  */

boolean
_bfd_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags, pltflags;
  register asection *s;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int ptralign;

  switch (bed->s->arch_size)
    {
    case 32:
      ptralign = 2;
      break;

    case 64:
      ptralign = 3;
      break;

    default:
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  /* We need to create .plt, .rel[a].plt, .got, .got.plt, .dynbss, and
     .rel[a].bss sections.  */

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  pltflags = flags;
  pltflags |= SEC_CODE;
  if (bed->plt_not_loaded)
    pltflags &= ~ (SEC_CODE | SEC_LOAD | SEC_HAS_CONTENTS);
  if (bed->plt_readonly)
    pltflags |= SEC_READONLY;

  s = bfd_make_section (abfd, ".plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, pltflags)
      || ! bfd_set_section_alignment (abfd, s, bed->plt_alignment))
    return false;

  if (bed->want_plt_sym)
    {
      /* Define the symbol _PROCEDURE_LINKAGE_TABLE_ at the start of the
	 .plt section.  */
      struct elf_link_hash_entry *h;
      struct bfd_link_hash_entry *bh = NULL;

      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, "_PROCEDURE_LINKAGE_TABLE_", BSF_GLOBAL, s,
	      (bfd_vma) 0, (const char *) NULL, false,
	      get_elf_backend_data (abfd)->collect, &bh)))
	return false;
      h = (struct elf_link_hash_entry *) bh;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (info->shared
	  && ! _bfd_elf_link_record_dynamic_symbol (info, h))
	return false;
    }

  s = bfd_make_section (abfd,
			bed->default_use_rela_p ? ".rela.plt" : ".rel.plt");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, ptralign))
    return false;

  if (! _bfd_elf_create_got_section (abfd, info))
    return false;

  if (bed->want_dynbss)
    {
      /* The .dynbss section is a place to put symbols which are defined
	 by dynamic objects, are referenced by regular objects, and are
	 not functions.  We must allocate space for them in the process
	 image and use a R_*_COPY reloc to tell the dynamic linker to
	 initialize them at run time.  The linker script puts the .dynbss
	 section into the .bss section of the final image.  */
      s = bfd_make_section (abfd, ".dynbss");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, SEC_ALLOC))
	return false;

      /* The .rel[a].bss section holds copy relocs.  This section is not
     normally needed.  We need to create it here, though, so that the
     linker will map it to an output section.  We can't just create it
     only if we need it, because we will not know whether we need it
     until we have seen all the input files, and the first time the
     main linker code calls BFD after examining all the input files
     (size_dynamic_sections) the input sections have already been
     mapped to the output sections.  If the section turns out not to
     be needed, we can discard it later.  We will never need this
     section when generating a shared object, since they do not use
     copy relocs.  */
      if (! info->shared)
	{
	  s = bfd_make_section (abfd,
				(bed->default_use_rela_p
				 ? ".rela.bss" : ".rel.bss"));
	  if (s == NULL
	      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
	      || ! bfd_set_section_alignment (abfd, s, ptralign))
	    return false;
	}
    }

  return true;
}

/* Record a new dynamic symbol.  We record the dynamic symbols as we
   read the input files, since we need to have a list of all of them
   before we can determine the final sizes of the output sections.
   Note that we may actually call this function even though we are not
   going to output any dynamic symbols; in some cases we know that a
   symbol should be in the dynamic symbol table, but only if there is
   one.  */

boolean
_bfd_elf_link_record_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  if (h->dynindx == -1)
    {
      struct elf_strtab_hash *dynstr;
      char *p, *alc;
      const char *name;
      boolean copy;
      bfd_size_type indx;

      /* XXX: The ABI draft says the linker must turn hidden and
	 internal symbols into STB_LOCAL symbols when producing the
	 DSO. However, if ld.so honors st_other in the dynamic table,
	 this would not be necessary.  */
      switch (ELF_ST_VISIBILITY (h->other))
	{
	case STV_INTERNAL:
	case STV_HIDDEN:
	  if (h->root.type != bfd_link_hash_undefined
	      && h->root.type != bfd_link_hash_undefweak)
	    {
	      h->elf_link_hash_flags |= ELF_LINK_FORCED_LOCAL;
	      return true;
	    }

	default:
	  break;
	}

      h->dynindx = elf_hash_table (info)->dynsymcount;
      ++elf_hash_table (info)->dynsymcount;

      dynstr = elf_hash_table (info)->dynstr;
      if (dynstr == NULL)
	{
	  /* Create a strtab to hold the dynamic symbol names.  */
	  elf_hash_table (info)->dynstr = dynstr = _bfd_elf_strtab_init ();
	  if (dynstr == NULL)
	    return false;
	}

      /* We don't put any version information in the dynamic string
         table.  */
      name = h->root.root.string;
      p = strchr (name, ELF_VER_CHR);
      if (p == NULL)
	{
	  alc = NULL;
	  copy = false;
	}
      else
	{
	  size_t len = p - name + 1;

	  alc = bfd_malloc ((bfd_size_type) len);
	  if (alc == NULL)
	    return false;
	  memcpy (alc, name, len - 1);
	  alc[len - 1] = '\0';
	  name = alc;
	  copy = true;
	}

      indx = _bfd_elf_strtab_add (dynstr, name, copy);

      if (alc != NULL)
	free (alc);

      if (indx == (bfd_size_type) -1)
	return false;
      h->dynstr_index = indx;
    }

  return true;
}

/* Record a new local dynamic symbol.  Returns 0 on failure, 1 on
   success, and 2 on a failure caused by attempting to record a symbol
   in a discarded section, eg. a discarded link-once section symbol.  */

int
elf_link_record_local_dynamic_symbol (info, input_bfd, input_indx)
     struct bfd_link_info *info;
     bfd *input_bfd;
     long input_indx;
{
  bfd_size_type amt;
  struct elf_link_local_dynamic_entry *entry;
  struct elf_link_hash_table *eht;
  struct elf_strtab_hash *dynstr;
  unsigned long dynstr_index;
  char *name;
  Elf_External_Sym_Shndx eshndx;
  char esym[sizeof (Elf64_External_Sym)];

  if (! is_elf_hash_table (info))
    return 0;

  /* See if the entry exists already.  */
  for (entry = elf_hash_table (info)->dynlocal; entry ; entry = entry->next)
    if (entry->input_bfd == input_bfd && entry->input_indx == input_indx)
      return 1;

  amt = sizeof (*entry);
  entry = (struct elf_link_local_dynamic_entry *) bfd_alloc (input_bfd, amt);
  if (entry == NULL)
    return 0;

  /* Go find the symbol, so that we can find it's name.  */
  if (!bfd_elf_get_elf_syms (input_bfd, &elf_tdata (input_bfd)->symtab_hdr,
			     (size_t) 1, (size_t) input_indx,
			     &entry->isym, esym, &eshndx))
    {
      bfd_release (input_bfd, entry);
      return 0;
    }

  if (entry->isym.st_shndx != SHN_UNDEF
      && (entry->isym.st_shndx < SHN_LORESERVE
	  || entry->isym.st_shndx > SHN_HIRESERVE))
    {
      asection *s;

      s = bfd_section_from_elf_index (input_bfd, entry->isym.st_shndx);
      if (s == NULL || bfd_is_abs_section (s->output_section))
	{
	  /* We can still bfd_release here as nothing has done another
	     bfd_alloc.  We can't do this later in this function.  */
	  bfd_release (input_bfd, entry);
	  return 2;
	}
    }

  name = (bfd_elf_string_from_elf_section
	  (input_bfd, elf_tdata (input_bfd)->symtab_hdr.sh_link,
	   entry->isym.st_name));

  dynstr = elf_hash_table (info)->dynstr;
  if (dynstr == NULL)
    {
      /* Create a strtab to hold the dynamic symbol names.  */
      elf_hash_table (info)->dynstr = dynstr = _bfd_elf_strtab_init ();
      if (dynstr == NULL)
	return 0;
    }

  dynstr_index = _bfd_elf_strtab_add (dynstr, name, false);
  if (dynstr_index == (unsigned long) -1)
    return 0;
  entry->isym.st_name = dynstr_index;

  eht = elf_hash_table (info);

  entry->next = eht->dynlocal;
  eht->dynlocal = entry;
  entry->input_bfd = input_bfd;
  entry->input_indx = input_indx;
  eht->dynsymcount++;

  /* Whatever binding the symbol had before, it's now local.  */
  entry->isym.st_info
    = ELF_ST_INFO (STB_LOCAL, ELF_ST_TYPE (entry->isym.st_info));

  /* The dynindx will be set at the end of size_dynamic_sections.  */

  return 1;
}

/* Return the dynindex of a local dynamic symbol.  */

long
_bfd_elf_link_lookup_local_dynindx (info, input_bfd, input_indx)
     struct bfd_link_info *info;
     bfd *input_bfd;
     long input_indx;
{
  struct elf_link_local_dynamic_entry *e;

  for (e = elf_hash_table (info)->dynlocal; e ; e = e->next)
    if (e->input_bfd == input_bfd && e->input_indx == input_indx)
      return e->dynindx;
  return -1;
}

/* This function is used to renumber the dynamic symbols, if some of
   them are removed because they are marked as local.  This is called
   via elf_link_hash_traverse.  */

static boolean elf_link_renumber_hash_table_dynsyms
  PARAMS ((struct elf_link_hash_entry *, PTR));

static boolean
elf_link_renumber_hash_table_dynsyms (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  size_t *count = (size_t *) data;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx != -1)
    h->dynindx = ++(*count);

  return true;
}

/* Assign dynsym indices.  In a shared library we generate a section
   symbol for each output section, which come first.  Next come all of
   the back-end allocated local dynamic syms, followed by the rest of
   the global symbols.  */

unsigned long
_bfd_elf_link_renumber_dynsyms (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  unsigned long dynsymcount = 0;

  if (info->shared)
    {
      asection *p;
      for (p = output_bfd->sections; p ; p = p->next)
	if ((p->flags & SEC_EXCLUDE) == 0)
	  elf_section_data (p)->dynindx = ++dynsymcount;
    }

  if (elf_hash_table (info)->dynlocal)
    {
      struct elf_link_local_dynamic_entry *p;
      for (p = elf_hash_table (info)->dynlocal; p ; p = p->next)
	p->dynindx = ++dynsymcount;
    }

  elf_link_hash_traverse (elf_hash_table (info),
			  elf_link_renumber_hash_table_dynsyms,
			  &dynsymcount);

  /* There is an unused NULL entry at the head of the table which
     we must account for in our count.  Unless there weren't any
     symbols, which means we'll have no table at all.  */
  if (dynsymcount != 0)
    ++dynsymcount;

  return elf_hash_table (info)->dynsymcount = dynsymcount;
}

/* Create a special linker section, or return a pointer to a linker
   section already created */

elf_linker_section_t *
_bfd_elf_create_linker_section (abfd, info, which, defaults)
     bfd *abfd;
     struct bfd_link_info *info;
     enum elf_linker_section_enum which;
     elf_linker_section_t *defaults;
{
  bfd *dynobj = elf_hash_table (info)->dynobj;
  elf_linker_section_t *lsect;

  /* Record the first bfd section that needs the special section */
  if (!dynobj)
    dynobj = elf_hash_table (info)->dynobj = abfd;

  /* If this is the first time, create the section */
  lsect = elf_linker_section (dynobj, which);
  if (!lsect)
    {
      asection *s;
      bfd_size_type amt = sizeof (elf_linker_section_t);

      lsect = (elf_linker_section_t *) bfd_alloc (dynobj, amt);

      *lsect = *defaults;
      elf_linker_section (dynobj, which) = lsect;
      lsect->which = which;
      lsect->hole_written_p = false;

      /* See if the sections already exist */
      lsect->section = s = bfd_get_section_by_name (dynobj, lsect->name);
      if (!s || (s->flags & defaults->flags) != defaults->flags)
	{
	  lsect->section = s = bfd_make_section_anyway (dynobj, lsect->name);

	  if (s == NULL)
	    return (elf_linker_section_t *)0;

	  bfd_set_section_flags (dynobj, s, defaults->flags);
	  bfd_set_section_alignment (dynobj, s, lsect->alignment);
	}
      else if (bfd_get_section_alignment (dynobj, s) < lsect->alignment)
	bfd_set_section_alignment (dynobj, s, lsect->alignment);

      s->_raw_size = align_power (s->_raw_size, lsect->alignment);

      /* Is there a hole we have to provide?  If so check whether the segment is
	 too big already */
      if (lsect->hole_size)
	{
	  lsect->hole_offset = s->_raw_size;
	  s->_raw_size += lsect->hole_size;
	  if (lsect->hole_offset > lsect->max_hole_offset)
	    {
	      (*_bfd_error_handler) (_("%s: Section %s is too large to add hole of %ld bytes"),
				     bfd_get_filename (abfd),
				     lsect->name,
				     (long) lsect->hole_size);

	      bfd_set_error (bfd_error_bad_value);
	      return (elf_linker_section_t *)0;
	    }
	}

#ifdef DEBUG
      fprintf (stderr, "Creating section %s, current size = %ld\n",
	       lsect->name, (long)s->_raw_size);
#endif

      if (lsect->sym_name)
	{
	  struct elf_link_hash_entry *h;
	  struct bfd_link_hash_entry *bh;

#ifdef DEBUG
	  fprintf (stderr, "Adding %s to section %s\n",
		   lsect->sym_name,
		   lsect->name);
#endif
	  bh = bfd_link_hash_lookup (info->hash, lsect->sym_name,
				     false, false, false);

	  if ((bh == NULL || bh->type == bfd_link_hash_undefined)
	      && !(_bfd_generic_link_add_one_symbol
		   (info, abfd, lsect->sym_name, BSF_GLOBAL, s,
		    (lsect->hole_size
		     ? s->_raw_size - lsect->hole_size + lsect->sym_offset
		     : lsect->sym_offset),
		    (const char *) NULL, false,
		    get_elf_backend_data (abfd)->collect, &bh)))
	    return (elf_linker_section_t *) 0;
	  h = (struct elf_link_hash_entry *) bh;

	  if ((defaults->which != LINKER_SECTION_SDATA)
	      && (defaults->which != LINKER_SECTION_SDATA2))
	    h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_DYNAMIC;

	  h->type = STT_OBJECT;
	  lsect->sym_hash = h;

	  if (info->shared
	      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
	    return (elf_linker_section_t *) 0;
	}
    }

#if 0
  /* This does not make sense.  The sections which may exist in the
     object file have nothing to do with the sections we want to
     create.  */

  /* Find the related sections if they have been created */
  if (lsect->bss_name && !lsect->bss_section)
    lsect->bss_section = bfd_get_section_by_name (dynobj, lsect->bss_name);

  if (lsect->rel_name && !lsect->rel_section)
    lsect->rel_section = bfd_get_section_by_name (dynobj, lsect->rel_name);
#endif

  return lsect;
}

/* Find a linker generated pointer with a given addend and type.  */

elf_linker_section_pointers_t *
_bfd_elf_find_pointer_linker_section (linker_pointers, addend, which)
     elf_linker_section_pointers_t *linker_pointers;
     bfd_vma addend;
     elf_linker_section_enum_t which;
{
  for ( ; linker_pointers != NULL; linker_pointers = linker_pointers->next)
    {
      if (which == linker_pointers->which && addend == linker_pointers->addend)
	return linker_pointers;
    }

  return (elf_linker_section_pointers_t *)0;
}

/* Make the .rela section corresponding to the generated linker section.  */

boolean
_bfd_elf_make_linker_section_rela (dynobj, lsect, alignment)
     bfd *dynobj;
     elf_linker_section_t *lsect;
     int alignment;
{
  if (lsect->rel_section)
    return true;

  lsect->rel_section = bfd_get_section_by_name (dynobj, lsect->rel_name);
  if (lsect->rel_section == NULL)
    {
      lsect->rel_section = bfd_make_section (dynobj, lsect->rel_name);
      if (lsect->rel_section == NULL
	  || ! bfd_set_section_flags (dynobj,
				      lsect->rel_section,
				      (SEC_ALLOC
				       | SEC_LOAD
				       | SEC_HAS_CONTENTS
				       | SEC_IN_MEMORY
				       | SEC_LINKER_CREATED
				       | SEC_READONLY))
	  || ! bfd_set_section_alignment (dynobj, lsect->rel_section, alignment))
	return false;
    }

  return true;
}
