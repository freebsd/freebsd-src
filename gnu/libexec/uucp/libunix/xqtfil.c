/* xqtfil.c
   Routines to read execute files.

   Copyright (C) 1991, 1992, 1993, 1995 Ian Lance Taylor

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
const char xqtfil_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libunix/xqtfil.c,v 1.7 1999/08/27 23:33:11 peter Exp $";
#endif

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

#if HAVE_OPENDIR
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* ! HAVE_DIRENT_H */
#include <sys/dir.h>
#define dirent direct
#endif /* ! HAVE_DIRENT_H */
#endif /* HAVE_OPENDIR */

/* Under the V2 or BSD42 spool directory scheme, all execute files are
   in the main spool directory.  Under the BSD43 scheme, they are all
   in the directory X..  Under the HDB or SVR4 scheme, they are in
   directories named after systems.  Under the ULTRIX scheme, they are
   in X.  subdirectories of subdirectories of sys.  Under the TAYLOR
   scheme, they are all in the subdirectory X. of a directory named
   after the system.

   This means that for HDB, ULTRIX, SVR4 or TAYLOR, we have to search
   directories of directories.  */

#if SPOOLDIR_V2 || SPOOLDIR_BSD42
#define ZDIR "."
#define SUBDIRS 0
#endif
#if SPOOLDIR_HDB || SPOOLDIR_SVR4 || SPOOLDIR_TAYLOR
#define ZDIR "."
#define SUBDIRS 1
#endif
#if SPOOLDIR_ULTRIX
#define ZDIR "sys"
#define SUBDIRS 1
#endif
#if SPOOLDIR_BSD43
#define ZDIR "X."
#define SUBDIRS 0
#endif

/* Static variables for the execute file scan.  */

static DIR *qSxqt_topdir;
#if ! SUBDIRS
static const char *zSdir;
#else /* SUBDIRS */
static boolean fSone_dir;
static char *zSdir;
static DIR *qSxqt_dir;
static char *zSsystem;
#endif /* SUBDIRS */

/* Initialize the scan for execute files.  The function
   usysdep_get_xqt_free will clear the data out when we are done with
   the system.  This returns FALSE on error.  */

