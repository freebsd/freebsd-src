/* ELF linker support.
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

/* $FreeBSD$ */

/* ELF linker code.  */

/* This struct is used to pass information to routines called via
   elf_link_hash_traverse which must return failure.  */

struct elf_info_failed
{
  boolean failed;
  struct bfd_link_info *info;
  struct bfd_elf_version_tree *verdefs;
};

static boolean is_global_data_symbol_definition
  PARAMS ((bfd *, Elf_Internal_Sym *));
static boolean elf_link_is_defined_archive_symbol
  PARAMS ((bfd *, carsym *));
static boolean elf_link_add_object_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_link_add_archive_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_merge_symbol
  PARAMS ((bfd *, struct bfd_link_info *, const char *,
	   Elf_Internal_Sym *, asection **, bfd_vma *,
	   struct elf_link_hash_entry **, boolean *, boolean *,
	   boolean *, boolean));
static boolean elf_add_default_symbol
  PARAMS ((bfd *, struct bfd_link_info *, struct elf_link_hash_entry *,
	   const char *, Elf_Internal_Sym *, asection **, bfd_vma *,
	   boolean *, boolean, boolean));
static boolean elf_export_symbol
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_finalize_dynstr
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean elf_fix_symbol_flags
  PARAMS ((struct elf_link_hash_entry *, struct elf_info_failed *));
static boolean elf_adjust_dynamic_symbol
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_find_version_dependencies
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_assign_sym_version
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_collect_hash_codes
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_read_relocs_from_section
  PARAMS ((bfd *, Elf_Internal_Shdr *, PTR, Elf_Internal_Rela *));
static size_t compute_bucket_count
  PARAMS ((struct bfd_link_info *));
static void elf_link_output_relocs
  PARAMS ((bfd *, asection *, Elf_Internal_Shdr *, Elf_Internal_Rela *));
static boolean elf_link_size_reloc_section
  PARAMS ((bfd *, Elf_Internal_Shdr *, asection *));
static void elf_link_adjust_relocs
  PARAMS ((bfd *, Elf_Internal_Shdr *, unsigned int,
	   struct elf_link_hash_entry **));
static int elf_link_sort_cmp1
  PARAMS ((const void *, const void *));
static int elf_link_sort_cmp2
  PARAMS ((const void *, const void *));
static size_t elf_link_sort_relocs
  PARAMS ((bfd *, struct bfd_link_info *, asection **));
static boolean elf_section_ignore_discarded_relocs
  PARAMS ((asection *));

/* Given an ELF BFD, add symbols to the global hash table as
   appropriate.  */

boolean
elf_bfd_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return elf_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return elf_link_add_archive_symbols (abfd, info);
    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }
}

/* Return true iff this is a non-common, definition of a non-function symbol.  */
static boolean
is_global_data_symbol_definition (abfd, sym)
     bfd * abfd ATTRIBUTE_UNUSED;
     Elf_Internal_Sym * sym;
{
  /* Local symbols do not count, but target specific ones might.  */
  if (ELF_ST_BIND (sym->st_info) != STB_GLOBAL
      && ELF_ST_BIND (sym->st_info) < STB_LOOS)
    return false;

  /* Function symbols do not count.  */
  if (ELF_ST_TYPE (sym->st_info) == STT_FUNC)
    return false;

  /* If the section is undefined, then so is the symbol.  */
  if (sym->st_shndx == SHN_UNDEF)
    return false;

  /* If the symbol is defined in the common section, then
     it is a common definition and so does not count.  */
  if (sym->st_shndx == SHN_COMMON)
    return false;

  /* If the symbol is in a target specific section then we
     must rely upon the backend to tell us what it is.  */
  if (sym->st_shndx >= SHN_LORESERVE && sym->st_shndx < SHN_ABS)
    /* FIXME - this function is not coded yet:

       return _bfd_is_global_symbol_definition (abfd, sym);

       Instead for now assume that the definition is not global,
       Even if this is wrong, at least the linker will behave
       in the same way that it used to do.  */
    return false;

  return true;
}

/* Search the symbol table of the archive element of the archive ABFD
   whose archive map contains a mention of SYMDEF, and determine if
   the symbol is defined in this element.  */
static boolean
elf_link_is_defined_archive_symbol (abfd, symdef)
     bfd * abfd;
     carsym * symdef;
{
  Elf_Internal_Shdr * hdr;
  Elf_Internal_Shdr * shndx_hdr;
  Elf_External_Sym *  esym;
  Elf_External_Sym *  esymend;
  Elf_External_Sym *  buf = NULL;
  Elf_External_Sym_Shndx * shndx_buf = NULL;
  Elf_External_Sym_Shndx * shndx;
  bfd_size_type symcount;
  bfd_size_type extsymcount;
  bfd_size_type extsymoff;
  boolean result = false;
  file_ptr pos;
  bfd_size_type amt;

  abfd = _bfd_get_elt_at_filepos (abfd, symdef->file_offset);
  if (abfd == (bfd *) NULL)
    return false;

  if (! bfd_check_format (abfd, bfd_object))
    return false;

  /* If we have already included the element containing this symbol in the
     link then we do not need to include it again.  Just claim that any symbol
     it contains is not a definition, so that our caller will not decide to
     (re)include this element.  */
  if (abfd->archive_pass)
    return false;

  /* Select the appropriate symbol table.  */
  if ((abfd->flags & DYNAMIC) == 0 || elf_dynsymtab (abfd) == 0)
    {
      hdr = &elf_tdata (abfd)->symtab_hdr;
      shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;
    }
  else
    {
      hdr = &elf_tdata (abfd)->dynsymtab_hdr;
      shndx_hdr = NULL;
    }

  symcount = hdr->sh_size / sizeof (Elf_External_Sym);

  /* The sh_info field of the symtab header tells us where the
     external symbols start.  We don't care about the local symbols.  */
  if (elf_bad_symtab (abfd))
    {
      extsymcount = symcount;
      extsymoff = 0;
    }
  else
    {
      extsymcount = symcount - hdr->sh_info;
      extsymoff = hdr->sh_info;
    }

  amt = extsymcount * sizeof (Elf_External_Sym);
  buf = (Elf_External_Sym *) bfd_malloc (amt);
  if (buf == NULL && extsymcount != 0)
    return false;

  /* Read in the symbol table.
     FIXME:  This ought to be cached somewhere.  */
  pos = hdr->sh_offset + extsymoff * sizeof (Elf_External_Sym);
  if (bfd_seek (abfd, pos, SEEK_SET) != 0
      || bfd_bread ((PTR) buf, amt, abfd) != amt)
    goto error_exit;

  if (shndx_hdr != NULL && shndx_hdr->sh_size != 0)
    {
      amt = extsymcount * sizeof (Elf_External_Sym_Shndx);
      shndx_buf = (Elf_External_Sym_Shndx *) bfd_malloc (amt);
      if (shndx_buf == NULL && extsymcount != 0)
	goto error_exit;

      pos = shndx_hdr->sh_offset + extsymoff * sizeof (Elf_External_Sym_Shndx);
      if (bfd_seek (abfd, pos, SEEK_SET) != 0
	  || bfd_bread ((PTR) shndx_buf, amt, abfd) != amt)
	goto error_exit;
    }

  /* Scan the symbol table looking for SYMDEF.  */
  esymend = buf + extsymcount;
  for (esym = buf, shndx = shndx_buf;
       esym < esymend;
       esym++, shndx = (shndx != NULL ? shndx + 1 : NULL))
    {
      Elf_Internal_Sym sym;
      const char * name;

      elf_swap_symbol_in (abfd, esym, shndx, &sym);

      name = bfd_elf_string_from_elf_section (abfd, hdr->sh_link, sym.st_name);
      if (name == (const char *) NULL)
	break;

      if (strcmp (name, symdef->name) == 0)
	{
	  result = is_global_data_symbol_definition (abfd, & sym);
	  break;
	}
    }

 error_exit:
  if (shndx_buf != NULL)
    free (shndx_buf);
  if (buf != NULL)
    free (buf);

  return result;
}

/* Add symbols from an ELF archive file to the linker hash table.  We
   don't use _bfd_generic_link_add_archive_symbols because of a
   problem which arises on UnixWare.  The UnixWare libc.so is an
   archive which includes an entry libc.so.1 which defines a bunch of
   symbols.  The libc.so archive also includes a number of other
   object files, which also define symbols, some of which are the same
   as those defined in libc.so.1.  Correct linking requires that we
   consider each object file in turn, and include it if it defines any
   symbols we need.  _bfd_generic_link_add_archive_symbols does not do
   this; it looks through the list of undefined symbols, and includes
   any object file which defines them.  When this algorithm is used on
   UnixWare, it winds up pulling in libc.so.1 early and defining a
   bunch of symbols.  This means that some of the other objects in the
   archive are not included in the link, which is incorrect since they
   precede libc.so.1 in the archive.

   Fortunately, ELF archive handling is simpler than that done by
   _bfd_generic_link_add_archive_symbols, which has to allow for a.out
   oddities.  In ELF, if we find a symbol in the archive map, and the
   symbol is currently undefined, we know that we must pull in that
   object file.

   Unfortunately, we do have to make multiple passes over the symbol
   table until nothing further is resolved.  */

static boolean
elf_link_add_archive_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  symindex c;
  boolean *defined = NULL;
  boolean *included = NULL;
  carsym *symdefs;
  boolean loop;
  bfd_size_type amt;

  if (! bfd_has_map (abfd))
    {
      /* An empty archive is a special case.  */
      if (bfd_openr_next_archived_file (abfd, (bfd *) NULL) == NULL)
	return true;
      bfd_set_error (bfd_error_no_armap);
      return false;
    }

  /* Keep track of all symbols we know to be already defined, and all
     files we know to be already included.  This is to speed up the
     second and subsequent passes.  */
  c = bfd_ardata (abfd)->symdef_count;
  if (c == 0)
    return true;
  amt = c;
  amt *= sizeof (boolean);
  defined = (boolean *) bfd_malloc (amt);
  included = (boolean *) bfd_malloc (amt);
  if (defined == (boolean *) NULL || included == (boolean *) NULL)
    goto error_return;
  memset (defined, 0, (size_t) amt);
  memset (included, 0, (size_t) amt);

  symdefs = bfd_ardata (abfd)->symdefs;

  do
    {
      file_ptr last;
      symindex i;
      carsym *symdef;
      carsym *symdefend;

      loop = false;
      last = -1;

      symdef = symdefs;
      symdefend = symdef + c;
      for (i = 0; symdef < symdefend; symdef++, i++)
	{
	  struct elf_link_hash_entry *h;
	  bfd *element;
	  struct bfd_link_hash_entry *undefs_tail;
	  symindex mark;

	  if (defined[i] || included[i])
	    continue;
	  if (symdef->file_offset == last)
	    {
	      included[i] = true;
	      continue;
	    }

	  h = elf_link_hash_lookup (elf_hash_table (info), symdef->name,
				    false, false, false);

	  if (h == NULL)
	    {
	      char *p, *copy;

	      /* If this is a default version (the name contains @@),
		 look up the symbol again without the version.  The
		 effect is that references to the symbol without the
		 version will be matched by the default symbol in the
		 archive.  */

	      p = strchr (symdef->name, ELF_VER_CHR);
	      if (p == NULL || p[1] != ELF_VER_CHR)
		continue;

	      copy = bfd_alloc (abfd, (bfd_size_type) (p - symdef->name + 1));
	      if (copy == NULL)
		goto error_return;
	      memcpy (copy, symdef->name, (size_t) (p - symdef->name));
	      copy[p - symdef->name] = '\0';

	      h = elf_link_hash_lookup (elf_hash_table (info), copy,
					false, false, false);

	      bfd_release (abfd, copy);
	    }

	  if (h == NULL)
	    continue;

	  if (h->root.type == bfd_link_hash_common)
	    {
	      /* We currently have a common symbol.  The archive map contains
		 a reference to this symbol, so we may want to include it.  We
		 only want to include it however, if this archive element
		 contains a definition of the symbol, not just another common
		 declaration of it.

		 Unfortunately some archivers (including GNU ar) will put
		 declarations of common symbols into their archive maps, as
		 well as real definitions, so we cannot just go by the archive
		 map alone.  Instead we must read in the element's symbol
		 table and check that to see what kind of symbol definition
		 this is.  */
	      if (! elf_link_is_defined_archive_symbol (abfd, symdef))
		continue;
	    }
	  else if (h->root.type != bfd_link_hash_undefined)
	    {
	      if (h->root.type != bfd_link_hash_undefweak)
		defined[i] = true;
	      continue;
	    }

	  /* We need to include this archive member.  */
	  element = _bfd_get_elt_at_filepos (abfd, symdef->file_offset);
	  if (element == (bfd *) NULL)
	    goto error_return;

	  if (! bfd_check_format (element, bfd_object))
	    goto error_return;

	  /* Doublecheck that we have not included this object
	     already--it should be impossible, but there may be
	     something wrong with the archive.  */
	  if (element->archive_pass != 0)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      goto error_return;
	    }
	  element->archive_pass = 1;

	  undefs_tail = info->hash->undefs_tail;

	  if (! (*info->callbacks->add_archive_element) (info, element,
							 symdef->name))
	    goto error_return;
	  if (! elf_link_add_object_symbols (element, info))
	    goto error_return;

	  /* If there are any new undefined symbols, we need to make
	     another pass through the archive in order to see whether
	     they can be defined.  FIXME: This isn't perfect, because
	     common symbols wind up on undefs_tail and because an
	     undefined symbol which is defined later on in this pass
	     does not require another pass.  This isn't a bug, but it
	     does make the code less efficient than it could be.  */
	  if (undefs_tail != info->hash->undefs_tail)
	    loop = true;

	  /* Look backward to mark all symbols from this object file
	     which we have already seen in this pass.  */
	  mark = i;
	  do
	    {
	      included[mark] = true;
	      if (mark == 0)
		break;
	      --mark;
	    }
	  while (symdefs[mark].file_offset == symdef->file_offset);

	  /* We mark subsequent symbols from this object file as we go
	     on through the loop.  */
	  last = symdef->file_offset;
	}
    }
  while (loop);

  free (defined);
  free (included);

  return true;

 error_return:
  if (defined != (boolean *) NULL)
    free (defined);
  if (included != (boolean *) NULL)
    free (included);
  return false;
}

/* This function is called when we want to define a new symbol.  It
   handles the various cases which arise when we find a definition in
   a dynamic object, or when there is already a definition in a
   dynamic object.  The new symbol is described by NAME, SYM, PSEC,
   and PVALUE.  We set SYM_HASH to the hash table entry.  We set
   OVERRIDE if the old symbol is overriding a new definition.  We set
   TYPE_CHANGE_OK if it is OK for the type to change.  We set
   SIZE_CHANGE_OK if it is OK for the size to change.  By OK to
   change, we mean that we shouldn't warn if the type or size does
   change. DT_NEEDED indicates if it comes from a DT_NEEDED entry of
   a shared object.  */

static boolean
elf_merge_symbol (abfd, info, name, sym, psec, pvalue, sym_hash,
		  override, type_change_ok, size_change_ok, dt_needed)
     bfd *abfd;
     struct bfd_link_info *info;
     const char *name;
     Elf_Internal_Sym *sym;
     asection **psec;
     bfd_vma *pvalue;
     struct elf_link_hash_entry **sym_hash;
     boolean *override;
     boolean *type_change_ok;
     boolean *size_change_ok;
     boolean dt_needed;
{
  asection *sec;
  struct elf_link_hash_entry *h;
  int bind;
  bfd *oldbfd;
  boolean newdyn, olddyn, olddef, newdef, newdyncommon, olddyncommon;

  *override = false;

  sec = *psec;
  bind = ELF_ST_BIND (sym->st_info);

  if (! bfd_is_und_section (sec))
    h = elf_link_hash_lookup (elf_hash_table (info), name, true, false, false);
  else
    h = ((struct elf_link_hash_entry *)
	 bfd_wrapped_link_hash_lookup (abfd, info, name, true, false, false));
  if (h == NULL)
    return false;
  *sym_hash = h;

  /* This code is for coping with dynamic objects, and is only useful
     if we are doing an ELF link.  */
  if (info->hash->creator != abfd->xvec)
    return true;

  /* For merging, we only care about real symbols.  */

  while (h->root.type == bfd_link_hash_indirect
	 || h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* If we just created the symbol, mark it as being an ELF symbol.
     Other than that, there is nothing to do--there is no merge issue
     with a newly defined symbol--so we just return.  */

  if (h->root.type == bfd_link_hash_new)
    {
      h->elf_link_hash_flags &=~ ELF_LINK_NON_ELF;
      return true;
    }

  /* OLDBFD is a BFD associated with the existing symbol.  */

  switch (h->root.type)
    {
    default:
      oldbfd = NULL;
      break;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      oldbfd = h->root.u.undef.abfd;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      oldbfd = h->root.u.def.section->owner;
      break;

    case bfd_link_hash_common:
      oldbfd = h->root.u.c.p->section->owner;
      break;
    }

  /* In cases involving weak versioned symbols, we may wind up trying
     to merge a symbol with itself.  Catch that here, to avoid the
     confusion that results if we try to override a symbol with
     itself.  The additional tests catch cases like
     _GLOBAL_OFFSET_TABLE_, which are regular symbols defined in a
     dynamic object, which we do want to handle here.  */
  if (abfd == oldbfd
      && ((abfd->flags & DYNAMIC) == 0
	  || (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0))
    return true;

  /* NEWDYN and OLDDYN indicate whether the new or old symbol,
     respectively, is from a dynamic object.  */

  if ((abfd->flags & DYNAMIC) != 0)
    newdyn = true;
  else
    newdyn = false;

  if (oldbfd != NULL)
    olddyn = (oldbfd->flags & DYNAMIC) != 0;
  else
    {
      asection *hsec;

      /* This code handles the special SHN_MIPS_{TEXT,DATA} section
	 indices used by MIPS ELF.  */
      switch (h->root.type)
	{
	default:
	  hsec = NULL;
	  break;

	case bfd_link_hash_defined:
	case bfd_link_hash_defweak:
	  hsec = h->root.u.def.section;
	  break;

	case bfd_link_hash_common:
	  hsec = h->root.u.c.p->section;
	  break;
	}

      if (hsec == NULL)
	olddyn = false;
      else
	olddyn = (hsec->symbol->flags & BSF_DYNAMIC) != 0;
    }

  /* NEWDEF and OLDDEF indicate whether the new or old symbol,
     respectively, appear to be a definition rather than reference.  */

  if (bfd_is_und_section (sec) || bfd_is_com_section (sec))
    newdef = false;
  else
    newdef = true;

  if (h->root.type == bfd_link_hash_undefined
      || h->root.type == bfd_link_hash_undefweak
      || h->root.type == bfd_link_hash_common)
    olddef = false;
  else
    olddef = true;

  /* NEWDYNCOMMON and OLDDYNCOMMON indicate whether the new or old
     symbol, respectively, appears to be a common symbol in a dynamic
     object.  If a symbol appears in an uninitialized section, and is
     not weak, and is not a function, then it may be a common symbol
     which was resolved when the dynamic object was created.  We want
     to treat such symbols specially, because they raise special
     considerations when setting the symbol size: if the symbol
     appears as a common symbol in a regular object, and the size in
     the regular object is larger, we must make sure that we use the
     larger size.  This problematic case can always be avoided in C,
     but it must be handled correctly when using Fortran shared
     libraries.

     Note that if NEWDYNCOMMON is set, NEWDEF will be set, and
     likewise for OLDDYNCOMMON and OLDDEF.

     Note that this test is just a heuristic, and that it is quite
     possible to have an uninitialized symbol in a shared object which
     is really a definition, rather than a common symbol.  This could
     lead to some minor confusion when the symbol really is a common
     symbol in some regular object.  However, I think it will be
     harmless.  */

  if (newdyn
      && newdef
      && (sec->flags & SEC_ALLOC) != 0
      && (sec->flags & SEC_LOAD) == 0
      && sym->st_size > 0
      && bind != STB_WEAK
      && ELF_ST_TYPE (sym->st_info) != STT_FUNC)
    newdyncommon = true;
  else
    newdyncommon = false;

  if (olddyn
      && olddef
      && h->root.type == bfd_link_hash_defined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->root.u.def.section->flags & SEC_ALLOC) != 0
      && (h->root.u.def.section->flags & SEC_LOAD) == 0
      && h->size > 0
      && h->type != STT_FUNC)
    olddyncommon = true;
  else
    olddyncommon = false;

  /* It's OK to change the type if either the existing symbol or the
     new symbol is weak unless it comes from a DT_NEEDED entry of
     a shared object, in which case, the DT_NEEDED entry may not be
     required at the run time.  */

  if ((! dt_needed && h->root.type == bfd_link_hash_defweak)
      || h->root.type == bfd_link_hash_undefweak
      || bind == STB_WEAK)
    *type_change_ok = true;

  /* It's OK to change the size if either the existing symbol or the
     new symbol is weak, or if the old symbol is undefined.  */

  if (*type_change_ok
      || h->root.type == bfd_link_hash_undefined)
    *size_change_ok = true;

  /* If both the old and the new symbols look like common symbols in a
     dynamic object, set the size of the symbol to the larger of the
     two.  */

  if (olddyncommon
      && newdyncommon
      && sym->st_size != h->size)
    {
      /* Since we think we have two common symbols, issue a multiple
	 common warning if desired.  Note that we only warn if the
	 size is different.  If the size is the same, we simply let
	 the old symbol override the new one as normally happens with
	 symbols defined in dynamic objects.  */

      if (! ((*info->callbacks->multiple_common)
	     (info, h->root.root.string, oldbfd, bfd_link_hash_common,
	      h->size, abfd, bfd_link_hash_common, sym->st_size)))
	return false;

      if (sym->st_size > h->size)
	h->size = sym->st_size;

      *size_change_ok = true;
    }

  /* If we are looking at a dynamic object, and we have found a
     definition, we need to see if the symbol was already defined by
     some other object.  If so, we want to use the existing
     definition, and we do not want to report a multiple symbol
     definition error; we do this by clobbering *PSEC to be
     bfd_und_section_ptr.

     We treat a common symbol as a definition if the symbol in the
     shared library is a function, since common symbols always
     represent variables; this can cause confusion in principle, but
     any such confusion would seem to indicate an erroneous program or
     shared library.  We also permit a common symbol in a regular
     object to override a weak symbol in a shared object.

     We prefer a non-weak definition in a shared library to a weak
     definition in the executable unless it comes from a DT_NEEDED
     entry of a shared object, in which case, the DT_NEEDED entry
     may not be required at the run time.  */

  if (newdyn
      && newdef
      && (olddef
	  || (h->root.type == bfd_link_hash_common
	      && (bind == STB_WEAK
		  || ELF_ST_TYPE (sym->st_info) == STT_FUNC)))
      && (h->root.type != bfd_link_hash_defweak
	  || dt_needed
	  || bind == STB_WEAK))
    {
      *override = true;
      newdef = false;
      newdyncommon = false;

      *psec = sec = bfd_und_section_ptr;
      *size_change_ok = true;

      /* If we get here when the old symbol is a common symbol, then
	 we are explicitly letting it override a weak symbol or
	 function in a dynamic object, and we don't want to warn about
	 a type change.  If the old symbol is a defined symbol, a type
	 change warning may still be appropriate.  */

      if (h->root.type == bfd_link_hash_common)
	*type_change_ok = true;
    }

  /* Handle the special case of an old common symbol merging with a
     new symbol which looks like a common symbol in a shared object.
     We change *PSEC and *PVALUE to make the new symbol look like a
     common symbol, and let _bfd_generic_link_add_one_symbol will do
     the right thing.  */

  if (newdyncommon
      && h->root.type == bfd_link_hash_common)
    {
      *override = true;
      newdef = false;
      newdyncommon = false;
      *pvalue = sym->st_size;
      *psec = sec = bfd_com_section_ptr;
      *size_change_ok = true;
    }

  /* If the old symbol is from a dynamic object, and the new symbol is
     a definition which is not from a dynamic object, then the new
     symbol overrides the old symbol.  Symbols from regular files
     always take precedence over symbols from dynamic objects, even if
     they are defined after the dynamic object in the link.

     As above, we again permit a common symbol in a regular object to
     override a definition in a shared object if the shared object
     symbol is a function or is weak.

     As above, we permit a non-weak definition in a shared object to
     override a weak definition in a regular object.  */

  if (! newdyn
      && (newdef
	  || (bfd_is_com_section (sec)
	      && (h->root.type == bfd_link_hash_defweak
		  || h->type == STT_FUNC)))
      && olddyn
      && olddef
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (bind != STB_WEAK
	  || h->root.type == bfd_link_hash_defweak))
    {
      /* Change the hash table entry to undefined, and let
	 _bfd_generic_link_add_one_symbol do the right thing with the
	 new definition.  */

      h->root.type = bfd_link_hash_undefined;
      h->root.u.undef.abfd = h->root.u.def.section->owner;
      *size_change_ok = true;

      olddef = false;
      olddyncommon = false;

      /* We again permit a type change when a common symbol may be
	 overriding a function.  */

      if (bfd_is_com_section (sec))
	*type_change_ok = true;

      /* This union may have been set to be non-NULL when this symbol
	 was seen in a dynamic object.  We must force the union to be
	 NULL, so that it is correct for a regular symbol.  */

      h->verinfo.vertree = NULL;

      /* In this special case, if H is the target of an indirection,
	 we want the caller to frob with H rather than with the
	 indirect symbol.  That will permit the caller to redefine the
	 target of the indirection, rather than the indirect symbol
	 itself.  FIXME: This will break the -y option if we store a
	 symbol with a different name.  */
      *sym_hash = h;
    }

  /* Handle the special case of a new common symbol merging with an
     old symbol that looks like it might be a common symbol defined in
     a shared object.  Note that we have already handled the case in
     which a new common symbol should simply override the definition
     in the shared library.  */

  if (! newdyn
      && bfd_is_com_section (sec)
      && olddyncommon)
    {
      /* It would be best if we could set the hash table entry to a
	 common symbol, but we don't know what to use for the section
	 or the alignment.  */
      if (! ((*info->callbacks->multiple_common)
	     (info, h->root.root.string, oldbfd, bfd_link_hash_common,
	      h->size, abfd, bfd_link_hash_common, sym->st_size)))
	return false;

      /* If the predumed common symbol in the dynamic object is
	 larger, pretend that the new symbol has its size.  */

      if (h->size > *pvalue)
	*pvalue = h->size;

      /* FIXME: We no longer know the alignment required by the symbol
	 in the dynamic object, so we just wind up using the one from
	 the regular object.  */

      olddef = false;
      olddyncommon = false;

      h->root.type = bfd_link_hash_undefined;
      h->root.u.undef.abfd = h->root.u.def.section->owner;

      *size_change_ok = true;
      *type_change_ok = true;

      h->verinfo.vertree = NULL;
    }

  /* Handle the special case of a weak definition in a regular object
     followed by a non-weak definition in a shared object.  In this
     case, we prefer the definition in the shared object unless it
     comes from a DT_NEEDED entry of a shared object, in which case,
     the DT_NEEDED entry may not be required at the run time.  */
  if (olddef
      && ! dt_needed
      && h->root.type == bfd_link_hash_defweak
      && newdef
      && newdyn
      && bind != STB_WEAK)
    {
      /* To make this work we have to frob the flags so that the rest
	 of the code does not think we are using the regular
	 definition.  */
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)
	h->elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;
      else if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0)
	h->elf_link_hash_flags |= ELF_LINK_HASH_REF_DYNAMIC;
      h->elf_link_hash_flags &= ~ (ELF_LINK_HASH_DEF_REGULAR
				   | ELF_LINK_HASH_DEF_DYNAMIC);

      /* If H is the target of an indirection, we want the caller to
	 use H rather than the indirect symbol.  Otherwise if we are
	 defining a new indirect symbol we will wind up attaching it
	 to the entry we are overriding.  */
      *sym_hash = h;
    }

  /* Handle the special case of a non-weak definition in a shared
     object followed by a weak definition in a regular object.  In
     this case we prefer to definition in the shared object.  To make
     this work we have to tell the caller to not treat the new symbol
     as a definition.  */
  if (olddef
      && olddyn
      && h->root.type != bfd_link_hash_defweak
      && newdef
      && ! newdyn
      && bind == STB_WEAK)
    *override = true;

  return true;
}

/* This function is called to create an indirect symbol from the
   default for the symbol with the default version if needed. The
   symbol is described by H, NAME, SYM, SEC, VALUE, and OVERRIDE.  We
   set DYNSYM if the new indirect symbol is dynamic. DT_NEEDED
   indicates if it comes from a DT_NEEDED entry of a shared object.  */

static boolean
elf_add_default_symbol (abfd, info, h, name, sym, sec, value,
			dynsym, override, dt_needed)
     bfd *abfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     const char *name;
     Elf_Internal_Sym *sym;
     asection **sec;
     bfd_vma *value;
     boolean *dynsym;
     boolean override;
     boolean dt_needed;
{
  boolean type_change_ok;
  boolean size_change_ok;
  char *shortname;
  struct elf_link_hash_entry *hi;
  struct elf_backend_data *bed;
  boolean collect;
  boolean dynamic;
  char *p;

  /* If this symbol has a version, and it is the default version, we
     create an indirect symbol from the default name to the fully
     decorated name.  This will cause external references which do not
     specify a version to be bound to this version of the symbol.  */
  p = strchr (name, ELF_VER_CHR);
  if (p == NULL || p[1] != ELF_VER_CHR)
    return true;

  if (override)
    {
      /* We are overridden by an old defition. We need to check if we
	 need to crreate the indirect symbol from the default name.  */
      hi = elf_link_hash_lookup (elf_hash_table (info), name, true,
				 false, false);
      BFD_ASSERT (hi != NULL);
      if (hi == h)
	return true;
      while (hi->root.type == bfd_link_hash_indirect
	     || hi->root.type == bfd_link_hash_warning)
	{
	  hi = (struct elf_link_hash_entry *) hi->root.u.i.link;
	  if (hi == h)
	    return true;
	}
    }

  bed = get_elf_backend_data (abfd);
  collect = bed->collect;
  dynamic = (abfd->flags & DYNAMIC) != 0;

  shortname = bfd_hash_allocate (&info->hash->table,
				 (size_t) (p - name + 1));
  if (shortname == NULL)
    return false;
  strncpy (shortname, name, (size_t) (p - name));
  shortname [p - name] = '\0';

  /* We are going to create a new symbol.  Merge it with any existing
     symbol with this name.  For the purposes of the merge, act as
     though we were defining the symbol we just defined, although we
     actually going to define an indirect symbol.  */
  type_change_ok = false;
  size_change_ok = false;
  if (! elf_merge_symbol (abfd, info, shortname, sym, sec, value,
			  &hi, &override, &type_change_ok,
			  &size_change_ok, dt_needed))
    return false;

  if (! override)
    {
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, shortname, BSF_INDIRECT, bfd_ind_section_ptr,
	      (bfd_vma) 0, name, false, collect,
	      (struct bfd_link_hash_entry **) &hi)))
	return false;
    }
  else
    {
      /* In this case the symbol named SHORTNAME is overriding the
	 indirect symbol we want to add.  We were planning on making
	 SHORTNAME an indirect symbol referring to NAME.  SHORTNAME
	 is the name without a version.  NAME is the fully versioned
	 name, and it is the default version.

	 Overriding means that we already saw a definition for the
	 symbol SHORTNAME in a regular object, and it is overriding
	 the symbol defined in the dynamic object.

	 When this happens, we actually want to change NAME, the
	 symbol we just added, to refer to SHORTNAME.  This will cause
	 references to NAME in the shared object to become references
	 to SHORTNAME in the regular object.  This is what we expect
	 when we override a function in a shared object: that the
	 references in the shared object will be mapped to the
	 definition in the regular object.  */

      while (hi->root.type == bfd_link_hash_indirect
	     || hi->root.type == bfd_link_hash_warning)
	hi = (struct elf_link_hash_entry *) hi->root.u.i.link;

      h->root.type = bfd_link_hash_indirect;
      h->root.u.i.link = (struct bfd_link_hash_entry *) hi;
      if (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC)
	{
	  h->elf_link_hash_flags &=~ ELF_LINK_HASH_DEF_DYNAMIC;
	  hi->elf_link_hash_flags |= ELF_LINK_HASH_REF_DYNAMIC;
	  if (hi->elf_link_hash_flags
	      & (ELF_LINK_HASH_REF_REGULAR
		 | ELF_LINK_HASH_DEF_REGULAR))
	    {
	      if (! _bfd_elf_link_record_dynamic_symbol (info, hi))
		return false;
	    }
	}

      /* Now set HI to H, so that the following code will set the
	 other fields correctly.  */
      hi = h;
    }

  /* If there is a duplicate definition somewhere, then HI may not
     point to an indirect symbol.  We will have reported an error to
     the user in that case.  */

  if (hi->root.type == bfd_link_hash_indirect)
    {
      struct elf_link_hash_entry *ht;

      /* If the symbol became indirect, then we assume that we have
	 not seen a definition before.  */
      BFD_ASSERT ((hi->elf_link_hash_flags
		   & (ELF_LINK_HASH_DEF_DYNAMIC
		      | ELF_LINK_HASH_DEF_REGULAR)) == 0);

      ht = (struct elf_link_hash_entry *) hi->root.u.i.link;
      (*bed->elf_backend_copy_indirect_symbol) (ht, hi);

      /* See if the new flags lead us to realize that the symbol must
	 be dynamic.  */
      if (! *dynsym)
	{
	  if (! dynamic)
	    {
	      if (info->shared
		  || ((hi->elf_link_hash_flags
		       & ELF_LINK_HASH_REF_DYNAMIC) != 0))
		*dynsym = true;
	    }
	  else
	    {
	      if ((hi->elf_link_hash_flags
		   & ELF_LINK_HASH_REF_REGULAR) != 0)
		*dynsym = true;
	    }
	}
    }

  /* We also need to define an indirection from the nondefault version
     of the symbol.  */

  shortname = bfd_hash_allocate (&info->hash->table, strlen (name));
  if (shortname == NULL)
    return false;
  strncpy (shortname, name, (size_t) (p - name));
  strcpy (shortname + (p - name), p + 1);

  /* Once again, merge with any existing symbol.  */
  type_change_ok = false;
  size_change_ok = false;
  if (! elf_merge_symbol (abfd, info, shortname, sym, sec, value,
			  &hi, &override, &type_change_ok,
			  &size_change_ok, dt_needed))
    return false;

  if (override)
    {
      /* Here SHORTNAME is a versioned name, so we don't expect to see
	 the type of override we do in the case above.  */
      (*_bfd_error_handler)
	(_("%s: warning: unexpected redefinition of `%s'"),
	 bfd_archive_filename (abfd), shortname);
    }
  else
    {
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, shortname, BSF_INDIRECT,
	      bfd_ind_section_ptr, (bfd_vma) 0, name, false,
	      collect, (struct bfd_link_hash_entry **) &hi)))
	return false;

      /* If there is a duplicate definition somewhere, then HI may not
	 point to an indirect symbol.  We will have reported an error
	 to the user in that case.  */

      if (hi->root.type == bfd_link_hash_indirect)
	{
	  /* If the symbol became indirect, then we assume that we have
	     not seen a definition before.  */
	  BFD_ASSERT ((hi->elf_link_hash_flags
		       & (ELF_LINK_HASH_DEF_DYNAMIC
			  | ELF_LINK_HASH_DEF_REGULAR)) == 0);

	  (*bed->elf_backend_copy_indirect_symbol) (h, hi);

	  /* See if the new flags lead us to realize that the symbol
	     must be dynamic.  */
	  if (! *dynsym)
	    {
	      if (! dynamic)
		{
		  if (info->shared
		      || ((hi->elf_link_hash_flags
			   & ELF_LINK_HASH_REF_DYNAMIC) != 0))
		    *dynsym = true;
		}
	      else
		{
		  if ((hi->elf_link_hash_flags
		       & ELF_LINK_HASH_REF_REGULAR) != 0)
		    *dynsym = true;
		}
	    }
	}
    }

  return true;
}

