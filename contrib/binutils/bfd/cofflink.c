/* COFF specific linker code.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This file contains the COFF backend linker code.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "libcoff.h"

static boolean coff_link_add_object_symbols
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean coff_link_check_archive_element
  PARAMS ((bfd *, struct bfd_link_info *, boolean *));
static boolean coff_link_check_ar_symbols
  PARAMS ((bfd *, struct bfd_link_info *, boolean *));
static boolean coff_link_add_symbols PARAMS ((bfd *, struct bfd_link_info *));
static char *dores_com PARAMS ((char *, bfd *, int));
static char *get_name PARAMS ((char *, char **));
static int process_embedded_commands
  PARAMS ((bfd *, struct bfd_link_info *, bfd *));
static void mark_relocs PARAMS ((struct coff_final_link_info *, bfd *));

/* Define macros so that the ISFCN, et. al., macros work correctly.
   These macros are defined in include/coff/internal.h in terms of
   N_TMASK, etc.  These definitions require a user to define local
   variables with the appropriate names, and with values from the
   coff_data (abfd) structure.  */

#define N_TMASK n_tmask
#define N_BTSHFT n_btshft
#define N_BTMASK n_btmask

/* Create an entry in a COFF linker hash table.  */

struct bfd_hash_entry *
_bfd_coff_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct coff_link_hash_entry *ret = (struct coff_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct coff_link_hash_entry *) NULL)
    ret = ((struct coff_link_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct coff_link_hash_entry)));
  if (ret == (struct coff_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct coff_link_hash_entry *)
	 _bfd_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				 table, string));
  if (ret != (struct coff_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->indx = -1;
      ret->type = T_NULL;
      ret->class = C_NULL;
      ret->numaux = 0;
      ret->auxbfd = NULL;
      ret->aux = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Initialize a COFF linker hash table.  */

boolean
_bfd_coff_link_hash_table_init (table, abfd, newfunc)
     struct coff_link_hash_table *table;
     bfd *abfd;
     struct bfd_hash_entry *(*newfunc) PARAMS ((struct bfd_hash_entry *,
						struct bfd_hash_table *,
						const char *));
{
  table->stab_info = NULL;
  return _bfd_link_hash_table_init (&table->root, abfd, newfunc);
}

/* Create a COFF linker hash table.  */

struct bfd_link_hash_table *
_bfd_coff_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct coff_link_hash_table *ret;

  ret = ((struct coff_link_hash_table *)
	 bfd_alloc (abfd, sizeof (struct coff_link_hash_table)));
  if (ret == NULL)
    return NULL;
  if (! _bfd_coff_link_hash_table_init (ret, abfd,
					_bfd_coff_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return (struct bfd_link_hash_table *) NULL;
    }
  return &ret->root;
}

/* Create an entry in a COFF debug merge hash table.  */

struct bfd_hash_entry *
_bfd_coff_debug_merge_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct coff_debug_merge_hash_entry *ret =
    (struct coff_debug_merge_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct coff_debug_merge_hash_entry *) NULL)
    ret = ((struct coff_debug_merge_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct coff_debug_merge_hash_entry)));
  if (ret == (struct coff_debug_merge_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct coff_debug_merge_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));
  if (ret != (struct coff_debug_merge_hash_entry *) NULL)
    {
      /* Set local fields.  */
      ret->types = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Given a COFF BFD, add symbols to the global hash table as
   appropriate.  */

boolean
_bfd_coff_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  switch (bfd_get_format (abfd))
    {
    case bfd_object:
      return coff_link_add_object_symbols (abfd, info);
    case bfd_archive:
      return (_bfd_generic_link_add_archive_symbols
	      (abfd, info, coff_link_check_archive_element));
    default:
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }
}

/* Add symbols from a COFF object file.  */

static boolean
coff_link_add_object_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  if (! _bfd_coff_get_external_symbols (abfd))
    return false;
  if (! coff_link_add_symbols (abfd, info))
    return false;

  if (! info->keep_memory)
    {
      if (! _bfd_coff_free_symbols (abfd))
	return false;
    }
  return true;
}

/* Check a single archive element to see if we need to include it in
   the link.  *PNEEDED is set according to whether this element is
   needed in the link or not.  This is called via
   _bfd_generic_link_add_archive_symbols.  */

static boolean
coff_link_check_archive_element (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  if (! _bfd_coff_get_external_symbols (abfd))
    return false;

  if (! coff_link_check_ar_symbols (abfd, info, pneeded))
    return false;

  if (*pneeded)
    {
      if (! coff_link_add_symbols (abfd, info))
	return false;
    }

  if (! info->keep_memory || ! *pneeded)
    {
      if (! _bfd_coff_free_symbols (abfd))
	return false;
    }

  return true;
}

/* Look through the symbols to see if this object file should be
   included in the link.  */

static boolean
coff_link_check_ar_symbols (abfd, info, pneeded)
     bfd *abfd;
     struct bfd_link_info *info;
     boolean *pneeded;
{
  bfd_size_type symesz;
  bfd_byte *esym;
  bfd_byte *esym_end;

  *pneeded = false;

  symesz = bfd_coff_symesz (abfd);
  esym = (bfd_byte *) obj_coff_external_syms (abfd);
  esym_end = esym + obj_raw_syment_count (abfd) * symesz;
  while (esym < esym_end)
    {
      struct internal_syment sym;
      enum coff_symbol_classification classification;

      bfd_coff_swap_sym_in (abfd, (PTR) esym, (PTR) &sym);

      classification = bfd_coff_classify_symbol (abfd, &sym);
      if (classification == COFF_SYMBOL_GLOBAL
	  || classification == COFF_SYMBOL_COMMON)
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];
	  struct bfd_link_hash_entry *h;

	  /* This symbol is externally visible, and is defined by this
             object file.  */

	  name = _bfd_coff_internal_syment_name (abfd, &sym, buf);
	  if (name == NULL)
	    return false;
	  h = bfd_link_hash_lookup (info->hash, name, false, false, true);

	  /* We are only interested in symbols that are currently
	     undefined.  If a symbol is currently known to be common,
	     COFF linkers do not bring in an object file which defines
	     it.  */
	  if (h != (struct bfd_link_hash_entry *) NULL
	      && h->type == bfd_link_hash_undefined)
	    {
	      if (! (*info->callbacks->add_archive_element) (info, abfd, name))
		return false;
	      *pneeded = true;
	      return true;
	    }
	}

      esym += (sym.n_numaux + 1) * symesz;
    }

  /* We do not need this object file.  */
  return true;
}

/* Add all the symbols from an object file to the hash table.  */

