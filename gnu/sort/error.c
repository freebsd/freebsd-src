/* error.c -- error handler for noninteractive utilities
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

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

/* Written by David MacKenzie.  */

#include <stdio.h>

#ifdef HAVE_VPRINTF

#if __STDC__
#include <stdarg.h>
#define VA_START(args, lastarg) va_start(args, lastarg)
#else /* !__STDC__ */
#include <varargs.h>
#define VA_START(args, lastarg) va_start(args)
#endif /* !__STDC__ */

#else /* !HAVE_VPRINTF */

#ifdef HAVE_DOPRNT
#define va_alist args
#define va_dcl int args;
#else /* !HAVE_DOPRNT */
#define va_alist a1, a2, a3, a4, a5, a6, a7, a8
#define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif /* !HAVE_DOPRNT */

#endif /* !HAVE_VPRINTF */

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else /* !STDC_HEADERS */
void exit ();
#endif /* !STDC_HEADERS */

extern char *program_name;

#ifndef HAVE_STRERROR
static char *
private_strerror (errnum)
     int errnum;
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum > 0 && errnum <= sys_nerr)
    return sys_errlist[errnum];
  return "Unknown system error";
}
#define strerror private_strerror
#endif /* !HAVE_STRERROR */

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status STATUS if it is nonzero.  */
/* VARARGS */
void
#if defined (HAVE_VPRINTF) && __STDC__
error (int status, int errnum, char *message, ...)
#else /* !HAVE_VPRINTF or !__STDC__ */
error (status, errnum, message, va_alist)
     int status;
     int errnum;
     char *message;
     va_dcl
#endif /* !HAVE_VPRINTF or !__STDC__ */
{
#ifdef HAVE_VPRINTF
  va_list args;
#endif /* HAVE_VPRINTF */

  fprintf (stderr, "%s: ", program_name);
#ifdef HAVE_VPRINTF
  VA_START (args, message);
  vfprintf (stderr, message, args);
  va_end (args);
#else /* !HAVE_VPRINTF */
#ifdef HAVE_DOPRNT
  _doprnt (message, &args, stderr);
#else /* !HAVE_DOPRNT */
  fprintf (stderr, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* !HAVE_DOPRNT */
#endif /* !HAVE_VPRINTF */
  if (errnum)
    fprintf (stderr, ": %s", strerror (errnum));
  putc ('\n', stderr);
  fflush (stderr);
  if (status)
    exit (status);
}
