/* grep.h - interface to grep driver for searching subroutines.
   Copyright (C) 1992 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#if __STDC__

extern void fatal(const char *, int);

/* Grep.c expects the matchers vector to be terminated
   by an entry with a NULL name, and to contain at least
   an entry named "default". */

extern struct matcher
{
  char *name;
  void (*compile)(char *, size_t);
  char *(*execute)(char *, size_t, char **);
} matchers[];

#else

extern void fatal();

extern struct matcher
{
  char *name;
  void (*compile)();
  char *(*execute)();
} matchers[];

#endif

/* Exported from grep.c. */
extern char *matcher;

/* The following flags are exported from grep for the matchers
   to look at. */
extern int match_icase;		/* -i */
extern int match_words;		/* -w */
extern int match_lines;		/* -x */
