#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# panic: vfs_mount_destroy: nonzero writeopcount, seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15		# Number of parallel scripts

if [ $# -eq 0 ]; then
	# start the parallel tests
	for i in `jot $mounts`; do
		[ -d ${mntpoint}$i ] || mkdir -p ${mntpoint}$i
		./$0 $i &
		./$0 find &
	done
	wait

else
	if [ $1 = find ]; then
		start=`date +%s`
		while [ $((`date +%s`- start)) -lt 300 ]; do
			find ${mntpoint}* -type f > /dev/null 2>&1
		done
	else

		# The test: Parallel mount and unmounts
		start=`date +%s`
		while [ $((`date +%s`- start)) -lt 300 ]; do
			m=$1
			opt=`[ $(( m % 2 )) -eq 0 ] && echo -f`
			mount -t tmpfs tmpfs ${mntpoint}$m
			cp -r /usr/include/machine/a* ${mntpoint}$m
			while mount | grep -qw $mntpoint$m; do
				umount $opt ${mntpoint}$m > /dev/null 2>&1
			done
		done
	fi
fi
