#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Test scenario for exhausting limited malloc KVA on 32bit machine, by
# extending a file on a UFS SU volume.

# Create a large file on a file system with trim enabled.
# Watchdog timeout seen:
# https://people.freebsd.org/~pho/stress/log/trim6.txt
# Fixed by r287361.

# "panic: negative mnt_ref" seen:
# https://people.freebsd.org/~pho/stress/log/kostik835.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

r=`mount | grep -w soft-updates | awk '{print $1}' | while read dev; do
	dumpfs $dev | head -20 | grep -qw trim || continue
	df -k $dev
done |  sort -rn +3 | head -1 | awk '{print $4, $6}'`
[ -z "$r" ] && exit	# No trim enabled file systems found
set $r
free=$(($1 / 1024 / 10 * 9))	# in Mb
max=$((128 * 1024)) # Max is 128 GB
[ $free -gt $max ] && free=$max
mp=$2
image=`echo $mp/diskimage | sed s#//#/#` # fix "//" for case mp = "/"
trap "rm -f $image" EXIT INT

echo "dd if=/dev/zero of=$image bs=1m count=$free"
dd if=/dev/zero of=$image bs=1m count=$free status=none
exit 0
