/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001
   Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MMAP

#include <sys/types.h>
#include <sys/mman.h>

/* The Net-2 man pages says that a MAP_FILE flag is required. */
#ifndef MAP_FILE
#define MAP_FILE 0
#endif

char *mapread(fd, nbytes)
     int fd;
     int nbytes;
{
  char *p = (char *)mmap((caddr_t)0, (size_t)nbytes, PROT_READ,
			 MAP_FILE|MAP_PRIVATE, fd, (off_t)0);
  if (p == (char *)-1)
    return 0;
  /* mmap() shouldn't return 0 since MAP_FIXED wasn't specified. */
  if (p == 0)
    abort();
  return p;
}

int unmap(p, len)
     char *p;
     int len;
{
  return munmap((caddr_t)p, len);
}

#else /* not HAVE_MMAP */

#include <errno.h>

char *mapread(fd, nbytes)
     int fd;
     int nbytes;
{
  errno = ENODEV;
  return 0;
}

int unmap(p, len)
     char *p;
     int len;
{
  errno = EINVAL;
  return -1;
}

#endif /* not HAVE_MMAP */
