/* opncls.c -- open and close a BFD.
   Copyright (C) 1990, 91, 92, 93, 94, 95, 96, 1997
   Free Software Foundation, Inc.

   Written by Cygnus Support.

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
#include "objalloc.h"
#include "libbfd.h"

#ifndef S_IXUSR
#define S_IXUSR 0100	/* Execute by owner.  */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010	/* Execute by group.  */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001	/* Execute by others.  */
#endif

/* fdopen is a loser -- we should use stdio exclusively.  Unfortunately
   if we do that we can't use fcntl.  */

/* FIXME: This is no longer used.  */
long _bfd_chunksize = -1;

/* Return a new BFD.  All BFD's are allocated through this routine.  */

bfd *
_bfd_new_bfd ()
{
  bfd *nbfd;

  nbfd = (bfd *) bfd_zmalloc (sizeof (bfd));
  if (nbfd == NULL)
    return NULL;

  nbfd->memory = (PTR) objalloc_create ();
  if (nbfd->memory == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  nbfd->arch_info = &bfd_default_arch_struct;

  nbfd->direction = no_direction;
  nbfd->iostream = NULL;
  nbfd->where = 0;
  nbfd->sections = (asection *) NULL;
  nbfd->format = bfd_unknown;
  nbfd->my_archive = (bfd *) NULL;
  nbfd->origin = 0;				
  nbfd->opened_once = false;
  nbfd->output_has_begun = false;
  nbfd->section_count = 0;
  nbfd->usrdata = (PTR) NULL;
  nbfd->cacheable = false;
  nbfd->flags = BFD_NO_FLAGS;
  nbfd->mtime_set = false;

  return nbfd;
}

/* Allocate a new BFD as a member of archive OBFD.  */

bfd *
_bfd_new_bfd_contained_in (obfd)
     bfd *obfd;
{
  bfd *nbfd;

  nbfd = _bfd_new_bfd ();
  nbfd->xvec = obfd->xvec;
  nbfd->my_archive = obfd;
  nbfd->direction = read_direction;
  nbfd->target_defaulted = obfd->target_defaulted;
  return nbfd;
}

/*
SECTION
	Opening and closing BFDs

*/

/*
FUNCTION
	bfd_openr

SYNOPSIS
        bfd *bfd_openr(CONST char *filename, CONST char *target);

DESCRIPTION
	Open the file @var{filename} (using <<fopen>>) with the target
	@var{target}.  Return a pointer to the created BFD.

	Calls <<bfd_find_target>>, so @var{target} is interpreted as by
	that function.

	If <<NULL>> is returned then an error has occured.   Possible errors
	are <<bfd_error_no_memory>>, <<bfd_error_invalid_target>> or <<system_call>> error.
*/

bfd *
bfd_openr (filename, target)
     CONST char *filename;
     CONST char *target;
{
  bfd *nbfd;
  const bfd_target *target_vec;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      bfd_set_error (bfd_error_invalid_target);
      return NULL;
    }

  nbfd->filename = filename;
  nbfd->direction = read_direction;

  if (bfd_open_file (nbfd) == NULL)
    {
      /* File didn't exist, or some such */
      bfd_set_error (bfd_error_system_call);
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }

  return nbfd;
}

/* Don't try to `optimize' this function:

   o - We lock using stack space so that interrupting the locking
       won't cause a storage leak.
   o - We open the file stream last, since we don't want to have to
       close it if anything goes wrong.  Closing the stream means closing
       the file descriptor too, even though we didn't open it.
 */
