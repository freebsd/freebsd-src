#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm
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

# Copy of rename15.sh, modified for msdosfs.

. ../default.cfg

[ -x /sbin/mount_msdosfs ] || exit 0
log=/tmp/msdos12.log

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos -F 32 -b 8192 /dev/md$mdstart$part > /dev/null
mount -t msdosfs /dev/md$mdstart$part $mntpoint
mkdir $mntpoint/stressX
chmod 0777 $mntpoint/stressX
set +e

export LOAD=80
export MAXSWAPPCT=80
export RUNDIR=$mntpoint/stressX
export dirnprenameLOAD=100
export dirrenameLOAD=100
export renameLOAD=100
export runRUNTIME=5m
export rwLOAD=80
export TESTPROGS='
testcases/rename/rename
testcases/swap/swap
'

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

../tools/killall.sh
leftover=`find $mntpoint -type f | wc -l`
if [ $leftover -gt 0 ]; then
	s=1
	find $mntpoint -type f | head -5
fi
for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
fsck -t msdosfs -y /dev/md$mdstart$part > $log 2>&1
if egrep -q "BAD|INCONSISTENCY|MODIFIED" $log; then
	echo "fsck problems:"
	cat $log
	s=2

	mount -t msdosfs /dev/md$mdstart$part $mntpoint || exit 1
	ls -lR $mntpoint | head -5
	umount $mntpoint
fi
mdconfig -d -u $mdstart
rm -f $log
exit $s
