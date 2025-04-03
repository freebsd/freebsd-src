#!/bin/sh

# Test scenario by se@ from https://reviews.freebsd.org/D43951

set -u
MDUNIT=10
FS=/mnt/test
mdconfig -u $MDUNIT -t malloc -s 512m
newfs_msdos -c 8 -F 32 /dev/md$MDUNIT
mkdir -p $FS
mount -t msdos /dev/md$MDUNIT $FS
rsync -r /usr/src/lib/libsysdecode $FS
rsync -r /usr/src/lib/libsysdecode $FS
rsync -r /usr/src/lib/libsysdecode $FS
umount $FS
fsck_msdosfs -y /dev/md$MDUNIT; s=$?
exit $s