static boolean
coff_link_add_symbols (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  unsigned int n_tmask = coff_data (abfd)->local_n_tmask;
  unsigned int n_btshft = coff_data (abfd)->local_n_btshft;
  unsigned int n_btmask = coff_data (abfd)->local_n_btmask;
  boolean keep_syms;
  boolean default_copy;
  bfd_size_type symcount;
  struct coff_link_hash_entry **sym_hash;
  bfd_size_type symesz;
  bfd_byte *esym;
  bfd_byte *esym_end;

  /* Keep the symbols during this function, in case the linker needs
     to read the generic symbols in order to report an error message.  */
  keep_syms = obj_coff_keep_syms (abfd);
  obj_coff_keep_syms (abfd) = true;

  if (info->keep_memory)
    default_copy = false;
  else
    default_copy = true;

  symcount = obj_raw_syment_count (abfd);

  /* We keep a list of the linker hash table entries that correspond
     to particular symbols.  */
  sym_hash = ((struct coff_link_hash_entry **)
	      bfd_alloc (abfd,
			 ((size_t) symcount
			  * sizeof (struct coff_link_hash_entry *))));
  if (sym_hash == NULL && symcount != 0)
    goto error_return;
  obj_coff_sym_hashes (abfd) = sym_hash;
  memset (sym_hash, 0,
	  (size_t) symcount * sizeof (struct coff_link_hash_entry *));

  symesz = bfd_coff_symesz (abfd);
  BFD_ASSERT (symesz == bfd_coff_auxesz (abfd));
  esym = (bfd_byte *) obj_coff_external_syms (abfd);
  esym_end = esym + symcount * symesz;
  while (esym < esym_end)
    {
      struct internal_syment sym;
      enum coff_symbol_classification classification;
      boolean copy;

      bfd_coff_swap_sym_in (abfd, (PTR) esym, (PTR) &sym);

      classification = bfd_coff_classify_symbol (abfd, &sym);
      if (classification != COFF_SYMBOL_LOCAL)
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];
	  flagword flags;
	  asection *section;
	  bfd_vma value;
	  boolean addit;

	  /* This symbol is externally visible.  */

	  name = _bfd_coff_internal_syment_name (abfd, &sym, buf);
	  if (name == NULL)
	    goto error_return;

	  /* We must copy the name into memory if we got it from the
             syment itself, rather than the string table.  */
	  copy = default_copy;
	  if (sym._n._n_n._n_zeroes != 0
	      || sym._n._n_n._n_offset == 0)
	    copy = true;

	  value = sym.n_value;

	  switch (classification)
	    {
	    default:
	      abort ();

	    case COFF_SYMBOL_GLOBAL:
	      flags = BSF_EXPORT | BSF_GLOBAL;
	      section = coff_section_from_bfd_index (abfd, sym.n_scnum);
	      if (! obj_pe (abfd))
		value -= section->vma;
	      break;

	    case COFF_SYMBOL_UNDEFINED:
	      flags = 0;
	      section = bfd_und_section_ptr;
	      break;

	    case COFF_SYMBOL_COMMON:
	      flags = BSF_GLOBAL;
	      section = bfd_com_section_ptr;
	      break;

	    case COFF_SYMBOL_PE_SECTION:
	      flags = BSF_SECTION_SYM | BSF_GLOBAL;
	      section = coff_section_from_bfd_index (abfd, sym.n_scnum);
	      break;
	    }

	  if (sym.n_sclass == C_WEAKEXT
	      || (obj_pe (abfd) && sym.n_sclass == C_NT_WEAK))
	    flags = BSF_WEAK;

	  addit = true;

	  /* In the PE format, section symbols actually refer to the
             start of the output section.  We handle them specially
             here.  */
	  if (obj_pe (abfd) && (flags & BSF_SECTION_SYM) != 0)
	    {
	      *sym_hash = coff_link_hash_lookup (coff_hash_table (info),
						 name, false, copy, false);
	      if (*sym_hash != NULL)
		{
		  if (((*sym_hash)->coff_link_hash_flags
		       & COFF_LINK_HASH_PE_SECTION_SYMBOL) == 0
		      && (*sym_hash)->root.type != bfd_link_hash_undefined
		      && (*sym_hash)->root.type != bfd_link_hash_undefweak)
		    (*_bfd_error_handler)
		      ("Warning: symbol `%s' is both section and non-section",
		       name);

		  addit = false;
		}
	    }

	  /* The Microsoft Visual C compiler does string pooling by
	     hashing the constants to an internal symbol name, and
	     relying on the the linker comdat support to discard
	     duplicate names.  However, if one string is a literal and
	     one is a data initializer, one will end up in the .data
	     section and one will end up in the .rdata section.  The
	     Microsoft linker will combine them into the .data
	     section, which seems to be wrong since it might cause the
	     literal to change.

	     As long as there are no external references to the
	     symbols, which there shouldn't be, we can treat the .data
	     and .rdata instances as separate symbols.  The comdat
	     code in the linker will do the appropriate merging.  Here
	     we avoid getting a multiple definition error for one of
	     these special symbols.

	     FIXME: I don't think this will work in the case where
	     there are two object files which use the constants as a
	     literal and two object files which use it as a data
	     initializer.  One or the other of the second object files
	     is going to wind up with an inappropriate reference.  */
	  if (obj_pe (abfd)
	      && (classification == COFF_SYMBOL_GLOBAL
		  || classification == COFF_SYMBOL_PE_SECTION)
	      && section->comdat != NULL
	      && strncmp (name, "??_", 3) == 0
	      && strcmp (name, section->comdat->name) == 0)
	    {
	      if (*sym_hash == NULL)
		*sym_hash = coff_link_hash_lookup (coff_hash_table (info),
						   name, false, copy, false);
	      if (*sym_hash != NULL
		  && (*sym_hash)->root.type == bfd_link_hash_defined
		  && (*sym_hash)->root.u.def.section->comdat != NULL
		  && strcmp ((*sym_hash)->root.u.def.section->comdat->name,
			     section->comdat->name) == 0)
		addit = false;
	    }

	  if (addit)
	    {
	      if (! (bfd_coff_link_add_one_symbol
		     (info, abfd, name, flags, section, value,
		      (const char *) NULL, copy, false,
		      (struct bfd_link_hash_entry **) sym_hash)))
		goto error_return;
	    }

	  if (obj_pe (abfd) && (flags & BSF_SECTION_SYM) != 0)
	    (*sym_hash)->coff_link_hash_flags |=
	      COFF_LINK_HASH_PE_SECTION_SYMBOL;

	  /* Limit the alignment of a common symbol to the possible
             alignment of a section.  There is no point to permitting
             a higher alignment for a common symbol: we can not
             guarantee it, and it may cause us to allocate extra space
             in the common section.  */
	  if (section == bfd_com_section_ptr
	      && (*sym_hash)->root.type == bfd_link_hash_common
	      && ((*sym_hash)->root.u.c.p->alignment_power
		  > bfd_coff_default_section_alignment_power (abfd)))
	    (*sym_hash)->root.u.c.p->alignment_power
	      = bfd_coff_default_section_alignment_power (abfd);

	  if (info->hash->creator->flavour == bfd_get_flavour (abfd))
	    {
	      /* If we don't have any symbol information currently in
                 the hash table, or if we are looking at a symbol
                 definition, then update the symbol class and type in
                 the hash table.  */
  	      if (((*sym_hash)->class == C_NULL
  		   && (*sym_hash)->type == T_NULL)
  		  || sym.n_scnum != 0
  		  || (sym.n_value != 0
  		      && (*sym_hash)->root.type != bfd_link_hash_defined
  		      && (*sym_hash)->root.type != bfd_link_hash_defweak))
  		{
  		  (*sym_hash)->class = sym.n_sclass;
  		  if (sym.n_type != T_NULL)
  		    {
  		      /* We want to warn if the type changed, but not
  			 if it changed from an unspecified type.
  			 Testing the whole type byte may work, but the
  			 change from (e.g.) a function of unspecified
  			 type to function of known type also wants to
  			 skip the warning.  */
  		      if ((*sym_hash)->type != T_NULL
  			  && (*sym_hash)->type != sym.n_type
  		          && !(DTYPE ((*sym_hash)->type) == DTYPE (sym.n_type)
  		               && (BTYPE ((*sym_hash)->type) == T_NULL
  		                   || BTYPE (sym.n_type) == T_NULL)))
  			(*_bfd_error_handler)
  			  (_("Warning: type of symbol `%s' changed from %d to %d in %s"),
  			   name, (*sym_hash)->type, sym.n_type,
  			   bfd_get_filename (abfd));

  		      /* We don't want to change from a meaningful
  			 base type to a null one, but if we know
  			 nothing, take what little we might now know.  */
  		      if (BTYPE (sym.n_type) != T_NULL
  			  || (*sym_hash)->type == T_NULL)
			(*sym_hash)->type = sym.n_type;
  		    }
  		  (*sym_hash)->auxbfd = abfd;
		  if (sym.n_numaux != 0)
		    {
		      union internal_auxent *alloc;
		      unsigned int i;
		      bfd_byte *eaux;
		      union internal_auxent *iaux;

		      (*sym_hash)->numaux = sym.n_numaux;
		      alloc = ((union internal_auxent *)
			       bfd_hash_allocate (&info->hash->table,
						  (sym.n_numaux
						   * sizeof (*alloc))));
		      if (alloc == NULL)
			goto error_return;
		      for (i = 0, eaux = esym + symesz, iaux = alloc;
			   i < sym.n_numaux;
			   i++, eaux += symesz, iaux++)
			bfd_coff_swap_aux_in (abfd, (PTR) eaux, sym.n_type,
					      sym.n_sclass, i, sym.n_numaux,
					      (PTR) iaux);
		      (*sym_hash)->aux = alloc;
		    }
		}
	    }

	  if (classification == COFF_SYMBOL_PE_SECTION
	      && (*sym_hash)->numaux != 0)
	    {
	      /* Some PE sections (such as .bss) have a zero size in
                 the section header, but a non-zero size in the AUX
                 record.  Correct that here.

		 FIXME: This is not at all the right place to do this.
		 For example, it won't help objdump.  This needs to be
		 done when we swap in the section header.  */

	      BFD_ASSERT ((*sym_hash)->numaux == 1);
	      if (section->_raw_size == 0)
		section->_raw_size = (*sym_hash)->aux[0].x_scn.x_scnlen;

	      /* FIXME: We could test whether the section sizes
                 matches the size in the aux entry, but apparently
                 that sometimes fails unexpectedly.  */
	    }
	}

      esym += (sym.n_numaux + 1) * symesz;
      sym_hash += sym.n_numaux + 1;
    }

  /* If this is a non-traditional, non-relocateable link, try to
     optimize the handling of any .stab/.stabstr sections.  */
  if (! info->relocateable
      && ! info->traditional_format
      && info->hash->creator->flavour == bfd_get_flavour (abfd)
      && (info->strip != strip_all && info->strip != strip_debugger))
    {
      asection *stab, *stabstr;

      stab = bfd_get_section_by_name (abfd, ".stab");
      if (stab != NULL)
	{
	  stabstr = bfd_get_section_by_name (abfd, ".stabstr");

	  if (stabstr != NULL)
	    {
	      struct coff_link_hash_table *table;
	      struct coff_section_tdata *secdata;

	      secdata = coff_section_data (abfd, stab);
	      if (secdata == NULL)
		{
		  stab->used_by_bfd =
		    (PTR) bfd_zalloc (abfd,
				      sizeof (struct coff_section_tdata));
		  if (stab->used_by_bfd == NULL)
		    goto error_return;
		  secdata = coff_section_data (abfd, stab);
		}

	      table = coff_hash_table (info);

	      if (! _bfd_link_section_stabs (abfd, &table->stab_info,
					     stab, stabstr,
					     &secdata->stab_info))
		goto error_return;
	    }
	}
    }

  obj_coff_keep_syms (abfd) = keep_syms;

  return true;

 error_return:
  obj_coff_keep_syms (abfd) = keep_syms;
  return false;
}

/* Do the final link step.  */

