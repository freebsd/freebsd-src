#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Lock seen with:
#  1001 61985 61981   0  52  0   9624   1028 ufs    D+   0  0:22,89 mkdir
#  1001 61986 61981   0  52  0   9624   1028 ufs    D+   0  0:21,63 mkdir
#  1001 61987 61981   0  52  0   9624   1028 ufs    D+   0  0:23,39 mkdir

# Fixed in rxxxxxx.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=20m
export RUNDIR=$mntpoint/stressX

set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 * 4))
export INODES=$(($2 * 4))

su $testuser -c 'cd ..; ./run.sh disk.cfg' 2>/dev/null

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
