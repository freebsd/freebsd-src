/* declarations for getopt
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

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

/* @(#)getopt.h 1.6 92/03/31 */

/* For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

extern char *optarg;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `getopt'.

   On entry to `getopt', zero means this is the first call; initialize.

   When `getopt' returns EOF, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `optind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

extern int optind;

/* Callers store zero here to inhibit the error message `getopt' prints
   for unrecognized options.  */

extern int opterr;

/* Describe the long-named options requested by the application.
   _GETOPT_LONG_OPTIONS is a vector of `struct option' terminated by an
   element containing a name which is zero.

   The field `has_arg' is:
   0 if the option does not take an argument,
   1 if the option requires an argument,
   2 if the option takes an optional argument.

   If the field `flag' is nonzero, it points to a variable that is set
   to the value given in the field `val' when the option is found, but
   left unchanged if the option is not found.

   To have a long-named option do something other than set an `int' to
   a compiled-in constant, such as set a value from `optarg', set the
   option's `flag' field to zero and its `val' field to a nonzero
   value (the equivalent single-letter option character, if there is
   one).  For long options that have a zero `flag' field, `getopt'
   returns the contents of the `val' field.  */

struct option
{
  char *name;
  int has_arg;
  int *flag;
  int val;
};

#if __STDC__
extern const struct option *_getopt_long_options;
#else
extern struct option *_getopt_long_options;
#endif

/* If nonzero, '-' can introduce long-named options.
   Set by getopt_long_only.  */

extern int _getopt_long_only;

/* The index in GETOPT_LONG_OPTIONS of the long-named option found.
   Only valid when a long-named option has been found by the most
   recent call to `getopt'.  */

extern int option_index;

#if __STDC__
int gnu_getopt (int argc, char **argv, const char *shortopts);
int gnu_getopt_long (int argc, char **argv, const char *shortopts,
		     const struct option *longopts, int *longind);
int gnu_getopt_long_only (int argc, char **argv, const char *shortopts,
			  const struct option *longopts, int *longind);
#else
int gnu_getopt ();
int gnu_getopt_long ();
int gnu_getopt_long_only ();
#endif
