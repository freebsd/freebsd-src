/* Extended cpio header from POSIX.1.
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

#ifndef _CPIOHDR_H

#define _CPIOHDR_H 1

#include <cpio.h>

struct old_cpio_header
{
  unsigned short c_magic;
  short c_dev;
  unsigned short c_ino;
  unsigned short c_mode;
  unsigned short c_uid;
  unsigned short c_gid;
  unsigned short c_nlink;
  short c_rdev;
  unsigned short c_mtimes[2];
  unsigned short c_namesize;
  unsigned short c_filesizes[2];
  unsigned long c_mtime;	/* Long-aligned copy of `c_mtimes'. */
  unsigned long c_filesize;	/* Long-aligned copy of `c_filesizes'. */
  char *c_name;
};

/* "New" portable format and CRC format:

   Each file has a 110 byte header,
   a variable length, NUL terminated filename,
   and variable length file data.
   A header for a filename "TRAILER!!!" indicates the end of the archive.  */

/* All the fields in the header are ISO 646 (approximately ASCII) strings
   of hexadecimal numbers, left padded, not NUL terminated.

   Field Name	Length in Bytes	Notes
   c_magic	6		"070701" for "new" portable format
				"070702" for CRC format
   c_ino	8
   c_mode	8
   c_uid	8
   c_gid	8
   c_nlink	8
   c_mtime	8
   c_filesize	8		must be 0 for FIFOs and directories
   c_maj	8
   c_min	8
   c_rmaj	8		only valid for chr and blk special files
   c_rmin	8		only valid for chr and blk special files
   c_namesize	8		count includes terminating NUL in pathname
   c_chksum	8		0 for "new" portable format; for CRC format
				the sum of all the bytes in the file  */

struct new_cpio_header
{
  unsigned short c_magic;
  unsigned long c_ino;
  unsigned long c_mode;
  unsigned long c_uid;
  unsigned long c_gid;
  unsigned long c_nlink;
  unsigned long c_mtime;
  unsigned long c_filesize;
  long c_dev_maj;
  long c_dev_min;
  long c_rdev_maj;
  long c_rdev_min;
  unsigned long c_namesize;
  unsigned long c_chksum;
  char *c_name;
  char *c_tar_linkname;
};

#endif /* cpiohdr.h */