boolean
_bfd_coff_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  bfd_size_type symesz;
  struct coff_final_link_info finfo;
  boolean debug_merge_allocated;
  boolean long_section_names;
  asection *o;
  struct bfd_link_order *p;
  size_t max_sym_count;
  size_t max_lineno_count;
  size_t max_reloc_count;
  size_t max_output_reloc_count;
  size_t max_contents_size;
  file_ptr rel_filepos;
  unsigned int relsz;
  file_ptr line_filepos;
  unsigned int linesz;
  bfd *sub;
  bfd_byte *external_relocs = NULL;
  char strbuf[STRING_SIZE_SIZE];

  symesz = bfd_coff_symesz (abfd);

  finfo.info = info;
  finfo.output_bfd = abfd;
  finfo.strtab = NULL;
  finfo.section_info = NULL;
  finfo.last_file_index = -1;
  finfo.last_bf_index = -1;
  finfo.internal_syms = NULL;
  finfo.sec_ptrs = NULL;
  finfo.sym_indices = NULL;
  finfo.outsyms = NULL;
  finfo.linenos = NULL;
  finfo.contents = NULL;
  finfo.external_relocs = NULL;
  finfo.internal_relocs = NULL;
  finfo.global_to_static = false;
  debug_merge_allocated = false;

  coff_data (abfd)->link_info = info;

  finfo.strtab = _bfd_stringtab_init ();
  if (finfo.strtab == NULL)
    goto error_return;

  if (! coff_debug_merge_hash_table_init (&finfo.debug_merge))
    goto error_return;
  debug_merge_allocated = true;

  /* Compute the file positions for all the sections.  */
  if (! abfd->output_has_begun)
    {
      if (! bfd_coff_compute_section_file_positions (abfd))
	goto error_return;
    }

  /* Count the line numbers and relocation entries required for the
     output file.  Set the file positions for the relocs.  */
  rel_filepos = obj_relocbase (abfd);
  relsz = bfd_coff_relsz (abfd);
  max_contents_size = 0;
  max_lineno_count = 0;
  max_reloc_count = 0;

  long_section_names = false;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      o->reloc_count = 0;
      o->lineno_count = 0;
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order)
	    {
	      asection *sec;

	      sec = p->u.indirect.section;

	      /* Mark all sections which are to be included in the
		 link.  This will normally be every section.  We need
		 to do this so that we can identify any sections which
		 the linker has decided to not include.  */
	      sec->linker_mark = true;

	      if (info->strip == strip_none
		  || info->strip == strip_some)
		o->lineno_count += sec->lineno_count;

	      if (info->relocateable)
		o->reloc_count += sec->reloc_count;

	      if (sec->_raw_size > max_contents_size)
		max_contents_size = sec->_raw_size;
	      if (sec->lineno_count > max_lineno_count)
		max_lineno_count = sec->lineno_count;
	      if (sec->reloc_count > max_reloc_count)
		max_reloc_count = sec->reloc_count;
	    }
	  else if (info->relocateable
		   && (p->type == bfd_section_reloc_link_order
		       || p->type == bfd_symbol_reloc_link_order))
	    ++o->reloc_count;
	}
      if (o->reloc_count == 0)
	o->rel_filepos = 0;
      else
	{
	  o->flags |= SEC_RELOC;
	  o->rel_filepos = rel_filepos;
	  rel_filepos += o->reloc_count * relsz;
	}

      if (bfd_coff_long_section_names (abfd)
	  && strlen (o->name) > SCNNMLEN)
	{
	  /* This section has a long name which must go in the string
             table.  This must correspond to the code in
             coff_write_object_contents which puts the string index
             into the s_name field of the section header.  That is why
             we pass hash as false.  */
	  if (_bfd_stringtab_add (finfo.strtab, o->name, false, false)
	      == (bfd_size_type) -1)
	    goto error_return;
	  long_section_names = true;
	}
    }

  /* If doing a relocateable link, allocate space for the pointers we
     need to keep.  */
  if (info->relocateable)
    {
      unsigned int i;

      /* We use section_count + 1, rather than section_count, because
         the target_index fields are 1 based.  */
      finfo.section_info =
	((struct coff_link_section_info *)
	 bfd_malloc ((abfd->section_count + 1)
		     * sizeof (struct coff_link_section_info)));
      if (finfo.section_info == NULL)
	goto error_return;
      for (i = 0; i <= abfd->section_count; i++)
	{
	  finfo.section_info[i].relocs = NULL;
	  finfo.section_info[i].rel_hashes = NULL;
	}
    }

  /* We now know the size of the relocs, so we can determine the file
     positions of the line numbers.  */
  line_filepos = rel_filepos;
  linesz = bfd_coff_linesz (abfd);
  max_output_reloc_count = 0;
  for (o = abfd->sections; o != NULL; o = o->next)
    {
      if (o->lineno_count == 0)
	o->line_filepos = 0;
      else
	{
	  o->line_filepos = line_filepos;
	  line_filepos += o->lineno_count * linesz;
	}

      if (o->reloc_count != 0)
	{
	  /* We don't know the indices of global symbols until we have
             written out all the local symbols.  For each section in
             the output file, we keep an array of pointers to hash
             table entries.  Each entry in the array corresponds to a
             reloc.  When we find a reloc against a global symbol, we
             set the corresponding entry in this array so that we can
             fix up the symbol index after we have written out all the
             local symbols.

	     Because of this problem, we also keep the relocs in
	     memory until the end of the link.  This wastes memory,
	     but only when doing a relocateable link, which is not the
	     common case.  */
	  BFD_ASSERT (info->relocateable);
	  finfo.section_info[o->target_index].relocs =
	    ((struct internal_reloc *)
	     bfd_malloc (o->reloc_count * sizeof (struct internal_reloc)));
	  finfo.section_info[o->target_index].rel_hashes =
	    ((struct coff_link_hash_entry **)
	     bfd_malloc (o->reloc_count
		     * sizeof (struct coff_link_hash_entry *)));
	  if (finfo.section_info[o->target_index].relocs == NULL
	      || finfo.section_info[o->target_index].rel_hashes == NULL)
	    goto error_return;

	  if (o->reloc_count > max_output_reloc_count)
	    max_output_reloc_count = o->reloc_count;
	}

      /* Reset the reloc and lineno counts, so that we can use them to
	 count the number of entries we have output so far.  */
      o->reloc_count = 0;
      o->lineno_count = 0;
    }

  obj_sym_filepos (abfd) = line_filepos;

  /* Figure out the largest number of symbols in an input BFD.  Take
     the opportunity to clear the output_has_begun fields of all the
     input BFD's.  */
  max_sym_count = 0;
  for (sub = info->input_bfds; sub != NULL; sub = sub->link_next)
    {
      size_t sz;

      sub->output_has_begun = false;
      sz = obj_raw_syment_count (sub);
      if (sz > max_sym_count)
	max_sym_count = sz;
    }

  /* Allocate some buffers used while linking.  */
  finfo.internal_syms = ((struct internal_syment *)
			 bfd_malloc (max_sym_count
				     * sizeof (struct internal_syment)));
  finfo.sec_ptrs = (asection **) bfd_malloc (max_sym_count
					     * sizeof (asection *));
  finfo.sym_indices = (long *) bfd_malloc (max_sym_count * sizeof (long));
  finfo.outsyms = ((bfd_byte *)
		   bfd_malloc ((size_t) ((max_sym_count + 1) * symesz)));
  finfo.linenos = (bfd_byte *) bfd_malloc (max_lineno_count
				       * bfd_coff_linesz (abfd));
  finfo.contents = (bfd_byte *) bfd_malloc (max_contents_size);
  finfo.external_relocs = (bfd_byte *) bfd_malloc (max_reloc_count * relsz);
  if (! info->relocateable)
    finfo.internal_relocs = ((struct internal_reloc *)
			     bfd_malloc (max_reloc_count
					 * sizeof (struct internal_reloc)));
  if ((finfo.internal_syms == NULL && max_sym_count > 0)
      || (finfo.sec_ptrs == NULL && max_sym_count > 0)
      || (finfo.sym_indices == NULL && max_sym_count > 0)
      || finfo.outsyms == NULL
      || (finfo.linenos == NULL && max_lineno_count > 0)
      || (finfo.contents == NULL && max_contents_size > 0)
      || (finfo.external_relocs == NULL && max_reloc_count > 0)
      || (! info->relocateable
	  && finfo.internal_relocs == NULL
	  && max_reloc_count > 0))
    goto error_return;

  /* We now know the position of everything in the file, except that
     we don't know the size of the symbol table and therefore we don't
     know where the string table starts.  We just build the string
     table in memory as we go along.  We process all the relocations
     for a single input file at once.  */
  obj_raw_syment_count (abfd) = 0;

  if (coff_backend_info (abfd)->_bfd_coff_start_final_link)
    {
      if (! bfd_coff_start_final_link (abfd, info))
	goto error_return;
    }

  for (o = abfd->sections; o != NULL; o = o->next)
    {
      for (p = o->link_order_head; p != NULL; p = p->next)
	{
	  if (p->type == bfd_indirect_link_order
	      && (bfd_get_flavour (p->u.indirect.section->owner)
		  == bfd_target_coff_flavour))
	    {
	      sub = p->u.indirect.section->owner;
	      if (! bfd_coff_link_output_has_begun (sub, & finfo))
		{
		  if (! _bfd_coff_link_input_bfd (&finfo, sub))
		    goto error_return;
		  sub->output_has_begun = true;
		}
	    }
	  else if (p->type == bfd_section_reloc_link_order
		   || p->type == bfd_symbol_reloc_link_order)
	    {
	      if (! _bfd_coff_reloc_link_order (abfd, &finfo, o, p))
		goto error_return;
	    }
	  else
	    {
	      if (! _bfd_default_link_order (abfd, info, o, p))
		goto error_return;
	    }
	}
    }

  if (! bfd_coff_final_link_postscript (abfd, & finfo))
    goto error_return;
  
  /* Free up the buffers used by _bfd_coff_link_input_bfd.  */

  coff_debug_merge_hash_table_free (&finfo.debug_merge);
  debug_merge_allocated = false;

  if (finfo.internal_syms != NULL)
    {
      free (finfo.internal_syms);
      finfo.internal_syms = NULL;
    }
  if (finfo.sec_ptrs != NULL)
    {
      free (finfo.sec_ptrs);
      finfo.sec_ptrs = NULL;
    }
  if (finfo.sym_indices != NULL)
    {
      free (finfo.sym_indices);
      finfo.sym_indices = NULL;
    }
  if (finfo.linenos != NULL)
    {
      free (finfo.linenos);
      finfo.linenos = NULL;
    }
  if (finfo.contents != NULL)
    {
      free (finfo.contents);
      finfo.contents = NULL;
    }
  if (finfo.external_relocs != NULL)
    {
      free (finfo.external_relocs);
      finfo.external_relocs = NULL;
    }
  if (finfo.internal_relocs != NULL)
    {
      free (finfo.internal_relocs);
      finfo.internal_relocs = NULL;
    }

  /* The value of the last C_FILE symbol is supposed to be the symbol
     index of the first external symbol.  Write it out again if
     necessary.  */
  if (finfo.last_file_index != -1
      && (unsigned int) finfo.last_file.n_value != obj_raw_syment_count (abfd))
    {
      finfo.last_file.n_value = obj_raw_syment_count (abfd);
      bfd_coff_swap_sym_out (abfd, (PTR) &finfo.last_file,
			     (PTR) finfo.outsyms);
      if (bfd_seek (abfd,
		    (obj_sym_filepos (abfd)
		     + finfo.last_file_index * symesz),
		    SEEK_SET) != 0
	  || bfd_write (finfo.outsyms, symesz, 1, abfd) != symesz)
	return false;
    }

  /* If doing task linking (ld --task-link) then make a pass through the
     global symbols, writing out any that are defined, and making them
     static. */
  if (info->task_link)
    {
      finfo.failed = false;
      coff_link_hash_traverse (coff_hash_table (info), _bfd_coff_write_task_globals,
			       (PTR) &finfo);
      if (finfo.failed)
	goto error_return;
    }

  /* Write out the global symbols.  */
  finfo.failed = false;
  coff_link_hash_traverse (coff_hash_table (info), _bfd_coff_write_global_sym,
			   (PTR) &finfo);
  if (finfo.failed)
    goto error_return;

  /* The outsyms buffer is used by _bfd_coff_write_global_sym.  */
  if (finfo.outsyms != NULL)
    {
      free (finfo.outsyms);
      finfo.outsyms = NULL;
    }

  if (info->relocateable && max_output_reloc_count > 0)
    {
      /* Now that we have written out all the global symbols, we know
	 the symbol indices to use for relocs against them, and we can
	 finally write out the relocs.  */
      external_relocs = ((bfd_byte *)
			 bfd_malloc (max_output_reloc_count * relsz));
      if (external_relocs == NULL)
	goto error_return;

      for (o = abfd->sections; o != NULL; o = o->next)
	{
	  struct internal_reloc *irel;
	  struct internal_reloc *irelend;
	  struct coff_link_hash_entry **rel_hash;
	  bfd_byte *erel;

	  if (o->reloc_count == 0)
	    continue;

	  irel = finfo.section_info[o->target_index].relocs;
	  irelend = irel + o->reloc_count;
	  rel_hash = finfo.section_info[o->target_index].rel_hashes;
	  erel = external_relocs;
	  for (; irel < irelend; irel++, rel_hash++, erel += relsz)
	    {
	      if (*rel_hash != NULL)
		{
		  BFD_ASSERT ((*rel_hash)->indx >= 0);
		  irel->r_symndx = (*rel_hash)->indx;
		}
	      bfd_coff_swap_reloc_out (abfd, (PTR) irel, (PTR) erel);
	    }

	  if (bfd_seek (abfd, o->rel_filepos, SEEK_SET) != 0
	      || bfd_write ((PTR) external_relocs, relsz, o->reloc_count,
			    abfd) != relsz * o->reloc_count)
	    goto error_return;
	}

      free (external_relocs);
      external_relocs = NULL;
    }

  /* Free up the section information.  */
  if (finfo.section_info != NULL)
    {
      unsigned int i;

      for (i = 0; i < abfd->section_count; i++)
	{
	  if (finfo.section_info[i].relocs != NULL)
	    free (finfo.section_info[i].relocs);
	  if (finfo.section_info[i].rel_hashes != NULL)
	    free (finfo.section_info[i].rel_hashes);
	}
      free (finfo.section_info);
      finfo.section_info = NULL;
    }

  /* If we have optimized stabs strings, output them.  */
  if (coff_hash_table (info)->stab_info != NULL)
    {
      if (! _bfd_write_stab_strings (abfd, &coff_hash_table (info)->stab_info))
	return false;
    }

  /* Write out the string table.  */
  if (obj_raw_syment_count (abfd) != 0 || long_section_names)
    {
      if (bfd_seek (abfd,
		    (obj_sym_filepos (abfd)
		     + obj_raw_syment_count (abfd) * symesz),
		    SEEK_SET) != 0)
	return false;

#if STRING_SIZE_SIZE == 4
      bfd_h_put_32 (abfd,
		    _bfd_stringtab_size (finfo.strtab) + STRING_SIZE_SIZE,
		    (bfd_byte *) strbuf);
#else
 #error Change bfd_h_put_32
#endif

      if (bfd_write (strbuf, 1, STRING_SIZE_SIZE, abfd) != STRING_SIZE_SIZE)
	return false;

      if (! _bfd_stringtab_emit (abfd, finfo.strtab))
	return false;
    }

  _bfd_stringtab_free (finfo.strtab);

  /* Setting bfd_get_symcount to 0 will cause write_object_contents to
     not try to write out the symbols.  */
  bfd_get_symcount (abfd) = 0;

  return true;

 error_return:
  if (debug_merge_allocated)
    coff_debug_merge_hash_table_free (&finfo.debug_merge);
  if (finfo.strtab != NULL)
    _bfd_stringtab_free (finfo.strtab);
  if (finfo.section_info != NULL)
    {
      unsigned int i;

      for (i = 0; i < abfd->section_count; i++)
	{
	  if (finfo.section_info[i].relocs != NULL)
	    free (finfo.section_info[i].relocs);
	  if (finfo.section_info[i].rel_hashes != NULL)
	    free (finfo.section_info[i].rel_hashes);
	}
      free (finfo.section_info);
    }
  if (finfo.internal_syms != NULL)
    free (finfo.internal_syms);
  if (finfo.sec_ptrs != NULL)
    free (finfo.sec_ptrs);
  if (finfo.sym_indices != NULL)
    free (finfo.sym_indices);
  if (finfo.outsyms != NULL)
    free (finfo.outsyms);
  if (finfo.linenos != NULL)
    free (finfo.linenos);
  if (finfo.contents != NULL)
    free (finfo.contents);
  if (finfo.external_relocs != NULL)
    free (finfo.external_relocs);
  if (finfo.internal_relocs != NULL)
    free (finfo.internal_relocs);
  if (external_relocs != NULL)
    free (external_relocs);
  return false;
}

