/* Assorted BFD support routines, only used internally.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
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
	libbfd

DESCRIPTION
	This file contains various routines which are used within BFD.
	They are not intended for export, but are documented here for
	completeness.
*/

boolean
DEFUN(_bfd_dummy_new_section_hook,(ignore, ignore_newsect),
      bfd *ignore AND
      asection *ignore_newsect)
{
  return true;
}

boolean
DEFUN(bfd_false ,(ignore),
      bfd *ignore)
{
  return false;
}

boolean
DEFUN(bfd_true,(ignore),
      bfd *ignore)
{
  return true;
}

PTR
DEFUN(bfd_nullvoidptr,(ignore),
      bfd *ignore)
{
  return (PTR)NULL;
}

int 
DEFUN(bfd_0,(ignore),
      bfd *ignore)
{
  return 0;
}

unsigned int 
DEFUN(bfd_0u,(ignore),
      bfd *ignore)
{
   return 0;
}

void 
DEFUN(bfd_void,(ignore),
      bfd *ignore)
{
}

boolean
DEFUN(_bfd_dummy_core_file_matches_executable_p,(ignore_core_bfd, ignore_exec_bfd),
      bfd *ignore_core_bfd AND
      bfd *ignore_exec_bfd)
{
  bfd_error = invalid_operation;
  return false;
}

/* of course you can't initialize a function to be the same as another, grr */

char *
DEFUN(_bfd_dummy_core_file_failing_command,(ignore_abfd),
      bfd *ignore_abfd)
{
  return (char *)NULL;
}

int
DEFUN(_bfd_dummy_core_file_failing_signal,(ignore_abfd),
     bfd *ignore_abfd)
{
  return 0;
}

bfd_target *
DEFUN(_bfd_dummy_target,(ignore_abfd),
     bfd *ignore_abfd)
{
  return 0;
}

/** zalloc -- allocate and clear storage */


#ifndef zalloc
char *
DEFUN(zalloc,(size),
      bfd_size_type size)
{
  char *ptr = (char *) malloc ((size_t)size);

  if ((ptr != NULL) && (size != 0))
   memset(ptr,0, (size_t) size);

  return ptr;
}
#endif

/*
INTERNAL_FUNCTION
	bfd_xmalloc

SYNOPSIS
	PTR  bfd_xmalloc( bfd_size_type size);

DESCRIPTION
	Like malloc, but exit if no more memory.

*/

/** There is major inconsistency in how running out of memory is handled.
  Some routines return a NULL, and set bfd_error to no_memory.
  However, obstack routines can't do this ... */


DEFUN(PTR bfd_xmalloc,(size),
      bfd_size_type size)
{
  static CONST char no_memory_message[] = "Virtual memory exhausted!\n";
  PTR ptr;
  if (size == 0) size = 1;
  ptr = (PTR)malloc((size_t) size);
  if (!ptr)
    {
      write (2, no_memory_message, sizeof(no_memory_message)-1);
      exit (-1);
    }
  return ptr;
}

/*
INTERNAL_FUNCTION
	bfd_xmalloc_by_size_t

SYNOPSIS
	PTR bfd_xmalloc_by_size_t ( size_t size);

DESCRIPTION
	Like malloc, but exit if no more memory.
	Uses size_t, so it's suitable for use as obstack_chunk_alloc.
 */
PTR
DEFUN(bfd_xmalloc_by_size_t, (size),
      size_t size)
{
  return bfd_xmalloc ((bfd_size_type) size);
}

/* Some IO code */


/* Note that archive entries don't have streams; they share their parent's.
   This allows someone to play with the iostream behind BFD's back.

   Also, note that the origin pointer points to the beginning of a file's
   contents (0 for non-archive elements).  For archive entries this is the
   first octet in the file, NOT the beginning of the archive header. */

static 
int DEFUN(real_read,(where, a,b, file),
          PTR where AND
          int a AND
          int b AND
          FILE *file)
{
  return fread(where, a,b,file);
}
bfd_size_type
DEFUN(bfd_read,(ptr, size, nitems, abfd),
      PTR ptr AND
      bfd_size_type size AND
      bfd_size_type nitems AND
      bfd *abfd)
{
  int nread;
  nread = real_read (ptr, 1, (int)(size*nitems), bfd_cache_lookup(abfd));
#ifdef FILE_OFFSET_IS_CHAR_INDEX
  if (nread > 0)
    abfd->where += nread;
#endif
  return nread;
}

