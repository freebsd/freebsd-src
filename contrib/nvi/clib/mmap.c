#include "config.h"

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>

/*
 * This function fakes mmap() by reading `len' bytes from the file descriptor
 * `fd' and returning a pointer to that memory.  The "mapped" region can later
 * be deallocated with munmap().
 *
 * Note: ONLY reading is supported and only reading of the exact size of the
 * file will work.
 *
 * PUBLIC: #ifndef HAVE_MMAP
 * PUBLIC: char *mmap __P((char *, size_t, int, int, int, off_t));
 * PUBLIC: #endif
 */
char *
mmap(addr, len, prot, flags, fd, off)
	char *addr;
	size_t len;
	int prot, flags, fd;
	off_t off;
{
	char *ptr;

	if ((ptr = (char *)malloc(len)) == 0)
		return ((char *)-1);
	if (read(fd, ptr, len) < 0) {
		free(ptr);
		return ((char *)-1);
	}
	return (ptr);
}

/*
 * PUBLIC: #ifndef HAVE_MMAP
 * PUBLIC: int munmap __P((char *, size_t));
 * PUBLIC: #endif
 */
int
munmap(addr, len)
	char *addr;
	size_t len;
{
	free(addr);
	return (0);
}
