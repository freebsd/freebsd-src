/* BFD back-end for archive files (libraries).
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 99, 2000
   Free Software Foundation, Inc.
   Written by Cygnus Support.  Mostly Gumby Henkel-Wallace's fault.

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
@setfilename archive-info
SECTION
	Archives

DESCRIPTION
	An archive (or library) is just another BFD.  It has a symbol
	table, although there's not much a user program will do with it.

	The big difference between an archive BFD and an ordinary BFD
	is that the archive doesn't have sections.  Instead it has a
	chain of BFDs that are considered its contents.  These BFDs can
	be manipulated like any other.  The BFDs contained in an
	archive opened for reading will all be opened for reading.  You
	may put either input or output BFDs into an archive opened for
	output; they will be handled correctly when the archive is closed.

	Use <<bfd_openr_next_archived_file>> to step through
	the contents of an archive opened for input.  You don't
	have to read the entire archive if you don't want
	to!  Read it until you find what you want.

	Archive contents of output BFDs are chained through the
	<<next>> pointer in a BFD.  The first one is findable through
	the <<archive_head>> slot of the archive.  Set it with
	<<bfd_set_archive_head>> (q.v.).  A given BFD may be in only one
	open output archive at a time.

	As expected, the BFD archive code is more general than the
	archive code of any given environment.  BFD archives may
	contain files of different formats (e.g., a.out and coff) and
	even different architectures.  You may even place archives
	recursively into archives!

	This can cause unexpected confusion, since some archive
	formats are more expressive than others.  For instance, Intel
	COFF archives can preserve long filenames; SunOS a.out archives
	cannot.  If you move a file from the first to the second
	format and back again, the filename may be truncated.
	Likewise, different a.out environments have different
	conventions as to how they truncate filenames, whether they
	preserve directory names in filenames, etc.  When
	interoperating with native tools, be sure your files are
	homogeneous.

	Beware: most of these formats do not react well to the
	presence of spaces in filenames.  We do the best we can, but
	can't always handle this case due to restrictions in the format of
	archives.  Many Unix utilities are braindead in regards to
	spaces and such in filenames anyway, so this shouldn't be much
	of a restriction.

	Archives are supported in BFD in <<archive.c>>.

*/

/* Assumes:
   o - all archive elements start on an even boundary, newline padded;
   o - all arch headers are char *;
   o - all arch headers are the same size (across architectures).
*/

/* Some formats provide a way to cram a long filename into the short
   (16 chars) space provided by a BSD archive.  The trick is: make a
   special "file" in the front of the archive, sort of like the SYMDEF
   entry.  If the filename is too long to fit, put it in the extended
   name table, and use its index as the filename.  To prevent
   confusion prepend the index with a space.  This means you can't
   have filenames that start with a space, but then again, many Unix
   utilities can't handle that anyway.

   This scheme unfortunately requires that you stand on your head in
   order to write an archive since you need to put a magic file at the
   front, and need to touch every entry to do so.  C'est la vie.

   We support two variants of this idea:
   The SVR4 format (extended name table is named "//"),
   and an extended pseudo-BSD variant (extended name table is named
   "ARFILENAMES/").  The origin of the latter format is uncertain.

   BSD 4.4 uses a third scheme:  It writes a long filename
   directly after the header.  This allows 'ar q' to work.
   We currently can read BSD 4.4 archives, but not write them.
*/

/* Summary of archive member names:

 Symbol table (must be first):
 "__.SYMDEF       " - Symbol table, Berkeley style, produced by ranlib.
 "/               " - Symbol table, system 5 style.

 Long name table (must be before regular file members):
 "//              " - Long name table, System 5 R4 style.
 "ARFILENAMES/    " - Long name table, non-standard extended BSD (not BSD 4.4).

 Regular file members with short names:
 "filename.o/     " - Regular file, System 5 style (embedded spaces ok).
 "filename.o      " - Regular file, Berkeley style (no embedded spaces).

 Regular files with long names (or embedded spaces, for BSD variants):
 "/18             " - SVR4 style, name at offset 18 in name table.
 "#1/23           " - Long name (or embedded paces) 23 characters long,
		      BSD 4.4 style, full name follows header.
		      Implemented for reading, not writing.
 " 18             " - Long name 18 characters long, extended pseudo-BSD.
 */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "aout/ar.h"
#include "aout/ranlib.h"
#include <ctype.h>

#ifndef errno
extern int errno;
#endif

#ifdef GNU960
#define BFD_GNU960_ARMAG(abfd)	(BFD_COFF_FILE_P((abfd)) ? ARMAG : ARMAGB)
#endif

/* Define offsetof for those systems which lack it */

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

/* We keep a cache of archive filepointers to archive elements to
   speed up searching the archive by filepos.  We only add an entry to
   the cache when we actually read one.  We also don't sort the cache;
   it's generally short enough to search linearly.
   Note that the pointers here point to the front of the ar_hdr, not
   to the front of the contents!
*/
struct ar_cache
{
  file_ptr ptr;
  bfd *arelt;
  struct ar_cache *next;
};

#define ar_padchar(abfd) ((abfd)->xvec->ar_pad_char)
#define ar_maxnamelen(abfd) ((abfd)->xvec->ar_max_namelen)

#define arch_eltdata(bfd) ((struct areltdata *)((bfd)->arelt_data))
#define arch_hdr(bfd) ((struct ar_hdr *)arch_eltdata(bfd)->arch_header)

static char *get_extended_arelt_filename PARAMS ((bfd *arch,
						  const char *name));
static boolean do_slurp_bsd_armap PARAMS ((bfd *abfd));
static boolean do_slurp_coff_armap PARAMS ((bfd *abfd));
static const char *normalize PARAMS ((bfd *, const char *file));
static struct areltdata *bfd_ar_hdr_from_filesystem PARAMS ((bfd *abfd,
							     const char *,
							     bfd *member));

boolean
_bfd_generic_mkarchive (abfd)
     bfd *abfd;
{
  abfd->tdata.aout_ar_data = ((struct artdata *)
			      bfd_zalloc (abfd, sizeof (struct artdata)));

  if (bfd_ardata (abfd) == NULL)
    return false;

  bfd_ardata (abfd)->cache = NULL;
  bfd_ardata (abfd)->archive_head = NULL;
  bfd_ardata (abfd)->symdefs = NULL;
  bfd_ardata (abfd)->extended_names = NULL;
  bfd_ardata (abfd)->tdata = NULL;

  return true;
}

/*
FUNCTION
	bfd_get_next_mapent

SYNOPSIS
	symindex bfd_get_next_mapent(bfd *abfd, symindex previous, carsym **sym);

DESCRIPTION
	Step through archive @var{abfd}'s symbol table (if it
	has one).  Successively update @var{sym} with the next symbol's
	information, returning that symbol's (internal) index into the
	symbol table.

	Supply <<BFD_NO_MORE_SYMBOLS>> as the @var{previous} entry to get
	the first one; returns <<BFD_NO_MORE_SYMBOLS>> when you've already
	got the last one.

	A <<carsym>> is a canonical archive symbol.  The only
	user-visible element is its name, a null-terminated string.
*/

symindex
bfd_get_next_mapent (abfd, prev, entry)
     bfd *abfd;
     symindex prev;
     carsym **entry;
{
  if (!bfd_has_map (abfd))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return BFD_NO_MORE_SYMBOLS;
    }

  if (prev == BFD_NO_MORE_SYMBOLS)
    prev = 0;
  else
    ++prev;
  if (prev >= bfd_ardata (abfd)->symdef_count)
    return BFD_NO_MORE_SYMBOLS;

  *entry = (bfd_ardata (abfd)->symdefs + prev);
  return prev;
}

/* To be called by backends only */

bfd *
_bfd_create_empty_archive_element_shell (obfd)
     bfd *obfd;
{
  return _bfd_new_bfd_contained_in (obfd);
}

/*
FUNCTION
	bfd_set_archive_head

SYNOPSIS
	boolean bfd_set_archive_head(bfd *output, bfd *new_head);

DESCRIPTION
	Set the head of the chain of
	BFDs contained in the archive @var{output} to @var{new_head}.
*/

boolean
bfd_set_archive_head (output_archive, new_head)
     bfd *output_archive;
     bfd *new_head;
{

  output_archive->archive_head = new_head;
  return true;
}

bfd *
_bfd_look_for_bfd_in_cache (arch_bfd, filepos)
     bfd *arch_bfd;
     file_ptr filepos;
{
  struct ar_cache *current;

  for (current = bfd_ardata (arch_bfd)->cache; current != NULL;
       current = current->next)
    if (current->ptr == filepos)
      return current->arelt;

  return NULL;
}

