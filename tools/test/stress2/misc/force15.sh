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

# FFS+SU with snapshots and forced unmounts

# "panic: flush_pagedep_deps: failed to flush inodedep ..." seen
# https://people.freebsd.org/~pho/stress/log/log0427.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

prog=$(basename "$0" .sh)
diskimage=/tmp/diskimage
log=/tmp/$prog.log
mdstart=10
mntpoint=/mnt
newfs_flags="-U"

set -u
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
dd if=/dev/zero of=$diskimage bs=1m count=1k status=none
mdconfig -a -t vnode -f $diskimage -u $mdstart
flags="-U"

newfs $flags md$mdstart > /dev/null 2>&1

start=`date +%s`
while [ $((`date +%s` - start)) -lt $((5 * 60)) ]; do
	mount /dev/md$mdstart $mntpoint
	rm -fr $mntpoint/lost+found

	jot 10 | xargs -I% mkdir -p $mntpoint/%
	n=5000
	for j in `jot 10`; do
		(
			jot $n | xargs -P0 -I% touch $mntpoint/$j/%
			jot $n | xargs -P0 -I% rm    $mntpoint/$j/%
		) > /dev/null 2>&1 &
	done 

	sleep `jot -r 1 5 20`
	t=`jot -r 1 60 180`
	st=`date +%s`
	mkdir -p $mntpoint/.snap
	for i in `jot 10`; do
		rm -f $mntpoint/.snap/$i
		mksnap_ffs $mntpoint $mntpoint/.snap/$i > /dev/null 2>&1 || break
		sleep `jot -r 1 1 5`
		[ $((`date +%s` - st)) -ge $t ] && break
	done &
	sleep `jot -r 1 2 5`
	while mdconfig -l | grep -q md$mdstart; do
		mdconfig -d -u $mdstart -o force || sleep 1
	done
	sleep 1
	wait
	n=0
	while mount | grep -q "on $mntpoint "; do
		umount $mntpoint || sleep 1
		[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
	done
	mdconfig -a -t vnode -f $diskimage -u $mdstart
	c=0
	for i in `jot 5`; do
		[ $i -ne 1 ] &&
		    echo "$i: fsck_ffs -fy /dev/md$mdstart"
		fsck_ffs -fy /dev/md$mdstart > $log 2>&1; s=$?
		grep 'INTERNAL ERROR: GOT TO reply' $log
		grep -q CLEAN $log && grep -q MODIFIED && c=$((c+=1))
		grep -Eq "WAS MODIFIED" $log || break
	done
	[ $c -gt 1 ] &&
	    { echo "Note: FS marked clean+modified $c times out of $i fsck runs"; s=101; }
	[ $s -ne 0 ] && break
	grep -Eq "IS CLEAN|MARKED CLEAN" $log || { s=102; break; }
done
if [ $s -eq 0 ]; then
	mount /dev/md$mdstart $mntpoint
	cp -R /usr/include $mntpoint
	dd if=/dev/zero of=$mntpoint/big bs=1m count=10 status=none
	find $mntpoint/* -delete

	umount $mntpoint
	mdconfig -d -u $mdstart
	rm -f $diskimage $log
fi
exit $s
