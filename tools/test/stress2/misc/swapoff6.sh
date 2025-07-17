#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# A swap test scenario, using swapoff(8) and sort(1) for VM pressure

# Out of free pages seen:://people.freebsd.org/~pho/stress/log/log0540.txt 

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

[ `swapinfo | wc -l` -eq 1 ] && exit 0
set -u
nmax=`sysctl -n hw.ncpu`
[ $nmax -gt 4 ] && nmax=4

for i in `jot $nmax`; do
	timeout -k 2m 1m sort /dev/zero &
	sleep .`jot -r 1 1 9`
done
while [ `swapinfo | tail -1 | awk '{sub("%","");print $NF}'` -lt 2 ]; do sleep 1; done

start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	while ! swapoff -a > /dev/null 2>&1; do sleep .1; done
	swapon -a > /dev/null
	ncur=`pgrep sort | wc -l`
	if [ $ncur -lt $nmax ]; then
		echo "Starting $((nmax - ncur)) sort"
		for i in `jot $((nmax - ncur))`; do
			timeout -k 2m 1m sort /dev/zero &
			sleep .`jot -r 1 1 9`
		done
	fi
done
pkill -9 sort
wait
exit 0
