// -*- C++ -*-
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

extern "C" {
#ifndef strerror
  char *strerror(int);
#endif
#ifndef __BORLANDC__
  const char *itoa(int);
  const char *iftoa(int, int);
#endif /* __BORLANDC__ */
}

#ifdef STDLIB_H_DECLARES_GETOPT
#include <stdlib.h>
#else /* not STDLIB_H_DECLARES_GETOPT */
#ifdef UNISTD_H_DECLARES_GETOPT
#include <sys/types.h>
#include <unistd.h>
#else /* not UNISTD_H_DECLARES_GETOPT */
extern "C" {
  int getopt(int, char *const *, const char *);
}
#endif /* not UNISTD_H_DECLARES_GETOPT */

extern "C" {
  extern char *optarg;
  extern int optind;
  extern int opterr;
}

#endif /* not STDLIB_H_DECLARES_GETOPT */

char *strsave(const char *s);
int is_prime(unsigned);

#include <stdio.h>

FILE *xtmpfile();

#ifndef STDIO_H_DECLARES_POPEN

extern "C" { FILE *popen(const char *, const char *); }

#endif /* not STDIO_H_DECLARES_POPEN */

#ifndef STDIO_H_DECLARES_PCLOSE

extern "C" { int pclose (FILE *); }

#endif /* not STDIO_H_DECLARES_PCLOSE */

int interpret_lf_args(const char *p);

extern char illegal_char_table[];

inline int illegal_input_char(int c)
{
  return c >= 0 && illegal_char_table[c];
}

#ifdef HAVE_CC_LIMITS_H
#include <limits.h>
#else /* not HAVE_CC_LIMITS_H */
#define INT_MAX 2147483647
#endif /* not HAVE_CC_LIMITS_H */

/* It's not safe to rely on people getting INT_MIN right (ie signed). */

#ifdef INT_MIN
#undef INT_MIN
#endif

#ifdef CFRONT_ANSI_BUG

/* This works around a bug in cfront 2.0 used with ANSI C compilers. */

#define INT_MIN ((long)(-INT_MAX-1))

#else /* not CFRONT_ANSI_BUG */

#define INT_MIN (-INT_MAX-1)

#endif /* not CFRONT_ANSI_BUG */

/* Maximum number of digits in the decimal representation of an int
(not including the -). */

#define INT_DIGITS 10

#ifdef PI
#undef PI
#endif

const double PI = 3.14159265358979323846;

/* ad_delete deletes an array of objects with destructors;
a_delete deletes an array of objects without destructors */

#ifdef ARRAY_DELETE_NEEDS_SIZE
/* for 2.0 systems */
#define ad_delete(size) delete [size]
#define a_delete delete
#else /* not ARRAY_DELETE_NEEDS_SIZE */
/* for ARM systems */
#define ad_delete(size) delete []
#define a_delete delete []
#endif /* not ARRAY_DELETE_NEEDS_SIZE */