/* Add symbols from an ELF object file to the linker hash table.  */

static boolean
elf_link_add_object_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean (*add_symbol_hook) PARAMS ((bfd *, struct bfd_link_info *,
				      const Elf_Internal_Sym *,
				      const char **, flagword *,
				      asection **, bfd_vma *));
  boolean (*check_relocs) PARAMS ((bfd *, struct bfd_link_info *,
				   asection *, const Elf_Internal_Rela *));
  boolean collect;
  Elf_Internal_Shdr *hdr;
  Elf_Internal_Shdr *shndx_hdr;
  bfd_size_type symcount;
  bfd_size_type extsymcount;
  bfd_size_type extsymoff;
  Elf_External_Sym *buf = NULL;
  Elf_External_Sym_Shndx *shndx_buf = NULL;
  Elf_External_Sym_Shndx *shndx;
  struct elf_link_hash_entry **sym_hash;
  boolean dynamic;
  Elf_External_Versym *extversym = NULL;
  Elf_External_Versym *ever;
  Elf_External_Dyn *dynbuf = NULL;
  struct elf_link_hash_entry *weaks;
  Elf_External_Sym *esym;
  Elf_External_Sym *esymend;
  struct elf_backend_data *bed;
  boolean dt_needed;
  struct elf_link_hash_table * hash_table;
  file_ptr pos;
  bfd_size_type amt;

  hash_table = elf_hash_table (info);

  bed = get_elf_backend_data (abfd);
  add_symbol_hook = bed->elf_add_symbol_hook;
  collect = bed->collect;

  if ((abfd->flags & DYNAMIC) == 0)
    dynamic = false;
  else
    {
      dynamic = true;

      /* You can't use -r against a dynamic object.  Also, there's no
	 hope of using a dynamic object which does not exactly match
	 the format of the output file.  */
      if (info->relocateable || info->hash->creator != abfd->xvec)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  goto error_return;
	}
    }

  /* As a GNU extension, any input sections which are named
     .gnu.warning.SYMBOL are treated as warning symbols for the given
     symbol.  This differs from .gnu.warning sections, which generate
     warnings when they are included in an output file.  */
  if (! info->shared)
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	{
	  const char *name;

	  name = bfd_get_section_name (abfd, s);
	  if (strncmp (name, ".gnu.warning.", sizeof ".gnu.warning." - 1) == 0)
	    {
	      char *msg;
	      bfd_size_type sz;

	      name += sizeof ".gnu.warning." - 1;

	      /* If this is a shared object, then look up the symbol
		 in the hash table.  If it is there, and it is already
		 been defined, then we will not be using the entry
		 from this shared object, so we don't need to warn.
		 FIXME: If we see the definition in a regular object
		 later on, we will warn, but we shouldn't.  The only
		 fix is to keep track of what warnings we are supposed
		 to emit, and then handle them all at the end of the
		 link.  */
	      if (dynamic && abfd->xvec == info->hash->creator)
		{
		  struct elf_link_hash_entry *h;

		  h = elf_link_hash_lookup (hash_table, name,
					    false, false, true);

		  /* FIXME: What about bfd_link_hash_common?  */
		  if (h != NULL
		      && (h->root.type == bfd_link_hash_defined
			  || h->root.type == bfd_link_hash_defweak))
		    {
		      /* We don't want to issue this warning.  Clobber
			 the section size so that the warning does not
			 get copied into the output file.  */
		      s->_raw_size = 0;
		      continue;
		    }
		}

	      sz = bfd_section_size (abfd, s);
	      msg = (char *) bfd_alloc (abfd, sz + 1);
	      if (msg == NULL)
		goto error_return;

	      if (! bfd_get_section_contents (abfd, s, msg, (file_ptr) 0, sz))
		goto error_return;

	      msg[sz] = '\0';

	      if (! (_bfd_generic_link_add_one_symbol
		     (info, abfd, name, BSF_WARNING, s, (bfd_vma) 0, msg,
		      false, collect, (struct bfd_link_hash_entry **) NULL)))
		goto error_return;

	      if (! info->relocateable)
		{
		  /* Clobber the section size so that the warning does
		     not get copied into the output file.  */
		  s->_raw_size = 0;
		}
	    }
	}
    }

  /* If this is a dynamic object, we always link against the .dynsym
     symbol table, not the .symtab symbol table.  The dynamic linker
     will only see the .dynsym symbol table, so there is no reason to
     look at .symtab for a dynamic object.  */

  if (! dynamic || elf_dynsymtab (abfd) == 0)
    {
      hdr = &elf_tdata (abfd)->symtab_hdr;
      shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;
    }
  else
    {
      hdr = &elf_tdata (abfd)->dynsymtab_hdr;
      shndx_hdr = NULL;
    }

  if (dynamic)
    {
      /* Read in any version definitions.  */

      if (! _bfd_elf_slurp_version_tables (abfd))
	goto error_return;

      /* Read in the symbol versions, but don't bother to convert them
	 to internal format.  */
      if (elf_dynversym (abfd) != 0)
	{
	  Elf_Internal_Shdr *versymhdr;

	  versymhdr = &elf_tdata (abfd)->dynversym_hdr;
	  extversym = (Elf_External_Versym *) bfd_malloc (versymhdr->sh_size);
	  if (extversym == NULL)
	    goto error_return;
	  amt = versymhdr->sh_size;
	  if (bfd_seek (abfd, versymhdr->sh_offset, SEEK_SET) != 0
	      || bfd_bread ((PTR) extversym, amt, abfd) != amt)
	    goto error_return;
	}
    }

  symcount = hdr->sh_size / sizeof (Elf_External_Sym);

  /* The sh_info field of the symtab header tells us where the
     external symbols start.  We don't care about the local symbols at
     this point.  */
  if (elf_bad_symtab (abfd))
    {
      extsymcount = symcount;
      extsymoff = 0;
    }
  else
    {
      extsymcount = symcount - hdr->sh_info;
      extsymoff = hdr->sh_info;
    }

  amt = extsymcount * sizeof (Elf_External_Sym);
  buf = (Elf_External_Sym *) bfd_malloc (amt);
  if (buf == NULL && extsymcount != 0)
    goto error_return;

  if (shndx_hdr != NULL && shndx_hdr->sh_size != 0)
    {
      amt = extsymcount * sizeof (Elf_External_Sym_Shndx);
      shndx_buf = (Elf_External_Sym_Shndx *) bfd_malloc (amt);
      if (shndx_buf == NULL && extsymcount != 0)
	goto error_return;
    }

  /* We store a pointer to the hash table entry for each external
     symbol.  */
  amt = extsymcount * sizeof (struct elf_link_hash_entry *);
  sym_hash = (struct elf_link_hash_entry **) bfd_alloc (abfd, amt);
  if (sym_hash == NULL)
    goto error_return;
  elf_sym_hashes (abfd) = sym_hash;

  dt_needed = false;

  if (! dynamic)
    {
      /* If we are creating a shared library, create all the dynamic
	 sections immediately.  We need to attach them to something,
	 so we attach them to this BFD, provided it is the right
	 format.  FIXME: If there are no input BFD's of the same
	 format as the output, we can't make a shared library.  */
      if (info->shared
	  && is_elf_hash_table (info)
	  && ! hash_table->dynamic_sections_created
	  && abfd->xvec == info->hash->creator)
	{
	  if (! elf_link_create_dynamic_sections (abfd, info))
	    goto error_return;
	}
    }
  else if (! is_elf_hash_table (info))
    goto error_return;
  else
    {
      asection *s;
      boolean add_needed;
      const char *name;
      bfd_size_type oldsize;
      bfd_size_type strindex;

      /* Find the name to use in a DT_NEEDED entry that refers to this
	 object.  If the object has a DT_SONAME entry, we use it.
	 Otherwise, if the generic linker stuck something in
	 elf_dt_name, we use that.  Otherwise, we just use the file
	 name.  If the generic linker put a null string into
	 elf_dt_name, we don't make a DT_NEEDED entry at all, even if
	 there is a DT_SONAME entry.  */
      add_needed = true;
      name = bfd_get_filename (abfd);
      if (elf_dt_name (abfd) != NULL)
	{
	  name = elf_dt_name (abfd);
	  if (*name == '\0')
	    {
	      if (elf_dt_soname (abfd) != NULL)
		dt_needed = true;

	      add_needed = false;
	    }
	}
      s = bfd_get_section_by_name (abfd, ".dynamic");
      if (s != NULL)
	{
	  Elf_External_Dyn *extdyn;
	  Elf_External_Dyn *extdynend;
	  int elfsec;
	  unsigned long shlink;
	  int rpath;
	  int runpath;

	  dynbuf = (Elf_External_Dyn *) bfd_malloc (s->_raw_size);
	  if (dynbuf == NULL)
	    goto error_return;

	  if (! bfd_get_section_contents (abfd, s, (PTR) dynbuf,
					  (file_ptr) 0, s->_raw_size))
	    goto error_return;

	  elfsec = _bfd_elf_section_from_bfd_section (abfd, s);
	  if (elfsec == -1)
	    goto error_return;
	  shlink = elf_elfsections (abfd)[elfsec]->sh_link;

	  {
	    /* The shared libraries distributed with hpux11 have a bogus
	       sh_link field for the ".dynamic" section.  This code detects
	       when SHLINK refers to a section that is not a string table
	       and tries to find the string table for the ".dynsym" section
	       instead.  */
	    Elf_Internal_Shdr *shdr = elf_elfsections (abfd)[shlink];
	    if (shdr->sh_type != SHT_STRTAB)
	      {
		asection *ds = bfd_get_section_by_name (abfd, ".dynsym");
		int elfdsec = _bfd_elf_section_from_bfd_section (abfd, ds);
		if (elfdsec == -1)
		  goto error_return;
		shlink = elf_elfsections (abfd)[elfdsec]->sh_link;
	      }
	  }

	  extdyn = dynbuf;
	  extdynend = extdyn + s->_raw_size / sizeof (Elf_External_Dyn);
	  rpath = 0;
	  runpath = 0;
	  for (; extdyn < extdynend; extdyn++)
	    {
	      Elf_Internal_Dyn dyn;

	      elf_swap_dyn_in (abfd, extdyn, &dyn);
	      if (dyn.d_tag == DT_SONAME)
		{
		  unsigned int tagv = dyn.d_un.d_val;
		  name = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (name == NULL)
		    goto error_return;
		}
	      if (dyn.d_tag == DT_NEEDED)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;
		  unsigned int tagv = dyn.d_un.d_val;

		  amt = sizeof (struct bfd_link_needed_list);
		  n = (struct bfd_link_needed_list *) bfd_alloc (abfd, amt);
		  fnm = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, (bfd_size_type) strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = & hash_table->needed;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		}
	      if (dyn.d_tag == DT_RUNPATH)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;
		  unsigned int tagv = dyn.d_un.d_val;

		  /* When we see DT_RPATH before DT_RUNPATH, we have
		     to clear runpath.  Do _NOT_ bfd_release, as that
		     frees all more recently bfd_alloc'd blocks as
		     well.  */
		  if (rpath && hash_table->runpath)
		    hash_table->runpath = NULL;

		  amt = sizeof (struct bfd_link_needed_list);
		  n = (struct bfd_link_needed_list *) bfd_alloc (abfd, amt);
		  fnm = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, (bfd_size_type) strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = & hash_table->runpath;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		  runpath = 1;
		  rpath = 0;
		}
	      /* Ignore DT_RPATH if we have seen DT_RUNPATH.  */
	      if (!runpath && dyn.d_tag == DT_RPATH)
		{
		  struct bfd_link_needed_list *n, **pn;
		  char *fnm, *anm;
		  unsigned int tagv = dyn.d_un.d_val;

		  amt = sizeof (struct bfd_link_needed_list);
		  n = (struct bfd_link_needed_list *) bfd_alloc (abfd, amt);
		  fnm = bfd_elf_string_from_elf_section (abfd, shlink, tagv);
		  if (n == NULL || fnm == NULL)
		    goto error_return;
		  anm = bfd_alloc (abfd, (bfd_size_type) strlen (fnm) + 1);
		  if (anm == NULL)
		    goto error_return;
		  strcpy (anm, fnm);
		  n->name = anm;
		  n->by = abfd;
		  n->next = NULL;
		  for (pn = & hash_table->runpath;
		       *pn != NULL;
		       pn = &(*pn)->next)
		    ;
		  *pn = n;
		  rpath = 1;
		}
	    }

	  free (dynbuf);
	  dynbuf = NULL;
	}

      /* We do not want to include any of the sections in a dynamic
	 object in the output file.  We hack by simply clobbering the
	 list of sections in the BFD.  This could be handled more
	 cleanly by, say, a new section flag; the existing
	 SEC_NEVER_LOAD flag is not the one we want, because that one
	 still implies that the section takes up space in the output
	 file.  */
      bfd_section_list_clear (abfd);

      /* If this is the first dynamic object found in the link, create
	 the special sections required for dynamic linking.  */
      if (! hash_table->dynamic_sections_created)
	if (! elf_link_create_dynamic_sections (abfd, info))
	  goto error_return;

      if (add_needed)
	{
	  /* Add a DT_NEEDED entry for this dynamic object.  */
	  oldsize = _bfd_elf_strtab_size (hash_table->dynstr);
	  strindex = _bfd_elf_strtab_add (hash_table->dynstr, name, false);
	  if (strindex == (bfd_size_type) -1)
	    goto error_return;

	  if (oldsize == _bfd_elf_strtab_size (hash_table->dynstr))
	    {
	      asection *sdyn;
	      Elf_External_Dyn *dyncon, *dynconend;

	      /* The hash table size did not change, which means that
		 the dynamic object name was already entered.  If we
		 have already included this dynamic object in the
		 link, just ignore it.  There is no reason to include
		 a particular dynamic object more than once.  */
	      sdyn = bfd_get_section_by_name (hash_table->dynobj, ".dynamic");
	      BFD_ASSERT (sdyn != NULL);

	      dyncon = (Elf_External_Dyn *) sdyn->contents;
	      dynconend = (Elf_External_Dyn *) (sdyn->contents +
						sdyn->_raw_size);
	      for (; dyncon < dynconend; dyncon++)
		{
		  Elf_Internal_Dyn dyn;

		  elf_swap_dyn_in (hash_table->dynobj, dyncon, & dyn);
		  if (dyn.d_tag == DT_NEEDED
		      && dyn.d_un.d_val == strindex)
		    {
		      if (buf != NULL)
			free (buf);
		      if (extversym != NULL)
			free (extversym);
		      _bfd_elf_strtab_delref (hash_table->dynstr, strindex);
		      return true;
		    }
		}
	    }

	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_NEEDED, strindex))
	    goto error_return;
	}

      /* Save the SONAME, if there is one, because sometimes the
	 linker emulation code will need to know it.  */
      if (*name == '\0')
	name = basename (bfd_get_filename (abfd));
      elf_dt_name (abfd) = name;
    }

  pos = hdr->sh_offset + extsymoff * sizeof (Elf_External_Sym);
  amt = extsymcount * sizeof (Elf_External_Sym);
  if (bfd_seek (abfd, pos, SEEK_SET) != 0
      || bfd_bread ((PTR) buf, amt, abfd) != amt)
    goto error_return;

  if (shndx_hdr != NULL && shndx_hdr->sh_size != 0)
    {
      amt = extsymcount * sizeof (Elf_External_Sym_Shndx);
      pos = shndx_hdr->sh_offset + extsymoff * sizeof (Elf_External_Sym_Shndx);
      if (bfd_seek (abfd, pos, SEEK_SET) != 0
	  || bfd_bread ((PTR) shndx_buf, amt, abfd) != amt)
	goto error_return;
    }

  weaks = NULL;

  ever = extversym != NULL ? extversym + extsymoff : NULL;
  esymend = buf + extsymcount;
  for (esym = buf, shndx = shndx_buf;
       esym < esymend;
       esym++, sym_hash++, ever = (ever != NULL ? ever + 1 : NULL),
	 shndx = (shndx != NULL ? shndx + 1 : NULL))
    {
      Elf_Internal_Sym sym;
      int bind;
      bfd_vma value;
      asection *sec;
      flagword flags;
      const char *name;
      struct elf_link_hash_entry *h;
      boolean definition;
      boolean size_change_ok, type_change_ok;
      boolean new_weakdef;
      unsigned int old_alignment;
      boolean override;

      override = false;

      elf_swap_symbol_in (abfd, esym, shndx, &sym);

      flags = BSF_NO_FLAGS;
      sec = NULL;
      value = sym.st_value;
      *sym_hash = NULL;

      bind = ELF_ST_BIND (sym.st_info);
      if (bind == STB_LOCAL)
	{
	  /* This should be impossible, since ELF requires that all
	     global symbols follow all local symbols, and that sh_info
	     point to the first global symbol.  Unfortunatealy, Irix 5
	     screws this up.  */
	  continue;
	}
      else if (bind == STB_GLOBAL)
	{
	  if (sym.st_shndx != SHN_UNDEF
	      && sym.st_shndx != SHN_COMMON)
	    flags = BSF_GLOBAL;
	}
      else if (bind == STB_WEAK)
	flags = BSF_WEAK;
      else
	{
	  /* Leave it up to the processor backend.  */
	}

      if (sym.st_shndx == SHN_UNDEF)
	sec = bfd_und_section_ptr;
      else if (sym.st_shndx < SHN_LORESERVE || sym.st_shndx > SHN_HIRESERVE)
	{
	  sec = section_from_elf_index (abfd, sym.st_shndx);
	  if (sec == NULL)
	    sec = bfd_abs_section_ptr;
	  else if ((abfd->flags & (EXEC_P | DYNAMIC)) != 0)
	    value -= sec->vma;
	}
      else if (sym.st_shndx == SHN_ABS)
	sec = bfd_abs_section_ptr;
      else if (sym.st_shndx == SHN_COMMON)
	{
	  sec = bfd_com_section_ptr;
	  /* What ELF calls the size we call the value.  What ELF
	     calls the value we call the alignment.  */
	  value = sym.st_size;
	}
      else
	{
	  /* Leave it up to the processor backend.  */
	}

      name = bfd_elf_string_from_elf_section (abfd, hdr->sh_link, sym.st_name);
      if (name == (const char *) NULL)
	goto error_return;

      if (add_symbol_hook)
	{
	  if (! (*add_symbol_hook) (abfd, info, &sym, &name, &flags, &sec,
				    &value))
	    goto error_return;

	  /* The hook function sets the name to NULL if this symbol
	     should be skipped for some reason.  */
	  if (name == (const char *) NULL)
	    continue;
	}

      /* Sanity check that all possibilities were handled.  */
      if (sec == (asection *) NULL)
	{
	  bfd_set_error (bfd_error_bad_value);
	  goto error_return;
	}

      if (bfd_is_und_section (sec)
	  || bfd_is_com_section (sec))
	definition = false;
      else
	definition = true;

      size_change_ok = false;
      type_change_ok = get_elf_backend_data (abfd)->type_change_ok;
      old_alignment = 0;
      if (info->hash->creator->flavour == bfd_target_elf_flavour)
	{
	  Elf_Internal_Versym iver;
	  unsigned int vernum = 0;

	  if (ever != NULL)
	    {
	      _bfd_elf_swap_versym_in (abfd, ever, &iver);
	      vernum = iver.vs_vers & VERSYM_VERSION;

	      /* If this is a hidden symbol, or if it is not version
		 1, we append the version name to the symbol name.
		 However, we do not modify a non-hidden absolute
		 symbol, because it might be the version symbol
		 itself.  FIXME: What if it isn't?  */
	      if ((iver.vs_vers & VERSYM_HIDDEN) != 0
		  || (vernum > 1 && ! bfd_is_abs_section (sec)))
		{
		  const char *verstr;
		  unsigned int namelen;
		  bfd_size_type newlen;
		  char *newname, *p;

		  if (sym.st_shndx != SHN_UNDEF)
		    {
		      if (vernum > elf_tdata (abfd)->dynverdef_hdr.sh_info)
			{
			  (*_bfd_error_handler)
			    (_("%s: %s: invalid version %u (max %d)"),
			     bfd_archive_filename (abfd), name, vernum,
			     elf_tdata (abfd)->dynverdef_hdr.sh_info);
			  bfd_set_error (bfd_error_bad_value);
			  goto error_return;
			}
		      else if (vernum > 1)
			verstr =
			  elf_tdata (abfd)->verdef[vernum - 1].vd_nodename;
		      else
			verstr = "";
		    }
		  else
		    {
		      /* We cannot simply test for the number of
			 entries in the VERNEED section since the
			 numbers for the needed versions do not start
			 at 0.  */
		      Elf_Internal_Verneed *t;

		      verstr = NULL;
		      for (t = elf_tdata (abfd)->verref;
			   t != NULL;
			   t = t->vn_nextref)
			{
			  Elf_Internal_Vernaux *a;

			  for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
			    {
			      if (a->vna_other == vernum)
				{
				  verstr = a->vna_nodename;
				  break;
				}
			    }
			  if (a != NULL)
			    break;
			}
		      if (verstr == NULL)
			{
			  (*_bfd_error_handler)
			    (_("%s: %s: invalid needed version %d"),
			     bfd_archive_filename (abfd), name, vernum);
			  bfd_set_error (bfd_error_bad_value);
			  goto error_return;
			}
		    }

		  namelen = strlen (name);
		  newlen = namelen + strlen (verstr) + 2;
		  if ((iver.vs_vers & VERSYM_HIDDEN) == 0)
		    ++newlen;

		  newname = (char *) bfd_alloc (abfd, newlen);
		  if (newname == NULL)
		    goto error_return;
		  strcpy (newname, name);
		  p = newname + namelen;
		  *p++ = ELF_VER_CHR;
		  /* If this is a defined non-hidden version symbol,
		     we add another @ to the name.  This indicates the
		     default version of the symbol.  */
		  if ((iver.vs_vers & VERSYM_HIDDEN) == 0
		      && sym.st_shndx != SHN_UNDEF)
		    *p++ = ELF_VER_CHR;
		  strcpy (p, verstr);

		  name = newname;
		}
	    }

	  if (! elf_merge_symbol (abfd, info, name, &sym, &sec, &value,
				  sym_hash, &override, &type_change_ok,
				  &size_change_ok, dt_needed))
	    goto error_return;

	  if (override)
	    definition = false;

	  h = *sym_hash;
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  /* Remember the old alignment if this is a common symbol, so
	     that we don't reduce the alignment later on.  We can't
	     check later, because _bfd_generic_link_add_one_symbol
	     will set a default for the alignment which we want to
	     override.  */
	  if (h->root.type == bfd_link_hash_common)
	    old_alignment = h->root.u.c.p->alignment_power;

	  if (elf_tdata (abfd)->verdef != NULL
	      && ! override
	      && vernum > 1
	      && definition)
	    h->verinfo.verdef = &elf_tdata (abfd)->verdef[vernum - 1];
	}

      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, name, flags, sec, value, (const char *) NULL,
	      false, collect, (struct bfd_link_hash_entry **) sym_hash)))
	goto error_return;

      h = *sym_hash;
      while (h->root.type == bfd_link_hash_indirect
	     || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      *sym_hash = h;

      new_weakdef = false;
      if (dynamic
	  && definition
	  && (flags & BSF_WEAK) != 0
	  && ELF_ST_TYPE (sym.st_info) != STT_FUNC
	  && info->hash->creator->flavour == bfd_target_elf_flavour
	  && h->weakdef == NULL)
	{
	  /* Keep a list of all weak defined non function symbols from
	     a dynamic object, using the weakdef field.  Later in this
	     function we will set the weakdef field to the correct
	     value.  We only put non-function symbols from dynamic
	     objects on this list, because that happens to be the only
	     time we need to know the normal symbol corresponding to a
	     weak symbol, and the information is time consuming to
	     figure out.  If the weakdef field is not already NULL,
	     then this symbol was already defined by some previous
	     dynamic object, and we will be using that previous
	     definition anyhow.  */

	  h->weakdef = weaks;
	  weaks = h;
	  new_weakdef = true;
	}

      /* Set the alignment of a common symbol.  */
      if (sym.st_shndx == SHN_COMMON
	  && h->root.type == bfd_link_hash_common)
	{
	  unsigned int align;

	  align = bfd_log2 (sym.st_value);
	  if (align > old_alignment
	      /* Permit an alignment power of zero if an alignment of one
		 is specified and no other alignments have been specified.  */
	      || (sym.st_value == 1 && old_alignment == 0))
	    h->root.u.c.p->alignment_power = align;
	}

      if (info->hash->creator->flavour == bfd_target_elf_flavour)
	{
	  int old_flags;
	  boolean dynsym;
	  int new_flag;

	  /* Remember the symbol size and type.  */
	  if (sym.st_size != 0
	      && (definition || h->size == 0))
	    {
	      if (h->size != 0 && h->size != sym.st_size && ! size_change_ok)
		(*_bfd_error_handler)
		  (_("Warning: size of symbol `%s' changed from %lu to %lu in %s"),
		   name, (unsigned long) h->size, (unsigned long) sym.st_size,
		   bfd_archive_filename (abfd));

	      h->size = sym.st_size;
	    }

	  /* If this is a common symbol, then we always want H->SIZE
	     to be the size of the common symbol.  The code just above
	     won't fix the size if a common symbol becomes larger.  We
	     don't warn about a size change here, because that is
	     covered by --warn-common.  */
	  if (h->root.type == bfd_link_hash_common)
	    h->size = h->root.u.c.size;

	  if (ELF_ST_TYPE (sym.st_info) != STT_NOTYPE
	      && (definition || h->type == STT_NOTYPE))
	    {
	      if (h->type != STT_NOTYPE
		  && h->type != ELF_ST_TYPE (sym.st_info)
		  && ! type_change_ok)
		(*_bfd_error_handler)
		  (_("Warning: type of symbol `%s' changed from %d to %d in %s"),
		   name, h->type, ELF_ST_TYPE (sym.st_info),
		   bfd_archive_filename (abfd));

	      h->type = ELF_ST_TYPE (sym.st_info);
	    }

	  /* If st_other has a processor-specific meaning, specific code
	     might be needed here.  */
	  if (sym.st_other != 0)
	    {
	      /* Combine visibilities, using the most constraining one.  */
	      unsigned char hvis   = ELF_ST_VISIBILITY (h->other);
	      unsigned char symvis = ELF_ST_VISIBILITY (sym.st_other);

	      if (symvis && (hvis > symvis || hvis == 0))
		h->other = sym.st_other;

	      /* If neither has visibility, use the st_other of the
		 definition.  This is an arbitrary choice, since the
		 other bits have no general meaning.  */
	      if (!symvis && !hvis
		  && (definition || h->other == 0))
		h->other = sym.st_other;
	    }

	  /* Set a flag in the hash table entry indicating the type of
	     reference or definition we just found.  Keep a count of
	     the number of dynamic symbols we find.  A dynamic symbol
	     is one which is referenced or defined by both a regular
	     object and a shared object.  */
	  old_flags = h->elf_link_hash_flags;
	  dynsym = false;
	  if (! dynamic)
	    {
	      if (! definition)
		{
		  new_flag = ELF_LINK_HASH_REF_REGULAR;
		  if (bind != STB_WEAK)
		    new_flag |= ELF_LINK_HASH_REF_REGULAR_NONWEAK;
		}
	      else
		new_flag = ELF_LINK_HASH_DEF_REGULAR;
	      if (info->shared
		  || (old_flags & (ELF_LINK_HASH_DEF_DYNAMIC
				   | ELF_LINK_HASH_REF_DYNAMIC)) != 0)
		dynsym = true;
	    }
	  else
	    {
	      if (! definition)
		new_flag = ELF_LINK_HASH_REF_DYNAMIC;
	      else
		new_flag = ELF_LINK_HASH_DEF_DYNAMIC;
	      if ((old_flags & (ELF_LINK_HASH_DEF_REGULAR
				| ELF_LINK_HASH_REF_REGULAR)) != 0
		  || (h->weakdef != NULL
		      && ! new_weakdef
		      && h->weakdef->dynindx != -1))
		dynsym = true;
	    }

	  h->elf_link_hash_flags |= new_flag;

	  /* Check to see if we need to add an indirect symbol for
	     the default name.  */
	  if (definition || h->root.type == bfd_link_hash_common)
	    if (! elf_add_default_symbol (abfd, info, h, name, &sym,
					  &sec, &value, &dynsym,
					  override, dt_needed))
	      goto error_return;

	  if (dynsym && h->dynindx == -1)
	    {
	      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		goto error_return;
	      if (h->weakdef != NULL
		  && ! new_weakdef
		  && h->weakdef->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, h->weakdef))
		    goto error_return;
		}
	    }
	  else if (dynsym && h->dynindx != -1)
	    /* If the symbol already has a dynamic index, but
	       visibility says it should not be visible, turn it into
	       a local symbol.  */
	    switch (ELF_ST_VISIBILITY (h->other))
	      {
	      case STV_INTERNAL:
	      case STV_HIDDEN:
		(*bed->elf_backend_hide_symbol) (info, h, true);
		break;
	      }

	  if (dt_needed && definition
	      && (h->elf_link_hash_flags
		  & ELF_LINK_HASH_REF_REGULAR) != 0)
	    {
	      bfd_size_type oldsize;
	      bfd_size_type strindex;

	      if (! is_elf_hash_table (info))
		goto error_return;

	      /* The symbol from a DT_NEEDED object is referenced from
		 the regular object to create a dynamic executable. We
		 have to make sure there is a DT_NEEDED entry for it.  */

	      dt_needed = false;
	      oldsize = _bfd_elf_strtab_size (hash_table->dynstr);
	      strindex = _bfd_elf_strtab_add (hash_table->dynstr,
					      elf_dt_soname (abfd), false);
	      if (strindex == (bfd_size_type) -1)
		goto error_return;

	      if (oldsize == _bfd_elf_strtab_size (hash_table->dynstr))
		{
		  asection *sdyn;
		  Elf_External_Dyn *dyncon, *dynconend;

		  sdyn = bfd_get_section_by_name (hash_table->dynobj,
						  ".dynamic");
		  BFD_ASSERT (sdyn != NULL);

		  dyncon = (Elf_External_Dyn *) sdyn->contents;
		  dynconend = (Elf_External_Dyn *) (sdyn->contents +
						    sdyn->_raw_size);
		  for (; dyncon < dynconend; dyncon++)
		    {
		      Elf_Internal_Dyn dyn;

		      elf_swap_dyn_in (hash_table->dynobj,
				       dyncon, &dyn);
		      BFD_ASSERT (dyn.d_tag != DT_NEEDED ||
				  dyn.d_un.d_val != strindex);
		    }
		}

	      if (! elf_add_dynamic_entry (info, (bfd_vma) DT_NEEDED, strindex))
		goto error_return;
	    }
	}
    }

  /* Now set the weakdefs field correctly for all the weak defined
     symbols we found.  The only way to do this is to search all the
     symbols.  Since we only need the information for non functions in
     dynamic objects, that's the only time we actually put anything on
     the list WEAKS.  We need this information so that if a regular
     object refers to a symbol defined weakly in a dynamic object, the
     real symbol in the dynamic object is also put in the dynamic
     symbols; we also must arrange for both symbols to point to the
     same memory location.  We could handle the general case of symbol
     aliasing, but a general symbol alias can only be generated in
     assembler code, handling it correctly would be very time
     consuming, and other ELF linkers don't handle general aliasing
     either.  */
  while (weaks != NULL)
    {
      struct elf_link_hash_entry *hlook;
      asection *slook;
      bfd_vma vlook;
      struct elf_link_hash_entry **hpp;
      struct elf_link_hash_entry **hppend;

      hlook = weaks;
      weaks = hlook->weakdef;
      hlook->weakdef = NULL;

      BFD_ASSERT (hlook->root.type == bfd_link_hash_defined
		  || hlook->root.type == bfd_link_hash_defweak
		  || hlook->root.type == bfd_link_hash_common
		  || hlook->root.type == bfd_link_hash_indirect);
      slook = hlook->root.u.def.section;
      vlook = hlook->root.u.def.value;

      hpp = elf_sym_hashes (abfd);
      hppend = hpp + extsymcount;
      for (; hpp < hppend; hpp++)
	{
	  struct elf_link_hash_entry *h;

	  h = *hpp;
	  if (h != NULL && h != hlook
	      && h->root.type == bfd_link_hash_defined
	      && h->root.u.def.section == slook
	      && h->root.u.def.value == vlook)
	    {
	      hlook->weakdef = h;

	      /* If the weak definition is in the list of dynamic
		 symbols, make sure the real definition is put there
		 as well.  */
	      if (hlook->dynindx != -1
		  && h->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		    goto error_return;
		}

	      /* If the real definition is in the list of dynamic
		 symbols, make sure the weak definition is put there
		 as well.  If we don't do this, then the dynamic
		 loader might not merge the entries for the real
		 definition and the weak definition.  */
	      if (h->dynindx != -1
		  && hlook->dynindx == -1)
		{
		  if (! _bfd_elf_link_record_dynamic_symbol (info, hlook))
		    goto error_return;
		}

	      break;
	    }
	}
    }

  if (buf != NULL)
    {
      free (buf);
      buf = NULL;
    }

  if (extversym != NULL)
    {
      free (extversym);
      extversym = NULL;
    }

  /* If this object is the same format as the output object, and it is
     not a shared library, then let the backend look through the
     relocs.

     This is required to build global offset table entries and to
     arrange for dynamic relocs.  It is not required for the
     particular common case of linking non PIC code, even when linking
     against shared libraries, but unfortunately there is no way of
     knowing whether an object file has been compiled PIC or not.
     Looking through the relocs is not particularly time consuming.
     The problem is that we must either (1) keep the relocs in memory,
     which causes the linker to require additional runtime memory or
     (2) read the relocs twice from the input file, which wastes time.
     This would be a good case for using mmap.

     I have no idea how to handle linking PIC code into a file of a
     different format.  It probably can't be done.  */
  check_relocs = get_elf_backend_data (abfd)->check_relocs;
  if (! dynamic
      && abfd->xvec == info->hash->creator
      && check_relocs != NULL)
    {
      asection *o;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  Elf_Internal_Rela *internal_relocs;
	  boolean ok;

	  if ((o->flags & SEC_RELOC) == 0
	      || o->reloc_count == 0
	      || ((info->strip == strip_all || info->strip == strip_debugger)
		  && (o->flags & SEC_DEBUGGING) != 0)
	      || bfd_is_abs_section (o->output_section))
	    continue;

	  internal_relocs = (NAME(_bfd_elf,link_read_relocs)
			     (abfd, o, (PTR) NULL,
			      (Elf_Internal_Rela *) NULL,
			      info->keep_memory));
	  if (internal_relocs == NULL)
	    goto error_return;

	  ok = (*check_relocs) (abfd, info, o, internal_relocs);

	  if (! info->keep_memory)
	    free (internal_relocs);

	  if (! ok)
	    goto error_return;
	}
    }

  /* If this is a non-traditional, non-relocateable link, try to
     optimize the handling of the .stab/.stabstr sections.  */
  if (! dynamic
      && ! info->relocateable
      && ! info->traditional_format
      && info->hash->creator->flavour == bfd_target_elf_flavour
      && is_elf_hash_table (info)
      && (info->strip != strip_all && info->strip != strip_debugger))
    {
      asection *stab, *stabstr;

      stab = bfd_get_section_by_name (abfd, ".stab");
      if (stab != NULL && !(stab->flags & SEC_MERGE))
	{
	  stabstr = bfd_get_section_by_name (abfd, ".stabstr");

	  if (stabstr != NULL)
	    {
	      struct bfd_elf_section_data *secdata;

	      secdata = elf_section_data (stab);
	      if (! _bfd_link_section_stabs (abfd,
					     & hash_table->stab_info,
					     stab, stabstr,
					     &secdata->sec_info))
		goto error_return;
	      if (secdata->sec_info)
		secdata->sec_info_type = ELF_INFO_TYPE_STABS;
	    }
	}
    }

  if (! info->relocateable && ! dynamic
      && is_elf_hash_table (info))
    {
      asection *s;

      for (s = abfd->sections; s != NULL; s = s->next)
	if (s->flags & SEC_MERGE)
	  {
	    struct bfd_elf_section_data *secdata;

	    secdata = elf_section_data (s);
	    if (! _bfd_merge_section (abfd,
				      & hash_table->merge_info,
				      s, &secdata->sec_info))
	      goto error_return;
	    else if (secdata->sec_info)
	      secdata->sec_info_type = ELF_INFO_TYPE_MERGE;
	  }
    }

  return true;

 error_return:
  if (buf != NULL)
    free (buf);
  if (dynbuf != NULL)
    free (dynbuf);
  if (extversym != NULL)
    free (extversym);
  return false;
}

