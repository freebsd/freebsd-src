/* dstring.h - Dynamic string handling include file.  Requires strings.h.
   Copyright (C) 1990, 1991, 1992, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef NULL
#define NULL 0
#endif

/* A dynamic string consists of record that records the size of an
   allocated string and the pointer to that string.  The actual string
   is a normal zero byte terminated string that can be used with the
   usual string functions.  The major difference is that the
   dynamic_string routines know how to get more space if it is needed
   by allocating new space and copying the current string.  */

typedef struct
{
  int ds_length;		/* Actual amount of storage allocated.  */
  char *ds_string;		/* String.  */
} dynamic_string;


/* Macros that look similar to the original string functions.
   WARNING:  These macros work only on pointers to dynamic string records.
   If used with a real record, an "&" must be used to get the pointer.  */
#define ds_strlen(s)		strlen ((s)->ds_string)
#define ds_strcmp(s1, s2)	strcmp ((s1)->ds_string, (s2)->ds_string)
#define ds_strncmp(s1, s2, n)	strncmp ((s1)->ds_string, (s2)->ds_string, n)
#define ds_index(s, c)		index ((s)->ds_string, c)
#define ds_rindex(s, c)		rindex ((s)->ds_string, c)

void ds_init (dynamic_string *string, int size);
void ds_resize (dynamic_string *string, int size);
char *ds_fgetname (FILE *f, dynamic_string *s);
char *ds_fgets (FILE *f, dynamic_string *s);
char *ds_fgetstr (FILE *f, dynamic_string *s, char eos);