bfd_size_type
DEFUN(bfd_write,(ptr, size, nitems, abfd),
      CONST PTR ptr AND
      bfd_size_type size AND
      bfd_size_type nitems AND
      bfd *abfd)
{
  int nwrote = fwrite (ptr, 1, (int)(size*nitems), bfd_cache_lookup(abfd));
#ifdef FILE_OFFSET_IS_CHAR_INDEX
  if (nwrote > 0)
    abfd->where += nwrote;
#endif
  return nwrote;
}

/*
INTERNAL_FUNCTION
	bfd_write_bigendian_4byte_int

SYNOPSIS
	void bfd_write_bigendian_4byte_int(bfd *abfd,  int i);

DESCRIPTION
	Writes a 4 byte integer to the outputing bfd, in big endian
	mode regardless of what else is going on.  This is useful in
	archives.

*/
void
DEFUN(bfd_write_bigendian_4byte_int,(abfd, i),
      bfd *abfd AND
      int i)
{
  bfd_byte buffer[4];
  bfd_putb32(i, buffer);
  bfd_write((PTR)buffer, 4, 1, abfd);
}

long
DEFUN(bfd_tell,(abfd),
      bfd *abfd)
{
  file_ptr ptr;

  ptr = ftell (bfd_cache_lookup(abfd));

  if (abfd->my_archive)
    ptr -= abfd->origin;
  abfd->where = ptr;
  return ptr;
}

int
DEFUN(bfd_flush,(abfd),
      bfd *abfd)
{
  return fflush (bfd_cache_lookup(abfd));
}

int
DEFUN(bfd_stat,(abfd, statbuf),
      bfd *abfd AND
      struct stat *statbuf)
{
  return fstat (fileno(bfd_cache_lookup(abfd)), statbuf);
}

