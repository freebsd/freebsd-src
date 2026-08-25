#!/bin/sh

# Test of RENAME_EXCHANGE for tmpfs

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

static char *dir, *f1, *f2;

void
test1(void)
{
	struct stat s, s1, s2;
	time_t start;
	char file1[1024], file2[1024];

	snprintf(file1, sizeof(file1), "%s/%s", dir, f1);
	snprintf(file2, sizeof(file2), "%s/%s", dir, f2);
	if (stat(file1, &s1) < 0)
		err(1, "stat(%s)", file1);
	if (stat(file2, &s2) < 0)
		err(1, "stat(%s)", file2);
	assert(s1.st_size != s2.st_size);
	start = time(NULL);
	while (time(NULL) - start < 30) {
		if (renameat2(AT_FDCWD, file1, AT_FDCWD, file2, RENAME_EXCHANGE) == -1)
			err(1, "%s: rename2(%s, %s)", __func__, file1, file2);

		if (stat(file1, &s) < 0)
			err(1, "statfs(%s)", file1);
		if (s.st_size != s2.st_size)
			errx(1, "Got size %jd, expected %jd", (uintmax_t)s.st_size, (uintmax_t)s2.st_size);

		if (renameat2(AT_FDCWD, file2, AT_FDCWD, file1, RENAME_EXCHANGE) == -1)
			err(1, "%s: rename2(%s, %s)", __func__, file2, file1);
	}
}

void
test2(void)
{
	time_t start;
	char cwd[1024];

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd()");
	chdir(dir);
	start = time(NULL);
	while (time(NULL) - start < 30) {
		if (renameat2(AT_FDCWD, f1, AT_FDCWD, f2, RENAME_EXCHANGE) == -1)
			err(1, "%s: rename2(%s, %s)", __func__, f1, f2);

		if (renameat2(AT_FDCWD, f2, AT_FDCWD, f1, RENAME_EXCHANGE) == -1)
			err(1, "%s: rename2(%s, %s)", __func__, f2, f1);
	}
	chdir(cwd);
}

/* rename(<file>, <non existing file>) */
void
test3(void)
{
	int e;
	char file1[1024], file2[1024];

	snprintf(file1, sizeof(file1), "%s/%s", dir, f1);
	snprintf(file2, sizeof(file2), "%s/NotThere", dir);
	if (renameat2(AT_FDCWD, file1, AT_FDCWD, file2, RENAME_EXCHANGE) != -1)
		err(1, "%s: rename2(%s, %s)", __func__, file1, file2);
	e = errno;
	if (e != ENOENT)
		errx(1, "%s: Expected errno %i, got %d\n", __func__, ENOENT, e);

	if (renameat2(AT_FDCWD, file2, AT_FDCWD, file1, RENAME_EXCHANGE) != -1)
		err(1, "%s: rename2(%s, %s)", __func__, file2, file1);
	e = errno;
	if (e != ENOENT)
		errx(1, "%s: Expected errno %i, got %d\n", __func__, ENOENT, e);
}

/* rename(<file>, <directory>) */
void
test4(void)
{
	int e;
	char file1[1024], file2[1024];

	snprintf(file1, sizeof(file1), "%s/%s", dir, f1);
	snprintf(file2, sizeof(file2), "%s", dir);
	if (renameat2(AT_FDCWD, file1, AT_FDCWD, file2, RENAME_EXCHANGE) != -1)
		err(1, "%s: rename2(%s, %s)", __func__, file1, file2);
	e = errno;
	if (e != EINVAL)
		errx(1, "%s: Expected errno %i, got %d\n", __func__, EINVAL, e);

	if (renameat2(AT_FDCWD, file2, AT_FDCWD, file1, RENAME_EXCHANGE) != -1)
		err(1, "%s: rename2(%s, %s)", __func__, file2, file1);
	e = errno;
	if (e != EINVAL)
		errx(1, "%s: Expected errno %i, got %d\n", __func__, EINVAL, e);
}

int
main(int argc, char *argv[])
{

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <dir> <file1> <file2>\n", argv[0]);
		exit(1);
	}
	dir = argv[1];
	f1 = argv[2];
	f2 = argv[3];

	test1();
	test2();
	test3();
	test4();
}
EOF
cc -o /tmp/$prog -Wall -Wextra -g -O0 /tmp/$prog.c || exit 1

mount | grep -q "on $mntpoint " && umount $mntpoint
size=2g
mount -o size=${size}m -t tmpfs tmpfs $mntpoint

../testcases/swap/swap -t 5m -i 20 > /dev/null &
sleep 1

for d in d1 d2 d3 d4 d5; do
	mkdir $mntpoint/$d
	echo 1  > $mntpoint/$d/f1
	echo 22 > $mntpoint/$d/f2
done
cd $mntpoint
for d in d1 d2 d3 d4 d5; do
	/tmp/$prog $mntpoint/$d f1 f2 &
done
while pgrep -q $prog; do sleep .2; done
cd -

while pkill swap; do sleep .5; done
wait

umount $mntpoint
exit 0
