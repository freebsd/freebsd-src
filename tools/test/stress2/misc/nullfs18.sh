#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Demonstate nullfs(5) inode leak.
# Fixed by r295717.

. ../default.cfg

N=3

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

newfs -n md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / N))
export INODES=$(($2 / N))

export runRUNTIME=2m
export LOAD=80
export symlinkLOAD=80
export rwLOAD=80
export TESTPROGS="
testcases/rw/rw
testcases/creat/creat
testcases/mkdir/mkdir
"

for i in `jot $N 1`; do
	eval mp$i=${mntpoint}$i
done

for i in `jot $N 1`; do
	eval mp=\$mp$i
	[ -d $mp ] || mkdir -p $mp
	mount | grep $mp | grep -q nullfs && umount -f $mp
	msrc=$mntpoint/d$i
	mkdir -p $msrc
	chmod 777 $msrc
	mount -t nullfs $msrc $mp
	chmod 777 $mp
	export RUNDIR=$mp/stressX
	export CTRLDIR=$mp/stressX.control
	mkdir $RUNDIR $CTRLDIR
	chmod 777 $RUNDIR $CTRLDIR
	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &
	mps="$mps $mp"
done

(cd ../testcases/swap; ./swap -t 10m -i 20 > /dev/null 2>&1) &
sleep 1
while pgrep -q run; do
	find $mps -ls > /dev/null 2>&1
done
while pgrep -q swap; do
	pkill -9 swap
done
wait

(cd $mntpoint; find . -delete)
sync; sleep 1; sync; sleep 1; sync
inodes=`df -i $mntpoint | tail -1 | awk '{print $6}'`
if [ $inodes -ne 4 ]; then
	echo "FAIL 1"
	e=1
	mount | sed -n "1p;/${mntpoint#/}/p"
	echo
	df -ik | sed -n "1p;/${mntpoint#/}/p"
	printf "\nfind ${mntpoint}* -ls\n"
	find ${mntpoint}* -ls

	for i in `jot $N 1`; do
		eval mp=\$mp$i
		echo "umount $mp"
		mount | grep $mp | grep -q nullfs && umount $mp
	done

	echo
	df -ik | sed -n "1p;/${mntpoint#/}/p"
else
	for i in `jot $N 1`; do
		eval mp=\$mp$i
		mount | grep $mp | grep -q nullfs && umount $mp
	done
	inodes=`df -i $mntpoint | tail -1 | awk '{print $6}'`
	if [ $inodes -ne 1 ]; then
		echo "FAIL 2"
		e=2
		mount | sed -n "1p;/${mntpoint#/}/p"
		echo
		df -ik | sed -n "1p;/${mntpoint#/}/p"
	fi
fi

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
exit $e
