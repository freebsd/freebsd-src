#!/bin/sh

# Test scenario by: David Cross <dcrosstech@gmail.com>

# "panic: softdep_deallocate_dependencies: dangling deps" seen.
# https://people.freebsd.org/~pho/stress/log/mmap29.txt
# Fixed by: r302567.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ -z "`which timeout`" ] && exit 0
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs -U md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mkdir $mntpoint/mmap29
cd /tmp
cat > mmap29.c <<EOFHERE
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
        int fd;
        unsigned char *memrange;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		exit(1);
	}
        unlink(argv[1]);
        if ((fd = open(argv[1], O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
		err(1, "open(%s)", argv[1]);
        lseek(fd, 0xbfff, SEEK_SET);
        write(fd, "\0", 1);
        if ((memrange = mmap(0, 0x2b6000, PROT_READ | PROT_WRITE, MAP_SHARED |
	    MAP_HASSEMAPHORE | MAP_NOSYNC, fd, 0)) == MAP_FAILED)
		err(1, "mmap");
        memrange[0] = 5;
        munmap(memrange, 0x2b6000);
        close(fd);

        return (0);
}
EOFHERE

cc -o mmap29 -Wall -Wextra -O0 -g mmap29.c || exit 1
rm mmap29.c
./mmap29 $mntpoint/mmap29/mmap291
old=`sysctl -n kern.maxvnodes`
trap "sysctl kern.maxvnodes=$old" EXIT INT
sysctl kern.maxvnodes=2000
timeout 60 find / -xdev -print >/dev/null
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm mmap29
exit 0
