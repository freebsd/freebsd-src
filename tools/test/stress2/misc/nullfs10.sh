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

# Regression test:

# There is an issue, namely, when the lower file is already
# opened r/w, but its nullfs alias is executed. This situation obviously
# shall result in ETXTBUSY, but it currently does not.

# Test scenario by kib@

. ../default.cfg

mnt2=${mntpoint}2
mount | grep -q $mnt2 && umount $mnt2

[ -d $mnt2 ] || mkdir $mnt2
mount | grep $mnt2 | grep -q /dev/md && umount -f $mnt2
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

mount -t nullfs $mntpoint $mnt2

cp /bin/ls $mntpoint
chmod +w $mntpoint/ls
sleep 2 >> $mntpoint/ls &
sleep .5
# This line should cause a "/mnt2/ls: Text file busy"
$mnt2/ls -l /bin/ls $mntpoint $mnt2 && echo FAIL || echo OK
kill $!
wait

while mount | grep -q "$mnt2 "; do
	umount $mnt2 || sleep 1
done

while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
