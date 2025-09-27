/* asctime a la ISO C.  */

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson.
*/

/*
** Avoid the temptation to punt entirely to strftime;
** strftime can behave badly when tm components are out of range, and
** the output of strftime is supposed to be locale specific
** whereas the output of asctime is supposed to be constant.
*/

/*LINTLIBRARY*/

#include "private.h"
#include <stdio.h>

enum { STD_ASCTIME_BUF_SIZE = 26 };
/*
** Big enough for something such as
** ??? ???-2147483648 -2147483648:-2147483648:-2147483648     -2147483648\n
** (two three-character abbreviations, five strings denoting integers,
** seven explicit spaces, two explicit colons, a newline,
** and a trailing NUL byte).
** The values above are for systems where an int is 32 bits and are provided
** as an example; the size expression below is a bound for the system at
** hand.
*/
static char buf_asctime[2*3 + 5*INT_STRLEN_MAXIMUM(int) + 7 + 2 + 1 + 1];

/* On pre-C99 platforms, a snprintf substitute good enough for us.  */
#if !HAVE_SNPRINTF
# include <stdarg.h>
ATTRIBUTE_FORMAT((printf, 3, 4)) static int
my_snprintf(char *s, size_t size, char const *format, ...)
{
  int n;
  va_list args;
  char stackbuf[sizeof buf_asctime];
  va_start(args, format);
  n = vsprintf(stackbuf, format, args);
  va_end (args);
  if (0 <= n && n < size)
    memcpy (s, stackbuf, n + 1);
  return n;
}
# undef snprintf
# define snprintf my_snprintf
#endif

/* Publish asctime_r and ctime_r only when supporting older POSIX.  */
#if SUPPORT_POSIX2008
# define asctime_static
#else
# define asctime_static static
# undef asctime_r
# undef ctime_r
# define asctime_r static_asctime_r
# define ctime_r static_ctime_r
#endif

asctime_static
char *
asctime_r(struct tm const *restrict timeptr, char *restrict buf)
{
	static const char	wday_name[][4] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	register const char *	wn;
	register const char *	mn;
	int year, mday, hour, min, sec;
	long long_TM_YEAR_BASE = TM_YEAR_BASE;
	size_t bufsize = (buf == buf_asctime
			  ? sizeof buf_asctime : STD_ASCTIME_BUF_SIZE);

	if (timeptr == NULL) {
		strcpy(buf, "??? ??? ?? ??:??:?? ????\n");
		/* Set errno now, since strcpy might change it in
		   POSIX.1-2017 and earlier.  */
		errno = EINVAL;
		return buf;
	}
	if (timeptr->tm_wday < 0 || timeptr->tm_wday >= DAYSPERWEEK)
		wn = "???";
	else	wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >= MONSPERYEAR)
		mn = "???";
	else	mn = mon_name[timeptr->tm_mon];

	year = timeptr->tm_year;
	mday = timeptr->tm_mday;
	hour = timeptr->tm_hour;
	min = timeptr->tm_min;
	sec = timeptr->tm_sec;

	/* Vintage programs are coded for years that are always four bytes long
	   and may assume that the newline always lands in the same place.
	   For years that are less than four bytes, pad the output with
	   leading zeroes to get the newline in the traditional place.
	   For years longer than four bytes, put extra spaces before the year
	   so that vintage code trying to overwrite the newline
	   won't overwrite a digit within a year and truncate the year,
	   using the principle that no output is better than wrong output.
	   This conforms to ISO C and POSIX standards, which say behavior
	   is undefined when the year is less than 1000 or greater than 9999.

	   Also, avoid overflow when formatting tm_year + TM_YEAR_BASE.  */

	if ((year <= LONG_MAX - TM_YEAR_BASE
	     ? snprintf (buf, bufsize,
			 ((-999 - TM_YEAR_BASE <= year
			   && year <= 9999 - TM_YEAR_BASE)
			  ? "%s %s%3d %.2d:%.2d:%.2d %04ld\n"
			  : "%s %s%3d %.2d:%.2d:%.2d     %ld\n"),
			 wn, mn, mday, hour, min, sec,
			 year + long_TM_YEAR_BASE)
	     : snprintf (buf, bufsize,
			 "%s %s%3d %.2d:%.2d:%.2d     %d%d\n",
			 wn, mn, mday, hour, min, sec,
			 year / 10 + TM_YEAR_BASE / 10,
			 year % 10))
	    < bufsize)
		return buf;
	else {
		errno = EOVERFLOW;
		return NULL;
	}
}

char *
asctime(register const struct tm *timeptr)
{
	return asctime_r(timeptr, buf_asctime);
}

asctime_static
char *
ctime_r(const time_t *timep, char *buf)
{
  struct tm mytm;
  struct tm *tmp = localtime_r(timep, &mytm);
  return tmp ? asctime_r(tmp, buf) : NULL;
}

char *
ctime(const time_t *timep)
{
  /* Do not call localtime_r, as C23 requires ctime to initialize the
     static storage that localtime updates.  */
  struct tm *tmp = localtime(timep);
  return tmp ? asctime(tmp) : NULL;
}
