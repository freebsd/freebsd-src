/* BFD library -- caching of file descriptors.
   Copyright 1990, 1991, 1992, 1993, 1994, 1996, 2000, 2001
   Free Software Foundation, Inc.
   Hacked by Steve Chamberlain of Cygnus Support (steve@cygnus.com).

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

/*
SECTION
	File caching

	The file caching mechanism is embedded within BFD and allows
	the application to open as many BFDs as it wants without
	regard to the underlying operating system's file descriptor
	limit (often as low as 20 open files).  The module in
	<<cache.c>> maintains a least recently used list of
	<<BFD_CACHE_MAX_OPEN>> files, and exports the name
	<<bfd_cache_lookup>>, which runs around and makes sure that
	the required BFD is open. If not, then it chooses a file to
	close, closes it and opens the one wanted, returning its file
	handle.

*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

static void insert PARAMS ((bfd *));
static void snip PARAMS ((bfd *));
static boolean close_one PARAMS ((void));
static boolean bfd_cache_delete PARAMS ((bfd *));

/*
INTERNAL_FUNCTION
	BFD_CACHE_MAX_OPEN macro

DESCRIPTION
	The maximum number of files which the cache will keep open at
	one time.

.#define BFD_CACHE_MAX_OPEN 10

*/

/* The number of BFD files we have open.  */

static int open_files;

/*
INTERNAL_FUNCTION
	bfd_last_cache

SYNOPSIS
	extern bfd *bfd_last_cache;

DESCRIPTION
	Zero, or a pointer to the topmost BFD on the chain.  This is
	used by the <<bfd_cache_lookup>> macro in @file{libbfd.h} to
	determine when it can avoid a function call.
*/

bfd *bfd_last_cache;

/*
  INTERNAL_FUNCTION
  	bfd_cache_lookup

  DESCRIPTION
 	Check to see if the required BFD is the same as the last one
 	looked up. If so, then it can use the stream in the BFD with
 	impunity, since it can't have changed since the last lookup;
 	otherwise, it has to perform the complicated lookup function.

  .#define bfd_cache_lookup(x) \
  .    ((x)==bfd_last_cache? \
  .      (FILE*) (bfd_last_cache->iostream): \
  .       bfd_cache_lookup_worker(x))

 */

/* Insert a BFD into the cache.  */

static INLINE void
insert (abfd)
     bfd *abfd;
{
  if (bfd_last_cache == NULL)
    {
      abfd->lru_next = abfd;
      abfd->lru_prev = abfd;
    }
  else
    {
      abfd->lru_next = bfd_last_cache;
      abfd->lru_prev = bfd_last_cache->lru_prev;
      abfd->lru_prev->lru_next = abfd;
      abfd->lru_next->lru_prev = abfd;
    }
  bfd_last_cache = abfd;
}

/* Remove a BFD from the cache.  */

static INLINE void
snip (abfd)
     bfd *abfd;
{
  abfd->lru_prev->lru_next = abfd->lru_next;
  abfd->lru_next->lru_prev = abfd->lru_prev;
  if (abfd == bfd_last_cache)
    {
      bfd_last_cache = abfd->lru_next;
      if (abfd == bfd_last_cache)
	bfd_last_cache = NULL;
    }
}

/* We need to open a new file, and the cache is full.  Find the least
   recently used cacheable BFD and close it.  */

static boolean
close_one ()
{
  register bfd *kill;

  if (bfd_last_cache == NULL)
    kill = NULL;
  else
    {
      for (kill = bfd_last_cache->lru_prev;
	   ! kill->cacheable;
	   kill = kill->lru_prev)
	{
	  if (kill == bfd_last_cache)
	    {
	      kill = NULL;
	      break;
	    }
	}
    }

  if (kill == NULL)
    {
      /* There are no open cacheable BFD's.  */
      return true;
    }

  kill->where = ftell ((FILE *) kill->iostream);

  return bfd_cache_delete (kill);
}

/* Close a BFD and remove it from the cache.  */

static boolean
bfd_cache_delete (abfd)
     bfd *abfd;
{
  boolean ret;

  if (fclose ((FILE *) abfd->iostream) == 0)
    ret = true;
  else
    {
      ret = false;
      bfd_set_error (bfd_error_system_call);
    }

  snip (abfd);

  abfd->iostream = NULL;
  --open_files;

  return ret;
}

/*
INTERNAL_FUNCTION
	bfd_cache_init

SYNOPSIS
	boolean bfd_cache_init (bfd *abfd);

DESCRIPTION
	Add a newly opened BFD to the cache.
*/

