#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# nullfs cache / nocache benchmark

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

export LANG=C
CACHE=nullfs11-cache.log
NOCACHE=nullfs11-nocache.log
mp1=$mntpoint
mp2=${mntpoint}2
[ -d $mp2 ] || mkdir $mp2
rm -f $CACHE $NOCACHE

mycc -o /tmp/fstool ../tools/fstool.c

test() {
	opt=$1
	mount | grep -wq $mp2 && umount $mp2
	mount | grep -wq $mp1 && umount $mp1
	mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
	mdconfig -a -t swap -s 2g -u $mdstart || exit 1
	newfs $newfs_flags md$mdstart > /dev/null
	mount /dev/md$mdstart $mp1

	mount -t nullfs $opt $mp1 $mp2

	i=1
	(mkdir $mp2/test$i; cd $mp2/test$i; /tmp/fstool -l -f 50 -n 500 -s 4k)
	rm -rf $mp2/test$i
	sync
	sleep 5

	parallel=4
	for j in `jot 5`; do
		s=`date '+%s'`
		for i in `jot $parallel`; do
			(mkdir $mp2/test$i; cd $mp2/test$i; \
			/tmp/fstool -l -f 50 -n 500 -s $((i * 4))k) &
		done
		for i in `jot $parallel`; do
			wait
		done
		rm -rf $mp2/*
		echo $((`date '+%s'` - $s))
	done
	while mount | grep -wq $mp2;  do
		umount $mp2 || sleep 1
	done
	while mount | grep $mp1 | grep -q /dev/md; do
		umount $mp1 || sleep 1
	done
	mdconfig -d -u $mdstart
}

test "-o nocache" > $NOCACHE
test "-o cache"   > $CACHE

ministat -s -w 60 $NOCACHE $CACHE
rm -f /tmp/fstool $CACHE $NOCACHE
