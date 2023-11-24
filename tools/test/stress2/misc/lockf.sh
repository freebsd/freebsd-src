#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Page fault seen:
# http://people.freebsd.org/~pho/stress/log/lockf.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

pgrep -q lockd || { echo "lockd not running"; exit 0; }
trap "rm -f /tmp/$0.$$.*" EXIT INT
mount | grep "$mntpoint" | grep nfs > /dev/null && umount $mntpoint

mount -t nfs -o tcp -o retrycnt=3 -o intr -o soft \
    -o rw $nfs_export $mntpoint
sleep .5
lockf -t 10 $mntpoint/$$.lock sleep 2 || fail=1
umount $mntpoint
[ $fail ] && exit 1

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ] ; do
	mount -t nfs -o tcp -o retrycnt=3 -o intr -o soft \
	    -o rw $nfs_export $mntpoint || break
	sleep 1

	for j in `jot 50`; do
		lockf -t 10 $mntpoint/$0.$$.$j sleep 3 &
	done

	while mount | grep -q $mntpoint; do
		umount -f $mntpoint > /dev/null 2>&1
	done
	wait
done 2>/dev/null
mount -t nfs -o tcp -o retrycnt=3 -o intr -o soft \
    -o rw $nfs_export $mntpoint || break
sleep .5
rm $mntpoint/$0.*
while mount | grep -q $mntpoint; do
	umount -f $mntpoint > /dev/null 2>&1
done
exit 0
