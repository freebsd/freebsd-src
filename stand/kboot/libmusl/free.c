#include <stdlib.h>

#include "kboot_runtime.h"

void
free(void *ptr)
{
	Free(ptr, NULL, 0);
}
