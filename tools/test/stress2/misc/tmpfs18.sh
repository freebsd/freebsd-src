#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# nfs loop back mount of a tmpfs file system.
# "[tcp] 127.0.0.1:/mnt: Permission denied" seen, when tmpfs options are
# used.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

grep -q $mntpoint /etc/exports ||
	{ echo "$mntpoint missing from /etc/exports"; exit 0; }

s=0
m2=${mntpoint}2
[ -d $m2 ] || mkdir $m2
mount | grep "on $m2 " | grep -q nfs && umount $m2
mount | grep "on $mntpoint " | grep -q tmpfs && umount -f $mntpoint

[ $# -eq 1 ] &&
    mount -t tmpfs tmpfs $mntpoint || s=1 # This works
[ $# -eq 0 ] &&
    mount -o size=100m -t tmpfs tmpfs $mntpoint || s=1
mount -t nfs -o tcp -o rw,retrycnt=1 127.0.0.1:$mntpoint $m2 || s=2

while mount | grep "on $m2 " | grep -q nfs; do
	umount $m2
done

while mount | grep "on $mntpoint " | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
exit $s
