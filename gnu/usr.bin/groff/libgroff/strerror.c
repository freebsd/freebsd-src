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

#include <stdio.h>

#define INT_DIGITS 19		/* enough for 64 bit integer */

extern int sys_nerr;
extern char *sys_errlist[];

char *strerror(n)
     int n;
{
  static char buf[sizeof("Error ") + 1 + INT_DIGITS];
  if (n >= 0 && n < sys_nerr && sys_errlist[n] != 0)
    return sys_errlist[n];
  else {
    sprintf(buf, "Error %d", n);
    return buf;
  }
}
