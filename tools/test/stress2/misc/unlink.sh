#!/bin/sh

# File-backed shared memory performance
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=222356

# For unlinked files, do not msync(2) or sync on inactivation.
# https://reviews.freebsd.org/D12411

# Original test scenario by: tijl@FreeBSD.org

# Fixed by r323768

. ../default.cfg

cat > /tmp/unlink.c <<EOF
#include <sys/mman.h>
#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(void) {
	struct timeval diff, start, stop;
	size_t sz;
	void *base;
	uint64_t usec;
	int fd;

	sz = 128 * 1024 * 1024;
	fd = open("nosync.data", O_RDWR | O_CREAT, 0600);
	if (fd == -1)
		err(1, "open");
	if (unlink("nosync.data") == -1)
		err(1, "unlink");
	if (ftruncate(fd, sz) == -1)
		err(1, "ftruncate");
	base = mmap(NULL, sz, PROT_READ | PROT_WRITE,
		     MAP_SHARED | MAP_NOSYNC, fd, 0);
	if (base == MAP_FAILED)
		err(1, "mmap");
	memset(base, '0', sz);
	if (munmap(base, sz) == -1)
		err(1, "unmap");
	gettimeofday(&start, NULL);
	close(fd);
	gettimeofday(&stop, NULL);
	timersub(&stop, &start, &diff);
	usec  = ((uint64_t)1000000 * diff.tv_sec + diff.tv_usec);
	if (usec > 500000) {
		fprintf(stdout, "%.3f seconds elapsed\n",
		    (double)usec / 1000000);
		return (0); /* Ignore error for now */
	} else
		return(0);
}
EOF

mycc -o /tmp/unlink -Wall -Wextra -O2 -g /tmp/unlink.c || exit 1
rm /tmp/unlink.c

cd $RUNDIR
/tmp/unlink; s=$?

rm /tmp/unlink
exit $s
