/* Standard debugging hooks for `malloc'.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation
   Written May 1989 by Mike Haertel.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.  */

#ifndef	_MALLOC_INTERNAL
#define	_MALLOC_INTERNAL
#include <malloc.h>
#endif

/* Old hook values.  */
static void (*old_free_hook) __P ((__ptr_t ptr));
static __ptr_t (*old_malloc_hook) __P ((size_t size));
static __ptr_t (*old_realloc_hook) __P ((__ptr_t ptr, size_t size));

/* Function to call when something awful happens.  */
static void (*abortfunc) __P ((void));

/* Arbitrary magical numbers.  */
#define MAGICWORD	0xfedabeeb
#define MAGICBYTE	((char) 0xd7)

struct hdr
  {
    size_t size;		/* Exact size requested by user.  */
    unsigned long int magic;	/* Magic number to check header integrity.  */
  };

static void checkhdr __P ((const struct hdr *));
static void
checkhdr (hdr)
     const struct hdr *hdr;
{
  if (hdr->magic != MAGICWORD || ((char *) &hdr[1])[hdr->size] != MAGICBYTE)
    (*abortfunc) ();
}

static void freehook __P ((__ptr_t));
static void
freehook (ptr)
     __ptr_t ptr;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  checkhdr (hdr);
  hdr->magic = 0;
  __free_hook = old_free_hook;
  free (hdr);
  __free_hook = freehook;
}

static __ptr_t mallochook __P ((size_t));
static __ptr_t
mallochook (size)
     size_t size;
{
  struct hdr *hdr;

  __malloc_hook = old_malloc_hook;
  hdr = (struct hdr *) malloc (sizeof (struct hdr) + size + 1);
  __malloc_hook = mallochook;
  if (hdr == NULL)
    return NULL;

  hdr->size = size;
  hdr->magic = MAGICWORD;
  ((char *) &hdr[1])[size] = MAGICBYTE;
  return (__ptr_t) (hdr + 1);
}

static __ptr_t reallochook __P ((__ptr_t, size_t));
static __ptr_t
reallochook (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;

  checkhdr (hdr);
  __free_hook = old_free_hook;
  __malloc_hook = old_malloc_hook;
  __realloc_hook = old_realloc_hook;
  hdr = (struct hdr *) realloc ((__ptr_t) hdr, sizeof (struct hdr) + size + 1);
  __free_hook = freehook;
  __malloc_hook = mallochook;
  __realloc_hook = reallochook;
  if (hdr == NULL)
    return NULL;

  hdr->size = size;
  ((char *) &hdr[1])[size] = MAGICBYTE;
  return (__ptr_t) (hdr + 1);
}

int
mcheck (func)
     void (*func) __P ((void));
{
  extern void abort __P ((void));
  static int mcheck_used = 0;

  abortfunc = (func != NULL) ? func : abort;

  /* These hooks may not be safely inserted if malloc is already in use.  */
  if (!__malloc_initialized && !mcheck_used)
    {
      old_free_hook = __free_hook;
      __free_hook = freehook;
      old_malloc_hook = __malloc_hook;
      __malloc_hook = mallochook;
      old_realloc_hook = __realloc_hook;
      __realloc_hook = reallochook;
      mcheck_used = 1;
    }

  return mcheck_used ? 0 : -1;
}
