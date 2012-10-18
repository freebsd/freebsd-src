#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	char *cp;
	size_t sz;
	cp = fgetln(stdin, &sz);
	return 0;
}
