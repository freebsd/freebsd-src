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
# Run stress2 from a tmpfs file system.
# Hunt for Assertion error_ == 0 failed at /usr/src/sys/vm/vm_map.c:553

# "vn_vget_ino_get: 0xfffff80de2e269e0 is not locked but should be" seen:
# https://people.freebsd.org/~pho/stress/log/kostik1219.txt
# Fixed by r351542

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -o size=2g -t tmpfs null $mntpoint || exit 1
chmod 777 $mntpoint

cp -r ../../stress2 $mntpoint

export LOAD=80
export MAXSWAPPCT=80
export RUNDIR=$mntpoint/stressX
export runRUNTIME=10m
export runRUNTIME=10m
export rwLOAD=80
export TESTPROGS=`cd ..; find testcases/ -perm -1 -type f | \
    egrep -v "/run/"`

here=`pwd`
cd $mntpoint/stress2/misc || exit 1
su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'
cd $here

umount $mntpoint
exit 0
