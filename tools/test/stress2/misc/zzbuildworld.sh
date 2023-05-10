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

# Run a "make buildworld" as last test in stress2/misc.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kernel=`sysctl -n kern.bootfile`
kerneldir=`strings -a $kernel | egrep -m 1 "$(hostname).*/compile/" | sed 's/.*://;s/ .*//'`
[ -z "$kerneldir" ] && { echo "Can not find kerneldir"; exit 1; }
src=`echo $kerneldir | sed 's#/sys/.*##'`
[ ! -d $src ] && { echo "Can not find source in $src"; exit 1; }

start=`date +%s`
mount | grep -q "on $mntpoint " && exit 1
mount -t tmpfs tmpfs $mntpoint
top=$mntpoint
export MAKEOBJDIRPREFIX=$top/obj
export log=$top/buildworld.`date +%Y%m%dT%H%M`
n=$((`sysctl -n hw.ncpu` + 1))
cd $src
make -j$n buildworld > $log 2>&1 && s=0 ||s=1
grep  '\*\*\*' $log && s=2
stop=`date +%s`
echo "Elapsed `date -u -j -f '%s' '+%H:%M' $((stop - start))`"
[ $s -ne 0 ] && { xz $log; cp -a $log.xz /tmp; }
umount $mntpoint
exit $s
