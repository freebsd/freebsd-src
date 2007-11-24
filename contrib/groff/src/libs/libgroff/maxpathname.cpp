// -*- C++ -*-
/* Copyright (C) 2005 Free Software Foundation, Inc.
     Written by Werner Lemberg (wl@gnu.org)

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

/* path_name_max(dir) does the same as pathconf(dir, _PC_PATH_MAX) */

#include "lib.h"

#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef _POSIX_VERSION

size_t path_name_max()
{
  return pathconf("/", _PC_PATH_MAX) < 1 ? 1024 : pathconf("/",_PC_PATH_MAX);
}

#else /* not _POSIX_VERSION */

#include <stdlib.h>

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else /* not HAVE_DIRENT_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYS_DIR_H */
#endif /* not HAVE_DIRENT_H */

#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX MAXPATHLEN
# else /* !MAXPATHLEN */
#  ifdef MAX_PATH
#   define PATH_MAX MAX_PATH
#  else /* !MAX_PATH */
#   ifdef _MAX_PATH
#    define PATH_MAX _MAX_PATH
#   else /* !_MAX_PATH */
#    define PATH_MAX 255
#   endif /* !_MAX_PATH */
#  endif /* !MAX_PATH */
# endif /* !MAXPATHLEN */
#endif /* !PATH_MAX */

size_t path_name_max()
{
  return PATH_MAX;
}

#endif /* not _POSIX_VERSION */
