/* Assorted BFD support routines, only used internally.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 1998
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
#include "libbfd.h"

#ifndef HAVE_GETPAGESIZE
#define getpagesize() 2048
#endif

static int real_read PARAMS ((PTR, size_t, size_t, FILE *));

/*
SECTION
	Internal functions

DESCRIPTION
	These routines are used within BFD.
	They are not intended for export, but are documented here for
	completeness.
*/

/* A routine which is used in target vectors for unsupported
   operations.  */

/*ARGSUSED*/
boolean
bfd_false (ignore)
     bfd *ignore;
{
  bfd_set_error (bfd_error_invalid_operation);
  return false;
}

/* A routine which is used in target vectors for supported operations
   which do not actually do anything.  */

/*ARGSUSED*/
boolean
bfd_true (ignore)
     bfd *ignore;
{
  return true;
}

/* A routine which is used in target vectors for unsupported
   operations which return a pointer value.  */

/*ARGSUSED*/
PTR
bfd_nullvoidptr (ignore)
     bfd *ignore;
{
  bfd_set_error (bfd_error_invalid_operation);
  return NULL;
}

/*ARGSUSED*/
int 
bfd_0 (ignore)
     bfd *ignore;
{
  return 0;
}

/*ARGSUSED*/
unsigned int 
bfd_0u (ignore)
     bfd *ignore;
{
   return 0;
}

/*ARGUSED*/
long
bfd_0l (ignore)
     bfd *ignore;
{
  return 0;
}

/* A routine which is used in target vectors for unsupported
   operations which return -1 on error.  */

/*ARGSUSED*/
long
_bfd_n1 (ignore_abfd)
     bfd *ignore_abfd;
{
  bfd_set_error (bfd_error_invalid_operation);
  return -1;
}

/*ARGSUSED*/
void 
bfd_void (ignore)
     bfd *ignore;
{
}

/*ARGSUSED*/
boolean
_bfd_nocore_core_file_matches_executable_p (ignore_core_bfd, ignore_exec_bfd)
     bfd *ignore_core_bfd;
     bfd *ignore_exec_bfd;
{
  bfd_set_error (bfd_error_invalid_operation);
  return false;
}

/* Routine to handle core_file_failing_command entry point for targets
   without core file support.  */

/*ARGSUSED*/
char *
_bfd_nocore_core_file_failing_command (ignore_abfd)
     bfd *ignore_abfd;
{
  bfd_set_error (bfd_error_invalid_operation);
  return (char *)NULL;
}

/* Routine to handle core_file_failing_signal entry point for targets
   without core file support.  */

/*ARGSUSED*/
int
_bfd_nocore_core_file_failing_signal (ignore_abfd)
     bfd *ignore_abfd;
{
  bfd_set_error (bfd_error_invalid_operation);
  return 0;
}

/*ARGSUSED*/
const bfd_target *
_bfd_dummy_target (ignore_abfd)
     bfd *ignore_abfd;
{
  bfd_set_error (bfd_error_wrong_format);
  return 0;
}

/* Allocate memory using malloc.  */

PTR
bfd_malloc (size)
     size_t size;
{
  PTR ptr;

  ptr = (PTR) malloc (size);
  if (ptr == NULL && size != 0)
    bfd_set_error (bfd_error_no_memory);
  return ptr;
}

/* Reallocate memory using realloc.  */

PTR
bfd_realloc (ptr, size)
     PTR ptr;
     size_t size;
{
  PTR ret;

  if (ptr == NULL)
    ret = malloc (size);
  else
    ret = realloc (ptr, size);

  if (ret == NULL)
    bfd_set_error (bfd_error_no_memory);

  return ret;
}

/* Allocate memory using malloc and clear it.  */

PTR
bfd_zmalloc (size)
     size_t size;
{
  PTR ptr;

  ptr = (PTR) malloc (size);

  if (size != 0)
    {
      if (ptr == NULL)
	bfd_set_error (bfd_error_no_memory);
      else
	memset (ptr, 0, size);
    }

  return ptr;
}

/* Some IO code */


/* Note that archive entries don't have streams; they share their parent's.
   This allows someone to play with the iostream behind BFD's back.

   Also, note that the origin pointer points to the beginning of a file's
   contents (0 for non-archive elements).  For archive entries this is the
   first octet in the file, NOT the beginning of the archive header. */

