/* Convert a `struct tm' to a time_t value.
   Copyright (C) 1993, 94, 95, 96, 97, 98, 99 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Paul Eggert (eggert@twinsun.com).

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Define this to have a standalone program to test this implementation of
   mktime.  */
/* #define DEBUG 1 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _LIBC
# define HAVE_LIMITS_H 1
# define STDC_HEADERS 1
#endif

/* Assume that leap seconds are possible, unless told otherwise.
   If the host has a `zic' command with a `-L leapsecondfilename' option,
   then it supports leap seconds; otherwise it probably doesn't.  */
#ifndef LEAP_SECONDS_POSSIBLE
# define LEAP_SECONDS_POSSIBLE 1
#endif

#include <sys/types.h>		/* Some systems define `time_t' here.  */
#include <time.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#if DEBUG
# include <stdio.h>
# if STDC_HEADERS
#  include <stdlib.h>
# endif
/* Make it work even if the system's libc has its own mktime routine.  */
# define mktime my_mktime
#endif /* DEBUG */

#ifndef __P
# if defined __GNUC__ || (defined __STDC__ && __STDC__)
#  define __P(args) args
# else
#  define __P(args) ()
# endif  /* GCC.  */
#endif  /* Not __P.  */

#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif

/* The extra casts work around common compiler bugs.  */
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
/* The outer cast is needed to work around a bug in Cray C 5.0.3.0.
   It is necessary at least when t == time_t.  */
#define TYPE_MINIMUM(t) ((t) (TYPE_SIGNED (t) \
			      ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) : (t) 0))
#define TYPE_MAXIMUM(t) ((t) (~ (t) 0 - TYPE_MINIMUM (t)))

#ifndef INT_MIN
# define INT_MIN TYPE_MINIMUM (int)
#endif
#ifndef INT_MAX
# define INT_MAX TYPE_MAXIMUM (int)
#endif

#ifndef TIME_T_MIN
# define TIME_T_MIN TYPE_MINIMUM (time_t)
#endif
#ifndef TIME_T_MAX
# define TIME_T_MAX TYPE_MAXIMUM (time_t)
#endif

#define TM_YEAR_BASE 1900
#define EPOCH_YEAR 1970

#ifndef __isleap
/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
# define __isleap(year)	\
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif

/* How many days come before each month (0-12).  */
const unsigned short int __mon_yday[2][13] =
  {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };


#ifdef _LIBC
# define my_mktime_localtime_r __localtime_r
#else
/* If we're a mktime substitute in a GNU program, then prefer
   localtime to localtime_r, since many localtime_r implementations
   are buggy.  */
static struct tm *
my_mktime_localtime_r (const time_t *t, struct tm *tp)
{
  struct tm *l = localtime (t);
  if (! l)
    return 0;
  *tp = *l;
  return tp;
}
#endif /* ! _LIBC */


/* Yield the difference between (YEAR-YDAY HOUR:MIN:SEC) and (*TP),
   measured in seconds, ignoring leap seconds.
   YEAR uses the same numbering as TM->tm_year.
   All values are in range, except possibly YEAR.
   If TP is null, return a nonzero value.
   If overflow occurs, yield the low order bits of the correct answer.  */
static time_t
ydhms_tm_diff (int year, int yday, int hour, int min, int sec,
	       const struct tm *tp)
{
  if (!tp)
    return 1;
  else
    {
      /* Compute intervening leap days correctly even if year is negative.
	 Take care to avoid int overflow.  time_t overflow is OK, since
	 only the low order bits of the correct time_t answer are needed.
	 Don't convert to time_t until after all divisions are done, since
	 time_t might be unsigned.  */
      int a4 = (year >> 2) + (TM_YEAR_BASE >> 2) - ! (year & 3);
      int b4 = (tp->tm_year >> 2) + (TM_YEAR_BASE >> 2) - ! (tp->tm_year & 3);
      int a100 = a4 / 25 - (a4 % 25 < 0);
      int b100 = b4 / 25 - (b4 % 25 < 0);
      int a400 = a100 >> 2;
      int b400 = b100 >> 2;
      int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
      time_t years = year - (time_t) tp->tm_year;
      time_t days = (365 * years + intervening_leap_days
		     + (yday - tp->tm_yday));
      return (60 * (60 * (24 * days + (hour - tp->tm_hour))
		    + (min - tp->tm_min))
	      + (sec - tp->tm_sec));
    }
}