/* Kind of stupid to call cons for each one, but we don't do too many */
boolean
_bfd_add_bfd_to_archive_cache (arch_bfd, filepos, new_elt)
     bfd *arch_bfd, *new_elt;
     file_ptr filepos;
{
  struct ar_cache *new_cache = ((struct ar_cache *)
				bfd_zalloc (arch_bfd,
					    sizeof (struct ar_cache)));

  if (new_cache == NULL)
    return false;

  new_cache->ptr = filepos;
  new_cache->arelt = new_elt;
  new_cache->next = (struct ar_cache *) NULL;
  if (bfd_ardata (arch_bfd)->cache == NULL)
    bfd_ardata (arch_bfd)->cache = new_cache;
  else
    {
      struct ar_cache *current = bfd_ardata (arch_bfd)->cache;

      while (current->next != NULL)
	current = current->next;
      current->next = new_cache;
    }

  return true;
}

/* The name begins with space.  Hence the rest of the name is an index into
   the string table. */

static char *
get_extended_arelt_filename (arch, name)
     bfd *arch;
     const char *name;
{
  unsigned long index = 0;

  /* Should extract string so that I can guarantee not to overflow into
     the next region, but I'm too lazy. */
  errno = 0;
  /* Skip first char, which is '/' in SVR4 or ' ' in some other variants. */
  index = strtol (name + 1, NULL, 10);
  if (errno != 0)
    {
      bfd_set_error (bfd_error_malformed_archive);
      return NULL;
    }

  return bfd_ardata (arch)->extended_names + index;
}

/* This functions reads an arch header and returns an areltdata pointer, or
   NULL on error.

   Presumes the file pointer is already in the right place (ie pointing
   to the ar_hdr in the file).   Moves the file pointer; on success it
   should be pointing to the front of the file contents; on failure it
   could have been moved arbitrarily.
*/

PTR
_bfd_generic_read_ar_hdr (abfd)
     bfd *abfd;
{
  return _bfd_generic_read_ar_hdr_mag (abfd, (const char *) NULL);
}

/* Alpha ECOFF uses an optional different ARFMAG value, so we have a
   variant of _bfd_generic_read_ar_hdr which accepts a magic string.  */

PTR
_bfd_generic_read_ar_hdr_mag (abfd, mag)
     bfd *abfd;
     const char *mag;
{
  struct ar_hdr hdr;
  char *hdrp = (char *) &hdr;
  unsigned int parsed_size;
  struct areltdata *ared;
  char *filename = NULL;
  unsigned int namelen = 0;
  unsigned int allocsize = sizeof (struct areltdata) + sizeof (struct ar_hdr);
  char *allocptr = 0;

  if (bfd_read ((PTR) hdrp, 1, sizeof (struct ar_hdr), abfd)
      != sizeof (struct ar_hdr))
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_no_more_archived_files);
      return NULL;
    }
  if (strncmp (hdr.ar_fmag, ARFMAG, 2) != 0
      && (mag == NULL
	  || strncmp (hdr.ar_fmag, mag, 2) != 0))
    {
      bfd_set_error (bfd_error_malformed_archive);
      return NULL;
    }

  errno = 0;
  parsed_size = strtol (hdr.ar_size, NULL, 10);
  if (errno != 0)
    {
      bfd_set_error (bfd_error_malformed_archive);
      return NULL;
    }

  /* Extract the filename from the archive - there are two ways to
     specify an extended name table, either the first char of the
     name is a space, or it's a slash.  */
  if ((hdr.ar_name[0] == '/'
       || (hdr.ar_name[0] == ' '
	   && memchr (hdr.ar_name, '/', ar_maxnamelen (abfd)) == NULL))
      && bfd_ardata (abfd)->extended_names != NULL)
    {
      filename = get_extended_arelt_filename (abfd, hdr.ar_name);
      if (filename == NULL)
	{
	  bfd_set_error (bfd_error_malformed_archive);
	  return NULL;
	}
    }
  /* BSD4.4-style long filename.
     Only implemented for reading, so far! */
  else if (hdr.ar_name[0] == '#'
	   && hdr.ar_name[1] == '1'
	   && hdr.ar_name[2] == '/'
	   && isdigit ((unsigned char) hdr.ar_name[3]))
    {
      /* BSD-4.4 extended name */
      namelen = atoi (&hdr.ar_name[3]);
      allocsize += namelen + 1;
      parsed_size -= namelen;

      allocptr = bfd_zalloc (abfd, allocsize);
      if (allocptr == NULL)
	return NULL;
      filename = (allocptr
		  + sizeof (struct areltdata)
		  + sizeof (struct ar_hdr));
      if (bfd_read (filename, 1, namelen, abfd) != namelen)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_no_more_archived_files);
	  return NULL;
	}
      filename[namelen] = '\0';
    }
  else
    {
      /* We judge the end of the name by looking for '/' or ' '.
	 Note:  The SYSV format (terminated by '/') allows embedded
	 spaces, so only look for ' ' if we don't find '/'. */

      char *e;
      e = memchr (hdr.ar_name, '\0', ar_maxnamelen (abfd));
      if (e == NULL)
	{
          e = memchr (hdr.ar_name, '/', ar_maxnamelen (abfd));
	  if (e == NULL)
            e = memchr (hdr.ar_name, ' ', ar_maxnamelen (abfd));
	}

      if (e != NULL)
	namelen = e - hdr.ar_name;
      else
	{
	  /* If we didn't find a termination character, then the name
	     must be the entire field.  */
	  namelen = ar_maxnamelen (abfd);
	}

      allocsize += namelen + 1;
    }

  if (!allocptr)
    {
      allocptr = bfd_zalloc (abfd, allocsize);
      if (allocptr == NULL)
	return NULL;
    }

  ared = (struct areltdata *) allocptr;

  ared->arch_header = allocptr + sizeof (struct areltdata);
  memcpy ((char *) ared->arch_header, (char *) &hdr, sizeof (struct ar_hdr));
  ared->parsed_size = parsed_size;

  if (filename != NULL)
    ared->filename = filename;
  else
    {
      ared->filename = allocptr + (sizeof (struct areltdata) +
				   sizeof (struct ar_hdr));
      if (namelen)
	memcpy (ared->filename, hdr.ar_name, namelen);
      ared->filename[namelen] = '\0';
    }

  return (PTR) ared;
}

/* This is an internal function; it's mainly used when indexing
   through the archive symbol table, but also used to get the next
   element, since it handles the bookkeeping so nicely for us.  */

bfd *
_bfd_get_elt_at_filepos (archive, filepos)
     bfd *archive;
     file_ptr filepos;
{
  struct areltdata *new_areldata;
  bfd *n_nfd;

  n_nfd = _bfd_look_for_bfd_in_cache (archive, filepos);
  if (n_nfd)
    return n_nfd;

  if (0 > bfd_seek (archive, filepos, SEEK_SET))
    return NULL;

  if ((new_areldata = (struct areltdata *) _bfd_read_ar_hdr (archive)) == NULL)
    return NULL;

  n_nfd = _bfd_create_empty_archive_element_shell (archive);
  if (n_nfd == NULL)
    {
      bfd_release (archive, (PTR) new_areldata);
      return NULL;
    }

  n_nfd->origin = bfd_tell (archive);
  n_nfd->arelt_data = (PTR) new_areldata;
  n_nfd->filename = new_areldata->filename;

  if (_bfd_add_bfd_to_archive_cache (archive, filepos, n_nfd))
    return n_nfd;

  /* huh? */
  bfd_release (archive, (PTR) n_nfd);
  bfd_release (archive, (PTR) new_areldata);
  return NULL;
}

/* Return the BFD which is referenced by the symbol in ABFD indexed by
   INDEX.  INDEX should have been returned by bfd_get_next_mapent.  */

bfd *
_bfd_generic_get_elt_at_index (abfd, index)
     bfd *abfd;
     symindex index;
{
  carsym *entry;

  entry = bfd_ardata (abfd)->symdefs + index;
  return _bfd_get_elt_at_filepos (abfd, entry->file_offset);
}

/*
FUNCTION
	bfd_openr_next_archived_file

SYNOPSIS
	bfd *bfd_openr_next_archived_file(bfd *archive, bfd *previous);

DESCRIPTION
	Provided a BFD, @var{archive}, containing an archive and NULL, open
	an input BFD on the first contained element and returns that.
	Subsequent calls should pass
	the archive and the previous return value to return a created
	BFD to the next contained element. NULL is returned when there
	are no more.

*/

