#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Composite test: nullfs2.sh + kinfo.sh

# Kernel page fault with the following non-sleepable locks held from
# nullfs/null_vnops.c:531

# Fatal trap 12: page fault while in kernel mode
# https://people.freebsd.org/~pho/stress/log/jeff106.txt

# panic: vholdl: inactive held vnode:
# https://people.freebsd.org/~pho/stress/log/kostik815.txt

# umount busy seen:
# https://people.freebsd.org/~pho/stress/log/kostik893.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

# NULLFS(5) and SUJ has known issues.
mount | grep "on `df $RUNDIR | sed  '1d;s/.* //'` " | \
    grep -q  "journaled soft-updates" &&
    { echo "Skipping test due to SUJ."; exit 0; }

odir=`pwd`
cd /tmp
sed '1,/^EOF/d;s/60/600/' < $odir/kinfo.sh > kinfo.c
mycc -o kinfo -Wall -g kinfo.c -lutil
rm -f kinfo.c
cd $odir

mount | grep -q procfs || mount -t procfs procfs /proc

for j in `jot 5`; do
	/tmp/kinfo &
done

mount | grep -q $mntpoint && umount -f $mntpoint

mount -t nullfs `dirname $RUNDIR` $mntpoint

export RUNDIR=$mntpoint/stressX
export runRUNTIME=10m
#(cd ..; ./run.sh marcus.cfg)
(cd ..; timeout -k 15m 12m ./run.sh marcus.cfg)

umount $mntpoint 2>&1 | grep -v busy

mount | grep -q $mntpoint && umount -f $mntpoint

wait
rm -f /tmp/kinfo
