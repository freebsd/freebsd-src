/* ldcref.c -- output a cross reference table
   Copyright 1996, 1997, 1998, 2000, 2002, 2003
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@cygnus.com>

This file is part of GLD, the Gnu Linker.

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

/* This file holds routines that manage the cross reference table.
   The table is used to generate cross reference reports.  It is also
   used to implement the NOCROSSREFS command in the linker script.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libiberty.h"

#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"

/* We keep an instance of this structure for each reference to a
   symbol from a given object.  */

struct cref_ref {
  /* The next reference.  */
  struct cref_ref *next;
  /* The object.  */
  bfd *abfd;
  /* True if the symbol is defined.  */
  unsigned int def : 1;
  /* True if the symbol is common.  */
  unsigned int common : 1;
  /* True if the symbol is undefined.  */
  unsigned int undef : 1;
};

/* We keep a hash table of symbols.  Each entry looks like this.  */

struct cref_hash_entry {
  struct bfd_hash_entry root;
  /* The demangled name.  */
  char *demangled;
  /* References to and definitions of this symbol.  */
  struct cref_ref *refs;
};

/* This is what the hash table looks like.  */

struct cref_hash_table {
  struct bfd_hash_table root;
};

/* Forward declarations.  */

static void output_one_cref (FILE *, struct cref_hash_entry *);
static void check_section_sym_xref (lang_input_statement_type *);
static bfd_boolean check_nocrossref (struct cref_hash_entry *, void *);
static void check_refs (const char *, asection *, bfd *,
			struct lang_nocrossrefs *);
static void check_reloc_refs (bfd *, asection *, void *);

/* Look up an entry in the cref hash table.  */

#define cref_hash_lookup(table, string, create, copy)		\
  ((struct cref_hash_entry *)					\
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* Traverse the cref hash table.  */

#define cref_hash_traverse(table, func, info)				\
  (bfd_hash_traverse							\
   (&(table)->root,							\
    (bfd_boolean (*) (struct bfd_hash_entry *, void *)) (func),		\
    (info)))

/* The cref hash table.  */

static struct cref_hash_table cref_table;

/* Whether the cref hash table has been initialized.  */

static bfd_boolean cref_initialized;

/* The number of symbols seen so far.  */

static size_t cref_symcount;

/* Create an entry in a cref hash table.  */

static struct bfd_hash_entry *
cref_hash_newfunc (struct bfd_hash_entry *entry,
		   struct bfd_hash_table *table,
		   const char *string)
{
  struct cref_hash_entry *ret = (struct cref_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct cref_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct cref_hash_entry)));
  if (ret == NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct cref_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));
  if (ret != NULL)
    {
      /* Set local fields.  */
      ret->demangled = NULL;
      ret->refs = NULL;

      /* Keep a count of the number of entries created in the hash
	 table.  */
      ++cref_symcount;
    }

  return &ret->root;
}

/* Add a symbol to the cref hash table.  This is called for every
   symbol that is seen during the link.  */

void
add_cref (const char *name,
	  bfd *abfd,
	  asection *section,
	  bfd_vma value ATTRIBUTE_UNUSED)
{
  struct cref_hash_entry *h;
  struct cref_ref *r;

  if (! cref_initialized)
    {
      if (! bfd_hash_table_init (&cref_table.root, cref_hash_newfunc))
	einfo (_("%X%P: bfd_hash_table_init of cref table failed: %E\n"));
      cref_initialized = TRUE;
    }

  h = cref_hash_lookup (&cref_table, name, TRUE, FALSE);
  if (h == NULL)
    einfo (_("%X%P: cref_hash_lookup failed: %E\n"));

  for (r = h->refs; r != NULL; r = r->next)
    if (r->abfd == abfd)
      break;

  if (r == NULL)
    {
      r = xmalloc (sizeof *r);
      r->next = h->refs;
      h->refs = r;
      r->abfd = abfd;
      r->def = FALSE;
      r->common = FALSE;
      r->undef = FALSE;
    }

  if (bfd_is_und_section (section))
    r->undef = TRUE;
  else if (bfd_is_com_section (section))
    r->common = TRUE;
  else
    r->def = TRUE;
}

/* Copy the addresses of the hash table entries into an array.  This
   is called via cref_hash_traverse.  We also fill in the demangled
   name.  */

static bfd_boolean
cref_fill_array (struct cref_hash_entry *h, void *data)
{
  struct cref_hash_entry ***pph = data;

  ASSERT (h->demangled == NULL);
  h->demangled = demangle (h->root.string);

  **pph = h;

  ++*pph;

  return TRUE;
}

/* Sort an array of cref hash table entries by name.  */

static int
cref_sort_array (const void *a1, const void *a2)
{
  const struct cref_hash_entry * const *p1 = a1;
  const struct cref_hash_entry * const *p2 = a2;

  return strcmp ((*p1)->demangled, (*p2)->demangled);
}

/* Write out the cref table.  */

#define FILECOL (50)

