/* tar.c - read in write tar headers for cpio
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "filetypes.h"
#include "system.h"
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"
#include "rmt.h"
#include "tarhdr.h"

static void to_oct ();
static char *stash_tar_linkname ();
static char *stash_tar_filename ();

/* Compute and return a checksum for TAR_HDR,
   counting the checksum bytes as if they were spaces.  */

unsigned long
tar_checksum (tar_hdr)
     struct tar_header *tar_hdr;
{
  unsigned long sum = 0;
  char *p = (char *) tar_hdr;
  char *q = p + TARRECORDSIZE;
  int i;

  while (p < tar_hdr->chksum)
    sum += *p++ & 0xff;
  for (i = 0; i < 8; ++i)
    {
      sum += ' ';
      ++p;
    }
  while (p < q)
    sum += *p++ & 0xff;
  return sum;
}

/* Write out header FILE_HDR, including the file name, to file
   descriptor OUT_DES.  */

void
write_out_tar_header (file_hdr, out_des)
     struct new_cpio_header *file_hdr;
     int out_des;
{
  int name_len;
  union tar_record tar_rec;
  struct tar_header *tar_hdr = (struct tar_header *) &tar_rec;

  bzero ((char *) &tar_rec, TARRECORDSIZE);

  /* process_copy_out must ensure that file_hdr->c_name is short enough,
     or we will lose here.  */

  name_len = strlen (file_hdr->c_name);
  if (name_len <= TARNAMESIZE)
    {
      strncpy (tar_hdr->name, file_hdr->c_name, name_len);
    }
  else
    {
      /* Fit as much as we can into `name', the rest into `prefix'.  */
      char *suffix = file_hdr->c_name + name_len - TARNAMESIZE;

      /* We have to put the boundary at a slash.  */
      name_len = TARNAMESIZE;
      while (*suffix != '/')
	{
	  --name_len;
	  ++suffix;
	}
      strncpy (tar_hdr->name, suffix + 1, name_len);
      strncpy (tar_hdr->prefix, file_hdr->c_name, suffix - file_hdr->c_name);
    }

  /* SVR4 seems to want the whole mode, not just protection modes.
     Nobody else seems to care, so we might as well put it all in.  */
  to_oct (file_hdr->c_mode, 8, tar_hdr->mode);
  to_oct (file_hdr->c_uid, 8, tar_hdr->uid);
  to_oct (file_hdr->c_gid, 8, tar_hdr->gid);
  to_oct (file_hdr->c_filesize, 12, tar_hdr->size);
  to_oct (file_hdr->c_mtime, 12, tar_hdr->mtime);

  switch (file_hdr->c_mode & CP_IFMT)
    {
    case CP_IFREG:
      if (file_hdr->c_tar_linkname)
	{
	  /* process_copy_out makes sure that c_tar_linkname is shorter
	     than TARLINKNAMESIZE.  */
	  strncpy (tar_hdr->linkname, file_hdr->c_tar_linkname,
		   TARLINKNAMESIZE);
	  tar_hdr->typeflag = LNKTYPE;
	  to_oct (0, 12, tar_hdr->size);
	}
      else
	tar_hdr->typeflag = REGTYPE;
      break;
    case CP_IFDIR:
      tar_hdr->typeflag = DIRTYPE;
      break;
#ifndef __MSDOS__
    case CP_IFCHR:
      tar_hdr->typeflag = CHRTYPE;
      break;
    case CP_IFBLK:
      tar_hdr->typeflag = BLKTYPE;
      break;
#ifdef CP_IFIFO
    case CP_IFIFO:
      tar_hdr->typeflag = FIFOTYPE;
      break;
#endif /* CP_IFIFO */
#ifdef CP_IFLNK
    case CP_IFLNK:
      tar_hdr->typeflag = SYMTYPE;
      /* process_copy_out makes sure that c_tar_linkname is shorter
	 than TARLINKNAMESIZE.  */
      strncpy (tar_hdr->linkname, file_hdr->c_tar_linkname,
	       TARLINKNAMESIZE);
      to_oct (0, 12, tar_hdr->size);
      break;
#endif /* CP_IFLNK */
#endif /* !__MSDOS__ */
    }

  if (archive_format == arf_ustar)
    {
      char *name;

      strncpy (tar_hdr->magic, TMAGIC, TMAGLEN);
      strncpy (tar_hdr->magic + TMAGLEN, TVERSION, TVERSLEN);

#ifndef __MSDOS__
      name = getuser (file_hdr->c_uid);
      if (name)
	strcpy (tar_hdr->uname, name);
      name = getgroup (file_hdr->c_gid);
      if (name)
	strcpy (tar_hdr->gname, name);
#endif

      to_oct (file_hdr->c_rdev_maj, 8, tar_hdr->devmajor);
      to_oct (file_hdr->c_rdev_min, 8, tar_hdr->devminor);
    }

  to_oct (tar_checksum (tar_hdr), 8, tar_hdr->chksum);

  tape_buffered_write ((char *) &tar_rec, out_des, TARRECORDSIZE);
}

