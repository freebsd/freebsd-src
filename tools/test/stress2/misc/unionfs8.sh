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

# unionfs(8) test with a cd9660 file system

# "panic: unionfs_noderem: vnode 0xfffffe014f9259c8 locked recursively" seen
# https://people.freebsd.org/~pho/stress/log/log0233.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "`type mkisofs 2>/dev/null`" ] &&
    { echo "cdrtools not installed"; exit 0; }
. ../default.cfg

I=`dirname $diskimage`/unionfs8.iso
md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2
mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done
for i in $md1 $md2; do
	mdconfig -l | grep -q md$i && mdconfig -d -u $i
done

mdconfig -a -t swap -s 4g -u $md1
mdconfig -a -t swap -s 4g -u $md2
newfs $newfs_flags -n md$md1 > /dev/null
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md1 $mp1
cp -r ../../stress2 $mp1
mkisofs -o $I -r $mp1 > /dev/null 2>&1
umount $mp1
mdconfig -d -u $md1
mdconfig -a -t vnode -f $I -u $md1
mount -t cd9660 /dev/md$mdstart $mp1
ls -l $mp1
mount /dev/md$md2 $mp2
chmod 777 $mp2

mount -t unionfs -o below $mp1 $mp2
set +e
mount | grep -E "$mp1|$mp2"
ls -ld $mp1 $mp2
ls -l  $mp1 $mp2

export CTRLDIR=$mp2/stressX.control
export INCARNATIONS=10
export LOAD=80
export RUNDIR=$mp2/stressX
export runRUNTIME=5m
export rwLOAD=80
export symlinkLOAD=80

export TESTPROGS="
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/mkfifo/mkfifo
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
testcases/swap/swap
"

export TESTPROGS=`echo $TESTPROGS | sed 's/\n/ /g'`

set +e
###su $testuser -c \
###	"(cd $mp2/stress2; ./testcases/run/run $TESTPROGS)"

umount $mp2	# The unionfs mount
umount $mp2
n=`find $mp1/stressX | wc -l`
[ $n -eq 1 ] && s=0 || { find $mp1/stressX -ls | head -12; s=1; }
umount $mp1
mdconfig -d -u $md2
mdconfig -d -u $md1
rm -f $I
exit $s