/* parse out a -heap <reserved>,<commit> line */

static char *
dores_com (ptr, output_bfd, heap)
     char *ptr;
     bfd *output_bfd;
     int heap;
{
  if (coff_data(output_bfd)->pe) 
    {
      int val = strtoul (ptr, &ptr, 0);
      if (heap)
	pe_data(output_bfd)->pe_opthdr.SizeOfHeapReserve =val;
      else
	pe_data(output_bfd)->pe_opthdr.SizeOfStackReserve =val;

      if (ptr[0] == ',') 
	{
	  int val = strtoul (ptr+1, &ptr, 0);
	  if (heap)
	    pe_data(output_bfd)->pe_opthdr.SizeOfHeapCommit =val;
	  else
	    pe_data(output_bfd)->pe_opthdr.SizeOfStackCommit =val;
	}
    }
  return ptr;
}

static char *get_name(ptr, dst)
char *ptr;
char **dst;
{
  while (*ptr == ' ')
    ptr++;
  *dst = ptr;
  while (*ptr && *ptr != ' ')
    ptr++;
  *ptr = 0;
  return ptr+1;
}

/* Process any magic embedded commands in a section called .drectve */
			
static int
process_embedded_commands (output_bfd, info,  abfd)
     bfd *output_bfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     bfd *abfd;
{
  asection *sec = bfd_get_section_by_name (abfd, ".drectve");
  char *s;
  char *e;
  char *copy;
  if (!sec) 
    return 1;
  
  copy = bfd_malloc ((size_t) sec->_raw_size);
  if (!copy) 
    return 0;
  if (! bfd_get_section_contents(abfd, sec, copy, 0, sec->_raw_size)) 
    {
      free (copy);
      return 0;
    }
  e = copy + sec->_raw_size;
  for (s = copy;  s < e ; ) 
    {
      if (s[0]!= '-') {
	s++;
	continue;
      }
      if (strncmp (s,"-attr", 5) == 0)
	{
	  char *name;
	  char *attribs;
	  asection *asec;

	  int loop = 1;
	  int had_write = 0;
	  int had_read = 0;
	  int had_exec= 0;
	  int had_shared= 0;
	  s += 5;
	  s = get_name(s, &name);
	  s = get_name(s, &attribs);
	  while (loop) {
	    switch (*attribs++) 
	      {
	      case 'W':
		had_write = 1;
		break;
	      case 'R':
		had_read = 1;
		break;
	      case 'S':
		had_shared = 1;
		break;
	      case 'X':
		had_exec = 1;
		break;
	      default:
		loop = 0;
	      }
	  }
	  asec = bfd_get_section_by_name (abfd, name);
	  if (asec) {
	    if (had_exec)
	      asec->flags |= SEC_CODE;
	    if (!had_write)
	      asec->flags |= SEC_READONLY;
	  }
	}
      else if (strncmp (s,"-heap", 5) == 0)
	{
	  s = dores_com (s+5, output_bfd, 1);
	}
      else if (strncmp (s,"-stack", 6) == 0)
	{
	  s = dores_com (s+6, output_bfd, 0);
	}
      else 
	s++;
    }
  free (copy);
  return 1;
}

/* Place a marker against all symbols which are used by relocations.
   This marker can be picked up by the 'do we skip this symbol ?'
   loop in _bfd_coff_link_input_bfd() and used to prevent skipping
   that symbol. 
   */

static void
mark_relocs (finfo, input_bfd)
     struct coff_final_link_info *	finfo;
     bfd * 				input_bfd;
{
  asection * a;

  if ((bfd_get_file_flags (input_bfd) & HAS_SYMS) == 0)
    return;
  
  for (a = input_bfd->sections; a != (asection *) NULL; a = a->next)
    {
      struct internal_reloc *	internal_relocs;
      struct internal_reloc *	irel;
      struct internal_reloc *	irelend;

      
      if ((a->flags & SEC_RELOC) == 0 || a->reloc_count  < 1)
	continue;

      /* Read in the relocs.  */
      internal_relocs = _bfd_coff_read_internal_relocs
	(input_bfd, a, false,
	 finfo->external_relocs,
	 finfo->info->relocateable,
	 (finfo->info->relocateable
	  ? (finfo->section_info[ a->output_section->target_index ].relocs + a->output_section->reloc_count)
	  : finfo->internal_relocs)
	);
      
      if (internal_relocs == NULL)
	continue;

      irel     = internal_relocs;
      irelend  = irel + a->reloc_count;

      /* Place a mark in the sym_indices array (whose entries have
	 been initialised to 0) for all of the symbols that are used
	 in the relocation table.  This will then be picked up in the
	 skip/don't pass */
      
      for (; irel < irelend; irel++)
	{
	  finfo->sym_indices[ irel->r_symndx ] = -1;
	}
    }
}

/* Link an input file into the linker output file.  This function
   handles all the sections and relocations of the input file at once.  */

