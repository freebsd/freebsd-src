#!/bin/sh

# Test scenario from https://reviews.freebsd.org/D43951 "Fix MSDOSFS rename (in case target exists)"
# Test scenario by se@

# Triggered a panic with a WiP kernel patch.

set -u
[ -f "`which rsync`" ] || exit 0
[ -d /usr/src/lib ]    || exit 0

MDUNIT=10
FS=/mnt/test
mdconfig -u $MDUNIT -t malloc -s 512m
newfs_msdos -c 8 -F 32 /dev/md$MDUNIT > /dev/null 2>&1
mkdir -p $FS
mount -t msdos /dev/md$MDUNIT $FS
rsync -r /usr/src/lib/libsysdecode $FS
rsync -r /usr/src/lib/libsysdecode $FS
rsync -r /usr/src/lib/libsysdecode $FS
umount $FS
fsck_msdosfs -y /dev/md$MDUNIT; s=$?
mdconfig -d -u $MDUNIT

exit $s
