#if defined(__linux__) || defined(__MINT__)
# define _GNU_SOURCE /* strptime(), getsubopt() */
#endif

#include <time.h>

int
main(int argc, char **argv)
{
	struct tm tm;
	strptime(*argv, "%D", &tm);
	return 0;
}
