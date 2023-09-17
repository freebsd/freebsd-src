#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# "panic: ext2_dirbad: /mnt: bad dir ino ...: mangled entry" seen.
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=191895

. ../default.cfg

# Uses mke2fs from sysutils/e2fsprogs
[ -x /usr/local/sbin/mke2fs ] || exit 0
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
mke2fs /dev/md${mdstart}a
# No panic seen when disabling hashed b-tree lookup for large directories
# tune2fs -O ^dir_index /dev/md$mdstart
mount -t ext2fs /dev/md${mdstart}a $mntpoint

export RUNDIR=$mntpoint/stressX
export runRUNTIME=10m            # Run tests for 10 minutes
(cd ..; ./run.sh disk.cfg)

while mount | grep "$mntpoint " | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
