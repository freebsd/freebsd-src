#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15	# Number of parallel scripts
: ${nullfs_srcdir:=/tmp}
: ${nullfs_dstdir:=$mntpoint}
runtime=300

for i in `jot $mounts`; do
	[ ! -d ${nullfs_dstdir}$i ] && mkdir ${nullfs_dstdir}$i
	mount | grep -q " ${nullfs_dstdir}$i " &&
	    umount ${nullfs_dstdir}$i
done

for i in `jot $mounts`; do
	start=`date '+%s'`
	while [ `date '+%s'` -lt $((start + $runtime)) ]; do
		find ${nullfs_dstdir}* -type f -maxdepth 2 -ls > \
		    /dev/null 2>&1
	done &
done

(cd ../testcases/swap; ./swap -t ${runtime}s -i 20) &
for i in `jot $mounts`; do
	# The test: Parallel mount and unmounts
	start=`date '+%s'`
	while [ `date '+%s'` -lt $((start + $runtime)) ]; do
		mount_nullfs $nullfs_srcdir ${nullfs_dstdir}$i > \
		    /dev/null 2>&1
		opt=$([ `jot -r 1 0 1` -eq 0 ] && echo "-f")
		while mount | grep -q ${nullfs_dstdir}$i; do
			umount $opt ${nullfs_dstdir}$i > \
			    /dev/null 2>&1
		done
	done &
done
wait
exit 0