bfd *
bfd_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  if ((bfd_get_format (archive) != bfd_archive) ||
      (archive->direction == write_direction))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return NULL;
    }

  return BFD_SEND (archive,
		   openr_next_archived_file,
		   (archive,
		    last_file));
}

bfd *
bfd_generic_openr_next_archived_file (archive, last_file)
     bfd *archive;
     bfd *last_file;
{
  file_ptr filestart;

  if (!last_file)
    filestart = bfd_ardata (archive)->first_file_filepos;
  else
    {
      unsigned int size = arelt_size (last_file);
      /* Pad to an even boundary...
	 Note that last_file->origin can be odd in the case of
	 BSD-4.4-style element with a long odd size. */
      filestart = last_file->origin + size;
      filestart += filestart % 2;
    }

  return _bfd_get_elt_at_filepos (archive, filestart);
}


const bfd_target *
bfd_generic_archive_p (abfd)
     bfd *abfd;
{
  struct artdata *tdata_hold;
  char armag[SARMAG + 1];

  tdata_hold = abfd->tdata.aout_ar_data;

  if (bfd_read ((PTR) armag, 1, SARMAG, abfd) != SARMAG)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

#ifdef GNU960
  if (strncmp (armag, BFD_GNU960_ARMAG (abfd), SARMAG) != 0)
    return 0;
#else
  if (strncmp (armag, ARMAG, SARMAG) != 0 &&
      strncmp (armag, ARMAGB, SARMAG) != 0)
    return 0;
#endif

  /* We are setting bfd_ardata(abfd) here, but since bfd_ardata
     involves a cast, we can't do it as the left operand of assignment. */
  abfd->tdata.aout_ar_data = ((struct artdata *)
			      bfd_zalloc (abfd, sizeof (struct artdata)));

  if (bfd_ardata (abfd) == NULL)
    return NULL;

  bfd_ardata (abfd)->first_file_filepos = SARMAG;
  bfd_ardata (abfd)->cache = NULL;
  bfd_ardata (abfd)->archive_head = NULL;
  bfd_ardata (abfd)->symdefs = NULL;
  bfd_ardata (abfd)->extended_names = NULL;
  bfd_ardata (abfd)->tdata = NULL;

  if (!BFD_SEND (abfd, _bfd_slurp_armap, (abfd)))
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = tdata_hold;
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (!BFD_SEND (abfd, _bfd_slurp_extended_name_table, (abfd)))
    {
      bfd_release (abfd, bfd_ardata (abfd));
      abfd->tdata.aout_ar_data = tdata_hold;
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (bfd_has_map (abfd))
    {
      bfd *first;

      /* This archive has a map, so we may presume that the contents
	 are object files.  Make sure that if the first file in the
	 archive can be recognized as an object file, it is for this
	 target.  If not, assume that this is the wrong format.  If
	 the first file is not an object file, somebody is doing
	 something weird, and we permit it so that ar -t will work.

	 This is done because any normal format will recognize any
	 normal archive, regardless of the format of the object files.
	 We do accept an empty archive.  */

      first = bfd_openr_next_archived_file (abfd, (bfd *) NULL);
      if (first != NULL)
	{
	  boolean fail;

	  first->target_defaulted = false;
	  fail = false;
	  if (bfd_check_format (first, bfd_object)
	      && first->xvec != abfd->xvec)
	    {
	      (void) bfd_close (first);
	      bfd_release (abfd, bfd_ardata (abfd));
	      abfd->tdata.aout_ar_data = tdata_hold;
	      bfd_set_error (bfd_error_wrong_format);
	      return NULL;
	    }

	  /* We ought to close first here, but we can't, because we
             have no way to remove it from the archive cache.  FIXME.  */
	}
    }

  return abfd->xvec;
}

/* Some constants for a 32 bit BSD archive structure.  We do not
   support 64 bit archives presently; so far as I know, none actually
   exist.  Supporting them would require changing these constants, and
   changing some bfd_h_get_32 to bfd_h_get_64.  */

/* The size of an external symdef structure.  */
#define BSD_SYMDEF_SIZE 8

/* The offset from the start of a symdef structure to the file offset.  */
#define BSD_SYMDEF_OFFSET_SIZE 4

/* The size of the symdef count.  */
#define BSD_SYMDEF_COUNT_SIZE 4

/* The size of the string count.  */
#define BSD_STRING_COUNT_SIZE 4

/* Returns false on error, true otherwise */

static boolean
do_slurp_bsd_armap (abfd)
     bfd *abfd;
{
  struct areltdata *mapdata;
  unsigned int counter;
  bfd_byte *raw_armap, *rbase;
  struct artdata *ardata = bfd_ardata (abfd);
  char *stringbase;
  unsigned int parsed_size;
  carsym *set;

  mapdata = (struct areltdata *) _bfd_read_ar_hdr (abfd);
  if (mapdata == NULL)
    return false;
  parsed_size = mapdata->parsed_size;
  bfd_release (abfd, (PTR) mapdata);	/* Don't need it any more. */

  raw_armap = (bfd_byte *) bfd_zalloc (abfd, parsed_size);
  if (raw_armap == (bfd_byte *) NULL)
    return false;

  if (bfd_read ((PTR) raw_armap, 1, parsed_size, abfd) != parsed_size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
    byebye:
      bfd_release (abfd, (PTR) raw_armap);
      return false;
    }

  ardata->symdef_count = bfd_h_get_32 (abfd, raw_armap) / BSD_SYMDEF_SIZE;

  if (ardata->symdef_count * BSD_SYMDEF_SIZE >
      parsed_size - BSD_SYMDEF_COUNT_SIZE)
    {
      /* Probably we're using the wrong byte ordering.  */
      bfd_set_error (bfd_error_wrong_format);
      goto byebye;
    }

  ardata->cache = 0;
  rbase = raw_armap + BSD_SYMDEF_COUNT_SIZE;
  stringbase = ((char *) rbase
		+ ardata->symdef_count * BSD_SYMDEF_SIZE
		+ BSD_STRING_COUNT_SIZE);
  ardata->symdefs = (carsym *) bfd_alloc (abfd,
					  (ardata->symdef_count
					   * sizeof (carsym)));
  if (!ardata->symdefs)
    return false;

  for (counter = 0, set = ardata->symdefs;
       counter < ardata->symdef_count;
       counter++, set++, rbase += BSD_SYMDEF_SIZE)
    {
      set->name = bfd_h_get_32 (abfd, rbase) + stringbase;
      set->file_offset = bfd_h_get_32 (abfd, rbase + BSD_SYMDEF_OFFSET_SIZE);
    }

  ardata->first_file_filepos = bfd_tell (abfd);
  /* Pad to an even boundary if you have to */
  ardata->first_file_filepos += (ardata->first_file_filepos) % 2;
  /* FIXME, we should provide some way to free raw_ardata when
     we are done using the strings from it.  For now, it seems
     to be allocated on an objalloc anyway... */
  bfd_has_map (abfd) = true;
  return true;
}

/* Returns false on error, true otherwise */
static boolean
do_slurp_coff_armap (abfd)
     bfd *abfd;
{
  struct areltdata *mapdata;
  int *raw_armap, *rawptr;
  struct artdata *ardata = bfd_ardata (abfd);
  char *stringbase;
  unsigned int stringsize;
  unsigned int parsed_size;
  carsym *carsyms;
  unsigned int nsymz;		/* Number of symbols in armap. */
  bfd_vma (*swap) PARAMS ((const bfd_byte *));
  char int_buf[sizeof (long)];
  unsigned int carsym_size, ptrsize, i;

  mapdata = (struct areltdata *) _bfd_read_ar_hdr (abfd);
  if (mapdata == NULL)
    return false;
  parsed_size = mapdata->parsed_size;
  bfd_release (abfd, (PTR) mapdata);	/* Don't need it any more. */

  if (bfd_read ((PTR) int_buf, 1, 4, abfd) != 4)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
      return false;
    }
  /* It seems that all numeric information in a coff archive is always
     in big endian format, nomatter the host or target. */
  swap = bfd_getb32;
  nsymz = bfd_getb32 ((PTR) int_buf);
  stringsize = parsed_size - (4 * nsymz) - 4;

#if 1
  /* ... except that some archive formats are broken, and it may be our
     fault - the i960 little endian coff sometimes has big and sometimes
     little, because our tools changed.  Here's a horrible hack to clean
     up the crap.  */

  if (stringsize > 0xfffff
      && bfd_get_arch (abfd) == bfd_arch_i960
      && bfd_get_flavour (abfd) == bfd_target_coff_flavour)
    {
      /* This looks dangerous, let's do it the other way around */
      nsymz = bfd_getl32 ((PTR) int_buf);
      stringsize = parsed_size - (4 * nsymz) - 4;
      swap = bfd_getl32;
    }
#endif

  /* The coff armap must be read sequentially.  So we construct a
     bsd-style one in core all at once, for simplicity. */

  carsym_size = (nsymz * sizeof (carsym));
  ptrsize = (4 * nsymz);

  ardata->symdefs = (carsym *) bfd_zalloc (abfd, carsym_size + stringsize + 1);
  if (ardata->symdefs == NULL)
    return false;
  carsyms = ardata->symdefs;
  stringbase = ((char *) ardata->symdefs) + carsym_size;

  /* Allocate and read in the raw offsets. */
  raw_armap = (int *) bfd_alloc (abfd, ptrsize);
  if (raw_armap == NULL)
    goto release_symdefs;
  if (bfd_read ((PTR) raw_armap, 1, ptrsize, abfd) != ptrsize
      || bfd_read ((PTR) stringbase, 1, stringsize, abfd) != stringsize)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
      goto release_raw_armap;
    }

  /* OK, build the carsyms */
  for (i = 0; i < nsymz; i++)
    {
      rawptr = raw_armap + i;
      carsyms->file_offset = swap ((PTR) rawptr);
      carsyms->name = stringbase;
      stringbase += strlen (stringbase) + 1;
      carsyms++;
    }
  *stringbase = 0;

  ardata->symdef_count = nsymz;
  ardata->first_file_filepos = bfd_tell (abfd);
  /* Pad to an even boundary if you have to */
  ardata->first_file_filepos += (ardata->first_file_filepos) % 2;


  bfd_has_map (abfd) = true;
  bfd_release (abfd, (PTR) raw_armap);


  /* Check for a second archive header (as used by PE) */
  {
    struct areltdata *tmp;

    bfd_seek (abfd,   ardata->first_file_filepos, SEEK_SET);
    tmp = (struct areltdata *) _bfd_read_ar_hdr (abfd);
    if (tmp != NULL) 
      {
	if (tmp->arch_header[0] == '/'
	    && tmp->arch_header[1] == ' ') 
	  {
	    ardata->first_file_filepos +=
	      (tmp->parsed_size + sizeof(struct ar_hdr) + 1) & ~1;
	  }
	bfd_release (abfd, tmp);
      }
  }

  return true;

