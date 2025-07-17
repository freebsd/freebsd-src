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

# Found: Fatal trap 12: page fault while in kernel mode

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15		# Number of parallel scripts
mdstart=$mdstart	# Use md unit numbers from this point
D=$diskimage

if [ $# -eq 0 ]; then
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
		mount | grep "$mntpoint" | grep -q md$m && umount ${mntpoint}$m
		mdconfig -l | grep -q md$m &&  mdconfig -d -u $m

		dd if=/dev/zero of=$D$m bs=1m count=10 status=none
		mdconfig -a -t vnode -f $D$m -u $m ||
		    { rm -f $D$m; exit 1; }
		newfs md${m} > /dev/null 2>&1
	done

	# start the parallel tests
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		./$0 $m &
		./$0 find &
	done
	wait

	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		mdconfig -d -u $m
		rm -f $D$m
	done

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
			mount $opt /dev/md${m} ${mntpoint}$m
			cp -r /usr/include/machine/a* ${mntpoint}$m
			while mount | grep -qw $mntpoint$m; do
				umount -f ${mntpoint}$m > /dev/null 2>&1
			done
		done
	fi
fi
