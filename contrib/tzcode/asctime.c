/* asctime a la ISO C.  */

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson.
*/

/*
** Avoid the temptation to punt entirely to strftime;
** the output of strftime is supposed to be locale specific
** whereas the output of asctime is supposed to be constant.
*/

/*LINTLIBRARY*/

#include "namespace.h"
#include "private.h"
#include "un-namespace.h"
#include <stdio.h>

/*
** All years associated with 32-bit time_t values are exactly four digits long;
** some years associated with 64-bit time_t values are not.
** Vintage programs are coded for years that are always four digits long
** and may assume that the newline always lands in the same place.
** For years that are less than four digits, we pad the output with
** leading zeroes to get the newline in the traditional place.
** The -4 ensures that we get four characters of output even if
** we call a strftime variant that produces fewer characters for some years.
** This conforms to recent ISO C and POSIX standards, which say behavior
** is undefined when the year is less than 1000 or greater than 9999.
*/
static char const ASCTIME_FMT[] = "%s %s%3d %.2d:%.2d:%.2d %-4s\n";
/*
** For years that are more than four digits we put extra spaces before the year
** so that code trying to overwrite the newline won't end up overwriting
** a digit within a year and truncating the year (operating on the assumption
** that no output is better than wrong output).
*/
static char const ASCTIME_FMT_B[] = "%s %s%3d %.2d:%.2d:%.2d     %s\n";

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

/* A similar buffer for ctime.
   C89 requires that they be the same buffer.
   This requirement was removed in C99, so support it only if requested,
   as support is more likely to lead to bugs in badly written programs.  */
#if SUPPORT_C89
# define buf_ctime buf_asctime
#else
static char buf_ctime[sizeof buf_asctime];
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
	const char *	wn;
	const char *	mn;
	char			year[INT_STRLEN_MAXIMUM(int) + 2];
	char result[sizeof buf_asctime];

	if (timeptr == NULL) {
		errno = EINVAL;
		return strcpy(buf, "??? ??? ?? ??:??:?? ????\n");
	}
	if (timeptr->tm_wday < 0 || timeptr->tm_wday >= DAYSPERWEEK)
		wn = "???";
	else	wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >= MONSPERYEAR)
		mn = "???";
	else	mn = mon_name[timeptr->tm_mon];
	/*
	** Use strftime's %Y to generate the year, to avoid overflow problems
	** when computing timeptr->tm_year + TM_YEAR_BASE.
	** Assume that strftime is unaffected by other out-of-range members
	** (e.g., timeptr->tm_mday) when processing "%Y".
	*/
	strftime(year, sizeof year, "%Y", timeptr);
	/*
	** We avoid using snprintf since it's not available on all systems.
	*/
	sprintf(result,
		((strlen(year) <= 4) ? ASCTIME_FMT : ASCTIME_FMT_B),
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		year);
	if (strlen(result) < STD_ASCTIME_BUF_SIZE
	    || buf == buf_ctime || buf == buf_asctime)
		return strcpy(buf, result);
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
  return ctime_r(timep, buf_ctime);
}
