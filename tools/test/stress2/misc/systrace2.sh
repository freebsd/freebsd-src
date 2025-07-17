#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm
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

# A variation of systrace.sh
# "Fatal trap 12: page fault while in kernel mode" seen:
# https://people.freebsd.org/~pho/stress/log/mark142.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
dtrace -n 'dtrace:::BEGIN { exit(0); }' > /dev/null 2>&1 || exit 0

echo "gvim or top should be running at this point"
for i in `jot 2`; do
	sleep 4 & pid=$!
	dtrace -w -n 'fbt::kern_ioctl:entry {\
	    @io[execname,probefunc] = count(); }' -p $pid > /dev/null 2>&1
	wait $pid
	kldstat | grep -q dtraceall.ko && kldunload dtraceall.ko
	mount -t tmpfs dummy $mntpoint && umount $mntpoint
done
kldstat | grep -q tmpfs && kldunload tmpfs.ko
exit 0
