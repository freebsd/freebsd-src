/* hdial.c
   Find a dialer in the HDB configuration files.

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
const char _uuconf_hdial_rcsid[] = "$Id: hdial.c,v 1.2 1994/05/07 18:12:20 ache Exp $";
#endif

#include <errno.h>
#include <ctype.h>

/* Find a dialer in the HDB configuration files by name.  */

int
uuconf_hdb_dialer_info (pglobal, zname, qdialer)
     pointer pglobal;
     const char *zname;
     struct uuconf_dialer *qdialer;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pz;
  char *zline;
  size_t cline;
  char **pzsplit;
  size_t csplit;
  int iret;

  zline = NULL;
  cline = 0;
  pzsplit = NULL;
  csplit = 0;

  iret = UUCONF_NOT_FOUND;

  for (pz = qglobal->qprocess->pzhdb_dialers; *pz != NULL; pz++)
    {
      FILE *e;
      int cchars;

      qglobal->ilineno = 0;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      while ((cchars = _uuconf_getline (qglobal, &zline, &cline, e)) > 0)
	{
	  int ctoks;
	  pointer pblock;

	  ++qglobal->ilineno;

	  --cchars;
	  if (zline[cchars] == '\n')
	    zline[cchars] = '\0';
	  if (isspace (BUCHAR (zline[0])) || zline[0] == '#')
	    continue;

	  ctoks = _uuconf_istrsplit (zline, '\0', &pzsplit, &csplit);
	  if (ctoks < 0)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }

	  if (ctoks < 1)
	    continue;

	  if (strcmp (zname, pzsplit[0]) != 0)
	    continue;

	  /* We found the dialer.  */
	  pblock = uuconf_malloc_block ();
	  if (pblock == NULL)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }
	  if (uuconf_add_block (pblock, zline) != 0)
	    {
	      qglobal->ierrno = errno;
	      uuconf_free_block (pblock);
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }
	  zline = NULL;

	  _uuconf_uclear_dialer (qdialer);
	  qdialer->uuconf_zname = pzsplit[0];
	  qdialer->uuconf_palloc = pblock;

	  if (ctoks > 1)
	    {
	      /* The second field is characters to send instead of "="
		 and "-" in phone numbers.  */
	      if (strcmp (pzsplit[1], "\"\"") == 0)
		{
		  char *zsubs;
		  char bnext;

		  zsubs = pzsplit[1];
		  bnext = *zsubs;
		  while (bnext != '\0')
		    {
		      if (bnext == '=') 
			qdialer->uuconf_zdialtone = zsubs + 1;
		      else if (bnext == '-')
			qdialer->uuconf_zpause = zsubs + 1;
		      if (zsubs[1] == '\0')
			break;
		      zsubs += 2;
		      bnext = *zsubs;
		      *zsubs = '\0';
		    }
		}

	      /* Any remaining fields form a chat script.  */
	      if (ctoks > 2)
		{
		  pzsplit[1] = (char *) "chat";
		  iret = _uuconf_ichat_cmd (qglobal, ctoks - 1,
					    pzsplit + 1,
					    &qdialer->uuconf_schat,
					    pblock);
		  iret &=~ UUCONF_CMDTABRET_KEEP;
		  if (iret != UUCONF_SUCCESS)
		    {
		      uuconf_free_block (pblock);
		      break;
		    }
		}
	    }

	  iret = UUCONF_SUCCESS;
	  break;
	}

      (void) fclose (e);

      if (iret != UUCONF_NOT_FOUND)
	break;
    }

  if (zline != NULL)
    free ((pointer) zline);
  if (pzsplit != NULL)
    free ((pointer) pzsplit);

  if (iret != UUCONF_SUCCESS && iret != UUCONF_NOT_FOUND)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
    }

  return iret;
}
