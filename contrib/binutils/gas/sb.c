/* sb.c - string buffer manipulation routines
   Copyright 1994, 1995, 2000 Free Software Foundation, Inc.

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
   02111-1307, USA.  */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "libiberty.h"
#include "sb.h"

/* These routines are about manipulating strings.

   They are managed in things called `sb's which is an abbreviation
   for string buffers.  An sb has to be created, things can be glued
   on to it, and at the end of it's life it should be freed.  The
   contents should never be pointed at whilst it is still growing,
   since it could be moved at any time

   eg:
   sb_new (&foo);
   sb_grow... (&foo,...);
   use foo->ptr[*];
   sb_kill (&foo);

*/

#define dsize 5

static void sb_check PARAMS ((sb *, int));

/* Statistics of sb structures.  */

int string_count[sb_max_power_two];

/* Free list of sb structures.  */

static sb_list_vector free_list;

/* initializes an sb.  */

void
sb_build (ptr, size)
     sb *ptr;
     int size;
{
  /* see if we can find one to allocate */
  sb_element *e;

  if (size > sb_max_power_two)
    abort ();

  e = free_list.size[size];
  if (!e)
    {
      /* nothing there, allocate one and stick into the free list */
      e = (sb_element *) xmalloc (sizeof (sb_element) + (1 << size));
      e->next = free_list.size[size];
      e->size = 1 << size;
      free_list.size[size] = e;
      string_count[size]++;
    }

  /* remove from free list */

  free_list.size[size] = e->next;

  /* copy into callers world */
  ptr->ptr = e->data;
  ptr->pot = size;
  ptr->len = 0;
  ptr->item = e;
}

void
sb_new (ptr)
     sb *ptr;
{
  sb_build (ptr, dsize);
}

/* deallocate the sb at ptr */

void
sb_kill (ptr)
     sb *ptr;
{
  /* return item to free list */
  ptr->item->next = free_list.size[ptr->pot];
  free_list.size[ptr->pot] = ptr->item;
}

/* add the sb at s to the end of the sb at ptr */

void
sb_add_sb (ptr, s)
     sb *ptr;
     sb *s;
{
  sb_check (ptr, s->len);
  memcpy (ptr->ptr + ptr->len, s->ptr, s->len);
  ptr->len += s->len;
}

/* make sure that the sb at ptr has room for another len characters,
   and grow it if it doesn't.  */

static void
sb_check (ptr, len)
     sb *ptr;
     int len;
{
  if (ptr->len + len >= 1 << ptr->pot)
    {
      sb tmp;
      int pot = ptr->pot;
      while (ptr->len + len >= 1 << pot)
	pot++;
      sb_build (&tmp, pot);
      sb_add_sb (&tmp, ptr);
      sb_kill (ptr);
      *ptr = tmp;
    }
}

/* make the sb at ptr point back to the beginning.  */

void
sb_reset (ptr)
     sb *ptr;
{
  ptr->len = 0;
}

/* add character c to the end of the sb at ptr.  */

void
sb_add_char (ptr, c)
     sb *ptr;
     int c;
{
  sb_check (ptr, 1);
  ptr->ptr[ptr->len++] = c;
}

/* add null terminated string s to the end of sb at ptr.  */

void
sb_add_string (ptr, s)
     sb *ptr;
     const char *s;
{
  int len = strlen (s);
  sb_check (ptr, len);
  memcpy (ptr->ptr + ptr->len, s, len);
  ptr->len += len;
}

/* add string at s of length len to sb at ptr */

void
sb_add_buffer (ptr, s, len)
     sb *ptr;
     const char *s;
     int len;
{
  sb_check (ptr, len);
  memcpy (ptr->ptr + ptr->len, s, len);
  ptr->len += len;
}

/* print the sb at ptr to the output file */

void
sb_print (outfile, ptr)
     FILE *outfile;
     sb *ptr;
{
  int i;
  int nc = 0;

  for (i = 0; i < ptr->len; i++)
    {
      if (nc)
	{
	  fprintf (outfile, ",");
	}
      fprintf (outfile, "%d", ptr->ptr[i]);
      nc = 1;
    }
}

void
sb_print_at (outfile, idx, ptr)
     FILE *outfile;
     int idx;
     sb *ptr;
{
  int i;
  for (i = idx; i < ptr->len; i++)
    putc (ptr->ptr[i], outfile);
}

/* put a null at the end of the sb at in and return the start of the
   string, so that it can be used as an arg to printf %s.  */

char *
sb_name (in)
     sb *in;
{
  /* stick a null on the end of the string */
  sb_add_char (in, 0);
  return in->ptr;
}

/* like sb_name, but don't include the null byte in the string.  */

char *
sb_terminate (in)
     sb *in;
{
  sb_add_char (in, 0);
  --in->len;
  return in->ptr;
}

/* start at the index idx into the string in sb at ptr and skip
   whitespace. return the index of the first non whitespace character */

int
sb_skip_white (idx, ptr)
     int idx;
     sb *ptr;
{
  while (idx < ptr->len
	 && (ptr->ptr[idx] == ' '
	     || ptr->ptr[idx] == '\t'))
    idx++;
  return idx;
}

/* start at the index idx into the sb at ptr. skips whitespace,
   a comma and any following whitespace. returnes the index of the
   next character.  */

int
sb_skip_comma (idx, ptr)
     int idx;
     sb *ptr;
{
  while (idx < ptr->len
	 && (ptr->ptr[idx] == ' '
	     || ptr->ptr[idx] == '\t'))
    idx++;

  if (idx < ptr->len
      && ptr->ptr[idx] == ',')
    idx++;

  while (idx < ptr->len
	 && (ptr->ptr[idx] == ' '
	     || ptr->ptr[idx] == '\t'))
    idx++;

  return idx;
}
