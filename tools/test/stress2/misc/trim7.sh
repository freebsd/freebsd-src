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

# "panic: Lock Per-Filesystem Softdep Lock not exclusively locked @
#  ../../../ufs/ffs/ffs_softdep.c:1950" seen.
# https://people.freebsd.org/~pho/stress/log/trim7.txt
# Fixed by: r297206.

# Test scenario by: Nick Evans <nevans@talkpoint.com>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

mount | grep -q /media ||
    echo "Prerequisite: /media is a TRIM enabled file system."

r=`mount | grep -w soft-updates | awk '{print $1}' | while read dev; do
	dumpfs $dev | grep -m1 flags | grep -qw trim || continue
	df -k $dev
done |  sort -rn +3 | head -1 | awk '{print $4, $6}'`
[ -z "$r" ] && exit	# No trim enabled file systems found
set $r
free=$(($1 / 1024 / 10 * 9))	# in Mb
max=$((128 * 1024)) # Max is 128 GB
[ $free -gt $max ] && free=$max
mp=$2
[ $mp = /media ] || exit 0
fs=`mount | grep "on $mp " | sed 's/ .*//'`
echo "Using $fs"
image=`echo $mp/diskimage | sed s#//#/#` # fix "//" for case mp = "/"

echo "dd if=/dev/zero of=$image bs=1m count=$free"
dd if=/dev/zero of=$image bs=1m count=$free status=none
rm $image
while mount | grep -q "on $mp "; do
	umount $mp
done
mount $fs $mp
