/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2003
   Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>		/* for MinGW */

#define INT_DIGITS 19		/* enough for 64 bit integer */

#ifndef HAVE_SYS_NERR
extern int sys_nerr;
#endif
#ifndef HAVE_SYS_ERRLIST
extern char *sys_errlist[];
#endif

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
