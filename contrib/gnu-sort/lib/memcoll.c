/* Locale-specific memory comparison.
   Copyright 1999, 2002 Free Software Foundation, Inc.

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

/* Contributed by Paul Eggert <eggert@twinsun.com>.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include <sys/types.h>

#if HAVE_STRING_H
# include <string.h>
#endif

/* Compare S1 (with length S1LEN) and S2 (with length S2LEN) according
   to the LC_COLLATE locale.  S1 and S2 do not overlap, and are not
   adjacent.  Temporarily modify the bytes after S1 and S2, but
   restore their original contents before returning.  Set errno to an
   error number if there is an error, and to zero otherwise.  */
int
memcoll (char *s1, size_t s1len, char *s2, size_t s2len)
{
  int diff;
  char n1 = s1[s1len];
  char n2 = s2[s2len];

  s1[s1len++] = '\0';
  s2[s2len++] = '\0';

  while (! (errno = 0, (diff = strcoll (s1, s2)) || errno))
    {
      /* strcoll found no difference, but perhaps it was fooled by NUL
	 characters in the data.  Work around this problem by advancing
	 past the NUL chars.  */
      size_t size1 = strlen (s1) + 1;
      size_t size2 = strlen (s2) + 1;
      s1 += size1;
      s2 += size2;
      s1len -= size1;
      s2len -= size2;

      if (s1len == 0)
	{
	  if (s2len != 0)
	    diff = -1;
	  break;
	}
      else if (s2len == 0)
	{
	  diff = 1;
	  break;
	}
    }

  s1[s1len - 1] = n1;
  s2[s2len - 1] = n2;

  return diff;
}
