/* ELF strtab with GC and suffix merging support.
   Copyright 2001, 2002 Free Software Foundation, Inc.
   Written by Jakub Jelinek <jakub@redhat.com>.

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
#include "elf-bfd.h"
#include "hashtab.h"
#include "libiberty.h"

/* An entry in the strtab hash table.  */

struct elf_strtab_hash_entry
{
  struct bfd_hash_entry root;
  /* Length of this entry.  */
  unsigned int len;
  unsigned int refcount;
  union {
    /* Index within the merged section.  */
    bfd_size_type index;
    /* Entry this is a suffix of (if len is 0).  */
    struct elf_strtab_hash_entry *suffix;
    struct elf_strtab_hash_entry *next;
  } u;
};

/* The strtab hash table.  */

struct elf_strtab_hash
{
  struct bfd_hash_table table;
  /* Next available index.  */
  bfd_size_type size;
  /* Number of array entries alloced.  */
  bfd_size_type alloced;
  /* Final strtab size.  */
  bfd_size_type sec_size;
  /* Array of pointers to strtab entries.  */
  struct elf_strtab_hash_entry **array;
};

static struct bfd_hash_entry *elf_strtab_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static int cmplengthentry PARAMS ((const PTR, const PTR));
static int last4_eq PARAMS ((const PTR, const PTR));

/* Routine to create an entry in a section merge hashtab.  */

static struct bfd_hash_entry *
elf_strtab_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct elf_strtab_hash_entry *ret = (struct elf_strtab_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct elf_strtab_hash_entry *) NULL)
    ret = ((struct elf_strtab_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct elf_strtab_hash_entry)));
  if (ret == (struct elf_strtab_hash_entry *) NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct elf_strtab_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  if (ret)
    {
      /* Initialize the local fields.  */
      ret->u.index = -1;
      ret->refcount = 0;
      ret->len = 0;
    }

  return (struct bfd_hash_entry *)ret;
}

/* Create a new hash table.  */

struct elf_strtab_hash *
_bfd_elf_strtab_init ()
{
  struct elf_strtab_hash *table;
  bfd_size_type amt = sizeof (struct elf_strtab_hash);

  table = (struct elf_strtab_hash *) bfd_malloc (amt);
  if (table == NULL)
    return NULL;

  if (! bfd_hash_table_init (&table->table, elf_strtab_hash_newfunc))
    {
      free (table);
      return NULL;
    }

  table->sec_size = 0;
  table->size = 1;
  table->alloced = 64;
  amt = sizeof (struct elf_strtab_hasn_entry *);
  table->array = (struct elf_strtab_hash_entry **)
		 bfd_malloc (table->alloced * amt);
  if (table->array == NULL)
    {
      free (table);
      return NULL;
    }

  table->array[0] = NULL;

  return table;
}

/* Free a strtab.  */

void
_bfd_elf_strtab_free (tab)
     struct elf_strtab_hash *tab;
{
  bfd_hash_table_free (&tab->table);
  free (tab->array);
  free (tab);
}

/* Get the index of an entity in a hash table, adding it if it is not
   already present.  */

bfd_size_type
_bfd_elf_strtab_add (tab, str, copy)
     struct elf_strtab_hash *tab;
     const char *str;
     boolean copy;
{
  register struct elf_strtab_hash_entry *entry;

  /* We handle this specially, since we don't want to do refcounting
     on it.  */
  if (*str == '\0')
    return 0;

  BFD_ASSERT (tab->sec_size == 0);
  entry = (struct elf_strtab_hash_entry *)
	  bfd_hash_lookup (&tab->table, str, true, copy);

  if (entry == NULL)
    return (bfd_size_type) -1;

  entry->refcount++;
  if (entry->len == 0)
    {
      entry->len = strlen (str) + 1;
      if (tab->size == tab->alloced)
	{
	  bfd_size_type amt = sizeof (struct elf_strtab_hash_entry *);
	  tab->alloced *= 2;
	  tab->array = (struct elf_strtab_hash_entry **)
		       bfd_realloc (tab->array, tab->alloced * amt);
	  if (tab->array == NULL)
	    return (bfd_size_type) -1;
	}

      entry->u.index = tab->size++;
      tab->array[entry->u.index] = entry;
    }
  return entry->u.index;
}