/* Use CONVERT to convert *T to a broken down time in *TP.
   If *T is out of range for conversion, adjust it so that
   it is the nearest in-range value and then convert that.  */
static struct tm *
ranged_convert (struct tm *(*convert) (const time_t *, struct tm *),
		time_t *t, struct tm *tp)
{
  struct tm *r;

  if (! (r = (*convert) (t, tp)) && *t)
    {
      time_t bad = *t;
      time_t ok = 0;
      struct tm tm;

      /* BAD is a known unconvertible time_t, and OK is a known good one.
	 Use binary search to narrow the range between BAD and OK until
	 they differ by 1.  */
      while (bad != ok + (bad < 0 ? -1 : 1))
	{
	  time_t mid = *t = (bad < 0
			     ? bad + ((ok - bad) >> 1)
			     : ok + ((bad - ok) >> 1));
	  if ((r = (*convert) (t, tp)))
	    {
	      tm = *r;
	      ok = mid;
	    }
	  else
	    bad = mid;
	}

      if (!r && ok)
	{
	  /* The last conversion attempt failed;
	     revert to the most recent successful attempt.  */
	  *t = ok;
	  *tp = tm;
	  r = tp;
	}
    }

  return r;
}


/* Convert *TP to a time_t value, inverting
   the monotonic and mostly-unit-linear conversion function CONVERT.
   Use *OFFSET to keep track of a guess at the offset of the result,
   compared to what the result would be for UTC without leap seconds.
   If *OFFSET's guess is correct, only one CONVERT call is needed.  */