/* Create some sections which will be filled in with dynamic linking
   information.  ABFD is an input file which requires dynamic sections
   to be created.  The dynamic sections take up virtual memory space
   when the final executable is run, so we need to create them before
   addresses are assigned to the output sections.  We work out the
   actual contents and size of these sections later.  */

boolean
elf_link_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct elf_backend_data *bed;

  if (! is_elf_hash_table (info))
    return false;

  if (elf_hash_table (info)->dynamic_sections_created)
    return true;

  /* Make sure that all dynamic sections use the same input BFD.  */
  if (elf_hash_table (info)->dynobj == NULL)
    elf_hash_table (info)->dynobj = abfd;
  else
    abfd = elf_hash_table (info)->dynobj;

  /* Note that we set the SEC_IN_MEMORY flag for all of these
     sections.  */
  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
	   | SEC_IN_MEMORY | SEC_LINKER_CREATED);

  /* A dynamically linked executable has a .interp section, but a
     shared library does not.  */
  if (! info->shared)
    {
      s = bfd_make_section (abfd, ".interp");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY))
	return false;
    }

  if (! info->traditional_format
      && info->hash->creator->flavour == bfd_target_elf_flavour)
    {
      s = bfd_make_section (abfd, ".eh_frame_hdr");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
	  || ! bfd_set_section_alignment (abfd, s, 2))
	return false;
    }

  /* Create sections to hold version informations.  These are removed
     if they are not needed.  */
  s = bfd_make_section (abfd, ".gnu.version_d");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  s = bfd_make_section (abfd, ".gnu.version");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, 1))
    return false;

  s = bfd_make_section (abfd, ".gnu.version_r");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  s = bfd_make_section (abfd, ".dynsym");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  s = bfd_make_section (abfd, ".dynstr");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY))
    return false;

  /* Create a strtab to hold the dynamic symbol names.  */
  if (elf_hash_table (info)->dynstr == NULL)
    {
      elf_hash_table (info)->dynstr = _bfd_elf_strtab_init ();
      if (elf_hash_table (info)->dynstr == NULL)
	return false;
    }

  s = bfd_make_section (abfd, ".dynamic");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;

  /* The special symbol _DYNAMIC is always set to the start of the
     .dynamic section.  This call occurs before we have processed the
     symbols for any dynamic object, so we don't have to worry about
     overriding a dynamic definition.  We could set _DYNAMIC in a
     linker script, but we only want to define it if we are, in fact,
     creating a .dynamic section.  We don't want to define it if there
     is no .dynamic section, since on some ELF platforms the start up
     code examines it to decide how to initialize the process.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_DYNAMIC", BSF_GLOBAL, s, (bfd_vma) 0,
	  (const char *) NULL, false, get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! _bfd_elf_link_record_dynamic_symbol (info, h))
    return false;

  bed = get_elf_backend_data (abfd);

  s = bfd_make_section (abfd, ".hash");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags | SEC_READONLY)
      || ! bfd_set_section_alignment (abfd, s, LOG_FILE_ALIGN))
    return false;
  elf_section_data (s)->this_hdr.sh_entsize = bed->s->sizeof_hash_entry;

  /* Let the backend create the rest of the sections.  This lets the
     backend set the right flags.  The backend will normally create
     the .got and .plt sections.  */
  if (! (*bed->elf_backend_create_dynamic_sections) (abfd, info))
    return false;

  elf_hash_table (info)->dynamic_sections_created = true;

  return true;
}

/* Add an entry to the .dynamic table.  */

boolean
elf_add_dynamic_entry (info, tag, val)
     struct bfd_link_info *info;
     bfd_vma tag;
     bfd_vma val;
{
  Elf_Internal_Dyn dyn;
  bfd *dynobj;
  asection *s;
  bfd_size_type newsize;
  bfd_byte *newcontents;

  if (! is_elf_hash_table (info))
    return false;

  dynobj = elf_hash_table (info)->dynobj;

  s = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (s != NULL);

  newsize = s->_raw_size + sizeof (Elf_External_Dyn);
  newcontents = (bfd_byte *) bfd_realloc (s->contents, newsize);
  if (newcontents == NULL)
    return false;

  dyn.d_tag = tag;
  dyn.d_un.d_val = val;
  elf_swap_dyn_out (dynobj, &dyn,
		    (Elf_External_Dyn *) (newcontents + s->_raw_size));

  s->_raw_size = newsize;
  s->contents = newcontents;

  return true;
}

/* Record a new local dynamic symbol.  */

boolean
elf_link_record_local_dynamic_symbol (info, input_bfd, input_indx)
     struct bfd_link_info *info;
     bfd *input_bfd;
     long input_indx;
{
  struct elf_link_local_dynamic_entry *entry;
  struct elf_link_hash_table *eht;
  struct elf_strtab_hash *dynstr;
  Elf_External_Sym esym;
  Elf_External_Sym_Shndx eshndx;
  Elf_External_Sym_Shndx *shndx;
  unsigned long dynstr_index;
  char *name;
  file_ptr pos;
  bfd_size_type amt;

  if (! is_elf_hash_table (info))
    return false;

  /* See if the entry exists already.  */
  for (entry = elf_hash_table (info)->dynlocal; entry ; entry = entry->next)
    if (entry->input_bfd == input_bfd && entry->input_indx == input_indx)
      return true;

  entry = (struct elf_link_local_dynamic_entry *)
    bfd_alloc (input_bfd, (bfd_size_type) sizeof (*entry));
  if (entry == NULL)
    return false;

  /* Go find the symbol, so that we can find it's name.  */
  amt = sizeof (Elf_External_Sym);
  pos = elf_tdata (input_bfd)->symtab_hdr.sh_offset + input_indx * amt;
  if (bfd_seek (input_bfd, pos, SEEK_SET) != 0
      || bfd_bread ((PTR) &esym, amt, input_bfd) != amt)
    return false;
  shndx = NULL;
  if (elf_tdata (input_bfd)->symtab_shndx_hdr.sh_size != 0)
    {
      amt = sizeof (Elf_External_Sym_Shndx);
      pos = elf_tdata (input_bfd)->symtab_shndx_hdr.sh_offset;
      pos += input_indx * amt;
      shndx = &eshndx;
      if (bfd_seek (input_bfd, pos, SEEK_SET) != 0
	  || bfd_bread ((PTR) shndx, amt, input_bfd) != amt)
	return false;
    }
  elf_swap_symbol_in (input_bfd, &esym, shndx, &entry->isym);

  name = (bfd_elf_string_from_elf_section
	  (input_bfd, elf_tdata (input_bfd)->symtab_hdr.sh_link,
	   entry->isym.st_name));

  dynstr = elf_hash_table (info)->dynstr;
  if (dynstr == NULL)
    {
      /* Create a strtab to hold the dynamic symbol names.  */
      elf_hash_table (info)->dynstr = dynstr = _bfd_elf_strtab_init ();
      if (dynstr == NULL)
	return false;
    }

  dynstr_index = _bfd_elf_strtab_add (dynstr, name, false);
  if (dynstr_index == (unsigned long) -1)
    return false;
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

  return true;
}

/* Read and swap the relocs from the section indicated by SHDR.  This
   may be either a REL or a RELA section.  The relocations are
   translated into RELA relocations and stored in INTERNAL_RELOCS,
   which should have already been allocated to contain enough space.
   The EXTERNAL_RELOCS are a buffer where the external form of the
   relocations should be stored.

   Returns false if something goes wrong.  */

static boolean
elf_link_read_relocs_from_section (abfd, shdr, external_relocs,
				   internal_relocs)
     bfd *abfd;
     Elf_Internal_Shdr *shdr;
     PTR external_relocs;
     Elf_Internal_Rela *internal_relocs;
{
  struct elf_backend_data *bed;
  bfd_size_type amt;

  /* If there aren't any relocations, that's OK.  */
  if (!shdr)
    return true;

  /* Position ourselves at the start of the section.  */
  if (bfd_seek (abfd, shdr->sh_offset, SEEK_SET) != 0)
    return false;

  /* Read the relocations.  */
  if (bfd_bread (external_relocs, shdr->sh_size, abfd) != shdr->sh_size)
    return false;

  bed = get_elf_backend_data (abfd);

  /* Convert the external relocations to the internal format.  */
  if (shdr->sh_entsize == sizeof (Elf_External_Rel))
    {
      Elf_External_Rel *erel;
      Elf_External_Rel *erelend;
      Elf_Internal_Rela *irela;
      Elf_Internal_Rel *irel;

      erel = (Elf_External_Rel *) external_relocs;
      erelend = erel + NUM_SHDR_ENTRIES (shdr);
      irela = internal_relocs;
      amt = bed->s->int_rels_per_ext_rel * sizeof (Elf_Internal_Rel);
      irel = bfd_alloc (abfd, amt);
      for (; erel < erelend; erel++, irela += bed->s->int_rels_per_ext_rel)
	{
	  unsigned int i;

	  if (bed->s->swap_reloc_in)
	    (*bed->s->swap_reloc_in) (abfd, (bfd_byte *) erel, irel);
	  else
	    elf_swap_reloc_in (abfd, erel, irel);

	  for (i = 0; i < bed->s->int_rels_per_ext_rel; ++i)
	    {
	      irela[i].r_offset = irel[i].r_offset;
	      irela[i].r_info = irel[i].r_info;
	      irela[i].r_addend = 0;
	    }
	}
    }
  else
    {
      Elf_External_Rela *erela;
      Elf_External_Rela *erelaend;
      Elf_Internal_Rela *irela;

      BFD_ASSERT (shdr->sh_entsize == sizeof (Elf_External_Rela));

      erela = (Elf_External_Rela *) external_relocs;
      erelaend = erela + NUM_SHDR_ENTRIES (shdr);
      irela = internal_relocs;
      for (; erela < erelaend; erela++, irela += bed->s->int_rels_per_ext_rel)
	{
	  if (bed->s->swap_reloca_in)
	    (*bed->s->swap_reloca_in) (abfd, (bfd_byte *) erela, irela);
	  else
	    elf_swap_reloca_in (abfd, erela, irela);
	}
    }

  return true;
}

/* Read and swap the relocs for a section O.  They may have been
   cached.  If the EXTERNAL_RELOCS and INTERNAL_RELOCS arguments are
   not NULL, they are used as buffers to read into.  They are known to
   be large enough.  If the INTERNAL_RELOCS relocs argument is NULL,
   the return value is allocated using either malloc or bfd_alloc,
   according to the KEEP_MEMORY argument.  If O has two relocation
   sections (both REL and RELA relocations), then the REL_HDR
   relocations will appear first in INTERNAL_RELOCS, followed by the
   REL_HDR2 relocations.  */

Elf_Internal_Rela *
NAME(_bfd_elf,link_read_relocs) (abfd, o, external_relocs, internal_relocs,
				 keep_memory)
     bfd *abfd;
     asection *o;
     PTR external_relocs;
     Elf_Internal_Rela *internal_relocs;
     boolean keep_memory;
{
  Elf_Internal_Shdr *rel_hdr;
  PTR alloc1 = NULL;
  Elf_Internal_Rela *alloc2 = NULL;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  if (elf_section_data (o)->relocs != NULL)
    return elf_section_data (o)->relocs;

  if (o->reloc_count == 0)
    return NULL;

  rel_hdr = &elf_section_data (o)->rel_hdr;

  if (internal_relocs == NULL)
    {
      bfd_size_type size;

      size = o->reloc_count;
      size *= bed->s->int_rels_per_ext_rel * sizeof (Elf_Internal_Rela);
      if (keep_memory)
	internal_relocs = (Elf_Internal_Rela *) bfd_alloc (abfd, size);
      else
	internal_relocs = alloc2 = (Elf_Internal_Rela *) bfd_malloc (size);
      if (internal_relocs == NULL)
	goto error_return;
    }

  if (external_relocs == NULL)
    {
      bfd_size_type size = rel_hdr->sh_size;

      if (elf_section_data (o)->rel_hdr2)
	size += elf_section_data (o)->rel_hdr2->sh_size;
      alloc1 = (PTR) bfd_malloc (size);
      if (alloc1 == NULL)
	goto error_return;
      external_relocs = alloc1;
    }

  if (!elf_link_read_relocs_from_section (abfd, rel_hdr,
					  external_relocs,
					  internal_relocs))
    goto error_return;
  if (!elf_link_read_relocs_from_section
      (abfd,
       elf_section_data (o)->rel_hdr2,
       ((bfd_byte *) external_relocs) + rel_hdr->sh_size,
       internal_relocs + (NUM_SHDR_ENTRIES (rel_hdr)
			  * bed->s->int_rels_per_ext_rel)))
    goto error_return;

  /* Cache the results for next time, if we can.  */
  if (keep_memory)
    elf_section_data (o)->relocs = internal_relocs;

  if (alloc1 != NULL)
    free (alloc1);

  /* Don't free alloc2, since if it was allocated we are passing it
     back (under the name of internal_relocs).  */

  return internal_relocs;

 error_return:
  if (alloc1 != NULL)
    free (alloc1);
  if (alloc2 != NULL)
    free (alloc2);
  return NULL;
}

/* Record an assignment to a symbol made by a linker script.  We need
   this in case some dynamic object refers to this symbol.  */

