#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Variation of nullfs.sh

# "panic: LK_RETRY set with incompatible flags (0x202400) or
#     an error occurred (11)" seen.
# https://people.freebsd.org/~pho/stress/log/matt001.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15	# Number of parallel scripts
: ${nullfs_srcdir:=/tmp}
: ${nullfs_dstdir:=$mntpoint}
CONT=/tmp/nullfs17.continue

if [ $# -eq 0 ]; then
	for i in `jot $mounts`; do
		[ ! -d ${nullfs_dstdir}$i ] && mkdir ${nullfs_dstdir}$i
		mount | grep -q " ${nullfs_dstdir}$i " &&
		    umount ${nullfs_dstdir}$i
	done

	# start the parallel tests
	for i in `jot $mounts`; do
		./$0 $i &
		./$0 find $i &
	done
	wait

	for i in `jot $mounts`; do
		umount ${nullfs_dstdir}$i > /dev/null 2>&1
	done
	exit 0
else
	if [ $1 = find ]; then
		while [ -f $CONT ]; do
			find ${nullfs_dstdir}$2 -type f -maxdepth 2 -ls > \
			    /dev/null 2>&1
		done
	else
		# The test: Parallel mount and unmounts
		touch $CONT
		start=`date '+%s'`
		while [ `date '+%s'` -lt $((start + 300)) ]; do
			m=$1
			mount_nullfs $nullfs_srcdir ${nullfs_dstdir}$m > \
			    /dev/null 2>&1
			opt=`[ $(( m % 2 )) -eq 0 ] && echo -f`
			while mount | grep -q ${nullfs_dstdir}$m; do
				umount $opt ${nullfs_dstdir}$m > \
				    /dev/null 2>&1
			done
		done
		rm -f $CONT
	fi
fi
