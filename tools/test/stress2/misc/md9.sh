#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

# memory disk with vnode on a tmpfs file system triggers:
# "g_handleattr(GEOM::ident): md10 bio_length 24 len 31 -> EFAULT"

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

set -e
mp2=${mntpoint}2
mkdir -p $mp2
mount | grep "on $mp2 " | grep -q /dev/md && umount -f $mp2
mount -t tmpfs tmpfs $mp2

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
diskimage=$mp2/diskimage
dd if=/dev/zero of=$diskimage bs=1m count=2k status=none
mdconfig -a -t vnode -f $diskimage -u $mdstart
newfs -U /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

for i in `jot 10`; do
	umount $mntpoint && break
	sleep .5
done
mount | grep "on $mntpoint " && { echo FATAL; exit 1; }
mdconfig -d -u $mdstart
umount $mp2
tail -5 /var/log/messages | grep g_handleattr && s=1 || s=0

exit $s
