/* More debugging hooks for `mmalloc'.
   Copyright 1991, 1992 Free Software Foundation

   Written April 2, 1991 by John Gilmore of Cygnus Support
   Based on mcheck.c by Mike Haertel.
   Modified Mar 1992 by Fred Fish.  (fnf@cygnus.com)

This file is part of the GNU C Library.

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
Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "mmalloc.h"

#ifndef	__GNU_LIBRARY__
extern char *getenv ();
#endif

static FILE *mallstream;

#if 0	/* FIXME:  Disabled for now. */
static char mallenv[] = "MALLOC_TRACE";
static char mallbuf[BUFSIZ];	/* Buffer for the output.  */
#endif

/* Address to breakpoint on accesses to... */
static PTR mallwatch;

/* Old hook values.  */

static void (*old_mfree_hook) PARAMS ((PTR, PTR));
static PTR (*old_mmalloc_hook) PARAMS ((PTR, size_t));
static PTR (*old_mrealloc_hook) PARAMS ((PTR, PTR, size_t));

/* This function is called when the block being alloc'd, realloc'd, or
   freed has an address matching the variable "mallwatch".  In a debugger,
   set "mallwatch" to the address of interest, then put a breakpoint on
   tr_break.  */

static void
tr_break ()
{
}

static void
tr_freehook (md, ptr)
  PTR md;
  PTR ptr;
{
  struct mdesc *mdp;

  mdp = MD_TO_MDP (md);
  /* Be sure to print it first.  */
  fprintf (mallstream, "- %08x\n", (unsigned int) ptr);
  if (ptr == mallwatch)
    tr_break ();
  mdp -> mfree_hook = old_mfree_hook;
  mfree (md, ptr);
  mdp -> mfree_hook = tr_freehook;
}

static PTR
tr_mallochook (md, size)
  PTR md;
  size_t size;
{
  PTR hdr;
  struct mdesc *mdp;

  mdp = MD_TO_MDP (md);
  mdp -> mmalloc_hook = old_mmalloc_hook;
  hdr = (PTR) mmalloc (md, size);
  mdp -> mmalloc_hook = tr_mallochook;

  /* We could be printing a NULL here; that's OK.  */
  fprintf (mallstream, "+ %08x %x\n", (unsigned int) hdr, size);

  if (hdr == mallwatch)
    tr_break ();

  return (hdr);
}

static PTR
tr_reallochook (md, ptr, size)
  PTR md;
  PTR ptr;
  size_t size;
{
  PTR hdr;
  struct mdesc *mdp;

  mdp = MD_TO_MDP (md);

  if (ptr == mallwatch)
    tr_break ();

  mdp -> mfree_hook = old_mfree_hook;
  mdp -> mmalloc_hook = old_mmalloc_hook;
  mdp -> mrealloc_hook = old_mrealloc_hook;
  hdr = (PTR) mrealloc (md, ptr, size);
  mdp -> mfree_hook = tr_freehook;
  mdp -> mmalloc_hook = tr_mallochook;
  mdp -> mrealloc_hook = tr_reallochook;
  if (hdr == NULL)
    /* Failed realloc.  */
    fprintf (mallstream, "! %08x %x\n", (unsigned int) ptr, size);
  else
    fprintf (mallstream, "< %08x\n> %08x %x\n", (unsigned int) ptr,
	     (unsigned int) hdr, size);

  if (hdr == mallwatch)
    tr_break ();

  return hdr;
}

/* We enable tracing if either the environment variable MALLOC_TRACE
   is set, or if the variable mallwatch has been patched to an address
   that the debugging user wants us to stop on.  When patching mallwatch,
   don't forget to set a breakpoint on tr_break!  */

int
mmtrace ()
{
#if 0	/* FIXME!  This is disabled for now until we figure out how to
	   maintain a stack of hooks per heap, since we might have other
	   hooks (such as set by mmcheck) active also. */
  char *mallfile;

  mallfile = getenv (mallenv);
  if (mallfile  != NULL || mallwatch != NULL)
    {
      mallstream = fopen (mallfile != NULL ? mallfile : "/dev/null", "w");
      if (mallstream != NULL)
	{
	  /* Be sure it doesn't mmalloc its buffer!  */
	  setbuf (mallstream, mallbuf);
	  fprintf (mallstream, "= Start\n");
	  old_mfree_hook = mdp -> mfree_hook;
	  mdp -> mfree_hook = tr_freehook;
	  old_mmalloc_hook = mdp -> mmalloc_hook;
	  mdp -> mmalloc_hook = tr_mallochook;
	  old_mrealloc_hook = mdp -> mrealloc_hook;
	  mdp -> mrealloc_hook = tr_reallochook;
	}
    }

#endif	/* 0 */

  return (1);
}

