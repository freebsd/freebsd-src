/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef LONG_MAX
#define LONG_MAX  2147483647
#endif

#ifndef LONG_MIN
#define LONG_MIN (-LONG_MAX-1)
#endif

long strtol(str, ptr, base)
     char *str, **ptr;
     int base;
{
  char *start = str;
  int neg = 0;
  long val;
  char *p;
  static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  while (isascii(*str) && isspace(*str))
    str++;

  if (*str == '-') {
    neg = 1;
    str++;
  }
  if (base == 0) {
    if (*str == '0') {
      if (str[1] == 'x' || str[1] == 'X') {
	str += 2;
	base = 16;
      }
      else
	base = 8;
    }
    else
      base = 10;
  }
  if (base < 2 || base > 36)
    base = 10;
  else if (base == 16 && *str == '0' && (str[1] == 'x' || str[1] == 'X'))
    str += 2;

  p = strchr(digits, isascii(*str) && isupper(*str) ? tolower(*str) : *str);
  if (p == 0 || (val = (p - digits)) >= base) {
    if (base == 16 && str > start && (str[-1] == 'x' || str[-1] == 'X')) {
      if (ptr)
	*ptr = str - 1;
    }
    else {
      if (ptr)
	*ptr = start;
      errno = ERANGE;
    }
    return 0;
  }
  if (neg)
    val = -val;
    
  while (*++str != '\0') {
    int n;

    p = strchr(digits, isascii(*str) && isupper(*str) ? tolower(*str) : *str);
    if (p == 0)
      break;
    n = p - digits;
    if (n >= base)
      break;
    if (neg) {
      if (-(unsigned long)val > (-(unsigned long)LONG_MIN - n)/base) {
	val = LONG_MIN;
	errno = ERANGE;
      }
      else
	val = val*base - n;
    }
    else {
      if (val > (LONG_MAX - n)/base) {
	val = LONG_MAX;
	errno = ERANGE;
      }
      else
	val = val*base + n;
    }
  }
  
  if (ptr)
    *ptr = str;

  return val;
}
