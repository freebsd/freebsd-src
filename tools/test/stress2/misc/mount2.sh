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

# Stress test by performing parallel calls to mount and umount. Alternate
# between forced and non-forced unmounts.

# "kernel: g_dev_taste: make_dev_p() failed (gp->name=md10, error=17)" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15	# Number of parallel scripts
D=$diskimage

for i in `jot $mounts`; do
	m=$(( i + mdstart - 1 ))
	[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
	mount | grep "$mntpoint" | grep -q md$m &&
	    umount ${mntpoint}$m
	mdconfig -l | grep -q md$m && mdconfig -d -u $m

	dd if=/dev/zero of=$D$m bs=1m count=1 status=none
	mdconfig -a -t vnode -f $D$m -u $m || { rm -f $D$m; exit 1; }
	newfs md${m} > /dev/null
done

# start the parallel tests
for i in `jot $mounts`; do
	m=$(( i + mdstart - 1 ))
	start=`date +%s`
	while [ $((`date +%s` - start)) -lt 300 ]; do
		opt=`[ $(( m % 2 )) -eq 0 ] && echo -f`
		mount /dev/md${m} ${mntpoint}$m
		while mount | grep -q ${mntpoint}$m; do
			umount $opt ${mntpoint}$m > /dev/null 2>&1
		done
	done &
done
wait

for i in `jot $mounts`; do
	m=$((i + mdstart - 1))
	mdconfig -d -u $m
	rm -f $D$m
done
exit 0