static int
real_read (where, a,b, file)
     PTR where;
     size_t a;
     size_t b;
     FILE *file;
{
  return fread (where, a, b, file);
}

/* Return value is amount read (FIXME: how are errors and end of file dealt
   with?  We never call bfd_set_error, which is probably a mistake).  */

bfd_size_type
bfd_read (ptr, size, nitems, abfd)
     PTR ptr;
     bfd_size_type size;
     bfd_size_type nitems;
     bfd *abfd;
{
  int nread;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      struct bfd_in_memory *bim;
      bfd_size_type get;

      bim = (struct bfd_in_memory *) abfd->iostream;
      get = size * nitems;
      if (abfd->where + get > bim->size)
	{
	  get = bim->size - abfd->where;
	  bfd_set_error (bfd_error_file_truncated);
	}
      memcpy (ptr, bim->buffer + abfd->where, get);
      abfd->where += get;
      return get;
    }

  nread = real_read (ptr, 1, (size_t)(size*nitems), bfd_cache_lookup(abfd));
  if (nread > 0)
    abfd->where += nread;

  /* Set bfd_error if we did not read as much data as we expected.

     If the read failed due to an error set the bfd_error_system_call,
     else set bfd_error_file_truncated.

     A BFD backend may wish to override bfd_error_file_truncated to
     provide something more useful (eg. no_symbols or wrong_format).  */
  if (nread < (int)(size * nitems))
    {
      if (ferror (bfd_cache_lookup (abfd)))
	bfd_set_error (bfd_error_system_call);
      else
	bfd_set_error (bfd_error_file_truncated);
    }

  return nread;
}

/* The window support stuff should probably be broken out into
   another file....  */
/* The idea behind the next and refcount fields is that one mapped
   region can suffice for multiple read-only windows or multiple
   non-overlapping read-write windows.  It's not implemented yet
   though.  */
struct _bfd_window_internal {
  struct _bfd_window_internal *next;
  PTR data;
  bfd_size_type size;
  int refcount : 31;		/* should be enough... */
  unsigned mapped : 1;		/* 1 = mmap, 0 = malloc */
};

void
bfd_init_window (windowp)
     bfd_window *windowp;
{
  windowp->data = 0;
  windowp->i = 0;
  windowp->size = 0;
}

/* Currently, if USE_MMAP is undefined, none if the window stuff is
   used.  Okay, so it's mis-named.  At least the command-line option
   "--without-mmap" is more obvious than "--without-windows" or some
   such.  */
#ifdef USE_MMAP

#undef HAVE_MPROTECT /* code's not tested yet */

#if HAVE_MMAP || HAVE_MPROTECT || HAVE_MADVISE
#include <sys/types.h>
#include <sys/mman.h>
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

static int debug_windows;

void
bfd_free_window (windowp)
     bfd_window *windowp;
{
  bfd_window_internal *i = windowp->i;
  windowp->i = 0;
  windowp->data = 0;
  if (i == 0)
    return;
  i->refcount--;
  if (debug_windows)
    fprintf (stderr, "freeing window @%p<%p,%lx,%p>\n",
	     windowp, windowp->data, windowp->size, windowp->i);
  if (i->refcount != 0)
    return;

  if (i->mapped)
    {
#ifdef HAVE_MMAP
      munmap (i->data, i->size);
      goto no_free;
#else
      abort ();
#endif
    }
#ifdef HAVE_MPROTECT
  mprotect (i->data, i->size, PROT_READ | PROT_WRITE);
#endif
  free (i->data);
#ifdef HAVE_MMAP
 no_free:
#endif
  i->data = 0;
  /* There should be no more references to i at this point.  */
  free (i);
}

static int ok_to_map = 1;

