/* getwd.c -- get current working directory pathname
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* Some systems which include both getwd() and getcwd() have an implementation
   of getwd() which is much faster than getcwd().  As a result, we use the
   system's getwd() if it is available */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "system.h"

/* Get the current working directory into PATHNAME */

char *
getwd (pathname)
     char *pathname;
{
  char *getcwd();

  return (getcwd(pathname, PATH_MAX));
}
