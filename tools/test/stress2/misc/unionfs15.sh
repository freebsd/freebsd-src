#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# O_PATH test scenario.  Variation of nullfs29.sh

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2
mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done
for i in $md1 $md2; do
	mdconfig -l | grep -q md$i && mdconfig -d -u $i
done

mdconfig -a -t swap -s 2g -u $md1
mdconfig -a -t swap -s 2g -u $md2
newfs $newfs_flags -n md$md1 > /dev/null
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2
mount -t unionfs -o noatime $mp1 $mp2
set +e

cat > /tmp/unionfs15.c <<EOF
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
mycc -o /tmp/unionfs15 -Wall -Wextra -O2 /tmp/unionfs15.c || exit 1
cd $mp2
/tmp/unionfs15; s=$?
cd $here
umount $mp2

while mount | grep -Eq "on $mp2 .*unionfs"; do
	umount $mp2 && break
	sleep 5
done
umount $mp2
umount $mp1
mdconfig -d -u $md2
mdconfig -d -u $md1
rm -f /tmp/unionfs15.c /tmp/unionfs15
exit $s
