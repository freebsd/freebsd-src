/* Assorted BFD support routines, only used internally.
   Copyright 1990, 91, 92, 93, 94 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

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


#ifndef bfd_zmalloc
/* allocate and clear storage */

char *
bfd_zmalloc (size)
     bfd_size_type size;
{
  char *ptr = (char *) malloc ((size_t) size);

  if (ptr && size)
   memset(ptr, 0, (size_t) size);

  return ptr;
}
#endif /* bfd_zmalloc */

/* Some IO code */


/* Note that archive entries don't have streams; they share their parent's.
   This allows someone to play with the iostream behind BFD's back.

   Also, note that the origin pointer points to the beginning of a file's
   contents (0 for non-archive elements).  For archive entries this is the
   first octet in the file, NOT the beginning of the archive header. */

static 
int
real_read (where, a,b, file)
     PTR where;
     int a;
     int b;
     FILE *file;
{
  return fread(where, a,b,file);
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
  nread = real_read (ptr, 1, (int)(size*nitems), bfd_cache_lookup(abfd));
#ifdef FILE_OFFSET_IS_CHAR_INDEX
  if (nread > 0)
    abfd->where += nread;
#endif

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

bfd_size_type
bfd_write (ptr, size, nitems, abfd)
     CONST PTR ptr;
     bfd_size_type size;
     bfd_size_type nitems;
     bfd *abfd;
{
  int nwrote = fwrite (ptr, 1, (int) (size * nitems), bfd_cache_lookup (abfd));
#ifdef FILE_OFFSET_IS_CHAR_INDEX
  if (nwrote > 0)
    abfd->where += nwrote;
#endif
  if (nwrote != size * nitems)
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
  return fflush (bfd_cache_lookup(abfd));
}

int
bfd_stat (abfd, statbuf)
     bfd *abfd;
     struct stat *statbuf;
{
  return fstat (fileno(bfd_cache_lookup(abfd)), statbuf);
}

/* Returns 0 for success, nonzero for failure (in which case bfd_get_error
   can retrieve the error code).  */

int
bfd_seek (abfd, position, direction)
     bfd * CONST abfd;
     CONST file_ptr position;
     CONST int direction;
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
#ifdef FILE_OFFSET_IS_CHAR_INDEX
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
#endif

  f = bfd_cache_lookup (abfd);
  file_position = position;
  if (direction == SEEK_SET && abfd->my_archive != NULL)
    file_position += abfd->origin;

  result = fseek (f, file_position, direction);

  if (result != 0)
    {
      /* Force redetermination of `where' field.  */
      bfd_tell (abfd);
      bfd_set_error (bfd_error_system_call);
    }
  else
    {
#ifdef FILE_OFFSET_IS_CHAR_INDEX
      /* Adjust `where' field.  */
      if (direction == SEEK_SET)
	abfd->where = position;
      else
	abfd->where += position;
#endif
    }
  return result;
}

/** Make a string table */

/*>bfd.h<
 Add string to table pointed to by table, at location starting with free_ptr.
   resizes the table if necessary (if it's NULL, creates it, ignoring
   table_length).  Updates free_ptr, table, table_length */

boolean
bfd_add_to_string_table (table, new_string, table_length, free_ptr)
     char **table;
     char *new_string;
     unsigned int *table_length;
     char **free_ptr;
{
  size_t string_length = strlen (new_string) + 1; /* include null here */
  char *base = *table;
  size_t space_length = *table_length;
  unsigned int offset = (base ? *free_ptr - base : 0);

  if (base == NULL) {
    /* Avoid a useless regrow if we can (but of course we still
       take it next time).  */
    space_length = (string_length < DEFAULT_STRING_SPACE_SIZE ?
                    DEFAULT_STRING_SPACE_SIZE : string_length+1);
    base = bfd_zmalloc ((bfd_size_type) space_length);

    if (base == NULL) {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  }

  if ((size_t)(offset + string_length) >= space_length) {
    /* Make sure we will have enough space */
    while ((size_t)(offset + string_length) >= space_length) 
      space_length += space_length/2; /* grow by 50% */

    base = (char *) realloc (base, space_length);
    if (base == NULL) {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }

  }

  memcpy (base + offset, new_string, string_length);
  *table = base;
  *table_length = space_length;
  *free_ptr = base + offset + string_length;
  
  return true;
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
bfd_generic_is_local_label (abfd, sym)
     bfd *abfd;
     asymbol *sym;
{
  char locals_prefix = (bfd_get_symbol_leading_char (abfd) == '_') ? 'L' : '.';

  return (sym->name[0] == locals_prefix);
}

