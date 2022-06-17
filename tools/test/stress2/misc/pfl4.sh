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

# "panic: invalid queue 255" seen:
# https://people.freebsd.org/~pho/stress/log/pfl4.txt

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -le 8 ] && exit 0

mounts=4
newfs_flags=""

export runRUNTIME=10m
export LOAD=80
export symlinkLOAD=80
export rwLOAD=80
export TESTPROGS="
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/mkfifo/mkfifo
"

prefix=$mntpoint
start=$mdstart
for i in `jot $mounts $start`; do
	mdstart=$i
	mntpoint=${prefix}$i
	[ -d $mntpoint ] || mkdir -p $mntpoint

	mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
	mdconfig -a -t swap -s 2g -u $mdstart
	newfs $newfs_flags md$mdstart > /dev/null
	mount /dev/md$mdstart $mntpoint
	chmod 777 $mntpoint

	export RUNDIR=$mntpoint/stressX
	export CTRLDIR=$mntpoint/stressX.control
	set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
	export KBLOCKS=$(($1 / 1))
	export INODES=$(($2 / 1))
	su $testuser -c 'sleep 2; cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &
done
su $testuser -c "sleep 2; cd ..; ./testcases/swap/swap -t 10m -i 20" &

wait

s=0
for i in `jot $mounts $start`; do
	mdstart=$i
	mntpoint=${prefix}$i
	n=0
	while mount | grep -q "on $mntpoint "; do
		umount $mntpoint || sleep 1
		n=$((n += 1))
		[ $n -gt 60 ] && exit 1
	done
	checkfs /dev/md$mdstart || s=$?
	mdconfig -d -u $mdstart
done
exit $s