void
_bfd_elf_strtab_addref (tab, idx)
     struct elf_strtab_hash *tab;
     bfd_size_type idx;
{
  if (idx == 0 || idx == (bfd_size_type) -1)
    return;
  BFD_ASSERT (tab->sec_size == 0);
  BFD_ASSERT (idx < tab->size);
  ++tab->array[idx]->refcount;
}

void
_bfd_elf_strtab_delref (tab, idx)
     struct elf_strtab_hash *tab;
     bfd_size_type idx;
{
  if (idx == 0 || idx == (bfd_size_type) -1)
    return;
  BFD_ASSERT (tab->sec_size == 0);
  BFD_ASSERT (idx < tab->size);
  BFD_ASSERT (tab->array[idx]->refcount > 0);
  --tab->array[idx]->refcount;
}

void
_bfd_elf_strtab_clear_all_refs (tab)
     struct elf_strtab_hash *tab;
{
  bfd_size_type idx;

  for (idx = 1; idx < tab->size; ++idx)
    tab->array[idx]->refcount = 0;
}

bfd_size_type
_bfd_elf_strtab_size (tab)
     struct elf_strtab_hash *tab;
{
  return tab->sec_size ? tab->sec_size : tab->size;
}

bfd_size_type
_bfd_elf_strtab_offset (tab, idx)
     struct elf_strtab_hash *tab;
     bfd_size_type idx;
{
  struct elf_strtab_hash_entry *entry;

  if (idx == 0)
    return 0;
  BFD_ASSERT (idx < tab->size);
  BFD_ASSERT (tab->sec_size);
  entry = tab->array[idx];
  BFD_ASSERT (entry->refcount > 0);
  entry->refcount--;
  return tab->array[idx]->u.index;
}

boolean
_bfd_elf_strtab_emit (abfd, tab)
     register bfd *abfd;
     struct elf_strtab_hash *tab;
{
  bfd_size_type off = 1, i;

  if (bfd_bwrite ("", 1, abfd) != 1)
    return false;

  for (i = 1; i < tab->size; ++i)
    {
      register const char *str;
      register size_t len;

      str = tab->array[i]->root.string;
      len = tab->array[i]->len;
      BFD_ASSERT (tab->array[i]->refcount == 0);
      if (len == 0)
	continue;

      if (bfd_bwrite ((PTR) str, (bfd_size_type) len, abfd) != len)
	return false;

      off += len;
    }

  BFD_ASSERT (off == tab->sec_size);
  return true;
}

/* Compare two elf_strtab_hash_entry structures.  This is called via qsort.  */

static int
cmplengthentry (a, b)
     const PTR a;
     const PTR b;
{
  struct elf_strtab_hash_entry * A = *(struct elf_strtab_hash_entry **) a;
  struct elf_strtab_hash_entry * B = *(struct elf_strtab_hash_entry **) b;

  if (A->len < B->len)
    return 1;
  else if (A->len > B->len)
    return -1;

  return memcmp (A->root.string, B->root.string, A->len);
}

static int
last4_eq (a, b)
     const PTR a;
     const PTR b;
{
  struct elf_strtab_hash_entry * A = (struct elf_strtab_hash_entry *) a;
  struct elf_strtab_hash_entry * B = (struct elf_strtab_hash_entry *) b;

  if (memcmp (A->root.string + A->len - 5, B->root.string + B->len - 5, 4)
      != 0)
    /* This was a hashtable collision.  */
    return 0;

  if (A->len <= B->len)
    /* B cannot be a suffix of A unless A is equal to B, which is guaranteed
       not to be equal by the hash table.  */
    return 0;

  return memcmp (A->root.string + (A->len - B->len),
		 B->root.string, B->len - 5) == 0;
}