void
output_cref (FILE *fp)
{
  int len;
  struct cref_hash_entry **csyms, **csym_fill, **csym, **csym_end;
  const char *msg;

  fprintf (fp, _("\nCross Reference Table\n\n"));
  msg = _("Symbol");
  fprintf (fp, "%s", msg);
  len = strlen (msg);
  while (len < FILECOL)
    {
      putc (' ', fp);
      ++len;
    }
  fprintf (fp, _("File\n"));

  if (! cref_initialized)
    {
      fprintf (fp, _("No symbols\n"));
      return;
    }

  csyms = xmalloc (cref_symcount * sizeof (*csyms));

  csym_fill = csyms;
  cref_hash_traverse (&cref_table, cref_fill_array, &csym_fill);
  ASSERT ((size_t) (csym_fill - csyms) == cref_symcount);

  qsort (csyms, cref_symcount, sizeof (*csyms), cref_sort_array);

  csym_end = csyms + cref_symcount;
  for (csym = csyms; csym < csym_end; csym++)
    output_one_cref (fp, *csym);
}

/* Output one entry in the cross reference table.  */

static void
output_one_cref (FILE *fp, struct cref_hash_entry *h)
{
  int len;
  struct bfd_link_hash_entry *hl;
  struct cref_ref *r;

  hl = bfd_link_hash_lookup (link_info.hash, h->root.string, FALSE,
			     FALSE, TRUE);
  if (hl == NULL)
    einfo ("%P: symbol `%T' missing from main hash table\n",
	   h->root.string);
  else
    {
      /* If this symbol is defined in a dynamic object but never
	 referenced by a normal object, then don't print it.  */
      if (hl->type == bfd_link_hash_defined)
	{
	  if (hl->u.def.section->output_section == NULL)
	    return;
	  if (hl->u.def.section->owner != NULL
	      && (hl->u.def.section->owner->flags & DYNAMIC) != 0)
	    {
	      for (r = h->refs; r != NULL; r = r->next)
		if ((r->abfd->flags & DYNAMIC) == 0)
		  break;
	      if (r == NULL)
		return;
	    }
	}
    }

  fprintf (fp, "%s ", h->demangled);
  len = strlen (h->demangled) + 1;

  for (r = h->refs; r != NULL; r = r->next)
    {
      if (r->def)
	{
	  while (len < FILECOL)
	    {
	      putc (' ', fp);
	      ++len;
	    }
	  lfinfo (fp, "%B\n", r->abfd);
	  len = 0;
	}
    }

  for (r = h->refs; r != NULL; r = r->next)
    {
      if (! r->def)
	{
	  while (len < FILECOL)
	    {
	      putc (' ', fp);
	      ++len;
	    }
	  lfinfo (fp, "%B\n", r->abfd);
	  len = 0;
	}
    }

  ASSERT (len == 0);
}

/* Check for prohibited cross references.  */

void
check_nocrossrefs (void)
{
  if (! cref_initialized)
    return;

  cref_hash_traverse (&cref_table, check_nocrossref, NULL);

  lang_for_each_file (check_section_sym_xref);
}

/* Checks for prohibited cross references to section symbols.  */

static void
check_section_sym_xref (lang_input_statement_type *statement)
{
  bfd *abfd;
  asection *sec;

  abfd = statement->the_bfd;
  if (abfd == NULL)
    return;

  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      asection *outsec;

      outsec = sec->output_section;
      if (outsec != NULL)
	{
	  const char *outsecname;
	  struct lang_nocrossrefs *ncrs;
	  struct lang_nocrossref *ncr;

	  outsecname = outsec->name;
	  for (ncrs = nocrossref_list; ncrs != NULL; ncrs = ncrs->next)
	    for (ncr = ncrs->list; ncr != NULL; ncr = ncr->next)
	      if (strcmp (ncr->name, outsecname) == 0)
		check_refs (NULL, sec, abfd, ncrs);
	}
    }
}

/* Check one symbol to see if it is a prohibited cross reference.  */

static bfd_boolean
check_nocrossref (struct cref_hash_entry *h, void *ignore ATTRIBUTE_UNUSED)
{
  struct bfd_link_hash_entry *hl;
  asection *defsec;
  const char *defsecname;
  struct lang_nocrossrefs *ncrs;
  struct lang_nocrossref *ncr;
  struct cref_ref *ref;

  hl = bfd_link_hash_lookup (link_info.hash, h->root.string, FALSE,
			     FALSE, TRUE);
  if (hl == NULL)
    {
      einfo (_("%P: symbol `%T' missing from main hash table\n"),
	     h->root.string);
      return TRUE;
    }

  if (hl->type != bfd_link_hash_defined
      && hl->type != bfd_link_hash_defweak)
    return TRUE;

  defsec = hl->u.def.section->output_section;
  if (defsec == NULL)
    return TRUE;
  defsecname = bfd_get_section_name (defsec->owner, defsec);

  for (ncrs = nocrossref_list; ncrs != NULL; ncrs = ncrs->next)
    for (ncr = ncrs->list; ncr != NULL; ncr = ncr->next)
      if (strcmp (ncr->name, defsecname) == 0)
	for (ref = h->refs; ref != NULL; ref = ref->next)
	  check_refs (hl->root.string, hl->u.def.section, ref->abfd, ncrs);

  return TRUE;
}