time_t
__mktime_internal (struct tm *tp,
		   struct tm *(*convert) (const time_t *, struct tm *),
		   time_t *offset)
{
  time_t t, dt, t0, t1, t2;
  struct tm tm;

  /* The maximum number of probes (calls to CONVERT) should be enough
     to handle any combinations of time zone rule changes, solar time,
     leap seconds, and oscillations around a spring-forward gap.
     POSIX.1 prohibits leap seconds, but some hosts have them anyway.  */
  int remaining_probes = 6;

  /* Time requested.  Copy it in case CONVERT modifies *TP; this can
     occur if TP is localtime's returned value and CONVERT is localtime.  */
  int sec = tp->tm_sec;
  int min = tp->tm_min;
  int hour = tp->tm_hour;
  int mday = tp->tm_mday;
  int mon = tp->tm_mon;
  int year_requested = tp->tm_year;
  int isdst = tp->tm_isdst;

  /* Ensure that mon is in range, and set year accordingly.  */
  int mon_remainder = mon % 12;
  int negative_mon_remainder = mon_remainder < 0;
  int mon_years = mon / 12 - negative_mon_remainder;
  int year = year_requested + mon_years;

  /* The other values need not be in range:
     the remaining code handles minor overflows correctly,
     assuming int and time_t arithmetic wraps around.
     Major overflows are caught at the end.  */

  /* Calculate day of year from year, month, and day of month.
     The result need not be in range.  */
  int yday = ((__mon_yday[__isleap (year + TM_YEAR_BASE)]
	       [mon_remainder + 12 * negative_mon_remainder])
	      + mday - 1);

  int sec_requested = sec;
#if LEAP_SECONDS_POSSIBLE
  /* Handle out-of-range seconds specially,
     since ydhms_tm_diff assumes every minute has 60 seconds.  */
  if (sec < 0)
    sec = 0;
  if (59 < sec)
    sec = 59;
#endif

  /* Invert CONVERT by probing.  First assume the same offset as last time.
     Then repeatedly use the error to improve the guess.  */

  tm.tm_year = EPOCH_YEAR - TM_YEAR_BASE;
  tm.tm_yday = tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
  t0 = ydhms_tm_diff (year, yday, hour, min, sec, &tm);

  for (t = t1 = t2 = t0 + *offset;
       (dt = ydhms_tm_diff (year, yday, hour, min, sec,
			    ranged_convert (convert, &t, &tm)));
       t1 = t2, t2 = t, t += dt)
    if (t == t1 && t != t2
	&& (isdst < 0 || tm.tm_isdst < 0
	    || (isdst != 0) != (tm.tm_isdst != 0)))
      /* We can't possibly find a match, as we are oscillating
	 between two values.  The requested time probably falls
	 within a spring-forward gap of size DT.  Follow the common
	 practice in this case, which is to return a time that is DT
	 away from the requested time, preferring a time whose
	 tm_isdst differs from the requested value.  In practice,
	 this is more useful than returning -1.  */
      break;
    else if (--remaining_probes == 0)
      return -1;

  /* If we have a match, check whether tm.tm_isdst has the requested
     value, if any.  */
  if (dt == 0 && isdst != tm.tm_isdst && 0 <= isdst && 0 <= tm.tm_isdst)
    {
      /* tm.tm_isdst has the wrong value.  Look for a neighboring
	 time with the right value, and use its UTC offset.
	 Heuristic: probe the previous three calendar quarters (approximately),
	 looking for the desired isdst.  This isn't perfect,
	 but it's good enough in practice.  */
      int quarter = 7889238; /* seconds per average 1/4 Gregorian year */
      int i;

      /* If we're too close to the time_t limit, look in future quarters.  */
      if (t < TIME_T_MIN + 3 * quarter)
	quarter = -quarter;

      for (i = 1; i <= 3; i++)
	{
	  time_t ot = t - i * quarter;
	  struct tm otm;
	  ranged_convert (convert, &ot, &otm);
	  if (otm.tm_isdst == isdst)
	    {
	      /* We found the desired tm_isdst.
		 Extrapolate back to the desired time.  */
	      t = ot + ydhms_tm_diff (year, yday, hour, min, sec, &otm);
	      ranged_convert (convert, &t, &tm);
	      break;
	    }
	}
    }

  *offset = t - t0;

#if LEAP_SECONDS_POSSIBLE
  if (sec_requested != tm.tm_sec)
    {
      /* Adjust time to reflect the tm_sec requested, not the normalized value.
	 Also, repair any damage from a false match due to a leap second.  */
      t += sec_requested - sec + (sec == 0 && tm.tm_sec == 60);
      if (! (*convert) (&t, &tm))
	return -1;
    }
#endif

  if (TIME_T_MAX / INT_MAX / 366 / 24 / 60 / 60 < 3)
    {
      /* time_t isn't large enough to rule out overflows in ydhms_tm_diff,
	 so check for major overflows.  A gross check suffices,
	 since if t has overflowed, it is off by a multiple of
	 TIME_T_MAX - TIME_T_MIN + 1.  So ignore any component of
	 the difference that is bounded by a small value.  */

      double dyear = (double) year_requested + mon_years - tm.tm_year;
      double dday = 366 * dyear + mday;
      double dsec = 60 * (60 * (24 * dday + hour) + min) + sec_requested;

      /* On Irix4.0.5 cc, dividing TIME_T_MIN by 3 does not produce
	 correct results, ie., it erroneously gives a positive value
	 of 715827882.  Setting a variable first then doing math on it
	 seems to work.  (ghazi@caip.rutgers.edu) */

      const time_t time_t_max = TIME_T_MAX;
      const time_t time_t_min = TIME_T_MIN;

      if (time_t_max / 3 - time_t_min / 3 < (dsec < 0 ? - dsec : dsec))
	return -1;
    }

  *tp = tm;
  return t;
}


static time_t localtime_offset;

/* Convert *TP to a time_t value.  */
time_t
mktime (tp)
     struct tm *tp;
{
#ifdef _LIBC
  /* POSIX.1 8.1.1 requires that whenever mktime() is called, the
     time zone names contained in the external variable `tzname' shall
     be set as if the tzset() function had been called.  */
  __tzset ();
#endif

  return __mktime_internal (tp, my_mktime_localtime_r, &localtime_offset);
}

#ifdef weak_alias
weak_alias (mktime, timelocal)
#endif

