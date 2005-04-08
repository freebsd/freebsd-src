#!/bin/sh
# $FreeBSD$
# A really simple script to create a swap-backed msdosfs filesystem, copy a few
# files to it, unmount/remount the filesystem, and make sure all is well.
# 
# Not very advanced, but better than nothing. 
mdconfig -a -t swap -s 128m -u 10
bsdlabel -w md10 auto
newfs_msdos -F 16 -b 8192 /dev/md10a
mount_msdosfs /dev/md10a /mnt/thumb/
cp -R /usr/src/bin/ /mnt/thumb/
umount /mnt/thumb
mount_msdosfs /dev/md10a /mnt/thumb/
diff -u -r /usr/src/bin /mnt/thumb
if [ $? -eq 0 ]; then
	echo "ok 1";
else
	echo "not ok 1";
fi
umount /mnt/thumb
mdconfig -d -u 10
