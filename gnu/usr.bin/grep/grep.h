/* grep.h - interface to grep driver for searching subroutines.
   Copyright (C) 1992, 1998 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* $FreeBSD$ */

#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 6) || __STRICT_ANSI__
# define __attribute__(x)
#endif

extern void fatal PARAMS ((const char *, int)) __attribute__((noreturn));
extern char *xmalloc PARAMS ((size_t size));
extern char *xrealloc PARAMS ((char *ptr, size_t size));

/* Grep.c expects the matchers vector to be terminated
   by an entry with a NULL name, and to contain at least
   an entry named "default". */

extern struct matcher
{
  char *name;
  void (*compile) PARAMS ((char *, size_t));
  char *(*execute) PARAMS ((char *, size_t, char **));
} matchers[];

/* Exported from fgrepmat.c, egrepmat.c, grepmat.c.  */
extern char const *matcher;

/* The following flags are exported from grep for the matchers
   to look at. */
extern int match_icase;		/* -i */
extern int match_words;		/* -w */
extern int match_lines;		/* -x */
extern unsigned char eolbyte;	/* -z */