/*
FUNCTION
         bfd_fdopenr

SYNOPSIS
         bfd *bfd_fdopenr(CONST char *filename, CONST char *target, int fd);

DESCRIPTION
         <<bfd_fdopenr>> is to <<bfd_fopenr>> much like <<fdopen>> is to <<fopen>>.
	 It opens a BFD on a file already described by the @var{fd}
	 supplied.

	 When the file is later <<bfd_close>>d, the file descriptor will be closed.

	 If the caller desires that this file descriptor be cached by BFD
	 (opened as needed, closed as needed to free descriptors for
	 other opens), with the supplied @var{fd} used as an initial
	 file descriptor (but subject to closure at any time), call
	 bfd_set_cacheable(bfd, 1) on the returned BFD.  The default is to
	 assume no cacheing; the file descriptor will remain open until
	 <<bfd_close>>, and will not be affected by BFD operations on other
	 files.

         Possible errors are <<bfd_error_no_memory>>, <<bfd_error_invalid_target>> and <<bfd_error_system_call>>.
*/

bfd *
bfd_fdopenr (filename, target, fd)
     CONST char *filename;
     CONST char *target;
     int fd;
{
  bfd *nbfd;
  const bfd_target *target_vec;
  int fdflags;

  bfd_set_error (bfd_error_system_call);
#if ! defined(HAVE_FCNTL) || ! defined(F_GETFL)
  fdflags = O_RDWR;			/* Assume full access */
#else
  fdflags = fcntl (fd, F_GETFL, NULL);
#endif
  if (fdflags == -1) return NULL;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      bfd_set_error (bfd_error_invalid_target);
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }

#ifndef HAVE_FDOPEN
  nbfd->iostream = (PTR) fopen (filename, FOPEN_RB);
#else
  /* (O_ACCMODE) parens are to avoid Ultrix header file bug */
  switch (fdflags & (O_ACCMODE))
    {
    case O_RDONLY: nbfd->iostream = (PTR) fdopen (fd, FOPEN_RB);   break;
    case O_WRONLY: nbfd->iostream = (PTR) fdopen (fd, FOPEN_RUB);  break;
    case O_RDWR:   nbfd->iostream = (PTR) fdopen (fd, FOPEN_RUB);  break;
    default: abort ();
    }
#endif

  if (nbfd->iostream == NULL)
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }

  /* OK, put everything where it belongs */

  nbfd->filename = filename;

  /* As a special case we allow a FD open for read/write to
     be written through, although doing so requires that we end
     the previous clause with a preposition.  */
  /* (O_ACCMODE) parens are to avoid Ultrix header file bug */
  switch (fdflags & O_ACCMODE)
    {
    case O_RDONLY: nbfd->direction = read_direction; break;
    case O_WRONLY: nbfd->direction = write_direction; break;
    case O_RDWR: nbfd->direction = both_direction; break;
    default: abort ();
    }

  if (! bfd_cache_init (nbfd))
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }
  nbfd->opened_once = true;

  return nbfd;
}

/*
FUNCTION
	bfd_openstreamr

SYNOPSIS
	bfd *bfd_openstreamr(const char *, const char *, PTR);

DESCRIPTION

	Open a BFD for read access on an existing stdio stream.  When
	the BFD is passed to <<bfd_close>>, the stream will be closed.
*/

bfd *
bfd_openstreamr (filename, target, streamarg)
     const char *filename;
     const char *target;
     PTR streamarg;
{
  FILE *stream = (FILE *) streamarg;
  bfd *nbfd;
  const bfd_target *target_vec;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      bfd_set_error (bfd_error_invalid_target);
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }

  nbfd->iostream = (PTR) stream;
  nbfd->filename = filename;
  nbfd->direction = read_direction;
				
  if (! bfd_cache_init (nbfd))
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }

  return nbfd;
}

/** bfd_openw -- open for writing.
  Returns a pointer to a freshly-allocated BFD on success, or NULL.

  See comment by bfd_fdopenr before you try to modify this function. */

/*
FUNCTION
	bfd_openw

SYNOPSIS
	bfd *bfd_openw(CONST char *filename, CONST char *target);

DESCRIPTION
	Create a BFD, associated with file @var{filename}, using the
	file format @var{target}, and return a pointer to it.

	Possible errors are <<bfd_error_system_call>>, <<bfd_error_no_memory>>,
	<<bfd_error_invalid_target>>.
*/

