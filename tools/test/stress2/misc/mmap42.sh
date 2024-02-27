#!/bin/sh

# Test scenario by: kib@
# Test program obtained from Kyle Evans <kevans@FreeBSD.org>

# Demonstrate UFS SU file corruption:
# ffs: on write into a buffer without content

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
s=0
cat > /tmp/$prog.c <<EOF
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define	FILE	"file"

int
main(void)
{
	struct stat sb;
	ssize_t wsz;
	size_t bufsz;
	void *buf, *obuf;
	int mfd, fd;
	int done = 0;

	mfd = open(FILE, O_RDONLY);
	assert(mfd >= 0);

	assert(fstat(mfd, &sb) == 0);
	bufsz = sb.st_size;
	buf = obuf = mmap(NULL, bufsz, PROT_READ, MAP_SHARED, mfd, 0);
	assert(buf != MAP_FAILED);

	/* O_RDWR */
	fd = open(FILE, O_RDWR);
	if (fd < 0)
		err(1, "open");
	assert(fd >= 0);

again:
	while (bufsz > 0) {
		wsz = write(fd, buf, bufsz);
		if (wsz < 0)
			err(1, "write");
		else if (wsz == 0)
			fprintf(stderr, "Huh?\n");
		bufsz -= wsz;
		buf += wsz;
	}

	bufsz = sb.st_size;
	buf = obuf;

	if (++done < 2)
		goto again;

	close(fd);
	munmap(obuf, sb.st_size);
	close(mfd);
	return (0);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 32m -u $mdstart

pagesize=$(sysctl -n hw.pagesize)
newfs -Un -b $pagesize  /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
dd if=/dev/random of=/mnt/file.orig bs=${pagesize} count=1 status=none
cp $mntpoint/file.orig $mntpoint/file
cat $mntpoint/file $mntpoint/file > $mntpoint/file.post
umount $mntpoint

mount /dev/md$mdstart $mntpoint
(cd $mntpoint; /tmp/$prog)

if ! cmp $mntpoint/file $mntpoint/file.post; then
	echo "Files differ"
	ls -l $mntpoint/file $mntpoint/file.post
	s=1
fi

umount $mntpoint
mdconfig -d -u $mdstart
rm /tmp/$prog /tmp/$prog.c
exit $s
