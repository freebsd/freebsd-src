#if defined(__linux__) || defined(__MINT__)
# define _GNU_SOURCE /* getsubopt() */
#endif

#include <stdlib.h>

int
main(int argc, char **argv)
{
	getsubopt(argv, argv, argv);
	return 0;
}
