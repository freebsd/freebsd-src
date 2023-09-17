#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	char	filename[] = "/tmp/temp.XXXXXX.suffix";

	if (mkstemps(filename, 7) == -1)
		return 1;
	return unlink(filename) == -1;
}
