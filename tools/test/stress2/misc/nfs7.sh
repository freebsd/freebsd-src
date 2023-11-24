#!/bin/sh

#
# Copyright (c) 2009-2013 Peter Holm <pho@FreeBSD.org>
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

# NFS test excluding lockd

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

[ ! -d $mntpoint ] &&  mkdir $mntpoint
mount | grep "$mntpoint" | grep nfs > /dev/null && umount $mntpoint
mount -t nfs -o nfsv3,tcp,nolockd -o retrycnt=3 -o intr,soft -o rw \
    $nfs_export $mntpoint
rm -rf /tmp/stressX.control

export RUNDIR=$mntpoint/nfs7.`jot -rc 8 a z | tr -d '\n'`/stressX
USE_TIMEOUT=1
rm -rf $RUNDIR
mkdir -p $RUNDIR
chmod 777 $RUNDIR
export runRUNTIME=10m
rm -rf /tmp/stressX.control/*

su $testuser -c "(cd ..; ./run.sh marcus.cfg) > /dev/null 2>&1"

umount $mntpoint
while mount | grep -q $mntpoint; do
	umount -f $mntpoint > /dev/null 2>&1
done

mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw $nfs_export $mntpoint
sleep .2
su $testuser -c "find $RUNDIR -delete 2>/dev/null"
find $RUNDIR -delete
for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1 || exit 0
