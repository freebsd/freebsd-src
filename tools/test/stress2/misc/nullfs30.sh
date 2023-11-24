#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# Parallel mount and umount test of file mounts,
# mounting over non-directories

# panic: free: address 0xffffffff80b229b4 ... has not been allocated
# https://people.freebsd.org/~pho/stress/log/log0454.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

cont=/tmp/nullfs30.continue
mounts=3	# Number of parallel scripts

set -eu
md1=$mdstart
md2=$((md1 + 1))
mp1=$mntpoint$md1
mp2=$mntpoint$md2
mkdir -p $mp1 $mp2

mdconfig -a -t swap -s 256m -u $md1
newfs $newfs_flags -n md$md1 > /dev/null
mount /dev/md$md1 $mp1
mdconfig -a -t swap -s 256m -u $md2
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md2 $mp2

for i in `jot $mounts`; do
	cp /etc/group $mp1/f$i
	touch $mp2/m$i
done
set +e

mount -t nullfs $mp1/f1 $mp2/m1 || {
    umount $mp2
    umount $mp1
    mdconfig -d -u $md2
    mdconfig -d -u $md1
    echo "File mount not implemented"
    exit 0
}

(cd ../testcases/swap; ./swap -t 5m -i 20 > /dev/null) &

# Start the parallel tests
touch $cont
while [ -f $cont ]; do
	while mount | grep -q "on $mp2/m1 "; do umount $mp2/m1; done 2> /dev/null
	mount -t nullfs $mp1/f1 $mp2/m1
done &
while [ -f $cont ]; do
	while mount | grep -q "on $mp2/m2 "; do umount $mp2/m2; done 2> /dev/null
	mount -t nullfs $mp1/f2 $mp2/m2
done &
while [ -f $cont ]; do
	while mount | grep -q "on $mp2/m3 "; do umount $mp2/m3; done 2> /dev/null
	mount -t nullfs $mp1/f3 $mp2/m3
done &

start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	cat $mp2/m1 $mp2/m2 $mp2/m3 > /dev/null
done
rm -f $cont
while pgrep -q swap; do pkill swap; done
wait

for i in 1 2 3; do
	while mount | grep -q "on $mp2/m$i "; do umount $mp2/m$i; done 2> /dev/null
done

# Check error handling
mount -t nullfs $mp1/f1 $mp2/m1
mount -t nullfs $mp1/f1 $mp2/m1 2>/dev/null && s=1 || s=0
while mount | grep -q "on $mp2/m1 "; do umount $mp2/m1; done 2> /dev/null

mkdir $mp2/dir
mount -t nullfs $mp1/f1 $mp2/dir 2>/dev/null && s=2

umount $mp2
umount $mp1
mdconfig -d -u $md2
mdconfig -d -u $md1
exit $s
