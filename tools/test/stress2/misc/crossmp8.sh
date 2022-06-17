#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Variation of crossmp3.sh. fifos and sockets added to load.
# Not really a cross mount point test, but a test of the old
# non-directory use of the vnode v_un union.
# mckusick@ suggested using fifos for this test.

# "panic: mtx_lock() of spin mutex  @ ../kern/vfs_subr.c:512" seen.
# https://people.freebsd.org/~pho/stress/log/crossmp8.txt
# Fixed by r291671.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

CONT=/tmp/crossmp8.continue
N=`sysctl -n hw.ncpu`
usermem=`sysctl -n hw.usermem`
[ `swapinfo | wc -l` -eq 1 ] && usermem=$((usermem/100*80))
size=$((usermem / 1024 / 1024 / N))

mounts=$N		# Number of parallel scripts

if [ $# -eq 0 ]; then
	oldmx=`sysctl -n kern.maxvnodes`
	trap "sysctl kern.maxvnodes=$oldmx > /dev/null" EXIT SIGINT
	sysctl kern.maxvnodes=3072 > /dev/null

	for i in `jot $mounts`; do
		m=$((i + mdstart - 1))
		[ ! -d ${mntpoint}$m ] &&
		    { mkdir ${mntpoint}$m;  chmod 755 ${mntpoint}$m; }
		mount | grep "${mntpoint}$m " | grep -q md$m &&
		    umount ${mntpoint}$m
		mdconfig -l | grep -q md$m && mdconfig -d -u $m

		mdconfig -a -t swap -s ${size}m -u $m
		newfs md${m} > /dev/null 2>&1
	done

	# start the parallel tests
	touch $CONT
	for i in `jot $mounts`; do
		m=$((i + mdstart - 1))
		./$0 $m &
		./$0 find &
	done
	sleep 60
	rm -f $CONT
	../tools/killall.sh
	wait

	for i in `jot $mounts`; do
		m=$((i + mdstart - 1))
		while mount | grep -q "on ${mntpoint}$m "; do
		    umount ${mntpoint}$m && break
		    sleep 1
		done
		mdconfig -d -u $m
	done
	./cleanup.sh
	exit 0
else
	if [ $1 = find ]; then
		while [ -f $CONT ]; do
			find ${mntpoint}* -maxdepth 1 -ls > /dev/null 2>&1
			sleep .1
		done
	else
		export RUNTIME=15s
		export runRUNTIME=15s
		# The test: Parallel mount and unmounts
		start=`date '+%s'`
		while [ $((`date '+%s'` - start)) -lt 300 ]; do
			m=$1
			mount /dev/md${m} ${mntpoint}$m &&
			   chmod 777 ${mntpoint}$m
			export RUNDIR=${mntpoint}$m/stressX
			export CTRLDIR=${mntpoint}$m/stressX.control
			export mkfifoLOAD=80
			export socketLOAD=80
			export TP="
			testcases/mkfifo/mkfifo
			testcases/mkdir/mkdir
			"
			(cd ${mntpoint}$m && find . -delete)
			su $testuser -c 'cd ..; ./testcases/run/run $TP' > \
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
