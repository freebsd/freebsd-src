#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Test scenario description by: Kyle Evans <kevans@FreeBSD.org>

# "panic: mtx_lock() of destroyed mutex 0xffffffff83717540 @ /usr/src/sys/fs/fdescfs/fdesc_vnops.c:151" seen

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

set -u

kldstat | grep -q fdescfs.ko && { kldunload fdescfs.ko && wasloaded=1; }
while true; do
	mount | grep -q "on $mntpoint " ||
		mount -t fdescfs dummy $mntpoint || continue
	ls $mntpoint > /dev/null
	if mount | grep -q "on $mntpoint "; then
		if ! umount $mntpoint; then
			umount -f $mntpoint || break
		fi
	fi
done > /dev/null 2>&1 &

start=`date +%s`
while [ $((`date +%s` - start)) -lt 10 ]; do
	kldstat | grep -q fdescfs.ko &&
		kldunload fdescfs.ko 2>/dev/null
	sleep .1
	kldstat | grep -q fdescfs.ko ||
		kldload fdescfs.ko
done
kill %1
wait
mount | grep -q "on $mntpoint " && umount $mntpoint
sleep .1
set +u
[ $wasloaded ] || kldunload fdescfs.ko
exit 0
