/* splcmd.c
   Spool a command.

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
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>
#include <ctype.h>

/* Given a set of commands to execute for a remote system, create a
   command file holding them.  This creates a single command file
   holding all the commands passed in.  It returns a jobid.  */

char *
zsysdep_spool_commands (qsys, bgrade, ccmds, pascmds)
     const struct uuconf_system *qsys;
     int bgrade;
     int ccmds;
     const struct scmd *pascmds;
{
  char *z;
  FILE *e;
  int i;
  const struct scmd *q;
  char *zjobid;

#if DEBUG > 0
  if (! UUCONF_GRADE_LEGAL (bgrade))
    ulog (LOG_FATAL, "Bad grade %d", bgrade);
#endif

  z = zscmd_file (qsys, bgrade);
  if (z == NULL)
    return NULL;

  e = esysdep_fopen (z, FALSE, FALSE, TRUE);
  if (e == NULL)
    {
      ubuffree (z);
      return NULL;
    }

  for (i = 0, q = pascmds; i < ccmds; i++, q++)
    {
      switch (q->bcmd)
	{
	case 'S':
	  fprintf (e, "S %s %s %s -%s %s 0%o %s\n", q->zfrom, q->zto,
		   q->zuser, q->zoptions, q->ztemp, q->imode,
		   q->znotify == NULL ? (const char *) "" : q->znotify);
	  break;
	case 'R':
	  fprintf (e, "R %s %s %s -%s\n", q->zfrom, q->zto, q->zuser,
		   q->zoptions);
	  break;
	case 'X':
	  fprintf (e, "X %s %s %s -%s\n", q->zfrom, q->zto, q->zuser,
		   q->zoptions);
	  break;
	case 'E':
	  fprintf (e, "E %s %s %s -%s %s 0%o %s 0 %s\n", q->zfrom, q->zto,
		   q->zuser, q->zoptions, q->ztemp, q->imode,
		   q->znotify, q->zcmd);
	  break;
	default:
	  ulog (LOG_ERROR,
		"zsysdep_spool_commands: Unrecognized type %d",
		q->bcmd);
	  (void) fclose (e);
	  (void) remove (z);
	  ubuffree (z);
	  return NULL;
	}
    }

  if (fclose (e) != 0)
    {
      ulog (LOG_ERROR, "fclose: %s", strerror (errno));
      (void) remove (z);
      ubuffree (z);
      return NULL;
    }

  zjobid = zsfile_to_jobid (qsys, z, bgrade);
  if (zjobid == NULL)
    (void) remove (z);
  ubuffree (z);
  return zjobid;
}
