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

# fsck_ffs(8) disk image fuzz test.

# "UFS /dev/md11 (/mnt11) cylinder checksum failed" seen.
# Fixed by r341510.

# 'panic: invalid counts on struct mount' seen:
# https://people.freebsd.org/~pho/stress/log/fsck-4.txt

# "panic: softdep_load_inodeblock: negative i_effnlink" seen.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

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
newfs $newfs_flags -n /dev/md$u1 > /dev/null
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

	clean=0
	rerun=0
	waccess=0
	fsck_ffs -fy $1 > $log 2>&1
	r=$?
	if grep -qE "Cannot find file system superblock|Superblock check-hash failed" $log; then
		for b in $backups; do
			echo "Using alternate SB $b"
			asbs=$((asbs + 1))
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
	grep -Eq "IS CLEAN|MARKED CLEAN" $log && clean=1
	# For now regard a "was modified" as a cause for a rerun,
	# disregarding "clean" claim.
	grep -Eq "WAS MODIFIED" $log && rerun=1
	grep -q RERUN $log && rerun=1
	grep -q "NO WRITE ACCESS" $log && waccess=1
	[ $r -ne 0 -a $clean -eq 1 ] && echo "Exit code $r w/ clean == 1"
}

cd $mp1
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 60 ]; do
	mount /dev/md$u2 $mp2 || { s=101; break; }
	ls -lR $mp2 > /dev/null || { s=102; echo "ls failed"; break; }
	touch $mp2/`jot -rc 8 a z | tr -d '\n'`
	while mount | grep -q "on $mp2 "; do umount $mp2; done
	echo * | grep -q core && break
	sync
	mdconfig -d -u $u2
	/tmp/flip -n 10 $diskimage

	sync
	gzip < $diskimage > $backup
	fsync $backup

	for i in `jot 3`; do
		chk $diskimage
		[ $rerun -eq 1 ] && { reruns=$((reruns + 1)); continue; }
		[ $clean -eq 1 ] && { cleans=$((cleans + 1)); break; }
		[ -f fsck_ffs.core ] &&
		    { cp $diskimage \
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
mount | grep -q "on $mp2 " && umount $mp2
mdconfig -d -u $u2 || exit 1

echo "$cleans cleans, $reruns reruns, $asbs alternate SBs."
if [ $clean -ne 1 ]; then
	echo "FS still not clean. Last fsck_ffs exit code was $r."
	cat $log
	cp -v $log /tmp
	[ $s -eq 0 ] && s=106
fi
echo * | grep -q core && { ls -l *.core; cp $log /tmp; exit 106; } ||
    rm -f $backup
cd /tmp
umount $mp1
mdconfig -d -u $u1
rm -f /tmp/flip
exit $s
