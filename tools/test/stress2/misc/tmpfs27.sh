#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# umount FS with memory mapped file. tmpfs version.

# "panic: object with writable mappings does not have a reference" seen:
# https://people.freebsd.org/~pho/stress/log/log0518.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -eu
prog=$(basename "$0" .sh)
here=`pwd`
mp1=$mntpoint

mount -t tmpfs dummy $mp1

export RUNDIR=$mp1/stressX
export runRUNTIME=2m
export LOAD=70
export mmapLOAD=100
export TESTPROGS="testcases/mmap/mmap testcases/swap/swap"
set +e

(cd ..; ./testcases/run/run $TESTPROGS > /dev/null 2>&1) & rpid=$!
sleep 5

start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	umount -f $mp1 &&
	    mount -t tmpfs dummy $mp1
	mount | grep -q "on $mp1 " || break
	pgrep -q mmap || break
done
pkill run swap mmap
while pgrep -q swap; do pkill swap; done
wait $rpid

while mount | grep -q "on $mp1 "; do
	umount $mp1
done
exit 0
