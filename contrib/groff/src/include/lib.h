// -*- C++ -*-
/* Copyright (C) 1989-2000, 2001, 2002 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

extern "C" {
#ifndef HAVE_STRERROR
  char *strerror(int);
#endif
  const char *i_to_a(int);
  const char *ui_to_a(unsigned int);
  const char *if_to_a(int, int);
}

/* stdio.h on IRIX, OSF/1, emx, and UWIN include getopt.h */
/* unistd.h on CYGWIN includes getopt.h */

#if !(defined(__sgi) \
      || (defined(__osf__) && defined(__alpha)) \
      || defined(_UWIN) \
      || defined(__EMX__) \
      || defined(__CYGWIN__))
#include <groff-getopt.h>
#else
#include <getopt.h>
#endif

char *strsave(const char *s);
int is_prime(unsigned);

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifndef HAVE_SNPRINTF
#include <stdarg.h>
extern "C" {
  int snprintf(char *, size_t, const char *, /*args*/ ...);
  int vsnprintf(char *, size_t, const char *, va_list);
}
#endif

#ifndef HAVE_MKSTEMP
/* since mkstemp() is defined as a real C++ function if taken from
   groff's mkstemp.cc we need a declaration */
int mkstemp(char *tmpl);
#endif /* HAVE_MKSTEMP */

int mksdir(char *tmpl);

FILE *xtmpfile(char **namep = 0,
	       const char *postfix_long = 0, const char *postfix_short = 0,
	       int do_unlink = 1);
char *xtmptemplate(const char *postfix_long, const char *postfix_short);

#ifdef NEED_DECLARATION_POPEN
extern "C" { FILE *popen(const char *, const char *); }
#endif /* NEED_DECLARATION_POPEN */

#ifdef NEED_DECLARATION_PCLOSE
extern "C" { int pclose (FILE *); }
#endif /* NEED_DECLARATION_PCLOSE */

size_t file_name_max(const char *fname);

int interpret_lf_args(const char *p);

extern char invalid_char_table[];

inline int invalid_input_char(int c)
{
  return c >= 0 && invalid_char_table[c];
}

#ifdef HAVE_STRCASECMP
#ifdef NEED_DECLARATION_STRCASECMP
extern "C" {
  // Ultrix4.3's string.h fails to declare this.
  int strcasecmp(const char *, const char *);
}
#endif /* NEED_DECLARATION_STRCASECMP */
#endif /* HAVE_STRCASECMP */

#if !defined(_AIX) && !defined(sinix) && !defined(__sinix__)
#ifdef HAVE_STRNCASECMP
#ifdef NEED_DECLARATION_STRNCASECMP
extern "C" {
  // SunOS's string.h fails to declare this.
  int strncasecmp(const char *, const char *, int);
}
#endif /* NEED_DECLARATION_STRNCASECMP */
#endif /* HAVE_STRNCASECMP */
#endif /* !_AIX && !sinix && !__sinix__ */

#ifndef HAVE_STRCASECMP
#define strcasecmp(a,b) strcmp((a),(b))
#endif

#ifndef HAVE_STRNCASECMP
#define strncasecmp(a,b,c) strncmp((a),(b),(c))
#endif

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
