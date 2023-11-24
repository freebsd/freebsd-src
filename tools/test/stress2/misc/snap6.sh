#!/bin/sh

# Problem Report kern/92272 : [ffs] [hang] Filling a filesystem while creating
# a snapshot on it locks the system

# John Kozubik <john@kozubik.com>

# If a filesystem is completely full (approaching 110% in `df` output on
# most systems), mksnap_ffs will refuse to begin a snapshot for that reason.
# There seem to be no negative consequences.

# However, if the filesystem is not quite completely full, mksnap_ffs will
# (correctly) begin creating the snapshot as expected.  If, during the course
# of snapshot creation, the filesystem being snapshotted becomes full,
# mksnap_ffs will exit with an error exactly as it does if it is started on
# an already full filesystem.

# However, several hours later, the system will lock up completely.  It still
# responds to pings, and open connections to and from it remain in that
# state, but they cannot be used and new connections cannot be established.

# *** This script does not provoke the problem. ***

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

root=/var

mount | grep "on `df /var | tail -1 | awk '{print $NF}'`" |
    grep -q 'journaled soft-updates' && exit 0
rm -f $root/.snap/stress2 $root/big $root/big2
trap "rm -f $root/.snap/stress2 $root/big $root/big2" 0
free=`df $root | tail -1 | awk '{print $4}'`
timeout 5m dd if=/dev/zero of=$root/big bs=1m count=$(( free / 1024 - 90)) \
    status=none
df $root

start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
   date
   nice -20 mksnap_ffs $root $root/.snap/stress2 &
   timeout 5m dd if=/dev/zero of=$root/big2 bs=1m status=none
   wait
   [ -f $root/.snap/stress2 ] && exit 0
   rm -f $root/.snap/stress2 $root/big2
done
df $root

rm -f $root/.snap/stress2 $root/big $root/big2
