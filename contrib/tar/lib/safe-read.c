/* safe-read.c -- an interface to read that retries after interrupts
   Copyright (C) 1993, 1994, 1998 Free Software Foundation, Inc.

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
   */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include "safe-read.h"

/* Read LEN bytes at PTR from descriptor DESC, retrying if interrupted.
   Return the actual number of bytes read, zero for EOF, or negative
   for an error.  */

ssize_t
safe_read (int desc, void *ptr, size_t len)
{
  ssize_t n_chars;

  if (len <= 0)
    return len;

#ifdef EINTR
  do
    {
      n_chars = read (desc, ptr, len);
    }
  while (n_chars < 0 && errno == EINTR);
#else
  n_chars = read (desc, ptr, len);
#endif

  return n_chars;
}
