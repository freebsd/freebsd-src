/* cmdlin.c
   Parse a command line.

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_cmdlin_rcsid[] = "$Id: cmdlin.c,v 1.2 1994/05/07 18:12:04 ache Exp $";
#endif

#include <errno.h>
#include <ctype.h>

/* Parse a command line into fields and process it via a command
   table.  The command table functions may keep the memory allocated
   for the line, but they may not keep the memory allocated for the
   argv list.  This function strips # comments.  */

#define CSTACK (16)

int
uuconf_cmd_line (pglobal, zline, qtab, pinfo, pfiunknown, iflags, pblock)
     pointer pglobal;
     char *zline;
     const struct uuconf_cmdtab *qtab;
     pointer pinfo;
     int (*pfiunknown) P((pointer, int, char **, pointer, pointer));
     int iflags;
     pointer pblock;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char *z;
  int cargs;
  char *azargs[CSTACK];
  char **pzargs;
  int iret;

  /* Any # not preceeded by a backslash starts a comment.  */
  z = zline;
  while ((z = strchr (z, '#')) != NULL)
    {
      if (z == zline || *(z - 1) != '\\')
	{
	  *z = '\0';
	  break;
	}
      /* Remove the backslash.  */
      while ((*(z - 1) = *z) != '\0')
	++z;
    }

  /* Parse the first CSTACK arguments by hand to avoid malloc.  */

  z = zline;
  cargs = 0;
  pzargs = azargs;
  while (TRUE)
    {
      while (*z != '\0' && isspace (BUCHAR (*z)))
	++z;

      if (*z == '\0')
	break;

      if (cargs >= CSTACK)
	{
	  char **pzsplit;
	  size_t csplit;
	  int cmore;

	  pzsplit = NULL;
	  csplit = 0;
	  cmore = _uuconf_istrsplit (z, '\0', &pzsplit, &csplit);
	  if (cmore < 0)
	    {
	      qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }

	  pzargs = (char **) malloc ((cmore + CSTACK) * sizeof (char *));
	  if (pzargs == NULL)
	    {
	      qglobal->ierrno = errno;
	      free ((pointer) pzsplit);
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }

	  memcpy ((pointer) pzargs, (pointer) azargs,
		  CSTACK * sizeof (char *));
	  memcpy ((pointer) (pzargs + CSTACK), (pointer) pzsplit,
		  cmore * sizeof (char *));
	  cargs = cmore + CSTACK;

	  free ((pointer) pzsplit);

	  break;
	}

      azargs[cargs] = z;
      ++cargs;

      while (*z != '\0' && ! isspace (BUCHAR (*z)))
	z++;

      if (*z == '\0')
	break;

      *z++ = '\0';
    }

  if (cargs <= 0)
    return UUCONF_CMDTABRET_CONTINUE;

  iret = uuconf_cmd_args (pglobal, cargs, pzargs, qtab, pinfo, pfiunknown,
			  iflags, pblock);

  if (pzargs != azargs)
    free ((pointer) pzargs);

  return iret;
}
