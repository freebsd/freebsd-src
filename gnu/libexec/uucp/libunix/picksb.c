/* picksb.c
   System dependent routines for uupick.

   Copyright (C) 1992, 1993 Ian Lance Taylor

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
const char picksb_rcsid[] = "$FreeBSD$";
#endif

#include "uudefs.h"
#include "system.h"
#include "sysdep.h"

#include <errno.h>
#include <pwd.h>

#if HAVE_OPENDIR
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* ! HAVE_DIRENT_H */
#include <sys/dir.h>
#define dirent direct
#endif /* ! HAVE_DIRENT_H */
#endif /* HAVE_OPENDIR */

#if GETPWUID_DECLARATION_OK
#ifndef getpwuid
extern struct passwd *getpwuid ();
#endif
#endif

/* Local variables.  */

/* Directory of ~/receive/USER.  */
static DIR *qStopdir;

/* Name of ~/receive/USER.  */
static char *zStopdir;

/* Directory of ~/receive/USER/SYSTEM.  */
static DIR *qSsysdir;

/* Name of system.  */
static char *zSsysdir;

/* Prepare to get a list of all the file to uupick for this user.  */

/*ARGSUSED*/
boolean
fsysdep_uupick_init (zsystem, zpubdir)
     const char *zsystem;
     const char *zpubdir;
{
  const char *zuser;

  zuser = zsysdep_login_name ();

  zStopdir = (char *) xmalloc (strlen (zpubdir)
			       + sizeof "/receive/"
			       + strlen (zuser));
  sprintf (zStopdir, "%s/receive/%s", zpubdir, zuser);

  qStopdir = opendir (zStopdir);
  if (qStopdir == NULL && errno != ENOENT)
    {
      ulog (LOG_ERROR, "opendir (%s): %s", zStopdir,
	    strerror (errno));
      return FALSE;
    }

  qSsysdir = NULL;

  return TRUE;
}

/* Return the next file from the uupick directories.  */

/*ARGSUSED*/
char *
zsysdep_uupick (zsysarg, zpubdir, pzfrom, pzfull)
     const char *zsysarg;
     const char *zpubdir;
     char **pzfrom;
     char **pzfull;
{
  struct dirent *qentry;

  while (TRUE)
    {
      while (qSsysdir == NULL)
	{
	  const char *zsystem;
	  char *zdir;

	  if (qStopdir == NULL)
	    return NULL;

	  if (zsysarg != NULL)
	    {
	      closedir (qStopdir);
	      qStopdir = NULL;
	      zsystem = zsysarg;
	    }
	  else
	    {
	      do
		{
		  qentry = readdir (qStopdir);
		  if (qentry == NULL)
		    {
		      closedir (qStopdir);
		      qStopdir = NULL;
		      return NULL;
		    }
		}
	      while (strcmp (qentry->d_name, ".") == 0
		     || strcmp (qentry->d_name, "..") == 0);

	      zsystem = qentry->d_name;
	    }

	  zdir = zbufalc (strlen (zStopdir) + strlen (zsystem) + sizeof "/");
	  sprintf (zdir, "%s/%s", zStopdir, zsystem);

	  qSsysdir = opendir (zdir);
	  if (qSsysdir == NULL)
	    {
	      if (errno != ENOENT && errno != ENOTDIR)
		ulog (LOG_ERROR, "opendir (%s): %s", zdir, strerror (errno));
	    }
	  else
	    {
	      ubuffree (zSsysdir);
	      zSsysdir = zbufcpy (zsystem);
	    }

	  ubuffree (zdir);
	}

      qentry = readdir (qSsysdir);
      if (qentry == NULL)
	{
	  closedir (qSsysdir);
	  qSsysdir = NULL;
	  continue;
	}

      if (strcmp (qentry->d_name, ".") == 0
	  || strcmp (qentry->d_name, "..") == 0)
	continue;

      *pzfrom = zbufcpy (zSsysdir);
      *pzfull = zsappend3 (zStopdir, zSsysdir, qentry->d_name);
      return zbufcpy (qentry->d_name);
    }
}

/*ARGSUSED*/
boolean
fsysdep_uupick_free (zsystem, zpubdir)
     const char *zsystem;
     const char *zpubdir;
{
  xfree ((pointer) zStopdir);
  if (qStopdir != NULL)
    {
      closedir (qStopdir);
      qStopdir = NULL;
    }
  ubuffree (zSsysdir);
  zSsysdir = NULL;
  if (qSsysdir != NULL)
    {
      closedir (qSsysdir);
      qSsysdir = NULL;
    }

  return TRUE;
}

/* Expand a local file name for uupick.  */

char *
zsysdep_uupick_local_file (zfile, pfbadname)
     const char *zfile;
     boolean *pfbadname;
{
  struct passwd *q;

  if (pfbadname != NULL)
    *pfbadname = FALSE;

  /* If this does not start with a simple ~, pass it to
     zsysdep_local_file_cwd; as it happens, zsysdep_local_file_cwd
     only uses the zpubdir argument if the file starts with a simple
     ~, so it doesn't really matter what we pass for zpubdir.  */
  if (zfile[0] != '~'
      || (zfile[1] != '/' && zfile[1] != '\0'))
    return zsysdep_local_file_cwd (zfile, (const char *) NULL, pfbadname);
  
  q = getpwuid (getuid ());
  if (q == NULL)
    {
      ulog (LOG_ERROR, "Can't get home directory");
      return NULL;
    }

  if (zfile[1] == '\0')
    return zbufcpy (q->pw_dir);

  return zsysdep_in_dir (q->pw_dir, zfile + 2);
}
