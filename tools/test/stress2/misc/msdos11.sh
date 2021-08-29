#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=257522
# https://people.freebsd.org/~pho/stress/log/log0158.txt
# Original test scenario by trasz@FreeBSD.org

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

[ -x /sbin/mount_msdosfs ] || exit 0
log=/tmp/msdos11.log
dir=/tmp
odir=`pwd`
cd $dir
cd $odir
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

set -e
mdconfig -a -t swap -s 4g -u $mdstart
bsdlabel -w md$mdstart auto
newfs_msdos -b 1024 /dev/md${mdstart}$part > /dev/null
mount -t msdosfs /dev/md${mdstart}$part $mntpoint
set +e

(cd $odir/../testcases/swap; ./swap -t 2m -i 20 -l 100) > /dev/null &
sleep 2
cd $mntpoint
for i in `jot 2`; do
	for i in `jot 10000`; do
		mkdir a
		mv a b
		rmdir b
	done > /dev/null 2>&1 &
done
wait
cd $odir

while mount | grep "$mntpoint" | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
fsck -t msdosfs -y /dev/md${mdstart}$part > $log 2>&1
if egrep -q "BAD|INCONSISTENCY|MODIFIED" $log; then
	cat $log
	s=1

	mount -t msdosfs /dev/md${mdstart}$part $mntpoint || exit 1
	ls -lR $mntpoint
	umount $mntpoint
fi
mdconfig -d -u $mdstart
rm $log
exit $s
