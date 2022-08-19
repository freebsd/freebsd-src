#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

# fsck_ffs(8) disk image fuzz test. SB focus.

# panic: wrong length 4098 for sectorsize 512
# FreeBSD 14.0-CURRENT #0 main-n255602-51adf913e8815: Fri May 13 07:55:32 CEST 2022
# pho@mercat1.netperf.freebsd.org:/usr/src/sys/amd64/compile/PHO

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

set -e
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
log=$mp1/fsck8.sh.log
diskimage=$mp1/fsck8.sh.diskimage
backup=/tmp/fsck8.sh.diskimage.`date +%Y%m%dT%H%M%S`.gz
asbs=0
cleans=0
reruns=0
waccess=0

max=$((10 * 1024 * 1024))
# UFS1 or UFS2 SU:
[ `jot -r 1 0 1` -eq 0 ] && newfs_flags='-O 1' || newfs_flags='-O 2 -U'

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
	waccess=0
	timeout 5m fsck_ffs -fy $1 > $log 2>&1
	r=$?
	if grep -qiE "super-?block.*failed" $log; then
		for b in $backups; do
			echo "Using alternate SB $b"
			asbs=$((asbs + 1))
			fsck_ffs -b $b -fy $1 > $log 2>&1
			r=$?
			grep -qiE "super-?block.*failed" $log ||
			   break
		done
		usedasb=1
	else
		usedasb=0
	fi
	LANG=C egrep -q "[A-Z][A-Z]" $log && clean=0
	grep -Eq "IS CLEAN|MARKED CLEAN" $log && clean=1
	# For now regard a "was modified" as a cause for a rerun,
	# disregarding the "clean" claim.
	grep -Eq "WAS MODIFIED" $log && rerun=1
	grep -q RERUN $log && rerun=1
	grep -q "NO WRITE ACCESS" $log && waccess=1
	[ $r -ne 0 -a $clean -eq 1 ] && echo "Exit code $r w/ clean == 1"
}

cd $mp1
clean=0
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	mount /dev/md$u2 $mp2 || break
	ls -lR $mp2 > /dev/null || { s=102; echo "ls failed"; break; }
	touch $mp2/`jot -rc 8 a z | tr -d '\n'`
	while mount | grep -q "on $mp2 "; do umount $mp2; done
	echo * | grep -q core && break
	sync
	mdconfig -d -u $u2
	# SBLOCK = 64k, SBLOCKSIZE = 8k
	/tmp/flip -n 5 -s $(((64 + 8) * 1024)) $diskimage

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
	[ $clean -ne 1 ] && break
	mdconfig -a -t vnode -f $diskimage -u $u2
	[ $r -ne 0 -a $clean -eq 1 ] &&
	    { echo "CLEAN && non zero exit code"; break; }
	[ $clean -eq 1 ] && continue
	[ $usedasb -eq 1 ] && { echo "Alt. SB failed"; s=104; }
	[ $waccess -eq 1 ] && { echo "No write access"; s=105; }
	break
done
sleep 2	# Wait for /dev to catch up
[ -c /dev/md$u2 ] && r1=1 || r1=0
for i in `jot 5`; do
	mount | grep -q "on $mp2 " || break
	umount $mp2 && break
	sleep 2
done
mdconfig -d -u $u2 2>/dev/null # XXX when mount fails

echo "$cleans cleans, $reruns reruns, $asbs alternate SBs."
if [ $clean -ne 1 ]; then
	echo "FS still not clean. Last fsck_ffs exit code was $r."
	echo =================
	cat $log
	echo =================
	cp -v $log /tmp
	[ $s -eq 0 ] && s=106
fi
echo * | grep -q core && { ls -l *.core; cp -v $log /tmp; exit 106; } ||
    rm -f $backup
[ $s -eq 101 ] && rm -f $backup	# mount error breakout
cd /tmp
for i in `jot 5`; do
	umount $mp1 && break
	sleep 2
done
mdconfig -d -u $u1
rm -f /tmp/flip
exit $s
