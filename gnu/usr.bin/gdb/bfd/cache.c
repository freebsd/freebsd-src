/* BFD library -- caching of file descriptors.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
SECTION
	File Caching

	The file caching mechanism is embedded within BFD and allows
	the application to open as many BFDs as it wants without
	regard to the underlying operating system's file descriptor
	limit (often as low as 20 open files).  The module in
	<<cache.c>> maintains a least recently used list of
	<<BFD_CACHE_MAX_OPEN>> files, and exports the name
	<<bfd_cache_lookup>> which runs around and makes sure that
	the required BFD is open. If not, then it chooses a file to
	close, closes it and opens the one wanted, returning its file
	handle. 

*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/*
INTERNAL_FUNCTION
	BFD_CACHE_MAX_OPEN macro

DESCRIPTION
	The maximum number of files which the cache will keep open at
	one time.

.#define BFD_CACHE_MAX_OPEN 10

*/


static boolean
bfd_cache_delete PARAMS ((bfd *));

/* Number of bfds on the chain.  All such bfds have their file open;
   if it closed, they get snipd()d from the chain.  */

static int open_files;

static bfd *cache_sentinel;	/* Chain of BFDs with active fds we've
				   opened */

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
 * INTERNAL_FUNCTION
 * 	bfd_cache_lookup
 *
 * DESCRIPTION
 *	Checks to see if the required BFD is the same as the last one
 *	looked up. If so then it can use the iostream in the BFD with
 *	impunity, since it can't have changed since the last lookup,
 *	otherwise it has to perform the complicated lookup function 
 *
 * .#define bfd_cache_lookup(x) \
 * .    ((x)==bfd_last_cache? \
 * .      (FILE*)(bfd_last_cache->iostream): \
 * .       bfd_cache_lookup_worker(x))
 *
 *
 */

static void
DEFUN_VOID(close_one)
{
    bfd *kill = cache_sentinel;
    if (kill == 0)		/* Nothing in the cache */
	return ;

    /* We can only close files that want to play this game.  */
    while (!kill->cacheable) {
	kill = kill->lru_prev;
	if (kill == cache_sentinel) /* Nobody wants to play */
	   return ;
    }

    kill->where = ftell((FILE *)(kill->iostream));
    (void) bfd_cache_delete(kill);
}

/* Cuts the BFD abfd out of the chain in the cache */
static void 
DEFUN(snip,(abfd),
      bfd *abfd)
{
  abfd->lru_prev->lru_next = abfd->lru_next;
  abfd->lru_next->lru_prev = abfd->lru_prev; 
  if (cache_sentinel == abfd) cache_sentinel = (bfd *)NULL;
}

static boolean
DEFUN(bfd_cache_delete,(abfd),
      bfd *abfd)
{
  boolean ret;

  if (fclose ((FILE *)(abfd->iostream)) == 0)
    ret = true;
  else
    {
      ret = false;
      bfd_error = system_call_error;
    }
  snip (abfd);
  abfd->iostream = NULL;
  open_files--;
  bfd_last_cache = 0;
  return ret;
}
  
static bfd *
DEFUN(insert,(x,y),
      bfd *x AND
      bfd *y)
{
  if (y) {
    x->lru_next = y;
    x->lru_prev = y->lru_prev;
    y->lru_prev->lru_next = x;
    y->lru_prev = x;

  }
  else {
    x->lru_prev = x;
    x->lru_next = x;
  }
  return x;
}


/* Initialize a BFD by putting it on the cache LRU.  */

void
DEFUN(bfd_cache_init,(abfd),
      bfd *abfd)
{
  if (open_files >= BFD_CACHE_MAX_OPEN)
    close_one ();
  cache_sentinel = insert(abfd, cache_sentinel);
  ++open_files;
}


/*
INTERNAL_FUNCTION
	bfd_cache_close

DESCRIPTION
	Remove the BFD from the cache. If the attached file is open,
	then close it too.

SYNOPSIS
	boolean bfd_cache_close (bfd *);

RETURNS
	<<false>> is returned if closing the file fails, <<true>> is
	returned if all is well.
*/
boolean
DEFUN(bfd_cache_close,(abfd),
      bfd *abfd)
{
  /* If this file is open then remove from the chain */
  if (abfd->iostream) 
    {
      return bfd_cache_delete(abfd);
    }
  else
    {
      return true;
    }
}

/*
INTERNAL_FUNCTION
	bfd_open_file

DESCRIPTION
	Call the OS to open a file for this BFD.  Returns the FILE *
	(possibly null) that results from this operation.  Sets up the
	BFD so that future accesses know the file is open. If the FILE
	* returned is null, then there is won't have been put in the
	cache, so it won't have to be removed from it.

SYNOPSIS
	FILE* bfd_open_file(bfd *);
*/

FILE *
DEFUN(bfd_open_file, (abfd),
      bfd *abfd)
{
  abfd->cacheable = true;	/* Allow it to be closed later. */

  if(open_files >= BFD_CACHE_MAX_OPEN) {
    close_one();
  }

  switch (abfd->direction) {
  case read_direction:
  case no_direction:
    abfd->iostream = (char *) fopen(abfd->filename, FOPEN_RB);
    break;
  case both_direction:
  case write_direction:
    if (abfd->opened_once == true) {
      abfd->iostream = (char *) fopen(abfd->filename, FOPEN_RUB);
      if (!abfd->iostream) {
	abfd->iostream = (char *) fopen(abfd->filename, FOPEN_WUB);
      }
    } else {
      /*open for creat */
      abfd->iostream = (char *) fopen(abfd->filename, FOPEN_WB);
      abfd->opened_once = true;
    }
    break;
  }

  if (abfd->iostream) {
    bfd_cache_init (abfd);
  }

  return (FILE *)(abfd->iostream);
}

/*
INTERNAL_FUNCTION
	bfd_cache_lookup_worker

DESCRIPTION
	Called when the macro <<bfd_cache_lookup>> fails to find a
	quick answer. Finds a file descriptor for this BFD.  If
	necessary, it open it. If there are already more than
	BFD_CACHE_MAX_OPEN files open, it trys to close one first, to
	avoid running out of file descriptors.  

SYNOPSIS
	FILE *bfd_cache_lookup_worker(bfd *);

*/

FILE *
DEFUN(bfd_cache_lookup_worker,(abfd),
      bfd *abfd)
{
  if (abfd->my_archive) 
      {
	abfd = abfd->my_archive;
      }
  /* Is this file already open .. if so then quick exit */
  if (abfd->iostream) 
      {
	if (abfd != cache_sentinel) {
	  /* Place onto head of lru chain */
	  snip (abfd);
	  cache_sentinel = insert(abfd, cache_sentinel);
	}
      }
  /* This is a BFD without a stream -
     so it must have been closed or never opened.
     find an empty cache entry and use it.  */
  else 
      {

	if (open_files >= BFD_CACHE_MAX_OPEN) 
	    {
	      close_one();
	    }

	BFD_ASSERT(bfd_open_file (abfd) != (FILE *)NULL) ;
	fseek((FILE *)(abfd->iostream), abfd->where, false);
      }
  bfd_last_cache = abfd;
  return (FILE *)(abfd->iostream);
}
