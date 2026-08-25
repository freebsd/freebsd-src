#!/bin/sh

#
# Copyright (c) 2026 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# "nullfs mount over a file" scenario, see D58178 and D58191

set -u
prog=$(basename "$0" .sh)

file=/tmp/$prog.file
mp=/tmp/$prog.mp
sleep=/tmp/$prog.sleep
rm -f $file
touch $mp

# Mounting and running a program
cp /bin/date $file
mount_nullfs $file $mp || exit 1
$mp
umount $mp

# Display binary information for the process
rm -f $file
cp /bin/sleep $file
mount_nullfs $file $mp || exit 1
$mp 60 &
sleep .2
# "panic: condition vp->v_type == VDIR || VN_IS_DOOMED(vp) not met at vfs_cache.c:3547 (vn_fullpath_dir)" seen.
procstat -b $!
kill $!
umount $mp

# Mounting a link
rm $file
cp /bin/sleep $sleep
ln $sleep $file
mount_nullfs $file $mp || exit 1
$mp 60 &
sleep .2
procstat -b $!
kill $!
umount $mp
rm $sleep

# Mounting a symbolic link
rm $file
ln -s /bin/sleep $file
mount_nullfs $file $mp || exit 1
$mp 60 &
sleep .2
procstat -b $!
kill $!
umount $mp

# Mounting a fifo must fail
rm $file
mkfifo $file
mount_nullfs $file $mp 2>/dev/null && exit 1

rm -f $file $mp
exit 0
