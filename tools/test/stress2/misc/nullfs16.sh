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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

# [Bug 178238] [nullfs] nullfs don't release i-nodes on unlink.
# Test scenaro by: noah.bergbauer@tum.de
# Fixed by: r292961

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -o size=1g -t tmpfs tmpfs $mntpoint

mp2=${mntpoint}2
[ -d $mp2 ] || mkdir -p $mp2
mount | grep -q "on $mp2 " && umount -f $mp2
mount -t nullfs $mntpoint $mp2

# Expect second dd to fail
for i in `jot 2`; do
	dd if=/dev/zero of=$mp2/file bs=1m count=1023 \
	    status=none
	rm -f $mp2/file
done
if [ `df -i $mp2 | tail -1 | awk '{print $5}'` = "100%" ]; then
	echo FAIL
	s=1
	ls -al $mntpoint $mp2
	df -i | egrep "${mntpoint}$|${mp2}$"
fi

umount $mp2
umount $mntpoint

exit $s
