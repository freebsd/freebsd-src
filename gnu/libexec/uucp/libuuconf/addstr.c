/* addstr.c
   Add a string to a list of strings.

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
const char _uuconf_addstr_rcsid[] = "$Id: addstr.c,v 1.1 1993/08/05 18:24:57 conklin Exp $";
#endif

#include <errno.h>

/* When setting system information, we need to be able to distinguish
   between a value that is not set and a value that has been set to
   NULL.  We do this by initializing the value to point to the
   variable _uuconf_unset, and then correcting it in the function
   _uuconf_isystem_basic_default.  This variable is declared in this
   file because some linkers will apparently not pull in an object
   file which merely declarates a variable.  This functions happens to
   be pulled in by almost everything.  */

char *_uuconf_unset;

/* Add a string to a list of strings.  The list is maintained as an
   array of elements ending in NULL.  The total number of available
   slots is always a multiple of CSLOTS, so by counting the current
   number of elements we can tell whether a new slot is needed.  If
   the fcopy argument is TRUE, the new string is duplicated into
   memory.  If the fcheck argument is TRUE, this does not add a string
   that is already in the list.  The pblock argument may be used to do
   the allocations within a memory block.  This returns a standard
   uuconf error code.  */

#define CSLOTS (8)

int
_uuconf_iadd_string (qglobal, zadd, fcopy, fcheck, ppzstrings, pblock)
     struct sglobal *qglobal;
     char *zadd;
     boolean fcopy;
     boolean fcheck;
     char ***ppzstrings;
     pointer pblock;
{
  char **pz;
  size_t c;

  if (fcheck && *ppzstrings != NULL)
    {
      for (pz = *ppzstrings; *pz != NULL; pz++)
	if (strcmp (zadd, *pz) == 0)
	  return UUCONF_SUCCESS;
    }

  if (fcopy)
    {
      size_t clen;
      char *znew;

      clen = strlen (zadd) + 1;
      znew = (char *) uuconf_malloc (pblock, clen);
      if (znew == NULL)
	{
	  if (qglobal != NULL)
	    qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
      memcpy ((pointer) znew, (pointer) zadd, clen);
      zadd = znew;
    }

  pz = *ppzstrings;
  if (pz == NULL || pz == (char **) &_uuconf_unset)
    {
      pz = (char **) uuconf_malloc (pblock, CSLOTS * sizeof (char *));
      if (pz == NULL)
	{
	  if (qglobal != NULL)
	    qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
      *ppzstrings = pz;
    }
  else
    {
      c = 0;
      while (*pz != NULL)
	{
	  ++pz;
	  ++c;
	}

      if ((c + 1) % CSLOTS == 0)
	{
	  char **pznew;

	  pznew = (char **) uuconf_malloc (pblock,
					   ((c + 1 + CSLOTS)
					    * sizeof (char *)));
	  if (pznew == NULL)
	    {
	      if (qglobal != NULL)
		qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
	  memcpy ((pointer) pznew, (pointer) *ppzstrings,
		  c * sizeof (char *));
	  uuconf_free (pblock, *ppzstrings);
	  *ppzstrings = pznew;
	  pz = pznew + c;
	}
    }

  pz[0] = zadd;
  pz[1] = NULL;

  return UUCONF_SUCCESS;
}