release_raw_armap:
  bfd_release (abfd, (PTR) raw_armap);
release_symdefs:
  bfd_release (abfd, (PTR) (ardata)->symdefs);
  return false;
}

/* This routine can handle either coff-style or bsd-style armaps.
   Returns false on error, true otherwise */

boolean
bfd_slurp_armap (abfd)
     bfd *abfd;
{
  char nextname[17];
  int i = bfd_read ((PTR) nextname, 1, 16, abfd);

  if (i == 0)
    return true;
  if (i != 16)
    return false;

  if (bfd_seek (abfd, (file_ptr) - 16, SEEK_CUR) != 0)
    return false;

  if (!strncmp (nextname, "__.SYMDEF       ", 16)
      || !strncmp (nextname, "__.SYMDEF/      ", 16)) /* old Linux archives */
    return do_slurp_bsd_armap (abfd);
  else if (!strncmp (nextname, "/               ", 16))
    return do_slurp_coff_armap (abfd);
  else if (!strncmp (nextname, "/SYM64/         ", 16))
    {
      /* Irix 6 archive--must be recognized by code in elf64-mips.c.  */
      bfd_set_error (bfd_error_wrong_format);
      return false;
    }

  bfd_has_map (abfd) = false;
  return true;
}

/* Returns false on error, true otherwise */
/* flavor 2 of a bsd armap, similar to bfd_slurp_bsd_armap except the
   header is in a slightly different order and the map name is '/'.
   This flavour is used by hp300hpux. */

#define HPUX_SYMDEF_COUNT_SIZE 2

boolean
bfd_slurp_bsd_armap_f2 (abfd)
     bfd *abfd;
{
  struct areltdata *mapdata;
  char nextname[17];
  unsigned int counter;
  bfd_byte *raw_armap, *rbase;
  struct artdata *ardata = bfd_ardata (abfd);
  char *stringbase;
  unsigned int stringsize;
  carsym *set;
  int i = bfd_read ((PTR) nextname, 1, 16, abfd);

  if (i == 0)
    return true;
  if (i != 16)
    return false;

  /* The archive has at least 16 bytes in it */
  if (bfd_seek (abfd, -16L, SEEK_CUR) != 0)
    return false;

  if (!strncmp (nextname, "__.SYMDEF       ", 16)
      || !strncmp (nextname, "__.SYMDEF/      ", 16)) /* old Linux archives */
    return do_slurp_bsd_armap (abfd);

  if (strncmp (nextname, "/               ", 16))
    {
      bfd_has_map (abfd) = false;
      return true;
    }

  mapdata = (struct areltdata *) _bfd_read_ar_hdr (abfd);
  if (mapdata == NULL)
    return false;

  raw_armap = (bfd_byte *) bfd_zalloc (abfd, mapdata->parsed_size);
  if (raw_armap == NULL)
    {
    byebye:
      bfd_release (abfd, (PTR) mapdata);
      return false;
    }

  if (bfd_read ((PTR) raw_armap, 1, mapdata->parsed_size, abfd) !=
      mapdata->parsed_size)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_malformed_archive);
    byebyebye:
      bfd_release (abfd, (PTR) raw_armap);
      goto byebye;
    }

  ardata->symdef_count = bfd_h_get_16 (abfd, (PTR) raw_armap);

  if (ardata->symdef_count * BSD_SYMDEF_SIZE
      > mapdata->parsed_size - HPUX_SYMDEF_COUNT_SIZE)
    {
      /* Probably we're using the wrong byte ordering.  */
      bfd_set_error (bfd_error_wrong_format);
      goto byebyebye;
    }

  ardata->cache = 0;

  stringsize = bfd_h_get_32 (abfd, raw_armap + HPUX_SYMDEF_COUNT_SIZE);
  /* skip sym count and string sz */
  stringbase = ((char *) raw_armap
		+ HPUX_SYMDEF_COUNT_SIZE
		+ BSD_STRING_COUNT_SIZE);
  rbase = (bfd_byte *) stringbase + stringsize;
  ardata->symdefs = (carsym *) bfd_alloc (abfd,
					  (ardata->symdef_count
					   * BSD_SYMDEF_SIZE));
  if (!ardata->symdefs)
    return false;

  for (counter = 0, set = ardata->symdefs;
       counter < ardata->symdef_count;
       counter++, set++, rbase += BSD_SYMDEF_SIZE)
    {
      set->name = bfd_h_get_32 (abfd, rbase) + stringbase;
      set->file_offset = bfd_h_get_32 (abfd, rbase + BSD_SYMDEF_OFFSET_SIZE);
    }

  ardata->first_file_filepos = bfd_tell (abfd);
  /* Pad to an even boundary if you have to */
  ardata->first_file_filepos += (ardata->first_file_filepos) % 2;
  /* FIXME, we should provide some way to free raw_ardata when
     we are done using the strings from it.  For now, it seems
     to be allocated on an objalloc anyway... */
  bfd_has_map (abfd) = true;
  return true;
}

/** Extended name table.

  Normally archives support only 14-character filenames.

  Intel has extended the format: longer names are stored in a special
  element (the first in the archive, or second if there is an armap);
  the name in the ar_hdr is replaced by <space><index into filename
  element>.  Index is the P.R. of an int (decimal).  Data General have
  extended the format by using the prefix // for the special element */

