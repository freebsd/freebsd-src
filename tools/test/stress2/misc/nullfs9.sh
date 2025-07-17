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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# nullfs + tmpfs regression test:
# ETXTBSY problem of executable
# Fails with "./nullfs9.sh: cannot create /mnt2/mp/true: Text file busy"

. ../default.cfg

mnt2=${mntpoint}2
mount | grep -q $mnt2/mp && umount $mnt2/mp

[ -d $mnt2 ] || mkdir $mnt2
mount | grep $mnt2 | grep -q /dev/md && umount -f $mnt2
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mnt2
chmod 777 $mnt2

mount | grep $mntpoint | grep -q tmpfs && umount -f $mntpoint
mount -t tmpfs tmpfs $mntpoint
chmod 777 $mntpoint

mkdir $mnt2/mp
mount -t nullfs $mntpoint $mnt2/mp

# OK
cp /usr/bin/true $mnt2/mp/true
$mntpoint/true
if ! > $mntpoint/true ; then
	echo FAIL 1
	mount | egrep "tmpfs|nullfs|$mntpoint |$mnt2 "
fi
rm -f $mntpoint/true

# Fails
cp /usr/bin/true $mnt2/mp/true
$mnt2/mp/true
if ! > $mnt2/mp/true; then
	echo FAIL 2
	mount | egrep "tmpfs|nullfs|$mntpoint |$mnt2 "
fi
rm  -f $mnt2/mp/true

umount $mnt2/mp
while mount | grep -q "$mnt2 "; do
	umount $mnt2 || sleep 1
done
mdconfig -d -u $mdstart

while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done
