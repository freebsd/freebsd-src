#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Test scenario for the change of a global SU lock to a per filesystem lock.
# "panic: softdep_write_inodeblock: indirect pointer #0 mismatch ..." seen.
# http://people.freebsd.org/~pho/stress/log/kirk063.txt

# https://people.freebsd.org/~pho/stress/log/kirk080.txt

. ../default.cfg

[ `swapinfo | wc -l` -eq 1 ] && exit 0

md1=$mdstart
md2=$((mdstart + 1))
mp1=${mntpoint}$md1
mp2=${mntpoint}$md2
mkdir -p $mp1 $mp2

usermem=`sysctl -n hw.usermem`
size=$((2 * 1024 * 1024 * 1024))	# Ideal disk size is 2G
[ $((size * 2)) -gt $usermem ] && size=$((usermem / 2))
size=$((size / 1024 / 1024))

opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-j" || echo "-U")
[ "$newfs_flags" = "-U" ] || opt=""
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
mdconfig -l | grep -q md$md1 &&  mdconfig -d -u $md1
mdconfig -a -t swap -s ${size}m -u $md1
newfs $opt md${md1} > /dev/null
mount /dev/md${md1} $mp1
chmod 777 $mp1

mount | grep "on $mp2 " | grep -q /dev/md && umount -f $mp2
mdconfig -l | grep -q md$md2 &&  mdconfig -d -u $md2
mdconfig -a -t swap -s ${size}m -u $md2
newfs $opt md${md2} > /dev/null
mount /dev/md${md2} $mp2
chmod 777 $mp2

export runRUNTIME=10m
export RUNDIR=$mp1/stressX
export CTRLDIR=$mp1/stressX.control
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
su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > /dev/null 2>&1 &

export TESTPROGS="$TESTPROGS testcases/swap/swap"
export RUNDIR=$mp2/stressX
export CTRLDIR=$mp2/stressX.control
su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > /dev/null 2>&1 &
wait

while mount | grep "$mp2 " | grep -q /dev/md; do
	umount $mp2 || sleep 1
done
mdconfig -d -u $md2
while mount | grep "$mp1 " | grep -q /dev/md; do
	umount $mp1 || sleep 1
done
mdconfig -d -u $md1
