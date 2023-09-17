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

# "panic: snapacct_ufs2: bad block" seen:
# https://people.freebsd.org/~pho/stress/log/snap5-1.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "on /tmp (ufs," || exit 0
mnt2=$mntpoint
[ ! -d $mnt2 ] && mkdir $mnt2

mkdir -p /tmp/.snap
trap "rm -f /tmp/.snap/stress2" 0
start=`date '+%s'`
while [ `date '+%s'` -lt $((start + 600)) ]; do
   if mount | grep -q "/dev/md$mdstart on $mnt2"; then
      umount $mnt2 || exit 2
   fi
   if mdconfig -l | grep -q md$mdstart; then
      mdconfig -d -u $mdstart || exit 3
   fi
   rm -f /tmp/.snap/stress2

   mksnap_ffs /tmp /tmp/.snap/stress2 || continue
   mdconfig -a -t vnode -f /tmp/.snap/stress2 -u $mdstart -o readonly ||
       exit 4
   mount -o ro /dev/md$mdstart $mnt2 || exit 5

   ls -l $mnt2 > /dev/null
   sleep `jot -r 1 0 119`
done
mount | grep -q "/dev/md$mdstart on $mnt2" && umount $mnt2
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
