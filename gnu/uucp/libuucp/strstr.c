/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

This file was modified slightly by Ian Lance Taylor, May 1992, for
Taylor UUCP.  */

#include "uucp.h"

/* Return the first ocurrence of NEEDLE in HAYSTACK.  */
char *
strstr (haystack, needle)
     const char *const haystack;
     const char *const needle;
{
  register const char *const needle_end = strchr(needle, '\0');
  register const char *const haystack_end = strchr(haystack, '\0');
  register const size_t needle_len = needle_end - needle;
  register const size_t needle_last = needle_len - 1;
  register const char *begin;

  if (needle_len == 0)
    return (char *) haystack_end;
  if ((size_t) (haystack_end - haystack) < needle_len)
    return NULL;

  for (begin = &haystack[needle_last]; begin < haystack_end; ++begin)
    {
      register const char *n = &needle[needle_last];
      register const char *h = begin;
      do
	if (*h != *n)
	  goto loop;
      while (--n >= needle && --h >= haystack);

      return (char *) h;
    loop:;
    }

  return NULL;
}
