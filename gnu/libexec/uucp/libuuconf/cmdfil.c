/* cmdfil.c
   Read and parse commands from a file.

   Copyright (C) 1992, 1993 Ian Lance Taylor

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
const char _uuconf_cmdfil_rcsid[] = "$Id: cmdfil.c,v 1.2 1994/05/07 18:12:03 ache Exp $";
#endif

#include <errno.h>

/* Read and parse commands from a file, updating uuconf_lineno as
   appropriate.  */

int
uuconf_cmd_file (pglobal, e, qtab, pinfo, pfiunknown, iflags, pblock)
     pointer pglobal;
     FILE *e;
     const struct uuconf_cmdtab *qtab;
     pointer pinfo;
     int (*pfiunknown) P((pointer, int, char **, pointer, pointer));
     int iflags;
     pointer pblock;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  boolean fcont;
  char *zline;
  size_t cline;
  int iret;

  fcont = (iflags & UUCONF_CMDTABFLAG_BACKSLASH) != 0;

  zline = NULL;
  cline = 0;

  iret = UUCONF_SUCCESS;

  qglobal->ilineno = 0;

  while ((fcont
	  ? _uuconf_getline (qglobal, &zline, &cline, e)
	  : getline (&zline, &cline, e)) > 0)
    {
      ++qglobal->ilineno;

      iret = uuconf_cmd_line (pglobal, zline, qtab, pinfo, pfiunknown,
			      iflags, pblock);

      if ((iret & UUCONF_CMDTABRET_KEEP) != 0)
	{
	  iret &=~ UUCONF_CMDTABRET_KEEP;

	  if (pblock != NULL)
	    {
	      if (uuconf_add_block (pblock, zline) != 0)
		{
		  qglobal->ierrno = errno;
		  iret = (UUCONF_MALLOC_FAILED
			  | UUCONF_ERROR_ERRNO
			  | UUCONF_ERROR_LINENO);
		  break;
		}
	    }

	  zline = NULL;
	  cline = 0;
	}

      if ((iret & UUCONF_CMDTABRET_EXIT) != 0)
	{
	  iret &=~ UUCONF_CMDTABRET_EXIT;
	  if (iret != UUCONF_SUCCESS)
	    iret |= UUCONF_ERROR_LINENO;
	  break;
	}

      iret = UUCONF_SUCCESS;
    }

  if (zline != NULL)
    free ((pointer) zline);

  return iret;
}