boolean
_bfd_coff_link_input_bfd (finfo, input_bfd)
     struct coff_final_link_info *finfo;
     bfd *input_bfd;
{
  unsigned int n_tmask = coff_data (input_bfd)->local_n_tmask;
  unsigned int n_btshft = coff_data (input_bfd)->local_n_btshft;
#if 0
  unsigned int n_btmask = coff_data (input_bfd)->local_n_btmask;
#endif
  boolean (*adjust_symndx) PARAMS ((bfd *, struct bfd_link_info *, bfd *,
				    asection *, struct internal_reloc *,
				    boolean *));
  bfd *output_bfd;
  const char *strings;
  bfd_size_type syment_base;
  boolean copy, hash;
  bfd_size_type isymesz;
  bfd_size_type osymesz;
  bfd_size_type linesz;
  bfd_byte *esym;
  bfd_byte *esym_end;
  struct internal_syment *isymp;
  asection **secpp;
  long *indexp;
  unsigned long output_index;
  bfd_byte *outsym;
  struct coff_link_hash_entry **sym_hash;
  asection *o;

  /* Move all the symbols to the output file.  */

  output_bfd = finfo->output_bfd;
  strings = NULL;
  syment_base = obj_raw_syment_count (output_bfd);
  isymesz = bfd_coff_symesz (input_bfd);
  osymesz = bfd_coff_symesz (output_bfd);
  linesz = bfd_coff_linesz (input_bfd);
  BFD_ASSERT (linesz == bfd_coff_linesz (output_bfd));

  copy = false;
  if (! finfo->info->keep_memory)
    copy = true;
  hash = true;
  if ((output_bfd->flags & BFD_TRADITIONAL_FORMAT) != 0)
    hash = false;

  if (! _bfd_coff_get_external_symbols (input_bfd))
    return false;

  esym = (bfd_byte *) obj_coff_external_syms (input_bfd);
  esym_end = esym + obj_raw_syment_count (input_bfd) * isymesz;
  isymp = finfo->internal_syms;
  secpp = finfo->sec_ptrs;
  indexp = finfo->sym_indices;
  output_index = syment_base;
  outsym = finfo->outsyms;

  if (coff_data (output_bfd)->pe)
    {
      if (! process_embedded_commands (output_bfd, finfo->info, input_bfd))
	return false;
    }

  /* If we are going to perform relocations and also strip/discard some symbols
     then we must make sure that we do not strip/discard those symbols that are
     going to be involved in the relocations */
  if ((   finfo->info->strip   != strip_none
       || finfo->info->discard != discard_none)
      && finfo->info->relocateable)
    {
      /* mark the symbol array as 'not-used' */
      memset (indexp, 0, obj_raw_syment_count (input_bfd) * sizeof * indexp); 
       
      mark_relocs (finfo, input_bfd);
    }

  while (esym < esym_end)
    {
      struct internal_syment isym;
      enum coff_symbol_classification classification;
      boolean skip;
      boolean global;
      boolean dont_skip_symbol;
      int add;

      bfd_coff_swap_sym_in (input_bfd, (PTR) esym, (PTR) isymp);

      /* Make a copy of *isymp so that the relocate_section function
	 always sees the original values.  This is more reliable than
	 always recomputing the symbol value even if we are stripping
	 the symbol.  */
      isym = *isymp;

      classification = bfd_coff_classify_symbol (input_bfd, &isym);
      switch (classification)
	{
	default:
	  abort ();
	case COFF_SYMBOL_GLOBAL:
	case COFF_SYMBOL_PE_SECTION:
	case COFF_SYMBOL_LOCAL:
	  *secpp = coff_section_from_bfd_index (input_bfd, isym.n_scnum);
	  break;
	case COFF_SYMBOL_COMMON:
	  *secpp = bfd_com_section_ptr;
	  break;
	case COFF_SYMBOL_UNDEFINED:
	  *secpp = bfd_und_section_ptr;
	  break;
	}

      /* Extract the flag indicating if this symbol is used by a
         relocation.  */
      if ((finfo->info->strip != strip_none
	   || finfo->info->discard != discard_none)
	  && finfo->info->relocateable)
	dont_skip_symbol = *indexp;
      else
	dont_skip_symbol = false;
      
      *indexp = -1;

      skip = false;
      global = false;
      add = 1 + isym.n_numaux;

      /* If we are stripping all symbols, we want to skip this one.  */
      if (finfo->info->strip == strip_all && ! dont_skip_symbol)
	skip = true;

      if (! skip)
	{
	  switch (classification)
	    {
	    default:
	      abort ();
	    case COFF_SYMBOL_GLOBAL:
	    case COFF_SYMBOL_COMMON:
	    case COFF_SYMBOL_PE_SECTION:
	      /* This is a global symbol.  Global symbols come at the
		 end of the symbol table, so skip them for now.
		 Locally defined function symbols, however, are an
		 exception, and are not moved to the end.  */
	      global = true;
	      if (! ISFCN (isym.n_type))
		skip = true;
	      break;

	    case COFF_SYMBOL_UNDEFINED:
	      /* Undefined symbols are left for the end.  */
	      global = true;
	      skip = true;
	      break;

	    case COFF_SYMBOL_LOCAL:
	      /* This is a local symbol.  Skip it if we are discarding
                 local symbols.  */
	      if (finfo->info->discard == discard_all && ! dont_skip_symbol)
		skip = true;
	      break;
	    }
	}

      /* If we stripping debugging symbols, and this is a debugging
         symbol, then skip it.  FIXME: gas sets the section to N_ABS
         for some types of debugging symbols; I don't know if this is
         a bug or not.  In any case, we handle it here.  */
      if (! skip
	  && finfo->info->strip == strip_debugger
	  && ! dont_skip_symbol
	  && (isym.n_scnum == N_DEBUG
	      || (isym.n_scnum == N_ABS
		  && (isym.n_sclass == C_AUTO
		      || isym.n_sclass == C_REG
		      || isym.n_sclass == C_MOS
		      || isym.n_sclass == C_MOE
		      || isym.n_sclass == C_MOU
		      || isym.n_sclass == C_ARG
		      || isym.n_sclass == C_REGPARM
		      || isym.n_sclass == C_FIELD
		      || isym.n_sclass == C_EOS))))
	skip = true;

      /* If some symbols are stripped based on the name, work out the
	 name and decide whether to skip this symbol.  */
      if (! skip
	  && (finfo->info->strip == strip_some
	      || finfo->info->discard == discard_l))
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];

	  name = _bfd_coff_internal_syment_name (input_bfd, &isym, buf);
	  if (name == NULL)
	    return false;

	  if (! dont_skip_symbol
	      && ((finfo->info->strip == strip_some
		   && (bfd_hash_lookup (finfo->info->keep_hash, name, false,
				    false) == NULL))
		   || (! global
		       && finfo->info->discard == discard_l
		       && bfd_is_local_label_name (input_bfd, name))))
	    skip = true;
	}

      /* If this is an enum, struct, or union tag, see if we have
         already output an identical type.  */
      if (! skip
	  && (finfo->output_bfd->flags & BFD_TRADITIONAL_FORMAT) == 0
	  && (isym.n_sclass == C_ENTAG
	      || isym.n_sclass == C_STRTAG
	      || isym.n_sclass == C_UNTAG)
	  && isym.n_numaux == 1)
	{
	  const char *name;
	  char buf[SYMNMLEN + 1];
	  struct coff_debug_merge_hash_entry *mh;
	  struct coff_debug_merge_type *mt;
	  union internal_auxent aux;
	  struct coff_debug_merge_element **epp;
	  bfd_byte *esl, *eslend;
	  struct internal_syment *islp;

	  name = _bfd_coff_internal_syment_name (input_bfd, &isym, buf);
	  if (name == NULL)
	    return false;

	  /* Ignore fake names invented by compiler; treat them all as
             the same name.  */
	  if (*name == '~' || *name == '.' || *name == '$'
	      || (*name == bfd_get_symbol_leading_char (input_bfd)
		  && (name[1] == '~' || name[1] == '.' || name[1] == '$')))
	    name = "";

	  mh = coff_debug_merge_hash_lookup (&finfo->debug_merge, name,
					     true, true);
	  if (mh == NULL)
	    return false;

	  /* Allocate memory to hold type information.  If this turns
             out to be a duplicate, we pass this address to
             bfd_release.  */
	  mt = ((struct coff_debug_merge_type *)
		bfd_alloc (input_bfd,
			   sizeof (struct coff_debug_merge_type)));
	  if (mt == NULL)
	    return false;
	  mt->class = isym.n_sclass;

	  /* Pick up the aux entry, which points to the end of the tag
             entries.  */
	  bfd_coff_swap_aux_in (input_bfd, (PTR) (esym + isymesz),
				isym.n_type, isym.n_sclass, 0, isym.n_numaux,
				(PTR) &aux);

	  /* Gather the elements.  */
	  epp = &mt->elements;
	  mt->elements = NULL;
	  islp = isymp + 2;
	  esl = esym + 2 * isymesz;
	  eslend = ((bfd_byte *) obj_coff_external_syms (input_bfd)
		    + aux.x_sym.x_fcnary.x_fcn.x_endndx.l * isymesz);
	  while (esl < eslend)
	    {
	      const char *elename;
	      char elebuf[SYMNMLEN + 1];
	      char *name_copy;

	      bfd_coff_swap_sym_in (input_bfd, (PTR) esl, (PTR) islp);

	      *epp = ((struct coff_debug_merge_element *)
		      bfd_alloc (input_bfd,
				 sizeof (struct coff_debug_merge_element)));
	      if (*epp == NULL)
		return false;

	      elename = _bfd_coff_internal_syment_name (input_bfd, islp,
							elebuf);
	      if (elename == NULL)
		return false;

	      name_copy = (char *) bfd_alloc (input_bfd,
					      strlen (elename) + 1);
	      if (name_copy == NULL)
		return false;
	      strcpy (name_copy, elename);

	      (*epp)->name = name_copy;
	      (*epp)->type = islp->n_type;
	      (*epp)->tagndx = 0;
	      if (islp->n_numaux >= 1
		  && islp->n_type != T_NULL
		  && islp->n_sclass != C_EOS)
		{
		  union internal_auxent eleaux;
		  long indx;

		  bfd_coff_swap_aux_in (input_bfd, (PTR) (esl + isymesz),
					islp->n_type, islp->n_sclass, 0,
					islp->n_numaux, (PTR) &eleaux);
		  indx = eleaux.x_sym.x_tagndx.l;

		  /* FIXME: If this tagndx entry refers to a symbol
		     defined later in this file, we just ignore it.
		     Handling this correctly would be tedious, and may
		     not be required.  */

		  if (indx > 0
		      && (indx
			  < ((esym -
			      (bfd_byte *) obj_coff_external_syms (input_bfd))
			     / (long) isymesz)))
		    {
		      (*epp)->tagndx = finfo->sym_indices[indx];
		      if ((*epp)->tagndx < 0)
			(*epp)->tagndx = 0;
		    }
		}
	      epp = &(*epp)->next;
	      *epp = NULL;

	      esl += (islp->n_numaux + 1) * isymesz;
	      islp += islp->n_numaux + 1;
	    }

	  /* See if we already have a definition which matches this
             type.  We always output the type if it has no elements,
             for simplicity.  */
	  if (mt->elements == NULL)
	    bfd_release (input_bfd, (PTR) mt);
	  else
	    {
	      struct coff_debug_merge_type *mtl;

	      for (mtl = mh->types; mtl != NULL; mtl = mtl->next)
		{
		  struct coff_debug_merge_element *me, *mel;

		  if (mtl->class != mt->class)
		    continue;

		  for (me = mt->elements, mel = mtl->elements;
		       me != NULL && mel != NULL;
		       me = me->next, mel = mel->next)
		    {
		      if (strcmp (me->name, mel->name) != 0
			  || me->type != mel->type
			  || me->tagndx != mel->tagndx)
			break;
		    }

		  if (me == NULL && mel == NULL)
		    break;
		}

	      if (mtl == NULL || (bfd_size_type) mtl->indx >= syment_base)
		{
		  /* This is the first definition of this type.  */
		  mt->indx = output_index;
		  mt->next = mh->types;
		  mh->types = mt;
		}
	      else
		{
		  /* This is a redefinition which can be merged.  */
		  bfd_release (input_bfd, (PTR) mt);
		  *indexp = mtl->indx;
		  add = (eslend - esym) / isymesz;
		  skip = true;
		}
	    }
	}

      /* We now know whether we are to skip this symbol or not.  */
      if (! skip)
	{
	  /* Adjust the symbol in order to output it.  */

	  if (isym._n._n_n._n_zeroes == 0
	      && isym._n._n_n._n_offset != 0)
	    {
	      const char *name;
	      bfd_size_type indx;

	      /* This symbol has a long name.  Enter it in the string
		 table we are building.  Note that we do not check
		 bfd_coff_symname_in_debug.  That is only true for
		 XCOFF, and XCOFF requires different linking code
		 anyhow.  */
	      name = _bfd_coff_internal_syment_name (input_bfd, &isym,
						     (char *) NULL);
	      if (name == NULL)
		return false;
	      indx = _bfd_stringtab_add (finfo->strtab, name, hash, copy);
	      if (indx == (bfd_size_type) -1)
		return false;
	      isym._n._n_n._n_offset = STRING_SIZE_SIZE + indx;
	    }

	  switch (isym.n_sclass)
	    {
	    case C_AUTO:
	    case C_MOS:
	    case C_EOS:
	    case C_MOE:
	    case C_MOU:
	    case C_UNTAG:
	    case C_STRTAG:
	    case C_ENTAG:
	    case C_TPDEF:
	    case C_ARG:
	    case C_USTATIC:
	    case C_REG:
	    case C_REGPARM:
	    case C_FIELD:
	      /* The symbol value should not be modified.  */
	      break;

	    case C_FCN:
	      if (obj_pe (input_bfd)
		  && strcmp (isym.n_name, ".bf") != 0
		  && isym.n_scnum > 0)
		{
		  /* For PE, .lf and .ef get their value left alone,
		     while .bf gets relocated.  However, they all have
		     "real" section numbers, and need to be moved into
		     the new section.  */
		  isym.n_scnum = (*secpp)->output_section->target_index;
		  break;
		}
	      /* Fall through.  */
	    default:
	    case C_LABEL:  /* Not completely sure about these 2 */
	    case C_EXTDEF:
	    case C_BLOCK:
	    case C_EFCN:
	    case C_NULL:
	    case C_EXT:
	    case C_STAT:
	    case C_SECTION:
	    case C_NT_WEAK:
	      /* Compute new symbol location.  */
	    if (isym.n_scnum > 0)
	      {
		isym.n_scnum = (*secpp)->output_section->target_index;
		isym.n_value += (*secpp)->output_offset;
		if (! obj_pe (input_bfd))
		  isym.n_value -= (*secpp)->vma;
		if (! obj_pe (finfo->output_bfd))
		  isym.n_value += (*secpp)->output_section->vma;
	      }
	    break;

	    case C_FILE:
	      /* The value of a C_FILE symbol is the symbol index of
		 the next C_FILE symbol.  The value of the last C_FILE
		 symbol is the symbol index to the first external
		 symbol (actually, coff_renumber_symbols does not get
		 this right--it just sets the value of the last C_FILE
		 symbol to zero--and nobody has ever complained about
		 it).  We try to get this right, below, just before we
		 write the symbols out, but in the general case we may
		 have to write the symbol out twice.  */

	      if (finfo->last_file_index != -1
		  && finfo->last_file.n_value != (long) output_index)
		{
		  /* We must correct the value of the last C_FILE
                     entry.  */
		  finfo->last_file.n_value = output_index;
		  if ((bfd_size_type) finfo->last_file_index >= syment_base)
		    {
		      /* The last C_FILE symbol is in this input file.  */
		      bfd_coff_swap_sym_out (output_bfd,
					     (PTR) &finfo->last_file,
					     (PTR) (finfo->outsyms
						    + ((finfo->last_file_index
							- syment_base)
						       * osymesz)));
		    }
		  else
		    {
		      /* We have already written out the last C_FILE
			 symbol.  We need to write it out again.  We
			 borrow *outsym temporarily.  */
		      bfd_coff_swap_sym_out (output_bfd,
					     (PTR) &finfo->last_file,
					     (PTR) outsym);
		      if (bfd_seek (output_bfd,
				    (obj_sym_filepos (output_bfd)
				     + finfo->last_file_index * osymesz),
				    SEEK_SET) != 0
			  || (bfd_write (outsym, osymesz, 1, output_bfd)
			      != osymesz))
			return false;
		    }
		}

	      finfo->last_file_index = output_index;
	      finfo->last_file = isym;
	      break;
	    }

	  /* If doing task linking, convert normal global function symbols to
	     static functions. */

	  if (finfo->info->task_link
	      && (isym.n_sclass == C_EXT
		  || isym.n_sclass == C_WEAKEXT
		  || (obj_pe (input_bfd) && isym.n_sclass == C_NT_WEAK)))
	    isym.n_sclass = C_STAT;

	  /* Output the symbol.  */

	  bfd_coff_swap_sym_out (output_bfd, (PTR) &isym, (PTR) outsym);

	  *indexp = output_index;

	  if (global)
	    {
	      long indx;
	      struct coff_link_hash_entry *h;

	      indx = ((esym - (bfd_byte *) obj_coff_external_syms (input_bfd))
		      / isymesz);
	      h = obj_coff_sym_hashes (input_bfd)[indx];
	      if (h == NULL)
		{
		  /* This can happen if there were errors earlier in
                     the link.  */
		  bfd_set_error (bfd_error_bad_value);
		  return false;
		}
	      h->indx = output_index;
	    }

	  output_index += add;
	  outsym += add * osymesz;
	}

      esym += add * isymesz;
      isymp += add;
      ++secpp;
      ++indexp;
      for (--add; add > 0; --add)
	{
	  *secpp++ = NULL;
	  *indexp++ = -1;
	}
    }

  /* Fix up the aux entries.  This must be done in a separate pass,
     because we don't know the correct symbol indices until we have
     already decided which symbols we are going to keep.  */

  esym = (bfd_byte *) obj_coff_external_syms (input_bfd);
  esym_end = esym + obj_raw_syment_count (input_bfd) * isymesz;
  isymp = finfo->internal_syms;
  indexp = finfo->sym_indices;
  sym_hash = obj_coff_sym_hashes (input_bfd);
  outsym = finfo->outsyms;
  while (esym < esym_end)
    {
      int add;

      add = 1 + isymp->n_numaux;

      if ((*indexp < 0
	   || (bfd_size_type) *indexp < syment_base)
	  && (*sym_hash == NULL
	      || (*sym_hash)->auxbfd != input_bfd))
	esym += add * isymesz;
      else
	{
	  struct coff_link_hash_entry *h;
	  int i;

	  h = NULL;
	  if (*indexp < 0)
	    {
	      h = *sym_hash;

	      /* The m68k-motorola-sysv assembler will sometimes
                 generate two symbols with the same name, but only one
                 will have aux entries.  */
	      BFD_ASSERT (isymp->n_numaux == 0
			  || h->numaux == isymp->n_numaux);
	    }

	  esym += isymesz;

	  if (h == NULL)
	    outsym += osymesz;

	  /* Handle the aux entries.  This handling is based on
	     coff_pointerize_aux.  I don't know if it always correct.  */
	  for (i = 0; i < isymp->n_numaux && esym < esym_end; i++)
	    {
	      union internal_auxent aux;
	      union internal_auxent *auxp;

	      if (h != NULL)
		auxp = h->aux + i;
	      else
		{
		  bfd_coff_swap_aux_in (input_bfd, (PTR) esym, isymp->n_type,
					isymp->n_sclass, i, isymp->n_numaux,
					(PTR) &aux);
		  auxp = &aux;
		}

	      if (isymp->n_sclass == C_FILE)
		{
		  /* If this is a long filename, we must put it in the
		     string table.  */
		  if (auxp->x_file.x_n.x_zeroes == 0
		      && auxp->x_file.x_n.x_offset != 0)
		    {
		      const char *filename;
		      bfd_size_type indx;

		      BFD_ASSERT (auxp->x_file.x_n.x_offset
				  >= STRING_SIZE_SIZE);
		      if (strings == NULL)
			{
			  strings = _bfd_coff_read_string_table (input_bfd);
			  if (strings == NULL)
			    return false;
			}
		      filename = strings + auxp->x_file.x_n.x_offset;
		      indx = _bfd_stringtab_add (finfo->strtab, filename,
						 hash, copy);
		      if (indx == (bfd_size_type) -1)
			return false;
		      auxp->x_file.x_n.x_offset = STRING_SIZE_SIZE + indx;
		    }
		}
	      else if (isymp->n_sclass != C_STAT || isymp->n_type != T_NULL)
		{
		  unsigned long indx;

		  if (ISFCN (isymp->n_type)
		      || ISTAG (isymp->n_sclass)
		      || isymp->n_sclass == C_BLOCK
		      || isymp->n_sclass == C_FCN)
		    {
		      indx = auxp->x_sym.x_fcnary.x_fcn.x_endndx.l;
		      if (indx > 0
			  && indx < obj_raw_syment_count (input_bfd))
			{
			  /* We look forward through the symbol for
                             the index of the next symbol we are going
                             to include.  I don't know if this is
                             entirely right.  */
			  while ((finfo->sym_indices[indx] < 0
				  || ((bfd_size_type) finfo->sym_indices[indx]
				      < syment_base))
				 && indx < obj_raw_syment_count (input_bfd))
			    ++indx;
			  if (indx >= obj_raw_syment_count (input_bfd))
			    indx = output_index;
			  else
			    indx = finfo->sym_indices[indx];
			  auxp->x_sym.x_fcnary.x_fcn.x_endndx.l = indx;
			}
		    }

		  indx = auxp->x_sym.x_tagndx.l;
		  if (indx > 0 && indx < obj_raw_syment_count (input_bfd))
		    {
		      long symindx;

		      symindx = finfo->sym_indices[indx];
		      if (symindx < 0)
			auxp->x_sym.x_tagndx.l = 0;
		      else
			auxp->x_sym.x_tagndx.l = symindx;
		    }

		  /* The .bf symbols are supposed to be linked through
		     the endndx field.  We need to carry this list
		     across object files.  */
		  if (i == 0
		      && h == NULL
		      && isymp->n_sclass == C_FCN
		      && (isymp->_n._n_n._n_zeroes != 0
			  || isymp->_n._n_n._n_offset == 0)
		      && isymp->_n._n_name[0] == '.'
		      && isymp->_n._n_name[1] == 'b'
		      && isymp->_n._n_name[2] == 'f'
		      && isymp->_n._n_name[3] == '\0')
		    {
		      if (finfo->last_bf_index != -1)
			{
			  finfo->last_bf.x_sym.x_fcnary.x_fcn.x_endndx.l =
			    *indexp;

			  if ((bfd_size_type) finfo->last_bf_index
			      >= syment_base)
			    {
			      PTR auxout;

			      /* The last .bf symbol is in this input
				 file.  This will only happen if the
				 assembler did not set up the .bf
				 endndx symbols correctly.  */
			      auxout = (PTR) (finfo->outsyms
					      + ((finfo->last_bf_index
						  - syment_base)
						 * osymesz));
			      bfd_coff_swap_aux_out (output_bfd,
						     (PTR) &finfo->last_bf,
						     isymp->n_type,
						     isymp->n_sclass,
						     0, isymp->n_numaux,
						     auxout);
			    }
			  else
			    {
			      /* We have already written out the last
                                 .bf aux entry.  We need to write it
                                 out again.  We borrow *outsym
                                 temporarily.  FIXME: This case should
                                 be made faster.  */
			      bfd_coff_swap_aux_out (output_bfd,
						     (PTR) &finfo->last_bf,
						     isymp->n_type,
						     isymp->n_sclass,
						     0, isymp->n_numaux,
						     (PTR) outsym);
			      if (bfd_seek (output_bfd,
					    (obj_sym_filepos (output_bfd)
					     + finfo->last_bf_index * osymesz),
					    SEEK_SET) != 0
				  || bfd_write (outsym, osymesz, 1,
						output_bfd) != osymesz)
				return false;
			    }
			}

		      if (auxp->x_sym.x_fcnary.x_fcn.x_endndx.l != 0)
			finfo->last_bf_index = -1;
		      else
			{
			  /* The endndx field of this aux entry must
                             be updated with the symbol number of the
                             next .bf symbol.  */
			  finfo->last_bf = *auxp;
			  finfo->last_bf_index = (((outsym - finfo->outsyms)
						   / osymesz)
						  + syment_base);
			}
		    }
		}

	      if (h == NULL)
		{
		  bfd_coff_swap_aux_out (output_bfd, (PTR) auxp, isymp->n_type,
					 isymp->n_sclass, i, isymp->n_numaux,
					 (PTR) outsym);
		  outsym += osymesz;
		}

	      esym += isymesz;
	    }
	}

      indexp += add;
      isymp += add;
      sym_hash += add;
    }

  /* Relocate the line numbers, unless we are stripping them.  */
  if (finfo->info->strip == strip_none
      || finfo->info->strip == strip_some)
    {
      for (o = input_bfd->sections; o != NULL; o = o->next)
	{
	  bfd_vma offset;
	  bfd_byte *eline;
	  bfd_byte *elineend;
	  bfd_byte *oeline;
	  boolean skipping;

	  /* FIXME: If SEC_HAS_CONTENTS is not for the section, then
	     build_link_order in ldwrite.c will not have created a
	     link order, which means that we will not have seen this
	     input section in _bfd_coff_final_link, which means that
	     we will not have allocated space for the line numbers of
	     this section.  I don't think line numbers can be
	     meaningful for a section which does not have
	     SEC_HAS_CONTENTS set, but, if they do, this must be
	     changed.  */
	  if (o->lineno_count == 0
	      || (o->output_section->flags & SEC_HAS_CONTENTS) == 0)
	    continue;

	  if (bfd_seek (input_bfd, o->line_filepos, SEEK_SET) != 0
	      || bfd_read (finfo->linenos, linesz, o->lineno_count,
			   input_bfd) != linesz * o->lineno_count)
	    return false;

	  offset = o->output_section->vma + o->output_offset - o->vma;
	  eline = finfo->linenos;
	  oeline = finfo->linenos;
	  elineend = eline + linesz * o->lineno_count;
	  skipping = false;
	  for (; eline < elineend; eline += linesz)
	    {
	      struct internal_lineno iline;

	      bfd_coff_swap_lineno_in (input_bfd, (PTR) eline, (PTR) &iline);

	      if (iline.l_lnno != 0)
		iline.l_addr.l_paddr += offset;
	      else if (iline.l_addr.l_symndx >= 0
		       && ((unsigned long) iline.l_addr.l_symndx
			   < obj_raw_syment_count (input_bfd)))
		{
		  long indx;

		  indx = finfo->sym_indices[iline.l_addr.l_symndx];

		  if (indx < 0)
		    {
		      /* These line numbers are attached to a symbol
			 which we are stripping.  We must discard the
			 line numbers because reading them back with
			 no associated symbol (or associating them all
			 with symbol #0) will fail.  We can't regain
			 the space in the output file, but at least
			 they're dense.  */
		      skipping = true;
		    }
		  else
		    {
		      struct internal_syment is;
		      union internal_auxent ia;

		      /* Fix up the lnnoptr field in the aux entry of
			 the symbol.  It turns out that we can't do
			 this when we modify the symbol aux entries,
			 because gas sometimes screws up the lnnoptr
			 field and makes it an offset from the start
			 of the line numbers rather than an absolute
			 file index.  */
		      bfd_coff_swap_sym_in (output_bfd,
					    (PTR) (finfo->outsyms
						   + ((indx - syment_base)
						      * osymesz)),
					    (PTR) &is);
		      if ((ISFCN (is.n_type)
			   || is.n_sclass == C_BLOCK)
			  && is.n_numaux >= 1)
			{
			  PTR auxptr;

			  auxptr = (PTR) (finfo->outsyms
					  + ((indx - syment_base + 1)
					     * osymesz));
			  bfd_coff_swap_aux_in (output_bfd, auxptr,
						is.n_type, is.n_sclass,
						0, is.n_numaux, (PTR) &ia);
			  ia.x_sym.x_fcnary.x_fcn.x_lnnoptr =
			    (o->output_section->line_filepos
			     + o->output_section->lineno_count * linesz
			     + eline - finfo->linenos);
			  bfd_coff_swap_aux_out (output_bfd, (PTR) &ia,
						 is.n_type, is.n_sclass, 0,
						 is.n_numaux, auxptr);
			}

			skipping = false;
		    }

		  iline.l_addr.l_symndx = indx;
		}

	      if (!skipping)
	        {
		  bfd_coff_swap_lineno_out (output_bfd, (PTR) &iline,
					    (PTR) oeline);
		  oeline += linesz;
		}
	    }

	  if (bfd_seek (output_bfd,
			(o->output_section->line_filepos
			 + o->output_section->lineno_count * linesz),
			SEEK_SET) != 0
	      || (bfd_write (finfo->linenos, 1, oeline - finfo->linenos,
			     output_bfd)
		  != (bfd_size_type) (oeline - finfo->linenos)))
	    return false;

	  o->output_section->lineno_count +=
	    (oeline - finfo->linenos) / linesz;
	}
    }

  /* If we swapped out a C_FILE symbol, guess that the next C_FILE
     symbol will be the first symbol in the next input file.  In the
     normal case, this will save us from writing out the C_FILE symbol
     again.  */
  if (finfo->last_file_index != -1
      && (bfd_size_type) finfo->last_file_index >= syment_base)
    {
      finfo->last_file.n_value = output_index;
      bfd_coff_swap_sym_out (output_bfd, (PTR) &finfo->last_file,
			     (PTR) (finfo->outsyms
 				    + ((finfo->last_file_index - syment_base)
 				       * osymesz)));
    }

  /* Write the modified symbols to the output file.  */
  if (outsym > finfo->outsyms)
    {
      if (bfd_seek (output_bfd,
		    obj_sym_filepos (output_bfd) + syment_base * osymesz,
		    SEEK_SET) != 0
	  || (bfd_write (finfo->outsyms, outsym - finfo->outsyms, 1,
			output_bfd)
	      != (bfd_size_type) (outsym - finfo->outsyms)))
	return false;

      BFD_ASSERT ((obj_raw_syment_count (output_bfd)
		   + (outsym - finfo->outsyms) / osymesz)
		  == output_index);

      obj_raw_syment_count (output_bfd) = output_index;
    }

  /* Relocate the contents of each section.  */
  adjust_symndx = coff_backend_info (input_bfd)->_bfd_coff_adjust_symndx;
  for (o = input_bfd->sections; o != NULL; o = o->next)
    {
      bfd_byte *contents;
      struct coff_section_tdata *secdata;

      if (! o->linker_mark)
	{
	  /* This section was omitted from the link.  */
	  continue;
	}

      if ((o->flags & SEC_HAS_CONTENTS) == 0
	  || (o->_raw_size == 0 && (o->flags & SEC_RELOC) == 0))
	{
	  if ((o->flags & SEC_RELOC) != 0
	      && o->reloc_count != 0)
	    {
	      ((*_bfd_error_handler)
	       (_("%s: relocs in section `%s', but it has no contents"),
		bfd_get_filename (input_bfd),
		bfd_get_section_name (input_bfd, o)));
	      bfd_set_error (bfd_error_no_contents);
	      return false;
	    }

	  continue;
	}

      secdata = coff_section_data (input_bfd, o);
      if (secdata != NULL && secdata->contents != NULL)
	contents = secdata->contents;
      else
	{
	  if (! bfd_get_section_contents (input_bfd, o, finfo->contents,
					  (file_ptr) 0, o->_raw_size))
	    return false;
	  contents = finfo->contents;
	}

      if ((o->flags & SEC_RELOC) != 0)
	{
	  int target_index;
	  struct internal_reloc *internal_relocs;
	  struct internal_reloc *irel;

	  /* Read in the relocs.  */
	  target_index = o->output_section->target_index;
	  internal_relocs = (_bfd_coff_read_internal_relocs
			     (input_bfd, o, false, finfo->external_relocs,
			      finfo->info->relocateable,
			      (finfo->info->relocateable
			       ? (finfo->section_info[target_index].relocs
				  + o->output_section->reloc_count)
			       : finfo->internal_relocs)));
	  if (internal_relocs == NULL)
	    return false;

	  /* Call processor specific code to relocate the section
             contents.  */
	  if (! bfd_coff_relocate_section (output_bfd, finfo->info,
					   input_bfd, o,
					   contents,
					   internal_relocs,
					   finfo->internal_syms,
					   finfo->sec_ptrs))
	    return false;

	  if (finfo->info->relocateable)
	    {
	      bfd_vma offset;
	      struct internal_reloc *irelend;
	      struct coff_link_hash_entry **rel_hash;

	      offset = o->output_section->vma + o->output_offset - o->vma;
	      irel = internal_relocs;
	      irelend = irel + o->reloc_count;
	      rel_hash = (finfo->section_info[target_index].rel_hashes
			  + o->output_section->reloc_count);
	      for (; irel < irelend; irel++, rel_hash++)
		{
		  struct coff_link_hash_entry *h;
		  boolean adjusted;

		  *rel_hash = NULL;

		  /* Adjust the reloc address and symbol index.  */

		  irel->r_vaddr += offset;

		  if (irel->r_symndx == -1)
		    continue;

		  if (adjust_symndx)
		    {
		      if (! (*adjust_symndx) (output_bfd, finfo->info,
					      input_bfd, o, irel,
					      &adjusted))
			return false;
		      if (adjusted)
			continue;
		    }

		  h = obj_coff_sym_hashes (input_bfd)[irel->r_symndx];
		  if (h != NULL)
		    {
		      /* This is a global symbol.  */
		      if (h->indx >= 0)
			irel->r_symndx = h->indx;
		      else
			{
			  /* This symbol is being written at the end
			     of the file, and we do not yet know the
			     symbol index.  We save the pointer to the
			     hash table entry in the rel_hash list.
			     We set the indx field to -2 to indicate
			     that this symbol must not be stripped.  */
			  *rel_hash = h;
			  h->indx = -2;
			}
		    }
		  else
		    {
		      long indx;

		      indx = finfo->sym_indices[irel->r_symndx];
		      if (indx != -1)
			irel->r_symndx = indx;
		      else
			{
			  struct internal_syment *is;
			  const char *name;
			  char buf[SYMNMLEN + 1];

			  /* This reloc is against a symbol we are
                             stripping.  This should have been handled
			     by the 'dont_skip_symbol' code in the while
			     loop at the top of this function. */
			  
			  is = finfo->internal_syms + irel->r_symndx;

			  name = (_bfd_coff_internal_syment_name
				  (input_bfd, is, buf));
			  if (name == NULL)
			    return false;

			  if (! ((*finfo->info->callbacks->unattached_reloc)
				 (finfo->info, name, input_bfd, o,
				  irel->r_vaddr)))
			    return false;
			}
		    }
		}

	      o->output_section->reloc_count += o->reloc_count;
	    }
	}

      /* Write out the modified section contents.  */
      if (secdata == NULL || secdata->stab_info == NULL)
	{
	  if (! bfd_set_section_contents (output_bfd, o->output_section,
					  contents, 
                                          (file_ptr) 
                                          (o->output_offset * 
                                           bfd_octets_per_byte (output_bfd)),
					  (o->_cooked_size != 0
					   ? o->_cooked_size
					   : o->_raw_size)))
	    return false;
	}
      else
	{
	  if (! (_bfd_write_section_stabs
		 (output_bfd, &coff_hash_table (finfo->info)->stab_info,
		  o, &secdata->stab_info, contents)))
	    return false;
	}
    }

  if (! finfo->info->keep_memory)
    {
      if (! _bfd_coff_free_symbols (input_bfd))
	return false;
    }

  return true;
}