/* Return nonzero iff all the bytes in BLOCK are NUL.
   SIZE is the number of bytes to check in BLOCK; it must be a
   multiple of sizeof (long).  */

int
null_block (block, size)
     long *block;
     int size;
{
  register long *p = block;
  register int i = size / sizeof (long);

  while (i--)
    if (*p++)
      return 0;
  return 1;
}

/* Read a tar header, including the file name, from file descriptor IN_DES
   into FILE_HDR.  */

void
read_in_tar_header (file_hdr, in_des)
     struct new_cpio_header *file_hdr;
     int in_des;
{
  long bytes_skipped = 0;
  int warned = FALSE;
  union tar_record tar_rec;
  struct tar_header *tar_hdr = (struct tar_header *) &tar_rec;
#ifndef __MSDOS__
  uid_t *uidp;
  gid_t *gidp;
#endif

  tape_buffered_read ((char *) &tar_rec, in_des, TARRECORDSIZE);

  /* Check for a block of 0's.  */
  if (null_block ((long *) &tar_rec, TARRECORDSIZE))
    {
#if 0
      /* Found one block of 512 0's.  If the next block is also all 0's
	 then this is the end of the archive.  If not, assume the
	 previous block was all corruption and continue reading
	 the archive.  */
      /* Commented out because GNU tar sometimes creates archives with
	 only one block of 0's at the end.  This happened for the
	 cpio 2.0 distribution!  */
      tape_buffered_read ((char *) &tar_rec, in_des, TARRECORDSIZE);
      if (null_block ((long *) &tar_rec, TARRECORDSIZE))
#endif
	{
	  file_hdr->c_name = "TRAILER!!!";
	  return;
	}
#if 0
      bytes_skipped = TARRECORDSIZE;
#endif
    }

  while (1)
    {
      otoa (tar_hdr->chksum, &file_hdr->c_chksum);

      if (file_hdr->c_chksum != tar_checksum (tar_hdr))
	{
	  /* If the checksum is bad, skip 1 byte and try again.  When
	     we try again we do not look for an EOF record (all zeros),
	     because when we start skipping bytes in a corrupted archive
	     the chances are pretty good that we might stumble across
	     2 blocks of 512 zeros (that probably is not really the last
	     record) and it is better to miss the EOF and give the user
	     a "premature EOF" error than to give up too soon on a corrupted
	     archive.  */
	  if (!warned)
	    {
	      error (0, 0, "invalid header: checksum error");
	      warned = TRUE;
	    }
	  bcopy (((char *) &tar_rec) + 1, (char *) &tar_rec,
		 TARRECORDSIZE - 1);
	  tape_buffered_read (((char *) &tar_rec) + (TARRECORDSIZE - 1), in_des, 1);
	  ++bytes_skipped;
	  continue;
	}

      if (archive_format != arf_ustar)
	file_hdr->c_name = stash_tar_filename (NULL, tar_hdr->name);
      else
	file_hdr->c_name = stash_tar_filename (tar_hdr->prefix, tar_hdr->name);
      file_hdr->c_nlink = 1;
      otoa (tar_hdr->mode, &file_hdr->c_mode);
      file_hdr->c_mode = file_hdr->c_mode & 07777;
#ifndef __MSDOS__
      if (archive_format == arf_ustar
	  && (uidp = getuidbyname (tar_hdr->uname)))
	file_hdr->c_uid = *uidp;
      else
#endif
	otoa (tar_hdr->uid, &file_hdr->c_uid);
#ifndef __MSDOS__
      if (archive_format == arf_ustar
	  && (gidp = getgidbyname (tar_hdr->gname)))
	file_hdr->c_gid = *gidp;
      else
#endif
	otoa (tar_hdr->gid, &file_hdr->c_gid);
      otoa (tar_hdr->size, &file_hdr->c_filesize);
      otoa (tar_hdr->mtime, &file_hdr->c_mtime);
      otoa (tar_hdr->devmajor, (unsigned long *) &file_hdr->c_rdev_maj);
      otoa (tar_hdr->devminor, (unsigned long *) &file_hdr->c_rdev_min);
      file_hdr->c_tar_linkname = NULL;

      switch (tar_hdr->typeflag)
	{
	case REGTYPE:
	case CONTTYPE:		/* For now, punt.  */
	default:
	  file_hdr->c_mode |= CP_IFREG;
	  break;
	case DIRTYPE:
	  file_hdr->c_mode |= CP_IFDIR;
	  break;
#ifndef __MSDOS__
	case CHRTYPE:
	  file_hdr->c_mode |= CP_IFCHR;
	  /* If a POSIX tar header has a valid linkname it's always supposed
	     to set typeflag to be LNKTYPE.  System V.4 tar seems to
	     be broken, and for device files with multiple links it
	     puts the name of the link into linkname, but leaves typeflag 
	     as CHRTYPE, BLKTYPE, FIFOTYPE, etc.  */
	  file_hdr->c_tar_linkname = stash_tar_linkname (tar_hdr->linkname);

	  /* Does POSIX say that the filesize must be 0 for devices?  We
	     assume so, but HPUX's POSIX tar sets it to be 1 which causes
	     us problems (when reading an archive we assume we can always
	     skip to the next file by skipping filesize bytes).  For 
	     now at least, it's easier to clear filesize for devices,
	     rather than check everywhere we skip in copyin.c.  */
	  file_hdr->c_filesize = 0;
	  break;
	case BLKTYPE:
	  file_hdr->c_mode |= CP_IFBLK;
	  file_hdr->c_tar_linkname = stash_tar_linkname (tar_hdr->linkname);
	  file_hdr->c_filesize = 0;
	  break;
#ifdef CP_IFIFO
	case FIFOTYPE:
	  file_hdr->c_mode |= CP_IFIFO;
	  file_hdr->c_tar_linkname = stash_tar_linkname (tar_hdr->linkname);
	  file_hdr->c_filesize = 0;
	  break;
#endif
	case SYMTYPE:
#ifdef CP_IFLNK
	  file_hdr->c_mode |= CP_IFLNK;
	  file_hdr->c_tar_linkname = stash_tar_linkname (tar_hdr->linkname);
	  file_hdr->c_filesize = 0;
	  break;
	  /* Else fall through.  */
#endif
	case LNKTYPE:
	  file_hdr->c_mode |= CP_IFREG;
	  file_hdr->c_tar_linkname = stash_tar_linkname (tar_hdr->linkname);
	  file_hdr->c_filesize = 0;
	  break;
#endif /* !__MSDOS__ */
	case AREGTYPE:
	  /* Old tar format; if the last char in filename is '/' then it is
	     a directory, otherwise it's a regular file.  */
	  if (file_hdr->c_name[strlen (file_hdr->c_name) - 1] == '/')
	    file_hdr->c_mode |= CP_IFDIR;
	  else
	    file_hdr->c_mode |= CP_IFREG;
	  break;
	}
      break;
    }
  if (bytes_skipped > 0)
    error (0, 0, "warning: skipped %ld bytes of junk", bytes_skipped);
}

