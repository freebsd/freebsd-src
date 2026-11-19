#include <stdlib.h>

#include "kboot_runtime.h"

void *
calloc(size_t n, size_t size)
{
	return (Calloc(n, size, NULL, 0));
}
