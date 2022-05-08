#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Peter Holm
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

# Bug 257557 Garbage on disk (or USB memory stick) can cause GEOM taste ffs_sbget() to crash.
# "panic: wrong length 4352 for sectorsize 512" seen:
# https://people.freebsd.org/~pho/stress/log/log0159.txt

. ../default.cfg

mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	dd if=/dev/zero of=$diskimage bs=8m count=1 status=none
	mdconfig -a -t vnode -f $diskimage -u $mdstart
	newfs $newfs_flags /dev/md$mdstart > /dev/null
	mdconfig -d -u $mdstart
	for i in `jot 10`; do
		/tmp/flip -n 4 $diskimage
		mdconfig -a -t vnode -f $diskimage -u $mdstart &&
		    mdconfig -d -u $mdstart
	done
done

mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
rm -f $diskimage /tmp/flip
exit 0
