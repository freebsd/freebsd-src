#!/bin/sh
#
# $FreeBSD$
#

mntpoint="/mnt/test-1"

#
# prepare
kldload geom_uzip
uudecode test-1.img.gz.uue
num=`mdconfig -an -f test-1.img.gz` || exit 1
sleep 1

#
# mount
mkdir -p "${mntpoint}"
mount -o ro /dev/md${num}.uzip "${mntpoint}" || exit 1

#
# compare
#cat "${mntpoint}/etalon.txt"
diff -u etalon/etalon.txt "${mntpoint}/etalon.txt"
if [ $? -eq 0 ]; then
	echo "PASS"
else
	echo "FAIL"
fi

#
# cleanup
umount "${mntpoint}"
rmdir "${mntpoint}"
mdconfig -d -u ${num}
kldunload geom_uzip