/* Write out a global symbol.  Called via coff_link_hash_traverse.  */

boolean
_bfd_coff_write_global_sym (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_final_link_info *finfo = (struct coff_final_link_info *) data;
  bfd *output_bfd;
  struct internal_syment isym;
  bfd_size_type symesz;
  unsigned int i;

  output_bfd = finfo->output_bfd;

  if (h->indx >= 0)
    return true;

  if (h->indx != -2
      && (finfo->info->strip == strip_all
	  || (finfo->info->strip == strip_some
	      && (bfd_hash_lookup (finfo->info->keep_hash,
				   h->root.root.string, false, false)
		  == NULL))))
    return true;

  switch (h->root.type)
    {
    default:
    case bfd_link_hash_new:
      abort ();
      return false;

    case bfd_link_hash_undefined:
    case bfd_link_hash_undefweak:
      isym.n_scnum = N_UNDEF;
      isym.n_value = 0;
      break;

    case bfd_link_hash_defined:
    case bfd_link_hash_defweak:
      {
	asection *sec;

	sec = h->root.u.def.section->output_section;
	if (bfd_is_abs_section (sec))
	  isym.n_scnum = N_ABS;
	else
	  isym.n_scnum = sec->target_index;
	isym.n_value = (h->root.u.def.value
			+ h->root.u.def.section->output_offset);
	if (! obj_pe (finfo->output_bfd))
	  isym.n_value += sec->vma;
      }
      break;

    case bfd_link_hash_common:
      isym.n_scnum = N_UNDEF;
      isym.n_value = h->root.u.c.size;
      break;

    case bfd_link_hash_indirect:
    case bfd_link_hash_warning:
      /* Just ignore these.  They can't be handled anyhow.  */
      return true;
    }

  if (strlen (h->root.root.string) <= SYMNMLEN)
    strncpy (isym._n._n_name, h->root.root.string, SYMNMLEN);
  else
    {
      boolean hash;
      bfd_size_type indx;

      hash = true;
      if ((output_bfd->flags & BFD_TRADITIONAL_FORMAT) != 0)
	hash = false;
      indx = _bfd_stringtab_add (finfo->strtab, h->root.root.string, hash,
				 false);
      if (indx == (bfd_size_type) -1)
	{
	  finfo->failed = true;
	  return false;
	}
      isym._n._n_n._n_zeroes = 0;
      isym._n._n_n._n_offset = STRING_SIZE_SIZE + indx;
    }

  isym.n_sclass = h->class;
  isym.n_type = h->type;

  if (isym.n_sclass == C_NULL)
    isym.n_sclass = C_EXT;

  /* If doing task linking and this is the pass where we convert
     defined globals to statics, then do that conversion now.  If the
     symbol is not being converted, just ignore it and it will be
     output during a later pass. */
  if (finfo->global_to_static)
    {
      if (isym.n_sclass != C_EXT
	  && isym.n_sclass != C_WEAKEXT
	  && (! obj_pe (output_bfd) || isym.n_sclass != C_NT_WEAK))
	{
	  return true;
	}
      isym.n_sclass = C_STAT;
    }

  isym.n_numaux = h->numaux;
  
  bfd_coff_swap_sym_out (output_bfd, (PTR) &isym, (PTR) finfo->outsyms);

  symesz = bfd_coff_symesz (output_bfd);

  if (bfd_seek (output_bfd,
		(obj_sym_filepos (output_bfd)
		 + obj_raw_syment_count (output_bfd) * symesz),
		SEEK_SET) != 0
      || bfd_write (finfo->outsyms, symesz, 1, output_bfd) != symesz)
    {
      finfo->failed = true;
      return false;
    }

  h->indx = obj_raw_syment_count (output_bfd);

  ++obj_raw_syment_count (output_bfd);

  /* Write out any associated aux entries.  Most of the aux entries
     will have been modified in _bfd_coff_link_input_bfd.  We have to
     handle section aux entries here, now that we have the final
     relocation and line number counts.  */
  for (i = 0; i < isym.n_numaux; i++)
    {
      union internal_auxent *auxp;

      auxp = h->aux + i;

      /* Look for a section aux entry here using the same tests that
         coff_swap_aux_out uses.  */
      if (i == 0
	  && (isym.n_sclass == C_STAT
	      || isym.n_sclass == C_HIDDEN)
	  && isym.n_type == T_NULL
	  && (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak))
	{
	  asection *sec;

	  sec = h->root.u.def.section->output_section;
	  if (sec != NULL)
	    {
	      auxp->x_scn.x_scnlen = (sec->_cooked_size != 0
				      ? sec->_cooked_size
				      : sec->_raw_size);

	      /* For PE, an overflow on the final link reportedly does
                 not matter.  FIXME: Why not?  */

	      if (sec->reloc_count > 0xffff
		  && (! obj_pe (output_bfd)
		      || finfo->info->relocateable))
		(*_bfd_error_handler)
		  (_("%s: %s: reloc overflow: 0x%lx > 0xffff"),
		   bfd_get_filename (output_bfd),
		   bfd_get_section_name (output_bfd, sec),
		   sec->reloc_count);

	      if (sec->lineno_count > 0xffff
		  && (! obj_pe (output_bfd)
		      || finfo->info->relocateable))
		(*_bfd_error_handler)
		  (_("%s: warning: %s: line number overflow: 0x%lx > 0xffff"),
		   bfd_get_filename (output_bfd),
		   bfd_get_section_name (output_bfd, sec),
		   sec->lineno_count);

	      auxp->x_scn.x_nreloc = sec->reloc_count;
	      auxp->x_scn.x_nlinno = sec->lineno_count;
	      auxp->x_scn.x_checksum = 0;
	      auxp->x_scn.x_associated = 0;
	      auxp->x_scn.x_comdat = 0;
	    }
	}

      bfd_coff_swap_aux_out (output_bfd, (PTR) auxp, isym.n_type,
			     isym.n_sclass, i, isym.n_numaux,
			     (PTR) finfo->outsyms);
      if (bfd_write (finfo->outsyms, symesz, 1, output_bfd) != symesz)
	{
	  finfo->failed = true;
	  return false;
	}
      ++obj_raw_syment_count (output_bfd);
    }

  return true;
}

