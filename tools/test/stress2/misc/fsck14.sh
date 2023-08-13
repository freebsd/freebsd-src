#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# fsck -B test scenario with SUJ. -B implies "preen mode".

# 'panic: ffs_blkfree_cg: bad size' seen:
# https://people.freebsd.org/~pho/stress/log/log0465.txt
# Fixed by: 220427da0e9b - Set UFS/FFS file type to snapshot before changing
# its block pointers.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
mycc -o /tmp/flip -Wall -Wextra ../tools/flip.c || exit 2

set -e
md=/dev/md$mdstart
prog=$(basename "$0" .sh)
backup=/tmp/$prog.sh.diskimage.`date +%Y%m%dT%H%M%S`.gz
log=/tmp/$prog.sh.log
[ -c $md ] && mdconfig -d -u $mdstart
dd if=/dev/zero of=$diskimage bs=128m count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $mdstart
backups=`newfs -N $j $md | grep -A1 "super-block backups" | \
    tail -1 | sed 's/,//g'`
newfs -j $md > /dev/null 2>&1
mount $md $mntpoint
set +e

jot 5000 | xargs -P0 -I% touch $mntpoint/a%
while ! umount $mntpoint; do :; done
/tmp/flip -n 10 $diskimage
gzip < $diskimage > $backup
fsync $diskimage $backup

mount -f $md $mntpoint

if ! fsck_ffs -B $md > $log 2>&1; then
	grep MANUALLY $log
	umount $mntpoint
	fsck_ffs -fy $md > $log 2>&1; s=$?
	grep -Eq "IS CLEAN|MARKED CLEAN" $log || {
		cat $log
		echo "fsck_ffs -f failed with exit code $s"
		umount $mntpoint; mdconfig -d -u $mdstart
		rm -f $log /tmp/flip $diskimage $backup
		exit 1
	}
	mount $md $mntpoint
fi

jot 5000 | xargs -P0 -I% rm $mntpoint/a%
jot 5000 | xargs -P0 -I% touch $mntpoint/b%

ls -lR $mntpoint > /dev/null || {
    echo "ls -lR $mntpoint failed after fsck -B"
    umount $mntpoint; mdconfig -d -u $mdstart
    rm -f $log /tmp/flip $diskimage $backup
    exit 0 # For now, ignore non fatal errors
}

jot 5000 | xargs -P0 -I% rm $mntpoint/b% || {
    echo "clean failed"
    umount $mntpoint; mdconfig -d -u $mdstart
    rm -f $log /tmp/flip $diskimage $backup
    exit 0 # For now, ignore non fatal errors
}
umount $mntpoint

r=0
for i in `jot 4`; do
	fsck_ffs -fy $diskimage > $log 2>&1; r=$?
	if grep -qiE "super-?block.*failed" $log; then
		for b in $backups; do
			echo "fsck_ffs -b $b -fy $diskimage"
			fsck_ffs -b $b -fy $diskimage > $log 2>&1
			r=$?
			grep -qiE "super-?block.*failed" $log ||
			   break
			echo "Checking next SB"
		done
	fi
	[ $r -ne 0 ] && continue
	grep -Eq "WAS MODIFIED" $log && continue
	grep -Eq "CLEAN" $log && break
done
mount $md $mntpoint || exit 3
ls -lR $mntpoint > /dev/null || { umount $mntpoint; mdconfig -d -u mdstart; echo "exit 4"; exit 4; }
umount $mntpoint
fsck_ffs -fy $md > $log 2>&1
grep -Eq 'IS CLEAN|MARKED CLEAN' $log && s=0 || { s=1; cat $log; }
mdconfig -d -u $mdstart
rm -f $log /tmp/flip $diskimage $backup
exit $s
