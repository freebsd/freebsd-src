/* SEC_MERGE support.
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

/* This file contains support for merging duplicate entities within sections,
   as used in ELF SHF_MERGE.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "hashtab.h"
#include "libiberty.h"

struct sec_merge_sec_info;

/* An entry in the section merge hash table.  */

struct sec_merge_hash_entry
{
  struct bfd_hash_entry root;
  /* Length of this entry.  */
  unsigned int len;
  /* Start of this string needs to be aligned to
     alignment octets (not 1 << align).  */
  unsigned int alignment;
  union
  {
    /* Index within the merged section.  */
    bfd_size_type index;
    /* Entity size (if present in suffix hash tables).  */
    unsigned int entsize;
    /* Entry this is a suffix of (if alignment is 0).  */
    struct sec_merge_hash_entry *suffix;
  } u;
  /* Which section is it in.  */
  struct sec_merge_sec_info *secinfo;
  /* Next entity in the hash table.  */
  struct sec_merge_hash_entry *next;
};

/* The section merge hash table.  */

struct sec_merge_hash
{
  struct bfd_hash_table table;
  /* Next available index.  */
  bfd_size_type size;
  /* First entity in the SEC_MERGE sections of this type.  */
  struct sec_merge_hash_entry *first;
  /* Last entity in the SEC_MERGE sections of this type.  */
  struct sec_merge_hash_entry *last;
  /* Entity size.  */
  unsigned int entsize;
  /* Are entries fixed size or zero terminated strings?  */
  boolean strings;
};

struct sec_merge_info
{
  /* Chain of sec_merge_infos.  */
  struct sec_merge_info *next;
  /* Chain of sec_merge_sec_infos.  */
  struct sec_merge_sec_info *chain;
  /* A hash table used to hold section content.  */
  struct sec_merge_hash *htab;
};

struct sec_merge_sec_info
{
  /* Chain of sec_merge_sec_infos.  */
  struct sec_merge_sec_info *next;
  /* The corresponding section.  */
  asection *sec;
  /* Pointer to merge_info pointing to us.  */
  PTR *psecinfo;
  /* A hash table used to hold section content.  */
  struct sec_merge_hash *htab;
  /* First string in this section.  */
  struct sec_merge_hash_entry *first;
  /* Original section content.  */
  unsigned char contents[1];
};

static struct bfd_hash_entry *sec_merge_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static struct sec_merge_hash_entry *sec_merge_hash_lookup
  PARAMS ((struct sec_merge_hash *, const char *, unsigned int, boolean));
static struct sec_merge_hash *sec_merge_init
  PARAMS ((unsigned int, boolean));
static struct sec_merge_hash_entry *sec_merge_add
  PARAMS ((struct sec_merge_hash *, const char *, unsigned int,
	   struct sec_merge_sec_info *));
static boolean sec_merge_emit
  PARAMS ((bfd *, struct sec_merge_hash_entry *));
static int cmplengthentry PARAMS ((const PTR, const PTR));
static int last4_eq PARAMS ((const PTR, const PTR));
static int last_eq PARAMS ((const PTR, const PTR));
static boolean record_section
  PARAMS ((struct sec_merge_info *, struct sec_merge_sec_info *));
static void merge_strings PARAMS ((struct sec_merge_info *));

/* Routine to create an entry in a section merge hashtab.  */

