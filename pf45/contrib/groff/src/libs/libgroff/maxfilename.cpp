// -*- C++ -*-
/* Copyright (C) 1992, 2001, 2003, 2005 Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

/* file_name_max(dir) does the same as pathconf(dir, _PC_NAME_MAX) */

#include "lib.h"

#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef _POSIX_VERSION

size_t file_name_max(const char *fname)
{
  return pathconf(fname, _PC_NAME_MAX);
}

#else /* not _POSIX_VERSION */

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#else /* not HAVE_DIRENT_H */
#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif /* HAVE_SYS_DIR_H */
#endif /* not HAVE_DIRENT_H */

#ifndef NAME_MAX
#ifdef MAXNAMLEN
#define NAME_MAX MAXNAMLEN
#else /* !MAXNAMLEN */
#ifdef MAXNAMELEN
#define NAME_MAX MAXNAMELEN
#else /* !MAXNAMELEN */
#define NAME_MAX 14
#endif /* !MAXNAMELEN */
#endif /* !MAXNAMLEN */
#endif /* !NAME_MAX */

size_t file_name_max(const char *)
{
  return NAME_MAX;
}

#endif /* not _POSIX_VERSION */
