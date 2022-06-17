#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Parallel mount and umount of file systems. Nullfs version.

# "panic: Lock (lockmgr) null not locked @ kern/vfs_default.c:523." seen.
# http://people.freebsd.org/~pho/stress/log/kostik698.txt
# Fixed by r269708.

# Not fixed: https://people.freebsd.org/~pho/stress/log/kostik798.txt
# https://people.freebsd.org/~pho/stress/log/kostik856.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

N=`sysctl -n hw.ncpu`
usermem=`sysctl -n hw.usermem`
[ `swapinfo | wc -l` -eq 1 ] && usermem=$((usermem/100*80))
size=$((usermem / 1024 / 1024 - 2))

CONT=/tmp/crossmp4.continue
mounts=$N		# Number of parallel scripts

if [ $# -eq 0 ]; then
	mount | grep "$mntpoint" | grep -q md && umount $mntpoint
	mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

	mdconfig -a -t swap -s ${size}m -u $mdstart
	newfs $newfs_flags md$mdstart > /dev/null 2>&1
	mount /dev/md$mdstart $mntpoint

	# start the parallel tests
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		./$0 $m &
		./$0 find &
	done

	wait

	while mount | grep -q "on $mntpoint "; do
		umount $mntpoint > /dev/null 2>&1 || sleep 1
	done
	mdconfig -d -u $mdstart
	exit 0
else
	touch $CONT
	if [ $1 = find ]; then
		while [ -f $CONT ]; do
			find ${mntpoint}* -type f > /dev/null 2>&1
		done
	else
		m=$1
		set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
		export KBLOCKS=$(($1 / N))
		export INODES=$(($2 / N))
		export runRUNTIME=1m
		export INCARNATIONS=4
		# The test: Parallel mount and unmounts
		for i in `jot 4`; do
			[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
			mount -t nullfs $mntpoint ${mntpoint}$m
			mkdir -p ${mntpoint}$m/$m
			chmod 777 ${mntpoint}$m/$m
			export RUNDIR=${mntpoint}$m/$m/stressX
			export CTRLDIR=${mntpoint}$m/$m/stressX.control
			(cd  ${mntpoint}$m/$m && find . -delete)
			su $testuser -c 'cd ..; ./run.sh disk.cfg' > \
			    /dev/null 2>&1 &
			sleep 30

			while mount | grep -q "on ${mntpoint}$m "; do
				opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
			done
			wait
		done
		rm -f $CONT
	fi
fi
