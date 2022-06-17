#!/bin/sh

#
# Copyright (c) 2016 Dell EMC Isilon
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

# ext2fs parallel mount & umount test scenario
# "panic: vm_fault_hold: fault on nofault entry" seen.
# https://people.freebsd.org/~pho/stress/log/crossmp10.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "`which mke2fs`" ] && echo "mke2fs not found" && exit 0

. ../default.cfg

CONT=/tmp/crossmp10.continue
mounts=4	# Number of parallel scripts
size=512	# Disk size in MB

if [ $# -eq 0 ]; then
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] &&
		    { mkdir ${mntpoint}$m;  chmod 755 ${mntpoint}$m; }
		mount | grep "${mntpoint}$m " | grep -q md$m && umount ${mntpoint}$m
		mdconfig -l | grep -q md$m && mdconfig -d -u $m

		mdconfig -a -t swap -s ${size}m -u $m
		mke2fs -m 0 /dev/md${m} > /dev/null 2>&1
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
			find ${mntpoint}* -ls > /dev/null 2>&1
			sleep .1
		done
	else
		export runRUNTIME=30s
		# The test: Parallel mount and unmounts
		start=`date '+%s'`
		while [ $((`date '+%s'` - start)) -lt 300 ]; do
			m=$1
			mount -t ext2fs /dev/md${m} ${mntpoint}$m &&
			   chmod 777 ${mntpoint}$m
			export RUNDIR=${mntpoint}$m/stressX
			export CTRLDIR=${mntpoint}$m/stressX.control
			(cd ${mntpoint}$m && find . -delete)
			su $testuser -c 'cd ..; ./run.sh disk.cfg' > /dev/null 2>&1 &

			sleep 2
			while mount | grep -q "on ${mntpoint}$m "; do
				opt=$([ `jot -r 1 0 1` -eq 0 ] && echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
				[ -f $CONT ] || break 2
			done
			wait $!
		done
		rm -f $CONT
		../tools/killall.sh
	fi
fi
