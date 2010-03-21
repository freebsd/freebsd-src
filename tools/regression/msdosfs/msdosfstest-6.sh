#!/bin/sh
# $FreeBSD: src/tools/regression/msdosfs/msdosfstest-6.sh,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $
# A really simple script to create a swap-backed msdosfs filesystem, then
# test to make sure the nmount conversion(mount_msdosfs.c rev 1.37)
# doesn't break multi-byte characters.

mkdir /tmp/msdosfstest/
mdconfig -a -t swap -s 128m -u 10
bsdlabel -w md10 auto
newfs_msdos -F 32 -b 8192 /dev/md10a
mount_msdosfs -L zh_TW.Big5 -D CP950 /dev/md10a /tmp/msdosfstest/
mkdir '/tmp/msdosfstest/是否看過坊間常見的許茹芸淚海慶功宴吃蓋飯第四集'
cd '/tmp/msdosfstest/是否看過坊間常見的許茹芸淚海慶功宴吃蓋飯第四集'
if [ $? -eq 0 ]; then
	echo "ok 6";
else
	echo "not ok 6";
fi
cd /tmp
umount /tmp/msdosfstest/
mdconfig -d -u 10
rm -rf /tmp/msdosfstest/
