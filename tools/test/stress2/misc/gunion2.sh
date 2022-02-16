#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# geom union test

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

kldstat -v | grep -q geom_union.ko  ||
    { kldload geom_union.ko 2>/dev/null || exit 0; loaded=1; }
log=/tmp/gunion2.sh.log
md1=$mdstart
md2=$((mdstart + 1))
mp1=$mntpoint$md1
mp2=$mntpoint$md2
s=0

set -e
mdconfig -a -t swap -s 5g -u $md1
newfs -n /dev/md$md1 > /dev/null
mkdir -p $mp1 $mp2
mount /dev/md$md1 $mp1
cp -r ../../stress2 $mp1
umount $mp1

mdconfig -a -t swap -s 5g -u $md2
gunion create -v /dev/md$md2 /dev/md$md1
mount /dev/md$md2-md$md1.union $mntpoint

export CTRLDIR=$mntpoint/stressX.control
export INCARNATIONS=10
export LOAD=80
export RUNDIR=$mntpoint/stressX
export runRUNTIME=5m
export rwLOAD=80
export symlinkLOAD=80

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
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
testcases/swap/swap
"
export TESTPROGS=`echo $TESTPROGS | sed 's/\n/ /g'`

set +e
chmod 777 $mntpoint
su $testuser -c \
	"(cd $mntpoint/stress2; ./testcases/run/run $TESTPROGS)" 

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
fsck_ffs -fyR /dev/md$md2-md$md1.union > $log 2>&1
grep -Eq "IS CLEAN|MARKED CLEAN" $log || { s=2; cat $log; }
set +e
gunion commit /dev/md$md2-md$md1.union
gunion list | egrep Block\|Current | egrep -v 0 && s=3
gunion destroy /dev/md$md2-md$md1.union
fsck_ffs -fyR /dev/md$md1 > $log 2>&1
grep -Eq "IS CLEAN|MARKED CLEAN" $log || { s=4; cat $log; }
mdconfig -d -u $md2
mdconfig -d -u $md1
rm -f $log
[ loaded ] && gunion unload
exit $s
