/* ldcref.c -- output a cross reference table
   Copyright (C) 1996, 97, 98, 99, 2000 Free Software Foundation, Inc.
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

struct cref_ref
{
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

struct cref_hash_entry
{
  struct bfd_hash_entry root;
  /* The demangled name.  */
  char *demangled;
  /* References to and definitions of this symbol.  */
  struct cref_ref *refs;
};

/* This is what the hash table looks like.  */

struct cref_hash_table
{
  struct bfd_hash_table root;
};

/* Local functions.  */

static struct bfd_hash_entry *cref_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static boolean cref_fill_array PARAMS ((struct cref_hash_entry *, PTR));
static int cref_sort_array PARAMS ((const PTR, const PTR));
static void output_one_cref PARAMS ((FILE *, struct cref_hash_entry *));
static boolean check_nocrossref PARAMS ((struct cref_hash_entry *, PTR));
static void check_refs
  PARAMS ((struct cref_hash_entry *, struct bfd_link_hash_entry *,
	   struct lang_nocrossrefs *));
static void check_reloc_refs PARAMS ((bfd *, asection *, PTR));

/* Look up an entry in the cref hash table.  */

#define cref_hash_lookup(table, string, create, copy)		\
  ((struct cref_hash_entry *)					\
   bfd_hash_lookup (&(table)->root, (string), (create), (copy)))

/* Traverse the cref hash table.  */

#define cref_hash_traverse(table, func, info)				\
  (bfd_hash_traverse							\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct bfd_hash_entry *, PTR))) (func),	\
    (info)))

/* The cref hash table.  */

static struct cref_hash_table cref_table;

/* Whether the cref hash table has been initialized.  */

static boolean cref_initialized;

/* The number of symbols seen so far.  */

static size_t cref_symcount;

/* Create an entry in a cref hash table.  */

static struct bfd_hash_entry *
cref_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct cref_hash_entry *ret = (struct cref_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == NULL)
    ret = ((struct cref_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct cref_hash_entry)));
  if (ret == NULL)
    return (struct bfd_hash_entry *) ret;

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

  return (struct bfd_hash_entry *) ret;
}

/* Add a symbol to the cref hash table.  This is called for every
   symbol that is seen during the link.  */

/*ARGSUSED*/
void
add_cref (name, abfd, section, value)
     const char *name;
     bfd *abfd;
     asection *section;
     bfd_vma value ATTRIBUTE_UNUSED;
{
  struct cref_hash_entry *h;
  struct cref_ref *r;

  if (! cref_initialized)
    {
      if (! bfd_hash_table_init (&cref_table.root, cref_hash_newfunc))
	einfo (_("%X%P: bfd_hash_table_init of cref table failed: %E\n"));
      cref_initialized = true;
    }

  h = cref_hash_lookup (&cref_table, name, true, false);
  if (h == NULL)
    einfo (_("%X%P: cref_hash_lookup failed: %E\n"));

  for (r = h->refs; r != NULL; r = r->next)
    if (r->abfd == abfd)
      break;

  if (r == NULL)
    {
      r = (struct cref_ref *) xmalloc (sizeof *r);
      r->next = h->refs;
      h->refs = r;
      r->abfd = abfd;
      r->def = false;
      r->common = false;
      r->undef = false;
    }

  if (bfd_is_und_section (section))
    r->undef = true;
  else if (bfd_is_com_section (section))
    r->common = true;
  else
    r->def = true;
}

/* Copy the addresses of the hash table entries into an array.  This
   is called via cref_hash_traverse.  We also fill in the demangled
   name.  */

static boolean
cref_fill_array (h, data)
     struct cref_hash_entry *h;
     PTR data;
{
  struct cref_hash_entry ***pph = (struct cref_hash_entry ***) data;

  ASSERT (h->demangled == NULL);
  h->demangled = demangle (h->root.string);

  **pph = h;

  ++*pph;

  return true;
}

/* Sort an array of cref hash table entries by name.  */

static int
cref_sort_array (a1, a2)
     const PTR a1;
     const PTR a2;
{
  const struct cref_hash_entry **p1 = (const struct cref_hash_entry **) a1;
  const struct cref_hash_entry **p2 = (const struct cref_hash_entry **) a2;

  return strcmp ((*p1)->demangled, (*p2)->demangled);
}

/* Write out the cref table.  */

#define FILECOL (50)

void
output_cref (fp)
     FILE *fp;
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
      putc (' ' , fp);
      ++len;
    }
  fprintf (fp, _("File\n"));

  if (! cref_initialized)
    {
      fprintf (fp, _("No symbols\n"));
      return;
    }

  csyms = ((struct cref_hash_entry **)
	   xmalloc (cref_symcount * sizeof (*csyms)));

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
output_one_cref (fp, h)
     FILE *fp;
     struct cref_hash_entry *h;
{
  int len;
  struct bfd_link_hash_entry *hl;
  struct cref_ref *r;

