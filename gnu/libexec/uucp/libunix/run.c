/* run.c
   Run a program.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

/* Start up a new program and end the current one.  We don't have to
   worry about SIGHUP because the current process is either not a
   process group leader (uucp, uux) or it does not have a controlling
   terminal (uucico).  */

boolean
fsysdep_run (zprogram, zarg1, zarg2)
     const char *zprogram;
     const char *zarg1;
     const char *zarg2;
{
  char *zlib;
  const char *azargs[4];
  int aidescs[3];
  pid_t ipid;

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
      return FALSE;
    }

  return TRUE;
}
