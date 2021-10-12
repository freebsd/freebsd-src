#!/bin/sh

#
# Copyright (c) 2008-2011 Peter Holm <pho@FreeBSD.org>
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

# unionfs test using two memory disks

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
mount | grep -E "$mp1|$mp2"
set +e

export RUNDIR=$mp2/stressX
export CTRLDIR=$mp2/stressX.control
export runRUNTIME=2m

# SU work around for "disk full"
set `df -ik $mp2 | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / 2))
export INODES=$(($2 / 2))

(cd ..; ./run.sh disk.cfg)

find $RUNDIR -ls
for i in `jot 5`; do
	mount |  grep -q unionfs || break
	umount $mp2 || sleep 2	# The unionfs mount
done
mount | grep unionfs && exit 1

for i in `jot 5`; do
	umount $mp2 && break
	sleep 2
done
mount | grep -q "on $mp2 " && exit 1
n=`find $mp1/stressX | wc -l`
[ $n -eq 1 ] && s=0 || s=1
umount $mp1
mdconfig -d -u $md2
mdconfig -d -u $md1
exit $s
