/* move.c
   Move a file.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

/* Move (rename) a file from one name to another.  This routine will
   optionally create necessary directories, and fpublic indicates
   whether the new directories should be publically accessible or not.
   If fcheck is true, it will try to determine whether the named user
   has write access to the new file.  */

boolean
fsysdep_move_file (zorig, zto, fmkdirs, fpublic, fcheck, zuser)
     const char *zorig;
     const char *zto;
     boolean fmkdirs;
     boolean fpublic;
     boolean fcheck;
     const char *zuser;
{
  struct stat s;
  int o;

  DEBUG_MESSAGE2 (DEBUG_SPOOLDIR,
		  "fsysdep_move_file: Moving %s to %s", zorig, zto);

  /* Optionally make sure that zuser has write access on the
     directory.  */
  if (fcheck)
    {
      char *zcopy;
      char *zslash;

      zcopy = zbufcpy (zto);
      zslash = strrchr (zcopy, '/');
      if (zslash == zcopy)
	zslash[1] = '\0';
      else
	*zslash = '\0';

      if (stat (zcopy, &s) != 0)
	{
	  ulog (LOG_ERROR, "stat (%s): %s", zcopy, strerror (errno));
	  ubuffree (zcopy);
	  return FALSE;
	}
      if (! fsuser_access (&s, W_OK, zuser))
	{
	  ulog (LOG_ERROR, "%s: %s", zcopy, strerror (EACCES));
	  ubuffree (zcopy);
	  return FALSE;
	}
      ubuffree (zcopy);

      /* A malicious user now has a few milliseconds to change a
	 symbolic link to a directory uucp has write permission on but
	 the user does not (the obvious choice being /usr/lib/uucp).
	 The only certain method I can come up with to close this race
	 is to fork an suid process which takes on the users identity
	 and does the actual copy.  This is sufficiently high overhead
	 that I'm not going to do it.  */
    }

  /* We try to use rename to move the file.  */

  if (rename (zorig, zto) == 0)
    return TRUE;

  if (fmkdirs && errno == ENOENT)
    {
      if (! fsysdep_make_dirs (zto, fpublic))
	return FALSE;
      if (rename (zorig, zto) == 0)
	return TRUE;
    }

#if HAVE_RENAME
  /* On some systems the system call rename seems to fail for
     arbitrary reasons.  To get around this, we always try to copy the
     file by hand if the rename failed.  */
  errno = EXDEV;
#endif

  /* If we can't link across devices, we must copy the file by hand.  */
  if (errno != EXDEV)
    {
      ulog (LOG_ERROR, "rename (%s, %s): %s", zorig, zto,
	    strerror (errno));
      return FALSE;
    }

  /* Copy the file.  */
  if (stat ((char *) zorig, &s) < 0)
    {
      ulog (LOG_ERROR, "stat (%s): %s", zorig, strerror (errno));
      return FALSE;
    }

  /* Make sure the file gets the right mode by creating it before we
     call fcopy_file.  */
  (void) remove (zto);
  o = creat ((char *) zto, s.st_mode);
  if (o < 0)
    {
      if (fmkdirs && errno == ENOENT)
	{
	  if (! fsysdep_make_dirs (zto, fpublic))
	    return FALSE;
	  o = creat ((char *) zto, s.st_mode);
	}
      if (o < 0)
	{
	  ulog (LOG_ERROR, "creat (%s): %s", zto, strerror (errno));
	  return FALSE;
	}
    }
  (void) close (o);

  if (! fcopy_file (zorig, zto, fpublic, fmkdirs))
    return FALSE;

  if (remove (zorig) != 0)
    ulog (LOG_ERROR, "remove (%s): %s", zorig, strerror (errno));

  return TRUE;
}
