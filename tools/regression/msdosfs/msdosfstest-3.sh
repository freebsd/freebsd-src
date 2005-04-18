#!/bin/sh
# $FreeBSD$
# A really simple script to create a swap-backed msdosfs filesystem, then
# test to make sure the mbnambuf optimisation(msdosfs_conv.c rev 1.40)
# doesn't break multi-byte characters.

mkdir /tmp/msdosfstest/
mdconfig -a -t swap -s 128m -u 10
bsdlabel -w md10 auto
newfs_msdos -F 16 -b 8192 /dev/md10a
mount_msdosfs -L zh_TW.Big5 -D CP950 /dev/md10a /tmp/msdosfstest/
mkdir /tmp/msdosfstest/012345678_邪惡之美
cd /tmp/msdosfstest/012345678_邪惡之美
if [ $? -eq 0 ]; then
	echo "ok 3";
else
	echo "not ok 3";
fi
cd /tmp
umount /tmp/msdosfstest/
mdconfig -d -u 10
rmdir /tmp/msdosfstest/
