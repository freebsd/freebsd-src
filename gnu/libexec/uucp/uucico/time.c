/* time.c
   Routines to deal with UUCP time spans.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char time_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/uucico/time.c,v 1.6 1999/08/27 23:33:51 peter Exp $";
#endif

#include <ctype.h>

#if TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "uudefs.h"
#include "uuconf.h"

/* External functions.  */
#ifndef time
extern time_t time ();
#endif
#ifndef localtime
extern struct tm *localtime ();
#endif

/* See if the current time matches a time span.  If it does, return
   TRUE, set *pival to the value for the matching span, and set
   *pcretry to the retry for the matching span.  Otherwise return
   FALSE.  */

boolean
ftimespan_match (qspan, pival, pcretry)
     const struct uuconf_timespan *qspan;
     long *pival;
     int *pcretry;
{
  time_t inow;
  struct tm *qtm;
  int itm;
  const struct uuconf_timespan *q;

  if (qspan == NULL)
    return FALSE;

  time (&inow);
  qtm = localtime (&inow);

  /* Get the number of minutes since Sunday for the time.  */
  itm = qtm->tm_wday * 24 * 60 + qtm->tm_hour * 60 + qtm->tm_min;

  for (q = qspan; q != NULL; q = q->uuconf_qnext)
    {
      if (q->uuconf_istart <= itm && itm <= q->uuconf_iend)
	{
	  if (pival != NULL)
	    *pival = q->uuconf_ival;
	  if (pcretry != NULL)
	    *pcretry = q->uuconf_cretry;
	  return TRUE;
	}
    }

  return FALSE;
}

/* Determine the maximum size that may ever be transferred, according
   to a timesize span.  This returns -1 if there is no limit.  */

long
cmax_size_ever (qtimesize)
     const struct uuconf_timespan *qtimesize;
{
  long imax;
  const struct uuconf_timespan *q;

  if (qtimesize == NULL)
    return -1;

  /* Look through the list of spans.  If there is any gap larger than
     1 hour, we assume there are no restrictions.  Otherwise we keep
     track of the largest value we see.  I picked 1 hour arbitrarily,
     on the theory that a 1 hour span to transfer large files might
     actually occur, and is probably not an accident.  */
  if (qtimesize->uuconf_istart >= 60)
    return -1;

  imax = -1;

  for (q = qtimesize; q != NULL; q = q->uuconf_qnext)
    {
      if (q->uuconf_qnext == NULL)
	{
	  if (q->uuconf_iend <= 6 * 24 * 60 + 23 * 60)
	    return -1;
	}
      else
	{
	  if (q->uuconf_iend + 60 <= q->uuconf_qnext->uuconf_istart)
	    return -1;
	}

      if (imax < q->uuconf_ival)
	imax = q->uuconf_ival;
    }

  return imax;
}