static struct bfd_hash_entry *
sec_merge_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct sec_merge_hash_entry *ret = (struct sec_merge_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct sec_merge_hash_entry *) NULL)
    ret = ((struct sec_merge_hash_entry *)
	   bfd_hash_allocate (table, sizeof (struct sec_merge_hash_entry)));
  if (ret == (struct sec_merge_hash_entry *) NULL)
    return NULL;

  /* Call the allocation method of the superclass.  */
  ret = ((struct sec_merge_hash_entry *)
	 bfd_hash_newfunc ((struct bfd_hash_entry *) ret, table, string));

  if (ret)
    {
      /* Initialize the local fields.  */
      ret->u.suffix = NULL;
      ret->alignment = 0;
      ret->secinfo = NULL;
      ret->next = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

/* Look up an entry in a section merge hash table.  */

static struct sec_merge_hash_entry *
sec_merge_hash_lookup (table, string, alignment, create)
     struct sec_merge_hash *table;
     const char *string;
     unsigned int alignment;
     boolean create;
{
  register const unsigned char *s;
  register unsigned long hash;
  register unsigned int c;
  struct sec_merge_hash_entry *hashp;
  unsigned int len, i;
  unsigned int index;

  hash = 0;
  len = 0;
  s = (const unsigned char *) string;
  if (table->strings)
    {
      if (table->entsize == 1)
	{
	  while ((c = *s++) != '\0')
	    {
	      hash += c + (c << 17);
	      hash ^= hash >> 2;
	      ++len;
	    }
	  hash += len + (len << 17);
	}
      else
	{
	  for (;;)
	    {
	      for (i = 0; i < table->entsize; ++i)
		if (s[i] != '\0')
		  break;
	      if (i == table->entsize)
		break;
	      for (i = 0; i < table->entsize; ++i)
		{
		  c = *s++;
		  hash += c + (c << 17);
		  hash ^= hash >> 2;
		}
	      ++len;
	    }
	  hash += len + (len << 17);
	  len *= table->entsize;
	}
      hash ^= hash >> 2;
      len += table->entsize;
    }
  else
    {
      for (i = 0; i < table->entsize; ++i)
	{
	  c = *s++;
	  hash += c + (c << 17);
	  hash ^= hash >> 2;
	}
      len = table->entsize;
    }

  index = hash % table->table.size;
  for (hashp = (struct sec_merge_hash_entry *) table->table.table[index];
       hashp != (struct sec_merge_hash_entry *) NULL;
       hashp = (struct sec_merge_hash_entry *) hashp->root.next)
    {
      if (hashp->root.hash == hash
	  && len == hashp->len
	  && memcmp (hashp->root.string, string, len) == 0)
	{
	  /* If the string we found does not have at least the required
	     alignment, we need to insert another copy.  */
	  if (hashp->alignment < alignment)
	    {
	      /*  Mark the less aligned copy as deleted.  */
	      hashp->len = 0;
	      hashp->alignment = 0;
	      break;
	    }
	  return hashp;
	}
    }

  if (! create)
    return (struct sec_merge_hash_entry *) NULL;

  hashp = (struct sec_merge_hash_entry *)
	  sec_merge_hash_newfunc ((struct bfd_hash_entry *) NULL,
				  (struct bfd_hash_table *) table, string);
  if (hashp == (struct sec_merge_hash_entry *) NULL)
    return (struct sec_merge_hash_entry *) NULL;
  hashp->root.string = string;
  hashp->root.hash = hash;
  hashp->len = len;
  hashp->alignment = alignment;
  hashp->root.next = table->table.table[index];
  table->table.table[index] = (struct bfd_hash_entry *) hashp;

  return hashp;
}

/* Create a new hash table.  */

static struct sec_merge_hash *
sec_merge_init (entsize, strings)
     unsigned int entsize;
     boolean strings;
{
  struct sec_merge_hash *table;
  bfd_size_type amt = sizeof (struct sec_merge_hash);

  table = (struct sec_merge_hash *) bfd_malloc (amt);
  if (table == NULL)
    return NULL;

  if (! bfd_hash_table_init (&table->table, sec_merge_hash_newfunc))
    {
      free (table);
      return NULL;
    }

  table->size = 0;
  table->first = NULL;
  table->last = NULL;
  table->entsize = entsize;
  table->strings = strings;

  return table;
}

/* Get the index of an entity in a hash table, adding it if it is not
   already present.  */

static struct sec_merge_hash_entry *
sec_merge_add (tab, str, alignment, secinfo)
     struct sec_merge_hash *tab;
     const char *str;
     unsigned int alignment;
     struct sec_merge_sec_info *secinfo;
{
  register struct sec_merge_hash_entry *entry;

  entry = sec_merge_hash_lookup (tab, str, alignment, true);
  if (entry == NULL)
    return NULL;

  if (entry->secinfo == NULL)
    {
      tab->size++;
      entry->secinfo = secinfo;
      if (tab->first == NULL)
	tab->first = entry;
      else
	tab->last->next = entry;
      tab->last = entry;
    }

  return entry;
}

static boolean
sec_merge_emit (abfd, entry)
     register bfd *abfd;
     struct sec_merge_hash_entry *entry;
{
  struct sec_merge_sec_info *secinfo = entry->secinfo;
  asection *sec = secinfo->sec;
  char *pad = "";
  bfd_size_type off = 0;
  int alignment_power = bfd_get_section_alignment (abfd, sec->output_section);

  if (alignment_power)
    pad = bfd_zmalloc ((bfd_size_type) 1 << alignment_power);

  for (; entry != NULL && entry->secinfo == secinfo; entry = entry->next)
    {
      register const char *str;
      register size_t len;

      len = off & (entry->alignment - 1);
      if (len)
	{
	  len = entry->alignment - len;
	  if (bfd_bwrite ((PTR) pad, (bfd_size_type) len, abfd) != len)
	    break;
	  off += len;
	}

      str = entry->root.string;
      len = entry->len;

      if (bfd_bwrite ((PTR) str, (bfd_size_type) len, abfd) != len)
	break;

      off += len;
    }

  if (alignment_power)
    free (pad);

  return entry == NULL || entry->secinfo != secinfo;
}

/* This function is called for each input file from the add_symbols
   pass of the linker.  */

boolean
_bfd_merge_section (abfd, psinfo, sec, psecinfo)
     bfd *abfd;
     PTR *psinfo;
     asection *sec;
     PTR *psecinfo;
{
  struct sec_merge_info *sinfo;
  struct sec_merge_sec_info *secinfo;
  unsigned int align;
  bfd_size_type amt;

  if (sec->_raw_size == 0
      || (sec->flags & SEC_EXCLUDE)
      || (sec->flags & SEC_MERGE) == 0
      || sec->entsize == 0)
    return true;

  if ((sec->flags & SEC_RELOC) != 0)
    {
      /* We aren't prepared to handle relocations in merged sections.  */
      return true;
    }

  align = bfd_get_section_alignment (sec->owner, sec);
  if ((sec->entsize < (unsigned int)(1 << align)
       && ((sec->entsize & (sec->entsize - 1))
	   || !(sec->flags & SEC_STRINGS)))
      || (sec->entsize > (unsigned int)(1 << align)
	  && (sec->entsize & ((1 << align) - 1))))
    {
      /* Sanity check.  If string character size is smaller than
	 alignment, then we require character size to be a power
	 of 2, otherwise character size must be integer multiple
	 of alignment.  For non-string constants, alignment must
	 be smaller than or equal to entity size and entity size
	 must be integer multiple of alignment.  */
      return true;
    }

  for (sinfo = (struct sec_merge_info *) *psinfo; sinfo; sinfo = sinfo->next)
    if ((secinfo = sinfo->chain)
	&& ! ((secinfo->sec->flags ^ sec->flags) & (SEC_MERGE | SEC_STRINGS))
	&& secinfo->sec->entsize == sec->entsize
	&& ! strcmp (secinfo->sec->name, sec->name))
      break;

  if (sinfo == NULL)
    {
      /* Initialize the information we need to keep track of.  */
      amt = sizeof (struct sec_merge_info);
      sinfo = (struct sec_merge_info *) bfd_alloc (abfd, amt);
      if (sinfo == NULL)
	goto error_return;
      sinfo->next = (struct sec_merge_info *) *psinfo;
      sinfo->chain = NULL;
      *psinfo = (PTR) sinfo;
      sinfo->htab = sec_merge_init (sec->entsize, (sec->flags & SEC_STRINGS));
      if (sinfo->htab == NULL)
	goto error_return;
    }

  /* Read the section from abfd.  */

  amt = sizeof (struct sec_merge_sec_info) + sec->_raw_size - 1;
  *psecinfo = bfd_alloc (abfd, amt);
  if (*psecinfo == NULL)
    goto error_return;

  secinfo = (struct sec_merge_sec_info *)*psecinfo;
  if (sinfo->chain)
    {
      secinfo->next = sinfo->chain->next;
      sinfo->chain->next = secinfo;
    }
  else
    secinfo->next = secinfo;
  sinfo->chain = secinfo;
  secinfo->sec = sec;
  secinfo->psecinfo = psecinfo;
  secinfo->htab = sinfo->htab;
  secinfo->first = NULL;

  if (! bfd_get_section_contents (sec->owner, sec, secinfo->contents,
				  (bfd_vma) 0, sec->_raw_size))
    goto error_return;

  return true;

 error_return:
  *psecinfo = NULL;
  return false;
}

/* Compare two sec_merge_hash_entry structures.  This is called via qsort.  */

static int
cmplengthentry (a, b)
     const PTR a;
     const PTR b;
{
  struct sec_merge_hash_entry * A = *(struct sec_merge_hash_entry **) a;
  struct sec_merge_hash_entry * B = *(struct sec_merge_hash_entry **) b;

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
  struct sec_merge_hash_entry * A = (struct sec_merge_hash_entry *) a;
  struct sec_merge_hash_entry * B = (struct sec_merge_hash_entry *) b;

  if (memcmp (A->root.string + A->len - 5 * A->u.entsize,
	      B->root.string + B->len - 5 * A->u.entsize,
	      4 * A->u.entsize) != 0)
    /* This was a hashtable collision.  */
    return 0;

  if (A->len <= B->len)
    /* B cannot be a suffix of A unless A is equal to B, which is guaranteed
       not to be equal by the hash table.  */
    return 0;

  if (A->alignment < B->alignment
      || ((A->len - B->len) & (B->alignment - 1)))
    /* The suffix is not sufficiently aligned.  */
    return 0;

  return memcmp (A->root.string + (A->len - B->len),
		 B->root.string, B->len - 5 * A->u.entsize) == 0;
}

static int
last_eq (a, b)
     const PTR a;
     const PTR b;
{
  struct sec_merge_hash_entry * A = (struct sec_merge_hash_entry *) a;
  struct sec_merge_hash_entry * B = (struct sec_merge_hash_entry *) b;

  if (B->len >= 5 * A->u.entsize)
    /* Longer strings are just pushed into the hash table,
       they'll be used when looking up for very short strings.  */
    return 0;

  if (memcmp (A->root.string + A->len - 2 * A->u.entsize,
	      B->root.string + B->len - 2 * A->u.entsize,
	      A->u.entsize) != 0)
    /* This was a hashtable collision.  */
    return 0;

  if (A->len <= B->len)
    /* B cannot be a suffix of A unless A is equal to B, which is guaranteed
       not to be equal by the hash table.  */
    return 0;

  if (A->alignment < B->alignment
      || ((A->len - B->len) & (B->alignment - 1)))
    /* The suffix is not sufficiently aligned.  */
    return 0;

  return memcmp (A->root.string + (A->len - B->len),
		 B->root.string, B->len - 2 * A->u.entsize) == 0;
}

/* Record one section into the hash table.  */
static boolean
record_section (sinfo, secinfo)
     struct sec_merge_info *sinfo;
     struct sec_merge_sec_info *secinfo;
{
  asection *sec = secinfo->sec;
  struct sec_merge_hash_entry *entry;
  boolean nul;
  unsigned char *p, *end;
  bfd_vma mask, eltalign;
  unsigned int align, i;

  align = bfd_get_section_alignment (sec->owner, sec);
  end = secinfo->contents + sec->_raw_size;
  nul = false;
  mask = ((bfd_vma) 1 << align) - 1;
  if (sec->flags & SEC_STRINGS)
    {
      for (p = secinfo->contents; p < end; )
	{
	  eltalign = p - secinfo->contents;
	  eltalign = ((eltalign ^ (eltalign - 1)) + 1) >> 1;
	  if (!eltalign || eltalign > mask)
	    eltalign = mask + 1;
	  entry = sec_merge_add (sinfo->htab, p, (unsigned) eltalign, secinfo);
	  if (! entry)
	    goto error_return;
	  p += entry->len;
	  if (sec->entsize == 1)
	    {
	      while (p < end && *p == 0)
		{
		  if (!nul && !((p - secinfo->contents) & mask))
		    {
		      nul = true;
		      entry = sec_merge_add (sinfo->htab, "",
					     (unsigned) mask + 1, secinfo);
		      if (! entry)
			goto error_return;
		    }
		  p++;
	        }
	    }
	  else
	    {
	      while (p < end)
		{
		  for (i = 0; i < sec->entsize; i++)
		    if (p[i] != '\0')
		      break;
		  if (i != sec->entsize)
		    break;
		  if (!nul && !((p - secinfo->contents) & mask))
		    {
		      nul = true;
		      entry = sec_merge_add (sinfo->htab, p,
					     (unsigned) mask + 1, secinfo);
		      if (! entry)
			goto error_return;
		    }
		  p += sec->entsize;
		}
	    }
	}
    }
  else
    {
      for (p = secinfo->contents; p < end; p += sec->entsize)
	{
	  entry = sec_merge_add (sinfo->htab, p, 1, secinfo);
	  if (! entry)
	    goto error_return;
	}
    }

  return true;

error_return:
  for (secinfo = sinfo->chain; secinfo; secinfo = secinfo->next)
    *secinfo->psecinfo = NULL;
  return false;
}

/* This is a helper function for _bfd_merge_sections.  It attempts to
   merge strings matching suffixes of longer strings.  */
static void
merge_strings (sinfo)
     struct sec_merge_info *sinfo;
{
  struct sec_merge_hash_entry **array, **a, **end, *e;
  struct sec_merge_sec_info *secinfo;
  htab_t lasttab = NULL, last4tab = NULL;
  bfd_size_type size, amt;

  /* Now sort the strings by length, longest first.  */
  array = NULL;
  amt = sinfo->htab->size * sizeof (struct sec_merge_hash_entry *);
  array = (struct sec_merge_hash_entry **) bfd_malloc (amt);
  if (array == NULL)
    goto alloc_failure;

  for (e = sinfo->htab->first, a = array; e; e = e->next)
    if (e->alignment)
      *a++ = e;

  sinfo->htab->size = a - array;

  qsort (array, (size_t) sinfo->htab->size,
	 sizeof (struct sec_merge_hash_entry *), cmplengthentry);

  last4tab = htab_create_alloc ((size_t) sinfo->htab->size * 4, 
				NULL, last4_eq, NULL, calloc, free);
  lasttab = htab_create_alloc ((size_t) sinfo->htab->size * 4, 
			       NULL, last_eq, NULL, calloc, free);
  if (lasttab == NULL || last4tab == NULL)
    goto alloc_failure;

  /* Now insert the strings into hash tables (strings with last 4 characters
     and strings with last character equal), look for longer strings which
     we're suffix of.  */
  for (a = array, end = array + sinfo->htab->size; a < end; a++)
    {
      register hashval_t hash;
      unsigned int c;
      unsigned int i;
      const unsigned char *s;
      PTR *p;

      e = *a;
      e->u.entsize = sinfo->htab->entsize;
      if (e->len <= e->u.entsize)
	break;
      if (e->len > 4 * e->u.entsize)
	{
	  s = e->root.string + e->len - e->u.entsize;
	  hash = 0;
	  for (i = 0; i < 4 * e->u.entsize; i++)
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
	      struct sec_merge_hash_entry *ent;

	      ent = (struct sec_merge_hash_entry *) *p;
	      e->u.suffix = ent;
	      e->alignment = 0;
	      continue;
	    }
	  else
	    *p = (PTR) e;
	}
      s = e->root.string + e->len - e->u.entsize;
      hash = 0;
      for (i = 0; i < e->u.entsize; i++)
	{
	  c = *--s;
	  hash += c + (c << 17);
	  hash ^= hash >> 2;
	}
      p = htab_find_slot_with_hash (lasttab, e, hash, INSERT);
      if (p == NULL)
	goto alloc_failure;
      if (*p)
	{
	  struct sec_merge_hash_entry *ent;

	  ent = (struct sec_merge_hash_entry *) *p;
	  e->u.suffix = ent;
	  e->alignment = 0;
	}
      else
	*p = (PTR) e;
    }

alloc_failure:
  if (array)
    free (array);
  if (lasttab)
    htab_delete (lasttab);
  if (last4tab)
    htab_delete (last4tab);

  /* Now assign positions to the strings we want to keep.  */
  size = 0;
  secinfo = sinfo->htab->first->secinfo;
  for (e = sinfo->htab->first; e; e = e->next)
    {
      if (e->secinfo != secinfo)
	{
	  secinfo->sec->_cooked_size = size;
	  secinfo = e->secinfo;
	}
      if (e->alignment)
	{
	  if (e->secinfo->first == NULL)
	    {
	      e->secinfo->first = e;
	      size = 0;
	    }
	  size = (size + e->alignment - 1) & ~((bfd_vma) e->alignment - 1);
	  e->u.index = size;
	  size += e->len;
	}
    }
  secinfo->sec->_cooked_size = size;

  /* And now adjust the rest, removing them from the chain (but not hashtable)
     at the same time.  */
  for (a = &sinfo->htab->first, e = *a; e; e = e->next)
    if (e->alignment)
      a = &e->next;
    else
      {
	*a = e->next;
	if (e->len)
	  {
	    e->secinfo = e->u.suffix->secinfo;
	    e->alignment = e->u.suffix->alignment;
	    e->u.index = e->u.suffix->u.index + (e->u.suffix->len - e->len);
	  }
      }
}

