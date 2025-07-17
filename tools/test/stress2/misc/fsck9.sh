#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# fsck_ffs(8) false CLEAN claim scenario

# https://people.freebsd.org/~pho/fsck9.sh.diskimage.20220716T150359.txt
# https://people.freebsd.org/~pho/fsck9.sh.diskimage.20220716T150359.gz

# https://people.freebsd.org/~pho/fsck9.sh.diskimage.20220716T172428.txt
# https://people.freebsd.org/~pho/fsck9.sh.diskimage.20220716T172428.gz

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

set -e
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
log=$mp1/fsck9.sh.log
diskimage=$mp1/fsck9.sh.diskimage
backup=/tmp/fsck9.sh.diskimage.`date +%Y%m%dT%H%M%S`.gz
cleans=0
reruns=0

max=$((10 * 1024 * 1024))
newfs_flags='-U'

mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 1g -u $u1
newfs $newfs_flags -n /dev/md$u1 > /dev/null
mount /dev/md$u1 $mp1

[ -c /dev/md$u2 ] && mdconfig -d -u $u2
dd if=/dev/zero of=$diskimage bs=$max count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $u2
backups=`newfs -N $newfs_flags md$u2 | grep -A1 "super-block backups" | \
    tail -1 | sed 's/,//g'`
newfs $newfs_flags -n md$u2 > /dev/null
mount /dev/md$u2 $mp2
[ -d /usr/include/sys ] && cp -r /usr/include/sys $mp2
umount $mp2
set +e

chk() {
	local i

	clean=0
	rerun=0
	fsck_ffs -fy $1 > $log 2>&1
	r=$?
	if grep -qiE "super-?block.*failed" $log; then
		for b in $backups; do
			echo "fsck_ffs -b $b -fy $1"
			fsck_ffs -b $b -fy $1 > $log 2>&1
			r=$?
			grep -qiE "super-?block.*failed" $log ||
			   break
			echo "Checking next SB"
		done
		usedasb=1
	else
		usedasb=0
	fi
	LANG=C egrep -q "[A-Z][A-Z]" $log && clean=0
	grep -Eq "IS CLEAN|MARKED CLEAN" $log && clean=1
	grep -q RERUN $log && rerun=1
	[ $r -ne 0 -a $clean -eq 1 ] && echo "Exit code $r w/ clean == 1"
}

cd $mp1
clean=0
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	mount /dev/md$u2 $mp2 || break
	if ! ls -lR $mp2 > /dev/null; then
		s=102
		echo "ls failed"; grep "core dumped" /var/log/messages | tail -1
		break
	fi
	touch $mp2/`jot -rc 8 a z | tr -d '\n'`
	while mount | grep -q "on $mp2 "; do umount $mp2; done
	echo * | grep -q core && break
	sync
	mdconfig -d -u $u2
	/tmp/flip -n 5 $diskimage

	sync
	if [ `stat -f%z $diskimage` -gt $max ]; then
		ls -lh $diskimage
		truncate -s $max $diskimage
	else
		gzip < $diskimage > $backup
	fi
	fsync $backup
	sync

	for i in `jot 5`; do
		[ $i -gt 2 ] && echo "fsck run #$i"
		chk $diskimage
		[ $rerun -eq 1 ] && { reruns=$((reruns + 1)); continue; }
		[ $clean -eq 1 ] && { cleans=$((cleans + 1)); break; }
		[ -f fsck_ffs.core ] &&
		    { cp -v $diskimage \
		        /tmp/fsck_ffs.core.diskimage.`date +%Y%m%dT%H%M%S`; break 2; }
	done
	if [ $clean -eq 1 ]; then
		fsck_ffs -fy $diskimage > $log 2>&1
		if grep -q MODIFIED $log; then
			echo "*** fsck of \"clean\" FS found more issues:"
			cat $log
			s=1
			break
		fi
	fi
	[ $clean -ne 1 ] && break
	mdconfig -a -t vnode -f $diskimage -u $u2
done
sleep 2	# Wait for /dev to catch up
[ -c /dev/md$u2 ] && r1=1 || r1=0
for i in `jot 5`; do
	mount | grep -q "on $mp2 " || break
	umount $mp2 && break
	sleep 2
done
mdconfig -d -u $u2 2>/dev/null # XXX when mount fails

[ $s -eq 0 ] && rm -f $backup || echo "Preserved $backup due to status code $s"
cd /tmp
for i in `jot 5`; do
	umount $mp1 && break
	sleep 2
done
mdconfig -d -u $u1
rm -f /tmp/flip
exit $s