int
DEFUN(bfd_seek,(abfd, position, direction),
      bfd * CONST abfd AND
      CONST file_ptr position AND
      CONST int direction)
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
#ifndef NDEBUG
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
    /* Force redetermination of `where' field.  */
    bfd_tell (abfd);
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
DEFUN(bfd_add_to_string_table,(table, new_string, table_length, free_ptr),
      char **table AND
      char *new_string AND
      unsigned int *table_length AND
      char **free_ptr)
{
  size_t string_length = strlen (new_string) + 1; /* include null here */
  char *base = *table;
  size_t space_length = *table_length;
  unsigned int offset = (base ? *free_ptr - base : 0);

  if (base == NULL) {
    /* Avoid a useless regrow if we can (but of course we still
       take it next time */
    space_length = (string_length < DEFAULT_STRING_SPACE_SIZE ?
                    DEFAULT_STRING_SPACE_SIZE : string_length+1);
    base = zalloc ((bfd_size_type) space_length);

    if (base == NULL) {
      bfd_error = no_memory;
      return false;
    }
  }

  if ((size_t)(offset + string_length) >= space_length) {
    /* Make sure we will have enough space */
    while ((size_t)(offset + string_length) >= space_length) 
      space_length += space_length/2; /* grow by 50% */

    base = (char *) realloc (base, space_length);
    if (base == NULL) {
      bfd_error = no_memory;
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
	around in macros--for example libaout.h defines GET_WORD to
	either bfd_get_32 or bfd_get_64.

	In the put routines, val must be a bfd_vma.  If we are on a
	system without prototypes, the caller is responsible for making
	sure that is true, with a cast if necessary.  We don't cast
	them in the macro definitions because that would prevent lint
	or gcc -Wall from detecting sins such as passing a pointer.
	To detect calling these with less than a bfd_vma, use gcc
	-Wconversion on a host with 64 bit bfd_vma's.

.
.{* Byte swapping macros for user section data.  *}
.
.#define bfd_put_8(abfd, val, ptr) \
.                (*((unsigned char *)(ptr)) = (unsigned char)val)
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
FUNCTION
	bfd_h_get_size

DESCRIPTION
	These macros have the same function as their <<bfd_get_x>>
	bretherin, except that they are used for removing information
	for the header records of object files. Believe it or not,
	some object files keep their header records in big endian
	order, and their data in little endian order.
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
#define EIGHT_GAZILLION (((HOST_64_BIT)0x80000000) << 32)
#define COERCE64(x) \
  (((bfd_signed_vma) (x) ^ EIGHT_GAZILLION) - EIGHT_GAZILLION)

bfd_vma
DEFUN(bfd_getb16,(addr),
      register bfd_byte *addr)
{
        return (addr[0] << 8) | addr[1];
}

bfd_vma
DEFUN(bfd_getl16,(addr),
      register bfd_byte *addr)
{
        return (addr[1] << 8) | addr[0];
}

bfd_signed_vma
DEFUN(bfd_getb_signed_16,(addr),
      register bfd_byte *addr)
{
        return COERCE16((addr[0] << 8) | addr[1]);
}

bfd_signed_vma
DEFUN(bfd_getl_signed_16,(addr),
      register bfd_byte *addr)
{
        return COERCE16((addr[1] << 8) | addr[0]);
}

void
DEFUN(bfd_putb16,(data, addr),
      bfd_vma data AND
      register bfd_byte *addr)
{
        addr[0] = (bfd_byte)(data >> 8);
        addr[1] = (bfd_byte )data;
}

void
DEFUN(bfd_putl16,(data, addr),
      bfd_vma data AND              
      register bfd_byte *addr)
{
        addr[0] = (bfd_byte )data;
        addr[1] = (bfd_byte)(data >> 8);
}

bfd_vma
bfd_getb32 (addr)
     register bfd_byte *addr;
{
        return (((((bfd_vma)addr[0] << 8) | addr[1]) << 8)
		| addr[2]) << 8 | addr[3];
}

bfd_vma
bfd_getl32 (addr)
        register bfd_byte *addr;
{
        return (((((bfd_vma)addr[3] << 8) | addr[2]) << 8)
		| addr[1]) << 8 | addr[0];
}

bfd_signed_vma
bfd_getb_signed_32 (addr)
     register bfd_byte *addr;
{
        return COERCE32((((((bfd_vma)addr[0] << 8) | addr[1]) << 8)
			 | addr[2]) << 8 | addr[3]);
}

bfd_signed_vma
bfd_getl_signed_32 (addr)
        register bfd_byte *addr;
{
        return COERCE32((((((bfd_vma)addr[3] << 8) | addr[2]) << 8)
			 | addr[1]) << 8 | addr[0]);
}

bfd_vma
DEFUN(bfd_getb64,(addr),
      register bfd_byte *addr)
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
DEFUN(bfd_getl64,(addr),
      register bfd_byte *addr)
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
DEFUN(bfd_getb_signed_64,(addr),
      register bfd_byte *addr)
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
DEFUN(bfd_getl_signed_64,(addr),
      register bfd_byte *addr)
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
DEFUN(bfd_putb32,(data, addr),
      bfd_vma data AND
      register bfd_byte *addr)
{
        addr[0] = (bfd_byte)(data >> 24);
        addr[1] = (bfd_byte)(data >> 16);
        addr[2] = (bfd_byte)(data >>  8);
        addr[3] = (bfd_byte)data;
}

void
DEFUN(bfd_putl32,(data, addr),
      bfd_vma data AND
      register bfd_byte *addr)
{
        addr[0] = (bfd_byte)data;
        addr[1] = (bfd_byte)(data >>  8);
        addr[2] = (bfd_byte)(data >> 16);
        addr[3] = (bfd_byte)(data >> 24);
}
void
DEFUN(bfd_putb64,(data, addr),
        bfd_vma data AND
        register bfd_byte *addr)
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
DEFUN(bfd_putl64,(data, addr),
      bfd_vma data AND
      register bfd_byte *addr)
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
DEFUN(bfd_generic_get_section_contents, (abfd, section, location, offset, count),
      bfd *abfd AND
      sec_ptr section AND
      PTR location AND
      file_ptr offset AND
      bfd_size_type count)
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
DEFUN(bfd_generic_set_section_contents, (abfd, section, location, offset, count),
      bfd *abfd AND
      sec_ptr section AND
      PTR location AND
      file_ptr offset AND
      bfd_size_type count)
{
    if (count == 0)
        return true;
    if ((bfd_size_type)(offset+count) > bfd_get_section_size_after_reloc(section)
        || bfd_seek(abfd, (file_ptr)(section->filepos + offset), SEEK_SET) == -1
        || bfd_write(location, (bfd_size_type)1, count, abfd) != count)
        return (false); /* on error */
    return (true);
}

/*
INTERNAL_FUNCTION
	bfd_log2

DESCRIPTION
	Return the log base 2 of the value supplied, rounded up. eg an
	arg of 1025 would return 11.

SYNOPSIS
	unsigned int bfd_log2(bfd_vma x);
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
