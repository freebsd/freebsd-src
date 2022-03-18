#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Test scenario from D17599 "Fix for double free when deleting entries from
# epoch managed lists"
# by Hans Petter Selasky <hselasky@freebsd.org>

# "panic: starting DAD on non-tentative address 0xfffff8010c311000" seen.
# https://people.freebsd.org/~pho/stress/log/epoch.txt

# Fatal trap 9: general protection fault while in kernel mode
# https://people.freebsd.org/~pho/stress/log/epoch-2.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
if=`ifconfig | grep -w mtu | grep -v RUNNING | sed 's/:.*//' | head -1`
[ -z "$if" ] &&
    if=`ifconfig | \
    awk  '/^[a-z].*: / {gsub(":", ""); ifn = $1}; /no car/{print ifn; exit}'`

[ -z "$if" ] && exit 0
echo "Using $if for test."
ifconfig $if | grep -q RUNNING && running=1

start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	for i in `jot 255`; do
		(ifconfig $if.$i create
		ifconfig $if.$i inet 224.0.0.$i netmask 255.255.255.0
		ifconfig $if.$i destroy) > /dev/null 2>&1 &
	done
	wait
done
[ $running ] || ifconfig $if down

exit 0
