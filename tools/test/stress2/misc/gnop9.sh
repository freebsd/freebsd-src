#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# fsck test with forced unmount of a SUJ FS.
# Variation of gnop8.sh by Kirk McKusick <mckusick@mckusick.com>

# https://people.freebsd.org/~pho/stress/log/gnop9.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

# Check for lingering threads from the last run
pgrep -x mount && { pgrep -x mount | xargs ps -lp; exit 1; }
../tools/killall.sh || exit 1

fsck=/sbin/fsck_ffs
fsck_loops=10
exp=/sbin/fsck_ffs.exp	# Experimental version
log=/tmp/gnop9.log
[ -f $exp ] && { echo "Using $exp"; fsck=$exp; }
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart || exit 1
md=md$mdstart
newfs -j /dev/$md > /dev/null 2>&1 || exit 1

export LOAD=80
export rwLOAD=80
export runRUNTIME=10m
export RUNDIR=$mntpoint/stressX
export CTRLDIR=$mntpoint/stressX.control
export MAXSWAPPCT=80
export TESTPROGS='
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/socket/socket
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/mkfifo/mkfifo
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
'

set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / 10 * 7))
export INODES=$(($2 / 10 * 7))

start=`date +%s`
while [ $((`date +%s` - start)) -lt 600 ]; do
	gnop create /dev/$md || exit 1
	mount /dev/$md.nop $mntpoint || exit 1
	mkdir -p $RUNDIR $CTRLDIR
	chmod 777 $RUNDIR $CTRLDIR

	# start your favorite I/O test here
	rm -rf /tmp/stressX.control
	(cd $RUNDIR && find . -delete)

	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' &

	# after some number of seconds
	sleep `jot -r 1 30 90`
	gnop destroy -f /dev/$md.nop
	../tools/killall.sh || exit 1
	wait

	# Wait until forcible unmount, may be up to about 30 seconds,
	# but typically very quick if I/O is in progress
	s=`date +%s`
	n=0
	while mount | grep -q "on $mntpoint "; do
		[ $n -eq 0 ] && /bin/echo -n "Waiting for $mntpoint to force umount ..."
		n=$((n + 1))
		sleep 2
		if [ $((`date +%s` - s)) -ge 180 ]; then
		    echo "Giving up on waiting for umount of $mntpoint"
		    umount $mntpoint || umount -f $mntpoint
		    break
		fi
	done
	[ $n -ne 0 ] && echo

	# first fsck will attempt journal recovery
	$fsck -fy /dev/$md > $log 2>&1

	# The second fsck will do traditional check for any errors
	# from journal recovery

	# There seems to be a gnop cache issue, which resolves by adding
	# delays between each fsck run
	for i in `jot $fsck_loops`; do
		sleep $((i * i))	# gnop workaround
		[ $i -ne 1 ] &&
		    echo "`date +%T` $fsck loop #$((i + 1))"
		$fsck -fy /dev/$md > $log 2>&1
		grep -Eq "IS CLEAN|MARKED CLEAN" $log && break
		[ $i -eq $fsck_loops ] &&
		    { echo "$fsck did not mark FS as clean"; exit 1; }
		[ $i -ne 1 ] && tail -3 $log
		sync; sleep 5; sync; sleep 5
	done
done
echo "Final $fsck"
sleep 3; # gnop workaround
$fsck -fy /dev/$md > $log || { tail -5 $log; exit 1; }
mdconfig -d -u ${md#md}
rm -f $log
exit 0
