#!/bin/sh

# No problems seen

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko; loaded=1; } ||
    exit 0

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/datamove.sh > zfs16.c
mycc -o zfs16 -Wall -O0 -g zfs16.c || exit 1
rm -f zfs16.c

mp1=/stress2_tank/test
u1=$mdstart
u2=$((u1 + 1))

set -eu
mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 2g -u $u1
mdconfig -s 2g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank md$u1 md$u2
zfs create stress2_tank/test
set +e

(cd $here/../testcases/swap; ./swap -t 2m -i 20 -l 100 -h > /dev/null) &
sleep 2
cd $mp1
while pgrep -q swap; do
	/tmp/zfs16; s=$?
	rm -f /stress2_tank/test/*
done
cd $here
while pkill swap; do sleep 1; done
wait

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank
mdconfig -d -u $u1
mdconfig -d -u $u2
rm -f /tmp/zfs16
set +u
[ $loaded ] && kldunload zfs.ko
exit $s
EOF
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SIZ  (500UL * 1024 * 1024)

int
main(int argc __unused, char *argv[])
{
	off_t hole;
	size_t len;
	int fd;
	char *p, *path;

	len = SIZ;

	path = argv[1];
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open()");
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return (1);
		err(1, "mmap(1)");
	}
	p[1 * 1024] = 1;
	p[2 * 1024] = 1;
	p[4 * 1024] = 1;

	if (msync(p, len, MS_SYNC | MS_INVALIDATE) == -1)
		err(1, "msync()");

	if ((hole = lseek(fd, 0, SEEK_HOLE)) == -1)
		err(1, "lseek(SEEK_HOLE)");
	if (hole != SIZ)
		printf("--> hole = %jd, file size=%jd\n",
		    (intmax_t)hole, (intmax_t)SIZ);
	close(fd);

	return (hole == SIZ ? 0 : 1);
}
