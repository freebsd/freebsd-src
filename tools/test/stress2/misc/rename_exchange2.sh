#!/bin/sh

# Test of RENAME_EXCHANGE for tmpfs, cross directory version

. ../default.cfg
set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#if ! defined(AT_RENAME_EXCHANGE)
#define AT_RENAME_EXCHANGE 0x0002
#endif

static char *f1, *f2;

void
test1(void)
{
	struct stat s, s1, s2;
	time_t start;

	if (stat(f1, &s1) < 0)
		err(1, "stat(%s)", f1);
	if (stat(f2, &s2) < 0)
		err(1, "stat(%s)", f2);
	assert(s1.st_size != s2.st_size);
	start = time(NULL);
	while (time(NULL) - start < 30) {
		if (renameat2(AT_FDCWD, f1, AT_FDCWD, f2, RENAME_EXCHANGE) == -1)
			err(1, "%s: rename2(%s, %s)", __func__, f1, f2);

		if (stat(f1, &s) < 0)
			err(1, "statfs(%s)", f1);
		if (s.st_size != s2.st_size)
			errx(1, "Got size %jd, expected %jd", (uintmax_t)s.st_size, (uintmax_t)s2.st_size);

		if (renameat2(AT_FDCWD, f2, AT_FDCWD, f1, RENAME_EXCHANGE) == -1)
			err(1, "%s: rename2(%s, %s)", __func__, f2, f1);
	}
}

int
main(int argc, char *argv[])
{

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file1> <file2>\n", argv[0]);
		exit(1);
	}
	f1 = argv[1];
	f2 = argv[2];

	test1();
}
EOF
cc -o /tmp/$prog -Wall -Wextra -g -O0 /tmp/$prog.c || exit 1

mount | grep -q "on $mntpoint " && umount $mntpoint
size=2g
mount -o size=${size}m -t tmpfs tmpfs $mntpoint

../testcases/swap/swap -t 5m -i 20 > /dev/null &
sleep 1

mkdir $mntpoint/d1 $mntpoint/d2
echo 1  > $mntpoint/d1/f1
echo 22 > $mntpoint/d2/f2
/tmp/$prog $mntpoint/d1/f1 $mntpoint/d2/f2

while pkill swap; do sleep .5; done
wait

umount $mntpoint
exit 0