  hl = bfd_link_hash_lookup (link_info.hash, h->root.string, false,
			     false, true);
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
check_nocrossrefs ()
{
  if (! cref_initialized)
    return;

  cref_hash_traverse (&cref_table, check_nocrossref, (PTR) NULL);
}

/* Check one symbol to see if it is a prohibited cross reference.  */

/*ARGSUSED*/
static boolean
check_nocrossref (h, ignore)
     struct cref_hash_entry *h;
     PTR ignore ATTRIBUTE_UNUSED;
{
  struct bfd_link_hash_entry *hl;
  asection *defsec;
  const char *defsecname;
  struct lang_nocrossrefs *ncrs;
  struct lang_nocrossref *ncr;

  hl = bfd_link_hash_lookup (link_info.hash, h->root.string, false,
			     false, true);
  if (hl == NULL)
    {
      einfo (_("%P: symbol `%T' missing from main hash table\n"),
	     h->root.string);
      return true;
    }

  if (hl->type != bfd_link_hash_defined
      && hl->type != bfd_link_hash_defweak)
    return true;

  defsec = hl->u.def.section->output_section;
  if (defsec == NULL)
    return true;
  defsecname = bfd_get_section_name (defsec->owner, defsec);

  for (ncrs = nocrossref_list; ncrs != NULL; ncrs = ncrs->next)
    for (ncr = ncrs->list; ncr != NULL; ncr = ncr->next)
      if (strcmp (ncr->name, defsecname) == 0)
	check_refs (h, hl, ncrs);

  return true;
}

/* The struct is used to pass information from check_refs to
   check_reloc_refs through bfd_map_over_sections.  */

struct check_refs_info
{
  struct cref_hash_entry *h;
  asection *defsec;
  struct lang_nocrossrefs *ncrs;
  asymbol **asymbols;
  boolean same;
};

/* This function is called for each symbol defined in a section which
   prohibits cross references.  We need to look through all references
   to this symbol, and ensure that the references are not from
   prohibited sections.  */

static void
check_refs (h, hl, ncrs)
     struct cref_hash_entry *h;
     struct bfd_link_hash_entry *hl;
     struct lang_nocrossrefs *ncrs;
{
  struct cref_ref *ref;

  for (ref = h->refs; ref != NULL; ref = ref->next)
    {
      lang_input_statement_type *li;
      asymbol **asymbols;
      struct check_refs_info info;

      /* We need to look through the relocations for this BFD, to see
         if any of the relocations which refer to this symbol are from
         a prohibited section.  Note that we need to do this even for
         the BFD in which the symbol is defined, since even a single
         BFD might contain a prohibited cross reference; for this
         case, we set the SAME field in INFO, which will cause
         CHECK_RELOCS_REFS to check for relocations against the
         section as well as against the symbol.  */

      li = (lang_input_statement_type *) ref->abfd->usrdata;
      if (li != NULL && li->asymbols != NULL)
	asymbols = li->asymbols;
      else
	{
	  long symsize;
	  long symbol_count;

	  symsize = bfd_get_symtab_upper_bound (ref->abfd);
	  if (symsize < 0)
	    einfo (_("%B%F: could not read symbols; %E\n"), ref->abfd);
	  asymbols = (asymbol **) xmalloc (symsize);
	  symbol_count = bfd_canonicalize_symtab (ref->abfd, asymbols);
	  if (symbol_count < 0)
	    einfo (_("%B%F: could not read symbols: %E\n"), ref->abfd);
	  if (li != NULL)
	    {
	      li->asymbols = asymbols;
	      li->symbol_count = symbol_count;
	    }
	}

      info.h = h;
      info.defsec = hl->u.def.section;
      info.ncrs = ncrs;
      info.asymbols = asymbols;
      if (ref->abfd == hl->u.def.section->owner)
	info.same = true;
      else
	info.same = false;
      bfd_map_over_sections (ref->abfd, check_reloc_refs, (PTR) &info);

      if (li == NULL)
	free (asymbols);
    }
}

/* This is called via bfd_map_over_sections.  INFO->H is a symbol
   defined in INFO->DEFSECNAME.  If this section maps into any of the
   sections listed in INFO->NCRS, other than INFO->DEFSECNAME, then we
   look through the relocations.  If any of the relocations are to
   INFO->H, then we report a prohibited cross reference error.  */

static void
check_reloc_refs (abfd, sec, iarg)
     bfd *abfd;
     asection *sec;
     PTR iarg;
{
  struct check_refs_info *info = (struct check_refs_info *) iarg;
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
     INFO->H.  */

  symname = info->h->root.string;

  relsize = bfd_get_reloc_upper_bound (abfd, sec);
  if (relsize < 0)
    einfo (_("%B%F: could not read relocs: %E\n"), abfd);
  if (relsize == 0)
    return;

  relpp = (arelent **) xmalloc (relsize);
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
	  && (strcmp (bfd_asymbol_name (*q->sym_ptr_ptr), symname) == 0
	      || (info->same
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