/* Returns false on error, true otherwise */
boolean
_bfd_slurp_extended_name_table (abfd)
     bfd *abfd;
{
  char nextname[17];
  struct areltdata *namedata;

  /* FIXME:  Formatting sucks here, and in case of failure of BFD_READ,
     we probably don't want to return true.  */
  bfd_seek (abfd, bfd_ardata (abfd)->first_file_filepos, SEEK_SET);
  if (bfd_read ((PTR) nextname, 1, 16, abfd) == 16)
    {
      if (bfd_seek (abfd, (file_ptr) - 16, SEEK_CUR) != 0)
	return false;

      if (strncmp (nextname, "ARFILENAMES/    ", 16) != 0 &&
	  strncmp (nextname, "//              ", 16) != 0)
	{
	  bfd_ardata (abfd)->extended_names = NULL;
	  return true;
	}

      namedata = (struct areltdata *) _bfd_read_ar_hdr (abfd);
      if (namedata == NULL)
	return false;

      bfd_ardata (abfd)->extended_names =
	bfd_zalloc (abfd, namedata->parsed_size);
      if (bfd_ardata (abfd)->extended_names == NULL)
	{
	byebye:
	  bfd_release (abfd, (PTR) namedata);
	  return false;
	}

      if (bfd_read ((PTR) bfd_ardata (abfd)->extended_names, 1,
		    namedata->parsed_size, abfd) != namedata->parsed_size)
	{
	  if (bfd_get_error () != bfd_error_system_call)
	    bfd_set_error (bfd_error_malformed_archive);
	  bfd_release (abfd, (PTR) (bfd_ardata (abfd)->extended_names));
	  bfd_ardata (abfd)->extended_names = NULL;
	  goto byebye;
	}

      /* Since the archive is supposed to be printable if it contains
	 text, the entries in the list are newline-padded, not null
	 padded. In SVR4-style archives, the names also have a
	 trailing '/'.  DOS/NT created archive often have \ in them
	 We'll fix all problems here..  */
      {
	char *temp = bfd_ardata (abfd)->extended_names;
	char *limit = temp + namedata->parsed_size;
	for (; temp < limit; ++temp) {
	  if (*temp == '\012')
	    temp[temp[-1] == '/' ? -1 : 0] = '\0';
	  if (*temp == '\\')
	    *temp = '/';
	}
      }

      /* Pad to an even boundary if you have to */
      bfd_ardata (abfd)->first_file_filepos = bfd_tell (abfd);
      bfd_ardata (abfd)->first_file_filepos +=
	(bfd_ardata (abfd)->first_file_filepos) % 2;

      /* FIXME, we can't release namedata here because it was allocated
	 below extended_names on the objalloc... */
      /* bfd_release (abfd, namedata); */
    }
  return true;
}

#ifdef VMS

/* Return a copy of the stuff in the filename between any :]> and a
   semicolon */
static const char *
normalize (abfd, file)
     bfd *abfd;
     const char *file;
{
  CONST char *first;
  CONST char *last;
  char *copy;

  first = file + strlen (file) - 1;
  last = first + 1;

  while (first != file)
    {
      if (*first == ';')
	last = first;
      if (*first == ':' || *first == ']' || *first == '>')
	{
	  first++;
	  break;
	}
      first--;
    }

  copy = (char *) bfd_alloc (abfd, last - first + 1);
  if (copy == NULL)
    return NULL;

  memcpy (copy, first, last - first);
  copy[last - first] = 0;

  return copy;
}

#else
static const char *
normalize (abfd, file)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *file;
{
  const char *filename = strrchr (file, '/');


#ifdef HAVE_DOS_BASED_FILE_SYSTEM
  {
    /* We could have foo/bar\\baz, or foo\\bar, or d:bar.  */
    char *bslash = strrchr (file, '\\');
    if (bslash > filename)
      filename = bslash;
    if (filename == NULL && file[0] != '\0' && file[1] == ':')
      filename = file + 1;
  }
#endif
  if (filename != (char *) NULL)
    filename++;
  else
    filename = file;
  return filename;
}
#endif

/* Build a BFD style extended name table.  */

boolean
_bfd_archive_bsd_construct_extended_name_table (abfd, tabloc, tablen, name)
     bfd *abfd;
     char **tabloc;
     bfd_size_type *tablen;
     const char **name;
{
  *name = "ARFILENAMES/";
  return _bfd_construct_extended_name_table (abfd, false, tabloc, tablen);
}

/* Build an SVR4 style extended name table.  */

boolean
_bfd_archive_coff_construct_extended_name_table (abfd, tabloc, tablen, name)
     bfd *abfd;
     char **tabloc;
     bfd_size_type *tablen;
     const char **name;
{
  *name = "//";
  return _bfd_construct_extended_name_table (abfd, true, tabloc, tablen);
}

/* Follows archive_head and produces an extended name table if
   necessary.  Returns (in tabloc) a pointer to an extended name
   table, and in tablen the length of the table.  If it makes an entry
   it clobbers the filename so that the element may be written without
   further massage.  Returns true if it ran successfully, false if
   something went wrong.  A successful return may still involve a
   zero-length tablen!  */

boolean
_bfd_construct_extended_name_table (abfd, trailing_slash, tabloc, tablen)
     bfd *abfd;
     boolean trailing_slash;
     char **tabloc;
     bfd_size_type *tablen;
{
  unsigned int maxname = abfd->xvec->ar_max_namelen;
  unsigned int total_namelen = 0;
  bfd *current;
  char *strptr;

  *tablen = 0;

  /* Figure out how long the table should be */
  for (current = abfd->archive_head; current != NULL; current = current->next)
    {
      const char *normal;
      unsigned int thislen;

      normal = normalize (current, current->filename);
      if (normal == NULL)
	return false;

      thislen = strlen (normal);

      if (thislen > maxname
	  && (bfd_get_file_flags (abfd) & BFD_TRADITIONAL_FORMAT) != 0)
	thislen = maxname;

      if (thislen > maxname)
	{
	  /* Add one to leave room for \n.  */
	  total_namelen += thislen + 1;
	  if (trailing_slash)
	    {
	      /* Leave room for trailing slash.  */
	      ++total_namelen;
	    }
	}
      else
	{
	  struct ar_hdr *hdr = arch_hdr (current);
	  if (strncmp (normal, hdr->ar_name, thislen) != 0
	      || (thislen < sizeof hdr->ar_name
		  && hdr->ar_name[thislen] != ar_padchar (current)))
	    {
	      /* Must have been using extended format even though it
	         didn't need to.  Fix it to use normal format.  */
	      memcpy (hdr->ar_name, normal, thislen);
	      if (thislen < maxname
		  || (thislen == maxname && thislen < sizeof hdr->ar_name))
		hdr->ar_name[thislen] = ar_padchar (current);
	    }
	}
    }

  if (total_namelen == 0)
    return true;

  *tabloc = bfd_zalloc (abfd, total_namelen);
  if (*tabloc == NULL)
    return false;

  *tablen = total_namelen;
  strptr = *tabloc;

  for (current = abfd->archive_head; current != NULL; current =
       current->next)
    {
      const char *normal;
      unsigned int thislen;

      normal = normalize (current, current->filename);
      if (normal == NULL)
	return false;

      thislen = strlen (normal);
      if (thislen > maxname)
	{
	  /* Works for now; may need to be re-engineered if we
	     encounter an oddball archive format and want to
	     generalise this hack. */
	  struct ar_hdr *hdr = arch_hdr (current);
	  strcpy (strptr, normal);
	  if (! trailing_slash)
	    strptr[thislen] = '\012';
	  else
	    {
	      strptr[thislen] = '/';
	      strptr[thislen + 1] = '\012';
	    }
	  hdr->ar_name[0] = ar_padchar (current);
	  /* We know there will always be enough room (one of the few
	     cases where you may safely use sprintf). */
	  sprintf ((hdr->ar_name) + 1, "%-d", (unsigned) (strptr - *tabloc));
	  /* Kinda Kludgy.  We should just use the returned value of
	     sprintf but not all implementations get this right */
	  {
	    char *temp = hdr->ar_name + 2;
	    for (; temp < hdr->ar_name + maxname; temp++)
	      if (*temp == '\0')
		*temp = ' ';
	  }
	  strptr += thislen + 1;
	  if (trailing_slash)
	    ++strptr;
	}
    }

  return true;
}

/** A couple of functions for creating ar_hdrs */

#ifndef HAVE_GETUID
#define getuid() 0
#endif

#ifndef HAVE_GETGID
#define getgid() 0
#endif

/* Takes a filename, returns an arelt_data for it, or NULL if it can't
   make one.  The filename must refer to a filename in the filesystem.
   The filename field of the ar_hdr will NOT be initialized.  If member
   is set, and it's an in-memory bfd, we fake it. */

