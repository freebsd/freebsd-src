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

# "panic: _PC_ASYNC_IO should not get here" seen:
# https://people.freebsd.org/~pho/stress/log/pathconf.txt
# Fixed by r320900

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

mp=${mntpoint}2
[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

syscall=`grep lpathconf /usr/include/sys/syscall.h 2>/dev/null`
[ -z "$syscall" ] && exit 0
syscall=`echo $syscall | sed 's/.*[\t ]//'`

mkdir -p $mp/nfs || exit 1
mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw $nfs_export $mp/nfs
chmod a+rw $mp/nfs

start=`date +%s`
while [ $((`date +%s` - start)) -lt 600 ]; do
	../misc/syscall4.sh $syscall
done

umount $mp/nfs
rm -rf $mp
exit 0