/* This function assigns final string table offsets for used strings,
   merging strings matching suffixes of longer strings if possible.  */

void
_bfd_elf_strtab_finalize (tab)
     struct elf_strtab_hash *tab;
{
  struct elf_strtab_hash_entry **array, **a, **end, *e;
  htab_t last4tab = NULL;
  bfd_size_type size, amt;
  struct elf_strtab_hash_entry *last[256], **last_ptr[256];

  /* GCC 2.91.66 (egcs-1.1.2) on i386 miscompiles this function when i is
     a 64-bit bfd_size_type: a 64-bit target or --enable-64-bit-bfd.
     Besides, indexing with a long long wouldn't give anything but extra
     cycles.  */
  size_t i;

  /* Now sort the strings by length, longest first.  */
  array = NULL;
  amt = tab->size * sizeof (struct elf_strtab_hash_entry *);
  array = (struct elf_strtab_hash_entry **) bfd_malloc (amt);
  if (array == NULL)
    goto alloc_failure;

  memset (last, 0, sizeof (last));
  for (i = 0; i < 256; ++i)
    last_ptr[i] = &last[i];
  for (i = 1, a = array; i < tab->size; ++i)
    if (tab->array[i]->refcount)
      *a++ = tab->array[i];
    else
      tab->array[i]->len = 0;

  size = a - array;

  qsort (array, size, sizeof (struct elf_strtab_hash_entry *), cmplengthentry);

  last4tab = htab_create_alloc (size * 4, NULL, last4_eq, NULL, calloc, free);
  if (last4tab == NULL)
    goto alloc_failure;

  /* Now insert the strings into hash tables (strings with last 4 characters
     and strings with last character equal), look for longer strings which
     we're suffix of.  */
  for (a = array, end = array + size; a < end; a++)
    {
      register hashval_t hash;
      unsigned int c;
      unsigned int j;
      const unsigned char *s;
      PTR *p;

      e = *a;
      if (e->len > 4)
	{
	  s = e->root.string + e->len - 1;
	  hash = 0;
	  for (j = 0; j < 4; j++)
	    {
	      c = *--s;
	      hash += c + (c << 17);
	      hash ^= hash >> 2;
	    }
	  p = htab_find_slot_with_hash (last4tab, e, hash, INSERT);
	  if (p == NULL)
	    goto alloc_failure;
	  if (*p)
	    {
	      struct elf_strtab_hash_entry *ent;

	      ent = (struct elf_strtab_hash_entry *) *p;
	      e->u.suffix = ent;
	      e->len = 0;
	      continue;
	    }
	  else
	    *p = (PTR) e;
	}
      else
	{
	  struct elf_strtab_hash_entry *tem;

	  c = e->root.string[e->len - 2] & 0xff;

	  for (tem = last[c]; tem; tem = tem->u.next)
	    if (tem->len > e->len
		&& memcmp (tem->root.string + (tem->len - e->len),
			   e->root.string, e->len - 1) == 0)
	      break;
	  if (tem)
	    {
	      e->u.suffix = tem;
	      e->len = 0;
	      continue;
	    }
	}

      c = e->root.string[e->len - 2] & 0xff;
      /* Put longest strings first.  */
      *last_ptr[c] = e;
      last_ptr[c] = &e->u.next;
      e->u.next = NULL;
    }

alloc_failure:
  if (array)
    free (array);
  if (last4tab)
    htab_delete (last4tab);

  /* Now assign positions to the strings we want to keep.  */
  size = 1;
  for (i = 1; i < tab->size; ++i)
    {
      e = tab->array[i];
      if (e->refcount && e->len)
	{
	  e->u.index = size;
	  size += e->len;
	}
    }

  tab->sec_size = size;

  /* And now adjust the rest.  */
  for (i = 1; i < tab->size; ++i)
    {
      e = tab->array[i];
      if (e->refcount && ! e->len)
	e->u.index = e->u.suffix->u.index
		     + (e->u.suffix->len - strlen (e->root.string) - 1);
    }
}
