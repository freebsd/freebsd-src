#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Run program from isofs file system
# "panic: userret: returning with the following locks held:"
# https://people.freebsd.org/~pho/stress/log/isofs2.txt
# Fixed by: r292772.

[ `id -u ` -ne 0 ] && echo "Must not be root!" && exit 1

[ -z "`type mkisofs 2>/dev/null`" ] && echo "mkisofs not found" && exit 0

. ../default.cfg

D=`dirname $diskimage`/dir
I=`dirname $diskimage`/dir.iso
here=`pwd`
cd /tmp
rm -rf $D $I
mkdir $D
cp `which date` $D
mkisofs -o $I -r $D > /dev/null 2>&1
mdconfig -a -t vnode -f $I -u $mdstart
mount -t cd9660 /dev/md$mdstart $mntpoint

cd $mntpoint
./date > /dev/null
cd $here

umount $mntpoint
mdconfig -d -u $mdstart

rm -rf $D $I
