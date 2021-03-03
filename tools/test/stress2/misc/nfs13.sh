#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# nfs loopback mount of a tmpfs file system.
# "[tcp] 127.0.0.1:/mnt: Permission denied" seen.

# " mount_nfs hangs in mntref" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

grep -q $mntpoint /etc/exports 2>/dev/null ||
	{ echo "$mntpoint missing from /etc/exports"; exit 0; }

s=0
m2=${mntpoint}2
[ -d $m2 ] || mkdir $m2
mount | grep "on $m2 " | grep -q nfs && umount $m2
mount | grep "on $mntpoint " | grep -q tmpfs && umount -f $mntpoint

mount -o size=1g -t tmpfs tmpfs $mntpoint || s=1
chmod 777 $mntpoint

[ $s -eq 0 ] &&
    mount -t nfs -o tcp -o rw,retrycnt=1 127.0.0.1:$mntpoint $m2 || s=2

export RUNDIR=$m2/stressX
export runRUNTIME=10m            # Run tests for 10 minutes

[ $s -eq 0 ] &&
    su $testuser -c "(cd ..; ./run.sh marcus.cfg)"

while mount | grep "on $m2 " | grep -q nfs; do
	umount $m2
done

while mount | grep "on $mntpoint " | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
exit $s
