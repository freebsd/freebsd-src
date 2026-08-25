#!/bin/sh

#
# Copyright (c) 2026 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Simple renameat2 test scenario

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg
err=0
prog=$(basename "$0" .sh)

cat > /tmp/$prog.c <<EOF
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	int fd;
	char *f1 = "f1";
	char *f2 = "f2";
	char *t1 = "t1";

	if ((fd = open(f1, O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
		err(1, "open(%s)", f1);
	close(fd);
	if ((fd = open(f2, O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
		err(1, "open(%s)", f2);
	close(fd);

	if (renameat2(AT_FDCWD, f1, AT_FDCWD, t1, AT_RENAME_NOREPLACE) == -1)
		err(1, "renameat2(%s, %s)", f1, t1);
	if (renameat2(AT_FDCWD, f2, AT_FDCWD, t1, AT_RENAME_NOREPLACE) == 0)
		errx(1, "renameat2(%s, %s)", f2, t1);
	if (errno != EEXIST && errno != EOPNOTSUPP)
		err(1, "renameat2(%s, %s)", f2, t1);
	if (unlink(f2) != 0)
		err(1, "unlink(%s)", f2);
	if (unlink(t1) != 0)
		err(1, "unlink(%s)", t1);
}
EOF
cc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1

echo ufs
set -eu
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 1g -u $mdstart
newfs $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
/tmp/$prog || err=$((err+1))
cd -
umount $mntpoint
mdconfig -d -u $mdstart

echo tmpfs
mount -t tmpfs dummy $mntpoint
cd $mntpoint
/tmp/$prog || err=$((err+1))
cd -
umount $mntpoint

echo msdosfs
if [ -x /sbin/mount_msdosfs ]; then
	mdconfig -a -t swap -s 1g -u $mdstart
	gpart create -s bsd md$mdstart > /dev/null
	gpart add -t freebsd-ufs md$mdstart > /dev/null
	part=a
	newfs_msdos -F 16 -b 8192 /dev/md${mdstart}$part > /dev/null 2>&1
	mount_msdosfs -m 777 /dev/md${mdstart}$part $mntpoint

	cd $mntpoint
	/tmp/$prog || err=$((err+1))
	cd -
	umount $mntpoint
fi
echo zfs
u1=$mdstart
u2=$((u1 + 1))

mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 1g -u $u1
mdconfig -s 1g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank raidz md$u1 md$u2
zfs create stress2_tank/test

cd /stress2_tank/test
/tmp/$prog || err=$((err+1))
cd -

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank
mdconfig -d -u $u1
mdconfig -d -u $u2

rm -f /tmp/$prog /tmp/$prog.c
exit $err
