/* getwd.c -- get current working directory pathname
   Copyright (C) 1992 Free Software Foundation, Inc.

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

/* Some systems which include both getwd() and getcwd() have an implementation
   of getwd() which is much faster than getcwd().  As a result, we use the
   system's getwd() if it is available */

#include "system.h"

/* Get the current working directory into PATHNAME */

char *
getwd (pathname)
     char *pathname;
{
  return (getcwd(pathname, PATH_MAX));
}