bfd *
bfd_openw (filename, target)
     CONST char *filename;
     CONST char *target;
{
  bfd *nbfd;
  const bfd_target *target_vec;

  bfd_set_error (bfd_error_system_call);

  /* nbfd has to point to head of malloc'ed block so that bfd_close may
     reclaim it correctly. */

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
    }

  nbfd->filename = filename;
  nbfd->direction = write_direction;

  if (bfd_open_file (nbfd) == NULL)
    {
      bfd_set_error (bfd_error_system_call);	/* File not writeable, etc */
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      return NULL;
  }

  return nbfd;
}

/*

FUNCTION
	bfd_close

SYNOPSIS
	boolean bfd_close(bfd *abfd);

DESCRIPTION

	Close a BFD. If the BFD was open for writing,
	then pending operations are completed and the file written out
	and closed. If the created file is executable, then
	<<chmod>> is called to mark it as such.

	All memory attached to the BFD is released.

	The file descriptor associated with the BFD is closed (even
	if it was passed in to BFD by <<bfd_fdopenr>>).

RETURNS
	<<true>> is returned if all is ok, otherwise <<false>>.
*/


boolean
bfd_close (abfd)
     bfd *abfd;
{
  boolean ret;

  if (!bfd_read_p (abfd))
    {
      if (! BFD_SEND_FMT (abfd, _bfd_write_contents, (abfd)))
	return false;
    }

  if (! BFD_SEND (abfd, _close_and_cleanup, (abfd)))
    return false;

  ret = bfd_cache_close (abfd);

  /* If the file was open for writing and is now executable,
     make it so */
  if (ret
      && abfd->direction == write_direction
      && abfd->flags & EXEC_P)
    {
      struct stat buf;

      if (stat (abfd->filename, &buf) == 0)
	{
 	  int mask = umask (0);
	  umask (mask);
	  chmod (abfd->filename,
		 (0777
		  & (buf.st_mode | ((S_IXUSR | S_IXGRP | S_IXOTH) &~ mask))));
	}
    }

  objalloc_free ((struct objalloc *) abfd->memory);
  free (abfd);

  return ret;
}

/*
FUNCTION
	bfd_close_all_done

SYNOPSIS
	boolean bfd_close_all_done(bfd *);

DESCRIPTION
	Close a BFD.  Differs from <<bfd_close>>
	since it does not complete any pending operations.  This
	routine would be used if the application had just used BFD for
	swapping and didn't want to use any of the writing code.

	If the created file is executable, then <<chmod>> is called
	to mark it as such.

	All memory attached to the BFD is released.

RETURNS
	<<true>> is returned if all is ok, otherwise <<false>>.

*/

boolean
bfd_close_all_done (abfd)
     bfd *abfd;
{
  boolean ret;

  ret = bfd_cache_close (abfd);

  /* If the file was open for writing and is now executable,
     make it so */
  if (ret
      && abfd->direction == write_direction
      && abfd->flags & EXEC_P)
    {
      struct stat buf;

      if (stat (abfd->filename, &buf) == 0)
	{
	  int mask = umask (0);
	  umask (mask);
	  chmod (abfd->filename,
		 (0777
		  & (buf.st_mode | ((S_IXUSR | S_IXGRP | S_IXOTH) &~ mask))));
	}
    }

  objalloc_free ((struct objalloc *) abfd->memory);
  free (abfd);

  return ret;
}

/*
FUNCTION
	bfd_create

SYNOPSIS
	bfd *bfd_create(CONST char *filename, bfd *templ);

DESCRIPTION
	Create a new BFD in the manner of
	<<bfd_openw>>, but without opening a file. The new BFD
	takes the target from the target used by @var{template}. The
	format is always set to <<bfd_object>>.

*/

