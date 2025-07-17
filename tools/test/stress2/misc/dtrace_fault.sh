#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Konstantin Belousov <kib@FreeBSD.org>
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

# Regression test for:

# panic: invalid signal
# KDB: stack backtrace:
# db_trace_self_wrapper(c161045c,c18b9e7c,0,c15df420,f3de7ad8,...) at db_trace_self_wrapper+0x2a/frame 0xf3de7aa8
# kdb_backtrace(c160aaf6,5984403e,0,f3de7b74,d,...) at kdb_backtrace+0x2d/frame 0xf3de7b10
# vpanic(c160b255,f3de7b74,c160b255,f3de7b74,f3de7b74,...) at vpanic+0x133/frame 0xf3de7b44
# kassert_panic(c160b255,c1612dc1,7d,cc97240c,0,...) at kassert_panic+0xd9/frame 0xf3de7b68
# trapsignal(cc29ea80,f3de7c30,c0cb259e,2,0,...) at trapsignal+0x246/frame 0xf3de7ba0
# trap(f3de7ce8) at trap+0x7fe/frame 0xf3de7cdc
# calltrap() at calltrap+0x6/frame 0xf3de7cdc
# --- trap 0x20, eip = 0x8048565, esp = 0xbfbfe7d4, ebp = 0xbfbfe7d4 ---

# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=221151
# Fixed by r321919

. ../default.cfg

uname -a | egrep -q "i386|amd64" || exit 0

cat > /tmp/dtrace_fault.c <<EOF
int
main(void)
{

	__asm __volatile ("int \$0x92");
}
EOF

mycc -o /tmp/dtrace_fault /tmp/dtrace_fault.c || exit 1
rm /tmp/dtrace_fault.c

/tmp/dtrace_fault

rm /tmp/dtrace_fault
exit 0