static struct areltdata *
bfd_ar_hdr_from_filesystem (abfd, filename, member)
     bfd *abfd;
     const char *filename;
     bfd *member;
{
  struct stat status;
  struct areltdata *ared;
  struct ar_hdr *hdr;
  char *temp, *temp1;

  if (member && (member->flags & BFD_IN_MEMORY) != 0)
    {
      /* Assume we just "made" the member, and fake it */
      struct bfd_in_memory *bim = (struct bfd_in_memory *) member->iostream;
      time(&status.st_mtime);
      status.st_uid = getuid();
      status.st_gid = getgid();
      status.st_mode = 0644;
      status.st_size = bim->size;
    }
  else if (stat (filename, &status) != 0)
    {
      bfd_set_error (bfd_error_system_call);
      return NULL;
    }

  ared = (struct areltdata *) bfd_zalloc (abfd, sizeof (struct ar_hdr) +
					  sizeof (struct areltdata));
  if (ared == NULL)
    return NULL;
  hdr = (struct ar_hdr *) (((char *) ared) + sizeof (struct areltdata));

  /* ar headers are space padded, not null padded! */
  memset ((PTR) hdr, ' ', sizeof (struct ar_hdr));

  strncpy (hdr->ar_fmag, ARFMAG, 2);

  /* Goddamned sprintf doesn't permit MAXIMUM field lengths */
  sprintf ((hdr->ar_date), "%-12ld", (long) status.st_mtime);
  sprintf ((hdr->ar_uid), "%ld", (long) status.st_uid);
  sprintf ((hdr->ar_gid), "%ld", (long) status.st_gid);
  sprintf ((hdr->ar_mode), "%-8o", (unsigned int) status.st_mode);
  sprintf ((hdr->ar_size), "%-10ld", (long) status.st_size);
  /* Correct for a lossage in sprintf whereby it null-terminates.  I cannot
     understand how these C losers could design such a ramshackle bunch of
     IO operations */
  temp = (char *) hdr;
  temp1 = temp + sizeof (struct ar_hdr) - 2;
  for (; temp < temp1; temp++)
    {
      if (*temp == '\0')
	*temp = ' ';
    }
  strncpy (hdr->ar_fmag, ARFMAG, 2);
  ared->parsed_size = status.st_size;
  ared->arch_header = (char *) hdr;

  return ared;
}

/* This is magic required by the "ar" program.  Since it's
    undocumented, it's undocumented.  You may think that it would take
    a strong stomach to write this, and it does, but it takes even a
    stronger stomach to try to code around such a thing!  */

struct ar_hdr *bfd_special_undocumented_glue PARAMS ((bfd *, const char *));

struct ar_hdr *
bfd_special_undocumented_glue (abfd, filename)
     bfd *abfd;
     const char *filename;
{
  struct areltdata *ar_elt = bfd_ar_hdr_from_filesystem (abfd, filename, 0);
  if (ar_elt == NULL)
    return NULL;
  return (struct ar_hdr *) ar_elt->arch_header;
}


/* Analogous to stat call */
int
bfd_generic_stat_arch_elt (abfd, buf)
     bfd *abfd;
     struct stat *buf;
{
  struct ar_hdr *hdr;
  char *aloser;

  if (abfd->arelt_data == NULL)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return -1;
    }

  hdr = arch_hdr (abfd);

#define foo(arelt, stelt, size)  \
  buf->stelt = strtol (hdr->arelt, &aloser, size); \
  if (aloser == hdr->arelt) return -1;

  foo (ar_date, st_mtime, 10);
  foo (ar_uid, st_uid, 10);
  foo (ar_gid, st_gid, 10);
  foo (ar_mode, st_mode, 8);

  buf->st_size = arch_eltdata (abfd)->parsed_size;

  return 0;
}

void
bfd_dont_truncate_arname (abfd, pathname, arhdr)
     bfd *abfd;
     CONST char *pathname;
     char *arhdr;
{
  /* FIXME: This interacts unpleasantly with ar's quick-append option.
     Fortunately ic960 users will never use that option.  Fixing this
     is very hard; fortunately I know how to do it and will do so once
     intel's release is out the door. */

  struct ar_hdr *hdr = (struct ar_hdr *) arhdr;
  size_t length;
  const char *filename;
  size_t maxlen = ar_maxnamelen (abfd);

  if ((bfd_get_file_flags (abfd) & BFD_TRADITIONAL_FORMAT) != 0)
    {
      bfd_bsd_truncate_arname (abfd, pathname, arhdr);
      return;
    }

  filename = normalize (abfd, pathname);
  if (filename == NULL)
    {
      /* FIXME */
      abort ();
    }

  length = strlen (filename);

  if (length <= maxlen)
    memcpy (hdr->ar_name, filename, length);

  /* Add the padding character if there is room for it.  */
  if (length < maxlen
      || (length == maxlen && length < sizeof hdr->ar_name))
    (hdr->ar_name)[length] = ar_padchar (abfd);
}

void
bfd_bsd_truncate_arname (abfd, pathname, arhdr)
     bfd *abfd;
     CONST char *pathname;
     char *arhdr;
{
  struct ar_hdr *hdr = (struct ar_hdr *) arhdr;
  int length;
  CONST char *filename = strrchr (pathname, '/');
  int maxlen = ar_maxnamelen (abfd);

#ifdef HAVE_DOS_BASED_FILE_SYSTEM
  {
    /* We could have foo/bar\\baz, or foo\\bar, or d:bar.  */
    char *bslash = strrchr (pathname, '\\');
    if (bslash > filename)
      filename = bslash;
    if (filename == NULL && pathname[0] != '\0' && pathname[1] == ':')
      filename = pathname + 1;
  }
#endif

  if (filename == NULL)
    filename = pathname;
  else
    ++filename;

  length = strlen (filename);

  if (length <= maxlen)
    memcpy (hdr->ar_name, filename, length);
  else
    {
      /* pathname: meet procrustes */
      memcpy (hdr->ar_name, filename, maxlen);
      length = maxlen;
    }

  if (length < maxlen)
    (hdr->ar_name)[length] = ar_padchar (abfd);
}

/* Store name into ar header.  Truncates the name to fit.
   1> strip pathname to be just the basename.
   2> if it's short enuf to fit, stuff it in.
   3> If it doesn't end with .o, truncate it to fit
   4> truncate it before the .o, append .o, stuff THAT in.  */

/* This is what gnu ar does.  It's better but incompatible with the
   bsd ar. */

void
bfd_gnu_truncate_arname (abfd, pathname, arhdr)
     bfd *abfd;
     CONST char *pathname;
     char *arhdr;
{
  struct ar_hdr *hdr = (struct ar_hdr *) arhdr;
  int length;
  CONST char *filename = strrchr (pathname, '/');
  int maxlen = ar_maxnamelen (abfd);

#ifdef HAVE_DOS_BASED_FILE_SYSTEM
  {
    /* We could have foo/bar\\baz, or foo\\bar, or d:bar.  */
    char *bslash = strrchr (pathname, '\\');
    if (bslash > filename)
      filename = bslash;
    if (filename == NULL && pathname[0] != '\0' && pathname[1] == ':')
      filename = pathname + 1;
  }
#endif

  if (filename == NULL)
    filename = pathname;
  else
    ++filename;

  length = strlen (filename);

  if (length <= maxlen)
    memcpy (hdr->ar_name, filename, length);
  else
    {				/* pathname: meet procrustes */
      memcpy (hdr->ar_name, filename, maxlen);
      if ((filename[length - 2] == '.') && (filename[length - 1] == 'o'))
	{
	  hdr->ar_name[maxlen - 2] = '.';
	  hdr->ar_name[maxlen - 1] = 'o';
	}
      length = maxlen;
    }

  if (length < 16)
    (hdr->ar_name)[length] = ar_padchar (abfd);
}

/* The BFD is open for write and has its format set to bfd_archive */

boolean
_bfd_write_archive_contents (arch)
     bfd *arch;
{
  bfd *current;
  char *etable = NULL;
  bfd_size_type elength = 0;
  const char *ename = NULL;
  boolean makemap = bfd_has_map (arch);
  boolean hasobjects = false;	/* if no .o's, don't bother to make a map */
  bfd_size_type wrote;
  unsigned int i;
  int tries;

  /* Verify the viability of all entries; if any of them live in the
     filesystem (as opposed to living in an archive open for input)
     then construct a fresh ar_hdr for them.  */
  for (current = arch->archive_head; current; current = current->next)
    {
      if (bfd_write_p (current))
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  return false;
	}
      if (!current->arelt_data)
	{
	  current->arelt_data =
	    (PTR) bfd_ar_hdr_from_filesystem (arch, current->filename, current);
	  if (!current->arelt_data)
	    return false;

	  /* Put in the file name */
	  BFD_SEND (arch, _bfd_truncate_arname, (arch,
						 current->filename,
					      (char *) arch_hdr (current)));
	}

      if (makemap && ! hasobjects)
	{			/* don't bother if we won't make a map! */
	  if ((bfd_check_format (current, bfd_object))
#if 0				/* FIXME -- these are not set correctly */
	      && ((bfd_get_file_flags (current) & HAS_SYMS))
#endif
	    )
	    hasobjects = true;
	}
    }

  if (!BFD_SEND (arch, _bfd_construct_extended_name_table,
		 (arch, &etable, &elength, &ename)))
    return false;

  if (bfd_seek (arch, (file_ptr) 0, SEEK_SET) != 0)
    return false;
