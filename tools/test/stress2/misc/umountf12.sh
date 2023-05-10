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

# acquiring duplicate lock of same type: "vnode interlock"
#  1st vnode interlock @ /usr/src/sys/fs/nullfs/null_vnops.c:342
#  2nd vnode interlock @ kern/vfs_default.c:1116
# seen.
# Fixed by r348698

# Test scenario suggestion by kib@

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

log=/tmp/umountf13.log
tail -F -n 0 /var/log/messages > $log & lpid=$!
set -e
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -o size=1g -t tmpfs null $mntpoint
mp2=$mntpoint$((mdstart + 1))
mkdir -p $mp2
mount | grep -q "on $mp2 " && umount -f $mp2

mount -t nullfs $mntpoint $mp2
set +e

for i in `jot 10 1`; do
	cp /bin/sleep $mp2/sleep$i
	$mp2/sleep$i .2
	rm $mp2/sleep$i
done
cp /bin/sleep $mp2/sleep
$mp2/sleep 10 & spid=$!
umount -f $mp2
wait $spid
grep -A2 "acquiring duplicate lock of same type" $log && s=1 || s=0
kill $lpid
rm $log
wait

umount $mntpoint
exit $s
