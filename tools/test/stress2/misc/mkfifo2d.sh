#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Copy of mkfifo2c.sh, but with a SU file system and async mount
# "panic: ufsdirhash_newblk: bad offset" seen from openat()
# https://people.freebsd.org/~pho/stress/log/mkfifo2d.txt
# Fixed by r305601.

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=5m
export RUNDIR=$mntpoint/stressX

export TESTPROGS="
testcases/fts/fts
testcases/link/link
testcases/mkfifo/mkfifo
testcases/mkdir/mkdir
testcases/rename/rename
testcases/swap/swap
"

export ftsLOAD=100
export linkLOAD=100
export mkdirLOAD=100
export mkfifoLOAD=100
export renameLOAD=100
export swapLOAD=100

export renameINCARNATIONS=4
export swapINCARNATIONS=4
export linkINCARNATIONS=12
export mkdirINCARNATIONS=20
export mkfifoINCARNATIONS=22
export ftsINCARNATIONS=2

export HOG=1

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
