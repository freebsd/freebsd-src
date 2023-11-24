#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# A sigreturn(2) syscall fuzzer

# panic: vm_fault_lookup: fault on nofault entry, addr: 0
# cpuid = 0
# time = 1661248103
# KDB: stack backtrace:
# db_trace_self_wrapper(e,27e1dae0,27e1dae2,ffc07db8,c1f0490,...) at db_trace_self_wrapper+0x28/frame 0xffc07d24
# vpanic(150b260,ffc07d60,ffc07d60,ffc07e20,12cb565,...) at vpanic+0xf4/frame 0xffc07d40
# panic(150b260,14ec2f3,0,149349d,1430,...) at panic+0x14/frame 0xffc07d54
# vm_fault(1e37000,0,4,0,0) at vm_fault+0x1725/frame 0xffc07e20
# vm_fault_trap(1e37000,3b,4,0,0,0) at vm_fault_trap+0x52/frame 0xffc07e48
# trap_pfault(3b,0,0) at trap_pfault+0x176/frame 0xffc07e94
# trap(ffc07f6c,8,28,28,1915b000,...) at trap+0x2d9/frame 0xffc07f60
# calltrap() at 0xffc031ef/frame 0xffc07f60
# --- trap 0xc, eip = 0x3b, esp = 0xffc07fac, ebp = 0xffc0340c ---
# (null)() at 0x3b/frame 0xffc0340c
# KDB: enter: panic
# [ thread pid 26126 tid 120029 ]
# Stopped at      kdb_enter+0x34: movl    $0,kdb_why
# db> x/s version
# version: FreeBSD 14.0-CURRENT #0 ast-n257558-eb20af97a66-dirty: Mon Aug 22 17:53:22 CEST 2022
# pho@mercat1.netperf.freebsd.org:/media/obj/var/tmp/deviant3/i386.i386/sys/PHO
# db> 

inc=/usr/include/sys/syscall.h
[ -f $inc ] || exit 0
num=`awk '/SYS_sigreturn/ {print $NF}' < $inc`
old=`grep -E 'sigreturn \*' < $inc | \
    sed 's/.* \([0-9]\{1,3\}\) .*/\1/' | tr '\n' ' '`
num="$num $old"

start=`date +%s`
while [ $((`date +%s` - start)) -lt 180 ]; do
	for i in $num; do
		echo "noswap=1 ./syscall4.sh $i"
		noswap=1 ./syscall4.sh $i
	done
done

exit 0
