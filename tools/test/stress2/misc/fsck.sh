#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Dell EMC Isilon
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

# fsck_ffs(8) test.
# "UFS /dev/md11 (/mnt11) cylinder checksum failed" seen.
# Fixed by r341510.

# 'panic: invalid counts on struct mount' seen:
# https://people.freebsd.org/~pho/stress/log/fsck-4.txt

[ $DEBUG ] || exit 0 # Still WiP

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

echo 'int sync(void) { return (0); }' > /tmp/fsck_preload.c
mycc -o /tmp/fsck_preload.so -shared -fpic /tmp/fsck_preload.c || exit 1
cc -o /tmp/fsck_preload.so -shared -fpic /tmp/fsck_preload.c || exit 1
rm /tmp/fsck_preload.c

set -e
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
log=$mp1/fsck.sh.log
diskimage=$mp1/fsck.sh.diskimage
backup=/tmp/fsck.sh.diskimage.`date +%Y%m%dT%H%M%S`.gz
asbs=0
cleans=0
reruns=0
waccess=0

max=$((10 * 1024 * 1024))
[ "$newfs_flags" = "-j" ] &&
    max=$((20 * 1024 * 1024))

set +e
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 1g -u $u1
newfs $newfs_flags /dev/md$u1 > /dev/null
mount /dev/md$u1 $mp1

[ -c /dev/md$u2 ] && mdconfig -d -u $u2
dd if=/dev/zero of=$diskimage bs=$max count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $u2
backups=`newfs -N $newfs_flags md$u2 | grep -A1 "super-block backups" | \
    tail -1 | sed 's/,//g'`
newfs $newfs_flags md$u2 > /dev/null
mount /dev/md$u2 $mp2
[ -d /usr/include/sys ] && cp -r /usr/include/sys $mp2
umount $mp2

chk() {
	local i

	LD_PRELOAD=/tmp/fsck_preload.so  \
	    fsck_ffs -fy $1 > $log 2>&1
	r=$?
	if grep -qE "Cannot find file system superblock|Superblock check-hash failed" $log; then
		for b in $backups; do
			echo "Using alternate SB $b"
			asbs=$((asbs + 1))
			LD_PRELOAD=/tmp/fsck_preload.so  \
			    fsck_ffs -b $b -fy $1 > $log 2>&1
			r=$?
			grep -qE "Cannot find file system superblock|Superblock check-hash failed" $log ||
			   break
		done
		usedasb=1
	else
		usedasb=0
	fi
	LANG=C egrep -q "[A-Z][A-Z]" $log && clean=0
	! grep -Eq "IS CLEAN|MARKED CLEAN" $log; clean=$?
	! grep -q RERUN $log; rerun=$?
	! grep -q "NO WRITE ACCESS" $log; waccess=$?
	[ $r -ne 0 -a $clean -eq 1 ] && echo "Exit code $r w/ clean == 1"

}

cd /tmp
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 60 ]; do
	gzip < $diskimage > $backup
	fsync $backup; sync # ; sleep .2; sync; sleep .2; sync
	mount /dev/md$u2 $mp2 || { s=101; break; }
	touch $mp2/`jot -rc 8 a z | tr -d '\n'`
	umount $mp2
	/tmp/flip -n 4 $diskimage
	for i in `jot 3`; do
		chk /dev/md$u2
		[ $clean -eq 1 ] && { cleans=$((cleans + 1)); break; }
		[ $rerun -eq 1 ] && { reruns=$((reruns + 1)); continue; }
	done
	[ $r -ne 0 -a $clean -eq 1 ] &&
	    { echo "CLEAN && non zero exit code"; break; }
	[ $clean -eq 1 ] && continue
	[ $usedasb -eq 1 ] && { echo "Alt. SB failed"; s=103; }
	[ $waccess -eq 1 ] && { echo "No write access"; s=555; }
	break
done
[ $DEBUG ] &&
    echo "$cleans cleans, $reruns reruns, $asbs alternate SBs." && cat $log
if [ $clean -ne 1 ]; then
	echo "FS still not clean. Last fsck_ffs exit code was $r."
	cat $log
	cp -v $log /tmp || rm $log
	[ $s -eq 0 ] && s=104
fi
mdconfig -d -u $u2 || exit 1
[ -f fsck_ffs.core ] && ls -l fsck_ffs.core

umount $mp1
mdconfig -d -u $u1
rm -f /tmp/fsck_preload.so $backup /tmp/flip
exit $s
