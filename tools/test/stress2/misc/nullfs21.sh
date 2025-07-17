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

# mv <nullfs src> <origin src> removes the file. This is OK on FreeBSD.
# This is handled on Linux with "mount --bind".

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 512m -u $mdstart || exit 1

newfs -n -b 4096 -f 512 -i 1024 md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint

mp2=${mntpoint}2
[ -d $mp2 ] || mkdir -p $mp2
mount | grep -wq $mp2 && umount $mp2
mount -t nullfs $mntpoint $mp2

touch $mntpoint/file

# mv's rename(2) fails as this is a "cross mount" ->
# rm dst; cp src dst; rm src

mv $mp2/file $mountpoint/file
[ -f $mntpoint/file ] ||
    { ls -ali $mntpoint $mp2; status=0; }
#    { echo FAIL; ls -ali $mntpoint $mp2; status=1; }

while mount | grep $mp2 | grep -q nullfs; do
	umount $mp2 || sleep 1
done
n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 30 ] && { echo FAIL; status=2; }
done
mdconfig -d -u $mdstart
exit $status
