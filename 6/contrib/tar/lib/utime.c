/* Copyright (C) 1998, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* derived from a function in touch.c */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#undef utime

#include <sys/types.h>

#ifdef HAVE_UTIME_H
# include <utime.h>
#endif

#include "full-write.h"
#include "safe-read.h"

/* Some systems (even some that do have <utime.h>) don't declare this
   structure anywhere.  */
#ifndef HAVE_STRUCT_UTIMBUF
struct utimbuf
{
  long actime;
  long modtime;
};
#endif

/* Emulate utime (file, NULL) for systems (like 4.3BSD) that do not
   interpret it to set the access and modification times of FILE to
   the current time.  Return 0 if successful, -1 if not. */

static int
utime_null (const char *file)
{
#if HAVE_UTIMES_NULL
  return utimes (file, 0);
#else
  int fd;
  char c;
  int status = 0;
  struct stat sb;

  fd = open (file, O_RDWR);
  if (fd < 0
      || fstat (fd, &sb) < 0
      || safe_read (fd, &c, sizeof c) < 0
      || lseek (fd, (off_t) 0, SEEK_SET) < 0
      || full_write (fd, &c, sizeof c) != sizeof c
      /* Maybe do this -- it's necessary on SunOS4.1.3 with some combination
	 of patches, but that system doesn't use this code: it has utimes.
	 || fsync (fd) < 0
      */
      || ftruncate (fd, st.st_size) < 0
      || close (fd) < 0)
    status = -1;
  return status;
#endif
}

int
rpl_utime (const char *file, const struct utimbuf *times)
{
  if (times)
    return utime (file, times);

  return utime_null (file);
}