boolean
NAME(bfd_elf,record_link_assignment) (output_bfd, info, name, provide)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info;
     const char *name;
     boolean provide;
{
  struct elf_link_hash_entry *h;

  if (info->hash->creator->flavour != bfd_target_elf_flavour)
    return true;

  h = elf_link_hash_lookup (elf_hash_table (info), name, true, true, false);
  if (h == NULL)
    return false;

  if (h->root.type == bfd_link_hash_new)
    h->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;

  /* If this symbol is being provided by the linker script, and it is
     currently defined by a dynamic object, but not by a regular
     object, then mark it as undefined so that the generic linker will
     force the correct value.  */
  if (provide
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    h->root.type = bfd_link_hash_undefined;

  /* If this symbol is not being provided by the linker script, and it is
     currently defined by a dynamic object, but not by a regular object,
     then clear out any version information because the symbol will not be
     associated with the dynamic object any more.  */
  if (!provide
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    h->verinfo.verdef = NULL;

  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;

  if (((h->elf_link_hash_flags & (ELF_LINK_HASH_DEF_DYNAMIC
				  | ELF_LINK_HASH_REF_DYNAMIC)) != 0
       || info->shared)
      && h->dynindx == -1)
    {
      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
	return false;

      /* If this is a weak defined symbol, and we know a corresponding
	 real symbol from the same dynamic object, make sure the real
	 symbol is also made into a dynamic symbol.  */
      if (h->weakdef != NULL
	  && h->weakdef->dynindx == -1)
	{
	  if (! _bfd_elf_link_record_dynamic_symbol (info, h->weakdef))
	    return false;
	}
    }

  return true;
}

/* This structure is used to pass information to
   elf_link_assign_sym_version.  */

struct elf_assign_sym_version_info
{
  /* Output BFD.  */
  bfd *output_bfd;
  /* General link information.  */
  struct bfd_link_info *info;
  /* Version tree.  */
  struct bfd_elf_version_tree *verdefs;
  /* Whether we had a failure.  */
  boolean failed;
};

/* This structure is used to pass information to
   elf_link_find_version_dependencies.  */

struct elf_find_verdep_info
{
  /* Output BFD.  */
  bfd *output_bfd;
  /* General link information.  */
  struct bfd_link_info *info;
  /* The number of dependencies.  */
  unsigned int vers;
  /* Whether we had a failure.  */
  boolean failed;
};

/* Array used to determine the number of hash table buckets to use
   based on the number of symbols there are.  If there are fewer than
   3 symbols we use 1 bucket, fewer than 17 symbols we use 3 buckets,
   fewer than 37 we use 17 buckets, and so forth.  We never use more
   than 32771 buckets.  */

static const size_t elf_buckets[] =
{
  1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209,
  16411, 32771, 0
};

/* Compute bucket count for hashing table.  We do not use a static set
   of possible tables sizes anymore.  Instead we determine for all
   possible reasonable sizes of the table the outcome (i.e., the
   number of collisions etc) and choose the best solution.  The
   weighting functions are not too simple to allow the table to grow
   without bounds.  Instead one of the weighting factors is the size.
   Therefore the result is always a good payoff between few collisions
   (= short chain lengths) and table size.  */
static size_t
compute_bucket_count (info)
     struct bfd_link_info *info;
{
  size_t dynsymcount = elf_hash_table (info)->dynsymcount;
  size_t best_size = 0;
  unsigned long int *hashcodes;
  unsigned long int *hashcodesp;
  unsigned long int i;
  bfd_size_type amt;

  /* Compute the hash values for all exported symbols.  At the same
     time store the values in an array so that we could use them for
     optimizations.  */
  amt = dynsymcount;
  amt *= sizeof (unsigned long int);
  hashcodes = (unsigned long int *) bfd_malloc (amt);
  if (hashcodes == NULL)
    return 0;
  hashcodesp = hashcodes;

  /* Put all hash values in HASHCODES.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_collect_hash_codes, &hashcodesp);

/* We have a problem here.  The following code to optimize the table
   size requires an integer type with more the 32 bits.  If
   BFD_HOST_U_64_BIT is set we know about such a type.  */
#ifdef BFD_HOST_U_64_BIT
  if (info->optimize == true)
    {
      unsigned long int nsyms = hashcodesp - hashcodes;
      size_t minsize;
      size_t maxsize;
      BFD_HOST_U_64_BIT best_chlen = ~((BFD_HOST_U_64_BIT) 0);
      unsigned long int *counts ;

      /* Possible optimization parameters: if we have NSYMS symbols we say
	 that the hashing table must at least have NSYMS/4 and at most
	 2*NSYMS buckets.  */
      minsize = nsyms / 4;
      if (minsize == 0)
	minsize = 1;
      best_size = maxsize = nsyms * 2;

      /* Create array where we count the collisions in.  We must use bfd_malloc
	 since the size could be large.  */
      amt = maxsize;
      amt *= sizeof (unsigned long int);
      counts = (unsigned long int *) bfd_malloc (amt);
      if (counts == NULL)
	{
	  free (hashcodes);
	  return 0;
	}

      /* Compute the "optimal" size for the hash table.  The criteria is a
	 minimal chain length.  The minor criteria is (of course) the size
	 of the table.  */
      for (i = minsize; i < maxsize; ++i)
	{
	  /* Walk through the array of hashcodes and count the collisions.  */
	  BFD_HOST_U_64_BIT max;
	  unsigned long int j;
	  unsigned long int fact;

	  memset (counts, '\0', i * sizeof (unsigned long int));

	  /* Determine how often each hash bucket is used.  */
	  for (j = 0; j < nsyms; ++j)
	    ++counts[hashcodes[j] % i];

	  /* For the weight function we need some information about the
	     pagesize on the target.  This is information need not be 100%
	     accurate.  Since this information is not available (so far) we
	     define it here to a reasonable default value.  If it is crucial
	     to have a better value some day simply define this value.  */
# ifndef BFD_TARGET_PAGESIZE
#  define BFD_TARGET_PAGESIZE	(4096)
# endif

	  /* We in any case need 2 + NSYMS entries for the size values and
	     the chains.  */
	  max = (2 + nsyms) * (ARCH_SIZE / 8);

# if 1
	  /* Variant 1: optimize for short chains.  We add the squares
	     of all the chain lengths (which favous many small chain
	     over a few long chains).  */
	  for (j = 0; j < i; ++j)
	    max += counts[j] * counts[j];

	  /* This adds penalties for the overall size of the table.  */
	  fact = i / (BFD_TARGET_PAGESIZE / (ARCH_SIZE / 8)) + 1;
	  max *= fact * fact;
# else
	  /* Variant 2: Optimize a lot more for small table.  Here we
	     also add squares of the size but we also add penalties for
	     empty slots (the +1 term).  */
	  for (j = 0; j < i; ++j)
	    max += (1 + counts[j]) * (1 + counts[j]);

	  /* The overall size of the table is considered, but not as
	     strong as in variant 1, where it is squared.  */
	  fact = i / (BFD_TARGET_PAGESIZE / (ARCH_SIZE / 8)) + 1;
	  max *= fact;
# endif

	  /* Compare with current best results.  */
	  if (max < best_chlen)
	    {
	      best_chlen = max;
	      best_size = i;
	    }
	}

      free (counts);
    }
  else
#endif /* defined (BFD_HOST_U_64_BIT) */
    {
      /* This is the fallback solution if no 64bit type is available or if we
	 are not supposed to spend much time on optimizations.  We select the
	 bucket count using a fixed set of numbers.  */
      for (i = 0; elf_buckets[i] != 0; i++)
	{
	  best_size = elf_buckets[i];
	  if (dynsymcount < elf_buckets[i + 1])
	    break;
	}
    }

  /* Free the arrays we needed.  */
  free (hashcodes);

  return best_size;
}

/* Set up the sizes and contents of the ELF dynamic sections.  This is
   called by the ELF linker emulation before_allocation routine.  We
   must set the sizes of the sections before the linker sets the
   addresses of the various sections.  */

boolean
NAME(bfd_elf,size_dynamic_sections) (output_bfd, soname, rpath,
				     filter_shlib,
				     auxiliary_filters, info, sinterpptr,
				     verdefs)
     bfd *output_bfd;
     const char *soname;
     const char *rpath;
     const char *filter_shlib;
     const char * const *auxiliary_filters;
     struct bfd_link_info *info;
     asection **sinterpptr;
     struct bfd_elf_version_tree *verdefs;
{
  bfd_size_type soname_indx;
  bfd *dynobj;
  struct elf_backend_data *bed;
  struct elf_assign_sym_version_info asvinfo;

  *sinterpptr = NULL;

  soname_indx = (bfd_size_type) -1;

  if (info->hash->creator->flavour != bfd_target_elf_flavour)
    return true;

  if (! is_elf_hash_table (info))
    return false;

  /* Any syms created from now on start with -1 in
     got.refcount/offset and plt.refcount/offset.  */
  elf_hash_table (info)->init_refcount = -1;

  /* The backend may have to create some sections regardless of whether
     we're dynamic or not.  */
  bed = get_elf_backend_data (output_bfd);
  if (bed->elf_backend_always_size_sections
      && ! (*bed->elf_backend_always_size_sections) (output_bfd, info))
    return false;

  dynobj = elf_hash_table (info)->dynobj;

  /* If there were no dynamic objects in the link, there is nothing to
     do here.  */
  if (dynobj == NULL)
    return true;

  if (! _bfd_elf_maybe_strip_eh_frame_hdr (info))
    return false;

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      struct elf_info_failed eif;
      struct elf_link_hash_entry *h;
      asection *dynstr;

      *sinterpptr = bfd_get_section_by_name (dynobj, ".interp");
      BFD_ASSERT (*sinterpptr != NULL || info->shared);

      if (soname != NULL)
	{
	  soname_indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					     soname, true);
	  if (soname_indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_SONAME,
					  soname_indx))
	    return false;
	}

      if (info->symbolic)
	{
	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_SYMBOLIC,
				       (bfd_vma) 0))
	    return false;
	  info->flags |= DF_SYMBOLIC;
	}

      if (rpath != NULL)
	{
	  bfd_size_type indx;

	  indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr, rpath,
				      true);
	  if (info->new_dtags)
	    _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr, indx);
	  if (indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_RPATH, indx)
	      || (info->new_dtags
		  && ! elf_add_dynamic_entry (info, (bfd_vma) DT_RUNPATH,
					      indx)))
	    return false;
	}

      if (filter_shlib != NULL)
	{
	  bfd_size_type indx;

	  indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
				      filter_shlib, true);
	  if (indx == (bfd_size_type) -1
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_FILTER, indx))
	    return false;
	}

      if (auxiliary_filters != NULL)
	{
	  const char * const *p;

	  for (p = auxiliary_filters; *p != NULL; p++)
	    {
	      bfd_size_type indx;

	      indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					  *p, true);
	      if (indx == (bfd_size_type) -1
		  || ! elf_add_dynamic_entry (info, (bfd_vma) DT_AUXILIARY,
					      indx))
		return false;
	    }
	}

      eif.info = info;
      eif.verdefs = verdefs;
      eif.failed = false;

      /* If we are supposed to export all symbols into the dynamic symbol
	 table (this is not the normal case), then do so.  */
      if (info->export_dynamic)
	{
	  elf_link_hash_traverse (elf_hash_table (info), elf_export_symbol,
				  (PTR) &eif);
	  if (eif.failed)
	    return false;
	}

      /* Attach all the symbols to their version information.  */
      asvinfo.output_bfd = output_bfd;
      asvinfo.info = info;
      asvinfo.verdefs = verdefs;
      asvinfo.failed = false;

      elf_link_hash_traverse (elf_hash_table (info),
			      elf_link_assign_sym_version,
			      (PTR) &asvinfo);
      if (asvinfo.failed)
	return false;

      /* Find all symbols which were defined in a dynamic object and make
	 the backend pick a reasonable value for them.  */
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_adjust_dynamic_symbol,
			      (PTR) &eif);
      if (eif.failed)
	return false;

      /* Add some entries to the .dynamic section.  We fill in some of the
	 values later, in elf_bfd_final_link, but we must add the entries
	 now so that we know the final size of the .dynamic section.  */

      /* If there are initialization and/or finalization functions to
	 call then add the corresponding DT_INIT/DT_FINI entries.  */
      h = (info->init_function
	   ? elf_link_hash_lookup (elf_hash_table (info),
				   info->init_function, false,
				   false, false)
	   : NULL);
      if (h != NULL
	  && (h->elf_link_hash_flags & (ELF_LINK_HASH_REF_REGULAR
					| ELF_LINK_HASH_DEF_REGULAR)) != 0)
	{
	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_INIT, (bfd_vma) 0))
	    return false;
	}
      h = (info->fini_function
	   ? elf_link_hash_lookup (elf_hash_table (info),
				   info->fini_function, false,
				   false, false)
	   : NULL);
      if (h != NULL
	  && (h->elf_link_hash_flags & (ELF_LINK_HASH_REF_REGULAR
					| ELF_LINK_HASH_DEF_REGULAR)) != 0)
	{
	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_FINI, (bfd_vma) 0))
	    return false;
	}

      if (bfd_get_section_by_name (output_bfd, ".preinit_array") != NULL)
	{
	  /* DT_PREINIT_ARRAY is not allowed in shared library.  */
	  if (info->shared)
	    {
	      bfd *sub;
	      asection *o;

	      for (sub = info->input_bfds; sub != NULL;
		   sub = sub->link_next)
		for (o = sub->sections; o != NULL; o = o->next)
		  if (elf_section_data (o)->this_hdr.sh_type
		      == SHT_PREINIT_ARRAY)
		    {
		      (*_bfd_error_handler)
			(_("%s: .preinit_array section is not allowed in DSO"),
			  bfd_archive_filename (sub));
		      break;
		    }

	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  if (!elf_add_dynamic_entry (info, (bfd_vma) DT_PREINIT_ARRAY,
				      (bfd_vma) 0)
	      || !elf_add_dynamic_entry (info, (bfd_vma) DT_PREINIT_ARRAYSZ,
					 (bfd_vma) 0))
	    return false;
	}
      if (bfd_get_section_by_name (output_bfd, ".init_array") != NULL)
	{
	  if (!elf_add_dynamic_entry (info, (bfd_vma) DT_INIT_ARRAY,
				      (bfd_vma) 0)
	      || !elf_add_dynamic_entry (info, (bfd_vma) DT_INIT_ARRAYSZ,
					 (bfd_vma) 0))
	    return false;
	}
      if (bfd_get_section_by_name (output_bfd, ".fini_array") != NULL)
	{
	  if (!elf_add_dynamic_entry (info, (bfd_vma) DT_FINI_ARRAY,
				      (bfd_vma) 0)
	      || !elf_add_dynamic_entry (info, (bfd_vma) DT_FINI_ARRAYSZ,
					 (bfd_vma) 0))
	    return false;
	}

      dynstr = bfd_get_section_by_name (dynobj, ".dynstr");
      /* If .dynstr is excluded from the link, we don't want any of
	 these tags.  Strictly, we should be checking each section
	 individually;  This quick check covers for the case where
	 someone does a /DISCARD/ : { *(*) }.  */
      if (dynstr != NULL && dynstr->output_section != bfd_abs_section_ptr)
	{
	  bfd_size_type strsize;

	  strsize = _bfd_elf_strtab_size (elf_hash_table (info)->dynstr);
	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_HASH, (bfd_vma) 0)
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_STRTAB, (bfd_vma) 0)
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_SYMTAB, (bfd_vma) 0)
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_STRSZ, strsize)
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_SYMENT,
					  (bfd_vma) sizeof (Elf_External_Sym)))
	    return false;
	}
    }

  /* The backend must work out the sizes of all the other dynamic
     sections.  */
  if (bed->elf_backend_size_dynamic_sections
      && ! (*bed->elf_backend_size_dynamic_sections) (output_bfd, info))
    return false;

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      bfd_size_type dynsymcount;
      asection *s;
      size_t bucketcount = 0;
      size_t hash_entry_size;
      unsigned int dtagcount;

      /* Set up the version definition section.  */
      s = bfd_get_section_by_name (dynobj, ".gnu.version_d");
      BFD_ASSERT (s != NULL);

      /* We may have created additional version definitions if we are
	 just linking a regular application.  */
      verdefs = asvinfo.verdefs;

      /* Skip anonymous version tag.  */
      if (verdefs != NULL && verdefs->vernum == 0)
	verdefs = verdefs->next;

      if (verdefs == NULL)
	_bfd_strip_section_from_output (info, s);
      else
	{
	  unsigned int cdefs;
	  bfd_size_type size;
	  struct bfd_elf_version_tree *t;
	  bfd_byte *p;
	  Elf_Internal_Verdef def;
	  Elf_Internal_Verdaux defaux;

	  cdefs = 0;
	  size = 0;

	  /* Make space for the base version.  */
	  size += sizeof (Elf_External_Verdef);
	  size += sizeof (Elf_External_Verdaux);
	  ++cdefs;

	  for (t = verdefs; t != NULL; t = t->next)
	    {
	      struct bfd_elf_version_deps *n;

	      size += sizeof (Elf_External_Verdef);
	      size += sizeof (Elf_External_Verdaux);
	      ++cdefs;

	      for (n = t->deps; n != NULL; n = n->next)
		size += sizeof (Elf_External_Verdaux);
	    }

	  s->_raw_size = size;
	  s->contents = (bfd_byte *) bfd_alloc (output_bfd, s->_raw_size);
	  if (s->contents == NULL && s->_raw_size != 0)
	    return false;

	  /* Fill in the version definition section.  */

	  p = s->contents;

	  def.vd_version = VER_DEF_CURRENT;
	  def.vd_flags = VER_FLG_BASE;
	  def.vd_ndx = 1;
	  def.vd_cnt = 1;
	  def.vd_aux = sizeof (Elf_External_Verdef);
	  def.vd_next = (sizeof (Elf_External_Verdef)
			 + sizeof (Elf_External_Verdaux));

	  if (soname_indx != (bfd_size_type) -1)
	    {
	      _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr,
				      soname_indx);
	      def.vd_hash = bfd_elf_hash (soname);
	      defaux.vda_name = soname_indx;
	    }
	  else
	    {
	      const char *name;
	      bfd_size_type indx;

	      name = basename (output_bfd->filename);
	      def.vd_hash = bfd_elf_hash (name);
	      indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					  name, false);
	      if (indx == (bfd_size_type) -1)
		return false;
	      defaux.vda_name = indx;
	    }
	  defaux.vda_next = 0;

	  _bfd_elf_swap_verdef_out (output_bfd, &def,
				    (Elf_External_Verdef *) p);
	  p += sizeof (Elf_External_Verdef);
	  _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
				     (Elf_External_Verdaux *) p);
	  p += sizeof (Elf_External_Verdaux);

	  for (t = verdefs; t != NULL; t = t->next)
	    {
	      unsigned int cdeps;
	      struct bfd_elf_version_deps *n;
	      struct elf_link_hash_entry *h;

	      cdeps = 0;
	      for (n = t->deps; n != NULL; n = n->next)
		++cdeps;

	      /* Add a symbol representing this version.  */
	      h = NULL;
	      if (! (_bfd_generic_link_add_one_symbol
		     (info, dynobj, t->name, BSF_GLOBAL, bfd_abs_section_ptr,
		      (bfd_vma) 0, (const char *) NULL, false,
		      get_elf_backend_data (dynobj)->collect,
		      (struct bfd_link_hash_entry **) &h)))
		return false;
	      h->elf_link_hash_flags &= ~ ELF_LINK_NON_ELF;
	      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	      h->type = STT_OBJECT;
	      h->verinfo.vertree = t;

	      if (! _bfd_elf_link_record_dynamic_symbol (info, h))
		return false;

	      def.vd_version = VER_DEF_CURRENT;
	      def.vd_flags = 0;
	      if (t->globals == NULL && t->locals == NULL && ! t->used)
		def.vd_flags |= VER_FLG_WEAK;
	      def.vd_ndx = t->vernum + 1;
	      def.vd_cnt = cdeps + 1;
	      def.vd_hash = bfd_elf_hash (t->name);
	      def.vd_aux = sizeof (Elf_External_Verdef);
	      if (t->next != NULL)
		def.vd_next = (sizeof (Elf_External_Verdef)
			       + (cdeps + 1) * sizeof (Elf_External_Verdaux));
	      else
		def.vd_next = 0;

	      _bfd_elf_swap_verdef_out (output_bfd, &def,
					(Elf_External_Verdef *) p);
	      p += sizeof (Elf_External_Verdef);

	      defaux.vda_name = h->dynstr_index;
	      _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr,
				      h->dynstr_index);
	      if (t->deps == NULL)
		defaux.vda_next = 0;
	      else
		defaux.vda_next = sizeof (Elf_External_Verdaux);
	      t->name_indx = defaux.vda_name;

	      _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
					 (Elf_External_Verdaux *) p);
	      p += sizeof (Elf_External_Verdaux);

	      for (n = t->deps; n != NULL; n = n->next)
		{
		  if (n->version_needed == NULL)
		    {
		      /* This can happen if there was an error in the
			 version script.  */
		      defaux.vda_name = 0;
		    }
		  else
		    {
		      defaux.vda_name = n->version_needed->name_indx;
		      _bfd_elf_strtab_addref (elf_hash_table (info)->dynstr,
					      defaux.vda_name);
		    }
		  if (n->next == NULL)
		    defaux.vda_next = 0;
		  else
		    defaux.vda_next = sizeof (Elf_External_Verdaux);

		  _bfd_elf_swap_verdaux_out (output_bfd, &defaux,
					     (Elf_External_Verdaux *) p);
		  p += sizeof (Elf_External_Verdaux);
		}
	    }

	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_VERDEF, (bfd_vma) 0)
	      || ! elf_add_dynamic_entry (info, (bfd_vma) DT_VERDEFNUM,
					  (bfd_vma) cdefs))
	    return false;

	  elf_tdata (output_bfd)->cverdefs = cdefs;
	}

      if (info->new_dtags && info->flags)
	{
	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_FLAGS, info->flags))
	    return false;
	}

      if (info->flags_1)
	{
	  if (! info->shared)
	    info->flags_1 &= ~ (DF_1_INITFIRST
				| DF_1_NODELETE
				| DF_1_NOOPEN);
	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_FLAGS_1,
				       info->flags_1))
	    return false;
	}

      /* Work out the size of the version reference section.  */

      s = bfd_get_section_by_name (dynobj, ".gnu.version_r");
      BFD_ASSERT (s != NULL);
      {
	struct elf_find_verdep_info sinfo;

	sinfo.output_bfd = output_bfd;
	sinfo.info = info;
	sinfo.vers = elf_tdata (output_bfd)->cverdefs;
	if (sinfo.vers == 0)
	  sinfo.vers = 1;
	sinfo.failed = false;

	elf_link_hash_traverse (elf_hash_table (info),
				elf_link_find_version_dependencies,
				(PTR) &sinfo);

	if (elf_tdata (output_bfd)->verref == NULL)
	  _bfd_strip_section_from_output (info, s);
	else
	  {
	    Elf_Internal_Verneed *t;
	    unsigned int size;
	    unsigned int crefs;
	    bfd_byte *p;

	    /* Build the version definition section.  */
	    size = 0;
	    crefs = 0;
	    for (t = elf_tdata (output_bfd)->verref;
		 t != NULL;
		 t = t->vn_nextref)
	      {
		Elf_Internal_Vernaux *a;

		size += sizeof (Elf_External_Verneed);
		++crefs;
		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  size += sizeof (Elf_External_Vernaux);
	      }

	    s->_raw_size = size;
	    s->contents = (bfd_byte *) bfd_alloc (output_bfd, s->_raw_size);
	    if (s->contents == NULL)
	      return false;

	    p = s->contents;
	    for (t = elf_tdata (output_bfd)->verref;
		 t != NULL;
		 t = t->vn_nextref)
	      {
		unsigned int caux;
		Elf_Internal_Vernaux *a;
		bfd_size_type indx;

		caux = 0;
		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  ++caux;

		t->vn_version = VER_NEED_CURRENT;
		t->vn_cnt = caux;
		indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
					    elf_dt_name (t->vn_bfd) != NULL
					    ? elf_dt_name (t->vn_bfd)
					    : basename (t->vn_bfd->filename),
					    false);
		if (indx == (bfd_size_type) -1)
		  return false;
		t->vn_file = indx;
		t->vn_aux = sizeof (Elf_External_Verneed);
		if (t->vn_nextref == NULL)
		  t->vn_next = 0;
		else
		  t->vn_next = (sizeof (Elf_External_Verneed)
				+ caux * sizeof (Elf_External_Vernaux));

		_bfd_elf_swap_verneed_out (output_bfd, t,
					   (Elf_External_Verneed *) p);
		p += sizeof (Elf_External_Verneed);

		for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
		  {
		    a->vna_hash = bfd_elf_hash (a->vna_nodename);
		    indx = _bfd_elf_strtab_add (elf_hash_table (info)->dynstr,
						a->vna_nodename, false);
		    if (indx == (bfd_size_type) -1)
		      return false;
		    a->vna_name = indx;
		    if (a->vna_nextptr == NULL)
		      a->vna_next = 0;
		    else
		      a->vna_next = sizeof (Elf_External_Vernaux);

		    _bfd_elf_swap_vernaux_out (output_bfd, a,
					       (Elf_External_Vernaux *) p);
		    p += sizeof (Elf_External_Vernaux);
		  }
	      }

	    if (! elf_add_dynamic_entry (info, (bfd_vma) DT_VERNEED,
					 (bfd_vma) 0)
		|| ! elf_add_dynamic_entry (info, (bfd_vma) DT_VERNEEDNUM,
					    (bfd_vma) crefs))
	      return false;

	    elf_tdata (output_bfd)->cverrefs = crefs;
	  }
      }

      /* Assign dynsym indicies.  In a shared library we generate a
	 section symbol for each output section, which come first.
	 Next come all of the back-end allocated local dynamic syms,
	 followed by the rest of the global symbols.  */

      dynsymcount = _bfd_elf_link_renumber_dynsyms (output_bfd, info);

      /* Work out the size of the symbol version section.  */
      s = bfd_get_section_by_name (dynobj, ".gnu.version");
      BFD_ASSERT (s != NULL);
      if (dynsymcount == 0
	  || (verdefs == NULL && elf_tdata (output_bfd)->verref == NULL))
	{
	  _bfd_strip_section_from_output (info, s);
	  /* The DYNSYMCOUNT might have changed if we were going to
	     output a dynamic symbol table entry for S.  */
	  dynsymcount = _bfd_elf_link_renumber_dynsyms (output_bfd, info);
	}
      else
	{
	  s->_raw_size = dynsymcount * sizeof (Elf_External_Versym);
	  s->contents = (bfd_byte *) bfd_zalloc (output_bfd, s->_raw_size);
	  if (s->contents == NULL)
	    return false;

	  if (! elf_add_dynamic_entry (info, (bfd_vma) DT_VERSYM, (bfd_vma) 0))
	    return false;
	}

      /* Set the size of the .dynsym and .hash sections.  We counted
	 the number of dynamic symbols in elf_link_add_object_symbols.
	 We will build the contents of .dynsym and .hash when we build
	 the final symbol table, because until then we do not know the
	 correct value to give the symbols.  We built the .dynstr
	 section as we went along in elf_link_add_object_symbols.  */
      s = bfd_get_section_by_name (dynobj, ".dynsym");
      BFD_ASSERT (s != NULL);
      s->_raw_size = dynsymcount * sizeof (Elf_External_Sym);
      s->contents = (bfd_byte *) bfd_alloc (output_bfd, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;

      if (dynsymcount != 0)
	{
	  Elf_Internal_Sym isym;

	  /* The first entry in .dynsym is a dummy symbol.  */
	  isym.st_value = 0;
	  isym.st_size = 0;
	  isym.st_name = 0;
	  isym.st_info = 0;
	  isym.st_other = 0;
	  isym.st_shndx = 0;
	  elf_swap_symbol_out (output_bfd, &isym, (PTR) s->contents, (PTR) 0);
	}

      /* Compute the size of the hashing table.  As a side effect this
	 computes the hash values for all the names we export.  */
      bucketcount = compute_bucket_count (info);

      s = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (s != NULL);
      hash_entry_size = elf_section_data (s)->this_hdr.sh_entsize;
      s->_raw_size = ((2 + bucketcount + dynsymcount) * hash_entry_size);
      s->contents = (bfd_byte *) bfd_alloc (output_bfd, s->_raw_size);
      if (s->contents == NULL)
	return false;
      memset (s->contents, 0, (size_t) s->_raw_size);

      bfd_put (8 * hash_entry_size, output_bfd, (bfd_vma) bucketcount,
	       s->contents);
      bfd_put (8 * hash_entry_size, output_bfd, (bfd_vma) dynsymcount,
	       s->contents + hash_entry_size);

      elf_hash_table (info)->bucketcount = bucketcount;

      s = bfd_get_section_by_name (dynobj, ".dynstr");
      BFD_ASSERT (s != NULL);

      elf_finalize_dynstr (output_bfd, info);

      s->_raw_size = _bfd_elf_strtab_size (elf_hash_table (info)->dynstr);

      for (dtagcount = 0; dtagcount <= info->spare_dynamic_tags; ++dtagcount)
	if (! elf_add_dynamic_entry (info, (bfd_vma) DT_NULL, (bfd_vma) 0))
	  return false;
    }

  return true;
}

/* This function is used to adjust offsets into .dynstr for
   dynamic symbols.  This is called via elf_link_hash_traverse.  */

static boolean elf_adjust_dynstr_offsets
PARAMS ((struct elf_link_hash_entry *, PTR));

static boolean
elf_adjust_dynstr_offsets (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_strtab_hash *dynstr = (struct elf_strtab_hash *) data;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx != -1)
    h->dynstr_index = _bfd_elf_strtab_offset (dynstr, h->dynstr_index);
  return true;
}

/* Assign string offsets in .dynstr, update all structures referencing
   them.  */

static boolean
elf_finalize_dynstr (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  struct elf_link_local_dynamic_entry *entry;
  struct elf_strtab_hash *dynstr = elf_hash_table (info)->dynstr;
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *sdyn;
  bfd_size_type size;
  Elf_External_Dyn *dyncon, *dynconend;

  _bfd_elf_strtab_finalize (dynstr);
  size = _bfd_elf_strtab_size (dynstr);

  /* Update all .dynamic entries referencing .dynstr strings.  */
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (sdyn != NULL);

  dyncon = (Elf_External_Dyn *) sdyn->contents;
  dynconend = (Elf_External_Dyn *) (sdyn->contents +
				    sdyn->_raw_size);
  for (; dyncon < dynconend; dyncon++)
    {
      Elf_Internal_Dyn dyn;

      elf_swap_dyn_in (dynobj, dyncon, & dyn);
      switch (dyn.d_tag)
	{
	case DT_STRSZ:
	  dyn.d_un.d_val = size;
	  elf_swap_dyn_out (dynobj, & dyn, dyncon);
	  break;
	case DT_NEEDED:
	case DT_SONAME:
	case DT_RPATH:
	case DT_RUNPATH:
	case DT_FILTER:
	case DT_AUXILIARY:
	  dyn.d_un.d_val = _bfd_elf_strtab_offset (dynstr, dyn.d_un.d_val);
	  elf_swap_dyn_out (dynobj, & dyn, dyncon);
	  break;
	default:
	  break;
	}
    }

  /* Now update local dynamic symbols.  */
  for (entry = elf_hash_table (info)->dynlocal; entry ; entry = entry->next)
    entry->isym.st_name = _bfd_elf_strtab_offset (dynstr,
						  entry->isym.st_name);

  /* And the rest of dynamic symbols.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_adjust_dynstr_offsets, dynstr);

  /* Adjust version definitions.  */
  if (elf_tdata (output_bfd)->cverdefs)
    {
      asection *s;
      bfd_byte *p;
      bfd_size_type i;
      Elf_Internal_Verdef def;
      Elf_Internal_Verdaux defaux;

      s = bfd_get_section_by_name (dynobj, ".gnu.version_d");
      p = (bfd_byte *) s->contents;
      do
	{
	  _bfd_elf_swap_verdef_in (output_bfd, (Elf_External_Verdef *) p,
				   &def);
	  p += sizeof (Elf_External_Verdef);
	  for (i = 0; i < def.vd_cnt; ++i)
	    {
	      _bfd_elf_swap_verdaux_in (output_bfd,
					(Elf_External_Verdaux *) p, &defaux);
	      defaux.vda_name = _bfd_elf_strtab_offset (dynstr,
							defaux.vda_name);
	      _bfd_elf_swap_verdaux_out (output_bfd,
					 &defaux, (Elf_External_Verdaux *) p);
	      p += sizeof (Elf_External_Verdaux);
	    }
	}
      while (def.vd_next);
    }

  /* Adjust version references.  */
  if (elf_tdata (output_bfd)->verref)
    {
      asection *s;
      bfd_byte *p;
      bfd_size_type i;
      Elf_Internal_Verneed need;
      Elf_Internal_Vernaux needaux;

      s = bfd_get_section_by_name (dynobj, ".gnu.version_r");
      p = (bfd_byte *) s->contents;
      do
	{
	  _bfd_elf_swap_verneed_in (output_bfd, (Elf_External_Verneed *) p,
				    &need);
	  need.vn_file = _bfd_elf_strtab_offset (dynstr, need.vn_file);
	  _bfd_elf_swap_verneed_out (output_bfd, &need,
				     (Elf_External_Verneed *) p);
	  p += sizeof (Elf_External_Verneed);
	  for (i = 0; i < need.vn_cnt; ++i)
	    {
	      _bfd_elf_swap_vernaux_in (output_bfd,
					(Elf_External_Vernaux *) p, &needaux);
	      needaux.vna_name = _bfd_elf_strtab_offset (dynstr,
							 needaux.vna_name);
	      _bfd_elf_swap_vernaux_out (output_bfd,
					 &needaux,
					 (Elf_External_Vernaux *) p);
	      p += sizeof (Elf_External_Vernaux);
	    }
	}
      while (need.vn_next);
    }

  return true;
}

/* Fix up the flags for a symbol.  This handles various cases which
   can only be fixed after all the input files are seen.  This is
   currently called by both adjust_dynamic_symbol and
   assign_sym_version, which is unnecessary but perhaps more robust in
   the face of future changes.  */

static boolean
elf_fix_symbol_flags (h, eif)
     struct elf_link_hash_entry *h;
     struct elf_info_failed *eif;
{
  /* If this symbol was mentioned in a non-ELF file, try to set
     DEF_REGULAR and REF_REGULAR correctly.  This is the only way to
     permit a non-ELF file to correctly refer to a symbol defined in
     an ELF dynamic object.  */
  if ((h->elf_link_hash_flags & ELF_LINK_NON_ELF) != 0)
    {
      while (h->root.type == bfd_link_hash_indirect)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;

      if (h->root.type != bfd_link_hash_defined
	  && h->root.type != bfd_link_hash_defweak)
	h->elf_link_hash_flags |= (ELF_LINK_HASH_REF_REGULAR
				   | ELF_LINK_HASH_REF_REGULAR_NONWEAK);
      else
	{
	  if (h->root.u.def.section->owner != NULL
	      && (bfd_get_flavour (h->root.u.def.section->owner)
		  == bfd_target_elf_flavour))
	    h->elf_link_hash_flags |= (ELF_LINK_HASH_REF_REGULAR
				       | ELF_LINK_HASH_REF_REGULAR_NONWEAK);
	  else
	    h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	}

      if (h->dynindx == -1
	  && ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	      || (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0))
	{
	  if (! _bfd_elf_link_record_dynamic_symbol (eif->info, h))
	    {
	      eif->failed = true;
	      return false;
	    }
	}
    }
  else
    {
      /* Unfortunately, ELF_LINK_NON_ELF is only correct if the symbol
	 was first seen in a non-ELF file.  Fortunately, if the symbol
	 was first seen in an ELF file, we're probably OK unless the
	 symbol was defined in a non-ELF file.  Catch that case here.
	 FIXME: We're still in trouble if the symbol was first seen in
	 a dynamic object, and then later in a non-ELF regular object.  */
      if ((h->root.type == bfd_link_hash_defined
	   || h->root.type == bfd_link_hash_defweak)
	  && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	  && (h->root.u.def.section->owner != NULL
	      ? (bfd_get_flavour (h->root.u.def.section->owner)
		 != bfd_target_elf_flavour)
	      : (bfd_is_abs_section (h->root.u.def.section)
		 && (h->elf_link_hash_flags
		     & ELF_LINK_HASH_DEF_DYNAMIC) == 0)))
	h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
    }

  /* If this is a final link, and the symbol was defined as a common
     symbol in a regular object file, and there was no definition in
     any dynamic object, then the linker will have allocated space for
     the symbol in a common section but the ELF_LINK_HASH_DEF_REGULAR
     flag will not have been set.  */
  if (h->root.type == bfd_link_hash_defined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
      && (h->root.u.def.section->owner->flags & DYNAMIC) == 0)
    h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;

  /* If -Bsymbolic was used (which means to bind references to global
     symbols to the definition within the shared object), and this
     symbol was defined in a regular object, then it actually doesn't
     need a PLT entry, and we can accomplish that by forcing it local.
     Likewise, if the symbol has hidden or internal visibility.
     FIXME: It might be that we also do not need a PLT for other
     non-hidden visibilities, but we would have to tell that to the
     backend specifically; we can't just clear PLT-related data here.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0
      && eif->info->shared
      && is_elf_hash_table (eif->info)
      && (eif->info->symbolic
	  || ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
	  || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN)
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)
    {
      struct elf_backend_data *bed;
      boolean force_local;

      bed = get_elf_backend_data (elf_hash_table (eif->info)->dynobj);

      force_local = (ELF_ST_VISIBILITY (h->other) == STV_INTERNAL
		     || ELF_ST_VISIBILITY (h->other) == STV_HIDDEN);
      (*bed->elf_backend_hide_symbol) (eif->info, h, force_local);
    }

  /* If this is a weak defined symbol in a dynamic object, and we know
     the real definition in the dynamic object, copy interesting flags
     over to the real definition.  */
  if (h->weakdef != NULL)
    {
      struct elf_link_hash_entry *weakdef;

      BFD_ASSERT (h->root.type == bfd_link_hash_defined
		  || h->root.type == bfd_link_hash_defweak);
      weakdef = h->weakdef;
      BFD_ASSERT (weakdef->root.type == bfd_link_hash_defined
		  || weakdef->root.type == bfd_link_hash_defweak);
      BFD_ASSERT (weakdef->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC);

      /* If the real definition is defined by a regular object file,
	 don't do anything special.  See the longer description in
	 elf_adjust_dynamic_symbol, below.  */
      if ((weakdef->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0)
	h->weakdef = NULL;
      else
	{
	  struct elf_backend_data *bed;

	  bed = get_elf_backend_data (elf_hash_table (eif->info)->dynobj);
	  (*bed->elf_backend_copy_indirect_symbol) (weakdef, h);
	}
    }

  return true;
}

/* Make the backend pick a good value for a dynamic symbol.  This is
   called via elf_link_hash_traverse, and also calls itself
   recursively.  */

static boolean
elf_adjust_dynamic_symbol (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_info_failed *eif = (struct elf_info_failed *) data;
  bfd *dynobj;
  struct elf_backend_data *bed;

  if (h->root.type == bfd_link_hash_warning)
    {
      h->plt.offset = (bfd_vma) -1;
      h->got.offset = (bfd_vma) -1;

      /* When warning symbols are created, they **replace** the "real"
	 entry in the hash table, thus we never get to see the real
	 symbol in a hash traversal.  So look at it now.  */
      h = (struct elf_link_hash_entry *) h->root.u.i.link;
    }

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->root.type == bfd_link_hash_indirect)
    return true;

  if (! is_elf_hash_table (eif->info))
    return false;

  /* Fix the symbol flags.  */
  if (! elf_fix_symbol_flags (h, eif))
    return false;

  /* If this symbol does not require a PLT entry, and it is not
     defined by a dynamic object, or is not referenced by a regular
     object, ignore it.  We do have to handle a weak defined symbol,
     even if no regular object refers to it, if we decided to add it
     to the dynamic symbol table.  FIXME: Do we normally need to worry
     about symbols which are defined by one dynamic object and
     referenced by another one?  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) == 0
      && ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
	  || (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
	  || ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0
	      && (h->weakdef == NULL || h->weakdef->dynindx == -1))))
    {
      h->plt.offset = (bfd_vma) -1;
      return true;
    }

  /* If we've already adjusted this symbol, don't do it again.  This
     can happen via a recursive call.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DYNAMIC_ADJUSTED) != 0)
    return true;

  /* Don't look at this symbol again.  Note that we must set this
     after checking the above conditions, because we may look at a
     symbol once, decide not to do anything, and then get called
     recursively later after REF_REGULAR is set below.  */
  h->elf_link_hash_flags |= ELF_LINK_HASH_DYNAMIC_ADJUSTED;

  /* If this is a weak definition, and we know a real definition, and
     the real symbol is not itself defined by a regular object file,
     then get a good value for the real definition.  We handle the
     real symbol first, for the convenience of the backend routine.

     Note that there is a confusing case here.  If the real definition
     is defined by a regular object file, we don't get the real symbol
     from the dynamic object, but we do get the weak symbol.  If the
     processor backend uses a COPY reloc, then if some routine in the
     dynamic object changes the real symbol, we will not see that
     change in the corresponding weak symbol.  This is the way other
     ELF linkers work as well, and seems to be a result of the shared
     library model.

     I will clarify this issue.  Most SVR4 shared libraries define the
     variable _timezone and define timezone as a weak synonym.  The
     tzset call changes _timezone.  If you write
       extern int timezone;
       int _timezone = 5;
       int main () { tzset (); printf ("%d %d\n", timezone, _timezone); }
     you might expect that, since timezone is a synonym for _timezone,
     the same number will print both times.  However, if the processor
     backend uses a COPY reloc, then actually timezone will be copied
     into your process image, and, since you define _timezone
     yourself, _timezone will not.  Thus timezone and _timezone will
     wind up at different memory locations.  The tzset call will set
     _timezone, leaving timezone unchanged.  */

  if (h->weakdef != NULL)
    {
      /* If we get to this point, we know there is an implicit
	 reference by a regular object file via the weak symbol H.
	 FIXME: Is this really true?  What if the traversal finds
	 H->WEAKDEF before it finds H?  */
      h->weakdef->elf_link_hash_flags |= ELF_LINK_HASH_REF_REGULAR;

      if (! elf_adjust_dynamic_symbol (h->weakdef, (PTR) eif))
	return false;
    }

  /* If a symbol has no type and no size and does not require a PLT
     entry, then we are probably about to do the wrong thing here: we
     are probably going to create a COPY reloc for an empty object.
     This case can arise when a shared object is built with assembly
     code, and the assembly code fails to set the symbol type.  */
  if (h->size == 0
      && h->type == STT_NOTYPE
      && (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) == 0)
    (*_bfd_error_handler)
      (_("warning: type and size of dynamic symbol `%s' are not defined"),
	 h->root.root.string);

  dynobj = elf_hash_table (eif->info)->dynobj;
  bed = get_elf_backend_data (dynobj);
  if (! (*bed->elf_backend_adjust_dynamic_symbol) (eif->info, h))
    {
      eif->failed = true;
      return false;
    }

  return true;
}

/* This routine is used to export all defined symbols into the dynamic
   symbol table.  It is called via elf_link_hash_traverse.  */

static boolean
elf_export_symbol (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_info_failed *eif = (struct elf_info_failed *) data;

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->root.type == bfd_link_hash_indirect)
    return true;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx == -1
      && (h->elf_link_hash_flags
	  & (ELF_LINK_HASH_DEF_REGULAR | ELF_LINK_HASH_REF_REGULAR)) != 0)
    {
      struct bfd_elf_version_tree *t;
      struct bfd_elf_version_expr *d;

      for (t = eif->verdefs; t != NULL; t = t->next)
	{
	  if (t->globals != NULL)
	    {
	      for (d = t->globals; d != NULL; d = d->next)
		{
		  if ((*d->match) (d, h->root.root.string))
		    goto doit;
		}
	    }

	  if (t->locals != NULL)
	    {
	      for (d = t->locals ; d != NULL; d = d->next)
		{
		  if ((*d->match) (d, h->root.root.string))
		    return true;
		}
	    }
	}

      if (!eif->verdefs)
	{
doit:
	  if (! _bfd_elf_link_record_dynamic_symbol (eif->info, h))
	    {
	      eif->failed = true;
	      return false;
	    }
	}
    }

  return true;
}

/* Look through the symbols which are defined in other shared
   libraries and referenced here.  Update the list of version
   dependencies.  This will be put into the .gnu.version_r section.
   This function is called via elf_link_hash_traverse.  */

static boolean
elf_link_find_version_dependencies (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_find_verdep_info *rinfo = (struct elf_find_verdep_info *) data;
  Elf_Internal_Verneed *t;
  Elf_Internal_Vernaux *a;
  bfd_size_type amt;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* We only care about symbols defined in shared objects with version
     information.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) == 0
      || (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) != 0
      || h->dynindx == -1
      || h->verinfo.verdef == NULL)
    return true;

  /* See if we already know about this version.  */
  for (t = elf_tdata (rinfo->output_bfd)->verref; t != NULL; t = t->vn_nextref)
    {
      if (t->vn_bfd != h->verinfo.verdef->vd_bfd)
	continue;

      for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	if (a->vna_nodename == h->verinfo.verdef->vd_nodename)
	  return true;

      break;
    }

  /* This is a new version.  Add it to tree we are building.  */

  if (t == NULL)
    {
      amt = sizeof *t;
      t = (Elf_Internal_Verneed *) bfd_zalloc (rinfo->output_bfd, amt);
      if (t == NULL)
	{
	  rinfo->failed = true;
	  return false;
	}

      t->vn_bfd = h->verinfo.verdef->vd_bfd;
      t->vn_nextref = elf_tdata (rinfo->output_bfd)->verref;
      elf_tdata (rinfo->output_bfd)->verref = t;
    }

  amt = sizeof *a;
  a = (Elf_Internal_Vernaux *) bfd_zalloc (rinfo->output_bfd, amt);

  /* Note that we are copying a string pointer here, and testing it
     above.  If bfd_elf_string_from_elf_section is ever changed to
     discard the string data when low in memory, this will have to be
     fixed.  */
  a->vna_nodename = h->verinfo.verdef->vd_nodename;

  a->vna_flags = h->verinfo.verdef->vd_flags;
  a->vna_nextptr = t->vn_auxptr;

  h->verinfo.verdef->vd_exp_refno = rinfo->vers;
  ++rinfo->vers;

  a->vna_other = h->verinfo.verdef->vd_exp_refno + 1;

  t->vn_auxptr = a;

  return true;
}

/* Figure out appropriate versions for all the symbols.  We may not
   have the version number script until we have read all of the input
   files, so until that point we don't know which symbols should be
   local.  This function is called via elf_link_hash_traverse.  */