#if DEBUG

static int
not_equal_tm (a, b)
     struct tm *a;
     struct tm *b;
{
  return ((a->tm_sec ^ b->tm_sec)
	  | (a->tm_min ^ b->tm_min)
	  | (a->tm_hour ^ b->tm_hour)
	  | (a->tm_mday ^ b->tm_mday)
	  | (a->tm_mon ^ b->tm_mon)
	  | (a->tm_year ^ b->tm_year)
	  | (a->tm_mday ^ b->tm_mday)
	  | (a->tm_yday ^ b->tm_yday)
	  | (a->tm_isdst ^ b->tm_isdst));
}

static void
print_tm (tp)
     struct tm *tp;
{
  if (tp)
    printf ("%04d-%02d-%02d %02d:%02d:%02d yday %03d wday %d isdst %d",
	    tp->tm_year + TM_YEAR_BASE, tp->tm_mon + 1, tp->tm_mday,
	    tp->tm_hour, tp->tm_min, tp->tm_sec,
	    tp->tm_yday, tp->tm_wday, tp->tm_isdst);
  else
    printf ("0");
}

static int
check_result (tk, tmk, tl, lt)
     time_t tk;
     struct tm tmk;
     time_t tl;
     struct tm *lt;
{
  if (tk != tl || !lt || not_equal_tm (&tmk, lt))
    {
      printf ("mktime (");
      print_tm (&tmk);
      printf (")\nyields (");
      print_tm (lt);
      printf (") == %ld, should be %ld\n", (long) tl, (long) tk);
      return 1;
    }

  return 0;
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int status = 0;
  struct tm tm, tmk, tml;
  struct tm *lt;
  time_t tk, tl;
  char trailer;

  if ((argc == 3 || argc == 4)
      && (sscanf (argv[1], "%d-%d-%d%c",
		  &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &trailer)
	  == 3)
      && (sscanf (argv[2], "%d:%d:%d%c",
		  &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &trailer)
	  == 3))
    {
      tm.tm_year -= TM_YEAR_BASE;
      tm.tm_mon--;
      tm.tm_isdst = argc == 3 ? -1 : atoi (argv[3]);
      tmk = tm;
      tl = mktime (&tmk);
      lt = localtime (&tl);
      if (lt)
	{
	  tml = *lt;
	  lt = &tml;
	}
      printf ("mktime returns %ld == ", (long) tl);
      print_tm (&tmk);
      printf ("\n");
      status = check_result (tl, tmk, tl, lt);
    }
  else if (argc == 4 || (argc == 5 && strcmp (argv[4], "-") == 0))
    {
      time_t from = atol (argv[1]);
      time_t by = atol (argv[2]);
      time_t to = atol (argv[3]);

      if (argc == 4)
	for (tl = from; tl <= to; tl += by)
	  {
	    lt = localtime (&tl);
	    if (lt)
	      {
		tmk = tml = *lt;
		tk = mktime (&tmk);
		status |= check_result (tk, tmk, tl, tml);
	      }
	    else
	      {
		printf ("localtime (%ld) yields 0\n", (long) tl);
		status = 1;
	      }
	  }
      else
	for (tl = from; tl <= to; tl += by)
	  {
	    /* Null benchmark.  */
	    lt = localtime (&tl);
	    if (lt)
	      {
		tmk = tml = *lt;
		tk = tl;
		status |= check_result (tk, tmk, tl, tml);
	      }
	    else
	      {
		printf ("localtime (%ld) yields 0\n", (long) tl);
		status = 1;
	      }
	  }
    }
  else
    printf ("Usage:\
\t%s YYYY-MM-DD HH:MM:SS [ISDST] # Test given time.\n\
\t%s FROM BY TO # Test values FROM, FROM+BY, ..., TO.\n\
\t%s FROM BY TO - # Do not test those values (for benchmark).\n",
	    argv[0], argv[0], argv[0]);

  return status;
}

#endif /* DEBUG */

/*
Local Variables:
compile-command: "gcc -DDEBUG -DHAVE_LIMITS_H -DSTDC_HEADERS -Wall -W -O -g mktime.c -o mktime"
End:
*/