#ifdef GNU960
  wrote = bfd_write (BFD_GNU960_ARMAG (arch), 1, SARMAG, arch);
#else
  wrote = bfd_write (ARMAG, 1, SARMAG, arch);
#endif
  if (wrote != SARMAG)
    return false;

  if (makemap && hasobjects)
    {
      if (_bfd_compute_and_write_armap (arch, elength) != true)
	return false;
    }

  if (elength != 0)
    {
      struct ar_hdr hdr;

      memset ((char *) (&hdr), 0, sizeof (struct ar_hdr));
      strcpy (hdr.ar_name, ename);
      /* Round size up to even number in archive header.  */
      sprintf (&(hdr.ar_size[0]), "%-10d",
	       (int) ((elength + 1) & ~1));
      strncpy (hdr.ar_fmag, ARFMAG, 2);
      for (i = 0; i < sizeof (struct ar_hdr); i++)
	if (((char *) (&hdr))[i] == '\0')
	  (((char *) (&hdr))[i]) = ' ';
      if ((bfd_write ((char *) &hdr, 1, sizeof (struct ar_hdr), arch)
	   != sizeof (struct ar_hdr))
	  || bfd_write (etable, 1, elength, arch) != elength)
	return false;
      if ((elength % 2) == 1)
	{
	  if (bfd_write ("\012", 1, 1, arch) != 1)
	    return false;
	}
    }

  for (current = arch->archive_head; current; current = current->next)
    {
      char buffer[DEFAULT_BUFFERSIZE];
      unsigned int remaining = arelt_size (current);
      struct ar_hdr *hdr = arch_hdr (current);

      /* write ar header */
      if (bfd_write ((char *) hdr, 1, sizeof (*hdr), arch) != sizeof (*hdr))
	return false;
      if (bfd_seek (current, (file_ptr) 0, SEEK_SET) != 0)
	return false;
      while (remaining)
	{
	  unsigned int amt = DEFAULT_BUFFERSIZE;
	  if (amt > remaining)
	    amt = remaining;
	  errno = 0;
	  if (bfd_read (buffer, amt, 1, current) != amt)
	    {
	      if (bfd_get_error () != bfd_error_system_call)
		bfd_set_error (bfd_error_malformed_archive);
	      return false;
	    }
	  if (bfd_write (buffer, amt, 1, arch) != amt)
	    return false;
	  remaining -= amt;
	}
      if ((arelt_size (current) % 2) == 1)
	{
	  if (bfd_write ("\012", 1, 1, arch) != 1)
	    return false;
	}
    }

  if (makemap && hasobjects)
    {
      /* Verify the timestamp in the archive file.  If it would not be
	 accepted by the linker, rewrite it until it would be.  If
	 anything odd happens, break out and just return.  (The
	 Berkeley linker checks the timestamp and refuses to read the
	 table-of-contents if it is >60 seconds less than the file's
	 modified-time.  That painful hack requires this painful hack.  */
      tries = 1;
      do
	{
	  if (bfd_update_armap_timestamp (arch))
	    break;
	  (*_bfd_error_handler)
	    (_("Warning: writing archive was slow: rewriting timestamp\n"));
	}
      while (++tries < 6);
    }

  return true;
}

/* Note that the namidx for the first symbol is 0 */

boolean
_bfd_compute_and_write_armap (arch, elength)
     bfd *arch;
     unsigned int elength;
{
  char *first_name = NULL;
  bfd *current;
  file_ptr elt_no = 0;
  struct orl *map = NULL;
  int orl_max = 1024;		/* fine initial default */
  int orl_count = 0;
  int stridx = 0;		/* string index */
  asymbol **syms = NULL;
  long syms_max = 0;
  boolean ret;

  /* Dunno if this is the best place for this info... */
  if (elength != 0)
    elength += sizeof (struct ar_hdr);
  elength += elength % 2;

  map = (struct orl *) bfd_malloc (orl_max * sizeof (struct orl));
  if (map == NULL)
    goto error_return;

  /* We put the symbol names on the arch objalloc, and then discard
     them when done.  */
  first_name = bfd_alloc (arch, 1);
  if (first_name == NULL)
    goto error_return;

  /* Drop all the files called __.SYMDEF, we're going to make our
     own */
  while (arch->archive_head &&
	 strcmp (arch->archive_head->filename, "__.SYMDEF") == 0)
    arch->archive_head = arch->archive_head->next;

  /* Map over each element */
  for (current = arch->archive_head;
       current != (bfd *) NULL;
       current = current->next, elt_no++)
    {
      if ((bfd_check_format (current, bfd_object) == true)
	  && ((bfd_get_file_flags (current) & HAS_SYMS)))
	{
	  long storage;
	  long symcount;
	  long src_count;

	  storage = bfd_get_symtab_upper_bound (current);
	  if (storage < 0)
	    goto error_return;

	  if (storage != 0)
	    {
	      if (storage > syms_max)
		{
		  if (syms_max > 0)
		    free (syms);
		  syms_max = storage;
		  syms = (asymbol **) bfd_malloc ((size_t) syms_max);
		  if (syms == NULL)
		    goto error_return;
		}
	      symcount = bfd_canonicalize_symtab (current, syms);
	      if (symcount < 0)
		goto error_return;

	      /* Now map over all the symbols, picking out the ones we want */
	      for (src_count = 0; src_count < symcount; src_count++)
		{
		  flagword flags = (syms[src_count])->flags;
		  asection *sec = syms[src_count]->section;

		  if ((flags & BSF_GLOBAL ||
		       flags & BSF_WEAK ||
		       flags & BSF_INDIRECT ||
		       bfd_is_com_section (sec))
		      && ! bfd_is_und_section (sec))
		    {
		      size_t namelen;
		      struct orl *new_map;

		      /* This symbol will go into the archive header */
		      if (orl_count == orl_max)
			{
			  orl_max *= 2;
			  new_map =
			    ((struct orl *)
			     bfd_realloc (map, orl_max * sizeof (struct orl)));
			  if (new_map == (struct orl *) NULL)
			    goto error_return;

			  map = new_map;
			}

		      namelen = strlen (syms[src_count]->name);
		      map[orl_count].name = ((char **)
					     bfd_alloc (arch,
							sizeof (char *)));
		      if (map[orl_count].name == NULL)
			goto error_return;
		      *(map[orl_count].name) = bfd_alloc (arch, namelen + 1);
		      if (*(map[orl_count].name) == NULL)
			goto error_return;
		      strcpy (*(map[orl_count].name), syms[src_count]->name);
		      (map[orl_count]).pos = (file_ptr) current;
		      (map[orl_count]).namidx = stridx;

		      stridx += namelen + 1;
		      ++orl_count;
		    }
		}
	    }

	  /* Now ask the BFD to free up any cached information, so we
	     don't fill all of memory with symbol tables.  */
	  if (! bfd_free_cached_info (current))
	    goto error_return;
	}
    }

  /* OK, now we have collected all the data, let's write them out */
  ret = BFD_SEND (arch, write_armap,
		  (arch, elength, map, orl_count, stridx));

  if (syms_max > 0)
    free (syms);
  if (map != NULL)
    free (map);
  if (first_name != NULL)
    bfd_release (arch, first_name);

  return ret;

 error_return:
  if (syms_max > 0)
    free (syms);
  if (map != NULL)
    free (map);
  if (first_name != NULL)
    bfd_release (arch, first_name);

  return false;
}