static boolean
elf_link_assign_sym_version (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_assign_sym_version_info *sinfo;
  struct bfd_link_info *info;
  struct elf_backend_data *bed;
  struct elf_info_failed eif;
  char *p;
  bfd_size_type amt;

  sinfo = (struct elf_assign_sym_version_info *) data;
  info = sinfo->info;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Fix the symbol flags.  */
  eif.failed = false;
  eif.info = info;
  if (! elf_fix_symbol_flags (h, &eif))
    {
      if (eif.failed)
	sinfo->failed = true;
      return false;
    }

  /* We only need version numbers for symbols defined in regular
     objects.  */
  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    return true;

  bed = get_elf_backend_data (sinfo->output_bfd);
  p = strchr (h->root.root.string, ELF_VER_CHR);
  if (p != NULL && h->verinfo.vertree == NULL)
    {
      struct bfd_elf_version_tree *t;
      boolean hidden;

      hidden = true;

      /* There are two consecutive ELF_VER_CHR characters if this is
	 not a hidden symbol.  */
      ++p;
      if (*p == ELF_VER_CHR)
	{
	  hidden = false;
	  ++p;
	}

      /* If there is no version string, we can just return out.  */
      if (*p == '\0')
	{
	  if (hidden)
	    h->elf_link_hash_flags |= ELF_LINK_HIDDEN;
	  return true;
	}

      /* Look for the version.  If we find it, it is no longer weak.  */
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (strcmp (t->name, p) == 0)
	    {
	      size_t len;
	      char *alc;
	      struct bfd_elf_version_expr *d;

	      len = p - h->root.root.string;
	      alc = bfd_malloc ((bfd_size_type) len);
	      if (alc == NULL)
		return false;
	      strncpy (alc, h->root.root.string, len - 1);
	      alc[len - 1] = '\0';
	      if (alc[len - 2] == ELF_VER_CHR)
		alc[len - 2] = '\0';

	      h->verinfo.vertree = t;
	      t->used = true;
	      d = NULL;

	      if (t->globals != NULL)
		{
		  for (d = t->globals; d != NULL; d = d->next)
		    if ((*d->match) (d, alc))
		      break;
		}

	      /* See if there is anything to force this symbol to
		 local scope.  */
	      if (d == NULL && t->locals != NULL)
		{
		  for (d = t->locals; d != NULL; d = d->next)
		    {
		      if ((*d->match) (d, alc))
			{
			  if (h->dynindx != -1
			      && info->shared
			      && ! info->export_dynamic)
			    {
			      (*bed->elf_backend_hide_symbol) (info, h, true);
			    }

			  break;
			}
		    }
		}

	      free (alc);
	      break;
	    }
	}

      /* If we are building an application, we need to create a
	 version node for this version.  */
      if (t == NULL && ! info->shared)
	{
	  struct bfd_elf_version_tree **pp;
	  int version_index;

	  /* If we aren't going to export this symbol, we don't need
	     to worry about it.  */
	  if (h->dynindx == -1)
	    return true;

	  amt = sizeof *t;
	  t = ((struct bfd_elf_version_tree *)
	       bfd_alloc (sinfo->output_bfd, amt));
	  if (t == NULL)
	    {
	      sinfo->failed = true;
	      return false;
	    }

	  t->next = NULL;
	  t->name = p;
	  t->globals = NULL;
	  t->locals = NULL;
	  t->deps = NULL;
	  t->name_indx = (unsigned int) -1;
	  t->used = true;

	  version_index = 1;
	  /* Don't count anonymous version tag.  */
	  if (sinfo->verdefs != NULL && sinfo->verdefs->vernum == 0)
	    version_index = 0;
	  for (pp = &sinfo->verdefs; *pp != NULL; pp = &(*pp)->next)
	    ++version_index;
	  t->vernum = version_index;

	  *pp = t;

	  h->verinfo.vertree = t;
	}
      else if (t == NULL)
	{
	  /* We could not find the version for a symbol when
	     generating a shared archive.  Return an error.  */
	  (*_bfd_error_handler)
	    (_("%s: undefined versioned symbol name %s"),
	     bfd_get_filename (sinfo->output_bfd), h->root.root.string);
	  bfd_set_error (bfd_error_bad_value);
	  sinfo->failed = true;
	  return false;
	}

      if (hidden)
	h->elf_link_hash_flags |= ELF_LINK_HIDDEN;
    }

  /* If we don't have a version for this symbol, see if we can find
     something.  */
  if (h->verinfo.vertree == NULL && sinfo->verdefs != NULL)
    {
      struct bfd_elf_version_tree *t;
      struct bfd_elf_version_tree *deflt;
      struct bfd_elf_version_expr *d;

      /* See if can find what version this symbol is in.  If the
	 symbol is supposed to be local, then don't actually register
	 it.  */
      deflt = NULL;
      for (t = sinfo->verdefs; t != NULL; t = t->next)
	{
	  if (t->globals != NULL)
	    {
	      for (d = t->globals; d != NULL; d = d->next)
		{
		  if ((*d->match) (d, h->root.root.string))
		    {
		      h->verinfo.vertree = t;
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }

	  if (t->locals != NULL)
	    {
	      for (d = t->locals; d != NULL; d = d->next)
		{
		  if (d->pattern[0] == '*' && d->pattern[1] == '\0')
		    deflt = t;
		  else if ((*d->match) (d, h->root.root.string))
		    {
		      h->verinfo.vertree = t;
		      if (h->dynindx != -1
			  && info->shared
			  && ! info->export_dynamic)
			{
			  (*bed->elf_backend_hide_symbol) (info, h, true);
			}
		      break;
		    }
		}

	      if (d != NULL)
		break;
	    }
	}

      if (deflt != NULL && h->verinfo.vertree == NULL)
	{
	  h->verinfo.vertree = deflt;
	  if (h->dynindx != -1
	      && info->shared
	      && ! info->export_dynamic)
	    {
	      (*bed->elf_backend_hide_symbol) (info, h, true);
	    }
	}
    }

  return true;
}

/* Final phase of ELF linker.  */

/* A structure we use to avoid passing large numbers of arguments.  */

struct elf_final_link_info
{
  /* General link information.  */
  struct bfd_link_info *info;
  /* Output BFD.  */
  bfd *output_bfd;
  /* Symbol string table.  */
  struct bfd_strtab_hash *symstrtab;
  /* .dynsym section.  */
  asection *dynsym_sec;
  /* .hash section.  */
  asection *hash_sec;
  /* symbol version section (.gnu.version).  */
  asection *symver_sec;
  /* Buffer large enough to hold contents of any section.  */
  bfd_byte *contents;
  /* Buffer large enough to hold external relocs of any section.  */
  PTR external_relocs;
  /* Buffer large enough to hold internal relocs of any section.  */
  Elf_Internal_Rela *internal_relocs;
  /* Buffer large enough to hold external local symbols of any input
     BFD.  */
  Elf_External_Sym *external_syms;
  /* And a buffer for symbol section indices.  */
  Elf_External_Sym_Shndx *locsym_shndx;
  /* Buffer large enough to hold internal local symbols of any input
     BFD.  */
  Elf_Internal_Sym *internal_syms;
  /* Array large enough to hold a symbol index for each local symbol
     of any input BFD.  */
  long *indices;
  /* Array large enough to hold a section pointer for each local
     symbol of any input BFD.  */
  asection **sections;
  /* Buffer to hold swapped out symbols.  */
  Elf_External_Sym *symbuf;
  /* And one for symbol section indices.  */
  Elf_External_Sym_Shndx *symshndxbuf;
  /* Number of swapped out symbols in buffer.  */
  size_t symbuf_count;
  /* Number of symbols which fit in symbuf.  */
  size_t symbuf_size;
};

static boolean elf_link_output_sym
  PARAMS ((struct elf_final_link_info *, const char *,
	   Elf_Internal_Sym *, asection *));
static boolean elf_link_flush_output_syms
  PARAMS ((struct elf_final_link_info *));
static boolean elf_link_output_extsym
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_sec_merge_syms
  PARAMS ((struct elf_link_hash_entry *, PTR));
static boolean elf_link_input_bfd
  PARAMS ((struct elf_final_link_info *, bfd *));
static boolean elf_reloc_link_order
  PARAMS ((bfd *, struct bfd_link_info *, asection *,
	   struct bfd_link_order *));

/* This struct is used to pass information to elf_link_output_extsym.  */

struct elf_outext_info
{
  boolean failed;
  boolean localsyms;
  struct elf_final_link_info *finfo;
};

/* Compute the size of, and allocate space for, REL_HDR which is the
   section header for a section containing relocations for O.  */

static boolean
elf_link_size_reloc_section (abfd, rel_hdr, o)
     bfd *abfd;
     Elf_Internal_Shdr *rel_hdr;
     asection *o;
{
  bfd_size_type reloc_count;
  bfd_size_type num_rel_hashes;

  /* Figure out how many relocations there will be.  */
  if (rel_hdr == &elf_section_data (o)->rel_hdr)
    reloc_count = elf_section_data (o)->rel_count;
  else
    reloc_count = elf_section_data (o)->rel_count2;

  num_rel_hashes = o->reloc_count;
  if (num_rel_hashes < reloc_count)
    num_rel_hashes = reloc_count;

  /* That allows us to calculate the size of the section.  */
  rel_hdr->sh_size = rel_hdr->sh_entsize * reloc_count;

  /* The contents field must last into write_object_contents, so we
     allocate it with bfd_alloc rather than malloc.  Also since we
     cannot be sure that the contents will actually be filled in,
     we zero the allocated space.  */
  rel_hdr->contents = (PTR) bfd_zalloc (abfd, rel_hdr->sh_size);
  if (rel_hdr->contents == NULL && rel_hdr->sh_size != 0)
    return false;

  /* We only allocate one set of hash entries, so we only do it the
     first time we are called.  */
  if (elf_section_data (o)->rel_hashes == NULL
      && num_rel_hashes)
    {
      struct elf_link_hash_entry **p;

      p = ((struct elf_link_hash_entry **)
	   bfd_zmalloc (num_rel_hashes
			* sizeof (struct elf_link_hash_entry *)));
      if (p == NULL)
	return false;

      elf_section_data (o)->rel_hashes = p;
    }

  return true;
}

/* When performing a relocateable link, the input relocations are
   preserved.  But, if they reference global symbols, the indices
   referenced must be updated.  Update all the relocations in
   REL_HDR (there are COUNT of them), using the data in REL_HASH.  */

static void
elf_link_adjust_relocs (abfd, rel_hdr, count, rel_hash)
     bfd *abfd;
     Elf_Internal_Shdr *rel_hdr;
     unsigned int count;
     struct elf_link_hash_entry **rel_hash;
{
  unsigned int i;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  Elf_Internal_Rel *irel;
  Elf_Internal_Rela *irela;
  bfd_size_type amt = sizeof (Elf_Internal_Rel) * bed->s->int_rels_per_ext_rel;

  irel = (Elf_Internal_Rel *) bfd_zmalloc (amt);
  if (irel == NULL)
    {
      (*_bfd_error_handler) (_("Error: out of memory"));
      abort ();
    }

  amt = sizeof (Elf_Internal_Rela) * bed->s->int_rels_per_ext_rel;
  irela = (Elf_Internal_Rela *) bfd_zmalloc (amt);
  if (irela == NULL)
    {
      (*_bfd_error_handler) (_("Error: out of memory"));
      abort ();
    }

  for (i = 0; i < count; i++, rel_hash++)
    {
      if (*rel_hash == NULL)
	continue;

      BFD_ASSERT ((*rel_hash)->indx >= 0);

      if (rel_hdr->sh_entsize == sizeof (Elf_External_Rel))
	{
	  Elf_External_Rel *erel;
	  unsigned int j;

	  erel = (Elf_External_Rel *) rel_hdr->contents + i;
	  if (bed->s->swap_reloc_in)
	    (*bed->s->swap_reloc_in) (abfd, (bfd_byte *) erel, irel);
	  else
	    elf_swap_reloc_in (abfd, erel, irel);

	  for (j = 0; j < bed->s->int_rels_per_ext_rel; j++)
	    irel[j].r_info = ELF_R_INFO ((*rel_hash)->indx,
					 ELF_R_TYPE (irel[j].r_info));

	  if (bed->s->swap_reloc_out)
	    (*bed->s->swap_reloc_out) (abfd, irel, (bfd_byte *) erel);
	  else
	    elf_swap_reloc_out (abfd, irel, erel);
	}
      else
	{
	  Elf_External_Rela *erela;
	  unsigned int j;

	  BFD_ASSERT (rel_hdr->sh_entsize
		      == sizeof (Elf_External_Rela));

	  erela = (Elf_External_Rela *) rel_hdr->contents + i;
	  if (bed->s->swap_reloca_in)
	    (*bed->s->swap_reloca_in) (abfd, (bfd_byte *) erela, irela);
	  else
	    elf_swap_reloca_in (abfd, erela, irela);

	  for (j = 0; j < bed->s->int_rels_per_ext_rel; j++)
	    irela[j].r_info = ELF_R_INFO ((*rel_hash)->indx,
				       ELF_R_TYPE (irela[j].r_info));

	  if (bed->s->swap_reloca_out)
	    (*bed->s->swap_reloca_out) (abfd, irela, (bfd_byte *) erela);
	  else
	    elf_swap_reloca_out (abfd, irela, erela);
	}
    }

  free (irel);
  free (irela);
}

struct elf_link_sort_rela {
  bfd_vma offset;
  enum elf_reloc_type_class type;
  union {
    Elf_Internal_Rel rel;
    Elf_Internal_Rela rela;
  } u;
};

static int
elf_link_sort_cmp1 (A, B)
     const PTR A;
     const PTR B;
{
  struct elf_link_sort_rela *a = (struct elf_link_sort_rela *) A;
  struct elf_link_sort_rela *b = (struct elf_link_sort_rela *) B;
  int relativea, relativeb;

  relativea = a->type == reloc_class_relative;
  relativeb = b->type == reloc_class_relative;

  if (relativea < relativeb)
    return 1;
  if (relativea > relativeb)
    return -1;
  if (ELF_R_SYM (a->u.rel.r_info) < ELF_R_SYM (b->u.rel.r_info))
    return -1;
  if (ELF_R_SYM (a->u.rel.r_info) > ELF_R_SYM (b->u.rel.r_info))
    return 1;
  if (a->u.rel.r_offset < b->u.rel.r_offset)
    return -1;
  if (a->u.rel.r_offset > b->u.rel.r_offset)
    return 1;
  return 0;
}

static int
elf_link_sort_cmp2 (A, B)
     const PTR A;
     const PTR B;
{
  struct elf_link_sort_rela *a = (struct elf_link_sort_rela *) A;
  struct elf_link_sort_rela *b = (struct elf_link_sort_rela *) B;
  int copya, copyb;

  if (a->offset < b->offset)
    return -1;
  if (a->offset > b->offset)
    return 1;
  copya = (a->type == reloc_class_copy) * 2 + (a->type == reloc_class_plt);
  copyb = (b->type == reloc_class_copy) * 2 + (b->type == reloc_class_plt);
  if (copya < copyb)
    return -1;
  if (copya > copyb)
    return 1;
  if (a->u.rel.r_offset < b->u.rel.r_offset)
    return -1;
  if (a->u.rel.r_offset > b->u.rel.r_offset)
    return 1;
  return 0;
}

static size_t
elf_link_sort_relocs (abfd, info, psec)
     bfd *abfd;
     struct bfd_link_info *info;
     asection **psec;
{
  bfd *dynobj = elf_hash_table (info)->dynobj;
  asection *reldyn, *o;
  boolean rel = false;
  bfd_size_type count, size;
  size_t i, j, ret;
  struct elf_link_sort_rela *rela;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);

  reldyn = bfd_get_section_by_name (abfd, ".rela.dyn");
  if (reldyn == NULL || reldyn->_raw_size == 0)
    {
      reldyn = bfd_get_section_by_name (abfd, ".rel.dyn");
      if (reldyn == NULL || reldyn->_raw_size == 0)
	return 0;
      rel = true;
      count = reldyn->_raw_size / sizeof (Elf_External_Rel);
    }
  else
    count = reldyn->_raw_size / sizeof (Elf_External_Rela);

  size = 0;
  for (o = dynobj->sections; o != NULL; o = o->next)
    if ((o->flags & (SEC_HAS_CONTENTS|SEC_LINKER_CREATED))
	== (SEC_HAS_CONTENTS|SEC_LINKER_CREATED)
	&& o->output_section == reldyn)
      size += o->_raw_size;

  if (size != reldyn->_raw_size)
    return 0;

  rela = (struct elf_link_sort_rela *) bfd_zmalloc (sizeof (*rela) * count);
  if (rela == NULL)
    {
      (*info->callbacks->warning)
	(info, _("Not enough memory to sort relocations"), 0, abfd, 0,
	 (bfd_vma) 0);
      return 0;
    }

  for (o = dynobj->sections; o != NULL; o = o->next)
    if ((o->flags & (SEC_HAS_CONTENTS|SEC_LINKER_CREATED))
	== (SEC_HAS_CONTENTS|SEC_LINKER_CREATED)
	&& o->output_section == reldyn)
      {
	if (rel)
	  {
	    Elf_External_Rel *erel, *erelend;
	    struct elf_link_sort_rela *s;

	    erel = (Elf_External_Rel *) o->contents;
	    erelend = (Elf_External_Rel *) (o->contents + o->_raw_size);
	    s = rela + o->output_offset / sizeof (Elf_External_Rel);
	    for (; erel < erelend; erel++, s++)
	      {
		if (bed->s->swap_reloc_in)
		  (*bed->s->swap_reloc_in) (abfd, (bfd_byte *) erel, &s->u.rel);
		else
		  elf_swap_reloc_in (abfd, erel, &s->u.rel);

		s->type = (*bed->elf_backend_reloc_type_class) (&s->u.rela);
	      }
	  }
	else
	  {
	    Elf_External_Rela *erela, *erelaend;
	    struct elf_link_sort_rela *s;

	    erela = (Elf_External_Rela *) o->contents;
	    erelaend = (Elf_External_Rela *) (o->contents + o->_raw_size);
	    s = rela + o->output_offset / sizeof (Elf_External_Rela);
	    for (; erela < erelaend; erela++, s++)
	      {
		if (bed->s->swap_reloca_in)
		  (*bed->s->swap_reloca_in) (dynobj, (bfd_byte *) erela,
					     &s->u.rela);
		else
		  elf_swap_reloca_in (dynobj, erela, &s->u.rela);

		s->type = (*bed->elf_backend_reloc_type_class) (&s->u.rela);
	      }
	  }
      }

  qsort (rela, (size_t) count, sizeof (*rela), elf_link_sort_cmp1);
  for (ret = 0; ret < count && rela[ret].type == reloc_class_relative; ret++)
    ;
  for (i = ret, j = ret; i < count; i++)
    {
      if (ELF_R_SYM (rela[i].u.rel.r_info) != ELF_R_SYM (rela[j].u.rel.r_info))
	j = i;
      rela[i].offset = rela[j].u.rel.r_offset;
    }
  qsort (rela + ret, (size_t) count - ret, sizeof (*rela), elf_link_sort_cmp2);

  for (o = dynobj->sections; o != NULL; o = o->next)
    if ((o->flags & (SEC_HAS_CONTENTS|SEC_LINKER_CREATED))
	== (SEC_HAS_CONTENTS|SEC_LINKER_CREATED)
	&& o->output_section == reldyn)
      {
	if (rel)
	  {
	    Elf_External_Rel *erel, *erelend;
	    struct elf_link_sort_rela *s;

	    erel = (Elf_External_Rel *) o->contents;
	    erelend = (Elf_External_Rel *) (o->contents + o->_raw_size);
	    s = rela + o->output_offset / sizeof (Elf_External_Rel);
	    for (; erel < erelend; erel++, s++)
	      {
		if (bed->s->swap_reloc_out)
		  (*bed->s->swap_reloc_out) (abfd, &s->u.rel,
					     (bfd_byte *) erel);
		else
		  elf_swap_reloc_out (abfd, &s->u.rel, erel);
	      }
	  }
	else
	  {
	    Elf_External_Rela *erela, *erelaend;
	    struct elf_link_sort_rela *s;

	    erela = (Elf_External_Rela *) o->contents;
	    erelaend = (Elf_External_Rela *) (o->contents + o->_raw_size);
	    s = rela + o->output_offset / sizeof (Elf_External_Rela);
	    for (; erela < erelaend; erela++, s++)
	      {
		if (bed->s->swap_reloca_out)
		  (*bed->s->swap_reloca_out) (dynobj, &s->u.rela,
					      (bfd_byte *) erela);
		else
		  elf_swap_reloca_out (dynobj, &s->u.rela, erela);
	      }
	  }
      }

  free (rela);
  *psec = reldyn;
  return ret;
}

/* Do the final step of an ELF link.  */

boolean
elf_bfd_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean dynamic;
  boolean emit_relocs;
  bfd *dynobj;
  struct elf_final_link_info finfo;
  register asection *o;
  register struct bfd_link_order *p;
  register bfd *sub;
  bfd_size_type max_contents_size;
  bfd_size_type max_external_reloc_size;
  bfd_size_type max_internal_reloc_count;
  bfd_size_type max_sym_count;
  bfd_size_type max_sym_shndx_count;
  file_ptr off;
  Elf_Internal_Sym elfsym;
  unsigned int i;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *symstrtab_hdr;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  struct elf_outext_info eoinfo;
  boolean merged;
  size_t relativecount = 0;
  asection *reldyn = 0;
  bfd_size_type amt;

  if (! is_elf_hash_table (info))
    return false;

  if (info->shared)
    abfd->flags |= DYNAMIC;

  dynamic = elf_hash_table (info)->dynamic_sections_created;
  dynobj = elf_hash_table (info)->dynobj;

  emit_relocs = (info->relocateable
		 || info->emitrelocations
		 || bed->elf_backend_emit_relocs);

  finfo.info = info;
  finfo.output_bfd = abfd;
  finfo.symstrtab = elf_stringtab_init ();
  if (finfo.symstrtab == NULL)
    return false;

  if (! dynamic)
    {
      finfo.dynsym_sec = NULL;
      finfo.hash_sec = NULL;
      finfo.symver_sec = NULL;
    }
  else
    {
      finfo.dynsym_sec = bfd_get_section_by_name (dynobj, ".dynsym");
      finfo.hash_sec = bfd_get_section_by_name (dynobj, ".hash");
      BFD_ASSERT (finfo.dynsym_sec != NULL && finfo.hash_sec != NULL);
      finfo.symver_sec = bfd_get_section_by_name (dynobj, ".gnu.version");
      /* Note that it is OK if symver_sec is NULL.  */
    }

  finfo.contents = NULL;
  finfo.external_relocs = NULL;
  finfo.internal_relocs = NULL;
  finfo.external_syms = NULL;
  finfo.locsym_shndx = NULL;
  finfo.internal_syms = NULL;
  finfo.indices = NULL;
  finfo.sections = NULL;
  finfo.symbuf = NULL;
  finfo.symshndxbuf = NULL;
  finfo.symbuf_count = 0;

  /* Count up the number of relocations we will output for each output
     section, so that we know the sizes of the reloc sections.  We
     also figure out some maximum sizes.  */
  max_contents_size = 0;
  max_external_reloc_size = 0;
  max_internal_reloc_count = 0;
  max_sym_count = 0;
  max_sym_shndx_count = 0;
  merged = false;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      o->reloc_count = 0;

      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_section_reloc_link_order
	      || p->type == bfd_symbol_reloc_link_order)
	    ++o->reloc_count;
	  else if (p->type == bfd_indirect_link_order)
	    {
	      asection *sec;

	      sec = p->u.indirect.section;

	      /* Mark all sections which are to be included in the
		 link.  This will normally be every section.  We need
		 to do this so that we can identify any sections which
		 the linker has decided to not include.  */
	      sec->linker_mark = true;

	      if (sec->flags & SEC_MERGE)
		merged = true;

	      if (info->relocateable || info->emitrelocations)
		o->reloc_count += sec->reloc_count;
	      else if (bed->elf_backend_count_relocs)
		{
		  Elf_Internal_Rela * relocs;

		  relocs = (NAME(_bfd_elf,link_read_relocs)
			    (abfd, sec, (PTR) NULL,
			     (Elf_Internal_Rela *) NULL, info->keep_memory));

		  o->reloc_count
		    += (*bed->elf_backend_count_relocs) (sec, relocs);

		  if (!info->keep_memory)
		    free (relocs);
		}

	      if (sec->_raw_size > max_contents_size)
		max_contents_size = sec->_raw_size;
	      if (sec->_cooked_size > max_contents_size)
		max_contents_size = sec->_cooked_size;

	      /* We are interested in just local symbols, not all
		 symbols.  */
	      if (bfd_get_flavour (sec->owner) == bfd_target_elf_flavour
		  && (sec->owner->flags & DYNAMIC) == 0)
		{
		  size_t sym_count;

		  if (elf_bad_symtab (sec->owner))
		    sym_count = (elf_tdata (sec->owner)->symtab_hdr.sh_size
				 / sizeof (Elf_External_Sym));
		  else
		    sym_count = elf_tdata (sec->owner)->symtab_hdr.sh_info;

		  if (sym_count > max_sym_count)
		    max_sym_count = sym_count;

		  if (sym_count > max_sym_shndx_count
		      && elf_symtab_shndx (sec->owner) != 0)
		    max_sym_shndx_count = sym_count;

		  if ((sec->flags & SEC_RELOC) != 0)
		    {
		      size_t ext_size;

		      ext_size = elf_section_data (sec)->rel_hdr.sh_size;
		      if (ext_size > max_external_reloc_size)
			max_external_reloc_size = ext_size;
		      if (sec->reloc_count > max_internal_reloc_count)
			max_internal_reloc_count = sec->reloc_count;
		    }
		}
	    }
	}

      if (o->reloc_count > 0)
	o->flags |= SEC_RELOC;
      else
	{
	  /* Explicitly clear the SEC_RELOC flag.  The linker tends to
	     set it (this is probably a bug) and if it is set
	     assign_section_numbers will create a reloc section.  */
	  o->flags &=~ SEC_RELOC;
	}

      /* If the SEC_ALLOC flag is not set, force the section VMA to
	 zero.  This is done in elf_fake_sections as well, but forcing
	 the VMA to 0 here will ensure that relocs against these
	 sections are handled correctly.  */
      if ((o->flags & SEC_ALLOC) == 0
	  && ! o->user_set_vma)
	o->vma = 0;
    }

  if (! info->relocateable && merged)
    elf_link_hash_traverse (elf_hash_table (info),
			    elf_link_sec_merge_syms, (PTR) abfd);

  /* Figure out the file positions for everything but the symbol table
     and the relocs.  We set symcount to force assign_section_numbers
     to create a symbol table.  */
  bfd_get_symcount (abfd) = info->strip == strip_all ? 0 : 1;
  BFD_ASSERT (! abfd->output_has_begun);
  if (! _bfd_elf_compute_section_file_positions (abfd, info))
    goto error_return;

  /* Figure out how many relocations we will have in each section.
     Just using RELOC_COUNT isn't good enough since that doesn't
     maintain a separate value for REL vs. RELA relocations.  */
  if (emit_relocs)
    for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
      for (o = sub->sections; o != NULL; o = o->next)
	{
	  asection *output_section;

	  if (! o->linker_mark)
	    {
	      /* This section was omitted from the link.  */
	      continue;
	    }

	  output_section = o->output_section;

	  if (output_section != NULL
	      && (o->flags & SEC_RELOC) != 0)
	    {
	      struct bfd_elf_section_data *esdi
		= elf_section_data (o);
	      struct bfd_elf_section_data *esdo
		= elf_section_data (output_section);
	      unsigned int *rel_count;
	      unsigned int *rel_count2;
	      bfd_size_type entsize;
	      bfd_size_type entsize2;

	      /* We must be careful to add the relocations from the
		 input section to the right output count.  */
	      entsize = esdi->rel_hdr.sh_entsize;
	      entsize2 = esdi->rel_hdr2 ? esdi->rel_hdr2->sh_entsize : 0;
	      BFD_ASSERT ((entsize == sizeof (Elf_External_Rel)
			   || entsize == sizeof (Elf_External_Rela))
			  && entsize2 != entsize
			  && (entsize2 == 0
			      || entsize2 == sizeof (Elf_External_Rel)
			      || entsize2 == sizeof (Elf_External_Rela)));
	      if (entsize == esdo->rel_hdr.sh_entsize)
		{
		  rel_count = &esdo->rel_count;
		  rel_count2 = &esdo->rel_count2;
		}
	      else
		{
		  rel_count = &esdo->rel_count2;
		  rel_count2 = &esdo->rel_count;
		}

	      *rel_count += NUM_SHDR_ENTRIES (& esdi->rel_hdr);
	      if (esdi->rel_hdr2)
		*rel_count2 += NUM_SHDR_ENTRIES (esdi->rel_hdr2);
	      output_section->flags |= SEC_RELOC;
	    }
	}

  /* That created the reloc sections.  Set their sizes, and assign
     them file positions, and allocate some buffers.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0)
	{
	  if (!elf_link_size_reloc_section (abfd,
					    &elf_section_data (o)->rel_hdr,
					    o))
	    goto error_return;

	  if (elf_section_data (o)->rel_hdr2
	      && !elf_link_size_reloc_section (abfd,
					       elf_section_data (o)->rel_hdr2,
					       o))
	    goto error_return;
	}

      /* Now, reset REL_COUNT and REL_COUNT2 so that we can use them
	 to count upwards while actually outputting the relocations.  */
      elf_section_data (o)->rel_count = 0;
      elf_section_data (o)->rel_count2 = 0;
    }

  _bfd_elf_assign_file_positions_for_relocs (abfd);

  /* We have now assigned file positions for all the sections except
     .symtab and .strtab.  We start the .symtab section at the current
     file position, and write directly to it.  We build the .strtab
     section in memory.  */
  bfd_get_symcount (abfd) = 0;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  /* sh_name is set in prep_headers.  */
  symtab_hdr->sh_type = SHT_SYMTAB;
  symtab_hdr->sh_flags = 0;
  symtab_hdr->sh_addr = 0;
  symtab_hdr->sh_size = 0;
  symtab_hdr->sh_entsize = sizeof (Elf_External_Sym);
  /* sh_link is set in assign_section_numbers.  */
  /* sh_info is set below.  */
  /* sh_offset is set just below.  */
  symtab_hdr->sh_addralign = bed->s->file_align;

  off = elf_tdata (abfd)->next_file_pos;
  off = _bfd_elf_assign_file_position_for_section (symtab_hdr, off, true);

  /* Note that at this point elf_tdata (abfd)->next_file_pos is
     incorrect.  We do not yet know the size of the .symtab section.
     We correct next_file_pos below, after we do know the size.  */

  /* Allocate a buffer to hold swapped out symbols.  This is to avoid
     continuously seeking to the right position in the file.  */
  if (! info->keep_memory || max_sym_count < 20)
    finfo.symbuf_size = 20;
  else
    finfo.symbuf_size = max_sym_count;
  amt = finfo.symbuf_size;
  amt *= sizeof (Elf_External_Sym);
  finfo.symbuf = (Elf_External_Sym *) bfd_malloc (amt);
  if (finfo.symbuf == NULL)
    goto error_return;
  if (elf_numsections (abfd) > SHN_LORESERVE)
    {
      amt = finfo.symbuf_size;
      amt *= sizeof (Elf_External_Sym_Shndx);
      finfo.symshndxbuf = (Elf_External_Sym_Shndx *) bfd_malloc (amt);
      if (finfo.symshndxbuf == NULL)
	goto error_return;
    }

  /* Start writing out the symbol table.  The first symbol is always a
     dummy symbol.  */
  if (info->strip != strip_all
      || emit_relocs)
    {
      elfsym.st_value = 0;
      elfsym.st_size = 0;
      elfsym.st_info = 0;
      elfsym.st_other = 0;
      elfsym.st_shndx = SHN_UNDEF;
      if (! elf_link_output_sym (&finfo, (const char *) NULL,
				 &elfsym, bfd_und_section_ptr))
	goto error_return;
    }

#if 0
  /* Some standard ELF linkers do this, but we don't because it causes
     bootstrap comparison failures.  */
  /* Output a file symbol for the output file as the second symbol.
     We output this even if we are discarding local symbols, although
     I'm not sure if this is correct.  */
  elfsym.st_value = 0;
  elfsym.st_size = 0;
  elfsym.st_info = ELF_ST_INFO (STB_LOCAL, STT_FILE);
  elfsym.st_other = 0;
  elfsym.st_shndx = SHN_ABS;
  if (! elf_link_output_sym (&finfo, bfd_get_filename (abfd),
			     &elfsym, bfd_abs_section_ptr))
    goto error_return;
