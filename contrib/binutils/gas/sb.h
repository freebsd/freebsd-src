/* sb.h - header file for string buffer manipulation routines
   Copyright (C) 1994, 1995 Free Software Foundation, Inc.

   Written by Steve and Judy Chamberlain of Cygnus Support,
      sac@cygnus.com

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#ifndef SB_H

#define SB_H

#include <stdio.h>
#include "ansidecl.h"

/* string blocks

   I had a couple of choices when deciding upon this data structure.
   gas uses null terminated strings for all its internal work.  This
   often means that parts of the program that want to examine
   substrings have to manipulate the data in the string to do the
   right thing (a common operation is to single out a bit of text by
   saving away the character after it, nulling it out, operating on
   the substring and then replacing the character which was under the
   null).  This is a pain and I remember a load of problems that I had with
   code in gas which almost got this right.  Also, it's harder to grow and
   allocate null terminated strings efficiently.

   Obstacks provide all the functionality needed, but are too
   complicated, hence the sb.

   An sb is allocated by the caller, and is initialzed to point to an
   sb_element.  sb_elements are kept on a free lists, and used when
   needed, replaced onto the free list when unused.
 */

#define sb_max_power_two    30	/* don't allow strings more than
			           2^sb_max_power_two long */
/* structure of an sb */
typedef struct sb
  {
    char *ptr;			/* points to the current block. */
    int len;			/* how much is used. */
    int pot;			/* the maximum length is 1<<pot */
    struct le *item;
  }
sb;

/* Structure of the free list object of an sb */
typedef struct le
  {
    struct le *next;
    int size;
    char data[1];
  }
sb_element;

/* The free list */
typedef struct
  {
    sb_element *size[sb_max_power_two];
  } sb_list_vector;

extern int string_count[sb_max_power_two];

extern void sb_build PARAMS ((sb *, int));
extern void sb_new PARAMS ((sb *));
extern void sb_kill PARAMS ((sb *));
extern void sb_add_sb PARAMS ((sb *, sb *));
extern void sb_reset PARAMS ((sb *));
extern void sb_add_char PARAMS ((sb *, int));
extern void sb_add_string PARAMS ((sb *, const char *));
extern void sb_add_buffer PARAMS ((sb *, const char *, int));
extern void sb_print PARAMS ((FILE *, sb *));
extern void sb_print_at PARAMS ((FILE *, int, sb *));
extern char *sb_name PARAMS ((sb *));
extern char *sb_terminate PARAMS ((sb *));
extern int sb_skip_white PARAMS ((int, sb *));
extern int sb_skip_comma PARAMS ((int, sb *));

/* Actually in input-scrub.c.  */
extern void input_scrub_include_sb PARAMS ((sb *, char *, int));

#endif /* SB_H */
