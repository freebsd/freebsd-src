#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# Another parallel mount(8) test scenario

. ../default.cfg

mounts=15

../testcases/swap/swap -t 2m -i 20 &
for i in `jot $mounts $mdstart`; do
	mdconfig -a -s 50m -u $i
	newfs -U /dev/md$i > /dev/null
	mkdir -p $mntpoint$i
	start=`date +%s`
	while [ $((`date +%s` - start)) -lt 120 ]; do
		mount /dev/md$i $mntpoint$i && cp /etc/passwd $mntpoint$i
		while mount | grep -q " on $mntpoint$i "; do
			umount $mntpoint$i > /dev/null 2>&1
		done
	done &
	while [ $((`date +%s` - start)) -lt 120 ]; do
		find $mntpoint$i -ls > /dev/null 2>&1
	done &
done
wait

for i in `jot $mounts $mdstart`; do
	mdconfig -d -u $i
	rmdir  $mntpoint$i
done
exit $s
