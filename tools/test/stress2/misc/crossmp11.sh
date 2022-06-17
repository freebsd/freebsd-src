#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
# All rights reserved.
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

# Copy of crossmp3.sh + elements from dfull.sh

# Live lock seen:
# https://people.freebsd.org/~pho/stress/log/crossmp11.txt

# umount stuck in mnt_ref seen:
# https://people.freebsd.org/~pho/stress/log/kostik1002.txt

# Fixed by r319518, r319519 and r319539.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

g3=$((3 * 1024 * 1024 * 1024))
CONT=/tmp/crossmp11.continue
N=`sysctl -n hw.ncpu`
[ $N -gt 4 ] && N=4
usermem=`sysctl -n hw.usermem`
[ $usermem -gt $g3 ] && usermem=$g3
[ `sysctl -n vm.swap_total` -eq 0 ] && usermem=$((usermem/100*80))
size=$((usermem / 1024 / 1024 / N))

mounts=$N		# Number of parallel scripts

if [ $# -eq 0 ]; then
	echo "Expect:
    /mnt6: write failed, filesystem is full
    /mnt6: create/symlink failed, no inodes free"

	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] &&
		    { mkdir ${mntpoint}$m;  chmod 755 ${mntpoint}$m; }
		mount | grep "${mntpoint}$m " | grep -q md$m && umount ${mntpoint}$m
		mdconfig -l | grep -q md$m && mdconfig -d -u $m

		mdconfig -a -t swap -s ${size}m -u $m
		newfs $newfs_flags md${m} > /dev/null 2>&1
	done

	# start the parallel tests
	touch $CONT
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		./$0 $m &
		./$0 find &
	done

	wait

	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		while mount | grep -q "on ${mntpoint}$m "; do
		    umount ${mntpoint}$m && break
		    sleep 1
		done
		mdconfig -d -u $m
	done
	exit 0
else
	if [ $1 = find ]; then
		while [ -f $CONT ]; do
			find ${mntpoint}* -type f > /dev/null 2>&1
		done
	else
		m=$1
		export runRUNTIME=20s
		# The test: Parallel mount and unmounts
		for i in `jot 3`; do
			mount /dev/md${m} ${mntpoint}$m &&
			   chmod 777 ${mntpoint}$m
			export RUNDIR=${mntpoint}$m/stressX
			export CTRLDIR=${mntpoint}$m/stressX.control
			(cd ${mntpoint}$m && find . -delete)
			if [ -z "$KBLOCKS" ]; then
				r=`df -ik ${mntpoint}$m | tail -1 | awk '{print $4,$7}'`
				export KBLOCKS=`echo $r | awk '{print $1 * 10}'`
				export INODES=`echo  $r | awk '{print $2 * 10}'`
			fi
			su $testuser -c 'cd ..; ./run.sh disk.cfg' > \
			    /dev/null 2>&1

			while mount | grep -q "on ${mntpoint}$m "; do
				opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
				[ -f $CONT ] || break 2
			done
		done
		rm -f $CONT
	fi
fi
