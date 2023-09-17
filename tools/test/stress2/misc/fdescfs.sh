#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Page fault seen in fdesc_allocvp+0x8f:
# http://people.freebsd.org/~pho/stress/log/fdescfs-2.txt
# Fixed by r279401
# https://people.freebsd.org/~pho/stress/log/fdescfs-3.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15		# Number of parallel scripts
cont=/tmp/fdescfs.continue

if [ $# -eq 0 ]; then
	kldstat -v | grep -q fdescfs || { kldload fdescfs.ko; unload=1; }
	touch $cont
	# start the parallel tests
	for i in `jot $mounts`; do
		[ -d ${mntpoint}$i ] || mkdir -p ${mntpoint}$i
		./$0 $i &
		./$0 find &
	done
	wait
	[ $unload ] && kldunload fdescfs.ko
	exit 0
else
	if [ $1 = find ]; then
		exec 6< /dev/zero
		exec 7< /dev/zero
		exec 8< /dev/zero
		exec 9< /dev/zero
		while [ -r $cont ]; do
			ls -l ${mntpoint}* > /dev/null 2>&1
		done
	else

		# The test: Parallel mount and unmounts
		start=`date '+%s'`
		while [ `date '+%s'` -lt $((start + 300)) ]; do
			mount -t fdescfs null ${mntpoint}$1
			while mount | grep -wq ${mntpoint}$1; do
				umount -f ${mntpoint}$1 > /dev/null 2>&1
			done
		done
		rm -f $cont
	fi
fi
