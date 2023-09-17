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

# Scenario: mount -t nullfs /mnt /mnt

# Seen:
# [root@mercat1 /home/pho]# ps -lUroot | grep -v grep | grep -E "find|umount"
#   0 23496  3144  0  52  0  12856  2544 ufs      D+    0   0:00.00 find /mnt -type f -maxdepth 2 -ls
#   0 23497  3126  6  52  0  12812  2512 mount dr D+    0   0:00.00 umount /mnt
# [root@mercat1 /home/pho]# 

# Test suggestion by Jason Harmening:

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

nullfs_srcdir=$mntpoint
nullfs_dstdir=$mntpoint
runtime=300

set -e
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
set +e

start=`date '+%s'`
while [ `date '+%s'` -lt $((start + $runtime)) ]; do
	find $nullfs_dstdir -type f -maxdepth 2 -ls > \
	    /dev/null 2>&1
done &

(cd ../testcases/swap; ./swap -t ${runtime}s -i 20) &
start=`date '+%s'`
while [ `date '+%s'` -lt $((start + $runtime)) ]; do
	mount_nullfs $nullfs_srcdir $nullfs_dstdir
	opt=$([ `jot -r 1 0 1` -eq 0 ] && echo "-f")
	while mount | grep nullfs | grep -q ${nullfs_dstdir}; do
		umount $opt $nullfs_dstdir
	done
done > /dev/null 2>&1
pkill swap
wait
n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 30 ] && { echo FAIL; status=2; }
done
mdconfig -d -u $mdstart
exit 0
