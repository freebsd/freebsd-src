/* error.c -- error handler for noninteractive utilities
   Copyright (C) 1990-1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* David MacKenzie */
/* Brian Berliner added support for CVS */

#include "cvs.h"

#include <stdio.h>

/* If non-zero, error will use the CVS protocol to stdout to report error
   messages.  This will only be set in the CVS server parent process;
   most other code is run via do_cvs_command, which forks off a child
   process and packages up its stderr in the protocol.  */
int error_use_protocol; 

#ifdef HAVE_VPRINTF

#ifdef __STDC__
#include <stdarg.h>
#define VA_START(args, lastarg) va_start(args, lastarg)
#else /* ! __STDC__ */
#include <varargs.h>
#define VA_START(args, lastarg) va_start(args)
#endif /* __STDC__ */

#else /* ! HAVE_VPRINTF */ 

#ifdef HAVE_DOPRNT
#define va_alist args
#define va_dcl int args;
#else /* ! HAVE_DOPRNT */
#define va_alist a1, a2, a3, a4, a5, a6, a7, a8
#define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif /* HAVE_DOPRNT */

#endif /* HAVE_VPRINTF */ 

#if STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else /* ! STDC_HEADERS */
#ifdef __STDC__
void exit(int status);
#else /* ! __STDC__ */
void exit ();
#endif /* __STDC__ */
#endif /* STDC_HEADERS */

#ifndef strerror
extern char *strerror ();
#endif

void
error_exit PROTO ((void))
{
    Lock_Cleanup();
#ifdef SERVER_SUPPORT
    if (server_active)
	server_cleanup (0);
#endif
#ifdef SYSTEM_CLEANUP
    /* Hook for OS-specific behavior, for example socket subsystems on
       NT and OS2 or dealing with windows and arguments on Mac.  */
    SYSTEM_CLEANUP ();
#endif
    exit (EXIT_FAILURE);
}

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args.  This is a very limited printf subset:
   %s, %d, %c, %x and %% only (without anything between the % and the s,
   d, &c).  Callers who want something fancier can use sprintf.

   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status EXIT_FAILURE if STATUS is nonzero.  If MESSAGE is "",
   no need to print a message.

   I think this is largely cleaned up to the point where it does the right
   thing for the server, whether the normal server_active (child process)
   case or the error_use_protocol (parent process) case.  The one exception
   is that STATUS nonzero for error_use_protocol probably doesn't work yet;
   in that case still need to use the pending_error machinery in server.c.

   error() does not molest errno; some code (e.g. Entries_Open) depends
   on being able to say something like:
      error (0, 0, "foo");
      error (0, errno, "bar");

   */

/* VARARGS */
void
#if defined (__STDC__)
error (int status, int errnum, const char *message, ...)
#else
error (status, errnum, message, va_alist)
    int status;
    int errnum;
    const char *message;
    va_dcl
#endif
{
    int save_errno = errno;

    if (message[0] != '\0')
    {
	va_list args;
	const char *p;
	char *q;
	char *str;
	int num;
	unsigned int unum;
	int ch;
	unsigned char buf[100];

	cvs_outerr (program_name, 0);
	if (command_name && *command_name)
	{
	    cvs_outerr (" ", 1);
	    if (status != 0)
		cvs_outerr ("[", 1);
	    cvs_outerr (command_name, 0);
	    if (status != 0)
		cvs_outerr (" aborted]", 0);
	}
	cvs_outerr (": ", 2);

	VA_START (args, message);
	p = message;
	while ((q = strchr (p, '%')) != NULL)
	{
	    static const char msg[] =
		"\ninternal error: bad % in error()\n";
	    if (q - p > 0)
		cvs_outerr (p, q - p);

	    switch (q[1])
	    {
	    case 's':
		str = va_arg (args, char *);
		cvs_outerr (str, strlen (str));
		break;
	    case 'd':
		num = va_arg (args, int);
		sprintf (buf, "%d", num);
		cvs_outerr (buf, strlen (buf));
		break;
	    case 'x':
		unum = va_arg (args, unsigned int);
		sprintf (buf, "%x", unum);
		cvs_outerr (buf, strlen (buf));
		break;
	    case 'c':
		ch = va_arg (args, int);
		buf[0] = ch;
		cvs_outerr (buf, 1);
		break;
	    case '%':
		cvs_outerr ("%", 1);
		break;
	    default:
		cvs_outerr (msg, sizeof (msg) - 1);
		/* Don't just keep going, because q + 1 might point to the
		   terminating '\0'.  */
		goto out;
	    }
	    p = q + 2;
	}
	cvs_outerr (p, strlen (p));
    out:
	va_end (args);

	if (errnum != 0)
	{
	    cvs_outerr (": ", 2);
	    cvs_outerr (strerror (errnum), 0);
	}
	cvs_outerr ("\n", 1);
    }

    if (status)
	error_exit ();
    errno = save_errno;
}

/* Print the program name and error message MESSAGE, which is a printf-style
   format string with optional args to the file specified by FP.
   If ERRNUM is nonzero, print its corresponding system error message.
   Exit with status EXIT_FAILURE if STATUS is nonzero.  */
/* VARARGS */
void
#if defined (HAVE_VPRINTF) && defined (__STDC__)
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
	error_exit ();
}
