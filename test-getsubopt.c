#if defined(__linux__) || defined(__MINT__)
# define _GNU_SOURCE /* getsubopt() */
#endif

#include <stdlib.h>

extern char *suboptarg;

int
main(void)
{
	char buf[] = "k=v";
	char *options = buf;
	char token0[] = "k";
	char *const tokens[] = { token0, NULL };
	char *value = NULL;
	return( ! (0 == getsubopt(&options, tokens, &value)
	    && suboptarg == buf && value == buf+2 && options == buf+3));
}
