#!/bin/sh

# File corruption scenario.
# Test program obtained from Kyle Evans <kevans@FreeBSD.org>

# "panic: VERIFY3(rc->rc_count == number) failed (4849664 == 0)" seen.

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/log0560.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n kern.kstack_pages` -lt 4 ] && exit 0

. ../default.cfg

prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

//#define	FILE	"2"
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
set -u
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
    exit 0; loaded=1; }

u1=$mdstart
u2=$((u1 + 1))
mp0=/stress2_tank/test		# zfs mount

mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 4g -u $u1
mdconfig -s 4g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank raidz md$u1 md$u2
zfs create ${mp0#/}

here=`pwd`
cd /stress2_tank
# Optimized file creation:
#jot -b 'A' -s '' 875998989 > file
dd if=/dev/random of=file bs=1m count=$(((875998990/1024/1024)+1)) status=none
truncate -s 875998990 file
cat file file > file.post
mv file file.orig

counter=1
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	cp file.orig file
	/tmp/$prog
	if ! cmp file file.post; then
		echo "Iteration #$counter"
		od -t x8 file      | head -1000 > /tmp/$prog.file1
		od -t x8 file.post | head -1000 > /tmp/$prog.file2
		diff /tmp/$prog.file1 /tmp/$prog.file2 | head -15
		rm /tmp/$prog.file1 /tmp/$prog.file2
		s=1
		break
	fi
	counter=$((counter + 1))
done
cd $here

zfs umount ${mp0#/}
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u2
mdconfig -d -u $u1
set +u
[ $loaded ] && kldunload zfs.ko
rm /tmp/$prog /tmp/$prog.c
exit $s