/* Write out task global symbols, converting them to statics.  Called
   via coff_link_hash_traverse.  Calls bfd_coff_write_global_sym to do
   the dirty work, if the symbol we are processing needs conversion. */

boolean
_bfd_coff_write_task_globals (h, data)
     struct coff_link_hash_entry *h;
     PTR data;
{
  struct coff_final_link_info *finfo = (struct coff_final_link_info *) data;
  boolean rtnval = true;
  boolean save_global_to_static;

  if (h->indx < 0)
    {
      switch (h->root.type)
	{
	case bfd_link_hash_defined:
	case bfd_link_hash_defweak:
	  save_global_to_static = finfo->global_to_static;
	  finfo->global_to_static = true;
	  rtnval = _bfd_coff_write_global_sym (h, data);
	  finfo->global_to_static = save_global_to_static;
	  break;
	default:
	  break;
	}
    }
  return (rtnval);
}

/* Handle a link order which is supposed to generate a reloc.  */

boolean
_bfd_coff_reloc_link_order (output_bfd, finfo, output_section, link_order)
     bfd *output_bfd;
     struct coff_final_link_info *finfo;
     asection *output_section;
     struct bfd_link_order *link_order;
{
  reloc_howto_type *howto;
  struct internal_reloc *irel;
  struct coff_link_hash_entry **rel_hash_ptr;

