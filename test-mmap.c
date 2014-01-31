#include <sys/types.h>
#include <sys/mman.h>

int
main(int argc, char **argv)
{

	mmap(0, 0, PROT_READ, MAP_SHARED, -1, 0);
	return 0;
}