boolean
bfd_get_file_window (abfd, offset, size, windowp, writable)
     bfd *abfd;
     file_ptr offset;
     bfd_size_type size;
     bfd_window *windowp;
     boolean writable;
{
  static size_t pagesize;
  bfd_window_internal *i = windowp->i;
  size_t size_to_alloc = size;

  if (debug_windows)
    fprintf (stderr, "bfd_get_file_window (%p, %6ld, %6ld, %p<%p,%lx,%p>, %d)",
	     abfd, (long) offset, (long) size,
	     windowp, windowp->data, (unsigned long) windowp->size,
	     windowp->i, writable);

  /* Make sure we know the page size, so we can be friendly to mmap.  */
  if (pagesize == 0)
    pagesize = getpagesize ();
  if (pagesize == 0)
    abort ();

  if (i == 0)
    {
      windowp->i = i = (bfd_window_internal *) bfd_zmalloc (sizeof (bfd_window_internal));
      if (i == 0)
	return false;
      i->data = 0;
    }
#ifdef HAVE_MMAP
  if (ok_to_map
      && (i->data == 0 || i->mapped == 1)
      && (abfd->flags & BFD_IN_MEMORY) == 0)
    {
      file_ptr file_offset, offset2;
      size_t real_size;
      int fd;
      FILE *f;

      /* Find the real file and the real offset into it.  */
      while (abfd->my_archive != NULL)
	{
	  offset += abfd->origin;
	  abfd = abfd->my_archive;
	}
      f = bfd_cache_lookup (abfd);
      fd = fileno (f);

      /* Compute offsets and size for mmap and for the user's data.  */
      offset2 = offset % pagesize;
      if (offset2 < 0)
	abort ();
      file_offset = offset - offset2;
      real_size = offset + size - file_offset;
      real_size = real_size + pagesize - 1;
      real_size -= real_size % pagesize;

      /* If we're re-using a memory region, make sure it's big enough.  */
      if (i->data && i->size < size)
	{
	  munmap (i->data, i->size);
	  i->data = 0;
	}
      i->data = mmap (i->data, real_size,
		      writable ? PROT_WRITE | PROT_READ : PROT_READ,
		      (writable
		       ? MAP_FILE | MAP_PRIVATE
		       : MAP_FILE | MAP_SHARED),
		      fd, file_offset);
      if (i->data == (PTR) -1)
	{
	  /* An error happened.  Report it, or try using malloc, or
	     something.  */
	  bfd_set_error (bfd_error_system_call);
	  i->data = 0;
	  windowp->data = 0;
	  if (debug_windows)
	    fprintf (stderr, "\t\tmmap failed!\n");
	  return false;
	}
      if (debug_windows)
	fprintf (stderr, "\n\tmapped %ld at %p, offset is %ld\n",
		 (long) real_size, i->data, (long) offset2);
      i->size = real_size;
      windowp->data = (PTR) ((bfd_byte *) i->data + offset2);
      windowp->size = size;
      i->mapped = 1;
      return true;
    }
  else if (debug_windows)
    {
      if (ok_to_map)
	fprintf (stderr, "not mapping: data=%lx mapped=%d\n",
		 (unsigned long) i->data, (int) i->mapped);
      else
	fprintf (stderr, "not mapping: env var not set\n");
    }
#else
  ok_to_map = 0;
#endif

#ifdef HAVE_MPROTECT
  if (!writable)
    {
      size_to_alloc += pagesize - 1;
      size_to_alloc -= size_to_alloc % pagesize;
    }
#endif
  if (debug_windows)
    fprintf (stderr, "\n\t%s(%6ld)",
	     i->data ? "realloc" : " malloc", (long) size_to_alloc);
  i->data = (PTR) bfd_realloc (i->data, size_to_alloc);
  if (debug_windows)
    fprintf (stderr, "\t-> %p\n", i->data);
  i->refcount = 1;
  if (i->data == NULL)
    {
      if (size_to_alloc == 0)
	return true;
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  if (bfd_seek (abfd, offset, SEEK_SET) != 0)
    return false;
  i->size = bfd_read (i->data, size, 1, abfd);
  if (i->size != size)
    return false;
  i->mapped = 0;
#ifdef HAVE_MPROTECT
  if (!writable)
    {
      if (debug_windows)
	fprintf (stderr, "\tmprotect (%p, %ld, PROT_READ)\n", i->data,
		 (long) i->size);
      mprotect (i->data, i->size, PROT_READ);
    }
#endif
  windowp->data = i->data;
  windowp->size = i->size;
  return true;
}

#endif /* USE_MMAP */

bfd_size_type
bfd_write (ptr, size, nitems, abfd)
     CONST PTR ptr;
     bfd_size_type size;
     bfd_size_type nitems;
     bfd *abfd;
{
  long nwrote;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    abort ();

  nwrote = fwrite (ptr, 1, (size_t) (size * nitems),
		   bfd_cache_lookup (abfd));
  if (nwrote > 0)
    abfd->where += nwrote;
  if ((bfd_size_type) nwrote != size * nitems)
    {
#ifdef ENOSPC
      if (nwrote >= 0)
	errno = ENOSPC;
#endif
      bfd_set_error (bfd_error_system_call);
    }
  return nwrote;
}

/*
INTERNAL_FUNCTION
	bfd_write_bigendian_4byte_int

SYNOPSIS
	void bfd_write_bigendian_4byte_int(bfd *abfd,  int i);

DESCRIPTION
	Write a 4 byte integer @var{i} to the output BFD @var{abfd}, in big
	endian order regardless of what else is going on.  This is useful in
	archives.

*/
void
bfd_write_bigendian_4byte_int (abfd, i)
     bfd *abfd;
     int i;
{
  bfd_byte buffer[4];
  bfd_putb32(i, buffer);
  if (bfd_write((PTR)buffer, 4, 1, abfd) != 4)
    abort ();
}

long
bfd_tell (abfd)
     bfd *abfd;
{
  file_ptr ptr;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    return abfd->where;

  ptr = ftell (bfd_cache_lookup(abfd));

  if (abfd->my_archive)
    ptr -= abfd->origin;
  abfd->where = ptr;
  return ptr;
}

int
bfd_flush (abfd)
     bfd *abfd;
{
  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    return 0;
  return fflush (bfd_cache_lookup(abfd));
}

/* Returns 0 for success, negative value for failure (in which case
   bfd_get_error can retrieve the error code).  */
int
bfd_stat (abfd, statbuf)
     bfd *abfd;
     struct stat *statbuf;
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
bfd_seek (abfd, position, direction)
     bfd *abfd;
     file_ptr position;
     int direction;
{
  int result;
  FILE *f;
  file_ptr file_position;
  /* For the time being, a BFD may not seek to it's end.  The problem
     is that we don't easily have a way to recognize the end of an
     element in an archive. */

  BFD_ASSERT (direction == SEEK_SET || direction == SEEK_CUR);

  if (direction == SEEK_CUR && position == 0)
    return 0;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      if (direction == SEEK_SET)
	abfd->where = position;
      else
	abfd->where += position;
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
      where_am_i_now = ftell (bfd_cache_lookup (abfd));
      if (abfd->my_archive)
	where_am_i_now -= abfd->origin;
      if (where_am_i_now != abfd->where)
	abort ();
#endif
      if (direction == SEEK_SET && position == abfd->where)
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

  result = fseek (f, file_position, direction);

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

/** The do-it-yourself (byte) sex-change kit */

/* The middle letter e.g. get<b>short indicates Big or Little endian
   target machine.  It doesn't matter what the byte order of the host
   machine is; these routines work for either.  */

/* FIXME: Should these take a count argument?
   Answer (gnu@cygnus.com):  No, but perhaps they should be inline
                             functions in swap.h #ifdef __GNUC__. 
                             Gprof them later and find out.  */

/*
FUNCTION
	bfd_put_size
FUNCTION
	bfd_get_size

DESCRIPTION
	These macros as used for reading and writing raw data in
	sections; each access (except for bytes) is vectored through
	the target format of the BFD and mangled accordingly. The
	mangling performs any necessary endian translations and
	removes alignment restrictions.  Note that types accepted and
	returned by these macros are identical so they can be swapped
	around in macros---for example, @file{libaout.h} defines <<GET_WORD>>
	to either <<bfd_get_32>> or <<bfd_get_64>>.

	In the put routines, @var{val} must be a <<bfd_vma>>.  If we are on a
	system without prototypes, the caller is responsible for making
	sure that is true, with a cast if necessary.  We don't cast
	them in the macro definitions because that would prevent <<lint>>
	or <<gcc -Wall>> from detecting sins such as passing a pointer.
	To detect calling these with less than a <<bfd_vma>>, use
	<<gcc -Wconversion>> on a host with 64 bit <<bfd_vma>>'s.

.
.{* Byte swapping macros for user section data.  *}
.
.#define bfd_put_8(abfd, val, ptr) \
.                (*((unsigned char *)(ptr)) = (unsigned char)(val))
.#define bfd_put_signed_8 \
.		bfd_put_8
.#define bfd_get_8(abfd, ptr) \
.                (*(unsigned char *)(ptr))
.#define bfd_get_signed_8(abfd, ptr) \
.		((*(unsigned char *)(ptr) ^ 0x80) - 0x80)
.
.#define bfd_put_16(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_putx16, ((val),(ptr)))
.#define bfd_put_signed_16 \
.		 bfd_put_16
.#define bfd_get_16(abfd, ptr) \
.                BFD_SEND(abfd, bfd_getx16, (ptr))
.#define bfd_get_signed_16(abfd, ptr) \
.         	 BFD_SEND (abfd, bfd_getx_signed_16, (ptr))
.
.#define bfd_put_32(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_putx32, ((val),(ptr)))
.#define bfd_put_signed_32 \
.		 bfd_put_32
.#define bfd_get_32(abfd, ptr) \
.                BFD_SEND(abfd, bfd_getx32, (ptr))
.#define bfd_get_signed_32(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_getx_signed_32, (ptr))
.
.#define bfd_put_64(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_putx64, ((val), (ptr)))
.#define bfd_put_signed_64 \
.		 bfd_put_64
.#define bfd_get_64(abfd, ptr) \
.                BFD_SEND(abfd, bfd_getx64, (ptr))
.#define bfd_get_signed_64(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_getx_signed_64, (ptr))
.
*/ 

/*
FUNCTION
	bfd_h_put_size
	bfd_h_get_size

DESCRIPTION
	These macros have the same function as their <<bfd_get_x>>
	bretheren, except that they are used for removing information
	for the header records of object files. Believe it or not,
	some object files keep their header records in big endian
	order and their data in little endian order.
.
.{* Byte swapping macros for file header data.  *}
.
.#define bfd_h_put_8(abfd, val, ptr) \
.		bfd_put_8 (abfd, val, ptr)
.#define bfd_h_put_signed_8(abfd, val, ptr) \
.		bfd_put_8 (abfd, val, ptr)
.#define bfd_h_get_8(abfd, ptr) \
.		bfd_get_8 (abfd, ptr)
.#define bfd_h_get_signed_8(abfd, ptr) \
.		bfd_get_signed_8 (abfd, ptr)
.
.#define bfd_h_put_16(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_h_putx16,(val,ptr))
.#define bfd_h_put_signed_16 \
.		 bfd_h_put_16
.#define bfd_h_get_16(abfd, ptr) \
.                BFD_SEND(abfd, bfd_h_getx16,(ptr))
.#define bfd_h_get_signed_16(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_h_getx_signed_16, (ptr))
.
.#define bfd_h_put_32(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_h_putx32,(val,ptr))
.#define bfd_h_put_signed_32 \
.		 bfd_h_put_32
.#define bfd_h_get_32(abfd, ptr) \
.                BFD_SEND(abfd, bfd_h_getx32,(ptr))
.#define bfd_h_get_signed_32(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_h_getx_signed_32, (ptr))
.
.#define bfd_h_put_64(abfd, val, ptr) \
.                BFD_SEND(abfd, bfd_h_putx64,(val, ptr))
.#define bfd_h_put_signed_64 \
.		 bfd_h_put_64
.#define bfd_h_get_64(abfd, ptr) \
.                BFD_SEND(abfd, bfd_h_getx64,(ptr))
.#define bfd_h_get_signed_64(abfd, ptr) \
.		 BFD_SEND(abfd, bfd_h_getx_signed_64, (ptr))
.
*/ 

/* Sign extension to bfd_signed_vma.  */
#define COERCE16(x) (((bfd_signed_vma) (x) ^ 0x8000) - 0x8000)
#define COERCE32(x) (((bfd_signed_vma) (x) ^ 0x80000000) - 0x80000000)
#define EIGHT_GAZILLION (((BFD_HOST_64_BIT)0x80000000) << 32)
#define COERCE64(x) \
  (((bfd_signed_vma) (x) ^ EIGHT_GAZILLION) - EIGHT_GAZILLION)

bfd_vma
bfd_getb16 (addr)
     register const bfd_byte *addr;
{
  return (addr[0] << 8) | addr[1];
}

bfd_vma
bfd_getl16 (addr)
     register const bfd_byte *addr;
{
  return (addr[1] << 8) | addr[0];
}

bfd_signed_vma
bfd_getb_signed_16 (addr)
     register const bfd_byte *addr;
{
  return COERCE16((addr[0] << 8) | addr[1]);
}

bfd_signed_vma
bfd_getl_signed_16 (addr)
     register const bfd_byte *addr;
{
  return COERCE16((addr[1] << 8) | addr[0]);
}

void
bfd_putb16 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
  addr[0] = (bfd_byte)(data >> 8);
  addr[1] = (bfd_byte )data;
}

void
bfd_putl16 (data, addr)
     bfd_vma data;             
     register bfd_byte *addr;
{
  addr[0] = (bfd_byte )data;
  addr[1] = (bfd_byte)(data >> 8);
}

bfd_vma
bfd_getb32 (addr)
     register const bfd_byte *addr;
{
  return (((((bfd_vma)addr[0] << 8) | addr[1]) << 8)
	  | addr[2]) << 8 | addr[3];
}

bfd_vma
bfd_getl32 (addr)
     register const bfd_byte *addr;
{
  return (((((bfd_vma)addr[3] << 8) | addr[2]) << 8)
	  | addr[1]) << 8 | addr[0];
}

bfd_signed_vma
bfd_getb_signed_32 (addr)
     register const bfd_byte *addr;
{
  return COERCE32((((((bfd_vma)addr[0] << 8) | addr[1]) << 8)
		   | addr[2]) << 8 | addr[3]);
}

bfd_signed_vma
bfd_getl_signed_32 (addr)
     register const bfd_byte *addr;
{
  return COERCE32((((((bfd_vma)addr[3] << 8) | addr[2]) << 8)
		   | addr[1]) << 8 | addr[0]);
}

bfd_vma
bfd_getb64 (addr)
     register const bfd_byte *addr;
{
#ifdef BFD64
  bfd_vma low, high;

  high= ((((((((addr[0]) << 8) |
              addr[1]) << 8) |
            addr[2]) << 8) |
          addr[3]) );

  low = (((((((((bfd_vma)addr[4]) << 8) |
              addr[5]) << 8) |
            addr[6]) << 8) |
          addr[7]));

  return high << 32 | low;
#else
  BFD_FAIL();
  return 0;
#endif
}

bfd_vma
bfd_getl64 (addr)
     register const bfd_byte *addr;
{
#ifdef BFD64
  bfd_vma low, high;
  high= (((((((addr[7] << 8) |
              addr[6]) << 8) |
            addr[5]) << 8) |
          addr[4]));

  low = ((((((((bfd_vma)addr[3] << 8) |
              addr[2]) << 8) |
            addr[1]) << 8) |
          addr[0]) );

  return high << 32 | low;
#else
  BFD_FAIL();
  return 0;
#endif

}

bfd_signed_vma
bfd_getb_signed_64 (addr)
     register const bfd_byte *addr;
{
#ifdef BFD64
  bfd_vma low, high;

  high= ((((((((addr[0]) << 8) |
              addr[1]) << 8) |
            addr[2]) << 8) |
          addr[3]) );

  low = (((((((((bfd_vma)addr[4]) << 8) |
              addr[5]) << 8) |
            addr[6]) << 8) |
          addr[7]));

  return COERCE64(high << 32 | low);
#else
  BFD_FAIL();
  return 0;
#endif
}

bfd_signed_vma
bfd_getl_signed_64 (addr)
     register const bfd_byte *addr;
{
#ifdef BFD64
  bfd_vma low, high;
  high= (((((((addr[7] << 8) |
              addr[6]) << 8) |
            addr[5]) << 8) |
          addr[4]));

  low = ((((((((bfd_vma)addr[3] << 8) |
              addr[2]) << 8) |
            addr[1]) << 8) |
          addr[0]) );

  return COERCE64(high << 32 | low);
#else
  BFD_FAIL();
  return 0;
#endif
}

void
bfd_putb32 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
        addr[0] = (bfd_byte)(data >> 24);
        addr[1] = (bfd_byte)(data >> 16);
        addr[2] = (bfd_byte)(data >>  8);
        addr[3] = (bfd_byte)data;
}

