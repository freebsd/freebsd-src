#include <string.h>

int
main(int argc, char **argv)
{
	strlcpy(argv[0], argv[1], 10);
	return 0;
}