bfd *
bfd_create (filename, templ)
     CONST char *filename;
     bfd *templ;
{
  bfd *nbfd;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;
  nbfd->filename = filename;
  if (templ)
    nbfd->xvec = templ->xvec;
  nbfd->direction = no_direction;
  bfd_set_format (nbfd, bfd_object);
  return nbfd;
}

/*
FUNCTION
	bfd_make_writable

SYNOPSIS
	boolean bfd_make_writable(bfd *abfd);

DESCRIPTION
	Takes a BFD as created by <<bfd_create>> and converts it
	into one like as returned by <<bfd_openw>>.  It does this
	by converting the BFD to BFD_IN_MEMORY.  It's assumed that
	you will call <<bfd_make_readable>> on this bfd later.

RETURNS
	<<true>> is returned if all is ok, otherwise <<false>>.
*/

boolean
bfd_make_writable(abfd)
     bfd *abfd;
{
  struct bfd_in_memory *bim;

  if (abfd->direction != no_direction)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  bim = (struct bfd_in_memory *) bfd_malloc (sizeof (struct bfd_in_memory));
  abfd->iostream = (PTR) bim;
  /* bfd_write will grow these as needed */
  bim->size = 0;
  bim->buffer = 0;

  abfd->flags |= BFD_IN_MEMORY;
  abfd->direction = write_direction;
  abfd->where = 0;

  return true;
}

/*
FUNCTION
	bfd_make_readable

SYNOPSIS
	boolean bfd_make_readable(bfd *abfd);

DESCRIPTION
	Takes a BFD as created by <<bfd_create>> and
	<<bfd_make_writable>> and converts it into one like as
	returned by <<bfd_openr>>.  It does this by writing the
	contents out to the memory buffer, then reversing the
	direction.

RETURNS
	<<true>> is returned if all is ok, otherwise <<false>>.  */

boolean
bfd_make_readable(abfd)
     bfd *abfd;
{
  if (abfd->direction != write_direction || !(abfd->flags & BFD_IN_MEMORY))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return false;
    }

  if (! BFD_SEND_FMT (abfd, _bfd_write_contents, (abfd)))
    return false;

  if (! BFD_SEND (abfd, _close_and_cleanup, (abfd)))
    return false;


  abfd->arch_info = &bfd_default_arch_struct;

  abfd->where = 0;
  abfd->sections = (asection *) NULL;
  abfd->format = bfd_unknown;
  abfd->my_archive = (bfd *) NULL;
  abfd->origin = 0;				
  abfd->opened_once = false;
  abfd->output_has_begun = false;
  abfd->section_count = 0;
  abfd->usrdata = (PTR) NULL;
  abfd->cacheable = false;
  abfd->flags = BFD_IN_MEMORY;
  abfd->mtime_set = false;

  abfd->target_defaulted = true;
  abfd->direction = read_direction;
  abfd->sections = 0;
  abfd->symcount = 0;
  abfd->outsymbols = 0;
  abfd->tdata.any = 0;

  bfd_check_format(abfd, bfd_object);

  return true;
}

/*
INTERNAL_FUNCTION
	bfd_alloc

SYNOPSIS
	PTR bfd_alloc (bfd *abfd, size_t wanted);

DESCRIPTION
	Allocate a block of @var{wanted} bytes of memory attached to
	<<abfd>> and return a pointer to it.
*/


PTR
bfd_alloc (abfd, size)
     bfd *abfd;
     size_t size;
{
  PTR ret;

  ret = objalloc_alloc (abfd->memory, (unsigned long) size);
  if (ret == NULL)
    bfd_set_error (bfd_error_no_memory);
  return ret;
}

PTR
bfd_zalloc (abfd, size)
     bfd *abfd;
     size_t size;
{
  PTR res;

  res = bfd_alloc (abfd, size);
  if (res)
    memset (res, 0, size);
  return res;
}

/* Free a block allocated for a BFD.  */

void
bfd_release (abfd, block)
     bfd *abfd;
     PTR block;
{
  objalloc_free_block ((struct objalloc *) abfd->memory, block);
}