#endif

  /* Output a symbol for each section.  We output these even if we are
     discarding local symbols, since they are used for relocs.  These
     symbols have no names.  We store the index of each one in the
     index field of the section, so that we can find it again when
     outputting relocs.  */
  if (info->strip != strip_all
      || emit_relocs)
    {
      elfsym.st_size = 0;
      elfsym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
      elfsym.st_other = 0;
      for (i = 1; i < elf_numsections (abfd); i++)
	{
	  o = section_from_elf_index (abfd, i);
	  if (o != NULL)
	    o->target_index = bfd_get_symcount (abfd);
	  elfsym.st_shndx = i;
	  if (info->relocateable || o == NULL)
	    elfsym.st_value = 0;
	  else
	    elfsym.st_value = o->vma;
	  if (! elf_link_output_sym (&finfo, (const char *) NULL,
				     &elfsym, o))
	    goto error_return;
	  if (i == SHN_LORESERVE)
	    i += SHN_HIRESERVE + 1 - SHN_LORESERVE;
	}
    }

  /* Allocate some memory to hold information read in from the input
     files.  */
  if (max_contents_size != 0)
    {
      finfo.contents = (bfd_byte *) bfd_malloc (max_contents_size);
      if (finfo.contents == NULL)
	goto error_return;
    }

  if (max_external_reloc_size != 0)
    {
      finfo.external_relocs = (PTR) bfd_malloc (max_external_reloc_size);
      if (finfo.external_relocs == NULL)
	goto error_return;
    }

  if (max_internal_reloc_count != 0)
    {
      amt = max_internal_reloc_count * bed->s->int_rels_per_ext_rel;
      amt *= sizeof (Elf_Internal_Rela);
      finfo.internal_relocs = (Elf_Internal_Rela *) bfd_malloc (amt);
      if (finfo.internal_relocs == NULL)
	goto error_return;
    }

  if (max_sym_count != 0)
    {
      amt = max_sym_count * sizeof (Elf_External_Sym);
      finfo.external_syms = (Elf_External_Sym *) bfd_malloc (amt);
      if (finfo.external_syms == NULL)
	goto error_return;

      amt = max_sym_count * sizeof (Elf_Internal_Sym);
      finfo.internal_syms = (Elf_Internal_Sym *) bfd_malloc (amt);
      if (finfo.internal_syms == NULL)
	goto error_return;

      amt = max_sym_count * sizeof (long);
      finfo.indices = (long *) bfd_malloc (amt);
      if (finfo.indices == NULL)
	goto error_return;

      amt = max_sym_count * sizeof (asection *);
      finfo.sections = (asection **) bfd_malloc (amt);
      if (finfo.sections == NULL)
	goto error_return;
    }

  if (max_sym_shndx_count != 0)
    {
      amt = max_sym_shndx_count * sizeof (Elf_External_Sym_Shndx);
      finfo.locsym_shndx = (Elf_External_Sym_Shndx *) bfd_malloc (amt);
      if (finfo.locsym_shndx == NULL)
	goto error_return;
    }

  /* Since ELF permits relocations to be against local symbols, we
     must have the local symbols available when we do the relocations.
     Since we would rather only read the local symbols once, and we
     would rather not keep them in memory, we handle all the
     relocations for a single input file at the same time.

     Unfortunately, there is no way to know the total number of local
     symbols until we have seen all of them, and the local symbol
     indices precede the global symbol indices.  This means that when
     we are generating relocateable output, and we see a reloc against
     a global symbol, we can not know the symbol index until we have
     finished examining all the local symbols to see which ones we are
     going to output.  To deal with this, we keep the relocations in
     memory, and don't output them until the end of the link.  This is
     an unfortunate waste of memory, but I don't see a good way around
     it.  Fortunately, it only happens when performing a relocateable
     link, which is not the common case.  FIXME: If keep_memory is set
     we could write the relocs out and then read them again; I don't
     know how bad the memory loss will be.  */

  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    sub->output_has_begun = false;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && (bfd_get_flavour ((sub = p->u.indirect.section->owner))
		  == bfd_target_elf_flavour)
	      && elf_elfheader (sub)->e_ident[EI_CLASS] == bed->s->elfclass)
	    {
	      if (! sub->output_has_begun)
		{
		  if (! elf_link_input_bfd (&finfo, sub))
		    goto error_return;
		  sub->output_has_begun = true;
		}
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      if (! elf_reloc_link_order (abfd, info, o, p))
		goto error_return;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		goto error_return;
	    }
	}
    }

  /* Output any global symbols that got converted to local in a
     version script or due to symbol visibility.  We do this in a
     separate step since ELF requires all local symbols to appear
     prior to any global symbols.  FIXME: We should only do this if
     some global symbols were, in fact, converted to become local.
     FIXME: Will this work correctly with the Irix 5 linker?  */
  eoinfo.failed = false;
  eoinfo.finfo = &finfo;
  eoinfo.localsyms = true;
  elf_link_hash_traverse (elf_hash_table (info), elf_link_output_extsym,
			  (PTR) &eoinfo);
  if (eoinfo.failed)
    return false;

  /* That wrote out all the local symbols.  Finish up the symbol table
     with the global symbols. Even if we want to strip everything we
     can, we still need to deal with those global symbols that got
     converted to local in a version script.  */

  /* The sh_info field records the index of the first non local symbol.  */
  symtab_hdr->sh_info = bfd_get_symcount (abfd);

  if (dynamic
      && finfo.dynsym_sec->output_section != bfd_abs_section_ptr)
    {
      Elf_Internal_Sym sym;
      Elf_External_Sym *dynsym =
	(Elf_External_Sym *) finfo.dynsym_sec->contents;
      long last_local = 0;

      /* Write out the section symbols for the output sections.  */
      if (info->shared)
	{
	  asection *s;

	  sym.st_size = 0;
	  sym.st_name = 0;
	  sym.st_info = ELF_ST_INFO (STB_LOCAL, STT_SECTION);
	  sym.st_other = 0;

	  for (s = abfd->sections; s != NULL; s = s->next)
	    {
	      int indx;
	      Elf_External_Sym *dest;

	      indx = elf_section_data (s)->this_idx;
	      BFD_ASSERT (indx > 0);
	      sym.st_shndx = indx;
	      sym.st_value = s->vma;
	      dest = dynsym + elf_section_data (s)->dynindx;
	      elf_swap_symbol_out (abfd, &sym, (PTR) dest, (PTR) 0);
	    }

	  last_local = bfd_count_sections (abfd);
	}

      /* Write out the local dynsyms.  */
      if (elf_hash_table (info)->dynlocal)
	{
	  struct elf_link_local_dynamic_entry *e;
	  for (e = elf_hash_table (info)->dynlocal; e ; e = e->next)
	    {
	      asection *s;
	      Elf_External_Sym *dest;

	      sym.st_size = e->isym.st_size;
	      sym.st_other = e->isym.st_other;

	      /* Copy the internal symbol as is.
		 Note that we saved a word of storage and overwrote
		 the original st_name with the dynstr_index.  */
	      sym = e->isym;

	      if (e->isym.st_shndx != SHN_UNDEF
		   && (e->isym.st_shndx < SHN_LORESERVE
		       || e->isym.st_shndx > SHN_HIRESERVE))
		{
		  s = bfd_section_from_elf_index (e->input_bfd,
						  e->isym.st_shndx);

		  sym.st_shndx =
		    elf_section_data (s->output_section)->this_idx;
		  sym.st_value = (s->output_section->vma
				  + s->output_offset
				  + e->isym.st_value);
		}

	      if (last_local < e->dynindx)
		last_local = e->dynindx;

	      dest = dynsym + e->dynindx;
	      elf_swap_symbol_out (abfd, &sym, (PTR) dest, (PTR) 0);
	    }
	}

      elf_section_data (finfo.dynsym_sec->output_section)->this_hdr.sh_info =
	last_local + 1;
    }

  /* We get the global symbols from the hash table.  */
  eoinfo.failed = false;
  eoinfo.localsyms = false;
  eoinfo.finfo = &finfo;
  elf_link_hash_traverse (elf_hash_table (info), elf_link_output_extsym,
			  (PTR) &eoinfo);
  if (eoinfo.failed)
    return false;

  /* If backend needs to output some symbols not present in the hash
     table, do it now.  */
  if (bed->elf_backend_output_arch_syms)
    {
      typedef boolean (*out_sym_func) PARAMS ((PTR, const char *,
					       Elf_Internal_Sym *,
					       asection *));

      if (! ((*bed->elf_backend_output_arch_syms)
	     (abfd, info, (PTR) &finfo, (out_sym_func) elf_link_output_sym)))
	return false;
    }

  /* Flush all symbols to the file.  */
  if (! elf_link_flush_output_syms (&finfo))
    return false;

  /* Now we know the size of the symtab section.  */
  off += symtab_hdr->sh_size;

  /* Finish up and write out the symbol string table (.strtab)
     section.  */
  symstrtab_hdr = &elf_tdata (abfd)->strtab_hdr;
  /* sh_name was set in prep_headers.  */
  symstrtab_hdr->sh_type = SHT_STRTAB;
  symstrtab_hdr->sh_flags = 0;
  symstrtab_hdr->sh_addr = 0;
  symstrtab_hdr->sh_size = _bfd_stringtab_size (finfo.symstrtab);
  symstrtab_hdr->sh_entsize = 0;
  symstrtab_hdr->sh_link = 0;
  symstrtab_hdr->sh_info = 0;
  /* sh_offset is set just below.  */
  symstrtab_hdr->sh_addralign = 1;

  off = _bfd_elf_assign_file_position_for_section (symstrtab_hdr, off, true);
  elf_tdata (abfd)->next_file_pos = off;

  if (bfd_get_symcount (abfd) > 0)
    {
      if (bfd_seek (abfd, symstrtab_hdr->sh_offset, SEEK_SET) != 0
	  || ! _bfd_stringtab_emit (abfd, finfo.symstrtab))
	return false;
    }

  /* Adjust the relocs to have the correct symbol indices.  */
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) == 0)
	continue;

      elf_link_adjust_relocs (abfd, &elf_section_data (o)->rel_hdr,
			      elf_section_data (o)->rel_count,
			      elf_section_data (o)->rel_hashes);
      if (elf_section_data (o)->rel_hdr2 != NULL)
	elf_link_adjust_relocs (abfd, elf_section_data (o)->rel_hdr2,
				elf_section_data (o)->rel_count2,
				(elf_section_data (o)->rel_hashes
				 + elf_section_data (o)->rel_count));

      /* Set the reloc_count field to 0 to prevent write_relocs from
	 trying to swap the relocs out itself.  */
      o->reloc_count = 0;
    }

  if (dynamic && info->combreloc && dynobj != NULL)
    relativecount = elf_link_sort_relocs (abfd, info, &reldyn);

  /* If we are linking against a dynamic object, or generating a
     shared library, finish up the dynamic linking information.  */
  if (dynamic)
    {
      Elf_External_Dyn *dyncon, *dynconend;

      /* Fix up .dynamic entries.  */
      o = bfd_get_section_by_name (dynobj, ".dynamic");
      BFD_ASSERT (o != NULL);

      dyncon = (Elf_External_Dyn *) o->contents;
      dynconend = (Elf_External_Dyn *) (o->contents + o->_raw_size);
      for (; dyncon < dynconend; dyncon++)
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  unsigned int type;

	  elf_swap_dyn_in (dynobj, dyncon, &dyn);

	  switch (dyn.d_tag)
	    {
	    default:
	      break;
	    case DT_NULL:
	      if (relativecount > 0 && dyncon + 1 < dynconend)
		{
		  switch (elf_section_data (reldyn)->this_hdr.sh_type)
		    {
		    case SHT_REL: dyn.d_tag = DT_RELCOUNT; break;
		    case SHT_RELA: dyn.d_tag = DT_RELACOUNT; break;
		    default: break;
		    }
		  if (dyn.d_tag != DT_NULL)
		    {
		      dyn.d_un.d_val = relativecount;
		      elf_swap_dyn_out (dynobj, &dyn, dyncon);
		      relativecount = 0;
		    }
		}
	      break;
	    case DT_INIT:
	      name = info->init_function;
	      goto get_sym;
	    case DT_FINI:
	      name = info->fini_function;
	    get_sym:
	      {
		struct elf_link_hash_entry *h;

		h = elf_link_hash_lookup (elf_hash_table (info), name,
					  false, false, true);
		if (h != NULL
		    && (h->root.type == bfd_link_hash_defined
			|| h->root.type == bfd_link_hash_defweak))
		  {
		    dyn.d_un.d_val = h->root.u.def.value;
		    o = h->root.u.def.section;
		    if (o->output_section != NULL)
		      dyn.d_un.d_val += (o->output_section->vma
					 + o->output_offset);
		    else
		      {
			/* The symbol is imported from another shared
			   library and does not apply to this one.  */
			dyn.d_un.d_val = 0;
		      }

		    elf_swap_dyn_out (dynobj, &dyn, dyncon);
		  }
	      }
	      break;

	    case DT_PREINIT_ARRAYSZ:
	      name = ".preinit_array";
	      goto get_size;
	    case DT_INIT_ARRAYSZ:
	      name = ".init_array";
	      goto get_size;
	    case DT_FINI_ARRAYSZ:
	      name = ".fini_array";
	    get_size:
	      o = bfd_get_section_by_name (abfd, name);
	      BFD_ASSERT (o != NULL);
	      if (o->_raw_size == 0)
		(*_bfd_error_handler)
		  (_("warning: %s section has zero size"), name);
	      dyn.d_un.d_val = o->_raw_size;
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_PREINIT_ARRAY:
	      name = ".preinit_array";
	      goto get_vma;
	    case DT_INIT_ARRAY:
	      name = ".init_array";
	      goto get_vma;
	    case DT_FINI_ARRAY:
	      name = ".fini_array";
	      goto get_vma;

	    case DT_HASH:
	      name = ".hash";
	      goto get_vma;
	    case DT_STRTAB:
	      name = ".dynstr";
	      goto get_vma;
	    case DT_SYMTAB:
	      name = ".dynsym";
	      goto get_vma;
	    case DT_VERDEF:
	      name = ".gnu.version_d";
	      goto get_vma;
	    case DT_VERNEED:
	      name = ".gnu.version_r";
	      goto get_vma;
	    case DT_VERSYM:
	      name = ".gnu.version";
	    get_vma:
	      o = bfd_get_section_by_name (abfd, name);
	      BFD_ASSERT (o != NULL);
	      dyn.d_un.d_ptr = o->vma;
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;

	    case DT_REL:
	    case DT_RELA:
	    case DT_RELSZ:
	    case DT_RELASZ:
	      if (dyn.d_tag == DT_REL || dyn.d_tag == DT_RELSZ)
		type = SHT_REL;
	      else
		type = SHT_RELA;
	      dyn.d_un.d_val = 0;
	      for (i = 1; i < elf_numsections (abfd); i++)
		{
		  Elf_Internal_Shdr *hdr;

		  hdr = elf_elfsections (abfd)[i];
		  if (hdr->sh_type == type
		      && (hdr->sh_flags & SHF_ALLOC) != 0)
		    {
		      if (dyn.d_tag == DT_RELSZ || dyn.d_tag == DT_RELASZ)
			dyn.d_un.d_val += hdr->sh_size;
		      else
			{
			  if (dyn.d_un.d_val == 0
			      || hdr->sh_addr < dyn.d_un.d_val)
			    dyn.d_un.d_val = hdr->sh_addr;
			}
		    }
		}
	      elf_swap_dyn_out (dynobj, &dyn, dyncon);
	      break;
	    }
	}
    }

  /* If we have created any dynamic sections, then output them.  */
  if (dynobj != NULL)
    {
      if (! (*bed->elf_backend_finish_dynamic_sections) (abfd, info))
	goto error_return;

      for (o = dynobj->sections; o != NULL; o = o->next)
	{
	  if ((o->flags & SEC_HAS_CONTENTS) == 0
	      || o->_raw_size == 0
	      || o->output_section == bfd_abs_section_ptr)
	    continue;
	  if ((o->flags & SEC_LINKER_CREATED) == 0)
	    {
	      /* At this point, we are only interested in sections
		 created by elf_link_create_dynamic_sections.  */
	      continue;
	    }
	  if ((elf_section_data (o->output_section)->this_hdr.sh_type
	       != SHT_STRTAB)
	      || strcmp (bfd_get_section_name (abfd, o), ".dynstr") != 0)
	    {
	      if (! bfd_set_section_contents (abfd, o->output_section,
					      o->contents,
					      (file_ptr) o->output_offset,
					      o->_raw_size))
		goto error_return;
	    }
	  else
	    {
	      /* The contents of the .dynstr section are actually in a
		 stringtab.  */
	      off = elf_section_data (o->output_section)->this_hdr.sh_offset;
	      if (bfd_seek (abfd, off, SEEK_SET) != 0
		  || ! _bfd_elf_strtab_emit (abfd,
					     elf_hash_table (info)->dynstr))
		goto error_return;
	    }
	}
    }

  /* If we have optimized stabs strings, output them.  */
  if (elf_hash_table (info)->stab_info != NULL)
    {
      if (! _bfd_write_stab_strings (abfd, &elf_hash_table (info)->stab_info))
	goto error_return;
    }

  if (info->eh_frame_hdr && elf_hash_table (info)->dynobj)
    {
      o = bfd_get_section_by_name (elf_hash_table (info)->dynobj,
				   ".eh_frame_hdr");
      if (o
	  && (elf_section_data (o)->sec_info_type
	      == ELF_INFO_TYPE_EH_FRAME_HDR))
	{
	  if (! _bfd_elf_write_section_eh_frame_hdr (abfd, o))
	    goto error_return;
	}
    }

  if (finfo.symstrtab != NULL)
    _bfd_stringtab_free (finfo.symstrtab);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (finfo.external_syms != NULL)
    free (finfo.external_syms);
  if (finfo.locsym_shndx != NULL)
    free (finfo.locsym_shndx);
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.indices != NULL)
    free (finfo.indices);
  if (finfo.sections != NULL)
    free (finfo.sections);
  if (finfo.symbuf != NULL)
    free (finfo.symbuf);
  if (finfo.symshndxbuf != NULL)
    free (finfo.symbuf);
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0
	  && elf_section_data (o)->rel_hashes != NULL)
	free (elf_section_data (o)->rel_hashes);
    }

  elf_tdata (abfd)->linker = true;

  return true;

 error_return:
  if (finfo.symstrtab != NULL)
    _bfd_stringtab_free (finfo.symstrtab);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (finfo.external_syms != NULL)
    free (finfo.external_syms);
  if (finfo.locsym_shndx != NULL)
    free (finfo.locsym_shndx);
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.indices != NULL)
    free (finfo.indices);
  if (finfo.sections != NULL)
    free (finfo.sections);
  if (finfo.symbuf != NULL)
    free (finfo.symbuf);
  if (finfo.symshndxbuf != NULL)
    free (finfo.symbuf);
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if ((o->flags & SEC_RELOC) != 0
	  && elf_section_data (o)->rel_hashes != NULL)
	free (elf_section_data (o)->rel_hashes);
    }

  return false;
}

/* Add a symbol to the output symbol table.  */

static boolean
elf_link_output_sym (finfo, name, elfsym, input_sec)
     struct elf_final_link_info *finfo;
     const char *name;
     Elf_Internal_Sym *elfsym;
     asection *input_sec;
{
  Elf_External_Sym *dest;
  Elf_External_Sym_Shndx *destshndx;

  boolean (*output_symbol_hook) PARAMS ((bfd *,
					 struct bfd_link_info *info,
					 const char *,
					 Elf_Internal_Sym *,
					 asection *));

  output_symbol_hook = get_elf_backend_data (finfo->output_bfd)->
    elf_backend_link_output_symbol_hook;
  if (output_symbol_hook != NULL)
    {
      if (! ((*output_symbol_hook)
	     (finfo->output_bfd, finfo->info, name, elfsym, input_sec)))
	return false;
    }

  if (name == (const char *) NULL || *name == '\0')
    elfsym->st_name = 0;
  else if (input_sec->flags & SEC_EXCLUDE)
    elfsym->st_name = 0;
  else
    {
      elfsym->st_name = (unsigned long) _bfd_stringtab_add (finfo->symstrtab,
							    name, true, false);
      if (elfsym->st_name == (unsigned long) -1)
	return false;
    }

  if (finfo->symbuf_count >= finfo->symbuf_size)
    {
      if (! elf_link_flush_output_syms (finfo))
	return false;
    }

  dest = finfo->symbuf + finfo->symbuf_count;
  destshndx = finfo->symshndxbuf;
  if (destshndx != NULL)
    destshndx += finfo->symbuf_count;
  elf_swap_symbol_out (finfo->output_bfd, elfsym, (PTR) dest, (PTR) destshndx);
  ++finfo->symbuf_count;

  ++ bfd_get_symcount (finfo->output_bfd);

  return true;
}

/* Flush the output symbols to the file.  */

static boolean
elf_link_flush_output_syms (finfo)
     struct elf_final_link_info *finfo;
{
  if (finfo->symbuf_count > 0)
    {
      Elf_Internal_Shdr *hdr;
      file_ptr pos;
      bfd_size_type amt;

      hdr = &elf_tdata (finfo->output_bfd)->symtab_hdr;
      pos = hdr->sh_offset + hdr->sh_size;
      amt = finfo->symbuf_count * sizeof (Elf_External_Sym);
      if (bfd_seek (finfo->output_bfd, pos, SEEK_SET) != 0
	  || bfd_bwrite ((PTR) finfo->symbuf, amt, finfo->output_bfd) != amt)
	return false;

      hdr->sh_size += amt;

      if (finfo->symshndxbuf != NULL)
	{
	  hdr = &elf_tdata (finfo->output_bfd)->symtab_shndx_hdr;
	  pos = hdr->sh_offset + hdr->sh_size;
	  amt = finfo->symbuf_count * sizeof (Elf_External_Sym_Shndx);
	  if (bfd_seek (finfo->output_bfd, pos, SEEK_SET) != 0
	      || (bfd_bwrite ((PTR) finfo->symshndxbuf, amt, finfo->output_bfd)
		  != amt))
	    return false;

	  hdr->sh_size += amt;
	}

      finfo->symbuf_count = 0;
    }

  return true;
}

/* Adjust all external symbols pointing into SEC_MERGE sections
   to reflect the object merging within the sections.  */

static boolean
elf_link_sec_merge_syms (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  asection *sec;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if ((h->root.type == bfd_link_hash_defined
       || h->root.type == bfd_link_hash_defweak)
      && ((sec = h->root.u.def.section)->flags & SEC_MERGE)
      && elf_section_data (sec)->sec_info_type == ELF_INFO_TYPE_MERGE)
    {
      bfd *output_bfd = (bfd *) data;

      h->root.u.def.value =
	_bfd_merged_section_offset (output_bfd,
				    &h->root.u.def.section,
				    elf_section_data (sec)->sec_info,
				    h->root.u.def.value, (bfd_vma) 0);
    }

  return true;
}

/* Add an external symbol to the symbol table.  This is called from
   the hash table traversal routine.  When generating a shared object,
   we go through the symbol table twice.  The first time we output
   anything that might have been forced to local scope in a version
   script.  The second time we output the symbols that are still
   global symbols.  */

