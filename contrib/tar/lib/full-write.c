/* full-write.c -- an interface to write that retries after interrupts

   Copyright 1993, 1994, 1997, 1998, 1999, 2000, 2001 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Paul Eggert.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#include "full-write.h"

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

/* Write LEN bytes at PTR to descriptor DESC, retrying if interrupted
   or if partial writes occur.  Return the number of bytes successfully
   written, setting errno if that is less than LEN.  */

size_t
full_write (int desc, const char *ptr, size_t len)
{
  size_t total_written = 0;

  while (len > 0)
    {
      ssize_t written = write (desc, ptr, len);
      if (written <= 0)
	{
	  /* Some buggy drivers return 0 when you fall off a device's end.  */
	  if (written == 0)
	    errno = ENOSPC;
#ifdef EINTR
	  if (errno == EINTR)
	    continue;
#endif
	  break;
	}
      total_written += written;
      ptr += written;
      len -= written;
    }
  return total_written;
}