/* Stash the tar linkname in static storage.  */

static char *
stash_tar_linkname (linkname)
     char *linkname;
{
  static char hold_tar_linkname[TARLINKNAMESIZE + 1];

  strncpy (hold_tar_linkname, linkname, TARLINKNAMESIZE);
  hold_tar_linkname[TARLINKNAMESIZE] = '\0';
  return hold_tar_linkname;
}

/* Stash the tar filename and optional prefix in static storage.  */

static char *
stash_tar_filename (prefix, filename)
     char *prefix;
     char *filename;
{
  static char hold_tar_filename[TARNAMESIZE + TARPREFIXSIZE + 2];
  if (prefix == NULL || *prefix == '\0')
    {
      strncpy (hold_tar_filename, filename, TARNAMESIZE);
      hold_tar_filename[TARNAMESIZE] = '\0';
    }
  else
    {
      strncpy (hold_tar_filename, prefix, TARPREFIXSIZE);
      hold_tar_filename[TARPREFIXSIZE] = '\0';
      strcat (hold_tar_filename, "/");
      strncat (hold_tar_filename, filename, TARNAMESIZE);
      hold_tar_filename[TARPREFIXSIZE + TARNAMESIZE] = '\0';
    }
  return hold_tar_filename;
}

/* Convert the string of octal digits S into a number and store
   it in *N.  Return nonzero if the whole string was converted,
   zero if there was something after the number.
   Skip leading and trailing spaces.  */

