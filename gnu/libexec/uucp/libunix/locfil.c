/* locfil.c
   Expand a file name on the local system.

   Copyright (C) 1991, 1992 Ian Lance Taylor

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <pwd.h>

#if GETPWNAM_DECLARATION_OK
#ifndef getpwnam
extern struct passwd *getpwnam ();
#endif
#endif

/* Turn a file name into an absolute path, by doing tilde expansion
   and moving any other type of file into the public directory.  */

char *
zsysdep_local_file (zfile, zpubdir)
     const char *zfile;
     const char *zpubdir;
{
  const char *zdir;

  if (*zfile == '/')
    return zbufcpy (zfile);

  if (*zfile != '~')
    zdir = zpubdir;
  else
    {
      if (zfile[1] == '\0')
	return zbufcpy (zpubdir);

      if (zfile[1] == '/')
	{
	  zdir = zpubdir;
	  zfile += 2;
	}
      else
	{
	  size_t cuserlen;
	  char *zcopy;
	  struct passwd *q;

	  ++zfile;
	  cuserlen = strcspn ((char *) zfile, "/");
	  zcopy = zbufalc (cuserlen + 1);
	  memcpy (zcopy, zfile, cuserlen);
	  zcopy[cuserlen] = '\0';
      
	  q = getpwnam (zcopy);
	  if (q == NULL)
	    {
	      ulog (LOG_ERROR, "User %s not found", zcopy);
	      ubuffree (zcopy);
	      return NULL;
	    }
	  ubuffree (zcopy);

	  if (zfile[cuserlen] == '\0')
	    return zbufcpy (q->pw_dir);

	  zdir = q->pw_dir;
	  zfile += cuserlen + 1;
	}
    }

  return zsysdep_in_dir (zdir, zfile);
}