boolean
bfd_cache_init (abfd)
     bfd *abfd;
{
  BFD_ASSERT (abfd->iostream != NULL);
  if (open_files >= BFD_CACHE_MAX_OPEN)
    {
      if (! close_one ())
	return false;
    }
  insert (abfd);
  ++open_files;
  return true;
}

/*
INTERNAL_FUNCTION
	bfd_cache_close

SYNOPSIS
	boolean bfd_cache_close (bfd *abfd);

DESCRIPTION
	Remove the BFD @var{abfd} from the cache. If the attached file is open,
	then close it too.

RETURNS
	<<false>> is returned if closing the file fails, <<true>> is
	returned if all is well.
*/

boolean
bfd_cache_close (abfd)
     bfd *abfd;
{
  if (abfd->iostream == NULL
      || (abfd->flags & BFD_IN_MEMORY) != 0)
    return true;

  return bfd_cache_delete (abfd);
}

/*
INTERNAL_FUNCTION
	bfd_open_file

SYNOPSIS
	FILE* bfd_open_file(bfd *abfd);

DESCRIPTION
	Call the OS to open a file for @var{abfd}.  Return the <<FILE *>>
	(possibly <<NULL>>) that results from this operation.  Set up the
	BFD so that future accesses know the file is open. If the <<FILE *>>
	returned is <<NULL>>, then it won't have been put in the
	cache, so it won't have to be removed from it.
*/

FILE *
bfd_open_file (abfd)
     bfd *abfd;
{
  abfd->cacheable = true;	/* Allow it to be closed later.  */

  if (open_files >= BFD_CACHE_MAX_OPEN)
    {
      if (! close_one ())
	return NULL;
    }

  switch (abfd->direction)
    {
    case read_direction:
    case no_direction:
      abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_RB);
      break;
    case both_direction:
    case write_direction:
      if (abfd->opened_once == true)
	{
	  abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_RUB);
	  if (abfd->iostream == NULL)
	    abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_WUB);
	}
      else
	{
	  /* Create the file.

	     Some operating systems won't let us overwrite a running
	     binary.  For them, we want to unlink the file first.

	     However, gcc 2.95 will create temporary files using
	     O_EXCL and tight permissions to prevent other users from
	     substituting other .o files during the compilation.  gcc
	     will then tell the assembler to use the newly created
	     file as an output file.  If we unlink the file here, we
	     open a brief window when another user could still
	     substitute a file.

	     So we unlink the output file if and only if it has
	     non-zero size.  */
#ifndef __MSDOS__
	  /* Don't do this for MSDOS: it doesn't care about overwriting
	     a running binary, but if this file is already open by
	     another BFD, we will be in deep trouble if we delete an
	     open file.  In fact, objdump does just that if invoked with
	     the --info option.  */
	  struct stat s;

	  if (stat (abfd->filename, &s) == 0 && s.st_size != 0)
	    unlink (abfd->filename);
#endif
	  abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_WUB);
	  abfd->opened_once = true;
	}
      break;
    }

  if (abfd->iostream != NULL)
    {
      if (! bfd_cache_init (abfd))
	return NULL;
    }

  return (FILE *) abfd->iostream;
}

/*
INTERNAL_FUNCTION
	bfd_cache_lookup_worker

SYNOPSIS
	FILE *bfd_cache_lookup_worker(bfd *abfd);

DESCRIPTION
	Called when the macro <<bfd_cache_lookup>> fails to find a
	quick answer.  Find a file descriptor for @var{abfd}.  If
	necessary, it open it.  If there are already more than
	<<BFD_CACHE_MAX_OPEN>> files open, it tries to close one first, to
	avoid running out of file descriptors.
*/

FILE *
bfd_cache_lookup_worker (abfd)
     bfd *abfd;
{
  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    abort ();

  if (abfd->my_archive)
    abfd = abfd->my_archive;

  if (abfd->iostream != NULL)
    {
      /* Move the file to the start of the cache.  */
      if (abfd != bfd_last_cache)
	{
	  snip (abfd);
	  insert (abfd);
	}
    }
  else
    {
      if (bfd_open_file (abfd) == NULL)
	return NULL;
      if (abfd->where != (unsigned long) abfd->where)
	return NULL;
      if (fseek ((FILE *) abfd->iostream, (long) abfd->where, SEEK_SET) != 0)
	return NULL;
    }

  return (FILE *) abfd->iostream;
}