int
otoa (s, n)
     char *s;
     unsigned long *n;
{
  unsigned long val = 0;

  while (*s == ' ')
    ++s;
  while (*s >= '0' && *s <= '7')
    val = 8 * val + *s++ - '0';
  while (*s == ' ')
    ++s;
  *n = val;
  return *s == '\0';
}

/* Convert a number into a string of octal digits.
   Convert long VALUE into a DIGITS-digit field at WHERE,
   including a trailing space and room for a NUL.  DIGITS==3 means
   1 digit, a space, and room for a NUL.

   We assume the trailing NUL is already there and don't fill it in.
   This fact is used by start_header and finish_header, so don't change it!

   This is be equivalent to:
   sprintf (where, "%*lo ", digits - 2, value);
   except that sprintf fills in the trailing NUL and we don't.  */

static void
to_oct (value, digits, where)
     register long value;
     register int digits;
     register char *where;
{
  --digits;			/* Leave the trailing NUL slot alone.  */
  where[--digits] = ' ';	/* Put in the space, though.  */

  /* Produce the digits -- at least one.  */
  do
    {
      where[--digits] = '0' + (char) (value & 7); /* One octal digit.  */
      value >>= 3;
    }
  while (digits > 0 && value != 0);

  /* Add leading spaces, if necessary.  */
  while (digits > 0)
    where[--digits] = ' ';
}

/* Return
   2 if BUF is a valid POSIX tar header (the checksum is correct
   and it has the "ustar" magic string),
   1 if BUF is a valid old tar header (the checksum is correct),
   0 otherwise.  */

int
is_tar_header (buf)
     char *buf;
{
  struct tar_header *tar_hdr = (struct tar_header *) buf;
  unsigned long chksum;

  otoa (tar_hdr->chksum, &chksum);

  if (chksum != tar_checksum (tar_hdr))
    return 0;

  /* GNU tar 1.10 and previous set the magic field to be "ustar " instead
     of "ustar\0".  Only look at the first 5 characters of the magic
     field so we can recognize old GNU tar ustar archives.  */
  if (!strncmp (tar_hdr->magic, TMAGIC, TMAGLEN - 1))
      return 2;
  return 1;
}

/* Return TRUE if the filename is too long to fit in a tar header.
   For old tar headers, if the filename's length is less than or equal
   to 100 then it will fit, otherwise it will not.  For POSIX tar headers,
   if the filename's length is less than or equal to 100 then it
   will definitely fit, and if it is greater than 256 then it
   will definitely not fit.  If the length is between 100 and 256,
   then the filename will fit only if it is possible to break it
   into a 155 character "prefix" and 100 character "name".  There
   must be a slash between the "prefix" and the "name", although
   the slash is not stored or counted in either the "prefix" or
   the "name", and there must be at least one character in both
   the "prefix" and the "name".  If it is not possible to break down
   the filename like this then it will not fit.  */

int
is_tar_filename_too_long (name)
     char *name;
{
  int whole_name_len;
  int prefix_name_len;
  char *p;

  whole_name_len = strlen (name);
  if (whole_name_len <= TARNAMESIZE)
    return FALSE;

  if (archive_format != arf_ustar)
    return TRUE;

  if (whole_name_len > TARNAMESIZE + TARPREFIXSIZE + 1)
    return TRUE;

  /* See whether we can split up the name into acceptably-sized
     `prefix' and `name' (`p') pieces.  Start out by making `name'
     as big as possible, then shrink it by looking for the first '/'.  */
  p = name + whole_name_len - TARNAMESIZE;
  while (*p != '/' && *p != '\0')
    ++p;
  if (*p == '\0')
    /* The last component of the path is longer than TARNAMESIZE.  */
    return TRUE;

  prefix_name_len = p - name - 1;
  /* Interestingly, a name consisting of a slash followed by
     TARNAMESIZE characters can't be stored, because the prefix
     would be empty, and thus ignored.  */
  if (prefix_name_len > TARPREFIXSIZE || prefix_name_len == 0)
    return TRUE;

  return FALSE;
}
