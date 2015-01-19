#include <sys/types.h>
#include <sys/mman.h>
#include <stddef.h>

int
main(void)
{
	return(MAP_FAILED != mmap(NULL, 1, PROT_READ, MAP_SHARED, -1, 0));
}
