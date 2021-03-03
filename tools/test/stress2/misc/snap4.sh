#!/bin/sh

#
# Copyright (c) 2008, 2011 Peter Holm <pho@FreeBSD.org>
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

# Test with snapshot file unlinked before unmount.

# "panic: snapacct_ufs2: bad block" seen:
# https://people.freebsd.org/~pho/stress/log/snap4.txt

. ../default.cfg

mount | grep -q "on /tmp (ufs," || exit 0
mount | grep -q "/dev/md$mdstart on $mntpoint" && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
rm -f /tmp/.snap/stress2
trap "rm -f /tmp/.snap/stress2" 0

start=`date '+%s'`
while [ `date '+%s'` -lt $((start + 1200)) ]; do
   mksnap_ffs /tmp /tmp/.snap/stress2
   mdconfig -a -t vnode -f /tmp/.snap/stress2 -u $mdstart -o readonly
   mount -o ro /dev/md$mdstart $mntpoint

   ls -l $mntpoint > /dev/null
   rm -f /tmp/.snap/stress2
   sleep 1

   umount $mntpoint
   mdconfig -d -u $mdstart
   rm -f /tmp/.snap/stress2
done
