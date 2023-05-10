#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# rename() livelock seen with a forced unmount of a FS
# https://people.freebsd.org/~pho/stress/log/log0375.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
echo "Known issue. Skipping ..."; exit 0

set -u
log=/tmp/force12.sh.log
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 4g -u $mdstart
flags=$newfs_flags
[ `jot -r 1 0 1` -eq 1 ] && flags="-j"
echo "newfs $flags md$mdstart"
newfs $flags md$mdstart > /dev/null 2>&1

export TESTPROGS='
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
testcases/fts/fts
testcases/mkdir/mkdir
testcases/rename/rename
testcases/rw/rw
testcases/swap/swap
'

export runRUNTIME=3m
export RUNDIR=$mntpoint/stressX
export CTRLDIR=$mntpoint/stressX.control

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
export dirnprenameLOAD=100
export dirrenameLOAD=100

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
    /dev/null 2>&1 &

sleep `jot -r 1 60 180`
while mdconfig -l | grep -q md$mdstart; do
	mdconfig -d -u $mdstart -o force || sleep 1
done
sleep 1
../tools/killall.sh
wait
n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 30 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart > /dev/null 2>&1
rm -f $log
exit 0
