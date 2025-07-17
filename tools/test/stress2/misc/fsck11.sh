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

# Demonstrate how the "CLEAN" message and the exit code can be misleading

# "panic: softdep_update_inodeblock inconsistent ip ..." seen:
# https://people.freebsd.org/~pho/stress/log/log0421.txt
# https://people.freebsd.org/~pho/fsck11.sh.diskimage.20230228T064402.gz

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

set -e
prog=$(basename "$0" .sh)
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
log=$mp1/$prog.sh.log
diskimage=$mp1/$prog.sh.diskimage
backup=/tmp/$prog.sh.diskimage.`date +%Y%m%dT%H%M%S`.gz
cleans=0
reruns=0

# Pick a random newfs flag
newfs_flags=$(echo "" "-U" "-O1" | awk -v N=`jot -r 1 1 3` '{print $N}')
[ $# -eq 1 ] && newfs_flags="$1" # or use script argument
max=$((2 * 1024 * 1024))
[ "$newfs_flags" = "-j" ] && max=$((20 * 1024 * 1024)) # Make room for the journal file

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
echo "newfs $newfs_flags -n md$u2"
newfs $newfs_flags -n md$u2 > /dev/null
[ "$newfs_flags" = "" ] && tunefs -n disable md$u2
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
errors=0
s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	mount /dev/md$u2 $mp2 || break
	if ! ls -lR $mp2 > /dev/null; then
		s=102
		echo "ls failed"; grep "core dumped" /var/log/messages | tail -1
		break
	fi

	find $mp2 -type f | xargs cat > /dev/null
	for j in `jot 9`; do
		rm -rf $mp2/$j
		mkdir $mp2/$j
		jot 10 | xargs -P0 -I% cp /etc/group $mp2/$j/%
	done 2>/dev/null

	while mount | grep -q "on $mp2 "; do umount $mp2; done
	echo * | grep -q core && break
	mdconfig -d -u $u2

	# Introduce 5 random single bit errors to the file system image
	/tmp/flip -n 5 $diskimage

	if [ `stat -f%z $diskimage` -gt $max ]; then
		ls -lh $diskimage
		truncate -s $max $diskimage
	else
		gzip < $diskimage > $backup
	fi
	fsync $backup
	sync; sleep 1

	[ "$newfs_flags" = "-j" ] &&
		fsck -fy $diskimage > $log 2>&1	# process the journal file
	for i in `jot 5`; do
		[ $i -gt 2 ] && echo "fsck run #$i"
		chk $diskimage
		[ $rerun -eq 1 ] && { reruns=$((reruns + 1)); continue; }
		[ $clean -eq 1 ] && { cleans=$((cleans + 1)); break; }
		if [ -f fsck_ffs.core ]; then
			tstamp=`date +%Y%m%dT%H%M%S`
			gzip < $backup > /tmp/fsck_ffs.core.diskimage.$tstamp
			gzip < fsck_ffs.core > /tmp/fsck_ffs.core.$tstamp
			break 2
		fi
	done
	[ $clean -ne 1 ] && { s=99; break; }	# broken image?
	mdconfig -a -t vnode -f $diskimage -u $u2
done
for i in `jot 5`; do
	mount | grep -q "on $mp2 " || break
	umount $mp2 && break
	sleep 2
done
mdconfig -l | grep -q $u2 && mdconfig -d -u $u2

[ $s -eq 0 ] && rm -f $backup || echo "Preserved $backup due to status code $s"
cd /tmp
for i in `jot 5`; do
	umount $mp1 && break
	sleep 2
done
mdconfig -d -u $u1
rm -f /tmp/flip
exit $s
