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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Hunt for "panic: ufsdirhash_newblk: bad offset"

# but page fault seen in scheduler() due to a _thread_lock_flags() call on
# an inactive td.

# Fault seen in "softdep_disk_io_initiation+0x41":
# https://people.freebsd.org/~pho/stress/log/mkfifo.txt

# Run with mkfifo.cfg on a 2g swap backed MD

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

[ "$newfs_flags" = "-U" ] && opt="-j"
newfs $opt md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=20m
export RUNDIR=$mntpoint/stressX

export TESTPROGS="
testcases/creat/creat
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/mkdir/mkdir
testcases/mkfifo/mkfifo
testcases/openat/openat
testcases/rename/rename
testcases/rw/rw
testcases/swap/swap
"
export creatLOAD=100
export ftsLOAD=100
export linkLOAD=100
export lockfLOAD=100
export mkdirLOAD=100
export mkfifoLOAD=100
export openatLOAD=100
export renameLOAD=100
export rwLOAD=100
export swapLOAD=100

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
exit 0
