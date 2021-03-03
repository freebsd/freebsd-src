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

[ `id -u ` -ne 0 ] && echo "Must not be root!" && exit 1

[ -z "`type mkisofs 2>/dev/null`" ] && echo "mkisofs not found" && exit 0

. ../default.cfg

D=`dirname $diskimage`/dir
I=`dirname $diskimage`/dir.iso
export here=`pwd`
cd /tmp

mycc -o fstool $here/../tools/fstool.c

rm -rf $D $I
mkdir $D

(cd $D; /tmp/fstool -n 10 -l -f 512)

mkisofs -o $I -r $D > /dev/null 2>&1

mdconfig -a -t vnode -f $I -u $mdstart
mount -t cd9660 /dev/md$mdstart $mntpoint

for i in `jot 64`; do
   find /$mntpoint -type f > /dev/null 2>&1 &
done
wait

umount $mntpoint
mdconfig -d -u $mdstart

rm -rf $D $I fstool