void
bfd_putl32 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
        addr[0] = (bfd_byte)data;
        addr[1] = (bfd_byte)(data >>  8);
        addr[2] = (bfd_byte)(data >> 16);
        addr[3] = (bfd_byte)(data >> 24);
}

void
bfd_putb64 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
#ifdef BFD64
  addr[0] = (bfd_byte)(data >> (7*8));
  addr[1] = (bfd_byte)(data >> (6*8));
  addr[2] = (bfd_byte)(data >> (5*8));
  addr[3] = (bfd_byte)(data >> (4*8));
  addr[4] = (bfd_byte)(data >> (3*8));
  addr[5] = (bfd_byte)(data >> (2*8));
  addr[6] = (bfd_byte)(data >> (1*8));
  addr[7] = (bfd_byte)(data >> (0*8));
#else
  BFD_FAIL();
#endif
}

void
bfd_putl64 (data, addr)
     bfd_vma data;
     register bfd_byte *addr;
{
#ifdef BFD64
  addr[7] = (bfd_byte)(data >> (7*8));
  addr[6] = (bfd_byte)(data >> (6*8));
  addr[5] = (bfd_byte)(data >> (5*8));
  addr[4] = (bfd_byte)(data >> (4*8));
  addr[3] = (bfd_byte)(data >> (3*8));
  addr[2] = (bfd_byte)(data >> (2*8));
  addr[1] = (bfd_byte)(data >> (1*8));
  addr[0] = (bfd_byte)(data >> (0*8));
#else
  BFD_FAIL();
#endif
}

