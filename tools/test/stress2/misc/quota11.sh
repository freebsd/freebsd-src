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

# "panic: share->excl" seen.
# http://people.freebsd.org/~pho/stress/log/kostik715.txt
# Fixed in r270795 and r270797

# Test scenario by Hiroki Sato <hrs FreeBSD org>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0

. ../default.cfg

mount | grep "on $mntpoint " | grep -q md$mdstart && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
export PATH_FSTAB=/tmp/fstab
trap "rm -f $PATH_FSTAB" 0
echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" > $PATH_FSTAB
mount $mntpoint
edquota -u -f $mntpoint -e $mntpoint:1000:2000:100:200 root
quotaon $mntpoint

while true; do repquota -av > /dev/null; done &

dd if=/dev/random of=$mntpoint/foo.data bs=512 count=1024x1024 2>&1 |
    egrep -v "trans|record"
kill $!
wait

n=0
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 60 ] && exit 1
done
mdconfig -d -u $mdstart
exit 0