  howto = bfd_reloc_type_lookup (output_bfd, link_order->u.reloc.p->reloc);
  if (howto == NULL)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  if (link_order->u.reloc.p->addend != 0)
    {
      bfd_size_type size;
      bfd_byte *buf;
      bfd_reloc_status_type rstat;
      boolean ok;

      size = bfd_get_reloc_size (howto);
      buf = (bfd_byte *) bfd_zmalloc (size);
      if (buf == NULL)
	return false;

      rstat = _bfd_relocate_contents (howto, output_bfd,
				      link_order->u.reloc.p->addend, buf);
      switch (rstat)
	{
	case bfd_reloc_ok:
	  break;
	default:
	case bfd_reloc_outofrange:
	  abort ();
	case bfd_reloc_overflow:
	  if (! ((*finfo->info->callbacks->reloc_overflow)
		 (finfo->info,
		  (link_order->type == bfd_section_reloc_link_order
		   ? bfd_section_name (output_bfd,
				       link_order->u.reloc.p->u.section)
		   : link_order->u.reloc.p->u.name),
		  howto->name, link_order->u.reloc.p->addend,
		  (bfd *) NULL, (asection *) NULL, (bfd_vma) 0)))
	    {
	      free (buf);
	      return false;
	    }
	  break;
	}
      ok = bfd_set_section_contents (output_bfd, output_section, (PTR) buf,
				     (file_ptr) 
                                     (link_order->offset *
                                      bfd_octets_per_byte (output_bfd)), size);
      free (buf);
      if (! ok)
	return false;
    }

  /* Store the reloc information in the right place.  It will get
     swapped and written out at the end of the final_link routine.  */

  irel = (finfo->section_info[output_section->target_index].relocs
	  + output_section->reloc_count);
  rel_hash_ptr = (finfo->section_info[output_section->target_index].rel_hashes
		  + output_section->reloc_count);

  memset (irel, 0, sizeof (struct internal_reloc));
  *rel_hash_ptr = NULL;

  irel->r_vaddr = output_section->vma + link_order->offset;

  if (link_order->type == bfd_section_reloc_link_order)
    {
      /* We need to somehow locate a symbol in the right section.  The
         symbol must either have a value of zero, or we must adjust
         the addend by the value of the symbol.  FIXME: Write this
         when we need it.  The old linker couldn't handle this anyhow.  */
      abort ();
      *rel_hash_ptr = NULL;
      irel->r_symndx = 0;
    }
  else
    {
      struct coff_link_hash_entry *h;

      h = ((struct coff_link_hash_entry *)
	   bfd_wrapped_link_hash_lookup (output_bfd, finfo->info,
					 link_order->u.reloc.p->u.name,
					 false, false, true));
      if (h != NULL)
	{
	  if (h->indx >= 0)
	    irel->r_symndx = h->indx;
	  else
	    {
	      /* Set the index to -2 to force this symbol to get
		 written out.  */
	      h->indx = -2;
	      *rel_hash_ptr = h;
	      irel->r_symndx = 0;
	    }
	}
      else
	{
	  if (! ((*finfo->info->callbacks->unattached_reloc)
		 (finfo->info, link_order->u.reloc.p->u.name, (bfd *) NULL,
		  (asection *) NULL, (bfd_vma) 0)))
	    return false;
	  irel->r_symndx = 0;
	}
    }

  /* FIXME: Is this always right?  */
  irel->r_type = howto->type;

  /* r_size is only used on the RS/6000, which needs its own linker
     routines anyhow.  r_extern is only used for ECOFF.  */

  /* FIXME: What is the right value for r_offset?  Is zero OK?  */

  ++output_section->reloc_count;

  return true;
}

/* A basic reloc handling routine which may be used by processors with
   simple relocs.  */

boolean
_bfd_coff_generic_relocate_section (output_bfd, info, input_bfd,
				    input_section, contents, relocs, syms,
				    sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  struct internal_reloc *rel;
  struct internal_reloc *relend;

  rel = relocs;
  relend = rel + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend;
      bfd_vma val;
      reloc_howto_type *howto;
      bfd_reloc_status_type rstat;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else if (symndx < 0
	       || (unsigned long) symndx >= obj_raw_syment_count (input_bfd))
	{
	  (*_bfd_error_handler)
	    ("%s: illegal symbol index %ld in relocs",
	     bfd_get_filename (input_bfd), symndx);
	  return false;
	}
      else
	{    
	  h = obj_coff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	}

      /* COFF treats common symbols in one of two ways.  Either the
         size of the symbol is included in the section contents, or it
         is not.  We assume that the size is not included, and force
         the rtype_to_howto function to adjust the addend as needed.  */

      if (sym != NULL && sym->n_scnum != 0)
	addend = - sym->n_value;
      else
	addend = 0;


      howto = bfd_coff_rtype_to_howto (input_bfd, input_section, rel, h,
				       sym, &addend);
      if (howto == NULL)
	return false;

      /* If we are doing a relocateable link, then we can just ignore
         a PC relative reloc that is pcrel_offset.  It will already
         have the correct value.  If this is not a relocateable link,
         then we should ignore the symbol value.  */
      if (howto->pc_relative && howto->pcrel_offset)
	{
	  if (info->relocateable)
	    continue;
	  if (sym != NULL && sym->n_scnum != 0)
	    addend += sym->n_value;
	}

      val = 0;

      if (h == NULL)
	{
	  asection *sec;

	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
              val = (sec->output_section->vma
		     + sec->output_offset
		     + sym->n_value);
	      if (! obj_pe (input_bfd))
		val -= sec->vma;
	    }
	}
      else
	{
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection *sec;

	      sec = h->root.u.def.section;
	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	      }

	  else if (! info->relocateable)
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma, true)))
		return false;
	    }
	}

      if (info->base_file)
	{
	  /* Emit a reloc if the backend thinks it needs it. */
	  if (sym && pe_data (output_bfd)->in_reloc_p (output_bfd, howto))
	    {
	      /* Relocation to a symbol in a section which isn't
		 absolute.  We output the address here to a file.
		 This file is then read by dlltool when generating the
		 reloc section.  Note that the base file is not
		 portable between systems.  We write out a long here,
		 and dlltool reads in a long.  */
	      long addr = (rel->r_vaddr 
			   - input_section->vma 
			   + input_section->output_offset 
			   + input_section->output_section->vma);
	      if (coff_data (output_bfd)->pe)
		addr -= pe_data(output_bfd)->pe_opthdr.ImageBase;
	      if (fwrite (&addr, 1, sizeof (long), (FILE *) info->base_file)
		  != sizeof (long))
		{
		  bfd_set_error (bfd_error_system_call);
		  return false;
		}
	    }
	}
  
      rstat = _bfd_final_link_relocate (howto, input_bfd, input_section,
					contents,
					rel->r_vaddr - input_section->vma,
					val, addend);

      switch (rstat)
	{
	default:
	  abort ();
	case bfd_reloc_ok:
	  break;
	case bfd_reloc_outofrange:
	  (*_bfd_error_handler)
	    (_("%s: bad reloc address 0x%lx in section `%s'"),
	     bfd_get_filename (input_bfd),
	     (unsigned long) rel->r_vaddr,
	     bfd_get_section_name (input_bfd, input_section));
	  return false;
	case bfd_reloc_overflow:
	  {
	    const char *name;
	    char buf[SYMNMLEN + 1];

	    if (symndx == -1)
	      name = "*ABS*";
	    else if (h != NULL)
	      name = h->root.root.string;
	    else
	      {
		name = _bfd_coff_internal_syment_name (input_bfd, sym, buf);
		if (name == NULL)
		  return false;
	      }

	    if (! ((*info->callbacks->reloc_overflow)
		   (info, name, howto->name, (bfd_vma) 0, input_bfd,
		    input_section, rel->r_vaddr - input_section->vma)))
	      return false;
	  }
	}
    }
  return true;
}

