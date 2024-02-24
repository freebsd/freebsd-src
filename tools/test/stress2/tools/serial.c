/* Fill a file with a sequence of byte values from 0 - 0xff */

#include <sys/param.h>
#include <sys/mman.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	size_t i, size;
	int fd;
	char *cp, *file;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file> <file length in bytes>\n", argv[0]);
		exit(1);
	}
	file = argv[1];
	size = atol(argv[2]);

	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
		err(1, "%s", file);

	if (lseek(fd, size - 1, SEEK_SET) == -1)
		err(1, "lseek error");

	/* write a dummy byte at the last location */
	if (write(fd, "\0", 1) != 1)
		err(1, "write error");

	if ((cp = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap()");

	for (i = 0; i < size; i++)
		cp[i] = i & 0xff;

	if (munmap(cp, size) == -1)
		err(1, "munmap");
	close(fd);
}
