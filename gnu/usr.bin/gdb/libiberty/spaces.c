/* Allocate memory region filled with spaces.
   Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/*

NAME

	spaces -- return a pointer to a buffer full of spaces

SYNOPSIS

	char *spaces (int count)

DESCRIPTION

	Returns a pointer to a memory region filled with the specified
	number of spaces and null terminated.  The returned pointer is
	valid until at least the next call.
	
BUGS

*/


char *
spaces (count)
  int count;
{
  register char *t;
  static char *buf;
  static int maxsize;
  extern char *malloc ();
  extern void free ();

  if (count > maxsize)
    {
      if (buf)
	{
	  free (buf);
	}
      buf = malloc (count + 1);
      for (t = buf + count ; t != buf ; )
	{
	  *--t = ' ';
	}
      maxsize = count;
      buf[count] = '\0';
    }
  return (buf + maxsize - count);
}

