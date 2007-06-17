/* stripslash.c -- remove redundant trailing slashes from a file name
   Copyright (C) 1990, 2001, 2003 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "dirname.h"

/* Remove trailing slashes from PATH.
   Return nonzero if a trailing slash was removed.
   This is useful when using filename completion from a shell that
   adds a "/" after directory names (such as tcsh and bash), because
   the Unix rename and rmdir system calls return an "Invalid argument" error
   when given a path that ends in "/" (except for the root directory).  */

int
strip_trailing_slashes (char *path)
{
  char *base = base_name (path);
  char *base_lim = base + base_len (base);
  int had_slash = *base_lim;
  *base_lim = '\0';
  return had_slash;
}
