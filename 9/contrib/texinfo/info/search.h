/* search.h -- Structure used to search large bodies of text, with bounds.
   $Id: search.h,v 1.3 2004/04/11 17:56:46 karl Exp $

   Copyright (C) 1993, 1997, 1998, 2002, 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

/* The search functions take two arguments:

     1) a string to search for, and

     2) a pointer to a SEARCH_BINDING which contains the buffer, start,
        and end of the search.

   They return a long, which is the offset from the start of the buffer
   at which the match was found.  An offset of -1 indicates failure. */

#ifndef INFO_SEARCH_H
#define INFO_SEARCH_H

typedef struct {
  char *buffer;                 /* The buffer of text to search. */
  long start;                   /* Offset of the start of the search. */
  long end;                     /* Offset of the end of the searh. */
  int flags;                    /* Flags controlling the type of search. */
} SEARCH_BINDING;

#define S_FoldCase      0x01    /* Set means fold case in searches. */
#define S_SkipDest      0x02    /* Set means return pointing after the dest. */

SEARCH_BINDING *make_binding (char *buffer, long int start, long int end);
SEARCH_BINDING *copy_binding (SEARCH_BINDING *binding);
extern long search_forward (char *string, SEARCH_BINDING *binding);
extern long search_backward (char *input_string, SEARCH_BINDING *binding);
extern long search (char *string, SEARCH_BINDING *binding);
extern int looking_at (char *string, SEARCH_BINDING *binding);

/* Note that STRING_IN_LINE () always returns the offset of the 1st character
   after the string. */
extern int string_in_line (char *string, char *line);

/* Function names that start with "skip" are passed a string, and return
   an offset from the start of that string.  Function names that start
   with "find" are passed a SEARCH_BINDING, and return an absolute position
   marker of the item being searched for.  "Find" functions return a value
   of -1 if the item being looked for couldn't be found. */
extern int skip_whitespace (char *string);
extern int skip_non_whitespace (char *string);
extern int skip_whitespace_and_newlines (char *string);
extern int skip_line (char *string);
extern int skip_node_characters (char *string, int newlines_okay);
extern int skip_node_separator (char *body);

#define DONT_SKIP_NEWLINES 0
#define SKIP_NEWLINES 1

extern long find_node_separator (SEARCH_BINDING *binding);
extern long find_tags_table (SEARCH_BINDING *binding);
extern long find_node_in_binding (char *nodename, SEARCH_BINDING *binding);

#endif /* not INFO_SEARCH_H */
