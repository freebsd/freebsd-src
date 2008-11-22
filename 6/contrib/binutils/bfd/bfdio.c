/* Low-level I/O routines for BFDs.

   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

#include "sysdep.h"

#include "bfd.h"
#include "libbfd.h"

#include <limits.h>

#ifndef S_IXUSR
#define S_IXUSR 0100    /* Execute by owner.  */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010    /* Execute by group.  */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001    /* Execute by others.  */
#endif

file_ptr
real_ftell (FILE *file)
{
#if defined (HAVE_FTELLO64)
  return ftello64 (file);
#elif defined (HAVE_FTELLO)
  return ftello (file);
#else
  return ftell (file);
#endif
}

int
real_fseek (FILE *file, file_ptr offset, int whence)
{
#if defined (HAVE_FSEEKO64)
  return fseeko64 (file, offset, whence);
#elif defined (HAVE_FSEEKO)
  return fseeko (file, offset, whence);
#else
  return fseek (file, offset, whence);
#endif
}

/* Note that archive entries don't have streams; they share their parent's.
   This allows someone to play with the iostream behind BFD's back.

   Also, note that the origin pointer points to the beginning of a file's
   contents (0 for non-archive elements).  For archive entries this is the
   first octet in the file, NOT the beginning of the archive header.  */

static size_t
real_read (void *where, size_t a, size_t b, FILE *file)
{
  /* FIXME - this looks like an optimization, but it's really to cover
     up for a feature of some OSs (not solaris - sigh) that
     ld/pe-dll.c takes advantage of (apparently) when it creates BFDs
     internally and tries to link against them.  BFD seems to be smart
     enough to realize there are no symbol records in the "file" that
     doesn't exist but attempts to read them anyway.  On Solaris,
     attempting to read zero bytes from a NULL file results in a core
     dump, but on other platforms it just returns zero bytes read.
     This makes it to something reasonable. - DJ */
  if (a == 0 || b == 0)
    return 0;


#if defined (__VAX) && defined (VMS)
  /* Apparently fread on Vax VMS does not keep the record length
     information.  */
  return read (fileno (file), where, a * b);
#else
  return fread (where, a, b, file);
#endif
}

/* Return value is amount read.  */

bfd_size_type
bfd_bread (void *ptr, bfd_size_type size, bfd *abfd)
{
  size_t nread;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      struct bfd_in_memory *bim;
      bfd_size_type get;

      bim = abfd->iostream;
      get = size;
      if (abfd->where + get > bim->size)
	{
	  if (bim->size < (bfd_size_type) abfd->where)
	    get = 0;
	  else
	    get = bim->size - abfd->where;
	  bfd_set_error (bfd_error_file_truncated);
	}
      memcpy (ptr, bim->buffer + abfd->where, (size_t) get);
      abfd->where += get;
      return get;
    }

  nread = real_read (ptr, 1, (size_t) size, bfd_cache_lookup (abfd));
  if (nread != (size_t) -1)
    abfd->where += nread;

  /* Set bfd_error if we did not read as much data as we expected.

     If the read failed due to an error set the bfd_error_system_call,
     else set bfd_error_file_truncated.

     A BFD backend may wish to override bfd_error_file_truncated to
     provide something more useful (eg. no_symbols or wrong_format).  */
  if (nread != size)
    {
      if (ferror (bfd_cache_lookup (abfd)))
	bfd_set_error (bfd_error_system_call);
      else
	bfd_set_error (bfd_error_file_truncated);
    }

  return nread;
}

bfd_size_type
bfd_bwrite (const void *ptr, bfd_size_type size, bfd *abfd)
{
  size_t nwrote;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      struct bfd_in_memory *bim = abfd->iostream;
      size = (size_t) size;
      if (abfd->where + size > bim->size)
	{
	  bfd_size_type newsize, oldsize;

	  oldsize = (bim->size + 127) & ~(bfd_size_type) 127;
	  bim->size = abfd->where + size;
	  /* Round up to cut down on memory fragmentation */
	  newsize = (bim->size + 127) & ~(bfd_size_type) 127;
	  if (newsize > oldsize)
	    {
	      bim->buffer = bfd_realloc (bim->buffer, newsize);
	      if (bim->buffer == 0)
		{
		  bim->size = 0;
		  return 0;
		}
	    }
	}
      memcpy (bim->buffer + abfd->where, ptr, (size_t) size);
      abfd->where += size;
      return size;
    }

  nwrote = fwrite (ptr, 1, (size_t) size, bfd_cache_lookup (abfd));
  if (nwrote != (size_t) -1)
    abfd->where += nwrote;
  if (nwrote != size)
    {
#ifdef ENOSPC
      errno = ENOSPC;
#endif
      bfd_set_error (bfd_error_system_call);
    }
  return nwrote;
}

file_ptr
bfd_tell (bfd *abfd)
{
  file_ptr ptr;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    return abfd->where;

  ptr = real_ftell (bfd_cache_lookup (abfd));

  if (abfd->my_archive)
    ptr -= abfd->origin;
  abfd->where = ptr;
  return ptr;
}

int
bfd_flush (bfd *abfd)
{
  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    return 0;
  return fflush (bfd_cache_lookup(abfd));
}

/* Returns 0 for success, negative value for failure (in which case
   bfd_get_error can retrieve the error code).  */
int
bfd_stat (bfd *abfd, struct stat *statbuf)
{
  FILE *f;
  int result;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    abort ();

  f = bfd_cache_lookup (abfd);
  if (f == NULL)
    {
      bfd_set_error (bfd_error_system_call);
      return -1;
    }
  result = fstat (fileno (f), statbuf);
  if (result < 0)
    bfd_set_error (bfd_error_system_call);
  return result;
}

