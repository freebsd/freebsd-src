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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Regression test: Delete an active md device
#
# panic(c088cd33,deadc000,c0943aa0,0,c08753e1) at panic+0x14b
# vm_fault(c1060000,deadc000,1,0,c54de480) at vm_fault+0x1e0
# trap_pfault(e76728b8,0,deadc112) at trap_pfault+0x137
# trap(8,c0870028,28,deadc0de,deadc0de) at trap+0x355
# calltrap() at calltrap+0x5
# --- trap 0xc, eip = 0xc060bcfb, esp = 0xe76728f8, ebp = 0xe767291c ---
# g_io_request(c53ff7bc,c5051d40,d8c72408,c54ca110,e7672950) at g_io_request+0x5f

. ../default.cfg

mount | grep -q "on /tmp (ufs," || exit 0
rm -f /tmp/.snap/stress2
trap "rm -f /tmp/.snap/stress2" 0
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mksnap_ffs /tmp /tmp/.snap/stress2
mdconfig -a -t vnode -f /tmp/.snap/stress2 -u $mdstart -o readonly
mount -r /dev/md$mdstart $mntpoint

ls -lR  > /dev/null 2>&1 &
mdconfig -d -u $mdstart > /dev/null 2>&1

umount $mntpoint > /dev/null 2>&1
umount -f $mntpoint > /dev/null 2>&1
mdconfig -d -u $mdstart
rm -f /tmp/.snap/stress2
wait
