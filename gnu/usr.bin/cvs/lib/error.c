/* error.c -- error handler for noninteractive utilities
   Copyright (C) 1990-1992 Free Software Foundation, Inc.

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

/* David MacKenzie */
/* Brian Berliner added support for CVS */

#ifndef lint
static char rcsid[] = "@(#)error.c 1.9 92/03/31";
#endif /* not lint */

#include <stdio.h>

/* turn on CVS support by default, since this is the CVS distribution */
#define	CVS_SUPPORT

#ifdef CVS_SUPPORT
#if __STDC__
void Lock_Cleanup(void);
#else
void Lock_Cleanup();
#endif /* __STDC__ */
#endif /* CVS_SUPPORT */

#ifndef VPRINTF_MISSING

#if __STDC__
#include <stdarg.h>
#define VA_START(args, lastarg) va_start(args, lastarg)
#else
#include <varargs.h>
#define VA_START(args, lastarg) va_start(args)
#endif

#else

#ifndef DOPRNT_MISSING
#define va_alist args
#define va_dcl int args;
#else
#define va_alist a1, a2, a3, a4, a5, a6, a7, a8
#define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif

#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else
#if __STDC__
void exit(int status);
#else
void exit ();
#endif /* __STDC__ */
#endif

#ifdef STRERROR_MISSING
static char *
strerror (errnum)
     int errnum;
{
  extern char *sys_errlist[];
  extern int sys_nerr;

  if (errnum > 0 && errnum < sys_nerr)
    return sys_errlist[errnum];
  return "Unknown system error";
}
#endif /* STRERROR_MISSING */

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status STATUS if it is nonzero. */
/* VARARGS */
void
#if !defined (VPRINTF_MISSING) && __STDC__
error (int status, int errnum, char *message, ...)
#else
error (status, errnum, message, va_alist)
     int status;
     int errnum;
     char *message;
     va_dcl
#endif
{
  extern char *program_name;
#ifdef CVS_SUPPORT
  extern char *command_name;
#endif
#ifndef VPRINTF_MISSING
  va_list args;
#endif

#ifdef CVS_SUPPORT
  if (command_name && *command_name)
    if (status)
      fprintf (stderr, "%s [%s aborted]: ", program_name, command_name);
    else
      fprintf (stderr, "%s %s: ", program_name, command_name);
  else
    fprintf (stderr, "%s: ", program_name);
#else
  fprintf (stderr, "%s: ", program_name);
#endif
#ifndef VPRINTF_MISSING
  VA_START (args, message);
  vfprintf (stderr, message, args);
  va_end (args);
#else
#ifndef DOPRNT_MISSING
  _doprnt (message, &args, stderr);
#else
  fprintf (stderr, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif
#endif
  if (errnum)
    fprintf (stderr, ": %s", strerror (errnum));
  putc ('\n', stderr);
  fflush (stderr);
  if (status)
    {
#ifdef CVS_SUPPORT
      Lock_Cleanup();
#endif
      exit (status);
    }
}

#ifdef CVS_SUPPORT

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args to the file specified by FP.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status STATUS if it is nonzero. */
/* VARARGS */
void
#if !defined (VPRINTF_MISSING) && __STDC__
fperror (FILE *fp, int status, int errnum, char *message, ...)
#else
fperror (fp, status, errnum, message, va_alist)
     FILE *fp;
     int status;
     int errnum;
     char *message;
     va_dcl
#endif
{
  extern char *program_name;
#ifndef VPRINTF_MISSING
  va_list args;
#endif

  fprintf (fp, "%s: ", program_name);
#ifndef VPRINTF_MISSING
  VA_START (args, message);
  vfprintf (fp, message, args);
  va_end (args);
#else
#ifndef DOPRNT_MISSING
  _doprnt (message, &args, fp);
#else
  fprintf (fp, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif
#endif
  if (errnum)
    fprintf (fp, ": %s", strerror (errnum));
  putc ('\n', fp);
  fflush (fp);
  if (status)
    {
#ifdef CVS_SUPPORT
      Lock_Cleanup();
#endif
      exit (status);
    }
}

#endif /* CVS_SUPPORT */
