#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm
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

# "panic: flush_newblk_dep: Bad newblk 0xfffff8015fd9e800" seen:
# https://people.freebsd.org/~pho/stress/log/rename15.txt

. ../default.cfg

set -u
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart
fstype=${fstype:-$(echo "-U -j -u" | sed 's/ /\n/g' | sort -R | head -1)} # SU, SUJ or UFS2
echo "newfs $fstype md$mdstart"
newfs $fstype md$mdstart > /dev/null 2>&1
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export LOAD=80
export MAXSWAPPCT=80
export RUNDIR=$mntpoint/stressX
export CTRLDIR=$mntpoint/stressX.control
export dirnprenameLOAD=100
export dirrenameLOAD=100
export ftsINCARNATIONS=2 # Workaround for kern.maxfiles limit exceeded
export renameLOAD=100
export runRUNTIME=10m
export rwLOAD=80
export TESTPROGS='
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/swap/swap
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
'

if [ "$fstype" = "-U" ]; then # workaround for SU disk full issue
	set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
	export KBLOCKS=$(($1 / 7))
	export INODES=$(($2 / 7))
fi

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS > /dev/null'

../tools/killall.sh
for i in `jot 3`; do
	umount $mntpoint && break
	sleep 30
done
if mount | grep -q "on $mntpoint "; then
	mount | sed -n "/\\$mntpoint/p"
	fstat -mf $mntpoint | sed 1d | awk '{print $3}' | while read pid; do
		ps -lp $pid
		procstat -k $pid
		procstat -f $pid
	done
fi
mount | grep -q "on $mntpoint " && exit 1
checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
exit $s
