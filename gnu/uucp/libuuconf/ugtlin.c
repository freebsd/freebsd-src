/* ugtlin.c
   Read a line with backslash continuations.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

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

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_ugtlin_rcsid[] = "$Id: ugtlin.c,v 1.1 1993/08/05 18:26:17 conklin Exp $";
#endif

/* Read a line from a file with backslash continuations.  This updates
   the qglobal->ilineno count for each additional line it reads.  */

int
_uuconf_getline (qglobal, pzline, pcline, e)
     struct sglobal *qglobal;
     char **pzline;
     size_t *pcline;
     FILE *e;
{
  int ctot;
  char *zline;
  size_t cline;

  ctot = -1;

  zline = NULL;
  cline = 0;

  while (TRUE)
    {
      int cchars;

      if (ctot < 0)
	cchars = getline (pzline, pcline, e);
      else
	cchars = getline (&zline, &cline, e);
      if (cchars < 0)
	{
	  if (zline != NULL)
	    free ((pointer) zline);
	  if (ctot >= 0)
	    return ctot;
	  else
	    return cchars;
	}

      if (ctot < 0)
	ctot = cchars;
      else
	{
	  if (*pcline <= ctot + cchars)
	    {
	      char *znew;

	      if (*pcline > 0)
		znew = (char *) realloc ((pointer) *pzline,
					 (size_t) (ctot + cchars + 1));
	      else
		znew = (char *) malloc ((size_t) (ctot + cchars + 1));
	      if (znew == NULL)
		{
		  free ((pointer) zline);
		  return -1;
		}
	      *pzline = znew;
	      *pcline = ctot + cchars + 1;
	    }

	  memcpy ((pointer) ((*pzline) + ctot), (pointer) zline,
		  (size_t) (cchars + 1));
	  ctot += cchars;
	}

      if (ctot < 2
	  || (*pzline)[ctot - 1] != '\n'
	  || (*pzline)[ctot - 2] != '\\')
	{
	  if (zline != NULL)
	    free ((pointer) zline);
	  return ctot;
	}

      ++qglobal->ilineno;

      ctot -= 2;
      (*pzline)[ctot] = '\0';
    }
}
