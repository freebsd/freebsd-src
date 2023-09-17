#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Test scenario for a lock cascade problem with sending SIGSTOP to processes
# accessing a NFS intr mount.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

[ ! -d $mntpoint ] &&  mkdir $mntpoint
mount | grep "$mntpoint" | grep nfs > /dev/null && umount $mntpoint
mount -t nfs -o nfsv3,tcp,nolockd -o retrycnt=3 -o intr,soft -o rw \
    $nfs_export $mntpoint
sleep .2

wdir=$mntpoint/nfs8.sh.dir
mkdir -p $wdir
jot 1000 | xargs -I% touch $wdir/%
for i in `jot 10`; do
	find $mntpoint > /dev/null 2>&1 &
	sleep 0.1
	kill -s STOP $!
	pids="$pids $!"
done
rm -rf $wdir

umount $mntpoint 2>&1 | grep -v "Device busy"
while mount | grep -q $mntpoint; do
	umount -f $mntpoint > /dev/null 2>&1
done

kill -s CONT $pids
for pid in $pids; do
	wait $pid
done
exit 0
