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

# Regression test. This script caused this panic:

# panic: lockmgr: locking against myself
# cpuid = 2
# KDB: enter: panic
# [thread pid 2526 tid 100070 ]
# Stopped at      kdb_enter+0x2b: nop
# db> bt
# Tracing pid 2526 tid 100070 td 0xc46f8360
# kdb_enter(c094247f) at kdb_enter+0x2b
# panic(c09402b6,c46f8360,0,12,c06af5d9,...) at panic+0x14b
# _lockmgr(d864a748,202122,c479f788,c46f8360,c094b01c,12d) at _lockmgr+0x41a
# getblk(c479f6b8,5d51940,0,4000,0,...) at getblk+0x13c
# breadn(c479f6b8,5d51940,0,4000,0,...) at breadn+0x2f
# bread(c479f6b8,5d51940,0,4000,0,e6d13544,c4743eac,0,c095a185,56d) at bread+0x20
# ffs_alloccg(c47408c4,104,1754628,0,4000,c4743eac,1,c095a185,4d8) at ffs_alloccg+0x11d
# ffs_hashalloc(c47408c4,104,1754628,0,4000,...) at ffs_hashalloc+0x45
# ffs_alloc(c47408c4,3f2cd,0,1754628,0,4000,c42fd400,e6d13674) at ffs_alloc+0x1a5
# ffs_balloc_ufs2(c4735d70,fcb34000,0,4000,c42fd400,...) at ffs_balloc_ufs2+0x1619
# ffs_copyonwrite(c479f6b8,d84e3d08) at ffs_copyonwrite+0x3d3
# ffs_geom_strategy(c479f7c0,d84e3d08) at ffs_geom_strategy+0xbd
# bufwrite(d84e3d08,4000,d84e3d08,e6d137e4,c070b7a9,...) at bufwrite+0x17a
# ffs_bufwrite(d84e3d08) at ffs_bufwrite+0x282
# vfs_bio_awrite(d84e3d08) at vfs_bio_awrite+0x235
# bdwrite(d864a6e8,c4743eac,0,c095a185,57c,...) at bdwrite+0x237
# ffs_alloccg(c4b54a50,104,1754628,0,4000,c4743eac,1,c095a185,4d8) at ffs_alloccg+0x1f6
# ffs_hashalloc(c4b54a50,104,1754628,0,4000,...) at ffs_hashalloc+0x45
# ffs_alloc(c4b54a50,1b00c,0,1754628,0,4000,c4b22a80,e6d139ac) at ffs_alloc+0x1a5
# ffs_balloc_ufs2(c4e72158,6c030000,0,4000,c4b22a80,...) at ffs_balloc_ufs2+0x1619
# ffs_write(e6d13b98) at ffs_write+0x2ac
# VOP_WRITE_APV(c0a00e80,e6d13b98) at VOP_WRITE_APV+0x132
# vn_write(c46c65a0,e6d13c60,c4b22a80,0,c46f8360) at vn_write+0x1f6
# dofilewrite(c46f8360,4,c46c65a0,e6d13c60,ffffffff,...) at dofilewrite+0x77
# kern_writev(c46f8360,4,e6d13c60,8430000,d0000,...) at kern_writev+0x36
# write(c46f8360,e6d13d00) at write+0x45
# syscall(e6d13d38) at syscall+0x256

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

persist () {
	false
	while [ $? -ne 0 ]; do
		$1 > /dev/null 2>&1
		sleep 1
	done
}

diskfree=`df -k /var/tmp | tail -1 | awk '{print $4}'`
[ $((diskfree / 1024 / 1024)) -lt 5 ] && echo "Not enough disk space" && exit 1

rm -f /var/.snap/stress2 /var/tmp/big.?
trap "rm -f /var/.snap/stress2 /var/tmp/big.?" EXIT INT
persist 'mksnap_ffs /var /var/.snap/stress2'
tresh=`sysctl  vfs.dirtybufthresh | awk '{print $NF}'`
sysctl vfs.dirtybufthresh=10

cd /var/tmp
for j in `jot 5`; do
	old=`sysctl  vfs.altbufferflushes | awk '{print $NF}'`
	for i in `jot 4`; do
		echo "`date '+%T'` Create big.$i"
		dd if=/dev/zero of=big.$i bs=1m count=4k status=none
	done
	sleep 1
	rm -rf /var/tmp/big.?
	new=`sysctl  vfs.altbufferflushes | awk '{print $NF}'`
	[ $new -ne $old ] && echo "vfs.altbufferflushes changed from $old to $new."
done
sysctl vfs.dirtybufthresh=$tresh
rm -f /var/.snap/stress2
