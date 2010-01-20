/* Extended cpio header from POSIX.1.
   Copyright (C) 1992, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.  */

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
};

struct old_ascii_header
{
  char c_magic[6];
  char c_dev[6];
  char c_ino[6];
  char c_mode[6];
  char c_uid[6];
  char c_gid[6];
  char c_nlink[6];
  char c_rdev[6];
  char c_mtime[11];
  char c_namesize[6];
  char c_filesize[11];
};

/* "New" portable format and CRC format:

   Each file has a 110 byte header,
   a variable length, NUL terminated filename,
   and variable length file data.
   A header for a filename "TRAILER!!!" indicates the end of the archive.  */

/* All the fields in the header are ISO 646 (approximately ASCII) strings
   of hexadecimal numbers, left padded, not NUL terminated: */

struct new_ascii_header
{
  char c_magic[6];     /* "070701" for "new" portable format
			  "070702" for CRC format */
  char c_ino[8];
  char c_mode[8];
  char c_uid[8];
  char c_gid[8];
  char c_nlink[8];
  char c_mtime[8];
  char c_filesize[8];  /* must be 0 for FIFOs and directories */
  char c_dev_maj[8];
  char c_dev_min[8];
  char c_rdev_maj[8];      /* only valid for chr and blk special files */
  char c_rdev_min[8];      /* only valid for chr and blk special files */
  char c_namesize[8];  /* count includes terminating NUL in pathname */
  char c_chksum[8];    /* 0 for "new" portable format; for CRC format
			  the sum of all the bytes in the file  */
};

struct cpio_file_stat /* Internal representation of a CPIO header */
{
  unsigned short c_magic;
  ino_t c_ino;
  mode_t c_mode;
  uid_t c_uid;
  gid_t c_gid;
  size_t c_nlink;
  time_t c_mtime;
  off_t c_filesize;
  long c_dev_maj;
  long c_dev_min;
  long c_rdev_maj;
  long c_rdev_min;
  size_t c_namesize;
  unsigned long c_chksum;
  char *c_name;
  char *c_tar_linkname;
};


#endif /* cpiohdr.h */
