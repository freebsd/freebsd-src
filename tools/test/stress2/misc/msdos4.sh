#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

# "panic: leaf should be empty" seen

# "panic: Assertion sq->sq_wchan != NULL" seen:
# https://people.freebsd.org/~pho/stress/log/msdos4.txt

. ../default.cfg

[ -x /sbin/mount_msdosfs ] || exit
log=/tmp/msdos4.sh.log
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos /dev/md${mdstart}$part > /dev/null
mount -t msdosfs /dev/md${mdstart}$part $mntpoint || exit 1

export RUNDIR=$mntpoint/stressX
export runRUNTIME=10m
export TESTPROGS='
testcases/lockf2/lockf2
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/swap/swap
'

(cd ..; ./testcases/run/run $TESTPROGS)

while mount | grep "$mntpoint" | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
fsck -t msdosfs -y /dev/md${mdstart}$part > $log 2>&1
s=0
if egrep -q "BAD|INCONSISTENCY|MODIFIED" $log; then
	cat $log
	rm $log
	s=1
fi
mdconfig -d -u $mdstart
s=0	# Ignore for now
exit $s
