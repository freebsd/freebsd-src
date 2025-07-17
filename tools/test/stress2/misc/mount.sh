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

# Mount regression test

# panic: vm_fault: fault on nofault entry, addr: deadc000
# cpuid = 1
# KDB: enter: panic
# [thread pid 69453 tid 100388 ]
# Stopped at      kdb_enter+0x2b: nop
# db> where
# Tracing pid 69453 tid 100388 td 0xc4b5c1b0
# kdb_enter(c091d9db) at kdb_enter+0x2b
# panic(c0938fa0,deadc000,e6b44834,c06c650e,c0a57d20,...) at panic+0x14b
# vm_fault(c1869000,deadc000,1,0) at vm_fault+0x1e0
# trap_pfault(e6b4499c,0,deadc112) at trap_pfault+0x137
# trap(8,c0910028,28,deadc0de,deadc0de,...) at trap+0x3f5
# calltrap() at calltrap+0x5
# --- trap 0xc, eip = 0xc0667def, esp = 0xe6b449dc, ebp = 0xe6b44a00 ---
# g_io_request(c66d6bdc,c4a1d840,d7c99940,d7c99940,e6b44a34,...) at g_io_request+0x5f
# g_vfs_strategy(c40624c4,d7c99940,d7c99940,0,c4e16dec,...) at g_vfs_strategy+0x49
# ffs_geom_strategy(c40624c4,d7c99940,4ba0,0,c09dad00,...) at ffs_geom_strategy+0x141
# ufs_strategy(e6b44a7c) at ufs_strategy+0xb5
# VOP_STRATEGY_APV(c09da7c0,e6b44a7c) at VOP_STRATEGY_APV+0x95
# bufstrategy(c50d2be0,d7c99940) at bufstrategy+0x55
# breadn(c50d2b2c,0,0,1000,0,...) at breadn+0xf7
# bread(c50d2b2c,0,0,1000,0,...) at bread+0x20
# ffs_read(e6b44bb0) at ffs_read+0x23f
# VOP_READ_APV(c09da7c0,e6b44bb0) at VOP_READ_APV+0x7e
# ufs_readdir(e6b44c38) at ufs_readdir+0xd1
# VOP_READDIR_APV(c09da7c0,e6b44c38) at VOP_READDIR_APV+0x7e
# getdirentries(c4b5c1b0,e6b44d04) at getdirentries+0x13f
# syscall(3b,3b,3b,8240160,1,...) at syscall+0x256

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart

newfs $newfs_flags md$mdstart > /dev/null

# The test:

echo "Expect: mount: /dev/md5a: Device busy"
mount -r /dev/md$mdstart $mntpoint
mount -r /dev/md$mdstart $mntpoint
umount $mntpoint

ls -lR $mntpoint > /dev/null	# panic

# End of test
mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart
