#include <sys/types.h>

#include <stdlib.h>

void *
__fix_realloc(p, n)
	void *p;
	size_t n;
{
	return (p == 0 ? malloc(n) : realloc(p, n));
}
