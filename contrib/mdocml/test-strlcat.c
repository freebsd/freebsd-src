#include <string.h>

int
main(void)
{
	char buf[3] = "a";
	return( ! (2 == strlcat(buf, "b", sizeof(buf)) &&
	    'a' == buf[0] && 'b' == buf[1] && '\0' == buf[2]));
}
