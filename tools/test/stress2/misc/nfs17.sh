#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
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

# Copy tests to a NFS FS and run from there.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

mount | grep "on $mntpoint " | grep -q nfs && umount $mntpoint
mount -t nfs -o tcp -o retrycnt=3 -o soft -o rw $nfs_export $mntpoint
grep -q $mntpoint /etc/exports ||
    { echo "$mntpoint missing from /etc/exports"; exit 0; }

chmod 777 $mntpoint

rm -rf $mntpoint/nfs17; mkdir -p $mntpoint/nfs17
chmod 0777 $mntpoint/nfs17
cp -r ../../stress2 $mntpoint/nfs17

log=/tmp/nfs17.log
export CTRLDIR=$mntpoint/nfs17/stressX.control
export LOAD=80
export MAXSWAPPCT=80
export RUNDIR=$mntpoint/nfs17/stressX
export runRUNTIME=5m
export rwLOAD=80
export TESTPROGS=`cd ..; find testcases/ -perm -1 -type f | \
    egrep -v "/run/|lockf|dirnprename"`

here=`pwd`
cd $mntpoint/nfs17/stress2/misc || exit 1
su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' 2>&1 | tee $log
cd $here
rm -rf $mntpoint/nfs17/stress2

for i in `jot 3`; do
	umount $mntpoint && break
	sleep 10
done
mount | grep -q "on $mntpoint " && { s=1; umount -f $mntpoint; }
sed < /tmp/nfs17.log | sed '/Loop/d;/run time/d'
s=0
[ `sed < /tmp/nfs17.log | sed '/Loop/d;/run time/d' | wc -l` -ne 0 ] &&
	 s=2
[ $s -ne 0 ] && echo "Exit value is $s"
exit $s
