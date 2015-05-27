#include <string.h>

int
main(void)
{
	char buf[6] = "aybxc";
	char *workp = buf;
	char *retp = strsep(&workp, "xy");
	return( ! (retp == buf && '\0' == buf[1] && buf + 2 == workp));
}
