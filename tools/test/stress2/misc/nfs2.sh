#!/bin/sh

#
# Copyright (c) 2008-2013 Peter Holm <pho@FreeBSD.org>
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

# Test msdos over nfs. "panic: wrong diroffset" seen.
# This needs to be in /etc/exports: /mnt	-maproot=root 127.0.0.1

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
grep -q $mntpoint /etc/exports ||
	{ echo "$mntpoint missing from /etc/exports"; exit 0; }
[ -x /sbin/mount_msdosfs ] || exit

D=$diskimage
dd if=/dev/zero of=$D bs=1m count=128 status=none || exit

mount | grep "${mntpoint}2" | grep nfs > /dev/null && umount -f ${mntpoint}2
mount | grep "$mntpoint"    | grep /md > /dev/null && umount -f $mntpoint
mdconfig -l | grep -q $mdstart  &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart

gpart create -s bsd md$mdstart
gpart add -t freebsd-ufs md$mdstart
part=a
newfs_msdos -F 16 -b 8192 /dev/md${mdstart}$part > /dev/null
mount -t msdosfs -o rw /dev/md${mdstart}$part $mntpoint

mkdir $mntpoint/stressX
chmod 777 $mntpoint/stressX

[ ! -d ${mntpoint}2 ] &&  mkdir ${mntpoint}2
chmod 777 ${mntpoint}2

mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw \
    127.0.0.1:$mntpoint ${mntpoint}2

export INODES=9999		# No inodes on a msdos fs
export RUNDIR=${mntpoint}2/stressX
export runRUNTIME=10m            # Run tests for 10 minutes
(cd ..; ./run.sh disk.cfg)

umount -f ${mntpoint}2 > /dev/null 2>&1
umount -f $mntpoint    > /dev/null 2>&1
mdconfig -d -u $mdstart
rm -f $D
