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

#include "cvs.h"

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)error.c 1.13 94/09/30 $";
USE(rcsid);
#endif /* not lint */

#include <stdio.h>

/* If non-zero, error will use the CVS protocol to stdout to report error
   messages.  This will only be set in the CVS server parent process;
   most other code is run via do_cvs_command, which forks off a child
   process and packages up its stderr in the protocol.  */
int error_use_protocol; 

#ifdef HAVE_VPRINTF

#if __STDC__
#include <stdarg.h>
#define VA_START(args, lastarg) va_start(args, lastarg)
#else
#include <varargs.h>
#define VA_START(args, lastarg) va_start(args)
#endif

#else

#ifdef HAVE_DOPRNT
#define va_alist args
#define va_dcl int args;
#else
#define va_alist a1, a2, a3, a4, a5, a6, a7, a8
#define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif

#endif

#if STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else
#if __STDC__
void exit(int status);
#else
void exit ();
#endif /* __STDC__ */
#endif

extern char *strerror ();

typedef void (*fn_returning_void) ();

/* Function to call before exiting.  */
static fn_returning_void cleanup_fn;

fn_returning_void
error_set_cleanup (arg)
     fn_returning_void arg;
{
  fn_returning_void retval = cleanup_fn;
  cleanup_fn = arg;
  return retval;
}

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status STATUS if it is nonzero. */
/* VARARGS */
void
#if defined (HAVE_VPRINTF) && __STDC__
error (int status, int errnum, const char *message, ...)
#else
error (status, errnum, message, va_alist)
     int status;
     int errnum;
     const char *message;
     va_dcl
#endif
{
  FILE *out = stderr;
  extern char *program_name;
  extern char *command_name;
#ifdef HAVE_VPRINTF
  va_list args;
#endif

  if (error_use_protocol)
    {
      out = stdout;
      printf ("E ");
    }

  if (command_name && *command_name)
    if (status)
      fprintf (out, "%s [%s aborted]: ", program_name, command_name);
    else
      fprintf (out, "%s %s: ", program_name, command_name);
  else
    fprintf (out, "%s: ", program_name);
#ifdef HAVE_VPRINTF
  VA_START (args, message);
  vfprintf (out, message, args);
  va_end (args);
#else
#ifdef HAVE_DOPRNT
  _doprnt (message, &args, out);
#else
  fprintf (out, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif
#endif
  if (errnum)
    fprintf (out, ": %s", strerror (errnum));
  putc ('\n', out);
  fflush (out);
  if (status)
    {
      if (cleanup_fn)
	(*cleanup_fn) ();
      exit (status);
    }
}

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args to the file specified by FP.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status STATUS if it is nonzero. */
/* VARARGS */
void
#if defined (HAVE_VPRINTF) && __STDC__
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
#ifdef HAVE_VPRINTF
  va_list args;
#endif

  fprintf (fp, "%s: ", program_name);
#ifdef HAVE_VPRINTF
  VA_START (args, message);
  vfprintf (fp, message, args);
  va_end (args);
#else
#ifdef HAVE_DOPRNT
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
      if (cleanup_fn)
	(*cleanup_fn) ();
      exit (status);
    }
}