/* The struct is used to pass information from check_refs to
   check_reloc_refs through bfd_map_over_sections.  */

struct check_refs_info {
  const char *sym_name;
  asection *defsec;
  struct lang_nocrossrefs *ncrs;
  asymbol **asymbols;
};

/* This function is called for each symbol defined in a section which
   prohibits cross references.  We need to look through all references
   to this symbol, and ensure that the references are not from
   prohibited sections.  */

static void
check_refs (const char *name,
	    asection *sec,
	    bfd *abfd,
	    struct lang_nocrossrefs *ncrs)
{
  lang_input_statement_type *li;
  asymbol **asymbols;
  struct check_refs_info info;

  /* We need to look through the relocations for this BFD, to see
     if any of the relocations which refer to this symbol are from
     a prohibited section.  Note that we need to do this even for
     the BFD in which the symbol is defined, since even a single
     BFD might contain a prohibited cross reference.  */

  li = abfd->usrdata;
  if (li != NULL && li->asymbols != NULL)
    asymbols = li->asymbols;
  else
    {
      long symsize;
      long symbol_count;

      symsize = bfd_get_symtab_upper_bound (abfd);
      if (symsize < 0)
	einfo (_("%B%F: could not read symbols; %E\n"), abfd);
      asymbols = xmalloc (symsize);
      symbol_count = bfd_canonicalize_symtab (abfd, asymbols);
      if (symbol_count < 0)
	einfo (_("%B%F: could not read symbols: %E\n"), abfd);
      if (li != NULL)
	{
	  li->asymbols = asymbols;
	  li->symbol_count = symbol_count;
	}
    }

  info.sym_name = name;
  info.defsec = sec;
  info.ncrs = ncrs;
  info.asymbols = asymbols;
  bfd_map_over_sections (abfd, check_reloc_refs, &info);

  if (li == NULL)
    free (asymbols);
}

/* This is called via bfd_map_over_sections.  INFO->SYM_NAME is a symbol
   defined in INFO->DEFSECNAME.  If this section maps into any of the
   sections listed in INFO->NCRS, other than INFO->DEFSECNAME, then we
   look through the relocations.  If any of the relocations are to
   INFO->SYM_NAME, then we report a prohibited cross reference error.  */

static void
check_reloc_refs (bfd *abfd, asection *sec, void *iarg)
{
  struct check_refs_info *info = iarg;
  asection *outsec;
  const char *outsecname;
  asection *outdefsec;
  const char *outdefsecname;
  struct lang_nocrossref *ncr;
  const char *symname;
  long relsize;
  arelent **relpp;
  long relcount;
  arelent **p, **pend;

  outsec = sec->output_section;
  outsecname = bfd_get_section_name (outsec->owner, outsec);

  outdefsec = info->defsec->output_section;
  outdefsecname = bfd_get_section_name (outdefsec->owner, outdefsec);

  /* The section where the symbol is defined is permitted.  */
  if (strcmp (outsecname, outdefsecname) == 0)
    return;

  for (ncr = info->ncrs->list; ncr != NULL; ncr = ncr->next)
    if (strcmp (outsecname, ncr->name) == 0)
      break;

  if (ncr == NULL)
    return;

  /* This section is one for which cross references are prohibited.
     Look through the relocations, and see if any of them are to
     INFO->SYM_NAME.  If INFO->SYMNAME is NULL, check for relocations
     against the section symbol.  */

  symname = info->sym_name;

  relsize = bfd_get_reloc_upper_bound (abfd, sec);
  if (relsize < 0)
    einfo (_("%B%F: could not read relocs: %E\n"), abfd);
  if (relsize == 0)
    return;

  relpp = xmalloc (relsize);
  relcount = bfd_canonicalize_reloc (abfd, sec, relpp, info->asymbols);
  if (relcount < 0)
    einfo (_("%B%F: could not read relocs: %E\n"), abfd);

  p = relpp;
  pend = p + relcount;
  for (; p < pend && *p != NULL; p++)
    {
      arelent *q = *p;

      if (q->sym_ptr_ptr != NULL
	  && *q->sym_ptr_ptr != NULL
	  && (symname != NULL
	      ? strcmp (bfd_asymbol_name (*q->sym_ptr_ptr), symname) == 0
	      : (((*q->sym_ptr_ptr)->flags & BSF_SECTION_SYM) != 0
		 && bfd_get_section (*q->sym_ptr_ptr) == info->defsec)))
	{
	  /* We found a reloc for the symbol.  The symbol is defined
	     in OUTSECNAME.  This reloc is from a section which is
	     mapped into a section from which references to OUTSECNAME
	     are prohibited.  We must report an error.  */
	  einfo (_("%X%C: prohibited cross reference from %s to `%T' in %s\n"),
		 abfd, sec, q->address, outsecname,
		 bfd_asymbol_name (*q->sym_ptr_ptr), outdefsecname);
	}
    }

  free (relpp);
}