boolean
bsd_write_armap (arch, elength, map, orl_count, stridx)
     bfd *arch;
     unsigned int elength;
     struct orl *map;
     unsigned int orl_count;
     int stridx;
{
  int padit = stridx & 1;
  unsigned int ranlibsize = orl_count * BSD_SYMDEF_SIZE;
  unsigned int stringsize = stridx + padit;
  /* Include 8 bytes to store ranlibsize and stringsize in output. */
  unsigned int mapsize = ranlibsize + stringsize + 8;
  file_ptr firstreal;
  bfd *current = arch->archive_head;
  bfd *last_elt = current;	/* last element arch seen */
  bfd_byte temp[4];
  unsigned int count;
  struct ar_hdr hdr;
  struct stat statbuf;
  unsigned int i;

  firstreal = mapsize + elength + sizeof (struct ar_hdr) + SARMAG;

  stat (arch->filename, &statbuf);
  memset ((char *) (&hdr), 0, sizeof (struct ar_hdr));
  sprintf (hdr.ar_name, RANLIBMAG);
  /* Remember the timestamp, to keep it holy.  But fudge it a little.  */
  bfd_ardata (arch)->armap_timestamp = statbuf.st_mtime + ARMAP_TIME_OFFSET;
  bfd_ardata (arch)->armap_datepos = (SARMAG
				      + offsetof (struct ar_hdr, ar_date[0]));
  sprintf (hdr.ar_date, "%ld", bfd_ardata (arch)->armap_timestamp);
  sprintf (hdr.ar_uid, "%ld", (long) getuid ());
  sprintf (hdr.ar_gid, "%ld", (long) getgid ());
  sprintf (hdr.ar_size, "%-10d", (int) mapsize);
  strncpy (hdr.ar_fmag, ARFMAG, 2);
  for (i = 0; i < sizeof (struct ar_hdr); i++)
    if (((char *) (&hdr))[i] == '\0')
      (((char *) (&hdr))[i]) = ' ';
  if (bfd_write ((char *) &hdr, 1, sizeof (struct ar_hdr), arch)
      != sizeof (struct ar_hdr))
    return false;
  bfd_h_put_32 (arch, (bfd_vma) ranlibsize, temp);
  if (bfd_write (temp, 1, sizeof (temp), arch) != sizeof (temp))
    return false;

  for (count = 0; count < orl_count; count++)
    {
      bfd_byte buf[BSD_SYMDEF_SIZE];

      if (((bfd *) (map[count]).pos) != last_elt)
	{
	  do
	    {
	      firstreal += arelt_size (current) + sizeof (struct ar_hdr);
	      firstreal += firstreal % 2;
	      current = current->next;
	    }
	  while (current != (bfd *) (map[count]).pos);
	}			/* if new archive element */

      last_elt = current;
      bfd_h_put_32 (arch, map[count].namidx, buf);
      bfd_h_put_32 (arch, firstreal, buf + BSD_SYMDEF_OFFSET_SIZE);
      if (bfd_write (buf, BSD_SYMDEF_SIZE, 1, arch) != BSD_SYMDEF_SIZE)
	return false;
    }

  /* now write the strings themselves */
  bfd_h_put_32 (arch, stringsize, temp);
  if (bfd_write (temp, 1, sizeof (temp), arch) != sizeof (temp))
    return false;
  for (count = 0; count < orl_count; count++)
    {
      size_t len = strlen (*map[count].name) + 1;

      if (bfd_write (*map[count].name, 1, len, arch) != len)
	return false;
    }

  /* The spec sez this should be a newline.  But in order to be
     bug-compatible for sun's ar we use a null. */
  if (padit)
    {
      if (bfd_write ("", 1, 1, arch) != 1)
	return false;
    }

  return true;
}

/* At the end of archive file handling, update the timestamp in the
   file, so the linker will accept it.

   Return true if the timestamp was OK, or an unusual problem happened.
   Return false if we updated the timestamp.  */

boolean
_bfd_archive_bsd_update_armap_timestamp (arch)
     bfd *arch;
{
  struct stat archstat;
  struct ar_hdr hdr;
  unsigned int i;

  /* Flush writes, get last-write timestamp from file, and compare it
     to the timestamp IN the file.  */
  bfd_flush (arch);
  if (bfd_stat (arch, &archstat) == -1)
    {
      perror (_("Reading archive file mod timestamp"));
      return true;		/* Can't read mod time for some reason */
    }
  if (archstat.st_mtime <= bfd_ardata (arch)->armap_timestamp)
    return true;		/* OK by the linker's rules */

  /* Update the timestamp.  */
  bfd_ardata (arch)->armap_timestamp = archstat.st_mtime + ARMAP_TIME_OFFSET;

  /* Prepare an ASCII version suitable for writing.  */
  memset (hdr.ar_date, 0, sizeof (hdr.ar_date));
  sprintf (hdr.ar_date, "%ld", bfd_ardata (arch)->armap_timestamp);
  for (i = 0; i < sizeof (hdr.ar_date); i++)
    if (hdr.ar_date[i] == '\0')
      (hdr.ar_date)[i] = ' ';

  /* Write it into the file.  */
  bfd_ardata (arch)->armap_datepos = (SARMAG
				      + offsetof (struct ar_hdr, ar_date[0]));
  if (bfd_seek (arch, bfd_ardata (arch)->armap_datepos, SEEK_SET) != 0
      || (bfd_write (hdr.ar_date, sizeof (hdr.ar_date), 1, arch)
	  != sizeof (hdr.ar_date)))
    {
      /* FIXME: bfd can't call perror.  */
      perror (_("Writing updated armap timestamp"));
      return true;		/* Some error while writing */
    }

  return false;			/* We updated the timestamp successfully.  */
}

/* A coff armap looks like :
   lARMAG
   struct ar_hdr with name = '/'
   number of symbols
   offset of file for symbol 0
   offset of file for symbol 1

   offset of file for symbol n-1
   symbol name 0
   symbol name 1

   symbol name n-1
*/

boolean
coff_write_armap (arch, elength, map, symbol_count, stridx)
     bfd *arch;
     unsigned int elength;
     struct orl *map;
     unsigned int symbol_count;
     int stridx;
{
  /* The size of the ranlib is the number of exported symbols in the
     archive * the number of bytes in a int, + an int for the count */
  unsigned int ranlibsize = (symbol_count * 4) + 4;
  unsigned int stringsize = stridx;
  unsigned int mapsize = stringsize + ranlibsize;
  file_ptr archive_member_file_ptr;
  bfd *current = arch->archive_head;
  unsigned int count;
  struct ar_hdr hdr;
  unsigned int i;
  int padit = mapsize & 1;

  if (padit)
    mapsize++;

  /* work out where the first object file will go in the archive */
  archive_member_file_ptr = (mapsize
			     + elength
			     + sizeof (struct ar_hdr)
			     + SARMAG);

  memset ((char *) (&hdr), 0, sizeof (struct ar_hdr));
  hdr.ar_name[0] = '/';
  sprintf (hdr.ar_size, "%-10d", (int) mapsize);
  sprintf (hdr.ar_date, "%ld", (long) time (NULL));
  /* This, at least, is what Intel coff sets the values to.: */
  sprintf ((hdr.ar_uid), "%d", 0);
  sprintf ((hdr.ar_gid), "%d", 0);
  sprintf ((hdr.ar_mode), "%-7o", (unsigned) 0);
  strncpy (hdr.ar_fmag, ARFMAG, 2);

  for (i = 0; i < sizeof (struct ar_hdr); i++)
    if (((char *) (&hdr))[i] == '\0')
      (((char *) (&hdr))[i]) = ' ';

  /* Write the ar header for this item and the number of symbols */

  if (bfd_write ((PTR) &hdr, 1, sizeof (struct ar_hdr), arch)
      != sizeof (struct ar_hdr))
    return false;

  bfd_write_bigendian_4byte_int (arch, symbol_count);

  /* Two passes, first write the file offsets for each symbol -
     remembering that each offset is on a two byte boundary.  */

  /* Write out the file offset for the file associated with each
     symbol, and remember to keep the offsets padded out.  */

  current = arch->archive_head;
  count = 0;
  while (current != (bfd *) NULL && count < symbol_count)
    {
      /* For each symbol which is used defined in this object, write out
	 the object file's address in the archive */

      while (((bfd *) (map[count]).pos) == current)
	{
	  bfd_write_bigendian_4byte_int (arch, archive_member_file_ptr);
	  count++;
	}
      /* Add size of this archive entry */
      archive_member_file_ptr += (arelt_size (current)
				  + sizeof (struct ar_hdr));
      /* remember aboout the even alignment */
      archive_member_file_ptr += archive_member_file_ptr % 2;
      current = current->next;
    }

  /* now write the strings themselves */
  for (count = 0; count < symbol_count; count++)
    {
      size_t len = strlen (*map[count].name) + 1;

      if (bfd_write (*map[count].name, 1, len, arch) != len)
	return false;
    }

  /* The spec sez this should be a newline.  But in order to be
     bug-compatible for arc960 we use a null. */
  if (padit)
    {
      if (bfd_write ("", 1, 1, arch) != 1)
	return false;
    }

  return true;
}
