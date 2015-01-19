#include <string.h>

int
main(void)
{
	char buf[2] = "";
	return( ! (1 == strlcpy(buf, "a", sizeof(buf)) &&
	    'a' == buf[0] && '\0' == buf[1]));
}
