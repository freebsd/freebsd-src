/* proctm.c
   Get the time spent in the process.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

/* Prefer gettimeofday to ftime to times.  */

#if HAVE_GETTIMEOFDAY || HAVE_FTIME
#undef HAVE_TIMES
#define HAVE_TIMES 0
#endif

#if HAVE_GETTIMEOFDAY
#undef HAVE_FTIME
#define HAVE_FTIME 0
#endif

#if HAVE_TIME_H && (TIME_WITH_SYS_TIME || ! HAVE_GETTIMEOFDAY)
#include <time.h>
#endif

#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif

#if HAVE_FTIME
#include <sys/timeb.h>
#endif

#if HAVE_TIMES

#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#if TIMES_DECLARATION_OK
/* We use a macro to protect this because times really returns clock_t
   and on some systems, such as Ultrix 4.0, clock_t is int.  We don't
   leave it out entirely because on some systems, such as System III,
   the declaration is necessary for correct compilation.  */
#ifndef times
extern long times ();
#endif
#endif /* TIMES_DECLARATION_OK */

#ifdef _SC_CLK_TCK
#define HAVE_SC_CLK_TCK 1
#else
#define HAVE_SC_CLK_TCK 0
#endif

/* TIMES_TICK may have been set in policy.h, or we may be able to get
   it using sysconf.  If neither is the case, try to find a useful
   definition from the system header files.  */
#if TIMES_TICK == 0 && (! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK)
#ifdef CLK_TCK
#undef TIMES_TICK
#define TIMES_TICK CLK_TCK
#else /* ! defined (CLK_TCK) */
#ifdef HZ
#undef TIMES_TICK
#define TIMES_TICK HZ
#endif /* defined (HZ) */
#endif /* ! defined (CLK_TCK) */
#endif /* TIMES_TICK == 0 && (! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK) */

#endif /* HAVE_TIMES */

#ifndef time
extern time_t time ();
#endif
#if HAVE_SYSCONF
#ifndef sysconf
extern long sysconf ();
#endif
#endif

/* Get the time in seconds and microseconds; this need only work
   within the process when called from the system independent code.
   It is also called by ixsysdep_time.  */

long
ixsysdep_process_time (pimicros)
     long *pimicros;
{
#if HAVE_GETTIMEOFDAY
  struct timeval stime;
  struct timezone stz;

  (void) gettimeofday (&stime, &stz);
  if (pimicros != NULL)
    *pimicros = (long) stime.tv_usec;
  return (long) stime.tv_sec;
#endif /* HAVE_GETTIMEOFDAY */

#if HAVE_FTIME
  static boolean fbad;

  if (! fbad)
    {
      struct timeb stime;
      static struct timeb slast;

      (void) ftime (&stime);

      /* On some systems, such as SCO 3.2.2, ftime can go backwards in
	 time.  If we detect this, we switch to using time.  */
      if (slast.time != 0
	  && (stime.time < slast.time
	      || (stime.time == slast.time &&
		  stime.millitm < slast.millitm)))
	fbad = TRUE;
      else
	{
	  slast = stime;
	  if (pimicros != NULL)
	    *pimicros = (long) stime.millitm * (long) 1000;
	  return (long) stime.time;
	}
    }

  if (pimicros != NULL)
    *pimicros = 0;
  return (long) time ((time_t *) NULL);
#endif /* HAVE_FTIME */

#if HAVE_TIMES
  struct tms s;
  long i;
  static int itick;

  if (itick == 0)
    {
#if TIMES_TICK == 0
#if HAVE_SYSCONF && HAVE_SC_CLK_TCK
      itick = (int) sysconf (_SC_CLK_TCK);
#else /* ! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK */
      const char *z;

      z = getenv ("HZ");
      if (z != NULL)
	itick = (int) strtol (z, (char **) NULL, 10);

      /* If we really couldn't get anything, just use 60.  */
      if (itick == 0)
	itick = 60;
#endif /* ! HAVE_SYSCONF || ! HAVE_SC_CLK_TCK */
#else /* TIMES_TICK != 0 */
      itick = TIMES_TICK;
#endif /* TIMES_TICK == 0 */
    }

  i = (long) times (&s);
  if (pimicros != NULL)
    *pimicros = (i % (long) itick) * ((long) 1000000 / (long) itick);
  return i / (long) itick;
#endif /* HAVE_TIMES */

#if ! HAVE_GETTIMEOFDAY && ! HAVE_FTIME && ! HAVE_TIMES
  if (pimicros != NULL)
    *pimicros = 0;
  return (long) time ((time_t *) NULL);
#endif /* ! HAVE_GETTIMEOFDAY && ! HAVE_FTIME && ! HAVE_TIMES  */
}
