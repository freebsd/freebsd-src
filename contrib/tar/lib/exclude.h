/* exclude.h -- declarations for excluding file names
   Copyright 1992, 1993, 1994, 1997, 1999, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert <eggert@twinsun.com>  */

#ifndef PARAMS
# if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

/* Exclude options, which can be ORed with fnmatch options.  */

/* Patterns must match the start of file names, instead of matching
   anywhere after a '/'.  */
#define EXCLUDE_ANCHORED (1 << 5)

/* Include instead of exclude.  */
#define EXCLUDE_INCLUDE (1 << 6)

/* '?', '*', '[', and '\\' are special in patterns.  Without this
   option, these characters are ordinary and fnmatch is not used.  */
#define EXCLUDE_WILDCARDS (1 << 7)

struct exclude;

struct exclude *new_exclude PARAMS ((void));
void free_exclude PARAMS ((struct exclude *));
void add_exclude PARAMS ((struct exclude *, char const *, int));
int add_exclude_file PARAMS ((void (*) (struct exclude *, char const *, int),
			      struct exclude *, char const *, int, char));
bool excluded_filename PARAMS ((struct exclude const *, char const *));
