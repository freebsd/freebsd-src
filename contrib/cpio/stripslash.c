/* stripslash.c -- remove trailing slashes from a string
   Copyright (C) 1990 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#else
#include <strings.h>
#endif

/* Remove trailing slashes from PATH.
   This is useful when using filename completion from a shell that
   adds a "/" after directory names (such as tcsh and bash), because
   the Unix rename and rmdir system calls return an "Invalid argument" error
   when given a path that ends in "/" (except for the root directory).  */

void
strip_trailing_slashes (path)
     char *path;
{
  int last;

  last = strlen (path) - 1;
  while (last > 0 && path[last] == '/')
    path[last--] = '\0';
}
