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

# msdosfs rename test over nfs loopback mount
# This needs to be in /etc/exports: /mnt -maproot=root 127.0.0.1

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
mp1=$mntpoint
mp2=$mntpoint$((mdstart + 1))
grep -q $mp1 /etc/exports ||
	{ echo "$mp1 missing from /etc/exports"; exit 0; }
[ -x /sbin/mount_msdosfs ] || exit

mount | grep "$mp2 " | grep nfs > /dev/null && umount -f $mp2
mount | grep "$mp1 " | grep /md > /dev/null && umount -f $mp1
mdconfig -l | grep -q $mdstart && mdconfig -d -u $mdstart

kill -HUP `pgrep mountd` # loopback workaround
mdconfig -a -t swap -s 1g -u $mdstart
set -e

bsdlabel -w md$mdstart auto
newfs_msdos -F 32 -b 8192 /dev/md${mdstart}$part > /dev/null
mkdir -p $mp1; chmod 777 $mp1
mount -t msdosfs -o rw /dev/md${mdstart}$part $mp1
set +e

mkdir $mp1/stressX
chmod 777 $mp1/stressX

mkdir -p $mp2
chmod 777 $mp2

mount -t nfs -o tcp -o retrycnt=3 -o rw \
    127.0.0.1:$mp1 $mp2; s=$?

export LOAD=80
export renameLOAD=100
export TESTPROGS="
testcases/rename/rename
testcases/swap/swap
"
export INODES=9999		# No inodes on a msdos fs

export RUNDIR=$mp2/stressX
export runRUNTIME=2m
if [ $s -eq 0 ]; then
	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

	for i in `jot 10`; do
		umount $mp2 && break
		sleep 2
	done
fi
sleep .5
for i in `jot 10`; do
	umount $mp1 && break
	sleep 2
done
mount | grep -q "on $mp1 " && umount -f $mp1
mdconfig -d -u $mdstart
exit 0
