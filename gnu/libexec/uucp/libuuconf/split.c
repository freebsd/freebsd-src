/* split.c
   Split a string into tokens.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_split_rcsid[] = "$FreeBSD$";
#endif

#include <ctype.h>

/* Split a string into tokens.  The bsep argument is the separator to
   use.  If it is the null byte, white space is used as the separator,
   and leading white space is discarded.  Otherwise, each occurrence
   of the separator character delimits a field (and thus some fields
   may be empty).  The array and size arguments may be used to reuse
   the same memory.  This function is not tied to uuconf; the only way
   it can fail is if malloc or realloc fails.  */

int
_uuconf_istrsplit (zline, bsep, ppzsplit, pcsplit)
     register char *zline;
     int bsep;
     char ***ppzsplit;
     size_t *pcsplit;
{
  size_t i;

  i = 0;

  while (TRUE)
    {
      if (bsep == '\0')
	{
	  while (isspace (BUCHAR (*zline)))
	    ++zline;
	  if (*zline == '\0')
	    break;
	}

      if (i >= *pcsplit)
	{
	  char **pznew;
	  size_t cnew;

	  if (*pcsplit == 0)
	    {
	      cnew = 8;
	      pznew = (char **) malloc (cnew * sizeof (char *));
	    }
	  else
	    {
	      cnew = *pcsplit * 2;
	      pznew = (char **) realloc ((pointer) *ppzsplit,
					 cnew * sizeof (char *));
	    }
	  if (pznew == NULL)
	    return -1;
	  *ppzsplit = pznew;
	  *pcsplit = cnew;
	}

      (*ppzsplit)[i] = zline;
      ++i;

      if (bsep == '\0')
	{
	  while (*zline != '\0' && ! isspace (BUCHAR (*zline)))
	    ++zline;
	}
      else
	{
	  while (*zline != '\0' && *zline != bsep)
	    ++zline;
	}

      if (*zline == '\0')
	break;

      *zline++ = '\0';
    }

  return i;
}
