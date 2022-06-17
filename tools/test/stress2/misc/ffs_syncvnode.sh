#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# "panic: flush_newblk_deps: Bad newblk 0xc8d2ac00" seen.

# "panic: softdep_deallocate_dependencies: dangling deps" seen with
# /mnt: out of inodes. 20130627.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=20m
export RUNDIR=$mntpoint/stressX

find -x / -type f > /dev/null 2>&1 &
su $testuser -c 'cd ..; ./run.sh disk.cfg'
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
