/* indir.c
   See if a file is in a directory.

   Copyright (C) 1992 Ian Lance Taylor

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

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

/* See whether a file is in a directory, and optionally check access.  */

boolean
fsysdep_in_directory (zfile, zdir, fcheck, freadable, zuser)
     const char *zfile;
     const char *zdir;
     boolean fcheck;
     boolean freadable;
     const char *zuser;
{
  size_t c;
  char *zcopy, *zslash;
  struct stat s;

  if (*zfile != '/')
    return FALSE;
  c = strlen (zdir);
  if (c > 0 && zdir[c - 1] == '/')
    c--;
  if (strncmp (zfile, zdir, c) != 0
      || (zfile[c] != '/' && zfile[c] != '\0'))
    return FALSE;
  if (strstr (zfile + c, "/../") != NULL)
    return FALSE;

  /* If we're not checking access, get out now.  */
  if (! fcheck)
    return TRUE;

  zcopy = zbufcpy (zfile);

  /* Start checking directories after zdir.  Otherwise, we would
     require that all directories down to /usr/spool/uucppublic be
     publically searchable; they probably are but it should not be a
     requirement.  */
  zslash = zcopy + c;
  do
    {
      char b;
      struct stat shold;

      b = *zslash;
      *zslash = '\0';

      shold = s;
      if (stat (zcopy, &s) != 0)
	{
	  if (errno != ENOENT)
	    {
	      ulog (LOG_ERROR, "stat (%s): %s", zcopy, strerror (errno));
	      ubuffree (zcopy);
	      return FALSE;
	    }

	  /* If this is the top directory, any problems will be caught
	     later when we try to open it.  */
	  if (zslash == zcopy + c)
	    {
	      ubuffree (zcopy);
	      return TRUE;
	    }

	  /* Go back and check the last directory for read or write
	     access.  */
	  s = shold;
	  break;
	}

      /* If this is not a directory, get out of the loop.  */
      if (! S_ISDIR (s.st_mode))
	break;

      /* Make sure the directory is searchable.  */
      if (! fsuser_access (&s, X_OK, zuser))
	{
	  ulog (LOG_ERROR, "%s: %s", zcopy, strerror (EACCES));
	  ubuffree (zcopy);
	  return FALSE;
	}

      /* If we've reached the end of the string, get out.  */
      if (b == '\0')
	break;

      *zslash = b;
    }
  while ((zslash = strchr (zslash + 1, '/')) != NULL);

  /* At this point s holds a stat on the last component of the path.
     We must check it for readability or writeability.  */
  if (! fsuser_access (&s, freadable ? R_OK : W_OK, zuser))
    {
      ulog (LOG_ERROR, "%s: %s", zcopy, strerror (EACCES));
      ubuffree (zcopy);
      return FALSE;
    }

  ubuffree (zcopy);
  return TRUE;
}