/* Default implementation */

boolean
_bfd_generic_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
    if (count == 0)
        return true;
    if ((bfd_size_type)(offset+count) > section->_raw_size
        || bfd_seek(abfd, (file_ptr)(section->filepos + offset), SEEK_SET) == -1
        || bfd_read(location, (bfd_size_type)1, count, abfd) != count)
        return (false); /* on error */
    return (true);
}

boolean
_bfd_generic_get_section_contents_in_window (abfd, section, w, offset, count)
     bfd *abfd;
     sec_ptr section;
     bfd_window *w;
     file_ptr offset;
     bfd_size_type count;
{
#ifdef USE_MMAP
  if (count == 0)
    return true;
  if (abfd->xvec->_bfd_get_section_contents != _bfd_generic_get_section_contents)
    {
      /* We don't know what changes the bfd's get_section_contents
	 method may have to make.  So punt trying to map the file
	 window, and let get_section_contents do its thing.  */
      /* @@ FIXME : If the internal window has a refcount of 1 and was
	 allocated with malloc instead of mmap, just reuse it.  */
      bfd_free_window (w);
      w->i = (bfd_window_internal *) bfd_zmalloc (sizeof (bfd_window_internal));
      if (w->i == NULL)
	return false;
      w->i->data = (PTR) bfd_malloc ((size_t) count);
      if (w->i->data == NULL)
	{
	  free (w->i);
	  w->i = NULL;
	  return false;
	}
      w->i->mapped = 0;
      w->i->refcount = 1;
      w->size = w->i->size = count;
      w->data = w->i->data;
      return bfd_get_section_contents (abfd, section, w->data, offset, count);
    }
  if ((bfd_size_type) (offset+count) > section->_raw_size
      || (bfd_get_file_window (abfd, section->filepos + offset, count, w, true)
	  == false))
    return false;
  return true;
#else
  abort ();
#endif
}

/* This generic function can only be used in implementations where creating
   NEW sections is disallowed.  It is useful in patching existing sections
   in read-write files, though.  See other set_section_contents functions
   to see why it doesn't work for new sections.  */
boolean
_bfd_generic_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (count == 0)
    return true;

  if (bfd_seek (abfd, (file_ptr) (section->filepos + offset), SEEK_SET) == -1
      || bfd_write (location, (bfd_size_type) 1, count, abfd) != count)
    return false;

  return true;
}

/*
INTERNAL_FUNCTION
	bfd_log2

SYNOPSIS
	unsigned int bfd_log2(bfd_vma x);

DESCRIPTION
	Return the log base 2 of the value supplied, rounded up.  E.g., an
	@var{x} of 1025 returns 11.
*/

unsigned
bfd_log2(x)
     bfd_vma x;
{
  unsigned result = 0;
  while ( (bfd_vma)(1<< result) < x)
    result++;
  return result;
}

boolean
bfd_generic_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  char locals_prefix = (bfd_get_symbol_leading_char (abfd) == '_') ? 'L' : '.';

  return (name[0] == locals_prefix);
}