/*ARGSUSED*/
boolean
fsysdep_get_xqt_init (zsystem)
     const char *zsystem;
{
  usysdep_get_xqt_free ((const char *) NULL);

#if SUBDIRS
  if (zsystem != NULL)
    {
#if SPOOLDIR_HDB || SPOOLDIR_SVR4
      zSdir = zbufcpy (zsystem);
#endif
#if SPOOLDIR_ULTRIX
      zSdir = zsappend3 ("sys", zsystem, "X.");
#endif
#if SPOOLDIR_TAYLOR
      zSdir = zsysdep_in_dir (zsystem, "X.");
#endif

      qSxqt_dir = opendir ((char *) zSdir);
      if (qSxqt_dir != NULL)
	{
	  qSxqt_topdir = qSxqt_dir;
	  fSone_dir = TRUE;
	  zSsystem = zbufcpy (zsystem);
	  return TRUE;
	}
    }

  fSone_dir = FALSE;
#endif

  qSxqt_topdir = opendir ((char *) ZDIR);
  if (qSxqt_topdir == NULL)
    {
      if (errno == ENOENT)
	return TRUE;
      ulog (LOG_ERROR, "opendir (%s): %s", ZDIR, strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/* Return the name of the next execute file to read and process.  If
   this returns NULL, *pferr must be checked.  If will be TRUE on
   error, FALSE if there are no more files.  On a successful return
   *pzsystem will be set to the system for which the execute file was
   created.  */

/*ARGSUSED*/
char *
zsysdep_get_xqt (zsystem, pzsystem, pferr)
     const char *zsystem;
     char **pzsystem;
     boolean *pferr;
{
  *pferr = FALSE;

  if (qSxqt_topdir == NULL)
    return NULL;

  /* This loop continues until we find a file.  */
  while (TRUE)
    {
      DIR *qdir;
      struct dirent *q;

#if ! SUBDIRS
      zSdir = ZDIR;
      qdir = qSxqt_topdir;
#else /* SUBDIRS */
      /* This loop continues until we find a subdirectory to read.  */
      while (qSxqt_dir == NULL)
	{
	  struct dirent *qtop;

	  qtop = readdir (qSxqt_topdir);
	  if (qtop == NULL)
	    {
	      (void) closedir (qSxqt_topdir);
	      qSxqt_topdir = NULL;
	      return NULL;
	    }

	  /* No system name may start with a dot This allows us to
	     quickly skip impossible directories.  */
	  if (qtop->d_name[0] == '.')
	    continue;

	  DEBUG_MESSAGE1 (DEBUG_SPOOLDIR,
			  "zsysdep_get_xqt: Found %s in top directory",
			  qtop->d_name);

	  ubuffree (zSdir);

#if SPOOLDIR_HDB || SPOOLDIR_SVR4
	  zSdir = zbufcpy (qtop->d_name);
#endif
#if SPOOLDIR_ULTRIX
	  zSdir = zsappend3 ("sys", qtop->d_name, "X.");
#endif
#if SPOOLDIR_TAYLOR
	  zSdir = zsysdep_in_dir (qtop->d_name, "X.");
#endif

	  ubuffree (zSsystem);
	  zSsystem = zbufcpy (qtop->d_name);

	  qSxqt_dir = opendir (zSdir);

	  if (qSxqt_dir == NULL
	      && errno != ENOTDIR
	      && errno != ENOENT)
	    ulog (LOG_ERROR, "opendir (%s): %s", zSdir, strerror (errno));
	}

      qdir = qSxqt_dir;
#endif /* SUBDIRS */

      q = readdir (qdir);

#if DEBUG > 1
      if (q != NULL)
	DEBUG_MESSAGE2 (DEBUG_SPOOLDIR,
			"zsysdep_get_xqt: Found %s in subdirectory %s",
			q->d_name, zSdir);
#endif

      /* If we've found an execute file, return it.  We have to get
	 the system name, which is easy for HDB or TAYLOR.  For other
	 spool directory schemes, we have to pull it out of the X.
	 file name; this would be insecure, except that zsfind_file
	 clobbers the file name to include the real system name.  */
      if (q != NULL
	  && q->d_name[0] == 'X'
	  && q->d_name[1] == '.')
	{
	  char *zret;

#if SPOOLDIR_HDB || SPOOLDIR_SVR4 || SPOOLDIR_TAYLOR
	  *pzsystem = zbufcpy (zSsystem);
#else
	  {
	    size_t clen;

	    clen = strlen (q->d_name) - 7;
	    *pzsystem = zbufalc (clen + 1);
	    memcpy (*pzsystem, q->d_name + 2, clen);
	    (*pzsystem)[clen] = '\0';
	  }
#endif

	  zret = zsysdep_in_dir (zSdir, q->d_name);
#if DEBUG > 1
	  DEBUG_MESSAGE2 (DEBUG_SPOOLDIR,
			  "zsysdep_get_xqt: Returning %s (system %s)",
			  zret, *pzsystem);
#endif
	  return zret;
	}
	    
      /* If we've reached the end of the directory, then if we are
	 using subdirectories loop around to read the next one,
	 otherwise we are finished.  */
      if (q == NULL)
	{
	  (void) closedir (qdir);

#if SUBDIRS
	  qSxqt_dir = NULL;
	  if (! fSone_dir)
	    continue;
#endif

	  qSxqt_topdir = NULL;
	  return NULL;
	}
    }
}

/* Free up the results of an execute file scan, when we're done with
   this system.  */

/*ARGSUSED*/
void
usysdep_get_xqt_free (zsystem)
     const char *zsystem;
{
  if (qSxqt_topdir != NULL)
    {
      (void) closedir (qSxqt_topdir);
      qSxqt_topdir = NULL;
    }
#if SUBDIRS
  if (qSxqt_dir != NULL)
    {
      (void) closedir (qSxqt_dir);
      qSxqt_dir = NULL;
    }
  ubuffree (zSdir);
  zSdir = NULL;
  ubuffree (zSsystem);
  zSsystem = NULL;
  fSone_dir = FALSE;
#endif
}
