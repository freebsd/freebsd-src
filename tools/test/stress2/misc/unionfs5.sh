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

# "mkdir: mkdir(d2), level 2. mkdir.c:96: No such file or directory" seen
# with a non debug kernel.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

# unionfs usage example from the man page:
#	   mount -t cd9660 -o ro /dev/cd0 /usr/src
#	   mount -t unionfs -o noatime /var/obj	/usr/src

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

mdconfig -a -t swap -s 2g -u $md1
mdconfig -a -t swap -s 2g -u $md2
newfs $newfs_flags -n md$md1 > /dev/null
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2

mount -t unionfs -o noatime $mp1 $mp2
set +e
mount | grep -E "$mp1|$mp2"

if [ $# -eq 0 ]; then
	echo "Using unionfs"
	export RUNDIR=$mp2/stressX
else
	echo "Using FFS"
	export RUNDIR=$mp1/stressX
fi
export runRUNTIME=2m 

(cd ../testcases/mkdir; ./mkdir -t 2m -i 20)

find $RUNDIR -ls | head -5
while mount | grep -Eq "on $mp2 .*unionfs"; do
	umount $mp2 && break
	sleep 5
done
umount $mp2
n=`find $mp1/stressX | wc -l`
[ $n -eq 1 ] && s=0 || s=1
umount $mp1
mdconfig -d -u $md2
mdconfig -d -u $md1
exit $s
