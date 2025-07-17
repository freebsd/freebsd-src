#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# MEMGUARD(9) test scenario: Page fault seen in softdep_setup_inomapdep().

. ../default.cfg

sysctl vm | grep -q memguard || { echo "MEMGUARD(9) not enabled"; exit 0; }

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 20m -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=5m
export RUNDIR=$mntpoint/stressX
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 * 2))
export  INODES=$(($2 * 2))

sysctl vm.memguard.options=3 > /dev/null
sysctl vm.memguard.desc=inodedep

su $testuser -c 'cd ..; ./run.sh disk.cfg' > /dev/null

sysctl vm.memguard.desc="" > /dev/null
sysctl vm.memguard.options=1 > /dev/null

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
