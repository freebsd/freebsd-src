#include <string.h>

int
main(void)
{
	const char *big = "BigString";
	char *cp = strcasestr(big, "Gst");
	return cp != big + 2;
}
