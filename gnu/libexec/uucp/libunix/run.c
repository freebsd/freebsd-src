/* run.c
   Run a program.

   Copyright (C) 1992, 1993, 1994 Ian Lance Taylor

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

/* Start up a new program.  */

boolean
fsysdep_run (ffork, zprogram, zarg1, zarg2)
     boolean ffork;
     const char *zprogram;
     const char *zarg1;
     const char *zarg2;
{
  char *zlib;
  const char *azargs[4];
  int aidescs[3];
  pid_t ipid;

  /* If we are supposed to fork, fork and then spawn so that we don't
     have to worry about zombie processes.  */
  if (ffork)
    {
      ipid = ixsfork ();
      if (ipid < 0)
	{
	  ulog (LOG_ERROR, "fork: %s", strerror (errno));
	  return FALSE;
	}

      if (ipid != 0)
	{
	  /* This is the parent.  Wait for the child we just forked to
	     exit (below) and return.  */
	  (void) ixswait ((unsigned long) ipid, (const char *) NULL);

	  /* Force the log files to be reopened in case the child just
	     output any error messages and stdio doesn't handle
	     appending correctly.  */
	  ulog_close ();

	  return TRUE;
	}

      /* This is the child.  Detach from the terminal to avoid any
	 unexpected SIGHUP signals.  At this point we are definitely
	 not a process group leader, so usysdep_detach will not fork
	 again.  */
      usysdep_detach ();

      /* Now spawn the program and then exit.  */
    }

  zlib = zbufalc (sizeof SBINDIR + sizeof "/" + strlen (zprogram));
  sprintf (zlib, "%s/%s", SBINDIR, zprogram);

  azargs[0] = zlib;
  azargs[1] = zarg1;
  azargs[2] = zarg2;
  azargs[3] = NULL;

  aidescs[0] = SPAWN_NULL;
  aidescs[1] = SPAWN_NULL;
  aidescs[2] = SPAWN_NULL;

  /* We pass fsetuid and fshell as TRUE, which permits uucico and
     uuxqt to be replaced by (non-setuid) shell scripts.  */
  ipid = ixsspawn (azargs, aidescs, TRUE, FALSE, (const char *) NULL,
		   FALSE, TRUE, (const char *) NULL,
		   (const char *) NULL, (const char *) NULL);
  ubuffree (zlib);

  if (ipid < 0)
    {
      ulog (LOG_ERROR, "ixsspawn: %s", strerror (errno));
      if (ffork)
	_exit (EXIT_FAILURE);
      return FALSE;
    }

  if (ffork)
    _exit (EXIT_SUCCESS);

  return TRUE;
}
