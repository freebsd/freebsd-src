#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Kirk McKusick <mckusick@mckusick.com>
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

# 'panic: Lock (lockmgr) ufs not locked @ kern/kern_lock.c:1271' seen:
# https://people.freebsd.org/~pho/stress/log/gnop8.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

fsck=/sbin/fsck_ffs
exp=/sbin/fsck_ffs.exp	# Experimental version
[ -f $exp ] && { echo "Using $exp"; fsck=$exp; }
mdconfig -a -t swap -s 5g -u $mdstart || exit 1
md=md$mdstart
newfs -j /dev/$md || exit 1
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	gnop create /dev/$md || exit 1
	mount /dev/$md.nop /mnt || exit 1

	# start your favorite I/O test here
	cp -rp /[a-l]* /[n-z]* /mnt &

	# after some number of seconds
	sleep 1
	gnop destroy -f /dev/$md.nop
	kill $!

	# wait until forcible unmount, may be up to about 30 seconds,
	# but typically very quick if I/O is in progress
	while (a=`mount | egrep /mnt`) do sleep 1; done

	# first fsck will attempt journal recovery
	$fsck -d -y /dev/$md

	# second fsck will do traditional fsck to check for any errors
	# from journal recovery
	$fsck -d -y /dev/$md
	wait
done
mdconfig -d -u ${md#md}
exit 0