static boolean
elf_link_output_extsym (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  struct elf_outext_info *eoinfo = (struct elf_outext_info *) data;
  struct elf_final_link_info *finfo = eoinfo->finfo;
  boolean strip;
  Elf_Internal_Sym sym;
  asection *input_sec;

  if (h->root.type == bfd_link_hash_warning)
    {
      h = (struct elf_link_hash_entry *) h->root.u.i.link;
      if (h->root.type == bfd_link_hash_new)
	return true;
    }

  /* Decide whether to output this symbol in this pass.  */
  if (eoinfo->localsyms)
    {
      if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
	return true;
    }
  else
    {
      if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
	return true;
    }

  /* If we are not creating a shared library, and this symbol is
     referenced by a shared library but is not defined anywhere, then
     warn that it is undefined.  If we do not do this, the runtime
     linker will complain that the symbol is undefined when the
     program is run.  We don't have to worry about symbols that are
     referenced by regular files, because we will already have issued
     warnings for them.  */
  if (! finfo->info->relocateable
      && ! finfo->info->allow_shlib_undefined
      && ! finfo->info->shared
      && h->root.type == bfd_link_hash_undefined
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    {
      if (! ((*finfo->info->callbacks->undefined_symbol)
	     (finfo->info, h->root.root.string, h->root.u.undef.abfd,
	      (asection *) NULL, (bfd_vma) 0, true)))
	{
	  eoinfo->failed = true;
	  return false;
	}
    }

  /* We don't want to output symbols that have never been mentioned by
     a regular file, or that we have been told to strip.  However, if
     h->indx is set to -2, the symbol is used by a reloc and we must
     output it.  */
  if (h->indx == -2)
    strip = false;
  else if (((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (finfo->info->strip == strip_all
	   || (finfo->info->strip == strip_some
	       && bfd_hash_lookup (finfo->info->keep_hash,
				   h->root.root.string,
				   false, false) == NULL))
    strip = true;
  else
    strip = false;

  /* If we're stripping it, and it's not a dynamic symbol, there's
     nothing else to do unless it is a forced local symbol.  */
  if (strip
      && h->dynindx == -1
      && (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
    return true;

  sym.st_value = 0;
  sym.st_size = h->size;
  sym.st_other = h->other;
  if ((h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
    sym.st_info = ELF_ST_INFO (STB_LOCAL, h->type);
  else if (h->root.type == bfd_link_hash_undefweak
	   || h->root.type == bfd_link_hash_defweak)
    sym.st_info = ELF_ST_INFO (STB_WEAK, h->type);
  else
    sym.st_info = ELF_ST_INFO (STB_GLOBAL, h->type);

  switch (h->root.type)
    {
    default:
    case bfd_link_hash_new:
    case bfd_link_hash_warning:
      abort ();
      return false;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      input_sec = bfd_und_section_ptr;
      sym.st_shndx = SHN_UNDEF;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      {
	input_sec = h->root.u.def.section;
	if (input_sec->output_section != NULL)
	  {
	    sym.st_shndx =
	      _bfd_elf_section_from_bfd_section (finfo->output_bfd,
						 input_sec->output_section);
	    if (sym.st_shndx == SHN_BAD)
	      {
		(*_bfd_error_handler)
		  (_("%s: could not find output section %s for input section %s"),
		   bfd_get_filename (finfo->output_bfd),
		   input_sec->output_section->name,
		   input_sec->name);
		eoinfo->failed = true;
		return false;
	      }

	    /* ELF symbols in relocateable files are section relative,
	       but in nonrelocateable files they are virtual
	       addresses.  */
	    sym.st_value = h->root.u.def.value + input_sec->output_offset;
	    if (! finfo->info->relocateable)
	      sym.st_value += input_sec->output_section->vma;
	  }
	else
	  {
	    BFD_ASSERT (input_sec->owner == NULL
			|| (input_sec->owner->flags & DYNAMIC) != 0);
	    sym.st_shndx = SHN_UNDEF;
	    input_sec = bfd_und_section_ptr;
	  }
      }
      break;

    case bfd_link_hash_common:
      input_sec = h->root.u.c.p->section;
      sym.st_shndx = SHN_COMMON;
      sym.st_value = 1 << h->root.u.c.p->alignment_power;
      break;

    case bfd_link_hash_indirect:
      /* These symbols are created by symbol versioning.  They point
	 to the decorated version of the name.  For example, if the
	 symbol foo@@GNU_1.2 is the default, which should be used when
	 foo is used with no version, then we add an indirect symbol
	 foo which points to foo@@GNU_1.2.  We ignore these symbols,
	 since the indirected symbol is already in the hash table.  */
      return true;
    }

  /* Give the processor backend a chance to tweak the symbol value,
     and also to finish up anything that needs to be done for this
     symbol.  FIXME: Not calling elf_backend_finish_dynamic_symbol for
     forced local syms when non-shared is due to a historical quirk.  */
  if ((h->dynindx != -1
       || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
      && (finfo->info->shared
	  || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) == 0)
      && elf_hash_table (finfo->info)->dynamic_sections_created)
    {
      struct elf_backend_data *bed;

      bed = get_elf_backend_data (finfo->output_bfd);
      if (! ((*bed->elf_backend_finish_dynamic_symbol)
	     (finfo->output_bfd, finfo->info, h, &sym)))
	{
	  eoinfo->failed = true;
	  return false;
	}
    }

  /* If we are marking the symbol as undefined, and there are no
     non-weak references to this symbol from a regular object, then
     mark the symbol as weak undefined; if there are non-weak
     references, mark the symbol as strong.  We can't do this earlier,
     because it might not be marked as undefined until the
     finish_dynamic_symbol routine gets through with it.  */
  if (sym.st_shndx == SHN_UNDEF
      && (h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) != 0
      && (ELF_ST_BIND (sym.st_info) == STB_GLOBAL
	  || ELF_ST_BIND (sym.st_info) == STB_WEAK))
    {
      int bindtype;

      if ((h->elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR_NONWEAK) != 0)
	bindtype = STB_GLOBAL;
      else
	bindtype = STB_WEAK;
      sym.st_info = ELF_ST_INFO (bindtype, ELF_ST_TYPE (sym.st_info));
    }

  /* If a symbol is not defined locally, we clear the visibility
     field.  */
  if (! finfo->info->relocateable
      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
    sym.st_other ^= ELF_ST_VISIBILITY (sym.st_other);

  /* If this symbol should be put in the .dynsym section, then put it
     there now.  We have already know the symbol index.  We also fill
     in the entry in the .hash section.  */
  if (h->dynindx != -1
      && elf_hash_table (finfo->info)->dynamic_sections_created)
    {
      size_t bucketcount;
      size_t bucket;
      size_t hash_entry_size;
      bfd_byte *bucketpos;
      bfd_vma chain;
      Elf_External_Sym *esym;

      sym.st_name = h->dynstr_index;
      esym = (Elf_External_Sym *) finfo->dynsym_sec->contents + h->dynindx;
      elf_swap_symbol_out (finfo->output_bfd, &sym, (PTR) esym, (PTR) 0);

      bucketcount = elf_hash_table (finfo->info)->bucketcount;
      bucket = h->elf_hash_value % bucketcount;
      hash_entry_size
	= elf_section_data (finfo->hash_sec)->this_hdr.sh_entsize;
      bucketpos = ((bfd_byte *) finfo->hash_sec->contents
		   + (bucket + 2) * hash_entry_size);
      chain = bfd_get (8 * hash_entry_size, finfo->output_bfd, bucketpos);
      bfd_put (8 * hash_entry_size, finfo->output_bfd, (bfd_vma) h->dynindx,
	       bucketpos);
      bfd_put (8 * hash_entry_size, finfo->output_bfd, chain,
	       ((bfd_byte *) finfo->hash_sec->contents
		+ (bucketcount + 2 + h->dynindx) * hash_entry_size));

      if (finfo->symver_sec != NULL && finfo->symver_sec->contents != NULL)
	{
	  Elf_Internal_Versym iversym;
	  Elf_External_Versym *eversym;

	  if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	    {
	      if (h->verinfo.verdef == NULL)
		iversym.vs_vers = 0;
	      else
		iversym.vs_vers = h->verinfo.verdef->vd_exp_refno + 1;
	    }
	  else
	    {
	      if (h->verinfo.vertree == NULL)
		iversym.vs_vers = 1;
	      else
		iversym.vs_vers = h->verinfo.vertree->vernum + 1;
	    }

	  if ((h->elf_link_hash_flags & ELF_LINK_HIDDEN) != 0)
	    iversym.vs_vers |= VERSYM_HIDDEN;

	  eversym = (Elf_External_Versym *) finfo->symver_sec->contents;
	  eversym += h->dynindx;
	  _bfd_elf_swap_versym_out (finfo->output_bfd, &iversym, eversym);
	}
    }

  /* If we're stripping it, then it was just a dynamic symbol, and
     there's nothing else to do.  */
  if (strip)
    return true;

  h->indx = bfd_get_symcount (finfo->output_bfd);

  if (! elf_link_output_sym (finfo, h->root.root.string, &sym, input_sec))
    {
      eoinfo->failed = true;
      return false;
    }

  return true;
}

/* Copy the relocations indicated by the INTERNAL_RELOCS (which
   originated from the section given by INPUT_REL_HDR) to the
   OUTPUT_BFD.  */

static void
elf_link_output_relocs (output_bfd, input_section, input_rel_hdr,
			internal_relocs)
     bfd *output_bfd;
     asection *input_section;
     Elf_Internal_Shdr *input_rel_hdr;
     Elf_Internal_Rela *internal_relocs;
{
  Elf_Internal_Rela *irela;
  Elf_Internal_Rela *irelaend;
  Elf_Internal_Shdr *output_rel_hdr;
  asection *output_section;
  unsigned int *rel_countp = NULL;
  struct elf_backend_data *bed;
  bfd_size_type amt;

  output_section = input_section->output_section;
  output_rel_hdr = NULL;

  if (elf_section_data (output_section)->rel_hdr.sh_entsize
      == input_rel_hdr->sh_entsize)
    {
      output_rel_hdr = &elf_section_data (output_section)->rel_hdr;
      rel_countp = &elf_section_data (output_section)->rel_count;
    }
  else if (elf_section_data (output_section)->rel_hdr2
	   && (elf_section_data (output_section)->rel_hdr2->sh_entsize
	       == input_rel_hdr->sh_entsize))
    {
      output_rel_hdr = elf_section_data (output_section)->rel_hdr2;
      rel_countp = &elf_section_data (output_section)->rel_count2;
    }

  BFD_ASSERT (output_rel_hdr != NULL);

  bed = get_elf_backend_data (output_bfd);
  irela = internal_relocs;
  irelaend = irela + NUM_SHDR_ENTRIES (input_rel_hdr)
		     * bed->s->int_rels_per_ext_rel;

  if (input_rel_hdr->sh_entsize == sizeof (Elf_External_Rel))
    {
      Elf_External_Rel *erel;
      Elf_Internal_Rel *irel;

      amt = bed->s->int_rels_per_ext_rel * sizeof (Elf_Internal_Rel);
      irel = (Elf_Internal_Rel *) bfd_zmalloc (amt);
      if (irel == NULL)
	{
	  (*_bfd_error_handler) (_("Error: out of memory"));
	  abort ();
	}

      erel = ((Elf_External_Rel *) output_rel_hdr->contents + *rel_countp);
      for (; irela < irelaend; irela += bed->s->int_rels_per_ext_rel, erel++)
	{
	  unsigned int i;

	  for (i = 0; i < bed->s->int_rels_per_ext_rel; i++)
	    {
	      irel[i].r_offset = irela[i].r_offset;
	      irel[i].r_info = irela[i].r_info;
	      BFD_ASSERT (irela[i].r_addend == 0);
	    }

	  if (bed->s->swap_reloc_out)
	    (*bed->s->swap_reloc_out) (output_bfd, irel, (PTR) erel);
	  else
	    elf_swap_reloc_out (output_bfd, irel, erel);
	}

      free (irel);
    }
  else
    {
      Elf_External_Rela *erela;

      BFD_ASSERT (input_rel_hdr->sh_entsize == sizeof (Elf_External_Rela));

      erela = ((Elf_External_Rela *) output_rel_hdr->contents + *rel_countp);
      for (; irela < irelaend; irela += bed->s->int_rels_per_ext_rel, erela++)
	if (bed->s->swap_reloca_out)
	  (*bed->s->swap_reloca_out) (output_bfd, irela, (PTR) erela);
	else
	  elf_swap_reloca_out (output_bfd, irela, erela);
    }

  /* Bump the counter, so that we know where to add the next set of
     relocations.  */
  *rel_countp += NUM_SHDR_ENTRIES (input_rel_hdr);
}

/* Link an input file into the linker output file.  This function
   handles all the sections and relocations of the input file at once.
   This is so that we only have to read the local symbols once, and
   don't have to keep them in memory.  */

static boolean
elf_link_input_bfd (finfo, input_bfd)
     struct elf_final_link_info *finfo;
     bfd *input_bfd;
{
  boolean (*relocate_section) PARAMS ((bfd *, struct bfd_link_info *,
				       bfd *, asection *, bfd_byte *,
				       Elf_Internal_Rela *,
				       Elf_Internal_Sym *, asection **));
  bfd *output_bfd;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *shndx_hdr;
  size_t locsymcount;
  size_t extsymoff;
  Elf_External_Sym *external_syms;
  Elf_External_Sym *esym;
  Elf_External_Sym *esymend;
  Elf_External_Sym_Shndx *shndx_buf;
  Elf_External_Sym_Shndx *shndx;
  Elf_Internal_Sym *isym;
  long *pindex;
  asection **ppsection;
  asection *o;
  struct elf_backend_data *bed;
  boolean emit_relocs;
  struct elf_link_hash_entry **sym_hashes;

  output_bfd = finfo->output_bfd;
  bed = get_elf_backend_data (output_bfd);
  relocate_section = bed->elf_backend_relocate_section;

  /* If this is a dynamic object, we don't want to do anything here:
     we don't want the local symbols, and we don't want the section
     contents.  */
  if ((input_bfd->flags & DYNAMIC) != 0)
    return true;

  emit_relocs = (finfo->info->relocateable
		 || finfo->info->emitrelocations
		 || bed->elf_backend_emit_relocs);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  if (elf_bad_symtab (input_bfd))
    {
      locsymcount = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
      extsymoff = 0;
    }
  else
    {
      locsymcount = symtab_hdr->sh_info;
      extsymoff = symtab_hdr->sh_info;
    }

  /* Read the local symbols.  */
  if (symtab_hdr->contents != NULL)
    external_syms = (Elf_External_Sym *) symtab_hdr->contents;
  else if (locsymcount == 0)
    external_syms = NULL;
  else
    {
      bfd_size_type amt = locsymcount * sizeof (Elf_External_Sym);
      external_syms = finfo->external_syms;
      if (bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	  || bfd_bread (external_syms, amt, input_bfd) != amt)
	return false;
    }

  shndx_hdr = &elf_tdata (input_bfd)->symtab_shndx_hdr;
  shndx_buf = NULL;
  if (shndx_hdr->sh_size != 0 && locsymcount != 0)
    {
      bfd_size_type amt = locsymcount * sizeof (Elf_External_Sym_Shndx);
      shndx_buf = finfo->locsym_shndx;
      if (bfd_seek (input_bfd, shndx_hdr->sh_offset, SEEK_SET) != 0
	  || bfd_bread (shndx_buf, amt, input_bfd) != amt)
	return false;
    }

  /* Swap in the local symbols and write out the ones which we know
     are going into the output file.  */
  for (esym = external_syms, esymend = esym + locsymcount,
	 isym = finfo->internal_syms, pindex = finfo->indices,
	 ppsection = finfo->sections, shndx = shndx_buf;
       esym < esymend;
       esym++, isym++, pindex++, ppsection++,
	 shndx = (shndx != NULL ? shndx + 1 : NULL))
    {
      asection *isec;
      const char *name;
      Elf_Internal_Sym osym;

      elf_swap_symbol_in (input_bfd, esym, shndx, isym);
      *pindex = -1;

      if (elf_bad_symtab (input_bfd))
	{
	  if (ELF_ST_BIND (isym->st_info) != STB_LOCAL)
	    {
	      *ppsection = NULL;
	      continue;
	    }
	}

      if (isym->st_shndx == SHN_UNDEF)
	isec = bfd_und_section_ptr;
      else if (isym->st_shndx < SHN_LORESERVE
	       || isym->st_shndx > SHN_HIRESERVE)
	{
	  isec = section_from_elf_index (input_bfd, isym->st_shndx);
	  if (isec
	      && elf_section_data (isec)->sec_info_type == ELF_INFO_TYPE_MERGE
	      && ELF_ST_TYPE (isym->st_info) != STT_SECTION)
	    isym->st_value =
	      _bfd_merged_section_offset (output_bfd, &isec,
					  elf_section_data (isec)->sec_info,
					  isym->st_value, (bfd_vma) 0);
	}
      else if (isym->st_shndx == SHN_ABS)
	isec = bfd_abs_section_ptr;
      else if (isym->st_shndx == SHN_COMMON)
	isec = bfd_com_section_ptr;
      else
	{
	  /* Who knows?  */
	  isec = NULL;
	}

      *ppsection = isec;

      /* Don't output the first, undefined, symbol.  */
      if (esym == external_syms)
	continue;

      if (ELF_ST_TYPE (isym->st_info) == STT_SECTION)
	{
	  /* We never output section symbols.  Instead, we use the
	     section symbol of the corresponding section in the output
	     file.  */
	  continue;
	}

      /* If we are stripping all symbols, we don't want to output this
	 one.  */
      if (finfo->info->strip == strip_all)
	continue;

      /* If we are discarding all local symbols, we don't want to
	 output this one.  If we are generating a relocateable output
	 file, then some of the local symbols may be required by
	 relocs; we output them below as we discover that they are
	 needed.  */
      if (finfo->info->discard == discard_all)
	continue;

      /* If this symbol is defined in a section which we are
	 discarding, we don't need to keep it, but note that
	 linker_mark is only reliable for sections that have contents.
	 For the benefit of the MIPS ELF linker, we check SEC_EXCLUDE
	 as well as linker_mark.  */
      if ((isym->st_shndx < SHN_LORESERVE || isym->st_shndx > SHN_HIRESERVE)
	  && isec != NULL
	  && ((! isec->linker_mark && (isec->flags & SEC_HAS_CONTENTS) != 0)
	      || (! finfo->info->relocateable
		  && (isec->flags & SEC_EXCLUDE) != 0)))
	continue;

      /* Get the name of the symbol.  */
      name = bfd_elf_string_from_elf_section (input_bfd, symtab_hdr->sh_link,
					      isym->st_name);
      if (name == NULL)
	return false;

      /* See if we are discarding symbols with this name.  */
      if ((finfo->info->strip == strip_some
	   && (bfd_hash_lookup (finfo->info->keep_hash, name, false, false)
	       == NULL))
	  || (((finfo->info->discard == discard_sec_merge
		&& (isec->flags & SEC_MERGE) && ! finfo->info->relocateable)
	       || finfo->info->discard == discard_l)
	      && bfd_is_local_label_name (input_bfd, name)))
	continue;

      /* If we get here, we are going to output this symbol.  */

      osym = *isym;

      /* Adjust the section index for the output file.  */
      osym.st_shndx = _bfd_elf_section_from_bfd_section (output_bfd,
							 isec->output_section);
      if (osym.st_shndx == SHN_BAD)
	return false;

      *pindex = bfd_get_symcount (output_bfd);

      /* ELF symbols in relocateable files are section relative, but
	 in executable files they are virtual addresses.  Note that
	 this code assumes that all ELF sections have an associated
	 BFD section with a reasonable value for output_offset; below
	 we assume that they also have a reasonable value for
	 output_section.  Any special sections must be set up to meet
	 these requirements.  */
      osym.st_value += isec->output_offset;
      if (! finfo->info->relocateable)
	osym.st_value += isec->output_section->vma;

      if (! elf_link_output_sym (finfo, name, &osym, isec))
	return false;
    }

  /* Relocate the contents of each section.  */
  sym_hashes = elf_sym_hashes (input_bfd);
  for (o = input_bfd->sections; o != NULL; o = o->next)
    {
      bfd_byte *contents;

      if (! o->linker_mark)
	{
	  /* This section was omitted from the link.  */
	  continue;
	}

      if ((o->flags & SEC_HAS_CONTENTS) == 0
	  || (o->_raw_size == 0 && (o->flags & SEC_RELOC) == 0))
	continue;

      if ((o->flags & SEC_LINKER_CREATED) != 0)
	{
	  /* Section was created by elf_link_create_dynamic_sections
	     or somesuch.  */
	  continue;
	}

      /* Get the contents of the section.  They have been cached by a
	 relaxation routine.  Note that o is a section in an input
	 file, so the contents field will not have been set by any of
	 the routines which work on output files.  */
      if (elf_section_data (o)->this_hdr.contents != NULL)
	contents = elf_section_data (o)->this_hdr.contents;
      else
	{
	  contents = finfo->contents;
	  if (! bfd_get_section_contents (input_bfd, o, contents,
					  (file_ptr) 0, o->_raw_size))
	    return false;
	}

      if ((o->flags & SEC_RELOC) != 0)
	{
	  Elf_Internal_Rela *internal_relocs;

	  /* Get the swapped relocs.  */
	  internal_relocs = (NAME(_bfd_elf,link_read_relocs)
			     (input_bfd, o, finfo->external_relocs,
			      finfo->internal_relocs, false));
	  if (internal_relocs == NULL
	      && o->reloc_count > 0)
	    return false;

	  /* Run through the relocs looking for any against symbols
	     from discarded sections and section symbols from
	     removed link-once sections.  Complain about relocs
	     against discarded sections.  Zero relocs against removed
	     link-once sections.  We should really complain if
	     anything in the final link tries to use it, but
	     DWARF-based exception handling might have an entry in
	     .eh_frame to describe a routine in the linkonce section,
	     and it turns out to be hard to remove the .eh_frame
	     entry too.  FIXME.  */
	  if (!finfo->info->relocateable
	      && !elf_section_ignore_discarded_relocs (o))
	    {
	      Elf_Internal_Rela *rel, *relend;

	      rel = internal_relocs;
	      relend = rel + o->reloc_count * bed->s->int_rels_per_ext_rel;
	      for ( ; rel < relend; rel++)
		{
		  unsigned long r_symndx = ELF_R_SYM (rel->r_info);

		  if (r_symndx >= locsymcount
		      || (elf_bad_symtab (input_bfd)
			  && finfo->sections[r_symndx] == NULL))
		    {
		      struct elf_link_hash_entry *h;

		      h = sym_hashes[r_symndx - extsymoff];
		      while (h->root.type == bfd_link_hash_indirect
			     || h->root.type == bfd_link_hash_warning)
			h = (struct elf_link_hash_entry *) h->root.u.i.link;

		      /* Complain if the definition comes from a
			 discarded section.  */
		      if ((h->root.type == bfd_link_hash_defined
			   || h->root.type == bfd_link_hash_defweak)
			  && elf_discarded_section (h->root.u.def.section))
			{
#if BFD_VERSION_DATE < 20031005
			  if ((o->flags & SEC_DEBUGGING) != 0)
			    {
#if BFD_VERSION_DATE > 20021005
			      (*finfo->info->callbacks->warning)
				(finfo->info,
				 _("warning: relocation against removed section; zeroing"),
				 NULL, input_bfd, o, rel->r_offset);
#endif
			      BFD_ASSERT (r_symndx != 0);
			      memset (rel, 0, sizeof (*rel));
			    }
			  else
#endif
			    {
			      if (! ((*finfo->info->callbacks->undefined_symbol)
				     (finfo->info, h->root.root.string,
				      input_bfd, o, rel->r_offset,
				      true)))
				return false;
			    }
			}
		    }
		  else
		    {
		      asection *sec = finfo->sections[r_symndx];

		      if (sec != NULL && elf_discarded_section (sec))
			{
#if BFD_VERSION_DATE < 20031005
			  if ((o->flags & SEC_DEBUGGING) != 0
			      || (sec->flags & SEC_LINK_ONCE) != 0)
			    {
#if BFD_VERSION_DATE > 20021005
			      (*finfo->info->callbacks->warning)
				(finfo->info,
				 _("warning: relocation against removed section"),
				 NULL, input_bfd, o, rel->r_offset);
#endif
			      BFD_ASSERT (r_symndx != 0);
			      rel->r_info
				= ELF_R_INFO (0, ELF_R_TYPE (rel->r_info));
			      rel->r_addend = 0;
			    }
			  else
#endif
			    {
			      boolean ok;
			      const char *msg
				= _("local symbols in discarded section %s");
			      bfd_size_type amt
				= strlen (sec->name) + strlen (msg) - 1;
			      char *buf = (char *) bfd_malloc (amt);

			      if (buf != NULL)
				sprintf (buf, msg, sec->name);
			      else
				buf = (char *) sec->name;
			      ok = (*finfo->info->callbacks
				    ->undefined_symbol) (finfo->info, buf,
							 input_bfd, o,
							 rel->r_offset,
							 true);
			      if (buf != sec->name)
				free (buf);
			      if (!ok)
				return false;
			    }
			}
		    }
		}
	    }

	  /* Relocate the section by invoking a back end routine.

	     The back end routine is responsible for adjusting the
	     section contents as necessary, and (if using Rela relocs
	     and generating a relocateable output file) adjusting the
	     reloc addend as necessary.

	     The back end routine does not have to worry about setting
	     the reloc address or the reloc symbol index.

	     The back end routine is given a pointer to the swapped in
	     internal symbols, and can access the hash table entries
	     for the external symbols via elf_sym_hashes (input_bfd).

	     When generating relocateable output, the back end routine
	     must handle STB_LOCAL/STT_SECTION symbols specially.  The
	     output symbol is going to be a section symbol
	     corresponding to the output section, which will require
	     the addend to be adjusted.  */

	  if (! (*relocate_section) (output_bfd, finfo->info,
				     input_bfd, o, contents,
				     internal_relocs,
				     finfo->internal_syms,
				     finfo->sections))
	    return false;

	  if (emit_relocs)
	    {
	      Elf_Internal_Rela *irela;
	      Elf_Internal_Rela *irelaend;
	      struct elf_link_hash_entry **rel_hash;
	      Elf_Internal_Shdr *input_rel_hdr;
	      unsigned int next_erel;
	      void (*reloc_emitter) PARAMS ((bfd *, asection *,
					     Elf_Internal_Shdr *,
					     Elf_Internal_Rela *));
	      boolean rela_normal;

	      input_rel_hdr = &elf_section_data (o)->rel_hdr;
	      rela_normal = (bed->rela_normal
			     && (input_rel_hdr->sh_entsize
				 == sizeof (Elf_External_Rela)));

	      /* Adjust the reloc addresses and symbol indices.  */

	      irela = internal_relocs;
	      irelaend = irela + o->reloc_count * bed->s->int_rels_per_ext_rel;
	      rel_hash = (elf_section_data (o->output_section)->rel_hashes
			  + elf_section_data (o->output_section)->rel_count
			  + elf_section_data (o->output_section)->rel_count2);
	      for (next_erel = 0; irela < irelaend; irela++, next_erel++)
		{
		  unsigned long r_symndx;
		  asection *sec;

		  if (next_erel == bed->s->int_rels_per_ext_rel)
		    {
		      rel_hash++;
		      next_erel = 0;
		    }

		  irela->r_offset += o->output_offset;

		  /* Relocs in an executable have to be virtual addresses.  */
		  if (!finfo->info->relocateable)
		    irela->r_offset += o->output_section->vma;

		  r_symndx = ELF_R_SYM (irela->r_info);

		  if (r_symndx == 0)
		    continue;

		  if (r_symndx >= locsymcount
		      || (elf_bad_symtab (input_bfd)
			  && finfo->sections[r_symndx] == NULL))
		    {
		      struct elf_link_hash_entry *rh;
		      unsigned long indx;

		      /* This is a reloc against a global symbol.  We
			 have not yet output all the local symbols, so
			 we do not know the symbol index of any global
			 symbol.  We set the rel_hash entry for this
			 reloc to point to the global hash table entry
			 for this symbol.  The symbol index is then
			 set at the end of elf_bfd_final_link.  */
		      indx = r_symndx - extsymoff;
		      rh = elf_sym_hashes (input_bfd)[indx];
		      while (rh->root.type == bfd_link_hash_indirect
			     || rh->root.type == bfd_link_hash_warning)
			rh = (struct elf_link_hash_entry *) rh->root.u.i.link;

		      /* Setting the index to -2 tells
			 elf_link_output_extsym that this symbol is
			 used by a reloc.  */
		      BFD_ASSERT (rh->indx < 0);
		      rh->indx = -2;

		      *rel_hash = rh;

		      continue;
		    }

		  /* This is a reloc against a local symbol.  */

		  *rel_hash = NULL;
		  isym = finfo->internal_syms + r_symndx;
		  sec = finfo->sections[r_symndx];
		  if (ELF_ST_TYPE (isym->st_info) == STT_SECTION)
		    {
		      /* I suppose the backend ought to fill in the
			 section of any STT_SECTION symbol against a
			 processor specific section.  If we have
			 discarded a section, the output_section will
			 be the absolute section.  */
		      if (bfd_is_abs_section (sec)
			  || (sec != NULL
			      && bfd_is_abs_section (sec->output_section)))
			r_symndx = 0;
		      else if (sec == NULL || sec->owner == NULL)
			{
			  bfd_set_error (bfd_error_bad_value);
			  return false;
			}
		      else
			{
			  r_symndx = sec->output_section->target_index;
			  BFD_ASSERT (r_symndx != 0);
			}

		      /* Adjust the addend according to where the
			 section winds up in the output section.  */ 
		      if (rela_normal)
			irela->r_addend += sec->output_offset;
		    }
		  else
		    {
		      if (finfo->indices[r_symndx] == -1)
			{
			  unsigned long shlink;
			  const char *name;
			  asection *osec;

			  if (finfo->info->strip == strip_all)
			    {
			      /* You can't do ld -r -s.  */
			      bfd_set_error (bfd_error_invalid_operation);
			      return false;
			    }

			  /* This symbol was skipped earlier, but
			     since it is needed by a reloc, we
			     must output it now.  */
			  shlink = symtab_hdr->sh_link;
			  name = (bfd_elf_string_from_elf_section
				  (input_bfd, shlink, isym->st_name));
			  if (name == NULL)
			    return false;

			  osec = sec->output_section;
			  isym->st_shndx =
			    _bfd_elf_section_from_bfd_section (output_bfd,
							       osec);
			  if (isym->st_shndx == SHN_BAD)
			    return false;

			  isym->st_value += sec->output_offset;
			  if (! finfo->info->relocateable)
			    isym->st_value += osec->vma;

			  finfo->indices[r_symndx]
			    = bfd_get_symcount (output_bfd);

			  if (! elf_link_output_sym (finfo, name, isym, sec))
			    return false;
			}

		      r_symndx = finfo->indices[r_symndx];
		    }

		  irela->r_info = ELF_R_INFO (r_symndx,
					      ELF_R_TYPE (irela->r_info));
		}

	      /* Swap out the relocs.  */
	      if (bed->elf_backend_emit_relocs
		  && !(finfo->info->relocateable
		       || finfo->info->emitrelocations))
		reloc_emitter = bed->elf_backend_emit_relocs;
	      else
		reloc_emitter = elf_link_output_relocs;

	      (*reloc_emitter) (output_bfd, o, input_rel_hdr, internal_relocs);

	      input_rel_hdr = elf_section_data (o)->rel_hdr2;
	      if (input_rel_hdr)
		{
		  internal_relocs += (NUM_SHDR_ENTRIES (input_rel_hdr)
				      * bed->s->int_rels_per_ext_rel);
		  (*reloc_emitter) (output_bfd, o, input_rel_hdr,
				    internal_relocs);
		}

	    }
	}

      /* Write out the modified section contents.  */
      if (bed->elf_backend_write_section
	  && (*bed->elf_backend_write_section) (output_bfd, o, contents))
	{
	  /* Section written out.  */
	}
      else switch (elf_section_data (o)->sec_info_type)
	{
	case ELF_INFO_TYPE_STABS:
	  if (! (_bfd_write_section_stabs
		 (output_bfd,
		  &elf_hash_table (finfo->info)->stab_info,
		  o, &elf_section_data (o)->sec_info, contents)))
	    return false;
	  break;
	case ELF_INFO_TYPE_MERGE:
	  if (! (_bfd_write_merged_section
		 (output_bfd, o, elf_section_data (o)->sec_info)))
	    return false;
	  break;
	case ELF_INFO_TYPE_EH_FRAME:
	  {
	    asection *ehdrsec;

	    ehdrsec
	      = bfd_get_section_by_name (elf_hash_table (finfo->info)->dynobj,
					 ".eh_frame_hdr");
	    if (! (_bfd_elf_write_section_eh_frame (output_bfd, o, ehdrsec,
						    contents)))
	      return false;
	  }
	  break;
	default:
	  {
	    bfd_size_type sec_size;

	    sec_size = (o->_cooked_size != 0 ? o->_cooked_size : o->_raw_size);
	    if (! (o->flags & SEC_EXCLUDE)
		&& ! bfd_set_section_contents (output_bfd, o->output_section,
					       contents,
					       (file_ptr) o->output_offset,
					       sec_size))
	      return false;
	  }
	  break;
	}
    }

  return true;
}

/* Generate a reloc when linking an ELF file.  This is a reloc
   requested by the linker, and does come from any input file.  This
   is used to build constructor and destructor tables when linking
   with -Ur.  */

static boolean
elf_reloc_link_order (output_bfd, info, output_section, link_order)
     bfd *output_bfd;
     struct bfd_link_info *info;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  reloc_howto_type *howto;
  long indx;
  bfd_vma offset;
  bfd_vma addend;
  struct elf_link_hash_entry **rel_hash_ptr;
  Elf_Internal_Shdr *rel_hdr;
  struct elf_backend_data *bed = get_elf_backend_data (output_bfd);

  howto = bfd_reloc_type_lookup (output_bfd, link_order->u.reloc.p->reloc);
  if (howto == NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  addend = link_order->u.reloc.p->addend;

  /* Figure out the symbol index.  */
  rel_hash_ptr = (elf_section_data (output_section)->rel_hashes
		  + elf_section_data (output_section)->rel_count
		  + elf_section_data (output_section)->rel_count2);
  if (link_order->type == bfd_section_reloc_link_order)
    {
      indx = link_order->u.reloc.p->u.section->target_index;
      BFD_ASSERT (indx != 0);
      *rel_hash_ptr = NULL;
    }
  else
    {
      struct elf_link_hash_entry *h;

      /* Treat a reloc against a defined symbol as though it were
	 actually against the section.  */
      h = ((struct elf_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (output_bfd, info,
					 link_order->u.reloc.p->u.name,
					 false, false, true));
      if (h != NULL
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak))
	{
	  asection *section;

	  section = h->root.u.def.section;
	  indx = section->output_section->target_index;
	  *rel_hash_ptr = NULL;
	  /* It seems that we ought to add the symbol value to the
	     addend here, but in practice it has already been added
	     because it was passed to constructor_callback.  */
	  addend += section->output_section->vma + section->output_offset;
	}
      else if (h != NULL)
	{
	  /* Setting the index to -2 tells elf_link_output_extsym that
	     this symbol is used by a reloc.  */
	  h->indx = -2;
	  *rel_hash_ptr = h;
	  indx = 0;
	}
      else
	{
	  if (! ((*info->callbacks->unattached_reloc)
		 (info, link_order->u.reloc.p->u.name, (bfd *) NULL,
		  (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  indx = 0;
	}
    }

  /* If this is an inplace reloc, we must write the addend into the
     object file.  */
  if (howto->partial_inplace && addend != 0)
    {
      bfd_size_type size;
      bfd_reloc_status_type rstat;
      bfd_byte *buf;
      boolean ok;
      const char *sym_name;

      size = bfd_get_reloc_size (howto);
      buf = (bfd_byte *) bfd_zmalloc (size);
      if (buf == (bfd_byte *) NULL)
	return false;
      rstat = _bfd_relocate_contents (howto, output_bfd, (bfd_vma) addend, buf);
      switch (rstat)
	{
	case bfd_reloc_ok:
	  break;

	default:
	case bfd_reloc_outofrange:
	  abort ();

	case bfd_reloc_overflow:
	  if (link_order->type == bfd_section_reloc_link_order)
	    sym_name = bfd_section_name (output_bfd,
					 link_order->u.reloc.p->u.section);
	  else
	    sym_name = link_order->u.reloc.p->u.name;
	  if (! ((*info->callbacks->reloc_overflow)
		 (info, sym_name, howto->name, addend,
		  (bfd *) NULL, (asection *) NULL, (bfd_vma) 0)))
	    {
	      free (buf);
	      return false;
	    }
	  break;
	}
      ok = bfd_set_section_contents (output_bfd, output_section, (PTR) buf,
				     (file_ptr) link_order->offset, size);
      free (buf);
      if (! ok)
	return false;
    }

  /* The address of a reloc is relative to the section in a
     relocateable file, and is a virtual address in an executable
     file.  */
  offset = link_order->offset;
  if (! info->relocateable)
    offset += output_section->vma;

  rel_hdr = &elf_section_data (output_section)->rel_hdr;

  if (rel_hdr->sh_type == SHT_REL)
    {
      bfd_size_type size;
      Elf_Internal_Rel *irel;
      Elf_External_Rel *erel;
      unsigned int i;

      size = bed->s->int_rels_per_ext_rel * sizeof (Elf_Internal_Rel);
      irel = (Elf_Internal_Rel *) bfd_zmalloc (size);
      if (irel == NULL)
	return false;

      for (i = 0; i < bed->s->int_rels_per_ext_rel; i++)
	irel[i].r_offset = offset;
      irel[0].r_info = ELF_R_INFO (indx, howto->type);

      erel = ((Elf_External_Rel *) rel_hdr->contents
	      + elf_section_data (output_section)->rel_count);

      if (bed->s->swap_reloc_out)
	(*bed->s->swap_reloc_out) (output_bfd, irel, (bfd_byte *) erel);
      else
	elf_swap_reloc_out (output_bfd, irel, erel);

      free (irel);
    }
  else
    {
      bfd_size_type size;
      Elf_Internal_Rela *irela;
      Elf_External_Rela *erela;
      unsigned int i;

      size = bed->s->int_rels_per_ext_rel * sizeof (Elf_Internal_Rela);
      irela = (Elf_Internal_Rela *) bfd_zmalloc (size);
      if (irela == NULL)
	return false;

      for (i = 0; i < bed->s->int_rels_per_ext_rel; i++)
	irela[i].r_offset = offset;
      irela[0].r_info = ELF_R_INFO (indx, howto->type);
      irela[0].r_addend = addend;

      erela = ((Elf_External_Rela *) rel_hdr->contents
	       + elf_section_data (output_section)->rel_count);

      if (bed->s->swap_reloca_out)
	(*bed->s->swap_reloca_out) (output_bfd, irela, (bfd_byte *) erela);
      else
	elf_swap_reloca_out (output_bfd, irela, erela);
    }

  ++elf_section_data (output_section)->rel_count;

  return true;
}

/* Allocate a pointer to live in a linker created section.  */

boolean
elf_create_pointer_linker_section (abfd, info, lsect, h, rel)
     bfd *abfd;
     struct bfd_link_info *info;
     elf_linker_section_t *lsect;
     struct elf_link_hash_entry *h;
     const Elf_Internal_Rela *rel;
{
  elf_linker_section_pointers_t **ptr_linker_section_ptr = NULL;
  elf_linker_section_pointers_t *linker_section_ptr;
  unsigned long r_symndx = ELF_R_SYM (rel->r_info);
  bfd_size_type amt;

  BFD_ASSERT (lsect != NULL);

  /* Is this a global symbol?  */
  if (h != NULL)
    {
      /* Has this symbol already been allocated?  If so, our work is done.  */
      if (_bfd_elf_find_pointer_linker_section (h->linker_section_pointer,
						rel->r_addend,
						lsect->which))
	return true;

      ptr_linker_section_ptr = &h->linker_section_pointer;
      /* Make sure this symbol is output as a dynamic symbol.  */
      if (h->dynindx == -1)
	{
	  if (! elf_link_record_dynamic_symbol (info, h))
	    return false;
	}

      if (lsect->rel_section)
	lsect->rel_section->_raw_size += sizeof (Elf_External_Rela);
    }
  else
    {
      /* Allocation of a pointer to a local symbol.  */
      elf_linker_section_pointers_t **ptr = elf_local_ptr_offsets (abfd);

      /* Allocate a table to hold the local symbols if first time.  */
      if (!ptr)
	{
	  unsigned int num_symbols = elf_tdata (abfd)->symtab_hdr.sh_info;
	  register unsigned int i;

	  amt = num_symbols;
	  amt *= sizeof (elf_linker_section_pointers_t *);
	  ptr = (elf_linker_section_pointers_t **) bfd_alloc (abfd, amt);

	  if (!ptr)
	    return false;

	  elf_local_ptr_offsets (abfd) = ptr;
	  for (i = 0; i < num_symbols; i++)
	    ptr[i] = (elf_linker_section_pointers_t *) 0;
	}

      /* Has this symbol already been allocated?  If so, our work is done.  */
      if (_bfd_elf_find_pointer_linker_section (ptr[r_symndx],
						rel->r_addend,
						lsect->which))
	return true;

      ptr_linker_section_ptr = &ptr[r_symndx];

      if (info->shared)
	{
	  /* If we are generating a shared object, we need to
	     output a R_<xxx>_RELATIVE reloc so that the
	     dynamic linker can adjust this GOT entry.  */
	  BFD_ASSERT (lsect->rel_section != NULL);
	  lsect->rel_section->_raw_size += sizeof (Elf_External_Rela);
	}
    }

  /* Allocate space for a pointer in the linker section, and allocate
     a new pointer record from internal memory.  */
  BFD_ASSERT (ptr_linker_section_ptr != NULL);
  amt = sizeof (elf_linker_section_pointers_t);
  linker_section_ptr = (elf_linker_section_pointers_t *) bfd_alloc (abfd, amt);

  if (!linker_section_ptr)
    return false;

  linker_section_ptr->next = *ptr_linker_section_ptr;
  linker_section_ptr->addend = rel->r_addend;
  linker_section_ptr->which = lsect->which;
  linker_section_ptr->written_address_p = false;
  *ptr_linker_section_ptr = linker_section_ptr;

#if 0
  if (lsect->hole_size && lsect->hole_offset < lsect->max_hole_offset)
    {
      linker_section_ptr->offset = (lsect->section->_raw_size
				    - lsect->hole_size + (ARCH_SIZE / 8));
      lsect->hole_offset += ARCH_SIZE / 8;
      lsect->sym_offset  += ARCH_SIZE / 8;
      if (lsect->sym_hash)
	{
	  /* Bump up symbol value if needed.  */
	  lsect->sym_hash->root.u.def.value += ARCH_SIZE / 8;
#ifdef DEBUG
	  fprintf (stderr, "Bump up %s by %ld, current value = %ld\n",
		   lsect->sym_hash->root.root.string,
		   (long) ARCH_SIZE / 8,
		   (long) lsect->sym_hash->root.u.def.value);
#endif
	}
    }
  else
#endif
    linker_section_ptr->offset = lsect->section->_raw_size;

  lsect->section->_raw_size += ARCH_SIZE / 8;

#ifdef DEBUG
  fprintf (stderr,
	   "Create pointer in linker section %s, offset = %ld, section size = %ld\n",
	   lsect->name, (long) linker_section_ptr->offset,
	   (long) lsect->section->_raw_size);
#endif

  return true;
}

#if ARCH_SIZE==64
#define bfd_put_ptr(BFD,VAL,ADDR) bfd_put_64 (BFD, VAL, ADDR)
#endif
#if ARCH_SIZE==32
#define bfd_put_ptr(BFD,VAL,ADDR) bfd_put_32 (BFD, VAL, ADDR)
#endif

/* Fill in the address for a pointer generated in a linker section.  */

bfd_vma
elf_finish_pointer_linker_section (output_bfd, input_bfd, info, lsect, h,
				   relocation, rel, relative_reloc)
     bfd *output_bfd;
     bfd *input_bfd;
     struct bfd_link_info *info;
     elf_linker_section_t *lsect;
     struct elf_link_hash_entry *h;
     bfd_vma relocation;
     const Elf_Internal_Rela *rel;
     int relative_reloc;
{
  elf_linker_section_pointers_t *linker_section_ptr;

  BFD_ASSERT (lsect != NULL);

  if (h != NULL)
    {
      /* Handle global symbol.  */
      linker_section_ptr = (_bfd_elf_find_pointer_linker_section
			    (h->linker_section_pointer,
			     rel->r_addend,
			     lsect->which));

      BFD_ASSERT (linker_section_ptr != NULL);

      if (! elf_hash_table (info)->dynamic_sections_created
	  || (info->shared
	      && info->symbolic
	      && (h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
	{
	  /* This is actually a static link, or it is a
	     -Bsymbolic link and the symbol is defined
	     locally.  We must initialize this entry in the
	     global section.

	     When doing a dynamic link, we create a .rela.<xxx>
	     relocation entry to initialize the value.  This
	     is done in the finish_dynamic_symbol routine.  */
	  if (!linker_section_ptr->written_address_p)
	    {
	      linker_section_ptr->written_address_p = true;
	      bfd_put_ptr (output_bfd,
			   relocation + linker_section_ptr->addend,
			   (lsect->section->contents
			    + linker_section_ptr->offset));
	    }
	}
    }
  else
    {
      /* Handle local symbol.  */
      unsigned long r_symndx = ELF_R_SYM (rel->r_info);
      BFD_ASSERT (elf_local_ptr_offsets (input_bfd) != NULL);
      BFD_ASSERT (elf_local_ptr_offsets (input_bfd)[r_symndx] != NULL);
      linker_section_ptr = (_bfd_elf_find_pointer_linker_section
			    (elf_local_ptr_offsets (input_bfd)[r_symndx],
			     rel->r_addend,
			     lsect->which));

      BFD_ASSERT (linker_section_ptr != NULL);

      /* Write out pointer if it hasn't been rewritten out before.  */
      if (!linker_section_ptr->written_address_p)
	{
	  linker_section_ptr->written_address_p = true;
	  bfd_put_ptr (output_bfd, relocation + linker_section_ptr->addend,
		       lsect->section->contents + linker_section_ptr->offset);

	  if (info->shared)
	    {
	      asection *srel = lsect->rel_section;
	      Elf_Internal_Rela *outrel;
	      Elf_External_Rela *erel;
	      struct elf_backend_data *bed = get_elf_backend_data (output_bfd);
	      unsigned int i;
	      bfd_size_type amt;

	      amt = sizeof (Elf_Internal_Rela) * bed->s->int_rels_per_ext_rel;
	      outrel = (Elf_Internal_Rela *) bfd_zmalloc (amt);
	      if (outrel == NULL)
		{
		  (*_bfd_error_handler) (_("Error: out of memory"));
		  return 0;
		}

	      /* We need to generate a relative reloc for the dynamic
		 linker.  */
	      if (!srel)
		{
		  srel = bfd_get_section_by_name (elf_hash_table (info)->dynobj,
						  lsect->rel_name);
		  lsect->rel_section = srel;
		}

	      BFD_ASSERT (srel != NULL);

	      for (i = 0; i < bed->s->int_rels_per_ext_rel; i++)
		outrel[i].r_offset = (lsect->section->output_section->vma
				      + lsect->section->output_offset
				      + linker_section_ptr->offset);
	      outrel[0].r_info = ELF_R_INFO (0, relative_reloc);
	      outrel[0].r_addend = 0;
	      erel = (Elf_External_Rela *) lsect->section->contents;
	      erel += elf_section_data (lsect->section)->rel_count;
	      elf_swap_reloca_out (output_bfd, outrel, erel);
	      ++elf_section_data (lsect->section)->rel_count;

	      free (outrel);
	    }
	}
    }

  relocation = (lsect->section->output_offset
		+ linker_section_ptr->offset
		- lsect->hole_offset
		- lsect->sym_offset);

#ifdef DEBUG
  fprintf (stderr,
	   "Finish pointer in linker section %s, offset = %ld (0x%lx)\n",
	   lsect->name, (long) relocation, (long) relocation);
#endif

  /* Subtract out the addend, because it will get added back in by the normal
     processing.  */
  return relocation - linker_section_ptr->addend;
}

/* Garbage collect unused sections.  */

static boolean elf_gc_mark
  PARAMS ((struct bfd_link_info *info, asection *sec,
	   asection * (*gc_mark_hook)
	     PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Rela *,
		      struct elf_link_hash_entry *, Elf_Internal_Sym *))));

static boolean elf_gc_sweep
  PARAMS ((struct bfd_link_info *info,
	   boolean (*gc_sweep_hook)
	     PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *o,
		      const Elf_Internal_Rela *relocs))));

static boolean elf_gc_sweep_symbol
  PARAMS ((struct elf_link_hash_entry *h, PTR idxptr));

static boolean elf_gc_allocate_got_offsets
  PARAMS ((struct elf_link_hash_entry *h, PTR offarg));

static boolean elf_gc_propagate_vtable_entries_used
  PARAMS ((struct elf_link_hash_entry *h, PTR dummy));

static boolean elf_gc_smash_unused_vtentry_relocs
  PARAMS ((struct elf_link_hash_entry *h, PTR dummy));

/* The mark phase of garbage collection.  For a given section, mark
   it and any sections in this section's group, and all the sections
   which define symbols to which it refers.  */

static boolean
elf_gc_mark (info, sec, gc_mark_hook)
     struct bfd_link_info *info;
     asection *sec;
     asection * (*gc_mark_hook)
       PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Rela *,
		struct elf_link_hash_entry *, Elf_Internal_Sym *));
{
  boolean ret;
  asection *group_sec;

  sec->gc_mark = 1;

  /* Mark all the sections in the group.  */
  group_sec = elf_section_data (sec)->next_in_group;
  if (group_sec && !group_sec->gc_mark)
    if (!elf_gc_mark (info, group_sec, gc_mark_hook))
      return false;

  /* Look through the section relocs.  */
  ret = true;
  if ((sec->flags & SEC_RELOC) != 0 && sec->reloc_count > 0)
    {
      Elf_Internal_Rela *relstart, *rel, *relend;
      Elf_Internal_Shdr *symtab_hdr;
      Elf_Internal_Shdr *shndx_hdr;
      struct elf_link_hash_entry **sym_hashes;
      size_t nlocsyms;
      size_t extsymoff;
      Elf_External_Sym *locsyms, *freesyms = NULL;
      Elf_External_Sym_Shndx *locsym_shndx;
      bfd *input_bfd = sec->owner;
      struct elf_backend_data *bed = get_elf_backend_data (input_bfd);

      /* GCFIXME: how to arrange so that relocs and symbols are not
	 reread continually?  */

      symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
      sym_hashes = elf_sym_hashes (input_bfd);

      /* Read the local symbols.  */
      if (elf_bad_symtab (input_bfd))
	{
	  nlocsyms = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
	  extsymoff = 0;
	}
      else
	extsymoff = nlocsyms = symtab_hdr->sh_info;

      if (symtab_hdr->contents)
	locsyms = (Elf_External_Sym *) symtab_hdr->contents;
      else if (nlocsyms == 0)
	locsyms = NULL;
      else
	{
	  bfd_size_type amt = nlocsyms * sizeof (Elf_External_Sym);
	  locsyms = freesyms = bfd_malloc (amt);
	  if (freesyms == NULL
	      || bfd_seek (input_bfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	      || bfd_bread (locsyms, amt, input_bfd) != amt)
	    {
	      ret = false;
	      goto out1;
	    }
	}

      shndx_hdr = &elf_tdata (input_bfd)->symtab_shndx_hdr;
      locsym_shndx = NULL;
      if (shndx_hdr->sh_size != 0 && nlocsyms != 0)
	{
	  bfd_size_type amt = nlocsyms * sizeof (Elf_External_Sym_Shndx);
	  locsym_shndx = (Elf_External_Sym_Shndx *) bfd_malloc (amt);
	  if (bfd_seek (input_bfd, shndx_hdr->sh_offset, SEEK_SET) != 0
	      || bfd_bread (locsym_shndx, amt, input_bfd) != amt)
	    return false;
	}

      /* Read the relocations.  */
      relstart = (NAME(_bfd_elf,link_read_relocs)
		  (sec->owner, sec, NULL, (Elf_Internal_Rela *) NULL,
		   info->keep_memory));
      if (relstart == NULL)
	{
	  ret = false;
	  goto out1;
	}
      relend = relstart + sec->reloc_count * bed->s->int_rels_per_ext_rel;

      for (rel = relstart; rel < relend; rel++)
	{
	  unsigned long r_symndx;
	  asection *rsec;
	  struct elf_link_hash_entry *h;
	  Elf_Internal_Sym s;

	  r_symndx = ELF_R_SYM (rel->r_info);
	  if (r_symndx == 0)
	    continue;

	  if (elf_bad_symtab (sec->owner))
	    {
	      elf_swap_symbol_in (input_bfd,
				  locsyms + r_symndx,
				  locsym_shndx + (locsym_shndx ? r_symndx : 0),
				  &s);
	      if (ELF_ST_BIND (s.st_info) == STB_LOCAL)
		rsec = (*gc_mark_hook) (sec->owner, info, rel, NULL, &s);
	      else
		{
		  h = sym_hashes[r_symndx - extsymoff];
		  rsec = (*gc_mark_hook) (sec->owner, info, rel, h, NULL);
		}
	    }
	  else if (r_symndx >= nlocsyms)
	    {
	      h = sym_hashes[r_symndx - extsymoff];
	      rsec = (*gc_mark_hook) (sec->owner, info, rel, h, NULL);
	    }
	  else
	    {
	      elf_swap_symbol_in (input_bfd,
				  locsyms + r_symndx,
				  locsym_shndx + (locsym_shndx ? r_symndx : 0),
				  &s);
	      rsec = (*gc_mark_hook) (sec->owner, info, rel, NULL, &s);
	    }

	  if (rsec && !rsec->gc_mark)
	    {
	      if (bfd_get_flavour (rsec->owner) != bfd_target_elf_flavour)
		rsec->gc_mark = 1;
	      else if (!elf_gc_mark (info, rsec, gc_mark_hook))
		{
		  ret = false;
		  goto out2;
		}
	    }
	}

    out2:
      if (!info->keep_memory)
	free (relstart);
    out1:
      if (freesyms)
	free (freesyms);
    }

  return ret;
}

/* The sweep phase of garbage collection.  Remove all garbage sections.  */

static boolean
elf_gc_sweep (info, gc_sweep_hook)
     struct bfd_link_info *info;
     boolean (*gc_sweep_hook)
       PARAMS ((bfd *abfd, struct bfd_link_info *info, asection *o,
		const Elf_Internal_Rela *relocs));
{
  bfd *sub;

  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      asection *o;

      if (bfd_get_flavour (sub) != bfd_target_elf_flavour)
	continue;

      for (o = sub->sections; o != NULL; o = o->next)
	{
	  /* Keep special sections.  Keep .debug sections.  */
	  if ((o->flags & SEC_LINKER_CREATED)
	      || (o->flags & SEC_DEBUGGING))
	    o->gc_mark = 1;

	  if (o->gc_mark)
	    continue;

	  /* Skip sweeping sections already excluded.  */
	  if (o->flags & SEC_EXCLUDE)
	    continue;

	  /* Since this is early in the link process, it is simple
	     to remove a section from the output.  */
	  o->flags |= SEC_EXCLUDE;

	  /* But we also have to update some of the relocation
	     info we collected before.  */
	  if (gc_sweep_hook
	      && (o->flags & SEC_RELOC) && o->reloc_count > 0)
	    {
	      Elf_Internal_Rela *internal_relocs;
	      boolean r;

	      internal_relocs = (NAME(_bfd_elf,link_read_relocs)
				 (o->owner, o, NULL, NULL, info->keep_memory));
	      if (internal_relocs == NULL)
		return false;

	      r = (*gc_sweep_hook) (o->owner, info, o, internal_relocs);

	      if (!info->keep_memory)
		free (internal_relocs);

	      if (!r)
		return false;
	    }
	}
    }

  /* Remove the symbols that were in the swept sections from the dynamic
     symbol table.  GCFIXME: Anyone know how to get them out of the
     static symbol table as well?  */
  {
    int i = 0;

    elf_link_hash_traverse (elf_hash_table (info),
			    elf_gc_sweep_symbol,
			    (PTR) &i);

    elf_hash_table (info)->dynsymcount = i;
  }

  return true;
}

/* Sweep symbols in swept sections.  Called via elf_link_hash_traverse.  */

static boolean
elf_gc_sweep_symbol (h, idxptr)
     struct elf_link_hash_entry *h;
     PTR idxptr;
{
  int *idx = (int *) idxptr;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->dynindx != -1
      && ((h->root.type != bfd_link_hash_defined
	   && h->root.type != bfd_link_hash_defweak)
	  || h->root.u.def.section->gc_mark))
    h->dynindx = (*idx)++;

  return true;
}

/* Propogate collected vtable information.  This is called through
   elf_link_hash_traverse.  */

static boolean
elf_gc_propagate_vtable_entries_used (h, okp)
     struct elf_link_hash_entry *h;
     PTR okp;
{
  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Those that are not vtables.  */
  if (h->vtable_parent == NULL)
    return true;

  /* Those vtables that do not have parents, we cannot merge.  */
  if (h->vtable_parent == (struct elf_link_hash_entry *) -1)
    return true;

  /* If we've already been done, exit.  */
  if (h->vtable_entries_used && h->vtable_entries_used[-1])
    return true;

  /* Make sure the parent's table is up to date.  */
  elf_gc_propagate_vtable_entries_used (h->vtable_parent, okp);

  if (h->vtable_entries_used == NULL)
    {
      /* None of this table's entries were referenced.  Re-use the
	 parent's table.  */
      h->vtable_entries_used = h->vtable_parent->vtable_entries_used;
      h->vtable_entries_size = h->vtable_parent->vtable_entries_size;
    }
  else
    {
      size_t n;
      boolean *cu, *pu;

      /* Or the parent's entries into ours.  */
      cu = h->vtable_entries_used;
      cu[-1] = true;
      pu = h->vtable_parent->vtable_entries_used;
      if (pu != NULL)
	{
	  asection *sec = h->root.u.def.section;
	  struct elf_backend_data *bed = get_elf_backend_data (sec->owner);
	  int file_align = bed->s->file_align;

	  n = h->vtable_parent->vtable_entries_size / file_align;
	  while (n--)
	    {
	      if (*pu)
		*cu = true;
	      pu++;
	      cu++;
	    }
	}
    }

  return true;
}

static boolean
elf_gc_smash_unused_vtentry_relocs (h, okp)
     struct elf_link_hash_entry *h;
     PTR okp;
{
  asection *sec;
  bfd_vma hstart, hend;
  Elf_Internal_Rela *relstart, *relend, *rel;
  struct elf_backend_data *bed;
  int file_align;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Take care of both those symbols that do not describe vtables as
     well as those that are not loaded.  */
  if (h->vtable_parent == NULL)
    return true;

  BFD_ASSERT (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak);

  sec = h->root.u.def.section;
  hstart = h->root.u.def.value;
  hend = hstart + h->size;

  relstart = (NAME(_bfd_elf,link_read_relocs)
	      (sec->owner, sec, NULL, (Elf_Internal_Rela *) NULL, true));
  if (!relstart)
    return *(boolean *) okp = false;
  bed = get_elf_backend_data (sec->owner);
  file_align = bed->s->file_align;

  relend = relstart + sec->reloc_count * bed->s->int_rels_per_ext_rel;

  for (rel = relstart; rel < relend; ++rel)
    if (rel->r_offset >= hstart && rel->r_offset < hend)
      {
	/* If the entry is in use, do nothing.  */
	if (h->vtable_entries_used
	    && (rel->r_offset - hstart) < h->vtable_entries_size)
	  {
	    bfd_vma entry = (rel->r_offset - hstart) / file_align;
	    if (h->vtable_entries_used[entry])
	      continue;
	  }
	/* Otherwise, kill it.  */
	rel->r_offset = rel->r_info = rel->r_addend = 0;
      }

  return true;
}

/* Do mark and sweep of unused sections.  */

boolean
elf_gc_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  boolean ok = true;
  bfd *sub;
  asection * (*gc_mark_hook)
    PARAMS ((bfd *, struct bfd_link_info *, Elf_Internal_Rela *,
	     struct elf_link_hash_entry *h, Elf_Internal_Sym *));

  if (!get_elf_backend_data (abfd)->can_gc_sections
      || info->relocateable || info->emitrelocations
      || elf_hash_table (info)->dynamic_sections_created)
    return true;

  /* Apply transitive closure to the vtable entry usage info.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_gc_propagate_vtable_entries_used,
			  (PTR) &ok);
  if (!ok)
    return false;

  /* Kill the vtable relocations that were not used.  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_gc_smash_unused_vtentry_relocs,
			  (PTR) &ok);
  if (!ok)
    return false;

  /* Grovel through relocs to find out who stays ...  */

  gc_mark_hook = get_elf_backend_data (abfd)->gc_mark_hook;
  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      asection *o;

      if (bfd_get_flavour (sub) != bfd_target_elf_flavour)
	continue;

      for (o = sub->sections; o != NULL; o = o->next)
	{
	  if (o->flags & SEC_KEEP)
	    if (!elf_gc_mark (info, o, gc_mark_hook))
	      return false;
	}
    }

  /* ... and mark SEC_EXCLUDE for those that go.  */
  if (!elf_gc_sweep (info, get_elf_backend_data (abfd)->gc_sweep_hook))
    return false;

  return true;
}

/* Called from check_relocs to record the existance of a VTINHERIT reloc.  */

boolean
elf_gc_record_vtinherit (abfd, sec, h, offset)
     bfd *abfd;
     asection *sec;
     struct elf_link_hash_entry *h;
     bfd_vma offset;
{
  struct elf_link_hash_entry **sym_hashes, **sym_hashes_end;
  struct elf_link_hash_entry **search, *child;
  bfd_size_type extsymcount;

  /* The sh_info field of the symtab header tells us where the
     external symbols start.  We don't care about the local symbols at
     this point.  */
  extsymcount = elf_tdata (abfd)->symtab_hdr.sh_size/sizeof (Elf_External_Sym);
  if (!elf_bad_symtab (abfd))
    extsymcount -= elf_tdata (abfd)->symtab_hdr.sh_info;

  sym_hashes = elf_sym_hashes (abfd);
  sym_hashes_end = sym_hashes + extsymcount;

  /* Hunt down the child symbol, which is in this section at the same
     offset as the relocation.  */
  for (search = sym_hashes; search != sym_hashes_end; ++search)
    {
      if ((child = *search) != NULL
	  && (child->root.type == bfd_link_hash_defined
	      || child->root.type == bfd_link_hash_defweak)
	  && child->root.u.def.section == sec
	  && child->root.u.def.value == offset)
	goto win;
    }

  (*_bfd_error_handler) ("%s: %s+%lu: No symbol found for INHERIT",
			 bfd_archive_filename (abfd), sec->name,
			 (unsigned long) offset);
  bfd_set_error (bfd_error_invalid_operation);
  return false;

 win:
  if (!h)
    {
      /* This *should* only be the absolute section.  It could potentially
	 be that someone has defined a non-global vtable though, which
	 would be bad.  It isn't worth paging in the local symbols to be
	 sure though; that case should simply be handled by the assembler.  */

      child->vtable_parent = (struct elf_link_hash_entry *) -1;
    }
  else
    child->vtable_parent = h;

  return true;
}

/* Called from check_relocs to record the existance of a VTENTRY reloc.  */

boolean
elf_gc_record_vtentry (abfd, sec, h, addend)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     struct elf_link_hash_entry *h;
     bfd_vma addend;
{
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  int file_align = bed->s->file_align;

  if (addend >= h->vtable_entries_size)
    {
      size_t size, bytes;
      boolean *ptr = h->vtable_entries_used;

      /* While the symbol is undefined, we have to be prepared to handle
	 a zero size.  */
      if (h->root.type == bfd_link_hash_undefined)
	size = addend;
      else
	{
	  size = h->size;
	  if (size < addend)
	    {
	      /* Oops!  We've got a reference past the defined end of
		 the table.  This is probably a bug -- shall we warn?  */
	      size = addend;
	    }
	}

      /* Allocate one extra entry for use as a "done" flag for the
	 consolidation pass.  */
      bytes = (size / file_align + 1) * sizeof (boolean);

      if (ptr)
	{
	  ptr = bfd_realloc (ptr - 1, (bfd_size_type) bytes);

	  if (ptr != NULL)
	    {
	      size_t oldbytes;

	      oldbytes = ((h->vtable_entries_size / file_align + 1)
			  * sizeof (boolean));
	      memset (((char *) ptr) + oldbytes, 0, bytes - oldbytes);
	    }
	}
      else
	ptr = bfd_zmalloc ((bfd_size_type) bytes);

      if (ptr == NULL)
	return false;

      /* And arrange for that done flag to be at index -1.  */
      h->vtable_entries_used = ptr + 1;
      h->vtable_entries_size = size;
    }

  h->vtable_entries_used[addend / file_align] = true;

  return true;
}

/* And an accompanying bit to work out final got entry offsets once
   we're done.  Should be called from final_link.  */

boolean
elf_gc_common_finalize_got_offsets (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  bfd *i;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  bfd_vma gotoff;

  /* The GOT offset is relative to the .got section, but the GOT header is
     put into the .got.plt section, if the backend uses it.  */
  if (bed->want_got_plt)
    gotoff = 0;
  else
    gotoff = bed->got_header_size;

  /* Do the local .got entries first.  */
  for (i = info->input_bfds; i; i = i->link_next)
    {
      bfd_signed_vma *local_got;
      bfd_size_type j, locsymcount;
      Elf_Internal_Shdr *symtab_hdr;

      if (bfd_get_flavour (i) != bfd_target_elf_flavour)
	continue;

      local_got = elf_local_got_refcounts (i);
      if (!local_got)
	continue;

      symtab_hdr = &elf_tdata (i)->symtab_hdr;
      if (elf_bad_symtab (i))
	locsymcount = symtab_hdr->sh_size / sizeof (Elf_External_Sym);
      else
	locsymcount = symtab_hdr->sh_info;

      for (j = 0; j < locsymcount; ++j)
	{
	  if (local_got[j] > 0)
	    {
	      local_got[j] = gotoff;
	      gotoff += ARCH_SIZE / 8;
	    }
	  else
	    local_got[j] = (bfd_vma) -1;
	}
    }

  /* Then the global .got entries.  .plt refcounts are handled by
     adjust_dynamic_symbol  */
  elf_link_hash_traverse (elf_hash_table (info),
			  elf_gc_allocate_got_offsets,
			  (PTR) &gotoff);
  return true;
}

/* We need a special top-level link routine to convert got reference counts
   to real got offsets.  */

static boolean
elf_gc_allocate_got_offsets (h, offarg)
     struct elf_link_hash_entry *h;
     PTR offarg;
{
  bfd_vma *off = (bfd_vma *) offarg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->got.refcount > 0)
    {
      h->got.offset = off[0];
      off[0] += ARCH_SIZE / 8;
    }
  else
    h->got.offset = (bfd_vma) -1;

  return true;
}

/* Many folk need no more in the way of final link than this, once
   got entry reference counting is enabled.  */

boolean
elf_gc_common_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  if (!elf_gc_common_finalize_got_offsets (abfd, info))
    return false;

  /* Invoke the regular ELF backend linker to do all the work.  */
  return elf_bfd_final_link (abfd, info);
}

/* This function will be called though elf_link_hash_traverse to store
   all hash value of the exported symbols in an array.  */

static boolean
elf_collect_hash_codes (h, data)
     struct elf_link_hash_entry *h;
     PTR data;
{
  unsigned long **valuep = (unsigned long **) data;
  const char *name;
  char *p;
  unsigned long ha;
  char *alc = NULL;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  /* Ignore indirect symbols.  These are added by the versioning code.  */
  if (h->dynindx == -1)
    return true;

  name = h->root.root.string;
  p = strchr (name, ELF_VER_CHR);
  if (p != NULL)
    {
      alc = bfd_malloc ((bfd_size_type) (p - name + 1));
      memcpy (alc, name, (size_t) (p - name));
      alc[p - name] = '\0';
      name = alc;
    }

  /* Compute the hash value.  */
  ha = bfd_elf_hash (name);

  /* Store the found hash value in the array given as the argument.  */
  *(*valuep)++ = ha;

  /* And store it in the struct so that we can put it in the hash table
     later.  */
  h->elf_hash_value = ha;

  if (alc != NULL)
    free (alc);

  return true;
}

boolean
elf_reloc_symbol_deleted_p (offset, cookie)
     bfd_vma offset;
     PTR cookie;
{
  struct elf_reloc_cookie *rcookie = (struct elf_reloc_cookie *) cookie;

  if (rcookie->bad_symtab)
    rcookie->rel = rcookie->rels;

  for (; rcookie->rel < rcookie->relend; rcookie->rel++)
    {
      unsigned long r_symndx = ELF_R_SYM (rcookie->rel->r_info);
      Elf_Internal_Sym isym;

      if (! rcookie->bad_symtab)
	if (rcookie->rel->r_offset > offset)
	  return false;
      if (rcookie->rel->r_offset != offset)
	continue;

      if (rcookie->locsyms && r_symndx < rcookie->locsymcount)
	{
	  Elf_External_Sym *lsym;
	  Elf_External_Sym_Shndx *lshndx;

	  lsym = (Elf_External_Sym *) rcookie->locsyms + r_symndx;
	  lshndx = (Elf_External_Sym_Shndx *) rcookie->locsym_shndx;
	  if (lshndx != NULL)
	    lshndx += r_symndx;
	  elf_swap_symbol_in (rcookie->abfd, lsym, lshndx, &isym);
	}

      if (r_symndx >= rcookie->locsymcount
	  || (rcookie->locsyms
	      && ELF_ST_BIND (isym.st_info) != STB_LOCAL))
	{
	  struct elf_link_hash_entry *h;

	  h = rcookie->sym_hashes[r_symndx - rcookie->extsymoff];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  if ((h->root.type == bfd_link_hash_defined
	       || h->root.type == bfd_link_hash_defweak)
	      && elf_discarded_section (h->root.u.def.section))
	    return true;
	  else
	    return false;
	}
      else if (rcookie->locsyms)
	{
	  /* It's not a relocation against a global symbol,
	     but it could be a relocation against a local
	     symbol for a discarded section.  */
	  asection *isec;

	  /* Need to: get the symbol; get the section.  */
	  if (isym.st_shndx < SHN_LORESERVE || isym.st_shndx > SHN_HIRESERVE)
	    {
	      isec = section_from_elf_index (rcookie->abfd, isym.st_shndx);
	      if (isec != NULL && elf_discarded_section (isec))
		return true;
	    }
	}
      return false;
    }
  return false;
}

/* Discard unneeded references to discarded sections.
   Returns true if any section's size was changed.  */
/* This function assumes that the relocations are in sorted order,
   which is true for all known assemblers.  */

boolean
elf_bfd_discard_info (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  struct elf_reloc_cookie cookie;
  asection *stab, *eh, *ehdr;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *shndx_hdr;
  Elf_External_Sym *freesyms;
  struct elf_backend_data *bed;
  bfd *abfd;
  boolean ret = false;
  boolean strip = info->strip == strip_all || info->strip == strip_debugger;

  if (info->relocateable
      || info->traditional_format
      || info->hash->creator->flavour != bfd_target_elf_flavour
      || ! is_elf_hash_table (info))
    return false;

  ehdr = NULL;
  if (elf_hash_table (info)->dynobj != NULL)
    ehdr = bfd_get_section_by_name (elf_hash_table (info)->dynobj,
				    ".eh_frame_hdr");

  for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    {
      if (bfd_get_flavour (abfd) != bfd_target_elf_flavour)
	continue;

      bed = get_elf_backend_data (abfd);

      if ((abfd->flags & DYNAMIC) != 0)
	continue;

      eh = NULL;
      if (ehdr)
	{
	  eh = bfd_get_section_by_name (abfd, ".eh_frame");
	  if (eh && eh->_raw_size == 0)
	    eh = NULL;
	}

      stab = strip ? NULL : bfd_get_section_by_name (abfd, ".stab");
      if ((! stab
	   || elf_section_data(stab)->sec_info_type != ELF_INFO_TYPE_STABS)
	  && ! eh
	  && (strip || ! bed->elf_backend_discard_info))
	continue;

      symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
      shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;

      cookie.abfd = abfd;
      cookie.sym_hashes = elf_sym_hashes (abfd);
      cookie.bad_symtab = elf_bad_symtab (abfd);
      if (cookie.bad_symtab)
	{
	  cookie.locsymcount =
	    symtab_hdr->sh_size / sizeof (Elf_External_Sym);
	  cookie.extsymoff = 0;
	}
      else
	{
	  cookie.locsymcount = symtab_hdr->sh_info;
	  cookie.extsymoff = symtab_hdr->sh_info;
	}

      freesyms = NULL;
      if (symtab_hdr->contents)
	cookie.locsyms = (void *) symtab_hdr->contents;
      else if (cookie.locsymcount == 0)
	cookie.locsyms = NULL;
      else
	{
	  bfd_size_type amt = cookie.locsymcount * sizeof (Elf_External_Sym);
	  cookie.locsyms = bfd_malloc (amt);
	  if (cookie.locsyms == NULL)
	    return false;
	  freesyms = cookie.locsyms;
	  if (bfd_seek (abfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	      || bfd_bread (cookie.locsyms, amt, abfd) != amt)
	    {
	    error_ret_free_loc:
	      free (cookie.locsyms);
	      return false;
	    }
	}

      cookie.locsym_shndx = NULL;
      if (shndx_hdr->sh_size != 0 && cookie.locsymcount != 0)
	{
	  bfd_size_type amt;
	  amt = cookie.locsymcount * sizeof (Elf_External_Sym_Shndx);
	  cookie.locsym_shndx = bfd_malloc (amt);
	  if (cookie.locsym_shndx == NULL)
	    goto error_ret_free_loc;
	  if (bfd_seek (abfd, shndx_hdr->sh_offset, SEEK_SET) != 0
	      || bfd_bread (cookie.locsym_shndx, amt, abfd) != amt)
	    {
	      free (cookie.locsym_shndx);
	      goto error_ret_free_loc;
	    }
	}

      if (stab)
	{
	  cookie.rels = (NAME(_bfd_elf,link_read_relocs)
			 (abfd, stab, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL,
			  info->keep_memory));
	  if (cookie.rels)
	    {
	      cookie.rel = cookie.rels;
	      cookie.relend =
		cookie.rels + stab->reloc_count * bed->s->int_rels_per_ext_rel;
	      if (_bfd_discard_section_stabs (abfd, stab,
					      elf_section_data (stab)->sec_info,
					      elf_reloc_symbol_deleted_p,
					      &cookie))
		ret = true;
	      if (! info->keep_memory)
		free (cookie.rels);
	    }
	}

      if (eh)
	{
	  cookie.rels = NULL;
	  cookie.rel = NULL;
	  cookie.relend = NULL;
	  if (eh->reloc_count)
	    cookie.rels = (NAME(_bfd_elf,link_read_relocs)
			   (abfd, eh, (PTR) NULL, (Elf_Internal_Rela *) NULL,
			    info->keep_memory));
	  if (cookie.rels)
	    {
	      cookie.rel = cookie.rels;
	      cookie.relend =
		cookie.rels + eh->reloc_count * bed->s->int_rels_per_ext_rel;
	    }
	  if (_bfd_elf_discard_section_eh_frame (abfd, info, eh, ehdr,
						 elf_reloc_symbol_deleted_p,
						 &cookie))
	    ret = true;
	  if (! info->keep_memory)
	    free (cookie.rels);
	}

      if (bed->elf_backend_discard_info)
	{
	  if (bed->elf_backend_discard_info (abfd, &cookie, info))
	    ret = true;
	}

      if (cookie.locsym_shndx != NULL)
	free (cookie.locsym_shndx);

      if (freesyms != NULL)
	free (freesyms);
    }

  if (ehdr && _bfd_elf_discard_section_eh_frame_hdr (output_bfd, info, ehdr))
    ret = true;
  return ret;
}

static boolean
elf_section_ignore_discarded_relocs (sec)
     asection *sec;
{
  struct elf_backend_data *bed;

  switch (elf_section_data (sec)->sec_info_type)
    {
    case ELF_INFO_TYPE_STABS:
    case ELF_INFO_TYPE_EH_FRAME:
      return true;
    default:
      break;
    }

  bed = get_elf_backend_data (sec->owner);
  if (bed->elf_backend_ignore_discarded_relocs != NULL
      && (*bed->elf_backend_ignore_discarded_relocs) (sec))
    return true;

  return false;
}
