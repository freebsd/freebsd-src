#if defined(__linux__) || defined(__MINT__)
# define _GNU_SOURCE /* strptime() */
#endif

#include <time.h>

int
main(void)
{
	struct tm tm;
	const char input[] = "2014-01-04";
	return( ! (input+10 == strptime(input, "%Y-%m-%d", &tm) &&
	    114 == tm.tm_year && 0 == tm.tm_mon && 4 == tm.tm_mday));
}
