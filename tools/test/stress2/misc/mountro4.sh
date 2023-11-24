#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Check that the time stamps are not updated for a RO mount.

. ../default.cfg

f1=$mntpoint/f1
f2=$mntpoint/f2
s=0

# ufs
mount | grep -q "on $mntpoint " && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint || exit 1
touch $f1
mount -u -o ro $mntpoint
touch $f2 2>/dev/null && { echo "ufs: ro failed"; s=1; }
d1=`stat -f '%a %m %c' $f1`
sleep 1
cat $f1 > /dev/null
d2=`stat -f '%a %m %c' $f1`
if [ "$d1" != "$d2" ]; then
	echo "ufs: Access time was updated. $d1 != $d2"
	s=1
fi
mount -u -o rw $mntpoint
touch $f2 2>/dev/null || { echo "ufs: rw failed"; s=1; }
umount $mntpoint
mdconfig -d -u $mdstart

# tmpfs
mount -o size=100m -t tmpfs null $mntpoint || exit 1
touch $f1
mount -u -o ro $mntpoint
touch $f2 2>/dev/null && { echo "tmpfs: ro failed"; s=1; }
d1=`stat -f '%a %m %c' $f1`
sleep 1
cat $f1 > /dev/null
d2=`stat -f '%a %m %c' $f1`
if [ "$d1" != "$d2" ]; then
	echo "tmpfs: Access time was updated. $d1 != $d2"
	s=1
fi
mount -u -o rw $mntpoint
touch $f2 2>/dev/null || { echo "tmpfs: rw failed"; s=1; }
umount $mntpoint

# msdosfs
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos /dev/md${mdstart}$part > /dev/null
mount -t msdosfs /dev/md${mdstart}$part $mntpoint || exit 1

touch $f1
mount -u -o ro $mntpoint
touch $f2 2>/dev/null && { echo "msdosfs: ro failed"; s=1; }
d1=`stat -f '%a %m %c' $f1`
sleep 1
cat $f1 > /dev/null
d2=`stat -f '%a %m %c' $f1`
if [ "$d1" != "$d2" ]; then
	echo "msdosfs: Access time was updated. $d1 != $d2"
	s=1
fi
mount -u -o rw $mntpoint
touch $f2 2>/dev/null || { echo "msdosfs: rw failed"; s=1; }
umount $mntpoint
mdconfig -d -u $mdstart

exit $s
