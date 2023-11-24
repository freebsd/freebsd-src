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

# NFS test

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

cont=/tmp/crossmp2.continue
mounts=10	# Number of parallel scripts

if [ $# -eq 0 ]; then
	[ -z "$nfs_export" ] && exit 0
	ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
	    exit 0
	touch $cont
	mount -t nfs -o tcp -o nfsv3 -o retrycnt=1 -o intr,soft,timeout=1 \
	    -o rw $nfs_export $mntpoint || exit 0
	sleep .2
	umount $mntpoint

	for i in `jot $mounts`; do
		mp=${mntpoint}$i
		[ ! -d $mp ] && mkdir $mp
		mount | grep -q "$mp " && umount $mp
	done

	# start the parallel tests
	for i in `jot $mounts`; do
		./$0 $i &
		./$0 find &
	done
	wait
	exit 0
else
	if [ $1 = find ]; then
		while [ -r $cont ]; do
			find ${mntpoint}* -maxdepth 1 -type f > \
			    /dev/null 2>&1
		done
	else

		# The test: Parallel mount and unmounts
		for i in `jot 128`; do
			m=$1
			mount -t nfs -o tcp -o nfsv3 -o retrycnt=3 \
			    -o intr,soft -o rw $nfs_export ${mntpoint}$m
			sleep .5
			opt=`[ $(( m % 2 )) -eq 0 ] && echo -f`
			n=0
			while mount | grep -q "${mntpoint}$m "; do
				umount $opt ${mntpoint}$m > /dev/null 2>&1
				[ $((n += 1)) -gt 99 ] && umount -f \
				    ${mntpoint}$m > /dev/null 2>&1
				[ $n -gt 100 ] && exit
			done
		done
		rm -f $cont
	fi
fi
