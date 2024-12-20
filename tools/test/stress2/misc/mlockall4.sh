#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# mlockall(2) / nullfs(4) scenario causes:
# http://people.freebsd.org/~pho/stress/log/kostik619.txt
# kern/182661, fixed in r256211.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mlockall4.c
mycc -o mlockall4 -Wall -Wextra mlockall4.c || exit 1
rm -f mlockall4.c

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -t nullfs /tmp $mntpoint
$mntpoint/mlockall4 &
sleep 2
umount -f $mntpoint

wait
rm -f /tmp/mlockall4
exit
EOF
#include <sys/types.h>
#include <err.h>
#include <sys/mman.h>
#include <unistd.h>

int
main(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
		err(1, "mlockall(MCL_CURRENT | MCL_FUTURE)");
	sleep(5);

	return (0);
}
