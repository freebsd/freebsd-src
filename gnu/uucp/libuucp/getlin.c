/* getlin.c
   Replacement for getline.

   Copyright (C) 1992 Ian Lance Taylor

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
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

/* Read a line from a file, returning the number of characters read.
   This should really return ssize_t.  Returns -1 on error.  */

#define CGETLINE_DEFAULT (63)

int
getline (pzline, pcline, e)
     char **pzline;
     size_t *pcline;
     FILE *e;
{
  char *zput, *zend;
  int bchar;

  if (*pzline == NULL)
    {
      *pzline = (char *) malloc (CGETLINE_DEFAULT);
      if (*pzline == NULL)
	return -1;
      *pcline = CGETLINE_DEFAULT;
    }

  zput = *pzline;
  zend = *pzline + *pcline - 1;

  while ((bchar = getc (e)) != EOF)
    {
      if (zput >= zend)
	{
	  size_t cnew;
	  char *znew;

	  cnew = *pcline * 2 + 1;
	  znew = (char *) realloc ((pointer) *pzline, cnew);
	  if (znew == NULL)
	    return -1;
	  zput = znew + *pcline - 1;
	  zend = znew + cnew - 1;
	  *pzline = znew;
	  *pcline = cnew;
	}

      *zput++ = bchar;

      if (bchar == '\n')
	break;
    }

  if (zput == *pzline)
    return -1;

  *zput = '\0';
  return zput - *pzline;
}
