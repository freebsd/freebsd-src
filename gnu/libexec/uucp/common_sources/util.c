/* util.c
   A couple of UUCP utility functions.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char util_rcsid[] = "$FreeBSD$";
#endif

#include <ctype.h>

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* Get information for an unknown system.  This will leave the name
   allocated on the heap.  We could fix this by breaking the
   abstraction and adding the name to qsys->palloc.  It makes sure the
   name is not too long, but takes no other useful action.  */

boolean
funknown_system (puuconf, zsystem, qsys)
     pointer puuconf;
     const char *zsystem;
     struct uuconf_system *qsys;
{
  char *z;
  int iuuconf;

  if (strlen (zsystem) <= cSysdep_max_name_len)
    z = zbufcpy (zsystem);
  else
    {
      char **pznames, **pz;
      boolean ffound;

      z = zbufalc (cSysdep_max_name_len + 1);
      memcpy (z, zsystem, cSysdep_max_name_len);
      z[cSysdep_max_name_len] = '\0';

      iuuconf = uuconf_system_names (puuconf, &pznames, TRUE);
      if (iuuconf != UUCONF_SUCCESS)
	ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

      ffound = FALSE;
      for (pz = pznames; *pz != NULL; pz++)
	{
	  if (strcmp (*pz, z) == 0)
	    ffound = TRUE;
	  xfree ((pointer) *pz);
	}
      xfree ((pointer) pznames);

      if (ffound)
	{
	  ubuffree (z);
	  return FALSE;
	}
    }

  iuuconf = uuconf_system_unknown (puuconf, qsys);
  if (iuuconf == UUCONF_NOT_FOUND)
    {
      ubuffree (z);
      return FALSE;
    }
  else if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  for (; qsys != NULL; qsys = qsys->uuconf_qalternate)
    qsys->uuconf_zname = z;

  return TRUE;
}

/* Remove all occurrences of the local system name followed by an
   exclamation point from the front of a string, returning the new
   string.  This is used by uucp and uux.  */

char *
zremove_local_sys (qlocalsys, z)
     struct uuconf_system *qlocalsys;
     char *z;
{
  size_t clen;
  char *zexclam;

  clen = strlen (qlocalsys->uuconf_zname);
  zexclam = strchr (z, '!');
  while (zexclam != NULL)
    {
      if (z == zexclam
	  || (zexclam - z == clen
	      && strncmp (z, qlocalsys->uuconf_zname, clen) == 0))
	;
      else if (qlocalsys->uuconf_pzalias == NULL)
	break;
      else
	{
	  char **pzal;

	  for (pzal = qlocalsys->uuconf_pzalias; *pzal != NULL; pzal++)
	    if (strlen (*pzal) == zexclam - z
		&& strncmp (z, *pzal, (size_t) (zexclam - z)) == 0)
	      break;
	  if (*pzal == NULL)
	    break;
	}
      z = zexclam + 1;
      zexclam = strchr (z, '!');
    }

  return z;
}

/* See whether a file is in a directory list, and make sure the user
   has appropriate access.  */

boolean
fin_directory_list (zfile, pzdirs, zpubdir, fcheck, freadable, zuser)
     const char *zfile;
     char **pzdirs;
     const char *zpubdir;
     boolean fcheck;
     boolean freadable;
     const char *zuser;
{
  boolean fmatch;
  char **pz;

  fmatch = FALSE;

  for (pz = pzdirs; *pz != NULL; pz++)
    {
      char *zuse;

      if (pz[0][0] == '!')
	{
	  zuse = zsysdep_local_file (*pz + 1, zpubdir, (boolean *) NULL);
	  if (zuse == NULL)
	    return FALSE;

	  if (fsysdep_in_directory (zfile, zuse, FALSE,
				    FALSE, (const char *) NULL))
	    fmatch = FALSE;
	}
      else
	{
	  zuse = zsysdep_local_file (*pz, zpubdir, (boolean *) NULL);
	  if (zuse == NULL)
	    return FALSE;

	  if (fsysdep_in_directory (zfile, zuse, fcheck,
				    freadable, zuser))
	    fmatch = TRUE;
	}

      ubuffree (zuse);
    }

  return fmatch;
}