/* This function is called once after all SEC_MERGE sections are registered
   with _bfd_merge_section.  */

boolean
_bfd_merge_sections (abfd, xsinfo, remove_hook)
     bfd *abfd ATTRIBUTE_UNUSED;
     PTR xsinfo;
     void (*remove_hook) PARAMS((bfd *, asection *));
{
  struct sec_merge_info *sinfo;

  for (sinfo = (struct sec_merge_info *) xsinfo; sinfo; sinfo = sinfo->next)
    {
      struct sec_merge_sec_info * secinfo;

      if (! sinfo->chain)
	continue;

      /* Move sinfo->chain to head of the chain, terminate it.  */
      secinfo = sinfo->chain;
      sinfo->chain = secinfo->next;
      secinfo->next = NULL;

      /* Record the sections into the hash table.  */
      for (secinfo = sinfo->chain; secinfo; secinfo = secinfo->next)
	if (secinfo->sec->flags & SEC_EXCLUDE)
	  {
	    *secinfo->psecinfo = NULL;
	    if (remove_hook)
	      (*remove_hook) (abfd, secinfo->sec);
	  }
	else if (! record_section (sinfo, secinfo))
	  break;

      if (secinfo)
	continue;

      if (sinfo->htab->first == NULL)
	continue;

      if (sinfo->htab->strings)
	merge_strings (sinfo);
      else
	{
	  struct sec_merge_hash_entry *e;
	  bfd_size_type size = 0;

	  /* Things are much simpler for non-strings.
	     Just assign them slots in the section.  */
	  secinfo = NULL;
	  for (e = sinfo->htab->first; e; e = e->next)
	    {
	      if (e->secinfo->first == NULL)
		{
		  if (secinfo)
		    secinfo->sec->_cooked_size = size;
		  e->secinfo->first = e;
		  size = 0;
		}
	      size = (size + e->alignment - 1)
		     & ~((bfd_vma) e->alignment - 1);
	      e->u.index = size;
	      size += e->len;
	      secinfo = e->secinfo;
	    }
	  secinfo->sec->_cooked_size = size;
	}

	/* Finally shrink all input sections which have not made it into
	   the hash table at all.  */
	for (secinfo = sinfo->chain; secinfo; secinfo = secinfo->next)
  	  if (secinfo->first == NULL)
	    secinfo->sec->_cooked_size = 0;
    }

  return true;
}

