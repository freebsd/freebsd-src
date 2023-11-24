#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Peter Holm
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

# umount a tmpfs fs while a rename of a directory is in progress
# Test scenario suggestion by:	 kib

# umount(8) stuck in suspwt seen.
# https://people.freebsd.org/~pho/stress/log/tmpfs21.txt
# Fixed by r355061

(cd ../testcases/swap; ./swap -t 10m -i 20) > /dev/null &
RUNDIR=$mntpoint/stressX
start=`date +%s`
mount | grep -q "on $mntpoint " && umount -f $mntpoint
while [ $((`date +%s` - start)) -lt 60 ]; do
	mount -t tmpfs tmpfs $mntpoint || exit 1
	(cd ../testcases/dirnprename; ./dirnprename -t 2m -i 20 > \
	    /dev/null 2>&1 ) &
	pid=$!
	sleep .`jot -r 1 1 9`
	for i in `jot 6`; do
		umount $mntpoint && break
		sleep .5
	done 2>&1 | grep -v "Device busy"
	mount | grep -q "on $mntpoint " && umount -f $mntpoint
	wait $pid
done
while pkill swap; do sleep .2; done
wait
exit 0
