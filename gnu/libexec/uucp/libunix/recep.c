/* recep.c
   See whether a file has already been received.

   Copyright (C) 1992, 1993, 1995 Ian Lance Taylor

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
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

#if HAVE_TIME_H
#include <time.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

static char *zsreceived_name P((const struct uuconf_system *qsys,
				const char *ztemp));

/* These routines are used to see whether we have already received a
   file in a previous UUCP connection.  It is possible for the
   acknowledgement of a received file to be lost.  The sending system
   will then not know that the file was correctly received, and will
   send it again.  This can be a problem particularly with protocols
   which support channels, since they may send several small files in
   a single window, all of which may be received correctly although
   the sending system never sees the acknowledgement.  If these files
   involve an execution, the execution will happen twice, which will
   be bad.

   We use a simple system.  For each file we want to remember, we
   create an empty file names .Received/SYS/TEMP, where SYS is the
   name of the system and TEMP is the name of the temporary file used
   by the sender.  If no temporary file is used by the sender, we
   don't remember that we received the file.  This is not perfect, but
   execution files will always have a temporary file, so the most
   important case is handled.  Also, any file received from Taylor
   UUCP 1.04 or greater will always have a temporary file.  */

/* Return the name we are going use for the marker, or NULL if we have
   no name.  */

static char *
zsreceived_name (qsys, ztemp)
     const struct uuconf_system *qsys;
     const char *ztemp;
{
  if (ztemp != NULL
      && *ztemp == 'D'
      && strcmp (ztemp, "D.0") != 0)
    return zsappend3 (".Received", qsys->uuconf_zname, ztemp);
  else
    return NULL;
}

/* Remember that we have already received a file.  */

/*ARGSUSED*/
boolean
fsysdep_remember_reception (qsys, zto, ztemp)
     const struct uuconf_system *qsys;
     const char *zto;
     const char *ztemp;
{
  char *zfile;
  int o;

  zfile = zsreceived_name (qsys, ztemp);
  if (zfile == NULL)
    return TRUE;
  o = creat (zfile, IPUBLIC_FILE_MODE);
  if (o < 0)
    {
      if (errno == ENOENT)
	{
	  if (fsysdep_make_dirs (zfile, FALSE))
	    {
	      ubuffree (zfile);
	      return FALSE;
	    }
	  o = creat (zfile, IPUBLIC_FILE_MODE);
	}
      if (o < 0)
	{
	  ulog (LOG_ERROR, "creat (%s): %s", zfile, strerror (errno));
	  ubuffree (zfile);
	  return FALSE;
	}
    }

  ubuffree (zfile);

  /* We don't have to actually put anything in the file; we just use
     the name.  This is more convenient than keeping a file with a
     list of names.  */
  if (close (o) < 0)
    {
      ulog (LOG_ERROR, "fsysdep_remember_reception: close: %s",
	    strerror (errno));
      return FALSE;
    }

  return TRUE;
}

/* The number of seconds in one week.  We must cast to long for this
   to be calculated correctly on a machine with 16 bit ints.  */
#define SECS_PER_WEEK ((long) 7 * (long) 24 * (long) 60 * (long) 60)

/* See if we have already received a file.  Note that don't delete the
   marker file here, because we need to know that the sending system
   has received our denial first.  This function returns TRUE if the
   file has already been received, FALSE if it has not.  */

/*ARGSUSED*/
boolean
fsysdep_already_received (qsys, zto, ztemp)
     const struct uuconf_system *qsys;
     const char *zto;
     const char *ztemp;
{
  char *zfile;
  struct stat s;
  boolean fret;

  zfile = zsreceived_name (qsys, ztemp);
  if (zfile == NULL)
    return FALSE;
  if (stat (zfile, &s) < 0)
    {
      if (errno != ENOENT)
	ulog (LOG_ERROR, "stat (%s): %s", zfile, strerror (errno));
      ubuffree (zfile);
      return FALSE;
    }

  /* Ignore the file (return FALSE) if it is over one week old.  */
  fret = s.st_mtime + SECS_PER_WEEK >= time ((time_t *) NULL);

  if (fret)
    DEBUG_MESSAGE1 (DEBUG_SPOOLDIR, "fsysdep_already_received: Found %s",
		    zfile);

  ubuffree (zfile);

  return fret;
}

/* Forget that we have received a file.  */

/*ARGSUSED*/
boolean
fsysdep_forget_reception (qsys, zto, ztemp)
     const struct uuconf_system *qsys;
     const char *zto;
     const char *ztemp;
{
  char *zfile;

  zfile = zsreceived_name (qsys, ztemp);
  if (zfile == NULL)
    return TRUE;
  if (remove (zfile) < 0
      && errno != ENOENT)
    {
      ulog (LOG_ERROR, "remove (%s): %s", zfile, strerror (errno));
      ubuffree (zfile);
      return FALSE;
    }
  return TRUE;
}
