/* mail.c
   Send mail to a user.

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

#if HAVE_TIME_H
#include <time.h>
#endif

#ifndef ctime
extern char *ctime ();
#endif

/* Mail a message to a user.  */

boolean
fsysdep_mail (zto, zsubject, cstrs, paz)
     const char *zto;
     const char *zsubject;
     int cstrs;
     const char **paz;
{
  const char *az[3];
  FILE *e;
  pid_t ipid;
  time_t itime;
  int i;

  az[0] = MAIL_PROGRAM;
  az[1] = zto;
  az[2] = NULL;

  e = espopen (az, FALSE, &ipid);
  if (e == NULL)
    {
      ulog (LOG_ERROR, "espopen (%s): %s", MAIL_PROGRAM,
	    strerror (errno));
      return FALSE;
    }

  fprintf (e, "Subject: %s\n", zsubject);
  fprintf (e, "To: %s\n", zto);

  fprintf (e, "\n");

  (void) time (&itime);
  /* Remember that ctime includes a \n, so this skips a line.  */
  fprintf (e, "Message from UUCP on %s %s\n", zSlocalname,
	   ctime (&itime));

  for (i = 0; i < cstrs; i++)
    fputs (paz[i], e);

  (void) fclose (e);

  return ixswait ((unsigned long) ipid, MAIL_PROGRAM) == 0;
}
