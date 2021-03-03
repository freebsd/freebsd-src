#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# "Fatal trap 9: general protection fault while in kernel mode" seen:
# https://people.freebsd.org/~pho/stress/log/lockf5.txt

# Test scenario by: ngie@FreeBSD.org.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
pgrep -q lockd || { echo "lockd not running."; exit 1; }

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -t nfs $nfs_export $mntpoint || exit 1
sleep .5
rm -rf $mntpoint/$0
mkdir -p $mntpoint/$0

for i in `jot 100`; do
	(cd $mntpoint/$0; lockf -t 10 f$i sleep 5) > /dev/null 2>&1 &
done
sleep 3

umount -f $mntpoint
wait

mount -t nfs $nfs_export $mntpoint || exit 1
sleep .5
rm -rf $mntpoint/$0
umount $mntpoint
exit 0
