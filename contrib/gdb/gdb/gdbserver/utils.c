/* General utility routines for the remote server for GDB.
   Copyright (C) 1986, 1989, 1993 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "server.h"
#include <stdio.h>
#include <string.h>

/* Generally useful subroutines used throughout the program.  */

/* Print the system error message for errno, and also mention STRING
   as the file name for which the error was encountered.
   Then return to command level.  */

void
perror_with_name (string)
     char *string;
{
  const char *err;
  char *combined;

  if (errno < sys_nerr)
    err = sys_errlist[errno];
  else
    err = "unknown error";

  combined = (char *) alloca (strlen (err) + strlen (string) + 3);
  strcpy (combined, string);
  strcat (combined, ": ");
  strcat (combined, err);

  error ("%s.", combined);
}

/* Print an error message and return to command level.
   STRING is the error message, used as a fprintf string,
   and ARG is passed as an argument to it.  */

#ifdef ANSI_PROTOTYPES
NORETURN void
error (const char *string, ...)
#else
void
error (va_alist)
     va_dcl
#endif
{
  extern jmp_buf toplevel;
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, string);
#else
  va_start (args);
#endif
  fflush (stdout);
#ifdef ANSI_PROTOTYPES
  vfprintf (stderr, string, args);
#else
  {
    char *string1;

    string1 = va_arg (args, char *);
    vfprintf (stderr, string1, args);
  }
#endif
  fprintf (stderr, "\n");
  longjmp(toplevel, 1);
}

/* Print an error message and exit reporting failure.
   This is for a error that we cannot continue from.
   STRING and ARG are passed to fprintf.  */

/* VARARGS */
NORETURN void
#ifdef ANSI_PROTOTYPES
fatal (char *string, ...)
#else
fatal (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, string);
#else
  char *string;
  va_start (args);
  string = va_arg (args, char *);
#endif
  fprintf (stderr, "gdb: ");
  vfprintf (stderr, string, args);
  fprintf (stderr, "\n");
  va_end (args);
  exit (1);
}
