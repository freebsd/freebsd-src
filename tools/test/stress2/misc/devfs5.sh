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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Test scenario for https://reviews.freebsd.org/D20411
# Add devfs(5) support for VOP_MKDIR(9) and VOP_RMDIR(9)

. ../default.cfg

mkdir /dev/devfs5 2>/dev/null || exit 0
rmdir /dev/devfs5
mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount  -t devfs null $mntpoint || exit 1

(cd ../testcases/swap; ./swap -t 30m -i 20 -h -l 100) &
spid=$!
cd $mntpoint
N=3000
for i in `jot 25`; do
	(
		mkdir s$i; cd s$i
		for k in `jot 5`; do
			for j in `jot $N`; do mkdir d$i.$j; done
			for j in `jot $N`; do rmdir d$i.$j; done
		done
		cd ..; rmdir s$i
	) &
	pids="$pids $!"
done
for i in $pids; do
	wait $i
done
while pkill swap; do :; done
wait spid

cd /
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint && break
	sleep 1
done
exit 0
