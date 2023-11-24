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

# fsck_ffs(8) disk image fuzz test.
# Test without mount(8) and umount(8)

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

# Disable the calls to sync(2) in fsck_ffs(8) to speed up the test
echo 'int sync(void) { return (0); }' > /tmp/fsck_preload.c
mycc -o /tmp/fsck_preload.so -shared -fpic /tmp/fsck_preload.c || exit 1
rm /tmp/fsck_preload.c

set -eu
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
prog=$(basename "$0" .sh)
mkdir -p $mp1

max=$((2 * 1024 * 1024)) # Two alternate super blocks @ 192 and 2240
i=`jot -r 1 1 3`
[ $i -eq 1 ] && flags="-O2"
[ $i -eq 2 ] && flags="-U"
[ $i -eq 3 ] && { flags="-j"; max=$((8 * 1024 * 1024)); }

backup=$mp1/$prog.diskimage.$flags.`date +%Y%m%dT%H%M%S`
core=/tmp/$prog.core.`date +%Y%m%dT%H%M%S`
diskimage=$mp1/$prog.diskimage
log=$mp1/$prog.log

asbs=0
cleans=0
reruns=0
waccess=0

set +e
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 10g -u $u1
newfs $newfs_flags /dev/md$u1 > /dev/null
mount /dev/md$u1 $mp1

[ -c /dev/md$u2 ] && mdconfig -d -u $u2
dd if=/dev/zero of=$diskimage bs=$max count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $u2
backups=`newfs -N $flags md$u2 | grep -A1 "super-block backups" | \
    tail -1 | sed 's/,//g'`
echo "newfs $flags /dev/md$u2"
newfs $flags md$u2 > /dev/null
mdconfig -d -u $u2

chk() {
	local i

	clean=0
	rerun=0
	waccess=0
	LD_PRELOAD=/tmp/fsck_preload.so  \
	    timeout 2m fsck_ffs -fy $1 > $log 2>&1
	r=$?
	if grep -qiE "super-?block.*failed" $log; then
		for b in $backups; do
			asbs=$((asbs + 1))
			LD_PRELOAD=/tmp/fsck_preload.so  \
			    timeout 2m fsck_ffs -b $b -fy $1 > $log 2>&1
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
	/tmp/flip -n 50 $diskimage

	cp $diskimage $backup

	for i in `jot 10`; do
		chk $diskimage
		[ $i -eq 1 -a "$flags" = "-j" ] && continue # First run processes the journal
		[ $rerun -eq 1 ] && { reruns=$((reruns + 1)); continue; }
		[ $clean -eq 1 ] && { cleans=$((cleans + 1)); break; }
		[ -f fsck_ffs.core ] && { s=1; break 2; }
		[ $r -eq 124 ] && { s=2; break 2; } # timeout
	done
	[ $clean -ne 1 ] && break
	[ $r -ne 0 -a $clean -eq 1 ] &&
	    { echo "CLEAN && non zero exit code"; break; }
	[ $clean -eq 1 ] && continue
	[ $usedasb -eq 1 ] && { echo "Alt. SB failed"; s=104; }
	[ $waccess -eq 1 ] && { echo "No write access"; s=105; }
	break
done

echo "$cleans cleans, $reruns reruns, $asbs alternate SBs."
if [ $clean -ne 1 ]; then
	echo "FS still not clean. Last fsck_ffs exit code was $r."
	[ $s -eq 0 ] && s=106
fi
grep -q "Superblock check-hash failed" $log && s=0 # Ignore for now
grep -q "is not a file system superblock" $log && s=0 # Ignore for now
[ $s -ne 0 ] && { gzip $backup; cp -v $backup.gz /tmp; }
cd /tmp
umount $mp1
mdconfig -d -u $u1
rm -f /tmp/flip /tmp/fsck_preload.so
exit $s