/* Returns 0 for success, nonzero for failure (in which case bfd_get_error
   can retrieve the error code).  */

int
bfd_seek (bfd *abfd, file_ptr position, int direction)
{
  int result;
  FILE *f;
  file_ptr file_position;
  /* For the time being, a BFD may not seek to it's end.  The problem
     is that we don't easily have a way to recognize the end of an
     element in an archive.  */

  BFD_ASSERT (direction == SEEK_SET || direction == SEEK_CUR);

  if (direction == SEEK_CUR && position == 0)
    return 0;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      struct bfd_in_memory *bim;

      bim = abfd->iostream;

      if (direction == SEEK_SET)
	abfd->where = position;
      else
	abfd->where += position;

      if (abfd->where > bim->size)
	{
	  if ((abfd->direction == write_direction) ||
	      (abfd->direction == both_direction))
	    {
	      bfd_size_type newsize, oldsize;
	      oldsize = (bim->size + 127) & ~(bfd_size_type) 127;
	      bim->size = abfd->where;
	      /* Round up to cut down on memory fragmentation */
	      newsize = (bim->size + 127) & ~(bfd_size_type) 127;
	      if (newsize > oldsize)
	        {
		  bim->buffer = bfd_realloc (bim->buffer, newsize);
		  if (bim->buffer == 0)
		    {
		      bim->size = 0;
		      return -1;
		    }
	        }
	    }
	  else
	    {
	      abfd->where = bim->size;
	      bfd_set_error (bfd_error_file_truncated);
	      return -1;
	    }
	}
      return 0;
    }

  if (abfd->format != bfd_archive && abfd->my_archive == 0)
    {
#if 0
      /* Explanation for this code: I'm only about 95+% sure that the above
	 conditions are sufficient and that all i/o calls are properly
	 adjusting the `where' field.  So this is sort of an `assert'
	 that the `where' field is correct.  If we can go a while without
	 tripping the abort, we can probably safely disable this code,
	 so that the real optimizations happen.  */
      file_ptr where_am_i_now;
      where_am_i_now = real_ftell (bfd_cache_lookup (abfd));
      if (abfd->my_archive)
	where_am_i_now -= abfd->origin;
      if (where_am_i_now != abfd->where)
	abort ();
#endif
      if (direction == SEEK_SET && (bfd_vma) position == abfd->where)
	return 0;
    }
  else
    {
      /* We need something smarter to optimize access to archives.
	 Currently, anything inside an archive is read via the file
	 handle for the archive.  Which means that a bfd_seek on one
	 component affects the `current position' in the archive, as
	 well as in any other component.

	 It might be sufficient to put a spike through the cache
	 abstraction, and look to the archive for the file position,
	 but I think we should try for something cleaner.

	 In the meantime, no optimization for archives.  */
    }

  f = bfd_cache_lookup (abfd);
  file_position = position;
  if (direction == SEEK_SET && abfd->my_archive != NULL)
    file_position += abfd->origin;

  result = real_fseek (f, file_position, direction);
  if (result != 0)
    {
      int hold_errno = errno;

      /* Force redetermination of `where' field.  */
      bfd_tell (abfd);

      /* An EINVAL error probably means that the file offset was
         absurd.  */
      if (hold_errno == EINVAL)
	bfd_set_error (bfd_error_file_truncated);
      else
	{
	  bfd_set_error (bfd_error_system_call);
	  errno = hold_errno;
	}
    }
  else
    {
      /* Adjust `where' field.  */
      if (direction == SEEK_SET)
	abfd->where = position;
      else
	abfd->where += position;
    }
  return result;
}

/*
FUNCTION
	bfd_get_mtime

SYNOPSIS
	long bfd_get_mtime (bfd *abfd);

DESCRIPTION
	Return the file modification time (as read from the file system, or
	from the archive header for archive members).

*/

long
bfd_get_mtime (bfd *abfd)
{
  FILE *fp;
  struct stat buf;

  if (abfd->mtime_set)
    return abfd->mtime;

  fp = bfd_cache_lookup (abfd);
  if (0 != fstat (fileno (fp), &buf))
    return 0;

  abfd->mtime = buf.st_mtime;		/* Save value in case anyone wants it */
  return buf.st_mtime;
}

/*
FUNCTION
	bfd_get_size

SYNOPSIS
	long bfd_get_size (bfd *abfd);

DESCRIPTION
	Return the file size (as read from file system) for the file
	associated with BFD @var{abfd}.

	The initial motivation for, and use of, this routine is not
	so we can get the exact size of the object the BFD applies to, since
	that might not be generally possible (archive members for example).
	It would be ideal if someone could eventually modify
	it so that such results were guaranteed.

	Instead, we want to ask questions like "is this NNN byte sized
	object I'm about to try read from file offset YYY reasonable?"
	As as example of where we might do this, some object formats
	use string tables for which the first <<sizeof (long)>> bytes of the
	table contain the size of the table itself, including the size bytes.
	If an application tries to read what it thinks is one of these
	string tables, without some way to validate the size, and for
	some reason the size is wrong (byte swapping error, wrong location
	for the string table, etc.), the only clue is likely to be a read
	error when it tries to read the table, or a "virtual memory
	exhausted" error when it tries to allocate 15 bazillon bytes
	of space for the 15 bazillon byte table it is about to read.
	This function at least allows us to answer the question, "is the
	size reasonable?".
*/

long
bfd_get_size (bfd *abfd)
{
  FILE *fp;
  struct stat buf;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    return ((struct bfd_in_memory *) abfd->iostream)->size;

  fp = bfd_cache_lookup (abfd);
  if (0 != fstat (fileno (fp), & buf))
    return 0;

  return buf.st_size;
}
