#include <stdlib.h>

void *
reallocf(void *ptr, size_t size)
{
    void *nptr;

    nptr = realloc(ptr, size);
    if (!nptr && ptr)
        free(ptr);
    return (nptr);
}