/* Write out the merged section.  */

boolean
_bfd_write_merged_section (output_bfd, sec, psecinfo)
     bfd *output_bfd;
     asection *sec;
     PTR psecinfo;
{
  struct sec_merge_sec_info *secinfo;
  file_ptr pos;

  secinfo = (struct sec_merge_sec_info *) psecinfo;

  if (!secinfo->first)
    return true;

  pos = sec->output_section->filepos + sec->output_offset;
  if (bfd_seek (output_bfd, pos, SEEK_SET) != 0)
    return false;

  if (! sec_merge_emit (output_bfd, secinfo->first))
    return false;

  return true;
}

/* Adjust an address in the SEC_MERGE section.  Given OFFSET within
   *PSEC, this returns the new offset in the adjusted SEC_MERGE
   section and writes the new section back into *PSEC.  */

bfd_vma
_bfd_merged_section_offset (output_bfd, psec, psecinfo, offset, addend)
     bfd *output_bfd ATTRIBUTE_UNUSED;
     asection **psec;
     PTR psecinfo;
     bfd_vma offset, addend;
{
  struct sec_merge_sec_info *secinfo;
  struct sec_merge_hash_entry *entry;
  unsigned char *p;
  asection *sec = *psec;

  secinfo = (struct sec_merge_sec_info *) psecinfo;

  if (offset + addend >= sec->_raw_size)
    {
      if (offset + addend > sec->_raw_size)
	{
	  (*_bfd_error_handler)
	    (_("%s: access beyond end of merged section (%ld + %ld)"),
	     bfd_get_filename (sec->owner), (long) offset, (long) addend);
	}
      return (secinfo->first ? sec->_cooked_size : 0);
    }

  if (secinfo->htab->strings)
    {
      if (sec->entsize == 1)
	{
	  p = secinfo->contents + offset + addend - 1;
	  while (p >= secinfo->contents && *p)
	    --p;
	  ++p;
	}
      else
	{
	  p = secinfo->contents
	      + ((offset + addend) / sec->entsize) * sec->entsize;
	  p -= sec->entsize;
	  while (p >= secinfo->contents)
	    {
	      unsigned int i;

	      for (i = 0; i < sec->entsize; ++i)
		if (p[i] != '\0')
		  break;
	      if (i == sec->entsize)
		break;
	      p -= sec->entsize;
	    }
	  p += sec->entsize;
	}
    }
  else
    {
      p = secinfo->contents
	  + ((offset + addend) / sec->entsize) * sec->entsize;
    }
  entry = sec_merge_hash_lookup (secinfo->htab, p, 0, false);
  if (!entry)
    {
      if (! secinfo->htab->strings)
	abort ();
      /* This should only happen if somebody points into the padding
	 after a NUL character but before next entity.  */
      if (*p)
	abort ();
      if (! secinfo->htab->first)
	abort ();
      entry = secinfo->htab->first;
      p = secinfo->contents
	  + ((offset + addend) / sec->entsize + 1) * sec->entsize
	  - entry->len;
    }

  *psec = entry->secinfo->sec;
  return entry->u.index + (secinfo->contents + offset - p);
}
