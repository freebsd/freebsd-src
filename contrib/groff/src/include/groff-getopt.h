// -*- C++ -*-
/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
     Written by Werner Lemberg (wl@gnu.org)

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

/*
   This file has to be included from within lib.h instead of getopt.h
   to avoid problems with picky C++ compilers.
*/

#ifndef _GROFF_GETOPT_H
#define _GROFF_GETOPT_H

#ifdef __cplusplus
extern "C" {
#endif

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

struct option
{
  const char *name;
  int has_arg;
  int *flag;
  int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

extern int getopt(int, 			// __argc
		  char *const *,	// __argv
		  const char *);	// __shortopts
extern int getopt_long(int,			// __argc
		       char *const *,		// __argv
		       const char *,		// __shortopts
		       const struct option *,	// __longopts
		       int *);			// __longind
extern int getopt_long_only(int, 			// __argc
			    char *const *,		// __argv
			    const char *,		// __shortopts
			    const struct option *,	// __longopts
			    int *);			// __longind

#ifdef __cplusplus
}
#endif

#endif /* _GROFF_GETOPT_H */
