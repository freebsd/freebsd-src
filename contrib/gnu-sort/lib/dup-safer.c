/* Invoke dup, but avoid some glitches.
   Copyright (C) 2001 Free Software Foundation, Inc.

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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

#include <unistd-safer.h>

/* Like dup, but do not return STDIN_FILENO, STDOUT_FILENO, or
   STDERR_FILENO.  */

int
dup_safer (int fd)
{
#ifdef F_DUPFD
  return fcntl (fd, F_DUPFD, STDERR_FILENO + 1);
#else
  int f = dup (fd);
  if (0 <= f && f <= STDERR_FILENO)
    {
      int f1 = dup_safer (f);
      int e = errno;
      close (f);
      errno = e;
      f = f1;
    }
  return f;
#endif
}
