/* Interface header file for GNU DIFF library.
   Copyright (C) 1998 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef DIFFRUN_H
#define DIFFRUN_H

/* This header file defines the interfaces used by the diff library.
   It should be included by programs which use the diff library.  */

#include <sys/types.h>

#if defined __STDC__ && __STDC__
#define DIFFPARAMS(args) args
#else
#define DIFFPARAMS(args) ()
#endif

/* The diff_callbacks structure is used to handle callbacks from the
   diff library.  All output goes through these callbacks.  When a
   pointer to this structure is passed in, it may be NULL.  Also, any
   of the individual callbacks may be NULL.  This means that the
   default action should be taken.  */

struct diff_callbacks
{
  /* Write output.  This function just writes a string of a given
     length to the output file.  The default is to fwrite to OUTFILE.
     If this callback is defined, flush_output must also be defined.
     If the length is zero, output zero bytes.  */
  void (*write_output) DIFFPARAMS((char const *, size_t));
  /* Flush output.  The default is to fflush OUTFILE.  If this
     callback is defined, write_output must also be defined.  */
  void (*flush_output) DIFFPARAMS((void));
  /* Write a '\0'-terminated string to stdout.
     This is called for version and help messages.  */
  void (*write_stdout) DIFFPARAMS((char const *));
  /* Print an error message.  The first argument is a printf format,
     and the next two are parameters.  The default is to print a
     message on stderr.  */
  void (*error) DIFFPARAMS((char const *, char const *, char const *));
};

/* Run a diff.  */

extern int diff_run DIFFPARAMS((int, char **, const char *,
				const struct diff_callbacks *));

/* Run a diff3.  */

extern int diff3_run DIFFPARAMS((int, char **, char *,
				 const struct diff_callbacks *));

#undef DIFFPARAMS

#endif /* DIFFRUN_H */
