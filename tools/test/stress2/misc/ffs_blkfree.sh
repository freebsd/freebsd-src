#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

D=$diskimage
DUMP=$RUNDIR/dump
trap "rm -f $D" EXIT INT
dd if=/dev/zero of=$D bs=1m count=1024 status=none || exit 1

mount | grep "$mntpoint" | grep md$mdstart > /dev/null &&
	umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null && mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
mount

for i in `jot 10`; do
	mkdir $mntpoint/d$i
	for j in `jot 100`; do
		touch $mntpoint/d$i/f$j
	done
done

dump -L0a -f $DUMP /dev/md$mdstart

ls -lf $DUMP

while mount | grep -q $mntpoint; do
	umount $([ $((`date '+%s'` % 2)) -eq 0 ] &&
	    echo "-f" || echo "") $mntpoint > /dev/null 2>&1
done

for i in `jot 10`; do
	newfs $newfs_flags -n md$mdstart > /dev/null
	mount /dev/md$mdstart $mntpoint
	(cd $mntpoint; restore -rf $DUMP)
	rm -rf $mntpoint/*
	while mount | grep -q $mntpoint; do
		umount $([ $((`date '+%s'` % 2)) -eq 0 ] &&
		    echo "-f" || echo "") $mntpoint > /dev/null 2>&1
	done
done

mdconfig -d -u $mdstart
rm -f $D $DUMP
