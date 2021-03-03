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

# growfs(8) test
# "checksum failed: cg 52, cgp: 0x0 != bp: 0xe35de2ca" seen.
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=222876
# Fixed by r324499

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

s=0
set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 32g -u $mdstart
/sbin/gpart create -s GPT md$mdstart
/sbin/gpart add -t freebsd-ufs -s 2g -a 4k md$mdstart
set +e

newfs $newfs_flags md${mdstart}p1 > /dev/null
mount /dev/md${mdstart}p1 $mntpoint
cp -r /usr/include $mntpoint/inc1
umount $mntpoint
checkfs /dev/md${mdstart}p1 || { s=1; echo "Initial fsck 1 fail";  }

gpart resize -i 1 -s 31g -a 4k md$mdstart
growfs -y md${mdstart}p1 > /dev/null
# This fsck make the checksum error go away
#checkfs /dev/md${mdstart}p1 || { s=1; echo "fsck after growfs fail";  }

mount /dev/md${mdstart}p1 $mntpoint
cp -r /usr/include $mntpoint/inc2
umount $mntpoint
checkfs /dev/md${mdstart}p1 || { s=1; echo "Final fsck fail";  }

mdconfig -d -u $mdstart
exit $s
