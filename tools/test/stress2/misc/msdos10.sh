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

# fsx test of with msdosfs and a 1k block size
# "panic: Assertion ma[i]->dirty == VM_PAGE_BITS_ALL failed" seen.
# Fixed by r324794

# Original test scenario by fsu@freebsd.org

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ -r /usr/src/tools/regression/fsx/fsx.c ] || exit 0

[ -x /sbin/mount_msdosfs ] || exit 0
dir=/tmp
odir=`pwd`
cd $dir
cc -o fsx -Wall -Wextra -O2 -g /usr/src/tools/regression/fsx/fsx.c || exit 1
rm -f fsx.c
cd $odir
log=/tmp/fsx.sh.log
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

set -e
mdconfig -a -t swap -s 4g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos -b 1024 /dev/md${mdstart}$part > /dev/null
mount -t msdosfs /dev/md${mdstart}$part $mntpoint
set +e

cp /tmp/fsx $mntpoint

cd $mntpoint

NUM_OPS=2000
SEED=2016
(
./fsx -S ${SEED} -N ${NUM_OPS}                       ./TEST_FILE0 &
./fsx -S ${SEED} -l 5234123 -o 5156343 -N ${NUM_OPS} ./TEST_FILE1 &
./fsx -S ${SEED} -l 2311244 -o 2311200 -N ${NUM_OPS} ./TEST_FILE2 &
./fsx -S ${SEED} -l 8773121 -o  863672 -N ${NUM_OPS} ./TEST_FILE3 &
./fsx -S ${SEED} -l  234521 -o  234521 -N ${NUM_OPS} ./TEST_FILE4 &
./fsx -S ${SEED} -l  454321 -o      33 -N ${NUM_OPS} ./TEST_FILE5 &
./fsx -S ${SEED} -l 7234125 -o 7876728 -N ${NUM_OPS} ./TEST_FILE6 &
wait
) > /dev/null
cd /

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
rm /tmp/fsx $log
exit $s
