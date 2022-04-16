#!/bin/sh

# Regression test for:
# bf13db086b84 - main - Mostly revert a5970a529c2d95271: Make files opened
# with O_PATH to not block non-forced unmount

# Based on a scenario by: ambrisko

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
mp1=$mntpoint
mp2=${mntpoint}$mdstart

set -e
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

mkdir -p $mp2
mount_nullfs -o nocache $mp1 $mp2
set +e
cat > /tmp/nullfs29.c <<EOF
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void) {
	int new_dir, new_file, ret;
	struct stat sb;
	char *dir = "test2";
	char *path= "test2/what2";

	if (mkdir(dir, 0755) == -1)
		err(1, "mkdir(test2)");
	new_dir = openat(AT_FDCWD, dir, O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_PATH, 0700);
	if (new_dir == -1)
		err(1, "openat(%s)", dir);

	ret = fstatat(new_dir, "what2", &sb, AT_SYMLINK_NOFOLLOW);
	if (ret == 0)
		errx(1, "Expected fstatat() to fail");
	if (ret == -1 && errno != ENOENT)
		err(1, "fstatat(%s)", dir);

	close(new_dir);
	new_file = openat(AT_FDCWD, path, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC, 0644);
	if (new_file== -1)
		err(1, "openat(%s)", path);
}

EOF
mycc -o /tmp/nullfs29 -Wall -Wextra -O2 /tmp/nullfs29.c || exit 1
cd $mp2
/tmp/nullfs29; s=$?
cd $here
umount $mp2

n=0
while mount | grep $mp1 | grep -q /dev/md; do
	umount $mp1 || sleep 1
	n=$((n + 1))
	[ $n -gt 30 ] && { echo FAIL; s=2; }
done
mdconfig -d -u $mdstart
rm -f /tmp/nullfs29.c
exit $s
