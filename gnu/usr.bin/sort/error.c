/* error.c -- error handler for noninteractive utilities
   Copyright (C) 1990, 91, 92, 93, 94, 95 Free Software Foundation, Inc.

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

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>.  */
/* $FreeBSD$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#if HAVE_VPRINTF || HAVE_DOPRNT || _LIBC
# if __STDC__
#  include <stdarg.h>
#  define VA_START(args, lastarg) va_start(args, lastarg)
# else
#  include <varargs.h>
#  define VA_START(args, lastarg) va_start(args)
# endif
#else
# define va_alist a1, a2, a3, a4, a5, a6, a7, a8
# define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif

#if STDC_HEADERS || _LIBC
# include <stdlib.h>
# include <string.h>
#else
void exit ();
#endif

/* This variable is incremented each time `error' is called.  */
unsigned int error_message_count;

/* If NULL, error will flush stdout, then print on stderr the program
   name, a colon and a space.  Otherwise, error will call this
   function without parameters instead.  */
void (*error_print_progname) () = NULL;

#ifdef _LIBC
#define program_name program_invocation_name
#endif

/* The calling program should define program_name and set it to the
   name of the executing program.  */
extern char *program_name;

#if HAVE_STRERROR || _LIBC
# ifndef strerror		/* On some systems, strerror is a macro */
char *strerror ();
# endif
#else
static char *
private_strerror (errnum)
     int errnum;
{
  extern int sys_nerr;

  if (errnum > 0 && errnum <= sys_nerr)
    return sys_errlist[errnum];
  return "Unknown system error";
}
#define strerror private_strerror
#endif

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status STATUS if it is nonzero.  */
/* VARARGS */

void
#if defined(VA_START) && __STDC__
error (int status, int errnum, const char *message, ...)
#else
error (status, errnum, message, va_alist)
     int status;
     int errnum;
     char *message;
     va_dcl
#endif
{
#ifdef VA_START
  va_list args;
#endif

  if (error_print_progname)
    (*error_print_progname) ();
  else
    {
      fflush (stdout);
      fprintf (stderr, "%s: ", program_name);
    }

#ifdef VA_START
  VA_START (args, message);
# if HAVE_VPRINTF || _LIBC
  vfprintf (stderr, message, args);
# else
  _doprnt (message, args, stderr);
# endif
  va_end (args);
#else
  fprintf (stderr, message, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

  ++error_message_count;

  if (errnum)
    fprintf (stderr, ": %s", strerror (errnum));
  putc ('\n', stderr);
  fflush (stderr);
  if (status)
    exit (status);
}
