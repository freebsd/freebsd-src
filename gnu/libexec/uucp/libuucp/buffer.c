/* buffer.c
   Manipulate buffers used to hold strings.

   Copyright (C) 1992, 1993 Ian Lance Taylor

   This file is part of Taylor UUCP.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#include "uudefs.h"

/* We keep a linked list of buffers.  The union is a hack because the
   default definition of offsetof, in uucp.h, takes the address of the
   field, and some C compilers will not let you take the address of an
   array.  */

struct sbuf
{
  struct sbuf *qnext;
  size_t c;
  union
    {
      char ab[4];
      char bdummy;
    }
  u;
};

static struct sbuf *qBlist;

/* Get a buffer of a given size.  The buffer is returned with the
   ubuffree function.  */

char *
zbufalc (c)
     size_t c;
{
  register struct sbuf *q;

  if (qBlist == NULL)
    {
      q = (struct sbuf *) xmalloc (sizeof (struct sbuf) + c - 4);
      q->c = c;
    }
  else
    {
      q = qBlist;
      qBlist = q->qnext;
      if (q->c < c)
	{
	  q = (struct sbuf *) xrealloc ((pointer) q,
					sizeof (struct sbuf) + c - 4);
	  q->c = c;
	}
    }
  return q->u.ab;
}

/* Get a buffer holding a given string.  */

char *
zbufcpy (z)
     const char *z;
{
  size_t csize;
  char *zret;

  if (z == NULL)
    return NULL;
  csize = strlen (z) + 1;
  zret = zbufalc (csize);
  memcpy (zret, z, csize);
  return zret;
}

/* Free up a buffer back onto the linked list.  */

void
ubuffree (z)
     char *z;
{
  struct sbuf *q;
  /* The type of ioff should be size_t, but making it int avoids a bug
     in some versions of the HP/UX compiler, and will always work.  */
  int ioff;

  if (z == NULL)
    return;
  ioff = offsetof (struct sbuf, u);
  q = (struct sbuf *) (pointer) (z - ioff);

#ifdef DEBUG_BUFFER
  {
    struct sbuf *qlook;

    for (qlook = qBlist; qlook != NULL; qlook = qlook->qnext)
      {
	if (qlook == q)
	  {
	    ulog (LOG_ERROR, "ubuffree: Attempt to free buffer twice");
	    abort ();
	  }
      }
  }
#endif

  q->qnext = qBlist;
  qBlist = q;
}
