/* Standard debugging hooks for `mmalloc'.
   Copyright 1990, 1991, 1992 Free Software Foundation

   Written May 1989 by Mike Haertel.
   Heavily modified Mar 1992 by Fred Fish (fnf@cygnus.com)

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

 The author may be reached (Email) at the address mike@ai.mit.edu,
 or (US mail) as Mike Haertel c/o Free Software Foundation. */

#include "mmalloc.h"

/* Default function to call when something awful happens.  The application
   can specify an alternate function to be called instead (and probably will
   want to). */

extern void abort PARAMS ((void));

/* Arbitrary magical numbers.  */

#define MAGICWORD	(unsigned int) 0xfedabeeb	/* Active chunk */
#define MAGICWORDFREE	(unsigned int) 0xdeadbeef	/* Inactive chunk */
#define MAGICBYTE	((char) 0xd7)

/* Each memory allocation is bounded by a header structure and a trailer
   byte.  I.E.

	<size><magicword><user's allocation><magicbyte>

   The pointer returned to the user points to the first byte in the
   user's allocation area.  The magic word can be tested to detect
   buffer underruns and the magic byte can be tested to detect overruns. */

struct hdr
  {
    size_t size;		/* Exact size requested by user.  */
    unsigned long int magic;	/* Magic number to check header integrity.  */
  };

/* Check the magicword and magicbyte, and if either is corrupted then
   call the emergency abort function specified for the heap in use. */

static void
checkhdr (mdp, hdr)
  struct mdesc *mdp;
  CONST struct hdr *hdr;
{
  if (hdr -> magic != MAGICWORD ||
      ((char *) &hdr[1])[hdr -> size] != MAGICBYTE)
    {
      (*mdp -> abortfunc)();
    }
}

static void
mfree_check (md, ptr)
  PTR md;
  PTR ptr;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  struct mdesc *mdp;

  mdp = MD_TO_MDP (md);
  checkhdr (mdp, hdr);
  hdr -> magic = MAGICWORDFREE;
  mdp -> mfree_hook = NULL;
  mfree (md, (PTR)hdr);
  mdp -> mfree_hook = mfree_check;
}

static PTR
mmalloc_check (md, size)
  PTR md;
  size_t size;
{
  struct hdr *hdr;
  struct mdesc *mdp;
  size_t nbytes;

  mdp = MD_TO_MDP (md);
  mdp -> mmalloc_hook = NULL;
  nbytes = sizeof (struct hdr) + size + 1;
  hdr = (struct hdr *) mmalloc (md, nbytes);
  mdp -> mmalloc_hook = mmalloc_check;
  if (hdr != NULL)
    {
      hdr -> size = size;
      hdr -> magic = MAGICWORD;
      hdr++;
      *((char *) hdr + size) = MAGICBYTE;
    }
  return ((PTR) hdr);
}

static PTR
mrealloc_check (md, ptr, size)
  PTR md;
  PTR ptr;
  size_t size;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  struct mdesc *mdp;
  size_t nbytes;

  mdp = MD_TO_MDP (md);
  checkhdr (mdp, hdr);
  mdp -> mfree_hook = NULL;
  mdp -> mmalloc_hook = NULL;
  mdp -> mrealloc_hook = NULL;
  nbytes = sizeof (struct hdr) + size + 1;
  hdr = (struct hdr *) mrealloc (md, (PTR) hdr, nbytes);
  mdp -> mfree_hook = mfree_check;
  mdp -> mmalloc_hook = mmalloc_check;
  mdp -> mrealloc_hook = mrealloc_check;
  if (hdr != NULL)
    {
      hdr -> size = size;
      hdr++;
      *((char *) hdr + size) = MAGICBYTE;
    }
  return ((PTR) hdr);
}

/* Turn on default checking for mmalloc/mrealloc/mfree, for the heap specified
   by MD.  If FUNC is non-NULL, it is a pointer to the function to call
   to abort whenever memory corruption is detected.  By default, this is the
   standard library function abort().

   Note that we disallow installation of initial checking hooks if mmalloc
   has been called at any time for this particular heap, since if any region
   that is allocated prior to installation of the hooks is subsequently
   reallocated or freed after installation of the hooks, it is guaranteed
   to trigger a memory corruption error.  We do this by checking the state
   of the MMALLOC_INITIALIZED flag.

   However, we can call this function at any time after the initial call,
   to update the function pointers to the checking routines and to the
   user defined corruption handler routine, as long as these function pointers
   have been previously extablished by the initial call.  Note that we
   do this automatically when remapping an previously used heap, to ensure
   that the hooks get updated to the correct values, although the corruption
   handler pointer gets set back to the default.  The application can then
   call mmcheck to use a different corruption handler if desired.

   Returns non-zero if checking is successfully enabled, zero otherwise. */

int
mmcheck (md, func)
  PTR md;
  void (*func) PARAMS ((void));
{
  struct mdesc *mdp;
  int rtnval;

  mdp = MD_TO_MDP (md);

  /* We can safely set or update the abort function at any time, regardless
     of whether or not we successfully do anything else. */

  mdp -> abortfunc = (func != NULL ? func : abort);

  /* If we haven't yet called mmalloc the first time for this heap, or if we
     have hooks that were previously installed, then allow the hooks to be
     initialized or updated. */

  if (1 /* FIXME: Always allow installation for now. */ ||
      !(mdp -> flags & MMALLOC_INITIALIZED) ||
      (mdp -> mfree_hook != NULL))
    {
      mdp -> mfree_hook = mfree_check;
      mdp -> mmalloc_hook = mmalloc_check;
      mdp -> mrealloc_hook = mrealloc_check;
      mdp -> flags |= MMALLOC_MMCHECK_USED;
      rtnval = 1;
    }
  else
    {
      rtnval = 0;
    }

  return (rtnval);
}
