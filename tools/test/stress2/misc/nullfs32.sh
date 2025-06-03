#!/bin/sh

# Test scenario from:
# Bug 254210 - jail: nullfs: deleted files does not free up space
# Fixed by: 1a0cb938f7b4

# Test scenario idea by: ronald@FreeBSD.org

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

set -u
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
df -h | grep "$mntpoint"
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
mkdir storage test1 test2
mount_nullfs -o rw,noatime ./storage ./test1
mount_nullfs -o rw,noatime ./storage ./test2

dd if=/dev/random of=./test1/random.dd bs=1M count=1024 status=none

rm ./test2/random.dd
df -h | grep "$mntpoint" > $log
grep -E "${mntpoint}$" $log | grep -q '16K    1.8G     0%' && s=0 || s=1
if [ $s -eq 1 ]; then
	echo "Leaking:"
	cat $log
	find $mntpoint -type f -ls
fi
cd -
umount $mntpoint/test1
umount $mntpoint/test2
umount $mntpoint
mdconfig -d -u $mdstart
rm -f rm -f $log
exit $s
